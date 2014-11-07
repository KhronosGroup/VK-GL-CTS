# -*- coding: utf-8 -*-

from src_util import *

def commandInitStatement (command):
	return "gl->%s\t= (%s)\tloader->get(\"%s\");" % (
		getFunctionMemberName(command.name),
		getFunctionTypeName(command.name),
		command.name)

def genFuncInit (registry):
	def check(api, version):
		if api == 'gl' and version >= "3.0":
			return 'core'
		return api == 'gles2'

	genCommandLists(registry, commandInitStatement,
					check		= check,
					directory	= OPENGL_INC_DIR,
					filePattern	= "glwInit%s.inl",
					align		= True)

if __name__ == "__main__":
	genFuncInit(getGLRegistry())
