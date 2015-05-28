# -*- coding: utf-8 -*-

#-------------------------------------------------------------------------
# drawElements Quality Program utilities
# --------------------------------------
#
# Copyright 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#-------------------------------------------------------------------------

import os
import re
import sys

sys.path.append(os.path.dirname(os.path.dirname(__file__)))

import khr_util.format
import khr_util.registry
import khr_util.registry_cache

SCRIPTS_DIR			= os.path.dirname(__file__)
OPENGL_DIR			= os.path.normpath(os.path.join(SCRIPTS_DIR, "..", "..", "framework", "opengl"))
EGL_DIR				= os.path.normpath(os.path.join(SCRIPTS_DIR, "..", "..", "framework", "egl"))
OPENGL_INC_DIR		= os.path.join(OPENGL_DIR, "wrapper")

GL_SOURCE			= khr_util.registry_cache.RegistrySource(
						"gl.xml",
						30159,
						"0af7e185d0db15e9f44a1b6ff6c72102f67509a8590f19a289b983d652008070")

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
	'GL_EXT_primitive_bounding_box',
	'GL_OES_EGL_image',
	'GL_OES_compressed_ETC1_RGB8_texture',
	'GL_OES_texture_half_float',
	'GL_OES_texture_storage_multisample_2d_array',
	'GL_OES_sample_shading',
	'GL_EXT_texture_compression_s3tc',
	'GL_IMG_texture_compression_pvrtc',
	'GL_EXT_copy_image',
	'GL_EXT_draw_buffers_indexed',
	'GL_EXT_texture_sRGB_decode',
	'GL_EXT_texture_border_clamp',
	'GL_EXT_texture_sRGB_R8',
	'GL_EXT_texture_sRGB_RG8',
	'GL_EXT_debug_marker',
]

def getGLRegistry ():
	return khr_util.registry_cache.getRegistry(GL_SOURCE)

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
	spec = khr_util.registry.InterfaceSpec()

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

	return khr_util.registry.createInterface(registry, spec, 'gles2')

def getInterface (registry, api, version=None, profile=None, **kwargs):
	spec = khr_util.registry.spec(registry, api, version, profile, **kwargs)
	if api == 'gl' and profile == 'core' and version < "3.2":
		gl32 = registry.features['GL_VERSION_3_2']
		for eRemove in gl32.xpath('remove'):
			spec.addComponent(eRemove)
	return khr_util.registry.createInterface(registry, spec, api)

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

INL_HEADER = khr_util.format.genInlHeader("Khronos GL API description (gl.xml)", GL_SOURCE.getRevision())

def writeInlFile (filename, source):
	khr_util.format.writeInlFile(filename, INL_HEADER, source)

# Aliases from khr_util.common
indentLines			= khr_util.format.indentLines
normalizeConstant	= khr_util.format.normalizeConstant
commandParams		= khr_util.format.commandParams
commandArgs			= khr_util.format.commandArgs
