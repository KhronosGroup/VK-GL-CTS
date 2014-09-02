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
 * \brief Color clear case.
 *//*--------------------------------------------------------------------*/

#include "teglColorClearCase.hpp"
#include "tcuTestLog.hpp"
#include "deRandom.hpp"
#include "deString.h"
#include "tcuImageCompare.hpp"
#include "tcuVector.hpp"
#include "tcuTextureUtil.hpp"
#include "deThread.hpp"
#include "deSemaphore.hpp"
#include "deSharedPtr.hpp"
#include "teglGLES1RenderUtil.hpp"
#include "teglGLES2RenderUtil.hpp"
#include "teglVGRenderUtil.hpp"

#include <memory>
#include <iterator>

#include <EGL/eglext.h>

#if !defined(EGL_OPENGL_ES3_BIT_KHR)
#	define EGL_OPENGL_ES3_BIT_KHR	0x0040
#endif
#if !defined(EGL_CONTEXT_MAJOR_VERSION_KHR)
#	define EGL_CONTEXT_MAJOR_VERSION_KHR EGL_CONTEXT_CLIENT_VERSION
#endif

using tcu::TestLog;
using tcu::RGBA;

using std::vector;

namespace deqp
{
namespace egl
{

// Utilities.

struct ClearOp
{
	ClearOp (int x_, int y_, int width_, int height_, const tcu::RGBA& color_)
		: x			(x_)
		, y			(y_)
		, width		(width_)
		, height	(height_)
		, color		(color_)
	{
	}

	ClearOp (void)
		: x			(0)
		, y			(0)
		, width		(0)
		, height	(0)
		, color		(0)
	{
	}

	int			x;
	int			y;
	int			width;
	int			height;
	tcu::RGBA	color;
};

static ClearOp computeRandomClear (de::Random& rnd, int width, int height)
{
	int			w		= rnd.getInt(1, width);
	int			h		= rnd.getInt(1, height);
	int			x		= rnd.getInt(0, width-w);
	int			y		= rnd.getInt(0, height-h);
	tcu::RGBA	col		(rnd.getUint32());

	return ClearOp(x, y, w, h, col);
}

static void renderReference (tcu::Surface& dst, const vector<ClearOp>& clears, const tcu::PixelFormat& pixelFormat)
{
	for (vector<ClearOp>::const_iterator clearIter = clears.begin(); clearIter != clears.end(); clearIter++)
	{
		tcu::PixelBufferAccess access = tcu::getSubregion(dst.getAccess(), clearIter->x, clearIter->y, 0, clearIter->width, clearIter->height, 1);
		tcu::clear(access, pixelFormat.convertColor(clearIter->color).toIVec());
	}
}

static void renderClear (EGLint api, const ClearOp& clear)
{
	switch (api)
	{
		case EGL_OPENGL_ES_BIT:			gles1::clear(clear.x, clear.y, clear.width, clear.height, clear.color.toVec());	break;
		case EGL_OPENGL_ES2_BIT:		gles2::clear(clear.x, clear.y, clear.width, clear.height, clear.color.toVec());	break;
		case EGL_OPENGL_ES3_BIT_KHR:	gles2::clear(clear.x, clear.y, clear.width, clear.height, clear.color.toVec());	break;
		case EGL_OPENVG_BIT:			vg::clear	(clear.x, clear.y, clear.width, clear.height, clear.color.toVec());	break;
		default:
			DE_ASSERT(DE_FALSE);
	}
}

static void readPixels (EGLint api, tcu::Surface& dst)
{
	switch (api)
	{
		case EGL_OPENGL_ES_BIT:			gles1::readPixels	(dst, 0, 0, dst.getWidth(), dst.getHeight());	break;
		case EGL_OPENGL_ES2_BIT:		gles2::readPixels	(dst, 0, 0, dst.getWidth(), dst.getHeight());	break;
		case EGL_OPENGL_ES3_BIT_KHR:	gles2::readPixels	(dst, 0, 0, dst.getWidth(), dst.getHeight());	break;
		case EGL_OPENVG_BIT:			vg::readPixels		(dst, 0, 0, dst.getWidth(), dst.getHeight());	break;
		default:
			DE_ASSERT(DE_FALSE);
	}
}

// SingleThreadColorClearCase

SingleThreadColorClearCase::SingleThreadColorClearCase (EglTestContext& eglTestCtx, const char* name, const char* description, EGLint api, EGLint surfaceType, const std::vector<EGLint>& configIds, int numContextsPerApi)
	: MultiContextRenderCase(eglTestCtx, name, description, api, surfaceType, configIds, numContextsPerApi)
{
}

void SingleThreadColorClearCase::executeForContexts (tcu::egl::Display& display, tcu::egl::Surface& surface, EGLConfig config, const std::vector<std::pair<EGLint, tcu::egl::Context*> >& contexts)
{
	int					width		= surface.getWidth();
	int					height		= surface.getHeight();

	TestLog&			log			= m_testCtx.getLog();

	tcu::Surface		refFrame	(width, height);
	tcu::Surface		frame		(width, height);
	tcu::PixelFormat	pixelFmt;

	de::Random			rnd			(deStringHash(getName()));
	vector<ClearOp>		clears;
	const int			ctxClears	= 2;
	const int			numIters	= 3;

	// Query pixel format.
	display.describeConfig(config, pixelFmt);

	// Clear to black using first context.
	{
		EGLint				api			= contexts[0].first;
		tcu::egl::Context*	context		= contexts[0].second;
		ClearOp				clear		(0, 0, width, height, RGBA::black);

		eglMakeCurrent(display.getEGLDisplay(), surface.getEGLSurface(), surface.getEGLSurface(), context->getEGLContext());
		TCU_CHECK_EGL();

		renderClear(api, clear);
		clears.push_back(clear);
	}

	// Render.
	for (int iterNdx = 0; iterNdx < numIters; iterNdx++)
	{
		for (vector<std::pair<EGLint, tcu::egl::Context*> >::const_iterator ctxIter = contexts.begin(); ctxIter != contexts.end(); ctxIter++)
		{
			EGLint				api			= ctxIter->first;
			tcu::egl::Context*	context		= ctxIter->second;

			eglMakeCurrent(display.getEGLDisplay(), surface.getEGLSurface(), surface.getEGLSurface(), context->getEGLContext());
			TCU_CHECK_EGL();

			for (int clearNdx = 0; clearNdx < ctxClears; clearNdx++)
			{
				ClearOp clear = computeRandomClear(rnd, width, height);

				renderClear(api, clear);
				clears.push_back(clear);
			}
		}
	}

	// Read pixels using first context. \todo [pyry] Randomize?
	{
		EGLint				api		= contexts[0].first;
		tcu::egl::Context*	context	= contexts[0].second;

		eglMakeCurrent(display.getEGLDisplay(), surface.getEGLSurface(), surface.getEGLSurface(), context->getEGLContext());
		TCU_CHECK_EGL();

		readPixels(api, frame);
	}

	// Render reference.
	renderReference(refFrame, clears, pixelFmt);

	// Compare images
	{
		bool imagesOk = tcu::pixelThresholdCompare(log, "ComparisonResult", "Image comparison result", refFrame, frame, RGBA(1,1,1,1) + pixelFmt.getColorThreshold(), tcu::COMPARE_LOG_RESULT);

		if (!imagesOk)
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");
	}
}

// MultiThreadColorClearCase

enum
{
	NUM_CLEARS_PER_PACKET	= 2 //!< Number of clears performed in one context activation in one thread.
};

class ColorClearThread;

typedef de::SharedPtr<ColorClearThread>	ColorClearThreadSp;
typedef de::SharedPtr<de::Semaphore>	SemaphoreSp;

struct ClearPacket
{
	ClearPacket (void)
	{
	}

	ClearOp			clears[NUM_CLEARS_PER_PACKET];
	SemaphoreSp		wait;
	SemaphoreSp		signal;
};

class ColorClearThread : public de::Thread
{
public:
	ColorClearThread (tcu::egl::Display& display, tcu::egl::Surface& surface, tcu::egl::Context& context, EGLint api, const std::vector<ClearPacket>& packets)
		: m_display	(display)
		, m_surface	(surface)
		, m_context	(context)
		, m_api		(api)
		, m_packets	(packets)
	{
	}

	void run (void)
	{
		for (std::vector<ClearPacket>::const_iterator packetIter = m_packets.begin(); packetIter != m_packets.end(); packetIter++)
		{
			// Wait until it is our turn.
			packetIter->wait->decrement();

			// Acquire context.
			eglMakeCurrent(m_display.getEGLDisplay(), m_surface.getEGLSurface(), m_surface.getEGLSurface(), m_context.getEGLContext());

			// Execute clears.
			for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(packetIter->clears); ndx++)
				renderClear(m_api, packetIter->clears[ndx]);

			// Release context.
			eglMakeCurrent(m_display.getEGLDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

			// Signal completion.
			packetIter->signal->increment();
		}
	}

private:
	tcu::egl::Display&				m_display;
	tcu::egl::Surface&				m_surface;
	tcu::egl::Context&				m_context;
	EGLint							m_api;
	const std::vector<ClearPacket>&	m_packets;
};

MultiThreadColorClearCase::MultiThreadColorClearCase (EglTestContext& eglTestCtx, const char* name, const char* description, EGLint api, EGLint surfaceType, const std::vector<EGLint>& configIds, int numContextsPerApi)
	: MultiContextRenderCase(eglTestCtx, name, description, api, surfaceType, configIds, numContextsPerApi)
{
}

void MultiThreadColorClearCase::executeForContexts (tcu::egl::Display& display, tcu::egl::Surface& surface, EGLConfig config, const std::vector<std::pair<EGLint, tcu::egl::Context*> >& contexts)
{
	int					width		= surface.getWidth();
	int					height		= surface.getHeight();

	TestLog&			log			= m_testCtx.getLog();

	tcu::Surface		refFrame	(width, height);
	tcu::Surface		frame		(width, height);
	tcu::PixelFormat	pixelFmt;

	de::Random			rnd			(deStringHash(getName()));

	// Query pixel format.
	display.describeConfig(config, pixelFmt);

	// Create clear packets.
	const int						numPacketsPerThread		= 2;
	int								numThreads				= (int)contexts.size();
	int								numPackets				= numThreads * numPacketsPerThread;

	vector<SemaphoreSp>				semaphores				(numPackets+1);
	vector<vector<ClearPacket> >	packets					(numThreads);
	vector<ColorClearThreadSp>		threads					(numThreads);

	// Initialize semaphores.
	for (vector<SemaphoreSp>::iterator sem = semaphores.begin(); sem != semaphores.end(); ++sem)
		*sem = SemaphoreSp(new de::Semaphore(0));

	// Create packets.
	for (int threadNdx = 0; threadNdx < numThreads; threadNdx++)
	{
		packets[threadNdx].resize(numPacketsPerThread);

		for (int packetNdx = 0; packetNdx < numPacketsPerThread; packetNdx++)
		{
			ClearPacket& packet = packets[threadNdx][packetNdx];

			// Threads take turns with packets.
			packet.wait		= semaphores[packetNdx*numThreads + threadNdx];
			packet.signal	= semaphores[packetNdx*numThreads + threadNdx + 1];

			for (int clearNdx = 0; clearNdx < DE_LENGTH_OF_ARRAY(packet.clears); clearNdx++)
			{
				// First clear is always full-screen black.
				if (threadNdx == 0 && packetNdx == 0 && clearNdx == 0)
					packet.clears[clearNdx] = ClearOp(0, 0, width, height, RGBA::black);
				else
					packet.clears[clearNdx] = computeRandomClear(rnd, width, height);
			}
		}
	}

	// Create and launch threads (actual rendering starts once first semaphore is signaled).
	for (int threadNdx = 0; threadNdx < numThreads; threadNdx++)
	{
		threads[threadNdx] = ColorClearThreadSp(new ColorClearThread(display, surface, *contexts[threadNdx].second, contexts[threadNdx].first, packets[threadNdx]));
		threads[threadNdx]->start();
	}

	// Signal start and wait until complete.
	semaphores.front()->increment();
	semaphores.back()->decrement();

	// Read pixels using first context. \todo [pyry] Randomize?
	{
		EGLint				api		= contexts[0].first;
		tcu::egl::Context*	context	= contexts[0].second;

		eglMakeCurrent(display.getEGLDisplay(), surface.getEGLSurface(), surface.getEGLSurface(), context->getEGLContext());
		TCU_CHECK_EGL();

		readPixels(api, frame);
	}

	// Join threads.
	for (int threadNdx = 0; threadNdx < numThreads; threadNdx++)
		threads[threadNdx]->join();

	// Render reference.
	for (int packetNdx = 0; packetNdx < numPacketsPerThread; packetNdx++)
	{
		for (int threadNdx = 0; threadNdx < numThreads; threadNdx++)
		{
			const ClearPacket& packet = packets[threadNdx][packetNdx];
			for (int clearNdx = 0; clearNdx < DE_LENGTH_OF_ARRAY(packet.clears); clearNdx++)
			{
				tcu::PixelBufferAccess access = tcu::getSubregion(refFrame.getAccess(),
																  packet.clears[clearNdx].x, packet.clears[clearNdx].y, 0,
																  packet.clears[clearNdx].width, packet.clears[clearNdx].height, 1);
				tcu::clear(access, pixelFmt.convertColor(packet.clears[clearNdx].color).toIVec());
			}
		}
	}

	// Compare images
	{
		bool imagesOk = tcu::pixelThresholdCompare(log, "ComparisonResult", "Image comparison result", refFrame, frame, RGBA(1,1,1,1) + pixelFmt.getColorThreshold(), tcu::COMPARE_LOG_RESULT);

		if (!imagesOk)
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");
	}
}

} // egl
} // deqp
