# -*- coding: utf-8 -*-

import os
import string

from common import *
from khr_util.format import indentLines, normalizeConstant

TYPED_VALUES = [
	"EGL_DONT_CARE",
	"EGL_UNKNOWN",
	"EGL_NO_CONTEXT",
	"EGL_NO_DISPLAY",
	"EGL_DEFAULT_DISPLAY",
	"EGL_NO_SURFACE",
	"EGL_NO_IMAGE",
	"EGL_NO_SYNC",
	"EGL_NO_IMAGE_KHR",
	"EGL_NO_SYNC_KHR"
]

def enumValue (enum, typePrefix = ""):
	if enum.name in TYPED_VALUES:
		return enum.value.replace("(EGL", "(%sEGL" % typePrefix)
	else:
		return normalizeConstant(enum.value)

def enumDefinition (enum):
	return "#define %s\t%s" % (enum.name, enumValue(enum, "eglw::"))

def gen (iface):
	writeInlFile(os.path.join(EGL_WRAPPER_DIR, "eglwEnums.inl"), indentLines(map(enumDefinition, iface.enums)))
