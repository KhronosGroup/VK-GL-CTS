# -*- coding: utf-8 -*-

from src_util import *

GEN_VERSIONS = set(["2.0", "3.0"])

def commandLibraryEntry (command):
	return "\t{ \"%s\",\t(deFunctionPtr)%s }," % (command.name, command.name)

def genESStaticLibrary (registry):
	genCommandLists(registry, commandLibraryEntry,
					check		= lambda api, version: api == 'gles2' and version in GEN_VERSIONS,
					directory	= EGL_DIR,
					filePattern	= "egluStatic%sLibrary.inl",
					align		= True)

if __name__ == "__main__":
	genESStaticLibrary(getGLRegistry())
