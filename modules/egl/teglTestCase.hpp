#ifndef _TEGLTESTCASE_HPP
#define _TEGLTESTCASE_HPP
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

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "tcuEgl.hpp"
#include "egluNativeWindow.hpp"
#include "egluConfigInfo.hpp"
#include "tcuFunctionLibrary.hpp"
#include "gluRenderContext.hpp"

#include <set>
#include <map>

namespace eglu
{
class NativeDisplay;
class NativeWindow;
class NativePixmap;
class NativeDisplayFactory;
class NativeWindowFactory;
class NativePixmapFactory;
} // eglu

namespace deqp
{
namespace egl
{

class EglTestContext
{
public:
												EglTestContext			(tcu::TestContext& testCtx, const eglu::NativeDisplayFactory& displayFactory, const eglu::NativeWindowFactory* windowFactory, const eglu::NativePixmapFactory* pixmapFactory);
												~EglTestContext			(void);

	tcu::TestContext&							getTestContext			(void) 			{ return m_testCtx;													}
	eglu::NativeDisplay&						getNativeDisplay		(void)			{ return *m_defaultNativeDisplay;									}
	tcu::egl::Display&							getDisplay				(void)			{ return *m_defaultEGLDisplay;										}
	const std::vector<eglu::ConfigInfo>&		getConfigs				(void) const	{ return m_configs;													}

	const eglu::NativeWindowFactory&			getNativeWindowFactory	(void) const;
	const eglu::NativePixmapFactory&			getNativePixmapFactory	(void) const;

	eglu::NativeWindow*							createNativeWindow		(EGLDisplay display, EGLConfig config, const EGLAttrib* attribList, int width, int height, eglu::WindowParams::Visibility visibility);
	eglu::NativePixmap*							createNativePixmap		(EGLDisplay display, EGLConfig config, const EGLAttrib* attribList, int width, int height);

	deFunctionPtr								getGLFunction			(glu::ApiType apiType, const char* name) const;
	void										getGLFunctions			(glw::Functions& gl, glu::ApiType apiType) const;

	bool										isAPISupported			(EGLint api)	{ return m_supportedAPIs.find(api) != m_supportedAPIs.end();		}

	// Test case wrapper will instruct test context to create display upon case init and destroy it in deinit
	void										createDefaultDisplay	(void);
	void										destroyDefaultDisplay	(void);

private:
												EglTestContext			(const EglTestContext&);
	EglTestContext&								operator=				(const EglTestContext&);

	const tcu::FunctionLibrary*					getGLLibrary			(glu::ApiType apiType) const;

	tcu::TestContext&							m_testCtx;
	const eglu::NativeDisplayFactory&			m_displayFactory;
	const eglu::NativeWindowFactory*			m_windowFactory;
	const eglu::NativePixmapFactory*			m_pixmapFactory;

	typedef std::map<deUint32, tcu::FunctionLibrary*> GLLibraryMap;
	mutable GLLibraryMap						m_glLibraries;			//!< GL library cache.

	eglu::NativeDisplay*						m_defaultNativeDisplay;
	tcu::egl::Display*							m_defaultEGLDisplay;
	std::vector<eglu::ConfigInfo>				m_configs;
	std::set<EGLint>							m_supportedAPIs;
};

class TestCaseGroup : public tcu::TestCaseGroup
{
public:
						TestCaseGroup	(EglTestContext& eglTestCtx, const char* name, const char* description);
	virtual				~TestCaseGroup	(void);

protected:
	EglTestContext&		m_eglTestCtx;
};

class TestCase : public tcu::TestCase
{
public:
						TestCase		(EglTestContext& eglTestCtx, const char* name, const char* description);
						TestCase		(EglTestContext& eglTestCtx, tcu::TestNodeType type, const char* name, const char* description);
	virtual				~TestCase		(void);

protected:
	EglTestContext&		m_eglTestCtx;
};

} // egl
} // deqp

#endif // _TEGLTESTCASE_HPP
