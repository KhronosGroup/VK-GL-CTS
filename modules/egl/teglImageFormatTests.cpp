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

#include "deStringUtil.hpp"
#include "deSTLUtil.hpp"

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
#include "egluUnique.hpp"
#include "egluUtil.hpp"

#include "eglwLibrary.hpp"
#include "eglwEnums.hpp"

#include "gluCallLogWrapper.hpp"
#include "gluShaderProgram.hpp"
#include "gluStrUtil.hpp"
#include "gluTexture.hpp"
#include "gluPixelTransfer.hpp"
#include "gluObjectWrapper.hpp"
#include "gluTextureUtil.hpp"

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

#include "teglImageUtil.hpp"
#include "teglAndroidUtil.hpp"

#include <vector>
#include <string>
#include <set>

using std::vector;
using std::set;
using std::string;

using de::MovePtr;
using de::UniquePtr;

using glu::Framebuffer;
using glu::Renderbuffer;
using glu::Texture;

using eglu::UniqueImage;

using tcu::ConstPixelBufferAccess;

using namespace glw;
using namespace eglw;

namespace deqp
{
namespace egl
{

namespace
{

glu::ProgramSources programSources (const string& vertexSource, const string& fragmentSource)
{
	glu::ProgramSources sources;

	sources << glu::VertexSource(vertexSource) << glu::FragmentSource(fragmentSource);

	return sources;
}

class Program : public glu::ShaderProgram
{
public:
	Program (const glw::Functions& gl, const char* vertexSource, const char* fragmentSource)
		: glu::ShaderProgram(gl, programSources(vertexSource, fragmentSource)) {}
};

} // anonymous

namespace Image
{

class ImageApi;

class IllegalRendererException : public std::exception
{
};

class Action
{
public:
	virtual			~Action					(void) {}
	virtual bool	invoke					(ImageApi& api, MovePtr<UniqueImage>& image, tcu::Texture2D& refImg) const = 0;
	virtual string	getRequiredExtension	(void) const = 0;
};

struct TestSpec
{
	std::string name;
	std::string desc;

	enum ApiContext
	{
		API_GLES2 = 0,
		API_GLES3,
		//API_VG
		//API_GLES1

		API_LAST
	};

	struct Operation
	{
		Operation (int apiIndex_, const Action& action_) : apiIndex(apiIndex_), action(&action_) {}
		int				apiIndex;
		const Action*	action;
	};

	vector<ApiContext>	contexts;
	vector<Operation>	operations;

};

class ImageApi
{
public:
					ImageApi		(const Library& egl, int contextId, EGLDisplay display, EGLSurface surface);
	virtual			~ImageApi		(void) {}

protected:
	const Library&	m_egl;
	int				m_contextId;
	EGLDisplay		m_display;
	EGLSurface		m_surface;
};

ImageApi::ImageApi (const Library& egl, int contextId, EGLDisplay display, EGLSurface surface)
	: m_egl				(egl)
	, m_contextId		(contextId)
	, m_display			(display)
	, m_surface			(surface)
{
}

class GLESImageApi : public ImageApi, private glu::CallLogWrapper
{
public:
	class GLESAction : public Action
	{
	public:
		bool				invoke					(ImageApi& api, MovePtr<UniqueImage>& image, tcu::Texture2D& ref) const;
		virtual bool		invokeGLES				(GLESImageApi& api, MovePtr<UniqueImage>& image, tcu::Texture2D& ref) const = 0;
	};

	class Create : public GLESAction
	{
	public:
								Create					(MovePtr<ImageSource> imgSource, deUint32 numLayers = 1u) : m_imgSource(imgSource), m_numLayers(numLayers) {}
		string					getRequiredExtension	(void) const { return m_imgSource->getRequiredExtension(); }
		bool					invokeGLES				(GLESImageApi& api, MovePtr<UniqueImage>& image, tcu::Texture2D& ref) const;
		deUint32				getNumLayers			(void) const { return m_numLayers; }
		glw::GLenum				getEffectiveFormat		(void) const { return m_imgSource->getEffectiveFormat(); }
		bool					isYUVFormatImage		(void) const { return m_imgSource->isYUVFormatImage(); }
	private:
		UniquePtr<ImageSource>	m_imgSource;
		deUint32				m_numLayers;
	};

	class Render : public GLESAction
	{
	public:
		virtual string			getRequiredExtension	(void) const { return "GL_OES_EGL_image"; }
	};

	class RenderTexture2D				: public Render { public: bool invokeGLES (GLESImageApi& api, MovePtr<UniqueImage>& image, tcu::Texture2D& ref) const override; };
	class RenderTextureCubemap			: public Render { public: bool invokeGLES (GLESImageApi& api, MovePtr<UniqueImage>& image, tcu::Texture2D& ref) const override; };
	class RenderReadPixelsRenderbuffer	: public Render { public: bool invokeGLES (GLESImageApi& api, MovePtr<UniqueImage>& image, tcu::Texture2D& ref) const override; };
	class RenderDepthbuffer				: public Render { public: bool invokeGLES (GLESImageApi& api, MovePtr<UniqueImage>& image, tcu::Texture2D& ref) const override; };
	class RenderStencilbuffer			: public Render { public: bool invokeGLES (GLESImageApi& api, MovePtr<UniqueImage>& image, tcu::Texture2D& ref) const override; };
	class RenderTryAll					: public Render { public: bool invokeGLES (GLESImageApi& api, MovePtr<UniqueImage>& image, tcu::Texture2D& ref) const override; };

	class RenderTexture2DArray : public Render
	{
		public:
			bool	invokeGLES				(GLESImageApi& api, MovePtr<UniqueImage>& image, tcu::Texture2D& ref) const override;
			string	getRequiredExtension	(void) const override { return "GL_EXT_EGL_image_array"; }
	};

	class RenderExternalTexture			: public Render
	{
		public:
			bool	invokeGLES				(GLESImageApi& api, MovePtr<UniqueImage>& image, tcu::Texture2D& ref) const override;
			string	getRequiredExtension	(void) const override { return "GL_OES_EGL_image_external"; }
	};

	class RenderExternalTextureSamplerArray	: public Render
	{
		public:
			bool	invokeGLES				(GLESImageApi& api, MovePtr<UniqueImage>& image, tcu::Texture2D& ref) const override;
			string	getRequiredExtension	(void) const override { return "GL_OES_EGL_image_external"; }
	};
	class RenderYUVTexture			: public Render
	{
		public:
			bool	invokeGLES				(GLESImageApi& api, MovePtr<UniqueImage>& image, tcu::Texture2D& ref) const override;
			string	getRequiredExtension	(void) const override { return "GL_EXT_YUV_target"; }
	};
	class Modify : public GLESAction
	{
	public:
		string				getRequiredExtension	(void) const { return "GL_OES_EGL_image"; }
	};

	class ModifyTexSubImage : public Modify
	{
	public:
							ModifyTexSubImage		(GLenum format, GLenum type) : m_format(format), m_type(type) {}
		bool				invokeGLES				(GLESImageApi& api, MovePtr<UniqueImage>& image, tcu::Texture2D& ref) const;
		GLenum				getFormat				(void) const { return m_format; }
		GLenum				getType					(void) const { return m_type; }

	private:
		GLenum				m_format;
		GLenum				m_type;
	};

	class ModifyRenderbuffer : public Modify
	{
	public:
		bool				invokeGLES				(GLESImageApi& api, MovePtr<UniqueImage>& image, tcu::Texture2D& ref) const;

	protected:
		virtual void		initializeRbo			(GLESImageApi& api, GLuint rbo, tcu::Texture2D& ref) const = 0;
	};

	class ModifyRenderbufferClearColor : public ModifyRenderbuffer
	{
	public:
					ModifyRenderbufferClearColor	(tcu::Vec4 color) : m_color(color) {}

	protected:
		void		initializeRbo					(GLESImageApi& api, GLuint rbo, tcu::Texture2D& ref) const;

		tcu::Vec4	m_color;
	};

	class ModifyRenderbufferClearDepth : public ModifyRenderbuffer
	{
	public:
					ModifyRenderbufferClearDepth	(GLfloat depth) : m_depth(depth) {}

	protected:
		void		initializeRbo					(GLESImageApi& api, GLuint rbo, tcu::Texture2D& ref) const;

		GLfloat		m_depth;
	};

	class ModifyRenderbufferClearStencil : public ModifyRenderbuffer
	{
	public:
					ModifyRenderbufferClearStencil	(GLint stencil) : m_stencil(stencil) {}

	protected:
		void		initializeRbo					(GLESImageApi& api, GLuint rbo, tcu::Texture2D& ref) const;

		GLint		m_stencil;
	};

					GLESImageApi					(const Library& egl, const glw::Functions& gl, int contextId, tcu::TestLog& log, EGLDisplay display, EGLSurface surface, EGLConfig config, EGLint apiVersion);
					~GLESImageApi					(void);

private:
	EGLContext					m_context;
	const glw::Functions&		m_gl;

	MovePtr<UniqueImage>		createImage			(const ImageSource& source, const ClientBuffer& buffer) const;
};

GLESImageApi::GLESImageApi (const Library& egl, const glw::Functions& gl, int contextId, tcu::TestLog& log, EGLDisplay display, EGLSurface surface, EGLConfig config, EGLint apiVersion)
	: ImageApi				(egl, contextId, display, surface)
	, glu::CallLogWrapper	(gl, log)
	, m_context				(DE_NULL)
	, m_gl					(gl)
{
	const EGLint attriblist[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, apiVersion,
		EGL_NONE
	};

	EGLint configId = -1;
	EGLU_CHECK_CALL(m_egl, getConfigAttrib(m_display, config, EGL_CONFIG_ID, &configId));
	getLog() << tcu::TestLog::Message << "Creating gles" << apiVersion << " context with config id: " << configId << " context: " << m_contextId << tcu::TestLog::EndMessage;
	egl.bindAPI(EGL_OPENGL_ES_API);
	m_context = m_egl.createContext(m_display, config, EGL_NO_CONTEXT, attriblist);
	EGLU_CHECK_MSG(m_egl, "Failed to create GLES context");

	egl.makeCurrent(display, m_surface, m_surface, m_context);
	EGLU_CHECK_MSG(m_egl, "Failed to make context current");
}

GLESImageApi::~GLESImageApi (void)
{
	m_egl.makeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	m_egl.destroyContext(m_display, m_context);
}

bool GLESImageApi::GLESAction::invoke (ImageApi& api, MovePtr<UniqueImage>& image, tcu::Texture2D& ref) const
{
	GLESImageApi& glesApi = dynamic_cast<GLESImageApi&>(api);

	glesApi.m_egl.makeCurrent(glesApi.m_display, glesApi.m_surface, glesApi.m_surface, glesApi.m_context);
	return invokeGLES(glesApi, image, ref);
}

bool GLESImageApi::Create::invokeGLES (GLESImageApi& api, MovePtr<UniqueImage>& image, tcu::Texture2D& ref) const
{
	de::UniquePtr<ClientBuffer>	buffer	(m_imgSource->createBuffer(api.m_egl, api.m_gl, &ref));

	GLU_CHECK_GLW_CALL(api.m_gl, finish());

	image = api.createImage(*m_imgSource, *buffer);
	return true;
}

MovePtr<UniqueImage> GLESImageApi::createImage (const ImageSource& source, const ClientBuffer& buffer) const
{
	const EGLImageKHR image = source.createImage(m_egl, m_display, m_context, buffer.get());
	return MovePtr<UniqueImage>(new UniqueImage(m_egl, m_display, image));
}

static void imageTargetTexture2D (const Library& egl, const glw::Functions& gl, GLeglImageOES img)
{
	gl.eglImageTargetTexture2DOES(GL_TEXTURE_2D, img);
	{
		const GLenum error = gl.getError();

		if (error == GL_INVALID_OPERATION)
			TCU_THROW(NotSupportedError, "Creating texture2D from EGLImage type not supported");

		GLU_EXPECT_NO_ERROR(error, "glEGLImageTargetTexture2DOES()");
		EGLU_CHECK_MSG(egl, "glEGLImageTargetTexture2DOES()");
	}
}

static void imageTargetExternalTexture (const Library& egl, const glw::Functions& gl, GLeglImageOES img)
{
	gl.eglImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, img);
	{
		const GLenum error = gl.getError();

		if (error == GL_INVALID_OPERATION)
			TCU_THROW(NotSupportedError, "Creating external texture from EGLImage type not supported");

		GLU_EXPECT_NO_ERROR(error, "glEGLImageTargetTexture2DOES()");
		EGLU_CHECK_MSG(egl, "glEGLImageTargetTexture2DOES()");
	}
}

static void imageTargetTexture2DArray (const Library& egl, const glw::Functions& gl, GLeglImageOES img)
{
	gl.eglImageTargetTexture2DOES(GL_TEXTURE_2D_ARRAY, img);
	{
		const GLenum error = gl.getError();

		if (error == GL_INVALID_OPERATION)
			TCU_THROW(NotSupportedError, "Creating texture2D array from EGLImage type not supported");

		GLU_EXPECT_NO_ERROR(error, "glEGLImageTargetTexture2DOES()");
		EGLU_CHECK_MSG(egl, "glEGLImageTargetTexture2DOES()");
	}
}

static void imageTargetRenderbuffer (const Library& egl, const glw::Functions& gl, GLeglImageOES img)
{
	gl.eglImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, img);
	{
		const GLenum error = gl.getError();

		if (error == GL_INVALID_OPERATION)
			TCU_THROW(NotSupportedError, "Creating renderbuffer from EGLImage type not supported");

		GLU_EXPECT_NO_ERROR(error, "glEGLImageTargetRenderbufferStorageOES()");
		EGLU_CHECK_MSG(egl, "glEGLImageTargetRenderbufferStorageOES()");
	}
}

static void framebufferRenderbuffer (const glw::Functions& gl, GLenum attachment, GLuint rbo)
{
	GLU_CHECK_GLW_CALL(gl, framebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, rbo));
	TCU_CHECK_AND_THROW(NotSupportedError,
						gl.checkFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
						("EGLImage as " + string(glu::getFramebufferAttachmentName(attachment)) + " not supported").c_str());
}

static const float squareTriangleCoords[] =
{
	-1.0, -1.0,
	1.0, -1.0,
	1.0,  1.0,

	1.0,  1.0,
	-1.0,  1.0,
	-1.0, -1.0
};

bool GLESImageApi::RenderTexture2D::invokeGLES (GLESImageApi& api, MovePtr<UniqueImage>& img, tcu::Texture2D& reference) const
{
	const glw::Functions&	gl		= api.m_gl;
	tcu::TestLog&			log		= api.getLog();
	Texture					srcTex	(gl);

	// Branch only taken in TryAll case
	if (reference.getFormat().order == tcu::TextureFormat::DS || reference.getFormat().order == tcu::TextureFormat::D)
		throw IllegalRendererException(); // Skip, GLES does not support sampling depth textures
	if (reference.getFormat().order == tcu::TextureFormat::S)
		throw IllegalRendererException(); // Skip, GLES does not support sampling stencil textures

	gl.clearColor(0.0, 0.0, 0.0, 0.0);
	gl.viewport(0, 0, reference.getWidth(), reference.getHeight());
	gl.clear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	gl.disable(GL_DEPTH_TEST);

	log << tcu::TestLog::Message << "Rendering EGLImage as GL_TEXTURE_2D in context: " << api.m_contextId << tcu::TestLog::EndMessage;
	TCU_CHECK(**img != EGL_NO_IMAGE_KHR);

	GLU_CHECK_GLW_CALL(gl, bindTexture(GL_TEXTURE_2D, *srcTex));
	imageTargetTexture2D(api.m_egl, gl, **img);

	GLU_CHECK_GLW_CALL(gl, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
	GLU_CHECK_GLW_CALL(gl, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
	GLU_CHECK_GLW_CALL(gl, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	GLU_CHECK_GLW_CALL(gl, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));

	const char* const vertexShader =
		"attribute highp vec2 a_coord;\n"
		"varying mediump vec2 v_texCoord;\n"
		"void main(void) {\n"
		"\tv_texCoord = vec2((a_coord.x + 1.0) * 0.5, (a_coord.y + 1.0) * 0.5);\n"
		"\tgl_Position = vec4(a_coord, -0.1, 1.0);\n"
		"}\n";

	const char* const fragmentShader =
		"varying mediump vec2 v_texCoord;\n"
		"uniform sampler2D u_sampler;\n"
		"void main(void) {\n"
		"\tmediump vec4 texColor = texture2D(u_sampler, v_texCoord);\n"
		"\tgl_FragColor = vec4(texColor);\n"
		"}";

	Program program(gl, vertexShader, fragmentShader);
	TCU_CHECK(program.isOk());

	GLuint glProgram = program.getProgram();
	GLU_CHECK_GLW_CALL(gl, useProgram(glProgram));

	GLuint coordLoc = gl.getAttribLocation(glProgram, "a_coord");
	TCU_CHECK_MSG((int)coordLoc != -1, "Couldn't find attribute a_coord");

	GLuint samplerLoc = gl.getUniformLocation(glProgram, "u_sampler");
	TCU_CHECK_MSG((int)samplerLoc != (int)-1, "Couldn't find uniform u_sampler");

	GLU_CHECK_GLW_CALL(gl, bindTexture(GL_TEXTURE_2D, *srcTex));
	GLU_CHECK_GLW_CALL(gl, uniform1i(samplerLoc, 0));
	GLU_CHECK_GLW_CALL(gl, enableVertexAttribArray(coordLoc));
	GLU_CHECK_GLW_CALL(gl, vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, squareTriangleCoords));

	GLU_CHECK_GLW_CALL(gl, drawArrays(GL_TRIANGLES, 0, 6));
	GLU_CHECK_GLW_CALL(gl, disableVertexAttribArray(coordLoc));
	GLU_CHECK_GLW_CALL(gl, bindTexture(GL_TEXTURE_2D, 0));

	tcu::Surface refSurface	(reference.getWidth(), reference.getHeight());
	tcu::Surface screen		(reference.getWidth(), reference.getHeight());
	GLU_CHECK_GLW_CALL(gl, readPixels(0, 0, screen.getWidth(), screen.getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, screen.getAccess().getDataPtr()));

	tcu::copy(refSurface.getAccess(), reference.getLevel(0));

	float	threshold	= 0.05f;
	bool	match		= tcu::fuzzyCompare(log, "ComparisonResult", "Image comparison result", refSurface, screen, threshold, tcu::COMPARE_LOG_RESULT);

	return match;
}

// Renders using a single layer from a texture array.
bool GLESImageApi::RenderTexture2DArray::invokeGLES (GLESImageApi& api, MovePtr<UniqueImage>& img, tcu::Texture2D& reference) const
{
	const glw::Functions&	gl		= api.m_gl;
	tcu::TestLog&			log		= api.getLog();
	Texture					srcTex	(gl);

	gl.clearColor(0.0, 0.0, 0.0, 0.0);
	gl.viewport(0, 0, reference.getWidth(), reference.getHeight());
	gl.clear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	gl.disable(GL_DEPTH_TEST);

	log << tcu::TestLog::Message << "Rendering EGLImage as GL_TEXTURE_2D_ARRAY in context: " << api.m_contextId << tcu::TestLog::EndMessage;
	TCU_CHECK(**img != EGL_NO_IMAGE_KHR);

	GLU_CHECK_GLW_CALL(gl, bindTexture(GL_TEXTURE_2D_ARRAY, *srcTex));
	imageTargetTexture2DArray(api.m_egl, gl, **img);

	glu::TransferFormat transferFormat = glu::getTransferFormat(reference.getFormat());
	// Initializes layer 1.
	GLU_CHECK_GLW_CALL(gl, texSubImage3D(GL_TEXTURE_2D_ARRAY,
			0,										// Mipmap level
            0,										// X offset
			0,										// Y offset
			1,										// Z offset (layer)
			reference.getWidth(),					// Width
			reference.getHeight(),					// Height
			1u,										// Depth
			transferFormat.format,					// Format
			transferFormat.dataType,				// Type
			reference.getLevel(0).getDataPtr()));	// Pixel data


	GLU_CHECK_GLW_CALL(gl, texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
	GLU_CHECK_GLW_CALL(gl, texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
	GLU_CHECK_GLW_CALL(gl, texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	GLU_CHECK_GLW_CALL(gl, texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));

	const char* const vertexShader =
	"#version 320 es\n"
	"precision highp int;\n"
	"precision highp float;\n"
	"layout(location = 0) in vec2 pos_in;\n"
	"layout(location = 0) out vec2 texcoord_out;\n"
	"void main()\n"
	"{\n"
	"    gl_Position = vec4(pos_in, -0.1, 1.0);\n"
	"    texcoord_out = vec2((pos_in.x + 1.0) * 0.5, (pos_in.y + 1.0) * 0.5);\n"
	"}\n";

	const char* const fragmentShader =
	"#version 320 es\n"
	"precision highp int;\n"
	"precision highp float;\n"
	"layout(location = 0) in vec2 texcoords_in;\n"
	"layout(location = 0) out vec4 color_out;\n"
	"uniform layout(binding=0) highp sampler2DArray tex_sampler;\n"
	"void main()\n"
	"{\n"
	// Samples layer 1.
	"    color_out = texture(tex_sampler, vec3(texcoords_in, 1));\n"
	"}\n";

	Program program(gl, vertexShader, fragmentShader);

	if (!program.isOk())
	{
		log << tcu::TestLog::Message << "Shader build failed.\n"
			<< "Vertex: " << program.getShaderInfo(glu::SHADERTYPE_VERTEX).infoLog << "\n"
			<< vertexShader << "\n"
			<< "Fragment: " << program.getShaderInfo(glu::SHADERTYPE_FRAGMENT).infoLog << "\n"
			<< fragmentShader << "\n"
			<< "Program: " << program.getProgramInfo().infoLog << tcu::TestLog::EndMessage;
	}

	TCU_CHECK(program.isOk());

	GLuint glProgram = program.getProgram();
	GLU_CHECK_GLW_CALL(gl, useProgram(glProgram));

	GLuint coordLoc = gl.getAttribLocation(glProgram, "pos_in");
	TCU_CHECK_MSG((int)coordLoc != -1, "Couldn't find attribute pos_in");

	GLuint samplerLoc = gl.getUniformLocation(glProgram, "tex_sampler");
	TCU_CHECK_MSG((int)samplerLoc != (int)-1, "Couldn't find uniform tex_sampler");

	GLU_CHECK_GLW_CALL(gl, bindTexture(GL_TEXTURE_2D_ARRAY, *srcTex));
	GLU_CHECK_GLW_CALL(gl, uniform1i(samplerLoc, 0));
	GLU_CHECK_GLW_CALL(gl, enableVertexAttribArray(coordLoc));
	GLU_CHECK_GLW_CALL(gl, vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, squareTriangleCoords));

	GLU_CHECK_GLW_CALL(gl, drawArrays(GL_TRIANGLES, 0, 6));
	GLU_CHECK_GLW_CALL(gl, disableVertexAttribArray(coordLoc));

	tcu::Surface refSurface	(reference.getWidth(), reference.getHeight());
	tcu::Surface screen		(reference.getWidth(), reference.getHeight());
	GLU_CHECK_GLW_CALL(gl, readPixels(0, 0, screen.getWidth(), screen.getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, screen.getAccess().getDataPtr()));

	tcu::copy(refSurface.getAccess(), reference.getLevel(0));

	float	threshold	= 0.05f;
	bool	match		= tcu::fuzzyCompare(log, "ComparisonResult", "Image comparison result", refSurface, screen, threshold, tcu::COMPARE_LOG_RESULT);

	return match;
}

bool GLESImageApi::RenderExternalTexture::invokeGLES (GLESImageApi& api, MovePtr<UniqueImage>& img, tcu::Texture2D& reference) const
{
	const glw::Functions&	gl		= api.m_gl;
	tcu::TestLog&			log		= api.getLog();
	Texture					srcTex	(gl);

	// Branch only taken in TryAll case
	if (reference.getFormat().order == tcu::TextureFormat::DS || reference.getFormat().order == tcu::TextureFormat::D)
		throw IllegalRendererException(); // Skip, GLES2 does not support sampling depth textures
	if (reference.getFormat().order == tcu::TextureFormat::S)
		throw IllegalRendererException(); // Skip, GLES2 does not support sampling stencil textures

	gl.clearColor(0.0, 0.0, 0.0, 0.0);
	gl.viewport(0, 0, reference.getWidth(), reference.getHeight());
	gl.clear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	gl.disable(GL_DEPTH_TEST);

	log << tcu::TestLog::Message << "Rendering EGLImage as GL_TEXTURE_EXTERNAL_OES in context: " << api.m_contextId << tcu::TestLog::EndMessage;
	TCU_CHECK(**img != EGL_NO_IMAGE_KHR);

	GLU_CHECK_GLW_CALL(gl, bindTexture(GL_TEXTURE_EXTERNAL_OES, *srcTex));
	imageTargetExternalTexture(api.m_egl, gl, **img);

	GLU_CHECK_GLW_CALL(gl, texParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
	GLU_CHECK_GLW_CALL(gl, texParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
	GLU_CHECK_GLW_CALL(gl, texParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	GLU_CHECK_GLW_CALL(gl, texParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));

	const char* const vertexShader =
		"attribute highp vec2 a_coord;\n"
		"varying mediump vec2 v_texCoord;\n"
		"void main(void) {\n"
		"\tv_texCoord = vec2((a_coord.x + 1.0) * 0.5, (a_coord.y + 1.0) * 0.5);\n"
		"\tgl_Position = vec4(a_coord, -0.1, 1.0);\n"
		"}\n";

	const char* const fragmentShader =
		"#extension GL_OES_EGL_image_external : require\n"
		"varying mediump vec2 v_texCoord;\n"
		"uniform samplerExternalOES u_sampler;\n"
		"void main(void) {\n"
		"\tmediump vec4 texColor = texture2D(u_sampler, v_texCoord);\n"
		"\tgl_FragColor = vec4(texColor);\n"
		"}";

	Program program(gl, vertexShader, fragmentShader);
	TCU_CHECK(program.isOk());

	GLuint glProgram = program.getProgram();
	GLU_CHECK_GLW_CALL(gl, useProgram(glProgram));

	GLuint coordLoc = gl.getAttribLocation(glProgram, "a_coord");
	TCU_CHECK_MSG((int)coordLoc != -1, "Couldn't find attribute a_coord");

	GLuint samplerLoc = gl.getUniformLocation(glProgram, "u_sampler");
	TCU_CHECK_MSG((int)samplerLoc != (int)-1, "Couldn't find uniform u_sampler");

	GLU_CHECK_GLW_CALL(gl, bindTexture(GL_TEXTURE_EXTERNAL_OES, *srcTex));
	GLU_CHECK_GLW_CALL(gl, uniform1i(samplerLoc, 0));
	GLU_CHECK_GLW_CALL(gl, enableVertexAttribArray(coordLoc));
	GLU_CHECK_GLW_CALL(gl, vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, squareTriangleCoords));

	GLU_CHECK_GLW_CALL(gl, drawArrays(GL_TRIANGLES, 0, 6));
	GLU_CHECK_GLW_CALL(gl, disableVertexAttribArray(coordLoc));
	GLU_CHECK_GLW_CALL(gl, bindTexture(GL_TEXTURE_EXTERNAL_OES, 0));

	tcu::Surface refSurface	(reference.getWidth(), reference.getHeight());
	tcu::Surface screen		(reference.getWidth(), reference.getHeight());
	GLU_CHECK_GLW_CALL(gl, readPixels(0, 0, screen.getWidth(), screen.getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, screen.getAccess().getDataPtr()));

	tcu::copy(refSurface.getAccess(), reference.getLevel(0));

	float	threshold	= 0.05f;
	bool	match		= tcu::fuzzyCompare(log, "ComparisonResult", "Image comparison result", refSurface, screen, threshold, tcu::COMPARE_LOG_RESULT);

	return match;
}

bool GLESImageApi::RenderYUVTexture::invokeGLES (GLESImageApi& api, MovePtr<UniqueImage>& img, tcu::Texture2D& reference) const
{
	const glw::Functions&	gl		= api.m_gl;
	tcu::TestLog&			log		= api.getLog();
	Texture					srcTex	(gl);

	DE_ASSERT(reference.isYUVTextureUsed());

	gl.clearColor(0.0, 0.0, 0.0, 0.0);
	gl.viewport(0, 0, reference.getWidth(), reference.getHeight());
	gl.clear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	gl.disable(GL_DEPTH_TEST);

	log << tcu::TestLog::Message << "Rendering EGLImage as GL_TEXTURE_EXTERNAL_OES in context: " << api.m_contextId << tcu::TestLog::EndMessage;
	TCU_CHECK(**img != EGL_NO_IMAGE_KHR);
	GLU_CHECK_GLW_CALL(gl, bindTexture(GL_TEXTURE_EXTERNAL_OES, *srcTex));
	imageTargetExternalTexture(api.m_egl, gl, **img);
	{
		/* init YUV texture with glClear, clear color value in YUV color space */
		glu::Framebuffer		fbo(gl);
		GLU_CHECK_GLW_CALL(gl, bindFramebuffer(GL_FRAMEBUFFER, *fbo));
		GLU_CHECK_GLW_CALL(gl, framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,  GL_TEXTURE_EXTERNAL_OES,*srcTex, 0));
		const tcu::Vec4 colorValues[] =
		{
			tcu::Vec4(0.9f, 0.5f, 0.65f, 1.0f),
			tcu::Vec4(0.5f, 0.7f, 0.65f, 1.0f),
			tcu::Vec4(0.2f, 0.5f, 0.65f, 1.0f),
			tcu::Vec4(0.3f, 0.1f, 0.5f, 1.0f),
			tcu::Vec4(0.8f, 0.2f, 0.3f, 1.0f),
			tcu::Vec4(0.9f, 0.4f, 0.8f, 1.0f),
		};
		tcu::clear(reference.getLevel(0), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
		GLU_CHECK_GLW_CALL(gl, enable(GL_SCISSOR_TEST));
		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(colorValues); ++ndx)
		{
			const tcu::IVec2	size	= tcu::IVec2((int)((float)(DE_LENGTH_OF_ARRAY(colorValues) - ndx) * ((float)reference.getWidth() / float(DE_LENGTH_OF_ARRAY(colorValues)))),
													(int)((float)(DE_LENGTH_OF_ARRAY(colorValues) - ndx) * ((float)reference.getHeight() / float(DE_LENGTH_OF_ARRAY(colorValues)))));

			if (size.x() == 0 || size.y() == 0)
				break;
			GLU_CHECK_GLW_CALL(gl, scissor(0, 0, size.x(), size.y()));

			GLU_CHECK_GLW_CALL(gl, clearColor(colorValues[ndx].x(), colorValues[ndx].y(), colorValues[ndx].z(), colorValues[ndx].w()));
			GLU_CHECK_GLW_CALL(gl, clear(GL_COLOR_BUFFER_BIT));
			GLU_CHECK_GLW_CALL(gl, finish());
			char tmp[4]={"0"};
			GLU_CHECK_GLW_CALL(gl, readPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, (void*)tmp));
			tcu::clear(tcu::getSubregion(reference.getLevel(0), 0, 0, size.x(), size.y()), tcu::Vec4(tmp[0]/(255.0f), tmp[1]/(255.0f), tmp[2]/(255.0f), tmp[3]/(255.0f)));
		}
		GLU_CHECK_GLW_CALL(gl, disable(GL_SCISSOR_TEST));
		GLU_CHECK_GLW_CALL(gl, framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_EXTERNAL_OES, 0, 0));
		GLU_CHECK_GLW_CALL(gl, bindFramebuffer(GL_FRAMEBUFFER, 0));
		GLU_CHECK_GLW_CALL(gl, finish());
	}

	GLU_CHECK_GLW_CALL(gl, texParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
	GLU_CHECK_GLW_CALL(gl, texParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
	GLU_CHECK_GLW_CALL(gl, texParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	GLU_CHECK_GLW_CALL(gl, texParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));

	const char* const vertexShader =
		"attribute highp vec2 a_coord;\n"
		"varying mediump vec2 v_texCoord;\n"
		"void main(void) {\n"
		"\tv_texCoord = vec2((a_coord.x + 1.0) * 0.5, (a_coord.y + 1.0) * 0.5);\n"
		"\tgl_Position = vec4(a_coord, -0.1, 1.0);\n"
		"}\n";

	const char* const fragmentShader =
		"#extension GL_OES_EGL_image_external : require\n"
		"varying mediump vec2 v_texCoord;\n"
		"uniform samplerExternalOES u_sampler;\n"
		"void main(void) {\n"
		"\tmediump vec4 texColor = texture2D(u_sampler, v_texCoord);\n"
		"\tgl_FragColor = vec4(texColor);\n"
		"}";

	Program program(gl, vertexShader, fragmentShader);
	TCU_CHECK(program.isOk());

	GLuint glProgram = program.getProgram();
	GLU_CHECK_GLW_CALL(gl, useProgram(glProgram));

	GLuint coordLoc = gl.getAttribLocation(glProgram, "a_coord");
	TCU_CHECK_MSG((int)coordLoc != -1, "Couldn't find attribute a_coord");

	GLuint samplerLoc = gl.getUniformLocation(glProgram, "u_sampler");
	TCU_CHECK_MSG((int)samplerLoc != (int)-1, "Couldn't find uniform u_sampler");

	GLU_CHECK_GLW_CALL(gl, bindTexture(GL_TEXTURE_EXTERNAL_OES, *srcTex));
	GLU_CHECK_GLW_CALL(gl, uniform1i(samplerLoc, 0));
	GLU_CHECK_GLW_CALL(gl, enableVertexAttribArray(coordLoc));
	GLU_CHECK_GLW_CALL(gl, vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, squareTriangleCoords));

	GLU_CHECK_GLW_CALL(gl, drawArrays(GL_TRIANGLES, 0, 6));
	GLU_CHECK_GLW_CALL(gl, disableVertexAttribArray(coordLoc));
	GLU_CHECK_GLW_CALL(gl, bindTexture(GL_TEXTURE_EXTERNAL_OES, 0));

	tcu::Surface refSurface	(reference.getWidth(), reference.getHeight());
	tcu::Surface screen		(reference.getWidth(), reference.getHeight());
	GLU_CHECK_GLW_CALL(gl, readPixels(0, 0, screen.getWidth(), screen.getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, screen.getAccess().getDataPtr()));

	tcu::copy(refSurface.getAccess(), reference.getLevel(0));

	float	threshold	= 0.05f;
	bool	match		= tcu::fuzzyCompare(log, "ComparisonResult", "Image comparison result", refSurface, screen, threshold, tcu::COMPARE_LOG_RESULT);

	return match;
}

bool GLESImageApi::RenderExternalTextureSamplerArray::invokeGLES (GLESImageApi& api, MovePtr<UniqueImage>& img, tcu::Texture2D& reference) const
{
	const glw::Functions&	gl		= api.m_gl;
	tcu::TestLog&			log		= api.getLog();
	Texture					srcTex	(gl);

	// Branch only taken in TryAll case
	if (reference.getFormat().order == tcu::TextureFormat::DS || reference.getFormat().order == tcu::TextureFormat::D)
		throw IllegalRendererException(); // Skip, GLES2 does not support sampling depth textures
	if (reference.getFormat().order == tcu::TextureFormat::S)
		throw IllegalRendererException(); // Skip, GLES2 does not support sampling stencil textures

	gl.clearColor(0.0, 0.0, 0.0, 0.0);
	gl.viewport(0, 0, reference.getWidth(), reference.getHeight());
	gl.clear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	gl.disable(GL_DEPTH_TEST);

	log << tcu::TestLog::Message << "Rendering EGLImage as GL_TEXTURE_EXTERNAL_OES using sampler array in context: " << api.m_contextId << tcu::TestLog::EndMessage;
	TCU_CHECK(**img != EGL_NO_IMAGE_KHR);

	GLU_CHECK_GLW_CALL(gl, bindTexture(GL_TEXTURE_EXTERNAL_OES, *srcTex));
	imageTargetExternalTexture(api.m_egl, gl, **img);

	GLU_CHECK_GLW_CALL(gl, texParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
	GLU_CHECK_GLW_CALL(gl, texParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
	GLU_CHECK_GLW_CALL(gl, texParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	GLU_CHECK_GLW_CALL(gl, texParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));

	// Texture not associated with an external texture will return (0, 0, 0, 1) when sampled.
	GLuint	emptyTex;
	gl.genTextures(1, &emptyTex);
	gl.activeTexture(GL_TEXTURE1);
	GLU_CHECK_GLW_CALL(gl, bindTexture(GL_TEXTURE_EXTERNAL_OES, emptyTex));

	const char* const vertexShader =
		"attribute highp vec2 a_coord;\n"
		"varying mediump vec2 v_texCoord;\n"
		"void main(void) {\n"
		"\tv_texCoord = vec2((a_coord.x + 1.0) * 0.5, (a_coord.y + 1.0) * 0.5);\n"
		"\tgl_Position = vec4(a_coord, -0.1, 1.0);\n"
		"}\n";

	const char* const fragmentShader =
		"#extension GL_OES_EGL_image_external : require\n"
		"varying mediump vec2 v_texCoord;\n"
		"uniform samplerExternalOES u_sampler[4];\n"
		"void main(void) {\n"
		"\tmediump vec4 texColor = texture2D(u_sampler[2], v_texCoord);\n"
		"\t//These will sample (0, 0, 0, 1) and should not affect the results.\n"
		"\ttexColor += texture2D(u_sampler[0], v_texCoord) - vec4(0, 0, 0, 1);\n"
		"\ttexColor += texture2D(u_sampler[1], v_texCoord) - vec4(0, 0, 0, 1);\n"
		"\ttexColor += texture2D(u_sampler[3], v_texCoord) - vec4(0, 0, 0, 1);\n"
		"\tgl_FragColor = vec4(texColor);\n"
		"}";

	Program program(gl, vertexShader, fragmentShader);
	TCU_CHECK(program.isOk());

	GLuint glProgram = program.getProgram();
	GLU_CHECK_GLW_CALL(gl, useProgram(glProgram));

	GLuint coordLoc = gl.getAttribLocation(glProgram, "a_coord");
	TCU_CHECK_MSG((int)coordLoc != -1, "Couldn't find attribute a_coord");

	GLuint samplerLoc0 = gl.getUniformLocation(glProgram, "u_sampler[0]");
	TCU_CHECK_MSG((int)samplerLoc0 != (int)-1, "Couldn't find uniform u_sampler[0]");
	GLuint samplerLoc1 = gl.getUniformLocation(glProgram, "u_sampler[1]");
	TCU_CHECK_MSG((int)samplerLoc1 != (int)-1, "Couldn't find uniform u_sampler[1]");
	GLuint samplerLoc2 = gl.getUniformLocation(glProgram, "u_sampler[2]");
	TCU_CHECK_MSG((int)samplerLoc2 != (int)-1, "Couldn't find uniform u_sampler[2]");
	GLuint samplerLoc3 = gl.getUniformLocation(glProgram, "u_sampler[3]");
	TCU_CHECK_MSG((int)samplerLoc3 != (int)-1, "Couldn't find uniform u_sampler[3]");

	gl.activeTexture(GL_TEXTURE0);
	GLU_CHECK_GLW_CALL(gl, bindTexture(GL_TEXTURE_EXTERNAL_OES, *srcTex));
	// One sampler reads a gradient and others opaque black.
	GLU_CHECK_GLW_CALL(gl, uniform1i(samplerLoc0, 1));
	GLU_CHECK_GLW_CALL(gl, uniform1i(samplerLoc1, 1));
	GLU_CHECK_GLW_CALL(gl, uniform1i(samplerLoc2, 0));
	GLU_CHECK_GLW_CALL(gl, uniform1i(samplerLoc3, 1));
	GLU_CHECK_GLW_CALL(gl, enableVertexAttribArray(coordLoc));
	GLU_CHECK_GLW_CALL(gl, vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, squareTriangleCoords));

	GLU_CHECK_GLW_CALL(gl, drawArrays(GL_TRIANGLES, 0, 6));
	GLU_CHECK_GLW_CALL(gl, disableVertexAttribArray(coordLoc));
	GLU_CHECK_GLW_CALL(gl, bindTexture(GL_TEXTURE_EXTERNAL_OES, 0));
	gl.activeTexture(GL_TEXTURE1);
	GLU_CHECK_GLW_CALL(gl, bindTexture(GL_TEXTURE_EXTERNAL_OES, 0));
	gl.deleteTextures(1, &emptyTex);
	gl.activeTexture(GL_TEXTURE0);

	tcu::Surface refSurface	(reference.getWidth(), reference.getHeight());
	tcu::Surface screen		(reference.getWidth(), reference.getHeight());
	GLU_CHECK_GLW_CALL(gl, readPixels(0, 0, screen.getWidth(), screen.getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, screen.getAccess().getDataPtr()));

	tcu::copy(refSurface.getAccess(), reference.getLevel(0));

	float	threshold	= 0.05f;
	bool	match		= tcu::fuzzyCompare(log, "ComparisonResult", "Image comparison result", refSurface, screen, threshold, tcu::COMPARE_LOG_RESULT);

	return match;
}

bool GLESImageApi::RenderDepthbuffer::invokeGLES (GLESImageApi& api, MovePtr<UniqueImage>& img, tcu::Texture2D& reference) const
{
	const glw::Functions&	gl					= api.m_gl;
	tcu::TestLog&			log					= api.getLog();
	Framebuffer				framebuffer			(gl);
	Renderbuffer			renderbufferColor	(gl);
	Renderbuffer			renderbufferDepth	(gl);
	const tcu::RGBA			compareThreshold	(32, 32, 32, 32); // layer colors are far apart, large thresholds are ok

	// Branch only taken in TryAll case
	if (reference.getFormat().order != tcu::TextureFormat::DS && reference.getFormat().order != tcu::TextureFormat::D)
		throw IllegalRendererException(); // Skip, interpreting non-depth data as depth data is not meaningful

	log << tcu::TestLog::Message << "Rendering with depth buffer" << tcu::TestLog::EndMessage;

	GLU_CHECK_GLW_CALL(gl, bindFramebuffer(GL_FRAMEBUFFER, *framebuffer));

	GLU_CHECK_GLW_CALL(gl, bindRenderbuffer(GL_RENDERBUFFER, *renderbufferColor));
	GLU_CHECK_GLW_CALL(gl, renderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, reference.getWidth(), reference.getHeight()));
	framebufferRenderbuffer(gl, GL_COLOR_ATTACHMENT0, *renderbufferColor);

	GLU_CHECK_GLW_CALL(gl, bindRenderbuffer(GL_RENDERBUFFER, *renderbufferDepth));
	imageTargetRenderbuffer(api.m_egl, gl, **img);
	framebufferRenderbuffer(gl, GL_DEPTH_ATTACHMENT, *renderbufferDepth);
	GLU_CHECK_GLW_CALL(gl, bindRenderbuffer(GL_RENDERBUFFER, 0));

	GLU_CHECK_GLW_CALL(gl, viewport(0, 0, reference.getWidth(), reference.getHeight()));

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

	Program program(gl, vertexShader, fragmentShader);
	TCU_CHECK(program.isOk());

	GLuint glProgram = program.getProgram();
	GLU_CHECK_GLW_CALL(gl, useProgram(glProgram));

	GLuint coordLoc = gl.getAttribLocation(glProgram, "a_coord");
	TCU_CHECK_MSG((int)coordLoc != -1, "Couldn't find attribute a_coord");

	GLuint colorLoc = gl.getUniformLocation(glProgram, "u_color");
	TCU_CHECK_MSG((int)colorLoc != (int)-1, "Couldn't find uniform u_color");

	GLuint depthLoc = gl.getUniformLocation(glProgram, "u_depth");
	TCU_CHECK_MSG((int)depthLoc != (int)-1, "Couldn't find uniform u_depth");

	GLU_CHECK_GLW_CALL(gl, clearColor(0.5f, 1.0f, 0.5f, 1.0f));
	GLU_CHECK_GLW_CALL(gl, clear(GL_COLOR_BUFFER_BIT));

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

	GLU_CHECK_GLW_CALL(gl, enableVertexAttribArray(coordLoc));
	GLU_CHECK_GLW_CALL(gl, vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, squareTriangleCoords));

	GLU_CHECK_GLW_CALL(gl, enable(GL_DEPTH_TEST));
	GLU_CHECK_GLW_CALL(gl, depthFunc(GL_LESS));
	GLU_CHECK_GLW_CALL(gl, depthMask(GL_FALSE));

	for (int level = 0; level < DE_LENGTH_OF_ARRAY(depthLevelColors); level++)
	{
		const tcu::Vec4	color		= depthLevelColors[level];
		const float		clipDepth	= ((float)(level + 1) * 0.1f) * 2.0f - 1.0f; // depth in clip coords

		GLU_CHECK_GLW_CALL(gl, uniform4f(colorLoc, color.x(), color.y(), color.z(), color.w()));
		GLU_CHECK_GLW_CALL(gl, uniform1f(depthLoc, clipDepth));
		GLU_CHECK_GLW_CALL(gl, drawArrays(GL_TRIANGLES, 0, 6));
	}

	GLU_CHECK_GLW_CALL(gl, depthMask(GL_TRUE));
	GLU_CHECK_GLW_CALL(gl, disable(GL_DEPTH_TEST));
	GLU_CHECK_GLW_CALL(gl, disableVertexAttribArray(coordLoc));

	const ConstPixelBufferAccess&	refAccess		= reference.getLevel(0);
	tcu::Surface					screen			(reference.getWidth(), reference.getHeight());
	tcu::Surface					referenceScreen	(reference.getWidth(), reference.getHeight());

	gl.readPixels(0, 0, screen.getWidth(), screen.getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, screen.getAccess().getDataPtr());

	for (int y = 0; y < reference.getHeight(); y++)
	{
		for (int x = 0; x < reference.getWidth(); x++)
		{
			tcu::Vec4 result = tcu::Vec4(0.5f, 1.0f, 0.5f, 1.0f);

			for (int level = 0; level < DE_LENGTH_OF_ARRAY(depthLevelColors); level++)
			{
				if ((float)(level + 1) * 0.1f < refAccess.getPixDepth(x, y))
					result = depthLevelColors[level];
			}

			referenceScreen.getAccess().setPixel(result, x, y);
		}
	}

	GLU_CHECK_GLW_CALL(gl, bindFramebuffer(GL_FRAMEBUFFER, 0));
	GLU_CHECK_GLW_CALL(gl, finish());

	return tcu::pixelThresholdCompare(log, "Depth buffer rendering result", "Result from rendering with depth buffer", referenceScreen, screen, compareThreshold, tcu::COMPARE_LOG_RESULT);
}

bool GLESImageApi::RenderStencilbuffer::invokeGLES (GLESImageApi& api, MovePtr<UniqueImage>& img, tcu::Texture2D& reference) const
{
	// Branch only taken in TryAll case
	if (reference.getFormat().order != tcu::TextureFormat::DS && reference.getFormat().order != tcu::TextureFormat::S)
		throw IllegalRendererException(); // Skip, interpreting non-stencil data as stencil data is not meaningful

	const glw::Functions&	gl					= api.m_gl;
	tcu::TestLog&			log					= api.getLog();
	Framebuffer				framebuffer			(gl);
	Renderbuffer			renderbufferColor	(gl);
	Renderbuffer			renderbufferStencil (gl);
	const tcu::RGBA			compareThreshold	(32, 32, 32, 32); // layer colors are far apart, large thresholds are ok
	const deUint32			numStencilBits		= tcu::getTextureFormatBitDepth(tcu::getEffectiveDepthStencilTextureFormat(reference.getLevel(0).getFormat(), tcu::Sampler::MODE_STENCIL)).x();
	const deUint32			maxStencil			= deBitMask32(0, numStencilBits);

	log << tcu::TestLog::Message << "Rendering with stencil buffer" << tcu::TestLog::EndMessage;

	GLU_CHECK_GLW_CALL(gl, bindFramebuffer(GL_FRAMEBUFFER, *framebuffer));

	GLU_CHECK_GLW_CALL(gl, bindRenderbuffer(GL_RENDERBUFFER, *renderbufferColor));
	GLU_CHECK_GLW_CALL(gl, renderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, reference.getWidth(), reference.getHeight()));
	framebufferRenderbuffer(gl, GL_COLOR_ATTACHMENT0, *renderbufferColor);

	GLU_CHECK_GLW_CALL(gl, bindRenderbuffer(GL_RENDERBUFFER, *renderbufferStencil));
	imageTargetRenderbuffer(api.m_egl, gl, **img);
	framebufferRenderbuffer(gl, GL_STENCIL_ATTACHMENT, *renderbufferStencil);
	GLU_CHECK_GLW_CALL(gl, bindRenderbuffer(GL_RENDERBUFFER, 0));

	GLU_CHECK_GLW_CALL(gl, viewport(0, 0, reference.getWidth(), reference.getHeight()));

	// Render
	const char* vertexShader =
		"attribute highp vec2 a_coord;\n"
		"void main(void) {\n"
		"\tgl_Position = vec4(a_coord, 0.0, 1.0);\n"
		"}\n";

	const char* fragmentShader =
		"uniform mediump vec4 u_color;\n"
		"void main(void) {\n"
		"\tgl_FragColor = u_color;\n"
		"}";

	Program program(gl, vertexShader, fragmentShader);
	TCU_CHECK(program.isOk());

	GLuint glProgram = program.getProgram();
	GLU_CHECK_GLW_CALL(gl, useProgram(glProgram));

	GLuint coordLoc = gl.getAttribLocation(glProgram, "a_coord");
	TCU_CHECK_MSG((int)coordLoc != -1, "Couldn't find attribute a_coord");

	GLuint colorLoc = gl.getUniformLocation(glProgram, "u_color");
	TCU_CHECK_MSG((int)colorLoc != (int)-1, "Couldn't find uniform u_color");

	GLU_CHECK_GLW_CALL(gl, clearColor(0.5f, 1.0f, 0.5f, 1.0f));
	GLU_CHECK_GLW_CALL(gl, clear(GL_COLOR_BUFFER_BIT));

	tcu::Vec4 stencilLevelColors[] = {
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

	GLU_CHECK_GLW_CALL(gl, enableVertexAttribArray(coordLoc));
	GLU_CHECK_GLW_CALL(gl, vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, squareTriangleCoords));

	GLU_CHECK_GLW_CALL(gl, enable(GL_STENCIL_TEST));
	GLU_CHECK_GLW_CALL(gl, stencilOp(GL_KEEP, GL_KEEP, GL_KEEP));

	for (int level = 0; level < DE_LENGTH_OF_ARRAY(stencilLevelColors); level++)
	{
		const tcu::Vec4	color	= stencilLevelColors[level];
		const int		stencil	= (int)(((float)(level + 1) * 0.1f) * (float)maxStencil);

		GLU_CHECK_GLW_CALL(gl, stencilFunc(GL_LESS, stencil, 0xFFFFFFFFu));
		GLU_CHECK_GLW_CALL(gl, uniform4f(colorLoc, color.x(), color.y(), color.z(), color.w()));
		GLU_CHECK_GLW_CALL(gl, drawArrays(GL_TRIANGLES, 0, 6));
	}

	GLU_CHECK_GLW_CALL(gl, disable(GL_STENCIL_TEST));
	GLU_CHECK_GLW_CALL(gl, disableVertexAttribArray(coordLoc));

	const ConstPixelBufferAccess&	refAccess		= reference.getLevel(0);
	tcu::Surface					screen			(reference.getWidth(), reference.getHeight());
	tcu::Surface					referenceScreen	(reference.getWidth(), reference.getHeight());

	gl.readPixels(0, 0, screen.getWidth(), screen.getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, screen.getAccess().getDataPtr());

	for (int y = 0; y < reference.getHeight(); y++)
	for (int x = 0; x < reference.getWidth(); x++)
	{
		tcu::Vec4 result = tcu::Vec4(0.5f, 1.0f, 0.5f, 1.0f);

		for (int level = 0; level < DE_LENGTH_OF_ARRAY(stencilLevelColors); level++)
		{
			const int levelStencil = (int)(((float)(level + 1) * 0.1f) * (float)maxStencil);
			if (levelStencil < refAccess.getPixStencil(x, y))
				result = stencilLevelColors[level];
		}

		referenceScreen.getAccess().setPixel(result, x, y);
	}

	GLU_CHECK_GLW_CALL(gl, bindFramebuffer(GL_FRAMEBUFFER, 0));
	GLU_CHECK_GLW_CALL(gl, finish());

	return tcu::pixelThresholdCompare(log, "StencilResult", "Result from rendering with stencil buffer", referenceScreen, screen, compareThreshold, tcu::COMPARE_LOG_RESULT);
}

bool GLESImageApi::RenderReadPixelsRenderbuffer::invokeGLES (GLESImageApi& api, MovePtr<UniqueImage>& img, tcu::Texture2D& reference) const
{
	switch (glu::getInternalFormat(reference.getFormat()))
	{
		case GL_RGBA4:
		case GL_RGB5_A1:
		case GL_RGB565:
			break;
		default:
			// Skip, not in the list of allowed render buffer formats for GLES.
			throw tcu::NotSupportedError("Image format not allowed for glReadPixels.");
	}

	const glw::Functions&	gl				= api.m_gl;
	const tcu::IVec4		bitDepth		= tcu::getTextureFormatMantissaBitDepth(reference.getFormat());
	const tcu::IVec4		threshold		(2 * (tcu::IVec4(1) << (tcu::IVec4(8) - bitDepth)));
	const tcu::RGBA			threshold8		((deUint8)(de::clamp(threshold[0], 0, 255)), (deUint8)(de::clamp(threshold[1], 0, 255)), (deUint8)(de::clamp(threshold[2], 0, 255)), (deUint8)(de::clamp(threshold[3], 0, 255)));
	tcu::TestLog&			log				= api.getLog();
	Framebuffer				framebuffer		(gl);
	Renderbuffer			renderbuffer	(gl);
	tcu::Surface			screen			(reference.getWidth(), reference.getHeight());
	tcu::Surface			refSurface		(reference.getWidth(), reference.getHeight());

	log << tcu::TestLog::Message << "Reading with ReadPixels from renderbuffer" << tcu::TestLog::EndMessage;

	GLU_CHECK_GLW_CALL(gl, bindFramebuffer(GL_FRAMEBUFFER, *framebuffer));
	GLU_CHECK_GLW_CALL(gl, bindRenderbuffer(GL_RENDERBUFFER, *renderbuffer));
	imageTargetRenderbuffer(api.m_egl, gl, **img);

	GLU_EXPECT_NO_ERROR(gl.getError(), "imageTargetRenderbuffer");
	framebufferRenderbuffer(gl, GL_COLOR_ATTACHMENT0, *renderbuffer);
	GLU_EXPECT_NO_ERROR(gl.getError(), "framebufferRenderbuffer");

	GLU_CHECK_GLW_CALL(gl, viewport(0, 0, reference.getWidth(), reference.getHeight()));

	GLU_CHECK_GLW_CALL(gl, readPixels(0, 0, screen.getWidth(), screen.getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, screen.getAccess().getDataPtr()));

	GLU_CHECK_GLW_CALL(gl, bindFramebuffer(GL_FRAMEBUFFER, 0));
	GLU_CHECK_GLW_CALL(gl, bindRenderbuffer(GL_RENDERBUFFER, 0));
	GLU_CHECK_GLW_CALL(gl, finish());

	tcu::copy(refSurface.getAccess(), reference.getLevel(0));

	return tcu::pixelThresholdCompare(log, "Renderbuffer read", "Result from reading renderbuffer", refSurface, screen, threshold8, tcu::COMPARE_LOG_RESULT);

}

bool GLESImageApi::RenderTryAll::invokeGLES (GLESImageApi& api, MovePtr<UniqueImage>& img, tcu::Texture2D& reference) const
{
	bool											foundSupported			= false;
	tcu::TestLog&									log						= api.getLog();
	GLESImageApi::RenderTexture2D					renderTex2D;
	GLESImageApi::RenderExternalTexture				renderExternal;
	GLESImageApi::RenderExternalTextureSamplerArray	renderExternalSamplerArray;
	GLESImageApi::RenderReadPixelsRenderbuffer		renderReadPixels;
	GLESImageApi::RenderDepthbuffer					renderDepth;
	GLESImageApi::RenderStencilbuffer				renderStencil;
	Action*											actions[]				= { &renderTex2D, &renderExternal, &renderExternalSamplerArray, &renderReadPixels, &renderDepth, &renderStencil };

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(actions); ++ndx)
	{
		try
		{
			if (!actions[ndx]->invoke(api, img, reference))
				return false;

			foundSupported = true;
		}
		catch (const tcu::NotSupportedError& error)
		{
			log << tcu::TestLog::Message << error.what() << tcu::TestLog::EndMessage;
		}
		catch (const IllegalRendererException&)
		{
			// not valid renderer
		}
	}

	if (!foundSupported)
		throw tcu::NotSupportedError("Rendering not supported", "", __FILE__, __LINE__);

	return true;
}

bool GLESImageApi::ModifyTexSubImage::invokeGLES (GLESImageApi& api, MovePtr<UniqueImage>& img, tcu::Texture2D& reference) const
{
	const glw::Functions&	gl		= api.m_gl;
	tcu::TestLog&			log		= api.getLog();
	glu::Texture			srcTex	(gl);
	const int				xOffset	= 8;
	const int				yOffset	= 16;
	const int				xSize	= de::clamp(16, 0, reference.getWidth() - xOffset);
	const int				ySize	= de::clamp(16, 0, reference.getHeight() - yOffset);
	tcu::Texture2D			src		(glu::mapGLTransferFormat(m_format, m_type), xSize, ySize);

	log << tcu::TestLog::Message << "Modifying EGLImage with gl.texSubImage2D" << tcu::TestLog::EndMessage;

	src.allocLevel(0);
	tcu::fillWithComponentGradients(src.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

	GLU_CHECK_GLW_CALL(gl, bindTexture(GL_TEXTURE_2D, *srcTex));
	imageTargetTexture2D(api.m_egl, gl, **img);
	GLU_CHECK_GLW_CALL(gl, texSubImage2D(GL_TEXTURE_2D, 0, xOffset, yOffset, src.getWidth(), src.getHeight(), m_format, m_type, src.getLevel(0).getDataPtr()));
	GLU_CHECK_GLW_CALL(gl, bindTexture(GL_TEXTURE_2D, 0));
	GLU_CHECK_GLW_CALL(gl, finish());

	tcu::copy(tcu::getSubregion(reference.getLevel(0), xOffset, yOffset, 0, xSize, ySize, 1), src.getLevel(0));

	return true;
}

bool GLESImageApi::ModifyRenderbuffer::invokeGLES (GLESImageApi& api, MovePtr<UniqueImage>& img, tcu::Texture2D& reference) const
{
	const glw::Functions&	gl				= api.m_gl;
	tcu::TestLog&			log				= api.getLog();
	glu::Framebuffer		framebuffer		(gl);
	glu::Renderbuffer		renderbuffer	(gl);

	log << tcu::TestLog::Message << "Modifying EGLImage with glClear to renderbuffer" << tcu::TestLog::EndMessage;

	GLU_CHECK_GLW_CALL(gl, bindFramebuffer(GL_FRAMEBUFFER, *framebuffer));
	GLU_CHECK_GLW_CALL(gl, bindRenderbuffer(GL_RENDERBUFFER, *renderbuffer));

	imageTargetRenderbuffer(api.m_egl, gl, **img);

	initializeRbo(api, *renderbuffer, reference);

	GLU_CHECK_GLW_CALL(gl, bindFramebuffer(GL_FRAMEBUFFER, 0));
	GLU_CHECK_GLW_CALL(gl, bindRenderbuffer(GL_RENDERBUFFER, 0));

	GLU_CHECK_GLW_CALL(gl, finish());

	return true;
}

void GLESImageApi::ModifyRenderbufferClearColor::initializeRbo (GLESImageApi& api, GLuint renderbuffer, tcu::Texture2D& reference) const
{
	const glw::Functions&	gl		= api.m_gl;

	framebufferRenderbuffer(gl, GL_COLOR_ATTACHMENT0, renderbuffer);

	GLU_CHECK_GLW_CALL(gl, viewport(0, 0, reference.getWidth(), reference.getHeight()));
	GLU_CHECK_GLW_CALL(gl, clearColor(m_color.x(), m_color.y(), m_color.z(), m_color.w()));
	GLU_CHECK_GLW_CALL(gl, clear(GL_COLOR_BUFFER_BIT));

	tcu::clear(reference.getLevel(0), m_color);
}

void GLESImageApi::ModifyRenderbufferClearDepth::initializeRbo (GLESImageApi& api, GLuint renderbuffer, tcu::Texture2D& reference) const
{
	const glw::Functions&	gl		= api.m_gl;

	framebufferRenderbuffer(gl, GL_DEPTH_ATTACHMENT, renderbuffer);

	GLU_CHECK_GLW_CALL(gl, viewport(0, 0, reference.getWidth(), reference.getHeight()));
	GLU_CHECK_GLW_CALL(gl, clearDepthf(m_depth));
	GLU_CHECK_GLW_CALL(gl, clear(GL_DEPTH_BUFFER_BIT));

	tcu::clearDepth(reference.getLevel(0), m_depth);
}

void GLESImageApi::ModifyRenderbufferClearStencil::initializeRbo (GLESImageApi& api, GLuint renderbuffer, tcu::Texture2D& reference) const
{
	const glw::Functions&	gl		= api.m_gl;

	framebufferRenderbuffer(gl, GL_STENCIL_ATTACHMENT, renderbuffer);

	GLU_CHECK_GLW_CALL(gl, viewport(0, 0, reference.getWidth(), reference.getHeight()));
	GLU_CHECK_GLW_CALL(gl, clearStencil(m_stencil));
	GLU_CHECK_GLW_CALL(gl, clear(GL_STENCIL_BUFFER_BIT));

	tcu::clearStencil(reference.getLevel(0), m_stencil);
}

class ImageFormatCase : public TestCase, private glu::CallLogWrapper
{
public:
							ImageFormatCase		(EglTestContext& eglTestCtx, const TestSpec& spec);
							~ImageFormatCase	(void);

	void					init				(void);
	void					deinit				(void);
	IterateResult			iterate				(void);
	void					checkExtensions		(void);

private:
	EGLConfig				getConfig			(void);

	const TestSpec			m_spec;

	vector<ImageApi*>		m_apiContexts;

	EGLDisplay				m_display;
	eglu::NativeWindow*		m_window;
	EGLSurface				m_surface;
	EGLConfig				m_config;
	int						m_curIter;
	MovePtr<UniqueImage>	m_img;
	tcu::Texture2D			m_refImg;
	glw::Functions			m_gl;
};

EGLConfig ImageFormatCase::getConfig (void)
{
	const GLint		glesApi			= m_spec.contexts[0] == TestSpec::API_GLES3 ? EGL_OPENGL_ES3_BIT : EGL_OPENGL_ES2_BIT;
	const EGLint	attribList[]	=
	{
		EGL_RENDERABLE_TYPE,	glesApi,
		EGL_SURFACE_TYPE,		EGL_WINDOW_BIT,
		EGL_RED_SIZE,			8,
		EGL_BLUE_SIZE,			8,
		EGL_GREEN_SIZE,			8,
		EGL_ALPHA_SIZE,			8,
		EGL_DEPTH_SIZE,			8,
		EGL_NONE
	};

	return eglu::chooseSingleConfig(m_eglTestCtx.getLibrary(), m_display, attribList);
}

ImageFormatCase::ImageFormatCase (EglTestContext& eglTestCtx, const TestSpec& spec)
	: TestCase				(eglTestCtx, spec.name.c_str(), spec.desc.c_str())
	, glu::CallLogWrapper	(m_gl, eglTestCtx.getTestContext().getLog())
	, m_spec				(spec)
	, m_display				(EGL_NO_DISPLAY)
	, m_window				(DE_NULL)
	, m_surface				(EGL_NO_SURFACE)
	, m_config				(0)
	, m_curIter				(0)
	, m_refImg				(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8), 1, 1)
{
}

ImageFormatCase::~ImageFormatCase (void)
{
	deinit();
}

void ImageFormatCase::checkExtensions (void)
{
	const Library&			egl		= m_eglTestCtx.getLibrary();
	const EGLDisplay		dpy		= m_display;
	set<string>				exts;
	const vector<string>	glExts	= de::splitString((const char*) m_gl.getString(GL_EXTENSIONS));
	const vector<string>	eglExts	= eglu::getDisplayExtensions(egl, dpy);

	exts.insert(glExts.begin(), glExts.end());
	exts.insert(eglExts.begin(), eglExts.end());

	if (eglu::getVersion(egl, dpy) >= eglu::Version(1, 5))
	{
		// EGL 1.5 has built-in support for EGLImage and GL sources
		exts.insert("EGL_KHR_image_base");
		exts.insert("EGL_KHR_gl_texture_2D_image");
		exts.insert("EGL_KHR_gl_texture_cubemap_image");
		exts.insert("EGL_KHR_gl_renderbuffer_image");
	}

	if (!de::contains(exts, "EGL_KHR_image_base") && !de::contains(exts, "EGL_KHR_image"))
	{
		getLog() << tcu::TestLog::Message
				 << "EGL version is under 1.5 and neither EGL_KHR_image nor EGL_KHR_image_base is supported."
				 << "One should be supported."
				 << tcu::TestLog::EndMessage;
		TCU_THROW(NotSupportedError, "Extension not supported: EGL_KHR_image_base");
	}

	for (int operationNdx = 0; operationNdx < (int)m_spec.operations.size(); operationNdx++)
	{
		const TestSpec::Operation&	op	= m_spec.operations[operationNdx];
		const string				ext	= op.action->getRequiredExtension();

		if (!de::contains(exts, ext))
			TCU_THROW_EXPR(NotSupportedError, "Extension not supported", ext.c_str());
	}
}

void ImageFormatCase::init (void)
{
	const Library&						egl				= m_eglTestCtx.getLibrary();
	const eglu::NativeWindowFactory&	windowFactory	= eglu::selectNativeWindowFactory(m_eglTestCtx.getNativeDisplayFactory(), m_testCtx.getCommandLine());

	try
	{
		m_display	= eglu::getAndInitDisplay(m_eglTestCtx.getNativeDisplay());

		// GLES3 requires either EGL 1.5 or EGL_KHR_create_context extension.
		if (m_spec.contexts[0] == TestSpec::API_GLES3 && eglu::getVersion(egl, m_display) < eglu::Version(1, 5))
		{
			set<string>				exts;
			const vector<string>	eglExts	= eglu::getDisplayExtensions(egl, m_display);
			exts.insert(eglExts.begin(), eglExts.end());

			if (!de::contains(exts, "EGL_KHR_create_context"))
			{
				getLog() << tcu::TestLog::Message
						 << "EGL version is under 1.5 and the test is using OpenGL ES 3.2."
						 << "This requires EGL_KHR_create_context extension."
						 << tcu::TestLog::EndMessage;
				TCU_THROW(NotSupportedError, "Extension not supported: EGL_KHR_create_context");
			}
		}

		m_config	= getConfig();
		m_window	= windowFactory.createWindow(&m_eglTestCtx.getNativeDisplay(), m_display, m_config, DE_NULL, eglu::WindowParams(480, 480, eglu::parseWindowVisibility(m_testCtx.getCommandLine())));
		m_surface	= eglu::createWindowSurface(m_eglTestCtx.getNativeDisplay(), *m_window, m_display, m_config, DE_NULL);

		{
			const char*	extensions[]	= { "GL_OES_EGL_image" };
			int			major			= 2;
			int			minor			= 0;

			if (m_spec.contexts[0] == TestSpec::API_GLES3)
			{
				major = 3;
				minor = 2;
			}
			m_eglTestCtx.initGLFunctions(&m_gl, glu::ApiType::es(major, minor), DE_LENGTH_OF_ARRAY(extensions), &extensions[0]);
		}

		for (int contextNdx = 0; contextNdx < (int)m_spec.contexts.size(); contextNdx++)
		{
			ImageApi* api = DE_NULL;
			switch (m_spec.contexts[contextNdx])
			{
				case TestSpec::API_GLES2:
				{
					api = new GLESImageApi(egl, m_gl, contextNdx, getLog(), m_display, m_surface, m_config, 2);
					break;
				}

				case TestSpec::API_GLES3:
				{
					api = new GLESImageApi(egl, m_gl, contextNdx, getLog(), m_display, m_surface, m_config, 3);
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
	catch (...)
	{
		deinit();
		throw;
	}
}

void ImageFormatCase::deinit (void)
{
	const Library& egl = m_eglTestCtx.getLibrary();

	m_img.clear();

	for (int contexNdx = 0 ; contexNdx < (int)m_apiContexts.size(); contexNdx++)
		delete m_apiContexts[contexNdx];

	m_apiContexts.clear();

	if (m_surface != EGL_NO_SURFACE)
	{
		egl.destroySurface(m_display, m_surface);
		m_surface = EGL_NO_SURFACE;
	}

	delete m_window;
	m_window = DE_NULL;

	if (m_display != EGL_NO_DISPLAY)
	{
		egl.terminate(m_display);
		m_display = EGL_NO_DISPLAY;
	}
}

TestCase::IterateResult ImageFormatCase::iterate (void)
{
	const TestSpec::Operation&	op		= m_spec.operations[m_curIter++];
	ImageApi&					api		= *m_apiContexts[op.apiIndex];
	const bool					isOk	= op.action->invoke(api, m_img, m_refImg);

	if (isOk && m_curIter < (int)m_spec.operations.size())
		return CONTINUE;
	else if (isOk)
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	else
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");

	return STOP;
}

struct LabeledAction
{
	string			label;
	MovePtr<Action>	action;
};

// A simple vector mockup that we need because MovePtr isn't copy-constructible.
struct LabeledActions
{
					LabeledActions	(void) : m_numActions(0){}
	LabeledAction&	operator[]		(int ndx)				{ DE_ASSERT(0 <= ndx && ndx < m_numActions); return m_actions[ndx]; }
	void			add				(const string& label, MovePtr<Action> action);
	int				size			(void) const			{ return m_numActions; }
private:
	LabeledAction	m_actions[64];
	int				m_numActions;
};

void LabeledActions::add (const string& label, MovePtr<Action> action)
{
	DE_ASSERT(m_numActions < DE_LENGTH_OF_ARRAY(m_actions));
	m_actions[m_numActions].label = label;
	m_actions[m_numActions].action = action;
	++m_numActions;
}

class ImageTests : public TestCaseGroup
{
protected:
					ImageTests						(EglTestContext& eglTestCtx, const string& name, const string& desc)
						: TestCaseGroup(eglTestCtx, name.c_str(), desc.c_str()) {}

	void			addCreateTexture				(const string& name, EGLenum source, GLenum internalFormat, GLenum format, GLenum type);
	void			addCreateRenderbuffer			(const string& name, GLenum format);
	void			addCreateAndroidNative			(const string& name, GLenum format, bool isYUV);
	void			addCreateAndroidNativeArray		(const string& name, GLenum format, deUint32 numLayers);
	void			addCreateTexture2DActions		(const string& prefix);
	void			addCreateTextureCubemapActions	(const string& suffix, GLenum internalFormat, GLenum format, GLenum type);
	void			addCreateRenderbufferActions	(void);
	void			addCreateAndroidNativeActions	(void);

	LabeledActions	m_createActions;
};

void ImageTests::addCreateTexture (const string& name, EGLenum source, GLenum internalFormat, GLenum format, GLenum type)
{
	m_createActions.add(name, MovePtr<Action>(new GLESImageApi::Create(createTextureImageSource(source, internalFormat, format, type))));
}

void ImageTests::addCreateRenderbuffer (const string& name, GLenum format)
{
	m_createActions.add(name, MovePtr<Action>(new GLESImageApi::Create(createRenderbufferImageSource(format))));
}

void ImageTests::addCreateAndroidNative (const string& name, GLenum format, bool isYUV = false)
{
	m_createActions.add(name, MovePtr<Action>(new GLESImageApi::Create(createAndroidNativeImageSource(format, 1u, isYUV))));
}

void ImageTests::addCreateAndroidNativeArray (const string& name, GLenum format, deUint32 numLayers)
{
	m_createActions.add(name, MovePtr<Action>(new GLESImageApi::Create(createAndroidNativeImageSource(format, numLayers, false), numLayers)));
}

void ImageTests::addCreateTexture2DActions (const string& prefix)
{
	addCreateTexture(prefix + "rgb8",		EGL_GL_TEXTURE_2D_KHR,	GL_RGB,		GL_RGB,		GL_UNSIGNED_BYTE);
	addCreateTexture(prefix + "rgb565",		EGL_GL_TEXTURE_2D_KHR,	GL_RGB,		GL_RGB,		GL_UNSIGNED_SHORT_5_6_5);
	addCreateTexture(prefix + "rgba8",		EGL_GL_TEXTURE_2D_KHR,	GL_RGBA,	GL_RGBA,	GL_UNSIGNED_BYTE);
	addCreateTexture(prefix + "rgb5_a1",	EGL_GL_TEXTURE_2D_KHR,	GL_RGBA,	GL_RGBA,	GL_UNSIGNED_SHORT_5_5_5_1);
	addCreateTexture(prefix + "rgba4",		EGL_GL_TEXTURE_2D_KHR,	GL_RGBA,	GL_RGBA,	GL_UNSIGNED_SHORT_4_4_4_4);
}

void ImageTests::addCreateTextureCubemapActions (const string& suffix, GLenum internalFormat, GLenum format, GLenum type)
{
	addCreateTexture("cubemap_positive_x" + suffix,	EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR, internalFormat,	format,	type);
	addCreateTexture("cubemap_positive_y" + suffix,	EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR, internalFormat,	format,	type);
	addCreateTexture("cubemap_positive_z" + suffix,	EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR, internalFormat,	format,	type);
	addCreateTexture("cubemap_negative_x" + suffix,	EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR, internalFormat,	format,	type);
	addCreateTexture("cubemap_negative_y" + suffix,	EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR, internalFormat,	format,	type);
	addCreateTexture("cubemap_negative_z" + suffix,	EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR, internalFormat,	format,	type);
}

void ImageTests::addCreateRenderbufferActions (void)
{
	addCreateRenderbuffer("renderbuffer_rgba4",		GL_RGBA4);
	addCreateRenderbuffer("renderbuffer_rgb5_a1",	GL_RGB5_A1);
	addCreateRenderbuffer("renderbuffer_rgb565",	GL_RGB565);
	addCreateRenderbuffer("renderbuffer_depth16",	GL_DEPTH_COMPONENT16);
	addCreateRenderbuffer("renderbuffer_stencil",	GL_STENCIL_INDEX8);
}

void ImageTests::addCreateAndroidNativeActions (void)
{
	addCreateAndroidNative("android_native_rgba4",		GL_RGBA4);
	addCreateAndroidNative("android_native_rgb5_a1",	GL_RGB5_A1);
	addCreateAndroidNative("android_native_rgb565",		GL_RGB565);
	addCreateAndroidNative("android_native_rgb8",		GL_RGB8);
	addCreateAndroidNative("android_native_rgba8",		GL_RGBA8);
	addCreateAndroidNative("android_native_d16",		GL_DEPTH_COMPONENT16);
	addCreateAndroidNative("android_native_d24",		GL_DEPTH_COMPONENT24);
	addCreateAndroidNative("android_native_d24s8",		GL_DEPTH24_STENCIL8);
	addCreateAndroidNative("android_native_d32f",		GL_DEPTH_COMPONENT32F);
	addCreateAndroidNative("android_native_d32fs8",		GL_DEPTH32F_STENCIL8);
	addCreateAndroidNative("android_native_rgb10a2",	GL_RGB10_A2);
	addCreateAndroidNative("android_native_rgba16f",	GL_RGBA16F);
	addCreateAndroidNative("android_native_s8",			GL_STENCIL_INDEX8);
	addCreateAndroidNative("android_native_yuv420",		GL_RGBA8, true);

	addCreateAndroidNativeArray("android_native_array_rgba4",	GL_RGBA4,	4u);
	addCreateAndroidNativeArray("android_native_array_rgb5_a1",	GL_RGB5_A1,	4u);
	addCreateAndroidNativeArray("android_native_array_rgb565",	GL_RGB565,	4u);
	addCreateAndroidNativeArray("android_native_array_rgb8",	GL_RGB8,	4u);
	addCreateAndroidNativeArray("android_native_array_rgba8",	GL_RGBA8,	4u);
}

class RenderTests : public ImageTests
{
protected:
											RenderTests				(EglTestContext& eglTestCtx, const string& name, const string& desc)
												: ImageTests			(eglTestCtx, name, desc) {}

	void									addRenderActions		(void);
	LabeledActions							m_renderActions;
};

void RenderTests::addRenderActions (void)
{
	m_renderActions.add("texture",			MovePtr<Action>(new GLESImageApi::RenderTexture2D()));
	m_renderActions.add("texture_array",	MovePtr<Action>(new GLESImageApi::RenderTexture2DArray()));
	m_renderActions.add("read_pixels",		MovePtr<Action>(new GLESImageApi::RenderReadPixelsRenderbuffer()));
	m_renderActions.add("depth_buffer",		MovePtr<Action>(new GLESImageApi::RenderDepthbuffer()));
	m_renderActions.add("stencil_buffer",	MovePtr<Action>(new GLESImageApi::RenderStencilbuffer()));
	m_renderActions.add("yuv_texture",		MovePtr<Action>(new GLESImageApi::RenderYUVTexture()));
}

class SimpleCreationTests : public RenderTests
{
public:
			SimpleCreationTests		(EglTestContext& eglTestCtx, const string& name, const string& desc) : RenderTests(eglTestCtx, name, desc) {}
	void	init					(void);
};

bool isDepthFormat (GLenum format)
{
	switch (format)
	{
		case GL_RGB:
		case GL_RGB8:
		case GL_RGB565:
		case GL_RGBA:
		case GL_RGBA4:
		case GL_RGBA8:
		case GL_RGB5_A1:
		case GL_RGB10_A2:
		case GL_RGBA16F:
			return false;

		case GL_DEPTH_COMPONENT16:
		case GL_DEPTH_COMPONENT24:
		case GL_DEPTH_COMPONENT32:
		case GL_DEPTH_COMPONENT32F:
		case GL_DEPTH24_STENCIL8:
		case GL_DEPTH32F_STENCIL8:
			return true;

		case GL_STENCIL_INDEX8:
			return false;

		default:
			DE_ASSERT(false);
			return false;
	}
}

bool isStencilFormat (GLenum format)
{
	switch (format)
	{
		case GL_RGB:
		case GL_RGB8:
		case GL_RGB565:
		case GL_RGBA:
		case GL_RGBA4:
		case GL_RGBA8:
		case GL_RGB5_A1:
		case GL_RGB10_A2:
		case GL_RGBA16F:
			return false;

		case GL_DEPTH_COMPONENT16:
		case GL_DEPTH_COMPONENT24:
		case GL_DEPTH_COMPONENT32:
		case GL_DEPTH_COMPONENT32F:
			return false;

		case GL_STENCIL_INDEX8:
		case GL_DEPTH24_STENCIL8:
		case GL_DEPTH32F_STENCIL8:
			return true;

		default:
			DE_ASSERT(false);
			return false;
	}
}

bool isCompatibleCreateAndRenderActions (const Action& create, const Action& render)
{
	if (const GLESImageApi::Create* glesCreate = dynamic_cast<const GLESImageApi::Create*>(&create))
	{
		bool  yuvFormatTest = glesCreate->isYUVFormatImage();
		// this path only for none-yuv format tests
		if(!yuvFormatTest)
		{
		    const GLenum createFormat = glesCreate->getEffectiveFormat();

			if (dynamic_cast<const GLESImageApi::RenderTexture2DArray*>(&render))
			{
				// Makes sense only for texture arrays.
				if (glesCreate->getNumLayers() <= 1u)
					return false;
			}
			else if (glesCreate->getNumLayers() != 1u)
			{
				// Skip other render actions for texture arrays.
				return false;
			}

			if (dynamic_cast<const GLESImageApi::RenderTexture2D*>(&render))
			{
				// GLES does not have depth or stencil textures
				if (isDepthFormat(createFormat) || isStencilFormat(createFormat))
					return false;
			}

			if (dynamic_cast<const GLESImageApi::RenderReadPixelsRenderbuffer*>(&render))
			{
				// GLES does not support readPixels for depth or stencil.
				if (isDepthFormat(createFormat) || isStencilFormat(createFormat))
					return false;
			}

			if (dynamic_cast<const GLESImageApi::RenderDepthbuffer*>(&render))
			{
				// Copying non-depth data to depth renderbuffer and expecting meaningful
				// results just doesn't make any sense.
				if (!isDepthFormat(createFormat))
					return false;
			}

			if (dynamic_cast<const GLESImageApi::RenderStencilbuffer*>(&render))
			{
				// Copying non-stencil data to stencil renderbuffer and expecting meaningful
				// results just doesn't make any sense.
				if (!isStencilFormat(createFormat))
					return false;
			}

			if (dynamic_cast<const GLESImageApi::RenderYUVTexture*>(&render))
			{
				// In yuv path rendering with non-yuv format native buffer and expecting meaningful
				// results just doesn't make any sense
				return false;
			}

			return true;
		}
		else if (dynamic_cast<const GLESImageApi::RenderYUVTexture*>(&render))
		{
			return true;
		}
	}
	else
		DE_ASSERT(false);

	return false;
}

void SimpleCreationTests::init (void)
{
	addCreateTexture2DActions("texture_");
	addCreateTextureCubemapActions("_rgba", GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
	addCreateTextureCubemapActions("_rgb", GL_RGB, GL_RGB, GL_UNSIGNED_BYTE);
	addCreateRenderbufferActions();
	addCreateAndroidNativeActions();
	addRenderActions();

	for (int createNdx = 0; createNdx < m_createActions.size(); createNdx++)
	{
		const LabeledAction& createAction = m_createActions[createNdx];

		for (int renderNdx = 0; renderNdx < m_renderActions.size(); renderNdx++)
		{
			const LabeledAction&	renderAction	= m_renderActions[renderNdx];
			TestSpec				spec;

			if (!isCompatibleCreateAndRenderActions(*createAction.action, *renderAction.action))
				continue;

			if (dynamic_cast<const GLESImageApi::RenderTexture2DArray*>(renderAction.action.get()) ||
				dynamic_cast<const GLESImageApi::RenderYUVTexture*>(renderAction.action.get()))
			{
				// Texture array tests require GLES3.
				spec.name = std::string("gles3_") + createAction.label + "_" + renderAction.label;
				spec.contexts.push_back(TestSpec::API_GLES3);
			}
			else
			{
				spec.name = std::string("gles2_") + createAction.label + "_" + renderAction.label;
				spec.contexts.push_back(TestSpec::API_GLES2);
			}

			spec.desc = spec.name;
			spec.operations.push_back(TestSpec::Operation(0, *createAction.action));
			spec.operations.push_back(TestSpec::Operation(0, *renderAction.action));

			addChild(new ImageFormatCase(m_eglTestCtx, spec));
		}
	}
}

TestCaseGroup* createSimpleCreationTests (EglTestContext& eglTestCtx, const string& name, const string& desc)
{
	return new SimpleCreationTests(eglTestCtx, name, desc);
}

bool isCompatibleFormats (GLenum createFormat, GLenum modifyFormat, GLenum modifyType)
{
	switch (modifyFormat)
	{
		case GL_RGB:
			switch (modifyType)
			{
				case GL_UNSIGNED_BYTE:
					return createFormat == GL_RGB
							|| createFormat == GL_RGB8
							|| createFormat == GL_RGB565
							|| createFormat == GL_SRGB8;

				case GL_BYTE:
					return createFormat == GL_RGB8_SNORM;

				case GL_UNSIGNED_SHORT_5_6_5:
					return createFormat == GL_RGB
							|| createFormat == GL_RGB565;

				case GL_UNSIGNED_INT_10F_11F_11F_REV:
					return createFormat == GL_R11F_G11F_B10F;

				case GL_UNSIGNED_INT_5_9_9_9_REV:
					return createFormat == GL_RGB9_E5;

				case GL_HALF_FLOAT:
					return createFormat == GL_RGB16F
							|| createFormat == GL_R11F_G11F_B10F
							|| createFormat == GL_RGB9_E5;

				case GL_FLOAT:
					return createFormat == GL_RGB16F
							|| createFormat == GL_RGB32F
							|| createFormat == GL_R11F_G11F_B10F
							|| createFormat == GL_RGB9_E5;

				default:
					DE_FATAL("Unknown modify type");
					return false;
			}

		case GL_RGBA:
			switch (modifyType)
			{
				case GL_UNSIGNED_BYTE:
					return createFormat == GL_RGBA8
						|| createFormat == GL_RGB5_A1
						|| createFormat == GL_RGBA4
						|| createFormat == GL_SRGB8_ALPHA8
						|| createFormat == GL_RGBA;

				case GL_UNSIGNED_SHORT_4_4_4_4:
					return createFormat == GL_RGBA4
						|| createFormat == GL_RGBA;

				case GL_UNSIGNED_SHORT_5_5_5_1:
					return createFormat == GL_RGB5_A1
						|| createFormat == GL_RGBA;

				case GL_UNSIGNED_INT_2_10_10_10_REV:
					return createFormat == GL_RGB10_A2
						|| createFormat == GL_RGB5_A1;

				case GL_HALF_FLOAT:
					return createFormat == GL_RGBA16F;

				case GL_FLOAT:
					return createFormat == GL_RGBA16F
						|| createFormat == GL_RGBA32F;

				default:
					DE_FATAL("Unknown modify type");
					return false;
			}

		default:
			DE_FATAL("Unknown modify format");
			return false;
	}
}

bool isCompatibleCreateAndModifyActions (const Action& create, const Action& modify)
{
	if (const GLESImageApi::Create* glesCreate = dynamic_cast<const GLESImageApi::Create*>(&create))
	{
		// No modify tests for texture arrays.
		if (glesCreate->getNumLayers() > 1u)
			return false;
		// No modify tests for yuv format image.
		if (glesCreate->isYUVFormatImage())
			return false;

		const GLenum createFormat = glesCreate->getEffectiveFormat();

		if (const GLESImageApi::ModifyTexSubImage* glesTexSubImageModify = dynamic_cast<const GLESImageApi::ModifyTexSubImage*>(&modify))
		{
			const GLenum modifyFormat	= glesTexSubImageModify->getFormat();
			const GLenum modifyType		= glesTexSubImageModify->getType();

			return isCompatibleFormats(createFormat, modifyFormat, modifyType);
		}

		if (dynamic_cast<const GLESImageApi::ModifyRenderbufferClearColor*>(&modify))
		{
			// reintepreting color as non-color is not meaningful
			if (isDepthFormat(createFormat) || isStencilFormat(createFormat))
				return false;
		}

		if (dynamic_cast<const GLESImageApi::ModifyRenderbufferClearDepth*>(&modify))
		{
			// reintepreting depth as non-depth is not meaningful
			if (!isDepthFormat(createFormat))
				return false;
		}

		if (dynamic_cast<const GLESImageApi::ModifyRenderbufferClearStencil*>(&modify))
		{
			// reintepreting stencil as non-stencil is not meaningful
			if (!isStencilFormat(createFormat))
				return false;
		}

		return true;
	}
	else
		DE_ASSERT(false);

	return false;
}

class MultiContextRenderTests : public RenderTests
{
public:
					MultiContextRenderTests		(EglTestContext& eglTestCtx, const string& name, const string& desc);
	void			init						(void);
	void			addClearActions				(void);
private:
	LabeledActions	m_clearActions;
};

MultiContextRenderTests::MultiContextRenderTests (EglTestContext& eglTestCtx, const string& name, const string& desc)
	: RenderTests	(eglTestCtx, name, desc)
{
}

void MultiContextRenderTests::addClearActions (void)
{
	m_clearActions.add("clear_color",	MovePtr<Action>(new GLESImageApi::ModifyRenderbufferClearColor(tcu::Vec4(0.8f, 0.2f, 0.9f, 1.0f))));
	m_clearActions.add("clear_depth",	MovePtr<Action>(new GLESImageApi::ModifyRenderbufferClearDepth(0.75f)));
	m_clearActions.add("clear_stencil",	MovePtr<Action>(new GLESImageApi::ModifyRenderbufferClearStencil(97)));
}

void MultiContextRenderTests::init (void)
{
	addCreateTexture2DActions("texture_");
	addCreateTextureCubemapActions("_rgba8", GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
	addCreateTextureCubemapActions("_rgb8", GL_RGB, GL_RGB, GL_UNSIGNED_BYTE);
	addCreateRenderbufferActions();
	addCreateAndroidNativeActions();
	addRenderActions();
	addClearActions();

	for (int createNdx = 0; createNdx < m_createActions.size(); createNdx++)
	for (int renderNdx = 0; renderNdx < m_renderActions.size(); renderNdx++)
	for (int clearNdx = 0; clearNdx < m_clearActions.size(); clearNdx++)
	{
		const LabeledAction&	createAction	= m_createActions[createNdx];
		const LabeledAction&	renderAction	= m_renderActions[renderNdx];
		const LabeledAction&	clearAction		= m_clearActions[clearNdx];
		TestSpec				spec;

		if (!isCompatibleCreateAndRenderActions(*createAction.action, *renderAction.action))
			continue;
		if (!isCompatibleCreateAndModifyActions(*createAction.action, *clearAction.action))
			continue;

		spec.name = std::string("gles2_") + createAction.label + "_" + renderAction.label;

		const GLESImageApi::Create* glesCreate = dynamic_cast<const GLESImageApi::Create*>(createAction.action.get());

		if (!glesCreate)
			DE_FATAL("Dynamic casting to GLESImageApi::Create* failed");

		const GLenum createFormat = glesCreate->getEffectiveFormat();

		if (isDepthFormat(createFormat) && isStencilFormat(createFormat))
		{
			// Combined depth and stencil format. Add the clear action label to avoid test
			// name clashes.
			spec.name += std::string("_") + clearAction.label;
		}

		spec.desc = spec.name;

		spec.contexts.push_back(TestSpec::API_GLES2);
		spec.contexts.push_back(TestSpec::API_GLES2);

		spec.operations.push_back(TestSpec::Operation(0, *createAction.action));
		spec.operations.push_back(TestSpec::Operation(0, *renderAction.action));
		spec.operations.push_back(TestSpec::Operation(0, *clearAction.action));
		spec.operations.push_back(TestSpec::Operation(1, *createAction.action));
		spec.operations.push_back(TestSpec::Operation(0, *renderAction.action));
		spec.operations.push_back(TestSpec::Operation(1, *renderAction.action));

		addChild(new ImageFormatCase(m_eglTestCtx, spec));
	}
}

TestCaseGroup* createMultiContextRenderTests (EglTestContext& eglTestCtx, const string& name, const string& desc)
{
	return new MultiContextRenderTests(eglTestCtx, name, desc);
}

class ModifyTests : public ImageTests
{
public:
								ModifyTests		(EglTestContext& eglTestCtx, const string& name, const string& desc)
									: ImageTests(eglTestCtx, name, desc) {}

	void						init			(void);

protected:
	void						addModifyActions(void);

	LabeledActions				m_modifyActions;
	GLESImageApi::RenderTryAll	m_renderAction;
};

void ModifyTests::addModifyActions (void)
{
	m_modifyActions.add("tex_subimage_rgb8",			MovePtr<Action>(new GLESImageApi::ModifyTexSubImage(GL_RGB,		GL_UNSIGNED_BYTE)));
	m_modifyActions.add("tex_subimage_rgb565",			MovePtr<Action>(new GLESImageApi::ModifyTexSubImage(GL_RGB,		GL_UNSIGNED_SHORT_5_6_5)));
	m_modifyActions.add("tex_subimage_rgba8",			MovePtr<Action>(new GLESImageApi::ModifyTexSubImage(GL_RGBA,	GL_UNSIGNED_BYTE)));
	m_modifyActions.add("tex_subimage_rgb5_a1",			MovePtr<Action>(new GLESImageApi::ModifyTexSubImage(GL_RGBA,	GL_UNSIGNED_SHORT_5_5_5_1)));
	m_modifyActions.add("tex_subimage_rgba4",			MovePtr<Action>(new GLESImageApi::ModifyTexSubImage(GL_RGBA,	GL_UNSIGNED_SHORT_4_4_4_4)));

	m_modifyActions.add("renderbuffer_clear_color",		MovePtr<Action>(new GLESImageApi::ModifyRenderbufferClearColor(tcu::Vec4(0.3f, 0.5f, 0.3f, 1.0f))));
	m_modifyActions.add("renderbuffer_clear_depth",		MovePtr<Action>(new GLESImageApi::ModifyRenderbufferClearDepth(0.7f)));
	m_modifyActions.add("renderbuffer_clear_stencil",	MovePtr<Action>(new GLESImageApi::ModifyRenderbufferClearStencil(78)));
}

void ModifyTests::init (void)
{
	addCreateTexture2DActions("tex_");
	addCreateRenderbufferActions();
	addCreateAndroidNativeActions();
	addModifyActions();

	for (int createNdx = 0; createNdx < m_createActions.size(); createNdx++)
	{
		LabeledAction& createAction = m_createActions[createNdx];

		for (int modifyNdx = 0; modifyNdx < m_modifyActions.size(); modifyNdx++)
		{
			LabeledAction& modifyAction = m_modifyActions[modifyNdx];

			if (!isCompatibleCreateAndModifyActions(*createAction.action, *modifyAction.action))
				continue;

			TestSpec spec;
			spec.name = createAction.label + "_" + modifyAction.label;
			spec.desc = "gles2_tex_sub_image";

			spec.contexts.push_back(TestSpec::API_GLES2);

			spec.operations.push_back(TestSpec::Operation(0, *createAction.action));
			spec.operations.push_back(TestSpec::Operation(0, m_renderAction));
			spec.operations.push_back(TestSpec::Operation(0, *modifyAction.action));
			spec.operations.push_back(TestSpec::Operation(0, m_renderAction));

			addChild(new ImageFormatCase(m_eglTestCtx, spec));
		}
	}
}

TestCaseGroup* createModifyTests (EglTestContext& eglTestCtx, const string& name, const string& desc)
{
	return new ModifyTests(eglTestCtx, name, desc);
}

} // Image
} // egl
} // deqp
