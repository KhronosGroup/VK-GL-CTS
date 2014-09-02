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
 * \brief EGL Test Case
 *//*--------------------------------------------------------------------*/

#include "teglTestCase.hpp"

#include "tcuPlatform.hpp"

#include "egluUtil.hpp"
#include "egluGLFunctionLoader.hpp"
#include "egluPlatform.hpp"

#include "gluRenderContext.hpp"
#include "glwInitFunctions.hpp"

#include <set>

using std::vector;
using std::set;

namespace deqp
{
namespace egl
{

namespace
{

void split (std::vector<std::string>& dst, const std::string& src)
{
	size_t start = 0;
	size_t end	 = std::string::npos;

	while ((end = src.find(' ', start)) != std::string::npos)
	{
		dst.push_back(src.substr(start, end-start));
		start = end+1;
	}

	if (start < end)
		dst.push_back(src.substr(start, end-start));
}

EGLint parseAPI (const std::string& api)
{
	if (api == "OpenGL")
		return EGL_OPENGL_API;
	else if (api == "OpenGL_ES")
		return EGL_OPENGL_ES_API;
	else if (api == "OpenVG")
		return EGL_OPENVG_API;
	else
	{
		tcu::print("Warning: Unknown API '%s'", api.c_str());
		return 0;
	}
}

} // anonymous

EglTestContext::EglTestContext (tcu::TestContext& testCtx, const eglu::NativeDisplayFactory& displayFactory, const eglu::NativeWindowFactory* windowFactory, const eglu::NativePixmapFactory* pixmapFactory)
	: m_testCtx					(testCtx)
	, m_displayFactory			(displayFactory)
	, m_windowFactory			(windowFactory)
	, m_pixmapFactory			(pixmapFactory)
	, m_defaultNativeDisplay	(DE_NULL)
	, m_defaultEGLDisplay		(DE_NULL)
{
	// Temporarily allocate default display for storing config list
	try
	{
		EGLDisplay	eglDisplay	= EGL_NO_DISPLAY;
		EGLint		majorVersion;
		EGLint		minorVersion;

		m_defaultNativeDisplay	= m_displayFactory.createDisplay();

		eglDisplay = eglu::getDisplay(*m_defaultNativeDisplay);
		TCU_CHECK_EGL_CALL(eglInitialize(eglDisplay, &majorVersion, &minorVersion));

		m_defaultEGLDisplay = new tcu::egl::Display(eglDisplay, majorVersion, minorVersion);

		// Create config list
		{
			vector<EGLConfig>	configs;
			set<EGLint>			idSet; // For checking for duplicate config IDs

			m_defaultEGLDisplay->getConfigs(configs);

			m_configs.resize(configs.size());
			for (int ndx = 0; ndx < (int)configs.size(); ndx++)
			{
				m_defaultEGLDisplay->describeConfig(configs[ndx], m_configs[ndx]);

				EGLint id = m_configs[ndx].configId;
				if (idSet.find(id) != idSet.end())
					tcu::print("Warning: Duplicate config ID %d\n", id);
				idSet.insert(id);
			}
		}

		// Query supported APIs
		{
			const char*					clientAPIs	= eglQueryString(eglDisplay, EGL_CLIENT_APIS);
			std::vector<std::string>	apis;
			TCU_CHECK(clientAPIs);

			split(apis, clientAPIs);
			for (std::vector<std::string>::const_iterator apiIter = apis.begin(); apiIter != apis.end(); apiIter++)
			{
				EGLint parsedAPI = parseAPI(*apiIter);
				if (parsedAPI != 0)
					m_supportedAPIs.insert(parsedAPI);
			}
		}

		delete m_defaultEGLDisplay;
		m_defaultEGLDisplay = DE_NULL;
		delete m_defaultNativeDisplay;
		m_defaultNativeDisplay = DE_NULL;
	}
	catch (...)
	{
		delete m_defaultEGLDisplay;
		m_defaultEGLDisplay = DE_NULL;
		delete m_defaultNativeDisplay;
		m_defaultNativeDisplay = DE_NULL;
		throw;
	}
}

EglTestContext::~EglTestContext (void)
{
	for (GLLibraryMap::iterator iter = m_glLibraries.begin(); iter != m_glLibraries.end(); ++iter)
		delete iter->second;

	delete m_defaultEGLDisplay;
	delete m_defaultNativeDisplay;
}

void EglTestContext::createDefaultDisplay (void)
{
	EGLDisplay	eglDisplay	= EGL_NO_DISPLAY;
	EGLint		majorVersion;
	EGLint		minorVersion;

	DE_ASSERT(!m_defaultEGLDisplay);
	DE_ASSERT(!m_defaultNativeDisplay);

	try
	{
		m_defaultNativeDisplay	= m_displayFactory.createDisplay();

		eglDisplay = eglu::getDisplay(*m_defaultNativeDisplay);
		TCU_CHECK_EGL_CALL(eglInitialize(eglDisplay, &majorVersion, &minorVersion));

		m_defaultEGLDisplay = new tcu::egl::Display(eglDisplay, majorVersion, minorVersion);
	}
	catch (const std::exception&)
	{
		delete m_defaultEGLDisplay;
		m_defaultEGLDisplay = DE_NULL;
		delete m_defaultNativeDisplay;
		m_defaultNativeDisplay = DE_NULL;
		throw;
	}
}

const eglu::NativeWindowFactory& EglTestContext::getNativeWindowFactory (void) const
{
	if (m_windowFactory)
		return *m_windowFactory;
	else
		throw tcu::NotSupportedError("No default native window factory available", "", __FILE__, __LINE__);
}

const eglu::NativePixmapFactory& EglTestContext::getNativePixmapFactory (void) const
{
	if (m_pixmapFactory)
		return *m_pixmapFactory;
	else
		throw tcu::NotSupportedError("No default native pixmap factory available", "", __FILE__, __LINE__);
}

void EglTestContext::destroyDefaultDisplay (void)
{
	DE_ASSERT(m_defaultEGLDisplay);
	DE_ASSERT(m_defaultNativeDisplay);

	delete m_defaultEGLDisplay;
	m_defaultEGLDisplay = DE_NULL;

	delete m_defaultNativeDisplay;
	m_defaultNativeDisplay = DE_NULL;
}

eglu::NativeWindow* EglTestContext::createNativeWindow (EGLDisplay display, EGLConfig config, const EGLAttrib* attribList, int width, int height, eglu::WindowParams::Visibility visibility)
{
	if (!m_windowFactory)
		throw tcu::NotSupportedError("Windows not supported", "", __FILE__, __LINE__);

	return m_windowFactory->createWindow(m_defaultNativeDisplay, display, config, attribList, eglu::WindowParams(width, height, visibility));
}

eglu::NativePixmap* EglTestContext::createNativePixmap (EGLDisplay display, EGLConfig config, const EGLAttrib* attribList, int width, int height)
{
	if (!m_pixmapFactory)
		throw tcu::NotSupportedError("Pixmaps not supported", "", __FILE__, __LINE__);

	return m_pixmapFactory->createPixmap(m_defaultNativeDisplay, display, config, attribList, width, height);
}

// \todo [2014-10-06 pyry] Quite hacky, expose ApiType internals?
static deUint32 makeKey (glu::ApiType apiType)
{
	return (apiType.getMajorVersion() << 8) | (apiType.getMinorVersion() << 4) | apiType.getProfile();
}

const tcu::FunctionLibrary* EglTestContext::getGLLibrary (glu::ApiType apiType) const
{
	tcu::FunctionLibrary*		library	= DE_NULL;
	const deUint32				key		= makeKey(apiType);
	GLLibraryMap::iterator		iter	= m_glLibraries.find(key);

	if (iter == m_glLibraries.end())
	{
		library = m_testCtx.getPlatform().getEGLPlatform().createDefaultGLFunctionLibrary(apiType, m_testCtx.getCommandLine());
		m_glLibraries.insert(std::make_pair(key, library));
	}
	else
		library = iter->second;

	return library;
}

deFunctionPtr EglTestContext::getGLFunction (glu::ApiType apiType, const char* name) const
{
	// \todo [2014-03-11 pyry] This requires fall-back to eglGetProcAddress(), right?
	const tcu::FunctionLibrary* const	library	= getGLLibrary(apiType);
	return library->getFunction(name);
}

void EglTestContext::getGLFunctions (glw::Functions& gl, glu::ApiType apiType) const
{
	const tcu::FunctionLibrary* const	library		= getGLLibrary(apiType);
	const eglu::GLFunctionLoader		loader		(library);

	// \note There may not be current context, so we can't use initFunctions().
	glu::initCoreFunctions(&gl, &loader, apiType);
}

TestCaseGroup::TestCaseGroup (EglTestContext& eglTestCtx, const char* name, const char* description)
	: tcu::TestCaseGroup	(eglTestCtx.getTestContext(), name, description)
	, m_eglTestCtx			(eglTestCtx)
{
}

TestCaseGroup::~TestCaseGroup (void)
{
}

TestCase::TestCase (EglTestContext& eglTestCtx, const char* name, const char* description)
	: tcu::TestCase		(eglTestCtx.getTestContext(), name, description)
	, m_eglTestCtx		(eglTestCtx)
{
}

TestCase::TestCase (EglTestContext& eglTestCtx, tcu::TestNodeType type,  const char* name, const char* description)
	: tcu::TestCase		(eglTestCtx.getTestContext(), type, name, description)
	, m_eglTestCtx		(eglTestCtx)
{
}

TestCase::~TestCase (void)
{
}

} // egl
} // deqp
