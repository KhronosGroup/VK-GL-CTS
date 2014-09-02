/* WARNING! THIS IS A PROGRAMMATICALLY GENERATED CODE. DO NOT MODIFY THE CODE,
 * SINCE THE CHANGES WILL BE LOST! MODIFY THE GENERATING PYTHON INSTEAD.
 */

EGLint CallLogWrapper::eglGetError ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetError(" << ");" << TestLog::EndMessage;
	EGLint returnValue = ::eglGetError();
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getErrorStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLDisplay CallLogWrapper::eglGetDisplay (EGLNativeDisplayType param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetDisplay(" << toHex(param0) << ");" << TestLog::EndMessage;
	EGLDisplay returnValue = ::eglGetDisplay(param0);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getEGLDisplayStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglInitialize (EGLDisplay param0, EGLint* param1, EGLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglInitialize(" << getEGLDisplayStr(param0) << ", " << toHex(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglInitialize(param0, param1, param2);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << (returnValue != EGL_FALSE ? "EGL_TRUE" : "EGL_FALSE") << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglTerminate (EGLDisplay param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglTerminate(" << getEGLDisplayStr(param0) << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglTerminate(param0);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << (returnValue != EGL_FALSE ? "EGL_TRUE" : "EGL_FALSE") << " returned" << TestLog::EndMessage;
	return returnValue;
}

const char* CallLogWrapper::eglQueryString (EGLDisplay param0, EGLint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglQueryString(" << getEGLDisplayStr(param0) << ", " << param1 << ");" << TestLog::EndMessage;
	const char* returnValue = ::eglQueryString(param0, param1);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getStringStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglGetConfigs (EGLDisplay param0, EGLConfig* param1, EGLint param2, EGLint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetConfigs(" << getEGLDisplayStr(param0) << ", " << toHex(param1) << ", " << param2 << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglGetConfigs(param0, param1, param2, param3);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << (returnValue != EGL_FALSE ? "EGL_TRUE" : "EGL_FALSE") << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglChooseConfig (EGLDisplay param0, const EGLint* param1, EGLConfig* param2, EGLint param3, EGLint* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglChooseConfig(" << getEGLDisplayStr(param0) << ", " << getConfigAttribListStr(param1) << ", " << toHex(param2) << ", " << param3 << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglChooseConfig(param0, param1, param2, param3, param4);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 2 = " << getPointerStr(param2, (param4 && returnValue) ? deMin32(param3, *param4) : 0) << TestLog::EndMessage;
		m_log << TestLog::Message << "// param 4 = " << (param4 ? de::toString(*param4) : "NULL") << TestLog::EndMessage;
	}
	if (m_enableLog)
		m_log << TestLog::Message << "// " << (returnValue != EGL_FALSE ? "EGL_TRUE" : "EGL_FALSE") << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglGetConfigAttrib (EGLDisplay param0, EGLConfig param1, EGLint param2, EGLint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetConfigAttrib(" << getEGLDisplayStr(param0) << ", " << toHex(param1) << ", " << getConfigAttribStr(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglGetConfigAttrib(param0, param1, param2, param3);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 3 = " << getConfigAttribValuePointerStr(param2, param3) << TestLog::EndMessage;
	}
	if (m_enableLog)
		m_log << TestLog::Message << "// " << (returnValue != EGL_FALSE ? "EGL_TRUE" : "EGL_FALSE") << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLSurface CallLogWrapper::eglCreateWindowSurface (EGLDisplay param0, EGLConfig param1, EGLNativeWindowType param2, const EGLint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCreateWindowSurface(" << getEGLDisplayStr(param0) << ", " << toHex(param1) << ", " << toHex(param2) << ", " << getSurfaceAttribListStr(param3) << ");" << TestLog::EndMessage;
	EGLSurface returnValue = ::eglCreateWindowSurface(param0, param1, param2, param3);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLSurface CallLogWrapper::eglCreatePbufferSurface (EGLDisplay param0, EGLConfig param1, const EGLint* param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCreatePbufferSurface(" << getEGLDisplayStr(param0) << ", " << toHex(param1) << ", " << getSurfaceAttribListStr(param2) << ");" << TestLog::EndMessage;
	EGLSurface returnValue = ::eglCreatePbufferSurface(param0, param1, param2);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLSurface CallLogWrapper::eglCreatePixmapSurface (EGLDisplay param0, EGLConfig param1, EGLNativePixmapType param2, const EGLint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCreatePixmapSurface(" << getEGLDisplayStr(param0) << ", " << toHex(param1) << ", " << toHex(param2) << ", " << getSurfaceAttribListStr(param3) << ");" << TestLog::EndMessage;
	EGLSurface returnValue = ::eglCreatePixmapSurface(param0, param1, param2, param3);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglDestroySurface (EGLDisplay param0, EGLSurface param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglDestroySurface(" << getEGLDisplayStr(param0) << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglDestroySurface(param0, param1);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << (returnValue != EGL_FALSE ? "EGL_TRUE" : "EGL_FALSE") << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglQuerySurface (EGLDisplay param0, EGLSurface param1, EGLint param2, EGLint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglQuerySurface(" << getEGLDisplayStr(param0) << ", " << toHex(param1) << ", " << getSurfaceAttribStr(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglQuerySurface(param0, param1, param2, param3);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 3 = " << getSurfaceAttribValuePointerStr(param2, param3) << TestLog::EndMessage;
	}
	if (m_enableLog)
		m_log << TestLog::Message << "// " << (returnValue != EGL_FALSE ? "EGL_TRUE" : "EGL_FALSE") << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglBindAPI (EGLenum param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglBindAPI(" << getAPIStr(param0) << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglBindAPI(param0);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << (returnValue != EGL_FALSE ? "EGL_TRUE" : "EGL_FALSE") << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLenum CallLogWrapper::eglQueryAPI ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglQueryAPI(" << ");" << TestLog::EndMessage;
	EGLenum returnValue = ::eglQueryAPI();
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getAPIStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglWaitClient ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglWaitClient(" << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglWaitClient();
	if (m_enableLog)
		m_log << TestLog::Message << "// " << (returnValue != EGL_FALSE ? "EGL_TRUE" : "EGL_FALSE") << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglReleaseThread ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglReleaseThread(" << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglReleaseThread();
	if (m_enableLog)
		m_log << TestLog::Message << "// " << (returnValue != EGL_FALSE ? "EGL_TRUE" : "EGL_FALSE") << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLSurface CallLogWrapper::eglCreatePbufferFromClientBuffer (EGLDisplay param0, EGLenum param1, EGLClientBuffer param2, EGLConfig param3, const EGLint* param4)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCreatePbufferFromClientBuffer(" << getEGLDisplayStr(param0) << ", " << toHex(param1) << ", " << toHex(param2) << ", " << toHex(param3) << ", " << toHex(param4) << ");" << TestLog::EndMessage;
	EGLSurface returnValue = ::eglCreatePbufferFromClientBuffer(param0, param1, param2, param3, param4);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglSurfaceAttrib (EGLDisplay param0, EGLSurface param1, EGLint param2, EGLint param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglSurfaceAttrib(" << getEGLDisplayStr(param0) << ", " << toHex(param1) << ", " << getSurfaceAttribStr(param2) << ", " << getSurfaceAttribValueStr(param2, param3) << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglSurfaceAttrib(param0, param1, param2, param3);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << (returnValue != EGL_FALSE ? "EGL_TRUE" : "EGL_FALSE") << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglBindTexImage (EGLDisplay param0, EGLSurface param1, EGLint param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglBindTexImage(" << getEGLDisplayStr(param0) << ", " << toHex(param1) << ", " << param2 << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglBindTexImage(param0, param1, param2);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << (returnValue != EGL_FALSE ? "EGL_TRUE" : "EGL_FALSE") << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglReleaseTexImage (EGLDisplay param0, EGLSurface param1, EGLint param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglReleaseTexImage(" << getEGLDisplayStr(param0) << ", " << toHex(param1) << ", " << param2 << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglReleaseTexImage(param0, param1, param2);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << (returnValue != EGL_FALSE ? "EGL_TRUE" : "EGL_FALSE") << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglSwapInterval (EGLDisplay param0, EGLint param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglSwapInterval(" << getEGLDisplayStr(param0) << ", " << param1 << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglSwapInterval(param0, param1);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << (returnValue != EGL_FALSE ? "EGL_TRUE" : "EGL_FALSE") << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLContext CallLogWrapper::eglCreateContext (EGLDisplay param0, EGLConfig param1, EGLContext param2, const EGLint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCreateContext(" << getEGLDisplayStr(param0) << ", " << toHex(param1) << ", " << getEGLContextStr(param2) << ", " << getContextAttribListStr(param3) << ");" << TestLog::EndMessage;
	EGLContext returnValue = ::eglCreateContext(param0, param1, param2, param3);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getEGLContextStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglDestroyContext (EGLDisplay param0, EGLContext param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglDestroyContext(" << getEGLDisplayStr(param0) << ", " << getEGLContextStr(param1) << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglDestroyContext(param0, param1);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << (returnValue != EGL_FALSE ? "EGL_TRUE" : "EGL_FALSE") << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglMakeCurrent (EGLDisplay param0, EGLSurface param1, EGLSurface param2, EGLContext param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglMakeCurrent(" << getEGLDisplayStr(param0) << ", " << toHex(param1) << ", " << toHex(param2) << ", " << getEGLContextStr(param3) << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglMakeCurrent(param0, param1, param2, param3);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << (returnValue != EGL_FALSE ? "EGL_TRUE" : "EGL_FALSE") << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLContext CallLogWrapper::eglGetCurrentContext ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetCurrentContext(" << ");" << TestLog::EndMessage;
	EGLContext returnValue = ::eglGetCurrentContext();
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getEGLContextStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLSurface CallLogWrapper::eglGetCurrentSurface (EGLint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetCurrentSurface(" << getSurfaceTargetStr(param0) << ");" << TestLog::EndMessage;
	EGLSurface returnValue = ::eglGetCurrentSurface(param0);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLDisplay CallLogWrapper::eglGetCurrentDisplay ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetCurrentDisplay(" << ");" << TestLog::EndMessage;
	EGLDisplay returnValue = ::eglGetCurrentDisplay();
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getEGLDisplayStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglQueryContext (EGLDisplay param0, EGLContext param1, EGLint param2, EGLint* param3)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglQueryContext(" << getEGLDisplayStr(param0) << ", " << getEGLContextStr(param1) << ", " << getContextAttribStr(param2) << ", " << toHex(param3) << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglQueryContext(param0, param1, param2, param3);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// param 3 = " << getContextAttribValuePointerStr(param2, param3) << TestLog::EndMessage;
	}
	if (m_enableLog)
		m_log << TestLog::Message << "// " << (returnValue != EGL_FALSE ? "EGL_TRUE" : "EGL_FALSE") << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglWaitGL ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglWaitGL(" << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglWaitGL();
	if (m_enableLog)
		m_log << TestLog::Message << "// " << (returnValue != EGL_FALSE ? "EGL_TRUE" : "EGL_FALSE") << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglWaitNative (EGLint param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglWaitNative(" << param0 << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglWaitNative(param0);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << (returnValue != EGL_FALSE ? "EGL_TRUE" : "EGL_FALSE") << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglSwapBuffers (EGLDisplay param0, EGLSurface param1)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglSwapBuffers(" << getEGLDisplayStr(param0) << ", " << toHex(param1) << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglSwapBuffers(param0, param1);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << (returnValue != EGL_FALSE ? "EGL_TRUE" : "EGL_FALSE") << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglCopyBuffers (EGLDisplay param0, EGLSurface param1, EGLNativePixmapType param2)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCopyBuffers(" << getEGLDisplayStr(param0) << ", " << toHex(param1) << ", " << toHex(param2) << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglCopyBuffers(param0, param1, param2);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << (returnValue != EGL_FALSE ? "EGL_TRUE" : "EGL_FALSE") << " returned" << TestLog::EndMessage;
	return returnValue;
}

__eglMustCastToProperFunctionPointerType CallLogWrapper::eglGetProcAddress (const char* param0)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetProcAddress(" << getStringStr(param0) << ");" << TestLog::EndMessage;
	__eglMustCastToProperFunctionPointerType returnValue = ::eglGetProcAddress(param0);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << tcu::toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}
