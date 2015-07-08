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
	"vkDestroyInstance",
	"vkEnumeratePhysicalDevices",
]
GET_PROC_ADDR		= "vkGetProcAddr"

OBJECT_TYPE_REPL	= {
	"VkObject":				None,
	"VkNonDispatchable":	None,
	"VkDynamicStateObject":	None,
	"VkCmdBuffer":			"VK_OBJECT_TYPE_COMMAND_BUFFER",
}

DEFINITIONS			= [
	"VK_API_VERSION",
	"VK_MAX_PHYSICAL_DEVICE_NAME",
	"VK_MAX_EXTENSION_NAME"
]

class Handle:
	TYPE_ROOT		= 0
	TYPE_DISP		= 1
	TYPE_NONDISP	= 2

	def __init__ (self, type, name, parent = None):
		assert (type == Handle.TYPE_ROOT) == (parent == None)
		self.type	= type
		self.name	= name
		self.parent	= parent

	def getObjectType (self):
		if self.name in OBJECT_TYPE_REPL:
			return OBJECT_TYPE_REPL[self.name]
		else:
			name = re.sub(r'([A-Z])', r'_\1', self.name)
			return "VK_OBJECT_TYPE_" + name[4:].upper()

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

class Struct:
	def __init__ (self, name, members):
		self.name		= name
		self.members	= members

class Function:
	TYPE_GET_PROC_ADDR	= 0 # Special
	TYPE_PLATFORM		= 1 # Not bound to VkPhysicalDevice
	TYPE_DEVICE			= 2 # Bound to VkPhysicalDevice

	def __init__ (self, name, returnType, arguments):
		self.name		= name
		self.returnType	= returnType
		self.arguments	= arguments

	def getType (self):
		if self.name == GET_PROC_ADDR:
			return Function.TYPE_GET_PROC_ADDR
		elif self.name in PLATFORM_FUNCTIONS:
			return Function.TYPE_PLATFORM
		else:
			return Function.TYPE_DEVICE

class API:
	def __init__ (self, definitions, handles, enums, bitfields, structs, functions):
		self.definitions	= definitions
		self.handles		= handles
		self.enums			= enums
		self.bitfields		= bitfields
		self.structs		= structs
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

	if function.name == "vkGetProcAddr":
		fixedReturnType = "FunctionPtr"

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
	matches	= re.findall(r'typedef enum ' + IDENT_PTRN + r'\s*{([^}]*)}\s*(' + IDENT_PTRN + r')\s*;', src)
	enums	= []

	for contents, name in matches:
		enums.append(parseEnum(name, contents))

	return enums

def parseStruct (name, src):
	# \todo [pyry] Array support is currently a hack (size coupled with name)
	typeNamePtrn	= r'(' + TYPE_PTRN + ')(\s' + IDENT_PTRN + r'(\[[^\]]+\])*)\s*;'
	matches			= re.findall(typeNamePtrn, src)
	members			= [Variable(fixupType(t.strip()), n.strip()) for t, n, a in matches]

	return Struct(name, members)

def parseStructs (src):
	matches	= re.findall(r'typedef struct ' + IDENT_PTRN + r'\s*{([^}]*)}\s*(' + IDENT_PTRN + r')\s*;', src)
	structs	= []

	for contents, name in matches:
		structs.append(parseStruct(name, contents))

	return structs

def parseHandles (src):
	matches	= re.findall(r'VK_DEFINE_(NONDISP|DISP)_SUBCLASS_HANDLE\((' + IDENT_PTRN + r'),\s*(' + IDENT_PTRN + r')\)[ \t]*[\n\r]', src)
	handles	= []
	typeMap	= {'DISP': Handle.TYPE_DISP, 'NONDISP': Handle.TYPE_NONDISP}
	byName	= {}
	root	= Handle(Handle.TYPE_ROOT, 'VkObject')

	byName[root.name] = root
	handles.append(root)

	for type, name, parentName in matches:
		parent	= byName[parentName]
		handle	= Handle(typeMap[type], name, parent)

		byName[handle.name] = handle
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
		definitions	= definitions,
		handles		= parseHandles(src),
		enums		= enums,
		bitfields	= bitfields,
		structs		= parseStructs(src),
		functions	= parseFunctions(src))

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

def genStructSrc (struct):
	yield "struct %s" % struct.name
	yield "{"
	for line in indentLines(["\t%s\t%s;" % (m.type, m.name) for m in struct.members]):
		yield line
	yield "};"

def genHandlesSrc (handles):
	def genLines (handles):
		for handle in handles:
			if handle.type == Handle.TYPE_ROOT:
				yield "VK_DEFINE_BASE_HANDLE\t(%s);" % handle.name
			elif handle.type == Handle.TYPE_DISP:
				yield "VK_DEFINE_DISP_SUBCLASS_HANDLE\t(%s,\t%s);" % (handle.name, handle.parent.name)
			elif handle.type == Handle.TYPE_NONDISP:
				yield "VK_DEFINE_NONDISP_SUBCLASS_HANDLE\t(%s,\t%s);" % (handle.name, handle.parent.name)

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
		for line in indentLines(["VK_DEFINE_HANDLE_TYPE_TRAITS(%s);" % handle.name for handle in api.handles]):
			yield line

	writeInlFile(filename, INL_HEADER, gen())

def writeGetObjectTypeImpl (api, filename):
	def gen ():
		for line in indentLines(["template<> VkObjectType\tgetObjectType<%sT>\t(void) { return %s;\t}" % (handle.name, handle.getObjectType()) for handle in api.handles if handle.getObjectType() != None]):
			yield line

	writeInlFile(filename, INL_HEADER, gen())

def writeStructTypes (api, filename):
	def gen ():
		for struct in api.structs:
			for line in genStructSrc(struct):
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
		for line in indentLines(["std::ostream&\toperator<<\t(std::ostream& s, const %s& value);" % (s.name) for s in api.structs]):
			yield line

	writeInlFile(filename, INL_HEADER, makeStrUtilProto())

def writeStrUtilImpl (api, filename):
	def makeStrUtilImpl ():
		for line in indentLines(["template<> const char*\tgetTypeName<%sT>\t(void) { return \"%s\";\t}" % (handle.name, handle.name) for handle in api.handles]):
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

		for struct in api.structs:
			yield ""
			yield "std::ostream& operator<< (std::ostream& s, const %s& value)" % struct.name
			yield "{"
			yield "\ts << \"%s = {\\n\";" % struct.name
			for member in struct.members:
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
	def __init__ (self, name, objectType, iface, arguments):
		self.name		= name
		self.objectType	= objectType
		self.iface		= iface
		self.arguments	= arguments

def getConstructorFunctions (api):
	funcs = []
	for function in api.functions:
		if function.name[:8] == "vkCreate":
			# \todo [pyry] Rather hacky
			iface = None
			if function.getType() == Function.TYPE_PLATFORM:
				iface = Variable("const PlatformInterface&", "vk")
			else:
				iface = Variable("const DeviceInterface&", "vk")
			objectType	= function.arguments[-1].type.replace("*", "").strip()
			arguments	= function.arguments[:-1]
			funcs.append(ConstructorFunction(getInterfaceName(function), objectType, iface, arguments))
	return funcs

def writeRefUtilProto (api, filename):
	functions = getConstructorFunctions(api)

	def makeRefUtilProto ():
		unindented = []
		for line in indentLines(["Move<%sT>\t%s\t(%s);" % (function.objectType, function.name, argListToStr([function.iface] + function.arguments)) for function in functions]):
			yield line

	writeInlFile(filename, INL_HEADER, makeRefUtilProto())

def writeRefUtilImpl (api, filename):
	functions = getConstructorFunctions(api)

	def makeRefUtilImpl ():
		for function in functions:
			maybeDevice = ", device" if "device" in set([a.name for a in function.arguments]) else ""

			yield "Move<%sT> %s (%s)" % (function.objectType, function.name, argListToStr([function.iface] + function.arguments))
			yield "{"
			yield "\t%s object = 0;" % function.objectType
			yield "\tVK_CHECK(vk.%s(%s));" % (function.name, ", ".join([a.name for a in function.arguments] + ["&object"]))
			yield "\treturn Move<%sT>(vk%s, check<%sT>(object));" % (function.objectType, maybeDevice, function.objectType)
			yield "}"
			yield ""

	writeInlFile(filename, INL_HEADER, makeRefUtilImpl())

if __name__ == "__main__":
	src				= readFile(sys.argv[1])
	api				= parseAPI(src)
	platformFuncs	= set([Function.TYPE_GET_PROC_ADDR, Function.TYPE_PLATFORM])
	deviceFuncs		= set([Function.TYPE_DEVICE])

	writeBasicTypes				(api, os.path.join(VULKAN_DIR, "vkBasicTypes.inl"))
	writeStructTypes			(api, os.path.join(VULKAN_DIR, "vkStructTypes.inl"))
	writeGetObjectTypeImpl		(api, os.path.join(VULKAN_DIR, "vkGetObjectTypeImpl.inl"))
	writeInterfaceDecl			(api, os.path.join(VULKAN_DIR, "vkVirtualPlatformInterface.inl"),		functionTypes = platformFuncs,	concrete = False)
	writeInterfaceDecl			(api, os.path.join(VULKAN_DIR, "vkVirtualDeviceInterface.inl"),			functionTypes = deviceFuncs,	concrete = False)
	writeInterfaceDecl			(api, os.path.join(VULKAN_DIR, "vkConcretePlatformInterface.inl"),		functionTypes = platformFuncs,	concrete = True)
	writeInterfaceDecl			(api, os.path.join(VULKAN_DIR, "vkConcreteDeviceInterface.inl"),		functionTypes = deviceFuncs,	concrete = True)
	writeFunctionPtrTypes		(api, os.path.join(VULKAN_DIR, "vkFunctionPointerTypes.inl"))
	writeFunctionPointers		(api, os.path.join(VULKAN_DIR, "vkPlatformFunctionPointers.inl"),		functionTypes = platformFuncs)
	writeFunctionPointers		(api, os.path.join(VULKAN_DIR, "vkDeviceFunctionPointers.inl"),			functionTypes = deviceFuncs)
	writeInitFunctionPointers	(api, os.path.join(VULKAN_DIR, "vkInitPlatformFunctionPointers.inl"),	functionTypes = set([Function.TYPE_PLATFORM])) # \note No vkGetProcAddr
	writeInitFunctionPointers	(api, os.path.join(VULKAN_DIR, "vkInitDeviceFunctionPointers.inl"),		functionTypes = deviceFuncs)
	writeFuncPtrInterfaceImpl	(api, os.path.join(VULKAN_DIR, "vkPlatformDriverImpl.inl"),				functionTypes = platformFuncs,	className = "PlatformDriver")
	writeFuncPtrInterfaceImpl	(api, os.path.join(VULKAN_DIR, "vkDeviceDriverImpl.inl"),				functionTypes = deviceFuncs,	className = "DeviceDriver")
	writeStrUtilProto			(api, os.path.join(VULKAN_DIR, "vkStrUtil.inl"))
	writeStrUtilImpl			(api, os.path.join(VULKAN_DIR, "vkStrUtilImpl.inl"))
	writeRefUtilProto			(api, os.path.join(VULKAN_DIR, "vkRefUtil.inl"))
	writeRefUtilImpl			(api, os.path.join(VULKAN_DIR, "vkRefUtilImpl.inl"))
