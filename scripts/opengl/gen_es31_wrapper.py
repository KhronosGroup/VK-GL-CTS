# -*- coding: utf-8 -*-

from src_util import *

def commandES31InitStatement (command):
	memberName = getFunctionMemberName(command.name)
	return "dst->%s\t= src.%s;" % (memberName, memberName)

def genES31WrapperFuncs (registry):
	iface = getInterface(registry, api='gles2', version="3.1")
	genCommandList(iface, commandES31InitStatement,
				   directory	= OPENGL_DIR,
				   filename		= "gluES3PlusWrapperFuncs.inl",
				   align		= True)

if __name__ == "__main__":
	genES31WrapperFuncs(getGLRegistry())
