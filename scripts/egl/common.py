# -*- coding: utf-8 -*-

import os
import re
import sys

def registerPaths():
	sys.path.append(os.path.dirname(os.path.dirname(__file__)))

registerPaths()

import khr_util.format
import khr_util.registry
import khr_util.registry_cache

SCRIPTS_DIR			= os.path.dirname(__file__)
EGL_DIR				= os.path.normpath(os.path.join(SCRIPTS_DIR, "..", "..", "framework", "egl"))

EGL_SOURCE			= khr_util.registry_cache.RegistrySource(
						"egl.xml",
						28861,
						"0e7e6381c4e518f915450fe5080c9b1307cbf3548999a74e2b7676de7b5e5a30")

EXTENSIONS			= [
	"EGL_KHR_create_context",
	"EGL_KHR_lock_surface"
]

def getEGLRegistry ():
	return khr_util.registry_cache.getRegistry(EGL_SOURCE)

def getInterface (registry, api, version=None, profile=None, **kwargs):
	spec = khr_util.registry.spec(registry, api, version, profile, **kwargs)
	return khr_util.registry.createInterface(registry, spec, api)

def getDefaultInterface ():
	return getInterface(getEGLRegistry(), 'egl', '1.4', extensionNames = EXTENSIONS)

INL_HEADER = khr_util.format.genInlHeader("Khronos EGL API description (egl.xml)", EGL_SOURCE.getRevision())

def writeInlFile (filename, source):
	khr_util.format.writeInlFile(filename, INL_HEADER, source)
