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
 * \brief Android test activity.
 *//*--------------------------------------------------------------------*/

#include "tcuAndroidTestActivity.hpp"
#include "tcuAndroidUtil.hpp"

#include <android/window.h>

#include <string>
#include <stdlib.h>

using std::string;

namespace tcu
{
namespace Android
{

// TestApp

TestApp::TestApp (NativeActivity& activity, ANativeWindow* window, const CommandLine& cmdLine)
	: m_cmdLine		(cmdLine)
	, m_platform	(window)
	, m_archive		(activity.getNativeActivity()->assetManager)
	, m_log			(m_cmdLine.getLogFileName())
	, m_app			(m_platform, m_archive, m_log, m_cmdLine)
{
}

TestApp::~TestApp (void)
{
}

bool TestApp::iterate (void)
{
	return m_app.iterate();
}

// TestThread

TestThread::TestThread (NativeActivity& activity, const CommandLine& cmdLine)
	: RenderThread	(activity)
	, m_cmdLine		(cmdLine)
	, m_testApp		(DE_NULL)
	, m_done		(false)
{
}

TestThread::~TestThread (void)
{
	// \note m_testApp is managed by thread.
}

void TestThread::run (void)
{
	RenderThread::run();

	delete m_testApp;
	m_testApp = DE_NULL;
}

void TestThread::onWindowCreated (ANativeWindow* window)
{
	DE_ASSERT(!m_testApp);
	m_testApp = new TestApp(getNativeActivity(), window, m_cmdLine);
}

void TestThread::onWindowDestroyed (ANativeWindow* window)
{
	DE_UNREF(window);
	DE_ASSERT(m_testApp);
	delete m_testApp;
	m_testApp = DE_NULL;

	if (!m_done)
	{
		// \note We could just throw exception here and RenderThread would gracefully terminate.
		//		 However, native window is often destroyed when app is closed and android may not
		//		 end up calling onStop().
		die("Window was destroyed during execution");
	}
}

void TestThread::onWindowResized (ANativeWindow* window)
{
	// \todo [2013-05-12 pyry] Handle this in some sane way.
	DE_UNREF(window);
	print("Warning: Native window was resized, results may be undefined");
}

bool TestThread::render (void)
{
	DE_ASSERT(m_testApp);
	m_done = !m_testApp->iterate();
	return !m_done;
}

// TestActivity

TestActivity::TestActivity (ANativeActivity* activity)
	: RenderActivity	(activity)
	, m_cmdLine			(getIntentStringExtra(activity, "cmdLine"))
	, m_testThread		(*this, m_cmdLine)
{
	// Provide RenderThread
	setThread(&m_testThread);

	// Set initial orientation.
	setRequestedOrientation(getNativeActivity(), mapScreenRotation(m_cmdLine.getScreenRotation()));

	// Set up window flags.
	ANativeActivity_setWindowFlags(activity, AWINDOW_FLAG_KEEP_SCREEN_ON	|
											 AWINDOW_FLAG_TURN_SCREEN_ON	|
											 AWINDOW_FLAG_FULLSCREEN		|
											 AWINDOW_FLAG_SHOW_WHEN_LOCKED, 0);
}

TestActivity::~TestActivity (void)
{
}

void TestActivity::onStop (void)
{
	RenderActivity::onStop();

	// Kill this process.
	print("Done, killing process");
	exit(0);
}

void TestActivity::onConfigurationChanged (void)
{
	RenderActivity::onConfigurationChanged();

	// Update rotation.
	setRequestedOrientation(getNativeActivity(), mapScreenRotation(m_cmdLine.getScreenRotation()));
}

} // Android
} // tcu
