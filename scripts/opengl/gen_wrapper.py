# -*- coding: utf-8 -*-

import os
from src_util import *
from itertools import imap, chain

def getMangledName (funcName):
	assert funcName[:2] == "gl"
	return "glw" + funcName[2:]

def commandAliasDefinition (command):
	return "#define\t%s\t%s" % (command.name, getMangledName(command.name))

def commandWrapperDeclaration (command):
	return "%s\t%s\t(%s);" % (
		command.type,
		getMangledName(command.name),
		", ".join([param.declaration for param in command.params]))

def genWrapperHeader (iface):
	defines = imap(commandAliasDefinition, iface.commands)
	prototypes = imap(commandWrapperDeclaration, iface.commands)
	src = indentLines(chain(defines, prototypes))
	writeInlFile(os.path.join(OPENGL_INC_DIR, "glwApi.inl"), src)

def getDefaultReturn (command):
	if command.name == "glGetError":
		return "GL_INVALID_OPERATION"
	else:
		assert command.type != 'void'
		return "(%s)0" % command.type

def commandWrapperDefinition (command):
	template = """
{returnType} {mangledName} ({paramDecls})
{{
	const glw::Functions* gl = glw::getCurrentThreadFunctions();
	if (!gl)
		return{defaultReturn};
	{maybeReturn}gl->{memberName}({arguments});
}}"""
	return template.format(
		returnType		= command.type,
		mangledName		= getMangledName(command.name),
		paramDecls		= commandParams(command),
		defaultReturn	= " " + getDefaultReturn(command) if command.type != 'void' else "",
		maybeReturn		= "return " if command.type != 'void' else "",
		memberName		= getFunctionMemberName(command.name),
		arguments		= commandArgs(command))

def genWrapperImplementation (iface):
	genCommandList(iface, commandWrapperDefinition, OPENGL_INC_DIR, "glwImpl.inl")

def genWrapper (iface):
	genWrapperHeader(iface)
	genWrapperImplementation(iface)

if __name__ == "__main__":
	genWrapper(getHybridInterface())
