# -*- coding: utf-8 -*-

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
