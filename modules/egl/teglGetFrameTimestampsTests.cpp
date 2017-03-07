/*-------------------------------------------------------------------------
 * drawElements Quality Program EGL Module
 * ---------------------------------------
 *
 * Copyright 2017 The Android Open Source Project
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
 * \brief Test the EGL_ANDROID_get_frame_timestamps extension.
 *//*--------------------------------------------------------------------*/

#include "teglGetFrameTimestampsTests.hpp"

#include "teglSimpleConfigCase.hpp"

#include "egluNativeWindow.hpp"
#include "egluUtil.hpp"
#include "egluUnique.hpp"
#include "eglwLibrary.hpp"
#include "eglwEnums.hpp"

#include "gluDefs.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"

#include "tcuResultCollector.hpp"
#include "tcuTestLog.hpp"
#include "tcuSurface.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuVector.hpp"
#include "tcuVectorUtil.hpp"

#include "deClock.h"
#include "deMath.h"
#include "deUniquePtr.hpp"
#include "deThread.hpp"

#include <algorithm>
#include <string>
#include <vector>
#include <sstream>

// Tentative EGL header definitions for EGL_ANDROID_get_Frame_timestamps.
// \todo [2017-01-25 brianderson] Remove once defined in the official headers.
#define EGL_TIMESTAMPS_ANDROID 0x314D
#define EGL_COMPOSITE_DEADLINE_ANDROID 0x314E
#define EGL_COMPOSITE_INTERVAL_ANDROID 0x314F
#define EGL_COMPOSITE_TO_PRESENT_LATENCY_ANDROID 0x3150
#define EGL_REQUESTED_PRESENT_TIME_ANDROID 0x3151
#define EGL_RENDERING_COMPLETE_TIME_ANDROID 0x3152
#define EGL_COMPOSITION_LATCH_TIME_ANDROID 0x3153
#define EGL_FIRST_COMPOSITION_START_TIME_ANDROID 0x3154
#define EGL_LAST_COMPOSITION_START_TIME_ANDROID 0x3155
#define EGL_FIRST_COMPOSITION_GPU_FINISHED_TIME_ANDROID 0x3156
#define EGL_DISPLAY_PRESENT_TIME_ANDROID 0x3157
#define EGL_DISPLAY_RETIRE_TIME_ANDROID 0x3158
#define EGL_DEQUEUE_READY_TIME_ANDROID 0x3159
#define EGL_READS_DONE_TIME_ANDROID 0x315A
typedef deInt64 EGLnsecsANDROID;
typedef deUint64 EGLuint64KHR;
typedef EGLW_APICALL eglw::EGLBoolean (EGLW_APIENTRY* eglGetNextFrameIdANDROIDFunc) (eglw::EGLDisplay dpy, eglw::EGLSurface surface, EGLuint64KHR *frameId);
typedef EGLW_APICALL eglw::EGLBoolean (EGLW_APIENTRY* eglGetCompositorTimingANDROIDFunc) (eglw::EGLDisplay dpy, eglw::EGLSurface surface, eglw::EGLint numTimestamps, const eglw::EGLint *names, EGLnsecsANDROID *values);
typedef EGLW_APICALL eglw::EGLBoolean (EGLW_APIENTRY* eglGetCompositorTimingSupportedANDROIDFunc) (eglw::EGLDisplay dpy, eglw::EGLSurface surface, eglw::EGLint name);
typedef EGLW_APICALL eglw::EGLBoolean (EGLW_APIENTRY* eglGetFrameTimestampsANDROIDFunc) (eglw::EGLDisplay dpy, eglw::EGLSurface surface, EGLuint64KHR frameId, eglw::EGLint numTimestamps, const eglw::EGLint *timestamps, EGLnsecsANDROID *values);
typedef EGLW_APICALL eglw::EGLBoolean (EGLW_APIENTRY* eglGetFrameTimestampSupportedANDROIDFunc) (eglw::EGLDisplay dpy, eglw::EGLSurface surface, eglw::EGLint timestamp);

#define CHECK_NAKED_EGL_CALL(EGLW, CALL)	do { CALL; eglu::checkError((EGLW).getError(), #CALL, __FILE__, __LINE__); } while (deGetFalse())

namespace deqp
{
namespace egl
{

using tcu::TestLog;
using std::string;
using std::vector;
using namespace eglw;

namespace
{

// Careful: This has microsecond precision, which can cause timestamps to
// appear non monotonic when compared to the nanosecond precision timestamps
// we get from the eglGetFrameTimestamps extension.
// Current test expectations only make sure microsecond precision timestamps
// are less than the nanosecond precision timestamps, so this is okay.
EGLnsecsANDROID getNanoseconds (void)
{
	return deGetMicroseconds() * 1000;
}

struct FrameTimes
{
	FrameTimes (void)
		: frameId						(-1)
		, swapBufferBeginNs				(-1)
		, compositeDeadline				(-1)
		, compositeInterval				(-1)
		, compositeToPresentLatency		(-1)
		, requestedPresent				(-1)
		, latch							(-1)
		, firstCompositionStart			(-1)
		, lastCompositionStart			(-1)
		, dequeueReady					(-1)
		, renderingComplete				(-1)
		, firstCompositionGpuFinished	(-1)
		, displayPresent				(-1)
		, displayRetire					(-1)
		, readsDone						(-1)
	{
	}

	EGLuint64KHR	frameId;

	// Timestamps sampled by the test.
	EGLnsecsANDROID	swapBufferBeginNs;

	// Compositor info.
	EGLnsecsANDROID	compositeDeadline;
	EGLnsecsANDROID	compositeInterval;
	EGLnsecsANDROID	compositeToPresentLatency;

	// CPU Timeline.
	EGLnsecsANDROID	requestedPresent;
	EGLnsecsANDROID	latch;
	EGLnsecsANDROID	firstCompositionStart;
	EGLnsecsANDROID	lastCompositionStart;
	EGLnsecsANDROID	dequeueReady;

	// GPU Timeline.
	EGLnsecsANDROID	renderingComplete;
	EGLnsecsANDROID	firstCompositionGpuFinished;
	EGLnsecsANDROID	displayPresent;
	EGLnsecsANDROID	displayRetire;
	EGLnsecsANDROID	readsDone;
};

bool timestampExists (EGLnsecsANDROID timestamp)
{
	return timestamp > 0;
}

void verifySingleFrame (const FrameTimes& frameTimes, tcu::ResultCollector& result, bool verifyReadsDone)
{
	// Verify CPU timeline is monotonic.
	result.check(frameTimes.swapBufferBeginNs < frameTimes.latch, "Buffer latched before it was swapped.");
	result.check(frameTimes.latch < frameTimes.firstCompositionStart, "Buffer composited before it was latched.");
	result.check(frameTimes.firstCompositionStart <= frameTimes.lastCompositionStart, "First composition start after last composition start.");
	result.check(frameTimes.lastCompositionStart < frameTimes.dequeueReady, "Buffer composited after it was ready to be dequeued.");

	// Verify GPU timeline is monotonic.
	if (timestampExists(frameTimes.firstCompositionGpuFinished))
		result.check(frameTimes.renderingComplete < frameTimes.firstCompositionGpuFinished, "Buffer rendering completed after compositor GPU work finished.");

	if (timestampExists(frameTimes.displayPresent))
		result.check(frameTimes.renderingComplete < frameTimes.displayPresent, "Buffer displayed before rendering completed.");

	if (timestampExists(frameTimes.firstCompositionGpuFinished) && timestampExists(frameTimes.displayPresent))
		result.check(frameTimes.firstCompositionGpuFinished < frameTimes.displayPresent, "Buffer displayed before compositor GPU work completed");

	if (timestampExists(frameTimes.displayRetire))
		result.check(frameTimes.renderingComplete < frameTimes.displayRetire, "Buffer retired before rendering completed.");

	if (timestampExists(frameTimes.firstCompositionGpuFinished) && timestampExists(frameTimes.displayRetire))
		result.check(frameTimes.firstCompositionGpuFinished < frameTimes.displayRetire, "Buffer retired before compositor GPU work completed.");

	// Drivers may maintain shadow copies of the buffer, so the readsDone time
	// of the real buffer may be earlier than apparent dependencies. We can only
	// be sure that the readsDone time must be after the renderingComplete time.
	if (verifyReadsDone)
		result.check(frameTimes.renderingComplete < frameTimes.readsDone, "Buffer rendering completed after reads completed.");

	// Verify CPU/GPU dependencies
	result.check(frameTimes.renderingComplete < frameTimes.latch, "Buffer latched before rendering completed.");
	if (timestampExists(frameTimes.firstCompositionGpuFinished))
		result.check(frameTimes.firstCompositionStart < frameTimes.firstCompositionGpuFinished, "Composition CPU work started after GPU work finished.");

	if (timestampExists(frameTimes.displayPresent))
		result.check(frameTimes.firstCompositionStart < frameTimes.displayPresent, "Buffer displayed before it was composited.");

	if (timestampExists(frameTimes.displayRetire))
		result.check(frameTimes.lastCompositionStart < frameTimes.displayRetire, "Buffer retired before final composition.");

	// One of Present or retire must exist.
	result.check(timestampExists(frameTimes.displayPresent) != timestampExists(frameTimes.displayRetire), "Either present or retire must exist.");
}

void verifyNeighboringFrames (const FrameTimes& frame1, const FrameTimes& frame2, tcu::ResultCollector& result, bool verifyReadsDone)
{
	// CPU timeline.
	result.check(frame1.swapBufferBeginNs < frame2.swapBufferBeginNs, "Swap begin times not monotonic.");
	result.check(frame1.latch < frame2.latch, "Latch times not monotonic.");
	result.check(frame1.lastCompositionStart < frame2.latch, "Old buffer composited after new buffer latched.");
	result.check(frame1.lastCompositionStart < frame2.firstCompositionStart, "Composition times overlap.");
	result.check(frame1.dequeueReady < frame2.dequeueReady, "Dequeue ready times not monotonic.");

	// GPU timeline.
	result.check(frame1.renderingComplete < frame2.renderingComplete, "Rendering complete times not monotonic.");

	if (timestampExists(frame1.firstCompositionGpuFinished) && timestampExists(frame2.firstCompositionGpuFinished))
		result.check(frame1.firstCompositionGpuFinished < frame2.firstCompositionGpuFinished, "Composition GPU work complete times not monotonic.");

	if (timestampExists(frame1.displayPresent) && timestampExists(frame2.displayPresent))
		result.check(frame1.displayPresent < frame2.displayPresent, "Display present times not monotonic.");

	if (timestampExists(frame1.displayRetire) && timestampExists(frame2.displayRetire))
		result.check(frame1.displayRetire < frame2.displayRetire, "Display retire times not monotonic.");

	if (verifyReadsDone && timestampExists(frame1.readsDone) && timestampExists(frame2.readsDone))
		result.check(frame1.readsDone < frame2.readsDone, "Reads done times not monotonic.");
}

EGLContext createGLES2Context (const Library& egl, EGLDisplay display, EGLConfig config)
{
	EGLContext		context = EGL_NO_CONTEXT;
	const EGLint	attribList[] =
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

class GetFrameTimestampTest : public SimpleConfigCase
{
public:
							GetFrameTimestampTest	(EglTestContext& eglTestCtx, const NamedFilterList& filters);
							~GetFrameTimestampTest	(void);

private:
	void					executeForConfig		(EGLDisplay display, EGLConfig config);
	void					initializeExtension		(const Library& egl);

	// Not allowed
							GetFrameTimestampTest	(const GetFrameTimestampTest&);
	GetFrameTimestampTest&	operator=				(const GetFrameTimestampTest&);

	// TODO: Move these to eglw::Library.
	eglGetNextFrameIdANDROIDFunc				m_eglGetNextFrameIdANDROID;
	eglGetCompositorTimingANDROIDFunc			m_eglGetCompositorTimingANDROID;
	eglGetCompositorTimingSupportedANDROIDFunc	m_eglGetCompositorTimingSupportedANDROID;
	eglGetFrameTimestampsANDROIDFunc			m_eglGetFrameTimestampsANDROID;
	eglGetFrameTimestampSupportedANDROIDFunc	m_eglGetFrameTimestampSupportedANDROID;

	tcu::ResultCollector						m_result;
};

GetFrameTimestampTest::GetFrameTimestampTest (EglTestContext& eglTestCtx, const NamedFilterList& filters)
	: SimpleConfigCase							(eglTestCtx, filters.getName(), filters.getDescription(), filters)
	, m_eglGetNextFrameIdANDROID				(DE_NULL)
	, m_eglGetCompositorTimingANDROID			(DE_NULL)
	, m_eglGetCompositorTimingSupportedANDROID	(DE_NULL)
	, m_eglGetFrameTimestampsANDROID			(DE_NULL)
	, m_eglGetFrameTimestampSupportedANDROID	(DE_NULL)
	, m_result									(m_testCtx.getLog())
{
}

GetFrameTimestampTest::~GetFrameTimestampTest (void)
{
}

void GetFrameTimestampTest::initializeExtension (const Library& egl)
{
	m_eglGetNextFrameIdANDROID = reinterpret_cast<eglGetNextFrameIdANDROIDFunc>(egl.getProcAddress("eglGetNextFrameIdANDROID"));
	EGLU_CHECK_MSG(egl, "getProcAddress of eglGetNextFrameIdANDROID failed.");
	m_eglGetCompositorTimingANDROID = reinterpret_cast<eglGetCompositorTimingANDROIDFunc>(egl.getProcAddress("eglGetCompositorTimingANDROID"));
	EGLU_CHECK_MSG(egl, "getProcAddress of eglGetCompositorTimingANDROID failed.");
	m_eglGetCompositorTimingSupportedANDROID = reinterpret_cast<eglGetCompositorTimingSupportedANDROIDFunc>(egl.getProcAddress("eglGetCompositorTimingSupportedANDROID"));
	EGLU_CHECK_MSG(egl, "getProcAddress of eglGetCompositorTimingSupportedANDROID failed.");
	m_eglGetFrameTimestampsANDROID = reinterpret_cast<eglGetFrameTimestampsANDROIDFunc>(egl.getProcAddress("eglGetFrameTimestampsANDROID"));
	EGLU_CHECK_MSG(egl, "getProcAddress of eglGetFrameTimestampsANDROID failed.");
	m_eglGetFrameTimestampSupportedANDROID = reinterpret_cast<eglGetFrameTimestampSupportedANDROIDFunc>(egl.getProcAddress("eglGetFrameTimestampSupportedANDROID"));
	EGLU_CHECK_MSG(egl, "getProcAddress of eglGetFrameTimestampSupportedANDROID failed.");
}


string getConfigIdString (const Library& egl, EGLDisplay display, EGLConfig config)
{
	std::ostringstream	stream;
	EGLint				id;

	EGLU_CHECK_CALL(egl, getConfigAttrib(display, config , EGL_CONFIG_ID, &id));

	stream << id;

	return stream.str();
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
	"void main (void)\n"
	"{\n"
	"\tgl_FragColor = vec4(0.9, 0.1, 0.4, 1.0);\n"
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

void GetFrameTimestampTest::executeForConfig (EGLDisplay display, EGLConfig config)
{
	const Library&						egl			= m_eglTestCtx.getLibrary();

	if (!eglu::hasExtension(egl, display, "EGL_ANDROID_get_frame_timestamps"))
		TCU_THROW(NotSupportedError, "EGL_ANDROID_get_frame_timestamps is not supported");

	initializeExtension(egl);

	const string						configIdStr	(getConfigIdString(egl, display, config));
	tcu::ScopedLogSection				logSection	(m_testCtx.getLog(), ("Config ID " + configIdStr).c_str(), ("Config ID " + configIdStr).c_str());
	const eglu::NativeWindowFactory&	factory		= eglu::selectNativeWindowFactory(m_eglTestCtx.getNativeDisplayFactory(), m_testCtx.getCommandLine());

	{
		TestLog& log = m_testCtx.getLog();

		log << TestLog::Message << "EGL_RED_SIZE: "		<< eglu::getConfigAttribInt(egl, display, config, EGL_RED_SIZE)		<< TestLog::EndMessage;
		log << TestLog::Message << "EGL_GREEN_SIZE: "	<< eglu::getConfigAttribInt(egl, display, config, EGL_GREEN_SIZE)	<< TestLog::EndMessage;
		log << TestLog::Message << "EGL_BLUE_SIZE: "	<< eglu::getConfigAttribInt(egl, display, config, EGL_BLUE_SIZE)	<< TestLog::EndMessage;
		log << TestLog::Message << "EGL_ALPHA_SIZE: "	<< eglu::getConfigAttribInt(egl, display, config, EGL_ALPHA_SIZE)	<< TestLog::EndMessage;
		log << TestLog::Message << "EGL_DEPTH_SIZE: "	<< eglu::getConfigAttribInt(egl, display, config, EGL_DEPTH_SIZE)	<< TestLog::EndMessage;
		log << TestLog::Message << "EGL_STENCIL_SIZE: "	<< eglu::getConfigAttribInt(egl, display, config, EGL_STENCIL_SIZE)	<< TestLog::EndMessage;
		log << TestLog::Message << "EGL_SAMPLES: "		<< eglu::getConfigAttribInt(egl, display, config, EGL_SAMPLES)		<< TestLog::EndMessage;
	}

	de::UniquePtr<eglu::NativeWindow>	window	(factory.createWindow(&m_eglTestCtx.getNativeDisplay(), display, config, DE_NULL, eglu::WindowParams(128, 128, eglu::WindowParams::VISIBILITY_VISIBLE)));

	eglu::UniqueSurface					surface	(egl, display, eglu::createWindowSurface(m_eglTestCtx.getNativeDisplay(), *window, display, config, DE_NULL));
	eglu::UniqueContext					context	(egl, display, createGLES2Context(egl, display, config));
	glw::Functions						gl;
	deUint32							program = 0;

	EGLU_CHECK_CALL(egl, surfaceAttrib(display, *surface, EGL_TIMESTAMPS_ANDROID, EGL_TRUE));

	m_eglTestCtx.initGLFunctions(&gl, glu::ApiType::es(2,0));

	EGLU_CHECK_CALL(egl, makeCurrent(display, *surface, *surface, *context));

	try
	{
		// Verify required timestamps are supported.
		const eglw::EGLint requiredTimestamps[] =
		{
			EGL_REQUESTED_PRESENT_TIME_ANDROID,
			EGL_RENDERING_COMPLETE_TIME_ANDROID,
			EGL_COMPOSITION_LATCH_TIME_ANDROID,
			EGL_FIRST_COMPOSITION_START_TIME_ANDROID,
			EGL_LAST_COMPOSITION_START_TIME_ANDROID,
			EGL_FIRST_COMPOSITION_GPU_FINISHED_TIME_ANDROID,
			EGL_DEQUEUE_READY_TIME_ANDROID,
			EGL_READS_DONE_TIME_ANDROID,
		};
		const size_t requiredTimestampsCount = DE_LENGTH_OF_ARRAY(requiredTimestamps);

		for (size_t i = 0; i < requiredTimestampsCount; i++)
		{
			const bool supported = m_eglGetFrameTimestampSupportedANDROID(display, *surface, requiredTimestamps[i]);
			EGLU_CHECK_MSG(egl, "eglGetFrameTimestampSupportedANDROID failed.");
			TCU_CHECK_MSG(supported, "Required timestamp not supported.");
		}

		// Verify either retire or present is supported.
		const bool retireSupported = m_eglGetFrameTimestampSupportedANDROID(display, *surface, EGL_DISPLAY_RETIRE_TIME_ANDROID);
		EGLU_CHECK_MSG(egl, "eglGetFrameTimestampSupportedANDROID failed.");
		const bool presentSupported = m_eglGetFrameTimestampSupportedANDROID(display, *surface, EGL_DISPLAY_PRESENT_TIME_ANDROID);
		EGLU_CHECK_MSG(egl, "eglGetFrameTimestampSupportedANDROID failed.");
		TCU_CHECK_MSG(retireSupported != presentSupported, "DISPLAY_RETIRE or DISPLAY_PRESENT must be supported, but not both.");

		// Verify compositor timings are supported.
		const bool deadlineSupported = m_eglGetCompositorTimingSupportedANDROID(display, *surface, EGL_COMPOSITE_DEADLINE_ANDROID);
		EGLU_CHECK_MSG(egl, "eglGetCompositorTimingSupportedANDROID failed.");
		TCU_CHECK_MSG(deadlineSupported, "EGL_COMPOSITE_DEADLINE_ANDROID not supported.");
		const bool intervalSupported = m_eglGetCompositorTimingSupportedANDROID(display, *surface, EGL_COMPOSITE_INTERVAL_ANDROID);
		EGLU_CHECK_MSG(egl, "eglGetCompositorTimingSupportedANDROID failed.");
		TCU_CHECK_MSG(intervalSupported, "EGL_COMPOSITE_INTERVAL_ANDROID not supported.");
		const bool latencySupported = m_eglGetCompositorTimingSupportedANDROID(display, *surface, EGL_COMPOSITE_TO_PRESENT_LATENCY_ANDROID);
		EGLU_CHECK_MSG(egl, "eglGetCompositorTimingSupportedANDROID failed.");
		TCU_CHECK_MSG(latencySupported, "EGL_COMPOSITE_TO_PRESENT_LATENCY_ANDROID not supported.");


		const eglw::EGLint frameTimestampNames[] =
		{
			EGL_REQUESTED_PRESENT_TIME_ANDROID,
			EGL_RENDERING_COMPLETE_TIME_ANDROID,
			EGL_COMPOSITION_LATCH_TIME_ANDROID,
			EGL_FIRST_COMPOSITION_START_TIME_ANDROID,
			EGL_LAST_COMPOSITION_START_TIME_ANDROID,
			EGL_FIRST_COMPOSITION_GPU_FINISHED_TIME_ANDROID,
			presentSupported ? EGL_DISPLAY_PRESENT_TIME_ANDROID : EGL_DISPLAY_RETIRE_TIME_ANDROID,
			EGL_DEQUEUE_READY_TIME_ANDROID,
			EGL_READS_DONE_TIME_ANDROID,
		};
		const size_t frameTimestampCount = DE_LENGTH_OF_ARRAY(frameTimestampNames);

		const float positions1[] =
		{
			 0.00f,  0.00f,
			 0.75f,  0.00f,
			 0.75f,  0.75f,

			 0.75f,  0.75f,
			 0.00f,  0.75f,
			 0.00f,  0.00f
		};

		const float positions2[] =
		{
			-0.75f, -0.75f,
			 0.00f, -0.75f,
			 0.00f,  0.00f,

			 0.00f,  0.00f,
			-0.75f,  0.00f,
			-0.75f, -0.75f
		};

		deUint32 posLocation;

		program	= createGLES2Program(gl, m_testCtx.getLog());

		gl.useProgram(program);
		posLocation	= gl.getAttribLocation(program, "a_pos");
		gl.enableVertexAttribArray(posLocation);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to setup shader program for rendering");

		const size_t frameCount = 120;
		std::vector<FrameTimes> frameTimes(frameCount);
		for (size_t i = 0; i < frameCount; i++)
		{
			FrameTimes& frame = frameTimes[i];

			const eglw::EGLint compositorTimingNames[] =
			{
				EGL_COMPOSITE_DEADLINE_ANDROID,
				EGL_COMPOSITE_INTERVAL_ANDROID,
				EGL_COMPOSITE_TO_PRESENT_LATENCY_ANDROID,
			};
			const EGLint compositorTimingCount = DE_LENGTH_OF_ARRAY(compositorTimingNames);
			EGLnsecsANDROID compositorTimingValues[compositorTimingCount] = { -2 };

			// Get the current time before making any API calls in case "now"
			// just happens to get sampled near one of the composite deadlines.
			EGLnsecsANDROID now = getNanoseconds();

			// Get the frame id.
			EGLuint64KHR nextFrameId = 0;
			CHECK_NAKED_EGL_CALL(egl, m_eglGetNextFrameIdANDROID(display, *surface, &nextFrameId));
			frame.frameId				=	nextFrameId;

			// Get the compositor timing.
			CHECK_NAKED_EGL_CALL(egl, m_eglGetCompositorTimingANDROID(
				display, *surface, compositorTimingCount,
				compositorTimingNames, compositorTimingValues));
			frame.compositeDeadline			=	compositorTimingValues[0];
			frame.compositeInterval			=	compositorTimingValues[1];
			frame.compositeToPresentLatency	=	compositorTimingValues[2];

			// Verify compositor timing is sane.
			m_result.check(1000000 < frame.compositeInterval, "Reported refresh rate greater than 1kHz.");
			m_result.check(frame.compositeInterval < 1000000000, "Reported refresh rate less than 1Hz.");
			m_result.check(0 < frame.compositeToPresentLatency, "Composite to present latency must be greater than 0.");
			m_result.check(frame.compositeToPresentLatency < frame.compositeInterval * 3, "Composite to present latency is more than 3 vsyncs.");
			const EGLnsecsANDROID minDeadline = now;
			m_result.check(minDeadline < frame.compositeDeadline, "Next composite deadline is in the past.");
			const EGLnsecsANDROID maxDeadline = now + frame.compositeInterval * 2;
			m_result.check(frame.compositeDeadline < maxDeadline, "Next composite deadline over two intervals away.");

			const float colorAngle = (static_cast<float>(i) / static_cast<float>(frameCount)) * 6.28318f;
			gl.clearColor((1.0f + deFloatSin(colorAngle)) / 2.0f, 0.7f, (1.0f + deFloatCos(colorAngle)) / 2.0f, 1.0f);
			gl.clear(GL_COLOR_BUFFER_BIT);
			GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to clear surface");

			const bool posSelect  = ((i % 2) == 0);
			gl.vertexAttribPointer(posLocation, 2, GL_FLOAT, GL_FALSE, 0, posSelect ? positions1 : positions2);
			gl.drawArrays(GL_TRIANGLES, 0, 6);
			GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to render");

			frame.swapBufferBeginNs = getNanoseconds();
			EGLU_CHECK_CALL(egl, swapBuffers(display, *surface));

			// All timestamps from 5 frames ago should definitely be available.
			const size_t frameDelay = 5;
			if (i >= frameDelay)
			{
				FrameTimes&		frame5ago									=	frameTimes[i-frameDelay];
				EGLnsecsANDROID frameTimestampValues[frameTimestampCount]	=	{ 0 };
				// \todo [2017-01-25 brianderson] Remove this work around once reads done is fixed.
				const bool verifyReadsDone									=	i > (frameDelay + 3);

				CHECK_NAKED_EGL_CALL(egl, m_eglGetFrameTimestampsANDROID(
					display, *surface, frame5ago.frameId, frameTimestampCount,
					frameTimestampNames, frameTimestampValues));

				frame5ago.requestedPresent				=	frameTimestampValues[0];
				frame5ago.renderingComplete				=	frameTimestampValues[1];
				frame5ago.latch							=	frameTimestampValues[2];
				frame5ago.firstCompositionStart			=	frameTimestampValues[3];
				frame5ago.lastCompositionStart			=	frameTimestampValues[4];
				frame5ago.firstCompositionGpuFinished	=	frameTimestampValues[5];
				frame5ago.dequeueReady					=	frameTimestampValues[7];
				frame5ago.readsDone						=	frameTimestampValues[8];
				if (presentSupported)
					frame5ago.displayPresent			=	frameTimestampValues[6];
				else
					frame5ago.displayRetire			=	frameTimestampValues[6];

				verifySingleFrame(frame5ago, m_result, verifyReadsDone);
				if (i >= frameDelay + 1)
				{
					FrameTimes& frame6ago = frameTimes[i-frameDelay-1];
					verifyNeighboringFrames(frame6ago, frame5ago, m_result, verifyReadsDone);
				}
			}
		}

		// All timestamps for the most recently swapped frame should
		// become available by only polling eglGetFrametimestamps.
		// No additional swaps should be necessary.
		FrameTimes&				lastFrame				=	frameTimes.back();
		const EGLnsecsANDROID	pollingDeadline			=	lastFrame.swapBufferBeginNs + 1000000000;
		bool					finalTimestampAvaiable	=	false;

		do
		{
			EGLnsecsANDROID frameTimestampValues[frameTimestampCount] = { 0 };
			CHECK_NAKED_EGL_CALL(egl, m_eglGetFrameTimestampsANDROID(
				display, *surface, lastFrame.frameId, frameTimestampCount,
				frameTimestampNames, frameTimestampValues));

			lastFrame.requestedPresent				=	frameTimestampValues[0];
			lastFrame.renderingComplete				=	frameTimestampValues[1];
			lastFrame.latch							=	frameTimestampValues[2];
			lastFrame.firstCompositionStart			=	frameTimestampValues[3];
			lastFrame.lastCompositionStart			=	frameTimestampValues[4];
			lastFrame.firstCompositionGpuFinished	=	frameTimestampValues[5];
			lastFrame.dequeueReady					=	frameTimestampValues[7];
			lastFrame.readsDone						=	frameTimestampValues[8];
			if (presentSupported)
			{
				lastFrame.displayPresent = frameTimestampValues[6];
				if (timestampExists(lastFrame.displayPresent))
					finalTimestampAvaiable = true;
			}
			else
			{
				lastFrame.displayRetire = frameTimestampValues[6];
				if (timestampExists(lastFrame.firstCompositionStart))
					finalTimestampAvaiable = true;
			}

			if (getNanoseconds() > pollingDeadline)
				break;
		} while (!finalTimestampAvaiable);

		m_result.check(finalTimestampAvaiable, "Timed out polling for timestamps of last swap.");
		m_result.check(timestampExists(lastFrame.requestedPresent), "Rendering complete of last swap not avaiable.");
		m_result.check(timestampExists(lastFrame.renderingComplete), "Rendering complete of last swap not avaiable.");
		m_result.check(timestampExists(lastFrame.latch), "Latch of last swap not avaiable.");
		m_result.check(timestampExists(lastFrame.firstCompositionStart), "First composite time of last swap not avaiable.");
		m_result.check(timestampExists(lastFrame.lastCompositionStart), "Last composite time of last swap not avaiable.");

		window->processEvents();
		gl.disableVertexAttribArray(posLocation);
		gl.useProgram(0);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to release program state");

		gl.deleteProgram(program);
		program = 0;
		GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteProgram()");

		m_result.setTestContextResult(m_testCtx);
	}
	catch (...)
	{
		if (program != 0)
			gl.deleteProgram(program);

		EGLU_CHECK_CALL(egl, makeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
		throw;
	}

	EGLU_CHECK_CALL(egl, makeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
}

class GetFrameTimestampsTests : public TestCaseGroup
{
public:
								GetFrameTimestampsTests	(EglTestContext& eglTestCtx);
	void						init					(void);

private:
								GetFrameTimestampsTests	(const GetFrameTimestampsTests&);
	GetFrameTimestampsTests&	operator=				(const GetFrameTimestampsTests&);
};


GetFrameTimestampsTests::GetFrameTimestampsTests (EglTestContext& eglTestCtx)
	: TestCaseGroup(eglTestCtx, "get_frame_timestamps", "Get frame timestamp tests")
{
}

bool isWindow (const eglu::CandidateConfig& c)
{
	return (c.surfaceType() & EGL_WINDOW_BIT) != 0;
}

void GetFrameTimestampsTests::init (void)
{
	eglu::FilterList baseFilters;
	baseFilters << isWindow;

	vector<NamedFilterList> filterLists;
	getDefaultFilterLists(filterLists, baseFilters);

	for (vector<NamedFilterList>::iterator i = filterLists.begin(); i != filterLists.end(); i++)
		addChild(new GetFrameTimestampTest(m_eglTestCtx, *i));
}

} // anonymous

TestCaseGroup* createGetFrameTimestampsTests (EglTestContext& eglTestCtx)
{
	return new GetFrameTimestampsTests(eglTestCtx);
}

} // egl
} // deqp
