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

def getNativeBuildDir (buildRoot, nativeLib, buildType):
	buildName = "%s-%d-%s" % (buildType.lower(), nativeLib.apiVersion, nativeLib.abiVersion)
	return os.path.normpath(os.path.join(buildRoot, "native", buildName))

def getAssetsDir (buildRoot, nativeLib, buildType):
	return os.path.join(getNativeBuildDir(buildRoot, nativeLib, buildType), "assets")

def getPrebuiltsDirName (abiName):
	PREBUILT_DIRS = {
			'x86':			'android-x86',
			'armeabi-v7a':	'android-arm',
			'arm64-v8a':	'android-arm64'
		}

	if not abiName in PREBUILT_DIRS:
		raise Exception("Unknown ABI %s, don't know where prebuilts are" % abiName)

	return PREBUILT_DIRS[abiName]

def buildNative (buildRoot, libTargetDir, nativeLib, buildType):
	deqpDir		= os.path.normpath(os.path.join(common.ANDROID_DIR, ".."))
	buildDir	= getNativeBuildDir(buildRoot, nativeLib, buildType)
	libsDir		= os.path.join(libTargetDir, nativeLib.abiVersion)
	srcLibFile	= os.path.join(buildDir, common.NATIVE_LIB_NAME)
	dstLibFile	= os.path.join(libsDir, common.NATIVE_LIB_NAME)

	# Make build directory if necessary
	if not os.path.exists(buildDir):
		os.makedirs(buildDir)
		toolchainFile = '%s/framework/delibs/cmake/toolchain-android-%s.cmake' % (deqpDir, common.ANDROID_NDK_TOOLCHAIN_VERSION)
		common.execArgsInDirectory([
				'cmake',
				'-G%s' % common.CMAKE_GENERATOR,
				'-DCMAKE_TOOLCHAIN_FILE=%s' % toolchainFile,
				'-DANDROID_NDK_HOST_OS=%s' % common.ANDROID_NDK_HOST_OS,
				'-DANDROID_NDK_PATH=%s' % common.ANDROID_NDK_PATH,
				'-DANDROID_ABI=%s' % nativeLib.abiVersion,
				'-DDE_ANDROID_API=%s' % nativeLib.apiVersion,
				'-DCMAKE_BUILD_TYPE=%s' % buildType,
				'-DDEQP_TARGET=android',
				deqpDir
			], buildDir)

	common.execArgsInDirectory(['cmake', '--build', '.'] + common.EXTRA_BUILD_ARGS, buildDir)

	if not os.path.exists(libsDir):
		os.makedirs(libsDir)

	shutil.copyfile(srcLibFile, dstLibFile)

	# Copy gdbserver for debugging
	if buildType.lower() == "debug":
		srcGdbserverPath = os.path.join(common.ANDROID_NDK_PATH,
										'prebuilt',
										getPrebuiltsDirName(nativeLib.abiVersion),
										'gdbserver',
										'gdbserver')
		dstGdbserverPath = os.path.join(libsDir, 'gdbserver')
		shutil.copyfile(srcGdbserverPath, dstGdbserverPath)
	else:
		assert not os.path.exists(os.path.join(libsDir, "gdbserver"))

def buildApp (buildRoot, isRelease, javaApi):
	appDir	= os.path.join(buildRoot, "package")

	# Set up app
	os.chdir(appDir)

	manifestSrcPath = os.path.normpath(os.path.join(common.ANDROID_DIR, "package", "AndroidManifest.xml"))
	manifestDstPath = os.path.normpath(os.path.join(appDir, "AndroidManifest.xml"))

	# Build dir can be the Android dir, in which case the copy is not needed.
	if manifestSrcPath != manifestDstPath:
		shutil.copy(manifestSrcPath, manifestDstPath)

	common.execArgs([
			common.ANDROID_BIN,
			'update', 'project',
			'--name', 'dEQP',
			'--path', '.',
			'--target', javaApi,
		])

	# Build
	common.execArgs([
			common.ANT_BIN,
			"release" if isRelease else "debug",
			"-Dsource.dir=" + os.path.join(common.ANDROID_DIR, "package", "src"),
			"-Dresource.absolute.dir=" + os.path.join(common.ANDROID_DIR, "package", "res")
		])

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

def build (buildRoot=common.ANDROID_DIR, isRelease=False, nativeBuildType="Release", javaApi=common.ANDROID_JAVA_API, doParallelBuild=False):
	curDir = os.getcwd()

	try:
		assetsSrcDir = getAssetsDir(buildRoot, common.NATIVE_LIBS[0], nativeBuildType)
		assetsDstDir = os.path.join(buildRoot, "package", "assets")

		# Remove assets from the first build dir where we copy assets from
		# to avoid collecting cruft there.
		if os.path.exists(assetsSrcDir):
			shutil.rmtree(assetsSrcDir)
		if os.path.exists(assetsDstDir):
			shutil.rmtree(assetsDstDir)

		# Remove old libs dir to avoid collecting out-of-date versions
		# of libs for ABIs not built this time.
		libTargetDir = os.path.join(buildRoot, "package", "libs")
		if os.path.exists(libTargetDir):
			shutil.rmtree(libTargetDir)

		# Build native code
		nativeBuildArgs = [(buildRoot, libTargetDir, nativeLib, nativeBuildType) for nativeLib in common.NATIVE_LIBS]
		if doParallelBuild:
			common.parallelApply(buildNative, nativeBuildArgs)
		else:
			common.serialApply(buildNative, nativeBuildArgs)

		# Copy assets
		if os.path.exists(assetsSrcDir):
			shutil.copytree(assetsSrcDir, assetsDstDir)

		# Build java code and .apk
		buildApp(buildRoot, isRelease, javaApi)

	finally:
		# Restore working dir
		os.chdir(curDir)

def dumpConfig ():
	print " "
	for entry in common.CONFIG_STRINGS:
		print "%-30s : %s" % (entry[0], entry[1])
	print " "

if __name__ == "__main__":
	parser = argparse.ArgumentParser()
	parser.add_argument('--is-release', dest='isRelease', type=bool, default=False, help="Build android project in release mode.")
	parser.add_argument('--native-build-type', dest='nativeBuildType', default="Release", help="Build type passed cmake when building native code.")
	parser.add_argument('--build-root', dest='buildRoot', default=common.ANDROID_DIR, help="Root directory for storing build results.")
	parser.add_argument('--dump-config', dest='dumpConfig', action='store_true', help="Print out all configurations variables")
	parser.add_argument('--java-api', dest='javaApi', default=common.ANDROID_JAVA_API, help="Set the API signature for the java build.")
	parser.add_argument('-p', '--parallel-build', dest='parallelBuild', action="store_true", help="Build native libraries in parallel.")

	args = parser.parse_args()

	if args.dumpConfig:
		dumpConfig()

	build(buildRoot=os.path.abspath(args.buildRoot), isRelease=args.isRelease, nativeBuildType=args.nativeBuildType, javaApi=args.javaApi, doParallelBuild=args.parallelBuild)
