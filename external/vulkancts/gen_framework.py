# -*- coding: utf-8 -*-

#-------------------------------------------------------------------------
# Vulkan CTS
# ----------
#
# Copyright (c) 2015 Google Inc.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and/or associated documentation files (the
# "Materials"), to deal in the Materials without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Materials, and to
# permit persons to whom the Materials are furnished to do so, subject to
# the following conditions:
#
# The above copyright notice(s) and this permission notice shall be
# included in all copies or substantial portions of the Materials.
#
# The Materials are Confidential Information as defined by the
# Khronos Membership Agreement until designated non-confidential by
# Khronos, at which point this condition clause shall be removed.
#
# THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
#
#-------------------------------------------------------------------------

import os
import re
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from build.common import DEQP_DIR
from khr_util.format import indentLines, writeInlFile

VULKAN_DIR = os.path.join(os.path.dirname(__file__), "framework", "vulkan")

INL_HEADER = """\
/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */\
"""

PLATFORM_FUNCTIONS	= [
	"vkCreateInstance",
	"vkGetInstanceProcAddr"
]
INSTANCE_FUNCTIONS	= [
	"vkDestroyInstance",
	"vkEnumeratePhysicalDevices",
	"vkGetPhysicalDeviceFeatures",
	"vkGetPhysicalDeviceFormatProperties",
	"vkGetPhysicalDeviceImageFormatProperties",
	"vkGetPhysicalDeviceLimits",
	"vkGetPhysicalDeviceProperties",
	"vkGetPhysicalDeviceQueueCount",
	"vkGetPhysicalDeviceQueueProperties",
	"vkGetPhysicalDeviceMemoryProperties",
	"vkCreateDevice",
	"vkGetDeviceProcAddr"
]

DEFINITIONS			= [
	"VK_API_VERSION",
	"VK_MAX_PHYSICAL_DEVICE_NAME",
	"VK_MAX_EXTENSION_NAME",
	"VK_UUID_LENGTH",
	"VK_MAX_MEMORY_TYPES",
	"VK_MAX_MEMORY_HEAPS",
	"VK_MAX_DESCRIPTION"
]

class Handle:
	TYPE_DISP		= 0
	TYPE_NONDISP	= 1

	def __init__ (self, type, name):
		self.type	= type
		self.name	= name

	def getHandleType (self):
		name = re.sub(r'([A-Z])', r'_\1', self.name)
		return "HANDLE_TYPE_" + name[4:].upper()

class Enum:
	def __init__ (self, name, values):
		self.name	= name
		self.values	= values

class Bitfield:
	def __init__ (self, name, values):
		self.name	= name
		self.values	= values

class Variable:
	def __init__ (self, type, name):
		self.type	= type
		self.name	= name

class CompositeType:
	CLASS_STRUCT	= 0
	CLASS_UNION		= 1

	def __init__ (self, typeClass, name, members):
		self.typeClass	= typeClass
		self.name		= name
		self.members	= members

	def getClassName (self):
		names = {CompositeType.CLASS_STRUCT: 'struct', CompositeType.CLASS_UNION: 'union'}
		return names[self.typeClass]

class Function:
	TYPE_PLATFORM		= 0 # Not bound to anything
	TYPE_INSTANCE		= 1 # Bound to VkInstance
	TYPE_DEVICE			= 2 # Bound to VkDevice

	def __init__ (self, name, returnType, arguments):
		self.name		= name
		self.returnType	= returnType
		self.arguments	= arguments

	def getType (self):
		if self.name in PLATFORM_FUNCTIONS:
			return Function.TYPE_PLATFORM
		elif self.name in INSTANCE_FUNCTIONS:
			return Function.TYPE_INSTANCE
		else:
			return Function.TYPE_DEVICE

class API:
	def __init__ (self, definitions, handles, enums, bitfields, compositeTypes, functions):
		self.definitions	= definitions
		self.handles		= handles
		self.enums			= enums
		self.bitfields		= bitfields
		self.compositeTypes	= compositeTypes
		self.functions		= functions

def readFile (filename):
	with open(filename, 'rb') as f:
		return f.read()

IDENT_PTRN	= r'[a-zA-Z_][a-zA-Z0-9_]*'
TYPE_PTRN	= r'[a-zA-Z_][a-zA-Z0-9_ \t*]*'

def endswith (s, postfix):
	return len(s) >= len(postfix) and s[len(s)-len(postfix):] == postfix

def fixupEnumValues (values):
	fixed = []
	for name, value in values:
		if endswith(name, "_BEGIN_RANGE") or endswith(name, "_END_RANGE"):
			continue
		fixed.append((name, value))
	return fixed

def fixupType (type):
	replacements = [
			("uint8_t",		"deUint8"),
			("uint16_t",	"deUint16"),
			("uint32_t",	"deUint32"),
			("uint64_t",	"deUint64"),
			("int8_t",		"deInt8"),
			("int16_t",		"deInt16"),
			("int32_t",		"deInt32"),
			("int64_t",		"deInt64"),
			("bool32_t",	"deUint32"),
			("size_t",		"deUintptr"),
		]

	for src, dst in replacements:
		type = type.replace(src, dst)

	return type

def fixupFunction (function):
	fixedArgs		= [Variable(fixupType(a.type), a.name) for a in function.arguments]
	fixedReturnType	= fixupType(function.returnType)

	return Function(function.name, fixedReturnType, fixedArgs)

def getInterfaceName (function):
	assert function.name[:2] == "vk"
	return function.name[2].lower() + function.name[3:]

def getFunctionTypeName (function):
	assert function.name[:2] == "vk"
	return function.name[2:] + "Func"

def getBitEnumNameForBitfield (bitfieldName):
	assert bitfieldName[-1] == "s"
	return bitfieldName[:-1] + "Bits"

def getBitfieldNameForBitEnum (bitEnumName):
	assert bitEnumName[-4:] == "Bits"
	return bitEnumName[:-4] + "s"

def parsePreprocDefinedValue (src, name):
	return re.search(r'#\s*define\s+' + name + r'\s+([^\n]+)\n', src).group(1).strip()

def parseEnum (name, src):
	keyValuePtrn	= '(' + IDENT_PTRN + r')\s*=\s*([^\s,}]+)\s*[,}]'
	matches			= re.findall(keyValuePtrn, src)

	return Enum(name, fixupEnumValues(matches))

# \note Parses raw enums, some are mapped to bitfields later
def parseEnums (src):
	matches	= re.findall(r'typedef enum\s*{([^}]*)}\s*(' + IDENT_PTRN + r')\s*;', src)
	enums	= []

	for contents, name in matches:
		enums.append(parseEnum(name, contents))

	return enums

def parseCompositeType (type, name, src):
	# \todo [pyry] Array support is currently a hack (size coupled with name)
	typeNamePtrn	= r'(' + TYPE_PTRN + ')(\s' + IDENT_PTRN + r'(\[[^\]]+\])*)\s*;'
	matches			= re.findall(typeNamePtrn, src)
	members			= [Variable(fixupType(t.strip()), n.strip()) for t, n, a in matches]

	return CompositeType(type, name, members)

def parseCompositeTypes (src):
	typeMap	= { 'struct': CompositeType.CLASS_STRUCT, 'union': CompositeType.CLASS_UNION }
	matches	= re.findall(r'typedef (struct|union)\s*{([^}]*)}\s*(' + IDENT_PTRN + r')\s*;', src)
	types	= []

	for type, contents, name in matches:
		types.append(parseCompositeType(typeMap[type], name, contents))

	return types

def parseHandles (src):
	matches	= re.findall(r'VK_DEFINE(_NONDISP|)_HANDLE\((' + IDENT_PTRN + r')\)[ \t]*[\n\r]', src)
	handles	= []
	typeMap	= {'': Handle.TYPE_DISP, '_NONDISP': Handle.TYPE_NONDISP}

	for type, name in matches:
		handle = Handle(typeMap[type], name)
		handles.append(handle)

	return handles

def parseArgList (src):
	typeNamePtrn	= r'(' + TYPE_PTRN + ')(\s' + IDENT_PTRN + r')'
	args			= []

	for rawArg in src.split(','):
		m = re.search(typeNamePtrn, rawArg)
		args.append(Variable(m.group(1).strip(), m.group(2).strip()))

	return args

def parseFunctions (src):
	ptrn		= r'(' + TYPE_PTRN + ')VKAPI\s+(' + IDENT_PTRN + r')\s*\(([^)]*)\)\s*;'
	matches		= re.findall(ptrn, src)
	functions	= []

	for returnType, name, argList in matches:
		functions.append(Function(name.strip(), returnType.strip(), parseArgList(argList)))

	return [fixupFunction(f) for f in functions]

def parseBitfieldNames (src):
	ptrn		= r'typedef\s+VkFlags\s(' + IDENT_PTRN + r')\s*;'
	matches		= re.findall(ptrn, src)

	return matches

def parseAPI (src):
	definitions		= [(name, parsePreprocDefinedValue(src, name)) for name in DEFINITIONS]
	rawEnums		= parseEnums(src)
	bitfieldNames	= parseBitfieldNames(src)
	enums			= []
	bitfields		= []
	bitfieldEnums	= set([getBitEnumNameForBitfield(n) for n in bitfieldNames])

	for enum in rawEnums:
		if enum.name in bitfieldEnums:
			bitfields.append(Bitfield(getBitfieldNameForBitEnum(enum.name), enum.values))
		else:
			enums.append(enum)

	return API(
		definitions		= definitions,
		handles			= parseHandles(src),
		enums			= enums,
		bitfields		= bitfields,
		compositeTypes	= parseCompositeTypes(src),
		functions		= parseFunctions(src))

def writeHandleType (api, filename):
	def gen ():
		yield "enum HandleType"
		yield "{"
		yield "\t%s = 0," % api.handles[0].getHandleType()
		for handle in api.handles[1:]:
			yield "\t%s," % handle.getHandleType()
		yield "\tHANDLE_TYPE_LAST"
		yield "};"
		yield ""

	writeInlFile(filename, INL_HEADER, gen())

def genEnumSrc (enum):
	yield "enum %s" % enum.name
	yield "{"
	for line in indentLines(["\t%s\t= %s," % v for v in enum.values]):
		yield line
	yield "};"

def genBitfieldSrc (bitfield):
	yield "enum %s" % getBitEnumNameForBitfield(bitfield.name)
	yield "{"
	for line in indentLines(["\t%s\t= %s," % v for v in bitfield.values]):
		yield line
	yield "};"
	yield "typedef deUint32 %s;" % bitfield.name

def genCompositeTypeSrc (type):
	yield "%s %s" % (type.getClassName(), type.name)
	yield "{"
	for line in indentLines(["\t%s\t%s;" % (m.type, m.name) for m in type.members]):
		yield line
	yield "};"

def genHandlesSrc (handles):
	def genLines (handles):
		for handle in handles:
			if handle.type == Handle.TYPE_DISP:
				yield "VK_DEFINE_HANDLE\t(%s,\t%s);" % (handle.name, handle.getHandleType())
			elif handle.type == Handle.TYPE_NONDISP:
				yield "VK_DEFINE_NONDISP_HANDLE\t(%s,\t%s);" % (handle.name, handle.getHandleType())

	for line in indentLines(genLines(handles)):
		yield line

def writeBasicTypes (api, filename):
	def gen ():
		for line in indentLines(["#define %s\t%s" % define for define in api.definitions]):
			yield line
		yield ""
		for line in genHandlesSrc(api.handles):
			yield line
		yield ""
		for enum in api.enums:
			for line in genEnumSrc(enum):
				yield line
			yield ""
		for bitfield in api.bitfields:
			for line in genBitfieldSrc(bitfield):
				yield line
			yield ""

	writeInlFile(filename, INL_HEADER, gen())

def writeCompositeTypes (api, filename):
	def gen ():
		for type in api.compositeTypes:
			for line in genCompositeTypeSrc(type):
				yield line
			yield ""

	writeInlFile(filename, INL_HEADER, gen())

def argListToStr (args):
	return ", ".join("%s %s" % (v.type, v.name) for v in args)

def writeInterfaceDecl (api, filename, functionTypes, concrete):
	def genProtos ():
		postfix = "" if concrete else " = 0"
		for function in api.functions:
			if function.getType() in functionTypes:
				yield "virtual %s\t%s\t(%s) const%s;" % (function.returnType, getInterfaceName(function), argListToStr(function.arguments), postfix)

	writeInlFile(filename, INL_HEADER, indentLines(genProtos()))

def writeFunctionPtrTypes (api, filename):
	def genTypes ():
		for function in api.functions:
			yield "typedef VK_APICALL %s\t(VK_APIENTRY* %s)\t(%s);" % (function.returnType, getFunctionTypeName(function), argListToStr(function.arguments))

	writeInlFile(filename, INL_HEADER, indentLines(genTypes()))

def writeFunctionPointers (api, filename, functionTypes):
	writeInlFile(filename, INL_HEADER, indentLines(["%s\t%s;" % (getFunctionTypeName(function), getInterfaceName(function)) for function in api.functions if function.getType() in functionTypes]))

def writeInitFunctionPointers (api, filename, functionTypes):
	def makeInitFunctionPointers ():
		for function in api.functions:
			if function.getType() in functionTypes:
				yield "m_vk.%s\t= (%s)\tGET_PROC_ADDR(\"%s\");" % (getInterfaceName(function), getFunctionTypeName(function), function.name)

	writeInlFile(filename, INL_HEADER, indentLines(makeInitFunctionPointers()))

def writeFuncPtrInterfaceImpl (api, filename, functionTypes, className):
	def makeFuncPtrInterfaceImpl ():
		for function in api.functions:
			if function.getType() in functionTypes:
				yield ""
				yield "%s %s::%s (%s) const" % (function.returnType, className, getInterfaceName(function), argListToStr(function.arguments))
				yield "{"
				yield "	%sm_vk.%s(%s);" % ("return " if function.returnType != "void" else "", getInterfaceName(function), ", ".join(a.name for a in function.arguments))
				yield "}"

	writeInlFile(filename, INL_HEADER, makeFuncPtrInterfaceImpl())

def writeStrUtilProto (api, filename):
	def makeStrUtilProto ():
		for line in indentLines(["const char*\tget%sName\t(%s value);" % (enum.name[2:], enum.name) for enum in api.enums]):
			yield line
		yield ""
		for line in indentLines(["inline tcu::Format::Enum<%s>\tget%sStr\t(%s value)\t{ return tcu::Format::Enum<%s>(get%sName, value);\t}" % (e.name, e.name[2:], e.name, e.name, e.name[2:]) for e in api.enums]):
			yield line
		yield ""
		for line in indentLines(["inline std::ostream&\toperator<<\t(std::ostream& s, %s value)\t{ return s << get%sStr(value);\t}" % (e.name, e.name[2:]) for e in api.enums]):
			yield line
		yield ""
		for line in indentLines(["tcu::Format::Bitfield<32>\tget%sStr\t(%s value);" % (bitfield.name[2:], bitfield.name) for bitfield in api.bitfields]):
			yield line
		yield ""
		for line in indentLines(["std::ostream&\toperator<<\t(std::ostream& s, const %s& value);" % (s.name) for s in api.compositeTypes]):
			yield line

	writeInlFile(filename, INL_HEADER, makeStrUtilProto())

def writeStrUtilImpl (api, filename):
	def makeStrUtilImpl ():
		for line in indentLines(["template<> const char*\tgetTypeName<%s>\t(void) { return \"%s\";\t}" % (handle.name, handle.name) for handle in api.handles]):
			yield line

		for enum in api.enums:
			yield ""
			yield "const char* get%sName (%s value)" % (enum.name[2:], enum.name)
			yield "{"
			yield "\tswitch (value)"
			yield "\t{"
			for line in indentLines(["\t\tcase %s:\treturn \"%s\";" % (n, n) for n, v in enum.values] + ["\t\tdefault:\treturn DE_NULL;"]):
				yield line
			yield "\t}"
			yield "}"

		for bitfield in api.bitfields:
			yield ""
			yield "tcu::Format::Bitfield<32> get%sStr (%s value)" % (bitfield.name[2:], bitfield.name)
			yield "{"
			yield "\tstatic const tcu::Format::BitDesc s_desc[] ="
			yield "\t{"
			for line in indentLines(["\t\ttcu::Format::BitDesc(%s,\t\"%s\")," % (n, n) for n, v in bitfield.values]):
				yield line
			yield "\t};"
			yield "\treturn tcu::Format::Bitfield<32>(value, DE_ARRAY_BEGIN(s_desc), DE_ARRAY_END(s_desc));"
			yield "}"

		bitfieldTypeNames = set([bitfield.name for bitfield in api.bitfields])

		for type in api.compositeTypes:
			yield ""
			yield "std::ostream& operator<< (std::ostream& s, const %s& value)" % type.name
			yield "{"
			yield "\ts << \"%s = {\\n\";" % type.name
			for member in type.members:
				memberName	= member.name
				valFmt		= None
				if member.type in bitfieldTypeNames:
					valFmt = "get%sStr(value.%s)" % (member.type[2:], member.name)
				elif '[' in member.name:
					baseName = member.name[:member.name.find('[')]
					if baseName == "extName" or baseName == "deviceName":
						valFmt = "(const char*)value.%s" % baseName
					else:
						valFmt = "tcu::formatArray(DE_ARRAY_BEGIN(value.%s), DE_ARRAY_END(value.%s))" % (baseName, baseName)
					memberName = baseName
				else:
					valFmt = "value.%s" % member.name
				yield ("\ts << \"%s = \" << " % memberName) + valFmt + " << '\\n';"
			yield "\ts << '}';"
			yield "\treturn s;"
			yield "}"


	writeInlFile(filename, INL_HEADER, makeStrUtilImpl())

class ConstructorFunction:
	def __init__ (self, type, name, objectType, iface, arguments):
		self.type		= type
		self.name		= name
		self.objectType	= objectType
		self.iface		= iface
		self.arguments	= arguments

def getConstructorFunctions (api):
	funcs = []
	for function in api.functions:
		if (function.name[:8] == "vkCreate" or function.name == "vkAllocMemory") and not "count" in [a.name for a in function.arguments]:
			# \todo [pyry] Rather hacky
			iface = None
			if function.getType() == Function.TYPE_PLATFORM:
				iface = Variable("const PlatformInterface&", "vk")
			elif function.getType() == Function.TYPE_INSTANCE:
				iface = Variable("const InstanceInterface&", "vk")
			else:
				iface = Variable("const DeviceInterface&", "vk")
			objectType	= function.arguments[-1].type.replace("*", "").strip()
			arguments	= function.arguments[:-1]
			funcs.append(ConstructorFunction(function.getType(), getInterfaceName(function), objectType, iface, arguments))
	return funcs

def writeRefUtilProto (api, filename):
	functions = getConstructorFunctions(api)

	def makeRefUtilProto ():
		unindented = []
		for line in indentLines(["Move<%s>\t%s\t(%s);" % (function.objectType, function.name, argListToStr([function.iface] + function.arguments)) for function in functions]):
			yield line

	writeInlFile(filename, INL_HEADER, makeRefUtilProto())

def writeRefUtilImpl (api, filename):
	functions = getConstructorFunctions(api)

	def makeRefUtilImpl ():
		yield "namespace refdetails"
		yield "{"
		yield ""

		for function in api.functions:
			if function.getType() == Function.TYPE_DEVICE \
			   and (function.name[:9] == "vkDestroy" or function.name == "vkFreeMemory") \
			   and not function.name == "vkDestroyDevice":
				objectType = function.arguments[-1].type
				yield "template<>"
				yield "void Deleter<%s>::operator() (%s obj) const" % (objectType, objectType)
				yield "{"
				yield "\tDE_TEST_ASSERT(m_deviceIface->%s(m_device, obj) == VK_SUCCESS);" % (getInterfaceName(function))
				yield "}"
				yield ""

		yield "} // refdetails"
		yield ""

		for function in functions:
			dtorObj = "device" if function.type == Function.TYPE_DEVICE else "object"

			yield "Move<%s> %s (%s)" % (function.objectType, function.name, argListToStr([function.iface] + function.arguments))
			yield "{"
			yield "\t%s object = 0;" % function.objectType
			yield "\tVK_CHECK(vk.%s(%s));" % (function.name, ", ".join([a.name for a in function.arguments] + ["&object"]))
			yield "\treturn Move<%s>(check<%s>(object), Deleter<%s>(vk, %s));" % (function.objectType, function.objectType, function.objectType, dtorObj)
			yield "}"
			yield ""

	writeInlFile(filename, INL_HEADER, makeRefUtilImpl())

if __name__ == "__main__":
	src				= readFile(sys.argv[1])
	api				= parseAPI(src)
	platformFuncs	= set([Function.TYPE_PLATFORM])
	instanceFuncs	= set([Function.TYPE_INSTANCE])
	deviceFuncs		= set([Function.TYPE_DEVICE])

	writeHandleType				(api, os.path.join(VULKAN_DIR, "vkHandleType.inl"))
	writeBasicTypes				(api, os.path.join(VULKAN_DIR, "vkBasicTypes.inl"))
	writeCompositeTypes			(api, os.path.join(VULKAN_DIR, "vkStructTypes.inl"))
	writeInterfaceDecl			(api, os.path.join(VULKAN_DIR, "vkVirtualPlatformInterface.inl"),		functionTypes = platformFuncs,	concrete = False)
	writeInterfaceDecl			(api, os.path.join(VULKAN_DIR, "vkVirtualInstanceInterface.inl"),		functionTypes = instanceFuncs,	concrete = False)
	writeInterfaceDecl			(api, os.path.join(VULKAN_DIR, "vkVirtualDeviceInterface.inl"),			functionTypes = deviceFuncs,	concrete = False)
	writeInterfaceDecl			(api, os.path.join(VULKAN_DIR, "vkConcretePlatformInterface.inl"),		functionTypes = platformFuncs,	concrete = True)
	writeInterfaceDecl			(api, os.path.join(VULKAN_DIR, "vkConcreteInstanceInterface.inl"),		functionTypes = instanceFuncs,	concrete = True)
	writeInterfaceDecl			(api, os.path.join(VULKAN_DIR, "vkConcreteDeviceInterface.inl"),		functionTypes = deviceFuncs,	concrete = True)
	writeFunctionPtrTypes		(api, os.path.join(VULKAN_DIR, "vkFunctionPointerTypes.inl"))
	writeFunctionPointers		(api, os.path.join(VULKAN_DIR, "vkPlatformFunctionPointers.inl"),		functionTypes = platformFuncs)
	writeFunctionPointers		(api, os.path.join(VULKAN_DIR, "vkInstanceFunctionPointers.inl"),		functionTypes = instanceFuncs)
	writeFunctionPointers		(api, os.path.join(VULKAN_DIR, "vkDeviceFunctionPointers.inl"),			functionTypes = deviceFuncs)
	writeInitFunctionPointers	(api, os.path.join(VULKAN_DIR, "vkInitPlatformFunctionPointers.inl"),	functionTypes = platformFuncs)
	writeInitFunctionPointers	(api, os.path.join(VULKAN_DIR, "vkInitInstanceFunctionPointers.inl"),	functionTypes = instanceFuncs)
	writeInitFunctionPointers	(api, os.path.join(VULKAN_DIR, "vkInitDeviceFunctionPointers.inl"),		functionTypes = deviceFuncs)
	writeFuncPtrInterfaceImpl	(api, os.path.join(VULKAN_DIR, "vkPlatformDriverImpl.inl"),				functionTypes = platformFuncs,	className = "PlatformDriver")
	writeFuncPtrInterfaceImpl	(api, os.path.join(VULKAN_DIR, "vkInstanceDriverImpl.inl"),				functionTypes = instanceFuncs,	className = "InstanceDriver")
	writeFuncPtrInterfaceImpl	(api, os.path.join(VULKAN_DIR, "vkDeviceDriverImpl.inl"),				functionTypes = deviceFuncs,	className = "DeviceDriver")
	writeStrUtilProto			(api, os.path.join(VULKAN_DIR, "vkStrUtil.inl"))
	writeStrUtilImpl			(api, os.path.join(VULKAN_DIR, "vkStrUtilImpl.inl"))
	writeRefUtilProto			(api, os.path.join(VULKAN_DIR, "vkRefUtil.inl"))
	writeRefUtilImpl			(api, os.path.join(VULKAN_DIR, "vkRefUtilImpl.inl"))
