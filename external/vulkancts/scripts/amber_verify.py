# -*- coding: utf-8 -*-

#-------------------------------------------------------------------------
# Vulkan CTS
# ----------
#
# Copyright (c) 2022 Google LLC.
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
import sys
import argparse
import tempfile

scriptPath = os.path.join(os.path.dirname(__file__), "..", "..", "..", "scripts")
sys.path.insert(0, scriptPath)

from ctsbuild.common import *
from ctsbuild.config import *
from ctsbuild.build import *

class Module:
	def __init__ (self, name, dirName, binName):
		self.name		= name
		self.dirName	= dirName
		self.binName	= binName

VULKAN_MODULE		= Module("dEQP-VK", "../external/vulkancts/modules/vulkan", "deqp-vk")
DEFAULT_BUILD_DIR	= os.path.join(tempfile.gettempdir(), "amber-verify", "{targetName}-{buildType}")
DEFAULT_TARGET		= "null"
DEFAULT_DST_DIR		= os.path.join(DEQP_DIR, "external", "vulkancts", "data", "vulkan", "prebuilt")

def getBuildConfig (buildPathPtrn, targetName, buildType):
	buildPath = buildPathPtrn.format(
		targetName	= targetName,
		buildType	= buildType)

	return BuildConfig(buildPath, buildType, [f"-DDEQP_TARGET={targetName}"])

def execBuildPrograms (buildCfg, generator, module):
	workDir		= os.path.join(buildCfg.getBuildDir(), "modules", module.dirName)

	pushWorkingDir(workDir)

	try:
		binPath = generator.getBinaryPath(buildCfg.getBuildType(), os.path.join(".", "deqp-vk"))
		execute([binPath, "--deqp-runmode=amber-verify"])
	finally:
		popWorkingDir()

def parseArgs ():
	parser = argparse.ArgumentParser(description = "Verify amber device requirements between CTS and .amber file",
									 formatter_class=argparse.ArgumentDefaultsHelpFormatter)
	parser.add_argument("-b",
						"--build-dir",
						dest="buildDir",
						default=DEFAULT_BUILD_DIR,
						help="Temporary build directory")
	parser.add_argument("-t",
						"--build-type",
						dest="buildType",
						default="Release",
						help="Build type")
	parser.add_argument("-c",
						"--deqp-target",
						dest="targetName",
						default=DEFAULT_TARGET,
						help="dEQP build target")
	parser.add_argument("-d",
						"--dst-path",
						dest="dstPath",
						default=DEFAULT_DST_DIR,
						help="Destination path")
	return parser.parse_args()

if __name__ == "__main__":
	args = parseArgs()

	generator	= ANY_GENERATOR
	buildCfg	= getBuildConfig(args.buildDir, args.targetName, args.buildType)
	module		= VULKAN_MODULE

	build(buildCfg, generator, ["deqp-vk"])

	if not os.path.exists(args.dstPath):
		os.makedirs(args.dstPath)

	execBuildPrograms(buildCfg, generator, module)
