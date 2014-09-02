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
 * \brief EGL Implementation Information Tests
 *//*--------------------------------------------------------------------*/

#include "teglInfoTests.hpp"
#include "teglConfigList.hpp"
#include "tcuTestLog.hpp"

#include <vector>
#include <string>
#include <sstream>

#include <EGL/egl.h>

using std::vector;
using std::string;

using tcu::TestLog;

namespace deqp
{
namespace egl
{

static std::vector<std::string> split(const std::string& str, const std::string& delim = " ")
{
	std::vector<std::string>	out;
	if (str.length() == 0) return out;

	size_t	start	= 0;
	size_t	end		= string::npos;

	while ((end = str.find(delim, start)) != string::npos)
	{
		out.push_back(str.substr(start, end-start));
		start = end + delim.length();
	}

	if (start < end)
		out.push_back(str.substr(start, end-start));

	return out;
}

static int toInt(std::string str)
{
	std::istringstream strStream(str);

	int out;
	strStream >> out;
	return out;
}

class QueryStringCase : public TestCase
{
public:
	QueryStringCase (EglTestContext& eglTestCtx, const char* name, const char* description, EGLint query)
		: TestCase	(eglTestCtx, name, description)
		, m_query	(query)
	{
	}

	void validateString (const std::string& result)
	{
		tcu::TestLog&				log		= m_testCtx.getLog();
		std::vector<std::string>	tokens	= split(result);

		if (m_query == EGL_VERSION)
		{
			const tcu::egl::Display&		display			= m_eglTestCtx.getDisplay();
			const int						dispMajor		= display.getEGLMajorVersion();
			const int						dispMinor		= display.getEGLMinorVersion();

			const std::vector<std::string>	versionTokens	= split(tokens[0], ".");

			if (versionTokens.size() < 2)
			{
				log << TestLog::Message << "  Fail, first part of the string must be in the format <major_version.minor_version>" << TestLog::EndMessage;
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid version string");
			}
			else
			{
				const	int	stringMajor	= toInt(versionTokens[0]);
				const	int	stringMinor	= toInt(versionTokens[1]);

				if (stringMajor != dispMajor || stringMinor != dispMinor)
				{
					log << TestLog::Message << "  Fail, version numer (" << stringMajor << "." << stringMinor
						<< ") does not match the one reported by eglInitialize (" << dispMajor << "." << dispMinor << ")" << TestLog::EndMessage;
					m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Version number mismatch");
				}
			}
		}
	}

	IterateResult iterate (void)
	{
		const char* result = eglQueryString(m_eglTestCtx.getDisplay().getEGLDisplay(), m_query);
		TCU_CHECK_EGL_MSG("eglQueryString() failed");

		m_testCtx.getLog() << tcu::TestLog::Message << result << tcu::TestLog::EndMessage;
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		validateString(result);

		return STOP;
	}

private:
	EGLint m_query;
};

class QueryExtensionsCase : public TestCase
{
public:
	QueryExtensionsCase (EglTestContext& eglTestCtx)
		: TestCase	(eglTestCtx, "extensions", "Supported Extensions")
	{
	}

	IterateResult iterate (void)
	{
		vector<string> extensions;
		m_eglTestCtx.getDisplay().getExtensions(extensions);

		for (vector<string>::const_iterator i = extensions.begin(); i != extensions.end(); i++)
			m_testCtx.getLog() << tcu::TestLog::Message << *i << tcu::TestLog::EndMessage;

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		return STOP;
	}
};

InfoTests::InfoTests (EglTestContext& eglTestCtx)
	: TestCaseGroup(eglTestCtx, "info", "Platform Information")
{
}

InfoTests::~InfoTests (void)
{
}

void InfoTests::init (void)
{
	addChild(new QueryStringCase(m_eglTestCtx, "version",		"EGL Version",				EGL_VERSION));
	addChild(new QueryStringCase(m_eglTestCtx, "vendor",		"EGL Vendor",				EGL_VENDOR));
	addChild(new QueryStringCase(m_eglTestCtx, "client_apis",	"Supported client APIs",	EGL_CLIENT_APIS));
	addChild(new QueryExtensionsCase(m_eglTestCtx));
	addChild(new ConfigList(m_eglTestCtx));
}

} // egl
} // deqp
