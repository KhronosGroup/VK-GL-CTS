# -*- coding: utf-8 -*-

import os
import sys

from common import *
from config import *

def initBuildDir (config, generator):
	cfgArgs = []

	# Build base configuration args
	cfgArgs += config.getArgs()

	# Generator args
	cfgArgs += generator.getGenerateArgs(config.getBuildType())

	if not os.path.exists(config.buildDir):
		os.makedirs(config.buildDir)

	pushWorkingDir(config.getBuildDir())
	execute(["cmake", config.getSrcPath()] + cfgArgs)
	popWorkingDir()

def build (config, generator, targets = None):
	# Initialize or update build dir.
	initBuildDir(config, generator)

	baseCmd		= ['cmake', '--build', '.']
	buildArgs	= generator.getBuildArgs(config.getBuildType())

	pushWorkingDir(config.getBuildDir())

	if targets == None:
		execute(baseCmd + buildArgs)
	else:
		for target in targets:
			execute(baseCmd + ['--target', target] + buildArgs)

	popWorkingDir()
