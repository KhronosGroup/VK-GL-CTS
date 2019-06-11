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

#include "deStringUtil.hpp"

#include "egluNativeWindow.hpp"
#include "egluStrUtil.hpp"
#include "egluUtil.hpp"

#include "eglwLibrary.hpp"
#include "eglwEnums.hpp"

#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"

#include "gluDefs.hpp"

#include "glwFunctions.hpp"
#include "glwEnums.hpp"

#include <vector>
#include <string>
#include <sstream>
#include <set>

using std::vector;
using std::string;
using std::set;

using tcu::TestLog;

using namespace eglw;
using namespace glw;

namespace deqp
{
namespace egl
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
	typedef EGLSync     (Library::*createSync)(EGLDisplay, EGLenum, const EGLAttrib *) const ;
	typedef EGLSyncKHR  (Library::*createSyncKHR)(EGLDisplay, EGLenum, const EGLint *) const ;
	typedef EGLint      (Library::*clientWaitSync)(EGLDisplay, EGLSync, EGLint, EGLTime) const ;
	typedef EGLint      (Library::*clientWaitSyncKHR)(EGLDisplay, EGLSyncKHR, EGLint, EGLTimeKHR) const ;
	typedef EGLBoolean  (Library::*getSyncAttrib)(EGLDisplay, EGLSync, EGLint, EGLAttrib *) const ;
	typedef EGLBoolean  (Library::*getSyncAttribKHR)(EGLDisplay, EGLSyncKHR, EGLint, EGLint *) const ;
	typedef EGLBoolean  (Library::*destroySync)(EGLDisplay, EGLSync) const ;
	typedef EGLBoolean  (Library::*destroySyncKHR)(EGLDisplay, EGLSyncKHR) const ;
	typedef EGLBoolean  (Library::*waitSync)(EGLDisplay, EGLSync, EGLint) const ;
	typedef EGLint      (Library::*waitSyncKHR)(EGLDisplay, EGLSyncKHR, EGLint) const ;

	enum FunctionName
	{
		FUNC_NAME_CREATE_SYNC,
		FUNC_NAME_CLIENT_WAIT_SYNC,
		FUNC_NAME_GET_SYNC_ATTRIB,
		FUNC_NAME_DESTROY_SYNC,
		FUNC_NAME_WAIT_SYNC,
		FUNC_NAME_NUM_NAMES
	};

	enum Extension
	{
		EXTENSION_NONE				= 0,
		EXTENSION_WAIT_SYNC			= (0x1 << 0),
		EXTENSION_FENCE_SYNC		= (0x1 << 1),
		EXTENSION_REUSABLE_SYNC		= (0x1 << 2)
	};
									SyncTest	(EglTestContext& eglTestCtx, EGLenum syncType, Extension extensions, bool useCurrentContext, const char* name, const char* description);
									virtual ~SyncTest	(void);

	void							init		(void);
	void							deinit		(void);
	bool							hasRequiredEGLVersion(int requiredMajor, int requiredMinor);
	bool							hasRequiredEGLExtensions(void);
	EGLDisplay						getEglDisplay()	{return m_eglDisplay;}

protected:
	const EGLenum					m_syncType;
	const bool						m_useCurrentContext;

	glw::Functions					m_gl;

	Extension						m_extensions;
	EGLDisplay						m_eglDisplay;
	EGLConfig						m_eglConfig;
	EGLSurface						m_eglSurface;
	eglu::NativeWindow*				m_nativeWindow;
	EGLContext						m_eglContext;
	EGLSyncKHR						m_sync;
	string							m_funcNames[FUNC_NAME_NUM_NAMES];
	string							m_funcNamesKHR[FUNC_NAME_NUM_NAMES];
};

SyncTest::SyncTest (EglTestContext& eglTestCtx, EGLenum syncType, Extension extensions,  bool useCurrentContext, const char* name, const char* description)
	: TestCase				(eglTestCtx, name, description)
	, m_syncType			(syncType)
	, m_useCurrentContext	(useCurrentContext)
	, m_extensions			(extensions)
	, m_eglDisplay			(EGL_NO_DISPLAY)
	, m_eglConfig           (((eglw::EGLConfig)0))  // EGL_NO_CONFIG
	, m_eglSurface			(EGL_NO_SURFACE)
	, m_nativeWindow		(DE_NULL)
	, m_eglContext			(EGL_NO_CONTEXT)
	, m_sync				(EGL_NO_SYNC_KHR)
{
	m_funcNames[FUNC_NAME_CREATE_SYNC] = "eglCreateSync";
	m_funcNames[FUNC_NAME_CLIENT_WAIT_SYNC] = "eglClientWaitSync";
	m_funcNames[FUNC_NAME_GET_SYNC_ATTRIB] = "eglGetSyncAttrib";
	m_funcNames[FUNC_NAME_DESTROY_SYNC] = "eglDestroySync";
	m_funcNames[FUNC_NAME_WAIT_SYNC] = "eglWaitSync";

	m_funcNamesKHR[FUNC_NAME_CREATE_SYNC] = "eglCreateSyncKHR";
	m_funcNamesKHR[FUNC_NAME_CLIENT_WAIT_SYNC] = "eglClientWaitSyncKHR";
	m_funcNamesKHR[FUNC_NAME_GET_SYNC_ATTRIB] = "eglGetSyncAttribKHR";
	m_funcNamesKHR[FUNC_NAME_DESTROY_SYNC] = "eglDestroySyncKHR";
	m_funcNamesKHR[FUNC_NAME_WAIT_SYNC] = "eglWaitSyncKHR";
}

SyncTest::~SyncTest (void)
{
	SyncTest::deinit();
}

bool SyncTest::hasRequiredEGLVersion (int requiredMajor, int requiredMinor)
{
	const Library&	egl		= m_eglTestCtx.getLibrary();
	TestLog&		log		= m_testCtx.getLog();
	eglu::Version	version	= eglu::getVersion(egl, m_eglDisplay);

	if (version < eglu::Version(requiredMajor, requiredMinor))
	{
		log << TestLog::Message << "Required EGL version is not supported. "
			"Has: " << version.getMajor() << "." << version.getMinor()
			<< ", Required: " << requiredMajor << "." << requiredMinor << TestLog::EndMessage;
		return false;
	}

	return true;
}

bool SyncTest::hasRequiredEGLExtensions (void)
{
	TestLog&		log	= m_testCtx.getLog();

	if (!eglu::hasExtension(m_eglTestCtx.getLibrary(), m_eglDisplay, "EGL_KHR_fence_sync"))
	{
		log << TestLog::Message << "EGL_KHR_fence_sync not supported" << TestLog::EndMessage;
		return false;
	}

	if (!eglu::hasExtension(m_eglTestCtx.getLibrary(), m_eglDisplay, "EGL_KHR_reusable_sync"))
	{
		log << TestLog::Message << "EGL_KHR_reusable_sync not supported" << TestLog::EndMessage;
		return false;
	}

	if (!eglu::hasExtension(m_eglTestCtx.getLibrary(), m_eglDisplay, "EGL_KHR_wait_sync"))
	{
		log << TestLog::Message << "EGL_KHR_wait_sync not supported" << TestLog::EndMessage;
		return false;
	}

	return true;
}

void requiredGLESExtensions (const glw::Functions& gl)
{
	bool				found = false;
	std::istringstream	extensionStream((const char*)gl.getString(GL_EXTENSIONS));
	string				extension;

	GLU_CHECK_GLW_MSG(gl, "glGetString(GL_EXTENSIONS)");

	while (std::getline(extensionStream, extension, ' '))
	{
		if (extension == "GL_OES_EGL_sync")
			found = true;
	}

	if (!found)
		TCU_THROW(NotSupportedError, "GL_OES_EGL_sync not supported");
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
	const Library&						egl				= m_eglTestCtx.getLibrary();
	const eglu::NativeWindowFactory&	windowFactory	= eglu::selectNativeWindowFactory(m_eglTestCtx.getNativeDisplayFactory(), m_testCtx.getCommandLine());

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

	m_eglDisplay	= eglu::getAndInitDisplay(m_eglTestCtx.getNativeDisplay());
	m_eglConfig		= eglu::chooseSingleConfig(egl, m_eglDisplay, displayAttribList);

	m_eglTestCtx.initGLFunctions(&m_gl, glu::ApiType::es(2,0));

	m_extensions = (Extension)(m_extensions | getSyncTypeExtension(m_syncType));

	if (m_useCurrentContext)
	{
		// Create context
		EGLU_CHECK_CALL(egl, bindAPI(EGL_OPENGL_ES_API));
		m_eglContext = egl.createContext(m_eglDisplay, m_eglConfig, EGL_NO_CONTEXT, contextAttribList);
		EGLU_CHECK_MSG(egl, "Failed to create GLES2 context");

		// Create surface
		m_nativeWindow = windowFactory.createWindow(&m_eglTestCtx.getNativeDisplay(), m_eglDisplay, m_eglConfig, DE_NULL, eglu::WindowParams(480, 480, eglu::parseWindowVisibility(m_testCtx.getCommandLine())));
		m_eglSurface = eglu::createWindowSurface(m_eglTestCtx.getNativeDisplay(), *m_nativeWindow, m_eglDisplay, m_eglConfig, DE_NULL);

		EGLU_CHECK_CALL(egl, makeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext));

		requiredGLESExtensions(m_gl);
	}

	// Verify EXTENSION_REUSABLE_SYNC is supported before running the tests
	if (m_syncType == EGL_SYNC_REUSABLE_KHR) {
		if (!eglu::hasExtension(m_eglTestCtx.getLibrary(), m_eglDisplay, "EGL_KHR_reusable_sync"))
		{
			TCU_THROW(NotSupportedError, "EGL_KHR_reusable_sync not supported");
		}
	}
}

void SyncTest::deinit (void)
{
	const Library&	egl		= m_eglTestCtx.getLibrary();

	if (m_eglDisplay != EGL_NO_DISPLAY)
	{
		if (m_sync != EGL_NO_SYNC_KHR)
		{
			EGLU_CHECK_CALL(egl, destroySyncKHR(m_eglDisplay, m_sync));
			m_sync = EGL_NO_SYNC_KHR;
		}

		EGLU_CHECK_CALL(egl, makeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

		if (m_eglContext != EGL_NO_CONTEXT)
		{
			EGLU_CHECK_CALL(egl, destroyContext(m_eglDisplay, m_eglContext));
			m_eglContext = EGL_NO_CONTEXT;
		}

		if (m_eglSurface != EGL_NO_SURFACE)
		{
			EGLU_CHECK_CALL(egl, destroySurface(m_eglDisplay, m_eglSurface));
			m_eglSurface = EGL_NO_SURFACE;
		}

		delete m_nativeWindow;
		m_nativeWindow = DE_NULL;

		egl.terminate(m_eglDisplay);
		m_eglDisplay = EGL_NO_DISPLAY;
	}
}

class CreateNullAttribsTest : public SyncTest
{
public:
					CreateNullAttribsTest	(EglTestContext& eglTestCtx, EGLenum syncType)
		: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, syncType != EGL_SYNC_REUSABLE_KHR, "create_null_attribs", "create_null_attribs")
	{
	}

	template <typename createSyncFuncType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  createSyncFuncType createSyncFunc)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl		= m_eglTestCtx.getLibrary();
		TestLog&		log		= m_testCtx.getLog();
		string			msgChk	= funcNames[FUNC_NAME_CREATE_SYNC] + "()";

		m_sync = (egl.*createSyncFunc)(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = " << funcNames[FUNC_NAME_CREATE_SYNC] << "(" <<
			m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" <<
			TestLog::EndMessage;
		EGLU_CHECK_MSG(egl, msgChk.c_str());
	}

	IterateResult iterate(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<createSync>(m_funcNames, &Library::createSync);
		}
		if (hasRequiredEGLExtensions())
		{
			test<createSyncKHR>(m_funcNamesKHR, &Library::createSyncKHR);
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}
};

class CreateEmptyAttribsTest : public SyncTest
{
public:
					CreateEmptyAttribsTest	(EglTestContext& eglTestCtx, EGLenum syncType)
		: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, syncType != EGL_SYNC_REUSABLE_KHR,  "create_empty_attribs", "create_empty_attribs")
	{
	}

	template <typename createSyncFuncType, typename attribType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  createSyncFuncType createSyncFunc)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	    egl				= m_eglTestCtx.getLibrary();
		TestLog&		    log				= m_testCtx.getLog();
		string              msgChk          = funcNames[FUNC_NAME_CREATE_SYNC] + "()";
		const attribType    attribList[]	=
		{
			EGL_NONE
		};

		m_sync = (egl.*createSyncFunc)(m_eglDisplay, m_syncType, attribList);
		log << TestLog::Message << m_sync << " = " << funcNames[FUNC_NAME_CREATE_SYNC] <<
			"(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) <<
			", { EGL_NONE })" << TestLog::EndMessage;
		EGLU_CHECK_MSG(egl, msgChk.c_str());
	}

	IterateResult iterate (void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<createSync, EGLAttrib>(m_funcNames, &Library::createSync);
		}
		if (hasRequiredEGLExtensions())
		{
			test<createSyncKHR, EGLint>(m_funcNamesKHR, &Library::createSyncKHR);
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}
};

class CreateInvalidDisplayTest : public SyncTest
{
public:
					CreateInvalidDisplayTest	(EglTestContext& eglTestCtx, EGLenum syncType)
		: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, syncType != EGL_SYNC_REUSABLE_KHR,  "create_invalid_display", "create_invalid_display")
	{
	}

	template <typename createSyncFuncType, typename syncType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  createSyncFuncType createSyncFunc, syncType eglNoSync)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl		= m_eglTestCtx.getLibrary();
		TestLog&		log		= m_testCtx.getLog();

		m_sync = (egl.*createSyncFunc)(EGL_NO_DISPLAY, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = " << funcNames[FUNC_NAME_CREATE_SYNC] <<
			"(EGL_NO_DISPLAY, " << getSyncTypeName(m_syncType) << ", NULL)" <<
			TestLog::EndMessage;

		EGLint error = egl.getError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_DISPLAY)
		{
			log << TestLog::Message << "Unexpected error '" <<
				eglu::getErrorStr(error) << "' expected EGL_BAD_DISPLAY" <<
				TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return;
		}

		TCU_CHECK(m_sync == eglNoSync);
	};

	IterateResult	iterate						(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<createSync, EGLSync>(m_funcNames, &Library::createSync, EGL_NO_SYNC);
		}
		if (hasRequiredEGLExtensions())
		{
			test<createSyncKHR, EGLSyncKHR>(m_funcNamesKHR, &Library::createSyncKHR, EGL_NO_SYNC_KHR);
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}
};

class CreateInvalidTypeTest : public SyncTest
{
public:
					CreateInvalidTypeTest	(EglTestContext& eglTestCtx, EGLenum syncType)
		: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, syncType != EGL_SYNC_REUSABLE_KHR,  "create_invalid_type", "create_invalid_type")
	{
	}

	template <typename createSyncFuncType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  createSyncFuncType createSyncFunc, EGLSyncKHR eglNoSync,
			  EGLint syncError, string syncErrorName)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl		= m_eglTestCtx.getLibrary();
		TestLog&		log		= m_testCtx.getLog();

		m_sync = (egl.*createSyncFunc)(m_eglDisplay, EGL_NONE, NULL);
		log << TestLog::Message << m_sync << " = " << funcNames[FUNC_NAME_CREATE_SYNC] << "(" <<
			m_eglDisplay << ", EGL_NONE, NULL)" << TestLog::EndMessage;

		EGLint error = egl.getError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != syncError)
		{
			log << TestLog::Message << "Unexpected error '" <<
				eglu::getErrorStr(error) << "' expected " << syncErrorName << " " <<
				TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return;
		}

		TCU_CHECK(m_sync == eglNoSync);
	}

	IterateResult	iterate					(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<createSync>(m_funcNames, &Library::createSync, EGL_NO_SYNC,
							 EGL_BAD_PARAMETER, "EGL_BAD_PARAMETER");
		}
		if (hasRequiredEGLExtensions())
		{
			test<createSyncKHR>(m_funcNamesKHR, &Library::createSyncKHR, EGL_NO_SYNC_KHR,
								EGL_BAD_ATTRIBUTE, "EGL_BAD_ATTRIBUTE");
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}
};

class CreateInvalidAttribsTest : public SyncTest
{
public:
					CreateInvalidAttribsTest	(EglTestContext& eglTestCtx, EGLenum syncType)
		: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, syncType != EGL_SYNC_REUSABLE_KHR,  "create_invalid_attribs", "create_invalid_attribs")
	{
	}

	template <typename createSyncFuncType, typename attribType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  createSyncFuncType createSyncFunc, EGLSyncKHR eglNoSync)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl		= m_eglTestCtx.getLibrary();
		TestLog&		log		= m_testCtx.getLog();

		attribType attribs[] = {
			2, 3, 4, 5,
			EGL_NONE
		};

		m_sync = (egl.*createSyncFunc)(m_eglDisplay, m_syncType, attribs);
		log << TestLog::Message << m_sync << " = " << funcNames[FUNC_NAME_CREATE_SYNC] <<
			"(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) <<
			", { 2, 3, 4, 5, EGL_NONE })" << TestLog::EndMessage;

		EGLint error = egl.getError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_ATTRIBUTE)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) <<
				"' expected EGL_BAD_ATTRIBUTE" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return;
		}

		TCU_CHECK(m_sync == eglNoSync);
	}

	IterateResult	iterate						(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<createSync, EGLAttrib>(m_funcNames, &Library::createSync, EGL_NO_SYNC);
		}
		if (hasRequiredEGLExtensions())
		{
			test<createSyncKHR, EGLint>(m_funcNamesKHR, &Library::createSyncKHR, EGL_NO_SYNC_KHR);
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}
};

class CreateInvalidContextTest : public SyncTest
{
public:
					CreateInvalidContextTest	(EglTestContext& eglTestCtx, EGLenum syncType)
		: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, syncType != EGL_SYNC_REUSABLE_KHR,  "create_invalid_context", "create_invalid_context")
	{
	}

	template <typename createSyncFuncType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  createSyncFuncType createSyncFunc, EGLSyncKHR eglNoSync)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl		= m_eglTestCtx.getLibrary();
		TestLog&		log		= m_testCtx.getLog();

		log << TestLog::Message << "eglMakeCurrent(" << m_eglDisplay <<
			", EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)" << TestLog::EndMessage;
		EGLU_CHECK_CALL(egl, makeCurrent(m_eglDisplay, EGL_NO_SURFACE,
										 EGL_NO_SURFACE, EGL_NO_CONTEXT));

		m_sync = (egl.*createSyncFunc)(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = " << funcNames[FUNC_NAME_CREATE_SYNC] <<
			"(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" <<
			TestLog::EndMessage;

		EGLint error = egl.getError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_MATCH)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) <<
				"' expected EGL_BAD_MATCH" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return;
		}

		TCU_CHECK(m_sync == eglNoSync);
	};

	IterateResult	iterate						(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<createSync>(m_funcNames, &Library::createSync, EGL_NO_SYNC);
		}
		if (hasRequiredEGLExtensions())
		{
			test<createSyncKHR>(m_funcNamesKHR, &Library::createSyncKHR, EGL_NO_SYNC_KHR);
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}
};

class ClientWaitNoTimeoutTest : public SyncTest
{
public:
					ClientWaitNoTimeoutTest	(EglTestContext& eglTestCtx, EGLenum syncType)
		: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, syncType != EGL_SYNC_REUSABLE_KHR,  "wait_no_timeout", "wait_no_timeout")
	{
	}

	template <typename createSyncFuncType, typename clientWaitSyncFuncType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  createSyncFuncType createSyncFunc,
			  clientWaitSyncFuncType clientWaitSyncFunc)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl		= m_eglTestCtx.getLibrary();
		TestLog&		log		= m_testCtx.getLog();
		string          msgChk  = funcNames[FUNC_NAME_CREATE_SYNC] + "()";

		m_sync = (egl.*createSyncFunc)(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = " << funcNames[FUNC_NAME_CREATE_SYNC] <<
			"(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" <<
			TestLog::EndMessage;
		EGLU_CHECK_MSG(egl, msgChk.c_str());

		EGLint status = (egl.*clientWaitSyncFunc)(m_eglDisplay, m_sync, 0, 0);
		log << TestLog::Message << status << " = " << funcNames[FUNC_NAME_CLIENT_WAIT_SYNC] <<
			"(" << m_eglDisplay << ", " << m_sync << ", 0, 0)" << TestLog::EndMessage;

		if (m_syncType == EGL_SYNC_FENCE_KHR)
			TCU_CHECK(status == EGL_CONDITION_SATISFIED_KHR || status == EGL_TIMEOUT_EXPIRED_KHR);
		else if (m_syncType == EGL_SYNC_REUSABLE_KHR)
			TCU_CHECK(status == EGL_TIMEOUT_EXPIRED_KHR);
		else
			DE_ASSERT(DE_FALSE);
	}

	IterateResult	iterate					(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<createSync, clientWaitSync>(m_funcNames, &Library::createSync,
											 &Library::clientWaitSync);
		}
		if (hasRequiredEGLExtensions())
		{
			test<createSyncKHR, clientWaitSyncKHR>(m_funcNamesKHR, &Library::createSyncKHR,
												   &Library::clientWaitSyncKHR);
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}

};

class ClientWaitForeverTest : public SyncTest
{
public:
					ClientWaitForeverTest	(EglTestContext& eglTestCtx, EGLenum syncType)
	: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, syncType != EGL_SYNC_REUSABLE_KHR, "wait_forever", "wait_forever")
	{
	}

	template <typename createSyncFuncType, typename clientWaitSyncFuncType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  createSyncFuncType createSyncFunc,
			  clientWaitSyncFuncType clientWaitSyncFunc,
			  EGLTime eglTime, const string &eglTimeName,
			  EGLint condSatisfied)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl		                = m_eglTestCtx.getLibrary();
		TestLog&		log		                = m_testCtx.getLog();
		string          createSyncMsgChk        = funcNames[FUNC_NAME_CREATE_SYNC] + "()";
		string          clientWaitSyncMsgChk    = funcNames[FUNC_NAME_CLIENT_WAIT_SYNC] + "()";

		m_sync = (egl.*createSyncFunc)(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = " << funcNames[FUNC_NAME_CREATE_SYNC] <<
			"(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" <<
			TestLog::EndMessage;
		EGLU_CHECK_MSG(egl, createSyncMsgChk.c_str());

		if (m_syncType == EGL_SYNC_REUSABLE_KHR)
		{
			EGLBoolean ret = egl.signalSyncKHR(m_eglDisplay, m_sync, EGL_SIGNALED_KHR);
			log << TestLog::Message << ret << " = eglSignalSyncKHR(" <<
				m_eglDisplay << ", " << m_sync << ", EGL_SIGNALED_KHR)" <<
				TestLog::EndMessage;
			EGLU_CHECK_MSG(egl, "eglSignalSyncKHR()");
		}
		else if (m_syncType == EGL_SYNC_FENCE_KHR)
		{
			GLU_CHECK_GLW_CALL(m_gl, flush());
			log << TestLog::Message << "glFlush()" << TestLog::EndMessage;
		}
		else
			DE_ASSERT(DE_FALSE);

		EGLint status = (egl.*clientWaitSyncFunc)(m_eglDisplay, m_sync, 0, eglTime);
		log << TestLog::Message << status << " = " << funcNames[FUNC_NAME_CLIENT_WAIT_SYNC] <<
			"(" << m_eglDisplay << ", " << m_sync << ", 0, " << eglTimeName << ")" <<
			TestLog::EndMessage;

		TCU_CHECK(status == condSatisfied);
		EGLU_CHECK_MSG(egl, clientWaitSyncMsgChk.c_str());
	};

	IterateResult	iterate					(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<createSync, clientWaitSync>(m_funcNames, &Library::createSync,
											 &Library::clientWaitSync,
											 EGL_FOREVER, "EGL_FOREVER",
											 EGL_CONDITION_SATISFIED);
		}
		if (hasRequiredEGLExtensions())
		{
			test<createSyncKHR, clientWaitSyncKHR>(m_funcNamesKHR, &Library::createSyncKHR,
												   &Library::clientWaitSyncKHR,
												   EGL_FOREVER_KHR, "EGL_FOREVER_KHR",
												   EGL_CONDITION_SATISFIED_KHR);
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}
};

class ClientWaitNoContextTest : public SyncTest
{
public:
					ClientWaitNoContextTest	(EglTestContext& eglTestCtx, EGLenum syncType)
		: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, syncType != EGL_SYNC_REUSABLE_KHR, "wait_no_context", "wait_no_Context")
	{
	}

	template <typename createSyncFuncType, typename clientWaitSyncFuncType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  createSyncFuncType createSyncFunc,
			  clientWaitSyncFuncType clientWaitSyncFunc,
			  EGLint condSatisfied, EGLTime eglTime, const string &eglTimeName)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl		          = m_eglTestCtx.getLibrary();
		TestLog&		log		          = m_testCtx.getLog();
		string          createSyncMsgChk  = funcNames[FUNC_NAME_CREATE_SYNC] + "()";

		m_sync = (egl.*createSyncFunc)(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = " << funcNames[FUNC_NAME_CREATE_SYNC] <<
			"(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" <<
			TestLog::EndMessage;
		EGLU_CHECK_MSG(egl, createSyncMsgChk.c_str());


		if (m_syncType == EGL_SYNC_REUSABLE_KHR)
		{
			EGLBoolean ret = egl.signalSyncKHR(m_eglDisplay, m_sync, EGL_SIGNALED_KHR);
			log << TestLog::Message << ret << " = eglSignalSyncKHR(" <<
				m_eglDisplay << ", " << m_sync << ", EGL_SIGNALED_KHR)" <<
				TestLog::EndMessage;
			EGLU_CHECK_MSG(egl, "eglSignalSyncKHR()");
		}
		else if (m_syncType == EGL_SYNC_FENCE_KHR)
		{
			GLU_CHECK_GLW_CALL(m_gl, flush());
			log << TestLog::Message << "glFlush()" << TestLog::EndMessage;
		}
		else
			DE_ASSERT(DE_FALSE);

		log << TestLog::Message << "eglMakeCurrent(" << m_eglDisplay <<
			", EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)" << TestLog::EndMessage;
		EGLU_CHECK_CALL(egl, makeCurrent(m_eglDisplay, EGL_NO_SURFACE,
										 EGL_NO_SURFACE, EGL_NO_CONTEXT));

		EGLint result = (egl.*clientWaitSyncFunc)(m_eglDisplay, m_sync, 0, eglTime);
		log << TestLog::Message << result << " = " << funcNames[FUNC_NAME_CLIENT_WAIT_SYNC] <<
			"(" << m_eglDisplay << ", " << m_sync << ", 0, " << eglTimeName << ")" <<
			TestLog::EndMessage;

		TCU_CHECK(result == condSatisfied);
	};

	IterateResult	iterate					(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<createSync, clientWaitSync>(m_funcNames, &Library::createSync,
											 &Library::clientWaitSync,
											 EGL_CONDITION_SATISFIED, EGL_FOREVER, "EGL_FOREVER");
		}
		if (hasRequiredEGLExtensions())
		{
			test<createSyncKHR, clientWaitSyncKHR>(m_funcNamesKHR, &Library::createSyncKHR,
												   &Library::clientWaitSyncKHR,
												   EGL_CONDITION_SATISFIED_KHR, EGL_FOREVER_KHR, "EGL_FOREVER_KHR");
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}
};

class ClientWaitForeverFlushTest : public SyncTest
{
public:
					ClientWaitForeverFlushTest	(EglTestContext& eglTestCtx, EGLenum syncType)
		: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, syncType != EGL_SYNC_REUSABLE_KHR, "wait_forever_flush", "wait_forever_flush")
	{
	}

	template <typename createSyncFuncType, typename clientWaitSyncFuncType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  createSyncFuncType createSyncFunc,
			  clientWaitSyncFuncType clientWaitSyncFunc,
			  EGLint flags, const string &flagsName,
			  EGLTime eglTime, const string &eglTimeName,
			  EGLint condSatisfied)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl		            = m_eglTestCtx.getLibrary();
		TestLog&		log		            = m_testCtx.getLog();
		string          createSyncMsgChk    = funcNames[FUNC_NAME_CREATE_SYNC] + "()";

		m_sync = (egl.*createSyncFunc)(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = " << funcNames[FUNC_NAME_CREATE_SYNC] <<
			"(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" <<
			TestLog::EndMessage;
		EGLU_CHECK_MSG(egl, createSyncMsgChk.c_str());

		if (m_syncType == EGL_SYNC_REUSABLE_KHR)
		{
			EGLBoolean ret = egl.signalSyncKHR(m_eglDisplay, m_sync, EGL_SIGNALED_KHR);
			log << TestLog::Message << ret << " = eglSignalSyncKHR(" <<
				m_eglDisplay << ", " << m_sync << ", EGL_SIGNALED_KHR)" <<
				TestLog::EndMessage;
			EGLU_CHECK_MSG(egl, "eglSignalSyncKHR()");
		}

		EGLint status = (egl.*clientWaitSyncFunc)(m_eglDisplay, m_sync, flags, eglTime);
		log << TestLog::Message << status << " = " << funcNames[FUNC_NAME_CLIENT_WAIT_SYNC] <<
			"(" << m_eglDisplay << ", " << m_sync << ", " << flagsName << ", " <<
			eglTimeName << ")" << TestLog::EndMessage;

		TCU_CHECK(status == condSatisfied);

	}

	IterateResult	iterate						(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<createSync, clientWaitSync>(m_funcNames, &Library::createSync,
											 &Library::clientWaitSync,
											 EGL_SYNC_FLUSH_COMMANDS_BIT, "EGL_SYNC_FLUSH_COMMANDS_BIT",
											 EGL_FOREVER, "EGL_FOREVER",
											 EGL_CONDITION_SATISFIED);
		}
		if (hasRequiredEGLExtensions())
		{
			test<createSyncKHR, clientWaitSyncKHR>(m_funcNamesKHR, &Library::createSyncKHR,
												   &Library::clientWaitSyncKHR,
												   EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, "EGL_SYNC_FLUSH_COMMANDS_BIT_KHR",
												   EGL_FOREVER_KHR, "EGL_FOREVER_KHR",
												   EGL_CONDITION_SATISFIED_KHR);
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}
};

class ClientWaitInvalidDisplayTest : public SyncTest
{
public:
					ClientWaitInvalidDisplayTest	(EglTestContext& eglTestCtx, EGLenum syncType)
		: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, syncType != EGL_SYNC_REUSABLE_KHR, "wait_invalid_display", "wait_invalid_display")
	{
	}

	template <typename createSyncFuncType, typename clientWaitSyncFuncType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  createSyncFuncType createSyncFunc,
			  clientWaitSyncFuncType clientWaitSyncFunc,
			  EGLint flags, const string &flagsName,
			  EGLTime eglTime, const string &eglTimeName)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl		            = m_eglTestCtx.getLibrary();
		TestLog&		log		            = m_testCtx.getLog();
		string          createSyncMsgChk    = funcNames[FUNC_NAME_CREATE_SYNC] + "()";

		m_sync = (egl.*createSyncFunc)(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = " << funcNames[FUNC_NAME_CREATE_SYNC] << "(" <<
			m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" <<
			TestLog::EndMessage;
		EGLU_CHECK_MSG(egl, createSyncMsgChk.c_str());

		EGLint status = (egl.*clientWaitSyncFunc)(EGL_NO_DISPLAY, m_sync, flags, eglTime);
		log << TestLog::Message << status << " = " << funcNames[FUNC_NAME_CLIENT_WAIT_SYNC] <<
			"(EGL_NO_DISPLAY, " << m_sync << ", " << flagsName << ", " <<
			eglTimeName << ")" << TestLog::EndMessage;

		EGLint error = egl.getError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_DISPLAY)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) <<
				"' expected EGL_BAD_DISPLAY" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return;
		}

		TCU_CHECK(status == EGL_FALSE);
	}

	IterateResult	iterate							(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<createSync, clientWaitSync>(m_funcNames, &Library::createSync,
											 &Library::clientWaitSync,
											 EGL_SYNC_FLUSH_COMMANDS_BIT, "EGL_SYNC_FLUSH_COMMANDS_BIT",
											 EGL_FOREVER, "EGL_FOREVER");
		}
		if (hasRequiredEGLExtensions())
		{
			test<createSyncKHR, clientWaitSyncKHR>(m_funcNamesKHR, &Library::createSyncKHR,
												   &Library::clientWaitSyncKHR,
												   EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, "EGL_SYNC_FLUSH_COMMANDS_BIT_KHR",
												   EGL_FOREVER_KHR, "EGL_FOREVER_KHR");
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}
};

class ClientWaitInvalidSyncTest : public SyncTest
{
public:
					ClientWaitInvalidSyncTest	(EglTestContext& eglTestCtx, EGLenum syncType)
		: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, syncType != EGL_SYNC_REUSABLE_KHR, "wait_invalid_sync", "wait_invalid_sync")
	{
	}

	template <typename clientWaitSyncFuncType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  clientWaitSyncFuncType clientWaitSyncFunc,
			  EGLSync sync, const string &syncName,
			  EGLTime eglTime, const string &eglTimeName)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl		= m_eglTestCtx.getLibrary();
		TestLog&		log		= m_testCtx.getLog();

		EGLint status = (egl.*clientWaitSyncFunc)(m_eglDisplay, sync, 0, eglTime);
		log << TestLog::Message << status << " = " << funcNames[FUNC_NAME_CLIENT_WAIT_SYNC] <<
			"(" << m_eglDisplay << ", " << syncName << ", 0, " << eglTimeName << ")" <<
			TestLog::EndMessage;

		EGLint error = egl.getError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_PARAMETER)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) <<
				"' expected EGL_BAD_PARAMETER" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return;
		}

		TCU_CHECK(status == EGL_FALSE);
	}

	IterateResult	iterate						(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<clientWaitSync>(m_funcNames, &Library::clientWaitSync,
								 EGL_NO_SYNC, "EGL_NO_SYNC",
								 EGL_FOREVER, "EGL_FOREVER");
		}
		if (hasRequiredEGLExtensions())
		{
			test<clientWaitSyncKHR>(m_funcNamesKHR, &Library::clientWaitSyncKHR,
									EGL_NO_SYNC_KHR, "EGL_NO_SYNC_KHR",
									EGL_FOREVER_KHR, "EGL_FOREVER_KHR");
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}
};

class GetSyncTypeTest : public SyncTest
{
public:
					GetSyncTypeTest	(EglTestContext& eglTestCtx, EGLenum syncType)
		: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, syncType != EGL_SYNC_REUSABLE_KHR, "get_type", "get_type")
	{
	}

	template <typename createSyncFuncType, typename getSyncAttribFuncType, typename getSyncAttribValueType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  createSyncFuncType createSyncFunc,
			  getSyncAttribFuncType getSyncAttribFunc,
			  EGLint attribute, const string &attributeName)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl		            = m_eglTestCtx.getLibrary();
		TestLog&		log		            = m_testCtx.getLog();
		string          createSyncMsgChk    = funcNames[FUNC_NAME_CREATE_SYNC] + "()";

		m_sync = (egl.*createSyncFunc)(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = " << funcNames[FUNC_NAME_CREATE_SYNC] <<
			"(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" <<
			TestLog::EndMessage;
		EGLU_CHECK_MSG(egl, createSyncMsgChk.c_str());

		getSyncAttribValueType type = 0;
		EGLU_CHECK_CALL_FPTR(egl, (egl.*getSyncAttribFunc)(m_eglDisplay, m_sync,
														   attribute, &type));
		log << TestLog::Message << funcNames[FUNC_NAME_GET_SYNC_ATTRIB] << "(" << m_eglDisplay <<
			", " << m_sync << ", " << attributeName << ", {" << type << "})" <<
			TestLog::EndMessage;

		TCU_CHECK(type == ((getSyncAttribValueType)m_syncType));
	}

	IterateResult	iterate			(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<createSync, getSyncAttrib, EGLAttrib>(m_funcNames, &Library::createSync,
													   &Library::getSyncAttrib,
													   EGL_SYNC_TYPE, "EGL_SYNC_TYPE");
		}
		if (hasRequiredEGLExtensions())
		{
			test<createSyncKHR, getSyncAttribKHR, EGLint>(m_funcNamesKHR, &Library::createSyncKHR,
														  &Library::getSyncAttribKHR,
														  EGL_SYNC_TYPE_KHR, "EGL_SYNC_TYPE_KHR");
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}
};

class GetSyncStatusTest : public SyncTest
{
public:
					GetSyncStatusTest	(EglTestContext& eglTestCtx, EGLenum syncType)
		: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, syncType != EGL_SYNC_REUSABLE_KHR, "get_status", "get_status")
	{
	}

	template <typename createSyncFuncType, typename getSyncAttribFuncType, typename getSyncAttribValueType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  createSyncFuncType createSyncFunc,
			  getSyncAttribFuncType getSyncAttribFunc,
			  EGLint attribute, const string &attributeName)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl		            = m_eglTestCtx.getLibrary();
		TestLog&		log		            = m_testCtx.getLog();
		string          createSyncMsgChk    = funcNames[FUNC_NAME_CREATE_SYNC] + "()";

		m_sync = (egl.*createSyncFunc)(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = " << funcNames[FUNC_NAME_CREATE_SYNC] <<
			"(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" <<
			TestLog::EndMessage;
		EGLU_CHECK_MSG(egl, createSyncMsgChk.c_str());

		getSyncAttribValueType status = 0;
		EGLU_CHECK_CALL_FPTR(egl, (egl.*getSyncAttribFunc)(m_eglDisplay, m_sync, attribute, &status));
		log << TestLog::Message << funcNames[FUNC_NAME_GET_SYNC_ATTRIB] << "(" <<
			m_eglDisplay << ", " << m_sync << ", " << attributeName << ", {" <<
			status << "})" << TestLog::EndMessage;

		if (m_syncType == EGL_SYNC_FENCE_KHR)
			TCU_CHECK(status == EGL_SIGNALED_KHR || status == EGL_UNSIGNALED_KHR);
		else if (m_syncType == EGL_SYNC_REUSABLE_KHR)
			TCU_CHECK(status == EGL_UNSIGNALED_KHR);
	}

	IterateResult	iterate				(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<createSync, getSyncAttrib, EGLAttrib>(m_funcNames, &Library::createSync,
													   &Library::getSyncAttrib,
													   EGL_SYNC_STATUS, "EGL_SYNC_STATUS");
		}
		if (hasRequiredEGLExtensions())
		{
			test<createSyncKHR, getSyncAttribKHR, EGLint>(m_funcNamesKHR, &Library::createSyncKHR,
														  &Library::getSyncAttribKHR,
														  EGL_SYNC_STATUS_KHR, "EGL_SYNC_STATUS_KHR");
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}
};

class GetSyncStatusSignaledTest : public SyncTest
{
public:
					GetSyncStatusSignaledTest	(EglTestContext& eglTestCtx, EGLenum syncType)
		: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, syncType != EGL_SYNC_REUSABLE_KHR, "get_status_signaled", "get_status_signaled")
	{
	}

	template <typename createSyncFuncType,
		typename clientWaitSyncFuncType,
		typename getSyncAttribFuncType,
		typename getSyncAttribValueType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  createSyncFuncType createSyncFunc,
			  clientWaitSyncFuncType clientWaitSyncFunc,
			  EGLint flags, const string &flagsName,
			  EGLTime eglTime, const string &eglTimeName,
			  EGLint condSatisfied,
			  getSyncAttribFuncType getSyncAttribFunc,
			  EGLint attribute, const string &attributeName,
			  getSyncAttribValueType statusVal)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl		            = m_eglTestCtx.getLibrary();
		TestLog&		log		            = m_testCtx.getLog();
		string          createSyncMsgChk    = funcNames[FUNC_NAME_CREATE_SYNC] + "()";

		m_sync = (egl.*createSyncFunc)(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = " << funcNames[FUNC_NAME_CREATE_SYNC] <<
			"(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" <<
			TestLog::EndMessage;
		EGLU_CHECK_MSG(egl, createSyncMsgChk.c_str());

		if (m_syncType == EGL_SYNC_REUSABLE_KHR)
		{
			EGLBoolean ret = egl.signalSyncKHR(m_eglDisplay, m_sync, EGL_SIGNALED_KHR);
			log << TestLog::Message << ret << " = eglSignalSyncKHR(" <<
				m_eglDisplay << ", " << m_sync << ", EGL_SIGNALED_KHR)" <<
				TestLog::EndMessage;
			EGLU_CHECK_MSG(egl, "eglSignalSyncKHR()");
		}
		else if (m_syncType == EGL_SYNC_FENCE_KHR)
		{
			GLU_CHECK_GLW_CALL(m_gl, finish());
			log << TestLog::Message << "glFinish()" << TestLog::EndMessage;
		}
		else
			DE_ASSERT(DE_FALSE);

		{
			EGLint status = (egl.*clientWaitSyncFunc)(m_eglDisplay, m_sync, flags, eglTime);
			log << TestLog::Message << status << " = " << funcNames[FUNC_NAME_CLIENT_WAIT_SYNC] << "(" <<
				m_eglDisplay << ", " << m_sync << ", " << flagsName << ", " <<
				eglTimeName << ")" << TestLog::EndMessage;
			TCU_CHECK(status == condSatisfied);
		}

		getSyncAttribValueType status = 0;
		EGLU_CHECK_CALL_FPTR(egl, (egl.*getSyncAttribFunc)(m_eglDisplay, m_sync, attribute, &status));
		log << TestLog::Message << funcNames[FUNC_NAME_GET_SYNC_ATTRIB] << "(" <<
			m_eglDisplay << ", " << m_sync << ", " << attributeName << ", {" <<
			status << "})" << TestLog::EndMessage;

		TCU_CHECK(status == statusVal);
	}

	IterateResult	iterate						(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<createSync, clientWaitSync, getSyncAttrib, EGLAttrib>(m_funcNames,
																	   &Library::createSync,
																	   &Library::clientWaitSync,
																	   EGL_SYNC_FLUSH_COMMANDS_BIT, "EGL_SYNC_FLUSH_COMMANDS_BIT",
																	   EGL_FOREVER, "EGL_FOREVER",
																	   EGL_CONDITION_SATISFIED,
																	   &Library::getSyncAttrib,
																	   EGL_SYNC_STATUS, "EGL_SYNC_STATUS",
																	   EGL_SIGNALED);
		}
		if (hasRequiredEGLExtensions())
		{
			test<createSyncKHR, clientWaitSyncKHR, getSyncAttribKHR, EGLint>(m_funcNamesKHR,
																			 &Library::createSyncKHR,
																			 &Library::clientWaitSyncKHR,
																			 EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, "EGL_SYNC_FLUSH_COMMANDS_BIT_KHR",
																			 EGL_FOREVER_KHR, "EGL_FOREVER_KHR",
																			 EGL_CONDITION_SATISFIED_KHR,
																			 &Library::getSyncAttribKHR,
																			 EGL_SYNC_STATUS_KHR, "EGL_SYNC_STATUS_KHR",
																			 EGL_SIGNALED_KHR);
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}
};

class GetSyncConditionTest : public SyncTest
{
public:
					GetSyncConditionTest	(EglTestContext& eglTestCtx, EGLenum syncType)
		: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, syncType != EGL_SYNC_REUSABLE_KHR, "get_condition", "get_condition")
	{
	}

	template <typename createSyncFuncType,
		typename getSyncAttribFuncType,
		typename getSyncAttribValueType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  createSyncFuncType createSyncFunc,
			  getSyncAttribFuncType getSyncAttribFunc,
			  EGLint attribute, const string &attributeName,
			  getSyncAttribValueType statusVal)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl		            = m_eglTestCtx.getLibrary();
		TestLog&		log		            = m_testCtx.getLog();
		string          createSyncMsgChk    = funcNames[FUNC_NAME_CREATE_SYNC] + "()";

		m_sync = (egl.*createSyncFunc)(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = " << funcNames[FUNC_NAME_CREATE_SYNC] <<
			"(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" <<
			TestLog::EndMessage;
		EGLU_CHECK_MSG(egl, createSyncMsgChk.c_str());

		getSyncAttribValueType condition = 0;
		EGLU_CHECK_CALL_FPTR(egl, (egl.*getSyncAttribFunc)(m_eglDisplay, m_sync,
														   attribute, &condition));
		log << TestLog::Message << funcNames[FUNC_NAME_GET_SYNC_ATTRIB] << "(" <<
			m_eglDisplay << ", " << m_sync << ", " << attributeName << ", {" <<
			condition << "})" << TestLog::EndMessage;

		TCU_CHECK(condition == statusVal);
	}

	IterateResult	iterate					(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<createSync, getSyncAttrib, EGLAttrib>(m_funcNames, &Library::createSync,
													   &Library::getSyncAttrib,
													   EGL_SYNC_CONDITION, "EGL_SYNC_CONDITION",
													   EGL_SYNC_PRIOR_COMMANDS_COMPLETE);
		}
		if (hasRequiredEGLExtensions())
		{
			test<createSyncKHR, getSyncAttribKHR, EGLint>(m_funcNamesKHR, &Library::createSyncKHR,
														  &Library::getSyncAttribKHR,
														  EGL_SYNC_CONDITION_KHR, "EGL_SYNC_CONDITION_KHR",
														  EGL_SYNC_PRIOR_COMMANDS_COMPLETE_KHR);
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}
};

class GetSyncInvalidDisplayTest : public SyncTest
{
public:
					GetSyncInvalidDisplayTest	(EglTestContext& eglTestCtx, EGLenum syncType)
		: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_NONE,  syncType != EGL_SYNC_REUSABLE_KHR,"get_invalid_display", "get_invalid_display")
	{
	}

	template <typename createSyncFuncType,
		typename getSyncAttribFuncType,
		typename getSyncAttribValueType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  createSyncFuncType createSyncFunc,
			  getSyncAttribFuncType getSyncAttribFunc,
			  EGLint attribute, const string &attributeName)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl		            = m_eglTestCtx.getLibrary();
		TestLog&		log		            = m_testCtx.getLog();
		string          createSyncMsgChk    = funcNames[FUNC_NAME_CREATE_SYNC] + "()";

		m_sync = (egl.*createSyncFunc)(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = " << funcNames[FUNC_NAME_CREATE_SYNC] <<
			"(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" <<
			TestLog::EndMessage;
		EGLU_CHECK_MSG(egl, createSyncMsgChk.c_str());

		getSyncAttribValueType condition = 0xF0F0F;
		EGLBoolean result = (egl.*getSyncAttribFunc)(EGL_NO_DISPLAY, m_sync, attribute, &condition);
		log << TestLog::Message << result << " = " << funcNames[FUNC_NAME_GET_SYNC_ATTRIB] <<
			"(EGL_NO_DISPLAY, " << m_sync << ", " << attributeName << ", {" <<
			condition << "})" << TestLog::EndMessage;

		EGLint error = egl.getError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_DISPLAY)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) <<
				"' expected EGL_BAD_DISPLAY" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return;
		}

		TCU_CHECK(result == EGL_FALSE);
		TCU_CHECK(condition == 0xF0F0F);
	};

	IterateResult	iterate						(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<createSync, getSyncAttrib, EGLAttrib>(m_funcNames, &Library::createSync,
													   &Library::getSyncAttrib,
													   EGL_SYNC_CONDITION, "EGL_SYNC_CONDITION");
		}
		if (hasRequiredEGLExtensions())
		{
			test<createSyncKHR, getSyncAttribKHR, EGLint>(m_funcNamesKHR, &Library::createSyncKHR,
														  &Library::getSyncAttribKHR,
														  EGL_SYNC_CONDITION_KHR, "EGL_SYNC_CONDITION_KHR");
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}
};

class GetSyncInvalidSyncTest : public SyncTest
{
public:
					GetSyncInvalidSyncTest	(EglTestContext& eglTestCtx, EGLenum syncType)\
		: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_NONE, syncType != EGL_SYNC_REUSABLE_KHR, "get_invalid_sync", "get_invalid_sync")
	{
	}

	template <typename getSyncAttribFuncType,
		typename getSyncSyncValueType,
		typename getSyncAttribValueType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  getSyncAttribFuncType getSyncAttribFunc,
			  getSyncSyncValueType syncValue, const string &syncName,
			  EGLint attribute, const string &attributeName)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl		= m_eglTestCtx.getLibrary();
		TestLog&		log		= m_testCtx.getLog();

		getSyncAttribValueType condition = 0xF0F0F;
		EGLBoolean result = (egl.*getSyncAttribFunc)(m_eglDisplay, syncValue,
			attribute, &condition);
		log << TestLog::Message << result << " = " << funcNames[FUNC_NAME_GET_SYNC_ATTRIB] <<
			"(" << m_eglDisplay << ", " << syncName << ", " << attributeName << ", {" <<
			condition << "})" << TestLog::EndMessage;

		EGLint error = egl.getError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_PARAMETER)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) <<
				"' expected EGL_BAD_PARAMETER" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return;
		}

		TCU_CHECK(result == EGL_FALSE);
		TCU_CHECK(condition == 0xF0F0F);
	}

	IterateResult	iterate					(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<getSyncAttrib, EGLSync, EGLAttrib>(m_funcNames, &Library::getSyncAttrib,
													EGL_NO_SYNC, "EGL_NO_SYNC",
													EGL_SYNC_CONDITION, "EGL_SYNC_CONDITION");
		}
		if (hasRequiredEGLExtensions())
		{
			test<getSyncAttribKHR, EGLSyncKHR, EGLint>(m_funcNamesKHR, &Library::getSyncAttribKHR,
													   EGL_NO_SYNC_KHR, "EGL_NO_SYNC_KHR",
													   EGL_SYNC_CONDITION_KHR, "EGL_SYNC_CONDITION_KHR");
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}
};

class GetSyncInvalidAttributeTest : public SyncTest
{
public:
					GetSyncInvalidAttributeTest	(EglTestContext& eglTestCtx, EGLenum syncType)
		: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_NONE,  syncType != EGL_SYNC_REUSABLE_KHR,"get_invalid_attribute", "get_invalid_attribute")
	{
	}

	template <typename createSyncFuncType,
		typename getSyncAttribFuncType,
		typename getSyncAttribValueType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  createSyncFuncType createSyncFunc,
			  getSyncAttribFuncType getSyncAttribFunc)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl		            = m_eglTestCtx.getLibrary();
		TestLog&		log		            = m_testCtx.getLog();
		string          createSyncMsgChk    = funcNames[FUNC_NAME_CREATE_SYNC] + "()";

		m_sync = (egl.*createSyncFunc)(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = " << funcNames[FUNC_NAME_CREATE_SYNC] <<
			"(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" <<
			TestLog::EndMessage;
		EGLU_CHECK_MSG(egl, createSyncMsgChk.c_str());

		getSyncAttribValueType condition = 0xF0F0F;
		EGLBoolean result = (egl.*getSyncAttribFunc)(m_eglDisplay, m_sync, EGL_NONE, &condition);
		log << TestLog::Message << result << " = " << funcNames[FUNC_NAME_GET_SYNC_ATTRIB] <<
			"(" << m_eglDisplay << ", " << m_sync << ", EGL_NONE, {" << condition << "})" <<
			TestLog::EndMessage;

		EGLint error = egl.getError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_ATTRIBUTE)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) <<
				"' expected EGL_BAD_ATTRIBUTE" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return;
		}

		TCU_CHECK(result == EGL_FALSE);
		TCU_CHECK(condition == 0xF0F0F);
	}

	IterateResult	iterate						(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<createSync, getSyncAttrib, EGLAttrib>(m_funcNames,
													   &Library::createSync,
													   &Library::getSyncAttrib);
		}
		if (hasRequiredEGLExtensions())
		{
			test<createSyncKHR, getSyncAttribKHR, EGLint>(m_funcNamesKHR,
														  &Library::createSyncKHR,
														  &Library::getSyncAttribKHR);
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}
};

class GetSyncInvalidValueTest : public SyncTest
{
public:
					GetSyncInvalidValueTest	(EglTestContext& eglTestCtx, EGLenum syncType)
		: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_NONE,  syncType != EGL_SYNC_REUSABLE_KHR,"get_invalid_value", "get_invalid_value")
	{
	}

	template <typename createSyncFuncType,
		typename getSyncAttribFuncType, typename valueType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  createSyncFuncType createSyncFunc,
			  getSyncAttribFuncType getSyncAttribFunc,
			  EGLint attribute, const string &attributeName,
			  valueType value)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl		            = m_eglTestCtx.getLibrary();
		TestLog&		log		            = m_testCtx.getLog();
		string          createSyncMsgChk    = funcNames[FUNC_NAME_CREATE_SYNC] + "()";

		m_sync = (egl.*createSyncFunc)(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = " << funcNames[FUNC_NAME_CREATE_SYNC] <<
			"(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" <<
			TestLog::EndMessage;
		EGLU_CHECK_MSG(egl, createSyncMsgChk.c_str());

		EGLBoolean result = (egl.*getSyncAttribFunc)(m_eglDisplay, NULL, attribute, &value);
		log << TestLog::Message << result << " = " << funcNames[FUNC_NAME_GET_SYNC_ATTRIB] <<
			"(" << m_eglDisplay << ", " << 0x0 << ", " << attributeName << ", " << &value << ")" <<
			TestLog::EndMessage;

		EGLint error = egl.getError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_PARAMETER)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) <<
				"' expected EGL_BAD_PARAMETER" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return;
		}

		TCU_CHECK(result == EGL_FALSE);
	}

	IterateResult	iterate					(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			EGLAttrib value = 0;
			test<createSync, getSyncAttrib>(m_funcNames, &Library::createSync,
											&Library::getSyncAttrib,
											EGL_SYNC_TYPE, "EGL_SYNC_TYPE", value);
		}
		if (hasRequiredEGLExtensions())
		{
			EGLint value = 0;
			test<createSyncKHR, getSyncAttribKHR>(m_funcNamesKHR, &Library::createSyncKHR,
												  &Library::getSyncAttribKHR,
												  EGL_SYNC_TYPE_KHR, "EGL_SYNC_TYPE_KHR", value);
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}
};

class DestroySyncTest : public SyncTest
{
public:
					DestroySyncTest	(EglTestContext& eglTestCtx, EGLenum syncType)
		: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_NONE,  syncType != EGL_SYNC_REUSABLE_KHR,"destroy", "destroy")
	{
	}

	template <typename createSyncFuncType,
		typename destroySyncFuncType,
		typename getSyncSyncValueType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  createSyncFuncType createSyncFunc,
			  destroySyncFuncType destroySyncFunc,
			  getSyncSyncValueType syncValue)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl		            = m_eglTestCtx.getLibrary();
		TestLog&		log		            = m_testCtx.getLog();
		string          createSyncMsgChk    = funcNames[FUNC_NAME_CREATE_SYNC] + "()";

		m_sync = (egl.*createSyncFunc)(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << funcNames[FUNC_NAME_CREATE_SYNC] << "(" <<
			m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" <<
			TestLog::EndMessage;
		EGLU_CHECK_MSG(egl, createSyncMsgChk.c_str());

		log << TestLog::Message << funcNames[FUNC_NAME_DESTROY_SYNC] << "(" <<
			m_eglDisplay << ", " << m_sync << ")" << TestLog::EndMessage;
		EGLU_CHECK_CALL_FPTR(egl, (egl.*destroySyncFunc)(m_eglDisplay, m_sync));
		m_sync = syncValue;
	}

	IterateResult	iterate			(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<createSync, destroySync, EGLSync>(m_funcNames,
												   &Library::createSync,
												   &Library::destroySync,
												   EGL_NO_SYNC);
		}
		if (hasRequiredEGLExtensions())
		{
			test<createSyncKHR, destroySyncKHR, EGLSyncKHR>(m_funcNamesKHR,
															&Library::createSyncKHR,
															&Library::destroySyncKHR,
															EGL_NO_SYNC_KHR);
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}
};

class DestroySyncInvalidDislayTest : public SyncTest
{
public:
					DestroySyncInvalidDislayTest	(EglTestContext& eglTestCtx, EGLenum syncType)
		: SyncTest(eglTestCtx, syncType, SyncTest::EXTENSION_NONE,  syncType != EGL_SYNC_REUSABLE_KHR,"destroy_invalid_display", "destroy_invalid_display")
	{
	}

	template <typename createSyncFuncType, typename destroySyncFuncType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  createSyncFuncType createSyncFunc,
			  destroySyncFuncType destroySyncFunc)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl		            = m_eglTestCtx.getLibrary();
		TestLog&		log		            = m_testCtx.getLog();
		string          createSyncMsgChk    = funcNames[FUNC_NAME_CREATE_SYNC] + "()";

		m_sync = (egl.*createSyncFunc)(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << funcNames[FUNC_NAME_CREATE_SYNC] << "(" <<
			m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" <<
			TestLog::EndMessage;
		EGLU_CHECK_MSG(egl, createSyncMsgChk.c_str());

		EGLBoolean result = (egl.*destroySyncFunc)(EGL_NO_DISPLAY, m_sync);
		log << TestLog::Message << result << " = " << funcNames[FUNC_NAME_DESTROY_SYNC] <<
			"(EGL_NO_DISPLAY, " << m_sync << ")" << TestLog::EndMessage;

		EGLint error = egl.getError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_DISPLAY)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) <<
				"' expected EGL_BAD_DISPLAY" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return;
		}

		TCU_CHECK(result == EGL_FALSE);
	}

	IterateResult	iterate							(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<createSync, destroySync>(m_funcNames,
										  &Library::createSync,
										  &Library::destroySync);
		}
		if (hasRequiredEGLExtensions())
		{
			test<createSyncKHR, destroySyncKHR>(m_funcNamesKHR,
												&Library::createSyncKHR,
												&Library::destroySyncKHR);
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}
};

class DestroySyncInvalidSyncTest : public SyncTest
{
public:
					DestroySyncInvalidSyncTest	(EglTestContext& eglTestCtx, EGLenum syncType)
		: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_NONE,  syncType != EGL_SYNC_REUSABLE_KHR,"destroy_invalid_sync", "destroy_invalid_sync")
	{
	}

	template <typename destroySyncFuncType, typename getSyncSyncValueType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  destroySyncFuncType destroySyncFunc,
			  getSyncSyncValueType syncValue)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl		= m_eglTestCtx.getLibrary();
		TestLog&		log		= m_testCtx.getLog();

		EGLBoolean result = (egl.*destroySyncFunc)(m_eglDisplay, syncValue);
		log << TestLog::Message << result << " = " << funcNames[FUNC_NAME_DESTROY_SYNC] <<
			"(" << m_eglDisplay << ", " << syncValue << ")" << TestLog::EndMessage;

		EGLint error = egl.getError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_PARAMETER)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) <<
				"' expected EGL_BAD_PARAMETER" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return;
		}

		TCU_CHECK(result == EGL_FALSE);
	}

	IterateResult	iterate						(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<destroySync, EGLSync>(m_funcNames,
									   &Library::destroySync,
									   EGL_NO_SYNC);
		}
		if (hasRequiredEGLExtensions())
		{
			test<destroySyncKHR, EGLSyncKHR>(m_funcNamesKHR,
											 &Library::destroySyncKHR,
											 EGL_NO_SYNC_KHR);
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}
};

class WaitSyncTest : public SyncTest
{
public:
					WaitSyncTest	(EglTestContext& eglTestCtx, EGLenum syncType)
		: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_WAIT_SYNC, true, "wait_server", "wait_server")
	{
	}

	template <typename createSyncFuncType,
		typename waitSyncFuncType,
		typename waitSyncStatusType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  createSyncFuncType createSyncFunc,
			  waitSyncFuncType waitSyncFunc)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl     = m_eglTestCtx.getLibrary();
		TestLog&		log     = m_testCtx.getLog();
		string          msgChk  = funcNames[FUNC_NAME_CREATE_SYNC] + "()";

		m_sync = (egl.*createSyncFunc)(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = " << funcNames[FUNC_NAME_CREATE_SYNC] <<
			"(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" <<
			TestLog::EndMessage;
		EGLU_CHECK_MSG(egl, msgChk.c_str());

		waitSyncStatusType status = (egl.*waitSyncFunc)(m_eglDisplay, m_sync, 0);
		log << TestLog::Message << status << " = " << funcNames[FUNC_NAME_WAIT_SYNC] << "(" <<
			m_eglDisplay << ", " << m_sync << ", 0, 0)" << TestLog::EndMessage;

		TCU_CHECK(status == EGL_TRUE);

		GLU_CHECK_GLW_CALL(m_gl, finish());
	}


	IterateResult	iterate			(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<createSync, waitSync, EGLBoolean>(m_funcNames,
												   &Library::createSync,
												   &Library::waitSync);
		}
		if (hasRequiredEGLExtensions())
		{
			test<createSyncKHR, waitSyncKHR, EGLint>(m_funcNamesKHR,
													 &Library::createSyncKHR,
													 &Library::waitSyncKHR);
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}

};

class WaitSyncInvalidDisplayTest : public SyncTest
{
public:
					WaitSyncInvalidDisplayTest	(EglTestContext& eglTestCtx, EGLenum syncType)
		: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_WAIT_SYNC, true, "wait_server_invalid_display", "wait_server_invalid_display")
	{
	}

	template <typename createSyncFuncType,
		typename waitSyncFuncType,
		typename waitSyncStatusType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  createSyncFuncType createSyncFunc,
			  waitSyncFuncType waitSyncFunc)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl     = m_eglTestCtx.getLibrary();
		TestLog&		log     = m_testCtx.getLog();
		string          msgChk  = funcNames[FUNC_NAME_CREATE_SYNC] + "()";

		m_sync = (egl.*createSyncFunc)(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = " << funcNames[FUNC_NAME_CREATE_SYNC] <<
			"(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" <<
			TestLog::EndMessage;
		EGLU_CHECK_MSG(egl, msgChk.c_str());

		waitSyncStatusType status = (egl.*waitSyncFunc)(EGL_NO_DISPLAY, m_sync, 0);
		log << TestLog::Message << status << " = " << funcNames[FUNC_NAME_WAIT_SYNC] <<
			"(EGL_NO_DISPLAY, " << m_sync << ", 0)" << TestLog::EndMessage;

		EGLint error = egl.getError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_DISPLAY)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) <<
				"' expected EGL_BAD_DISPLAY" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return;
		}

		TCU_CHECK(status == EGL_FALSE);
	}

	IterateResult	iterate						(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<createSync, waitSync, EGLBoolean>(m_funcNames,
												   &Library::createSync,
												   &Library::waitSync);
		}
		if (hasRequiredEGLExtensions())
		{
			test<createSyncKHR, waitSyncKHR, EGLint>(m_funcNamesKHR,
													 &Library::createSyncKHR,
													 &Library::waitSyncKHR);
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}
};

class WaitSyncInvalidSyncTest : public SyncTest
{
public:
					WaitSyncInvalidSyncTest	(EglTestContext& eglTestCtx, EGLenum syncType)
		: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_WAIT_SYNC, true, "wait_server_invalid_sync", "wait_server_invalid_sync")
	{
	}

	template <typename waitSyncFuncType, typename waitSyncSyncType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  waitSyncFuncType waitSyncFunc,
			  waitSyncSyncType syncValue)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl		= m_eglTestCtx.getLibrary();
		TestLog&		log		= m_testCtx.getLog();

		EGLint status = (egl.*waitSyncFunc)(m_eglDisplay, syncValue, 0);
		log << TestLog::Message << status << " = " << funcNames[FUNC_NAME_WAIT_SYNC] <<
			"(" << m_eglDisplay << ", " << syncValue << ", 0)" << TestLog::EndMessage;

		EGLint error = egl.getError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_PARAMETER)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) <<
				"' expected EGL_BAD_PARAMETER" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return;
		}

		TCU_CHECK(status == EGL_FALSE);
	}

	IterateResult	iterate					(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<waitSync, EGLSync>(m_funcNames,
									&Library::waitSync,
									EGL_NO_SYNC);
		}
		if (hasRequiredEGLExtensions())
		{
			test<waitSyncKHR, EGLSyncKHR>(m_funcNamesKHR,
										  &Library::waitSyncKHR,
										  EGL_NO_SYNC_KHR);
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}
};

class WaitSyncInvalidFlagTest : public SyncTest
{
public:
					WaitSyncInvalidFlagTest	(EglTestContext& eglTestCtx, EGLenum syncType)
		: SyncTest	(eglTestCtx, syncType, SyncTest::EXTENSION_WAIT_SYNC, true, "wait_server_invalid_flag", "wait_server_invalid_flag")
	{
	}

	template <typename createSyncFuncType, typename waitSyncFuncType>
	void test(string funcNames[FUNC_NAME_NUM_NAMES],
			  createSyncFuncType createSyncFunc,
			  waitSyncFuncType waitSyncFunc)
	{
		// Reset before each test
		deinit();
		init();

		const Library&	egl		            = m_eglTestCtx.getLibrary();
		TestLog&		log		            = m_testCtx.getLog();
		string          createSyncMsgChk    = funcNames[FUNC_NAME_CREATE_SYNC] + "()";

		m_sync = (egl.*createSyncFunc)(m_eglDisplay, m_syncType, NULL);
		log << TestLog::Message << m_sync << " = " << funcNames[FUNC_NAME_CREATE_SYNC] <<
			"(" << m_eglDisplay << ", " << getSyncTypeName(m_syncType) << ", NULL)" <<
			TestLog::EndMessage;
		EGLU_CHECK_MSG(egl, createSyncMsgChk.c_str());

		EGLint status = (egl.*waitSyncFunc)(m_eglDisplay, m_sync, 0xFFFFFFFF);
		log << TestLog::Message << status << " = " << funcNames[FUNC_NAME_WAIT_SYNC] <<
			"(" << m_eglDisplay << ", " << m_sync << ", 0xFFFFFFFF)" << TestLog::EndMessage;

		EGLint error = egl.getError();
		log << TestLog::Message << error << " = eglGetError()" << TestLog::EndMessage;

		if (error != EGL_BAD_PARAMETER)
		{
			log << TestLog::Message << "Unexpected error '" << eglu::getErrorStr(error) <<
				"' expected EGL_BAD_PARAMETER" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return;
		}

		TCU_CHECK(status == EGL_FALSE);
	}

	IterateResult	iterate					(void)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

		if (hasRequiredEGLVersion(1, 5))
		{
			test<createSync, waitSync>(m_funcNames,
									   &Library::createSync,
									   &Library::waitSync);
		}
		if (hasRequiredEGLExtensions())
		{
			test<createSyncKHR, waitSyncKHR>(m_funcNamesKHR,
											 &Library::createSyncKHR,
											 &Library::waitSyncKHR);
		}
		else if (!hasRequiredEGLVersion(1, 5))
		{
			TCU_THROW(NotSupportedError, "Required extensions not supported");
		}

		return STOP;
	}
};

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

		addChild(valid);
	}

	// Add negative API tests
	{
		TestCaseGroup* const invalid = new TestCaseGroup(m_eglTestCtx, "invalid", "Invalid function calls");

		// eglCreateSyncKHR tests
		invalid->addChild(new CreateInvalidDisplayTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));
		invalid->addChild(new CreateInvalidTypeTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));
		invalid->addChild(new CreateInvalidAttribsTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));

		// eglClientWaitSyncKHR tests
		invalid->addChild(new ClientWaitInvalidDisplayTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));
		invalid->addChild(new ClientWaitInvalidSyncTest(m_eglTestCtx, EGL_SYNC_REUSABLE_KHR));

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

		addChild(invalid);
	}
}

} // egl
} // deqp

