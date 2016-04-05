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

from build.common import DEQP_DIR
from build.config import ANY_GENERATOR
from build_caselists import Module, getModuleByName, getBuildConfig, DEFAULT_BUILD_DIR, DEFAULT_TARGET
from mustpass import Project, Package, Mustpass, Configuration, include, exclude, genMustpassLists

import os

COPYRIGHT_DECLARATION = """
     Copyright (C) 2016 The Android Open Source Project

     Licensed under the Apache License, Version 2.0 (the "License");
     you may not use this file except in compliance with the License.
     You may obtain a copy of the License at

          http://www.apache.org/licenses/LICENSE-2.0

     Unless required by applicable law or agreed to in writing, software
     distributed under the License is distributed on an "AS IS" BASIS,
     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     See the License for the specific language governing permissions and
     limitations under the License.
     """

CTS_DATA_DIR					= os.path.join(DEQP_DIR, "android", "cts")

CTS_PROJECT						= Project(path = CTS_DATA_DIR, copyright = COPYRIGHT_DECLARATION)

EGL_MODULE						= getModuleByName("dEQP-EGL")
GLES2_MODULE					= getModuleByName("dEQP-GLES2")
GLES3_MODULE					= getModuleByName("dEQP-GLES3")
GLES31_MODULE					= getModuleByName("dEQP-GLES31")
VULKAN_MODULE					= getModuleByName("dEQP-VK")

# Lollipop

LMP_GLES3_PKG					= Package(module = GLES3_MODULE, configurations = [
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= [include("es30-lmp.txt")]),
	])
LMP_GLES31_PKG					= Package(module = GLES31_MODULE, configurations = [
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= [include("es31-lmp.txt")]),
	])

# Lollipop MR1

LMP_MR1_GLES3_PKG				= Package(module = GLES3_MODULE, configurations = [
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= [include("es30-lmp-mr1.txt")]),
	])
LMP_MR1_GLES31_PKG				= Package(module = GLES31_MODULE, configurations = [
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= [include("es31-lmp-mr1.txt")]),
	])

# Marshmallow

MNC_EGL_PKG						= Package(module = EGL_MODULE, configurations = [
		# Master
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= [include("egl-master.txt")]),
	])
MNC_GLES2_PKG					= Package(module = GLES2_MODULE, configurations = [
		# Master
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= [include("gles2-master.txt")]),
	])
MNC_GLES3_PKG					= Package(module = GLES3_MODULE, configurations = [
		# Master
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= [include("gles3-master.txt")]),
		# Rotations
		Configuration(name			= "rotate-portrait",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "0",
					  surfacetype	= "window",
					  filters		= [include("gles3-master.txt"), include("gles3-rotation.txt")]),
		Configuration(name			= "rotate-landscape",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "90",
					  surfacetype	= "window",
					  filters		= [include("gles3-master.txt"), include("gles3-rotation.txt")]),
		Configuration(name			= "rotate-reverse-portrait",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "180",
					  surfacetype	= "window",
					  filters		= [include("gles3-master.txt"), include("gles3-rotation.txt")]),
		Configuration(name			= "rotate-reverse-landscape",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "270",
					  surfacetype	= "window",
					  filters		= [include("gles3-master.txt"), include("gles3-rotation.txt")]),

		# MSAA
		Configuration(name			= "multisample",
					  glconfig		= "rgba8888d24s8ms4",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= [include("gles3-master.txt"),
									   include("gles3-multisample.txt"),
									   exclude("gles3-multisample-issues.txt")]),

		# Pixel format
		Configuration(name			= "565-no-depth-no-stencil",
					  glconfig		= "rgb565d0s0ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= [include("gles3-master.txt"),
									   include("gles3-pixelformat.txt"),
									   exclude("gles3-pixelformat-issues.txt")]),
	])
MNC_GLES31_PKG					= Package(module = GLES31_MODULE, configurations = [
		# Master
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= [include("gles31-master.txt")]),

		# Rotations
		Configuration(name			= "rotate-portrait",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "0",
					  surfacetype	= "window",
					  filters		= [include("gles31-master.txt"), include("gles31-rotation.txt")]),
		Configuration(name			= "rotate-landscape",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "90",
					  surfacetype	= "window",
					  filters		= [include("gles31-master.txt"), include("gles31-rotation.txt")]),
		Configuration(name			= "rotate-reverse-portrait",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "180",
					  surfacetype	= "window",
					  filters		= [include("gles31-master.txt"), include("gles31-rotation.txt")]),
		Configuration(name			= "rotate-reverse-landscape",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "270",
					  surfacetype	= "window",
					  filters		= [include("gles31-master.txt"), include("gles31-rotation.txt")]),

		# MSAA
		Configuration(name			= "multisample",
					  glconfig		= "rgba8888d24s8ms4",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= [include("gles31-master.txt"), include("gles31-multisample.txt")]),

		# Pixel format
		Configuration(name			= "565-no-depth-no-stencil",
					  glconfig		= "rgb565d0s0ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= [include("gles31-master.txt"), include("gles31-pixelformat.txt")]),
	])

# Master

MASTER_EGL_COMMON_FILTERS		= [include("egl-master.txt"),
								   exclude("egl-test-issues.txt"),
								   exclude("egl-internal-api-tests.txt")]
MASTER_EGL_PKG					= Package(module = EGL_MODULE, configurations = [
		# Master
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= MASTER_EGL_COMMON_FILTERS,
				      runtime 		= "24m"),
	])

MASTER_GLES2_COMMON_FILTERS		= [
		include("gles2-master.txt"),
		exclude("gles2-test-issues.txt"),
		exclude("gles2-failures.txt")
	]
MASTER_GLES2_PKG				= Package(module = GLES2_MODULE, configurations = [
		# Master
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= MASTER_GLES2_COMMON_FILTERS,
					  runtime 		= "40m"),
	])

MASTER_GLES3_COMMON_FILTERS		= [
		include("gles3-master.txt"),
		exclude("gles3-hw-issues.txt"),
		exclude("gles3-driver-issues.txt"),
		exclude("gles3-test-issues.txt"),
		exclude("gles3-spec-issues.txt")
	]
MASTER_GLES3_PKG				= Package(module = GLES3_MODULE, configurations = [
		# Master
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= MASTER_GLES3_COMMON_FILTERS,
					  runtime		= "1h15m"),
		# Rotations
		Configuration(name			= "rotate-portrait",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "0",
					  surfacetype	= "window",
					  filters		= MASTER_GLES3_COMMON_FILTERS + [include("gles3-rotation.txt")],
					  runtime		= "7m"),
		Configuration(name			= "rotate-landscape",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "90",
					  surfacetype	= "window",
					  filters		= MASTER_GLES3_COMMON_FILTERS + [include("gles3-rotation.txt")],
					  runtime		= "7m"),
		Configuration(name			= "rotate-reverse-portrait",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "180",
					  surfacetype	= "window",
					  filters		= MASTER_GLES3_COMMON_FILTERS + [include("gles3-rotation.txt")],
					  runtime		= "7m"),
		Configuration(name			= "rotate-reverse-landscape",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "270",
					  surfacetype	= "window",
					  filters		= MASTER_GLES3_COMMON_FILTERS + [include("gles3-rotation.txt")],
					  runtime		= "7m"),

		# MSAA
		Configuration(name			= "multisample",
					  glconfig		= "rgba8888d24s8ms4",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= MASTER_GLES3_COMMON_FILTERS + [include("gles3-multisample.txt"),
																	 exclude("gles3-multisample-issues.txt")],
					  runtime		= "10m"),

		# Pixel format
		Configuration(name			= "565-no-depth-no-stencil",
					  glconfig		= "rgb565d0s0ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= MASTER_GLES3_COMMON_FILTERS + [include("gles3-pixelformat.txt"),
																	 exclude("gles3-pixelformat-issues.txt")],
					  runtime		= "10m"),
	])

MASTER_GLES31_COMMON_FILTERS	= [
		include("gles31-master.txt"),
		exclude("gles31-hw-issues.txt"),
		exclude("gles31-driver-issues.txt"),
		exclude("gles31-test-issues.txt"),
		exclude("gles31-spec-issues.txt"),
	]
MASTER_GLES31_PKG				= Package(module = GLES31_MODULE, configurations = [
		# Master
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= MASTER_GLES31_COMMON_FILTERS,
					  runtime 		= "7h30m"),

		# Rotations
		Configuration(name			= "rotate-portrait",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "0",
					  surfacetype	= "window",
					  filters		= MASTER_GLES31_COMMON_FILTERS + [include("gles31-rotation.txt")],
					  runtime		= "1m30s"),
		Configuration(name			= "rotate-landscape",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "90",
					  surfacetype	= "window",
					  filters		= MASTER_GLES31_COMMON_FILTERS + [include("gles31-rotation.txt")],
					  runtime		= "1m30s"),
		Configuration(name			= "rotate-reverse-portrait",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "180",
					  surfacetype	= "window",
					  filters		= MASTER_GLES31_COMMON_FILTERS + [include("gles31-rotation.txt")],
					  runtime		= "1m30s"),
		Configuration(name			= "rotate-reverse-landscape",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "270",
					  surfacetype	= "window",
					  filters		= MASTER_GLES31_COMMON_FILTERS + [include("gles31-rotation.txt")],
					  runtime		= "1m30s"),

		# MSAA
		Configuration(name			= "multisample",
					  glconfig		= "rgba8888d24s8ms4",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= MASTER_GLES31_COMMON_FILTERS + [include("gles31-multisample.txt")],
					  runtime		= "2m"),

		# Pixel format
		Configuration(name			= "565-no-depth-no-stencil",
					  glconfig		= "rgb565d0s0ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= MASTER_GLES31_COMMON_FILTERS + [include("gles31-pixelformat.txt")],
					  runtime		= "1m"),
	])

MASTER_VULKAN_FILTERS			= [
		include("vulkan-master.txt"),
		exclude("vulkan-not-applicable.txt"),
		exclude("vulkan-test-issues.txt"),
		exclude("vulkan-hw-issues.txt")
	]
MASTER_VULKAN_PKG				= Package(module = VULKAN_MODULE, configurations = [
		Configuration(name			= "master",
					  filters		= MASTER_VULKAN_FILTERS,
					  runtime		= "3h45m"),
	])

MUSTPASS_LISTS				= [
		Mustpass(project = CTS_PROJECT, version = "lmp",		packages = [LMP_GLES3_PKG, LMP_GLES31_PKG]),
		Mustpass(project = CTS_PROJECT, version = "lmp-mr1",	packages = [LMP_MR1_GLES3_PKG, LMP_MR1_GLES31_PKG]),
		Mustpass(project = CTS_PROJECT, version = "mnc",		packages = [MNC_EGL_PKG, MNC_GLES2_PKG, MNC_GLES3_PKG, MNC_GLES31_PKG]),
		Mustpass(project = CTS_PROJECT, version = "master",		packages = [MASTER_EGL_PKG, MASTER_GLES2_PKG, MASTER_GLES3_PKG, MASTER_GLES31_PKG, MASTER_VULKAN_PKG])
	]

BUILD_CONFIG				= getBuildConfig(DEFAULT_BUILD_DIR, DEFAULT_TARGET, "Debug")

if __name__ == "__main__":
	genMustpassLists(MUSTPASS_LISTS, ANY_GENERATOR, BUILD_CONFIG)
