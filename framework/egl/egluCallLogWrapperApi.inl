/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 *
 * Generated from Khronos EGL API description (egl.xml) revision 28861.
 */
EGLBoolean									eglBindAPI							(EGLenum api);
EGLBoolean									eglBindTexImage						(EGLDisplay dpy, EGLSurface surface, EGLint buffer);
EGLBoolean									eglChooseConfig						(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config);
EGLBoolean									eglCopyBuffers						(EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target);
EGLContext									eglCreateContext					(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list);
EGLSurface									eglCreatePbufferFromClientBuffer	(EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer, EGLConfig config, const EGLint *attrib_list);
EGLSurface									eglCreatePbufferSurface				(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list);
EGLSurface									eglCreatePixmapSurface				(EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint *attrib_list);
EGLSurface									eglCreateWindowSurface				(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list);
EGLBoolean									eglDestroyContext					(EGLDisplay dpy, EGLContext ctx);
EGLBoolean									eglDestroySurface					(EGLDisplay dpy, EGLSurface surface);
EGLBoolean									eglGetConfigAttrib					(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value);
EGLBoolean									eglGetConfigs						(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config);
EGLContext									eglGetCurrentContext				(void);
EGLDisplay									eglGetCurrentDisplay				(void);
EGLSurface									eglGetCurrentSurface				(EGLint readdraw);
EGLDisplay									eglGetDisplay						(EGLNativeDisplayType display_id);
EGLint										eglGetError							(void);
__eglMustCastToProperFunctionPointerType	eglGetProcAddress					(const char *procname);
EGLBoolean									eglInitialize						(EGLDisplay dpy, EGLint *major, EGLint *minor);
EGLBoolean									eglMakeCurrent						(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx);
EGLenum										eglQueryAPI							(void);
EGLBoolean									eglQueryContext						(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value);
const char *								eglQueryString						(EGLDisplay dpy, EGLint name);
EGLBoolean									eglQuerySurface						(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value);
EGLBoolean									eglReleaseTexImage					(EGLDisplay dpy, EGLSurface surface, EGLint buffer);
EGLBoolean									eglReleaseThread					(void);
EGLBoolean									eglSurfaceAttrib					(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value);
EGLBoolean									eglSwapBuffers						(EGLDisplay dpy, EGLSurface surface);
EGLBoolean									eglSwapInterval						(EGLDisplay dpy, EGLint interval);
EGLBoolean									eglTerminate						(EGLDisplay dpy);
EGLBoolean									eglWaitClient						(void);
EGLBoolean									eglWaitGL							(void);
EGLBoolean									eglWaitNative						(EGLint engine);
