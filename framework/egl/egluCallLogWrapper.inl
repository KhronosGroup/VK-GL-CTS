/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 *
 * Generated from Khronos EGL API description (egl.xml) revision 28861.
 */

EGLBoolean CallLogWrapper::eglBindAPI (EGLenum api)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglBindAPI(" << getAPIStr(api) << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglBindAPI(api);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglBindTexImage (EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglBindTexImage(" << dpy << ", " << toHex(surface) << ", " << buffer << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglBindTexImage(dpy, surface, buffer);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglChooseConfig (EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglChooseConfig(" << dpy << ", " << getConfigAttribListStr(attrib_list) << ", " << configs << ", " << config_size << ", " << num_config << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglChooseConfig(dpy, attrib_list, configs, config_size, num_config);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// configs = " << getPointerStr(configs, (num_config && returnValue) ? deMin32(config_size, *num_config) : 0) << TestLog::EndMessage;
		m_log << TestLog::Message << "// num_config = " << (num_config ? de::toString(*num_config) : "NULL") << TestLog::EndMessage;
	}
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglCopyBuffers (EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCopyBuffers(" << dpy << ", " << toHex(surface) << ", " << toHex(target) << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglCopyBuffers(dpy, surface, target);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLContext CallLogWrapper::eglCreateContext (EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCreateContext(" << dpy << ", " << toHex(config) << ", " << share_context << ", " << getContextAttribListStr(attrib_list) << ");" << TestLog::EndMessage;
	EGLContext returnValue = ::eglCreateContext(dpy, config, share_context, attrib_list);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLSurface CallLogWrapper::eglCreatePbufferFromClientBuffer (EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer, EGLConfig config, const EGLint *attrib_list)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCreatePbufferFromClientBuffer(" << dpy << ", " << toHex(buftype) << ", " << toHex(buffer) << ", " << toHex(config) << ", " << attrib_list << ");" << TestLog::EndMessage;
	EGLSurface returnValue = ::eglCreatePbufferFromClientBuffer(dpy, buftype, buffer, config, attrib_list);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLSurface CallLogWrapper::eglCreatePbufferSurface (EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCreatePbufferSurface(" << dpy << ", " << toHex(config) << ", " << getSurfaceAttribListStr(attrib_list) << ");" << TestLog::EndMessage;
	EGLSurface returnValue = ::eglCreatePbufferSurface(dpy, config, attrib_list);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLSurface CallLogWrapper::eglCreatePixmapSurface (EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint *attrib_list)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCreatePixmapSurface(" << dpy << ", " << toHex(config) << ", " << toHex(pixmap) << ", " << getSurfaceAttribListStr(attrib_list) << ");" << TestLog::EndMessage;
	EGLSurface returnValue = ::eglCreatePixmapSurface(dpy, config, pixmap, attrib_list);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLSurface CallLogWrapper::eglCreateWindowSurface (EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCreateWindowSurface(" << dpy << ", " << toHex(config) << ", " << toHex(win) << ", " << getSurfaceAttribListStr(attrib_list) << ");" << TestLog::EndMessage;
	EGLSurface returnValue = ::eglCreateWindowSurface(dpy, config, win, attrib_list);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglDestroyContext (EGLDisplay dpy, EGLContext ctx)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglDestroyContext(" << dpy << ", " << ctx << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglDestroyContext(dpy, ctx);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglDestroySurface (EGLDisplay dpy, EGLSurface surface)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglDestroySurface(" << dpy << ", " << toHex(surface) << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglDestroySurface(dpy, surface);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglGetConfigAttrib (EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetConfigAttrib(" << dpy << ", " << toHex(config) << ", " << getConfigAttribStr(attribute) << ", " << value << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglGetConfigAttrib(dpy, config, attribute, value);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// value = " << getConfigAttribValuePointerStr(attribute, value) << TestLog::EndMessage;
	}
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglGetConfigs (EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetConfigs(" << dpy << ", " << configs << ", " << config_size << ", " << num_config << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglGetConfigs(dpy, configs, config_size, num_config);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLContext CallLogWrapper::eglGetCurrentContext ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetCurrentContext(" << ");" << TestLog::EndMessage;
	EGLContext returnValue = ::eglGetCurrentContext();
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLDisplay CallLogWrapper::eglGetCurrentDisplay ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetCurrentDisplay(" << ");" << TestLog::EndMessage;
	EGLDisplay returnValue = ::eglGetCurrentDisplay();
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLSurface CallLogWrapper::eglGetCurrentSurface (EGLint readdraw)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetCurrentSurface(" << getSurfaceTargetStr(readdraw) << ");" << TestLog::EndMessage;
	EGLSurface returnValue = ::eglGetCurrentSurface(readdraw);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLDisplay CallLogWrapper::eglGetDisplay (EGLNativeDisplayType display_id)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetDisplay(" << toHex(display_id) << ");" << TestLog::EndMessage;
	EGLDisplay returnValue = ::eglGetDisplay(display_id);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLint CallLogWrapper::eglGetError ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetError(" << ");" << TestLog::EndMessage;
	EGLint returnValue = ::eglGetError();
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getErrorStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

__eglMustCastToProperFunctionPointerType CallLogWrapper::eglGetProcAddress (const char *procname)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetProcAddress(" << getStringStr(procname) << ");" << TestLog::EndMessage;
	__eglMustCastToProperFunctionPointerType returnValue = ::eglGetProcAddress(procname);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << tcu::toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglInitialize (EGLDisplay dpy, EGLint *major, EGLint *minor)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglInitialize(" << dpy << ", " << major << ", " << minor << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglInitialize(dpy, major, minor);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglMakeCurrent (EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglMakeCurrent(" << dpy << ", " << toHex(draw) << ", " << toHex(read) << ", " << ctx << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglMakeCurrent(dpy, draw, read, ctx);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
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

EGLBoolean CallLogWrapper::eglQueryContext (EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglQueryContext(" << dpy << ", " << ctx << ", " << getContextAttribStr(attribute) << ", " << value << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglQueryContext(dpy, ctx, attribute, value);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// value = " << getContextAttribValuePointerStr(attribute, value) << TestLog::EndMessage;
	}
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

const char * CallLogWrapper::eglQueryString (EGLDisplay dpy, EGLint name)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglQueryString(" << dpy << ", " << name << ");" << TestLog::EndMessage;
	const char * returnValue = ::eglQueryString(dpy, name);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getStringStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglQuerySurface (EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglQuerySurface(" << dpy << ", " << toHex(surface) << ", " << getSurfaceAttribStr(attribute) << ", " << value << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglQuerySurface(dpy, surface, attribute, value);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// value = " << getSurfaceAttribValuePointerStr(attribute, value) << TestLog::EndMessage;
	}
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglReleaseTexImage (EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglReleaseTexImage(" << dpy << ", " << toHex(surface) << ", " << buffer << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglReleaseTexImage(dpy, surface, buffer);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglReleaseThread ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglReleaseThread(" << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglReleaseThread();
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglSurfaceAttrib (EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglSurfaceAttrib(" << dpy << ", " << toHex(surface) << ", " << getSurfaceAttribStr(attribute) << ", " << getSurfaceAttribValueStr(attribute, value) << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglSurfaceAttrib(dpy, surface, attribute, value);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglSwapBuffers (EGLDisplay dpy, EGLSurface surface)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglSwapBuffers(" << dpy << ", " << toHex(surface) << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglSwapBuffers(dpy, surface);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglSwapInterval (EGLDisplay dpy, EGLint interval)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglSwapInterval(" << dpy << ", " << interval << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglSwapInterval(dpy, interval);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglTerminate (EGLDisplay dpy)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglTerminate(" << dpy << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglTerminate(dpy);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglWaitClient ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglWaitClient(" << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglWaitClient();
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglWaitGL ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglWaitGL(" << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglWaitGL();
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

EGLBoolean CallLogWrapper::eglWaitNative (EGLint engine)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglWaitNative(" << engine << ");" << TestLog::EndMessage;
	EGLBoolean returnValue = ::eglWaitNative(engine);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}
