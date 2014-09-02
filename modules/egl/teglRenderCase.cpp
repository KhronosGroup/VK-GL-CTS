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
 * \brief Base class for rendering tests.
 *//*--------------------------------------------------------------------*/

#include "teglRenderCase.hpp"

#include "teglSimpleConfigCase.hpp"

#include "egluNativeDisplay.hpp"
#include "egluNativeWindow.hpp"
#include "egluNativePixmap.hpp"
#include "egluUtil.hpp"

#include "tcuRenderTarget.hpp"
#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include <algorithm>
#include <iterator>
#include <memory>
#include <set>

#include <EGL/eglext.h>

#if !defined(EGL_OPENGL_ES3_BIT_KHR)
#	define EGL_OPENGL_ES3_BIT_KHR	0x0040
#endif
#if !defined(EGL_CONTEXT_MAJOR_VERSION_KHR)
#	define EGL_CONTEXT_MAJOR_VERSION_KHR EGL_CONTEXT_CLIENT_VERSION
#endif

using std::string;
using std::vector;
using std::set;

using tcu::TestLog;

namespace deqp
{
namespace egl
{

// \todo [2013-04-24 pyry] Should we instead store surface bit somewhere?
template<class Derived, class Base>
inline bool instanceOf (Base& obj)
{
	return dynamic_cast<Derived*>(&obj) != DE_NULL;
}

static void postSurface (tcu::egl::Surface& surface)
{
	const bool	isWindow	= instanceOf<tcu::egl::WindowSurface>(surface);
	const bool	isPixmap	= instanceOf<tcu::egl::PixmapSurface>(surface);
	const bool	isPbuffer	= instanceOf<tcu::egl::PbufferSurface>(surface);

	DE_ASSERT((isWindow?1:0) + (isPixmap?1:0) + (isPbuffer?1:0) == 1);

	if (isWindow)
	{
		tcu::egl::WindowSurface& window = static_cast<tcu::egl::WindowSurface&>(surface);
		window.swapBuffers();
	}
	else if (isPixmap)
	{
		TCU_CHECK_EGL_CALL(eglWaitClient());
	}
	else
	{
		DE_ASSERT(isPbuffer);
		DE_UNREF(isPbuffer);
		TCU_CHECK_EGL_CALL(eglWaitClient());
	}
}

// RenderCase

RenderCase::RenderCase (EglTestContext& eglTestCtx, const char* name, const char* description, EGLint apiMask, EGLint surfaceTypeMask, const vector<EGLint>& configIds)
	: SimpleConfigCase	(eglTestCtx, name, description, configIds)
	, m_apiMask			(apiMask)
	, m_surfaceTypeMask	(surfaceTypeMask)
{
}

RenderCase::~RenderCase (void)
{
}

EGLint RenderCase::getSupportedApis (void)
{
	EGLint apiMask = 0;

#if defined(DEQP_SUPPORT_GLES2)
	apiMask |= EGL_OPENGL_ES2_BIT;
#endif

#if defined(DEQP_SUPPORT_GLES3)
	apiMask |= EGL_OPENGL_ES3_BIT_KHR;
#endif

#if defined(DEQP_SUPPORT_GLES1)
	apiMask |= EGL_OPENGL_ES_BIT;
#endif

#if defined(DEQP_SUPPORT_VG)
	apiMask |= EGL_OPENVG_BIT;
#endif

	return apiMask;
}

void RenderCase::executeForConfig (tcu::egl::Display& defaultDisplay, EGLConfig config)
{
	tcu::TestLog&			log				= m_testCtx.getLog();
	int						width			= 128;
	int						height			= 128;
	EGLint					configId		= defaultDisplay.getConfigAttrib(config, EGL_CONFIG_ID);
	bool					isOk			= true;
	string					failReason		= "";

	if (m_surfaceTypeMask & EGL_WINDOW_BIT)
	{
		tcu::ScopedLogSection(log, (string("Config") + de::toString(configId) + "-Window").c_str(),
										(string("Config ID ") + de::toString(configId) + ", window surface").c_str());

		try
		{
			tcu::egl::Display&					display		= m_eglTestCtx.getDisplay();
			de::UniquePtr<eglu::NativeWindow>	window		(m_eglTestCtx.createNativeWindow(display.getEGLDisplay(), config, DE_NULL, width, height, eglu::parseWindowVisibility(m_testCtx.getCommandLine())));
			EGLSurface							eglSurface	= createWindowSurface(m_eglTestCtx.getNativeDisplay(), *window, display.getEGLDisplay(), config, DE_NULL);
			tcu::egl::WindowSurface				surface		(display, eglSurface);

			executeForSurface(display, surface, config);
		}
		catch (const tcu::TestError& e)
		{
			log << e;
			isOk = false;
			failReason = e.what();
		}
	}

	if (m_surfaceTypeMask & EGL_PIXMAP_BIT)
	{
		tcu::ScopedLogSection(log, (string("Config") + de::toString(configId) + "-Pixmap").c_str(),
										(string("Config ID ") + de::toString(configId) + ", pixmap surface").c_str());

		try
		{
			tcu::egl::Display&					display		= m_eglTestCtx.getDisplay();
			std::auto_ptr<eglu::NativePixmap>	pixmap		(m_eglTestCtx.createNativePixmap(display.getEGLDisplay(), config, DE_NULL, width, height));
			EGLSurface							eglSurface	= createPixmapSurface(m_eglTestCtx.getNativeDisplay(), *pixmap, display.getEGLDisplay(), config, DE_NULL);
			tcu::egl::PixmapSurface				surface		(display, eglSurface);

			executeForSurface(display, surface, config);
		}
		catch (const tcu::TestError& e)
		{
			log << e;
			isOk = false;
			failReason = e.what();
		}
	}

	if (m_surfaceTypeMask & EGL_PBUFFER_BIT)
	{
		tcu::ScopedLogSection(log, (string("Config") + de::toString(configId) + "-Pbuffer").c_str(),
										(string("Config ID ") + de::toString(configId) + ", pbuffer surface").c_str());
		try
		{
			EGLint surfaceAttribs[] =
			{
				EGL_WIDTH,	width,
				EGL_HEIGHT,	height,
				EGL_NONE
			};

			tcu::egl::PbufferSurface surface(defaultDisplay, config, surfaceAttribs);

			executeForSurface(defaultDisplay, surface, config);
		}
		catch (const tcu::TestError& e)
		{
			log << e;
			isOk = false;
			failReason = e.what();
		}
	}

	if (!isOk && m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, failReason.c_str());
}

// SingleContextRenderCase

SingleContextRenderCase::SingleContextRenderCase (EglTestContext& eglTestCtx, const char* name, const char* description, EGLint apiMask, EGLint surfaceTypeMask, const std::vector<EGLint>& configIds)
	: RenderCase(eglTestCtx, name, description, apiMask, surfaceTypeMask, configIds)
{
}

SingleContextRenderCase::~SingleContextRenderCase (void)
{
}

void SingleContextRenderCase::executeForSurface (tcu::egl::Display& display, tcu::egl::Surface& surface, EGLConfig config)
{
	EGLint				supportedApis	= getSupportedApis();
	const EGLint		apis[]			= { EGL_OPENGL_ES2_BIT, EGL_OPENGL_ES3_BIT_KHR, EGL_OPENGL_ES_BIT, EGL_OPENVG_BIT };
	tcu::TestLog&		log				= m_testCtx.getLog();

	// Check if case is supported
	if ((m_apiMask & supportedApis) != m_apiMask)
		throw tcu::NotSupportedError("Client APIs not supported", "", __FILE__, __LINE__);

	for (int apiNdx = 0; apiNdx < DE_LENGTH_OF_ARRAY(apis); apiNdx++)
	{
		EGLint apiBit = apis[apiNdx];

		if ((apiBit & m_apiMask) == 0)
			continue; // Skip this api.

		EGLint			api		= EGL_NONE;
		const char*		apiName	= DE_NULL;
		vector<EGLint>	contextAttribs;

		// Select api enum and build context attributes.
		switch (apiBit)
		{
			case EGL_OPENGL_ES2_BIT:
				api		= EGL_OPENGL_ES_API;
				apiName	= "OpenGL ES 2.x";
				contextAttribs.push_back(EGL_CONTEXT_CLIENT_VERSION);
				contextAttribs.push_back(2);
				break;

			case EGL_OPENGL_ES3_BIT_KHR:
				api		= EGL_OPENGL_ES_API;
				apiName	= "OpenGL ES 3.x";
				contextAttribs.push_back(EGL_CONTEXT_MAJOR_VERSION_KHR);
				contextAttribs.push_back(3);
				break;

			case EGL_OPENGL_ES_BIT:
				api		= EGL_OPENGL_ES_API;
				apiName	= "OpenGL ES 1.x";
				contextAttribs.push_back(EGL_CONTEXT_CLIENT_VERSION);
				contextAttribs.push_back(1);
				break;

			case EGL_OPENVG_BIT:
				api		= EGL_OPENVG_API;
				apiName	= "OpenVG";
				break;

			default:
				DE_ASSERT(DE_FALSE);
		}

		contextAttribs.push_back(EGL_NONE);

		log << TestLog::Message << apiName << TestLog::EndMessage;

		tcu::egl::Context context(display, config, &contextAttribs[0], api);

		context.makeCurrent(surface, surface);
		executeForContext(display, context, surface, apiBit);

		// Call SwapBuffers() / WaitClient() to finish rendering
		postSurface(surface);
	}
}

// MultiContextRenderCase

MultiContextRenderCase::MultiContextRenderCase (EglTestContext& eglTestCtx, const char* name, const char* description, EGLint api, EGLint surfaceType, const vector<EGLint>& configIds, int numContextsPerApi)
	: RenderCase			(eglTestCtx, name, description, api, surfaceType, configIds)
	, m_numContextsPerApi	(numContextsPerApi)
{
}

MultiContextRenderCase::~MultiContextRenderCase (void)
{
}

void MultiContextRenderCase::executeForSurface (tcu::egl::Display& display, tcu::egl::Surface& surface, EGLConfig config)
{
	vector<std::pair<EGLint, tcu::egl::Context*> > contexts;
	contexts.reserve(3*m_numContextsPerApi); // 3 types of contexts at maximum.

	try
	{
		// Create contexts that will participate in rendering.
		for (int ndx = 0; ndx < m_numContextsPerApi; ndx++)
		{
			if (m_apiMask & EGL_OPENGL_ES2_BIT)
			{
				static const EGLint attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
				contexts.push_back(std::make_pair(EGL_OPENGL_ES2_BIT, new tcu::egl::Context(display, config, &attribs[0], EGL_OPENGL_ES_API)));
			}

			if (m_apiMask & EGL_OPENGL_ES3_BIT_KHR)
			{
				static const EGLint attribs[] = { EGL_CONTEXT_MAJOR_VERSION_KHR, 3, EGL_NONE };
				contexts.push_back(std::make_pair(EGL_OPENGL_ES3_BIT_KHR, new tcu::egl::Context(display, config, &attribs[0], EGL_OPENGL_ES_API)));
			}

			if (m_apiMask & EGL_OPENGL_ES_BIT)
			{
				static const EGLint attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 1, EGL_NONE };
				contexts.push_back(std::make_pair(EGL_OPENGL_ES_BIT, new tcu::egl::Context(display, config, &attribs[0], EGL_OPENGL_ES_API)));
			}

			if (m_apiMask & EGL_OPENVG_BIT)
			{
				static const EGLint attribs[] = { EGL_NONE };
				contexts.push_back(std::make_pair(EGL_OPENVG_BIT, new tcu::egl::Context(display, config, &attribs[0], EGL_OPENVG_API)));
			}
		}

		// Execute for contexts.
		executeForContexts(display, surface, config, contexts);
	}
	catch (const std::exception&)
	{
		// Make sure all contexts have been destroyed.
		for (vector<std::pair<EGLint, tcu::egl::Context*> >::iterator i = contexts.begin(); i != contexts.end(); i++)
			delete i->second;
		throw;
	}

	// Destroy contexts.
	for (vector<std::pair<EGLint, tcu::egl::Context*> >::iterator i = contexts.begin(); i != contexts.end(); i++)
		delete i->second;
}

// Utilities

void addRenderConfigIdSet (
	vector<RenderConfigIdSet>&			configSets,
	const vector<eglu::ConfigInfo>&		configInfos,
	const eglu::FilterList&				baseFilters,
	const char*							name,
	tcu::RGBA							colorBits,
	EGLint								surfaceType)
{
	eglu::FilterList filters = baseFilters;
	filters << (eglu::ConfigColorBits() == colorBits) << (eglu::ConfigSurfaceType() & surfaceType);

	vector<EGLint> matchingConfigs;

	for (vector<eglu::ConfigInfo>::const_iterator configIter = configInfos.begin(); configIter != configInfos.end(); configIter++)
	{
		if (!filters.match(*configIter))
			continue;

		matchingConfigs.push_back(configIter->configId);
	}

	configSets.push_back(RenderConfigIdSet(name, "", matchingConfigs, surfaceType));
}

void addRenderConfigIdSet (
	vector<RenderConfigIdSet>&			configSets,
	const vector<eglu::ConfigInfo>&		configInfos,
	const eglu::FilterList&				baseFilters,
	const char*							name,
	tcu::RGBA							colorBits)
{
	addRenderConfigIdSet(configSets, configInfos, baseFilters, (string(name) + "_window").c_str(),	colorBits, EGL_WINDOW_BIT);
	addRenderConfigIdSet(configSets, configInfos, baseFilters, (string(name) + "_pixmap").c_str(),	colorBits, EGL_PIXMAP_BIT);
	addRenderConfigIdSet(configSets, configInfos, baseFilters, (string(name) + "_pbuffer").c_str(),	colorBits, EGL_PBUFFER_BIT);
}

void getDefaultRenderConfigIdSets (vector<RenderConfigIdSet>& configSets, const vector<eglu::ConfigInfo>& configInfos, const eglu::FilterList& baseFilters)
{
	using tcu::RGBA;

	addRenderConfigIdSet(configSets, configInfos, baseFilters, "rgb565",	RGBA(5, 6, 5, 0));
	addRenderConfigIdSet(configSets, configInfos, baseFilters, "rgb888",	RGBA(8, 8, 8, 0));
	addRenderConfigIdSet(configSets, configInfos, baseFilters, "rgba4444",	RGBA(4, 4, 4, 4));
	addRenderConfigIdSet(configSets, configInfos, baseFilters, "rgba5551",	RGBA(5, 5, 5, 1));
	addRenderConfigIdSet(configSets, configInfos, baseFilters, "rgba8888",	RGBA(8, 8, 8, 8));

	// Add other config ids to "other" set
	{
		set<EGLint>		usedConfigs;
		vector<EGLint>	otherCfgSet;

		for (vector<RenderConfigIdSet>::const_iterator setIter = configSets.begin(); setIter != configSets.end(); setIter++)
		{
			const vector<EGLint>& setCfgs = setIter->getConfigIds();
			for (vector<EGLint>::const_iterator i = setCfgs.begin(); i != setCfgs.end(); i++)
				usedConfigs.insert(*i);
		}

		for (vector<eglu::ConfigInfo>::const_iterator cfgIter = configInfos.begin(); cfgIter != configInfos.end(); cfgIter++)
		{
			if (!baseFilters.match(*cfgIter))
				continue;

			EGLint id = cfgIter->configId;

			if (usedConfigs.find(id) == usedConfigs.end())
				otherCfgSet.push_back(id);
		}

		configSets.push_back(RenderConfigIdSet("other", "", otherCfgSet, EGL_WINDOW_BIT|EGL_PIXMAP_BIT|EGL_PBUFFER_BIT));
	}
}

} // egl
} // deqp
