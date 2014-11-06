# -*- coding: utf-8 -*-

from src_util import *

# Functions that have special implementation
OVERRIDE_FUNCS = set([
	"glGetError",
	"glGetIntegerv",
	"glGetBooleanv",
	"glGetFloatv",
	"glGetString",
	"glGetStringi",
	"glCreateShader",
	"glCreateProgram",
	"glGetShaderiv",
	"glGetProgramiv",
	"glGenTextures",
	"glGenQueries",
	"glGenBuffers",
	"glGenRenderbuffers",
	"glGenFramebuffers",
	"glGenVertexArrays",
	"glGenSamplers",
	"glGenTransformFeedbacks",
	"glGenProgramPipelines",
	"glMapBufferRange",
	"glCheckFramebufferStatus",
	"glReadPixels",
	"glBindBuffer",
	"glDeleteBuffers"
])

NULL_PLATFORM_DIR = os.path.normpath(os.path.join(SCRIPTS_DIR, "..", "..", "framework", "platform", "null"))

def commandDummyImpl (command):
	if command.name in OVERRIDE_FUNCS:
		return None
	template = """
GLW_APICALL {returnType} GLW_APIENTRY {commandName} ({paramDecls})
{{
{body}{maybeReturn}
}}"""
	return template.format(
		returnType	= command.type,
		commandName	= command.name,
		paramDecls	= commandParams(command),
		body		= ''.join("\tDE_UNREF(%s);\n" % p.name for p in command.params),
		maybeReturn = "\n\treturn (%s)0;" % command.type if command.type != 'void' else "")

def commandInitStatement (command):
	return "gl->%s\t= %s;" % (getFunctionMemberName(command.name), command.name)

def genNullRenderContext (iface):
	genCommandList(iface, commandInitStatement,
				   directory	= NULL_PLATFORM_DIR,
				   filename		= "tcuNullRenderContextInitFuncs.inl",
				   align		= True)
	genCommandList(iface, commandDummyImpl,
				   directory	= NULL_PLATFORM_DIR,
				   filename		= "tcuNullRenderContextFuncs.inl")

if __name__ == "__main__":
	genNullRenderContext(getHybridInterface())
