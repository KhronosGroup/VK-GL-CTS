/*-------------------------------------------------------------------------
 * drawElements Quality Program EGL Module
 * ---------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief EGL image tests.
 *//*--------------------------------------------------------------------*/

#include "teglImageTests.hpp"

#include "teglImageFormatTests.hpp"

#include "egluNativeDisplay.hpp"
#include "egluNativeWindow.hpp"
#include "egluNativePixmap.hpp"
#include "egluStrUtil.hpp"
#include "egluUtil.hpp"

#include "gluDefs.hpp"
#include "gluStrUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"


#include <algorithm>
#include <sstream>
#include <string>
#include <vector>
#include <set>

#include <EGL/eglext.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

using tcu::TestLog;

using std::string;
using std::vector;
using std::set;

namespace deqp
{
namespace egl
{

namespace Image
{

bool checkExtensions (const tcu::egl::Display& dpy, const char** first, const char** last, vector<const char*>& unsupported)
{
	vector<string> extensions;
	dpy.getExtensions(extensions);

	set<string> extSet(extensions.begin(), extensions.end());

	unsupported.clear();

	for (const char** extIter = first; extIter != last; extIter++)
	{
		const char* ext = *extIter;

		if (extSet.find(ext) == extSet.end())
			unsupported.push_back(ext);
	}

	return unsupported.size() == 0;
}

string join (const vector<const char*>& parts, const char* separator)
{
	std::ostringstream str;
	for (std::vector<const char*>::const_iterator i = parts.begin(); i != parts.end(); i++)
	{
		if (i != parts.begin())
			str << separator;
		str << *i;
	}
	return str.str();
}

void checkExtensions (const tcu::egl::Display& dpy, const char** first, const char** last)
{
	vector<const char*> unsupported;
	if (!checkExtensions(dpy, first, last, unsupported))
		throw tcu::NotSupportedError("Extension not supported", join(unsupported, " ").c_str(), __FILE__, __LINE__);
}

template <size_t N>
void checkExtensions (const tcu::egl::Display& dpy, const char* (&extensions)[N])
{
	checkExtensions(dpy, &extensions[0], &extensions[N]);
}

#define CHECK_EXTENSIONS(EXTENSIONS) do { static const char* ext[] = EXTENSIONS; checkExtensions(m_eglTestCtx.getDisplay(), ext); } while (deGetFalse())

template <typename RetVal>
RetVal checkCallError (tcu::TestContext& testCtx, const char* call, RetVal returnValue, EGLint expectError)
{
	TestLog& log = testCtx.getLog();
	log << TestLog::Message << call << TestLog::EndMessage;

	EGLint error = eglGetError();

	if (error != expectError)
	{
		log << TestLog::Message << "  Fail: Error code mismatch! Expected " << eglu::getErrorStr(expectError) << ", got " << eglu::getErrorStr(error) << TestLog::EndMessage;
		log << TestLog::Message << "  " << returnValue << " was returned" << TestLog::EndMessage;

		if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
			testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid error code");
	}

	return returnValue;
}

template <typename RetVal>
void checkCallReturn (tcu::TestContext& testCtx, const char* call, RetVal returnValue, RetVal expectReturnValue, EGLint expectError)
{
	TestLog& log = testCtx.getLog();
	log << TestLog::Message << call << TestLog::EndMessage;

	EGLint error = eglGetError();

	if (returnValue != expectReturnValue)
	{
		log << TestLog::Message << "  Fail: Return value mismatch! Expected " << expectReturnValue << ", got " << returnValue << TestLog::EndMessage;

		if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
			testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid return value");
	}

	if (error != expectError)
	{
		log << TestLog::Message << "  Fail: Error code mismatch! Expected " << eglu::getErrorStr(expectError) << ", got " << eglu::getErrorStr(error) << TestLog::EndMessage;

		if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
			testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid error code");
	}
}

void checkGLCall (tcu::TestContext& testCtx, const char* call, GLenum expectError)
{
	TestLog& log = testCtx.getLog();
	log << TestLog::Message << call << TestLog::EndMessage;

	GLenum error = glGetError();

	if (error != expectError)
	{
		log << TestLog::Message << "  Fail: Error code mismatch! Expected " << glu::getErrorStr(expectError) << ", got " << glu::getErrorStr(error) << TestLog::EndMessage;

		if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
			testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid error code");
	}
}

// \note These macros expect "TestContext m_testCtx" and "ExtFuncTable efTable" variables to be defined.
#define CHECK_EXT_CALL_RET(CALL, EXPECT_RETURN_VALUE, EXPECT_ERROR)	checkCallReturn(m_testCtx, #CALL, efTable.CALL, (EXPECT_RETURN_VALUE), (EXPECT_ERROR))
#define CHECK_EXT_CALL_ERR(CALL, EXPECT_ERROR)						checkCallError(m_testCtx, #CALL, efTable.CALL, (EXPECT_ERROR))
#define CHECK_GL_EXT_CALL(CALL, EXPECT_ERROR)						do { efTable.CALL; checkGLCall(m_testCtx, #CALL, (EXPECT_ERROR)); } while (deGetFalse())

class ExtFuncTable
{
public:
	PFNEGLCREATEIMAGEKHRPROC						eglCreateImageKHR;
	PFNEGLDESTROYIMAGEKHRPROC						eglDestroyImageKHR;

	PFNGLEGLIMAGETARGETTEXTURE2DOESPROC				glEGLImageTargetTexture2DOES;
	PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC	glEGLImageTargetRenderbufferStorageOES;

	ExtFuncTable (void)
	{
		// EGL_KHR_image_base
		eglCreateImageKHR						= (PFNEGLCREATEIMAGEKHRPROC)						eglGetProcAddress("eglCreateImageKHR");
		eglDestroyImageKHR						= (PFNEGLDESTROYIMAGEKHRPROC)						eglGetProcAddress("eglDestroyImageKHR");

		// OES_EGL_image
		glEGLImageTargetTexture2DOES			= (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)				eglGetProcAddress("glEGLImageTargetTexture2DOES");
		glEGLImageTargetRenderbufferStorageOES	= (PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC)	eglGetProcAddress("glEGLImageTargetRenderbufferStorageOES");
	}
};

class InvalidCreateImage : public TestCase
{
public:
	InvalidCreateImage (EglTestContext& eglTestCtx)
		: TestCase(eglTestCtx, "invalid_create_image", "eglCreateImageKHR() with invalid arguments")
	{
	}

	IterateResult iterate (void)
	{
		EGLDisplay		dpy = m_eglTestCtx.getDisplay().getEGLDisplay();
		TestLog&		log	= m_testCtx.getLog();
		ExtFuncTable	efTable;

		CHECK_EXTENSIONS({ "EGL_KHR_image_base" });

		// Initialize result to pass.
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		log << TestLog::Message << "Testing bad display (-1)..." << TestLog::EndMessage;
		CHECK_EXT_CALL_RET(eglCreateImageKHR((EGLDisplay)-1, EGL_NO_CONTEXT, EGL_NONE, 0, DE_NULL),
						   EGL_NO_IMAGE_KHR, EGL_BAD_DISPLAY);

		log << TestLog::Message << "Testing bad context (-1)..." << TestLog::EndMessage;
		CHECK_EXT_CALL_RET(eglCreateImageKHR(dpy, (EGLContext)-1, EGL_NONE, 0, DE_NULL),
						   EGL_NO_IMAGE_KHR, EGL_BAD_CONTEXT);

		log << TestLog::Message << "Testing bad parameter (-1).." << TestLog::EndMessage;
		CHECK_EXT_CALL_RET(eglCreateImageKHR(dpy, EGL_NO_CONTEXT, (EGLenum)-1, 0, DE_NULL),
						   EGL_NO_IMAGE_KHR, EGL_BAD_PARAMETER);

		return STOP;
	}
};

class GLES2Context
{
public:
	GLES2Context (EglTestContext& eglTestCtx, EGLint configId, int width, int height)
		: m_eglTestCtx	(eglTestCtx)
		, m_config		(getConfigById(eglTestCtx.getDisplay(), configId))
		, m_context		(eglTestCtx.getDisplay(), m_config, m_ctxAttrs, EGL_OPENGL_ES_API)
		, m_window		(DE_NULL)
		, m_pixmap		(DE_NULL)
		, m_surface		(DE_NULL)
	{
		tcu::egl::Display&	dpy				= eglTestCtx.getDisplay();
		EGLint				surfaceTypeBits	= dpy.getConfigAttrib(m_config, EGL_SURFACE_TYPE);

		if (surfaceTypeBits & EGL_PBUFFER_BIT)
		{
			EGLint pbufferAttrs[] =
			{
				EGL_WIDTH,		width,
				EGL_HEIGHT,		height,
				EGL_NONE
			};

			m_surface = new tcu::egl::PbufferSurface(dpy, m_config, pbufferAttrs);
		}
		else if (surfaceTypeBits & EGL_WINDOW_BIT)
		{
			m_window	= eglTestCtx.createNativeWindow(dpy.getEGLDisplay(), m_config, DE_NULL, width, height, eglu::parseWindowVisibility(eglTestCtx.getTestContext().getCommandLine()));
			m_surface	= new tcu::egl::WindowSurface(dpy, eglu::createWindowSurface(eglTestCtx.getNativeDisplay(), *m_window, dpy.getEGLDisplay(), m_config, DE_NULL));
		}
		else if (surfaceTypeBits & EGL_PIXMAP_BIT)
		{
			m_pixmap	= eglTestCtx.createNativePixmap(dpy.getEGLDisplay(), m_config, DE_NULL, width, height);
			m_surface	= new tcu::egl::PixmapSurface(dpy, eglu::createPixmapSurface(eglTestCtx.getNativeDisplay(), *m_pixmap, dpy.getEGLDisplay(), m_config, DE_NULL));
		}
		else
			TCU_FAIL("No valid surface types supported in config");

		m_context.makeCurrent(*m_surface, *m_surface);
	}

	~GLES2Context (void)
	{
		eglMakeCurrent(m_eglTestCtx.getDisplay().getEGLDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

		delete m_window;
		delete m_pixmap;
		delete m_surface;
	}

	EGLDisplay getEglDisplay (void)
	{
		return m_eglTestCtx.getDisplay().getEGLDisplay();
	}

	EGLContext getEglContext (void)
	{
		return m_context.getEGLContext();
	}

	// Helper for selecting config.
	static EGLint getConfigIdForApi (const vector<eglu::ConfigInfo>& configInfos, EGLint apiBits)
	{
		EGLint	windowCfg	= 0;
		EGLint	pixmapCfg	= 0;
		EGLint	pbufferCfg	= 0;

		for (vector<eglu::ConfigInfo>::const_iterator cfgIter = configInfos.begin(); cfgIter != configInfos.end(); cfgIter++)
		{
			if ((cfgIter->renderableType & apiBits) == 0)
				continue;

			if (windowCfg == 0 && (cfgIter->surfaceType & EGL_WINDOW_BIT) != 0)
				windowCfg = cfgIter->configId;

			if (pixmapCfg == 0 && (cfgIter->surfaceType & EGL_PIXMAP_BIT) != 0)
				pixmapCfg = cfgIter->configId;

			if (pbufferCfg == 0 && (cfgIter->surfaceType & EGL_PBUFFER_BIT) != 0)
				pbufferCfg = cfgIter->configId;

			if (windowCfg && pixmapCfg && pbufferCfg)
				break;
		}

		// Prefer configs in order: pbuffer, window, pixmap
		if (pbufferCfg)
			return pbufferCfg;
		else if (windowCfg)
			return windowCfg;
		else if (pixmapCfg)
			return pixmapCfg;
		else
			throw tcu::NotSupportedError("No compatible EGL configs found", "", __FILE__, __LINE__);
	}

private:
	static EGLConfig getConfigById (const tcu::egl::Display& dpy, EGLint configId)
	{
		EGLint attributes[] = { EGL_CONFIG_ID, configId, EGL_NONE };
		vector<EGLConfig> configs;
		dpy.chooseConfig(attributes, configs);
		TCU_CHECK(configs.size() == 1);
		return configs[0];
	}

	static const EGLint			m_ctxAttrs[];

	EglTestContext&				m_eglTestCtx;
	EGLConfig					m_config;
	tcu::egl::Context			m_context;
	eglu::NativeWindow*			m_window;
	eglu::NativePixmap*			m_pixmap;
	tcu::egl::Surface*			m_surface;

								GLES2Context	(const GLES2Context&);
	GLES2Context&				operator=		(const GLES2Context&);
};

const EGLint GLES2Context::m_ctxAttrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };

class CreateImageGLES2 : public TestCase
{
public:
	static const char* getTargetName (EGLint target)
	{
		switch (target)
		{
			case EGL_GL_TEXTURE_2D_KHR:						return "tex2d";
			case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR:	return "cubemap_pos_x";
			case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR:	return "cubemap_neg_x";
			case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR:	return "cubemap_pos_y";
			case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR:	return "cubemap_neg_y";
			case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR:	return "cubemap_pos_z";
			case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR:	return "cubemap_neg_z";
			case EGL_GL_RENDERBUFFER_KHR:					return "renderbuffer";
			default:		DE_ASSERT(DE_FALSE);			return "";
		}
	}

	static const char* getStorageName (GLenum storage)
	{
		switch (storage)
		{
			case GL_RGB:				return "rgb";
			case GL_RGBA:				return "rgba";
			case GL_DEPTH_COMPONENT16:	return "depth_component_16";
			case GL_RGBA4:				return "rgba4";
			case GL_RGB5_A1:			return "rgb5_a1";
			case GL_RGB565:				return "rgb565";
			case GL_STENCIL_INDEX8:		return "stencil_index8";
			default:
				DE_ASSERT(DE_FALSE);
				return "";
		}
	}

	CreateImageGLES2 (EglTestContext& eglTestCtx, EGLint target, GLenum storage, bool useTexLevel0 = false)
		: TestCase			(eglTestCtx, (string("create_image_gles2_") + getTargetName(target) + "_" + getStorageName(storage) + (useTexLevel0 ? "_level0_only" : "")).c_str(), "Create EGLImage from GLES2 object")
		, m_target			(target)
		, m_storage			(storage)
		, m_useTexLevel0	(useTexLevel0)
	{
	}

	IterateResult iterate (void)
	{
		TestLog&		log	= m_testCtx.getLog();
		ExtFuncTable	efTable;

		if (m_target == EGL_GL_TEXTURE_2D_KHR)
			CHECK_EXTENSIONS({"EGL_KHR_gl_texture_2D_image"});
		else if (m_target == EGL_GL_RENDERBUFFER_KHR)
			CHECK_EXTENSIONS({"EGL_KHR_gl_renderbuffer_image"});
		else
			CHECK_EXTENSIONS({"EGL_KHR_gl_texture_cubemap_image"});

		// Initialize result.
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		// Create GLES2 context
		EGLint configId = GLES2Context::getConfigIdForApi(m_eglTestCtx.getConfigs(), EGL_OPENGL_ES2_BIT);
		log << TestLog::Message << "Using EGL config " << configId << TestLog::EndMessage;

		GLES2Context context(m_eglTestCtx, configId, 64, 64);

		switch (m_target)
		{
			case EGL_GL_TEXTURE_2D_KHR:
			{
				deUint32 tex = 1;
				GLU_CHECK_CALL(glBindTexture(GL_TEXTURE_2D, tex));

				// Specify mipmap level 0
				GLU_CHECK_CALL(glTexImage2D(GL_TEXTURE_2D, 0, m_storage, 64, 64, 0, m_storage, GL_UNSIGNED_BYTE, DE_NULL));

				if (!m_useTexLevel0)
				{
					// Set minification filter to linear. This makes the texture complete.
					GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
				}
				// Else spec allows using incomplete texture when miplevel 0 is only used and specified.

				// Create EGL image
				EGLint		attribs[]	= { EGL_GL_TEXTURE_LEVEL_KHR, 0, EGL_NONE };
				EGLImageKHR	image		= CHECK_EXT_CALL_ERR(eglCreateImageKHR(context.getEglDisplay(), context.getEglContext(), EGL_GL_TEXTURE_2D_KHR, (EGLClientBuffer)(deUintptr)tex, attribs), EGL_SUCCESS);
				if (image == EGL_NO_IMAGE_KHR)
				{
					log << TestLog::Message << "  Fail: Got EGL_NO_IMAGE_KHR!" << TestLog::EndMessage;

					if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
						m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got EGL_NO_IMAGE_KHR");
				}

				// Destroy image
				CHECK_EXT_CALL_RET(eglDestroyImageKHR(context.getEglDisplay(), image), (EGLBoolean)EGL_TRUE, EGL_SUCCESS);

				// Destroy texture object
				GLU_CHECK_CALL(glDeleteTextures(1, &tex));

				break;
			}

			case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR:
			case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR:
			case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR:
			case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR:
			case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR:
			case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR:
			{
				deUint32 tex = 1;
				GLU_CHECK_CALL(glBindTexture(GL_TEXTURE_CUBE_MAP, tex));

				// Specify mipmap level 0 for all faces
				GLenum faces[] =
				{
					GL_TEXTURE_CUBE_MAP_POSITIVE_X,
					GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
					GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
					GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
					GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
					GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
				};
				for (int faceNdx = 0; faceNdx < DE_LENGTH_OF_ARRAY(faces); faceNdx++)
					GLU_CHECK_CALL(glTexImage2D(faces[faceNdx], 0, m_storage, 64, 64, 0, m_storage, GL_UNSIGNED_BYTE, DE_NULL));

				if (!m_useTexLevel0)
				{
					// Set minification filter to linear.
					GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
				}

				// Create EGL image
				EGLint		attribs[]	= { EGL_GL_TEXTURE_LEVEL_KHR, 0, EGL_NONE };
				EGLImageKHR	image		= CHECK_EXT_CALL_ERR(eglCreateImageKHR(context.getEglDisplay(), context.getEglContext(), m_target, (EGLClientBuffer)(deUintptr)tex, attribs), EGL_SUCCESS);
				if (image == EGL_NO_IMAGE_KHR)
				{
					log << TestLog::Message << "  Fail: Got EGL_NO_IMAGE_KHR!" << TestLog::EndMessage;

					if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
						m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got EGL_NO_IMAGE_KHR");
				}

				// Destroy image
				CHECK_EXT_CALL_RET(eglDestroyImageKHR(context.getEglDisplay(), image), (EGLBoolean)EGL_TRUE, EGL_SUCCESS);

				// Destroy texture object
				GLU_CHECK_CALL(glDeleteTextures(1, &tex));

				break;
			}

			case EGL_GL_RENDERBUFFER_KHR:
			{
				// Create renderbuffer.
				deUint32 rbo = 1;
				GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, rbo));

				// Specify storage.
				GLU_CHECK_CALL(glRenderbufferStorage(GL_RENDERBUFFER, m_storage, 64, 64));

				// Create EGL image
				EGLImageKHR image = CHECK_EXT_CALL_ERR(eglCreateImageKHR(context.getEglDisplay(), context.getEglContext(), EGL_GL_RENDERBUFFER_KHR, (EGLClientBuffer)(deUintptr)rbo, DE_NULL), EGL_SUCCESS);
				if (image == EGL_NO_IMAGE_KHR)
				{
					log << TestLog::Message << "  Fail: Got EGL_NO_IMAGE_KHR!" << TestLog::EndMessage;

					if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
						m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got EGL_NO_IMAGE_KHR");
				}

				// Destroy image
				CHECK_EXT_CALL_RET(eglDestroyImageKHR(context.getEglDisplay(), image), (EGLBoolean)EGL_TRUE, EGL_SUCCESS);

				// Destroy texture object
				GLU_CHECK_CALL(glDeleteRenderbuffers(1, &rbo));

				break;
			}

			default:
				DE_ASSERT(DE_FALSE);
				break;
		}

		return STOP;
	}

private:
	EGLint	m_target;
	GLenum	m_storage;
	bool	m_useTexLevel0;
};

class ImageTargetGLES2 : public TestCase
{
public:
	static const char* getTargetName (GLenum target)
	{
		switch (target)
		{
			case GL_TEXTURE_2D:		return "tex2d";
			case GL_RENDERBUFFER:	return "renderbuffer";
			default:
				DE_ASSERT(DE_FALSE);
				return "";
		}
	}

	ImageTargetGLES2 (EglTestContext& eglTestCtx, GLenum target)
		: TestCase	(eglTestCtx, (string("image_target_gles2_") + getTargetName(target)).c_str(), "Use EGLImage as GLES2 object")
		, m_target	(target)
	{
	}

	IterateResult iterate (void)
	{
		TestLog&		log	= m_testCtx.getLog();
		ExtFuncTable	efTable;

		// \todo [2011-07-21 pyry] Try all possible EGLImage sources
		CHECK_EXTENSIONS({"EGL_KHR_gl_texture_2D_image"});

		// Initialize result.
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		// Create GLES2 context
		EGLint configId = GLES2Context::getConfigIdForApi(m_eglTestCtx.getConfigs(), EGL_OPENGL_ES2_BIT);
		log << TestLog::Message << "Using EGL config " << configId << TestLog::EndMessage;

		GLES2Context context(m_eglTestCtx, configId, 64, 64);

		// Check for OES_EGL_image
		{
			const char* glExt = (const char*)glGetString(GL_EXTENSIONS);

			if (string(glExt).find("GL_OES_EGL_image") == string::npos)
				throw tcu::NotSupportedError("Extension not supported", "GL_OES_EGL_image", __FILE__, __LINE__);

			TCU_CHECK(efTable.glEGLImageTargetTexture2DOES);
			TCU_CHECK(efTable.glEGLImageTargetRenderbufferStorageOES);
		}

		// Create GL_TEXTURE_2D and EGLImage from it.
		log << TestLog::Message << "Creating EGLImage using GL_TEXTURE_2D with GL_RGBA storage" << TestLog::EndMessage;

		deUint32 srcTex = 1;
		GLU_CHECK_CALL(glBindTexture(GL_TEXTURE_2D, srcTex));
		GLU_CHECK_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, DE_NULL));
		GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));

		// Create EGL image
		EGLint		attribs[]	= { EGL_GL_TEXTURE_LEVEL_KHR, 0, EGL_NONE };
		EGLImageKHR	image		= CHECK_EXT_CALL_ERR(eglCreateImageKHR(context.getEglDisplay(), context.getEglContext(), EGL_GL_TEXTURE_2D_KHR, (EGLClientBuffer)(deUintptr)srcTex, attribs), EGL_SUCCESS);
		if (image == EGL_NO_IMAGE_KHR)
		{
			log << TestLog::Message << "  Fail: Got EGL_NO_IMAGE_KHR!" << TestLog::EndMessage;

			if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got EGL_NO_IMAGE_KHR");
		}

		// Create texture or renderbuffer
		if (m_target == GL_TEXTURE_2D)
		{
			log << TestLog::Message << "Creating GL_TEXTURE_2D from EGLimage" << TestLog::EndMessage;

			deUint32 dstTex = 2;
			GLU_CHECK_CALL(glBindTexture(GL_TEXTURE_2D, dstTex));
			CHECK_GL_EXT_CALL(glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)image), GL_NO_ERROR);
			GLU_CHECK_CALL(glDeleteTextures(1, &dstTex));
		}
		else
		{
			DE_ASSERT(m_target == GL_RENDERBUFFER);

			log << TestLog::Message << "Creating GL_RENDERBUFFER from EGLimage" << TestLog::EndMessage;

			deUint32 dstRbo = 2;
			GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, dstRbo));
			CHECK_GL_EXT_CALL(glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, (GLeglImageOES)image), GL_NO_ERROR);
			GLU_CHECK_CALL(glDeleteRenderbuffers(1, &dstRbo));
		}

		// Destroy image
		CHECK_EXT_CALL_RET(eglDestroyImageKHR(context.getEglDisplay(), image), (EGLBoolean)EGL_TRUE, EGL_SUCCESS);

		// Destroy source texture object
		GLU_CHECK_CALL(glDeleteTextures(1, &srcTex));

		return STOP;
	}

private:
	GLenum	m_target;
};

class ApiTests : public TestCaseGroup
{
public:
	ApiTests (EglTestContext& eglTestCtx)
		: TestCaseGroup(eglTestCtx, "api", "EGLImage API tests")
	{
	}

	void init (void)
	{
		addChild(new Image::InvalidCreateImage(m_eglTestCtx));

		addChild(new Image::CreateImageGLES2(m_eglTestCtx, EGL_GL_TEXTURE_2D_KHR, GL_RGB));
		addChild(new Image::CreateImageGLES2(m_eglTestCtx, EGL_GL_TEXTURE_2D_KHR, GL_RGBA));
		addChild(new Image::CreateImageGLES2(m_eglTestCtx, EGL_GL_TEXTURE_2D_KHR, GL_RGBA, true));

		addChild(new Image::CreateImageGLES2(m_eglTestCtx, EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR, GL_RGB));
		addChild(new Image::CreateImageGLES2(m_eglTestCtx, EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR, GL_RGBA));
		addChild(new Image::CreateImageGLES2(m_eglTestCtx, EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR, GL_RGBA, true));

		addChild(new Image::CreateImageGLES2(m_eglTestCtx, EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR, GL_RGBA));
		addChild(new Image::CreateImageGLES2(m_eglTestCtx, EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR, GL_RGBA));
		addChild(new Image::CreateImageGLES2(m_eglTestCtx, EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR, GL_RGBA));
		addChild(new Image::CreateImageGLES2(m_eglTestCtx, EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR, GL_RGBA));
		addChild(new Image::CreateImageGLES2(m_eglTestCtx, EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR, GL_RGBA));

		static const GLenum rboStorages[] =
		{
			GL_DEPTH_COMPONENT16,
			GL_RGBA4,
			GL_RGB5_A1,
			GL_RGB565,
			GL_STENCIL_INDEX8
		};
		for (int storageNdx = 0; storageNdx < DE_LENGTH_OF_ARRAY(rboStorages); storageNdx++)
			addChild(new Image::CreateImageGLES2(m_eglTestCtx, EGL_GL_RENDERBUFFER_KHR, rboStorages[storageNdx]));

		addChild(new Image::ImageTargetGLES2(m_eglTestCtx, GL_TEXTURE_2D));
		addChild(new Image::ImageTargetGLES2(m_eglTestCtx, GL_RENDERBUFFER));
	}
};

} // Image

ImageTests::ImageTests (EglTestContext& eglTestCtx)
	: TestCaseGroup(eglTestCtx, "image", "EGLImage Tests")
{
}

ImageTests::~ImageTests (void)
{
}

void ImageTests::init (void)
{
	addChild(new Image::ApiTests(m_eglTestCtx));
	addChild(new Image::SimpleCreationTests(m_eglTestCtx));
	addChild(new Image::ModifyTests(m_eglTestCtx));
	addChild(new Image::MultiContextRenderTests(m_eglTestCtx));
}

} // egl
} // deqp
