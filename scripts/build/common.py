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
import shlex
import subprocess

SRC_BASE_DIR		= os.path.realpath(os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "..", "..")))
DEQP_DIR			= os.path.join(SRC_BASE_DIR, "deqp")

def die (msg):
	print msg
	exit(-1)

def shellquote(s):
	return '"%s"' % s.replace('\\', '\\\\').replace('"', '\"').replace('$', '\$').replace('`', '\`')

g_workDirStack = []

def pushWorkingDir (path):
	oldDir = os.getcwd()
	os.chdir(path)
	g_workDirStack.append(oldDir)

def popWorkingDir ():
	assert len(g_workDirStack) > 0
	newDir = g_workDirStack[-1]
	g_workDirStack.pop()
	os.chdir(newDir)

def execute (args):
	retcode	= subprocess.call(args)
	if retcode != 0:
		raise Exception("Failed to execute '%s', got %d" % (str(args), retcode))

def readFile (filename):
	f = open(filename, 'rb')
	data = f.read()
	f.close()
	return data

def writeFile (filename, data):
	f = open(filename, 'wb')
	f.write(data)
	f.close()

def which (binName):
	for path in os.environ['PATH'].split(os.pathsep):
		path = path.strip('"')
		fullPath = os.path.join(path, binName)
		if os.path.isfile(fullPath) and os.access(fullPath, os.X_OK):
			return fullPath

	return None
