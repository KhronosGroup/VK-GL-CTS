# -*- coding: utf-8 -*-

from src_util import *

def commandDirectInitStatement (command):
	# Workaround for broken headers
	if command.name == "glShaderSource":
		cast = "(%s)" % getFunctionTypeName(command.name)
	else:
		cast = ""
	return "gl->%s\t= %s&%s;" % (getFunctionMemberName(command.name),
								 cast,
								 command.name)

def genESDirectInit (registry):
	genCommandLists(registry, commandDirectInitStatement,
					check		= lambda api, _: api == 'gles2',
					directory	= OPENGL_INC_DIR,
					filePattern	= "glwInit%sDirect.inl",
					align		= True)

if __name__ == "__main__":
	genESDirectInit(getGLRegistry())
