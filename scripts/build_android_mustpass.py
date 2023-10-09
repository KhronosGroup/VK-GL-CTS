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

from ctsbuild.common import DEQP_DIR
from ctsbuild.config import ANY_GENERATOR
from build_caselists import Module, getModuleByName, getBuildConfig, DEFAULT_BUILD_DIR, DEFAULT_TARGET
from mustpass import Project, Package, Mustpass, Configuration, include, exclude, genMustpassLists, parseBuildConfigFromCmdLineArgs

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
VULKANSC_MODULE					= getModuleByName("dEQP-VKSC")

# Main

MAIN_EGL_COMMON_FILTERS		= [include("egl-master.txt"),
								   exclude("egl-test-issues.txt"),
								   exclude("egl-manual-robustness.txt"),
								   exclude("egl-driver-issues.txt"),
								   exclude("egl-temp-excluded.txt")]
MAIN_EGL_PKG					= Package(module = EGL_MODULE, configurations = [
		# Main
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  required		= True,
					  filters		= MAIN_EGL_COMMON_FILTERS,
					  runtime		= "23m",
					  runByDefault	= False),
		Configuration(name			= "master-2020-03-01",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  required		= True,
					  filters		= [include("egl-master-2020-03-01.txt")],
					  runtime		= "23m"),
		Configuration(name			= "master-2022-03-01",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  required		= True,
					  filters		= MAIN_EGL_COMMON_FILTERS + [exclude("egl-master-2021-03-01.txt")],
					  runtime		= "5m"),
		# Risky subset
		Configuration(name			= "master-risky",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  required		= True,
					  filters		= [include("egl-temp-excluded.txt")],
					  runtime		= "2m"),
	])

MAIN_GLES2_COMMON_FILTERS		= [
		include("gles2-master.txt"),
		exclude("gles2-test-issues.txt"),
		exclude("gles2-failures.txt"),
		exclude("gles2-temp-excluded.txt"),
	]
MAIN_GLES2_PKG				= Package(module = GLES2_MODULE, configurations = [
		# Main
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  required		= True,
					  filters		= MAIN_GLES2_COMMON_FILTERS,
					  runtime		= "46m",
					  runByDefault		= False),
		Configuration(name			= "master-2020-03-01",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  required		= True,
					  filters		= [include("gles2-master-2020-03-01.txt")],
					  runtime		= "46m"),
		Configuration(name			= "master-2021-03-01",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  required		= True,
					  filters		= [include("gles2-master-2021-03-01.txt")],
					  runtime		= "10m"),
		Configuration(name			= "master-2022-03-01",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  required		= True,
					  filters		= MAIN_GLES2_COMMON_FILTERS + [exclude("gles2-master-2020-03-01.txt"), exclude("gles2-master-2021-03-01.txt")],
					  runtime		= "10m"),
	])

MAIN_GLES3_COMMON_FILTERS		= [
		include("gles3-master.txt"),
		exclude("gles3-hw-issues.txt"),
		exclude("gles3-driver-issues.txt"),
		exclude("gles3-test-issues.txt"),
		exclude("gles3-spec-issues.txt"),
		exclude("gles3-temp-excluded.txt"),
		exclude("gles3-waivers.txt"),
	]
MAIN_GLES3_PKG				= Package(module = GLES3_MODULE, configurations = [
		# Main
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  required		= True,
					  filters		= MAIN_GLES3_COMMON_FILTERS,
					  runtime		= "1h50m",
					  runByDefault	= False),
		Configuration(name			= "master-2020-03-01",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  required		= True,
					  filters		= [include("gles3-master-2020-03-01.txt")],
					  runtime		= "1h50m"),
		Configuration(name			= "master-2021-03-01",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  required		= True,
					  filters		= [include("gles3-master-2021-03-01.txt")],
					  runtime		= "10m"),
		Configuration(name			= "master-2022-03-01",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  required		= True,
					  filters		= MAIN_GLES3_COMMON_FILTERS + [exclude("gles3-master-2020-03-01.txt"), exclude("gles3-master-2021-03-01.txt")],
					  runtime		= "10m"),
		# Rotations
		Configuration(name			= "rotate-portrait",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "0",
					  surfacetype	= "window",
					  filters		= MAIN_GLES3_COMMON_FILTERS + [include("gles3-rotation.txt")],
					  runtime		= "1m"),
		Configuration(name			= "rotate-landscape",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "90",
					  surfacetype	= "window",
					  filters		= MAIN_GLES3_COMMON_FILTERS + [include("gles3-rotation.txt")],
					  runtime		= "1m"),
		Configuration(name			= "rotate-reverse-portrait",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "180",
					  surfacetype	= "window",
					  filters		= MAIN_GLES3_COMMON_FILTERS + [include("gles3-rotation.txt")],
					  runtime		= "1m"),
		Configuration(name			= "rotate-reverse-landscape",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "270",
					  surfacetype	= "window",
					  filters		= MAIN_GLES3_COMMON_FILTERS + [include("gles3-rotation.txt")],
					  runtime		= "1m"),

		# MSAA
		Configuration(name			= "multisample",
					  glconfig		= "rgba8888d24s8ms4",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= MAIN_GLES3_COMMON_FILTERS + [include("gles3-multisample.txt"),
																	 exclude("gles3-multisample-issues.txt")],
					  runtime		= "1m"),

		# Pixel format
		Configuration(name			= "565-no-depth-no-stencil",
					  glconfig		= "rgb565d0s0ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= MAIN_GLES3_COMMON_FILTERS + [include("gles3-pixelformat.txt"),
																	 exclude("gles3-pixelformat-issues.txt")],
					  runtime		= "1m"),
		# Incremental dEQP
		Configuration(name			= "incremental-deqp",
					  filters		= [include("gles3-incremental-deqp.txt")],
					  runtime		= "5m",
					  runByDefault	= False),
	])

MAIN_GLES31_COMMON_FILTERS	= [
		include("gles31-master.txt"),
		exclude("gles31-hw-issues.txt"),
		exclude("gles31-driver-issues.txt"),
		exclude("gles31-test-issues.txt"),
		exclude("gles31-spec-issues.txt"),
		exclude("gles31-temp-excluded.txt"),
		exclude("gles31-waivers.txt"),
	]
MAIN_GLES31_PKG				= Package(module = GLES31_MODULE, configurations = [
		# Main
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  required		= True,
					  filters		= MAIN_GLES31_COMMON_FILTERS,
					  runtime		= "1h40m",
					  runByDefault		= False),
		Configuration(name			= "master-2020-03-01",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  required		= True,
					  filters		= [include("gles31-master-2020-03-01.txt")],
					  runtime		= "1h40m"),
		Configuration(name			= "master-2021-03-01",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  required		= True,
					  filters		= [include("gles31-master-2021-03-01.txt")],
					  runtime		= "10m"),
		Configuration(name			= "master-2022-03-01",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  required		= True,
					  filters		= MAIN_GLES31_COMMON_FILTERS + [exclude("gles31-master-2020-03-01.txt"), exclude("gles31-master-2021-03-01.txt")],
					  runtime		= "10m"),

		# Rotations
		Configuration(name			= "rotate-portrait",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "0",
					  surfacetype	= "window",
					  filters		= MAIN_GLES31_COMMON_FILTERS + [include("gles31-rotation.txt")],
					  runtime		= "1m30s"),
		Configuration(name			= "rotate-landscape",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "90",
					  surfacetype	= "window",
					  filters		= MAIN_GLES31_COMMON_FILTERS + [include("gles31-rotation.txt")],
					  runtime		= "1m30s"),
		Configuration(name			= "rotate-reverse-portrait",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "180",
					  surfacetype	= "window",
					  filters		= MAIN_GLES31_COMMON_FILTERS + [include("gles31-rotation.txt")],
					  runtime		= "1m30s"),
		Configuration(name			= "rotate-reverse-landscape",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "270",
					  surfacetype	= "window",
					  filters		= MAIN_GLES31_COMMON_FILTERS + [include("gles31-rotation.txt")],
					  runtime		= "1m30s"),

		# MSAA
		Configuration(name			= "multisample",
					  glconfig		= "rgba8888d24s8ms4",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= MAIN_GLES31_COMMON_FILTERS + [include("gles31-multisample.txt")],
					  runtime		= "2m"),

		# Pixel format
		Configuration(name			= "565-no-depth-no-stencil",
					  glconfig		= "rgb565d0s0ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= MAIN_GLES31_COMMON_FILTERS + [include("gles31-pixelformat.txt")],
					  runtime		= "1m"),
	])

MAIN_VULKAN_FILTERS			= [
		include("vk-master.txt"),
		exclude("vk-not-applicable.txt"),
		exclude("vk-excluded-tests.txt"),
		exclude("vk-test-issues.txt"),
		exclude("vk-waivers.txt"),
		exclude("vk-temp-excluded.txt"),
	]
MAIN_VULKAN_PKG				= Package(module = VULKAN_MODULE, configurations = [
		Configuration(name					= "master",
					  filters				= MAIN_VULKAN_FILTERS,
					  runtime				= "2h39m",
					  runByDefault			= False,
					  listOfGroupsToSplit	= ["dEQP-VK", "dEQP-VK.pipeline", "dEQP-VK.image", "dEQP-VK.shader_object"]),
		Configuration(name					= "master-2019-03-01",
					  filters				= [include("vk-master-2019-03-01.txt")],
					  runtime				= "2h29m",
					  listOfGroupsToSplit	= ["dEQP-VK"]),
		Configuration(name					= "master-2020-03-01",
					  filters				= [include("vk-master-2020-03-01.txt")],
					  runtime				= "2h29m",
					  listOfGroupsToSplit	= ["dEQP-VK"]),
		Configuration(name					= "master-2021-03-01",
					  filters				= [include("vk-master-2021-03-01.txt")],
					  runtime				= "2h29m",
					  listOfGroupsToSplit	= ["dEQP-VK"]),
		Configuration(name					= "master-2022-03-01",
					  filters				= MAIN_VULKAN_FILTERS + [exclude("vk-master-2019-03-01.txt"), exclude("vk-master-2020-03-01.txt"), exclude("vk-master-2021-03-01.txt")],
					  runtime				= "10m",
					  listOfGroupsToSplit	= ["dEQP-VK", "dEQP-VK.pipeline", "dEQP-VK.image", "dEQP-VK.shader_object"]),
		Configuration(name					= "incremental-deqp",
					  filters				= [include("vk-incremental-deqp.txt")],
					  runtime				= "5m",
					  runByDefault			= False,
					  listOfGroupsToSplit	= ["dEQP-VK"]),
	])

MAIN_VULKANSC_FILTERS			= [
		include("vksc-master.txt"),
	]
MAIN_VULKANSC_PKG				= Package(module = VULKANSC_MODULE, configurations = [
		Configuration(name					= "main",
					  filters				= MAIN_VULKANSC_FILTERS,
					  runtime				= "2h39m",
					  runByDefault			= False,
					  listOfGroupsToSplit	= ["dEQP-VKSC", "dEQP-VKSC.pipeline", "dEQP-VKSC.image", "dEQP-VKSC.shader_object"]),
	])

MUSTPASS_LISTS				= [
		Mustpass(project = CTS_PROJECT, version = "main",		packages = [MAIN_EGL_PKG, MAIN_GLES2_PKG, MAIN_GLES3_PKG, MAIN_GLES31_PKG, MAIN_VULKAN_PKG, MAIN_VULKANSC_PKG])
	]

if __name__ == "__main__":
	genMustpassLists(MUSTPASS_LISTS, ANY_GENERATOR, parseBuildConfigFromCmdLineArgs())
