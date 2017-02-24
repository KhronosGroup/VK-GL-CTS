/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 *
 * Generated from Khronos GL API description (gl.xml) revision db2965fc26513b254e2f964171f79e416a05fe29.
 */

if (de::contains(extSet, "GL_KHR_blend_equation_advanced"))
{
	gl->blendBarrier	= (glBlendBarrierFunc)	loader->get("glBlendBarrierKHR");
}

if (de::contains(extSet, "GL_KHR_debug"))
{
	gl->debugMessageCallback	= (glDebugMessageCallbackFunc)	loader->get("glDebugMessageCallback");
	gl->debugMessageControl		= (glDebugMessageControlFunc)	loader->get("glDebugMessageControl");
	gl->debugMessageInsert		= (glDebugMessageInsertFunc)	loader->get("glDebugMessageInsert");
	gl->getDebugMessageLog		= (glGetDebugMessageLogFunc)	loader->get("glGetDebugMessageLog");
	gl->getObjectLabel			= (glGetObjectLabelFunc)		loader->get("glGetObjectLabel");
	gl->getObjectPtrLabel		= (glGetObjectPtrLabelFunc)		loader->get("glGetObjectPtrLabel");
	gl->objectLabel				= (glObjectLabelFunc)			loader->get("glObjectLabel");
	gl->objectPtrLabel			= (glObjectPtrLabelFunc)		loader->get("glObjectPtrLabel");
	gl->popDebugGroup			= (glPopDebugGroupFunc)			loader->get("glPopDebugGroup");
	gl->pushDebugGroup			= (glPushDebugGroupFunc)		loader->get("glPushDebugGroup");
}

if (de::contains(extSet, "GL_EXT_debug_marker"))
{
	gl->insertEventMarkerEXT	= (glInsertEventMarkerEXTFunc)	loader->get("glInsertEventMarkerEXT");
	gl->popGroupMarkerEXT		= (glPopGroupMarkerEXTFunc)		loader->get("glPopGroupMarkerEXT");
	gl->pushGroupMarkerEXT		= (glPushGroupMarkerEXTFunc)	loader->get("glPushGroupMarkerEXT");
}

if (de::contains(extSet, "GL_KHR_robustness"))
{
	gl->getGraphicsResetStatus	= (glGetGraphicsResetStatusFunc)	loader->get("glGetGraphicsResetStatus");
	gl->getnUniformfv			= (glGetnUniformfvFunc)				loader->get("glGetnUniformfv");
	gl->getnUniformiv			= (glGetnUniformivFunc)				loader->get("glGetnUniformiv");
	gl->getnUniformuiv			= (glGetnUniformuivFunc)			loader->get("glGetnUniformuiv");
	gl->readnPixels				= (glReadnPixelsFunc)				loader->get("glReadnPixels");
}
