# -*- coding: utf-8 -*-

#-------------------------------------------------------------------------
# Vulkan CTS
# ----------
#
# Copyright (c) 2016 Google Inc.
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

from fnmatch import fnmatch

sys.path.append(os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))
sys.path.append(os.path.join(os.path.dirname(__file__), "..", "..", "scripts", "log"))

from build.common import readFile
from log_parser import StatusCode, BatchResultParser

ALLOWED_STATUS_CODES = set([
		StatusCode.PASS,
		StatusCode.NOT_SUPPORTED,
		StatusCode.QUALITY_WARNING,
		StatusCode.COMPATIBILITY_WARNING
	])

STATEMENT_PATTERN	= "STATEMENT-*"
TEST_LOG_PATTERN	= "*.qpa"
GIT_STATUS_PATTERN	= "git-status.txt"
GIT_LOG_PATTERN		= "git-log.txt"
PATCH_PATTERN		= "*.patch"

class PackageDescription:
	def __init__ (self, basePath, statement, testLogs, gitStatus, gitLog, patches, otherItems):
		self.basePath		= basePath
		self.statement		= statement
		self.testLogs		= testLogs
		self.gitStatus		= gitStatus
		self.gitLog			= gitLog
		self.patches		= patches
		self.otherItems		= otherItems

class ValidationMessage:
	TYPE_ERROR		= 0
	TYPE_WARNING	= 1

	def __init__ (self, type, filename, message):
		self.type		= type
		self.filename	= filename
		self.message	= message

	def __str__ (self):
		prefix = {self.TYPE_ERROR: "ERROR: ", self.TYPE_WARNING: "WARNING: "}
		return prefix[self.type] + os.path.basename(self.filename) + ": " + self.message

def error (filename, message):
	return ValidationMessage(ValidationMessage.TYPE_ERROR, filename, message)

def warning (filename, message):
	return ValidationMessage(ValidationMessage.TYPE_WARNING, filename, message)

def getPackageDescription (packagePath):
	allItems	= os.listdir(packagePath)
	statement	= None
	testLogs	= []
	gitStatus	= None
	gitLog		= None
	patches		= []
	otherItems	= []

	for item in allItems:
		if fnmatch(item, STATEMENT_PATTERN):
			assert statement == None
			statement = item
		elif fnmatch(item, TEST_LOG_PATTERN):
			testLogs.append(item)
		elif fnmatch(item, GIT_STATUS_PATTERN):
			assert gitStatus == None
			gitStatus = item
		elif fnmatch(item, GIT_LOG_PATTERN):
			assert gitLog == None
			gitLog = item
		elif fnmatch(item, PATCH_PATTERN):
			patches.append(item)
		else:
			otherItems.append(item)

	return PackageDescription(packagePath, statement, testLogs, gitStatus, gitLog, patches, otherItems)

def readMustpass (filename):
	f = open(filename, 'rb')
	cases = []
	for line in f:
		s = line.strip()
		if len(s) > 0:
			cases.append(s)
	return cases

def readTestLog (filename):
	parser = BatchResultParser()
	return parser.parseFile(filename)

def verifyTestLog (filename, mustpass):
	results			= readTestLog(filename)
	messages			= []
	resultOrderOk	= True

	# Mustpass case names must be unique
	assert len(mustpass) == len(set(mustpass))

	# Verify number of results
	if len(results) != len(mustpass):
		messages.append(error(filename, "Wrong number of test results, expected %d, found %d" % (len(mustpass), len(results))))

	caseNameToResultNdx = {}
	for ndx in xrange(len(results)):
		result = results[ndx]
		if not result in caseNameToResultNdx:
			caseNameToResultNdx[result.name] = ndx
		else:
			messages.append(error(filename, "Multiple results for " + result.name))

	# Verify that all results are present and valid
	for ndx in xrange(len(mustpass)):
		caseName = mustpass[ndx]

		if caseName in caseNameToResultNdx:
			resultNdx	= caseNameToResultNdx[caseName]
			result		= results[resultNdx]

			if resultNdx != ndx:
				resultOrderOk = False

			if not result.statusCode in ALLOWED_STATUS_CODES:
				messages.append(error(filename, result.name + ": " + result.statusCode))
		else:
			messages.append(error(filename, "Missing result for " + caseName))

	if len(results) == len(mustpass) and not resultOrderOk:
		messages.append(error(filename, "Results are not in the expected order"))

	return messages

def beginsWith (str, prefix):
	return str[:len(prefix)] == prefix

def verifyStatement (package):
	messages	= []

	if package.statement != None:
		statementPath	= os.path.join(package.basePath, package.statement)
		statement		= readFile(statementPath)
		hasVersion		= False
		hasProduct		= False
		hasCpu			= False
		hasOs			= False

		for line in statement.splitlines():
			if beginsWith(line, "CONFORM_VERSION:"):
				if hasVersion:
					messages.append(error(statementPath, "Multiple CONFORM_VERSIONs"))
				else:
					hasVersion = True
			elif beginsWith(line, "PRODUCT:"):
				hasProduct = True # Multiple products allowed
			elif beginsWith(line, "CPU:"):
				if hasCpu:
					messages.append(error(statementPath, "Multiple PRODUCTs"))
				else:
					hasCpu = True
			elif beginsWith(line, "OS:"):
				if hasOs:
					messages.append(error(statementPath, "Multiple OSes"))
				else:
					hasOs = True

		if not hasVersion:
			messages.append(error(statementPath, "No CONFORM_VERSION"))
		if not hasProduct:
			messages.append(error(statementPath, "No PRODUCT"))
		if not hasCpu:
			messages.append(error(statementPath, "No CPU"))
		if not hasOs:
			messages.append(error(statementPath, "No OS"))
	else:
		messages.append(error(package.basePath, "Missing conformance statement file"))

	return messages

def verifyGitStatus (package):
	messages = []

	if package.gitStatus != None:
		statusPath	= os.path.join(package.basePath, package.gitStatus)
		status		= readFile(statusPath)

		if status.find("nothing to commit, working directory clean") < 0:
			messages.append(error(package.basePath, "Working directory is not clean"))
	else:
		messages.append(error(package.basePath, "Missing git-status.txt"))

	return messages

def isGitLogEmpty (package):
	assert package.gitLog != None

	logPath	= os.path.join(package.basePath, package.gitLog)
	log		= readFile(logPath)

	return len(log.strip()) == 0

def verifyGitLog (package):
	messages = []

	if package.gitLog != None:
		if not isGitLogEmpty(package):
			messages.append(warning(os.path.join(package.basePath, package.gitLog), "Log is not empty"))
	else:
		messages.append(error(package.basePath, "Missing git-log.txt"))

	return messages

def verifyPatches (package):
	messages	= []
	hasPatches	= len(package.patches)
	logEmpty	= package.gitLog and isGitLogEmpty(package)

	if hasPatches and logEmpty:
		messages.append(error(package.basePath, "Package includes patches but log is empty"))
	elif not hasPatches and not logEmpty:
		messages.append(error(package.basePath, "Test log is not empty but package doesn't contain patches"))

	return messages

def verifyTestLogs (package, mustpass):
	messages	= []

	for testLogFile in package.testLogs:
		messages += verifyTestLog(os.path.join(package.basePath, testLogFile), mustpass)

	if len(package.testLogs) == 0:
		messages.append(error(package.basePath, "No test log files found"))

	return messages

def verifyPackage (package, mustpass):
	messages = []

	messages += verifyStatement(package)
	messages += verifyGitStatus(package)
	messages += verifyGitLog(package)
	messages += verifyPatches(package)
	messages += verifyTestLogs(package, mustpass)

	for item in package.otherItems:
		messages.append(warning(os.path.join(package.basePath, item), "Unknown file"))

	return messages

if __name__ == "__main__":
	if len(sys.argv) != 3:
		print "%s: [extracted submission package] [mustpass]" % sys.argv[0]
		sys.exit(-1)

	packagePath		= os.path.normpath(sys.argv[1])
	mustpassPath	= sys.argv[2]
	package			= getPackageDescription(packagePath)
	mustpass		= readMustpass(mustpassPath)
	messages		= verifyPackage(package, mustpass)

	errors			= [m for m in messages if m.type == ValidationMessage.TYPE_ERROR]
	warnings		= [m for m in messages if m.type == ValidationMessage.TYPE_WARNING]

	for message in messages:
		print str(message)

	print ""

	if len(errors) > 0:
		print "Found %d validation errors and %d warnings!" % (len(errors), len(warnings))
		sys.exit(-2)
	elif len(warnings) > 0:
		print "Found %d warnings, manual review required" % len(warnings)
		sys.exit(-1)
	else:
		print "All validation checks passed"
