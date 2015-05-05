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

def registerPaths():
	sys.path.append(os.path.dirname(os.path.dirname(__file__)))

registerPaths()

import khr_util.format
import khr_util.registry
import khr_util.registry_cache

SCRIPTS_DIR			= os.path.dirname(__file__)
EGL_DIR				= os.path.normpath(os.path.join(SCRIPTS_DIR, "..", "..", "framework", "egl"))
EGL_WRAPPER_DIR		= os.path.normpath(os.path.join(EGL_DIR, "wrapper"))

EGL_SOURCE			= khr_util.registry_cache.RegistrySource(
						"egl.xml",
						31042,
						"f5a731f46958a7cb6a5a96c811086fbaede9cc078541a26de009228eb089ae2c")

VERSION				= '1.5'

EXTENSIONS			= [
	# \todo [2014-12-05 pyry] Use 1.5 core functions/enums instead
	"EGL_KHR_create_context",
	"EGL_KHR_lock_surface",
	"EGL_KHR_image_base",
	"EGL_KHR_fence_sync",
	"EGL_KHR_reusable_sync",
	"EGL_KHR_wait_sync",
	"EGL_KHR_gl_texture_2D_image",
	"EGL_KHR_gl_texture_cubemap_image",
	"EGL_KHR_gl_renderbuffer_image",
	"EGL_KHR_gl_texture_3D_image",
	"EGL_EXT_create_context_robustness",
	"EGL_EXT_platform_base",
	"EGL_EXT_platform_x11",
	"EGL_ANDROID_image_native_buffer",
	"EGL_EXT_yuv_surface"
]
PROTECTS			= [
	"KHRONOS_SUPPORT_INT64"
]

def getEGLRegistry ():
	return khr_util.registry_cache.getRegistry(EGL_SOURCE)

def getInterface (registry, api, version=None, profile=None, **kwargs):
	spec = khr_util.registry.spec(registry, api, version, profile, **kwargs)
	return khr_util.registry.createInterface(registry, spec, api)

def getDefaultInterface ():
	return getInterface(getEGLRegistry(), 'egl', VERSION, extensionNames = EXTENSIONS, protects = PROTECTS)

def getFunctionTypeName (funcName):
	return "%sFunc" % funcName

def getFunctionMemberName (funcName):
	assert funcName[:3] == "egl"
	return "%c%s" % (funcName[3].lower(), funcName[4:])

def genCommandList (iface, renderCommand, directory, filename, align=False):
	lines = map(renderCommand, iface.commands)
	if align:
		lines = khr_util.format.indentLines(lines)
	writeInlFile(os.path.join(directory, filename), lines)

def getVersionToken (version):
	return version.replace(".", "")

def genCommandLists (registry, renderCommand, check, directory, filePattern, align=False):
	for eFeature in registry.features:
		api			= eFeature.get('api')
		version		= eFeature.get('number')
		profile		= check(api, version)
		if profile is True:
			profile = None
		elif profile is False:
			continue
		iface		= getInterface(registry, api, version=version, profile=profile)
		filename	= filePattern % getVersionToken(version)
		genCommandList(iface, renderCommand, directory, filename, align)

INL_HEADER = khr_util.format.genInlHeader("Khronos EGL API description (egl.xml)", EGL_SOURCE.getRevision())

def writeInlFile (filename, source):
	khr_util.format.writeInlFile(filename, INL_HEADER, source)
