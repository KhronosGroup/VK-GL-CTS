import argparse
import string

class TestGroup:
	def __init__(self, name, parent = None):
		self.parent = parent
		self.name = name
		self.testGroups = []
		self.testCases = []

		if parent:
			assert not parent.hasGroup(name)
			parent.testGroups.append(self)

	def getName (self):
		return self.name

	def getPath (self):
		if self.parent:
			return self.parent.getPath() + "." + self.name
		else:
			return self.name

	def hasGroup(self, groupName):
		for group in self.testGroups:
			if group.getName() == groupName:
				return True
		return False

	def getGroup(self, groupName):
		for group in self.testGroups:
			if group.getName() == groupName:
				return group
		assert False

	def hasTest(self, testName):
		for test in self.testCases:
			if test.getName() == testName:
				return True
		return False

	def getTest(self, testName):
		for test in self.testCases:
			if test.getName() == testName:
				return test
		assert False

	def hasTestCases(self):
		return len(self.testCases) != 0

	def hasTestGroups(self):
		return len(self.testGroups) != 0

	def getTestCases(self):
		return self.testCases

	def getTestGroups(self):
		return self.testGroups

class TestCase:
	def __init__(self, name, parent):
		self.name = name
		self.parent = parent

		assert not parent.hasTest(name)
		self.parent.testCases.append(self)

	def getPath (self):
		return self.parent.getPath() + "." + self.name

	def getName(self):
		return self.name

def addTestToHierarchy(rootGroup, path):
	pathComponents = string.split(path, ".")
	currentGroup = rootGroup

	assert pathComponents[0] == rootGroup.getName()

	for i in range(1, len(pathComponents)):
		component = pathComponents[i]

		if i == len(pathComponents) - 1:
			TestCase(component, currentGroup)
		else:
			if currentGroup.hasGroup(component):
				currentGroup = currentGroup.getGroup(component)
			else:
				currentGroup = TestGroup(component, parent=currentGroup)

def loadTestHierarchy (input, packageName):
	line = input.readline()
	rootGroup = None

	if line.startswith(packageName + "."):
		groupName	= packageName
		rootGroup	= TestGroup(groupName)
	else:
		print(line)
		assert False

	for line in input:
		addTestToHierarchy(rootGroup, line.strip());

	return rootGroup

def writeAndroidCTSTest(test, output):
	output.write('<Test name="%s" />\n' % test.getName())

def writeAndroidCTSTestCase(group, output):
	assert group.hasTestCases()
	assert not group.hasTestGroups()

	output.write('<TestCase name="%s">\n' % group.getName())

	for testCase in group.getTestCases():
		writeAndroidCTSTest(testCase, output)

	output.write('</TestCase>\n')

def writeAndroidCTSTestSuite(group, output):
	if group.getName() == "performance":
		return;

	output.write('<TestSuite name="%s">\n' % group.getName())

	for childGroup in group.getTestGroups():
		if group.getName() == "performance":
			continue;

		if childGroup.hasTestCases():
			assert not childGroup.hasTestGroups()
			writeAndroidCTSTestCase(childGroup, output)
		elif childGroup.hasTestGroups():
			writeAndroidCTSTestSuite(childGroup, output)
		# \note Skips groups without testcases or child groups

	output.write('</TestSuite>\n')

def writeAndroidCTSFile(rootGroup, output, name, appPackageName):
	output.write('<?xml version="1.0" encoding="UTF-8"?>\n')
	output.write('<TestPackage name="%s" appPackageName="%s" testType="deqpTest">\n' % (name, appPackageName))

	writeAndroidCTSTestSuite(rootGroup, output)

	output.write('</TestPackage>\n')

if __name__ == "__main__":
	parser = argparse.ArgumentParser()
	parser.add_argument('input',                      type=argparse.FileType('r'),    help="Input file containing dEQP test names.")
	parser.add_argument('output',                     type=argparse.FileType('w'),    help="Output file for Android CTS test file.")
	parser.add_argument('--name',     dest="name",    type=str,                       required=True, help="Name of the test package")
	parser.add_argument('--package',  dest="package", type=str,                       required=True, help="Name of the app package")

	args = parser.parse_args()

	rootGroup = loadTestHierarchy(args.input, args.name)
	writeAndroidCTSFile(rootGroup, args.output, name=args.name, appPackageName=args.package)
