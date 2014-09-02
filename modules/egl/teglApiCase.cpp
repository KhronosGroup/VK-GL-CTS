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
 * \brief API test case.
 *//*--------------------------------------------------------------------*/

#include "teglApiCase.hpp"
#include "egluStrUtil.hpp"

using tcu::TestLog;

namespace deqp
{
namespace egl
{

ApiCase::ApiCase (EglTestContext& eglTestCtx, const char* name, const char* description)
	: TestCase		(eglTestCtx, name, description)
	, CallLogWrapper(eglTestCtx.getTestContext().getLog())
{
}

ApiCase::~ApiCase (void)
{
}

ApiCase::IterateResult ApiCase::iterate (void)
{
	// Initialize result to pass.
	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

	// Enable call logging.
	enableLogging(true);

	// Run test.
	test();

	return STOP;
}

void ApiCase::expectError (EGLenum expected)
{
	EGLenum err = eglGetError();
	if (err != expected)
	{
		m_testCtx.getLog() << TestLog::Message << "// ERROR: expected " << eglu::getErrorStr(expected) << TestLog::EndMessage;
		if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid error");
	}
}

void ApiCase::expectBoolean (EGLBoolean expected, EGLBoolean got)
{
	if (expected != got)
	{
		m_testCtx.getLog() << TestLog::Message << "// ERROR: expected " << (expected ? "EGL_TRUE" : "EGL_FALSE") << TestLog::EndMessage;
		if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid value");
	}
}

void ApiCase::expectNoContext (EGLContext got)
{
	if (got != EGL_NO_CONTEXT)
	{
		m_testCtx.getLog() << TestLog::Message << "// ERROR: expected EGL_NO_CONTEXT" << TestLog::EndMessage;
		if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid value");
		eglDestroyContext(getDisplay(), got);
	}
}

void ApiCase::expectNoSurface (EGLSurface got)
{
	if (got != EGL_NO_CONTEXT)
	{
		m_testCtx.getLog() << TestLog::Message << "// ERROR: expected EGL_NO_SURFACE" << TestLog::EndMessage;
		if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid value");
		eglDestroySurface(getDisplay(), got);
	}
}

void ApiCase::expectNoDisplay (EGLDisplay got)
{
	if (got != EGL_NO_CONTEXT)
	{
		m_testCtx.getLog() << TestLog::Message << "// ERROR: expected EGL_NO_DISPLAY" << TestLog::EndMessage;
		if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid value");
	}
}

void ApiCase::expectNull (const void* got)
{
	if (got != EGL_NO_CONTEXT)
	{
		m_testCtx.getLog() << TestLog::Message << "// ERROR: expected NULL" << TestLog::EndMessage;
		if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid value");
	}
}

bool ApiCase::getConfig (EGLConfig* config,const eglu::FilterList& filters)
{
	for (std::vector<eglu::ConfigInfo>::const_iterator cfgIter = m_eglTestCtx.getConfigs().begin(); cfgIter != m_eglTestCtx.getConfigs().end(); ++cfgIter)
	{
		if (filters.match(*cfgIter))
		{
			EGLint		numCfgs;
			EGLBoolean	ok;
			EGLint		attribs[] =
			{
				EGL_CONFIG_ID,			cfgIter->configId,
				EGL_TRANSPARENT_TYPE,	EGL_DONT_CARE,
				EGL_COLOR_BUFFER_TYPE,	EGL_DONT_CARE,
				EGL_RENDERABLE_TYPE,	EGL_DONT_CARE,
				EGL_SURFACE_TYPE,		EGL_DONT_CARE,
				EGL_NONE
			};

			ok = eglChooseConfig(getDisplay(), &attribs[0], config, 1, &numCfgs);
			expectTrue(ok);

			if (ok && numCfgs >= 1)
				return true;
			else
			{
				m_testCtx.getLog() << TestLog::Message << "// ERROR: expected at least one config with id " << cfgIter->configId << TestLog::EndMessage;
				if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
					m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid value");
				return 0;
			}
		}
	}

	return DE_NULL;
}

} // egl
} // deqp
