# -*- coding: utf-8 -*-

import os
import re
import sys
import shutil
import argparse

import common

def getStoreKeyPasswords (filename):
	f			= open(filename)
	storepass	= None
	keypass		= None
	for line in f:
		m = re.search('([a-z]+)\s*\=\s*"([^"]+)"', line)
		if m != None:
			if m.group(1) == "storepass":
				storepass = m.group(2)
			elif m.group(1) == "keypass":
				keypass = m.group(2)
	f.close()
	if storepass == None or keypass == None:
		common.die("Could not read signing key passwords")
	return (storepass, keypass)

def getNativeBuildDir (nativeLib, buildType):
	deqpDir = os.path.normpath(os.path.join(common.ANDROID_DIR, ".."))
	return os.path.normpath(os.path.join(deqpDir, "android", "build", "%s-%d-%s" % (buildType.lower(), nativeLib.apiVersion, nativeLib.abiVersion)))

def buildNative (nativeLib, buildType):
	deqpDir		= os.path.normpath(os.path.join(common.ANDROID_DIR, ".."))
	buildDir	= getNativeBuildDir(nativeLib, buildType)
	assetsDir	= os.path.join(buildDir, "assets")
	libsDir		= os.path.join(common.ANDROID_DIR, "package", "libs", nativeLib.abiVersion)
	srcLibFile	= os.path.join(buildDir, "libtestercore.so")
	dstLibFile	= os.path.join(libsDir, "lib%s.so" % nativeLib.libName)

	# Remove old lib files if such exist
	if os.path.exists(srcLibFile):
		os.unlink(srcLibFile)

	if os.path.exists(dstLibFile):
		os.unlink(dstLibFile)

	# Remove assets directory so that we don't collect unnecessary cruft to the APK
	if os.path.exists(assetsDir):
		shutil.rmtree(assetsDir)

	# Make build directory if necessary
	if not os.path.exists(buildDir):
		os.makedirs(buildDir)
		os.chdir(buildDir)
		common.execArgs([
				'cmake',
				'-G%s' % common.CMAKE_GENERATOR,
				'-DCMAKE_TOOLCHAIN_FILE=%s/framework/delibs/cmake/toolchain-android-%s.cmake' % (deqpDir, common.ANDROID_NDK_TOOLCHAIN_VERSION),
				'-DANDROID_NDK_HOST_OS=%s' % common.ANDROID_NDK_HOST_OS,
				'-DANDROID_NDK_PATH=%s' % common.ANDROID_NDK_PATH,
				'-DANDROID_ABI=%s' % nativeLib.abiVersion,
				'-DDE_ANDROID_API=%s' % nativeLib.apiVersion,
				'-DCMAKE_BUILD_TYPE=%s' % buildType,
				'-DDEQP_TARGET=android',
				deqpDir
			])

	os.chdir(buildDir)
	common.execute(common.BUILD_CMD)

	if not os.path.exists(libsDir):
		os.makedirs(libsDir)

	# Copy libtestercore.so
	shutil.copyfile(srcLibFile, dstLibFile)

	# Copy gdbserver for debugging
	if buildType.lower() == "debug":
		if nativeLib.abiVersion == "x86":
			shutil.copyfile(os.path.join(common.ANDROID_NDK_PATH, "prebuilt/android-x86/gdbserver/gdbserver"), os.path.join(libsDir, "gdbserver"))
		elif nativeLib.abiVersion == "armeabi-v7a":
			shutil.copyfile(os.path.join(common.ANDROID_NDK_PATH, "prebuilt/android-arm/gdbserver/gdbserver"), os.path.join(libsDir, "gdbserver"))
		else:
			print("Unknown ABI. Won't copy gdbserver to package")
	elif os.path.exists(os.path.join(libsDir, "gdbserver")):
		# Make sure there is no gdbserver if build is not debug build
		os.unlink(os.path.join(libsDir, "gdbserver"))

def copyAssets (nativeLib, buildType):
	srcDir = os.path.join(getNativeBuildDir(nativeLib, buildType), "assets")
	dstDir = os.path.join(common.ANDROID_DIR, "package", "assets")

	if os.path.exists(dstDir):
		shutil.rmtree(dstDir)

	if os.path.exists(srcDir):
		shutil.copytree(srcDir, dstDir)

def fileContains (filename, str):
	f = open(filename, 'rb')
	data = f.read()
	f.close()

	return data.find(str) >= 0

def buildApp (isRelease):
	appDir	= os.path.join(common.ANDROID_DIR, "package")

	# Set up app
	os.chdir(appDir)
	common.execute("%s update project --name dEQP --path . --target %s" % (common.shellquote(common.ANDROID_BIN), common.ANDROID_JAVA_API))

	# Build
	common.execute("%s %s" % (common.shellquote(common.ANT_BIN), "release" if isRelease else "debug"))

def signApp (keystore, keyname, storepass, keypass):
	os.chdir(os.path.join(common.ANDROID_DIR, "package"))
	common.execute("%s -keystore %s -storepass %s -keypass %s -sigfile CERT -digestalg SHA1 -sigalg MD5withRSA -signedjar bin/dEQP-unaligned.apk bin/dEQP-release-unsigned.apk %s" % (common.shellquote(common.JARSIGNER_BIN), common.shellquote(keystore), storepass, keypass, keyname))
	common.execute("%s -f 4 bin/dEQP-unaligned.apk bin/dEQP-release.apk" % (common.shellquote(common.ZIPALIGN_BIN)))

def build (isRelease=False, nativeBuildType="Release"):
	curDir = os.getcwd()

	try:
		# Build native code
		for lib in common.NATIVE_LIBS:
			buildNative(lib, nativeBuildType)

		# Copy assets from first build dir
		copyAssets(common.NATIVE_LIBS[0], nativeBuildType)

		# Build java code and .apk
		buildApp(isRelease)

	finally:
		# Restore working dir
		os.chdir(curDir)

if __name__ == "__main__":
	parser = argparse.ArgumentParser()
	parser.add_argument('--is-release', dest='isRelease', type=bool, default=False, help="Build android project in release mode.")
	parser.add_argument('--native-build-type', dest='nativeBuildType', default="Release", help="Build type passed cmake when building native code.")

	args = parser.parse_args()

	build(isRelease=args.isRelease, nativeBuildType=args.nativeBuildType)
