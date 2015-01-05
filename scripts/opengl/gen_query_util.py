# -*- coding: utf-8 -*-

from src_util import *
from khr_util.gen_str_util import genEnumUtilImpls

QUERY_NUM_OUT_ARGUMENTS = [

	("Basic", [
		("VIEWPORT",						4),
		("DEPTH_RANGE",						2),
		("SCISSOR_BOX",						4),
		("COLOR_WRITEMASK",					4),
		("ALIASED_POINT_SIZE_RANGE",		2),
		("ALIASED_LINE_WIDTH_RANGE",		2),
		("MAX_VIEWPORT_DIMS",				2),
		("MAX_COMPUTE_WORK_GROUP_COUNT",	3),
		("MAX_COMPUTE_WORK_GROUP_SIZE",		3),
		("PRIMITIVE_BOUNDING_BOX_EXT",		8),
		]),

	("Attribute", [
		("CURRENT_VERTEX_ATTRIB",		4),
		]),
]

def addNamePrefix (prefix, groups):
	return [(groupName, [(prefix + queryName, querySize) for queryName, querySize in groupQueries]) for groupName, groupQueries in groups]

def genQueryUtil (iface):
	queryNumOutArgs = addNamePrefix("GL_", QUERY_NUM_OUT_ARGUMENTS);
	utilFile = os.path.join(OPENGL_DIR, "gluQueryUtil.inl")

	writeInlFile(utilFile, genEnumUtilImpls(iface, queryNumOutArgs))

if __name__ == "__main__":
	genQueryUtil(getHybridInterface())
