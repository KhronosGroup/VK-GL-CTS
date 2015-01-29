#ifndef _TCUTESTCONTEXT_HPP
#define _TCUTESTCONTEXT_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
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
 * \brief Context shared between test cases.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "qpWatchDog.h"
#include "qpTestLog.h"

#include <string>

namespace tcu
{

class Archive;
class Platform;
class CommandLine;
class TestLog;

/*--------------------------------------------------------------------*//*!
 * \brief Test context
 *
 * Test context holds common resources that are available to test cases.
 * This includes test log and resource archive.
 *
 * Test case can write to test log and must set test result to test context.
 *//*--------------------------------------------------------------------*/
class TestContext
{
public:
							TestContext			(Platform& platform, Archive& rootArchive, TestLog& log, const CommandLine& cmdLine, qpWatchDog* watchDog);
							~TestContext		(void) {}

	// API for test cases
	TestLog&				getLog				(void)			{ return m_log;			}
	Archive&				getArchive			(void)			{ return *m_curArchive;	} //!< \note Do not access in TestNode constructors.
	Platform&				getPlatform			(void)			{ return m_platform;	}
	void					setTestResult		(qpTestResult result, const char* description);
	void					touchWatchdog		(void);
	const CommandLine&		getCommandLine		(void) const	{ return m_cmdLine;		}

	// API for test framework
	qpTestResult			getTestResult		(void) const	{ return m_testResult;				}
	const char*				getTestResultDesc	(void) const	{ return m_testResultDesc.c_str();	}
	qpWatchDog*				getWatchDog			(void)			{ return m_watchDog;				}

	Archive&				getRootArchive		(void) const		{ return m_rootArchive;		}
	void					setCurrentArchive	(Archive& archive)	{ m_curArchive = &archive;	}

	void					setTerminateAfter	(bool terminate)	{ m_terminateAfter = terminate;	}
	bool					getTerminateAfter	(void) const		{ return m_terminateAfter; 		}
protected:
	Platform&				m_platform;			//!< Platform port implementation.
	Archive&				m_rootArchive;		//!< Root archive.
	TestLog&				m_log;				//!< Test log.
	const CommandLine&		m_cmdLine;			//!< Command line.
	qpWatchDog*				m_watchDog;			//!< Watchdog (can be null).

	Archive*				m_curArchive;		//!< Current archive for test cases.
	qpTestResult			m_testResult;		//!< Latest test result.
	std::string				m_testResultDesc;	//!< Latest test result description.
	bool					m_terminateAfter;	//!< Should tester terminate after execution of the current test
};

/*--------------------------------------------------------------------*//*!
 * \brief Test result collector
 *
 * This utility class collects test results with associated messages,
 * optionally logs them, and finally sets the test result of a TestContext to
 * the most severe collected result. This allows multiple problems to be
 * easily reported from a single test run.
 *//*--------------------------------------------------------------------*/
class ResultCollector
{
public:
					ResultCollector			(void);
					ResultCollector			(TestLog& log, const std::string& prefix = "");

	qpTestResult	getResult				(void) const  { return m_result; }

	void			fail					(const std::string& msg);
	bool			check					(bool condition, const std::string& msg);

	void			addResult				(qpTestResult result, const std::string& msg);
	bool			checkResult				(bool condition, qpTestResult result, const std::string& msg);

	void			setTestContextResult	(TestContext& testCtx);

private:
	TestLog*		m_log;
	std::string		m_prefix;
	qpTestResult	m_result;
	std::string		m_message;
};

} // tcu

#endif // _TCUTESTCONTEXT_HPP
