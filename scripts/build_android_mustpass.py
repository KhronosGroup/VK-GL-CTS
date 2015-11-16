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

from build.common import *
from build.config import ANY_GENERATOR
from build.build import build
from build_caselists import Module, getBuildConfig, genCaseList, getCaseListPath, DEFAULT_BUILD_DIR, DEFAULT_TARGET
from fnmatch import fnmatch
from copy import copy

import xml.etree.cElementTree as ElementTree
import xml.dom.minidom as minidom

CTS_DATA_DIR	= os.path.join(DEQP_DIR, "android", "cts")
APK_NAME 		= "com.drawelements.deqp.apk"

COPYRIGHT_DECLARATION = """
     Copyright (C) 2015 The Android Open Source Project

     Licensed under the Apache License, Version 2.0 (the "License");
     you may not use this file except in compliance with the License.
     You may obtain a copy of the License at

          http://www.apache.org/licenses/LICENSE-2.0

     Unless required by applicable law or agreed to in writing, software
     distributed under the License is distributed on an "AS IS" BASIS,
     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     See the License for the specific language governing permissions and
     limitations under the License.
     """

GENERATED_FILE_WARNING = """
     This file has been automatically generated. Edit with caution.
     """

class Configuration:
	def __init__ (self, name, glconfig, rotation, surfacetype, filters):
		self.name			= name
		self.glconfig		= glconfig
		self.rotation		= rotation
		self.surfacetype	= surfacetype
		self.filters		= filters

class Package:
	def __init__ (self, module, configurations):
		self.module			= module
		self.configurations	= configurations

class Mustpass:
	def __init__ (self, version, packages):
		self.version	= version
		self.packages	= packages

class Filter:
	TYPE_INCLUDE = 0
	TYPE_EXCLUDE = 1

	def __init__ (self, type, filename):
		self.type		= type
		self.filename	= filename

class TestRoot:
	def __init__ (self):
		self.children	= []

class TestGroup:
	def __init__ (self, name):
		self.name		= name
		self.children	= []

class TestCase:
	def __init__ (self, name):
		self.name			= name
		self.configurations	= []

class GLESVersion:
	def __init__(self, major, minor):
		self.major = major
		self.minor = minor

	def encode (self):
		return (self.major << 16) | (self.minor)

def getModuleGLESVersion (module):
	versions = {
		'dEQP-EGL':		GLESVersion(2,0),
		'dEQP-GLES2':	GLESVersion(2,0),
		'dEQP-GLES3':	GLESVersion(3,0),
		'dEQP-GLES31':	GLESVersion(3,1)
	}
	return versions[module.name]

def getSrcDir (mustpass):
	return os.path.join(CTS_DATA_DIR, mustpass.version, "src")

def getTmpDir (mustpass):
	return os.path.join(CTS_DATA_DIR, mustpass.version, "tmp")

def getModuleShorthand (module):
	assert module.name[:5] == "dEQP-"
	return module.name[5:].lower()

def getCaseListFileName (package, configuration):
	return "%s-%s.txt" % (getModuleShorthand(package.module), configuration.name)

def getDstCaseListPath (mustpass, package, configuration):
	return os.path.join(CTS_DATA_DIR, mustpass.version, getCaseListFileName(package, configuration))

def getCTSPackageName (package):
	return "com.drawelements.deqp." + getModuleShorthand(package.module)

def getCommandLine (config):
	return "--deqp-gl-config-name=%s --deqp-screen-rotation=%s --deqp-surface-type=%s --deqp-watchdog=enable" % (config.glconfig, config.rotation, config.surfacetype)

def readCaseList (filename):
	cases = []
	with open(filename, 'rb') as f:
		for line in f:
			if line[:6] == "TEST: ":
				cases.append(line[6:].strip())
	return cases

def getCaseList (buildCfg, generator, module):
	build(buildCfg, generator, [module.binName])
	genCaseList(buildCfg, generator, module, "txt")
	return readCaseList(getCaseListPath(buildCfg, module, "txt"))

def readPatternList (filename):
	ptrns = []
	with open(filename, 'rb') as f:
		for line in f:
			line = line.strip()
			if len(line) > 0 and line[0] != '#':
				ptrns.append(line)
	return ptrns

def applyPatterns (caseList, patterns, filename, op):
	matched			= set()
	errors			= []
	curList			= copy(caseList)
	trivialPtrns	= [p for p in patterns if p.find('*') < 0]
	regularPtrns	= [p for p in patterns if p.find('*') >= 0]

	# Apply trivial (just case paths)
	allCasesSet		= set(caseList)
	for path in trivialPtrns:
		if path in allCasesSet:
			if path in matched:
				errors.append((path, "Same case specified more than once"))
			matched.add(path)
		else:
			errors.append((path, "Test case not found"))

	curList = [c for c in curList if c not in matched]

	for pattern in regularPtrns:
		matchedThisPtrn = set()

		for case in curList:
			if fnmatch(case, pattern):
				matchedThisPtrn.add(case)

		if len(matchedThisPtrn) == 0:
			errors.append((pattern, "Pattern didn't match any cases"))

		matched	= matched | matchedThisPtrn
		curList = [c for c in curList if c not in matched]

	for pattern, reason in errors:
		print "ERROR: %s: %s" % (reason, pattern)

	if len(errors) > 0:
		die("Found %s invalid patterns while processing file %s" % (len(errors), filename))

	return [c for c in caseList if op(c in matched)]

def applyInclude (caseList, patterns, filename):
	return applyPatterns(caseList, patterns, filename, lambda b: b)

def applyExclude (caseList, patterns, filename):
	return applyPatterns(caseList, patterns, filename, lambda b: not b)

def readPatternLists (mustpass):
	lists = {}
	for package in mustpass.packages:
		for cfg in package.configurations:
			for filter in cfg.filters:
				if not filter.filename in lists:
					lists[filter.filename] = readPatternList(os.path.join(getSrcDir(mustpass), filter.filename))
	return lists

def applyFilters (caseList, patternLists, filters):
	res = copy(caseList)
	for filter in filters:
		ptrnList = patternLists[filter.filename]
		if filter.type == Filter.TYPE_INCLUDE:
			res = applyInclude(res, ptrnList, filter.filename)
		else:
			assert filter.type == Filter.TYPE_EXCLUDE
			res = applyExclude(res, ptrnList, filter.filename)
	return res

def appendToHierarchy (root, casePath):
	def findChild (node, name):
		for child in node.children:
			if child.name == name:
				return child
		return None

	curNode		= root
	components	= casePath.split('.')

	for component in components[:-1]:
		nextNode = findChild(curNode, component)
		if not nextNode:
			nextNode = TestGroup(component)
			curNode.children.append(nextNode)
		curNode = nextNode

	if not findChild(curNode, components[-1]):
		curNode.children.append(TestCase(components[-1]))

def buildTestHierachy (caseList):
	root = TestRoot()
	for case in caseList:
		appendToHierarchy(root, case)
	return root

def buildTestCaseMap (root):
	caseMap = {}

	def recursiveBuild (curNode, prefix):
		curPath = prefix + curNode.name
		if isinstance(curNode, TestCase):
			caseMap[curPath] = curNode
		else:
			for child in curNode.children:
				recursiveBuild(child, curPath + '.')

	for child in root.children:
		recursiveBuild(child, '')

	return caseMap

def include (filename):
	return Filter(Filter.TYPE_INCLUDE, filename)

def exclude (filename):
	return Filter(Filter.TYPE_EXCLUDE, filename)

def prettifyXML (doc):
	doc.insert(0, ElementTree.Comment(COPYRIGHT_DECLARATION))
	doc.insert(1, ElementTree.Comment(GENERATED_FILE_WARNING))
	uglyString	= ElementTree.tostring(doc, 'utf-8')
	reparsed	= minidom.parseString(uglyString)
	return reparsed.toprettyxml(indent='\t', encoding='utf-8')

def genCTSPackageXML (package, root):
	def isLeafGroup (testGroup):
		numGroups	= 0
		numTests	= 0

		for child in testGroup.children:
			if isinstance(child, TestCase):
				numTests += 1
			else:
				numGroups += 1

		assert numGroups + numTests > 0

		if numGroups > 0 and numTests > 0:
			die("Mixed groups and cases in %s" % testGroup.name)

		return numGroups == 0

	def makeConfiguration (parentElem, configuration):
		return ElementTree.SubElement(parentElem, "TestInstance", glconfig=configuration.glconfig, rotation=configuration.rotation, surfacetype=configuration.surfacetype)

	def makeTestCase (parentElem, testCase):
		caseElem = ElementTree.SubElement(parentElem, "Test", name=testCase.name)
		for config in testCase.configurations:
			makeConfiguration(caseElem, config)
		return caseElem

	def makeTestGroup (parentElem, testGroup):
		groupElem = ElementTree.SubElement(parentElem, "TestCase" if isLeafGroup(testGroup) else "TestSuite", name=testGroup.name)
		for child in testGroup.children:
			if isinstance(child, TestCase):
				makeTestCase(groupElem, child)
			else:
				makeTestGroup(groupElem, child)
		return groupElem

	pkgElem = ElementTree.Element("TestPackage",
								  name				= package.module.name,
								  appPackageName	= getCTSPackageName(package),
								  testType			= "deqpTest")

	pkgElem.set("xmlns:deqp", "http://drawelements.com/deqp")
	pkgElem.set("deqp:glesVersion", str(getModuleGLESVersion(package.module).encode()))

	for child in root.children:
		makeTestGroup(pkgElem, child)

	return pkgElem

def genSpecXML (mustpass):
	mustpassElem = ElementTree.Element("Mustpass", version = mustpass.version)

	for package in mustpass.packages:
		packageElem = ElementTree.SubElement(mustpassElem, "TestPackage", name = package.module.name)

		for config in package.configurations:
			configElem = ElementTree.SubElement(packageElem, "Configuration",
												name			= config.name,
												caseListFile	= getCaseListFileName(package, config),
												commandLine		= getCommandLine(config))

	return mustpassElem

def addOptionElement (parent, optionName, optionValue):
	ElementTree.SubElement(parent, "option", name=optionName, value=optionValue)

def genAndroidTestXml (mustpass):
	INSTALLER_CLASS = "com.android.compatibility.common.tradefed.targetprep.ApkInstaller"
	RUNNER_CLASS = "com.drawelements.deqp.runner.DeqpTestRunner"
	configElement = ElementTree.Element("configuration")
	preparerElement = ElementTree.SubElement(configElement, "target_preparer")
	preparerElement.set("class", INSTALLER_CLASS)
	addOptionElement(preparerElement, "cleanup-apks", "true")
	addOptionElement(preparerElement, "test-file-name", APK_NAME)

	for package in mustpass.packages:
		for config in package.configurations:
			testElement = ElementTree.SubElement(configElement, "test")
			testElement.set("class", RUNNER_CLASS)
			addOptionElement(testElement, "deqp-package", package.module.name)
			addOptionElement(testElement, "deqp-caselist-file", getCaseListFileName(package,config))
			# \todo [2015-10-16 kalle]: Replace with just command line? - requires simplifications in the runner/tests as well.
			addOptionElement(testElement, "deqp-gl-config-name", config.glconfig)
			addOptionElement(testElement, "deqp-surface-type", config.surfacetype)
			addOptionElement(testElement, "deqp-screen-rotation", config.rotation)

	return configElement


def genMustpass (mustpass, moduleCaseLists):
	print "Generating mustpass '%s'" % mustpass.version

	patternLists = readPatternLists(mustpass)

	for package in mustpass.packages:
		allCasesInPkg		= moduleCaseLists[package.module]
		matchingByConfig	= {}
		allMatchingSet		= set()

		for config in package.configurations:
			filtered	= applyFilters(allCasesInPkg, patternLists, config.filters)
			dstFile		= getDstCaseListPath(mustpass, package, config)

			print "  Writing deqp caselist: " + dstFile
			writeFile(dstFile, "\n".join(filtered) + "\n")

			matchingByConfig[config]	= filtered
			allMatchingSet				= allMatchingSet | set(filtered)

		allMatchingCases	= [c for c in allCasesInPkg if c in allMatchingSet] # To preserve ordering
		root				= buildTestHierachy(allMatchingCases)
		testCaseMap			= buildTestCaseMap(root)

		for config in package.configurations:
			for case in matchingByConfig[config]:
				testCaseMap[case].configurations.append(config)

		# NOTE: CTS v2 does not need package XML files. Remove when transition is complete.
		packageXml	= genCTSPackageXML(package, root)
		xmlFilename	= os.path.join(CTS_DATA_DIR, mustpass.version, getCTSPackageName(package) + ".xml")

		print "  Writing CTS caselist: " + xmlFilename
		writeFile(xmlFilename, prettifyXML(packageXml))

	specXML			= genSpecXML(mustpass)
	specFilename	= os.path.join(CTS_DATA_DIR, mustpass.version, "mustpass.xml")

	print "  Writing spec: " + specFilename
	writeFile(specFilename, prettifyXML(specXML))

	# TODO: Which is the best selector mechanism?
	if (mustpass.version == "mnc"):
		androidTestXML		= genAndroidTestXml(mustpass)
		androidTestFilename	= os.path.join(CTS_DATA_DIR, "AndroidTest.xml")

		print "  Writing AndroidTest.xml: " + androidTestFilename
		writeFile(androidTestFilename, prettifyXML(androidTestXML))

	print "Done!"

def genMustpassLists (mustpassLists, generator, buildCfg):
	moduleCaseLists = {}

	# Getting case lists involves invoking build, so we want to cache the results
	for mustpass in mustpassLists:
		for package in mustpass.packages:
			if not package.module in moduleCaseLists:
				moduleCaseLists[package.module] = getCaseList(buildCfg, generator, package.module)

	for mustpass in mustpassLists:
		genMustpass(mustpass, moduleCaseLists)

EGL_MODULE						= Module(name = "dEQP-EGL", dirName = "egl", binName = "deqp-egl")
GLES2_MODULE					= Module(name = "dEQP-GLES2", dirName = "gles2", binName = "deqp-gles2")
GLES3_MODULE					= Module(name = "dEQP-GLES3", dirName = "gles3", binName = "deqp-gles3")
GLES31_MODULE					= Module(name = "dEQP-GLES31", dirName = "gles31", binName = "deqp-gles31")

# Lollipop

LMP_GLES3_PKG					= Package(module = GLES3_MODULE, configurations = [
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= [include("es30-lmp.txt")]),
	])
LMP_GLES31_PKG					= Package(module = GLES31_MODULE, configurations = [
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= [include("es31-lmp.txt")]),
	])

# Lollipop MR1

LMP_MR1_GLES3_PKG				= Package(module = GLES3_MODULE, configurations = [
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= [include("es30-lmp-mr1.txt")]),
	])
LMP_MR1_GLES31_PKG				= Package(module = GLES31_MODULE, configurations = [
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= [include("es31-lmp-mr1.txt")]),
	])

# Marshmallow

MNC_EGL_PKG						= Package(module = EGL_MODULE, configurations = [
		# Master
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= [include("egl-master.txt")]),
	])
MNC_GLES2_PKG					= Package(module = GLES2_MODULE, configurations = [
		# Master
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= [include("gles2-master.txt")]),
	])
MNC_GLES3_PKG					= Package(module = GLES3_MODULE, configurations = [
		# Master
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= [include("gles3-master.txt")]),
		# Rotations
		Configuration(name			= "rotate-portrait",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "0",
					  surfacetype	= "window",
					  filters		= [include("gles3-master.txt"), include("gles3-rotation.txt")]),
		Configuration(name			= "rotate-landscape",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "90",
					  surfacetype	= "window",
					  filters		= [include("gles3-master.txt"), include("gles3-rotation.txt")]),
		Configuration(name			= "rotate-reverse-portrait",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "180",
					  surfacetype	= "window",
					  filters		= [include("gles3-master.txt"), include("gles3-rotation.txt")]),
		Configuration(name			= "rotate-reverse-landscape",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "270",
					  surfacetype	= "window",
					  filters		= [include("gles3-master.txt"), include("gles3-rotation.txt")]),

		# MSAA
		Configuration(name			= "multisample",
					  glconfig		= "rgba8888d24s8ms4",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= [include("gles3-master.txt"),
									   include("gles3-multisample.txt"),
									   exclude("gles3-multisample-issues.txt")]),

		# Pixel format
		Configuration(name			= "565-no-depth-no-stencil",
					  glconfig		= "rgb565d0s0ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= [include("gles3-master.txt"),
									   include("gles3-pixelformat.txt"),
									   exclude("gles3-pixelformat-issues.txt")]),
	])
MNC_GLES31_PKG					= Package(module = GLES31_MODULE, configurations = [
		# Master
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= [include("gles31-master.txt")]),

		# Rotations
		Configuration(name			= "rotate-portrait",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "0",
					  surfacetype	= "window",
					  filters		= [include("gles31-master.txt"), include("gles31-rotation.txt")]),
		Configuration(name			= "rotate-landscape",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "90",
					  surfacetype	= "window",
					  filters		= [include("gles31-master.txt"), include("gles31-rotation.txt")]),
		Configuration(name			= "rotate-reverse-portrait",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "180",
					  surfacetype	= "window",
					  filters		= [include("gles31-master.txt"), include("gles31-rotation.txt")]),
		Configuration(name			= "rotate-reverse-landscape",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "270",
					  surfacetype	= "window",
					  filters		= [include("gles31-master.txt"), include("gles31-rotation.txt")]),

		# MSAA
		Configuration(name			= "multisample",
					  glconfig		= "rgba8888d24s8ms4",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= [include("gles31-master.txt"), include("gles31-multisample.txt")]),

		# Pixel format
		Configuration(name			= "565-no-depth-no-stencil",
					  glconfig		= "rgb565d0s0ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= [include("gles31-master.txt"), include("gles31-pixelformat.txt")]),
	])

# Master

MASTER_EGL_COMMON_FILTERS		= [include("egl-master.txt")]
MASTER_EGL_PKG					= Package(module = EGL_MODULE, configurations = [
		# Master
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= MASTER_EGL_COMMON_FILTERS),
	])

MASTER_GLES2_COMMON_FILTERS		= [
		include("gles2-master.txt"),
		exclude("gles2-test-issues.txt"),
		exclude("gles2-failures.txt")
	]
MASTER_GLES2_PKG				= Package(module = GLES2_MODULE, configurations = [
		# Master
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= MASTER_GLES2_COMMON_FILTERS),
	])

MASTER_GLES3_COMMON_FILTERS		= [
		include("gles3-master.txt"),
		exclude("gles3-hw-issues.txt"),
		exclude("gles3-driver-issues.txt"),
		exclude("gles3-test-issues.txt"),
		exclude("gles3-spec-issues.txt")
	]
MASTER_GLES3_PKG				= Package(module = GLES3_MODULE, configurations = [
		# Master
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= MASTER_GLES3_COMMON_FILTERS),
		# Rotations
		Configuration(name			= "rotate-portrait",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "0",
					  surfacetype	= "window",
					  filters		= MASTER_GLES3_COMMON_FILTERS + [include("gles3-rotation.txt")]),
		Configuration(name			= "rotate-landscape",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "90",
					  surfacetype	= "window",
					  filters		= MASTER_GLES3_COMMON_FILTERS + [include("gles3-rotation.txt")]),
		Configuration(name			= "rotate-reverse-portrait",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "180",
					  surfacetype	= "window",
					  filters		= MASTER_GLES3_COMMON_FILTERS + [include("gles3-rotation.txt")]),
		Configuration(name			= "rotate-reverse-landscape",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "270",
					  surfacetype	= "window",
					  filters		= MASTER_GLES3_COMMON_FILTERS + [include("gles3-rotation.txt")]),

		# MSAA
		Configuration(name			= "multisample",
					  glconfig		= "rgba8888d24s8ms4",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= MASTER_GLES3_COMMON_FILTERS + [include("gles3-multisample.txt"),
																	 exclude("gles3-multisample-issues.txt")]),

		# Pixel format
		Configuration(name			= "565-no-depth-no-stencil",
					  glconfig		= "rgb565d0s0ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= MASTER_GLES3_COMMON_FILTERS + [include("gles3-pixelformat.txt"),
																	 exclude("gles3-pixelformat-issues.txt")]),
	])

MASTER_GLES31_COMMON_FILTERS	= [
		include("gles31-master.txt"),
		exclude("gles31-hw-issues.txt"),
		exclude("gles31-driver-issues.txt"),
		exclude("gles31-test-issues.txt"),
		exclude("gles31-spec-issues.txt"),
	]
MASTER_GLES31_PKG				= Package(module = GLES31_MODULE, configurations = [
		# Master
		Configuration(name			= "master",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= MASTER_GLES31_COMMON_FILTERS),

		# Rotations
		Configuration(name			= "rotate-portrait",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "0",
					  surfacetype	= "window",
					  filters		= MASTER_GLES31_COMMON_FILTERS + [include("gles31-rotation.txt")]),
		Configuration(name			= "rotate-landscape",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "90",
					  surfacetype	= "window",
					  filters		= MASTER_GLES31_COMMON_FILTERS + [include("gles31-rotation.txt")]),
		Configuration(name			= "rotate-reverse-portrait",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "180",
					  surfacetype	= "window",
					  filters		= MASTER_GLES31_COMMON_FILTERS + [include("gles31-rotation.txt")]),
		Configuration(name			= "rotate-reverse-landscape",
					  glconfig		= "rgba8888d24s8ms0",
					  rotation		= "270",
					  surfacetype	= "window",
					  filters		= MASTER_GLES31_COMMON_FILTERS + [include("gles31-rotation.txt")]),

		# MSAA
		Configuration(name			= "multisample",
					  glconfig		= "rgba8888d24s8ms4",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= MASTER_GLES31_COMMON_FILTERS + [include("gles31-multisample.txt")]),

		# Pixel format
		Configuration(name			= "565-no-depth-no-stencil",
					  glconfig		= "rgb565d0s0ms0",
					  rotation		= "unspecified",
					  surfacetype	= "window",
					  filters		= MASTER_GLES31_COMMON_FILTERS + [include("gles31-pixelformat.txt")]),
	])

MUSTPASS_LISTS				= [
		Mustpass(version = "lmp",		packages = [LMP_GLES3_PKG, LMP_GLES31_PKG]),
		Mustpass(version = "lmp-mr1",	packages = [LMP_MR1_GLES3_PKG, LMP_MR1_GLES31_PKG]),
		Mustpass(version = "mnc",		packages = [MNC_EGL_PKG, MNC_GLES2_PKG, MNC_GLES3_PKG, MNC_GLES31_PKG]),
		Mustpass(version = "master",	packages = [MASTER_EGL_PKG, MASTER_GLES2_PKG, MASTER_GLES3_PKG, MASTER_GLES31_PKG])
	]

if __name__ == "__main__":
	genMustpassLists(MUSTPASS_LISTS, ANY_GENERATOR, getBuildConfig(DEFAULT_BUILD_DIR, DEFAULT_TARGET, "Debug"))
