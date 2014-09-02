#ifndef _TCUX11_HPP
#define _TCUX11_HPP
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
 * \brief X11 utilities.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "gluRenderConfig.hpp"
#include "gluPlatform.hpp"
#include "deMutex.hpp"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>

namespace tcu
{
namespace x11
{

class EventState
{
public:
							EventState				(void);
	virtual					~EventState				(void);

	void					setQuitFlag				(bool quit);
	bool					getQuitFlag				(void);

protected:
	de::Mutex				m_mutex;
	bool					m_quit;

private:
							EventState				(const EventState&);
	EventState&				operator=				(const EventState&);
};

class Display
{
public:
							Display					(EventState& platform, const char* name);
	virtual					~Display				(void);

	::Display*				getXDisplay				(void) { return m_display;		}
	Atom					getDeleteAtom			(void) { return m_deleteAtom;	}

	::Visual*				getVisual				(VisualID visualID);
	bool					getVisualInfo			(VisualID visualID, XVisualInfo& dst);
	void					processEvents			(void);

protected:
	EventState&				m_eventState;
	::Display*				m_display;
	Atom					m_deleteAtom;

private:
							Display					(const Display&);
	Display&				operator=				(const Display&);
};

class Window
{
public:
							Window					(Display& display, int width, int height,
													 ::Visual* visual);
							~Window					(void);

	void					setVisibility			(bool visible);

	void					processEvents			(void);
	Display&				getDisplay				(void) { return m_display; }
	::Window&				getXID					(void) { return m_window; }

	void					getDimensions			(int* width, int* height) const;
	void					setDimensions			(int width, int height);

protected:

	Display&				m_display;
	::Colormap				m_colormap;
	::Window				m_window;
	bool					m_visible;

private:
							Window					(const Window&);
	Window&					operator=				(const Window&);
};

} // x11
} // tcu

#endif // _TCUX11_HPP
