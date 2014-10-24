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
 * \brief Config query tests.
 *//*--------------------------------------------------------------------*/

#include "teglQueryContextTests.hpp"
#include "teglSimpleConfigCase.hpp"
#include "teglRenderCase.hpp"
#include "egluCallLogWrapper.hpp"
#include "egluStrUtil.hpp"
#include "tcuCommandLine.hpp"
#include "tcuTestLog.hpp"
#include "tcuTestContext.hpp"

#include "egluUtil.hpp"
#include "egluNativeDisplay.hpp"
#include "egluNativeWindow.hpp"
#include "egluNativePixmap.hpp"

#include "deUniquePtr.hpp"

#include <vector>

#include <EGL/eglext.h>

#if !defined(EGL_OPENGL_ES3_BIT_KHR)
#	define EGL_OPENGL_ES3_BIT_KHR	0x0040
#endif
#if !defined(EGL_CONTEXT_MAJOR_VERSION_KHR)
#	define EGL_CONTEXT_MAJOR_VERSION_KHR EGL_CONTEXT_CLIENT_VERSION
#endif

namespace deqp
{
namespace egl
{

using eglu::ConfigInfo;
using tcu::TestLog;

struct ContextCaseInfo
{
	EGLint	surfaceType;
	EGLint	clientType;
	EGLint	clientVersion;
};

class ContextCase : public SimpleConfigCase, protected eglu::CallLogWrapper
{
public:
						ContextCase			(EglTestContext& eglTestCtx, const char* name, const char* description, const std::vector<EGLint>& configIds, EGLint surfaceTypeMask);
	virtual				~ContextCase		(void);

	void				executeForConfig	(tcu::egl::Display& display, EGLConfig config);
	void				executeForSurface	(tcu::egl::Display& display, EGLConfig config, EGLSurface surface, ContextCaseInfo& info);

	virtual void		executeForContext	(tcu::egl::Display& display, EGLConfig config, EGLSurface surface, EGLContext context, ContextCaseInfo& info) = 0;

private:
	EGLint				m_surfaceTypeMask;
};

ContextCase::ContextCase (EglTestContext& eglTestCtx, const char* name, const char* description, const std::vector<EGLint>& configIds, EGLint surfaceTypeMask)
	: SimpleConfigCase	(eglTestCtx, name, description, configIds)
	, CallLogWrapper	(eglTestCtx.getTestContext().getLog())
	, m_surfaceTypeMask	(surfaceTypeMask)
{
}

ContextCase::~ContextCase (void)
{
}

void ContextCase::executeForConfig (tcu::egl::Display& display, EGLConfig config)
{
	tcu::TestLog&			log				= m_testCtx.getLog();
	const int				width			= 64;
	const int				height			= 64;
	const EGLint			configId		= display.getConfigAttrib(config, EGL_CONFIG_ID);
	eglu::NativeDisplay&	nativeDisplay	= m_eglTestCtx.getNativeDisplay();
	bool					isOk			= true;
	std::string				failReason		= "";

	if (m_surfaceTypeMask & EGL_WINDOW_BIT)
	{
		log << TestLog::Message << "Creating window surface with config ID " << configId << TestLog::EndMessage;

		try
		{
			de::UniquePtr<eglu::NativeWindow>	window		(m_eglTestCtx.createNativeWindow(display.getEGLDisplay(), config, DE_NULL, width, height, eglu::parseWindowVisibility(m_testCtx.getCommandLine())));
			tcu::egl::WindowSurface				surface		(display, eglu::createWindowSurface(nativeDisplay, *window, display.getEGLDisplay(), config, DE_NULL));

			ContextCaseInfo info;
			info.surfaceType	= EGL_WINDOW_BIT;

			executeForSurface(m_eglTestCtx.getDisplay(), config, surface.getEGLSurface(), info);
		}
		catch (const tcu::TestError& e)
		{
			log << e;
			isOk = false;
			failReason = e.what();
		}

		log << TestLog::Message << TestLog::EndMessage;
	}

	if (m_surfaceTypeMask & EGL_PIXMAP_BIT)
	{
		log << TestLog::Message << "Creating pixmap surface with config ID " << configId << TestLog::EndMessage;

		try
		{
			de::UniquePtr<eglu::NativePixmap>	pixmap		(m_eglTestCtx.createNativePixmap(display.getEGLDisplay(), config, DE_NULL, width, height));
			tcu::egl::PixmapSurface				surface		(display, eglu::createPixmapSurface(nativeDisplay, *pixmap, display.getEGLDisplay(), config, DE_NULL));

			ContextCaseInfo info;
			info.surfaceType	= EGL_PIXMAP_BIT;

			executeForSurface(display, config, surface.getEGLSurface(), info);
		}
		catch (const tcu::TestError& e)
		{
			log << e;
			isOk = false;
			failReason = e.what();
		}

		log << TestLog::Message << TestLog::EndMessage;
	}

	if (m_surfaceTypeMask & EGL_PBUFFER_BIT)
	{
		log << TestLog::Message << "Creating pbuffer surface with config ID " << configId << TestLog::EndMessage;

		try
		{
			const EGLint surfaceAttribs[] =
			{
				EGL_WIDTH,	width,
				EGL_HEIGHT,	height,
				EGL_NONE
			};

			tcu::egl::PbufferSurface surface(display, config, surfaceAttribs);

			ContextCaseInfo info;
			info.surfaceType	= EGL_PBUFFER_BIT;

			executeForSurface(display, config, surface.getEGLSurface(), info);
		}
		catch (const tcu::TestError& e)
		{
			log << e;
			isOk = false;
			failReason = e.what();
		}

		log << TestLog::Message << TestLog::EndMessage;
	}

	if (!isOk && m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, failReason.c_str());
}

void ContextCase::executeForSurface (tcu::egl::Display& display, EGLConfig config, EGLSurface surface, ContextCaseInfo& info)
{
	TestLog&	log		= m_testCtx.getLog();
	EGLint		apiBits	= display.getConfigAttrib(config, EGL_RENDERABLE_TYPE);

	static const EGLint es1Attrs[] = { EGL_CONTEXT_CLIENT_VERSION,		1, EGL_NONE };
	static const EGLint es2Attrs[] = { EGL_CONTEXT_CLIENT_VERSION,		2, EGL_NONE };
	static const EGLint es3Attrs[] = { EGL_CONTEXT_MAJOR_VERSION_KHR,	3, EGL_NONE };

	static const struct
	{
		const char*		name;
		EGLenum			api;
		EGLint			apiBit;
		const EGLint*	ctxAttrs;
		EGLint			apiVersion;
	} apis[] =
	{
		{ "OpenGL",			EGL_OPENGL_API,		EGL_OPENGL_BIT,			DE_NULL,	0	},
		{ "OpenGL ES 1",	EGL_OPENGL_ES_API,	EGL_OPENGL_ES_BIT,		es1Attrs,	1	},
		{ "OpenGL ES 2",	EGL_OPENGL_ES_API,	EGL_OPENGL_ES2_BIT,		es2Attrs,	2	},
		{ "OpenGL ES 3",	EGL_OPENGL_ES_API,	EGL_OPENGL_ES3_BIT_KHR,	es3Attrs,	3	},
		{ "OpenVG",			EGL_OPENVG_API,		EGL_OPENVG_BIT,			DE_NULL,	0	}
	};

	for (int apiNdx = 0; apiNdx < (int)DE_LENGTH_OF_ARRAY(apis); apiNdx++)
	{
		if ((apiBits & apis[apiNdx].apiBit) == 0)
			continue; // Not supported API

		TCU_CHECK_EGL_CALL(eglBindAPI(apis[apiNdx].api));

		log << TestLog::Message << "Creating " << apis[apiNdx].name << " context" << TestLog::EndMessage;

		const EGLContext	context = eglCreateContext(display.getEGLDisplay(), config, EGL_NO_CONTEXT, apis[apiNdx].ctxAttrs);
		TCU_CHECK_EGL();
		TCU_CHECK(context != EGL_NO_CONTEXT);

		TCU_CHECK_EGL_CALL(eglMakeCurrent(display.getEGLDisplay(), surface, surface, context));

		info.clientType		= apis[apiNdx].api;
		info.clientVersion	= apis[apiNdx].apiVersion;

		executeForContext(display, config, surface, context, info);

		// Destroy
		TCU_CHECK_EGL_CALL(eglMakeCurrent(display.getEGLDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
		TCU_CHECK_EGL_CALL(eglDestroyContext(display.getEGLDisplay(), context));
	}
}

class GetCurrentContextCase : public ContextCase
{
public:
	GetCurrentContextCase (EglTestContext& eglTestCtx, const char* name, const char* description, const std::vector<EGLint>& configIds, EGLint surfaceTypeMask)
		: ContextCase(eglTestCtx, name, description, configIds, surfaceTypeMask)
	{
	}

	void executeForContext (tcu::egl::Display& display, EGLConfig config, EGLSurface surface, EGLContext context, ContextCaseInfo& info)
	{
		TestLog&	log	= m_testCtx.getLog();

		DE_UNREF(display);
		DE_UNREF(config && surface);
		DE_UNREF(info);

		enableLogging(true);

		const EGLContext	gotContext	= eglGetCurrentContext();
		TCU_CHECK_EGL();

		if (gotContext == context)
		{
			log << TestLog::Message << "  Pass" << TestLog::EndMessage;
		}
		else if (gotContext == EGL_NO_CONTEXT)
		{
			log << TestLog::Message << "  Fail, got EGL_NO_CONTEXT" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Unexpected EGL_NO_CONTEXT");
		}
		else if (gotContext != context)
		{
			log << TestLog::Message << "  Fail, call returned the wrong context. Expected: " << tcu::toHex(context) << ", got: " << tcu::toHex(gotContext) << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid context");
		}

		enableLogging(false);
	}
};

class GetCurrentSurfaceCase : public ContextCase
{
public:
	GetCurrentSurfaceCase (EglTestContext& eglTestCtx, const char* name, const char* description, const std::vector<EGLint>& configIds, EGLint surfaceTypeMask)
		: ContextCase(eglTestCtx, name, description, configIds, surfaceTypeMask)
	{
	}

	void executeForContext (tcu::egl::Display& display, EGLConfig config, EGLSurface surface, EGLContext context, ContextCaseInfo& info)
	{
		TestLog&	log	= m_testCtx.getLog();

		DE_UNREF(display);
		DE_UNREF(config && context);
		DE_UNREF(info);

		enableLogging(true);

		const EGLContext	gotReadSurface	= eglGetCurrentSurface(EGL_READ);
		TCU_CHECK_EGL();

		const EGLContext	gotDrawSurface	= eglGetCurrentSurface(EGL_DRAW);
		TCU_CHECK_EGL();

		if (gotReadSurface == surface && gotDrawSurface == surface)
		{
			log << TestLog::Message << "  Pass" << TestLog::EndMessage;
		}
		else
		{
			log << TestLog::Message << "  Fail, read surface: " << tcu::toHex(gotReadSurface)
									<< ", draw surface: " << tcu::toHex(gotDrawSurface)
									<< ", expected: " << tcu::toHex(surface) << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid surface");
		}

		enableLogging(false);
	}
};

class GetCurrentDisplayCase : public ContextCase
{
public:
	GetCurrentDisplayCase (EglTestContext& eglTestCtx, const char* name, const char* description, const std::vector<EGLint>& configIds, EGLint surfaceTypeMask)
		: ContextCase(eglTestCtx, name, description, configIds, surfaceTypeMask)
	{
	}

	void executeForContext (tcu::egl::Display& display, EGLConfig config, EGLSurface surface, EGLContext context, ContextCaseInfo& info)
	{
		TestLog&	log	= m_testCtx.getLog();

		DE_UNREF(config && surface && context);
		DE_UNREF(info);

		enableLogging(true);

		const EGLDisplay	gotDisplay	= eglGetCurrentDisplay();
		TCU_CHECK_EGL();

		if (gotDisplay == display.getEGLDisplay())
		{
			log << TestLog::Message << "  Pass" << TestLog::EndMessage;
		}
		else if (gotDisplay == EGL_NO_DISPLAY)
		{
			log << TestLog::Message << "  Fail, got EGL_NO_DISPLAY" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Unexpected EGL_NO_DISPLAY");
		}
		else if (gotDisplay != display.getEGLDisplay())
		{
			log << TestLog::Message << "  Fail, call returned the wrong display. Expected: " << tcu::toHex(display.getEGLDisplay()) << ", got: " << tcu::toHex(gotDisplay) << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid display");
		}

		enableLogging(false);
	}
};

class QueryContextCase : public ContextCase
{
public:
	QueryContextCase (EglTestContext& eglTestCtx, const char* name, const char* description, const std::vector<EGLint>& configIds, EGLint surfaceTypeMask)
		: ContextCase(eglTestCtx, name, description, configIds, surfaceTypeMask)
	{
	}

	EGLint getContextAttrib (tcu::egl::Display& display, EGLContext context, EGLint attrib)
	{
		EGLint	value;
		TCU_CHECK_EGL_CALL(eglQueryContext(display.getEGLDisplay(), context, attrib, &value));

		return value;
	}

	void executeForContext (tcu::egl::Display& display, EGLConfig config, EGLSurface surface, EGLContext context, ContextCaseInfo& info)
	{
		TestLog&			log		= m_testCtx.getLog();
		const eglu::Version	version	(display.getEGLMajorVersion(), display.getEGLMinorVersion());

		DE_UNREF(surface);
		enableLogging(true);

		// Config ID
		{
			const EGLint	configID		= getContextAttrib(display, context, EGL_CONFIG_ID);
			const EGLint	surfaceConfigID	= display.getConfigAttrib(config, EGL_CONFIG_ID);

			if (configID != surfaceConfigID)
			{
				log << TestLog::Message << "  Fail, config ID doesn't match the one used to create the context." << TestLog::EndMessage;
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid config ID");
			}
		}

		// Client API type
		if (version >= eglu::Version(1, 2))
		{
			const EGLint	clientType		= getContextAttrib(display, context, EGL_CONTEXT_CLIENT_TYPE);

			if (clientType != info.clientType)
			{
				log << TestLog::Message << "  Fail, client API type doesn't match." << TestLog::EndMessage;
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid client API type");
			}
		}

		// Client API version
		if (version >= eglu::Version(1, 3))
		{
			const EGLint	clientVersion	= getContextAttrib(display, context, EGL_CONTEXT_CLIENT_VERSION);

			// \todo [2014-10-21 mika] Query actual supported api version from client api to make this check stricter.
			if (info.clientType == EGL_OPENGL_ES_API && ((info.clientVersion == 1 && clientVersion != 1) || clientVersion < info.clientVersion))
			{
				log << TestLog::Message << "  Fail, client API version doesn't match." << TestLog::EndMessage;
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid client API version");
			}
		}

		// Render buffer
		if (version >= eglu::Version(1, 2))
		{
			const EGLint	renderBuffer	= getContextAttrib(display, context, EGL_RENDER_BUFFER);

			if (info.surfaceType == EGL_PIXMAP_BIT && renderBuffer != EGL_SINGLE_BUFFER)
			{
				log << TestLog::Message << "  Fail, render buffer should be EGL_SINGLE_BUFFER for a pixmap surface." << TestLog::EndMessage;
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid render buffer");
			}
			else if (info.surfaceType == EGL_PBUFFER_BIT && renderBuffer != EGL_BACK_BUFFER)
			{
				log << TestLog::Message << "  Fail, render buffer should be EGL_BACK_BUFFER for a pbuffer surface." << TestLog::EndMessage;
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid render buffer");
			}
			else if (info.surfaceType == EGL_WINDOW_BIT && renderBuffer != EGL_SINGLE_BUFFER && renderBuffer != EGL_BACK_BUFFER)
			{
				log << TestLog::Message << "  Fail, render buffer should be either EGL_SINGLE_BUFFER or EGL_BACK_BUFFER for a window surface." << TestLog::EndMessage;
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid render buffer");
			}
		}

		enableLogging(false);

		log << TestLog::Message << "  Pass" << TestLog::EndMessage;
	}
};

class QueryAPICase : public TestCase, protected eglu::CallLogWrapper
{
public:
	QueryAPICase (EglTestContext& eglTestCtx, const char* name, const char* description)
		: TestCase(eglTestCtx, name, description)
		, CallLogWrapper(eglTestCtx.getTestContext().getLog())
	{
	}

	void init (void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	}

	IterateResult iterate (void)
	{
		tcu::TestLog&	log		= m_testCtx.getLog();
		const EGLenum	apis[]	= { EGL_OPENGL_API, EGL_OPENGL_ES_API, EGL_OPENVG_API };

		enableLogging(true);

		{
			const EGLenum	api	= eglQueryAPI();

			if (api != EGL_OPENGL_ES_API && m_eglTestCtx.isAPISupported(EGL_OPENGL_ES_API))
			{
				log << TestLog::Message << "  Fail, initial value should be EGL_OPENGL_ES_API if OpenGL ES is supported." << TestLog::EndMessage;
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid default value");
			}
			else if(api != EGL_NONE && !m_eglTestCtx.isAPISupported(EGL_OPENGL_ES_API))
			{
				log << TestLog::Message << "  Fail, initial value should be EGL_NONE if OpenGL ES is not supported." << TestLog::EndMessage;
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid default value");
			}
		}

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(apis); ndx++)
		{
			const EGLenum	api	= apis[ndx];

			log << TestLog::Message << TestLog::EndMessage;

			if (m_eglTestCtx.isAPISupported(api))
			{
				eglBindAPI(api);

				if (api != eglQueryAPI())
				{
					log << TestLog::Message << "  Fail, return value does not match previously bound API." << TestLog::EndMessage;
					m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid return value");
				}
			}
			else
			{
				log << TestLog::Message << eglu::getAPIStr(api) << " not supported." << TestLog::EndMessage;
			}
		}

		enableLogging(false);
		return STOP;
	}
};

QueryContextTests::QueryContextTests (EglTestContext& eglTestCtx)
	: TestCaseGroup(eglTestCtx, "query_context", "Rendering context query tests")
{
}

QueryContextTests::~QueryContextTests (void)
{
}

template<class QueryContextClass>
void createQueryContextGroups (EglTestContext& eglTestCtx, tcu::TestCaseGroup* group)
{
	std::vector<RenderConfigIdSet>	configSets;
	eglu::FilterList				filters;

	getDefaultRenderConfigIdSets(configSets, eglTestCtx.getConfigs(), filters);

	for (std::vector<RenderConfigIdSet>::const_iterator setIter = configSets.begin(); setIter != configSets.end(); setIter++)
		group->addChild(new QueryContextClass(eglTestCtx, setIter->getName(), "", setIter->getConfigIds(), setIter->getSurfaceTypeMask()));
}

void QueryContextTests::init (void)
{
	{
		tcu::TestCaseGroup* simpleGroup = new tcu::TestCaseGroup(m_testCtx, "simple", "Simple API tests");
		addChild(simpleGroup);

		simpleGroup->addChild(new QueryAPICase(m_eglTestCtx, "query_api", "eglQueryAPI() test"));
	}

	// eglGetCurrentContext
	{
		tcu::TestCaseGroup* getCurrentContextGroup = new tcu::TestCaseGroup(m_testCtx, "get_current_context", "eglGetCurrentContext() tests");
		addChild(getCurrentContextGroup);

		createQueryContextGroups<GetCurrentContextCase>(m_eglTestCtx, getCurrentContextGroup);
	}

	// eglGetCurrentSurface
	{
		tcu::TestCaseGroup* getCurrentSurfaceGroup = new tcu::TestCaseGroup(m_testCtx, "get_current_surface", "eglGetCurrentSurface() tests");
		addChild(getCurrentSurfaceGroup);

		createQueryContextGroups<GetCurrentSurfaceCase>(m_eglTestCtx, getCurrentSurfaceGroup);
	}

	// eglGetCurrentDisplay
	{
		tcu::TestCaseGroup* getCurrentDisplayGroup = new tcu::TestCaseGroup(m_testCtx, "get_current_display", "eglGetCurrentDisplay() tests");
		addChild(getCurrentDisplayGroup);

		createQueryContextGroups<GetCurrentDisplayCase>(m_eglTestCtx, getCurrentDisplayGroup);
	}

	// eglQueryContext
	{
		tcu::TestCaseGroup* queryContextGroup = new tcu::TestCaseGroup(m_testCtx, "query_context", "eglQueryContext() tests");
		addChild(queryContextGroup);

		createQueryContextGroups<QueryContextCase>(m_eglTestCtx, queryContextGroup);
	}
}

} // egl
} // deqp
