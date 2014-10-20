# -*- coding: utf-8 -*-

import os
import re
import sys
import shutil
import argparse

import common

BASE_LIBS_DIR = os.path.join(common.ANDROID_DIR, "package", "libs")

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

def getAssetsDir (nativeLib, buildType):
	return os.path.join(getNativeBuildDir(nativeLib, buildType), "assets")

def getPrebuiltsDirName (abiName):
	PREBUILT_DIRS = {
			'x86':			'android-x86',
			'armeabi-v7a':	'android-arm',
			'arm64-v8a':	'android-arm64'
		}

	if not abiName in PREBUILT_DIRS:
		raise Exception("Unknown ABI %s, don't know where prebuilts are" % abiName)

	return PREBUILT_DIRS[abiName]

def buildNative (nativeLib, buildType):
	deqpDir		= os.path.normpath(os.path.join(common.ANDROID_DIR, ".."))
	buildDir	= getNativeBuildDir(nativeLib, buildType)
	libsDir		= os.path.join(BASE_LIBS_DIR, nativeLib.abiVersion)
	srcLibFile	= os.path.join(buildDir, common.NATIVE_LIB_NAME)
	dstLibFile	= os.path.join(libsDir, common.NATIVE_LIB_NAME)

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
	common.execArgs(['cmake', '--build', '.'] + common.EXTRA_BUILD_ARGS)

	if not os.path.exists(libsDir):
		os.makedirs(libsDir)

	shutil.copyfile(srcLibFile, dstLibFile)

	# Copy gdbserver for debugging
	if buildType.lower() == "debug":
		srcGdbserverPath	= os.path.join(common.ANDROID_NDK_PATH, 'prebuilt', getPrebuiltsDirName(nativeLib.abiVersion), 'gdbserver', 'gdbserver')
		dstGdbserverPath	= os.path.join(libsDir, 'gdbserver')
		shutil.copyfile(srcGdbserverPath, dstGdbserverPath)
	else:
		assert not os.path.exists(os.path.join(libsDir, "gdbserver"))

def buildApp (isRelease):
	appDir	= os.path.join(common.ANDROID_DIR, "package")

	# Set up app
	os.chdir(appDir)
	common.execArgs([
			common.ANDROID_BIN,
			'update', 'project',
			'--name', 'dEQP',
			'--path', '.',
			'--target', str(common.ANDROID_JAVA_API),
		])

	# Build
	common.execArgs([common.ANT_BIN, "release" if isRelease else "debug"])

def signApp (keystore, keyname, storepass, keypass):
	os.chdir(os.path.join(common.ANDROID_DIR, "package"))
	common.execArgs([
			common.JARSIGNER_BIN,
			'-keystore', keystore,
			'-storepass', storepass,
			'-keypass', keypass,
			'-sigfile', 'CERT',
			'-digestalg', 'SHA1',
			'-sigalg', 'MD5withRSA',
			'-signedjar', 'bin/dEQP-unaligned.apk',
			'bin/dEQP-release-unsigned.apk',
			keyname
		])
	common.execArgs([
			common.ZIPALIGN_BIN,
			'-f', '4',
			'bin/dEQP-unaligned.apk',
			'bin/dEQP-release.apk'
		])

def build (isRelease=False, nativeBuildType="Release"):
	curDir = os.getcwd()

	try:
		assetsSrcDir	= getAssetsDir(common.NATIVE_LIBS[0], nativeBuildType)
		assetsDstDir	= os.path.join(common.ANDROID_DIR, "package", "assets")

		# Remove assets from the first build dir where we copy assets from
		# to avoid collecting cruft there.
		if os.path.exists(assetsSrcDir):
			shutil.rmtree(assetsSrcDir)
		if os.path.exists(assetsDstDir):
			shutil.rmtree(assetsDstDir)

		# Remove old libs dir to avoid collecting out-of-date versions
		# of libs for ABIs not built this time.
		if os.path.exists(BASE_LIBS_DIR):
			shutil.rmtree(BASE_LIBS_DIR)

		# Build native code
		for lib in common.NATIVE_LIBS:
			buildNative(lib, nativeBuildType)

		# Copy assets
		if os.path.exists(assetsSrcDir):
			shutil.copytree(assetsSrcDir, assetsDstDir)

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
