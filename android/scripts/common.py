# -*- coding: utf-8 -*-

import os
import re
import sys
import shlex
import subprocess
import multiprocessing

class NativeLib:
	def __init__ (self, apiVersion, abiVersion):
		self.apiVersion	= apiVersion
		self.abiVersion	= abiVersion

def getPlatform ():
	if sys.platform.startswith('linux'):
		return 'linux'
	else:
		return sys.platform

def selectByOS (variants):
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

def selectFirstExistingBinary (filenames):
	for filename in filenames:
		if filename != None and isExecutable(filename):
			return filename

	return None

def selectFirstExistingDir (paths):
	for path in paths:
		if path != None and os.path.isdir(path):
			return path

	return None

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

class Device:
	def __init__(self, serial, product, model, device):
		self.serial		= serial
		self.product	= product
		self.model		= model
		self.device		= device

	def __str__ (self):
		return "%s: {product: %s, model: %s, device: %s}" % (self.serial, self.product, self.model, self.device)

def getDevices (adb):
	proc = subprocess.Popen([adb, 'devices', '-l'], stdout=subprocess.PIPE)
	(stdout, stderr) = proc.communicate()

	if proc.returncode != 0:
		raise Exception("adb devices -l failed, got %d" % retcode)

	ptrn = re.compile(r'^([a-zA-Z0-9]+)\s+.*product:([^\s]+)\s+model:([^\s]+)\s+device:([^\s]+)')
	devices = []
	for line in stdout.splitlines()[1:]:
		if len(line.strip()) == 0:
			continue

		m = ptrn.match(line)
		if m == None:
			print "WARNING: Failed to parse device info '%s'" % line
			continue

		devices.append(Device(m.group(1), m.group(2), m.group(3), m.group(4)))

	return devices

def getWin32Generator ():
	if which("jom.exe") != None:
		return "NMake Makefiles JOM"
	else:
		return "NMake Makefiles"

def isNinjaSupported ():
	return which("ninja") != None

def getUnixGenerator ():
	if isNinjaSupported():
		return "Ninja"
	else:
		return "Unix Makefiles"

def getExtraBuildArgs (generator):
	if generator == "Unix Makefiles":
		return ["--", "-j%d" % multiprocessing.cpu_count()]
	else:
		return []

NDK_HOST_OS_NAMES = [
	"windows",
	"windows_x86-64",
	"darwin-x86",
	"darwin-x86-64",
	"linux-x86",
	"linux-x86_64"
]

def getNDKHostOsName (ndkPath):
	for name in NDK_HOST_OS_NAMES:
		if os.path.exists(os.path.join(ndkPath, "prebuilt", name)):
			return name

	raise Exception("Couldn't determine NDK host OS")

# deqp/android path
ANDROID_DIR				= os.path.realpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

# Build configuration
NATIVE_LIBS				= [
		#		  API		ABI
		NativeLib(13,		"armeabi-v7a"),		# ARM v7a ABI
		NativeLib(13,		"x86"),				# x86
		NativeLib(21,		"arm64-v8a"),		# ARM64 v8a ABI
	]
ANDROID_JAVA_API		= "android-13"
NATIVE_LIB_NAME			= "libdeqp.so"

# NDK paths
ANDROID_NDK_PATH		= selectFirstExistingDir([
		os.path.expanduser("~/android-ndk-r10c"),
		"C:/android/android-ndk-r10c",
	])
ANDROID_NDK_HOST_OS				= getNDKHostOsName(ANDROID_NDK_PATH)
ANDROID_NDK_TOOLCHAIN_VERSION	= "r10c" # Toolchain file is selected based on this

# Native code build settings
CMAKE_GENERATOR			= selectByOS({
		'win32':	getWin32Generator(),
		'other':	getUnixGenerator()
	})
EXTRA_BUILD_ARGS		= getExtraBuildArgs(CMAKE_GENERATOR)

# SDK paths
ANDROID_SDK_PATH		= selectFirstExistingDir([
		os.path.expanduser("~/android-sdk-linux"),
		os.path.expanduser("~/android-sdk-mac_x86"),
		"C:/android/android-sdk-windows",
	])
ANDROID_BIN				= selectFirstExistingBinary([
		os.path.join(ANDROID_SDK_PATH, "tools", "android"),
		os.path.join(ANDROID_SDK_PATH, "tools", "android.bat"),
		which('android'),
	])
ADB_BIN					= selectFirstExistingBinary([
		which('adb'), # \note Prefer adb in path to avoid version issues on dev machines
		os.path.join(ANDROID_SDK_PATH, "platform-tools", "adb"),
		os.path.join(ANDROID_SDK_PATH, "platform-tools", "adb.exe"),
	])
ZIPALIGN_BIN			= selectFirstExistingBinary([
		os.path.join(ANDROID_SDK_PATH, "tools", "zipalign"),
		os.path.join(ANDROID_SDK_PATH, "tools", "zipalign.exe"),
		which('zipalign'),
	])
JARSIGNER_BIN			= which('jarsigner')

# Apache ant
ANT_BIN					= selectFirstExistingBinary([
		which('ant'),
		"C:/android/apache-ant-1.8.4/bin/ant.bat",
		"C:/android/apache-ant-1.9.2/bin/ant.bat",
		"C:/android/apache-ant-1.9.3/bin/ant.bat",
		"C:/android/apache-ant-1.9.4/bin/ant.bat",
	])
