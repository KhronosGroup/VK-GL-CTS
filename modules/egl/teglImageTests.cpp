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

#include "teglImageUtil.hpp"
#include "teglImageFormatTests.hpp"

#include "egluExtensions.hpp"
#include "egluNativeDisplay.hpp"
#include "egluNativeWindow.hpp"
#include "egluNativePixmap.hpp"
#include "egluStrUtil.hpp"
#include "egluUnique.hpp"
#include "egluUtil.hpp"
#include "egluGLUtil.hpp"

#include "gluDefs.hpp"
#include "gluCallLogWrapper.hpp"
#include "gluObjectWrapper.hpp"
#include "gluStrUtil.hpp"

#include "glwDefs.hpp"
#include "glwEnums.hpp"

#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"

#include "deUniquePtr.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>
#include <set>

using tcu::TestLog;

using std::string;
using std::vector;
using std::set;
using std::ostringstream;

using de::MovePtr;
using de::UniquePtr;
using glu::ApiType;
using glu::ContextType;
using glu::Texture;
using eglu::AttribMap;
using eglu::NativeWindow;
using eglu::NativePixmap;
using eglu::UniqueImage;
using eglu::UniqueSurface;
using eglu::ScopedCurrentContext;

using namespace glw;

namespace deqp
{
namespace egl
{

namespace Image
{

#define CHECK_EXTENSION(DPY, EXTNAME) \
	TCU_CHECK_AND_THROW(NotSupportedError, eglu::hasExtension(DPY, EXTNAME), (string("Unsupported extension: ") + EXTNAME).c_str())

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

// \note These macros expect "TestContext m_testCtx" to be defined.
#define CHECK_EXT_CALL_RET(CALL, EXPECT_RETURN_VALUE, EXPECT_ERROR)	checkCallReturn(m_testCtx, #CALL, CALL, (EXPECT_RETURN_VALUE), (EXPECT_ERROR))
#define CHECK_EXT_CALL_ERR(CALL, EXPECT_ERROR)						checkCallError(m_testCtx, #CALL, CALL, (EXPECT_ERROR))

class ImageTestCase : public TestCase, public glu::CallLogWrapper
{
public:
				ImageTestCase		(EglTestContext& eglTestCtx, ApiType api, const string& name, const string& desc)
					: TestCase				(eglTestCtx, name.c_str(), desc.c_str())
					, glu::CallLogWrapper	(m_gl, m_testCtx.getLog())
					, m_api					(api) {}

	void		init				(void)
	{
		m_eglTestCtx.getGLFunctions(m_gl, m_api, vector<const char*>(1, "GL_OES_EGL_image"));
		m_imageExt = de::newMovePtr<eglu::ImageFunctions>(eglu::getImageFunctions(m_eglTestCtx.getEGLDisplay()));
	}

protected:
	EGLImageKHR	eglCreateImageKHR	(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attribList)
	{
		return m_imageExt->createImage(dpy, ctx, target, buffer, attribList);
	}

	EGLBoolean	eglDestroyImageKHR	(EGLDisplay dpy, EGLImageKHR image)
	{
		return m_imageExt->destroyImage(dpy, image);
	}

	glw::Functions					m_gl;
	MovePtr<eglu::ImageFunctions>	m_imageExt;
	ApiType							m_api;
};

class InvalidCreateImage : public ImageTestCase
{
public:
	InvalidCreateImage (EglTestContext& eglTestCtx)
		: ImageTestCase(eglTestCtx, ApiType::es(2, 0), "invalid_create_image", "eglCreateImageKHR() with invalid arguments")
	{
	}

	void checkCreate (const char* desc, EGLDisplay dpy, const char* dpyStr, EGLContext context, const char* ctxStr, EGLenum source, const char* srcStr, EGLint expectError);

	IterateResult iterate (void)
	{
		const EGLDisplay			dpy 		= m_eglTestCtx.getEGLDisplay();

#define CHECK_CREATE(MSG, DPY, CONTEXT, SOURCE, ERR) checkCreate(MSG, DPY, #DPY, CONTEXT, #CONTEXT, SOURCE, #SOURCE, ERR)
		CHECK_CREATE("Testing bad display (-1)...", (EGLDisplay)-1, EGL_NO_CONTEXT, EGL_NONE, EGL_BAD_DISPLAY);
		CHECK_CREATE("Testing bad context (-1)...", dpy, (EGLContext)-1, EGL_NONE, EGL_BAD_CONTEXT);
		CHECK_CREATE("Testing bad source (-1)...", dpy, EGL_NO_CONTEXT, (EGLenum)-1, EGL_BAD_PARAMETER);
#undef CHECK_CREATE

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}

};

void InvalidCreateImage::checkCreate (const char* msg, EGLDisplay dpy, const char* dpyStr, EGLContext context, const char* ctxStr, EGLenum source, const char* srcStr, EGLint expectError)
{
	m_testCtx.getLog() << TestLog::Message << msg << TestLog::EndMessage;
	{
		const EGLImageKHR	image = m_imageExt->createImage(dpy, context, source, 0, DE_NULL);
		ostringstream		call;

		call << "eglCreateImage(" << dpyStr << ", " << ctxStr << ", " << srcStr << ", 0, DE_NULL)";
		checkCallReturn(m_testCtx, call.str().c_str(), image, EGL_NO_IMAGE_KHR, expectError);
	}
}



EGLConfig chooseConfig (EGLDisplay display, ApiType apiType)
{
	AttribMap				attribs;
	vector<EGLConfig>		configs;
	// Prefer configs in order: pbuffer, window, pixmap
	static const EGLenum	s_surfaceTypes[] = { EGL_PBUFFER_BIT, EGL_WINDOW_BIT, EGL_PIXMAP_BIT };

	attribs[EGL_RENDERABLE_TYPE] = eglu::apiRenderableType(apiType);

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_surfaceTypes); ++ndx)
	{
		attribs[EGL_SURFACE_TYPE] = s_surfaceTypes[ndx];
		configs = eglu::chooseConfig(display, attribs);

		if (!configs.empty())
			return configs.front();
	}

	TCU_THROW(NotSupportedError, "No compatible EGL configs found");
	return (EGLConfig)0;
}

class Context
{
public:
								Context (EglTestContext& eglTestCtx, ContextType ctxType, int width, int height)
									: m_eglTestCtx	(eglTestCtx)
									, m_config		(chooseConfig(eglTestCtx.getEGLDisplay(), ctxType.getAPI()))
									, m_context		(eglu::createGLContext(eglTestCtx.getEGLDisplay(), m_config, ctxType))
									, m_surface		(createSurface(eglTestCtx, m_config, width, height))
									, m_current		(eglTestCtx.getEGLDisplay(), m_surface->get(), m_surface->get(), m_context)
	{
		m_eglTestCtx.getGLFunctions(m_gl, ctxType.getAPI());
	}

	EGLConfig					getConfig		(void) const { return m_config; }
	EGLDisplay 					getEglDisplay	(void) const { return m_eglTestCtx.getEGLDisplay(); }
	EGLContext 					getEglContext	(void) const { return m_context; }
	const glw::Functions&		gl				(void) const { return m_gl; }

private:
	EglTestContext&				m_eglTestCtx;
	EGLConfig					m_config;
	EGLContext					m_context;
	UniquePtr<ManagedSurface>	m_surface;
	ScopedCurrentContext		m_current;
	glw::Functions				m_gl;

								Context			(const Context&);
	Context&					operator=		(const Context&);
};

class CreateImageGLES2 : public ImageTestCase
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

	MovePtr<ImageSource> getImageSource (EGLint target, GLenum format)
	{
		switch (target)
		{
			case EGL_GL_TEXTURE_2D_KHR:
			case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR:
			case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR:
			case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR:
			case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR:
			case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR:
			case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR:
				return createTextureImageSource(target, format, GL_UNSIGNED_BYTE);
			case EGL_GL_RENDERBUFFER_KHR:
				return createRenderbufferImageSource(format);
			default:
				DE_ASSERT(!"Impossible");
				return MovePtr<ImageSource>();
		}
	}

	CreateImageGLES2 (EglTestContext& eglTestCtx, EGLint target, GLenum storage, bool useTexLevel0 = false)
		: ImageTestCase		(eglTestCtx, ApiType::es(2, 0), string("create_image_gles2_") + getTargetName(target) + "_" + getStorageName(storage) + (useTexLevel0 ? "_level0_only" : ""), "Create EGLImage from GLES2 object")
		, m_source			(getImageSource(target, storage))
	{
	}

	IterateResult iterate (void)
	{
		const EGLDisplay	dpy			= m_eglTestCtx.getEGLDisplay();

		if (eglu::getVersion(dpy) < eglu::Version(1, 5))
			CHECK_EXTENSION(dpy, m_source->getRequiredExtension());

		// Initialize result.
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		// Create GLES2 context
		TestLog&				log				= m_testCtx.getLog();
		const ContextType		contextType		(ApiType::es(2, 0));
		Context					context			(m_eglTestCtx, contextType, 64, 64);
		const EGLContext		eglContext		= context.getEglContext();

		log << TestLog::Message << "Using EGL config " << eglu::getConfigID(dpy, context.getConfig()) << TestLog::EndMessage;

		UniquePtr<ClientBuffer>	clientBuffer	(m_source->createBuffer(context.gl()));
		const EGLImageKHR		image			= m_source->createImage(*m_imageExt, dpy, eglContext, clientBuffer->get());

		if (image == EGL_NO_IMAGE_KHR)
		{
			log << TestLog::Message << "  Fail: Got EGL_NO_IMAGE_KHR!" << TestLog::EndMessage;

			if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got EGL_NO_IMAGE_KHR");
		}

		// Destroy image
		CHECK_EXT_CALL_RET(eglDestroyImageKHR(context.getEglDisplay(), image), (EGLBoolean)EGL_TRUE, EGL_SUCCESS);

		return STOP;
	}

private:
	UniquePtr<ImageSource>	m_source;
	bool					m_useTexLevel0;
};

class ImageTargetGLES2 : public ImageTestCase
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
		: ImageTestCase	(eglTestCtx, ApiType::es(2, 0), string("image_target_gles2_") + getTargetName(target), "Use EGLImage as GLES2 object")
		, m_target		(target)
	{
	}

	IterateResult iterate (void)
	{
		TestLog&		log	= m_testCtx.getLog();

		// \todo [2011-07-21 pyry] Try all possible EGLImage sources
		CHECK_EXTENSION(m_eglTestCtx.getEGLDisplay(), "EGL_KHR_gl_texture_2D_image");

		// Initialize result.
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		// Create GLES2 context

		Context context(m_eglTestCtx, ContextType(ApiType::es(2, 0)), 64, 64);
		log << TestLog::Message << "Using EGL config " << eglu::getConfigID(context.getEglDisplay(), context.getConfig()) << TestLog::EndMessage;

		// Check for OES_EGL_image
		{
			const char* glExt = (const char*)glGetString(GL_EXTENSIONS);

			if (string(glExt).find("GL_OES_EGL_image") == string::npos)
				throw tcu::NotSupportedError("Extension not supported", "GL_OES_EGL_image", __FILE__, __LINE__);

			TCU_CHECK(m_gl.eglImageTargetTexture2DOES);
			TCU_CHECK(m_gl.eglImageTargetRenderbufferStorageOES);
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
			GLU_CHECK_CALL(glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)image));
			GLU_CHECK_CALL(glDeleteTextures(1, &dstTex));
		}
		else
		{
			DE_ASSERT(m_target == GL_RENDERBUFFER);

			log << TestLog::Message << "Creating GL_RENDERBUFFER from EGLimage" << TestLog::EndMessage;

			deUint32 dstRbo = 2;
			GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, dstRbo));
			GLU_CHECK_CALL(glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, (GLeglImageOES)image));
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
	ApiTests (EglTestContext& eglTestCtx, const string& name, const string& desc) : TestCaseGroup(eglTestCtx, name.c_str(), desc.c_str()) {}

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
	addChild(new Image::ApiTests(m_eglTestCtx, "api", "EGLImage API tests"));
	addChild(Image::createSimpleCreationTests(m_eglTestCtx, "create", "EGLImage creation tests"));
	addChild(Image::createModifyTests(m_eglTestCtx, "modify", "EGLImage modifying tests"));
	addChild(Image::createMultiContextRenderTests(m_eglTestCtx, "render_multiple_contexts", "EGLImage render tests on multiple contexts"));
}

} // egl
} // deqp
