/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 *
 * Generated from Khronos GL API description (gl.xml) revision 30159.
 */

if (de::contains(extSet, "GL_KHR_blend_equation_advanced"))
{
	gl->blendBarrierKHR	= (glBlendBarrierKHRFunc)	loader->get("glBlendBarrierKHR");
}

if (de::contains(extSet, "GL_KHR_debug"))
{
	gl->debugMessageCallback	= (glDebugMessageCallbackFunc)	loader->get("glDebugMessageCallbackKHR");
	gl->debugMessageControl		= (glDebugMessageControlFunc)	loader->get("glDebugMessageControlKHR");
	gl->debugMessageInsert		= (glDebugMessageInsertFunc)	loader->get("glDebugMessageInsertKHR");
	gl->getDebugMessageLog		= (glGetDebugMessageLogFunc)	loader->get("glGetDebugMessageLogKHR");
	gl->getObjectLabel			= (glGetObjectLabelFunc)		loader->get("glGetObjectLabelKHR");
	gl->getObjectPtrLabel		= (glGetObjectPtrLabelFunc)		loader->get("glGetObjectPtrLabelKHR");
	gl->getPointerv				= (glGetPointervFunc)			loader->get("glGetPointervKHR");
	gl->objectLabel				= (glObjectLabelFunc)			loader->get("glObjectLabelKHR");
	gl->objectPtrLabel			= (glObjectPtrLabelFunc)		loader->get("glObjectPtrLabelKHR");
	gl->popDebugGroup			= (glPopDebugGroupFunc)			loader->get("glPopDebugGroupKHR");
	gl->pushDebugGroup			= (glPushDebugGroupFunc)		loader->get("glPushDebugGroupKHR");
}

if (de::contains(extSet, "GL_EXT_tessellation_shader"))
{
	gl->patchParameteri	= (glPatchParameteriFunc)	loader->get("glPatchParameteriEXT");
}

if (de::contains(extSet, "GL_EXT_geometry_shader"))
{
	gl->framebufferTexture	= (glFramebufferTextureFunc)	loader->get("glFramebufferTextureEXT");
}

if (de::contains(extSet, "GL_EXT_texture_buffer"))
{
	gl->texBuffer		= (glTexBufferFunc)			loader->get("glTexBufferEXT");
	gl->texBufferRange	= (glTexBufferRangeFunc)	loader->get("glTexBufferRangeEXT");
}

if (de::contains(extSet, "GL_EXT_primitive_bounding_box"))
{
	gl->primitiveBoundingBoxEXT	= (glPrimitiveBoundingBoxEXTFunc)	loader->get("glPrimitiveBoundingBoxEXT");
}

if (de::contains(extSet, "GL_OES_EGL_image"))
{
	gl->eglImageTargetRenderbufferStorageOES	= (glEGLImageTargetRenderbufferStorageOESFunc)	loader->get("glEGLImageTargetRenderbufferStorageOES");
	gl->eglImageTargetTexture2DOES				= (glEGLImageTargetTexture2DOESFunc)			loader->get("glEGLImageTargetTexture2DOES");
}

if (de::contains(extSet, "GL_OES_texture_storage_multisample_2d_array"))
{
	gl->texStorage3DMultisample	= (glTexStorage3DMultisampleFunc)	loader->get("glTexStorage3DMultisampleOES");
}

if (de::contains(extSet, "GL_OES_sample_shading"))
{
	gl->minSampleShading	= (glMinSampleShadingFunc)	loader->get("glMinSampleShadingOES");
}

if (de::contains(extSet, "GL_EXT_copy_image"))
{
	gl->copyImageSubData	= (glCopyImageSubDataFunc)	loader->get("glCopyImageSubDataEXT");
}

if (de::contains(extSet, "GL_EXT_draw_buffers_indexed"))
{
	gl->blendEquationSeparatei	= (glBlendEquationSeparateiFunc)	loader->get("glBlendEquationSeparateiEXT");
	gl->blendEquationi			= (glBlendEquationiFunc)			loader->get("glBlendEquationiEXT");
	gl->blendFuncSeparatei		= (glBlendFuncSeparateiFunc)		loader->get("glBlendFuncSeparateiEXT");
	gl->blendFunci				= (glBlendFunciFunc)				loader->get("glBlendFunciEXT");
	gl->colorMaski				= (glColorMaskiFunc)				loader->get("glColorMaskiEXT");
	gl->disablei				= (glDisableiFunc)					loader->get("glDisableiEXT");
	gl->enablei					= (glEnableiFunc)					loader->get("glEnableiEXT");
	gl->isEnabledi				= (glIsEnablediFunc)				loader->get("glIsEnablediEXT");
}

if (de::contains(extSet, "GL_EXT_texture_border_clamp"))
{
	gl->getSamplerParameterIiv	= (glGetSamplerParameterIivFunc)	loader->get("glGetSamplerParameterIivEXT");
	gl->getSamplerParameterIuiv	= (glGetSamplerParameterIuivFunc)	loader->get("glGetSamplerParameterIuivEXT");
	gl->getTexParameterIiv		= (glGetTexParameterIivFunc)		loader->get("glGetTexParameterIivEXT");
	gl->getTexParameterIuiv		= (glGetTexParameterIuivFunc)		loader->get("glGetTexParameterIuivEXT");
	gl->samplerParameterIiv		= (glSamplerParameterIivFunc)		loader->get("glSamplerParameterIivEXT");
	gl->samplerParameterIuiv	= (glSamplerParameterIuivFunc)		loader->get("glSamplerParameterIuivEXT");
	gl->texParameterIiv			= (glTexParameterIivFunc)			loader->get("glTexParameterIivEXT");
	gl->texParameterIuiv		= (glTexParameterIuivFunc)			loader->get("glTexParameterIuivEXT");
}

if (de::contains(extSet, "GL_EXT_debug_marker"))
{
	gl->insertEventMarkerEXT	= (glInsertEventMarkerEXTFunc)	loader->get("glInsertEventMarkerEXT");
	gl->popGroupMarkerEXT		= (glPopGroupMarkerEXTFunc)		loader->get("glPopGroupMarkerEXT");
	gl->pushGroupMarkerEXT		= (glPushGroupMarkerEXTFunc)	loader->get("glPushGroupMarkerEXT");
}
