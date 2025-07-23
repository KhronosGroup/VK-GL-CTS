# -*- coding: utf-8 -*-

#-------------------------------------------------------------------------
# drawElements Quality Program utilities
# --------------------------------------
#
# Copyright 2017 The Ohos Open Source Project
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

# \todo [2017-04-10 pyry]
# * Use smarter asset copy in main build
#   * cmake -E copy_directory doesn't copy timestamps which will cause
#     assets to be always re-packaged
# * Consider adding an option for downloading SDK & NDK

import os
import re
import sys
import glob
import shutil
import argparse
import tempfile
import logging
import json
import xml.etree.ElementTree

# Import from <root>/scripts
sys.path.append(os.path.join(os.path.dirname(__file__), ".."))

from ctsbuild.common import *
from ctsbuild.config import *
from ctsbuild.build import *

class SDKEnv:
    def __init__(self, path, desired_version):
        self.path = path
        self.buildToolsVersion = SDKEnv.selectBuildToolsVersion(self.path, desired_version)

    @staticmethod
    def getBuildToolsVersions (path):
        buildToolsPath = os.path.join(path, "version.txt")
        versions = {}
        logging.info("getBuildToolsVersions %s." % (buildToolsPath))
        if os.path.exists(buildToolsPath):
            # for item in os.listdir(buildToolsPath):
            #     m = re.match(r'^([0-9]+)\.([0-9]+)\.([0-9]+)$', item)
            #     if m != None:
            #         versions[int(m.group(1))] = (int(m.group(1)), int(m.group(2)), int(m.group(3)))
            with open(buildToolsPath) as buildToolsFile:
                for line in buildToolsFile:
                    keyValue = list(map(lambda x: x.strip(), line.split(":")))
                    logging.info("keyValue[0] %s", keyValue[0])
                    if keyValue[0] == "HarmonyOS SDK":
                        logging.info("keyValue[1] %s", keyValue[1].split()[0])
                        versionsList = keyValue[1].split()[0].split(".")
                        versions[int(versionsList[0])] = (int(versionsList[0]), int(versionsList[1]), int(versionsList[2]))
        return versions

    @staticmethod
    def selectBuildToolsVersion (path, preferred):
        logging.info("selectBuildToolsVersion %s, %d ." % (path, preferred))
        versions = SDKEnv.getBuildToolsVersions(path)

        if len(versions) == 0:
            return (0,0,0)

        if preferred == -1:
            return max(versions.values())

        if preferred in versions:
            return versions[preferred]

        # Pick newest
        newest_version = max(versions.values())
        logging.info("Couldn't find Ohos Tool version %d, %d was selected." % (preferred, newest_version[0]))
        return newest_version

    def getPlatformLibrary (self, apiVersion):
        return os.path.join(self.path, "platforms", "ohos-%d" % apiVersion, "ohos.jar")

    def getHvigorPath (self):
        return os.path.join(self.path, "hvigor", "bin")
    
    def getHapSignToolPath (self):
        return os.path.join(self.path, "sdk", "default", "openharmony", "toolchains", "lib")

class CLTEnv:
    def __init__(self, path):
        self.path = path
        self.version = CLTEnv.detectVersion(self.path)
        self.hostOsName = CLTEnv.detectHostOsName(self.path)

    @staticmethod
    def getKnownAbis ():
        return ["armeabi-v7a", "arm64-v8a"]

    @staticmethod
    def getAbiPrebuiltsName (abiName):
        prebuilts = {
            "armeabi-v7a": 'ohos-arm',
            "arm64-v8a": 'ohos-arm64',
            "x86": 'ohos-x86',
            "x86_64": 'ohos-x86_64',
        }

        if not abiName in prebuilts:
            raise Exception("Unknown ABI: " + abiName)

        return prebuilts[abiName]

    @staticmethod
    def detectVersion (path):
        propFilePath = os.path.join(path, "linux/native/oh-uni-package.json")
        try:
            with open(propFilePath) as propFile:
                for line in propFile:
                    keyValue = list(map(lambda x: x.strip(), line.split(":")))
                    logging.info("keyValue[0] %s", keyValue[0])
                    if keyValue[0] == "\"version\"":
                        logging.info("keyValue[1] %s", keyValue[1])
                        versionParts = keyValue[1].replace("\"", "").split(".")
                        return tuple(map(int, versionParts[0:2]))
        except Exception as e:
            raise Exception("Failed to read source prop file '%s': %s" % (propFilePath, str(e)))
        except:
            raise Exception("Failed to read source prop file '%s': unkown error")

        raise Exception("Failed to detect NDK version (does %s/native/oh-uni-package.json have version?)" % path)

    @staticmethod
    def isHostOsSupported (hostOsName):
        os = HostInfo.getOs()
        bits = HostInfo.getArchBits()
        hostOsParts = hostOsName.split('-')

        if len(hostOsParts) > 1:
            assert(len(hostOsParts) == 2)
            assert(hostOsParts[1] == "x86_64")

            if bits != 64:
                return False

        if os == HostInfo.OS_WINDOWS:
            return hostOsParts[0] == 'windows'
        elif os == HostInfo.OS_LINUX:
            return hostOsParts[0] == 'linux'
        elif os == HostInfo.OS_OSX:
            return hostOsParts[0] == 'darwin'
        else:
            raise Exception("Unhandled HostInfo.getOs() '%d'" % os)

    @staticmethod
    def detectHostOsName (path):
        logging.info("detectHostOsName path %s", path)
        hostOsNames = [
            "linux",
            "windows",
        ]

        for name in hostOsNames:
            if os.path.exists(os.path.join(path, name)):
                return name

        raise Exception("Failed to determine NDK host OS")

class Environment:
    def __init__(self, sdk, ndk):
        self.sdk = sdk
        self.ndk = ndk

class Configuration:
    def __init__(self, env, buildPath, abis, nativeApi, arktsApi, minApi, nativeBuildType, verbose, layers, angle):
        self.env = env
        self.sourcePath = DEQP_DIR
        self.buildPath = buildPath
        self.abis = abis
        self.nativeApi = nativeApi
        self.arktsApi = arktsApi
        self.minApi = minApi
        self.nativeBuildType = nativeBuildType
        self.verbose = verbose
        self.layers = layers
        self.angle = angle
        self.dCompilerName = "./hvigorw"
        self.cmakeGenerator = selectFirstAvailableGenerator([MAKEFILE_GENERATOR])

    def check (self):
        if self.cmakeGenerator == None:
            raise Exception("Failed to find build tools for CMake")

        if not os.path.exists(self.env.ndk.path):
            raise Exception("Ohos NDK not found at %s" % self.env.ndk.path)

        if not CLTEnv.isHostOsSupported(self.env.ndk.hostOsName):
            raise Exception("NDK '%s' is not supported on this machine" % self.env.ndk.hostOsName)

        if self.env.ndk.version[0] < 4:
            raise Exception("Ohos public sdk version %d is not supported; build requires public sdk version >= 4" % (self.env.ndk.version[0]))

        if not (self.minApi <= self.arktsApi <= self.nativeApi):
            raise Exception("Requires: min-api (%d) <= arkts-api (%d) <= native-api (%d)" % (self.minApi, self.arktsApi, self.nativeApi))

        if self.env.sdk.buildToolsVersion == (0,0,0):
            raise Exception("No build tools directory found at %s" % os.path.join(self.env.sdk.path, "build-tools"))

        if not os.path.exists(os.path.join(self.env.sdk.path, "version.txt")):
            # [wshi]: TODO, need complete later
            raise Exception("No SDK with api version %d directory found at %s for Api" % (self.arktsApi, os.path.join(self.env.sdk.path, "platforms")))

        # Try to find first d8 since dx was deprecated
        if which(self.dCompilerName, [self.env.sdk.getHvigorPath()]) == None:
            logging.info("Couldn't find %s in path %s, will try to find dx", 
                         self.dCompilerName, self.env.sdk.getHvigorPath())
            self.dCompilerName = "hvigorw"

        ohosBuildTools = ["hvigorw.bat", "hvigorw.js", self.dCompilerName]
        for tool in ohosBuildTools:
            if which(tool, [self.env.sdk.getHvigorPath()]) == None:
                raise Exception("Missing Ohos build tool: %s in %s" % (tool, self.env.sdk.getHvigorPath()))

        requiredToolsInPath = ["javac", "jar", "keytool"]
        for tool in requiredToolsInPath:
            if which(tool) == None:
                raise Exception("%s not in PATH" % tool)

def log (config, msg):
    if config.verbose:
        logging.info(msg)

def executeAndLog (config, args):
    if config.verbose:
        logging.info(" ".join(args))
    execute(args)

# Path components

class ResolvablePathComponent:
    def __init__ (self):
        pass

class SourceRoot (ResolvablePathComponent):
    def resolve (self, config):
        return config.sourcePath

class BuildRoot (ResolvablePathComponent):
    def resolve (self, config):
        return config.buildPath

class NativeBuildPath (ResolvablePathComponent):
    def __init__ (self, abiName):
        self.abiName = abiName

    def resolve (self, config):
        return getNativeBuildPath(config, self.abiName)

# class GeneratedResSourcePath (ResolvablePathComponent):
#     def __init__ (self, package):
#         self.package = package

#     def resolve (self, config):
#         log(config, "buildPath: %s" % config.buildPath)
#         packageComps = self.package.getPackageName(config).split('.')
#         packageDir = os.path.join(*packageComps)
#         log(config, "packageDir: %s" % packageDir)
#         log(config, "buildPath: %s" % config.buildPath)
#         return os.path.join(config.buildPath, self.package.getAppDirName(), "src", packageDir, "R.java")

def resolvePath (config, path):
    resolvedComps = []

    for component in path:
        if isinstance(component, ResolvablePathComponent):
            resolvedComps.append(component.resolve(config))
        else:
            resolvedComps.append(str(component))
    log(config, "do resolvePath: %s" % (resolvedComps))
    return os.path.join(*resolvedComps)

def resolvePaths (config, paths):
    return list(map(lambda p: resolvePath(config, p), paths))

class BuildStep:
    def __init__ (self):
        pass

    def getInputs (self):
        return []

    def getOutputs (self):
        return []

    @staticmethod
    def expandPathsToFiles (paths):
        """
        Expand mixed list of file and directory paths into a flattened list
        of files. Any non-existent input paths are preserved as is.
        """

        def getFiles (dirPath):
            for root, dirs, files in os.walk(dirPath):
                for file in files:
                    yield os.path.join(root, file)

        files = []
        for path in paths:
            if os.path.isdir(path):
                files += list(getFiles(path))
            else:
                files.append(path)

        return files

    def isUpToDate (self, config):
        log(config, "BuildStep isUpToDate: --- ")
        inputs = resolvePaths(config, self.getInputs())
        outputs = resolvePaths(config, self.getOutputs())

        assert len(inputs) > 0 and len(outputs) > 0
        log(config, "isuptodate outputs: %s" % outputs)

        expandedInputs = BuildStep.expandPathsToFiles(inputs)
        expandedOutputs = BuildStep.expandPathsToFiles(outputs)
        log(config, "isuptodate expandedOutputs: %s" % expandedOutputs)

        existingInputs = list(filter(os.path.exists, expandedInputs))
        existingOutputs = list(filter(os.path.exists, expandedOutputs))
        log(config, "isuptodate existingOutputs: %s" % existingOutputs)

        if len(existingInputs) != len(expandedInputs):
            for file in expandedInputs:
                if file not in existingInputs:
                    logging.info("ERROR: Missing input file: %s" % file)
            die("Missing input files")

        if len(existingOutputs) != len(expandedOutputs):
            return False # One or more output files are missing

        lastInputChange = max(map(os.path.getmtime, existingInputs))
        firstOutputChange = min(map(os.path.getmtime, existingOutputs))

        return lastInputChange <= firstOutputChange

    def update (config):
        die("BuildStep.update() not implemented")

def getNativeBuildPath (config, abiName):
    return os.path.join(config.buildPath, "%s-%s-%d" % (abiName, config.nativeBuildType, config.nativeApi))

def clearCMakeCacheVariables(args):
    # New value, so clear the necessary cmake variables
    args.append('-UANGLE_LIBS')
    args.append('-UGLES1_LIBRARY')
    args.append('-UGLES2_LIBRARY')
    args.append('-UEGL_LIBRARY')

def buildNativeLibrary (config, abiName):
    def makeNDKVersionString (version):
        minorVersionString = (chr(ord('a') + version[1]) if version[1] > 0 else "")
        return "r%d%s" % (version[0], minorVersionString)

    def getBuildArgs (config, abiName):
        # [wshi] TODO: remove some config of android
        # '-DDEQP_TARGET_TOOLCHAIN=ndk-modern',
        # '-DOHOS_NDK_PATH=%s' % config.env.ndk.path,
        # '-DDE_OHOS_API=%s' % config.nativeApi
        sdkpath = os.path.join(config.env.sdk.path, "sdk/default/openharmony/native/build/cmake/ohos.toolchain.cmake")
        log(config, "ndkpath=%s" % config.env.ndk.path)
        log(config, 'sdkpath=%s' % config.env.sdk.path)
        log(config, 'sdkjoinpath=%s' % sdkpath)

        args = ['-DDEQP_TARGET=ohos',
                '-DCMAKE_C_FLAGS=-Werror',
                '-DCMAKE_CXX_FLAGS=-Werror',      
                '-DOHOS_ABI=%s' % abiName,
                '-DCMAKE_TOOLCHAIN_FILE=%s' % sdkpath]

        if config.angle is None:
            # Find any previous builds that may have embedded ANGLE libs and clear the CMake cache
            for abi in CLTEnv.getKnownAbis():
                cMakeCachePath = os.path.join(getNativeBuildPath(config, abi), "CMakeCache.txt")
                try:
                    if 'ANGLE_LIBS' in open(cMakeCachePath).read():
                        clearCMakeCacheVariables(args)
                except IOError:
                    pass
        else:
            cMakeCachePath = os.path.join(getNativeBuildPath(config, abiName), "CMakeCache.txt")
            angleLibsDir = os.path.join(config.angle, abiName)
            # Check if the user changed where the ANGLE libs are being loaded from
            try:
                if angleLibsDir not in open(cMakeCachePath).read():
                    clearCMakeCacheVariables(args)
            except IOError:
                pass
            args.append('-DANGLE_LIBS=%s' % angleLibsDir)

        return args

    nativeBuildPath = getNativeBuildPath(config, abiName)
    buildConfig = BuildConfig(nativeBuildPath, config.nativeBuildType, getBuildArgs(config, abiName))

    # [wshi] TODO: there is no deqp target in default
    #build(buildConfig, config.cmakeGenerator, ["deqp"])
    build(buildConfig, config.cmakeGenerator, [])

def executeSteps (config, steps):
    for step in steps:
        if not step.isUpToDate(config):
            step.update(config)

def parsePackageName (manifestPath):
    # [wshi] TODO: 原来是根据manifest文件找bundlename
    logging.info("parsePackageName= %s", manifestPath)
    
    with open(manifestPath, 'r', encoding='utf-8') as f:
        data = json.load(f)
    logging.info("bundleName= %s", data['app']['bundleName'])
    return data['app']['bundleName']

    # tree = xml.etree.ElementTree.parse(manifestPath)

    # if not 'package' in tree.getroot().attrib:
    #     raise Exception("'package' attribute missing from root element in %s" % manifestPath)

    # return tree.getroot().attrib['package']

class PackageDescription:
    def __init__ (self, appDirName, appName, hasResources = True):
        logging.info("appDirName=%s, appName=%s", appDirName, appName)
        self.appDirName = appDirName
        self.hapDirName = "ohos"
        self.appName = appName
        self.hasResources = hasResources

    def getAppName (self):
        return self.appName

    def getAppDirName (self):
        return self.appDirName
    
    def getHapDirName (self):
        return self.hapDirName

    def getPackageName (self, config):
        log(config, "getPackageName manifestPath: %s" % self.getAppJsonPath())
        manifestPath = resolvePath(config, self.getAppJsonPath())

        return parsePackageName(manifestPath)

    def getAppJsonPath (self):
        # [wshi] TODO: 发现app配置文件
        logging.info("getManifestPath appDirName: %s", self.appDirName)
        return [SourceRoot(), "ohos", self.appDirName, "AppScope/app.json5"]

    def getResPath (self):
        return [SourceRoot(), "ohos", self.appDirName, "entry/src/main/resources"]

    def getSourcePaths (self):
        return [
                [SourceRoot(), "ohos", self.appDirName, ""]
            ]

    def getClassesHapPath (self):
        return [BuildRoot(), self.appDirName, "", "happroject"]

# Build step implementations

class BuildNativeLibrary (BuildStep):
    def __init__ (self, abi):
        self.abi = abi

    def isUpToDate (self, config):
        return False

    def update (self, config):
        log(config, "BuildNativeLibrary: %s" % self.abi)
        buildNativeLibrary(config, self.abi)

class GenResourcesSrc (BuildStep):
    def __init__ (self, package):
        self.package = package

    def getInputs (self):
        return [self.package.getResPath(), self.package.getManifestPath()]

    def getOutputs (self):
        #return [[GeneratedResSourcePath(self.package)]]
        return []

    def update (self, config):
        log(config, "GenResourcesSrc update: ")
        # [wshi] TODO: aapt是android资源打包工具，ohos不用
        # aaptPath = which("aapt", [config.env.sdk.getHvigorPath()])
        # dstDir = os.path.dirname(resolvePath(config, [GeneratedResSourcePath(self.package)]))

        # if not os.path.exists(dstDir):
        #     os.makedirs(dstDir)

        # executeAndLog(config, [
        #         aaptPath,
        #         "package",
        #         "-f",
        #         "-m",
        #         "-S", resolvePath(config, self.package.getResPath()),
        #         "-M", resolvePath(config, self.package.getManifestPath()),
        #         "-J", resolvePath(config, [BuildRoot(), self.package.getAppDirName(), "src"]),
        #         "-I", config.env.sdk.getPlatformLibrary(config.arktsApi)
        #     ])

class CreateKeystore (BuildStep):
    def __init__ (self):
        self.keystorePath = [BuildRoot(), "debug.keystore"]

    def getOutputs (self):
        return [self.keystorePath]

    def isUpToDate (self, config):
        return os.path.exists(resolvePath(config, self.keystorePath))

    def update (self, config):
        executeAndLog(config, [
                "keytool",
                "-genkeypair",
                "-keystore", resolvePath(config, self.keystorePath),
                "-storepass", "ohos",
                "-alias", "ohosdebugkey",
                "-keypass", "ohos",
                "-keyalg", "RSA",
                "-keysize", "2048",
                "-validity", "10000",
                "-dname", "CN=, OU=, O=, L=, S=, C=",
            ])

# Builds HAP without code
class BuildBaseHap (BuildStep):
    def __init__ (self, package, libraries = []):
        self.package = package
        self.libraries = libraries
        self.srcPath = [BuildRoot(), self.package.getAppDirName(), "", "happroject"]
        self.dstPath = [BuildRoot(), self.package.getAppDirName(), "happroject", "autosign", "entry-default-signed.hap"]

    def getResPaths (self):
        paths = []
        for pkg in [self.package] + self.libraries:
            if pkg.hasResources:
                paths.append(pkg.getResPath())
        return paths

    def getInputs (self):
        return [self.srcPath]

    def getOutputs (self):
        return [self.dstPath]

    def update (self, config):
        log(config, "BuildBaseAPK update: ")
        # [wshi] TODO: 编译base.apk，主要编译资源
        hvigorwPath = which("hvigorw", [config.env.sdk.getHvigorPath()])
        log(config, "BuildBaseAPK hvigorwPath=%s" % hvigorwPath)

        args = [
            "./hvigorw",
            "assembleHap",
            "--mode",
            "module", "-p", "product=default",
            "-p", "buildMode=debug", "--no-daemon"
        ]
        workPath = resolvePath(config, self.srcPath)
        pushWorkingDir(workPath)
        args = [
            "./hvigorw",
            "assembleHap",
            "--mode",
            "module", "-p", "product=default",
            "-p", "buildMode=debug", "--no-daemon"
        ]
        executeAndLog(config, args)
        popWorkingDir()

def addFilesToHap (config, apkPath, baseDir, relFilePaths):
    log(config, "addFilesToAPK ---: ")
    # [wshi] TODO: 添加资源
    # aaptPath = which("aapt", [config.env.sdk.getHvigorPath()])
    # maxBatchSize = 25

    # pushWorkingDir(baseDir)
    # try:
    #     workQueue = list(relFilePaths)
    #     # Workaround for Windows.
    #     if os.path.sep == "\\":
    #         workQueue = [i.replace("\\", "/") for i in workQueue]

    #     while len(workQueue) > 0:
    #         batchSize = min(len(workQueue), maxBatchSize)
    #         items = workQueue[0:batchSize]

    #         executeAndLog(config, [
    #                 aaptPath,
    #                 "add",
    #                 "-f", apkPath,
    #             ] + items)

    #         del workQueue[0:batchSize]
    # finally:
    #     popWorkingDir()

def addFileToHap (config, apkPath, baseDir, relFilePath):
    addFilesToHap(config, apkPath, baseDir, [relFilePath])

class AddHapToBuild (BuildStep):
    def __init__ (self, package):
        self.package = package
        self.srcPath = [SourceRoot(), "ohos", "happroject"]
        self.dstPath = [BuildRoot(), self.package.getAppDirName(), "happroject"]

    def getInputs (self):
        return [
                self.srcPath,
            ]

    def getOutputs (self):
        return [self.dstPath]

    def update (self, config):
        srcPath = resolvePath(config, self.srcPath)
        dstPath = resolvePath(config, self.dstPath)
        
        log(config, "AddHapToBuild srcPath=%s, dstPath=%s " % (srcPath, dstPath))

        # shutil.copyfile(srcPath, dstPath)
        if os.path.exists(dstPath):
            shutil.rmtree(dstPath)

        shutil.copytree(srcPath, dstPath)

class AddDataToHap (BuildStep):
    def __init__ (self, package, abi):
        self.package = package
        self.buildPath = [NativeBuildPath(abi)]
        self.srcPath = [SourceRoot(), "external", "openglcts", "data", "gl_cts"]
        self.dstPath = [BuildRoot(), self.package.getAppDirName(), "happroject", "entry", "src", "main", "resources", "rawfile", "gl_cts"]
        self.srcDataPath = [SourceRoot(), "data"]
        self.dstDataPath = [BuildRoot(), self.package.getAppDirName(), "happroject", "entry", "src", "main", "resources", "rawfile"]

    def getInputs (self):
        return [
                self.srcPath,
                # self.buildPath + ["assets"]
            ]

    def getOutputs (self):
        return [self.dstPath]

    def update (self, config):
        srcPath = resolvePath(config, self.srcPath)
        dstPath = resolvePath(config, self.dstPath)
        buildPath = resolvePath(config, self.buildPath)
        srcDataPath = resolvePath(config, self.srcDataPath)
        dstDataPath = resolvePath(config, self.dstDataPath)

        log(config, "AddAssetsToAPK srcPath=%s, dstPath=%s, buildPath=%s " % (srcPath, dstPath, buildPath))

        if os.path.exists(dstDataPath):
            shutil.rmtree(dstDataPath)

        shutil.copytree(srcDataPath, dstDataPath)

        shutil.copytree(srcPath, dstPath)     

class AddNativeLibsToHap (BuildStep):
    def __init__ (self, package, abis):
        self.package = package
        self.abis = abis
        self.srcPath = AddDataToHap(self.package, "").getOutputs()[0]
        self.dstPath = [BuildRoot(), self.package.getAppDirName(), "tmp", "with-native-libs.apk"]

    def getInputs (self):
        paths = [self.srcPath]
        for abi in self.abis:
            paths.append([NativeBuildPath(abi), "libdeqp.so"])
        return paths

    def getOutputs (self):
        return [self.dstPath]

    def update (self, config):
        srcPath = resolvePath(config, self.srcPath)
        dstPath = resolvePath(config, self.getOutputs()[0])
        pkgPath = resolvePath(config, [BuildRoot(), self.package.getAppDirName()])
        libFiles = []

        # Create right directory structure first
        for abi in self.abis:
            libSrcPath = resolvePath(config, [NativeBuildPath(abi), "libdeqp.so"])
            libRelPath = os.path.join("lib", abi, "libdeqp.so")
            libAbsPath = os.path.join(pkgPath, libRelPath)

            if not os.path.exists(os.path.dirname(libAbsPath)):
                os.makedirs(os.path.dirname(libAbsPath))

            shutil.copyfile(libSrcPath, libAbsPath)
            libFiles.append(libRelPath)

            if config.layers:
                # Need to copy everything in the layer folder
                layersGlob = os.path.join(config.layers, abi, "*")
                libVkLayers = glob.glob(layersGlob)
                for layer in libVkLayers:
                    layerFilename = os.path.basename(layer)
                    layerRelPath = os.path.join("lib", abi, layerFilename)
                    layerAbsPath = os.path.join(pkgPath, layerRelPath)
                    shutil.copyfile(layer, layerAbsPath)
                    libFiles.append(layerRelPath)
                    log(config, "Adding layer binary: %s" % (layer,))

            # if config.angle:
            #     angleGlob = os.path.join(config.angle, abi, "lib*_angle.so")
            #     libAngle = glob.glob(angleGlob)
            #     for lib in libAngle:
            #         libFilename = os.path.basename(lib)
            #         libRelPath = os.path.join("lib", abi, libFilename)
            #         libAbsPath = os.path.join(pkgPath, libRelPath)
            #         shutil.copyfile(lib, libAbsPath)
            #         libFiles.append(libRelPath)
            #         log(config, "Adding ANGLE binary: %s" % (lib,))

        shutil.copyfile(srcPath, dstPath)
        addFilesToHap(config, dstPath, pkgPath, libFiles)

class SignHap (BuildStep):
    def __init__ (self, package):
        self.package = package
        self.srcPath = [BuildRoot(), self.package.getAppDirName(), "happroject", "autosign"]
        self.dstPath = [BuildRoot(), self.package.getAppDirName(), "happroject", "autosign", "entry-default-signed.hap"]
        self.keystorePath = [BuildRoot(), self.package.getAppDirName(), "", "happroject"]

    def getInputs (self):
        return [self.srcPath, self.keystorePath]

    def getOutputs (self):
        return [self.dstPath]

    def update (self, config):
        log(config, "signapk update: ")
        # [wshi] TODO: 签名apk

        hapSignToolPath = which("hap-sign-tool.jar", [config.env.sdk.getHapSignToolPath()])
        log(config, "sign-tool-path=%s" % config.env.sdk.getHapSignToolPath())
        log(config, "SignAPK hapSignToolPath=%s" % hapSignToolPath)

        workPath = resolvePath(config, self.srcPath)
        hapSignToolPath = resolvePath(config, [config.env.sdk.getHapSignToolPath(), "hap-sign-tool.jar"])
        log(config, "SignAPK hapSignToolPath=%s" % hapSignToolPath)
        pushWorkingDir(workPath)
        
        unsignHapPath = resolvePath(config, [BuildRoot(), self.package.getAppDirName(), 
                "happroject", "entry", "build", "default", "outputs", "default", "entry-default-unsigned.hap"])

        args = [
            "java",
            "-jar", hapSignToolPath, "sign-app",  "-keyAlias", "oh-app1-key-v1",
            "-signAlg", "SHA256withECDSA", "-mode", "localSign", "-appCertFile",
            "app1.cer", "-profileFile", "app1-profile.p7b", "-inFile", unsignHapPath,
            "-keystoreFile", "OpenHarmony.p12", "-outFile", "app1-signed.hap", "-keyPwd", "123456",
            "-keystorePwd", "123456"
        ]

        executeAndLog(config, args)
        popWorkingDir()

def getBuildRootRelativeHapPath (package):
    return os.path.join(package.getAppDirName(), package.getAppName() + ".apk")

class AlignAPK (BuildStep):
    def __init__ (self, package):
        self.package = package
        self.srcPath = AddNativeLibsToHap(self.package, []).getOutputs()[0]
        self.dstPath = [BuildRoot(), self.package.getAppDirName(), "tmp", "aligned.apk"]
        self.keystorePath = CreateKeystore().getOutputs()[0]

    def getInputs (self):
        return [self.srcPath]

    def getOutputs (self):
        return [self.dstPath]

    def update (self, config):
        log(config, "alignapk update: ")
        # [wshi] TODO: zipalign是apk编译对齐工具
        # srcPath = resolvePath(config, self.srcPath)
        # dstPath = resolvePath(config, self.dstPath)
        # zipalignPath = os.path.join(config.env.sdk.getHvigorPath(), "zipalign")

        # executeAndLog(config, [
        #         zipalignPath,
        #         "-f", "4",
        #         srcPath,
        #         dstPath
        #     ])

def getBuildStepsForPackage (abis, package, libraries = []):
    steps = []

    assert len(abis) > 0

    # Build native code first
    for abi in abis:
        logging.info("BuildNativeLibrary: %s", abi)
        steps += [BuildNativeLibrary(abi)]
    logging.info("after BuildNativeLibrary")
    # [wshi] TODO: these are android native function
    # # Build library packages
    # for library in libraries:
    #     if library.hasResources:
    #         steps.append(GenResourcesSrc(library))
    #     steps.append(BuildJavaSource(library))

    # # Build main package .java sources
    # if package.hasResources:
    #     steps.append(GenResourcesSrc(package))
    # steps.append(BuildJavaSource(package, libraries))
    # steps.append(BuildDex(package, libraries))

    # Build base APK
    # [wshi] TODO: copy ets files to build dir;
    steps.append(AddHapToBuild(package))
    #steps.append(BuildBaseAPK(package, libraries))
    #steps.append(AddJavaToAPK(package))

    # Add assets from first ABI
    # [wshi] TODO: first copy data files to build dir;
    steps.append(AddDataToHap(package, abis[0]))

    # Add native libs to APK
    # [wshi] TODO: copy libs to build dir
    #steps.append(AddNativeLibsToAPK(package, abis))

    # [wshi] TODO: build hap 
    steps.append(BuildBaseHap(package, libraries))

    # Finalize APK
    # steps.append(CreateKeystore())
    # steps.append(AlignAPK(package))

    # [wshi] TODO: sign hap 
    steps.append(SignHap(package))

    return steps

def getPackageAndLibrariesForTarget (target):
    # [wshi] TODO: apkpacakge: cpy lib & rawfile
    deqpPackage = PackageDescription("", "dEQP")
    ctsPackage = PackageDescription("openglcts", "Khronos-CTS", hasResources = False)

    if target == 'deqp':
        return (deqpPackage, [])
    elif target == 'openglcts':
        return (ctsPackage, [deqpPackage])
    else:
        raise Exception("Uknown target '%s'" % target)

def findSDK ():
    ndkBuildPath = which('ndk-build')
    if ndkBuildPath != None:
        return os.path.dirname(ndkBuildPath)
    else:
        return None

def findCLT ():
    sdkBuildPath = which('ohos')
    if sdkBuildPath != None:
        return os.path.dirname(os.path.dirname(sdkBuildPath))
    else:
        return None

def getDefaultBuildRoot ():
    return os.path.join(tempfile.gettempdir(), "deqp-ohos-build")

def parseArgs ():
    nativeBuildTypes = ['Release', 'Debug', 'MinSizeRel', 'RelWithAsserts', 'RelWithDebInfo']
    defaultSDKPath = findSDK()
    defaultCLTPath = findCLT()
    defaultBuildRoot = getDefaultBuildRoot()

    parser = argparse.ArgumentParser(os.path.basename(__file__),
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('--native-build-type',
        dest='nativeBuildType',
        default="Release",
        choices=nativeBuildTypes,
        help="Native code build type")
    parser.add_argument('--build-root',
        dest='buildRoot',
        default=defaultBuildRoot,
        help="Root build directory")
    parser.add_argument('--abis',
        dest='abis',
        default=",".join(CLTEnv.getKnownAbis()),
        help="ABIs to build")
    parser.add_argument('--native-api',
        type=int,
        dest='nativeApi',
        default=20,
        help="Ohos API level to target in native code")
    parser.add_argument('--arkts-api',
        type=int,
        dest='arktsApi',
        default=11,
        help="Ohos API level to target in Arkts code")
    parser.add_argument('--tool-api',
        type=int,
        dest='toolApi',
        default=-1,
        help="Ohos Tools level to target (-1 being maximum present)")
    parser.add_argument('--min-api',
        type=int,
        dest='minApi',
        default=10,
        help="Minimum Ohos API level for which the APK can be installed")
    parser.add_argument('--clt',
        dest='cltPath',
        default=defaultCLTPath,
        help="HarmonyOS Command Line Tools path",
        required=(True if defaultCLTPath == None else False))
    parser.add_argument('--sdk',
        dest='sdkPath',
        default=defaultSDKPath,
        help="Ohos Public SDK path",
        required=(True if defaultSDKPath == None else False))
    parser.add_argument('-v', '--verbose',
        dest='verbose',
        help="Verbose output",
        default=True,
        action='store_true')
    parser.add_argument('--target',
        dest='target',
        help='Build target',
        choices=['deqp', 'openglcts'],
        default='deqp')
    parser.add_argument('--layers-path',
        dest='layers',
        default=None,
        required=False)
    parser.add_argument('--angle-path',
        dest='angle',
        default=None,
        required=False)

    args = parser.parse_args()

    def parseAbis (abisStr):
        knownAbis = set(CLTEnv.getKnownAbis())
        abis = []

        for abi in abisStr.split(','):
            abi = abi.strip()
            if not abi in knownAbis:
                raise Exception("Unknown ABI: %s" % abi)
            abis.append(abi)

        return abis

    # Custom parsing & checks
    try:
        args.abis = parseAbis(args.abis)
        if len(args.abis) == 0:
            raise Exception("--abis can't be empty")
    except Exception as e:
        logging.info("ERROR: %s" % str(e))
        parser.logging.info_help()
        sys.exit(-1)

    return args

if __name__ == "__main__":
    logging.root.setLevel(logging.INFO)
    args = parseArgs()

    ndk = CLTEnv(os.path.realpath(args.sdkPath))
    sdk = SDKEnv(os.path.realpath(args.cltPath), args.toolApi)
    buildPath = os.path.realpath(args.buildRoot)
    env = Environment(sdk, ndk)
    config = Configuration(env, buildPath, abis=args.abis, nativeApi=args.nativeApi, arktsApi=args.arktsApi, minApi=args.minApi, nativeBuildType=args.nativeBuildType,
                         verbose=args.verbose, layers=args.layers, angle=args.angle)

    try:
        config.check()
    except Exception as e:
        logging.info("ERROR: %s" % str(e))
        logging.info("")
        logging.info("Please check your configuration:")
        logging.info("  --sdk=%s" % args.sdkPath)
        logging.info("  --ndk=%s" % args.ndkPath)
        sys.exit(-1)

    pkg, libs = getPackageAndLibrariesForTarget(args.target)
    steps = getBuildStepsForPackage(config.abis, pkg, libs)

    executeSteps(config, steps)

    logging.info("")
    logging.info("Built %s" % os.path.join(buildPath, getBuildRootRelativeHapPath(pkg)))
