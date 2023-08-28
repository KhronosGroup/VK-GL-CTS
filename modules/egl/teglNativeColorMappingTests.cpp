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
 * \brief Test for mapping client color values to native surface colors
 *//*--------------------------------------------------------------------*/

#include "teglNativeColorMappingTests.hpp"

#include "teglSimpleConfigCase.hpp"

#include "tcuTexture.hpp"

#include "egluNativeDisplay.hpp"
#include "egluNativeWindow.hpp"
#include "egluNativePixmap.hpp"
#include "egluUnique.hpp"
#include "egluUtil.hpp"

#include "eglwLibrary.hpp"
#include "eglwEnums.hpp"

#include "gluDefs.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

#include "deThread.hpp"

#include <vector>
#include <string>
#include <limits>

using tcu::TestLog;
using std::vector;
using std::string;

using namespace eglw;

namespace deqp
{
namespace egl
{
namespace
{

EGLContext createGLES2Context (const Library& egl, EGLDisplay display, EGLConfig config)
{
	EGLContext		context			= EGL_NO_CONTEXT;
	const EGLint	attribList[]	=
	{
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	EGLU_CHECK_CALL(egl, bindAPI(EGL_OPENGL_ES_API));

	context = egl.createContext(display, config, EGL_NO_CONTEXT, attribList);
	EGLU_CHECK_MSG(egl, "eglCreateContext() failed");
	TCU_CHECK(context);

	return context;
}

deUint32 createGLES2Program (const glw::Functions& gl, TestLog& log)
{
	const char* const vertexShaderSource =
	"attribute highp vec2 a_pos;\n"
	"void main (void)\n"
	"{\n"
	"\tgl_Position = vec4(a_pos, 0.0, 1.0);\n"
	"}";

	const char* const fragmentShaderSource =
	"uniform mediump vec4 u_color;\n"
	"void main (void)\n"
	"{\n"
	"\tgl_FragColor = u_color;\n"
	"}";

	deUint32	program			= 0;
	deUint32	vertexShader	= 0;
	deUint32	fragmentShader	= 0;

	deInt32		vertexCompileStatus;
	string		vertexInfoLog;
	deInt32		fragmentCompileStatus;
	string		fragmentInfoLog;
	deInt32		linkStatus;
	string		programInfoLog;

	try
	{
		program			= gl.createProgram();
		vertexShader	= gl.createShader(GL_VERTEX_SHADER);
		fragmentShader	= gl.createShader(GL_FRAGMENT_SHADER);

		GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to create shaders and program");

		gl.shaderSource(vertexShader, 1, &vertexShaderSource, DE_NULL);
		gl.compileShader(vertexShader);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to setup vertex shader");

		gl.shaderSource(fragmentShader, 1, &fragmentShaderSource, DE_NULL);
		gl.compileShader(fragmentShader);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to setup fragment shader");

		{
			deInt32		infoLogLength = 0;

			gl.getShaderiv(vertexShader, GL_COMPILE_STATUS, &vertexCompileStatus);
			gl.getShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &infoLogLength);

			vertexInfoLog.resize(infoLogLength, '\0');

			gl.getShaderInfoLog(vertexShader, (glw::GLsizei)vertexInfoLog.length(), &infoLogLength, &(vertexInfoLog[0]));
			GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to get vertex shader compile info");

			vertexInfoLog.resize(infoLogLength);
		}

		{
			deInt32		infoLogLength = 0;

			gl.getShaderiv(fragmentShader, GL_COMPILE_STATUS, &fragmentCompileStatus);
			gl.getShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &infoLogLength);

			fragmentInfoLog.resize(infoLogLength, '\0');

			gl.getShaderInfoLog(fragmentShader, (glw::GLsizei)fragmentInfoLog.length(), &infoLogLength, &(fragmentInfoLog[0]));
			GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to get fragment shader compile info");

			fragmentInfoLog.resize(infoLogLength);
		}

		gl.attachShader(program, vertexShader);
		gl.attachShader(program, fragmentShader);
		gl.linkProgram(program);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to setup program");

		{
			deInt32		infoLogLength = 0;

			gl.getProgramiv(program, GL_LINK_STATUS, &linkStatus);
			gl.getProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);

			programInfoLog.resize(infoLogLength, '\0');

			gl.getProgramInfoLog(program, (glw::GLsizei)programInfoLog.length(), &infoLogLength, &(programInfoLog[0]));
			GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to get program link info");

			programInfoLog.resize(infoLogLength);
		}

		if (linkStatus == 0 || vertexCompileStatus == 0 || fragmentCompileStatus == 0)
		{

			log.startShaderProgram(linkStatus != 0, programInfoLog.c_str());

			log << TestLog::Shader(QP_SHADER_TYPE_VERTEX, vertexShaderSource, vertexCompileStatus != 0, vertexInfoLog);
			log << TestLog::Shader(QP_SHADER_TYPE_FRAGMENT, fragmentShaderSource, fragmentCompileStatus != 0, fragmentInfoLog);

			log.endShaderProgram();
		}

		gl.deleteShader(vertexShader);
		gl.deleteShader(fragmentShader);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to delete shaders");

		TCU_CHECK(linkStatus != 0 && vertexCompileStatus != 0 && fragmentCompileStatus != 0);
	}
	catch (...)
	{
		if (program)
			gl.deleteProgram(program);

		if (vertexShader)
			gl.deleteShader(vertexShader);

		if (fragmentShader)
			gl.deleteShader(fragmentShader);

		throw;
	}

	return program;
}

void clear (const glw::Functions& gl, const tcu::Vec4& color)
{
	gl.clearColor(color.x(), color.y(), color.z(), color.w());
	gl.clear(GL_COLOR_BUFFER_BIT);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Color clear failed");
}

void render (const glw::Functions& gl, deUint32 program, const tcu::Vec4& color)
{
	const float positions[] =
	{
		-1.0f, -1.0f,
		 1.0f, -1.0f,
		 1.0f,  1.0f,

		 1.0f,  1.0f,
		-1.0f,  1.0f,
		-1.0f, -1.0f
	};

	deUint32 posLocation;
	deUint32 colorLocation;

	gl.useProgram(program);
	posLocation	= gl.getAttribLocation(program, "a_pos");
	gl.enableVertexAttribArray(posLocation);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to setup shader program for rendering");

	colorLocation = gl.getUniformLocation(program, "u_color");
	gl.uniform4fv(colorLocation, 1, color.getPtr());

	gl.vertexAttribPointer(posLocation, 2, GL_FLOAT, GL_FALSE, 0, positions);
	gl.drawArrays(GL_TRIANGLES, 0, 6);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to render");
}

bool validate (TestLog& log, const Library& egl, EGLDisplay display, EGLConfig config, const tcu::TextureLevel& result, const tcu::Vec4& color)
{
	const tcu::UVec4 eglBitDepth((deUint32)eglu::getConfigAttribInt(egl, display, config, EGL_RED_SIZE),
								 (deUint32)eglu::getConfigAttribInt(egl, display, config, EGL_GREEN_SIZE),
								 (deUint32)eglu::getConfigAttribInt(egl, display, config, EGL_BLUE_SIZE),
								 (deUint32)eglu::getConfigAttribInt(egl, display, config, EGL_ALPHA_SIZE));

	const tcu::UVec4 nativeBitDepth(tcu::getTextureFormatBitDepth(result.getFormat()).asUint());
	const tcu::UVec4 bitDepth(deMinu32(nativeBitDepth.x(), eglBitDepth.x()),
							  deMinu32(nativeBitDepth.y(), eglBitDepth.y()),
							  deMinu32(nativeBitDepth.z(), eglBitDepth.z()),
							  deMinu32(nativeBitDepth.w(), eglBitDepth.w()));

	const tcu::UVec4 uColor = tcu::UVec4((deUint32)((float)((1u << bitDepth.x()) - 1u) * color.x()),
										 (deUint32)((float)((1u << bitDepth.y()) - 1u) * color.y()),
										 (deUint32)((float)((1u << bitDepth.z()) - 1u) * color.z()),
										 (deUint32)((float)((1u << bitDepth.w()) - 1u) * color.w()));

	tcu::TextureLevel reference(result.getFormat(), result.getWidth(), result.getHeight());

	for (int y = 0; y < result.getHeight(); y++)
	{
		for (int x = 0; x < result.getWidth(); x++)
			reference.getAccess().setPixel(uColor, x, y);
	}

	return tcu::intThresholdCompare(log, "Result compare", "Compare results", reference.getAccess(), result.getAccess(), tcu::UVec4(1u, 1u, 1u, (bitDepth.w() > 0 ? 1u : std::numeric_limits<deUint32>::max())), tcu::COMPARE_LOG_RESULT);
}

class NativeColorMappingCase : public SimpleConfigCase
{
public:
	enum NativeType
	{
		NATIVETYPE_WINDOW = 0,
		NATIVETYPE_PIXMAP,
		NATIVETYPE_PBUFFER_COPY_TO_PIXMAP
	};

				NativeColorMappingCase	(EglTestContext& eglTestCtx, const char* name, const char* description, bool render, NativeType nativeType, const eglu::FilterList& filters);
				~NativeColorMappingCase	(void);

private:
	void		executeForConfig		(EGLDisplay display, EGLConfig config);

	NativeType	m_nativeType;
	bool		m_render;
};

NativeColorMappingCase::NativeColorMappingCase (EglTestContext& eglTestCtx, const char* name, const char* description, bool render, NativeType nativeType, const eglu::FilterList& filters)
	: SimpleConfigCase	(eglTestCtx, name, description, filters)
	, m_nativeType		(nativeType)
	, m_render			(render)
{
}

NativeColorMappingCase::~NativeColorMappingCase	(void)
{
	deinit();
}

void logConfigInfo (TestLog& log, const Library& egl, EGLDisplay display, EGLConfig config, NativeColorMappingCase::NativeType nativeType, int waitFrames)
{
	log << TestLog::Message << "EGL_RED_SIZE: "		<< eglu::getConfigAttribInt(egl, display, config, EGL_RED_SIZE)		<< TestLog::EndMessage;
	log << TestLog::Message << "EGL_GREEN_SIZE: "	<< eglu::getConfigAttribInt(egl, display, config, EGL_GREEN_SIZE)	<< TestLog::EndMessage;
	log << TestLog::Message << "EGL_BLUE_SIZE: "	<< eglu::getConfigAttribInt(egl, display, config, EGL_BLUE_SIZE)	<< TestLog::EndMessage;
	log << TestLog::Message << "EGL_ALPHA_SIZE: "	<< eglu::getConfigAttribInt(egl, display, config, EGL_ALPHA_SIZE)	<< TestLog::EndMessage;
	log << TestLog::Message << "EGL_DEPTH_SIZE: "	<< eglu::getConfigAttribInt(egl, display, config, EGL_DEPTH_SIZE)	<< TestLog::EndMessage;
	log << TestLog::Message << "EGL_STENCIL_SIZE: "	<< eglu::getConfigAttribInt(egl, display, config, EGL_STENCIL_SIZE)	<< TestLog::EndMessage;
	log << TestLog::Message << "EGL_SAMPLES: "		<< eglu::getConfigAttribInt(egl, display, config, EGL_SAMPLES)		<< TestLog::EndMessage;

	if (nativeType == NativeColorMappingCase::NATIVETYPE_WINDOW)
		log << TestLog::Message << "Waiting " << waitFrames * 16 << "ms after eglSwapBuffers() and glFinish() for frame to become visible" << TestLog::EndMessage;
}

bool testNativeWindow (TestLog& log, eglu::NativeDisplay& nativeDisplay, eglu::NativeWindow& nativeWindow, EGLDisplay display, EGLContext context, EGLConfig config, const glw::Functions& gl, bool renderColor, int waitFrames, size_t colorCount, const tcu::Vec4* colors)
{
	const Library&		egl			= nativeDisplay.getLibrary();
	eglu::UniqueSurface	surface		(egl, display, eglu::createWindowSurface(nativeDisplay, nativeWindow, display, config, DE_NULL));
	tcu::TextureLevel	result;
	deUint32			program		= 0;
	bool				isOk		= true;

	try
	{
		EGLU_CHECK_CALL(egl, makeCurrent(display, *surface, *surface, context));

		if (renderColor)
			program = createGLES2Program(gl, log);

		for (int colorNdx = 0; colorNdx < (int)colorCount; colorNdx++)
		{
			if (renderColor)
				render(gl, program, colors[colorNdx]);
			else
				clear(gl, colors[colorNdx]);

			EGLU_CHECK_CALL(egl, swapBuffers(display, *surface));
			EGLU_CHECK_CALL(egl, waitClient());
			deSleep(waitFrames*16);
			nativeWindow.processEvents();
			nativeWindow.readScreenPixels(&result);

			if (!validate(log, egl, display, config, result, colors[colorNdx]))
				isOk = false;
		}

		EGLU_CHECK_CALL(egl, makeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
	}
	catch (...)
	{
		if (program)
			gl.deleteProgram(program);
		throw;
	}

	return isOk;
}

bool testNativePixmap (TestLog& log, eglu::NativeDisplay& nativeDisplay, eglu::NativePixmap& nativePixmap, EGLDisplay display, EGLContext context, EGLConfig config, const glw::Functions& gl, bool renderColor, size_t colorCount, const tcu::Vec4* colors)
{
	const Library&		egl			= nativeDisplay.getLibrary();
	eglu::UniqueSurface	surface		(egl, display, eglu::createPixmapSurface(nativeDisplay, nativePixmap, display, config, DE_NULL));
	tcu::TextureLevel	result;
	deUint32			program		= 0;
	bool				isOk		= true;

	try
	{
		EGLU_CHECK_CALL(egl, makeCurrent(display, *surface, *surface, context));

		if (renderColor)
			program = createGLES2Program(gl, log);

		for (int colorNdx = 0; colorNdx < (int)colorCount; colorNdx++)
		{
			if (renderColor)
				render(gl, program, colors[colorNdx]);
			else
				clear(gl, colors[colorNdx]);

			EGLU_CHECK_CALL(egl, waitClient());
			nativePixmap.readPixels(&result);

			if (!validate(log, egl, display, config, result, colors[colorNdx]))
				isOk = false;
		}

		EGLU_CHECK_CALL(egl, makeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
	}
	catch (...)
	{
		if (program)
			gl.deleteProgram(program);
		throw;
	}

	return isOk;
}

bool testNativePixmapCopy (TestLog& log, const Library& egl, eglu::NativePixmap& nativePixmap, EGLDisplay display, EGLContext context, EGLConfig config, const glw::Functions& gl, bool renderColor, size_t colorCount, const tcu::Vec4* colors)
{
	eglu::UniqueSurface	surface		(egl, display, egl.createPbufferSurface(display, config, DE_NULL));
	tcu::TextureLevel	result;
	deUint32			program		= 0;
	bool				isOk		= true;

	try
	{
		EGLU_CHECK_CALL(egl, makeCurrent(display, *surface, *surface, context));

		if (renderColor)
			program = createGLES2Program(gl, log);

		for (int colorNdx = 0; colorNdx < (int)colorCount; colorNdx++)
		{
			if (renderColor)
				render(gl, program, colors[colorNdx]);
			else
				clear(gl, colors[colorNdx]);

			EGLU_CHECK_CALL(egl, copyBuffers(display, *surface, nativePixmap.getLegacyNative()));
			EGLU_CHECK_CALL(egl, waitClient());
			nativePixmap.readPixels(&result);

			if (!validate(log, egl, display, config, result, colors[colorNdx]))
				isOk = false;
		}

		EGLU_CHECK_CALL(egl, makeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
	}
	catch (...)
	{
		if (program)
			gl.deleteProgram(program);
		throw;
	}

	return isOk;
}

void NativeColorMappingCase::executeForConfig (EGLDisplay display, EGLConfig config)
{
	const int width		= 128;
	const int height	= 128;

	const tcu::Vec4 colors[] =
	{
		tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
		tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f),
		tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f),
		tcu::Vec4(0.0f, 1.0f, 1.0f, 1.0f),
		tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
		tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f),
		tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
		tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f),

		tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
		tcu::Vec4(0.0f, 0.0f, 0.5f, 1.0f),
		tcu::Vec4(0.0f, 0.5f, 0.0f, 1.0f),
		tcu::Vec4(0.0f, 0.5f, 0.5f, 1.0f),
		tcu::Vec4(0.5f, 0.0f, 0.0f, 1.0f),
		tcu::Vec4(0.5f, 0.0f, 0.5f, 1.0f),
		tcu::Vec4(0.5f, 0.5f, 0.0f, 1.0f),
		tcu::Vec4(0.5f, 0.5f, 0.5f, 1.0f)
	};

	const Library&						egl				= m_eglTestCtx.getLibrary();
	const string						configIdStr		(de::toString(eglu::getConfigAttribInt(egl, display, config, EGL_CONFIG_ID)));
	tcu::ScopedLogSection				logSection		(m_testCtx.getLog(), ("Config ID " + configIdStr).c_str(), ("Config ID " + configIdStr).c_str());
	const int							waitFrames		= 5;
	const eglu::NativeWindowFactory*	windowFactory;
	const eglu::NativePixmapFactory*	pixmapFactory;

	logConfigInfo(m_testCtx.getLog(), egl, display, config, m_nativeType, waitFrames);

	try
	{
		windowFactory = &eglu::selectNativeWindowFactory(m_eglTestCtx.getNativeDisplayFactory(), m_testCtx.getCommandLine());

		if ((windowFactory->getCapabilities() & eglu::NativeWindow::CAPABILITY_READ_SCREEN_PIXELS) == 0)
			TCU_THROW(NotSupportedError, "Native window doesn't support readPixels()");
	}
	catch (const tcu::NotSupportedError&)
	{
		if (m_nativeType == NATIVETYPE_WINDOW)
			throw;
		else
			windowFactory = DE_NULL;
	}

	try
	{
		pixmapFactory = &eglu::selectNativePixmapFactory(m_eglTestCtx.getNativeDisplayFactory(), m_testCtx.getCommandLine());

		if (m_nativeType == NATIVETYPE_PIXMAP)
		{
			if ((pixmapFactory->getCapabilities() & eglu::NativePixmap::CAPABILITY_READ_PIXELS) == 0)
				TCU_THROW(NotSupportedError, "Native pixmap doesn't support readPixels()");
		}
		else if (m_nativeType == NATIVETYPE_PBUFFER_COPY_TO_PIXMAP)
		{
			if ((pixmapFactory->getCapabilities() & eglu::NativePixmap::CAPABILITY_READ_PIXELS) == 0 ||
				(pixmapFactory->getCapabilities() & eglu::NativePixmap::CAPABILITY_CREATE_SURFACE_LEGACY) == 0)
				TCU_THROW(NotSupportedError, "Native pixmap doesn't support readPixels() or legacy create surface");
		}
	}
	catch (const tcu::NotSupportedError&)
	{
		if (m_nativeType == NATIVETYPE_PIXMAP || m_nativeType == NATIVETYPE_PBUFFER_COPY_TO_PIXMAP)
			throw;
		else
			pixmapFactory = DE_NULL;
	}

	DE_ASSERT(m_nativeType != NATIVETYPE_WINDOW || windowFactory);
	DE_ASSERT((m_nativeType != NATIVETYPE_PIXMAP && m_nativeType != NATIVETYPE_PBUFFER_COPY_TO_PIXMAP) || pixmapFactory);

	eglu::UniqueContext		context		(egl, display, createGLES2Context(egl, display, config));
	glw::Functions			gl;

	m_eglTestCtx.initGLFunctions(&gl, glu::ApiType::es(2,0));

	switch (m_nativeType)
	{
		case NATIVETYPE_WINDOW:
		{
			de::UniquePtr<eglu::NativeWindow>	nativeWindow	(windowFactory->createWindow(&m_eglTestCtx.getNativeDisplay(), display, config, DE_NULL, eglu::WindowParams(width, height, eglu::WindowParams::VISIBILITY_VISIBLE)));

			if (!testNativeWindow(m_testCtx.getLog(), m_eglTestCtx.getNativeDisplay(), *nativeWindow, display, *context, config, gl, m_render, waitFrames, DE_LENGTH_OF_ARRAY(colors), colors))
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid color rendered");
			break;
		}

		case NATIVETYPE_PIXMAP:
		{
			de::UniquePtr<eglu::NativePixmap> nativePixmap		(pixmapFactory->createPixmap(&m_eglTestCtx.getNativeDisplay(), display, config, DE_NULL, width, height));

			if (!testNativePixmap(m_testCtx.getLog(), m_eglTestCtx.getNativeDisplay(), *nativePixmap, display, *context, config, gl, m_render, DE_LENGTH_OF_ARRAY(colors), colors))
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid color rendered");
			break;
		}

		case NATIVETYPE_PBUFFER_COPY_TO_PIXMAP:
		{
			de::UniquePtr<eglu::NativePixmap> nativePixmap		(pixmapFactory->createPixmap(&m_eglTestCtx.getNativeDisplay(), display, config, DE_NULL, width, height));

			if (!testNativePixmapCopy(m_testCtx.getLog(), egl, *nativePixmap, display, *context, config, gl, m_render, DE_LENGTH_OF_ARRAY(colors), colors))
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid color rendered");
			break;
		}

		default:
			DE_ASSERT(DE_FALSE);
	}
}

template<deUint32 Type>
static bool surfaceType (const eglu::CandidateConfig& c)
{
	return (c.surfaceType() & Type) == Type;
}

void addTestGroups (EglTestContext& eglTestCtx, TestCaseGroup* group, NativeColorMappingCase::NativeType type)
{
	eglu::FilterList baseFilters;

	switch (type)
	{
		case NativeColorMappingCase::NATIVETYPE_WINDOW:
			baseFilters << surfaceType<EGL_WINDOW_BIT>;
			break;

		case NativeColorMappingCase::NATIVETYPE_PIXMAP:
			baseFilters << surfaceType<EGL_PIXMAP_BIT>;
			break;

		case NativeColorMappingCase::NATIVETYPE_PBUFFER_COPY_TO_PIXMAP:
			baseFilters << surfaceType<EGL_PBUFFER_BIT>;
			break;

		default:
			DE_ASSERT(DE_FALSE);
	}

	vector<NamedFilterList> filterLists;
	getDefaultFilterLists(filterLists, baseFilters);

	for (vector<NamedFilterList>::iterator i = filterLists.begin(); i != filterLists.end(); i++)
	{
		group->addChild(new NativeColorMappingCase(eglTestCtx, (string(i->getName()) + "_clear").c_str(), i->getDescription(), false, type, *i));
		group->addChild(new NativeColorMappingCase(eglTestCtx, (string(i->getName()) + "_render").c_str(), i->getDescription(), true, type, *i));
	}
}

} // anonymous

NativeColorMappingTests::NativeColorMappingTests (EglTestContext& eglTestCtx)
	: TestCaseGroup(eglTestCtx, "native_color_mapping", "Tests for mapping client colors to native surface")
{
}

void NativeColorMappingTests::init (void)
{
	{
		TestCaseGroup* windowGroup = new TestCaseGroup(m_eglTestCtx, "native_window", "Tests for mapping client color to native window");
		addTestGroups(m_eglTestCtx, windowGroup, NativeColorMappingCase::NATIVETYPE_WINDOW);
		addChild(windowGroup);
	}

	{
		TestCaseGroup* pixmapGroup = new TestCaseGroup(m_eglTestCtx, "native_pixmap", "Tests for mapping client color to native pixmap");
		addTestGroups(m_eglTestCtx, pixmapGroup, NativeColorMappingCase::NATIVETYPE_PIXMAP);
		addChild(pixmapGroup);
	}

	{
		TestCaseGroup* pbufferGroup = new TestCaseGroup(m_eglTestCtx, "pbuffer_to_native_pixmap", "Tests for mapping client color to native pixmap with eglCopyBuffers()");
		addTestGroups(m_eglTestCtx, pbufferGroup, NativeColorMappingCase::NATIVETYPE_PBUFFER_COPY_TO_PIXMAP);
		addChild(pbufferGroup);
	}
}

} // egl
} // deqp
