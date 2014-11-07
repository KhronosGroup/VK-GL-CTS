# -*- coding: utf-8 -*-

from src_util import *

def commandTypedefDecl (command):
	return "typedef GLW_APICALL %s\t(GLW_APIENTRY* %s)\t(%s);" % (
		command.type,
		getFunctionTypeName(command.name),
		commandParams(command))

def commandMemberDecl (command):
	return "%s\t%s;" % (getFunctionTypeName(command.name),
						getFunctionMemberName(command.name))

def genFunctionPointers (iface):
	genCommandList(iface, commandTypedefDecl, OPENGL_INC_DIR, "glwFunctionTypes.inl", True)
	genCommandList(iface, commandMemberDecl, OPENGL_INC_DIR, "glwFunctions.inl", True)

if __name__ == "__main__":
	genFunctionPointers(getHybridInterface())
