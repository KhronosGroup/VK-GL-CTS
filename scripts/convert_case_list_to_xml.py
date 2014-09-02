import re
import sys
from xml.dom.minidom import Document

class TestCase:
	def __init__(self, casePath, description, caseType):
		self.casePath		= casePath
		self.description	= description
		self.caseType		= caseType
		self.children		= []

def findAllMatches(haystack, needle):
	matches = []
	ndx = -1
	while True:
		ndx = haystack.find(needle, ndx+1)
		if (ndx == -1):
			break
		matches.append(ndx)
	return matches

def createAncestors(casePath):
	parentCase = None
	for dotNdx in findAllMatches(casePath, "."):
		ancestorPath = casePath[:dotNdx]
		if ancestorPath not in caseNameHash:
			case = TestCase(ancestorPath, "Test Group", "TestGroup")
			parentCase.children.append(case)
			caseNameHash[ancestorPath] = case
			parentCase = case
		parentCase = caseNameHash[ancestorPath]
	return parentCase

def exportCase (doc, parent, testCase):
	#print testCase.name, testCase.caseType
	element = doc.createElement("TestCase")
	element.setAttribute("Name", testCase.casePath.rsplit(".", 2)[-1])
	element.setAttribute("Description", testCase.description)
	element.setAttribute("CaseType", testCase.caseType)
	parent.appendChild(element)
	for child in testCase.children:
		exportCase(doc, element, child)

# Main.

packageName = sys.argv[1]

rootCase = TestCase(packageName, packageName, "TestPackage" )
caseNameHash = { packageName:rootCase }
caseRE = re.compile(r"^\s*([a-zA-Z0-9_\.\-]+) '([^']*)' (\w+)\s*$".replace(" ", r"\s+"))

lines = open(packageName + ".cases").readlines()
numMatches = 0
for line in lines:
	line = line[:-1]
	if line.startswith(packageName + "."):
		m = caseRE.match(line)
		if m:
			casePath	= m.group(1)
			description	= m.group(2)
			caseType	= m.group(3)
			parent = createAncestors(casePath)
			parent.children.append(TestCase(casePath, description, caseType))
			numMatches += 1

# Create XML document.
doc = Document()
element = doc.createElement("TestCaseList")
doc.appendChild(element)
for testCase in rootCase.children:
	exportCase(doc, element, testCase)

# Dump XML document.
xml = doc.toprettyxml(indent="  ")
open(packageName + "-cases.xml", "wt").write(xml)

print "%d cases converted." % numMatches

