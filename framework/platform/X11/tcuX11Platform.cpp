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
 * \brief X11 Platform.
 *//*--------------------------------------------------------------------*/

#include "tcuX11Platform.hpp"

#include "deUniquePtr.hpp"
#include "gluPlatform.hpp"
#include "tcuX11.hpp"

#if defined (DEQP_SUPPORT_GLX)
#	include "tcuX11GlxPlatform.hpp"
#endif
#if defined (DEQP_SUPPORT_EGL)
#	include "tcuX11EglPlatform.hpp"
#endif


namespace tcu
{
namespace x11
{

class X11GLPlatform : public glu::Platform
{
public:
	void		registerFactory	(de::MovePtr<glu::ContextFactory> factory)
	{
		m_contextFactoryRegistry.registerFactory(factory.release());
	}
};

class X11Platform : public tcu::Platform
{
public:
							X11Platform		(void);
	bool					processEvents	(void) { return !m_eventState.getQuitFlag(); }
	const glu::Platform&	getGLPlatform	(void) const { return m_glPlatform; }

#if defined (DEQP_SUPPORT_EGL)
	const eglu::Platform&	getEGLPlatform	(void) const { return m_eglPlatform; }
#endif // DEQP_SUPPORT_EGL

private:
	EventState				m_eventState;
#if defined (DEQP_SUPPORT_EGL)
	x11::egl::Platform		m_eglPlatform;
#endif // DEQP_SPPORT_EGL
	X11GLPlatform			m_glPlatform;
};

X11Platform::X11Platform (void)
#if defined (DEQP_SUPPORT_EGL)
	: m_eglPlatform	(m_eventState)
#endif // DEQP_SUPPORT_EGL
{
#if defined (DEQP_SUPPORT_GLX)
	m_glPlatform.registerFactory(glx::createContextFactory(m_eventState));
#endif // DEQP_SUPPORT_GLX
#if defined (DEQP_SUPPORT_EGL)
	m_glPlatform.registerFactory(m_eglPlatform.createContextFactory());
#endif // DEQP_SUPPORT_EGL
}

} // x11
} // tcu

tcu::Platform* createPlatform (void)
{
	return new tcu::x11::X11Platform();
}
