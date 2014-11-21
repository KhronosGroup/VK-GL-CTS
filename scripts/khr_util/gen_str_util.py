# -*- coding: utf-8 -*-

from itertools import chain
from format import indentLines

def isValueDefined (definitions, value):
	return value in definitions

def allValuesUndefined (definitions, values):
	for value in values:
		if isValueDefined(definitions, value):
			return False
	return True

def anyValueDefined (definitions, values):
	return not allValuesUndefined(definitions, values)

def makeDefSet (iface):
	return set(enum.name for enum in iface.enums) | \
		   set(enum.alias for enum in iface.enums if enum.alias != None)

def genStrUtilProtos (iface, enumGroups, bitfieldGroups):
	definitions = makeDefSet(iface)

	def genNameProtos ():
		for groupName, values in enumGroups:
			if anyValueDefined(definitions, values):
				yield "const char*\tget%sName\t(int value);" % groupName
			else:
				print "Warning: Empty value set for %s, skipping" % groupName

	def genBitfieldProtos ():
		for groupName, values in bitfieldGroups:
			if anyValueDefined(definitions, values):
				yield "tcu::Format::Bitfield<16>\tget%sStr\t(int value);" % groupName
			else:
				print "Warning: Empty value set for %s, skipping" % groupName

	def genStrImpl ():
		for groupName, values in enumGroups:
			if anyValueDefined(definitions, values):
				yield "inline tcu::Format::Enum\tget%sStr\t(int value)\t{ return tcu::Format::Enum(get%sName,\tvalue); }" % (groupName, groupName)

	return chain(genNameProtos(), genBitfieldProtos(), genStrImpl())

def genEnumStrImpl (groupName, values, definitions):
	if allValuesUndefined(definitions, values):
		return

	yield ""
	yield "const char* get%sName (int value)" % groupName
	yield "{"
	yield "\tswitch (value)"
	yield "\t{"

	def genCases ():
		for value in values:
			if isValueDefined(definitions, value):
				yield "case %s:\treturn \"%s\";" % (value, value)
			else:
				print "Warning: %s not defined, skipping" % value
		yield "default:\treturn DE_NULL;"

	for caseLine in indentLines(genCases()):
		yield "\t\t" + caseLine

	yield "\t}"
	yield "}"

def genBitfieldStrImpl (groupName, values, definitions):
	if allValuesUndefined(definitions, values):
		return

	yield ""
	yield "tcu::Format::Bitfield<16> get%sStr (int value)" % groupName
	yield "{"
	yield "\tstatic const tcu::Format::BitDesc s_desc[] ="
	yield "\t{"

	def genFields ():
		for value in values:
			if isValueDefined(definitions, value):
				yield "tcu::Format::BitDesc(%s,\t\"%s\")," % (value, value)
			else:
				print "Warning: %s not defined, skipping" % value

	for fieldLine in indentLines(genFields()):
		yield "\t\t" + fieldLine

	yield "\t};"
	yield "\treturn tcu::Format::Bitfield<16>(value, &s_desc[0], &s_desc[DE_LENGTH_OF_ARRAY(s_desc)]);"
	yield "}"

def genStrUtilImpls (iface, enumGroups, bitfieldGroups):
	definitions = makeDefSet(iface)

	for groupName, values in enumGroups:
		for line in genEnumStrImpl(groupName, values, definitions):
			yield line
	for groupName, values in bitfieldGroups:
		for line in genBitfieldStrImpl(groupName, values, definitions):
			yield line

def addValuePrefix (groups, prefix):
	return [(groupName, [prefix + value for value in values]) for groupName, values in groups]
