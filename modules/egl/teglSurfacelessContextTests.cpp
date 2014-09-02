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
 * \brief EGL_KHR_surfaceless_context extension tests
 *//*--------------------------------------------------------------------*/

#include "teglSurfacelessContextTests.hpp"
#include "teglSimpleConfigCase.hpp"
#include "egluStrUtil.hpp"
#include "tcuTestLog.hpp"

#include <EGL/eglext.h>

#include <string>
#include <vector>
#include <algorithm>

#if !defined(EGL_OPENGL_ES3_BIT_KHR)
#	define EGL_OPENGL_ES3_BIT_KHR	0x0040
#endif
#if !defined(EGL_CONTEXT_MAJOR_VERSION_KHR)
#	define EGL_CONTEXT_MAJOR_VERSION_KHR EGL_CONTEXT_CLIENT_VERSION
#endif

using std::vector;
using std::string;
using tcu::TestLog;

namespace deqp
{
namespace egl
{
namespace
{

class SurfacelessContextCase : public SimpleConfigCase
{
public:
						SurfacelessContextCase			(EglTestContext& eglTestCtx, const char* name, const char* description, const vector<EGLint>& configIds);
						~SurfacelessContextCase			(void);

	void				executeForConfig				(tcu::egl::Display& display, EGLConfig config);
};

SurfacelessContextCase::SurfacelessContextCase (EglTestContext& eglTestCtx, const char* name, const char* description, const vector<EGLint>& configIds)
	: SimpleConfigCase(eglTestCtx, name, description, configIds)
{
}

SurfacelessContextCase::~SurfacelessContextCase (void)
{
}

void SurfacelessContextCase::executeForConfig (tcu::egl::Display& display, EGLConfig config)
{
	TestLog&		log		= m_testCtx.getLog();
	const EGLint	id		= display.getConfigAttrib(config, EGL_CONFIG_ID);
	const EGLint	apiBits	= display.getConfigAttrib(config, EGL_RENDERABLE_TYPE);

	static const EGLint es1Attrs[] = { EGL_CONTEXT_CLIENT_VERSION,		1, EGL_NONE };
	static const EGLint es2Attrs[] = { EGL_CONTEXT_CLIENT_VERSION,		2, EGL_NONE };
	static const EGLint es3Attrs[] = { EGL_CONTEXT_MAJOR_VERSION_KHR,	3, EGL_NONE };

	static const struct
	{
		const char*		name;
		EGLenum			api;
		EGLint			apiBit;
		const EGLint*	ctxAttrs;
	} apis[] =
	{
		{ "OpenGL",			EGL_OPENGL_API,		EGL_OPENGL_BIT,			DE_NULL		},
		{ "OpenGL ES 1",	EGL_OPENGL_ES_API,	EGL_OPENGL_ES_BIT,		es1Attrs	},
		{ "OpenGL ES 2",	EGL_OPENGL_ES_API,	EGL_OPENGL_ES2_BIT,		es2Attrs	},
		{ "OpenGL ES 3",	EGL_OPENGL_ES_API,	EGL_OPENGL_ES3_BIT_KHR,	es3Attrs	},
		{ "OpenVG",			EGL_OPENVG_API,		EGL_OPENVG_BIT,			DE_NULL		}
	};

	{
		vector<string> extensions;
		display.getExtensions(extensions);

		if (std::find(extensions.begin(), extensions.end(), string("EGL_KHR_surfaceless_context")) == extensions.end())
			throw tcu::NotSupportedError("EGL_KHR_surfaceless_context not supported", "", __FILE__, __LINE__);
	}

	for (int apiNdx = 0; apiNdx < (int)DE_LENGTH_OF_ARRAY(apis); apiNdx++)
	{
		if ((apiBits & apis[apiNdx].apiBit) == 0)
			continue; // Not supported API

		log << TestLog::Message << "Creating " << apis[apiNdx].name << " context with config ID " << id << TestLog::EndMessage;
		TCU_CHECK_EGL();

		TCU_CHECK_EGL_CALL(eglBindAPI(apis[apiNdx].api));

		EGLContext context = eglCreateContext(display.getEGLDisplay(), config, EGL_NO_CONTEXT, apis[apiNdx].ctxAttrs);
		TCU_CHECK_EGL();

		if (!eglMakeCurrent(display.getEGLDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, context))
		{
			const EGLenum err = eglGetError();

			if (err == EGL_BAD_MATCH)
			{
				log << TestLog::Message << "  eglMakeCurrent() failed with EGL_BAD_MATCH. Context doesn't support surfaceless mode." << TestLog::EndMessage;
				TCU_CHECK_EGL_CALL(eglDestroyContext(display.getEGLDisplay(), context));
				continue;
			}
			else
			{
				log << TestLog::Message << "  Fail, context: " << tcu::toHex(context) << ", error: " << eglu::getErrorName(err) << TestLog::EndMessage;
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Failed to make context current");
				continue;
			}
		}

		TCU_CHECK_EGL_CALL(eglMakeCurrent(display.getEGLDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

		// Destroy
		TCU_CHECK_EGL_CALL(eglDestroyContext(display.getEGLDisplay(), context));
		log << TestLog::Message << "  Pass" << TestLog::EndMessage;
	}
}



} // anonymous

SurfacelessContextTests::SurfacelessContextTests (EglTestContext& eglTestCtx)
	: TestCaseGroup (eglTestCtx, "surfaceless_context", "EGL_KHR_surfaceless_context extension tests")
{
}

void SurfacelessContextTests::init (void)
{
	vector<NamedConfigIdSet>	configIdSets;
	eglu::FilterList			filters;
	NamedConfigIdSet::getDefaultSets(configIdSets, m_eglTestCtx.getConfigs(), filters);

	for (vector<NamedConfigIdSet>::const_iterator i = configIdSets.begin(); i != configIdSets.end(); i++)
		addChild(new SurfacelessContextCase(m_eglTestCtx, i->getName(), i->getDescription(), i->getConfigIds()));
}

} // egl
} // deqp
