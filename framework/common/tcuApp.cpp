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
 * \brief Render target info.
 *//*--------------------------------------------------------------------*/

#include "tcuApp.hpp"
#include "tcuPlatform.hpp"
#include "tcuTestContext.hpp"
#include "tcuTestSessionExecutor.hpp"
#include "tcuTestHierarchyUtil.hpp"
#include "tcuCommandLine.hpp"
#include "tcuTestLog.hpp"
#include "qpInfo.h"
#include "qpDebugOut.h"
#include "deMath.h"

namespace tcu
{

/*--------------------------------------------------------------------*//*!
 * \brief Construct test application
 *
 * If a fatal error occurs during initialization constructor will call
 * die() with debug information.
 *
 * \param platform Reference to platform implementation.
 *//*--------------------------------------------------------------------*/
App::App (Platform& platform, Archive& archive, TestLog& log, const CommandLine& cmdLine)
	: m_platform		(platform)
	, m_watchDog		(DE_NULL)
	, m_crashHandler	(DE_NULL)
	, m_crashed			(false)
	, m_testCtx			(DE_NULL)
	, m_testRoot		(DE_NULL)
	, m_testExecutor	(DE_NULL)
{
	print("dEQP Core %s (0x%08x) starting..\n", qpGetReleaseName(), qpGetReleaseId());
	print("  target implementation = '%s'\n", qpGetTargetName());

	if (!deSetRoundingMode(DE_ROUNDINGMODE_TO_NEAREST))
		qpPrintf("WARNING: Failed to set floating-point rounding mode!\n");

	try
	{
		const RunMode	runMode	= cmdLine.getRunMode();

		// Initialize watchdog
		if (cmdLine.isWatchDogEnabled())
			TCU_CHECK_INTERNAL(m_watchDog = qpWatchDog_create(onWatchdogTimeout, this, 300, 30));

		// Initialize crash handler.
		if (cmdLine.isCrashHandlingEnabled())
			TCU_CHECK_INTERNAL(m_crashHandler = qpCrashHandler_create(onCrash, this));

		// Create test context
		m_testCtx = new TestContext(m_platform, archive, log, cmdLine, m_watchDog);

		// Create root from registry
		m_testRoot = new TestPackageRoot(*m_testCtx, TestPackageRegistry::getSingleton());

		// \note No executor is created if runmode is not EXECUTE
		if (runMode == RUNMODE_EXECUTE)
			m_testExecutor = new TestSessionExecutor(*m_testRoot, *m_testCtx);
		else if (runMode == RUNMODE_DUMP_XML_CASELIST)
			writeXmlCaselists(*m_testRoot, *m_testCtx, m_testCtx->getCommandLine());
		else if (runMode == RUNMODE_DUMP_TEXT_CASELIST)
			writeTxtCaselists(*m_testRoot, *m_testCtx, m_testCtx->getCommandLine());
		else
			DE_ASSERT(false);
	}
	catch (const std::exception& e)
	{
		cleanup();
		die("Failed to initialize dEQP: %s", e.what());
	}
}

App::~App (void)
{
	cleanup();
}

void App::cleanup (void)
{
	delete m_testExecutor;
	delete m_testRoot;
	delete m_testCtx;

	if (m_crashHandler)
		qpCrashHandler_destroy(m_crashHandler);

	if (m_watchDog)
		qpWatchDog_destroy(m_watchDog);
}

/*--------------------------------------------------------------------*//*!
 * \brief Step forward test execution
 * \return true if application should call iterate() again and false
 *         if test execution session is complete.
 *//*--------------------------------------------------------------------*/
bool App::iterate (void)
{
	if (!m_testExecutor)
	{
		DE_ASSERT(m_testCtx->getCommandLine().getRunMode() != RUNMODE_EXECUTE);
		return false;
	}

	// Poll platform events
	const bool platformOk = m_platform.processEvents();

	// Iterate a step.
	bool testExecOk = false;
	if (platformOk)
	{
		try
		{
			testExecOk = m_testExecutor->iterate();
		}
		catch (const std::exception& e)
		{
			die("%s", e.what());
		}
	}

	if (!platformOk || !testExecOk)
	{
		if (!platformOk)
			print("\nABORTED!\n");
		else
			print("\nDONE!\n");

		const RunMode runMode = m_testCtx->getCommandLine().getRunMode();
		if (runMode == RUNMODE_EXECUTE)
		{
			const TestRunStatus& result = m_testExecutor->getStatus();

			// Report statistics.
			print("\nTest run totals:\n");
			print("  Passed:        %d/%d (%.1f%%)\n", result.numPassed,		result.numExecuted, (result.numExecuted > 0 ? (100.0f * result.numPassed		/ result.numExecuted) : 0.0f));
			print("  Failed:        %d/%d (%.1f%%)\n", result.numFailed,		result.numExecuted, (result.numExecuted > 0 ? (100.0f * result.numFailed		/ result.numExecuted) : 0.0f));
			print("  Not supported: %d/%d (%.1f%%)\n", result.numNotSupported,	result.numExecuted, (result.numExecuted > 0 ? (100.0f * result.numNotSupported	/ result.numExecuted) : 0.0f));
			print("  Warnings:      %d/%d (%.1f%%)\n", result.numWarnings,		result.numExecuted, (result.numExecuted > 0 ? (100.0f * result.numWarnings		/ result.numExecuted) : 0.0f));
			if (!result.isComplete)
				print("Test run was ABORTED!\n");
		}
	}

	return platformOk && testExecOk;
}

void App::onWatchdogTimeout (qpWatchDog* watchDog, void* userPtr)
{
	DE_UNREF(watchDog);
	static_cast<App*>(userPtr)->onWatchdogTimeout();
}

void App::onCrash (qpCrashHandler* crashHandler, void* userPtr)
{
	DE_UNREF(crashHandler);
	static_cast<App*>(userPtr)->onCrash();
}

void App::onWatchdogTimeout (void)
{
	if (!m_crashLock.tryLock() || m_crashed)
		return; // In crash handler already.

	m_crashed = true;

	m_testCtx->getLog().terminateCase(QP_TEST_RESULT_TIMEOUT);
	die("Watchdog timer timeout");
}

static void writeCrashToLog (void* userPtr, const char* infoString)
{
	// \note THIS IS CALLED BY SIGNAL HANDLER! CALLING MALLOC/FREE IS NOT ALLOWED!
	TestLog* log = static_cast<TestLog*>(userPtr);
	log->writeMessage(infoString);
}

static void writeCrashToConsole (void* userPtr, const char* infoString)
{
	// \note THIS IS CALLED BY SIGNAL HANDLER! CALLING MALLOC/FREE IS NOT ALLOWED!
	DE_UNREF(userPtr);
	qpPrint(infoString);
}

void App::onCrash (void)
{
	// \note THIS IS CALLED BY SIGNAL HANDLER! CALLING MALLOC/FREE IS NOT ALLOWED!

	if (!m_crashLock.tryLock() || m_crashed)
		return; // In crash handler already.

	m_crashed = true;

	bool isInCase = m_testExecutor ? m_testExecutor->isInTestCase() : false;

	if (isInCase)
	{
		qpCrashHandler_writeCrashInfo(m_crashHandler, writeCrashToLog, &m_testCtx->getLog());
		m_testCtx->getLog().terminateCase(QP_TEST_RESULT_CRASH);
	}
	else
		qpCrashHandler_writeCrashInfo(m_crashHandler, writeCrashToConsole, DE_NULL);

	die("Test program crashed");
}

} // tcu
