# -*- coding: utf-8 -*-
import logging

#-------------------------------------------------------------------------
# drawElements Quality Program utilities
# --------------------------------------
#
# Copyright 2016 The Android Open Source Project
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

from ctsbuild.common import *
from ctsbuild.build import build
from build_caselists import Module, getModuleByName, getBuildConfig, genCaseList, getCaseListPath, DEFAULT_BUILD_DIR, DEFAULT_TARGET
from fnmatch import fnmatch
from copy import copy
from collections import defaultdict

import argparse
import re
import xml.etree.cElementTree as ElementTree
import xml.dom.minidom as minidom

GENERATED_FILE_WARNING = """
     This file has been automatically generated. Edit with caution.
     """

class Project:
    def __init__ (self, path, copyright = None):
        self.path = path
        self.copyright = copyright

class Configuration:
    def __init__ (self, name, filters, glconfig = None, rotation = None, surfacetype = None, required = False, runtime = None, runByDefault = True, listOfGroupsToSplit = []):
        self.name = name
        self.glconfig = glconfig
        self.rotation = rotation
        self.surfacetype = surfacetype
        self.required = required
        self.filters = filters
        self.expectedRuntime = runtime
        self.runByDefault = runByDefault
        self.listOfGroupsToSplit = listOfGroupsToSplit

class Package:
    def __init__ (self, module, configurations):
        self.module = module
        self.configurations = configurations

class Mustpass:
    def __init__ (self, project, version, packages):
        self.project = project
        self.version = version
        self.packages = packages

class Filter:
    TYPE_INCLUDE = 0
    TYPE_EXCLUDE = 1

    def __init__ (self, type, filenames):
        self.type = type
        self.filenames = filenames
        self.key = ",".join(filenames)

class TestRoot:
    def __init__ (self):
        self.children = []

class TestGroup:
    def __init__ (self, name):
        self.name = name
        self.children = []

class TestCase:
    def __init__ (self, name):
        self.name = name
        self.configurations = []

def getSrcDir (mustpass):
    return os.path.join(mustpass.project.path, mustpass.version, "src")

def getModuleShorthand (module):
    assert module.name[:5] == "dEQP-"
    return module.name[5:].lower()

def getCaseListFileName (package, configuration):
    return "%s-%s.txt" % (getModuleShorthand(package.module), configuration.name)

def getDstCaseListPath (mustpass):
    return os.path.join(mustpass.project.path, mustpass.version)

def getCommandLine (config):
    cmdLine = ""

    if config.glconfig != None:
        cmdLine += "--deqp-gl-config-name=%s " % config.glconfig

    if config.rotation != None:
        cmdLine += "--deqp-screen-rotation=%s " % config.rotation

    if config.surfacetype != None:
        cmdLine += "--deqp-surface-type=%s " % config.surfacetype

    cmdLine += "--deqp-watchdog=enable"

    return cmdLine

class CaseList:
    def __init__(self, filePath, sortedLines):
        self.filePath = filePath
        self.sortedLines = sortedLines

def readAndSortCaseList (buildCfg, generator, module):
    build(buildCfg, generator, [module.binName])
    genCaseList(buildCfg, generator, module, "txt")
    filePath = getCaseListPath(buildCfg, module, "txt")
    with open(filePath, 'r') as first_file:
        lines = first_file.readlines()
        lines.sort()
        caseList = CaseList(filePath, lines)
        return caseList

def readPatternList (filename, patternList):
    with open(filename, 'rt') as f:
        for line in f:
            line = line.strip()
            if len(line) > 0 and line[0] != '#':
                patternList.append(line)

def include (*filenames):
    return Filter(Filter.TYPE_INCLUDE, filenames)

def exclude (*filenames):
    return Filter(Filter.TYPE_EXCLUDE, filenames)

def insertXMLHeaders (mustpass, doc):
    if mustpass.project.copyright != None:
        doc.insert(0, ElementTree.Comment(mustpass.project.copyright))
    doc.insert(1, ElementTree.Comment(GENERATED_FILE_WARNING))

def prettifyXML (doc):
    uglyString = ElementTree.tostring(doc, 'utf-8')
    reparsed = minidom.parseString(uglyString)
    return reparsed.toprettyxml(indent='\t', encoding='utf-8')

def genSpecXML (mustpass):
    mustpassElem = ElementTree.Element("Mustpass", version = mustpass.version)
    insertXMLHeaders(mustpass, mustpassElem)

    for package in mustpass.packages:
        packageElem = ElementTree.SubElement(mustpassElem, "TestPackage", name = package.module.name)

        for config in package.configurations:
            configElem = ElementTree.SubElement(packageElem, "Configuration",
                                                caseListFile = getCaseListFileName(package, config),
                                                commandLine = getCommandLine(config),
                                                name = config.name)

    return mustpassElem

def addOptionElement (parent, optionName, optionValue):
    ElementTree.SubElement(parent, "option", name=optionName, value=optionValue)

def genAndroidTestXml (mustpass):
    RUNNER_CLASS = "com.drawelements.deqp.runner.DeqpTestRunner"
    configElement = ElementTree.Element("configuration")

    # have the deqp package installed on the device for us
    preparerElement = ElementTree.SubElement(configElement, "target_preparer")
    preparerElement.set("class", "com.android.tradefed.targetprep.suite.SuiteApkInstaller")
    addOptionElement(preparerElement, "cleanup-apks", "true")
    addOptionElement(preparerElement, "test-file-name", "com.drawelements.deqp.apk")

    # Target preparer for incremental dEQP
    preparerElement = ElementTree.SubElement(configElement, "target_preparer")
    preparerElement.set("class", "com.android.compatibility.common.tradefed.targetprep.FilePusher")
    addOptionElement(preparerElement, "cleanup", "true")
    addOptionElement(preparerElement, "disable", "true")
    addOptionElement(preparerElement, "push", "deqp-binary32->/data/local/tmp/deqp-binary32")
    addOptionElement(preparerElement, "push", "deqp-binary64->/data/local/tmp/deqp-binary64")
    addOptionElement(preparerElement, "push", "gles2->/data/local/tmp/gles2")
    addOptionElement(preparerElement, "push", "gles3->/data/local/tmp/gles3")
    addOptionElement(preparerElement, "push", "gles3-incremental-deqp.txt->/data/local/tmp/gles3-incremental-deqp.txt")
    addOptionElement(preparerElement, "push", "gles31->/data/local/tmp/gles31")
    addOptionElement(preparerElement, "push", "internal->/data/local/tmp/internal")
    addOptionElement(preparerElement, "push", "vk-incremental-deqp.txt->/data/local/tmp/vk-incremental-deqp.txt")
    addOptionElement(preparerElement, "push", "vulkan->/data/local/tmp/vulkan")
    preparerElement = ElementTree.SubElement(configElement, "target_preparer")
    preparerElement.set("class", "com.android.compatibility.common.tradefed.targetprep.IncrementalDeqpPreparer")
    addOptionElement(preparerElement, "disable", "true")

    # add in metadata option for component name
    ElementTree.SubElement(configElement, "option", name="test-suite-tag", value="cts")
    ElementTree.SubElement(configElement, "option", key="component", name="config-descriptor:metadata", value="deqp")
    ElementTree.SubElement(configElement, "option", key="parameter", name="config-descriptor:metadata", value="not_instant_app")
    ElementTree.SubElement(configElement, "option", key="parameter", name="config-descriptor:metadata", value="multi_abi")
    ElementTree.SubElement(configElement, "option", key="parameter", name="config-descriptor:metadata", value="secondary_user")
    ElementTree.SubElement(configElement, "option", key="parameter", name="config-descriptor:metadata", value="no_foldable_states")
    controllerElement = ElementTree.SubElement(configElement, "object")
    controllerElement.set("class", "com.android.tradefed.testtype.suite.module.TestFailureModuleController")
    controllerElement.set("type", "module_controller")
    addOptionElement(controllerElement, "screenshot-on-failure", "false")

    for package in mustpass.packages:
        for config in package.configurations:
            if not config.runByDefault:
                continue

            testElement = ElementTree.SubElement(configElement, "test")
            testElement.set("class", RUNNER_CLASS)
            addOptionElement(testElement, "deqp-package", package.module.name)
            caseListFile = getCaseListFileName(package,config)
            addOptionElement(testElement, "deqp-caselist-file", caseListFile)
            if caseListFile.startswith("gles3"):
                addOptionElement(testElement, "incremental-deqp-include-file", "gles3-incremental-deqp.txt")
            elif caseListFile.startswith("vk"):
                addOptionElement(testElement, "incremental-deqp-include-file", "vk-incremental-deqp.txt")
            # \todo [2015-10-16 kalle]: Replace with just command line? - requires simplifications in the runner/tests as well.
            if config.glconfig != None:
                addOptionElement(testElement, "deqp-gl-config-name", config.glconfig)

            if config.surfacetype != None:
                addOptionElement(testElement, "deqp-surface-type", config.surfacetype)

            if config.rotation != None:
                addOptionElement(testElement, "deqp-screen-rotation", config.rotation)

            if config.expectedRuntime != None:
                addOptionElement(testElement, "runtime-hint", config.expectedRuntime)

            if config.required:
                addOptionElement(testElement, "deqp-config-required", "true")

    insertXMLHeaders(mustpass, configElement)

    return configElement

class PatternSet:
    def __init__(self):
        self.namedPatternsTree = {}
        self.namedPatternsDict = {}
        self.wildcardPatternsDict = {}

def readPatternSets (mustpass):
    patternSets = {}
    for package in mustpass.packages:
        for cfg in package.configurations:
            for filter in cfg.filters:
                if not filter.key in patternSets:
                    patternList = []
                    for filename in filter.filenames:
                        readPatternList(os.path.join(getSrcDir(mustpass), filename), patternList)
                    patternSet = PatternSet()
                    for pattern in patternList:
                        if pattern.find('*') == -1:
                            patternSet.namedPatternsDict[pattern] = 0
                            t = patternSet.namedPatternsTree
                            parts = pattern.split('.')
                            for part in parts:
                                t = t.setdefault(part, {})
                        else:
                            # We use regex instead of fnmatch because it's faster
                            patternSet.wildcardPatternsDict[re.compile("^" + pattern.replace(".", r"\.").replace("*", ".*?") + "$")] = 0
                    patternSets[filter.key] = patternSet
    return patternSets

def genMustpassFromLists (mustpass, moduleCaseLists):
    print("Generating mustpass '%s'" % mustpass.version)
    patternSets = readPatternSets(mustpass)

    for package in mustpass.packages:
        currentCaseList = moduleCaseLists[package.module]
        logging.debug("Reading " + currentCaseList.filePath)

        for config in package.configurations:
            # construct components of path to main destination file
            mainDstFileDir = getDstCaseListPath(mustpass)
            mainDstFileName = getCaseListFileName(package, config)
            mainDstFilePath = os.path.join(mainDstFileDir, mainDstFileName)
            mainGroupSubDir = mainDstFileName[:-4]

            if not os.path.exists(mainDstFileDir):
                os.makedirs(mainDstFileDir)
            mainDstFile = open(mainDstFilePath, 'w')
            print(mainDstFilePath)
            output_files = {}
            def openAndStoreFile(filePath, testFilePath, parentFile):
                if filePath not in output_files:
                    try:
                        print("    " + filePath)
                        parentFile.write(mainGroupSubDir + "/" + testFilePath + "\n")
                        currentDir = os.path.dirname(filePath)
                        if not os.path.exists(currentDir):
                            os.makedirs(currentDir)
                        output_files[filePath] = open(filePath, 'w')

                    except FileNotFoundError:
                        print(f"File not found: {filePath}")
                return output_files[filePath]

            lastOutputFile = ""
            currentOutputFile = None
            for line in currentCaseList.sortedLines:
                if not line.startswith("TEST: "):
                    continue
                caseName = line.replace("TEST: ", "").strip("\n")
                caseParts = caseName.split(".")
                keep = True
                # Do the includes with the complex patterns first
                for filter in config.filters:
                    if filter.type == Filter.TYPE_INCLUDE:
                        keep = False
                        patterns = patternSets[filter.key].wildcardPatternsDict
                        for pattern in patterns.keys():
                            keep = pattern.match(caseName)
                            if keep:
                                patterns[pattern] += 1
                                break

                        if not keep:
                            t = patternSets[filter.key].namedPatternsTree
                            if len(t.keys()) == 0:
                                continue
                            for part in caseParts:
                                if part in t:
                                    t = t[part]
                                else:
                                    t = None  # Not found
                                    break
                            keep = t == {}
                            if keep:
                                patternSets[filter.key].namedPatternsDict[caseName] += 1

                    # Do the excludes
                    if filter.type == Filter.TYPE_EXCLUDE:
                        patterns = patternSets[filter.key].wildcardPatternsDict
                        for pattern in patterns.keys():
                            discard = pattern.match(caseName)
                            if discard:
                                patterns[pattern] += 1
                                keep = False
                                break
                        if keep:
                            t = patternSets[filter.key].namedPatternsTree
                            if len(t.keys()) == 0:
                                continue
                            for part in caseParts:
                                if part in t:
                                    t = t[part]
                                else:
                                    t = None  # Not found
                                    break
                            if t == {}:
                                patternSets[filter.key].namedPatternsDict[caseName] += 1
                                keep = False
                    if not keep:
                        break
                if not keep:
                    continue

                parts = caseName.split('.')
                if len(config.listOfGroupsToSplit) > 0:
                    if len(parts) > 2:
                        groupName = parts[1].replace("_", "-")
                        for splitPattern in config.listOfGroupsToSplit:
                            splitParts = splitPattern.split(".")
                            if len(splitParts) > 1 and caseName.startswith(splitPattern + "."):
                                groupName = groupName + "/" + parts[2].replace("_", "-")
                        filePath = os.path.join(mainDstFileDir, mainGroupSubDir, groupName + ".txt")
                        if lastOutputFile != filePath:
                            currentOutputFile = openAndStoreFile(filePath, groupName + ".txt", mainDstFile)
                            lastOutputFile = filePath
                        currentOutputFile.write(caseName + "\n")
                else:
                    mainDstFile.write(caseName + "\n")

            # Check that all patterns have been used in the filters
            # This check will help identifying typos and patterns becoming stale
            for filter in config.filters:
                if filter.type == Filter.TYPE_INCLUDE:
                    patternSet = patternSets[filter.key]
                    for pattern, usage in patternSet.namedPatternsDict.items():
                        if usage == 0:
                            logging.debug("Case %s in file %s for module %s was never used!" % (pattern, filter.key, config.name))
                    for pattern, usage in patternSet.wildcardPatternsDict.items():
                        if usage == 0:
                            logging.debug("Pattern %s in file %s for module %s was never used!" % (pattern, filter.key, config.name))

    # Generate XML
    specXML = genSpecXML(mustpass)
    specFilename = os.path.join(mustpass.project.path, mustpass.version, "mustpass.xml")

    print("  Writing spec: " + specFilename)
    writeFile(specFilename, prettifyXML(specXML).decode())

    # TODO: Which is the best selector mechanism?
    if (mustpass.version == "main"):
        androidTestXML = genAndroidTestXml(mustpass)
        androidTestFilename = os.path.join(mustpass.project.path, "AndroidTest.xml")

        print("  Writing AndroidTest.xml: " + androidTestFilename)
        writeFile(androidTestFilename, prettifyXML(androidTestXML).decode())

    print("Done!")


def genMustpassLists (mustpassLists, generator, buildCfg):
    moduleCaseLists = {}

    # Getting case lists involves invoking build, so we want to cache the results
    for mustpass in mustpassLists:
        for package in mustpass.packages:
            if not package.module in moduleCaseLists:
                moduleCaseLists[package.module] = readAndSortCaseList(buildCfg, generator, package.module)

    for mustpass in mustpassLists:
        genMustpassFromLists(mustpass, moduleCaseLists)

def parseCmdLineArgs ():
    parser = argparse.ArgumentParser(description = "Build Android CTS mustpass",
                                     formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("-b",
                        "--build-dir",
                        dest="buildDir",
                        default=DEFAULT_BUILD_DIR,
                        help="Temporary build directory")
    parser.add_argument("-t",
                        "--build-type",
                        dest="buildType",
                        default="Debug",
                        help="Build type")
    parser.add_argument("-c",
                        "--deqp-target",
                        dest="targetName",
                        default=DEFAULT_TARGET,
                        help="dEQP build target")
    return parser.parse_args()

def parseBuildConfigFromCmdLineArgs ():
    args = parseCmdLineArgs()
    return getBuildConfig(args.buildDir, args.targetName, args.buildType)
