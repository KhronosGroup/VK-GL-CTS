/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 *
 * Generated from Khronos GL API description (gl.xml) revision 33216.
 */

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

if (de::contains(extSet, "GL_KHR_robustness"))
{
	gl->getGraphicsResetStatus	= (glGetGraphicsResetStatusFunc)	loader->get("glGetGraphicsResetStatus");
	gl->getnUniformfv			= (glGetnUniformfvFunc)				loader->get("glGetnUniformfv");
	gl->getnUniformiv			= (glGetnUniformivFunc)				loader->get("glGetnUniformiv");
	gl->getnUniformuiv			= (glGetnUniformuivFunc)			loader->get("glGetnUniformuiv");
	gl->readnPixels				= (glReadnPixelsFunc)				loader->get("glReadnPixels");
}

if (de::contains(extSet, "GL_ARB_clip_control"))
{
	gl->clipControl	= (glClipControlFunc)	loader->get("glClipControl");
}

if (de::contains(extSet, "GL_ARB_buffer_storage"))
{
	gl->bufferStorage	= (glBufferStorageFunc)	loader->get("glBufferStorage");
}

if (de::contains(extSet, "GL_ARB_compute_shader"))
{
	gl->dispatchCompute			= (glDispatchComputeFunc)			loader->get("glDispatchCompute");
	gl->dispatchComputeIndirect	= (glDispatchComputeIndirectFunc)	loader->get("glDispatchComputeIndirect");
}

if (de::contains(extSet, "GL_ARB_draw_elements_base_vertex"))
{
	gl->drawElementsBaseVertex			= (glDrawElementsBaseVertexFunc)			loader->get("glDrawElementsBaseVertex");
	gl->drawElementsInstancedBaseVertex	= (glDrawElementsInstancedBaseVertexFunc)	loader->get("glDrawElementsInstancedBaseVertex");
	gl->drawRangeElementsBaseVertex		= (glDrawRangeElementsBaseVertexFunc)		loader->get("glDrawRangeElementsBaseVertex");
	gl->multiDrawElementsBaseVertex		= (glMultiDrawElementsBaseVertexFunc)		loader->get("glMultiDrawElementsBaseVertex");
}

if (de::contains(extSet, "GL_ARB_direct_state_access"))
{
	gl->bindTextureUnit								= (glBindTextureUnitFunc)							loader->get("glBindTextureUnit");
	gl->blitNamedFramebuffer						= (glBlitNamedFramebufferFunc)						loader->get("glBlitNamedFramebuffer");
	gl->checkNamedFramebufferStatus					= (glCheckNamedFramebufferStatusFunc)				loader->get("glCheckNamedFramebufferStatus");
	gl->clearNamedBufferData						= (glClearNamedBufferDataFunc)						loader->get("glClearNamedBufferData");
	gl->clearNamedBufferSubData						= (glClearNamedBufferSubDataFunc)					loader->get("glClearNamedBufferSubData");
	gl->clearNamedFramebufferfi						= (glClearNamedFramebufferfiFunc)					loader->get("glClearNamedFramebufferfi");
	gl->clearNamedFramebufferfv						= (glClearNamedFramebufferfvFunc)					loader->get("glClearNamedFramebufferfv");
	gl->clearNamedFramebufferiv						= (glClearNamedFramebufferivFunc)					loader->get("glClearNamedFramebufferiv");
	gl->clearNamedFramebufferuiv					= (glClearNamedFramebufferuivFunc)					loader->get("glClearNamedFramebufferuiv");
	gl->compressedTextureSubImage1D					= (glCompressedTextureSubImage1DFunc)				loader->get("glCompressedTextureSubImage1D");
	gl->compressedTextureSubImage2D					= (glCompressedTextureSubImage2DFunc)				loader->get("glCompressedTextureSubImage2D");
	gl->compressedTextureSubImage3D					= (glCompressedTextureSubImage3DFunc)				loader->get("glCompressedTextureSubImage3D");
	gl->copyNamedBufferSubData						= (glCopyNamedBufferSubDataFunc)					loader->get("glCopyNamedBufferSubData");
	gl->copyTextureSubImage1D						= (glCopyTextureSubImage1DFunc)						loader->get("glCopyTextureSubImage1D");
	gl->copyTextureSubImage2D						= (glCopyTextureSubImage2DFunc)						loader->get("glCopyTextureSubImage2D");
	gl->copyTextureSubImage3D						= (glCopyTextureSubImage3DFunc)						loader->get("glCopyTextureSubImage3D");
	gl->createBuffers								= (glCreateBuffersFunc)								loader->get("glCreateBuffers");
	gl->createFramebuffers							= (glCreateFramebuffersFunc)						loader->get("glCreateFramebuffers");
	gl->createProgramPipelines						= (glCreateProgramPipelinesFunc)					loader->get("glCreateProgramPipelines");
	gl->createQueries								= (glCreateQueriesFunc)								loader->get("glCreateQueries");
	gl->createRenderbuffers							= (glCreateRenderbuffersFunc)						loader->get("glCreateRenderbuffers");
	gl->createSamplers								= (glCreateSamplersFunc)							loader->get("glCreateSamplers");
	gl->createTextures								= (glCreateTexturesFunc)							loader->get("glCreateTextures");
	gl->createTransformFeedbacks					= (glCreateTransformFeedbacksFunc)					loader->get("glCreateTransformFeedbacks");
	gl->createVertexArrays							= (glCreateVertexArraysFunc)						loader->get("glCreateVertexArrays");
	gl->disableVertexArrayAttrib					= (glDisableVertexArrayAttribFunc)					loader->get("glDisableVertexArrayAttrib");
	gl->enableVertexArrayAttrib						= (glEnableVertexArrayAttribFunc)					loader->get("glEnableVertexArrayAttrib");
	gl->flushMappedNamedBufferRange					= (glFlushMappedNamedBufferRangeFunc)				loader->get("glFlushMappedNamedBufferRange");
	gl->generateTextureMipmap						= (glGenerateTextureMipmapFunc)						loader->get("glGenerateTextureMipmap");
	gl->getCompressedTextureImage					= (glGetCompressedTextureImageFunc)					loader->get("glGetCompressedTextureImage");
	gl->getNamedBufferParameteri64v					= (glGetNamedBufferParameteri64vFunc)				loader->get("glGetNamedBufferParameteri64v");
	gl->getNamedBufferParameteriv					= (glGetNamedBufferParameterivFunc)					loader->get("glGetNamedBufferParameteriv");
	gl->getNamedBufferPointerv						= (glGetNamedBufferPointervFunc)					loader->get("glGetNamedBufferPointerv");
	gl->getNamedBufferSubData						= (glGetNamedBufferSubDataFunc)						loader->get("glGetNamedBufferSubData");
	gl->getNamedFramebufferAttachmentParameteriv	= (glGetNamedFramebufferAttachmentParameterivFunc)	loader->get("glGetNamedFramebufferAttachmentParameteriv");
	gl->getNamedFramebufferParameteriv				= (glGetNamedFramebufferParameterivFunc)			loader->get("glGetNamedFramebufferParameteriv");
	gl->getNamedRenderbufferParameteriv				= (glGetNamedRenderbufferParameterivFunc)			loader->get("glGetNamedRenderbufferParameteriv");
	gl->getQueryBufferObjecti64v					= (glGetQueryBufferObjecti64vFunc)					loader->get("glGetQueryBufferObjecti64v");
	gl->getQueryBufferObjectiv						= (glGetQueryBufferObjectivFunc)					loader->get("glGetQueryBufferObjectiv");
	gl->getQueryBufferObjectui64v					= (glGetQueryBufferObjectui64vFunc)					loader->get("glGetQueryBufferObjectui64v");
	gl->getQueryBufferObjectuiv						= (glGetQueryBufferObjectuivFunc)					loader->get("glGetQueryBufferObjectuiv");
	gl->getTextureImage								= (glGetTextureImageFunc)							loader->get("glGetTextureImage");
	gl->getTextureLevelParameterfv					= (glGetTextureLevelParameterfvFunc)				loader->get("glGetTextureLevelParameterfv");
	gl->getTextureLevelParameteriv					= (glGetTextureLevelParameterivFunc)				loader->get("glGetTextureLevelParameteriv");
	gl->getTextureParameterIiv						= (glGetTextureParameterIivFunc)					loader->get("glGetTextureParameterIiv");
	gl->getTextureParameterIuiv						= (glGetTextureParameterIuivFunc)					loader->get("glGetTextureParameterIuiv");
	gl->getTextureParameterfv						= (glGetTextureParameterfvFunc)						loader->get("glGetTextureParameterfv");
	gl->getTextureParameteriv						= (glGetTextureParameterivFunc)						loader->get("glGetTextureParameteriv");
	gl->getTransformFeedbacki64_v					= (glGetTransformFeedbacki64_vFunc)					loader->get("glGetTransformFeedbacki64_v");
	gl->getTransformFeedbacki_v						= (glGetTransformFeedbacki_vFunc)					loader->get("glGetTransformFeedbacki_v");
	gl->getTransformFeedbackiv						= (glGetTransformFeedbackivFunc)					loader->get("glGetTransformFeedbackiv");
	gl->getVertexArrayIndexed64iv					= (glGetVertexArrayIndexed64ivFunc)					loader->get("glGetVertexArrayIndexed64iv");
	gl->getVertexArrayIndexediv						= (glGetVertexArrayIndexedivFunc)					loader->get("glGetVertexArrayIndexediv");
	gl->getVertexArrayiv							= (glGetVertexArrayivFunc)							loader->get("glGetVertexArrayiv");
	gl->invalidateNamedFramebufferData				= (glInvalidateNamedFramebufferDataFunc)			loader->get("glInvalidateNamedFramebufferData");
	gl->invalidateNamedFramebufferSubData			= (glInvalidateNamedFramebufferSubDataFunc)			loader->get("glInvalidateNamedFramebufferSubData");
	gl->mapNamedBuffer								= (glMapNamedBufferFunc)							loader->get("glMapNamedBuffer");
	gl->mapNamedBufferRange							= (glMapNamedBufferRangeFunc)						loader->get("glMapNamedBufferRange");
	gl->namedBufferData								= (glNamedBufferDataFunc)							loader->get("glNamedBufferData");
	gl->namedBufferStorage							= (glNamedBufferStorageFunc)						loader->get("glNamedBufferStorage");
	gl->namedBufferSubData							= (glNamedBufferSubDataFunc)						loader->get("glNamedBufferSubData");
	gl->namedFramebufferDrawBuffer					= (glNamedFramebufferDrawBufferFunc)				loader->get("glNamedFramebufferDrawBuffer");
	gl->namedFramebufferDrawBuffers					= (glNamedFramebufferDrawBuffersFunc)				loader->get("glNamedFramebufferDrawBuffers");
	gl->namedFramebufferParameteri					= (glNamedFramebufferParameteriFunc)				loader->get("glNamedFramebufferParameteri");
	gl->namedFramebufferReadBuffer					= (glNamedFramebufferReadBufferFunc)				loader->get("glNamedFramebufferReadBuffer");
	gl->namedFramebufferRenderbuffer				= (glNamedFramebufferRenderbufferFunc)				loader->get("glNamedFramebufferRenderbuffer");
	gl->namedFramebufferTexture						= (glNamedFramebufferTextureFunc)					loader->get("glNamedFramebufferTexture");
	gl->namedFramebufferTextureLayer				= (glNamedFramebufferTextureLayerFunc)				loader->get("glNamedFramebufferTextureLayer");
	gl->namedRenderbufferStorage					= (glNamedRenderbufferStorageFunc)					loader->get("glNamedRenderbufferStorage");
	gl->namedRenderbufferStorageMultisample			= (glNamedRenderbufferStorageMultisampleFunc)		loader->get("glNamedRenderbufferStorageMultisample");
	gl->textureBuffer								= (glTextureBufferFunc)								loader->get("glTextureBuffer");
	gl->textureBufferRange							= (glTextureBufferRangeFunc)						loader->get("glTextureBufferRange");
	gl->textureParameterIiv							= (glTextureParameterIivFunc)						loader->get("glTextureParameterIiv");
	gl->textureParameterIuiv						= (glTextureParameterIuivFunc)						loader->get("glTextureParameterIuiv");
	gl->textureParameterf							= (glTextureParameterfFunc)							loader->get("glTextureParameterf");
	gl->textureParameterfv							= (glTextureParameterfvFunc)						loader->get("glTextureParameterfv");
	gl->textureParameteri							= (glTextureParameteriFunc)							loader->get("glTextureParameteri");
	gl->textureParameteriv							= (glTextureParameterivFunc)						loader->get("glTextureParameteriv");
	gl->textureStorage1D							= (glTextureStorage1DFunc)							loader->get("glTextureStorage1D");
	gl->textureStorage2D							= (glTextureStorage2DFunc)							loader->get("glTextureStorage2D");
	gl->textureStorage2DMultisample					= (glTextureStorage2DMultisampleFunc)				loader->get("glTextureStorage2DMultisample");
	gl->textureStorage3D							= (glTextureStorage3DFunc)							loader->get("glTextureStorage3D");
	gl->textureStorage3DMultisample					= (glTextureStorage3DMultisampleFunc)				loader->get("glTextureStorage3DMultisample");
	gl->textureSubImage1D							= (glTextureSubImage1DFunc)							loader->get("glTextureSubImage1D");
	gl->textureSubImage2D							= (glTextureSubImage2DFunc)							loader->get("glTextureSubImage2D");
	gl->textureSubImage3D							= (glTextureSubImage3DFunc)							loader->get("glTextureSubImage3D");
	gl->transformFeedbackBufferBase					= (glTransformFeedbackBufferBaseFunc)				loader->get("glTransformFeedbackBufferBase");
	gl->transformFeedbackBufferRange				= (glTransformFeedbackBufferRangeFunc)				loader->get("glTransformFeedbackBufferRange");
	gl->unmapNamedBuffer							= (glUnmapNamedBufferFunc)							loader->get("glUnmapNamedBuffer");
	gl->vertexArrayAttribBinding					= (glVertexArrayAttribBindingFunc)					loader->get("glVertexArrayAttribBinding");
	gl->vertexArrayAttribFormat						= (glVertexArrayAttribFormatFunc)					loader->get("glVertexArrayAttribFormat");
	gl->vertexArrayAttribIFormat					= (glVertexArrayAttribIFormatFunc)					loader->get("glVertexArrayAttribIFormat");
	gl->vertexArrayAttribLFormat					= (glVertexArrayAttribLFormatFunc)					loader->get("glVertexArrayAttribLFormat");
	gl->vertexArrayBindingDivisor					= (glVertexArrayBindingDivisorFunc)					loader->get("glVertexArrayBindingDivisor");
	gl->vertexArrayElementBuffer					= (glVertexArrayElementBufferFunc)					loader->get("glVertexArrayElementBuffer");
	gl->vertexArrayVertexBuffer						= (glVertexArrayVertexBufferFunc)					loader->get("glVertexArrayVertexBuffer");
	gl->vertexArrayVertexBuffers					= (glVertexArrayVertexBuffersFunc)					loader->get("glVertexArrayVertexBuffers");
}

if (de::contains(extSet, "GL_ARB_get_program_binary"))
{
	gl->getProgramBinary	= (glGetProgramBinaryFunc)	loader->get("glGetProgramBinary");
	gl->programBinary		= (glProgramBinaryFunc)		loader->get("glProgramBinary");
	gl->programParameteri	= (glProgramParameteriFunc)	loader->get("glProgramParameteri");
}

if (de::contains(extSet, "GL_ARB_internalformat_query"))
{
	gl->getInternalformativ	= (glGetInternalformativFunc)	loader->get("glGetInternalformativ");
}

if (de::contains(extSet, "GL_ARB_program_interface_query"))
{
	gl->getProgramInterfaceiv			= (glGetProgramInterfaceivFunc)				loader->get("glGetProgramInterfaceiv");
	gl->getProgramResourceIndex			= (glGetProgramResourceIndexFunc)			loader->get("glGetProgramResourceIndex");
	gl->getProgramResourceLocation		= (glGetProgramResourceLocationFunc)		loader->get("glGetProgramResourceLocation");
	gl->getProgramResourceLocationIndex	= (glGetProgramResourceLocationIndexFunc)	loader->get("glGetProgramResourceLocationIndex");
	gl->getProgramResourceName			= (glGetProgramResourceNameFunc)			loader->get("glGetProgramResourceName");
	gl->getProgramResourceiv			= (glGetProgramResourceivFunc)				loader->get("glGetProgramResourceiv");
}

if (de::contains(extSet, "GL_ARB_separate_shader_objects"))
{
	gl->activeShaderProgram			= (glActiveShaderProgramFunc)		loader->get("glActiveShaderProgram");
	gl->bindProgramPipeline			= (glBindProgramPipelineFunc)		loader->get("glBindProgramPipeline");
	gl->createShaderProgramv		= (glCreateShaderProgramvFunc)		loader->get("glCreateShaderProgramv");
	gl->deleteProgramPipelines		= (glDeleteProgramPipelinesFunc)	loader->get("glDeleteProgramPipelines");
	gl->genProgramPipelines			= (glGenProgramPipelinesFunc)		loader->get("glGenProgramPipelines");
	gl->getProgramPipelineInfoLog	= (glGetProgramPipelineInfoLogFunc)	loader->get("glGetProgramPipelineInfoLog");
	gl->getProgramPipelineiv		= (glGetProgramPipelineivFunc)		loader->get("glGetProgramPipelineiv");
	gl->isProgramPipeline			= (glIsProgramPipelineFunc)			loader->get("glIsProgramPipeline");
	gl->programUniform1d			= (glProgramUniform1dFunc)			loader->get("glProgramUniform1d");
	gl->programUniform1dv			= (glProgramUniform1dvFunc)			loader->get("glProgramUniform1dv");
	gl->programUniform1f			= (glProgramUniform1fFunc)			loader->get("glProgramUniform1f");
	gl->programUniform1fv			= (glProgramUniform1fvFunc)			loader->get("glProgramUniform1fv");
	gl->programUniform1i			= (glProgramUniform1iFunc)			loader->get("glProgramUniform1i");
	gl->programUniform1iv			= (glProgramUniform1ivFunc)			loader->get("glProgramUniform1iv");
	gl->programUniform1ui			= (glProgramUniform1uiFunc)			loader->get("glProgramUniform1ui");
	gl->programUniform1uiv			= (glProgramUniform1uivFunc)		loader->get("glProgramUniform1uiv");
	gl->programUniform2d			= (glProgramUniform2dFunc)			loader->get("glProgramUniform2d");
	gl->programUniform2dv			= (glProgramUniform2dvFunc)			loader->get("glProgramUniform2dv");
	gl->programUniform2f			= (glProgramUniform2fFunc)			loader->get("glProgramUniform2f");
	gl->programUniform2fv			= (glProgramUniform2fvFunc)			loader->get("glProgramUniform2fv");
	gl->programUniform2i			= (glProgramUniform2iFunc)			loader->get("glProgramUniform2i");
	gl->programUniform2iv			= (glProgramUniform2ivFunc)			loader->get("glProgramUniform2iv");
	gl->programUniform2ui			= (glProgramUniform2uiFunc)			loader->get("glProgramUniform2ui");
	gl->programUniform2uiv			= (glProgramUniform2uivFunc)		loader->get("glProgramUniform2uiv");
	gl->programUniform3d			= (glProgramUniform3dFunc)			loader->get("glProgramUniform3d");
	gl->programUniform3dv			= (glProgramUniform3dvFunc)			loader->get("glProgramUniform3dv");
	gl->programUniform3f			= (glProgramUniform3fFunc)			loader->get("glProgramUniform3f");
	gl->programUniform3fv			= (glProgramUniform3fvFunc)			loader->get("glProgramUniform3fv");
	gl->programUniform3i			= (glProgramUniform3iFunc)			loader->get("glProgramUniform3i");
	gl->programUniform3iv			= (glProgramUniform3ivFunc)			loader->get("glProgramUniform3iv");
	gl->programUniform3ui			= (glProgramUniform3uiFunc)			loader->get("glProgramUniform3ui");
	gl->programUniform3uiv			= (glProgramUniform3uivFunc)		loader->get("glProgramUniform3uiv");
	gl->programUniform4d			= (glProgramUniform4dFunc)			loader->get("glProgramUniform4d");
	gl->programUniform4dv			= (glProgramUniform4dvFunc)			loader->get("glProgramUniform4dv");
	gl->programUniform4f			= (glProgramUniform4fFunc)			loader->get("glProgramUniform4f");
	gl->programUniform4fv			= (glProgramUniform4fvFunc)			loader->get("glProgramUniform4fv");
	gl->programUniform4i			= (glProgramUniform4iFunc)			loader->get("glProgramUniform4i");
	gl->programUniform4iv			= (glProgramUniform4ivFunc)			loader->get("glProgramUniform4iv");
	gl->programUniform4ui			= (glProgramUniform4uiFunc)			loader->get("glProgramUniform4ui");
	gl->programUniform4uiv			= (glProgramUniform4uivFunc)		loader->get("glProgramUniform4uiv");
	gl->programUniformMatrix2dv		= (glProgramUniformMatrix2dvFunc)	loader->get("glProgramUniformMatrix2dv");
	gl->programUniformMatrix2fv		= (glProgramUniformMatrix2fvFunc)	loader->get("glProgramUniformMatrix2fv");
	gl->programUniformMatrix2x3dv	= (glProgramUniformMatrix2x3dvFunc)	loader->get("glProgramUniformMatrix2x3dv");
	gl->programUniformMatrix2x3fv	= (glProgramUniformMatrix2x3fvFunc)	loader->get("glProgramUniformMatrix2x3fv");
	gl->programUniformMatrix2x4dv	= (glProgramUniformMatrix2x4dvFunc)	loader->get("glProgramUniformMatrix2x4dv");
	gl->programUniformMatrix2x4fv	= (glProgramUniformMatrix2x4fvFunc)	loader->get("glProgramUniformMatrix2x4fv");
	gl->programUniformMatrix3dv		= (glProgramUniformMatrix3dvFunc)	loader->get("glProgramUniformMatrix3dv");
	gl->programUniformMatrix3fv		= (glProgramUniformMatrix3fvFunc)	loader->get("glProgramUniformMatrix3fv");
	gl->programUniformMatrix3x2dv	= (glProgramUniformMatrix3x2dvFunc)	loader->get("glProgramUniformMatrix3x2dv");
	gl->programUniformMatrix3x2fv	= (glProgramUniformMatrix3x2fvFunc)	loader->get("glProgramUniformMatrix3x2fv");
	gl->programUniformMatrix3x4dv	= (glProgramUniformMatrix3x4dvFunc)	loader->get("glProgramUniformMatrix3x4dv");
	gl->programUniformMatrix3x4fv	= (glProgramUniformMatrix3x4fvFunc)	loader->get("glProgramUniformMatrix3x4fv");
	gl->programUniformMatrix4dv		= (glProgramUniformMatrix4dvFunc)	loader->get("glProgramUniformMatrix4dv");
	gl->programUniformMatrix4fv		= (glProgramUniformMatrix4fvFunc)	loader->get("glProgramUniformMatrix4fv");
	gl->programUniformMatrix4x2dv	= (glProgramUniformMatrix4x2dvFunc)	loader->get("glProgramUniformMatrix4x2dv");
	gl->programUniformMatrix4x2fv	= (glProgramUniformMatrix4x2fvFunc)	loader->get("glProgramUniformMatrix4x2fv");
	gl->programUniformMatrix4x3dv	= (glProgramUniformMatrix4x3dvFunc)	loader->get("glProgramUniformMatrix4x3dv");
	gl->programUniformMatrix4x3fv	= (glProgramUniformMatrix4x3fvFunc)	loader->get("glProgramUniformMatrix4x3fv");
	gl->useProgramStages			= (glUseProgramStagesFunc)			loader->get("glUseProgramStages");
	gl->validateProgramPipeline		= (glValidateProgramPipelineFunc)	loader->get("glValidateProgramPipeline");
}

if (de::contains(extSet, "GL_ARB_shader_image_load_store"))
{
	gl->bindImageTexture	= (glBindImageTextureFunc)	loader->get("glBindImageTexture");
	gl->memoryBarrier		= (glMemoryBarrierFunc)		loader->get("glMemoryBarrier");
}

if (de::contains(extSet, "GL_ARB_sparse_buffer"))
{
	gl->bufferPageCommitmentARB			= (glBufferPageCommitmentARBFunc)		loader->get("glBufferPageCommitmentARB");
	gl->namedBufferPageCommitmentARB	= (glNamedBufferPageCommitmentARBFunc)	loader->get("glNamedBufferPageCommitmentARB");
	gl->namedBufferPageCommitmentEXT	= (glNamedBufferPageCommitmentEXTFunc)	loader->get("glNamedBufferPageCommitmentEXT");
}

if (de::contains(extSet, "GL_ARB_sparse_texture"))
{
	gl->texPageCommitmentARB	= (glTexPageCommitmentARBFunc)	loader->get("glTexPageCommitmentARB");
}

if (de::contains(extSet, "GL_ARB_tessellation_shader"))
{
	gl->patchParameterfv	= (glPatchParameterfvFunc)	loader->get("glPatchParameterfv");
	gl->patchParameteri		= (glPatchParameteriFunc)	loader->get("glPatchParameteri");
}

if (de::contains(extSet, "GL_ARB_texture_barrier"))
{
	gl->textureBarrier	= (glTextureBarrierFunc)	loader->get("glTextureBarrier");
}

if (de::contains(extSet, "GL_ARB_texture_storage"))
{
	gl->texStorage1D	= (glTexStorage1DFunc)	loader->get("glTexStorage1D");
	gl->texStorage2D	= (glTexStorage2DFunc)	loader->get("glTexStorage2D");
	gl->texStorage3D	= (glTexStorage3DFunc)	loader->get("glTexStorage3D");
}

if (de::contains(extSet, "GL_ARB_texture_storage_multisample"))
{
	gl->texStorage2DMultisample	= (glTexStorage2DMultisampleFunc)	loader->get("glTexStorage2DMultisample");
	gl->texStorage3DMultisample	= (glTexStorage3DMultisampleFunc)	loader->get("glTexStorage3DMultisample");
}

if (de::contains(extSet, "GL_ARB_texture_multisample"))
{
	gl->getMultisamplefv		= (glGetMultisamplefvFunc)		loader->get("glGetMultisamplefv");
	gl->sampleMaski				= (glSampleMaskiFunc)			loader->get("glSampleMaski");
	gl->texImage2DMultisample	= (glTexImage2DMultisampleFunc)	loader->get("glTexImage2DMultisample");
	gl->texImage3DMultisample	= (glTexImage3DMultisampleFunc)	loader->get("glTexImage3DMultisample");
}

if (de::contains(extSet, "GL_ARB_texture_view"))
{
	gl->textureView	= (glTextureViewFunc)	loader->get("glTextureView");
}

if (de::contains(extSet, "GL_ARB_transform_feedback2"))
{
	gl->bindTransformFeedback		= (glBindTransformFeedbackFunc)		loader->get("glBindTransformFeedback");
	gl->deleteTransformFeedbacks	= (glDeleteTransformFeedbacksFunc)	loader->get("glDeleteTransformFeedbacks");
	gl->drawTransformFeedback		= (glDrawTransformFeedbackFunc)		loader->get("glDrawTransformFeedback");
	gl->genTransformFeedbacks		= (glGenTransformFeedbacksFunc)		loader->get("glGenTransformFeedbacks");
	gl->isTransformFeedback			= (glIsTransformFeedbackFunc)		loader->get("glIsTransformFeedback");
	gl->pauseTransformFeedback		= (glPauseTransformFeedbackFunc)	loader->get("glPauseTransformFeedback");
	gl->resumeTransformFeedback		= (glResumeTransformFeedbackFunc)	loader->get("glResumeTransformFeedback");
}

if (de::contains(extSet, "GL_ARB_transform_feedback3"))
{
	gl->beginQueryIndexed			= (glBeginQueryIndexedFunc)				loader->get("glBeginQueryIndexed");
	gl->drawTransformFeedbackStream	= (glDrawTransformFeedbackStreamFunc)	loader->get("glDrawTransformFeedbackStream");
	gl->endQueryIndexed				= (glEndQueryIndexedFunc)				loader->get("glEndQueryIndexed");
	gl->getQueryIndexediv			= (glGetQueryIndexedivFunc)				loader->get("glGetQueryIndexediv");
}

if (de::contains(extSet, "GL_ARB_vertex_attrib_64bit"))
{
	gl->getVertexAttribLdv		= (glGetVertexAttribLdvFunc)	loader->get("glGetVertexAttribLdv");
	gl->vertexAttribL1d			= (glVertexAttribL1dFunc)		loader->get("glVertexAttribL1d");
	gl->vertexAttribL1dv		= (glVertexAttribL1dvFunc)		loader->get("glVertexAttribL1dv");
	gl->vertexAttribL2d			= (glVertexAttribL2dFunc)		loader->get("glVertexAttribL2d");
	gl->vertexAttribL2dv		= (glVertexAttribL2dvFunc)		loader->get("glVertexAttribL2dv");
	gl->vertexAttribL3d			= (glVertexAttribL3dFunc)		loader->get("glVertexAttribL3d");
	gl->vertexAttribL3dv		= (glVertexAttribL3dvFunc)		loader->get("glVertexAttribL3dv");
	gl->vertexAttribL4d			= (glVertexAttribL4dFunc)		loader->get("glVertexAttribL4d");
	gl->vertexAttribL4dv		= (glVertexAttribL4dvFunc)		loader->get("glVertexAttribL4dv");
	gl->vertexAttribLPointer	= (glVertexAttribLPointerFunc)	loader->get("glVertexAttribLPointer");
}

if (de::contains(extSet, "GL_ARB_vertex_attrib_binding"))
{
	gl->bindVertexBuffer		= (glBindVertexBufferFunc)		loader->get("glBindVertexBuffer");
	gl->vertexAttribBinding		= (glVertexAttribBindingFunc)	loader->get("glVertexAttribBinding");
	gl->vertexAttribFormat		= (glVertexAttribFormatFunc)	loader->get("glVertexAttribFormat");
	gl->vertexAttribIFormat		= (glVertexAttribIFormatFunc)	loader->get("glVertexAttribIFormat");
	gl->vertexAttribLFormat		= (glVertexAttribLFormatFunc)	loader->get("glVertexAttribLFormat");
	gl->vertexBindingDivisor	= (glVertexBindingDivisorFunc)	loader->get("glVertexBindingDivisor");
}
