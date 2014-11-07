# -*- coding: utf-8 -*-

import os
import re
import urllib2
import hashlib
import khronos_registry

from itertools import chain

SCRIPTS_DIR			= os.path.dirname(__file__)
OPENGL_DIR			= os.path.normpath(os.path.join(SCRIPTS_DIR, "..", "..", "framework", "opengl"))
EGL_DIR				= os.path.normpath(os.path.join(SCRIPTS_DIR, "..", "..", "framework", "egl"))
OPENGL_IN_DIR		= os.path.join(SCRIPTS_DIR, "src")
OPENGL_INC_DIR		= os.path.join(OPENGL_DIR, "wrapper")
GL_XML				= os.path.join(OPENGL_IN_DIR, "gl.xml")
OPENGL_ENUMS_IN		= os.path.join(OPENGL_IN_DIR, "glEnums.in")
OPENGL_FUNCS_IN		= os.path.join(OPENGL_IN_DIR, "glFunctions.in")
OPENGL_ES_FUNCS_IN	= os.path.join(OPENGL_IN_DIR, "esFunctions.in")

SRC_URL				= "https://cvs.khronos.org/svn/repos/ogl/trunk/doc/registry/public/api/gl.xml"
SRC_REVISION		= 28861
SRC_CHECKSUM		= "65564395098c82ec9d18cc19100357cb11d99f7baf1d99133bb543ffca7a0f0e"

EXTENSIONS			= [
	'GL_KHR_texture_compression_astc_ldr',
	'GL_KHR_blend_equation_advanced',
	'GL_KHR_blend_equation_advanced_coherent',
	'GL_KHR_debug',
	'GL_EXT_geometry_point_size',
	'GL_EXT_tessellation_shader',
	'GL_EXT_geometry_shader',
	'GL_EXT_texture_buffer',
	'GL_EXT_texture_snorm',
	'GL_OES_EGL_image',
	'GL_OES_compressed_ETC1_RGB8_texture',
	'GL_OES_texture_half_float',
	'GL_OES_texture_storage_multisample_2d_array',
	'GL_OES_sample_shading',
]

gl_registry = None

def getGLRegistry ():
	global gl_registry
	if gl_registry is None:
		updateSrc()
		gl_registry = khronos_registry.parse(GL_XML)
	return gl_registry

# return the name of a core command corresponding to an extension command.
# Ideally this should be done using the alias attribute of commands, but dEQP
# just strips the extension suffix.
def getCoreName (name):
	return re.sub('[A-Z]+$', '', name)

def getHybridInterface ():
	# This is a bit awkward, since we have to create a strange hybrid
	# interface that includes both GL and ES features and extensions.
	registry = getGLRegistry()
	glFeatures = registry.getFeatures('gl')
	esFeatures = registry.getFeatures('gles2')
	spec = khronos_registry.InterfaceSpec()

	for feature in registry.getFeatures('gl'):
		spec.addFeature(feature, 'gl', 'core')

	for feature in registry.getFeatures('gles2'):
		spec.addFeature(feature, 'gles2')

	for extName in EXTENSIONS:
		extension = registry.extensions[extName]
		# Add all extensions using the ES2 api, but force even non-ES2
		# extensions to be included.
		spec.addExtension(extension, 'gles2', 'core', force=True)

	# Remove redundant extension commands that are already provided by core.
	for commandName in list(spec.commands):
		coreName = getCoreName(commandName)
		if coreName != commandName and coreName in spec.commands:
			spec.commands.remove(commandName)

	return khronos_registry.createInterface(registry, spec, 'gles2')

def getInterface (registry, api, version=None, profile=None, **kwargs):
	spec = khronos_registry.spec(registry, api, version, profile, **kwargs)
	if api == 'gl' and profile == 'core' and version < "3.2":
		gl32 = registry.features['GL_VERSION_3_2']
		for eRemove in gl32.xpath('remove'):
			spec.addComponent(eRemove)
	return khronos_registry.createInterface(registry, spec, api)

def getVersionToken (api, version):
	prefixes = { 'gles2': "ES", 'gl': "GL" }
	return prefixes[api] + version.replace(".", "")

def genCommandList(iface, renderCommand, directory, filename, align=False):
	lines = map(renderCommand, iface.commands)
	if align:
		lines = indentLines(lines)
	writeInlFile(os.path.join(directory, filename), lines)

def genCommandLists(registry, renderCommand, check, directory, filePattern, align=False):
	for eFeature in registry.features:
		api			= eFeature.get('api')
		version		= eFeature.get('number')
		profile		= check(api, version)
		if profile is True:
			profile = None
		elif profile is False:
			continue
		iface		= getInterface(registry, api, version=version, profile=profile)
		filename	= filePattern % getVersionToken(api, version)
		genCommandList(iface, renderCommand, directory, filename, align)

def getFunctionTypeName (funcName):
	return "%sFunc" % funcName

def getFunctionMemberName (funcName):
	assert funcName[:2] == "gl"
	if funcName[:5] == "glEGL":
		# Otherwise we end up with gl.eGLImage...
		return "egl%s" % funcName[5:]
	else:
		return "%c%s" % (funcName[2].lower(), funcName[3:])

INL_HEADER = """\
/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 *
 * Generated from Khronos GL API description (gl.xml) revision {REVISION}.
 */\
""".replace("{REVISION}", str(SRC_REVISION))

SRC_HEADER = """\
#ifndef {INCLUDE_GUARD}
#define {INCLUDE_GUARD}
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL Utilities
 * ---------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \\file
 * \\brief {DESCRIPTION}
 *
 * WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 *//*--------------------------------------------------------------------*/

#ifndef _DEDEFS_H
#	include "deDefs.h"
#endif
#ifndef _GLTYPES_H
#	include "glTypes.h"
#endif

DE_BEGIN_EXTERN_C

"""

SRC_FOOTER = """

DE_END_EXTERN_C

#endif /* {INCLUDE_GUARD} */
"""[1:]

def nextMod (val, mod):
	if val % mod == 0:
		return val + mod
	else:
		return int(val/mod)*mod + mod

def indentLines (lines):
	tabSize = 4

	# Split into columns
	lineColumns = [line.split("\t") for line in lines if line is not None]
	if len(lineColumns) == 0:
		return

	numColumns = max(len(line) for line in lineColumns)

	# Figure out max length per column
	columnLengths = [nextMod(max(len(line[ndx]) for line in lineColumns if len(line) > ndx), tabSize) for ndx in range(numColumns)]

	for line in lineColumns:
		indented = []
		for columnNdx, col in enumerate(line[:-1]):
			colLen	= len(col)
			while colLen < columnLengths[columnNdx]:
				col		+= "\t"
				colLen	 = nextMod(colLen, tabSize)
			indented.append(col)

		# Append last col
		indented.append(line[-1])
		yield "".join(indented)

def normalizeConstant(constant):
	value = int(constant, base=0)
	if value >= 1 << 63:
		suffix = 'ull'
	elif value >= 1 << 32:
		suffix = 'll'
	elif value >= 1 << 31:
		suffix = 'u'
	else:
		suffix = ''
	return constant + suffix

def commandParams(command):
	if len(command.params) > 0:
		return ", ".join(param.declaration for param in command.params)
	else:
		return "void"

def commandArgs(command):
	return ", ".join(param.name for param in command.params)

def getIncludeGuardName (filename):
	filename = filename.upper()
	filename = filename.replace('.', '_')
	return "_%s" % filename

def writeFile (filename, lines):
	with open(filename, 'wb') as f:
		for line in lines:
			if line is not None:
				f.write(line)
				f.write('\n')
	print filename

def writeSrcFile (filename, description, source):
	includeGuard = getIncludeGuardName(os.path.basename(filename))

	header = SRC_HEADER.format(INCLUDE_GUARD=includeGuard, DESCRIPTION=description)
	footer = SRC_FOOTER.format(INCLUDE_GUARD=includeGuard)
	writeFile(filename, chain([header], lines, [footer]))

def writeInlFile (filename, source):
	includeGuard = getIncludeGuardName(os.path.basename(filename))
	writeFile(filename, chain([INL_HEADER], source))

def computeChecksum (data):
	return hashlib.sha256(data).hexdigest()

def fetchUrl (url):
	req		= urllib2.urlopen(url)
	data	= req.read()
	return data

def fetchSrc ():
	def writeFile (filename, data):
		f = open(filename, 'wb')
		f.write(data)
		f.close()

	if not os.path.exists(os.path.dirname(GL_XML)):
		os.makedirs(os.path.dirname(GL_XML))

	fullUrl = "%s?r=%d" % (SRC_URL, SRC_REVISION)

	print "Fetching %s" % fullUrl
	data		= fetchUrl(fullUrl)
	checksum	= computeChecksum(data)

	if checksum != SRC_CHECKSUM:
		raise Exception("Checksum mismatch, exepected %s, got %s" % (SRC_CHECKSUM, checksum))

	writeFile(GL_XML, data)

def checkSrc ():
	def readFile (filename):
		f = open(filename, 'rb')
		data = f.read()
		f.close()
		return data

	if os.path.exists(GL_XML):
		return computeChecksum(readFile(GL_XML)) == SRC_CHECKSUM
	else:
		return False

def updateSrc ():
	if not checkSrc():
		fetchSrc()
