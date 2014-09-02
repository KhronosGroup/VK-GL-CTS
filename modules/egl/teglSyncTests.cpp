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
 * \brief EGL EGL_KHR_fence_sync and EGL_KHR_reusable_sync tests
 *//*--------------------------------------------------------------------*/

#include "teglSyncTests.hpp"

#include "egluNativeWindow.hpp"
#include "egluStrUtil.hpp"
#include "egluUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"

#include "gluDefs.hpp"

#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#ifndef EGL_KHR_wait_sync
#define EGL_KHR_wait_sync 1
typedef EGLint (EGLAPIENTRYP PFNEGLWAITSYNCKHRPROC) (EGLDisplay dpy, EGLSyncKHR sync, EGLint flags);
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLint EGLAPIENTRY eglWaitSyncKHR (EGLDisplay dpy, EGLSyncKHR sync, EGLint flags);
#endif
#endif /* EGL_KHR_wait_sync */

#include <vector>
#include <string>
#include <sstream>
#include <set>

using std::vector;
using std::string;
using std::set;

using tcu::TestLog;

namespace deqp
{
namespace egl
{
namespace
{

const char* getSyncTypeName (EGLenum syncType)
{
	switch (syncType)
	{
		case EGL_SYNC_FENCE_KHR:	return "EGL_SYNC_FENCE_KHR";
		case EGL_SYNC_REUSABLE_KHR:	return "EGL_SYNC_REUSABLE_KHR";
		default:
			DE_ASSERT(DE_FALSE);
			return "<Unknown>";
	}
}

class SyncTest : public TestCase
{
public:
	enum Extension
	{
		EXTENSION_NONE				= 0,
		EXTENSION_WAIT_SYNC			= (0x1 << 0),
		EXTENSION_FENCE_SYNC		= (0x1 << 1),
		EXTENSION_REUSABLE_SYNC		= (0x1 << 2)
	};
									SyncTest	(EglTestContext& eglTestCtx, EGLenum syncType, Extension extensions, const char* name, const char* description);
									~SyncTest	(void);

	void							init		(void);
	void							deinit		(void);

protected:
	const EGLenum					m_syncType;
	const Extension					m_extensions;

	EGLDisplay						m_eglDisplay;
	EGLConfig						m_eglConfig;
	EGLSurface						m_eglSurface;
	eglu::NativeWindow*				m_nativeWindow;
	EGLContext						m_eglContext;
	EGLSyncKHR						m_sync;

	struct
	{
		PFNEGLCREATESYNCKHRPROC		createSync;
		PFNEGLDESTROYSYNCKHRPROC	destroySync;
		PFNEGLCLIENTWAITSYNCKHRPROC	clientWaitSync;
		PFNEGLGETSYNCATTRIBKHRPROC	getSyncAttrib;

		PFNEGLWAITSYNCKHRPROC		waitSync;
		PFNEGLSIGNALSYNCKHRPROC		signalSync;
	} m_ext;
};

SyncTest::SyncTest (EglTestContext& eglTestCtx, EGLenum syncType, Extension extensions, const char* name, const char* description)
	: TestCase			(eglTestCtx, name, description)
	, m_syncType		(syncType)
	, m_extensions		(extensions)
	, m_eglDisplay		(EGL_NO_DISPLAY)
	, m_eglSurface		(EGL_NO_SURFACE)
	, m_nativeWindow	(DE_NULL)
	, m_eglContext		(EGL_NO_CONTEXT)
	, m_sync			(EGL_NO_SYNC_KHR)
{
	m_ext.createSync		= DE_NULL;
	m_ext.destroySync		= DE_NULL;
	m_ext.clientWaitSync	= DE_NULL;
	m_ext.getSyncAttrib		= DE_NULL;
	m_ext.waitSync			= DE_NULL;
}

SyncTest::~SyncTest (void)
{
	SyncTest::deinit();
}

void requiredEGLExtensions (EGLDisplay display, SyncTest::Extension requiredExtensions)
{
	SyncTest::Extension foundExtensions = SyncTest::EXTENSION_NONE;
	std::istringstream	extensionStream(eglQueryString(display, EGL_EXTENSIONS));
	string				extension;

	TCU_CHECK_EGL_MSG("eglQueryString(display, EGL_EXTENSIONS)");

	while (std::getline(extensionStream, extension, ' '))
	{
		if (extension == "EGL_KHR_fence_sync")
			foundExtensions = (SyncTest::Extension)(foundExtensions | SyncTest::EXTENSION_FENCE_SYNC);
		else if (extension == "EGL_KHR_reusable_sync")
			foundExtensions = (SyncTest::Extension)(foundExtensions | SyncTest::EXTENSION_REUSABLE_SYNC);
		else if (extension == "EGL_KHR_wait_sync")
			foundExtensions = (SyncTest::Extension)(foundExtensions | SyncTest::EXTENSION_WAIT_SYNC);
	}

	{
		const SyncTest::Extension missingExtensions = (SyncTest::Extension)((foundExtensions & requiredExtensions) ^ requiredExtensions);

		if ((missingExtensions & SyncTest::EXTENSION_FENCE_SYNC) != 0)
			throw tcu::NotSupportedError("EGL_KHR_fence_sync not supported", "", __FILE__, __LINE__);

		if ((missingExtensions & SyncTest::EXTENSION_REUSABLE_SYNC) != 0)
			throw tcu::NotSupportedError("EGL_KHR_reusable_sync not supported", "", __FILE__, __LINE__);

		if ((missingExtensions & SyncTest::EXTENSION_WAIT_SYNC) != 0)
			throw tcu::NotSupportedError("EGL_KHR_wait_sync not supported", "", __FILE__, __LINE__);
	}
}

void requiredGLESExtensions (void)
{
	bool				found = false;
	std::istringstream	extensionStream((const char*)glGetString(GL_EXTENSIONS));
	string				extension;

	GLU_CHECK_MSG("glGetString(GL_EXTENSIONS)");

	while (std::getline(extensionStream, extension, ' '))
	{
		if (extension == "GL_OES_EGL_sync")
			found = true;
	}

	if (!found)
		throw tcu::NotSupportedError("GL_OES_EGL_sync not supported", "", __FILE__, __LINE__);
}

SyncTest::Extension getSyncTypeExtension (EGLenum syncType)
{
	switch (syncType)
	{
		case EGL_SYNC_FENCE_KHR:	return SyncTest::EXTENSION_FENCE_SYNC;
		case EGL_SYNC_REUSABLE_KHR:	return SyncTest::EXTENSION_REUSABLE_SYNC;
		default:
			DE_ASSERT(DE_FALSE);
			return SyncTest::EXTENSION_NONE;
	}
}

void SyncTest::init (void)
{
	const EGLint displayAttribList[] =
	{
		EGL_RENDERABLE_TYPE,	EGL_OPENGL_ES2_BIT,
		EGL_SURFACE_TYPE,		EGL_WINDOW_BIT,
		EGL_ALPHA_SIZE,			1,
		EGL_NONE
	};

	const EGLint contextAttribList[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	tcu::egl::Display&	display = m_eglTestCtx.getDisplay();
	vector<EGLConfig>	configs;

	display.chooseConfig(displayAttribList, configs);
	m_eglDisplay	= display.getEGLDisplay();
	m_eglConfig 	= configs[0];

	{
		const Extension syncTypeExtension = getSyncTypeExtension(m_syncType);
		requiredEGLExtensions(m_eglDisplay, (Extension)(m_extensions | syncTypeExtension));
	}

	m_ext.createSync		= (PFNEGLCREATESYNCKHRPROC)eglGetProcAddress("eglCreateSyncKHR");
	m_ext.destroySync		= (PFNEGLDESTROYSYNCKHRPROC)eglGetProcAddress("eglDestroySyncKHR");
	m_ext.clientWaitSync	= (PFNEGLCLIENTWAITSYNCKHRPROC)eglGetProcAddress("eglClientWaitSyncKHR");
	m_ext.getSyncAttrib		= (PFNEGLGETSYNCATTRIBKHRPROC)eglGetProcAddress("eglGetSyncAttribKHR");
	m_ext.waitSync			= (PFNEGLWAITSYNCKHRPROC)eglGetProcAddress("eglWaitSyncKHR");
	m_ext.signalSync		= (PFNEGLSIGNALSYNCKHRPROC)eglGetProcAddress("eglSignalSyncKHR");

	// Create context
	TCU_CHECK_EGL_CALL(eglBindAPI(EGL_OPENGL_ES_API));
	m_eglContext = eglCreateContext(m_eglDisplay, m_eglConfig, EGL_NO_CONTEXT, contextAttribList);
	TCU_CHECK_EGL_MSG("Failed to create GLES2 context");

	// Create surface
	m_nativeWindow = m_eglTestCtx.createNativeWindow(m_eglDisplay, m_eglConfig, DE_NULL, 480, 480, eglu::parseWindowVisibility(m_testCtx.getCommandLine()));
	m_eglSurface = eglu::createWindowSurface(m_eglTestCtx.getNativeDisplay(), *m_nativeWindow, m_eglDisplay, m_eglConfig, DE_NULL);

	TCU_CHECK_EGL_CALL(eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext));

	requiredGLESExtensions();
}

void SyncTest::deinit (void)
{
	if (m_eglDisplay != EGL_NO_DISPLAY)
	{
		if (m_sync != EGL_NO_SYNC_KHR)
		{
			TCU_CHECK_EGL_CALL(m_ext.destroySync(m_eglDisplay, m_sync));
			m_sync = EGL_NO_SYNC_KHR;
		}

		TCU_CHECK_EGL_CALL(eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

		if (m_eglContext != EGL_NO_CONTEXT)
		{
			TCU_CHECK_EGL_CALL(eglDestroyContext(m_eglDisplay, m_eglContext));
			m_eglContext = EGL_NO_CONTEXT;
		}

		if (m_eglSurface != EGL_NO_SURFACE)
		{
			TCU_CHECK_EGL_CALL(eglDestroySurface(m_eglDisplay, m_eglSurface));
			m_eglSurface = EGL_NO_SURFACE;
		}

		delete m_nativeWindow;
		m_nativeWindow = DE_NULL;

		m_eglDisplay = EGL_NO_DISPLAY;
	}
}

class CreateNullAttribsTest : public SyncTest
{
public:
					CreateNullAttribsTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, "create_null_attribs", "create_null_attribs") {}

	IterateResult	iterate					(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.createSync);

		m_sync = m_ext.createSync(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = eglCreateSyncKHR(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" << TestLog::EndMessage;
		TCU_CHECK_EGL_MSG("eglCreateSyncKHR()");

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class CreateEmptyAttribsTest : public SyncTest
{
public:
					CreateEmptyAttribsTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, "create_empty_attribs", "create_empty_attribs") {}

	IterateResult	iterate					(void)
	{

		const EGLint attribList[] = {
			EGL_NONE
		};
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.createSync);

		m_sync = m_ext.createSync(m_eglDisplay, m_syncType, attribList);
		log << TestLog::Message << m_sync << " = eglCreateSyncKHR(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", { EGL_NONE })" << TestLog::EndMessage;
		TCU_CHECK_EGL_MSG("eglCreateSyncKHR()");

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class CreateInvalidDisplayTest : public SyncTest
{
public:
					CreateInvalidDisplayTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, "create_invalid_display", "create_invalid_display") {}

	IterateResult	iterate						(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.createSync);

		m_sync = m_ext.createSync(EGL_NO_DISPLAY, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = eglCreateSyncKHR(EGL_NO_DISPLAY, " << getSyncTypeName(m_syncType) << ", NULL)" << TestLog::EndMessage;

		EGLint error = eglGetError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_DISPLAY)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) << "' expected EGL_BAD_DISPLAY" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return STOP;
		}

		TCU_CHECK(m_sync == EGL_NO_SYNC_KHR);

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class CreateInvalidTypeTest : public SyncTest
{
public:
					CreateInvalidTypeTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, "create_invalid_type", "create_invalid_type") {}

	IterateResult	iterate					(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.createSync);

		m_sync = m_ext.createSync(m_eglDisplay, EGL_NONE, NULL);
		log << TestLog::Message << m_sync << " = eglCreateSyncKHR(" << m_eglDisplay << ", EGL_NONE, NULL)" << TestLog::EndMessage;

		EGLint error = eglGetError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_ATTRIBUTE)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) << "' expected EGL_BAD_ATTRIBUTE" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return STOP;
		}

		TCU_CHECK(m_sync == EGL_NO_SYNC_KHR);

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class CreateInvalidAttribsTest : public SyncTest
{
public:
					CreateInvalidAttribsTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, "create_invalid_attribs", "create_invalid_attribs") {}

	IterateResult	iterate						(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.createSync);

		EGLint attribs[] = {
			2, 3, 4, 5,
			EGL_NONE
		};

		m_sync = m_ext.createSync(m_eglDisplay, m_syncType, attribs);
		log << TestLog::Message << m_sync << " = eglCreateSyncKHR(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", { 2, 3, 4, 5, EGL_NONE })" << TestLog::EndMessage;

		EGLint error = eglGetError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_ATTRIBUTE)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) << "' expected EGL_BAD_ATTRIBUTE" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return STOP;
		}

		TCU_CHECK(m_sync == EGL_NO_SYNC_KHR);

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class CreateInvalidContextTest : public SyncTest
{
public:
					CreateInvalidContextTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, "create_invalid_context", "create_invalid_context") {}

	IterateResult	iterate						(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.createSync);

		log << TestLog::Message << "eglMakeCurrent(" << m_eglDisplay << ", EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)" << TestLog::EndMessage;
		TCU_CHECK_EGL_CALL(eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

		m_sync = m_ext.createSync(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = eglCreateSyncKHR(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" << TestLog::EndMessage;

		EGLint error = eglGetError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_MATCH)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) << "' expected EGL_BAD_MATCH" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return STOP;
		}

		TCU_CHECK(m_sync == EGL_NO_SYNC_KHR);

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class ClientWaitNoTimeoutTest : public SyncTest
{
public:
					ClientWaitNoTimeoutTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, "wait_no_timeout", "wait_no_timeout") {}

	IterateResult	iterate					(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.createSync);
		TCU_CHECK(m_ext.clientWaitSync);

		m_sync = m_ext.createSync(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = eglCreateSyncKHR(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" << TestLog::EndMessage;
		TCU_CHECK_EGL_MSG("eglCreateSyncKHR()");

		EGLint status = m_ext.clientWaitSync(m_eglDisplay, m_sync, 0, 0);
		log << TestLog::Message << status << " = eglClientWaitSyncKHR(" << m_eglDisplay << ", " << m_sync << ", 0, 0)" << TestLog::EndMessage;

		if (m_syncType == EGL_SYNC_FENCE_KHR)
			TCU_CHECK(status == EGL_CONDITION_SATISFIED_KHR || status == EGL_TIMEOUT_EXPIRED_KHR);
		else if (m_syncType == EGL_SYNC_REUSABLE_KHR)
			TCU_CHECK(status == EGL_TIMEOUT_EXPIRED_KHR);
		else
			DE_ASSERT(DE_FALSE);

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}

};

class ClientWaitForeverTest : public SyncTest
{
public:
					ClientWaitForeverTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, "wait_forever", "wait_forever") {}

	IterateResult	iterate					(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.createSync);
		TCU_CHECK(m_ext.clientWaitSync);

		m_sync = m_ext.createSync(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = eglCreateSyncKHR(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" << TestLog::EndMessage;
		TCU_CHECK_EGL_MSG("eglCreateSyncKHR()");

		if (m_syncType == EGL_SYNC_REUSABLE_KHR)
		{
			EGLBoolean ret = m_ext.signalSync(m_eglDisplay, m_sync, EGL_SIGNALED_KHR);
			log << TestLog::Message << ret << " = eglSignalSyncKHR(" << m_eglDisplay << ", " << m_sync << ", EGL_SIGNALED_KHR)" << TestLog::EndMessage;
			TCU_CHECK_EGL_MSG("eglSignalSyncKHR()");
		}
		else if (m_syncType == EGL_SYNC_FENCE_KHR)
		{
			GLU_CHECK_CALL(glFlush());
			log << TestLog::Message << "glFlush()" << TestLog::EndMessage;
		}
		else
			DE_ASSERT(DE_FALSE);

		EGLint status = m_ext.clientWaitSync(m_eglDisplay, m_sync, 0, EGL_FOREVER_KHR);
		log << TestLog::Message << status << " = eglClientWaitSyncKHR(" << m_eglDisplay << ", " << m_sync << ", 0, EGL_FOREVER_KHR)" << TestLog::EndMessage;

		TCU_CHECK(status == EGL_CONDITION_SATISFIED_KHR);
		TCU_CHECK_EGL_MSG("eglClientWaitSyncKHR()");

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class ClientWaitNoContextTest : public SyncTest
{
public:
					ClientWaitNoContextTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, "wait_no_context", "wait_no_Context") {}

	IterateResult	iterate					(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.createSync);
		TCU_CHECK(m_ext.clientWaitSync);

		m_sync = m_ext.createSync(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = eglCreateSyncKHR(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" << TestLog::EndMessage;
		TCU_CHECK_EGL_MSG("eglCreateSyncKHR()");


		if (m_syncType == EGL_SYNC_REUSABLE_KHR)
		{
			EGLBoolean ret = m_ext.signalSync(m_eglDisplay, m_sync, EGL_SIGNALED_KHR);
			log << TestLog::Message << ret << " = eglSignalSyncKHR(" << m_eglDisplay << ", " << m_sync << ", EGL_SIGNALED_KHR)" << TestLog::EndMessage;
			TCU_CHECK_EGL_MSG("eglSignalSyncKHR()");
		}
		else if (m_syncType == EGL_SYNC_FENCE_KHR)
		{
			GLU_CHECK_CALL(glFlush());
			log << TestLog::Message << "glFlush()" << TestLog::EndMessage;
		}
		else
			DE_ASSERT(DE_FALSE);

		log << TestLog::Message << "eglMakeCurrent(" << m_eglDisplay << ", EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)" << TestLog::EndMessage;
		TCU_CHECK_EGL_CALL(eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

		EGLint result = m_ext.clientWaitSync(m_eglDisplay, m_sync, 0, EGL_FOREVER_KHR);
		log << TestLog::Message << result << " = eglClientWaitSyncKHR(" << m_eglDisplay << ", " << m_sync << ", 0, EGL_FOREVER_KHR)" << TestLog::EndMessage;

		TCU_CHECK(result == EGL_CONDITION_SATISFIED_KHR);

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class ClientWaitForeverFlushTest : public SyncTest
{
public:
					ClientWaitForeverFlushTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, "wait_forever_flush", "wait_forever_flush") {}

	IterateResult	iterate						(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.createSync);
		TCU_CHECK(m_ext.clientWaitSync);

		m_sync = m_ext.createSync(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = eglCreateSyncKHR(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" << TestLog::EndMessage;
		TCU_CHECK_EGL_MSG("eglCreateSyncKHR()");

		if (m_syncType == EGL_SYNC_REUSABLE_KHR)
		{
			EGLBoolean ret = m_ext.signalSync(m_eglDisplay, m_sync, EGL_SIGNALED_KHR);
			log << TestLog::Message << ret << " = eglSignalSyncKHR(" << m_eglDisplay << ", " << m_sync << ", EGL_SIGNALED_KHR)" << TestLog::EndMessage;
			TCU_CHECK_EGL_MSG("eglSignalSyncKHR()");
		}

		EGLint status = m_ext.clientWaitSync(m_eglDisplay, m_sync, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, EGL_FOREVER_KHR);
		log << TestLog::Message << status << " = eglClientWaitSyncKHR(" << m_eglDisplay << ", " << m_sync << ", EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, EGL_FOREVER_KHR)" << TestLog::EndMessage;

		TCU_CHECK(status == EGL_CONDITION_SATISFIED_KHR);

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class ClientWaitInvalidDisplayTest : public SyncTest
{
public:
					ClientWaitInvalidDisplayTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, "wait_invalid_display", "wait_invalid_display") {}

	IterateResult	iterate							(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.createSync);
		TCU_CHECK(m_ext.clientWaitSync);

		m_sync = m_ext.createSync(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = eglCreateSyncKHR(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" << TestLog::EndMessage;
		TCU_CHECK_EGL_MSG("eglCreateSyncKHR()");

		EGLint status = m_ext.clientWaitSync(EGL_NO_DISPLAY, m_sync, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, EGL_FOREVER_KHR);
		log << TestLog::Message << status << " = eglClientWaitSyncKHR(EGL_NO_DISPLAY, " << m_sync << ", EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, EGL_FOREVER_KHR)" << TestLog::EndMessage;

		EGLint error = eglGetError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_DISPLAY)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) << "' expected EGL_BAD_DISPLAY" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return STOP;
		}

		TCU_CHECK(status == EGL_FALSE);

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class ClientWaitInvalidSyncTest : public SyncTest
{
public:
					ClientWaitInvalidSyncTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, "wait_invalid_sync", "wait_invalid_sync") {}

	IterateResult	iterate						(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.clientWaitSync);

		EGLint status = m_ext.clientWaitSync(m_eglDisplay, EGL_NO_SYNC_KHR, 0, EGL_FOREVER_KHR);
		log << TestLog::Message << status << " = eglClientWaitSyncKHR(" << m_eglDisplay << ", EGL_NO_SYNC_KHR, 0, EGL_FOREVER_KHR)" << TestLog::EndMessage;

		EGLint error = eglGetError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_PARAMETER)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) << "' expected EGL_BAD_PARAMETER" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return STOP;
		}

		TCU_CHECK(status == EGL_FALSE);

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class ClientWaitInvalidFlagTest : public SyncTest
{
public:
					ClientWaitInvalidFlagTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, "wait_invalid_flag", "wait_invalid_flag") {}

	IterateResult	iterate						(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.createSync);
		TCU_CHECK(m_ext.clientWaitSync);

		m_sync = m_ext.createSync(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = eglCreateSyncKHR(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" << TestLog::EndMessage;
		TCU_CHECK_EGL_MSG("eglCreateSyncKHR()");

		EGLint status = m_ext.clientWaitSync(m_eglDisplay, m_sync, 0xFFFFFFFF, EGL_FOREVER_KHR);
		log << TestLog::Message << status << " = eglClientWaitSyncKHR(" << m_eglDisplay << ", " << m_sync << ", 0xFFFFFFFF, EGL_FOREVER_KHR)" << TestLog::EndMessage;

		EGLint error = eglGetError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_PARAMETER)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) << "' expected EGL_BAD_PARAMETER" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return STOP;
		}

		TCU_CHECK(status == EGL_FALSE);

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class GetSyncTypeTest : public SyncTest
{
public:
					GetSyncTypeTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, "get_type", "get_type") {}

	IterateResult	iterate			(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.createSync);
		TCU_CHECK(m_ext.getSyncAttrib);

		m_sync = m_ext.createSync(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = eglCreateSyncKHR(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" << TestLog::EndMessage;
		TCU_CHECK_EGL_MSG("eglCreateSyncKHR()");

		EGLint type = 0;
		TCU_CHECK_EGL_CALL(m_ext.getSyncAttrib(m_eglDisplay, m_sync, EGL_SYNC_TYPE_KHR, &type));
		log << TestLog::Message << "eglGetSyncAttribKHR(" << m_eglDisplay << ", " << m_sync << ", EGL_SYNC_TYPE_KHR, {" << type << "})" << TestLog::EndMessage;

		TCU_CHECK(type == ((EGLint)m_syncType));

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class GetSyncStatusTest : public SyncTest
{
public:
					GetSyncStatusTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, "get_status", "get_status") {}

	IterateResult	iterate				(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.createSync);
		TCU_CHECK(m_ext.getSyncAttrib);

		m_sync = m_ext.createSync(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = eglCreateSyncKHR(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" << TestLog::EndMessage;
		TCU_CHECK_EGL_MSG("eglCreateSyncKHR()");

		EGLint status = 0;
		TCU_CHECK_EGL_CALL(m_ext.getSyncAttrib(m_eglDisplay, m_sync, EGL_SYNC_STATUS_KHR, &status));
		log << TestLog::Message << "eglGetSyncAttribKHR(" << m_eglDisplay << ", " << m_sync << ", EGL_SYNC_STATUS_KHR, {" << status << "})" << TestLog::EndMessage;

		if (m_syncType == EGL_SYNC_FENCE_KHR)
			TCU_CHECK(status == EGL_SIGNALED_KHR || status == EGL_UNSIGNALED_KHR);
		else if (m_syncType == EGL_SYNC_REUSABLE_KHR)
			TCU_CHECK(status == EGL_UNSIGNALED_KHR);

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class GetSyncStatusSignaledTest : public SyncTest
{
public:
					GetSyncStatusSignaledTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, "get_status_signaled", "get_status_signaled") {}

	IterateResult	iterate						(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.createSync);
		TCU_CHECK(m_ext.clientWaitSync);
		TCU_CHECK(m_ext.getSyncAttrib);

		m_sync = m_ext.createSync(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = eglCreateSyncKHR(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" << TestLog::EndMessage;
		TCU_CHECK_EGL_MSG("eglCreateSyncKHR()");

		if (m_syncType == EGL_SYNC_REUSABLE_KHR)
		{
			EGLBoolean ret = m_ext.signalSync(m_eglDisplay, m_sync, EGL_SIGNALED_KHR);
			log << TestLog::Message << ret << " = eglSignalSyncKHR(" << m_eglDisplay << ", " << m_sync << ", EGL_SIGNALED_KHR)" << TestLog::EndMessage;
			TCU_CHECK_EGL_MSG("eglSignalSyncKHR()");
		}
		else if (m_syncType == EGL_SYNC_FENCE_KHR)
		{
			GLU_CHECK_CALL(glFinish());
			log << TestLog::Message << "glFinish()" << TestLog::EndMessage;
		}
		else
			DE_ASSERT(DE_FALSE);

		{
			EGLint status = m_ext.clientWaitSync(m_eglDisplay, m_sync, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, EGL_FOREVER_KHR);
			log << TestLog::Message << status << " = eglClientWaitSyncKHR(" << m_eglDisplay << ", " << m_sync << ", EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, EGL_FOREVER_KHR)" << TestLog::EndMessage;
			TCU_CHECK(status == EGL_CONDITION_SATISFIED_KHR);
		}

		EGLint status = 0;
		TCU_CHECK_EGL_CALL(m_ext.getSyncAttrib(m_eglDisplay, m_sync, EGL_SYNC_STATUS_KHR, &status));
		log << TestLog::Message << "eglGetSyncAttribKHR(" << m_eglDisplay << ", " << m_sync << ", EGL_SYNC_STATUS_KHR, {" << status << "})" << TestLog::EndMessage;

		TCU_CHECK(status == EGL_SIGNALED_KHR);

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class GetSyncConditionTest : public SyncTest
{
public:
					GetSyncConditionTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, "get_condition", "get_condition") {}

	IterateResult	iterate					(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.createSync);
		TCU_CHECK(m_ext.getSyncAttrib);

		m_sync = m_ext.createSync(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = eglCreateSyncKHR(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" << TestLog::EndMessage;
		TCU_CHECK_EGL_MSG("eglCreateSyncKHR()");

		EGLint condition = 0;
		TCU_CHECK_EGL_CALL(m_ext.getSyncAttrib(m_eglDisplay, m_sync, EGL_SYNC_CONDITION_KHR, &condition));
		log << TestLog::Message << "eglGetSyncAttribKHR(" << m_eglDisplay << ", " << m_sync << ", EGL_SYNC_CONDITION_KHR, {" << condition << "})" << TestLog::EndMessage;

		TCU_CHECK(condition == EGL_SYNC_PRIOR_COMMANDS_COMPLETE_KHR);

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class GetSyncInvalidDisplayTest : public SyncTest
{
public:
					GetSyncInvalidDisplayTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, "get_invalid_display", "get_invalid_display") {}

	IterateResult	iterate						(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.createSync);
		TCU_CHECK(m_ext.getSyncAttrib);

		m_sync = m_ext.createSync(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = eglCreateSyncKHR(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" << TestLog::EndMessage;
		TCU_CHECK_EGL_MSG("eglCreateSyncKHR()");

		EGLint condition = 0xF0F0F;
		EGLBoolean result = m_ext.getSyncAttrib(EGL_NO_DISPLAY, m_sync, EGL_SYNC_CONDITION_KHR, &condition);
		log << TestLog::Message << result << " = eglGetSyncAttribKHR(EGL_NO_DISPLAY, " << m_sync << ", EGL_SYNC_CONDITION_KHR, {" << condition << "})" << TestLog::EndMessage;

		EGLint error = eglGetError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_DISPLAY)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) << "' expected EGL_BAD_DISPLAY" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return STOP;
		}

		TCU_CHECK(result == EGL_FALSE);
		TCU_CHECK(condition == 0xF0F0F);

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class GetSyncInvalidSyncTest : public SyncTest
{
public:
					GetSyncInvalidSyncTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, "get_invalid_sync", "get_invalid_sync") {}

	IterateResult	iterate					(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.getSyncAttrib);

		EGLint condition = 0xF0F0F;
		EGLBoolean result = m_ext.getSyncAttrib(m_eglDisplay, EGL_NO_SYNC_KHR, EGL_SYNC_CONDITION_KHR, &condition);
		log << TestLog::Message << result << " = eglGetSyncAttribKHR(" << m_eglDisplay << ", EGL_NO_SYNC_KHR, EGL_SYNC_CONDITION_KHR, {" << condition << "})" << TestLog::EndMessage;

		EGLint error = eglGetError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_PARAMETER)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) << "' expected EGL_BAD_PARAMETER" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return STOP;
		}

		TCU_CHECK(result == EGL_FALSE);
		TCU_CHECK(condition == 0xF0F0F);

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class GetSyncInvalidAttributeTest : public SyncTest
{
public:
					GetSyncInvalidAttributeTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, "get_invalid_attribute", "get_invalid_attribute") {}

	IterateResult	iterate						(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.createSync);
		TCU_CHECK(m_ext.getSyncAttrib);

		m_sync = m_ext.createSync(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = eglCreateSyncKHR(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" << TestLog::EndMessage;
		TCU_CHECK_EGL_MSG("eglCreateSyncKHR()");

		EGLint condition = 0xF0F0F;
		EGLBoolean result = m_ext.getSyncAttrib(m_eglDisplay, m_sync, EGL_NONE, &condition);
		log << TestLog::Message << result << " = eglGetSyncAttribKHR(" << m_eglDisplay << ", " << m_sync << ", EGL_NONE, {" << condition << "})" << TestLog::EndMessage;

		EGLint error = eglGetError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_ATTRIBUTE)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) << "' expected EGL_BAD_ATTRIBUTE" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return STOP;
		}

		TCU_CHECK(result == EGL_FALSE);
		TCU_CHECK(condition == 0xF0F0F);

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class GetSyncInvalidValueTest : public SyncTest
{
public:
					GetSyncInvalidValueTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, "get_invalid_value", "get_invalid_value") {}

	IterateResult	iterate					(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.createSync);
		TCU_CHECK(m_ext.getSyncAttrib);

		m_sync = m_ext.createSync(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = eglCreateSyncKHR(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" << TestLog::EndMessage;
		TCU_CHECK_EGL_MSG("eglCreateSyncKHR()");

		EGLBoolean result = m_ext.getSyncAttrib(m_eglDisplay, m_sync, EGL_SYNC_TYPE_KHR, NULL);
		log << TestLog::Message << result << " = eglGetSyncAttribKHR(" << m_eglDisplay << ", " << m_sync << ", EGL_SYNC_CONDITION_KHR, NULL)" << TestLog::EndMessage;

		EGLint error = eglGetError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_PARAMETER)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) << "' expected EGL_BAD_PARAMETER" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return STOP;
		}

		TCU_CHECK(result == EGL_FALSE);

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class DestroySyncTest : public SyncTest
{
public:
					DestroySyncTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, "destroy", "destroy") {}

	IterateResult	iterate			(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.createSync);
		TCU_CHECK(m_ext.destroySync);

		m_sync = m_ext.createSync(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << "eglCreateSyncKHR(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" << TestLog::EndMessage;
		TCU_CHECK_EGL_MSG("eglCreateSyncKHR()");

		log << TestLog::Message << "eglDestroySyncKHR(" << m_eglDisplay << ", " << m_sync << ")" << TestLog::EndMessage;
		TCU_CHECK_EGL_CALL(m_ext.destroySync(m_eglDisplay, m_sync));
		m_sync = EGL_NO_SYNC_KHR;

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class DestroySyncInvalidDislayTest : public SyncTest
{
public:
					DestroySyncInvalidDislayTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, "destroy_invalid_display", "destroy_invalid_display") {}

	IterateResult	iterate							(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.createSync);
		TCU_CHECK(m_ext.destroySync);

		m_sync = m_ext.createSync(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << "eglCreateSyncKHR(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" << TestLog::EndMessage;
		TCU_CHECK_EGL_MSG("eglCreateSyncKHR()");

		EGLBoolean result = m_ext.destroySync(EGL_NO_DISPLAY, m_sync);
		log << TestLog::Message << result << " = eglDestroySyncKHR(EGL_NO_DISPLAY, " << m_sync << ")" << TestLog::EndMessage;

		EGLint error = eglGetError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_DISPLAY)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) << "' expected EGL_BAD_DISPLAY" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return STOP;
		}

		TCU_CHECK(result == EGL_FALSE);

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class DestroySyncInvalidSyncTest : public SyncTest
{
public:
					DestroySyncInvalidSyncTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, "destroy_invalid_sync", "destroy_invalid_sync") {}

	IterateResult	iterate						(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.destroySync);

		EGLBoolean result = m_ext.destroySync(m_eglDisplay, EGL_NO_SYNC_KHR);
		log << TestLog::Message << result << " = eglDestroySyncKHR(" << m_eglDisplay << ", EGL_NO_SYNC_KHR)" << TestLog::EndMessage;

		EGLint error = eglGetError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_PARAMETER)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) << "' expected EGL_BAD_PARAMETER" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return STOP;
		}

		TCU_CHECK(result == EGL_FALSE);

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class WaitSyncTest : public SyncTest
{
public:
					WaitSyncTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_WAIT_SYNC, "wait_server", "wait_server") {}

	IterateResult	iterate			(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.createSync);
		TCU_CHECK(m_ext.waitSync);

		m_sync = m_ext.createSync(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = eglCreateSyncKHR(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" << TestLog::EndMessage;
		TCU_CHECK_EGL_MSG("eglCreateSyncKHR()");

		EGLint status = m_ext.waitSync(m_eglDisplay, m_sync, 0);
		log << TestLog::Message << status << " = eglWaitSyncKHR(" << m_eglDisplay << ", " << m_sync << ", 0, 0)" << TestLog::EndMessage;

		TCU_CHECK(status == EGL_TRUE);

		GLU_CHECK_CALL(glFinish());

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}

};

class WaitSyncInvalidDisplayTest : public SyncTest
{
public:
					WaitSyncInvalidDisplayTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_WAIT_SYNC, "wait_server_invalid_display", "wait_server_invalid_display") {}

	IterateResult	iterate						(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.createSync);
		TCU_CHECK(m_ext.waitSync);

		m_sync = m_ext.createSync(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = eglCreateSyncKHR(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" << TestLog::EndMessage;
		TCU_CHECK_EGL_MSG("eglCreateSyncKHR()");

		EGLint status = m_ext.waitSync(EGL_NO_DISPLAY, m_sync, 0);
		log << TestLog::Message << status << " = eglWaitSyncKHR(EGL_NO_DISPLAY, " << m_sync << ", 0)" << TestLog::EndMessage;

		EGLint error = eglGetError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_DISPLAY)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) << "' expected EGL_BAD_DISPLAY" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return STOP;
		}

		TCU_CHECK(status == EGL_FALSE);

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class WaitSyncInvalidSyncTest : public SyncTest
{
public:
					WaitSyncInvalidSyncTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_WAIT_SYNC, "wait_server_invalid_sync", "wait_server_invalid_sync") {}

	IterateResult	iterate					(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.waitSync);

		EGLint status = m_ext.waitSync(m_eglDisplay, EGL_NO_SYNC_KHR, 0);
		log << TestLog::Message << status << " = eglWaitSyncKHR(" << m_eglDisplay << ", EGL_NO_SYNC_KHR, 0)" << TestLog::EndMessage;

		EGLint error = eglGetError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_PARAMETER)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) << "' expected EGL_BAD_PARAMETER" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return STOP;
		}

		TCU_CHECK(status == EGL_FALSE);

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class WaitSyncInvalidFlagTest : public SyncTest
{
public:
					WaitSyncInvalidFlagTest	(EglTestContext& eglTestCtx, EGLenum syncType) : SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_WAIT_SYNC, "wait_server_invalid_flag", "wait_server_invalid_flag") {}

	IterateResult	iterate					(void)
	{
		TestLog& log = m_testCtx.getLog();
		TCU_CHECK(m_ext.createSync);
		TCU_CHECK(m_ext.waitSync);

		m_sync = m_ext.createSync(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = eglCreateSyncKHR(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" << TestLog::EndMessage;
		TCU_CHECK_EGL_MSG("eglCreateSyncKHR()");

		EGLint status = m_ext.waitSync(m_eglDisplay, m_sync, 0xFFFFFFFF);
		log << TestLog::Message << status << " = eglWaitSyncKHR(" << m_eglDisplay << ", " << m_sync << ", 0xFFFFFFFF)" << TestLog::EndMessage;

		EGLint error = eglGetError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_PARAMETER)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) << "' expected EGL_BAD_PARAMETER" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return STOP;
		}

		TCU_CHECK(status == EGL_FALSE);

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

} // anonymous

FenceSyncTests::FenceSyncTests (EglTestContext& eglTestCtx)
	: TestCaseGroup	(eglTestCtx, "fence_sync", "EGL_KHR_fence_sync extension tests")
{
}

void FenceSyncTests::init (void)
{
	// Add valid API test
	{
		TestCaseGroup* const valid = new TestCaseGroup(m_eglTestCtx, "valid", "Valid function calls");

		// eglCreateSyncKHR tests
		valid->addChild(new CreateNullAttribsTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));
		valid->addChild(new CreateEmptyAttribsTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));

		// eglClientWaitSyncKHR tests
		valid->addChild(new ClientWaitNoTimeoutTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));
		valid->addChild(new ClientWaitForeverTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));
		valid->addChild(new ClientWaitNoContextTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));
		valid->addChild(new ClientWaitForeverFlushTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));

		// eglGetSyncAttribKHR tests
		valid->addChild(new GetSyncTypeTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));
		valid->addChild(new GetSyncStatusTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));
		valid->addChild(new GetSyncStatusSignaledTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));
		valid->addChild(new GetSyncConditionTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));

		// eglDestroySyncKHR tests
		valid->addChild(new DestroySyncTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));

		// eglWaitSyncKHR tests
		valid->addChild(new WaitSyncTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));

		addChild(valid);
	}

	// Add negative API tests
	{
		TestCaseGroup* const invalid = new TestCaseGroup(m_eglTestCtx, "invalid", "Invalid function calls");

		// eglCreateSyncKHR tests
		invalid->addChild(new CreateInvalidDisplayTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));
		invalid->addChild(new CreateInvalidTypeTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));
		invalid->addChild(new CreateInvalidAttribsTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));
		invalid->addChild(new CreateInvalidContextTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));

		// eglClientWaitSyncKHR tests
		invalid->addChild(new ClientWaitInvalidDisplayTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));
		invalid->addChild(new ClientWaitInvalidSyncTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));
		invalid->addChild(new ClientWaitInvalidFlagTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));

		// eglGetSyncAttribKHR tests
		invalid->addChild(new GetSyncInvalidDisplayTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));
		invalid->addChild(new GetSyncInvalidSyncTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));
		invalid->addChild(new GetSyncInvalidAttributeTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));
		invalid->addChild(new GetSyncInvalidValueTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));

		// eglDestroySyncKHR tests
		invalid->addChild(new DestroySyncInvalidDislayTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));
		invalid->addChild(new DestroySyncInvalidSyncTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));

		// eglWaitSyncKHR tests
		invalid->addChild(new WaitSyncInvalidDisplayTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));
		invalid->addChild(new WaitSyncInvalidSyncTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));
		invalid->addChild(new WaitSyncInvalidFlagTest(m_eglTestCtx, EGL_SYNC_FENCE_KHR));

		addChild(invalid);
	}
}

ReusableSyncTests::ReusableSyncTests (EglTestContext& eglTestCtx)
	: TestCaseGroup	(eglTestCtx, "reusable_sync", "EGL_KHR_reusable_sync extension tests")
{
}

void ReusableSyncTests::init (void)
{
	// Add valid API test
	{
		TestCaseGroup* const valid = new TestCaseGroup(m_eglTestCtx, "valid", "Valid function calls");

		// eglCreateSyncKHR tests
		valid->addChild(new CreateNullAttribsTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));
		valid->addChild(new CreateEmptyAttribsTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));

		// eglClientWaitSyncKHR tests
		valid->addChild(new ClientWaitNoTimeoutTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));
		valid->addChild(new ClientWaitForeverTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));
		valid->addChild(new ClientWaitNoContextTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));
		valid->addChild(new ClientWaitForeverFlushTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));

		// eglGetSyncAttribKHR tests
		valid->addChild(new GetSyncTypeTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));
		valid->addChild(new GetSyncStatusTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));
		valid->addChild(new GetSyncStatusSignaledTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));

		// eglDestroySyncKHR tests
		valid->addChild(new DestroySyncTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));

		// eglWaitSyncKHR tests
		valid->addChild(new WaitSyncTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));

		addChild(valid);
	}

	// Add negative API tests
	{
		TestCaseGroup* const invalid = new TestCaseGroup(m_eglTestCtx, "invalid", "Invalid function calls");

		// eglCreateSyncKHR tests
		invalid->addChild(new CreateInvalidDisplayTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));
		invalid->addChild(new CreateInvalidTypeTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));
		invalid->addChild(new CreateInvalidAttribsTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));
		invalid->addChild(new CreateInvalidContextTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));

		// eglClientWaitSyncKHR tests
		invalid->addChild(new ClientWaitInvalidDisplayTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));
		invalid->addChild(new ClientWaitInvalidSyncTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));
		invalid->addChild(new ClientWaitInvalidFlagTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));

		// eglGetSyncAttribKHR tests
		invalid->addChild(new GetSyncInvalidDisplayTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));
		invalid->addChild(new GetSyncInvalidSyncTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));
		invalid->addChild(new GetSyncInvalidAttributeTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));
		invalid->addChild(new GetSyncInvalidValueTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));

		// eglDestroySyncKHR tests
		invalid->addChild(new DestroySyncInvalidDislayTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));
		invalid->addChild(new DestroySyncInvalidSyncTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));

		// eglWaitSyncKHR tests
		invalid->addChild(new WaitSyncInvalidDisplayTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));
		invalid->addChild(new WaitSyncInvalidSyncTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));
		invalid->addChild(new WaitSyncInvalidFlagTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));

		addChild(invalid);
	}
}

} // egl
} // deqp
