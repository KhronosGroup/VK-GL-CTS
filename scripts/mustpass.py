# -*- coding: utf-8 -*-

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
from ctsbuild.config import ANY_GENERATOR
from ctsbuild.build import build
from build_caselists import Module, getModuleByName, getBuildConfig, genCaseList, getCaseListPath, DEFAULT_BUILD_DIR, DEFAULT_TARGET
from fnmatch import fnmatch
from copy import copy
from collections import defaultdict

import argparse
import xml.etree.cElementTree as ElementTree
import xml.dom.minidom as minidom

APK_NAME		= "com.drawelements.deqp.apk"

GENERATED_FILE_WARNING = """
     This file has been automatically generated. Edit with caution.
     """

class Project:
	def __init__ (self, path, copyright = None):
		self.path		= path
		self.copyright	= copyright

class Configuration:
	def __init__ (self, name, filters, glconfig = None, rotation = None, surfacetype = None, required = False, runtime = None, runByDefault = True, listOfGroupsToSplit = []):
		self.name					= name
		self.glconfig				= glconfig
		self.rotation				= rotation
		self.surfacetype			= surfacetype
		self.required				= required
		self.filters				= filters
		self.expectedRuntime		= runtime
		self.runByDefault			= runByDefault
		self.listOfGroupsToSplit	= listOfGroupsToSplit

class Package:
	def __init__ (self, module, configurations):
		self.module			= module
		self.configurations	= configurations

class Mustpass:
	def __init__ (self, project, version, packages):
		self.project	= project
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
	return versions[module.name] if module.name in versions else None

def getSrcDir (mustpass):
	return os.path.join(mustpass.project.path, mustpass.version, "src")

def getTmpDir (mustpass):
	return os.path.join(mustpass.project.path, mustpass.version, "tmp")

def getModuleShorthand (module):
	assert module.name[:5] == "dEQP-"
	return module.name[5:].lower()

def getCaseListFileName (package, configuration):
	return "%s-%s.txt" % (getModuleShorthand(package.module), configuration.name)

def getDstCaseListPath (mustpass):
	return os.path.join(mustpass.project.path, mustpass.version)

def getCTSPackageName (package):
	return "com.drawelements.deqp." + getModuleShorthand(package.module)

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

def readCaseDict (filename):
	# read all cases and organize them in a tree; this is needed for chunked mustpass
	# groups are stored as dictionaries and cases as list of strings with full case paths
	groupTree = {}
	# limit how deep constructed tree should be - this later simplifies applying filters;
	# if in future we will need to split to separate .txt files deeper groups thet this value should be increased
	limitGroupTreeDepth = 3
	# create helper stack that will contain references to currently filled groups, from top to bottom
	groupStack = None
	# cretae variable that will hold currentlt processed line from the file
	processedLine = None
	with open(filename, 'rt') as f:
		for nextLine in f:
			# to be able to build tree structure we need to know what is the next line in the file this is
			# why the first line read from the file will be actually processed during the second iteration
			if processedLine is None:
				processedLine		= nextLine
				# to simplify code use this section to also extract root node name
				rootName			= processedLine[7:processedLine.rfind('.')]
				groupTree[rootName]	= {}
				groupStack			= [groupTree[rootName]]
				continue
			# check if currently processed line is a test case or a group
			processedEntryType = processedLine[:6]
			if processedEntryType == "TEST: ":
				# append this test case to the last group on the stack
				groupStack[-1].append(processedLine[6:].strip())
			elif processedEntryType == "GROUP:":
				# count number of dots in path to determine what is the depth of current group in the tree
				processedGroupDepth = processedLine.count('.')
				# limit tree construction just to specified level
				availableLimit = limitGroupTreeDepth - processedGroupDepth
				if availableLimit > 0:
					# check how deep is stack currently
					groupStackDepth = len(groupStack)
					# if stack is deeper then depth of current group then we need to pop number of items
					if processedGroupDepth < groupStackDepth:
						groupStack = groupStack[:groupStackDepth-(groupStackDepth-processedGroupDepth)]
					# get group that will have new child - this is the last group on the stack
					parentGroup = groupStack[-1]
					# add new dict that will contain other groups or list of cases depending on the next line
					# and available depth limit (if are about to reach limit we won't add group dictionaries
					# but just add all cases from deeper groups to the group at this depth)
					processedGroupName = processedLine[7:-1]
					parentGroup[processedGroupName] = {} if (nextLine[:6] == "GROUP:") and (availableLimit > 1) else []
					# add new group to the stack (items in groupStack can be either list or dict)
					groupStack.append(parentGroup[processedGroupName])
			# before going to the next line set procesedLine for the next iteration
			processedLine = nextLine
	# handle last test case - we need to do it after the loop as in the loop we needed to know what is the next line
	assert(processedLine[:6] == "TEST: ")
	groupStack[-1].append(processedLine[6:].strip())
	return groupTree

def getCaseDict (buildCfg, generator, module):
	build(buildCfg, generator, [module.binName])
	genCaseList(buildCfg, generator, module, "txt")
	return readCaseDict(getCaseListPath(buildCfg, module, "txt"))

def readPatternList (filename):
	ptrns = []
	with open(filename, 'rt') as f:
		for line in f:
			line = line.strip()
			if len(line) > 0 and line[0] != '#':
				ptrns.append(line)
	return ptrns


def constructNewDict(oldDict, listOfCases, op = lambda a: not a):
	# Helper function used to construct case dictionary without specific cases
	rootName		= list(oldDict.keys())[0]
	newDict			= {rootName : {}}
	newDictStack	= [newDict]
	oldDictStack	= [oldDict]
	while True:
		# mak sure that both stacks have same number of items
		assert(len(oldDictStack) == len(newDictStack))
		# when all items from stack were processed then we can exit the loop
		if len(oldDictStack) == 0:
			break
		# grab last item from both stacks
		itemOnOldStack = oldDictStack.pop()
		itemOnNewStack = newDictStack.pop()
		# if item on stack is dictionary then it represents groups and
		# we need to reconstruct them in new dictionary
		if type(itemOnOldStack) is dict:
			assert(type(itemOnNewStack) is dict)
			listOfGroups = list(itemOnOldStack.keys())
			for groupName in listOfGroups:
				# create list or dictionary depending on contnent of child group
				doesGroupsContainCases = type(itemOnOldStack[groupName]) is list
				itemOnNewStack[groupName] = [] if doesGroupsContainCases else {}
				# append groups on stacks
				assert(type(itemOnNewStack[groupName]) == type(itemOnOldStack[groupName]))
				newDictStack.append(itemOnNewStack[groupName])
				oldDictStack.append(itemOnOldStack[groupName])
		else:
			# if item on stack is list then it represents group that contain cases we need
			# to apply filter on each of them to make sure only proper cases are appended
			assert(type(itemOnOldStack) is list)
			assert(type(itemOnNewStack) is list)
			for caseName in itemOnOldStack:
				if op(caseName in listOfCases):
					itemOnNewStack.append(caseName)
	return newDict

def constructSet(caseDict, perGroupOperation):
	casesSet		= set()
	dictStack		= [caseDict]
	while True:
		# when all items from stack were processed then we can exit the loop
		if len(dictStack) == 0:
			break
		# grab last item from stack
		itemOnStack = dictStack.pop()
		# if item on stack is dictionary then it represents groups and we need to add them to stack
		if type(itemOnStack) is dict:
			for groupName in itemOnStack.keys():
				dictStack.append(itemOnStack[groupName])
		else:
			# if item on stack is a list of cases we can add them to set containing all cases
			assert(type(itemOnStack) is list)
			casesSet = perGroupOperation(casesSet, itemOnStack)
	return casesSet

def applyPatterns (caseDict, patterns, filename, op):
	matched			= set()
	errors			= []
	trivialPtrns	= [p for p in patterns if p.find('*') < 0]
	regularPtrns	= [p for p in patterns if p.find('*') >= 0]

	# Construct helper set that contains cases from all groups
	unionOperation	= lambda resultCasesSet, groupCaseList: resultCasesSet.union(set(groupCaseList))
	allCasesSet		= constructSet(caseDict, unionOperation)

	# Apply trivial patterns - plain case paths without wildcard
	for path in trivialPtrns:
		if path in allCasesSet:
			if path in matched:
				errors.append((path, "Same case specified more than once"))
			matched.add(path)
		else:
			errors.append((path, "Test case not found"))

	# Construct new dictionary but without already matched paths
	curDict = constructNewDict(caseDict, matched)

	# Apply regular patterns - paths with wildcard
	for pattern in regularPtrns:

		# Helper function that checks if cases from case group match pattern
		def matchOperation(resultCasesSet, groupCaseList):
			for caseName in groupCaseList:
				if fnmatch(caseName, pattern):
					resultCasesSet.add(caseName)
			return resultCasesSet

		matchedThisPtrn = constructSet(curDict, matchOperation)

		if len(matchedThisPtrn) == 0:
			errors.append((pattern, "Pattern didn't match any cases"))

		matched = matched | matchedThisPtrn

		# To speed up search construct smaller case dictionary without already matched paths
		curDict = constructNewDict(curDict, matched)

	for pattern, reason in errors:
		print("ERROR: %s: %s" % (reason, pattern))

	if len(errors) > 0:
		die("Found %s invalid patterns while processing file %s" % (len(errors), filename))

	# Construct final dictionary using aproperiate operation
	return constructNewDict(caseDict, matched, op)

def applyInclude (caseDict, patterns, filename):
	return applyPatterns(caseDict, patterns, filename, lambda b: b)

def applyExclude (caseDict, patterns, filename):
	return applyPatterns(caseDict, patterns, filename, lambda b: not b)

def readPatternLists (mustpass):
	lists = {}
	for package in mustpass.packages:
		for cfg in package.configurations:
			for filter in cfg.filters:
				if not filter.filename in lists:
					lists[filter.filename] = readPatternList(os.path.join(getSrcDir(mustpass), filter.filename))
	return lists

def applyFilters (caseDict, patternLists, filters):
	res = copy(caseDict)
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

def insertXMLHeaders (mustpass, doc):
	if mustpass.project.copyright != None:
		doc.insert(0, ElementTree.Comment(mustpass.project.copyright))
	doc.insert(1, ElementTree.Comment(GENERATED_FILE_WARNING))

def prettifyXML (doc):
	uglyString	= ElementTree.tostring(doc, 'utf-8')
	reparsed	= minidom.parseString(uglyString)
	return reparsed.toprettyxml(indent='\t', encoding='utf-8')

def genSpecXML (mustpass):
	mustpassElem = ElementTree.Element("Mustpass", version = mustpass.version)
	insertXMLHeaders(mustpass, mustpassElem)

	for package in mustpass.packages:
		packageElem = ElementTree.SubElement(mustpassElem, "TestPackage", name = package.module.name)

		for config in package.configurations:
			configElem = ElementTree.SubElement(packageElem, "Configuration",
												caseListFile	= getCaseListFileName(package, config),
												commandLine		= getCommandLine(config),
												name			= config.name)

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

def genMustpass (mustpass, moduleCaseDicts):
	print("Generating mustpass '%s'" % mustpass.version)

	patternLists = readPatternLists(mustpass)

	for package in mustpass.packages:
		allCasesInPkgDict	= moduleCaseDicts[package.module]

		for config in package.configurations:

			# construct dictionary with all filters applyed
			filteredCaseDict	= applyFilters(allCasesInPkgDict, patternLists, config.filters)

			# construct components of path to main destination file
			mainDstFilePath		= getDstCaseListPath(mustpass)
			mainDstFileName		= getCaseListFileName(package, config)
			mainDstFile			= os.path.join(mainDstFilePath, mainDstFileName)
			mainGruopSubDir		= mainDstFileName[:-4]

			# if case paths should be split to multiple files then main
			# destination file will contain paths to individual files containing cases
			if len(config.listOfGroupsToSplit) > 0:
				# make sure directory for group files exists
				rootGroupPath = os.path.join(mainDstFilePath, mainGruopSubDir)
				if not os.path.exists(rootGroupPath):
					os.makedirs(rootGroupPath)

				# iterate over case dictionary and split it to .txt files acording to
				# groups that were specified in config.listOfGroupsToSplit
				splitedGroupsDict	= {}
				dictStack			= [filteredCaseDict]
				helperListStack		= [ [] ]
				while True:
					# when all items from stack were processed then we can exit the loop
					if len(dictStack) == 0:
						break
					assert(len(dictStack) == len(helperListStack))
					# grab last item from stack
					itemOnStack = dictStack.pop()
					caseListFromHelperStack = helperListStack.pop()
					# if item on stack is dictionary then it represents groups and we need to add them to stack
					if type(itemOnStack) is dict:
						for groupName in sorted(itemOnStack):

							# check if this group should be split to multiple .txt files
							if groupName in config.listOfGroupsToSplit:
								# we can split only groups that contain other groups,
								# listOfGroupsToSplit should not contain groups that contain test cases
								assert(type(itemOnStack[groupName]) is dict)
								# add child groups of this group to splitedGroupsDict
								for childGroupName in itemOnStack[groupName]:
									# make sure that child group should not be splited
									# (if it should then this will be handle in one of the next iterations)
									if childGroupName not in config.listOfGroupsToSplit:
										splitedGroupsDict[childGroupName] = []

							# add this group to stack used for iteration over casses tree
							dictStack.append(itemOnStack[groupName])

							# decide what list we should append to helperListStack;
							# if this group represents one of individual .txt files then grab
							# propper array of cases from splitedGroupsDict and add it to helper stack;
							# if groupName is not in splitedGroupsDict then use the same list as was used
							# by parent group (we are merging casses from those groups to single .txt file)
							helperListStack.append(splitedGroupsDict.get(groupName, caseListFromHelperStack))
					else:
						# if item on stack is a list of cases we can add them to proper list
						assert(type(itemOnStack) is list)
						caseListFromHelperStack.extend(itemOnStack)

				print("  Writing separated caselists:")
				groupPathsList = []
				for groupPath in splitedGroupsDict:
					# skip groups that after filtering have no casses left
					if len(splitedGroupsDict[groupPath]) == 0:
						continue
					# remove root node name from the beginning of group and replace all '_' with '-'
					processedGroupPath = groupPath[groupPath.find('.')+1:].replace('_', '-')
					# split group paths
					groupList = processedGroupPath.split('.')
					groupSubDir = '/'
					# create subdirectories if there is more then one group name in groupList
					path = rootGroupPath
					if len(groupList) > 1:
						for groupName in groupList[:-1]:
							# make sure directory for group files exists
							groupSubDir	= groupSubDir + groupName + '/'
							path		= os.path.join(path, groupName)
							if not os.path.exists(path):
								os.makedirs(path)
					# construct path to .txt file and save all cases
					groupDstFileName	= groupList[-1] + ".txt"
					groupDstFileFullDir	= os.path.join(path, groupDstFileName)
					groupPathsList.append(mainGruopSubDir + groupSubDir + groupDstFileName)
					print("    " + groupDstFileFullDir)
					writeFile(groupDstFileFullDir, "\n".join(splitedGroupsDict[groupPath]) + "\n")

				# write file containing names of all group files
				print("  Writing file containing list of separated case files: " + mainDstFile)
				groupPathsList.sort()
				writeFile(mainDstFile, "\n".join(groupPathsList) + "\n")
			else:
				# merge all cases to single case list
				filteredCaseList	= []
				dictStack			= [filteredCaseDict]
				while True:
					# when all items from stack were processed then we can exit the loop
					if len(dictStack) == 0:
						break
					# grab last item from stack
					itemOnStack = dictStack.pop()
					# if item on stack is dictionary then it represents groups and we need to add them to stack
					if type(itemOnStack) is dict:
						for groupName in itemOnStack.keys():
							dictStack.append(itemOnStack[groupName])
					else:
						# if item on stack is a list of cases we can add them to filteredCaseList
						assert(type(itemOnStack) is list)
						filteredCaseList.extend(itemOnStack)
				# write file containing all cases
				if len(filteredCaseList) > 0:
					print("  Writing deqp caselist: " + mainDstFile)
					writeFile(mainDstFile, "\n".join(filteredCaseList) + "\n")

	specXML = genSpecXML(mustpass)
	specFilename = os.path.join(mustpass.project.path, mustpass.version, "mustpass.xml")

	print("  Writing spec: " + specFilename)
	writeFile(specFilename, prettifyXML(specXML).decode())

	# TODO: Which is the best selector mechanism?
	if (mustpass.version == "master"):
		androidTestXML		= genAndroidTestXml(mustpass)
		androidTestFilename	= os.path.join(mustpass.project.path, "AndroidTest.xml")

		print("  Writing AndroidTest.xml: " + androidTestFilename)
		writeFile(androidTestFilename, prettifyXML(androidTestXML).decode())

	print("Done!")

def genMustpassLists (mustpassLists, generator, buildCfg):
	moduleCaseDicts = {}

	# Getting case lists involves invoking build, so we want to cache the results
	for mustpass in mustpassLists:
		for package in mustpass.packages:
			if not package.module in moduleCaseDicts:
				moduleCaseDicts[package.module] = getCaseDict(buildCfg, generator, package.module)

	for mustpass in mustpassLists:
		genMustpass(mustpass, moduleCaseDicts)

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
