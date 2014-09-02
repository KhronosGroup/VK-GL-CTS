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
 * \brief Extension function pointer query tests.
 *//*--------------------------------------------------------------------*/

#include "teglGetProcAddressTests.hpp"
#include "teglTestCase.hpp"
#include "egluCallLogWrapper.hpp"
#include "egluStrUtil.hpp"
#include "tcuTestLog.hpp"

#include <vector>
#include <string>
#include <algorithm>
#include <cctype>

#if !defined(EGL_OPENGL_ES3_BIT_KHR)
#	define EGL_OPENGL_ES3_BIT_KHR	0x0040
#endif

using tcu::TestLog;

namespace deqp
{
namespace egl
{

namespace
{

// Function name strings generated from API headers

#include "teglGetProcAddressTests.inl"

std::vector<std::string> makeStringVector (const char** cStrArray)
{
	std::vector<std::string>	out;

	for (int ndx = 0; cStrArray[ndx] != DE_NULL; ndx++)
	{
		out.push_back(std::string(cStrArray[ndx]));
	}

	return out;
}

std::vector<std::string> getExtensionNames (void)
{
	return makeStringVector(getExtensionStrs());
}

std::vector<std::string> getExtFunctionNames (const std::string& extName)
{
	const char** names_raw = getExtensionFuncStrs(extName);

	return (names_raw != 0) ? makeStringVector(names_raw) : std::vector<std::string>();
}

std::vector<std::string> getCoreFunctionNames (EGLint apiBit)
{
	switch (apiBit)
	{
		case 0:							return makeStringVector(getCoreFunctionStrs());
		case EGL_OPENGL_ES_BIT:			return makeStringVector(getGlesFunctionStrs());
		case EGL_OPENGL_ES2_BIT:		return makeStringVector(getGles2FunctionStrs());
		case EGL_OPENGL_ES3_BIT_KHR:	return makeStringVector(getGles3FunctionStrs());
		default:
			DE_ASSERT(DE_FALSE);
	}

	return std::vector<std::string>();
}

} // anonymous

// Base class for eglGetProcAddress() test cases

class GetProcAddressCase : public TestCase, protected eglu::CallLogWrapper
{
public:
								GetProcAddressCase		(EglTestContext& eglTestCtx, const char* name, const char* description);
	virtual						~GetProcAddressCase		(void);

	void						init					(void);
	IterateResult				iterate					(void);

	bool						isSupported				(const std::string& extName);

	virtual void				executeTest				(void) = 0;

private:
	std::vector<std::string>	m_supported;
};

GetProcAddressCase::GetProcAddressCase (EglTestContext& eglTestCtx, const char* name, const char* description)
	: TestCase			(eglTestCtx, name, description)
	, CallLogWrapper	(eglTestCtx.getTestContext().getLog())
{
}

GetProcAddressCase::~GetProcAddressCase (void)
{
}

void GetProcAddressCase::init (void)
{
	const tcu::egl::Display&	display		= m_eglTestCtx.getDisplay();
	display.getExtensions(m_supported);

	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
}

tcu::TestNode::IterateResult GetProcAddressCase::iterate (void)
{
	enableLogging(true);

	executeTest();

	enableLogging(false);

	return STOP;
}

bool GetProcAddressCase::isSupported (const std::string& extName)
{
	return std::find(m_supported.begin(), m_supported.end(), extName) != m_supported.end();
}

// Test by extension

class GetProcAddressExtensionCase : public GetProcAddressCase
{
public:
	GetProcAddressExtensionCase (EglTestContext& eglTestCtx, const char* name, const char* description, const std::string& extName)
		: GetProcAddressCase	(eglTestCtx, name, description)
		, m_extName				(extName)
	{
	}

	virtual ~GetProcAddressExtensionCase (void)
	{
	}

	void executeTest (void)
	{
		TestLog&						log			= m_testCtx.getLog();
		bool							supported	= isSupported(m_extName);
		const std::vector<std::string>	funcNames	= getExtFunctionNames(m_extName);

		DE_ASSERT(!funcNames.empty());

		log << TestLog::Message << m_extName << ": " << (supported ? "supported" : "not supported") << TestLog::EndMessage;
		log << TestLog::Message << TestLog::EndMessage;

		for (std::vector<std::string>::const_iterator funcIter = funcNames.begin(); funcIter != funcNames.end(); funcIter++)
		{
			const std::string&	funcName			= *funcIter;
			void				(*funcPtr)(void);

			funcPtr = eglGetProcAddress(funcName.c_str());
			TCU_CHECK_EGL();

			if (supported && funcPtr == 0)
			{
				log << TestLog::Message << "Fail, received null pointer for supported extension function: " << funcName << TestLog::EndMessage;
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Unexpected null pointer");
			}
		}
	}

private:
	std::string	m_extName;
};

// Test core functions

class GetProcAddressCoreFunctionsCase : public GetProcAddressCase
{
public:
	GetProcAddressCoreFunctionsCase (EglTestContext& eglTestCtx, const char* name, const char* description, const EGLint apiBit = 0)
		: GetProcAddressCase	(eglTestCtx, name, description)
		, m_funcNames			(getCoreFunctionNames(apiBit))
		, m_apiBit				(apiBit)
	{
	}

	virtual ~GetProcAddressCoreFunctionsCase (void)
	{
	}

	bool isApiSupported (void)
	{
		const std::vector<eglu::ConfigInfo>	configs	= m_eglTestCtx.getConfigs();

		if (m_apiBit == 0)
			return true;

		for (std::vector<eglu::ConfigInfo>::const_iterator configIter = configs.begin(); configIter != configs.end(); configIter++)
		{
			const eglu::ConfigInfo&	config	= *configIter;

			if ((config.renderableType & m_apiBit) != 0)
				return true;
		}

		return false;
	}

	void executeTest (void)
	{
		TestLog&	log					= m_testCtx.getLog();
		const bool	funcPtrSupported	= isSupported("EGL_KHR_get_all_proc_addresses");
		const bool	apiSupported		= isApiSupported();

		log << TestLog::Message << "EGL_KHR_get_all_proc_addresses: " << (funcPtrSupported ? "supported" : "not supported") << TestLog::EndMessage;
		log << TestLog::Message << TestLog::EndMessage;

		if (!apiSupported)
		{
			log << TestLog::Message << eglu::getConfigAttribValueStr(EGL_RENDERABLE_TYPE, m_apiBit) << " not supported by any available configuration." << TestLog::EndMessage;
			log << TestLog::Message << TestLog::EndMessage;
		}

		for (std::vector<std::string>::const_iterator funcIter = m_funcNames.begin(); funcIter != m_funcNames.end(); funcIter++)
		{
			const std::string&	funcName			= *funcIter;
			void				(*funcPtr)(void);

			funcPtr = eglGetProcAddress(funcName.c_str());
			TCU_CHECK_EGL();

			if (apiSupported && funcPtrSupported && (funcPtr == 0))
			{
				log << TestLog::Message << "Fail, received null pointer for supported function: " << funcName << TestLog::EndMessage;
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Unexpected null pointer");
			}
			else if (!apiSupported && (funcPtr != 0))
			{
				log << TestLog::Message << "Warning, received non-null value for unsupported function: " << funcName << TestLog::EndMessage;
				m_testCtx.setTestResult(QP_TEST_RESULT_QUALITY_WARNING, "Non-null value for unsupported function");
			}
		}
	}

private:
	const std::vector<std::string>	m_funcNames;
	const EGLint					m_apiBit;
};

GetProcAddressTests::GetProcAddressTests (EglTestContext& eglTestCtx)
	: TestCaseGroup(eglTestCtx, "get_proc_address", "eglGetProcAddress() tests")
{
}

GetProcAddressTests::~GetProcAddressTests (void)
{
}

void GetProcAddressTests::init (void)
{
	// extensions
	{
		tcu::TestCaseGroup* extensionsGroup = new tcu::TestCaseGroup(m_testCtx, "extension", "Test EGL extensions");
		addChild(extensionsGroup);

		const std::vector<std::string>	extNames	= getExtensionNames();

		for (std::vector<std::string>::const_iterator extIter = extNames.begin(); extIter != extNames.end(); extIter++)
		{
			const std::string&				extName		= *extIter;
			const std::vector<std::string>	funcNames	= getExtFunctionNames(extName);

			std::string						testName	(extName);

			if (funcNames.empty())
				continue;

			for (size_t ndx = 0; ndx < extName.length(); ndx++)
				testName[ndx] = std::tolower(extName[ndx]);

			extensionsGroup->addChild(new GetProcAddressExtensionCase(m_eglTestCtx, testName.c_str(), ("Test " + extName).c_str(), extName));
		}
	}

	// core functions
	{
		tcu::TestCaseGroup* coreFuncGroup = new tcu::TestCaseGroup(m_testCtx, "core", "Test core functions");
		addChild(coreFuncGroup);

		coreFuncGroup->addChild(new GetProcAddressCoreFunctionsCase	(m_eglTestCtx,	"egl",		"Test EGL core functions"));
		coreFuncGroup->addChild(new GetProcAddressCoreFunctionsCase	(m_eglTestCtx,	"gles",		"Test OpenGL ES core functions",	EGL_OPENGL_ES_BIT));
		coreFuncGroup->addChild(new GetProcAddressCoreFunctionsCase	(m_eglTestCtx,	"gles2",	"Test OpenGL ES 2 core functions",	EGL_OPENGL_ES2_BIT));
		coreFuncGroup->addChild(new GetProcAddressCoreFunctionsCase	(m_eglTestCtx,	"gles3",	"Test OpenGL ES 3 core functions",	EGL_OPENGL_ES3_BIT_KHR));
	}
}

} // egl
} // deqp
