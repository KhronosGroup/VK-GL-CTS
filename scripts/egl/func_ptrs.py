# -*- coding: utf-8 -*-

from common import *
from khr_util.format import commandParams

def commandTypedefDecl (command):
	return "typedef EGLW_APICALL %s\t(EGLW_APIENTRY* %s)\t(%s);" % (
		command.type,
		getFunctionTypeName(command.name),
		commandParams(command))

def commandMemberDecl (command):
	return "%s\t%s;" % (getFunctionTypeName(command.name),
						getFunctionMemberName(command.name))

def gen (iface):
	genCommandList(iface, commandTypedefDecl, EGL_WRAPPER_DIR, "eglwFunctionTypes.inl", True)
	genCommandList(iface, commandMemberDecl, EGL_WRAPPER_DIR, "eglwFunctions.inl", True)
