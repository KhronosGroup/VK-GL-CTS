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
from build.common import *
from build.build import *
from argparse import ArgumentParser
import multiprocessing

# This is a bit silly, but CMake needs to know the word width prior to
# parsing the project files, hence cannot use our own defines.
X86_64_ARGS = ["-DDE_CPU=DE_CPU_X86_64", "-DCMAKE_C_FLAGS=-m64", "-DCMAKE_CXX_FLAGS=-m64"]

BUILD_CONFIGS = {
	"gcc-x86_64-x11_glx":   X86_64_ARGS + ["-DDEQP_TARGET=x11_glx"],
	"clang-x86_64-x11_glx": X86_64_ARGS + ["-DDEQP_TARGET=x11_glx", "-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++"]
}

def buildWithMake (workingDir):
	pushWorkingDir(workingDir)
	# CMake docs advised this to be the best magic formula...
	threadCount = multiprocessing.cpu_count() + 1
	print "Invoke make with %d threads" % threadCount
	execute(["make", "-j%d" % threadCount])
	popWorkingDir()

def parseOptions ():
	parser = ArgumentParser()

	parser.add_argument("-d",
						"--build-dir",
						dest="buildDir",
						default="out",
						help="Temporary build directory")
	parser.add_argument("-c",
						"--config",
						dest="config",
						choices=BUILD_CONFIGS.keys(),
						required=True,
						help="Build configuration name")
	parser.add_argument("-t",
						"--build-type",
						dest="buildType",
						choices=["Debug", "Release"],
						default="Debug",
						help="Build type")
	return parser.parse_args()

if __name__ == "__main__":
	options = parseOptions()

	print "\n############################################################"
	print "# %s %s BUILD" % (options.config.upper(), options.buildType.upper())
	print "############################################################\n"

	buildDir = os.path.realpath(os.path.normpath(options.buildDir))
	config = BuildConfig(buildDir, options.buildType, BUILD_CONFIGS[options.config])
	initBuildDir(config, MAKEFILE_GENERATOR)
	buildWithMake(buildDir)

	print "\n--- BUILD SCRIPT COMPLETE"
