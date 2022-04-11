/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 *
 * Generated from Khronos GL API description (gl.xml) revision b0643365776c260efc6317d2ff67dd8bf5078be9.
 */

int getBasicQueryNumArgsOut (int pname)
{
	switch(pname)
	{
		case GL_VIEWPORT:						return 4;
		case GL_DEPTH_RANGE:					return 2;
		case GL_SCISSOR_BOX:					return 4;
		case GL_COLOR_WRITEMASK:				return 4;
		case GL_ALIASED_POINT_SIZE_RANGE:		return 2;
		case GL_ALIASED_LINE_WIDTH_RANGE:		return 2;
		case GL_MAX_VIEWPORT_DIMS:				return 2;
		case GL_MAX_COMPUTE_WORK_GROUP_COUNT:	return 3;
		case GL_MAX_COMPUTE_WORK_GROUP_SIZE:	return 3;
		case GL_PRIMITIVE_BOUNDING_BOX_EXT:		return 8;
		default:								return 1;
	}
}

int getIndexedQueryNumArgsOut (int pname)
{
	switch(pname)
	{
		case GL_COLOR_WRITEMASK:	return 4;
		default:					return 1;
	}
}

int getAttributeQueryNumArgsOut (int pname)
{
	switch(pname)
	{
		case GL_CURRENT_VERTEX_ATTRIB:	return 4;
		default:						return 1;
	}
}

int getProgramQueryNumArgsOut (int pname)
{
	switch(pname)
	{
		case GL_COMPUTE_WORK_GROUP_SIZE:	return 3;
		default:							return 1;
	}
}

int getTextureParamQueryNumArgsOut (int pname)
{
	switch(pname)
	{
		case GL_TEXTURE_BORDER_COLOR:	return 4;
		default:						return 1;
	}
}
