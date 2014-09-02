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
#include "teglImageFormatTests.hpp"

#include "tcuTestLog.hpp"
#include "tcuSurface.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuCommandLine.hpp"

#include "egluNativeDisplay.hpp"
#include "egluNativeWindow.hpp"
#include "egluNativePixmap.hpp"
#include "egluConfigFilter.hpp"
#include "egluUtil.hpp"

#include "gluTexture.hpp"
#include "gluPixelTransfer.hpp"
#include "gluTextureUtil.hpp"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/eglext.h>

#include <vector>
#include <string>
#include <set>

using std::vector;
using std::set;
using std::string;

namespace deqp
{
namespace egl
{

namespace
{

// \todo [2013-04-09 pyry] Use glu::Program
class Program
{
public:
	Program (const char* vertexSource, const char* fragmentSource)
		: m_program			(0)
		, m_vertexShader	(0)
		, m_fragmentShader	(0)
		, m_isOk			(false)
	{
		m_program			= glCreateProgram();
		m_vertexShader		= glCreateShader(GL_VERTEX_SHADER);
		m_fragmentShader	= glCreateShader(GL_FRAGMENT_SHADER);

		try
		{
			bool	vertexCompileOk		= false;
			bool	fragmentCompileOk	= false;
			bool	linkOk				= false;

			for (int ndx = 0; ndx < 2; ndx++)
			{
				const char*		source			= ndx ? fragmentSource		: vertexSource;
				const deUint32	shader			= ndx ? m_fragmentShader	: m_vertexShader;
				int				compileStatus	= 0;
				bool&			compileOk		= ndx ? fragmentCompileOk	: vertexCompileOk;

				glShaderSource(shader, 1, &source, DE_NULL);
				glCompileShader(shader);
				glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);

				compileOk = (compileStatus == GL_TRUE);
			}

			if (vertexCompileOk && fragmentCompileOk)
			{
				int linkStatus = 0;

				glAttachShader(m_program, m_vertexShader);
				glAttachShader(m_program, m_fragmentShader);
				glLinkProgram(m_program);
				glGetProgramiv(m_program, GL_LINK_STATUS, &linkStatus);

				linkOk = (linkStatus == GL_TRUE);
			}

			m_isOk = linkOk;
		}
		catch (...)
		{
			glDeleteShader(m_vertexShader);
			glDeleteShader(m_fragmentShader);
			glDeleteProgram(m_program);
			throw;
		}
	}

	~Program (void)
	{
		glDeleteShader(m_vertexShader);
		glDeleteShader(m_fragmentShader);
		glDeleteProgram(m_program);
	}

	bool			isOk			(void) const { return m_isOk;	}
	deUint32		getProgram		(void) const {return m_program;	}

private:
	deUint32		m_program;
	deUint32		m_vertexShader;
	deUint32		m_fragmentShader;
	bool			m_isOk;
};

} // anonymous

namespace Image
{

class EglExt
{
public:
	EglExt (void)
	{
		eglCreateImageKHR						= (PFNEGLCREATEIMAGEKHRPROC)						eglGetProcAddress("eglCreateImageKHR");
		eglDestroyImageKHR						= (PFNEGLDESTROYIMAGEKHRPROC)						eglGetProcAddress("eglDestroyImageKHR");

		glEGLImageTargetTexture2DOES			= (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)				eglGetProcAddress("glEGLImageTargetTexture2DOES");
		glEGLImageTargetRenderbufferStorageOES	= (PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC)	eglGetProcAddress("glEGLImageTargetRenderbufferStorageOES");
	}

	PFNEGLCREATEIMAGEKHRPROC						eglCreateImageKHR;
	PFNEGLDESTROYIMAGEKHRPROC						eglDestroyImageKHR;

	PFNGLEGLIMAGETARGETTEXTURE2DOESPROC				glEGLImageTargetTexture2DOES;
	PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC	glEGLImageTargetRenderbufferStorageOES;

private:
};

struct TestSpec
{
	std::string name;
	std::string desc;

	enum ApiContext
	{
		API_GLES2 = 0,
		//API_VG
		//API_GLES1

		API_LAST
	};

	struct Operation
	{
		enum Type
		{
				TYPE_CREATE = 0,
				TYPE_RENDER,
				TYPE_MODIFY,

				TYPE_LAST
		};

		ApiContext	requiredApi;
		int			apiIndex;
		Type		type;
		int			operationIndex;
	};

	vector<ApiContext>	contexts;
	vector<Operation>	operations;

};

class ImageApi
{
public:
							ImageApi				(int contextId, tcu::TestLog& log, tcu::egl::Display& display, tcu::egl::Surface* surface);
	virtual 				~ImageApi				(void) {}

	virtual EGLImageKHR		create					(int operationNdx, tcu::Texture2D& ref) = 0;
	virtual bool			render					(int operationNdx, EGLImageKHR img, const tcu::Texture2D& reference) = 0;
	virtual void			modify					(int operationNdx, EGLImageKHR img, tcu::Texture2D& reference) = 0;

	virtual void			checkRequiredExtensions	(set<string>& extensions, TestSpec::Operation::Type type, int operationNdx) = 0;

protected:
	int						m_contextId;
	tcu::TestLog&			m_log;
	tcu::egl::Display&		m_display;
	tcu::egl::Surface*		m_surface;
};

ImageApi::ImageApi (int contextId, tcu::TestLog& log, tcu::egl::Display& display, tcu::egl::Surface* surface)
	: m_contextId		(contextId)
	, m_log				(log)
	, m_display			(display)
	, m_surface			(surface)
{
}

class GLES2ImageApi : public ImageApi
{
public:
	enum Create
	{
		CREATE_TEXTURE2D_RGB8 = 0,
		CREATE_TEXTURE2D_RGB565,
		CREATE_TEXTURE2D_RGBA8,
		CREATE_TEXTURE2D_RGBA5_A1,
		CREATE_TEXTURE2D_RGBA4,

		CREATE_CUBE_MAP_POSITIVE_X_RGBA8,
		CREATE_CUBE_MAP_NEGATIVE_X_RGBA8,
		CREATE_CUBE_MAP_POSITIVE_Y_RGBA8,
		CREATE_CUBE_MAP_NEGATIVE_Y_RGBA8,
		CREATE_CUBE_MAP_POSITIVE_Z_RGBA8,
		CREATE_CUBE_MAP_NEGATIVE_Z_RGBA8,

		CREATE_CUBE_MAP_POSITIVE_X_RGB8,
		CREATE_CUBE_MAP_NEGATIVE_X_RGB8,
		CREATE_CUBE_MAP_POSITIVE_Y_RGB8,
		CREATE_CUBE_MAP_NEGATIVE_Y_RGB8,
		CREATE_CUBE_MAP_POSITIVE_Z_RGB8,
		CREATE_CUBE_MAP_NEGATIVE_Z_RGB8,

		CREATE_RENDER_BUFFER_DEPTH16,
		CREATE_RENDER_BUFFER_RGBA4,
		CREATE_RENDER_BUFFER_RGB5_A1,
		CREATE_RENDER_BUFFER_RGB565,
		CREATE_RENDER_BUFFER_STENCIL,

		CREATE_LAST
	};

	enum Render
	{
		RENDER_TEXTURE2D = 0,

		// \note Not supported
		RENDER_CUBE_MAP_POSITIVE_X,
		RENDER_CUBE_MAP_NEGATIVE_X,
		RENDER_CUBE_MAP_POSITIVE_Y,
		RENDER_CUBE_MAP_NEGATIVE_Y,
		RENDER_CUBE_MAP_POSITIVE_Z,
		RENDER_CUBE_MAP_NEGATIVE_Z,

		RENDER_READ_PIXELS_RENDERBUFFER,
		RENDER_DEPTHBUFFER,

		RENDER_TRY_ALL,

		RENDER_LAST
	};

	enum Modify
	{
		MODIFY_TEXSUBIMAGE_RGBA8,
		MODIFY_TEXSUBIMAGE_RGBA5_A1,
		MODIFY_TEXSUBIMAGE_RGBA4,
		MODIFY_TEXSUBIMAGE_RGB8,
		MODIFY_TEXSUBIMAGE_RGB565,
		MODIFY_RENDERBUFFER_CLEAR_COLOR,
		MODIFY_RENDERBUFFER_CLEAR_DEPTH,
		MODIFY_RENDERBUFFER_CLEAR_STENCIL,

		MODIFY_LAST
	};

					GLES2ImageApi					(int contextId, tcu::TestLog& log, tcu::egl::Display& display, tcu::egl::Surface* surface, EGLConfig config);
					~GLES2ImageApi					(void);

	EGLImageKHR		create							(int operationNdx, tcu::Texture2D& ref);
	EGLImageKHR		createTexture2D					(tcu::Texture2D& ref, GLenum target, GLenum format, GLenum type);
	EGLImageKHR		createRenderBuffer				(tcu::Texture2D& ref, GLenum type);

	bool			render							(int operationNdx, EGLImageKHR img, const tcu::Texture2D& reference);
	bool			renderTexture2D					(EGLImageKHR img, const tcu::Texture2D& reference);
	bool			renderDepth						(EGLImageKHR img, const tcu::Texture2D& reference);
	bool			renderReadPixelsRenderBuffer	(EGLImageKHR img, const tcu::Texture2D& reference);
	bool			renderTryAll					(EGLImageKHR img, const tcu::Texture2D& reference);

	// \note Not supported
	bool			renderCubeMap					(EGLImageKHR img, const tcu::Surface& reference, GLenum face);

	void			modify							(int operationNdx, EGLImageKHR img, tcu::Texture2D& reference);
	void			modifyTexSubImage				(EGLImageKHR img, tcu::Texture2D& reference, GLenum format, GLenum type);
	void			modifyRenderbufferClearColor	(EGLImageKHR img, tcu::Texture2D& reference);
	void			modifyRenderbufferClearDepth	(EGLImageKHR img, tcu::Texture2D& reference);
	void			modifyRenderbufferClearStencil	(EGLImageKHR img, tcu::Texture2D& reference);

	void			checkRequiredExtensions			(set<string>& extensions, TestSpec::Operation::Type type, int operationNdx);

private:
	tcu::egl::Context*	m_context;
	EglExt				m_eglExt;
};

GLES2ImageApi::GLES2ImageApi (int contextId, tcu::TestLog& log, tcu::egl::Display& display, tcu::egl::Surface* surface, EGLConfig config)
	: ImageApi	(contextId, log, display, surface)
	, m_context	(DE_NULL)
{
	EGLint attriblist[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	EGLint configId = -1;
	TCU_CHECK_EGL_CALL(eglGetConfigAttrib(m_display.getEGLDisplay(), config, EGL_CONFIG_ID, &configId));
	m_log << tcu::TestLog::Message << "Creating gles2 context with config id: " << configId << " context: " << m_contextId << tcu::TestLog::EndMessage;
	m_context = new tcu::egl::Context(m_display, config, attriblist, EGL_OPENGL_ES_API);
	TCU_CHECK_EGL_MSG("Failed to create GLES2 context");

	m_context->makeCurrent(*m_surface, *m_surface);
	TCU_CHECK_EGL_MSG("Failed to make context current");
}

GLES2ImageApi::~GLES2ImageApi (void)
{
	delete m_context;
}

EGLImageKHR GLES2ImageApi::create (int operationNdx, tcu::Texture2D& ref)
{
	m_context->makeCurrent(*m_surface, *m_surface);
	EGLImageKHR	img = EGL_NO_IMAGE_KHR;
	switch (operationNdx)
	{
		case CREATE_TEXTURE2D_RGB8:				img = createTexture2D(ref, GL_TEXTURE_2D, GL_RGB,	GL_UNSIGNED_BYTE);			break;
		case CREATE_TEXTURE2D_RGB565:			img = createTexture2D(ref, GL_TEXTURE_2D, GL_RGB,	GL_UNSIGNED_SHORT_5_6_5);	break;
		case CREATE_TEXTURE2D_RGBA8:			img = createTexture2D(ref, GL_TEXTURE_2D, GL_RGBA,	GL_UNSIGNED_BYTE);			break;
		case CREATE_TEXTURE2D_RGBA4:			img = createTexture2D(ref, GL_TEXTURE_2D, GL_RGBA,	GL_UNSIGNED_SHORT_4_4_4_4);	break;
		case CREATE_TEXTURE2D_RGBA5_A1:			img = createTexture2D(ref, GL_TEXTURE_2D, GL_RGBA,	GL_UNSIGNED_SHORT_5_5_5_1);	break;

		case CREATE_CUBE_MAP_POSITIVE_X_RGBA8:	img = createTexture2D(ref, GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_RGBA, GL_UNSIGNED_BYTE); break;
		case CREATE_CUBE_MAP_NEGATIVE_X_RGBA8:	img = createTexture2D(ref, GL_TEXTURE_CUBE_MAP_NEGATIVE_X, GL_RGBA, GL_UNSIGNED_BYTE); break;
		case CREATE_CUBE_MAP_POSITIVE_Y_RGBA8:	img = createTexture2D(ref, GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_RGBA, GL_UNSIGNED_BYTE); break;
		case CREATE_CUBE_MAP_NEGATIVE_Y_RGBA8:	img = createTexture2D(ref, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, GL_RGBA, GL_UNSIGNED_BYTE); break;
		case CREATE_CUBE_MAP_POSITIVE_Z_RGBA8:	img = createTexture2D(ref, GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_RGBA, GL_UNSIGNED_BYTE); break;
		case CREATE_CUBE_MAP_NEGATIVE_Z_RGBA8:	img = createTexture2D(ref, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, GL_RGBA, GL_UNSIGNED_BYTE); break;

		case CREATE_CUBE_MAP_POSITIVE_X_RGB8:	img = createTexture2D(ref, GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_RGB, GL_UNSIGNED_BYTE); break;
		case CREATE_CUBE_MAP_NEGATIVE_X_RGB8:	img = createTexture2D(ref, GL_TEXTURE_CUBE_MAP_NEGATIVE_X, GL_RGB, GL_UNSIGNED_BYTE); break;
		case CREATE_CUBE_MAP_POSITIVE_Y_RGB8:	img = createTexture2D(ref, GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_RGB, GL_UNSIGNED_BYTE); break;
		case CREATE_CUBE_MAP_NEGATIVE_Y_RGB8:	img = createTexture2D(ref, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, GL_RGB, GL_UNSIGNED_BYTE); break;
		case CREATE_CUBE_MAP_POSITIVE_Z_RGB8:	img = createTexture2D(ref, GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_RGB, GL_UNSIGNED_BYTE); break;
		case CREATE_CUBE_MAP_NEGATIVE_Z_RGB8:	img = createTexture2D(ref, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, GL_RGB, GL_UNSIGNED_BYTE); break;

		case CREATE_RENDER_BUFFER_DEPTH16:		img = createRenderBuffer(ref, GL_DEPTH_COMPONENT16);	break;
		case CREATE_RENDER_BUFFER_RGBA4:		img = createRenderBuffer(ref, GL_RGBA4);				break;
		case CREATE_RENDER_BUFFER_RGB5_A1:		img = createRenderBuffer(ref, GL_RGB5_A1);				break;
		case CREATE_RENDER_BUFFER_RGB565:		img = createRenderBuffer(ref, GL_RGB565);				break;
		case CREATE_RENDER_BUFFER_STENCIL:		img = createRenderBuffer(ref, GL_STENCIL_INDEX8);		break;

		default:
			DE_ASSERT(false);
			break;
	}

	return img;
}

namespace
{

const char* glTargetToString (GLenum target)
{
	switch (target)
	{
		case GL_TEXTURE_2D: return "GL_TEXTURE_2D";
		break;
		case GL_TEXTURE_CUBE_MAP_POSITIVE_X: return "GL_TEXTURE_CUBE_MAP_POSITIVE_X";
		break;
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_X: return "GL_TEXTURE_CUBE_MAP_NEGATIVE_X";
		break;
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Y: return "GL_TEXTURE_CUBE_MAP_POSITIVE_Y";
		break;
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y: return "GL_TEXTURE_CUBE_MAP_NEGATIVE_Y";
		break;
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Z: return "GL_TEXTURE_CUBE_MAP_POSITIVE_Z";
		break;
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z: return "GL_TEXTURE_CUBE_MAP_NEGATIVE_Z";
		break;
		default:
			DE_ASSERT(false);
		break;
	};
	return "";
}

const char* glFormatToString (GLenum format)
{
	switch (format)
	{
		case GL_RGB:
			return "GL_RGB";

		case GL_RGBA:
			return "GL_RGBA";

		default:
			DE_ASSERT(false);
			return "";
	}
}

const char* glTypeToString (GLenum type)
{
	switch (type)
	{
		case GL_UNSIGNED_BYTE:
			return "GL_UNSIGNED_BYTE";

		case GL_UNSIGNED_SHORT_5_6_5:
			return "GL_UNSIGNED_SHORT_5_6_5";

		case GL_UNSIGNED_SHORT_4_4_4_4:
			return "GL_UNSIGNED_SHORT_4_4_4_4";

		case GL_UNSIGNED_SHORT_5_5_5_1:
			return "GL_UNSIGNED_SHORT_5_5_5_1";

		default:
			DE_ASSERT(false);
			return "";
	}
}

} // anonymous

EGLImageKHR	GLES2ImageApi::createTexture2D (tcu::Texture2D& reference, GLenum target, GLenum format, GLenum type)
{
	tcu::Texture2D src(glu::mapGLTransferFormat(format, type), 64, 64);
	src.allocLevel(0);

	tcu::fillWithComponentGradients(src.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
	m_log << tcu::TestLog::Message << "Creating EGLImage from " << glTargetToString(target) << " " << glFormatToString(format) << " " << glTypeToString(type) << " in context: " << m_contextId << tcu::TestLog::EndMessage;

	deUint32 srcTex = 0;
	glGenTextures(1, &srcTex);
	TCU_CHECK(srcTex != 0);
	if (GL_TEXTURE_2D == target)
	{
		GLU_CHECK_CALL(glBindTexture(GL_TEXTURE_2D, srcTex));
		GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
		GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
		GLU_CHECK_CALL(glTexImage2D(GL_TEXTURE_2D, 0, format, src.getWidth(), src.getHeight(), 0, format, type, src.getLevel(0).getDataPtr()));
	}
	else
	{
		GLU_CHECK_CALL(glBindTexture(GL_TEXTURE_CUBE_MAP, srcTex));
		GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
		GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));

		// First fill all faces, required by eglCreateImageKHR
		GLU_CHECK_CALL(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, format, src.getWidth(), src.getHeight(), 0, format, type, 0));
		GLU_CHECK_CALL(glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, format, src.getWidth(), src.getHeight(), 0, format, type, 0));
		GLU_CHECK_CALL(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, format, src.getWidth(), src.getHeight(), 0, format, type, 0));
		GLU_CHECK_CALL(glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, format, src.getWidth(), src.getHeight(), 0, format, type, 0));
		GLU_CHECK_CALL(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, format, src.getWidth(), src.getHeight(), 0, format, type, 0));
		GLU_CHECK_CALL(glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, format, src.getWidth(), src.getHeight(), 0, format, type, 0));
		GLU_CHECK_CALL(glTexImage2D(target, 0, format, src.getWidth(), src.getHeight(), 0, format, type, src.getLevel(0).getDataPtr()));
	}

	EGLint attrib[] = {
		EGL_GL_TEXTURE_LEVEL_KHR, 0,
		EGL_NONE
	};

	EGLImageKHR img = EGL_NO_IMAGE_KHR;

	if (GL_TEXTURE_2D == target)
	{
		img = m_eglExt.eglCreateImageKHR(m_display.getEGLDisplay(), m_context->getEGLContext(), EGL_GL_TEXTURE_2D_KHR, (EGLClientBuffer)(deUintptr)srcTex, attrib);
	}
	else
	{
		switch (target)
		{
			case GL_TEXTURE_CUBE_MAP_POSITIVE_X: img = m_eglExt.eglCreateImageKHR(m_display.getEGLDisplay(), m_context->getEGLContext(), EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR, (EGLClientBuffer)(deUintptr)srcTex, attrib); break;
			case GL_TEXTURE_CUBE_MAP_NEGATIVE_X: img = m_eglExt.eglCreateImageKHR(m_display.getEGLDisplay(), m_context->getEGLContext(), EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR, (EGLClientBuffer)(deUintptr)srcTex, attrib); break;
			case GL_TEXTURE_CUBE_MAP_POSITIVE_Y: img = m_eglExt.eglCreateImageKHR(m_display.getEGLDisplay(), m_context->getEGLContext(), EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR, (EGLClientBuffer)(deUintptr)srcTex, attrib); break;
			case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y: img = m_eglExt.eglCreateImageKHR(m_display.getEGLDisplay(), m_context->getEGLContext(), EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR, (EGLClientBuffer)(deUintptr)srcTex, attrib); break;
			case GL_TEXTURE_CUBE_MAP_POSITIVE_Z: img = m_eglExt.eglCreateImageKHR(m_display.getEGLDisplay(), m_context->getEGLContext(), EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR, (EGLClientBuffer)(deUintptr)srcTex, attrib); break;
			case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z: img = m_eglExt.eglCreateImageKHR(m_display.getEGLDisplay(), m_context->getEGLContext(), EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR, (EGLClientBuffer)(deUintptr)srcTex, attrib); break;

			default:
				DE_ASSERT(false);
				break;
		}
	}

	GLU_CHECK_CALL(glDeleteTextures(1, &srcTex));
	TCU_CHECK_EGL_MSG("Failed to create EGLImage");
	TCU_CHECK_MSG(img != EGL_NO_IMAGE_KHR, "Failed to create EGLImage, got EGL_NO_IMAGE_KHR");
	glBindTexture(GL_TEXTURE_2D, 0);

	reference = src;

	return img;
}

static std::string glRenderbufferTargetToString (GLenum format)
{
	switch (format)
	{
		case GL_RGBA4:
			return "GL_RGBA4";
			break;

		case GL_RGB5_A1:
			return "GL_RGB5_A1";
			break;

		case GL_RGB565:
			return "GL_RGB565";
			break;

		case GL_DEPTH_COMPONENT16:
			return "GL_DEPTH_COMPONENT16";
			break;

		case GL_STENCIL_INDEX8:
			return "GL_STENCIL_INDEX8";
			break;

		default:
			DE_ASSERT(false);
			break;
	}

	DE_ASSERT(false);
	return "";
}

EGLImageKHR GLES2ImageApi::createRenderBuffer (tcu::Texture2D& ref, GLenum format)
{
	m_log << tcu::TestLog::Message << "Creating EGLImage from GL_RENDERBUFFER " << glRenderbufferTargetToString(format) << " " << " in context: " << m_contextId << tcu::TestLog::EndMessage;
	GLuint renderBuffer = 1;

	GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, renderBuffer));
	GLU_CHECK_CALL(glRenderbufferStorage(GL_RENDERBUFFER, format, 64, 64));

	GLuint frameBuffer = 1;
	GLU_CHECK_CALL(glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer));

	switch (format)
	{
		case GL_STENCIL_INDEX8:
			GLU_CHECK_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderBuffer));
			TCU_CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
			GLU_CHECK_CALL(glClearStencil(235));
			GLU_CHECK_CALL(glClear(GL_STENCIL_BUFFER_BIT));
			GLU_CHECK_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0));
			ref = tcu::Texture2D(tcu::TextureFormat(tcu::TextureFormat::I, tcu::TextureFormat::UNORM_INT8),  64, 64);
			ref.allocLevel(0);

			for (int x = 0; x < 64; x++)
			{
				for (int y = 0; y < 64; y++)
				{
					ref.getLevel(0).setPixel(tcu::IVec4(235, 235, 235, 235), x, y);
				}
			}
			break;

		case GL_DEPTH_COMPONENT16:
			GLU_CHECK_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderBuffer));
			TCU_CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
			GLU_CHECK_CALL(glClearDepthf(0.5f));
			GLU_CHECK_CALL(glClear(GL_DEPTH_BUFFER_BIT));
			GLU_CHECK_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0));
			ref = tcu::Texture2D(tcu::TextureFormat(tcu::TextureFormat::I, tcu::TextureFormat::UNORM_INT16),  64, 64);
			ref.allocLevel(0);

			for (int x = 0; x < 64; x++)
			{
				for (int y = 0; y < 64; y++)
				{
					ref.getLevel(0).setPixel(tcu::Vec4(0.5f, 0.5f, 0.5f, 0.5f), x, y);
				}
			}
			break;

		case GL_RGBA4:
			GLU_CHECK_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderBuffer));
			TCU_CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
			GLU_CHECK_CALL(glClearColor(0.9f, 0.5f, 0.65f, 1.0f));
			GLU_CHECK_CALL(glClear(GL_COLOR_BUFFER_BIT));
			GLU_CHECK_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0));
			ref = tcu::Texture2D(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_SHORT_4444),  64, 64);
			ref.allocLevel(0);

			for (int x = 0; x < 64; x++)
			{
				for (int y = 0; y < 64; y++)
				{
					ref.getLevel(0).setPixel(tcu::Vec4(0.9f, 0.5f, 0.65f, 1.0f), x, y);
				}
			}
			break;

		case GL_RGB5_A1:
			GLU_CHECK_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderBuffer));
			TCU_CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
			GLU_CHECK_CALL(glClearColor(0.5f, 0.7f, 0.65f, 1.0f));
			GLU_CHECK_CALL(glClear(GL_COLOR_BUFFER_BIT));
			GLU_CHECK_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0));
			ref = tcu::Texture2D(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_SHORT_5551),  64, 64);
			ref.allocLevel(0);

			for (int x = 0; x < 64; x++)
			{
				for (int y = 0; y < 64; y++)
				{
					ref.getLevel(0).setPixel(tcu::Vec4(0.5f, 0.7f, 0.65f, 1.0f), x, y);
				}
			}
			break;

		case GL_RGB565:
			GLU_CHECK_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderBuffer));
			TCU_CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
			GLU_CHECK_CALL(glClearColor(0.2f, 0.5f, 0.65f, 1.0f));
			GLU_CHECK_CALL(glClear(GL_COLOR_BUFFER_BIT));
			GLU_CHECK_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0));
			ref = tcu::Texture2D(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_SHORT_565),  64, 64);
			ref.allocLevel(0);

			for (int x = 0; x < 64; x++)
			{
				for (int y = 0; y < 64; y++)
				{
					ref.getLevel(0).setPixel(tcu::Vec4(0.2f, 0.5f, 0.65f, 1.0f), x, y);
				}
			}
			break;

		default:
			DE_ASSERT(false);
	}

	GLU_CHECK_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
	GLU_CHECK_CALL(glDeleteFramebuffers(1, &frameBuffer));

	EGLint attrib[] = {
		EGL_NONE
	};

	EGLImageKHR img = m_eglExt.eglCreateImageKHR(m_display.getEGLDisplay(), m_context->getEGLContext(), EGL_GL_RENDERBUFFER_KHR, (EGLClientBuffer)(deUintptr)renderBuffer, attrib);

	GLU_CHECK_CALL(glDeleteRenderbuffers(1, &renderBuffer));
	return img;
}

bool GLES2ImageApi::render (int operationNdx, EGLImageKHR img, const tcu::Texture2D& reference)
{
	m_context->makeCurrent(*m_surface, *m_surface);
	switch (operationNdx)
	{
		case RENDER_TEXTURE2D:
			return renderTexture2D(img, reference);

		case RENDER_READ_PIXELS_RENDERBUFFER:
			return renderReadPixelsRenderBuffer(img, reference);

		case RENDER_DEPTHBUFFER:
			return renderDepth(img, reference);

		case RENDER_TRY_ALL:
			return renderTryAll(img, reference);

		default:
			DE_ASSERT(false);
			break;
	};
	return false;
}

bool GLES2ImageApi::renderTexture2D (EGLImageKHR img, const tcu::Texture2D& reference)
{
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glViewport(0, 0, reference.getWidth(), reference.getHeight());
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);

	m_log << tcu::TestLog::Message << "Rendering EGLImage as GL_TEXTURE_2D in context: " << m_contextId << tcu::TestLog::EndMessage;
	TCU_CHECK(img != EGL_NO_IMAGE_KHR);

	deUint32 srcTex = 0;
	glGenTextures(1, &srcTex);
	TCU_CHECK(srcTex != 0);
	GLU_CHECK_CALL(glBindTexture(GL_TEXTURE_2D, srcTex));
	m_eglExt.glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)img);
	GLenum error = glGetError();

	if (error == GL_INVALID_OPERATION)
	{
		GLU_CHECK_CALL(glDeleteTextures(1, &srcTex));
		throw tcu::NotSupportedError("Creating texture2D from EGLImage type not supported", "glEGLImageTargetTexture2DOES", __FILE__, __LINE__);
	}

	TCU_CHECK(error == GL_NONE);

	TCU_CHECK_EGL_MSG("glEGLImageTargetTexture2DOES() failed");

	GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
	GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
	GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));

	const char* vertexShader =
		"attribute highp vec2 a_coord;\n"
		"varying mediump vec2 v_texCoord;\n"
		"void main(void) {\n"
		"\tv_texCoord = vec2((a_coord.x + 1.0) * 0.5, (a_coord.y + 1.0) * 0.5);\n"
		"\tgl_Position = vec4(a_coord, -0.1, 1.0);\n"
		"}\n";

	const char* fragmentShader =
		"varying mediump vec2 v_texCoord;\n"
		"uniform sampler2D u_sampler;\n"
		"void main(void) {\n"
		"\tmediump vec4 texColor = texture2D(u_sampler, v_texCoord);\n"
		"\tgl_FragColor = vec4(texColor);\n"
		"}";

	Program program(vertexShader, fragmentShader);
	TCU_CHECK(program.isOk());

	GLuint glProgram = program.getProgram();
	GLU_CHECK_CALL(glUseProgram(glProgram));

	GLuint coordLoc = glGetAttribLocation(glProgram, "a_coord");
	TCU_CHECK_MSG((int)coordLoc != -1, "Couldn't find attribute a_coord");

	GLuint samplerLoc = glGetUniformLocation(glProgram, "u_sampler");
	TCU_CHECK_MSG((int)samplerLoc != (int)-1, "Couldn't find uniform u_sampler");

	float coords[] =
	{
		-1.0, -1.0,
		1.0, -1.0,
		1.0,  1.0,

		1.0,  1.0,
		-1.0,  1.0,
		-1.0, -1.0
	};

	GLU_CHECK_CALL(glBindTexture(GL_TEXTURE_2D, srcTex));
	GLU_CHECK_CALL(glUniform1i(samplerLoc, 0));
	GLU_CHECK_CALL(glEnableVertexAttribArray(coordLoc));
	GLU_CHECK_CALL(glVertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, coords));

	GLU_CHECK_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
	GLU_CHECK_CALL(glDisableVertexAttribArray(coordLoc));
	GLU_CHECK_CALL(glBindTexture(GL_TEXTURE_2D, 0));
	GLU_CHECK_CALL(glDeleteTextures(1, &srcTex));

	tcu::Surface screen(reference.getWidth(), reference.getHeight());
	glReadPixels(0, 0, screen.getWidth(), screen.getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, screen.getAccess().getDataPtr());
	GLU_CHECK_MSG("glReadPixels()");

	tcu::Surface referenceScreen(reference.getWidth(), reference.getHeight());

	for (int y = 0; y < referenceScreen.getHeight(); y++)
	{
		for (int x = 0; x < referenceScreen.getWidth(); x++)
		{
			tcu::Vec4 src = reference.getLevel(0).getPixel(x, y);
			referenceScreen.setPixel(x, y, tcu::RGBA(src));
		}
	}

	float	threshold	= 0.05f;
	bool	match		= tcu::fuzzyCompare(m_log, "ComparisonResult", "Image comparison result", referenceScreen, screen, threshold, tcu::COMPARE_LOG_RESULT);

	return match;
}

bool GLES2ImageApi::renderDepth (EGLImageKHR img, const tcu::Texture2D& reference)
{
	m_log << tcu::TestLog::Message << "Rendering with depth buffer" << tcu::TestLog::EndMessage;

	deUint32 framebuffer;
	glGenFramebuffers(1, &framebuffer);
	TCU_CHECK(framebuffer != (GLuint)-1);
	GLU_CHECK_CALL(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer));

	deUint32 renderbufferColor = 0;
	glGenRenderbuffers(1, &renderbufferColor);
	TCU_CHECK(renderbufferColor != (GLuint)-1);
	GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, renderbufferColor));
	GLU_CHECK_CALL(glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, reference.getWidth(), reference.getHeight()));
	GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, 0));

	deUint32 renderbufferDepth = 0;
	glGenRenderbuffers(1, &renderbufferDepth);
	TCU_CHECK(renderbufferDepth != (GLuint)-1);
	GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, renderbufferDepth));

	m_eglExt.glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, (GLeglImageOES)img);

	GLenum error = glGetError();

	if (error == GL_INVALID_OPERATION)
	{
		GLU_CHECK_CALL(glDeleteRenderbuffers(1, &renderbufferDepth));
		GLU_CHECK_CALL(glDeleteRenderbuffers(1, &renderbufferColor));
		throw tcu::NotSupportedError("Creating renderbuffer from EGLImage type not supported", "glEGLImageTargetRenderbufferStorageOES", __FILE__, __LINE__);
	}

	TCU_CHECK(error == GL_NONE);

	TCU_CHECK_EGL_MSG("glEGLImageTargetRenderbufferStorageOES() failed");

	GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, renderbufferDepth));
	GLU_CHECK_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderbufferDepth));

	GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, renderbufferColor));
	GLU_CHECK_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbufferColor));
	GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, 0));

	GLU_CHECK_CALL(glViewport(0, 0, reference.getWidth(), reference.getHeight()));
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		GLU_CHECK_CALL(glDeleteFramebuffers(1, &framebuffer));
		GLU_CHECK_CALL(glDeleteRenderbuffers(1, &renderbufferDepth));
		GLU_CHECK_CALL(glDeleteRenderbuffers(1, &renderbufferColor));
		throw tcu::NotSupportedError("EGLImage as depth attachment not supported", "", __FILE__, __LINE__);
	}

	// Render
	const char* vertexShader =
		"attribute highp vec2 a_coord;\n"
		"uniform highp float u_depth;\n"
		"void main(void) {\n"
		"\tgl_Position = vec4(a_coord, u_depth, 1.0);\n"
		"}\n";

	const char* fragmentShader =
		"uniform mediump vec4 u_color;\n"
		"void main(void) {\n"
		"\tgl_FragColor = u_color;\n"
		"}";

	Program program(vertexShader, fragmentShader);
	TCU_CHECK(program.isOk());

	GLuint glProgram = program.getProgram();
	GLU_CHECK_CALL(glUseProgram(glProgram));

	GLuint coordLoc = glGetAttribLocation(glProgram, "a_coord");
	TCU_CHECK_MSG((int)coordLoc != -1, "Couldn't find attribute a_coord");

	GLuint colorLoc = glGetUniformLocation(glProgram, "u_color");
	TCU_CHECK_MSG((int)colorLoc != (int)-1, "Couldn't find uniform u_color");

	GLuint depthLoc = glGetUniformLocation(glProgram, "u_depth");
	TCU_CHECK_MSG((int)depthLoc != (int)-1, "Couldn't find uniform u_depth");

	float coords[] =
	{
		-1.0, -1.0,
		1.0, -1.0,
		1.0,  1.0,

		1.0,  1.0,
		-1.0,  1.0,
		-1.0, -1.0
	};

	float depthLevels[] = {
		0.1f,
		0.2f,
		0.3f,
		0.4f,
		0.5f,
		0.6f,
		0.7f,
		0.8f,
		0.9f,
		1.0f
	};

	tcu::Vec4 depthLevelColors[] = {
		tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
		tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f),
		tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
		tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f),
		tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f),

		tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
		tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f),
		tcu::Vec4(0.5f, 0.0f, 0.0f, 1.0f),
		tcu::Vec4(0.0f, 0.5f, 0.0f, 1.0f),
		tcu::Vec4(0.5f, 0.5f, 0.0f, 1.0f)
	};

	DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(depthLevels) == DE_LENGTH_OF_ARRAY(depthLevelColors));

	GLU_CHECK_CALL(glEnableVertexAttribArray(coordLoc));
	GLU_CHECK_CALL(glVertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, coords));

	GLU_CHECK_CALL(glEnable(GL_DEPTH_TEST));
	GLU_CHECK_CALL(glDepthFunc(GL_LESS));

	for (int level = 0; level < DE_LENGTH_OF_ARRAY(depthLevels); level++)
	{
		tcu::Vec4 color = depthLevelColors[level];
		GLU_CHECK_CALL(glUniform4f(colorLoc, color.x(), color.y(), color.z(), color.w()));
		GLU_CHECK_CALL(glUniform1f(depthLoc, depthLevels[level]));
		GLU_CHECK_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
	}

	GLU_CHECK_CALL(glDisable(GL_DEPTH_TEST));
	GLU_CHECK_CALL(glDisableVertexAttribArray(coordLoc));

	tcu::Surface screen(reference.getWidth(), reference.getHeight());
	tcu::Surface referenceScreen(reference.getWidth(), reference.getHeight());

	glReadPixels(0, 0, screen.getWidth(), screen.getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, screen.getAccess().getDataPtr());

	for (int y = 0; y < reference.getHeight(); y++)
	{
		for (int x = 0; x < reference.getWidth(); x++)
		{
			tcu::RGBA result;
			for (int level = 0; level < DE_LENGTH_OF_ARRAY(depthLevels); level++)
			{
				tcu::Vec4 src = reference.getLevel(0).getPixel(x, y);

				if (src.x() < depthLevels[level])
				{
					result = tcu::RGBA((int)(depthLevelColors[level].x() * 255.0f), (int)(depthLevelColors[level].y() * 255.0f), (int)(depthLevelColors[level].z() * 255.0f), (int)(depthLevelColors[level].w() * 255.0f));
				}
			}

			referenceScreen.setPixel(x, reference.getHeight(), result);
		}
	}

	bool isOk = tcu::pixelThresholdCompare(m_log, "Depth buffer rendering result", "Result from rendering with depth buffer", referenceScreen, screen, tcu::RGBA(1,1,1,1), tcu::COMPARE_LOG_RESULT);

	GLU_CHECK_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
	GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, 0));
	GLU_CHECK_CALL(glDeleteRenderbuffers(1, &renderbufferDepth));
	GLU_CHECK_CALL(glDeleteRenderbuffers(1, &renderbufferColor));
	GLU_CHECK_CALL(glDeleteFramebuffers(1, &framebuffer));
	GLU_CHECK_CALL(glFinish());

	return isOk;
}

bool GLES2ImageApi::renderReadPixelsRenderBuffer (EGLImageKHR img, const tcu::Texture2D& reference)
{
	m_log << tcu::TestLog::Message << "Reading with ReadPixels from renderbuffer" << tcu::TestLog::EndMessage;

	deUint32 framebuffer;
	glGenFramebuffers(1, &framebuffer);
	TCU_CHECK(framebuffer != (GLuint)-1);
	GLU_CHECK_CALL(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer));

	deUint32 renderbuffer = 0;
	glGenRenderbuffers(1, &renderbuffer);
	TCU_CHECK(renderbuffer != (GLuint)-1);
	GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer));

	m_eglExt.glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, (GLeglImageOES)img);

	GLenum error = glGetError();

	if (error == GL_INVALID_OPERATION)
	{
		GLU_CHECK_CALL(glDeleteRenderbuffers(1, &renderbuffer));
		throw tcu::NotSupportedError("Creating renderbuffer from EGLImage type not supported", "glEGLImageTargetRenderbufferStorageOES", __FILE__, __LINE__);
	}

	TCU_CHECK(error == GL_NONE);

	TCU_CHECK_EGL_MSG("glEGLImageTargetRenderbufferStorageOES() failed");

	GLU_CHECK_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer));

	GLU_CHECK_CALL(glViewport(0, 0, reference.getWidth(), reference.getHeight()));
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		GLU_CHECK_CALL(glDeleteFramebuffers(1, &framebuffer));
		GLU_CHECK_CALL(glDeleteRenderbuffers(1, &renderbuffer));
		throw tcu::NotSupportedError("EGLImage as color attachment not supported", "", __FILE__, __LINE__);
	}

	tcu::Surface screen(reference.getWidth(), reference.getHeight());
	tcu::Surface referenceScreen(reference.getWidth(), reference.getHeight());

	glReadPixels(0, 0, screen.getWidth(), screen.getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, screen.getAccess().getDataPtr());

	for (int y = 0; y < reference.getHeight(); y++)
	{
		for (int x = 0; x < reference.getWidth(); x++)
		{
			tcu::Vec4 src = reference.getLevel(0).getPixel(x, y);
			referenceScreen.setPixel(x, y, tcu::RGBA(src));
		}
	}

	bool isOk = tcu::pixelThresholdCompare(m_log, "Renderbuffer read", "Result from reading renderbuffer", referenceScreen, screen, tcu::RGBA(1,1,1,1), tcu::COMPARE_LOG_RESULT);

	GLU_CHECK_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
	GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, 0));
	GLU_CHECK_CALL(glDeleteRenderbuffers(1, &renderbuffer));
	GLU_CHECK_CALL(glDeleteFramebuffers(1, &framebuffer));
	GLU_CHECK_CALL(glFinish());

	return isOk;
}

bool GLES2ImageApi::renderTryAll (EGLImageKHR img, const tcu::Texture2D& reference)
{
	bool isOk = true;
	bool foundSupportedRendering = false;

	try
	{
		if (!renderTexture2D(img, reference))
			isOk = false;

		foundSupportedRendering = true;
	}
	catch (const tcu::NotSupportedError& error)
	{
		m_log << tcu::TestLog::Message << error.what() << tcu::TestLog::EndMessage;
	}

	if (!isOk)
		return false;

	try
	{
		if (!renderReadPixelsRenderBuffer(img, reference))
			isOk = false;

		foundSupportedRendering = true;
	}
	catch (const tcu::NotSupportedError& error)
	{
		m_log << tcu::TestLog::Message << error.what() << tcu::TestLog::EndMessage;
	}

	if (!isOk)
		return false;

	try
	{
		if (!renderDepth(img, reference))
			isOk = false;

		foundSupportedRendering = true;
	}
	catch (const tcu::NotSupportedError& error)
	{
		m_log << tcu::TestLog::Message << error.what() << tcu::TestLog::EndMessage;
	}

	if (!foundSupportedRendering)
		throw tcu::NotSupportedError("Rendering not supported", "", __FILE__, __LINE__);

	return isOk;
}

bool GLES2ImageApi::renderCubeMap (EGLImageKHR img, const tcu::Surface& reference, GLenum face)
{
	// \note This is not supported by EGLImage
	DE_ASSERT(false);

	glClearColor(0.5, 0.5, 0.5, 1.0);
	glViewport(0, 0, reference.getWidth(), reference.getHeight());
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);

	m_log << tcu::TestLog::Message << "Rendering EGLImage as " <<  glTargetToString(face) << " in context: " << m_contextId << tcu::TestLog::EndMessage;
	DE_ASSERT(img != EGL_NO_IMAGE_KHR);

	deUint32 srcTex = 0;
	glGenTextures(1, &srcTex);
	DE_ASSERT(srcTex != 0);
	GLU_CHECK_CALL(glBindTexture(GL_TEXTURE_CUBE_MAP, srcTex));
	GLU_CHECK_CALL(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, reference.getWidth(), reference.getHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, 0));
	GLU_CHECK_CALL(glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, reference.getWidth(), reference.getHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, 0));
	GLU_CHECK_CALL(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA, reference.getWidth(), reference.getHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, 0));
	GLU_CHECK_CALL(glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGBA, reference.getWidth(), reference.getHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, 0));
	GLU_CHECK_CALL(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, reference.getWidth(), reference.getHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, 0));
	GLU_CHECK_CALL(glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGBA, reference.getWidth(), reference.getHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, 0));

	m_eglExt.glEGLImageTargetTexture2DOES(face, (GLeglImageOES)img);
	GLenum error = glGetError();

	if (error == GL_INVALID_OPERATION)
	{
		GLU_CHECK_CALL(glDeleteTextures(1, &srcTex));
		throw tcu::NotSupportedError("Creating texture cubemap from EGLImage type not supported", "glEGLImageTargetTexture2DOES", __FILE__, __LINE__);
	}

	TCU_CHECK(error == GL_NONE);

	TCU_CHECK_EGL_MSG("glEGLImageTargetTexture2DOES() failed");

	GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
	GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
	GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));

	const char* vertexShader =
		"attribute highp vec3 a_coord;\n"
		"attribute highp vec3 a_texCoord;\n"
		"varying mediump vec3 v_texCoord;\n"
		"void main(void) {\n"
		"\tv_texCoord = a_texCoord;\n"
		"\tgl_Position = vec4(a_coord.xy, -0.1, 1.0);\n"
		"}\n";

	const char* fragmentShader =
		"varying mediump vec3 v_texCoord;\n"
		"uniform samplerCube u_sampler;\n"
		"void main(void) {\n"
		"\tmediump vec4 texColor = textureCube(u_sampler, v_texCoord);\n"
		"\tgl_FragColor = vec4(texColor.rgb, 1.0);\n"
		"}";

	Program program(vertexShader, fragmentShader);
	DE_ASSERT(program.isOk());

	GLuint glProgram = program.getProgram();
	GLU_CHECK_CALL(glUseProgram(glProgram));

	GLint coordLoc = glGetAttribLocation(glProgram, "a_coord");
	DE_ASSERT(coordLoc != -1);

	GLint texCoordLoc = glGetAttribLocation(glProgram, "a_texCoord");
	DE_ASSERT(texCoordLoc != -1);

	GLint samplerLoc = glGetUniformLocation(glProgram, "u_sampler");
	DE_ASSERT(samplerLoc != -1);

	float coords[] =
	{
		-1.0, -1.0,
		1.0, -1.0,
		1.0,  1.0,

		1.0,  1.0,
		-1.0,  1.0,
		-1.0, -1.0,
	};

	float sampleTexCoords[] =
	{
		10.0, -1.0, -1.0,
		10.0,  1.0, -1.0,
		10.0,  1.0,  1.0,

		10.0,  1.0,  1.0,
		10.0, -1.0,  1.0,
		10.0, -1.0, -1.0,
	};

	vector<float> texCoords;
	float	sign	= 0.0f;
	int		dir		= -1;

	switch (face)
	{
		case GL_TEXTURE_CUBE_MAP_POSITIVE_X: sign =  1.0; dir = 0; break;
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_X: sign = -1.0; dir = 0; break;
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Y: sign =  1.0; dir = 1; break;
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y: sign = -1.0; dir = 1; break;
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Z: sign =  1.0; dir = 2; break;
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z: sign = -1.0; dir = 2; break;
		default:
				DE_ASSERT(false);
	}

	for (int i = 0; i < 6; i++)
	{
		texCoords.push_back(sign * sampleTexCoords[i*3 + (dir % 3)]);
		texCoords.push_back(sampleTexCoords[i*3 + ((dir + 1) % 3)]);
		texCoords.push_back(sampleTexCoords[i*3 + ((dir + 2) % 3)]);
	}

	GLU_CHECK_CALL(glBindTexture(GL_TEXTURE_CUBE_MAP, srcTex));
	GLU_CHECK_CALL(glUniform1i(samplerLoc, 0));
	GLU_CHECK_CALL(glEnableVertexAttribArray(coordLoc));
	GLU_CHECK_CALL(glEnableVertexAttribArray(texCoordLoc));
	GLU_CHECK_CALL(glVertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, coords));
	GLU_CHECK_CALL(glVertexAttribPointer(texCoordLoc, 3, GL_FLOAT, GL_FALSE, 0, coords));

	GLU_CHECK_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
	GLU_CHECK_CALL(glDisableVertexAttribArray(coordLoc));
	GLU_CHECK_CALL(glDisableVertexAttribArray(texCoordLoc));
	GLU_CHECK_CALL(glBindTexture(GL_TEXTURE_CUBE_MAP, 0));
	GLU_CHECK_CALL(glDeleteTextures(1, &srcTex));

	tcu::Surface screen(reference.getWidth(), reference.getHeight());
	glReadPixels(0, 0, screen.getWidth(), screen.getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, screen.getAccess().getDataPtr());
	GLU_CHECK_MSG("glReadPixels()");

	float	threshold	= 0.05f;
	bool	match		= tcu::fuzzyCompare(m_log, "ComparisonResult", "Image comparison result", reference, screen, threshold, tcu::COMPARE_LOG_RESULT);

	return match;
}

void GLES2ImageApi::modify (int operationNdx, EGLImageKHR img, tcu::Texture2D& reference)
{
	switch (operationNdx)
	{
		case MODIFY_TEXSUBIMAGE_RGBA8:
			modifyTexSubImage(img, reference, GL_RGBA, GL_UNSIGNED_BYTE);
			break;

		case MODIFY_TEXSUBIMAGE_RGBA5_A1:
			modifyTexSubImage(img, reference, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1);
			break;

		case MODIFY_TEXSUBIMAGE_RGBA4:
			modifyTexSubImage(img, reference, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4);
			break;

		case MODIFY_TEXSUBIMAGE_RGB8:
			modifyTexSubImage(img, reference, GL_RGB, GL_UNSIGNED_BYTE);
			break;

		case MODIFY_TEXSUBIMAGE_RGB565:
			modifyTexSubImage(img, reference, GL_RGB, GL_UNSIGNED_SHORT_5_6_5);
			break;

		case MODIFY_RENDERBUFFER_CLEAR_COLOR:
			modifyRenderbufferClearColor(img, reference);
			break;

		case MODIFY_RENDERBUFFER_CLEAR_DEPTH:
			modifyRenderbufferClearDepth(img, reference);
			break;

		case MODIFY_RENDERBUFFER_CLEAR_STENCIL:
			modifyRenderbufferClearStencil(img, reference);
			break;

		default:
			DE_ASSERT(false);
			break;
	}
}

void GLES2ImageApi::modifyTexSubImage (EGLImageKHR img, tcu::Texture2D& reference, GLenum format, GLenum type)
{
	m_log << tcu::TestLog::Message << "Modifying EGLImage with glTexSubImage2D" << tcu::TestLog::EndMessage;

	deUint32 srcTex = 0;
	glGenTextures(1, &srcTex);
	TCU_CHECK(srcTex != 0);
	GLU_CHECK_CALL(glBindTexture(GL_TEXTURE_2D, srcTex));

	m_eglExt.glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)img);

	GLenum error = glGetError();

	if (error == GL_INVALID_OPERATION)
	{
		GLU_CHECK_CALL(glDeleteTextures(1, &srcTex));
		throw tcu::NotSupportedError("Creating texture2D from EGLImage type not supported", "glEGLImageTargetTexture2DOES", __FILE__, __LINE__);
	}
	TCU_CHECK(error == GL_NONE);

	TCU_CHECK_EGL_MSG("glEGLImageTargetTexture2DOES() failed");

	int xOffset = 8;
	int yOffset = 16;

	tcu::Texture2D src(glu::mapGLTransferFormat(format, type), 16, 16);
	src.allocLevel(0);
	tcu::fillWithComponentGradients(src.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

	GLU_CHECK_CALL(glBindTexture(GL_TEXTURE_2D, srcTex));
	GLU_CHECK_CALL(glTexSubImage2D(GL_TEXTURE_2D, 0, xOffset, yOffset, src.getWidth(), src.getHeight(), format, type, src.getLevel(0).getDataPtr()));

	for (int x = 0; x < src.getWidth(); x++)
	{
		if (x + xOffset >= reference.getWidth())
			continue;

		for (int y = 0; y < src.getHeight(); y++)
		{
			if (y + yOffset >= reference.getHeight())
				continue;

			reference.getLevel(0).setPixel(src.getLevel(0).getPixel(x, y), x+xOffset, y+yOffset);
		}
	}

	GLU_CHECK_CALL(glDeleteTextures(1, &srcTex));
	GLU_CHECK_CALL(glFinish());
	GLU_CHECK_CALL(glBindTexture(GL_TEXTURE_2D, 0));
}

void GLES2ImageApi::modifyRenderbufferClearColor (EGLImageKHR img, tcu::Texture2D& reference)
{
	m_log << tcu::TestLog::Message << "Modifying EGLImage with glClear to renderbuffer" << tcu::TestLog::EndMessage;

	deUint32 framebuffer;
	glGenFramebuffers(1, &framebuffer);
	TCU_CHECK(framebuffer != (GLuint)-1);
	GLU_CHECK_CALL(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer));

	deUint32 renderbuffer = 0;
	glGenRenderbuffers(1, &renderbuffer);
	TCU_CHECK(renderbuffer != (GLuint)-1);
	GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer));

	m_eglExt.glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, (GLeglImageOES)img);

	GLenum error = glGetError();

	if (error == GL_INVALID_OPERATION)
	{
		GLU_CHECK_CALL(glDeleteFramebuffers(1, &framebuffer));
		GLU_CHECK_CALL(glDeleteRenderbuffers(1, &renderbuffer));
		throw tcu::NotSupportedError("Creating renderbuffer from EGLImage type not supported", "glEGLImageTargetRenderbufferStorageOES", __FILE__, __LINE__);
	}
	TCU_CHECK(error == GL_NONE);

	TCU_CHECK_EGL_MSG("glEGLImageTargetRenderbufferStorageOES() failed");

	float red	= 0.3f;
	float green	= 0.5f;
	float blue	= 0.3f;
	float alpha	= 1.0f;

	GLU_CHECK_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer));

	GLU_CHECK_CALL(glViewport(0, 0, reference.getWidth(), reference.getHeight()));
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		GLU_CHECK_CALL(glDeleteFramebuffers(1, &framebuffer));
		GLU_CHECK_CALL(glDeleteRenderbuffers(1, &renderbuffer));
		throw tcu::NotSupportedError("EGLImage type as color attachment not supported", "", __FILE__, __LINE__);
	}

	GLU_CHECK_CALL(glClearColor(red, green, blue, alpha));
	GLU_CHECK_CALL(glClear(GL_COLOR_BUFFER_BIT));

	for (int x = 0; x < reference.getWidth(); x++)
	{
		for (int y = 0; y < reference.getHeight(); y++)
		{
			tcu::Vec4 color = tcu::Vec4(red, green, blue, alpha);
			reference.getLevel(0).setPixel(color, x, y);
		}
	}

	GLU_CHECK_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
	GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, 0));
	GLU_CHECK_CALL(glDeleteRenderbuffers(1, &renderbuffer));
	GLU_CHECK_CALL(glDeleteFramebuffers(1, &framebuffer));
	GLU_CHECK_CALL(glFinish());
}

void GLES2ImageApi::modifyRenderbufferClearDepth (EGLImageKHR img, tcu::Texture2D& reference)
{
	m_log << tcu::TestLog::Message << "Modifying EGLImage with glClear to renderbuffer" << tcu::TestLog::EndMessage;

	deUint32 framebuffer;
	glGenFramebuffers(1, &framebuffer);
	TCU_CHECK(framebuffer != (GLuint)-1);
	GLU_CHECK_CALL(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer));

	deUint32 renderbuffer = 0;
	glGenRenderbuffers(1, &renderbuffer);
	TCU_CHECK(renderbuffer != 0);
	GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer));

	m_eglExt.glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, (GLeglImageOES)img);

	GLenum error = glGetError();

	if (error == GL_INVALID_OPERATION)
	{
		GLU_CHECK_CALL(glDeleteFramebuffers(1, &framebuffer));
		GLU_CHECK_CALL(glDeleteRenderbuffers(1, &renderbuffer));
		throw tcu::NotSupportedError("Creating renderbuffer from EGLImage type not supported", "glEGLImageTargetRenderbufferStorageOES", __FILE__, __LINE__);
	}
	TCU_CHECK(error == GL_NONE);

	TCU_CHECK_EGL_MSG("glEGLImageTargetRenderbufferStorageOES() failed");

	float depth = 0.7f;

	GLU_CHECK_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderbuffer));

	GLU_CHECK_CALL(glViewport(0, 0, reference.getWidth(), reference.getHeight()));
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		GLU_CHECK_CALL(glDeleteFramebuffers(1, &framebuffer));
		GLU_CHECK_CALL(glDeleteRenderbuffers(1, &renderbuffer));
		throw tcu::NotSupportedError("EGLImage type as depth attachment not supported", "", __FILE__, __LINE__);
	}

	GLU_CHECK_CALL(glClearDepthf(depth));
	GLU_CHECK_CALL(glClear(GL_DEPTH_BUFFER_BIT));

	for (int x = 0; x < reference.getWidth(); x++)
	{
		for (int y = 0; y < reference.getHeight(); y++)
		{
			tcu::Vec4 color = tcu::Vec4(depth, depth, depth, depth);
			reference.getLevel(0).setPixel(color, x, y);
		}
	}

	GLU_CHECK_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
	GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, 0));
	GLU_CHECK_CALL(glDeleteRenderbuffers(1, &renderbuffer));
	GLU_CHECK_CALL(glDeleteFramebuffers(1, &framebuffer));
	GLU_CHECK_CALL(glFinish());
}

void GLES2ImageApi::modifyRenderbufferClearStencil (EGLImageKHR img, tcu::Texture2D& reference)
{
	m_log << tcu::TestLog::Message << "Modifying EGLImage with glClear to renderbuffer" << tcu::TestLog::EndMessage;

	deUint32 framebuffer;
	glGenFramebuffers(1, &framebuffer);
	TCU_CHECK(framebuffer != (GLuint)-1);
	GLU_CHECK_CALL(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer));

	deUint32 renderbuffer = 0;
	glGenRenderbuffers(1, &renderbuffer);
	TCU_CHECK(renderbuffer != 0);
	GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer));

	m_eglExt.glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, (GLeglImageOES)img);
	GLenum error = glGetError();

	if (error == GL_INVALID_OPERATION)
	{
		GLU_CHECK_CALL(glDeleteFramebuffers(1, &framebuffer));
		GLU_CHECK_CALL(glDeleteRenderbuffers(1, &renderbuffer));
		throw tcu::NotSupportedError("Creating renderbuffer from EGLImage type not supported", "glEGLImageTargetRenderbufferStorageOES", __FILE__, __LINE__);
	}
	TCU_CHECK(error == GL_NONE);

	TCU_CHECK_EGL_MSG("glEGLImageTargetRenderbufferStorageOES() failed");

	int stencilValue = 78;

	GLU_CHECK_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderbuffer));

	GLU_CHECK_CALL(glViewport(0, 0, reference.getWidth(), reference.getHeight()));
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		GLU_CHECK_CALL(glDeleteFramebuffers(1, &framebuffer));
		GLU_CHECK_CALL(glDeleteRenderbuffers(1, &renderbuffer));
		throw tcu::NotSupportedError("EGLImage type as stencil attachment not supported", "", __FILE__, __LINE__);
	}

	GLU_CHECK_CALL(glClearStencil(stencilValue));
	GLU_CHECK_CALL(glClear(GL_STENCIL_BUFFER_BIT));

	for (int x = 0; x < reference.getWidth(); x++)
	{
		for (int y = 0; y < reference.getHeight(); y++)
		{
			tcu::IVec4 color = tcu::IVec4(stencilValue, stencilValue, stencilValue, stencilValue);
			reference.getLevel(0).setPixel(color, x, y);
		}
	}

	GLU_CHECK_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
	GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, 0));
	GLU_CHECK_CALL(glDeleteRenderbuffers(1, &renderbuffer));
	GLU_CHECK_CALL(glDeleteFramebuffers(1, &framebuffer));
	GLU_CHECK_CALL(glFinish());
}


void GLES2ImageApi::checkRequiredExtensions (set<string>& extensions, TestSpec::Operation::Type type, int operationNdx)
{
	switch (type)
	{
		case TestSpec::Operation::TYPE_CREATE:
			switch (operationNdx)
			{
				case CREATE_TEXTURE2D_RGB8:
				case CREATE_TEXTURE2D_RGB565:
				case CREATE_TEXTURE2D_RGBA8:
				case CREATE_TEXTURE2D_RGBA5_A1:
				case CREATE_TEXTURE2D_RGBA4:
					extensions.insert("EGL_KHR_gl_texture_2D_image");
					break;

				case CREATE_CUBE_MAP_POSITIVE_X_RGB8:
				case CREATE_CUBE_MAP_NEGATIVE_X_RGB8:
				case CREATE_CUBE_MAP_POSITIVE_Y_RGB8:
				case CREATE_CUBE_MAP_NEGATIVE_Y_RGB8:
				case CREATE_CUBE_MAP_POSITIVE_Z_RGB8:
				case CREATE_CUBE_MAP_NEGATIVE_Z_RGB8:
				case CREATE_CUBE_MAP_POSITIVE_X_RGBA8:
				case CREATE_CUBE_MAP_NEGATIVE_X_RGBA8:
				case CREATE_CUBE_MAP_POSITIVE_Y_RGBA8:
				case CREATE_CUBE_MAP_NEGATIVE_Y_RGBA8:
				case CREATE_CUBE_MAP_POSITIVE_Z_RGBA8:
				case CREATE_CUBE_MAP_NEGATIVE_Z_RGBA8:
					extensions.insert("EGL_KHR_gl_texture_cubemap_image");
					break;

				case CREATE_RENDER_BUFFER_RGBA4:
				case CREATE_RENDER_BUFFER_RGB5_A1:
				case CREATE_RENDER_BUFFER_RGB565:
				case CREATE_RENDER_BUFFER_DEPTH16:
				case CREATE_RENDER_BUFFER_STENCIL:
					extensions.insert("EGL_KHR_gl_renderbuffer_image");
					break;

				default:
					DE_ASSERT(false);
			}
			break;

		case TestSpec::Operation::TYPE_RENDER:
			switch (operationNdx)
			{
				case RENDER_TEXTURE2D:
				case RENDER_READ_PIXELS_RENDERBUFFER:
				case RENDER_DEPTHBUFFER:
				case RENDER_TRY_ALL:
					extensions.insert("GL_OES_EGL_image");
					break;

				default:
					DE_ASSERT(false);
					break;
			}
			break;

		case TestSpec::Operation::TYPE_MODIFY:
			switch (operationNdx)
			{
				case MODIFY_TEXSUBIMAGE_RGB565:
				case MODIFY_TEXSUBIMAGE_RGB8:
				case MODIFY_TEXSUBIMAGE_RGBA8:
				case MODIFY_TEXSUBIMAGE_RGBA5_A1:
				case MODIFY_TEXSUBIMAGE_RGBA4:
				case MODIFY_RENDERBUFFER_CLEAR_COLOR:
				case MODIFY_RENDERBUFFER_CLEAR_DEPTH:
				case MODIFY_RENDERBUFFER_CLEAR_STENCIL:
					extensions.insert("GL_OES_EGL_image");
					break;

				default:
					DE_ASSERT(false);
					break;
			};
			break;

		default:
			DE_ASSERT(false);
			break;
	}
}

class ImageFormatCase : public TestCase
{
public:
						ImageFormatCase		(EglTestContext& eglTestCtx, const TestSpec& spec);
						~ImageFormatCase	(void);

	void				init				(void);
	void				deinit				(void);
	IterateResult		iterate				(void);
	void				checkExtensions		(void);

private:
	EGLConfig			getConfig			(void);

	const TestSpec		m_spec;
	tcu::TestLog&		m_log;

	vector<ImageApi*>	m_apiContexts;

	tcu::egl::Display*	m_display;
	eglu::NativeWindow*	m_window;
	tcu::egl::Surface*	m_surface;
	EGLConfig			m_config;
	int					m_curIter;
	EGLImageKHR			m_img;
	tcu::Texture2D		m_refImg;
	EglExt				m_eglExt;
};

EGLConfig ImageFormatCase::getConfig (void)
{
	vector<EGLConfig>	configs;
	eglu::FilterList	filter;

	EGLint attribList[] =
	{
		EGL_RENDERABLE_TYPE, 	EGL_OPENGL_ES2_BIT,
		EGL_SURFACE_TYPE,	 	EGL_WINDOW_BIT,
		EGL_ALPHA_SIZE,			1,
		EGL_DEPTH_SIZE,			8,
		EGL_NONE
	};
	m_display->chooseConfig(attribList, configs);

	return configs[0];
}

ImageFormatCase::ImageFormatCase (EglTestContext& eglTestCtx, const TestSpec& spec)
	: TestCase			(eglTestCtx, spec.name.c_str(), spec.desc.c_str())
	, m_spec			(spec)
	, m_log				(eglTestCtx.getTestContext().getLog())
	, m_display			(DE_NULL)
	, m_window			(DE_NULL)
	, m_surface			(DE_NULL)
	, m_config			(0)
	, m_curIter			(0)
	, m_img				(EGL_NO_IMAGE_KHR)
	, m_refImg			(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8), 1, 1)
{
}

ImageFormatCase::~ImageFormatCase (void)
{
	deinit();
}

void ImageFormatCase::checkExtensions (void)
{
	vector<string> extensions;
	m_display->getExtensions(extensions);

	set<string> extSet(extensions.begin(), extensions.end());

	const char* glExt = (const char*)glGetString(GL_EXTENSIONS);

	for (const char* c = glExt; true; c++)
	{
		if (*c == '\0')
		{
			extSet.insert(string(glExt));
			break;
		}

		if (*c == ' ')
		{
			extSet.insert(string(glExt, c));
			glExt = (c+1);
		}
	}

	if (extSet.find("EGL_KHR_image_base") == extSet.end()
			&& extSet.find("EGL_KHR_image") == extSet.end())
	{
		m_log << tcu::TestLog::Message
			<< "EGL_KHR_image and EGL_KHR_image_base not supported."
			<< "One should be supported."
			<< tcu::TestLog::EndMessage;
		throw tcu::NotSupportedError("Extension not supported", "EGL_KHR_image_base", __FILE__, __LINE__);
	}

	set<string> requiredExtensions;
	for (int operationNdx = 0; operationNdx < (int)m_spec.operations.size(); operationNdx++)
		m_apiContexts[m_spec.operations[m_curIter].apiIndex]->checkRequiredExtensions(requiredExtensions, m_spec.operations[operationNdx].type, m_spec.operations[operationNdx].operationIndex);

	std::set<string>::iterator extIter = requiredExtensions.begin();
	for (; extIter != requiredExtensions.end(); extIter++)
	{
		if (extSet.find(*extIter) == extSet.end())
			throw tcu::NotSupportedError("Extension not supported", (*extIter).c_str(), __FILE__, __LINE__);
	}
}

void ImageFormatCase::init (void)
{
	m_display	= &m_eglTestCtx.getDisplay();
	m_config	= getConfig();
	m_window	= m_eglTestCtx.createNativeWindow(m_display->getEGLDisplay(), m_config, DE_NULL, 480, 480, eglu::parseWindowVisibility(m_testCtx.getCommandLine()));
	m_surface	= new tcu::egl::WindowSurface(*m_display, eglu::createWindowSurface(m_eglTestCtx.getNativeDisplay(), *m_window, m_display->getEGLDisplay(), m_config, DE_NULL));

	for (int contextNdx = 0; contextNdx < (int)m_spec.contexts.size(); contextNdx++)
	{
		ImageApi* api = DE_NULL;
		switch (m_spec.contexts[contextNdx])
		{
			case TestSpec::API_GLES2:
			{
				api = new GLES2ImageApi(contextNdx, m_log, *m_display, m_surface, m_config);
				break;
			}

			default:
				DE_ASSERT(false);
				break;
		}
		m_apiContexts.push_back(api);
	}
	checkExtensions();
}

void ImageFormatCase::deinit (void)
{
	for (int contexNdx = 0 ; contexNdx < (int)m_apiContexts.size(); contexNdx++)
		delete m_apiContexts[contexNdx];

	m_apiContexts.clear();
	delete m_surface;
	m_surface = DE_NULL;
	delete m_window;
	m_window = DE_NULL;
}

TestCase::IterateResult ImageFormatCase::iterate (void)
{
	bool isOk = true;

	switch (m_spec.operations[m_curIter].type)
	{
		case TestSpec::Operation::TYPE_CREATE:
		{
			// Delete old image if exists
			if (m_img != EGL_NO_IMAGE_KHR)
			{
				m_log << tcu::TestLog::Message << "Destroying old EGLImage" << tcu::TestLog::EndMessage;
				TCU_CHECK_EGL_CALL(m_eglExt.eglDestroyImageKHR(m_display->getEGLDisplay(), m_img));

				m_img = EGL_NO_IMAGE_KHR;
			}

			m_img = m_apiContexts[m_spec.operations[m_curIter].apiIndex]->create(m_spec.operations[m_curIter].operationIndex, m_refImg);
			break;
		}

		case TestSpec::Operation::TYPE_RENDER:
		{
			DE_ASSERT(m_apiContexts[m_spec.operations[m_curIter].apiIndex]);
			isOk = m_apiContexts[m_spec.operations[m_curIter].apiIndex]->render(m_spec.operations[m_curIter].operationIndex, m_img, m_refImg);
			break;
		}

		case TestSpec::Operation::TYPE_MODIFY:
		{
			m_apiContexts[m_spec.operations[m_curIter].apiIndex]->modify(m_spec.operations[m_curIter].operationIndex, m_img, m_refImg);
			break;
		}

		default:
			DE_ASSERT(false);
			break;
	}

	if (isOk && ++m_curIter < (int)m_spec.operations.size())
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return CONTINUE;
	}
	else if (!isOk)
	{
		if (m_img != EGL_NO_IMAGE_KHR)
		{
			m_log << tcu::TestLog::Message << "Destroying EGLImage" << tcu::TestLog::EndMessage;
			TCU_CHECK_EGL_CALL(m_eglExt.eglDestroyImageKHR(m_display->getEGLDisplay(), m_img));
			m_img = EGL_NO_IMAGE_KHR;
		}
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
		return STOP;
	}
	else
	{
		if (m_img != EGL_NO_IMAGE_KHR)
		{
			m_log << tcu::TestLog::Message << "Destroying EGLImage" << tcu::TestLog::EndMessage;
			TCU_CHECK_EGL_CALL(m_eglExt.eglDestroyImageKHR(m_display->getEGLDisplay(), m_img));
			m_img = EGL_NO_IMAGE_KHR;
		}
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
}

SimpleCreationTests::SimpleCreationTests (EglTestContext& eglTestCtx)
	: TestCaseGroup	(eglTestCtx, "create", "EGLImage creation tests")
{
}

#define PUSH_VALUES_TO_VECTOR(type, vector, ...)\
do {\
	type array[] = __VA_ARGS__;\
	for (int i = 0; i < DE_LENGTH_OF_ARRAY(array); i++)\
	{\
		vector.push_back(array[i]);\
	}\
} while(false);

void SimpleCreationTests::init (void)
{
	GLES2ImageApi::Create createOperations[] = {
		GLES2ImageApi::CREATE_TEXTURE2D_RGB8,
		GLES2ImageApi::CREATE_TEXTURE2D_RGB565,
		GLES2ImageApi::CREATE_TEXTURE2D_RGBA8,
		GLES2ImageApi::CREATE_TEXTURE2D_RGBA5_A1,
		GLES2ImageApi::CREATE_TEXTURE2D_RGBA4,

		GLES2ImageApi::CREATE_CUBE_MAP_POSITIVE_X_RGBA8,
		GLES2ImageApi::CREATE_CUBE_MAP_POSITIVE_Y_RGBA8,
		GLES2ImageApi::CREATE_CUBE_MAP_POSITIVE_Z_RGBA8,

		GLES2ImageApi::CREATE_CUBE_MAP_NEGATIVE_X_RGBA8,
		GLES2ImageApi::CREATE_CUBE_MAP_NEGATIVE_Y_RGBA8,
		GLES2ImageApi::CREATE_CUBE_MAP_NEGATIVE_Z_RGBA8,

		GLES2ImageApi::CREATE_CUBE_MAP_POSITIVE_X_RGB8,
		GLES2ImageApi::CREATE_CUBE_MAP_POSITIVE_Y_RGB8,
		GLES2ImageApi::CREATE_CUBE_MAP_POSITIVE_Z_RGB8,

		GLES2ImageApi::CREATE_CUBE_MAP_NEGATIVE_X_RGB8,
		GLES2ImageApi::CREATE_CUBE_MAP_NEGATIVE_Y_RGB8,
		GLES2ImageApi::CREATE_CUBE_MAP_NEGATIVE_Z_RGB8,

		GLES2ImageApi::CREATE_RENDER_BUFFER_RGBA4,
		GLES2ImageApi::CREATE_RENDER_BUFFER_RGB5_A1,
		GLES2ImageApi::CREATE_RENDER_BUFFER_RGB565,
		GLES2ImageApi::CREATE_RENDER_BUFFER_DEPTH16,
		GLES2ImageApi::CREATE_RENDER_BUFFER_STENCIL
	};

	const char* createOperationsStr[] = {
		"texture_rgb8",
		"texture_rgb565",
		"texture_rgba8",
		"texture_rgba5_a1",
		"texture_rgba4",

		"cubemap_positive_x_rgba",
		"cubemap_positive_y_rgba",
		"cubemap_positive_z_rgba",

		"cubemap_negative_x_rgba",
		"cubemap_negative_y_rgba",
		"cubemap_negative_z_rgba",

		"cubemap_positive_x_rgb",
		"cubemap_positive_y_rgb",
		"cubemap_positive_z_rgb",

		"cubemap_negative_x_rgb",
		"cubemap_negative_y_rgb",
		"cubemap_negative_z_rgb",

		"renderbuffer_rgba4",
		"renderbuffer_rgb5_a1",
		"renderbuffer_rgb565",
		"renderbuffer_depth16",
		"renderbuffer_stencil"
	};

	GLES2ImageApi::Render renderOperations[] = {
			GLES2ImageApi::RENDER_TEXTURE2D,
			GLES2ImageApi::RENDER_READ_PIXELS_RENDERBUFFER,
			GLES2ImageApi::RENDER_DEPTHBUFFER
	};
	const char* renderOperationsStr[] = {
			"texture",
			"read_pixels",
			"depth_buffer"
	};

	DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(createOperations) == DE_LENGTH_OF_ARRAY(createOperationsStr));
	DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(renderOperations) == DE_LENGTH_OF_ARRAY(renderOperationsStr));

	for (int createNdx = 0; createNdx < DE_LENGTH_OF_ARRAY(createOperations); createNdx++)
	{
		for (int renderNdx = 0; renderNdx < DE_LENGTH_OF_ARRAY(renderOperations); renderNdx++)
		{
			TestSpec spec;
			spec.name = std::string("gles2_") + createOperationsStr[createNdx] + "_" + renderOperationsStr[renderNdx];
			spec.desc = spec.name;

			PUSH_VALUES_TO_VECTOR(TestSpec::ApiContext, spec.contexts,
			{
				TestSpec::API_GLES2
			});
			PUSH_VALUES_TO_VECTOR(TestSpec::Operation, spec.operations,
			{
				{ TestSpec::API_GLES2, 0, TestSpec::Operation::TYPE_CREATE, createOperations[createNdx] },
				{ TestSpec::API_GLES2, 0, TestSpec::Operation::TYPE_RENDER, renderOperations[renderNdx] },
			});

			addChild(new ImageFormatCase(m_eglTestCtx, spec));
		}
	}
}

MultiContextRenderTests::MultiContextRenderTests (EglTestContext& eglTestCtx)
	: TestCaseGroup	(eglTestCtx, "render_multiple_contexts", "EGLImage render tests on multiple contexts")
{
}

void MultiContextRenderTests::init (void)
{
	GLES2ImageApi::Create createOperations[] = {
		GLES2ImageApi::CREATE_TEXTURE2D_RGB8,
		GLES2ImageApi::CREATE_TEXTURE2D_RGB565,
		GLES2ImageApi::CREATE_TEXTURE2D_RGBA8,
		GLES2ImageApi::CREATE_TEXTURE2D_RGBA5_A1,
		GLES2ImageApi::CREATE_TEXTURE2D_RGBA4,

		GLES2ImageApi::CREATE_CUBE_MAP_POSITIVE_X_RGBA8,
		GLES2ImageApi::CREATE_CUBE_MAP_POSITIVE_Y_RGBA8,
		GLES2ImageApi::CREATE_CUBE_MAP_POSITIVE_Z_RGBA8,

		GLES2ImageApi::CREATE_CUBE_MAP_NEGATIVE_X_RGBA8,
		GLES2ImageApi::CREATE_CUBE_MAP_NEGATIVE_Y_RGBA8,
		GLES2ImageApi::CREATE_CUBE_MAP_NEGATIVE_Z_RGBA8,

		GLES2ImageApi::CREATE_CUBE_MAP_POSITIVE_X_RGB8,
		GLES2ImageApi::CREATE_CUBE_MAP_POSITIVE_Y_RGB8,
		GLES2ImageApi::CREATE_CUBE_MAP_POSITIVE_Z_RGB8,

		GLES2ImageApi::CREATE_CUBE_MAP_NEGATIVE_X_RGB8,
		GLES2ImageApi::CREATE_CUBE_MAP_NEGATIVE_Y_RGB8,
		GLES2ImageApi::CREATE_CUBE_MAP_NEGATIVE_Z_RGB8,

		GLES2ImageApi::CREATE_RENDER_BUFFER_RGBA4,
		GLES2ImageApi::CREATE_RENDER_BUFFER_RGB5_A1,
		GLES2ImageApi::CREATE_RENDER_BUFFER_RGB565,
		GLES2ImageApi::CREATE_RENDER_BUFFER_DEPTH16,
		GLES2ImageApi::CREATE_RENDER_BUFFER_STENCIL
	};

	const char* createOperationsStr[] = {
		"texture_rgb8",
		"texture_rgb565",
		"texture_rgba8",
		"texture_rgba5_a1",
		"texture_rgba4",

		"cubemap_positive_x_rgba8",
		"cubemap_positive_y_rgba8",
		"cubemap_positive_z_rgba8",

		"cubemap_negative_x_rgba8",
		"cubemap_negative_y_rgba8",
		"cubemap_negative_z_rgba8",

		"cubemap_positive_x_rgb8",
		"cubemap_positive_y_rgb8",
		"cubemap_positive_z_rgb8",

		"cubemap_negative_x_rgb8",
		"cubemap_negative_y_rgb8",
		"cubemap_negative_z_rgb8",

		"renderbuffer_rgba4",
		"renderbuffer_rgb5_a1",
		"renderbuffer_rgb565",
		"renderbuffer_depth16",
		"renderbuffer_stencil"
	};

	GLES2ImageApi::Render renderOperations[] = {
			GLES2ImageApi::RENDER_TEXTURE2D,
			GLES2ImageApi::RENDER_READ_PIXELS_RENDERBUFFER,
			GLES2ImageApi::RENDER_DEPTHBUFFER
	};
	const char* renderOperationsStr[] = {
			"texture",
			"read_pixels",
			"depth_buffer"
	};

	DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(createOperations) == DE_LENGTH_OF_ARRAY(createOperationsStr));
	DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(renderOperations) == DE_LENGTH_OF_ARRAY(renderOperationsStr));

	for (int createNdx = 0; createNdx < DE_LENGTH_OF_ARRAY(createOperations); createNdx++)
	{
		for (int renderNdx = 0; renderNdx < DE_LENGTH_OF_ARRAY(renderOperations); renderNdx++)
		{
			TestSpec spec;
			spec.name = std::string("gles2_") + createOperationsStr[createNdx] + "_" + renderOperationsStr[renderNdx];
			spec.desc = spec.name;

			PUSH_VALUES_TO_VECTOR(TestSpec::ApiContext, spec.contexts,
			{
				TestSpec::API_GLES2,
				TestSpec::API_GLES2
			});
			PUSH_VALUES_TO_VECTOR(TestSpec::Operation, spec.operations,
			{
				{ TestSpec::API_GLES2, 0, TestSpec::Operation::TYPE_CREATE, createOperations[createNdx] },
				{ TestSpec::API_GLES2, 1, TestSpec::Operation::TYPE_RENDER, renderOperations[renderNdx] },
				{ TestSpec::API_GLES2, 0, TestSpec::Operation::TYPE_RENDER, renderOperations[renderNdx] },
				{ TestSpec::API_GLES2, 1, TestSpec::Operation::TYPE_CREATE, createOperations[createNdx] },
				{ TestSpec::API_GLES2, 1, TestSpec::Operation::TYPE_RENDER, renderOperations[renderNdx] },
				{ TestSpec::API_GLES2, 0, TestSpec::Operation::TYPE_RENDER, renderOperations[renderNdx] }
			});
			addChild(new ImageFormatCase(m_eglTestCtx, spec));
		}
	}
}

ModifyTests::ModifyTests (EglTestContext& eglTestCtx)
	: TestCaseGroup	(eglTestCtx, "modify", "EGLImage modifying tests")
{
}

void ModifyTests::init (void)
{
	GLES2ImageApi::Create createOperations[] = {
		GLES2ImageApi::CREATE_TEXTURE2D_RGB8,
		GLES2ImageApi::CREATE_TEXTURE2D_RGB565,
		GLES2ImageApi::CREATE_TEXTURE2D_RGBA8,
		GLES2ImageApi::CREATE_TEXTURE2D_RGBA5_A1,
		GLES2ImageApi::CREATE_TEXTURE2D_RGBA4,

		GLES2ImageApi::CREATE_RENDER_BUFFER_RGBA4,
		GLES2ImageApi::CREATE_RENDER_BUFFER_RGB5_A1,
		GLES2ImageApi::CREATE_RENDER_BUFFER_RGB565,
		GLES2ImageApi::CREATE_RENDER_BUFFER_DEPTH16,
		GLES2ImageApi::CREATE_RENDER_BUFFER_STENCIL
	};

	const char* createOperationsStr[] = {
		"tex_rgb8",
		"tex_rgb565",
		"tex_rgba8",
		"tex_rgba5_a1",
		"tex_rgba4",

		"renderbuffer_rgba4",
		"renderbuffer_rgb5_a1",
		"renderbuffer_rgb565",
		"renderbuffer_depth16",
		"renderbuffer_stencil"
	};

	GLES2ImageApi::Modify modifyOperations[] = {
		GLES2ImageApi::MODIFY_TEXSUBIMAGE_RGB8,
		GLES2ImageApi::MODIFY_TEXSUBIMAGE_RGB565,
		GLES2ImageApi::MODIFY_TEXSUBIMAGE_RGBA8,
		GLES2ImageApi::MODIFY_TEXSUBIMAGE_RGBA5_A1,
		GLES2ImageApi::MODIFY_TEXSUBIMAGE_RGBA4,

		GLES2ImageApi::MODIFY_RENDERBUFFER_CLEAR_COLOR,
		GLES2ImageApi::MODIFY_RENDERBUFFER_CLEAR_DEPTH,
		GLES2ImageApi::MODIFY_RENDERBUFFER_CLEAR_STENCIL,
	};

	const char* modifyOperationsStr[] = {
		"tex_subimage_rgb8",
		"tex_subimage_rgb565",
		"tex_subimage_rgba8",
		"tex_subimage_rgba5_a1",
		"tex_subimage_rgba4",

		"renderbuffer_clear_color",
		"renderbuffer_clear_depth",
		"renderbuffer_clear_stencil",
	};

	DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(modifyOperations) == DE_LENGTH_OF_ARRAY(modifyOperationsStr));
	DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(createOperations) == DE_LENGTH_OF_ARRAY(createOperationsStr));

	for (int createNdx = 0; createNdx < DE_LENGTH_OF_ARRAY(createOperations); createNdx++)
	{
		for (int modifyNdx = 0; modifyNdx < DE_LENGTH_OF_ARRAY(modifyOperations); modifyNdx++)
		{
			TestSpec spec;
			spec.name = "gles2_tex_sub_image";
			spec.desc = spec.name;

			PUSH_VALUES_TO_VECTOR(TestSpec::ApiContext, spec.contexts,
			{
				TestSpec::API_GLES2
			});
			PUSH_VALUES_TO_VECTOR(TestSpec::Operation, spec.operations,
			{
				{ TestSpec::API_GLES2, 0, TestSpec::Operation::TYPE_CREATE, createOperations[createNdx] },
				{ TestSpec::API_GLES2, 0, TestSpec::Operation::TYPE_RENDER, GLES2ImageApi::RENDER_TRY_ALL },
				{ TestSpec::API_GLES2, 0, TestSpec::Operation::TYPE_MODIFY, modifyOperations[modifyNdx] },
				{ TestSpec::API_GLES2, 0, TestSpec::Operation::TYPE_RENDER, GLES2ImageApi::RENDER_TRY_ALL }
			});

			spec.name = std::string(createOperationsStr[createNdx]) + "_" + modifyOperationsStr[modifyNdx];
			addChild(new ImageFormatCase(m_eglTestCtx, spec));
		}
	}
}

} // Image
} // egl
} // deqp
