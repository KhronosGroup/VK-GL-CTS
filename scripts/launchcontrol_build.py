# -*- coding: utf-8 -*-

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
