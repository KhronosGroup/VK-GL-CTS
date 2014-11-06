# -*- coding: utf-8 -*-

import os
import string

from src_util import *

def enumDefinition (enum):
	return "#define %s\t%s" % (enum.name, normalizeConstant(enum.value))

def genEnums (iface):
	src = indentLines(map(enumDefinition, iface.enums))
	writeInlFile(os.path.join(OPENGL_INC_DIR, "glwEnums.inl"), src)

if __name__ == "__main__":
	import logging, sys
	logging.basicConfig(stream=sys.stderr, level=logging.INFO)
	genEnums(getGLInterface())
