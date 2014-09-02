/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */

glw::GLsync CallLogWrapper::glCreateSyncFromCLeventARB (struct _cl_context* param0, struct _cl_event* param1, glw::GLbitfield param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCreateSyncFromCLeventARB(" << toHex(param0) << ", " << toHex(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	glw::GLsync returnValue = m_gl.createSyncFromCLeventARB(param0, param1, param2);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glBlendBarrierKHR ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBlendBarrierKHR(" << ");" << TestLog::EndMessage;
	m_gl.blendBarrierKHR();
}

void CallLogWrapper::glCullFace (glw::GLenum param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCullFace(" << getFaceStr(param0) << ");" << TestLog::EndMessage;
	m_gl.cullFace(param0);
}

void CallLogWrapper::glFrontFace (glw::GLenum param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFrontFace(" << getWindingStr(param0) << ");" << TestLog::EndMessage;
	m_gl.frontFace(param0);
}

void CallLogWrapper::glHint (glw::GLenum param0, glw::GLenum param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glHint(" << getHintStr(param0) << ", " << getHintModeStr(param1) << ");" << TestLog::EndMessage;
	m_gl.hint(param0, param1);
}

void CallLogWrapper::glLineWidth (glw::GLfloat param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glLineWidth(" << param0 << ");" << TestLog::EndMessage;
	m_gl.lineWidth(param0);
}

void CallLogWrapper::glPointSize (glw::GLfloat param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPointSize(" << param0 << ");" << TestLog::EndMessage;
	m_gl.pointSize(param0);
}

void CallLogWrapper::glPolygonMode (glw::GLenum param0, glw::GLenum param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPolygonMode(" << toHex(param0) << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.polygonMode(param0, param1);
}

void CallLogWrapper::glScissor (glw::GLint param0, glw::GLint param1, glw::GLsizei param2, glw::GLsizei param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glScissor(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.scissor(param0, param1, param2, param3);
}

void CallLogWrapper::glTexParameterf (glw::GLenum param0, glw::GLenum param1, glw::GLfloat param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexParameterf(" << getTextureTargetStr(param0) << ", " << getTextureParameterStr(param1) << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.texParameterf(param0, param1, param2);
}

void CallLogWrapper::glTexParameterfv (glw::GLenum param0, glw::GLenum param1, const glw::GLfloat* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexParameterfv(" << getTextureTargetStr(param0) << ", " << getTextureParameterStr(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.texParameterfv(param0, param1, param2);
}

void CallLogWrapper::glTexParameteri (glw::GLenum param0, glw::GLenum param1, glw::GLint param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexParameteri(" << getTextureTargetStr(param0) << ", " << getTextureParameterStr(param1) << ", " << getTextureParameterValueStr(param1, param2) << ");" << TestLog::EndMessage;
	m_gl.texParameteri(param0, param1, param2);
}

void CallLogWrapper::glTexParameteriv (glw::GLenum param0, glw::GLenum param1, const glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexParameteriv(" << getTextureTargetStr(param0) << ", " << getTextureParameterStr(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.texParameteriv(param0, param1, param2);
}

void CallLogWrapper::glTexImage1D (glw::GLenum param0, glw::GLint param1, glw::GLint param2, glw::GLsizei param3, glw::GLint param4, glw::GLenum param5, glw::GLenum param6, const glw::GLvoid* param7)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexImage1D(" << getTextureTargetStr(param0) << ", " << param1 << ", " << getPixelFormatStr(param2) << ", " << param3 << ", " << param4 << ", " << getPixelFormatStr(param5) << ", " << getTypeStr(param6) << ", " << toHex(param7) << ");" << TestLog::EndMessage;
	m_gl.texImage1D(param0, param1, param2, param3, param4, param5, param6, param7);
}

void CallLogWrapper::glTexImage2D (glw::GLenum param0, glw::GLint param1, glw::GLint param2, glw::GLsizei param3, glw::GLsizei param4, glw::GLint param5, glw::GLenum param6, glw::GLenum param7, const glw::GLvoid* param8)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexImage2D(" << getTextureTargetStr(param0) << ", " << param1 << ", " << getPixelFormatStr(param2) << ", " << param3 << ", " << param4 << ", " << param5 << ", " << getPixelFormatStr(param6) << ", " << getTypeStr(param7) << ", " << toHex(param8) << ");" << TestLog::EndMessage;
	m_gl.texImage2D(param0, param1, param2, param3, param4, param5, param6, param7, param8);
}

void CallLogWrapper::glDrawBuffer (glw::GLenum param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawBuffer(" << toHex(param0) << ");" << TestLog::EndMessage;
	m_gl.drawBuffer(param0);
}

void CallLogWrapper::glClear (glw::GLbitfield param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClear(" << getBufferMaskStr(param0) << ");" << TestLog::EndMessage;
	m_gl.clear(param0);
}

void CallLogWrapper::glClearColor (glw::GLfloat param0, glw::GLfloat param1, glw::GLfloat param2, glw::GLfloat param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearColor(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.clearColor(param0, param1, param2, param3);
}

void CallLogWrapper::glClearStencil (glw::GLint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearStencil(" << param0 << ");" << TestLog::EndMessage;
	m_gl.clearStencil(param0);
}

void CallLogWrapper::glClearDepth (glw::GLdouble param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearDepth(" << param0 << ");" << TestLog::EndMessage;
	m_gl.clearDepth(param0);
}

void CallLogWrapper::glStencilMask (glw::GLuint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glStencilMask(" << param0 << ");" << TestLog::EndMessage;
	m_gl.stencilMask(param0);
}

void CallLogWrapper::glColorMask (glw::GLboolean param0, glw::GLboolean param1, glw::GLboolean param2, glw::GLboolean param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glColorMask(" << getBooleanStr(param0) << ", " << getBooleanStr(param1) << ", " << getBooleanStr(param2) << ", " << getBooleanStr(param3) << ");" << TestLog::EndMessage;
	m_gl.colorMask(param0, param1, param2, param3);
}

void CallLogWrapper::glDepthMask (glw::GLboolean param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDepthMask(" << getBooleanStr(param0) << ");" << TestLog::EndMessage;
	m_gl.depthMask(param0);
}

void CallLogWrapper::glDisable (glw::GLenum param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDisable(" << getEnableCapStr(param0) << ");" << TestLog::EndMessage;
	m_gl.disable(param0);
}

void CallLogWrapper::glEnable (glw::GLenum param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glEnable(" << getEnableCapStr(param0) << ");" << TestLog::EndMessage;
	m_gl.enable(param0);
}

void CallLogWrapper::glFinish ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFinish(" << ");" << TestLog::EndMessage;
	m_gl.finish();
}

void CallLogWrapper::glFlush ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFlush(" << ");" << TestLog::EndMessage;
	m_gl.flush();
}

void CallLogWrapper::glBlendFunc (glw::GLenum param0, glw::GLenum param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBlendFunc(" << getBlendFactorStr(param0) << ", " << getBlendFactorStr(param1) << ");" << TestLog::EndMessage;
	m_gl.blendFunc(param0, param1);
}

void CallLogWrapper::glLogicOp (glw::GLenum param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glLogicOp(" << toHex(param0) << ");" << TestLog::EndMessage;
	m_gl.logicOp(param0);
}

void CallLogWrapper::glStencilFunc (glw::GLenum param0, glw::GLint param1, glw::GLuint param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glStencilFunc(" << getCompareFuncStr(param0) << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.stencilFunc(param0, param1, param2);
}

void CallLogWrapper::glStencilOp (glw::GLenum param0, glw::GLenum param1, glw::GLenum param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glStencilOp(" << getStencilOpStr(param0) << ", " << getStencilOpStr(param1) << ", " << getStencilOpStr(param2) << ");" << TestLog::EndMessage;
	m_gl.stencilOp(param0, param1, param2);
}

void CallLogWrapper::glDepthFunc (glw::GLenum param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDepthFunc(" << getCompareFuncStr(param0) << ");" << TestLog::EndMessage;
	m_gl.depthFunc(param0);
}

void CallLogWrapper::glPixelStoref (glw::GLenum param0, glw::GLfloat param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPixelStoref(" << toHex(param0) << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.pixelStoref(param0, param1);
}

void CallLogWrapper::glPixelStorei (glw::GLenum param0, glw::GLint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPixelStorei(" << getPixelStoreParameterStr(param0) << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.pixelStorei(param0, param1);
}

void CallLogWrapper::glReadBuffer (glw::GLenum param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glReadBuffer(" << getDrawReadBufferStr(param0) << ");" << TestLog::EndMessage;
	m_gl.readBuffer(param0);
}

void CallLogWrapper::glReadPixels (glw::GLint param0, glw::GLint param1, glw::GLsizei param2, glw::GLsizei param3, glw::GLenum param4, glw::GLenum param5, glw::GLvoid* param6)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glReadPixels(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ", " << getPixelFormatStr(param4) << ", " << getTypeStr(param5) << ", " << toHex(param6) << ");" << TestLog::EndMessage;
	m_gl.readPixels(param0, param1, param2, param3, param4, param5, param6);
}

void CallLogWrapper::glGetBooleanv (glw::GLenum param0, glw::GLboolean* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetBooleanv(" << getGettableStateStr(param0) << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.getBooleanv(param0, param1);
}

void CallLogWrapper::glGetDoublev (glw::GLenum param0, glw::GLdouble* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetDoublev(" << toHex(param0) << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.getDoublev(param0, param1);
}

glw::GLenum CallLogWrapper::glGetError ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetError(" << ");" << TestLog::EndMessage;
	glw::GLenum returnValue = m_gl.getError();
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getErrorStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glGetFloatv (glw::GLenum param0, glw::GLfloat* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetFloatv(" << getGettableStateStr(param0) << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.getFloatv(param0, param1);
}

void CallLogWrapper::glGetIntegerv (glw::GLenum param0, glw::GLint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetIntegerv(" << getGettableStateStr(param0) << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.getIntegerv(param0, param1);
}

const glw::GLubyte* CallLogWrapper::glGetString (glw::GLenum param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetString(" << getGettableStringStr(param0) << ");" << TestLog::EndMessage;
	const glw::GLubyte* returnValue = m_gl.getString(param0);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getStringStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glGetTexImage (glw::GLenum param0, glw::GLint param1, glw::GLenum param2, glw::GLenum param3, glw::GLvoid* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTexImage(" << toHex(param0) << ", " << param1 << ", " << toHex(param2) << ", " << toHex(param3) << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.getTexImage(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glGetTexParameterfv (glw::GLenum param0, glw::GLenum param1, glw::GLfloat* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTexParameterfv(" << getTextureTargetStr(param0) << ", " << getTextureParameterStr(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getTexParameterfv(param0, param1, param2);
}

void CallLogWrapper::glGetTexParameteriv (glw::GLenum param0, glw::GLenum param1, glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTexParameteriv(" << getTextureTargetStr(param0) << ", " << getTextureParameterStr(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getTexParameteriv(param0, param1, param2);
}

void CallLogWrapper::glGetTexLevelParameterfv (glw::GLenum param0, glw::GLint param1, glw::GLenum param2, glw::GLfloat* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTexLevelParameterfv(" << getTextureTargetStr(param0) << ", " << param1 << ", " << getTextureLevelParameterStr(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.getTexLevelParameterfv(param0, param1, param2, param3);
}

void CallLogWrapper::glGetTexLevelParameteriv (glw::GLenum param0, glw::GLint param1, glw::GLenum param2, glw::GLint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTexLevelParameteriv(" << getTextureTargetStr(param0) << ", " << param1 << ", " << getTextureLevelParameterStr(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.getTexLevelParameteriv(param0, param1, param2, param3);
}

glw::GLboolean CallLogWrapper::glIsEnabled (glw::GLenum param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsEnabled(" << getEnableCapStr(param0) << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isEnabled(param0);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glDepthRange (glw::GLdouble param0, glw::GLdouble param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDepthRange(" << param0 << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.depthRange(param0, param1);
}

void CallLogWrapper::glViewport (glw::GLint param0, glw::GLint param1, glw::GLsizei param2, glw::GLsizei param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glViewport(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.viewport(param0, param1, param2, param3);
}

void CallLogWrapper::glDrawArrays (glw::GLenum param0, glw::GLint param1, glw::GLsizei param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawArrays(" << getPrimitiveTypeStr(param0) << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.drawArrays(param0, param1, param2);
}

void CallLogWrapper::glDrawElements (glw::GLenum param0, glw::GLsizei param1, glw::GLenum param2, const glw::GLvoid* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawElements(" << getPrimitiveTypeStr(param0) << ", " << param1 << ", " << getTypeStr(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.drawElements(param0, param1, param2, param3);
}

void CallLogWrapper::glGetPointerv (glw::GLenum param0, glw::GLvoid** param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetPointerv(" << toHex(param0) << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.getPointerv(param0, param1);
}

void CallLogWrapper::glPolygonOffset (glw::GLfloat param0, glw::GLfloat param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPolygonOffset(" << param0 << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.polygonOffset(param0, param1);
}

void CallLogWrapper::glCopyTexImage1D (glw::GLenum param0, glw::GLint param1, glw::GLenum param2, glw::GLint param3, glw::GLint param4, glw::GLsizei param5, glw::GLint param6)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyTexImage1D(" << getTextureTargetStr(param0) << ", " << param1 << ", " << getPixelFormatStr(param2) << ", " << param3 << ", " << param4 << ", " << param5 << ", " << param6 << ");" << TestLog::EndMessage;
	m_gl.copyTexImage1D(param0, param1, param2, param3, param4, param5, param6);
}

void CallLogWrapper::glCopyTexImage2D (glw::GLenum param0, glw::GLint param1, glw::GLenum param2, glw::GLint param3, glw::GLint param4, glw::GLsizei param5, glw::GLsizei param6, glw::GLint param7)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyTexImage2D(" << getTextureTargetStr(param0) << ", " << param1 << ", " << getPixelFormatStr(param2) << ", " << param3 << ", " << param4 << ", " << param5 << ", " << param6 << ", " << param7 << ");" << TestLog::EndMessage;
	m_gl.copyTexImage2D(param0, param1, param2, param3, param4, param5, param6, param7);
}

void CallLogWrapper::glCopyTexSubImage1D (glw::GLenum param0, glw::GLint param1, glw::GLint param2, glw::GLint param3, glw::GLint param4, glw::GLsizei param5)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyTexSubImage1D(" << toHex(param0) << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ", " << param5 << ");" << TestLog::EndMessage;
	m_gl.copyTexSubImage1D(param0, param1, param2, param3, param4, param5);
}

void CallLogWrapper::glCopyTexSubImage2D (glw::GLenum param0, glw::GLint param1, glw::GLint param2, glw::GLint param3, glw::GLint param4, glw::GLint param5, glw::GLsizei param6, glw::GLsizei param7)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyTexSubImage2D(" << toHex(param0) << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ", " << param5 << ", " << param6 << ", " << param7 << ");" << TestLog::EndMessage;
	m_gl.copyTexSubImage2D(param0, param1, param2, param3, param4, param5, param6, param7);
}

void CallLogWrapper::glTexSubImage1D (glw::GLenum param0, glw::GLint param1, glw::GLint param2, glw::GLsizei param3, glw::GLenum param4, glw::GLenum param5, const glw::GLvoid* param6)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexSubImage1D(" << getTextureTargetStr(param0) << ", " << param1 << ", " << param2 << ", " << param3 << ", " << getPixelFormatStr(param4) << ", " << getTypeStr(param5) << ", " << toHex(param6) << ");" << TestLog::EndMessage;
	m_gl.texSubImage1D(param0, param1, param2, param3, param4, param5, param6);
}

void CallLogWrapper::glTexSubImage2D (glw::GLenum param0, glw::GLint param1, glw::GLint param2, glw::GLint param3, glw::GLsizei param4, glw::GLsizei param5, glw::GLenum param6, glw::GLenum param7, const glw::GLvoid* param8)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexSubImage2D(" << getTextureTargetStr(param0) << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ", " << param5 << ", " << getPixelFormatStr(param6) << ", " << getTypeStr(param7) << ", " << toHex(param8) << ");" << TestLog::EndMessage;
	m_gl.texSubImage2D(param0, param1, param2, param3, param4, param5, param6, param7, param8);
}

void CallLogWrapper::glBindTexture (glw::GLenum param0, glw::GLuint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindTexture(" << getTextureTargetStr(param0) << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.bindTexture(param0, param1);
}

void CallLogWrapper::glDeleteTextures (glw::GLsizei param0, const glw::GLuint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDeleteTextures(" << param0 << ", " << getPointerStr(param1, param0) << ");" << TestLog::EndMessage;
	m_gl.deleteTextures(param0, param1);
}

void CallLogWrapper::glGenTextures (glw::GLsizei param0, glw::GLuint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGenTextures(" << param0 << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.genTextures(param0, param1);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 1 = " << getPointerStr(param1, param0) << TestLog::EndMessage;
	}
}

glw::GLboolean CallLogWrapper::glIsTexture (glw::GLuint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsTexture(" << param0 << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isTexture(param0);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glBlendColor (glw::GLfloat param0, glw::GLfloat param1, glw::GLfloat param2, glw::GLfloat param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBlendColor(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.blendColor(param0, param1, param2, param3);
}

void CallLogWrapper::glBlendEquation (glw::GLenum param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBlendEquation(" << getBlendEquationStr(param0) << ");" << TestLog::EndMessage;
	m_gl.blendEquation(param0);
}

void CallLogWrapper::glDrawRangeElements (glw::GLenum param0, glw::GLuint param1, glw::GLuint param2, glw::GLsizei param3, glw::GLenum param4, const glw::GLvoid* param5)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawRangeElements(" << getPrimitiveTypeStr(param0) << ", " << param1 << ", " << param2 << ", " << param3 << ", " << getTypeStr(param4) << ", " << toHex(param5) << ");" << TestLog::EndMessage;
	m_gl.drawRangeElements(param0, param1, param2, param3, param4, param5);
}

void CallLogWrapper::glTexImage3D (glw::GLenum param0, glw::GLint param1, glw::GLint param2, glw::GLsizei param3, glw::GLsizei param4, glw::GLsizei param5, glw::GLint param6, glw::GLenum param7, glw::GLenum param8, const glw::GLvoid* param9)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexImage3D(" << getTextureTargetStr(param0) << ", " << param1 << ", " << getPixelFormatStr(param2) << ", " << param3 << ", " << param4 << ", " << param5 << ", " << param6 << ", " << getPixelFormatStr(param7) << ", " << getTypeStr(param8) << ", " << toHex(param9) << ");" << TestLog::EndMessage;
	m_gl.texImage3D(param0, param1, param2, param3, param4, param5, param6, param7, param8, param9);
}

void CallLogWrapper::glTexSubImage3D (glw::GLenum param0, glw::GLint param1, glw::GLint param2, glw::GLint param3, glw::GLint param4, glw::GLsizei param5, glw::GLsizei param6, glw::GLsizei param7, glw::GLenum param8, glw::GLenum param9, const glw::GLvoid* param10)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexSubImage3D(" << getTextureTargetStr(param0) << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ", " << param5 << ", " << param6 << ", " << param7 << ", " << getPixelFormatStr(param8) << ", " << getTypeStr(param9) << ", " << toHex(param10) << ");" << TestLog::EndMessage;
	m_gl.texSubImage3D(param0, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10);
}

void CallLogWrapper::glCopyTexSubImage3D (glw::GLenum param0, glw::GLint param1, glw::GLint param2, glw::GLint param3, glw::GLint param4, glw::GLint param5, glw::GLint param6, glw::GLsizei param7, glw::GLsizei param8)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyTexSubImage3D(" << toHex(param0) << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ", " << param5 << ", " << param6 << ", " << param7 << ", " << param8 << ");" << TestLog::EndMessage;
	m_gl.copyTexSubImage3D(param0, param1, param2, param3, param4, param5, param6, param7, param8);
}

void CallLogWrapper::glActiveTexture (glw::GLenum param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glActiveTexture(" << getTextureUnitStr(param0) << ");" << TestLog::EndMessage;
	m_gl.activeTexture(param0);
}

void CallLogWrapper::glSampleCoverage (glw::GLfloat param0, glw::GLboolean param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glSampleCoverage(" << param0 << ", " << getBooleanStr(param1) << ");" << TestLog::EndMessage;
	m_gl.sampleCoverage(param0, param1);
}

void CallLogWrapper::glCompressedTexImage3D (glw::GLenum param0, glw::GLint param1, glw::GLenum param2, glw::GLsizei param3, glw::GLsizei param4, glw::GLsizei param5, glw::GLint param6, glw::GLsizei param7, const glw::GLvoid* param8)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedTexImage3D(" << toHex(param0) << ", " << param1 << ", " << toHex(param2) << ", " << param3 << ", " << param4 << ", " << param5 << ", " << param6 << ", " << param7 << ", " << toHex(param8) << ");" << TestLog::EndMessage;
	m_gl.compressedTexImage3D(param0, param1, param2, param3, param4, param5, param6, param7, param8);
}

void CallLogWrapper::glCompressedTexImage2D (glw::GLenum param0, glw::GLint param1, glw::GLenum param2, glw::GLsizei param3, glw::GLsizei param4, glw::GLint param5, glw::GLsizei param6, const glw::GLvoid* param7)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedTexImage2D(" << getTextureTargetStr(param0) << ", " << param1 << ", " << getPixelFormatStr(param2) << ", " << param3 << ", " << param4 << ", " << param5 << ", " << param6 << ", " << toHex(param7) << ");" << TestLog::EndMessage;
	m_gl.compressedTexImage2D(param0, param1, param2, param3, param4, param5, param6, param7);
}

void CallLogWrapper::glCompressedTexImage1D (glw::GLenum param0, glw::GLint param1, glw::GLenum param2, glw::GLsizei param3, glw::GLint param4, glw::GLsizei param5, const glw::GLvoid* param6)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedTexImage1D(" << toHex(param0) << ", " << param1 << ", " << toHex(param2) << ", " << param3 << ", " << param4 << ", " << param5 << ", " << toHex(param6) << ");" << TestLog::EndMessage;
	m_gl.compressedTexImage1D(param0, param1, param2, param3, param4, param5, param6);
}

void CallLogWrapper::glCompressedTexSubImage3D (glw::GLenum param0, glw::GLint param1, glw::GLint param2, glw::GLint param3, glw::GLint param4, glw::GLsizei param5, glw::GLsizei param6, glw::GLsizei param7, glw::GLenum param8, glw::GLsizei param9, const glw::GLvoid* param10)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedTexSubImage3D(" << toHex(param0) << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ", " << param5 << ", " << param6 << ", " << param7 << ", " << toHex(param8) << ", " << param9 << ", " << toHex(param10) << ");" << TestLog::EndMessage;
	m_gl.compressedTexSubImage3D(param0, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10);
}

void CallLogWrapper::glCompressedTexSubImage2D (glw::GLenum param0, glw::GLint param1, glw::GLint param2, glw::GLint param3, glw::GLsizei param4, glw::GLsizei param5, glw::GLenum param6, glw::GLsizei param7, const glw::GLvoid* param8)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedTexSubImage2D(" << getTextureTargetStr(param0) << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ", " << param5 << ", " << getPixelFormatStr(param6) << ", " << param7 << ", " << toHex(param8) << ");" << TestLog::EndMessage;
	m_gl.compressedTexSubImage2D(param0, param1, param2, param3, param4, param5, param6, param7, param8);
}

void CallLogWrapper::glCompressedTexSubImage1D (glw::GLenum param0, glw::GLint param1, glw::GLint param2, glw::GLsizei param3, glw::GLenum param4, glw::GLsizei param5, const glw::GLvoid* param6)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompressedTexSubImage1D(" << toHex(param0) << ", " << param1 << ", " << param2 << ", " << param3 << ", " << toHex(param4) << ", " << param5 << ", " << toHex(param6) << ");" << TestLog::EndMessage;
	m_gl.compressedTexSubImage1D(param0, param1, param2, param3, param4, param5, param6);
}

void CallLogWrapper::glGetCompressedTexImage (glw::GLenum param0, glw::GLint param1, glw::GLvoid* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetCompressedTexImage(" << toHex(param0) << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getCompressedTexImage(param0, param1, param2);
}

void CallLogWrapper::glBlendFuncSeparate (glw::GLenum param0, glw::GLenum param1, glw::GLenum param2, glw::GLenum param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBlendFuncSeparate(" << getBlendFactorStr(param0) << ", " << getBlendFactorStr(param1) << ", " << getBlendFactorStr(param2) << ", " << getBlendFactorStr(param3) << ");" << TestLog::EndMessage;
	m_gl.blendFuncSeparate(param0, param1, param2, param3);
}

void CallLogWrapper::glMultiDrawArrays (glw::GLenum param0, const glw::GLint* param1, const glw::GLsizei* param2, glw::GLsizei param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiDrawArrays(" << getPrimitiveTypeStr(param0) << ", " << toHex(param1) << ", " << toHex(param2) << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.multiDrawArrays(param0, param1, param2, param3);
}

void CallLogWrapper::glMultiDrawElements (glw::GLenum param0, const glw::GLsizei* param1, glw::GLenum param2, const glw::GLvoid* const* param3, glw::GLsizei param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiDrawElements(" << getPrimitiveTypeStr(param0) << ", " << toHex(param1) << ", " << getTypeStr(param2) << ", " << toHex(param3) << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.multiDrawElements(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glPointParameterf (glw::GLenum param0, glw::GLfloat param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPointParameterf(" << toHex(param0) << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.pointParameterf(param0, param1);
}

void CallLogWrapper::glPointParameterfv (glw::GLenum param0, const glw::GLfloat* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPointParameterfv(" << toHex(param0) << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.pointParameterfv(param0, param1);
}

void CallLogWrapper::glPointParameteri (glw::GLenum param0, glw::GLint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPointParameteri(" << toHex(param0) << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.pointParameteri(param0, param1);
}

void CallLogWrapper::glPointParameteriv (glw::GLenum param0, const glw::GLint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPointParameteriv(" << toHex(param0) << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.pointParameteriv(param0, param1);
}

void CallLogWrapper::glGenQueries (glw::GLsizei param0, glw::GLuint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGenQueries(" << param0 << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.genQueries(param0, param1);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 1 = " << getPointerStr(param1, param0) << TestLog::EndMessage;
	}
}

void CallLogWrapper::glDeleteQueries (glw::GLsizei param0, const glw::GLuint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDeleteQueries(" << param0 << ", " << getPointerStr(param1, param0) << ");" << TestLog::EndMessage;
	m_gl.deleteQueries(param0, param1);
}

glw::GLboolean CallLogWrapper::glIsQuery (glw::GLuint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsQuery(" << param0 << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isQuery(param0);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glBeginQuery (glw::GLenum param0, glw::GLuint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBeginQuery(" << getQueryTargetStr(param0) << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.beginQuery(param0, param1);
}

void CallLogWrapper::glEndQuery (glw::GLenum param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glEndQuery(" << getQueryTargetStr(param0) << ");" << TestLog::EndMessage;
	m_gl.endQuery(param0);
}

void CallLogWrapper::glGetQueryiv (glw::GLenum param0, glw::GLenum param1, glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetQueryiv(" << getQueryTargetStr(param0) << ", " << getQueryParamStr(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getQueryiv(param0, param1, param2);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 2 = " << getPointerStr(param2, 1) << TestLog::EndMessage;
	}
}

void CallLogWrapper::glGetQueryObjectiv (glw::GLuint param0, glw::GLenum param1, glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetQueryObjectiv(" << param0 << ", " << getQueryObjectParamStr(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getQueryObjectiv(param0, param1, param2);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 2 = " << getPointerStr(param2, 1) << TestLog::EndMessage;
	}
}

void CallLogWrapper::glGetQueryObjectuiv (glw::GLuint param0, glw::GLenum param1, glw::GLuint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetQueryObjectuiv(" << param0 << ", " << getQueryObjectParamStr(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getQueryObjectuiv(param0, param1, param2);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 2 = " << getPointerStr(param2, 1) << TestLog::EndMessage;
	}
}

void CallLogWrapper::glBindBuffer (glw::GLenum param0, glw::GLuint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindBuffer(" << getBufferTargetStr(param0) << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.bindBuffer(param0, param1);
}

void CallLogWrapper::glDeleteBuffers (glw::GLsizei param0, const glw::GLuint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDeleteBuffers(" << param0 << ", " << getPointerStr(param1, param0) << ");" << TestLog::EndMessage;
	m_gl.deleteBuffers(param0, param1);
}

void CallLogWrapper::glGenBuffers (glw::GLsizei param0, glw::GLuint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGenBuffers(" << param0 << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.genBuffers(param0, param1);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 1 = " << getPointerStr(param1, param0) << TestLog::EndMessage;
	}
}

glw::GLboolean CallLogWrapper::glIsBuffer (glw::GLuint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsBuffer(" << param0 << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isBuffer(param0);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glBufferData (glw::GLenum param0, glw::GLsizeiptr param1, const glw::GLvoid* param2, glw::GLenum param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBufferData(" << getBufferTargetStr(param0) << ", " << param1 << ", " << toHex(param2) << ", " << getUsageStr(param3) << ");" << TestLog::EndMessage;
	m_gl.bufferData(param0, param1, param2, param3);
}

void CallLogWrapper::glBufferSubData (glw::GLenum param0, glw::GLintptr param1, glw::GLsizeiptr param2, const glw::GLvoid* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBufferSubData(" << getBufferTargetStr(param0) << ", " << param1 << ", " << param2 << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.bufferSubData(param0, param1, param2, param3);
}

void CallLogWrapper::glGetBufferSubData (glw::GLenum param0, glw::GLintptr param1, glw::GLsizeiptr param2, glw::GLvoid* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetBufferSubData(" << toHex(param0) << ", " << param1 << ", " << param2 << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.getBufferSubData(param0, param1, param2, param3);
}

glw::GLvoid* CallLogWrapper::glMapBuffer (glw::GLenum param0, glw::GLenum param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMapBuffer(" << toHex(param0) << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	glw::GLvoid* returnValue = m_gl.mapBuffer(param0, param1);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLboolean CallLogWrapper::glUnmapBuffer (glw::GLenum param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUnmapBuffer(" << getBufferTargetStr(param0) << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.unmapBuffer(param0);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glGetBufferParameteriv (glw::GLenum param0, glw::GLenum param1, glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetBufferParameteriv(" << getBufferTargetStr(param0) << ", " << getBufferQueryStr(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getBufferParameteriv(param0, param1, param2);
}

void CallLogWrapper::glGetBufferPointerv (glw::GLenum param0, glw::GLenum param1, glw::GLvoid** param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetBufferPointerv(" << toHex(param0) << ", " << toHex(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getBufferPointerv(param0, param1, param2);
}

void CallLogWrapper::glBlendEquationSeparate (glw::GLenum param0, glw::GLenum param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBlendEquationSeparate(" << getBlendEquationStr(param0) << ", " << getBlendEquationStr(param1) << ");" << TestLog::EndMessage;
	m_gl.blendEquationSeparate(param0, param1);
}

void CallLogWrapper::glDrawBuffers (glw::GLsizei param0, const glw::GLenum* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawBuffers(" << param0 << ", " << getEnumPointerStr(param1, param0, getDrawReadBufferName) << ");" << TestLog::EndMessage;
	m_gl.drawBuffers(param0, param1);
}

void CallLogWrapper::glStencilOpSeparate (glw::GLenum param0, glw::GLenum param1, glw::GLenum param2, glw::GLenum param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glStencilOpSeparate(" << getFaceStr(param0) << ", " << getStencilOpStr(param1) << ", " << getStencilOpStr(param2) << ", " << getStencilOpStr(param3) << ");" << TestLog::EndMessage;
	m_gl.stencilOpSeparate(param0, param1, param2, param3);
}

void CallLogWrapper::glStencilFuncSeparate (glw::GLenum param0, glw::GLenum param1, glw::GLint param2, glw::GLuint param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glStencilFuncSeparate(" << getFaceStr(param0) << ", " << getCompareFuncStr(param1) << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.stencilFuncSeparate(param0, param1, param2, param3);
}

void CallLogWrapper::glStencilMaskSeparate (glw::GLenum param0, glw::GLuint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glStencilMaskSeparate(" << getFaceStr(param0) << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.stencilMaskSeparate(param0, param1);
}

void CallLogWrapper::glAttachShader (glw::GLuint param0, glw::GLuint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glAttachShader(" << param0 << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.attachShader(param0, param1);
}

void CallLogWrapper::glBindAttribLocation (glw::GLuint param0, glw::GLuint param1, const glw::GLchar* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindAttribLocation(" << param0 << ", " << param1 << ", " << getStringStr(param2) << ");" << TestLog::EndMessage;
	m_gl.bindAttribLocation(param0, param1, param2);
}

void CallLogWrapper::glCompileShader (glw::GLuint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCompileShader(" << param0 << ");" << TestLog::EndMessage;
	m_gl.compileShader(param0);
}

glw::GLuint CallLogWrapper::glCreateProgram ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCreateProgram(" << ");" << TestLog::EndMessage;
	glw::GLuint returnValue = m_gl.createProgram();
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLuint CallLogWrapper::glCreateShader (glw::GLenum param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCreateShader(" << getShaderTypeStr(param0) << ");" << TestLog::EndMessage;
	glw::GLuint returnValue = m_gl.createShader(param0);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glDeleteProgram (glw::GLuint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDeleteProgram(" << param0 << ");" << TestLog::EndMessage;
	m_gl.deleteProgram(param0);
}

void CallLogWrapper::glDeleteShader (glw::GLuint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDeleteShader(" << param0 << ");" << TestLog::EndMessage;
	m_gl.deleteShader(param0);
}

void CallLogWrapper::glDetachShader (glw::GLuint param0, glw::GLuint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDetachShader(" << param0 << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.detachShader(param0, param1);
}

void CallLogWrapper::glDisableVertexAttribArray (glw::GLuint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDisableVertexAttribArray(" << param0 << ");" << TestLog::EndMessage;
	m_gl.disableVertexAttribArray(param0);
}

void CallLogWrapper::glEnableVertexAttribArray (glw::GLuint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glEnableVertexAttribArray(" << param0 << ");" << TestLog::EndMessage;
	m_gl.enableVertexAttribArray(param0);
}

void CallLogWrapper::glGetActiveAttrib (glw::GLuint param0, glw::GLuint param1, glw::GLsizei param2, glw::GLsizei* param3, glw::GLint* param4, glw::GLenum* param5, glw::GLchar* param6)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetActiveAttrib(" << param0 << ", " << param1 << ", " << param2 << ", " << toHex(param3) << ", " << toHex(param4) << ", " << toHex(param5) << ", " << toHex(param6) << ");" << TestLog::EndMessage;
	m_gl.getActiveAttrib(param0, param1, param2, param3, param4, param5, param6);
}

void CallLogWrapper::glGetActiveUniform (glw::GLuint param0, glw::GLuint param1, glw::GLsizei param2, glw::GLsizei* param3, glw::GLint* param4, glw::GLenum* param5, glw::GLchar* param6)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetActiveUniform(" << param0 << ", " << param1 << ", " << param2 << ", " << toHex(param3) << ", " << toHex(param4) << ", " << toHex(param5) << ", " << toHex(param6) << ");" << TestLog::EndMessage;
	m_gl.getActiveUniform(param0, param1, param2, param3, param4, param5, param6);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 3 = " << getPointerStr(param3, 1) << TestLog::EndMessage;
		m_log << TestLog::Message << "// param 4 = " << getPointerStr(param4, 1) << TestLog::EndMessage;
		m_log << TestLog::Message << "// param 5 = " << getEnumPointerStr(param5, 1, getShaderVarTypeName) << TestLog::EndMessage;
		m_log << TestLog::Message << "// param 6 = " << getStringStr(param6) << TestLog::EndMessage;
	}
}

void CallLogWrapper::glGetAttachedShaders (glw::GLuint param0, glw::GLsizei param1, glw::GLsizei* param2, glw::GLuint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetAttachedShaders(" << param0 << ", " << param1 << ", " << toHex(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.getAttachedShaders(param0, param1, param2, param3);
}

glw::GLint CallLogWrapper::glGetAttribLocation (glw::GLuint param0, const glw::GLchar* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetAttribLocation(" << param0 << ", " << getStringStr(param1) << ");" << TestLog::EndMessage;
	glw::GLint returnValue = m_gl.getAttribLocation(param0, param1);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glGetProgramiv (glw::GLuint param0, glw::GLenum param1, glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetProgramiv(" << param0 << ", " << getProgramParamStr(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getProgramiv(param0, param1, param2);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 2 = " << getPointerStr(param2, 1) << TestLog::EndMessage;
	}
}

void CallLogWrapper::glGetProgramInfoLog (glw::GLuint param0, glw::GLsizei param1, glw::GLsizei* param2, glw::GLchar* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetProgramInfoLog(" << param0 << ", " << param1 << ", " << toHex(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.getProgramInfoLog(param0, param1, param2, param3);
}

void CallLogWrapper::glGetShaderiv (glw::GLuint param0, glw::GLenum param1, glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetShaderiv(" << param0 << ", " << getShaderParamStr(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getShaderiv(param0, param1, param2);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 2 = " << getPointerStr(param2, 1) << TestLog::EndMessage;
	}
}

void CallLogWrapper::glGetShaderInfoLog (glw::GLuint param0, glw::GLsizei param1, glw::GLsizei* param2, glw::GLchar* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetShaderInfoLog(" << param0 << ", " << param1 << ", " << toHex(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.getShaderInfoLog(param0, param1, param2, param3);
}

void CallLogWrapper::glGetShaderSource (glw::GLuint param0, glw::GLsizei param1, glw::GLsizei* param2, glw::GLchar* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetShaderSource(" << param0 << ", " << param1 << ", " << toHex(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.getShaderSource(param0, param1, param2, param3);
}

glw::GLint CallLogWrapper::glGetUniformLocation (glw::GLuint param0, const glw::GLchar* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetUniformLocation(" << param0 << ", " << getStringStr(param1) << ");" << TestLog::EndMessage;
	glw::GLint returnValue = m_gl.getUniformLocation(param0, param1);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glGetUniformfv (glw::GLuint param0, glw::GLint param1, glw::GLfloat* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetUniformfv(" << param0 << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getUniformfv(param0, param1, param2);
}

void CallLogWrapper::glGetUniformiv (glw::GLuint param0, glw::GLint param1, glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetUniformiv(" << param0 << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getUniformiv(param0, param1, param2);
}

void CallLogWrapper::glGetVertexAttribdv (glw::GLuint param0, glw::GLenum param1, glw::GLdouble* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetVertexAttribdv(" << param0 << ", " << toHex(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getVertexAttribdv(param0, param1, param2);
}

void CallLogWrapper::glGetVertexAttribfv (glw::GLuint param0, glw::GLenum param1, glw::GLfloat* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetVertexAttribfv(" << param0 << ", " << getVertexAttribParameterNameStr(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getVertexAttribfv(param0, param1, param2);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 2 = " << getPointerStr(param2, (param1 == GL_CURRENT_VERTEX_ATTRIB ? 4 : 1)) << TestLog::EndMessage;
	}
}

void CallLogWrapper::glGetVertexAttribiv (glw::GLuint param0, glw::GLenum param1, glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetVertexAttribiv(" << param0 << ", " << getVertexAttribParameterNameStr(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getVertexAttribiv(param0, param1, param2);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 2 = " << getPointerStr(param2, (param1 == GL_CURRENT_VERTEX_ATTRIB ? 4 : 1)) << TestLog::EndMessage;
	}
}

void CallLogWrapper::glGetVertexAttribPointerv (glw::GLuint param0, glw::GLenum param1, glw::GLvoid** param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetVertexAttribPointerv(" << param0 << ", " << toHex(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getVertexAttribPointerv(param0, param1, param2);
}

glw::GLboolean CallLogWrapper::glIsProgram (glw::GLuint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsProgram(" << param0 << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isProgram(param0);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLboolean CallLogWrapper::glIsShader (glw::GLuint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsShader(" << param0 << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isShader(param0);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glLinkProgram (glw::GLuint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glLinkProgram(" << param0 << ");" << TestLog::EndMessage;
	m_gl.linkProgram(param0);
}

void CallLogWrapper::glShaderSource (glw::GLuint param0, glw::GLsizei param1, const glw::GLchar* const* param2, const glw::GLint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glShaderSource(" << param0 << ", " << param1 << ", " << toHex(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.shaderSource(param0, param1, param2, param3);
}

void CallLogWrapper::glUseProgram (glw::GLuint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUseProgram(" << param0 << ");" << TestLog::EndMessage;
	m_gl.useProgram(param0);
}

void CallLogWrapper::glUniform1f (glw::GLint param0, glw::GLfloat param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform1f(" << param0 << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.uniform1f(param0, param1);
}

void CallLogWrapper::glUniform2f (glw::GLint param0, glw::GLfloat param1, glw::GLfloat param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform2f(" << param0 << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.uniform2f(param0, param1, param2);
}

void CallLogWrapper::glUniform3f (glw::GLint param0, glw::GLfloat param1, glw::GLfloat param2, glw::GLfloat param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform3f(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.uniform3f(param0, param1, param2, param3);
}

void CallLogWrapper::glUniform4f (glw::GLint param0, glw::GLfloat param1, glw::GLfloat param2, glw::GLfloat param3, glw::GLfloat param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform4f(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.uniform4f(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glUniform1i (glw::GLint param0, glw::GLint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform1i(" << param0 << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.uniform1i(param0, param1);
}

void CallLogWrapper::glUniform2i (glw::GLint param0, glw::GLint param1, glw::GLint param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform2i(" << param0 << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.uniform2i(param0, param1, param2);
}

void CallLogWrapper::glUniform3i (glw::GLint param0, glw::GLint param1, glw::GLint param2, glw::GLint param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform3i(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.uniform3i(param0, param1, param2, param3);
}

void CallLogWrapper::glUniform4i (glw::GLint param0, glw::GLint param1, glw::GLint param2, glw::GLint param3, glw::GLint param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform4i(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.uniform4i(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glUniform1fv (glw::GLint param0, glw::GLsizei param1, const glw::GLfloat* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform1fv(" << param0 << ", " << param1 << ", " << getPointerStr(param2, (param1 * 1)) << ");" << TestLog::EndMessage;
	m_gl.uniform1fv(param0, param1, param2);
}

void CallLogWrapper::glUniform2fv (glw::GLint param0, glw::GLsizei param1, const glw::GLfloat* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform2fv(" << param0 << ", " << param1 << ", " << getPointerStr(param2, (param1 * 2)) << ");" << TestLog::EndMessage;
	m_gl.uniform2fv(param0, param1, param2);
}

void CallLogWrapper::glUniform3fv (glw::GLint param0, glw::GLsizei param1, const glw::GLfloat* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform3fv(" << param0 << ", " << param1 << ", " << getPointerStr(param2, (param1 * 3)) << ");" << TestLog::EndMessage;
	m_gl.uniform3fv(param0, param1, param2);
}

void CallLogWrapper::glUniform4fv (glw::GLint param0, glw::GLsizei param1, const glw::GLfloat* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform4fv(" << param0 << ", " << param1 << ", " << getPointerStr(param2, (param1 * 4)) << ");" << TestLog::EndMessage;
	m_gl.uniform4fv(param0, param1, param2);
}

void CallLogWrapper::glUniform1iv (glw::GLint param0, glw::GLsizei param1, const glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform1iv(" << param0 << ", " << param1 << ", " << getPointerStr(param2, (param1 * 1)) << ");" << TestLog::EndMessage;
	m_gl.uniform1iv(param0, param1, param2);
}

void CallLogWrapper::glUniform2iv (glw::GLint param0, glw::GLsizei param1, const glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform2iv(" << param0 << ", " << param1 << ", " << getPointerStr(param2, (param1 * 2)) << ");" << TestLog::EndMessage;
	m_gl.uniform2iv(param0, param1, param2);
}

void CallLogWrapper::glUniform3iv (glw::GLint param0, glw::GLsizei param1, const glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform3iv(" << param0 << ", " << param1 << ", " << getPointerStr(param2, (param1 * 3)) << ");" << TestLog::EndMessage;
	m_gl.uniform3iv(param0, param1, param2);
}

void CallLogWrapper::glUniform4iv (glw::GLint param0, glw::GLsizei param1, const glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform4iv(" << param0 << ", " << param1 << ", " << getPointerStr(param2, (param1 * 4)) << ");" << TestLog::EndMessage;
	m_gl.uniform4iv(param0, param1, param2);
}

void CallLogWrapper::glUniformMatrix2fv (glw::GLint param0, glw::GLsizei param1, glw::GLboolean param2, const glw::GLfloat* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix2fv(" << param0 << ", " << param1 << ", " << getBooleanStr(param2) << ", " << getPointerStr(param3, (param1 * 2*2)) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix2fv(param0, param1, param2, param3);
}

void CallLogWrapper::glUniformMatrix3fv (glw::GLint param0, glw::GLsizei param1, glw::GLboolean param2, const glw::GLfloat* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix3fv(" << param0 << ", " << param1 << ", " << getBooleanStr(param2) << ", " << getPointerStr(param3, (param1 * 3*3)) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix3fv(param0, param1, param2, param3);
}

void CallLogWrapper::glUniformMatrix4fv (glw::GLint param0, glw::GLsizei param1, glw::GLboolean param2, const glw::GLfloat* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix4fv(" << param0 << ", " << param1 << ", " << getBooleanStr(param2) << ", " << getPointerStr(param3, (param1 * 4*4)) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix4fv(param0, param1, param2, param3);
}

void CallLogWrapper::glValidateProgram (glw::GLuint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glValidateProgram(" << param0 << ");" << TestLog::EndMessage;
	m_gl.validateProgram(param0);
}

void CallLogWrapper::glVertexAttrib1d (glw::GLuint param0, glw::GLdouble param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib1d(" << param0 << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib1d(param0, param1);
}

void CallLogWrapper::glVertexAttrib1dv (glw::GLuint param0, const glw::GLdouble* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib1dv(" << param0 << ", " << getPointerStr(param1, 1) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib1dv(param0, param1);
}

void CallLogWrapper::glVertexAttrib1f (glw::GLuint param0, glw::GLfloat param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib1f(" << param0 << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib1f(param0, param1);
}

void CallLogWrapper::glVertexAttrib1fv (glw::GLuint param0, const glw::GLfloat* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib1fv(" << param0 << ", " << getPointerStr(param1, 1) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib1fv(param0, param1);
}

void CallLogWrapper::glVertexAttrib1s (glw::GLuint param0, glw::GLshort param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib1s(" << param0 << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib1s(param0, param1);
}

void CallLogWrapper::glVertexAttrib1sv (glw::GLuint param0, const glw::GLshort* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib1sv(" << param0 << ", " << getPointerStr(param1, 1) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib1sv(param0, param1);
}

void CallLogWrapper::glVertexAttrib2d (glw::GLuint param0, glw::GLdouble param1, glw::GLdouble param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib2d(" << param0 << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib2d(param0, param1, param2);
}

void CallLogWrapper::glVertexAttrib2dv (glw::GLuint param0, const glw::GLdouble* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib2dv(" << param0 << ", " << getPointerStr(param1, 2) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib2dv(param0, param1);
}

void CallLogWrapper::glVertexAttrib2f (glw::GLuint param0, glw::GLfloat param1, glw::GLfloat param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib2f(" << param0 << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib2f(param0, param1, param2);
}

void CallLogWrapper::glVertexAttrib2fv (glw::GLuint param0, const glw::GLfloat* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib2fv(" << param0 << ", " << getPointerStr(param1, 2) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib2fv(param0, param1);
}

void CallLogWrapper::glVertexAttrib2s (glw::GLuint param0, glw::GLshort param1, glw::GLshort param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib2s(" << param0 << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib2s(param0, param1, param2);
}

void CallLogWrapper::glVertexAttrib2sv (glw::GLuint param0, const glw::GLshort* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib2sv(" << param0 << ", " << getPointerStr(param1, 2) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib2sv(param0, param1);
}

void CallLogWrapper::glVertexAttrib3d (glw::GLuint param0, glw::GLdouble param1, glw::GLdouble param2, glw::GLdouble param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib3d(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib3d(param0, param1, param2, param3);
}

void CallLogWrapper::glVertexAttrib3dv (glw::GLuint param0, const glw::GLdouble* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib3dv(" << param0 << ", " << getPointerStr(param1, 3) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib3dv(param0, param1);
}

void CallLogWrapper::glVertexAttrib3f (glw::GLuint param0, glw::GLfloat param1, glw::GLfloat param2, glw::GLfloat param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib3f(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib3f(param0, param1, param2, param3);
}

void CallLogWrapper::glVertexAttrib3fv (glw::GLuint param0, const glw::GLfloat* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib3fv(" << param0 << ", " << getPointerStr(param1, 3) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib3fv(param0, param1);
}

void CallLogWrapper::glVertexAttrib3s (glw::GLuint param0, glw::GLshort param1, glw::GLshort param2, glw::GLshort param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib3s(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib3s(param0, param1, param2, param3);
}

void CallLogWrapper::glVertexAttrib3sv (glw::GLuint param0, const glw::GLshort* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib3sv(" << param0 << ", " << getPointerStr(param1, 3) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib3sv(param0, param1);
}

void CallLogWrapper::glVertexAttrib4Nbv (glw::GLuint param0, const glw::GLbyte* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4Nbv(" << param0 << ", " << getPointerStr(param1, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4Nbv(param0, param1);
}

void CallLogWrapper::glVertexAttrib4Niv (glw::GLuint param0, const glw::GLint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4Niv(" << param0 << ", " << getPointerStr(param1, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4Niv(param0, param1);
}

void CallLogWrapper::glVertexAttrib4Nsv (glw::GLuint param0, const glw::GLshort* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4Nsv(" << param0 << ", " << getPointerStr(param1, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4Nsv(param0, param1);
}

void CallLogWrapper::glVertexAttrib4Nub (glw::GLuint param0, glw::GLubyte param1, glw::GLubyte param2, glw::GLubyte param3, glw::GLubyte param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4Nub(" << param0 << ", " << toHex(param1) << ", " << toHex(param2) << ", " << toHex(param3) << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4Nub(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glVertexAttrib4Nubv (glw::GLuint param0, const glw::GLubyte* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4Nubv(" << param0 << ", " << getPointerStr(param1, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4Nubv(param0, param1);
}

void CallLogWrapper::glVertexAttrib4Nuiv (glw::GLuint param0, const glw::GLuint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4Nuiv(" << param0 << ", " << getPointerStr(param1, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4Nuiv(param0, param1);
}

void CallLogWrapper::glVertexAttrib4Nusv (glw::GLuint param0, const glw::GLushort* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4Nusv(" << param0 << ", " << getPointerStr(param1, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4Nusv(param0, param1);
}

void CallLogWrapper::glVertexAttrib4bv (glw::GLuint param0, const glw::GLbyte* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4bv(" << param0 << ", " << getPointerStr(param1, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4bv(param0, param1);
}

void CallLogWrapper::glVertexAttrib4d (glw::GLuint param0, glw::GLdouble param1, glw::GLdouble param2, glw::GLdouble param3, glw::GLdouble param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4d(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4d(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glVertexAttrib4dv (glw::GLuint param0, const glw::GLdouble* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4dv(" << param0 << ", " << getPointerStr(param1, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4dv(param0, param1);
}

void CallLogWrapper::glVertexAttrib4f (glw::GLuint param0, glw::GLfloat param1, glw::GLfloat param2, glw::GLfloat param3, glw::GLfloat param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4f(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4f(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glVertexAttrib4fv (glw::GLuint param0, const glw::GLfloat* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4fv(" << param0 << ", " << getPointerStr(param1, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4fv(param0, param1);
}

void CallLogWrapper::glVertexAttrib4iv (glw::GLuint param0, const glw::GLint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4iv(" << param0 << ", " << getPointerStr(param1, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4iv(param0, param1);
}

void CallLogWrapper::glVertexAttrib4s (glw::GLuint param0, glw::GLshort param1, glw::GLshort param2, glw::GLshort param3, glw::GLshort param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4s(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4s(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glVertexAttrib4sv (glw::GLuint param0, const glw::GLshort* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4sv(" << param0 << ", " << getPointerStr(param1, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4sv(param0, param1);
}

void CallLogWrapper::glVertexAttrib4ubv (glw::GLuint param0, const glw::GLubyte* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4ubv(" << param0 << ", " << getPointerStr(param1, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4ubv(param0, param1);
}

void CallLogWrapper::glVertexAttrib4uiv (glw::GLuint param0, const glw::GLuint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4uiv(" << param0 << ", " << getPointerStr(param1, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4uiv(param0, param1);
}

void CallLogWrapper::glVertexAttrib4usv (glw::GLuint param0, const glw::GLushort* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttrib4usv(" << param0 << ", " << getPointerStr(param1, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttrib4usv(param0, param1);
}

void CallLogWrapper::glVertexAttribPointer (glw::GLuint param0, glw::GLint param1, glw::GLenum param2, glw::GLboolean param3, glw::GLsizei param4, const glw::GLvoid* param5)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribPointer(" << param0 << ", " << param1 << ", " << getTypeStr(param2) << ", " << getBooleanStr(param3) << ", " << param4 << ", " << toHex(param5) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribPointer(param0, param1, param2, param3, param4, param5);
}

void CallLogWrapper::glUniformMatrix2x3fv (glw::GLint param0, glw::GLsizei param1, glw::GLboolean param2, const glw::GLfloat* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix2x3fv(" << param0 << ", " << param1 << ", " << getBooleanStr(param2) << ", " << getPointerStr(param3, (param1 * 2*3)) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix2x3fv(param0, param1, param2, param3);
}

void CallLogWrapper::glUniformMatrix3x2fv (glw::GLint param0, glw::GLsizei param1, glw::GLboolean param2, const glw::GLfloat* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix3x2fv(" << param0 << ", " << param1 << ", " << getBooleanStr(param2) << ", " << getPointerStr(param3, (param1 * 3*2)) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix3x2fv(param0, param1, param2, param3);
}

void CallLogWrapper::glUniformMatrix2x4fv (glw::GLint param0, glw::GLsizei param1, glw::GLboolean param2, const glw::GLfloat* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix2x4fv(" << param0 << ", " << param1 << ", " << getBooleanStr(param2) << ", " << getPointerStr(param3, (param1 * 2*4)) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix2x4fv(param0, param1, param2, param3);
}

void CallLogWrapper::glUniformMatrix4x2fv (glw::GLint param0, glw::GLsizei param1, glw::GLboolean param2, const glw::GLfloat* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix4x2fv(" << param0 << ", " << param1 << ", " << getBooleanStr(param2) << ", " << getPointerStr(param3, (param1 * 4*2)) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix4x2fv(param0, param1, param2, param3);
}

void CallLogWrapper::glUniformMatrix3x4fv (glw::GLint param0, glw::GLsizei param1, glw::GLboolean param2, const glw::GLfloat* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix3x4fv(" << param0 << ", " << param1 << ", " << getBooleanStr(param2) << ", " << getPointerStr(param3, (param1 * 3*4)) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix3x4fv(param0, param1, param2, param3);
}

void CallLogWrapper::glUniformMatrix4x3fv (glw::GLint param0, glw::GLsizei param1, glw::GLboolean param2, const glw::GLfloat* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix4x3fv(" << param0 << ", " << param1 << ", " << getBooleanStr(param2) << ", " << getPointerStr(param3, (param1 * 4*3)) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix4x3fv(param0, param1, param2, param3);
}

void CallLogWrapper::glColorMaski (glw::GLuint param0, glw::GLboolean param1, glw::GLboolean param2, glw::GLboolean param3, glw::GLboolean param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glColorMaski(" << param0 << ", " << getBooleanStr(param1) << ", " << getBooleanStr(param2) << ", " << getBooleanStr(param3) << ", " << getBooleanStr(param4) << ");" << TestLog::EndMessage;
	m_gl.colorMaski(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glGetBooleani_v (glw::GLenum param0, glw::GLuint param1, glw::GLboolean* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetBooleani_v(" << toHex(param0) << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getBooleani_v(param0, param1, param2);
}

void CallLogWrapper::glGetIntegeri_v (glw::GLenum param0, glw::GLuint param1, glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetIntegeri_v(" << getGettableIndexedStateStr(param0) << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getIntegeri_v(param0, param1, param2);
}

void CallLogWrapper::glEnablei (glw::GLenum param0, glw::GLuint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glEnablei(" << toHex(param0) << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.enablei(param0, param1);
}

void CallLogWrapper::glDisablei (glw::GLenum param0, glw::GLuint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDisablei(" << toHex(param0) << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.disablei(param0, param1);
}

glw::GLboolean CallLogWrapper::glIsEnabledi (glw::GLenum param0, glw::GLuint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsEnabledi(" << toHex(param0) << ", " << param1 << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isEnabledi(param0, param1);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glBeginTransformFeedback (glw::GLenum param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBeginTransformFeedback(" << getPrimitiveTypeStr(param0) << ");" << TestLog::EndMessage;
	m_gl.beginTransformFeedback(param0);
}

void CallLogWrapper::glEndTransformFeedback ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "glEndTransformFeedback(" << ");" << TestLog::EndMessage;
	m_gl.endTransformFeedback();
}

void CallLogWrapper::glBindBufferRange (glw::GLenum param0, glw::GLuint param1, glw::GLuint param2, glw::GLintptr param3, glw::GLsizeiptr param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindBufferRange(" << getBufferTargetStr(param0) << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.bindBufferRange(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glBindBufferBase (glw::GLenum param0, glw::GLuint param1, glw::GLuint param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindBufferBase(" << getBufferTargetStr(param0) << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.bindBufferBase(param0, param1, param2);
}

void CallLogWrapper::glTransformFeedbackVaryings (glw::GLuint param0, glw::GLsizei param1, const glw::GLchar* const* param2, glw::GLenum param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTransformFeedbackVaryings(" << param0 << ", " << param1 << ", " << toHex(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.transformFeedbackVaryings(param0, param1, param2, param3);
}

void CallLogWrapper::glGetTransformFeedbackVarying (glw::GLuint param0, glw::GLuint param1, glw::GLsizei param2, glw::GLsizei* param3, glw::GLsizei* param4, glw::GLenum* param5, glw::GLchar* param6)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTransformFeedbackVarying(" << param0 << ", " << param1 << ", " << param2 << ", " << toHex(param3) << ", " << toHex(param4) << ", " << toHex(param5) << ", " << toHex(param6) << ");" << TestLog::EndMessage;
	m_gl.getTransformFeedbackVarying(param0, param1, param2, param3, param4, param5, param6);
}

void CallLogWrapper::glClampColor (glw::GLenum param0, glw::GLenum param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClampColor(" << toHex(param0) << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.clampColor(param0, param1);
}

void CallLogWrapper::glBeginConditionalRender (glw::GLuint param0, glw::GLenum param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBeginConditionalRender(" << param0 << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.beginConditionalRender(param0, param1);
}

void CallLogWrapper::glEndConditionalRender ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "glEndConditionalRender(" << ");" << TestLog::EndMessage;
	m_gl.endConditionalRender();
}

void CallLogWrapper::glVertexAttribIPointer (glw::GLuint param0, glw::GLint param1, glw::GLenum param2, glw::GLsizei param3, const glw::GLvoid* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribIPointer(" << param0 << ", " << param1 << ", " << getTypeStr(param2) << ", " << param3 << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribIPointer(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glGetVertexAttribIiv (glw::GLuint param0, glw::GLenum param1, glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetVertexAttribIiv(" << param0 << ", " << getVertexAttribParameterNameStr(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getVertexAttribIiv(param0, param1, param2);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 2 = " << getPointerStr(param2, (param1 == GL_CURRENT_VERTEX_ATTRIB ? 4 : 1)) << TestLog::EndMessage;
	}
}

void CallLogWrapper::glGetVertexAttribIuiv (glw::GLuint param0, glw::GLenum param1, glw::GLuint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetVertexAttribIuiv(" << param0 << ", " << toHex(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getVertexAttribIuiv(param0, param1, param2);
}

void CallLogWrapper::glVertexAttribI1i (glw::GLuint param0, glw::GLint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI1i(" << param0 << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI1i(param0, param1);
}

void CallLogWrapper::glVertexAttribI2i (glw::GLuint param0, glw::GLint param1, glw::GLint param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI2i(" << param0 << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI2i(param0, param1, param2);
}

void CallLogWrapper::glVertexAttribI3i (glw::GLuint param0, glw::GLint param1, glw::GLint param2, glw::GLint param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI3i(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI3i(param0, param1, param2, param3);
}

void CallLogWrapper::glVertexAttribI4i (glw::GLuint param0, glw::GLint param1, glw::GLint param2, glw::GLint param3, glw::GLint param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI4i(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI4i(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glVertexAttribI1ui (glw::GLuint param0, glw::GLuint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI1ui(" << param0 << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI1ui(param0, param1);
}

void CallLogWrapper::glVertexAttribI2ui (glw::GLuint param0, glw::GLuint param1, glw::GLuint param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI2ui(" << param0 << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI2ui(param0, param1, param2);
}

void CallLogWrapper::glVertexAttribI3ui (glw::GLuint param0, glw::GLuint param1, glw::GLuint param2, glw::GLuint param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI3ui(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI3ui(param0, param1, param2, param3);
}

void CallLogWrapper::glVertexAttribI4ui (glw::GLuint param0, glw::GLuint param1, glw::GLuint param2, glw::GLuint param3, glw::GLuint param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI4ui(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI4ui(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glVertexAttribI1iv (glw::GLuint param0, const glw::GLint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI1iv(" << param0 << ", " << getPointerStr(param1, 1) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI1iv(param0, param1);
}

void CallLogWrapper::glVertexAttribI2iv (glw::GLuint param0, const glw::GLint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI2iv(" << param0 << ", " << getPointerStr(param1, 2) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI2iv(param0, param1);
}

void CallLogWrapper::glVertexAttribI3iv (glw::GLuint param0, const glw::GLint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI3iv(" << param0 << ", " << getPointerStr(param1, 3) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI3iv(param0, param1);
}

void CallLogWrapper::glVertexAttribI4iv (glw::GLuint param0, const glw::GLint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI4iv(" << param0 << ", " << getPointerStr(param1, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI4iv(param0, param1);
}

void CallLogWrapper::glVertexAttribI1uiv (glw::GLuint param0, const glw::GLuint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI1uiv(" << param0 << ", " << getPointerStr(param1, 1) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI1uiv(param0, param1);
}

void CallLogWrapper::glVertexAttribI2uiv (glw::GLuint param0, const glw::GLuint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI2uiv(" << param0 << ", " << getPointerStr(param1, 2) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI2uiv(param0, param1);
}

void CallLogWrapper::glVertexAttribI3uiv (glw::GLuint param0, const glw::GLuint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI3uiv(" << param0 << ", " << getPointerStr(param1, 3) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI3uiv(param0, param1);
}

void CallLogWrapper::glVertexAttribI4uiv (glw::GLuint param0, const glw::GLuint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI4uiv(" << param0 << ", " << getPointerStr(param1, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI4uiv(param0, param1);
}

void CallLogWrapper::glVertexAttribI4bv (glw::GLuint param0, const glw::GLbyte* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI4bv(" << param0 << ", " << getPointerStr(param1, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI4bv(param0, param1);
}

void CallLogWrapper::glVertexAttribI4sv (glw::GLuint param0, const glw::GLshort* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI4sv(" << param0 << ", " << getPointerStr(param1, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI4sv(param0, param1);
}

void CallLogWrapper::glVertexAttribI4ubv (glw::GLuint param0, const glw::GLubyte* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI4ubv(" << param0 << ", " << getPointerStr(param1, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI4ubv(param0, param1);
}

void CallLogWrapper::glVertexAttribI4usv (glw::GLuint param0, const glw::GLushort* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribI4usv(" << param0 << ", " << getPointerStr(param1, 4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribI4usv(param0, param1);
}

void CallLogWrapper::glGetUniformuiv (glw::GLuint param0, glw::GLint param1, glw::GLuint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetUniformuiv(" << param0 << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getUniformuiv(param0, param1, param2);
}

void CallLogWrapper::glBindFragDataLocation (glw::GLuint param0, glw::GLuint param1, const glw::GLchar* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindFragDataLocation(" << param0 << ", " << param1 << ", " << getStringStr(param2) << ");" << TestLog::EndMessage;
	m_gl.bindFragDataLocation(param0, param1, param2);
}

glw::GLint CallLogWrapper::glGetFragDataLocation (glw::GLuint param0, const glw::GLchar* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetFragDataLocation(" << param0 << ", " << getStringStr(param1) << ");" << TestLog::EndMessage;
	glw::GLint returnValue = m_gl.getFragDataLocation(param0, param1);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glUniform1ui (glw::GLint param0, glw::GLuint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform1ui(" << param0 << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.uniform1ui(param0, param1);
}

void CallLogWrapper::glUniform2ui (glw::GLint param0, glw::GLuint param1, glw::GLuint param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform2ui(" << param0 << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.uniform2ui(param0, param1, param2);
}

void CallLogWrapper::glUniform3ui (glw::GLint param0, glw::GLuint param1, glw::GLuint param2, glw::GLuint param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform3ui(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.uniform3ui(param0, param1, param2, param3);
}

void CallLogWrapper::glUniform4ui (glw::GLint param0, glw::GLuint param1, glw::GLuint param2, glw::GLuint param3, glw::GLuint param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform4ui(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.uniform4ui(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glUniform1uiv (glw::GLint param0, glw::GLsizei param1, const glw::GLuint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform1uiv(" << param0 << ", " << param1 << ", " << getPointerStr(param2, (param1 * 1)) << ");" << TestLog::EndMessage;
	m_gl.uniform1uiv(param0, param1, param2);
}

void CallLogWrapper::glUniform2uiv (glw::GLint param0, glw::GLsizei param1, const glw::GLuint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform2uiv(" << param0 << ", " << param1 << ", " << getPointerStr(param2, (param1 * 2)) << ");" << TestLog::EndMessage;
	m_gl.uniform2uiv(param0, param1, param2);
}

void CallLogWrapper::glUniform3uiv (glw::GLint param0, glw::GLsizei param1, const glw::GLuint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform3uiv(" << param0 << ", " << param1 << ", " << getPointerStr(param2, (param1 * 3)) << ");" << TestLog::EndMessage;
	m_gl.uniform3uiv(param0, param1, param2);
}

void CallLogWrapper::glUniform4uiv (glw::GLint param0, glw::GLsizei param1, const glw::GLuint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform4uiv(" << param0 << ", " << param1 << ", " << getPointerStr(param2, (param1 * 4)) << ");" << TestLog::EndMessage;
	m_gl.uniform4uiv(param0, param1, param2);
}

void CallLogWrapper::glTexParameterIiv (glw::GLenum param0, glw::GLenum param1, const glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexParameterIiv(" << toHex(param0) << ", " << toHex(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.texParameterIiv(param0, param1, param2);
}

void CallLogWrapper::glTexParameterIuiv (glw::GLenum param0, glw::GLenum param1, const glw::GLuint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexParameterIuiv(" << toHex(param0) << ", " << toHex(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.texParameterIuiv(param0, param1, param2);
}

void CallLogWrapper::glGetTexParameterIiv (glw::GLenum param0, glw::GLenum param1, glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTexParameterIiv(" << toHex(param0) << ", " << toHex(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getTexParameterIiv(param0, param1, param2);
}

void CallLogWrapper::glGetTexParameterIuiv (glw::GLenum param0, glw::GLenum param1, glw::GLuint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetTexParameterIuiv(" << toHex(param0) << ", " << toHex(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getTexParameterIuiv(param0, param1, param2);
}

void CallLogWrapper::glClearBufferiv (glw::GLenum param0, glw::GLint param1, const glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearBufferiv(" << getBufferStr(param0) << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.clearBufferiv(param0, param1, param2);
}

void CallLogWrapper::glClearBufferuiv (glw::GLenum param0, glw::GLint param1, const glw::GLuint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearBufferuiv(" << getBufferStr(param0) << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.clearBufferuiv(param0, param1, param2);
}

void CallLogWrapper::glClearBufferfv (glw::GLenum param0, glw::GLint param1, const glw::GLfloat* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearBufferfv(" << getBufferStr(param0) << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.clearBufferfv(param0, param1, param2);
}

void CallLogWrapper::glClearBufferfi (glw::GLenum param0, glw::GLint param1, glw::GLfloat param2, glw::GLint param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearBufferfi(" << getBufferStr(param0) << ", " << param1 << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.clearBufferfi(param0, param1, param2, param3);
}

const glw::GLubyte* CallLogWrapper::glGetStringi (glw::GLenum param0, glw::GLuint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetStringi(" << getGettableStringStr(param0) << ", " << param1 << ");" << TestLog::EndMessage;
	const glw::GLubyte* returnValue = m_gl.getStringi(param0, param1);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getStringStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLboolean CallLogWrapper::glIsRenderbuffer (glw::GLuint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsRenderbuffer(" << param0 << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isRenderbuffer(param0);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glBindRenderbuffer (glw::GLenum param0, glw::GLuint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindRenderbuffer(" << getFramebufferTargetStr(param0) << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.bindRenderbuffer(param0, param1);
}

void CallLogWrapper::glDeleteRenderbuffers (glw::GLsizei param0, const glw::GLuint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDeleteRenderbuffers(" << param0 << ", " << getPointerStr(param1, param0) << ");" << TestLog::EndMessage;
	m_gl.deleteRenderbuffers(param0, param1);
}

void CallLogWrapper::glGenRenderbuffers (glw::GLsizei param0, glw::GLuint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGenRenderbuffers(" << param0 << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.genRenderbuffers(param0, param1);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 1 = " << getPointerStr(param1, param0) << TestLog::EndMessage;
	}
}

void CallLogWrapper::glRenderbufferStorage (glw::GLenum param0, glw::GLenum param1, glw::GLsizei param2, glw::GLsizei param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glRenderbufferStorage(" << getFramebufferTargetStr(param0) << ", " << getPixelFormatStr(param1) << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.renderbufferStorage(param0, param1, param2, param3);
}

void CallLogWrapper::glGetRenderbufferParameteriv (glw::GLenum param0, glw::GLenum param1, glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetRenderbufferParameteriv(" << getFramebufferTargetStr(param0) << ", " << getRenderbufferParameterStr(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getRenderbufferParameteriv(param0, param1, param2);
}

glw::GLboolean CallLogWrapper::glIsFramebuffer (glw::GLuint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsFramebuffer(" << param0 << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isFramebuffer(param0);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glBindFramebuffer (glw::GLenum param0, glw::GLuint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindFramebuffer(" << getFramebufferTargetStr(param0) << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.bindFramebuffer(param0, param1);
}

void CallLogWrapper::glDeleteFramebuffers (glw::GLsizei param0, const glw::GLuint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDeleteFramebuffers(" << param0 << ", " << getPointerStr(param1, param0) << ");" << TestLog::EndMessage;
	m_gl.deleteFramebuffers(param0, param1);
}

void CallLogWrapper::glGenFramebuffers (glw::GLsizei param0, glw::GLuint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGenFramebuffers(" << param0 << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.genFramebuffers(param0, param1);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 1 = " << getPointerStr(param1, param0) << TestLog::EndMessage;
	}
}

glw::GLenum CallLogWrapper::glCheckFramebufferStatus (glw::GLenum param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCheckFramebufferStatus(" << getFramebufferTargetStr(param0) << ");" << TestLog::EndMessage;
	glw::GLenum returnValue = m_gl.checkFramebufferStatus(param0);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getFramebufferStatusStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glFramebufferTexture1D (glw::GLenum param0, glw::GLenum param1, glw::GLenum param2, glw::GLuint param3, glw::GLint param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFramebufferTexture1D(" << toHex(param0) << ", " << toHex(param1) << ", " << toHex(param2) << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.framebufferTexture1D(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glFramebufferTexture2D (glw::GLenum param0, glw::GLenum param1, glw::GLenum param2, glw::GLuint param3, glw::GLint param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFramebufferTexture2D(" << getFramebufferTargetStr(param0) << ", " << getFramebufferAttachmentStr(param1) << ", " << getTextureTargetStr(param2) << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.framebufferTexture2D(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glFramebufferTexture3D (glw::GLenum param0, glw::GLenum param1, glw::GLenum param2, glw::GLuint param3, glw::GLint param4, glw::GLint param5)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFramebufferTexture3D(" << toHex(param0) << ", " << toHex(param1) << ", " << toHex(param2) << ", " << param3 << ", " << param4 << ", " << param5 << ");" << TestLog::EndMessage;
	m_gl.framebufferTexture3D(param0, param1, param2, param3, param4, param5);
}

void CallLogWrapper::glFramebufferRenderbuffer (glw::GLenum param0, glw::GLenum param1, glw::GLenum param2, glw::GLuint param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFramebufferRenderbuffer(" << getFramebufferTargetStr(param0) << ", " << getFramebufferAttachmentStr(param1) << ", " << getFramebufferTargetStr(param2) << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.framebufferRenderbuffer(param0, param1, param2, param3);
}

void CallLogWrapper::glGetFramebufferAttachmentParameteriv (glw::GLenum param0, glw::GLenum param1, glw::GLenum param2, glw::GLint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetFramebufferAttachmentParameteriv(" << getFramebufferTargetStr(param0) << ", " << getFramebufferAttachmentStr(param1) << ", " << getFramebufferAttachmentParameterStr(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.getFramebufferAttachmentParameteriv(param0, param1, param2, param3);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 3 = " << getFramebufferAttachmentParameterValueStr(param2, param3) << TestLog::EndMessage;
	}
}

void CallLogWrapper::glGenerateMipmap (glw::GLenum param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGenerateMipmap(" << getTextureTargetStr(param0) << ");" << TestLog::EndMessage;
	m_gl.generateMipmap(param0);
}

void CallLogWrapper::glBlitFramebuffer (glw::GLint param0, glw::GLint param1, glw::GLint param2, glw::GLint param3, glw::GLint param4, glw::GLint param5, glw::GLint param6, glw::GLint param7, glw::GLbitfield param8, glw::GLenum param9)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBlitFramebuffer(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ", " << param5 << ", " << param6 << ", " << param7 << ", " << getBufferMaskStr(param8) << ", " << getTextureFilterStr(param9) << ");" << TestLog::EndMessage;
	m_gl.blitFramebuffer(param0, param1, param2, param3, param4, param5, param6, param7, param8, param9);
}

void CallLogWrapper::glRenderbufferStorageMultisample (glw::GLenum param0, glw::GLsizei param1, glw::GLenum param2, glw::GLsizei param3, glw::GLsizei param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glRenderbufferStorageMultisample(" << getFramebufferTargetStr(param0) << ", " << param1 << ", " << getPixelFormatStr(param2) << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.renderbufferStorageMultisample(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glFramebufferTextureLayer (glw::GLenum param0, glw::GLenum param1, glw::GLuint param2, glw::GLint param3, glw::GLint param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFramebufferTextureLayer(" << getFramebufferTargetStr(param0) << ", " << getFramebufferAttachmentStr(param1) << ", " << param2 << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.framebufferTextureLayer(param0, param1, param2, param3, param4);
}

glw::GLvoid* CallLogWrapper::glMapBufferRange (glw::GLenum param0, glw::GLintptr param1, glw::GLsizeiptr param2, glw::GLbitfield param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMapBufferRange(" << getBufferTargetStr(param0) << ", " << param1 << ", " << param2 << ", " << getBufferMapFlagsStr(param3) << ");" << TestLog::EndMessage;
	glw::GLvoid* returnValue = m_gl.mapBufferRange(param0, param1, param2, param3);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glFlushMappedBufferRange (glw::GLenum param0, glw::GLintptr param1, glw::GLsizeiptr param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFlushMappedBufferRange(" << getBufferTargetStr(param0) << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.flushMappedBufferRange(param0, param1, param2);
}

void CallLogWrapper::glBindVertexArray (glw::GLuint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindVertexArray(" << param0 << ");" << TestLog::EndMessage;
	m_gl.bindVertexArray(param0);
}

void CallLogWrapper::glDeleteVertexArrays (glw::GLsizei param0, const glw::GLuint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDeleteVertexArrays(" << param0 << ", " << getPointerStr(param1, param0) << ");" << TestLog::EndMessage;
	m_gl.deleteVertexArrays(param0, param1);
}

void CallLogWrapper::glGenVertexArrays (glw::GLsizei param0, glw::GLuint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGenVertexArrays(" << param0 << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.genVertexArrays(param0, param1);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 1 = " << getPointerStr(param1, param0) << TestLog::EndMessage;
	}
}

glw::GLboolean CallLogWrapper::glIsVertexArray (glw::GLuint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsVertexArray(" << param0 << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isVertexArray(param0);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glTexBuffer (glw::GLenum param0, glw::GLenum param1, glw::GLuint param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexBuffer(" << getBufferTargetStr(param0) << ", " << getPixelFormatStr(param1) << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.texBuffer(param0, param1, param2);
}

void CallLogWrapper::glPrimitiveRestartIndex (glw::GLuint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPrimitiveRestartIndex(" << param0 << ");" << TestLog::EndMessage;
	m_gl.primitiveRestartIndex(param0);
}

void CallLogWrapper::glCopyBufferSubData (glw::GLenum param0, glw::GLenum param1, glw::GLintptr param2, glw::GLintptr param3, glw::GLsizeiptr param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyBufferSubData(" << toHex(param0) << ", " << toHex(param1) << ", " << param2 << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.copyBufferSubData(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glDrawArraysInstanced (glw::GLenum param0, glw::GLint param1, glw::GLsizei param2, glw::GLsizei param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawArraysInstanced(" << getPrimitiveTypeStr(param0) << ", " << param1 << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.drawArraysInstanced(param0, param1, param2, param3);
}

void CallLogWrapper::glDrawElementsInstanced (glw::GLenum param0, glw::GLsizei param1, glw::GLenum param2, const glw::GLvoid* param3, glw::GLsizei param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawElementsInstanced(" << getPrimitiveTypeStr(param0) << ", " << param1 << ", " << getTypeStr(param2) << ", " << toHex(param3) << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.drawElementsInstanced(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glGetUniformIndices (glw::GLuint param0, glw::GLsizei param1, const glw::GLchar* const* param2, glw::GLuint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetUniformIndices(" << param0 << ", " << param1 << ", " << getPointerStr(param2, param1) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.getUniformIndices(param0, param1, param2, param3);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 3 = " << getPointerStr(param3, param1) << TestLog::EndMessage;
	}
}

void CallLogWrapper::glGetActiveUniformsiv (glw::GLuint param0, glw::GLsizei param1, const glw::GLuint* param2, glw::GLenum param3, glw::GLint* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetActiveUniformsiv(" << param0 << ", " << param1 << ", " << getPointerStr(param2, param1) << ", " << getUniformParamStr(param3) << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.getActiveUniformsiv(param0, param1, param2, param3, param4);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 4 = " << getPointerStr(param4, param1) << TestLog::EndMessage;
	}
}

void CallLogWrapper::glGetActiveUniformName (glw::GLuint param0, glw::GLuint param1, glw::GLsizei param2, glw::GLsizei* param3, glw::GLchar* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetActiveUniformName(" << param0 << ", " << param1 << ", " << param2 << ", " << toHex(param3) << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.getActiveUniformName(param0, param1, param2, param3, param4);
}

glw::GLuint CallLogWrapper::glGetUniformBlockIndex (glw::GLuint param0, const glw::GLchar* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetUniformBlockIndex(" << param0 << ", " << getStringStr(param1) << ");" << TestLog::EndMessage;
	glw::GLuint returnValue = m_gl.getUniformBlockIndex(param0, param1);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glGetActiveUniformBlockiv (glw::GLuint param0, glw::GLuint param1, glw::GLenum param2, glw::GLint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetActiveUniformBlockiv(" << param0 << ", " << param1 << ", " << toHex(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.getActiveUniformBlockiv(param0, param1, param2, param3);
}

void CallLogWrapper::glGetActiveUniformBlockName (glw::GLuint param0, glw::GLuint param1, glw::GLsizei param2, glw::GLsizei* param3, glw::GLchar* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetActiveUniformBlockName(" << param0 << ", " << param1 << ", " << param2 << ", " << toHex(param3) << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.getActiveUniformBlockName(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glUniformBlockBinding (glw::GLuint param0, glw::GLuint param1, glw::GLuint param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformBlockBinding(" << param0 << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.uniformBlockBinding(param0, param1, param2);
}

void CallLogWrapper::glGetInteger64i_v (glw::GLenum param0, glw::GLuint param1, glw::GLint64* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetInteger64i_v(" << getGettableIndexedStateStr(param0) << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getInteger64i_v(param0, param1, param2);
}

void CallLogWrapper::glGetBufferParameteri64v (glw::GLenum param0, glw::GLenum param1, glw::GLint64* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetBufferParameteri64v(" << getBufferTargetStr(param0) << ", " << getBufferQueryStr(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getBufferParameteri64v(param0, param1, param2);
}

void CallLogWrapper::glFramebufferTexture (glw::GLenum param0, glw::GLenum param1, glw::GLuint param2, glw::GLint param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFramebufferTexture(" << getFramebufferTargetStr(param0) << ", " << getFramebufferAttachmentStr(param1) << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.framebufferTexture(param0, param1, param2, param3);
}

void CallLogWrapper::glDrawElementsBaseVertex (glw::GLenum param0, glw::GLsizei param1, glw::GLenum param2, const glw::GLvoid* param3, glw::GLint param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawElementsBaseVertex(" << getPrimitiveTypeStr(param0) << ", " << param1 << ", " << getTypeStr(param2) << ", " << toHex(param3) << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.drawElementsBaseVertex(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glDrawRangeElementsBaseVertex (glw::GLenum param0, glw::GLuint param1, glw::GLuint param2, glw::GLsizei param3, glw::GLenum param4, const glw::GLvoid* param5, glw::GLint param6)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawRangeElementsBaseVertex(" << getPrimitiveTypeStr(param0) << ", " << param1 << ", " << param2 << ", " << param3 << ", " << getTypeStr(param4) << ", " << toHex(param5) << ", " << param6 << ");" << TestLog::EndMessage;
	m_gl.drawRangeElementsBaseVertex(param0, param1, param2, param3, param4, param5, param6);
}

void CallLogWrapper::glDrawElementsInstancedBaseVertex (glw::GLenum param0, glw::GLsizei param1, glw::GLenum param2, const glw::GLvoid* param3, glw::GLsizei param4, glw::GLint param5)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawElementsInstancedBaseVertex(" << getPrimitiveTypeStr(param0) << ", " << param1 << ", " << getTypeStr(param2) << ", " << toHex(param3) << ", " << param4 << ", " << param5 << ");" << TestLog::EndMessage;
	m_gl.drawElementsInstancedBaseVertex(param0, param1, param2, param3, param4, param5);
}

void CallLogWrapper::glMultiDrawElementsBaseVertex (glw::GLenum param0, const glw::GLsizei* param1, glw::GLenum param2, const glw::GLvoid* const* param3, glw::GLsizei param4, const glw::GLint* param5)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiDrawElementsBaseVertex(" << getPrimitiveTypeStr(param0) << ", " << toHex(param1) << ", " << getTypeStr(param2) << ", " << toHex(param3) << ", " << param4 << ", " << toHex(param5) << ");" << TestLog::EndMessage;
	m_gl.multiDrawElementsBaseVertex(param0, param1, param2, param3, param4, param5);
}

void CallLogWrapper::glProvokingVertex (glw::GLenum param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProvokingVertex(" << getProvokingVertexStr(param0) << ");" << TestLog::EndMessage;
	m_gl.provokingVertex(param0);
}

glw::GLsync CallLogWrapper::glFenceSync (glw::GLenum param0, glw::GLbitfield param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFenceSync(" << toHex(param0) << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	glw::GLsync returnValue = m_gl.fenceSync(param0, param1);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLboolean CallLogWrapper::glIsSync (glw::GLsync param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsSync(" << param0 << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isSync(param0);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glDeleteSync (glw::GLsync param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDeleteSync(" << param0 << ");" << TestLog::EndMessage;
	m_gl.deleteSync(param0);
}

glw::GLenum CallLogWrapper::glClientWaitSync (glw::GLsync param0, glw::GLbitfield param1, glw::GLuint64 param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClientWaitSync(" << param0 << ", " << toHex(param1) << ", " << param2 << ");" << TestLog::EndMessage;
	glw::GLenum returnValue = m_gl.clientWaitSync(param0, param1, param2);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glWaitSync (glw::GLsync param0, glw::GLbitfield param1, glw::GLuint64 param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glWaitSync(" << param0 << ", " << toHex(param1) << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.waitSync(param0, param1, param2);
}

void CallLogWrapper::glGetInteger64v (glw::GLenum param0, glw::GLint64* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetInteger64v(" << getGettableStateStr(param0) << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.getInteger64v(param0, param1);
}

void CallLogWrapper::glGetSynciv (glw::GLsync param0, glw::GLenum param1, glw::GLsizei param2, glw::GLsizei* param3, glw::GLint* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetSynciv(" << param0 << ", " << toHex(param1) << ", " << param2 << ", " << toHex(param3) << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.getSynciv(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glTexImage2DMultisample (glw::GLenum param0, glw::GLsizei param1, glw::GLint param2, glw::GLsizei param3, glw::GLsizei param4, glw::GLboolean param5)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexImage2DMultisample(" << getTextureTargetStr(param0) << ", " << param1 << ", " << getPixelFormatStr(param2) << ", " << param3 << ", " << param4 << ", " << getBooleanStr(param5) << ");" << TestLog::EndMessage;
	m_gl.texImage2DMultisample(param0, param1, param2, param3, param4, param5);
}

void CallLogWrapper::glTexImage3DMultisample (glw::GLenum param0, glw::GLsizei param1, glw::GLint param2, glw::GLsizei param3, glw::GLsizei param4, glw::GLsizei param5, glw::GLboolean param6)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexImage3DMultisample(" << toHex(param0) << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ", " << param5 << ", " << getBooleanStr(param6) << ");" << TestLog::EndMessage;
	m_gl.texImage3DMultisample(param0, param1, param2, param3, param4, param5, param6);
}

void CallLogWrapper::glGetMultisamplefv (glw::GLenum param0, glw::GLuint param1, glw::GLfloat* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetMultisamplefv(" << getMultisampleParameterStr(param0) << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getMultisamplefv(param0, param1, param2);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 2 = " << getPointerStr(param2, 2) << TestLog::EndMessage;
	}
}

void CallLogWrapper::glSampleMaski (glw::GLuint param0, glw::GLbitfield param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glSampleMaski(" << param0 << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.sampleMaski(param0, param1);
}

void CallLogWrapper::glVertexAttribDivisor (glw::GLuint param0, glw::GLuint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribDivisor(" << param0 << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.vertexAttribDivisor(param0, param1);
}

void CallLogWrapper::glBindFragDataLocationIndexed (glw::GLuint param0, glw::GLuint param1, glw::GLuint param2, const glw::GLchar* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindFragDataLocationIndexed(" << param0 << ", " << param1 << ", " << param2 << ", " << getStringStr(param3) << ");" << TestLog::EndMessage;
	m_gl.bindFragDataLocationIndexed(param0, param1, param2, param3);
}

glw::GLint CallLogWrapper::glGetFragDataIndex (glw::GLuint param0, const glw::GLchar* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetFragDataIndex(" << param0 << ", " << getStringStr(param1) << ");" << TestLog::EndMessage;
	glw::GLint returnValue = m_gl.getFragDataIndex(param0, param1);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glGenSamplers (glw::GLsizei param0, glw::GLuint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGenSamplers(" << param0 << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.genSamplers(param0, param1);
}

void CallLogWrapper::glDeleteSamplers (glw::GLsizei param0, const glw::GLuint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDeleteSamplers(" << param0 << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.deleteSamplers(param0, param1);
}

glw::GLboolean CallLogWrapper::glIsSampler (glw::GLuint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsSampler(" << param0 << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isSampler(param0);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glBindSampler (glw::GLuint param0, glw::GLuint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindSampler(" << param0 << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.bindSampler(param0, param1);
}

void CallLogWrapper::glSamplerParameteri (glw::GLuint param0, glw::GLenum param1, glw::GLint param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glSamplerParameteri(" << param0 << ", " << toHex(param1) << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.samplerParameteri(param0, param1, param2);
}

void CallLogWrapper::glSamplerParameteriv (glw::GLuint param0, glw::GLenum param1, const glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glSamplerParameteriv(" << param0 << ", " << toHex(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.samplerParameteriv(param0, param1, param2);
}

void CallLogWrapper::glSamplerParameterf (glw::GLuint param0, glw::GLenum param1, glw::GLfloat param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glSamplerParameterf(" << param0 << ", " << toHex(param1) << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.samplerParameterf(param0, param1, param2);
}

void CallLogWrapper::glSamplerParameterfv (glw::GLuint param0, glw::GLenum param1, const glw::GLfloat* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glSamplerParameterfv(" << param0 << ", " << toHex(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.samplerParameterfv(param0, param1, param2);
}

void CallLogWrapper::glSamplerParameterIiv (glw::GLuint param0, glw::GLenum param1, const glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glSamplerParameterIiv(" << param0 << ", " << toHex(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.samplerParameterIiv(param0, param1, param2);
}

void CallLogWrapper::glSamplerParameterIuiv (glw::GLuint param0, glw::GLenum param1, const glw::GLuint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glSamplerParameterIuiv(" << param0 << ", " << toHex(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.samplerParameterIuiv(param0, param1, param2);
}

void CallLogWrapper::glGetSamplerParameteriv (glw::GLuint param0, glw::GLenum param1, glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetSamplerParameteriv(" << param0 << ", " << getTextureParameterStr(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getSamplerParameteriv(param0, param1, param2);
}

void CallLogWrapper::glGetSamplerParameterIiv (glw::GLuint param0, glw::GLenum param1, glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetSamplerParameterIiv(" << param0 << ", " << toHex(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getSamplerParameterIiv(param0, param1, param2);
}

void CallLogWrapper::glGetSamplerParameterfv (glw::GLuint param0, glw::GLenum param1, glw::GLfloat* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetSamplerParameterfv(" << param0 << ", " << getTextureParameterStr(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getSamplerParameterfv(param0, param1, param2);
}

void CallLogWrapper::glGetSamplerParameterIuiv (glw::GLuint param0, glw::GLenum param1, glw::GLuint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetSamplerParameterIuiv(" << param0 << ", " << toHex(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getSamplerParameterIuiv(param0, param1, param2);
}

void CallLogWrapper::glQueryCounter (glw::GLuint param0, glw::GLenum param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glQueryCounter(" << param0 << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.queryCounter(param0, param1);
}

void CallLogWrapper::glGetQueryObjecti64v (glw::GLuint param0, glw::GLenum param1, glw::GLint64* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetQueryObjecti64v(" << param0 << ", " << getQueryObjectParamStr(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getQueryObjecti64v(param0, param1, param2);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 2 = " << getPointerStr(param2, 1) << TestLog::EndMessage;
	}
}

void CallLogWrapper::glGetQueryObjectui64v (glw::GLuint param0, glw::GLenum param1, glw::GLuint64* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetQueryObjectui64v(" << param0 << ", " << getQueryObjectParamStr(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getQueryObjectui64v(param0, param1, param2);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 2 = " << getPointerStr(param2, 1) << TestLog::EndMessage;
	}
}

void CallLogWrapper::glVertexAttribP1ui (glw::GLuint param0, glw::GLenum param1, glw::GLboolean param2, glw::GLuint param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribP1ui(" << param0 << ", " << toHex(param1) << ", " << getBooleanStr(param2) << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.vertexAttribP1ui(param0, param1, param2, param3);
}

void CallLogWrapper::glVertexAttribP1uiv (glw::GLuint param0, glw::GLenum param1, glw::GLboolean param2, const glw::GLuint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribP1uiv(" << param0 << ", " << toHex(param1) << ", " << getBooleanStr(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribP1uiv(param0, param1, param2, param3);
}

void CallLogWrapper::glVertexAttribP2ui (glw::GLuint param0, glw::GLenum param1, glw::GLboolean param2, glw::GLuint param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribP2ui(" << param0 << ", " << toHex(param1) << ", " << getBooleanStr(param2) << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.vertexAttribP2ui(param0, param1, param2, param3);
}

void CallLogWrapper::glVertexAttribP2uiv (glw::GLuint param0, glw::GLenum param1, glw::GLboolean param2, const glw::GLuint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribP2uiv(" << param0 << ", " << toHex(param1) << ", " << getBooleanStr(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribP2uiv(param0, param1, param2, param3);
}

void CallLogWrapper::glVertexAttribP3ui (glw::GLuint param0, glw::GLenum param1, glw::GLboolean param2, glw::GLuint param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribP3ui(" << param0 << ", " << toHex(param1) << ", " << getBooleanStr(param2) << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.vertexAttribP3ui(param0, param1, param2, param3);
}

void CallLogWrapper::glVertexAttribP3uiv (glw::GLuint param0, glw::GLenum param1, glw::GLboolean param2, const glw::GLuint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribP3uiv(" << param0 << ", " << toHex(param1) << ", " << getBooleanStr(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribP3uiv(param0, param1, param2, param3);
}

void CallLogWrapper::glVertexAttribP4ui (glw::GLuint param0, glw::GLenum param1, glw::GLboolean param2, glw::GLuint param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribP4ui(" << param0 << ", " << toHex(param1) << ", " << getBooleanStr(param2) << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.vertexAttribP4ui(param0, param1, param2, param3);
}

void CallLogWrapper::glVertexAttribP4uiv (glw::GLuint param0, glw::GLenum param1, glw::GLboolean param2, const glw::GLuint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribP4uiv(" << param0 << ", " << toHex(param1) << ", " << getBooleanStr(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribP4uiv(param0, param1, param2, param3);
}

void CallLogWrapper::glBlendEquationi (glw::GLuint param0, glw::GLenum param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBlendEquationi(" << param0 << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.blendEquationi(param0, param1);
}

void CallLogWrapper::glBlendEquationSeparatei (glw::GLuint param0, glw::GLenum param1, glw::GLenum param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBlendEquationSeparatei(" << param0 << ", " << toHex(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.blendEquationSeparatei(param0, param1, param2);
}

void CallLogWrapper::glBlendFunci (glw::GLuint param0, glw::GLenum param1, glw::GLenum param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBlendFunci(" << param0 << ", " << toHex(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.blendFunci(param0, param1, param2);
}

void CallLogWrapper::glBlendFuncSeparatei (glw::GLuint param0, glw::GLenum param1, glw::GLenum param2, glw::GLenum param3, glw::GLenum param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBlendFuncSeparatei(" << param0 << ", " << toHex(param1) << ", " << toHex(param2) << ", " << toHex(param3) << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.blendFuncSeparatei(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glDrawArraysIndirect (glw::GLenum param0, const glw::GLvoid* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawArraysIndirect(" << getPrimitiveTypeStr(param0) << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.drawArraysIndirect(param0, param1);
}

void CallLogWrapper::glDrawElementsIndirect (glw::GLenum param0, glw::GLenum param1, const glw::GLvoid* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawElementsIndirect(" << getPrimitiveTypeStr(param0) << ", " << getTypeStr(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.drawElementsIndirect(param0, param1, param2);
}

void CallLogWrapper::glUniform1d (glw::GLint param0, glw::GLdouble param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform1d(" << param0 << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.uniform1d(param0, param1);
}

void CallLogWrapper::glUniform2d (glw::GLint param0, glw::GLdouble param1, glw::GLdouble param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform2d(" << param0 << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.uniform2d(param0, param1, param2);
}

void CallLogWrapper::glUniform3d (glw::GLint param0, glw::GLdouble param1, glw::GLdouble param2, glw::GLdouble param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform3d(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.uniform3d(param0, param1, param2, param3);
}

void CallLogWrapper::glUniform4d (glw::GLint param0, glw::GLdouble param1, glw::GLdouble param2, glw::GLdouble param3, glw::GLdouble param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform4d(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.uniform4d(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glUniform1dv (glw::GLint param0, glw::GLsizei param1, const glw::GLdouble* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform1dv(" << param0 << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.uniform1dv(param0, param1, param2);
}

void CallLogWrapper::glUniform2dv (glw::GLint param0, glw::GLsizei param1, const glw::GLdouble* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform2dv(" << param0 << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.uniform2dv(param0, param1, param2);
}

void CallLogWrapper::glUniform3dv (glw::GLint param0, glw::GLsizei param1, const glw::GLdouble* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform3dv(" << param0 << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.uniform3dv(param0, param1, param2);
}

void CallLogWrapper::glUniform4dv (glw::GLint param0, glw::GLsizei param1, const glw::GLdouble* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniform4dv(" << param0 << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.uniform4dv(param0, param1, param2);
}

void CallLogWrapper::glUniformMatrix2dv (glw::GLint param0, glw::GLsizei param1, glw::GLboolean param2, const glw::GLdouble* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix2dv(" << param0 << ", " << param1 << ", " << getBooleanStr(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix2dv(param0, param1, param2, param3);
}

void CallLogWrapper::glUniformMatrix3dv (glw::GLint param0, glw::GLsizei param1, glw::GLboolean param2, const glw::GLdouble* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix3dv(" << param0 << ", " << param1 << ", " << getBooleanStr(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix3dv(param0, param1, param2, param3);
}

void CallLogWrapper::glUniformMatrix4dv (glw::GLint param0, glw::GLsizei param1, glw::GLboolean param2, const glw::GLdouble* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix4dv(" << param0 << ", " << param1 << ", " << getBooleanStr(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix4dv(param0, param1, param2, param3);
}

void CallLogWrapper::glUniformMatrix2x3dv (glw::GLint param0, glw::GLsizei param1, glw::GLboolean param2, const glw::GLdouble* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix2x3dv(" << param0 << ", " << param1 << ", " << getBooleanStr(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix2x3dv(param0, param1, param2, param3);
}

void CallLogWrapper::glUniformMatrix2x4dv (glw::GLint param0, glw::GLsizei param1, glw::GLboolean param2, const glw::GLdouble* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix2x4dv(" << param0 << ", " << param1 << ", " << getBooleanStr(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix2x4dv(param0, param1, param2, param3);
}

void CallLogWrapper::glUniformMatrix3x2dv (glw::GLint param0, glw::GLsizei param1, glw::GLboolean param2, const glw::GLdouble* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix3x2dv(" << param0 << ", " << param1 << ", " << getBooleanStr(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix3x2dv(param0, param1, param2, param3);
}

void CallLogWrapper::glUniformMatrix3x4dv (glw::GLint param0, glw::GLsizei param1, glw::GLboolean param2, const glw::GLdouble* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix3x4dv(" << param0 << ", " << param1 << ", " << getBooleanStr(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix3x4dv(param0, param1, param2, param3);
}

void CallLogWrapper::glUniformMatrix4x2dv (glw::GLint param0, glw::GLsizei param1, glw::GLboolean param2, const glw::GLdouble* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix4x2dv(" << param0 << ", " << param1 << ", " << getBooleanStr(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix4x2dv(param0, param1, param2, param3);
}

void CallLogWrapper::glUniformMatrix4x3dv (glw::GLint param0, glw::GLsizei param1, glw::GLboolean param2, const glw::GLdouble* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformMatrix4x3dv(" << param0 << ", " << param1 << ", " << getBooleanStr(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.uniformMatrix4x3dv(param0, param1, param2, param3);
}

void CallLogWrapper::glGetUniformdv (glw::GLuint param0, glw::GLint param1, glw::GLdouble* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetUniformdv(" << param0 << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getUniformdv(param0, param1, param2);
}

void CallLogWrapper::glMinSampleShading (glw::GLfloat param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMinSampleShading(" << param0 << ");" << TestLog::EndMessage;
	m_gl.minSampleShading(param0);
}

glw::GLint CallLogWrapper::glGetSubroutineUniformLocation (glw::GLuint param0, glw::GLenum param1, const glw::GLchar* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetSubroutineUniformLocation(" << param0 << ", " << toHex(param1) << ", " << getStringStr(param2) << ");" << TestLog::EndMessage;
	glw::GLint returnValue = m_gl.getSubroutineUniformLocation(param0, param1, param2);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLuint CallLogWrapper::glGetSubroutineIndex (glw::GLuint param0, glw::GLenum param1, const glw::GLchar* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetSubroutineIndex(" << param0 << ", " << toHex(param1) << ", " << getStringStr(param2) << ");" << TestLog::EndMessage;
	glw::GLuint returnValue = m_gl.getSubroutineIndex(param0, param1, param2);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glGetActiveSubroutineUniformiv (glw::GLuint param0, glw::GLenum param1, glw::GLuint param2, glw::GLenum param3, glw::GLint* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetActiveSubroutineUniformiv(" << param0 << ", " << toHex(param1) << ", " << param2 << ", " << toHex(param3) << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.getActiveSubroutineUniformiv(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glGetActiveSubroutineUniformName (glw::GLuint param0, glw::GLenum param1, glw::GLuint param2, glw::GLsizei param3, glw::GLsizei* param4, glw::GLchar* param5)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetActiveSubroutineUniformName(" << param0 << ", " << toHex(param1) << ", " << param2 << ", " << param3 << ", " << toHex(param4) << ", " << toHex(param5) << ");" << TestLog::EndMessage;
	m_gl.getActiveSubroutineUniformName(param0, param1, param2, param3, param4, param5);
}

void CallLogWrapper::glGetActiveSubroutineName (glw::GLuint param0, glw::GLenum param1, glw::GLuint param2, glw::GLsizei param3, glw::GLsizei* param4, glw::GLchar* param5)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetActiveSubroutineName(" << param0 << ", " << toHex(param1) << ", " << param2 << ", " << param3 << ", " << toHex(param4) << ", " << toHex(param5) << ");" << TestLog::EndMessage;
	m_gl.getActiveSubroutineName(param0, param1, param2, param3, param4, param5);
}

void CallLogWrapper::glUniformSubroutinesuiv (glw::GLenum param0, glw::GLsizei param1, const glw::GLuint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUniformSubroutinesuiv(" << toHex(param0) << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.uniformSubroutinesuiv(param0, param1, param2);
}

void CallLogWrapper::glGetUniformSubroutineuiv (glw::GLenum param0, glw::GLint param1, glw::GLuint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetUniformSubroutineuiv(" << toHex(param0) << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getUniformSubroutineuiv(param0, param1, param2);
}

void CallLogWrapper::glGetProgramStageiv (glw::GLuint param0, glw::GLenum param1, glw::GLenum param2, glw::GLint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetProgramStageiv(" << param0 << ", " << toHex(param1) << ", " << toHex(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.getProgramStageiv(param0, param1, param2, param3);
}

void CallLogWrapper::glPatchParameteri (glw::GLenum param0, glw::GLint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPatchParameteri(" << toHex(param0) << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.patchParameteri(param0, param1);
}

void CallLogWrapper::glPatchParameterfv (glw::GLenum param0, const glw::GLfloat* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPatchParameterfv(" << toHex(param0) << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.patchParameterfv(param0, param1);
}

void CallLogWrapper::glBindTransformFeedback (glw::GLenum param0, glw::GLuint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindTransformFeedback(" << getTransformFeedbackTargetStr(param0) << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.bindTransformFeedback(param0, param1);
}

void CallLogWrapper::glDeleteTransformFeedbacks (glw::GLsizei param0, const glw::GLuint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDeleteTransformFeedbacks(" << param0 << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.deleteTransformFeedbacks(param0, param1);
}

void CallLogWrapper::glGenTransformFeedbacks (glw::GLsizei param0, glw::GLuint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGenTransformFeedbacks(" << param0 << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.genTransformFeedbacks(param0, param1);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 1 = " << getPointerStr(param1, param0) << TestLog::EndMessage;
	}
}

glw::GLboolean CallLogWrapper::glIsTransformFeedback (glw::GLuint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsTransformFeedback(" << param0 << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isTransformFeedback(param0);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glPauseTransformFeedback ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPauseTransformFeedback(" << ");" << TestLog::EndMessage;
	m_gl.pauseTransformFeedback();
}

void CallLogWrapper::glResumeTransformFeedback ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "glResumeTransformFeedback(" << ");" << TestLog::EndMessage;
	m_gl.resumeTransformFeedback();
}

void CallLogWrapper::glDrawTransformFeedback (glw::GLenum param0, glw::GLuint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawTransformFeedback(" << toHex(param0) << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.drawTransformFeedback(param0, param1);
}

void CallLogWrapper::glDrawTransformFeedbackStream (glw::GLenum param0, glw::GLuint param1, glw::GLuint param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawTransformFeedbackStream(" << toHex(param0) << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.drawTransformFeedbackStream(param0, param1, param2);
}

void CallLogWrapper::glBeginQueryIndexed (glw::GLenum param0, glw::GLuint param1, glw::GLuint param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBeginQueryIndexed(" << toHex(param0) << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.beginQueryIndexed(param0, param1, param2);
}

void CallLogWrapper::glEndQueryIndexed (glw::GLenum param0, glw::GLuint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glEndQueryIndexed(" << toHex(param0) << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.endQueryIndexed(param0, param1);
}

void CallLogWrapper::glGetQueryIndexediv (glw::GLenum param0, glw::GLuint param1, glw::GLenum param2, glw::GLint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetQueryIndexediv(" << toHex(param0) << ", " << param1 << ", " << toHex(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.getQueryIndexediv(param0, param1, param2, param3);
}

void CallLogWrapper::glReleaseShaderCompiler ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "glReleaseShaderCompiler(" << ");" << TestLog::EndMessage;
	m_gl.releaseShaderCompiler();
}

void CallLogWrapper::glShaderBinary (glw::GLsizei param0, const glw::GLuint* param1, glw::GLenum param2, const glw::GLvoid* param3, glw::GLsizei param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glShaderBinary(" << param0 << ", " << toHex(param1) << ", " << toHex(param2) << ", " << toHex(param3) << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.shaderBinary(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glGetShaderPrecisionFormat (glw::GLenum param0, glw::GLenum param1, glw::GLint* param2, glw::GLint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetShaderPrecisionFormat(" << getShaderTypeStr(param0) << ", " << getPrecisionFormatTypeStr(param1) << ", " << toHex(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.getShaderPrecisionFormat(param0, param1, param2, param3);
}

void CallLogWrapper::glDepthRangef (glw::GLfloat param0, glw::GLfloat param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDepthRangef(" << param0 << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.depthRangef(param0, param1);
}

void CallLogWrapper::glClearDepthf (glw::GLfloat param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearDepthf(" << param0 << ");" << TestLog::EndMessage;
	m_gl.clearDepthf(param0);
}

void CallLogWrapper::glGetProgramBinary (glw::GLuint param0, glw::GLsizei param1, glw::GLsizei* param2, glw::GLenum* param3, glw::GLvoid* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetProgramBinary(" << param0 << ", " << param1 << ", " << toHex(param2) << ", " << toHex(param3) << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.getProgramBinary(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glProgramBinary (glw::GLuint param0, glw::GLenum param1, const glw::GLvoid* param2, glw::GLsizei param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramBinary(" << param0 << ", " << toHex(param1) << ", " << toHex(param2) << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.programBinary(param0, param1, param2, param3);
}

void CallLogWrapper::glProgramParameteri (glw::GLuint param0, glw::GLenum param1, glw::GLint param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramParameteri(" << param0 << ", " << toHex(param1) << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.programParameteri(param0, param1, param2);
}

void CallLogWrapper::glUseProgramStages (glw::GLuint param0, glw::GLbitfield param1, glw::GLuint param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glUseProgramStages(" << param0 << ", " << toHex(param1) << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.useProgramStages(param0, param1, param2);
}

void CallLogWrapper::glActiveShaderProgram (glw::GLuint param0, glw::GLuint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glActiveShaderProgram(" << param0 << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.activeShaderProgram(param0, param1);
}

glw::GLuint CallLogWrapper::glCreateShaderProgramv (glw::GLenum param0, glw::GLsizei param1, const glw::GLchar* const* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCreateShaderProgramv(" << toHex(param0) << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	glw::GLuint returnValue = m_gl.createShaderProgramv(param0, param1, param2);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glBindProgramPipeline (glw::GLuint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindProgramPipeline(" << param0 << ");" << TestLog::EndMessage;
	m_gl.bindProgramPipeline(param0);
}

void CallLogWrapper::glDeleteProgramPipelines (glw::GLsizei param0, const glw::GLuint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDeleteProgramPipelines(" << param0 << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.deleteProgramPipelines(param0, param1);
}

void CallLogWrapper::glGenProgramPipelines (glw::GLsizei param0, glw::GLuint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGenProgramPipelines(" << param0 << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.genProgramPipelines(param0, param1);
}

glw::GLboolean CallLogWrapper::glIsProgramPipeline (glw::GLuint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glIsProgramPipeline(" << param0 << ");" << TestLog::EndMessage;
	glw::GLboolean returnValue = m_gl.isProgramPipeline(param0);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glGetProgramPipelineiv (glw::GLuint param0, glw::GLenum param1, glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetProgramPipelineiv(" << param0 << ", " << getPipelineParamStr(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getProgramPipelineiv(param0, param1, param2);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 2 = " << getPointerStr(param2, 1) << TestLog::EndMessage;
	}
}

void CallLogWrapper::glProgramUniform1i (glw::GLuint param0, glw::GLint param1, glw::GLint param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform1i(" << param0 << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.programUniform1i(param0, param1, param2);
}

void CallLogWrapper::glProgramUniform1iv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, const glw::GLint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform1iv(" << param0 << ", " << param1 << ", " << param2 << ", " << getPointerStr(param3, (param2 * 1)) << ");" << TestLog::EndMessage;
	m_gl.programUniform1iv(param0, param1, param2, param3);
}

void CallLogWrapper::glProgramUniform1f (glw::GLuint param0, glw::GLint param1, glw::GLfloat param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform1f(" << param0 << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.programUniform1f(param0, param1, param2);
}

void CallLogWrapper::glProgramUniform1fv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, const glw::GLfloat* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform1fv(" << param0 << ", " << param1 << ", " << param2 << ", " << getPointerStr(param3, (param2 * 1)) << ");" << TestLog::EndMessage;
	m_gl.programUniform1fv(param0, param1, param2, param3);
}

void CallLogWrapper::glProgramUniform1d (glw::GLuint param0, glw::GLint param1, glw::GLdouble param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform1d(" << param0 << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.programUniform1d(param0, param1, param2);
}

void CallLogWrapper::glProgramUniform1dv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, const glw::GLdouble* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform1dv(" << param0 << ", " << param1 << ", " << param2 << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.programUniform1dv(param0, param1, param2, param3);
}

void CallLogWrapper::glProgramUniform1ui (glw::GLuint param0, glw::GLint param1, glw::GLuint param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform1ui(" << param0 << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.programUniform1ui(param0, param1, param2);
}

void CallLogWrapper::glProgramUniform1uiv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, const glw::GLuint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform1uiv(" << param0 << ", " << param1 << ", " << param2 << ", " << getPointerStr(param3, (param2 * 1)) << ");" << TestLog::EndMessage;
	m_gl.programUniform1uiv(param0, param1, param2, param3);
}

void CallLogWrapper::glProgramUniform2i (glw::GLuint param0, glw::GLint param1, glw::GLint param2, glw::GLint param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform2i(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.programUniform2i(param0, param1, param2, param3);
}

void CallLogWrapper::glProgramUniform2iv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, const glw::GLint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform2iv(" << param0 << ", " << param1 << ", " << param2 << ", " << getPointerStr(param3, (param2 * 2)) << ");" << TestLog::EndMessage;
	m_gl.programUniform2iv(param0, param1, param2, param3);
}

void CallLogWrapper::glProgramUniform2f (glw::GLuint param0, glw::GLint param1, glw::GLfloat param2, glw::GLfloat param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform2f(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.programUniform2f(param0, param1, param2, param3);
}

void CallLogWrapper::glProgramUniform2fv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, const glw::GLfloat* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform2fv(" << param0 << ", " << param1 << ", " << param2 << ", " << getPointerStr(param3, (param2 * 2)) << ");" << TestLog::EndMessage;
	m_gl.programUniform2fv(param0, param1, param2, param3);
}

void CallLogWrapper::glProgramUniform2d (glw::GLuint param0, glw::GLint param1, glw::GLdouble param2, glw::GLdouble param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform2d(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.programUniform2d(param0, param1, param2, param3);
}

void CallLogWrapper::glProgramUniform2dv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, const glw::GLdouble* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform2dv(" << param0 << ", " << param1 << ", " << param2 << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.programUniform2dv(param0, param1, param2, param3);
}

void CallLogWrapper::glProgramUniform2ui (glw::GLuint param0, glw::GLint param1, glw::GLuint param2, glw::GLuint param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform2ui(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.programUniform2ui(param0, param1, param2, param3);
}

void CallLogWrapper::glProgramUniform2uiv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, const glw::GLuint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform2uiv(" << param0 << ", " << param1 << ", " << param2 << ", " << getPointerStr(param3, (param2 * 2)) << ");" << TestLog::EndMessage;
	m_gl.programUniform2uiv(param0, param1, param2, param3);
}

void CallLogWrapper::glProgramUniform3i (glw::GLuint param0, glw::GLint param1, glw::GLint param2, glw::GLint param3, glw::GLint param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform3i(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.programUniform3i(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glProgramUniform3iv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, const glw::GLint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform3iv(" << param0 << ", " << param1 << ", " << param2 << ", " << getPointerStr(param3, (param2 * 3)) << ");" << TestLog::EndMessage;
	m_gl.programUniform3iv(param0, param1, param2, param3);
}

void CallLogWrapper::glProgramUniform3f (glw::GLuint param0, glw::GLint param1, glw::GLfloat param2, glw::GLfloat param3, glw::GLfloat param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform3f(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.programUniform3f(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glProgramUniform3fv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, const glw::GLfloat* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform3fv(" << param0 << ", " << param1 << ", " << param2 << ", " << getPointerStr(param3, (param2 * 3)) << ");" << TestLog::EndMessage;
	m_gl.programUniform3fv(param0, param1, param2, param3);
}

void CallLogWrapper::glProgramUniform3d (glw::GLuint param0, glw::GLint param1, glw::GLdouble param2, glw::GLdouble param3, glw::GLdouble param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform3d(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.programUniform3d(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glProgramUniform3dv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, const glw::GLdouble* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform3dv(" << param0 << ", " << param1 << ", " << param2 << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.programUniform3dv(param0, param1, param2, param3);
}

void CallLogWrapper::glProgramUniform3ui (glw::GLuint param0, glw::GLint param1, glw::GLuint param2, glw::GLuint param3, glw::GLuint param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform3ui(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.programUniform3ui(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glProgramUniform3uiv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, const glw::GLuint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform3uiv(" << param0 << ", " << param1 << ", " << param2 << ", " << getPointerStr(param3, (param2 * 3)) << ");" << TestLog::EndMessage;
	m_gl.programUniform3uiv(param0, param1, param2, param3);
}

void CallLogWrapper::glProgramUniform4i (glw::GLuint param0, glw::GLint param1, glw::GLint param2, glw::GLint param3, glw::GLint param4, glw::GLint param5)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform4i(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ", " << param5 << ");" << TestLog::EndMessage;
	m_gl.programUniform4i(param0, param1, param2, param3, param4, param5);
}

void CallLogWrapper::glProgramUniform4iv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, const glw::GLint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform4iv(" << param0 << ", " << param1 << ", " << param2 << ", " << getPointerStr(param3, (param2 * 4)) << ");" << TestLog::EndMessage;
	m_gl.programUniform4iv(param0, param1, param2, param3);
}

void CallLogWrapper::glProgramUniform4f (glw::GLuint param0, glw::GLint param1, glw::GLfloat param2, glw::GLfloat param3, glw::GLfloat param4, glw::GLfloat param5)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform4f(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ", " << param5 << ");" << TestLog::EndMessage;
	m_gl.programUniform4f(param0, param1, param2, param3, param4, param5);
}

void CallLogWrapper::glProgramUniform4fv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, const glw::GLfloat* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform4fv(" << param0 << ", " << param1 << ", " << param2 << ", " << getPointerStr(param3, (param2 * 4)) << ");" << TestLog::EndMessage;
	m_gl.programUniform4fv(param0, param1, param2, param3);
}

void CallLogWrapper::glProgramUniform4d (glw::GLuint param0, glw::GLint param1, glw::GLdouble param2, glw::GLdouble param3, glw::GLdouble param4, glw::GLdouble param5)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform4d(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ", " << param5 << ");" << TestLog::EndMessage;
	m_gl.programUniform4d(param0, param1, param2, param3, param4, param5);
}

void CallLogWrapper::glProgramUniform4dv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, const glw::GLdouble* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform4dv(" << param0 << ", " << param1 << ", " << param2 << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.programUniform4dv(param0, param1, param2, param3);
}

void CallLogWrapper::glProgramUniform4ui (glw::GLuint param0, glw::GLint param1, glw::GLuint param2, glw::GLuint param3, glw::GLuint param4, glw::GLuint param5)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform4ui(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ", " << param5 << ");" << TestLog::EndMessage;
	m_gl.programUniform4ui(param0, param1, param2, param3, param4, param5);
}

void CallLogWrapper::glProgramUniform4uiv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, const glw::GLuint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniform4uiv(" << param0 << ", " << param1 << ", " << param2 << ", " << getPointerStr(param3, (param2 * 4)) << ");" << TestLog::EndMessage;
	m_gl.programUniform4uiv(param0, param1, param2, param3);
}

void CallLogWrapper::glProgramUniformMatrix2fv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, glw::GLboolean param3, const glw::GLfloat* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix2fv(" << param0 << ", " << param1 << ", " << param2 << ", " << getBooleanStr(param3) << ", " << getPointerStr(param4, (param2 * 2*2)) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix2fv(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glProgramUniformMatrix3fv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, glw::GLboolean param3, const glw::GLfloat* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix3fv(" << param0 << ", " << param1 << ", " << param2 << ", " << getBooleanStr(param3) << ", " << getPointerStr(param4, (param2 * 3*3)) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix3fv(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glProgramUniformMatrix4fv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, glw::GLboolean param3, const glw::GLfloat* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix4fv(" << param0 << ", " << param1 << ", " << param2 << ", " << getBooleanStr(param3) << ", " << getPointerStr(param4, (param2 * 4*4)) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix4fv(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glProgramUniformMatrix2dv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, glw::GLboolean param3, const glw::GLdouble* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix2dv(" << param0 << ", " << param1 << ", " << param2 << ", " << getBooleanStr(param3) << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix2dv(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glProgramUniformMatrix3dv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, glw::GLboolean param3, const glw::GLdouble* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix3dv(" << param0 << ", " << param1 << ", " << param2 << ", " << getBooleanStr(param3) << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix3dv(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glProgramUniformMatrix4dv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, glw::GLboolean param3, const glw::GLdouble* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix4dv(" << param0 << ", " << param1 << ", " << param2 << ", " << getBooleanStr(param3) << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix4dv(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glProgramUniformMatrix2x3fv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, glw::GLboolean param3, const glw::GLfloat* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix2x3fv(" << param0 << ", " << param1 << ", " << param2 << ", " << getBooleanStr(param3) << ", " << getPointerStr(param4, (param2 * 2*3)) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix2x3fv(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glProgramUniformMatrix3x2fv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, glw::GLboolean param3, const glw::GLfloat* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix3x2fv(" << param0 << ", " << param1 << ", " << param2 << ", " << getBooleanStr(param3) << ", " << getPointerStr(param4, (param2 * 3*2)) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix3x2fv(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glProgramUniformMatrix2x4fv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, glw::GLboolean param3, const glw::GLfloat* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix2x4fv(" << param0 << ", " << param1 << ", " << param2 << ", " << getBooleanStr(param3) << ", " << getPointerStr(param4, (param2 * 2*4)) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix2x4fv(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glProgramUniformMatrix4x2fv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, glw::GLboolean param3, const glw::GLfloat* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix4x2fv(" << param0 << ", " << param1 << ", " << param2 << ", " << getBooleanStr(param3) << ", " << getPointerStr(param4, (param2 * 4*2)) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix4x2fv(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glProgramUniformMatrix3x4fv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, glw::GLboolean param3, const glw::GLfloat* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix3x4fv(" << param0 << ", " << param1 << ", " << param2 << ", " << getBooleanStr(param3) << ", " << getPointerStr(param4, (param2 * 3*4)) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix3x4fv(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glProgramUniformMatrix4x3fv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, glw::GLboolean param3, const glw::GLfloat* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix4x3fv(" << param0 << ", " << param1 << ", " << param2 << ", " << getBooleanStr(param3) << ", " << getPointerStr(param4, (param2 * 4*3)) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix4x3fv(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glProgramUniformMatrix2x3dv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, glw::GLboolean param3, const glw::GLdouble* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix2x3dv(" << param0 << ", " << param1 << ", " << param2 << ", " << getBooleanStr(param3) << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix2x3dv(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glProgramUniformMatrix3x2dv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, glw::GLboolean param3, const glw::GLdouble* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix3x2dv(" << param0 << ", " << param1 << ", " << param2 << ", " << getBooleanStr(param3) << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix3x2dv(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glProgramUniformMatrix2x4dv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, glw::GLboolean param3, const glw::GLdouble* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix2x4dv(" << param0 << ", " << param1 << ", " << param2 << ", " << getBooleanStr(param3) << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix2x4dv(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glProgramUniformMatrix4x2dv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, glw::GLboolean param3, const glw::GLdouble* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix4x2dv(" << param0 << ", " << param1 << ", " << param2 << ", " << getBooleanStr(param3) << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix4x2dv(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glProgramUniformMatrix3x4dv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, glw::GLboolean param3, const glw::GLdouble* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix3x4dv(" << param0 << ", " << param1 << ", " << param2 << ", " << getBooleanStr(param3) << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix3x4dv(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glProgramUniformMatrix4x3dv (glw::GLuint param0, glw::GLint param1, glw::GLsizei param2, glw::GLboolean param3, const glw::GLdouble* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glProgramUniformMatrix4x3dv(" << param0 << ", " << param1 << ", " << param2 << ", " << getBooleanStr(param3) << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.programUniformMatrix4x3dv(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glValidateProgramPipeline (glw::GLuint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glValidateProgramPipeline(" << param0 << ");" << TestLog::EndMessage;
	m_gl.validateProgramPipeline(param0);
}

void CallLogWrapper::glGetProgramPipelineInfoLog (glw::GLuint param0, glw::GLsizei param1, glw::GLsizei* param2, glw::GLchar* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetProgramPipelineInfoLog(" << param0 << ", " << param1 << ", " << toHex(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.getProgramPipelineInfoLog(param0, param1, param2, param3);
}

void CallLogWrapper::glVertexAttribL1d (glw::GLuint param0, glw::GLdouble param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribL1d(" << param0 << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.vertexAttribL1d(param0, param1);
}

void CallLogWrapper::glVertexAttribL2d (glw::GLuint param0, glw::GLdouble param1, glw::GLdouble param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribL2d(" << param0 << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.vertexAttribL2d(param0, param1, param2);
}

void CallLogWrapper::glVertexAttribL3d (glw::GLuint param0, glw::GLdouble param1, glw::GLdouble param2, glw::GLdouble param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribL3d(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.vertexAttribL3d(param0, param1, param2, param3);
}

void CallLogWrapper::glVertexAttribL4d (glw::GLuint param0, glw::GLdouble param1, glw::GLdouble param2, glw::GLdouble param3, glw::GLdouble param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribL4d(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.vertexAttribL4d(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glVertexAttribL1dv (glw::GLuint param0, const glw::GLdouble* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribL1dv(" << param0 << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribL1dv(param0, param1);
}

void CallLogWrapper::glVertexAttribL2dv (glw::GLuint param0, const glw::GLdouble* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribL2dv(" << param0 << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribL2dv(param0, param1);
}

void CallLogWrapper::glVertexAttribL3dv (glw::GLuint param0, const glw::GLdouble* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribL3dv(" << param0 << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribL3dv(param0, param1);
}

void CallLogWrapper::glVertexAttribL4dv (glw::GLuint param0, const glw::GLdouble* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribL4dv(" << param0 << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribL4dv(param0, param1);
}

void CallLogWrapper::glVertexAttribLPointer (glw::GLuint param0, glw::GLint param1, glw::GLenum param2, glw::GLsizei param3, const glw::GLvoid* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribLPointer(" << param0 << ", " << param1 << ", " << toHex(param2) << ", " << param3 << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.vertexAttribLPointer(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glGetVertexAttribLdv (glw::GLuint param0, glw::GLenum param1, glw::GLdouble* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetVertexAttribLdv(" << param0 << ", " << toHex(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getVertexAttribLdv(param0, param1, param2);
}

void CallLogWrapper::glViewportArrayv (glw::GLuint param0, glw::GLsizei param1, const glw::GLfloat* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glViewportArrayv(" << param0 << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.viewportArrayv(param0, param1, param2);
}

void CallLogWrapper::glViewportIndexedf (glw::GLuint param0, glw::GLfloat param1, glw::GLfloat param2, glw::GLfloat param3, glw::GLfloat param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glViewportIndexedf(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.viewportIndexedf(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glViewportIndexedfv (glw::GLuint param0, const glw::GLfloat* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glViewportIndexedfv(" << param0 << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.viewportIndexedfv(param0, param1);
}

void CallLogWrapper::glScissorArrayv (glw::GLuint param0, glw::GLsizei param1, const glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glScissorArrayv(" << param0 << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.scissorArrayv(param0, param1, param2);
}

void CallLogWrapper::glScissorIndexed (glw::GLuint param0, glw::GLint param1, glw::GLint param2, glw::GLsizei param3, glw::GLsizei param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glScissorIndexed(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.scissorIndexed(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glScissorIndexedv (glw::GLuint param0, const glw::GLint* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glScissorIndexedv(" << param0 << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.scissorIndexedv(param0, param1);
}

void CallLogWrapper::glDepthRangeArrayv (glw::GLuint param0, glw::GLsizei param1, const glw::GLdouble* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDepthRangeArrayv(" << param0 << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.depthRangeArrayv(param0, param1, param2);
}

void CallLogWrapper::glDepthRangeIndexed (glw::GLuint param0, glw::GLdouble param1, glw::GLdouble param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDepthRangeIndexed(" << param0 << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.depthRangeIndexed(param0, param1, param2);
}

void CallLogWrapper::glGetFloati_v (glw::GLenum param0, glw::GLuint param1, glw::GLfloat* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetFloati_v(" << toHex(param0) << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getFloati_v(param0, param1, param2);
}

void CallLogWrapper::glGetDoublei_v (glw::GLenum param0, glw::GLuint param1, glw::GLdouble* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetDoublei_v(" << toHex(param0) << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getDoublei_v(param0, param1, param2);
}

void CallLogWrapper::glDrawArraysInstancedBaseInstance (glw::GLenum param0, glw::GLint param1, glw::GLsizei param2, glw::GLsizei param3, glw::GLuint param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawArraysInstancedBaseInstance(" << toHex(param0) << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.drawArraysInstancedBaseInstance(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glDrawElementsInstancedBaseInstance (glw::GLenum param0, glw::GLsizei param1, glw::GLenum param2, const void* param3, glw::GLsizei param4, glw::GLuint param5)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawElementsInstancedBaseInstance(" << toHex(param0) << ", " << param1 << ", " << toHex(param2) << ", " << toHex(param3) << ", " << param4 << ", " << param5 << ");" << TestLog::EndMessage;
	m_gl.drawElementsInstancedBaseInstance(param0, param1, param2, param3, param4, param5);
}

void CallLogWrapper::glDrawElementsInstancedBaseVertexBaseInstance (glw::GLenum param0, glw::GLsizei param1, glw::GLenum param2, const void* param3, glw::GLsizei param4, glw::GLint param5, glw::GLuint param6)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawElementsInstancedBaseVertexBaseInstance(" << toHex(param0) << ", " << param1 << ", " << toHex(param2) << ", " << toHex(param3) << ", " << param4 << ", " << param5 << ", " << param6 << ");" << TestLog::EndMessage;
	m_gl.drawElementsInstancedBaseVertexBaseInstance(param0, param1, param2, param3, param4, param5, param6);
}

void CallLogWrapper::glDrawTransformFeedbackInstanced (glw::GLenum param0, glw::GLuint param1, glw::GLsizei param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawTransformFeedbackInstanced(" << toHex(param0) << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.drawTransformFeedbackInstanced(param0, param1, param2);
}

void CallLogWrapper::glDrawTransformFeedbackStreamInstanced (glw::GLenum param0, glw::GLuint param1, glw::GLuint param2, glw::GLsizei param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDrawTransformFeedbackStreamInstanced(" << toHex(param0) << ", " << param1 << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.drawTransformFeedbackStreamInstanced(param0, param1, param2, param3);
}

void CallLogWrapper::glGetInternalformativ (glw::GLenum param0, glw::GLenum param1, glw::GLenum param2, glw::GLsizei param3, glw::GLint* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetInternalformativ(" << getInternalFormatTargetStr(param0) << ", " << getPixelFormatStr(param1) << ", " << getInternalFormatParameterStr(param2) << ", " << param3 << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.getInternalformativ(param0, param1, param2, param3, param4);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 4 = " << getPointerStr(param4, param3) << TestLog::EndMessage;
	}
}

void CallLogWrapper::glGetActiveAtomicCounterBufferiv (glw::GLuint param0, glw::GLuint param1, glw::GLenum param2, glw::GLint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetActiveAtomicCounterBufferiv(" << param0 << ", " << param1 << ", " << toHex(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.getActiveAtomicCounterBufferiv(param0, param1, param2, param3);
}

void CallLogWrapper::glBindImageTexture (glw::GLuint param0, glw::GLuint param1, glw::GLint param2, glw::GLboolean param3, glw::GLint param4, glw::GLenum param5, glw::GLenum param6)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindImageTexture(" << param0 << ", " << param1 << ", " << param2 << ", " << getBooleanStr(param3) << ", " << param4 << ", " << getImageAccessStr(param5) << ", " << getPixelFormatStr(param6) << ");" << TestLog::EndMessage;
	m_gl.bindImageTexture(param0, param1, param2, param3, param4, param5, param6);
}

void CallLogWrapper::glMemoryBarrier (glw::GLbitfield param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMemoryBarrier(" << getMemoryBarrierFlagsStr(param0) << ");" << TestLog::EndMessage;
	m_gl.memoryBarrier(param0);
}

void CallLogWrapper::glTexStorage1D (glw::GLenum param0, glw::GLsizei param1, glw::GLenum param2, glw::GLsizei param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexStorage1D(" << toHex(param0) << ", " << param1 << ", " << toHex(param2) << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.texStorage1D(param0, param1, param2, param3);
}

void CallLogWrapper::glTexStorage2D (glw::GLenum param0, glw::GLsizei param1, glw::GLenum param2, glw::GLsizei param3, glw::GLsizei param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexStorage2D(" << getTextureTargetStr(param0) << ", " << param1 << ", " << getPixelFormatStr(param2) << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.texStorage2D(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glTexStorage3D (glw::GLenum param0, glw::GLsizei param1, glw::GLenum param2, glw::GLsizei param3, glw::GLsizei param4, glw::GLsizei param5)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexStorage3D(" << getTextureTargetStr(param0) << ", " << param1 << ", " << getPixelFormatStr(param2) << ", " << param3 << ", " << param4 << ", " << param5 << ");" << TestLog::EndMessage;
	m_gl.texStorage3D(param0, param1, param2, param3, param4, param5);
}

void CallLogWrapper::glClearBufferData (glw::GLenum param0, glw::GLenum param1, glw::GLenum param2, glw::GLenum param3, const void* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearBufferData(" << toHex(param0) << ", " << toHex(param1) << ", " << toHex(param2) << ", " << toHex(param3) << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.clearBufferData(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glClearBufferSubData (glw::GLenum param0, glw::GLenum param1, glw::GLintptr param2, glw::GLsizeiptr param3, glw::GLenum param4, glw::GLenum param5, const void* param6)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearBufferSubData(" << toHex(param0) << ", " << toHex(param1) << ", " << param2 << ", " << param3 << ", " << toHex(param4) << ", " << toHex(param5) << ", " << toHex(param6) << ");" << TestLog::EndMessage;
	m_gl.clearBufferSubData(param0, param1, param2, param3, param4, param5, param6);
}

void CallLogWrapper::glDispatchCompute (glw::GLuint param0, glw::GLuint param1, glw::GLuint param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDispatchCompute(" << param0 << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.dispatchCompute(param0, param1, param2);
}

void CallLogWrapper::glDispatchComputeIndirect (glw::GLintptr param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDispatchComputeIndirect(" << param0 << ");" << TestLog::EndMessage;
	m_gl.dispatchComputeIndirect(param0);
}

void CallLogWrapper::glCopyImageSubData (glw::GLuint param0, glw::GLenum param1, glw::GLint param2, glw::GLint param3, glw::GLint param4, glw::GLint param5, glw::GLuint param6, glw::GLenum param7, glw::GLint param8, glw::GLint param9, glw::GLint param10, glw::GLint param11, glw::GLsizei param12, glw::GLsizei param13, glw::GLsizei param14)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glCopyImageSubData(" << param0 << ", " << toHex(param1) << ", " << param2 << ", " << param3 << ", " << param4 << ", " << param5 << ", " << param6 << ", " << toHex(param7) << ", " << param8 << ", " << param9 << ", " << param10 << ", " << param11 << ", " << param12 << ", " << param13 << ", " << param14 << ");" << TestLog::EndMessage;
	m_gl.copyImageSubData(param0, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14);
}

void CallLogWrapper::glDebugMessageControl (glw::GLenum param0, glw::GLenum param1, glw::GLenum param2, glw::GLsizei param3, const glw::GLuint* param4, glw::GLboolean param5)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDebugMessageControl(" << getDebugMessageSourceStr(param0) << ", " << getDebugMessageTypeStr(param1) << ", " << getDebugMessageSeverityStr(param2) << ", " << param3 << ", " << getPointerStr(param4, (param3)) << ", " << getBooleanStr(param5) << ");" << TestLog::EndMessage;
	m_gl.debugMessageControl(param0, param1, param2, param3, param4, param5);
}

void CallLogWrapper::glDebugMessageInsert (glw::GLenum param0, glw::GLenum param1, glw::GLuint param2, glw::GLenum param3, glw::GLsizei param4, const glw::GLchar* param5)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDebugMessageInsert(" << getDebugMessageSourceStr(param0) << ", " << getDebugMessageTypeStr(param1) << ", " << param2 << ", " << getDebugMessageSeverityStr(param3) << ", " << param4 << ", " << getStringStr(param5) << ");" << TestLog::EndMessage;
	m_gl.debugMessageInsert(param0, param1, param2, param3, param4, param5);
}

void CallLogWrapper::glDebugMessageCallback (glw::GLDEBUGPROC param0, const void* param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glDebugMessageCallback(" << param0 << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	m_gl.debugMessageCallback(param0, param1);
}

glw::GLuint CallLogWrapper::glGetDebugMessageLog (glw::GLuint param0, glw::GLsizei param1, glw::GLenum* param2, glw::GLenum* param3, glw::GLuint* param4, glw::GLenum* param5, glw::GLsizei* param6, glw::GLchar* param7)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetDebugMessageLog(" << param0 << ", " << param1 << ", " << toHex(param2) << ", " << toHex(param3) << ", " << toHex(param4) << ", " << toHex(param5) << ", " << toHex(param6) << ", " << toHex(param7) << ");" << TestLog::EndMessage;
	glw::GLuint returnValue = m_gl.getDebugMessageLog(param0, param1, param2, param3, param4, param5, param6, param7);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glPushDebugGroup (glw::GLenum param0, glw::GLuint param1, glw::GLsizei param2, const glw::GLchar* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPushDebugGroup(" << getDebugMessageSourceStr(param0) << ", " << param1 << ", " << param2 << ", " << getStringStr(param3) << ");" << TestLog::EndMessage;
	m_gl.pushDebugGroup(param0, param1, param2, param3);
}

void CallLogWrapper::glPopDebugGroup ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "glPopDebugGroup(" << ");" << TestLog::EndMessage;
	m_gl.popDebugGroup();
}

void CallLogWrapper::glObjectLabel (glw::GLenum param0, glw::GLuint param1, glw::GLsizei param2, const glw::GLchar* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glObjectLabel(" << toHex(param0) << ", " << param1 << ", " << param2 << ", " << getStringStr(param3) << ");" << TestLog::EndMessage;
	m_gl.objectLabel(param0, param1, param2, param3);
}

void CallLogWrapper::glGetObjectLabel (glw::GLenum param0, glw::GLuint param1, glw::GLsizei param2, glw::GLsizei* param3, glw::GLchar* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetObjectLabel(" << toHex(param0) << ", " << param1 << ", " << param2 << ", " << toHex(param3) << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.getObjectLabel(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glObjectPtrLabel (const void* param0, glw::GLsizei param1, const glw::GLchar* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glObjectPtrLabel(" << toHex(param0) << ", " << param1 << ", " << getStringStr(param2) << ");" << TestLog::EndMessage;
	m_gl.objectPtrLabel(param0, param1, param2);
}

void CallLogWrapper::glGetObjectPtrLabel (const void* param0, glw::GLsizei param1, glw::GLsizei* param2, glw::GLchar* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetObjectPtrLabel(" << toHex(param0) << ", " << param1 << ", " << toHex(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.getObjectPtrLabel(param0, param1, param2, param3);
}

void CallLogWrapper::glFramebufferParameteri (glw::GLenum param0, glw::GLenum param1, glw::GLint param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glFramebufferParameteri(" << getFramebufferTargetStr(param0) << ", " << getFramebufferParameterStr(param1) << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.framebufferParameteri(param0, param1, param2);
}

void CallLogWrapper::glGetFramebufferParameteriv (glw::GLenum param0, glw::GLenum param1, glw::GLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetFramebufferParameteriv(" << getFramebufferTargetStr(param0) << ", " << getFramebufferParameterStr(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.getFramebufferParameteriv(param0, param1, param2);
}

void CallLogWrapper::glGetInternalformati64v (glw::GLenum param0, glw::GLenum param1, glw::GLenum param2, glw::GLsizei param3, glw::GLint64* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetInternalformati64v(" << toHex(param0) << ", " << toHex(param1) << ", " << toHex(param2) << ", " << param3 << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.getInternalformati64v(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glInvalidateTexSubImage (glw::GLuint param0, glw::GLint param1, glw::GLint param2, glw::GLint param3, glw::GLint param4, glw::GLsizei param5, glw::GLsizei param6, glw::GLsizei param7)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glInvalidateTexSubImage(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ", " << param5 << ", " << param6 << ", " << param7 << ");" << TestLog::EndMessage;
	m_gl.invalidateTexSubImage(param0, param1, param2, param3, param4, param5, param6, param7);
}

void CallLogWrapper::glInvalidateTexImage (glw::GLuint param0, glw::GLint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glInvalidateTexImage(" << param0 << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.invalidateTexImage(param0, param1);
}

void CallLogWrapper::glInvalidateBufferSubData (glw::GLuint param0, glw::GLintptr param1, glw::GLsizeiptr param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glInvalidateBufferSubData(" << param0 << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.invalidateBufferSubData(param0, param1, param2);
}

void CallLogWrapper::glInvalidateBufferData (glw::GLuint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glInvalidateBufferData(" << param0 << ");" << TestLog::EndMessage;
	m_gl.invalidateBufferData(param0);
}

void CallLogWrapper::glInvalidateFramebuffer (glw::GLenum param0, glw::GLsizei param1, const glw::GLenum* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glInvalidateFramebuffer(" << getFramebufferTargetStr(param0) << ", " << param1 << ", " << getEnumPointerStr(param2, param1, getInvalidateAttachmentName) << ");" << TestLog::EndMessage;
	m_gl.invalidateFramebuffer(param0, param1, param2);
}

void CallLogWrapper::glInvalidateSubFramebuffer (glw::GLenum param0, glw::GLsizei param1, const glw::GLenum* param2, glw::GLint param3, glw::GLint param4, glw::GLsizei param5, glw::GLsizei param6)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glInvalidateSubFramebuffer(" << getFramebufferTargetStr(param0) << ", " << param1 << ", " << getEnumPointerStr(param2, param1, getInvalidateAttachmentName) << ", " << param3 << ", " << param4 << ", " << param5 << ", " << param6 << ");" << TestLog::EndMessage;
	m_gl.invalidateSubFramebuffer(param0, param1, param2, param3, param4, param5, param6);
}

void CallLogWrapper::glMultiDrawArraysIndirect (glw::GLenum param0, const void* param1, glw::GLsizei param2, glw::GLsizei param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiDrawArraysIndirect(" << toHex(param0) << ", " << toHex(param1) << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.multiDrawArraysIndirect(param0, param1, param2, param3);
}

void CallLogWrapper::glMultiDrawElementsIndirect (glw::GLenum param0, glw::GLenum param1, const void* param2, glw::GLsizei param3, glw::GLsizei param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glMultiDrawElementsIndirect(" << toHex(param0) << ", " << toHex(param1) << ", " << toHex(param2) << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.multiDrawElementsIndirect(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glGetProgramInterfaceiv (glw::GLuint param0, glw::GLenum param1, glw::GLenum param2, glw::GLint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetProgramInterfaceiv(" << param0 << ", " << toHex(param1) << ", " << toHex(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.getProgramInterfaceiv(param0, param1, param2, param3);
}

glw::GLuint CallLogWrapper::glGetProgramResourceIndex (glw::GLuint param0, glw::GLenum param1, const glw::GLchar* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetProgramResourceIndex(" << param0 << ", " << getProgramInterfaceStr(param1) << ", " << getStringStr(param2) << ");" << TestLog::EndMessage;
	glw::GLuint returnValue = m_gl.getProgramResourceIndex(param0, param1, param2);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glGetProgramResourceName (glw::GLuint param0, glw::GLenum param1, glw::GLuint param2, glw::GLsizei param3, glw::GLsizei* param4, glw::GLchar* param5)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetProgramResourceName(" << param0 << ", " << toHex(param1) << ", " << param2 << ", " << param3 << ", " << toHex(param4) << ", " << toHex(param5) << ");" << TestLog::EndMessage;
	m_gl.getProgramResourceName(param0, param1, param2, param3, param4, param5);
}

void CallLogWrapper::glGetProgramResourceiv (glw::GLuint param0, glw::GLenum param1, glw::GLuint param2, glw::GLsizei param3, const glw::GLenum* param4, glw::GLsizei param5, glw::GLsizei* param6, glw::GLint* param7)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetProgramResourceiv(" << param0 << ", " << getProgramInterfaceStr(param1) << ", " << param2 << ", " << param3 << ", " << toHex(param4) << ", " << param5 << ", " << toHex(param6) << ", " << toHex(param7) << ");" << TestLog::EndMessage;
	m_gl.getProgramResourceiv(param0, param1, param2, param3, param4, param5, param6, param7);
}

glw::GLint CallLogWrapper::glGetProgramResourceLocation (glw::GLuint param0, glw::GLenum param1, const glw::GLchar* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetProgramResourceLocation(" << param0 << ", " << toHex(param1) << ", " << getStringStr(param2) << ");" << TestLog::EndMessage;
	glw::GLint returnValue = m_gl.getProgramResourceLocation(param0, param1, param2);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

glw::GLint CallLogWrapper::glGetProgramResourceLocationIndex (glw::GLuint param0, glw::GLenum param1, const glw::GLchar* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glGetProgramResourceLocationIndex(" << param0 << ", " << toHex(param1) << ", " << getStringStr(param2) << ");" << TestLog::EndMessage;
	glw::GLint returnValue = m_gl.getProgramResourceLocationIndex(param0, param1, param2);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

void CallLogWrapper::glShaderStorageBlockBinding (glw::GLuint param0, glw::GLuint param1, glw::GLuint param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glShaderStorageBlockBinding(" << param0 << ", " << param1 << ", " << param2 << ");" << TestLog::EndMessage;
	m_gl.shaderStorageBlockBinding(param0, param1, param2);
}

void CallLogWrapper::glTexBufferRange (glw::GLenum param0, glw::GLenum param1, glw::GLuint param2, glw::GLintptr param3, glw::GLsizeiptr param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexBufferRange(" << getBufferTargetStr(param0) << ", " << getPixelFormatStr(param1) << ", " << param2 << ", " << param3 << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.texBufferRange(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glTexStorage2DMultisample (glw::GLenum param0, glw::GLsizei param1, glw::GLenum param2, glw::GLsizei param3, glw::GLsizei param4, glw::GLboolean param5)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexStorage2DMultisample(" << getTextureTargetStr(param0) << ", " << param1 << ", " << getPixelFormatStr(param2) << ", " << param3 << ", " << param4 << ", " << getBooleanStr(param5) << ");" << TestLog::EndMessage;
	m_gl.texStorage2DMultisample(param0, param1, param2, param3, param4, param5);
}

void CallLogWrapper::glTexStorage3DMultisample (glw::GLenum param0, glw::GLsizei param1, glw::GLenum param2, glw::GLsizei param3, glw::GLsizei param4, glw::GLsizei param5, glw::GLboolean param6)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTexStorage3DMultisample(" << getTextureTargetStr(param0) << ", " << param1 << ", " << getPixelFormatStr(param2) << ", " << param3 << ", " << param4 << ", " << param5 << ", " << getBooleanStr(param6) << ");" << TestLog::EndMessage;
	m_gl.texStorage3DMultisample(param0, param1, param2, param3, param4, param5, param6);
}

void CallLogWrapper::glTextureView (glw::GLuint param0, glw::GLenum param1, glw::GLuint param2, glw::GLenum param3, glw::GLuint param4, glw::GLuint param5, glw::GLuint param6, glw::GLuint param7)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glTextureView(" << param0 << ", " << toHex(param1) << ", " << param2 << ", " << toHex(param3) << ", " << param4 << ", " << param5 << ", " << param6 << ", " << param7 << ");" << TestLog::EndMessage;
	m_gl.textureView(param0, param1, param2, param3, param4, param5, param6, param7);
}

void CallLogWrapper::glBindVertexBuffer (glw::GLuint param0, glw::GLuint param1, glw::GLintptr param2, glw::GLsizei param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindVertexBuffer(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.bindVertexBuffer(param0, param1, param2, param3);
}

void CallLogWrapper::glVertexAttribFormat (glw::GLuint param0, glw::GLint param1, glw::GLenum param2, glw::GLboolean param3, glw::GLuint param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribFormat(" << param0 << ", " << param1 << ", " << getTypeStr(param2) << ", " << getBooleanStr(param3) << ", " << param4 << ");" << TestLog::EndMessage;
	m_gl.vertexAttribFormat(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glVertexAttribIFormat (glw::GLuint param0, glw::GLint param1, glw::GLenum param2, glw::GLuint param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribIFormat(" << param0 << ", " << param1 << ", " << getTypeStr(param2) << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.vertexAttribIFormat(param0, param1, param2, param3);
}

void CallLogWrapper::glVertexAttribLFormat (glw::GLuint param0, glw::GLint param1, glw::GLenum param2, glw::GLuint param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribLFormat(" << param0 << ", " << param1 << ", " << toHex(param2) << ", " << param3 << ");" << TestLog::EndMessage;
	m_gl.vertexAttribLFormat(param0, param1, param2, param3);
}

void CallLogWrapper::glVertexAttribBinding (glw::GLuint param0, glw::GLuint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexAttribBinding(" << param0 << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.vertexAttribBinding(param0, param1);
}

void CallLogWrapper::glVertexBindingDivisor (glw::GLuint param0, glw::GLuint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glVertexBindingDivisor(" << param0 << ", " << param1 << ");" << TestLog::EndMessage;
	m_gl.vertexBindingDivisor(param0, param1);
}

void CallLogWrapper::glBufferStorage (glw::GLenum param0, glw::GLsizeiptr param1, const void* param2, glw::GLbitfield param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBufferStorage(" << toHex(param0) << ", " << param1 << ", " << toHex(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.bufferStorage(param0, param1, param2, param3);
}

void CallLogWrapper::glClearTexImage (glw::GLuint param0, glw::GLint param1, glw::GLenum param2, glw::GLenum param3, const void* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearTexImage(" << param0 << ", " << param1 << ", " << toHex(param2) << ", " << toHex(param3) << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.clearTexImage(param0, param1, param2, param3, param4);
}

void CallLogWrapper::glClearTexSubImage (glw::GLuint param0, glw::GLint param1, glw::GLint param2, glw::GLint param3, glw::GLint param4, glw::GLsizei param5, glw::GLsizei param6, glw::GLsizei param7, glw::GLenum param8, glw::GLenum param9, const void* param10)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glClearTexSubImage(" << param0 << ", " << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ", " << param5 << ", " << param6 << ", " << param7 << ", " << toHex(param8) << ", " << toHex(param9) << ", " << toHex(param10) << ");" << TestLog::EndMessage;
	m_gl.clearTexSubImage(param0, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10);
}

void CallLogWrapper::glBindBuffersBase (glw::GLenum param0, glw::GLuint param1, glw::GLsizei param2, const glw::GLuint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindBuffersBase(" << toHex(param0) << ", " << param1 << ", " << param2 << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	m_gl.bindBuffersBase(param0, param1, param2, param3);
}

void CallLogWrapper::glBindBuffersRange (glw::GLenum param0, glw::GLuint param1, glw::GLsizei param2, const glw::GLuint* param3, const glw::GLintptr* param4, const glw::GLsizeiptr* param5)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindBuffersRange(" << toHex(param0) << ", " << param1 << ", " << param2 << ", " << toHex(param3) << ", " << toHex(param4) << ", " << toHex(param5) << ");" << TestLog::EndMessage;
	m_gl.bindBuffersRange(param0, param1, param2, param3, param4, param5);
}

void CallLogWrapper::glBindTextures (glw::GLuint param0, glw::GLsizei param1, const glw::GLuint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindTextures(" << param0 << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.bindTextures(param0, param1, param2);
}

void CallLogWrapper::glBindSamplers (glw::GLuint param0, glw::GLsizei param1, const glw::GLuint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindSamplers(" << param0 << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.bindSamplers(param0, param1, param2);
}

void CallLogWrapper::glBindImageTextures (glw::GLuint param0, glw::GLsizei param1, const glw::GLuint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindImageTextures(" << param0 << ", " << param1 << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	m_gl.bindImageTextures(param0, param1, param2);
}

void CallLogWrapper::glBindVertexBuffers (glw::GLuint param0, glw::GLsizei param1, const glw::GLuint* param2, const glw::GLintptr* param3, const glw::GLsizei* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "glBindVertexBuffers(" << param0 << ", " << param1 << ", " << toHex(param2) << ", " << toHex(param3) << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	m_gl.bindVertexBuffers(param0, param1, param2, param3, param4);
}
