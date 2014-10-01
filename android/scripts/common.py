# -*- coding: utf-8 -*-

import os
import sys
import shlex
import subprocess

class NativeLib:
	def __init__ (self, libName, apiVersion, abiVersion):
		self.libName		= libName
		self.apiVersion		= apiVersion
		self.abiVersion		= abiVersion

def getPlatform ():
	if sys.platform.startswith('linux'):
		return 'linux'
	else:
		return sys.platform

def getCfg (variants):
	platform = getPlatform()
	if platform in variants:
		return variants[platform]
	elif 'other' in variants:
		return variants['other']
	else:
		raise Exception("No configuration for '%s'" % platform)

def isExecutable (path):
	return os.path.isfile(path) and os.access(path, os.X_OK)

def which (binName):
	for path in os.environ['PATH'].split(os.pathsep):
		path = path.strip('"')
		fullPath = os.path.join(path, binName)
		if isExecutable(fullPath):
			return fullPath

	return None

def isBinaryInPath (binName):
	return which(binName) != None

def selectBin (basePaths, relBinPath):
	for basePath in basePaths:
		fullPath = os.path.normpath(os.path.join(basePath, relBinPath))
		if isExecutable(fullPath):
			return fullPath
	return which(os.path.basename(relBinPath))

def die (msg):
	print msg
	exit(-1)

def shellquote(s):
	return '"%s"' % s.replace('\\', '\\\\').replace('"', '\"').replace('$', '\$').replace('`', '\`')

def execute (commandLine):
	args	= shlex.split(commandLine)
	retcode	= subprocess.call(args)
	if retcode != 0:
		raise Exception("Failed to execute '%s', got %d" % (commandLine, retcode))

def execArgs (args):
	retcode	= subprocess.call(args)
	if retcode != 0:
		raise Exception("Failed to execute '%s', got %d" % (str(args), retcode))

# deqp/android path
ANDROID_DIR				= os.path.realpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

# Build configuration
NATIVE_LIBS				= [
		#		  library name		API		ABI
#		NativeLib("testercore",		13,		"armeabi"),			# ARM v5 ABI
		NativeLib("testercore",		13,		"armeabi-v7a"),		# ARM v7a ABI
		NativeLib("testercore",		13,		"x86"),				# x86
#		NativeLib("testercore",		21,		"arm64-v8a"),		# ARM64 v8a ABI
	]
ANDROID_JAVA_API		= "android-13"

# NDK paths
ANDROID_NDK_HOST_OS		= getCfg({
		'win32':	"windows",
		'darwin':	"darwin-x86",
		'linux':	"linux-x86"
	})
ANDROID_NDK_PATH		= getCfg({
		'win32':	"C:/android/android-ndk-r9d",
		'darwin':	os.path.expanduser("~/android-ndk-r9d"),
		'linux':	os.path.expanduser("~/android-ndk-r9d")
	})
ANDROID_NDK_TOOLCHAIN_VERSION = "clang-r9d" # Toolchain file is selected based on this

def getWin32Generator ():
	if which("jom.exe") != None:
		return "NMake Makefiles JOM"
	else:
		return "NMake Makefiles"

# Native code build settings
CMAKE_GENERATOR			= getCfg({
		'win32':	getWin32Generator(),
		'darwin':	"Unix Makefiles",
		'linux':	"Unix Makefiles"
	})
BUILD_CMD				= getCfg({
		'win32':	"cmake --build .",
		'darwin':	"cmake --build . -- -j 4",
		'linux':	"cmake --build . -- -j 4"
	})

# SDK paths
ANDROID_SDK_PATHS		= [
	"C:/android/android-sdk-windows",
	os.path.expanduser("~/android-sdk-mac_x86"),
	os.path.expanduser("~/android-sdk-linux")
	]
ANDROID_BIN				= getCfg({
		'win32':	selectBin(ANDROID_SDK_PATHS, "tools/android.bat"),
		'other':	selectBin(ANDROID_SDK_PATHS, "tools/android"),
	})
ADB_BIN					= getCfg({
		'win32':	selectBin(ANDROID_SDK_PATHS, "platform-tools/adb.exe"),
		'other':	selectBin(ANDROID_SDK_PATHS, "platform-tools/adb"),
	})
ZIPALIGN_BIN			= getCfg({
		'win32':	selectBin(ANDROID_SDK_PATHS, "tools/zipalign.exe"),
		'other':	selectBin(ANDROID_SDK_PATHS, "tools/zipalign"),
	})
JARSIGNER_BIN			= "jarsigner"

# Apache ant
ANT_PATHS				= [
	"C:/android/apache-ant-1.8.4",
	"C:/android/apache-ant-1.9.2",
	"C:/android/apache-ant-1.9.3",
	"C:/android/apache-ant-1.9.4",
	]
ANT_BIN					= getCfg({
		'win32':	selectBin(ANT_PATHS, "bin/ant.bat"),
		'other':	selectBin(ANT_PATHS, "bin/ant")
	})
