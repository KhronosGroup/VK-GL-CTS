# -*- coding: utf-8 -*-

import os
import string

from common import *
from opengl.src_util import getGLRegistry
from itertools import chain

import khr_util.registry
from khr_util.format import indentLines

def toCamelCase (extName):
	return "".join([x.title() for x in extName.split("_")])

def makeStringList (name, strings):
	yield ""
	yield "static const char* s_%s[] =" % name
	yield "{"

	for entry in strings:
		yield "\t\"%s\"," % (entry)

	yield "};"

def makeFunctionList (name, iface):
	return makeStringList(name, [command.name for command in iface.commands])

def makeExtensionList (extensions):
	for name, iface in extensions:
		for line in makeFunctionList(name, iface):
			yield line

	yield ""
	yield "static const struct"
	yield "{"
	yield "\tconst char*\t\t\tname;"
	yield "\tconst int\t\t\tnumFunctions;"
	yield "\tconst char* const*\tfunctions;"
	yield "} s_extensions[] ="
	yield "{"

	entries = []
	for name, iface in extensions:
		entries.append("\t{ \"%s\",\tDE_LENGTH_OF_ARRAY(s_%s),\ts_%s\t}," % (name, name, name))

	for line in indentLines(entries):
		yield line

	yield "};"

def getExtensionList (registry, api):
	exts = []

	for extension in registry.extensions:
		if not khr_util.registry.extensionSupports(extension, api):
			continue

		spec = khr_util.registry.InterfaceSpec()
		spec.addExtension(extension, api)
		iface = khr_util.registry.createInterface(registry, spec, api)

		if len(iface.commands) == 0:
			continue

		exts.append((khr_util.registry.getExtensionName(extension),
					 iface))

	return exts

def uniqueExtensions (extensions):
	res = []
	seen = set()

	for name, iface in extensions:
		if not name in seen:
			res.append((name, iface))
			seen.add(name)

	return res

def getInterfaceExactVersion (registry, api, version):
	spec = khr_util.registry.InterfaceSpec()

	def check (v): return v == version

	for feature in registry.getFeatures(api, check):
		spec.addFeature(feature, api)

	return khr_util.registry.createInterface(registry, spec, api)

def gen ():
	eglRegistry		= getEGLRegistry()
	eglCoreIface	= getInterface(eglRegistry, 'egl', '1.4')
	eglExtensions	= getExtensionList(eglRegistry, 'egl')

	glRegistry		= getGLRegistry()
	gles1Extensions	= getExtensionList(glRegistry, 'gles1')
	gles2Extensions	= getExtensionList(glRegistry, 'gles2')
	gles10CoreIface	= getInterface(glRegistry, 'gles1', '1.0')
	gles20CoreIface	= getInterface(glRegistry, 'gles2', '2.0')
	gles30CoreIface	= getInterfaceExactVersion(glRegistry, 'gles2', '3.0')
#	gles31CoreIface	= getInterfaceExactVersion(glRegistry, 'gles2', '3.1')

	allExtensions	= eglExtensions + uniqueExtensions(gles1Extensions + gles2Extensions)

	writeInlFile(os.path.normpath(os.path.join(SCRIPTS_DIR, "..", "..", "modules", "egl", "teglGetProcAddressTests.inl")),
				 chain(makeFunctionList		("EGL14",	eglCoreIface),
				 	   makeFunctionList		("GLES10",	gles10CoreIface),
				 	   makeFunctionList		("GLES20",	gles20CoreIface),
				 	   makeFunctionList		("GLES30",	gles30CoreIface),
#				 	   makeFunctionList		("GLES31",	gles31CoreIface),
				 	   makeExtensionList	(allExtensions)))
