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

#include "tcuX11.hpp"
#include "gluRenderConfig.hpp"
#include "deMemory.h"

#include <X11/Xutil.h>

namespace tcu
{
namespace x11
{

enum
{
	DEFAULT_WINDOW_WIDTH	= 400,
	DEFAULT_WINDOW_HEIGHT	= 300
};

EventState::EventState (void)
	: m_quit(false)
{
}

EventState::~EventState (void)
{
}

void EventState::setQuitFlag (bool quit)
{
	de::ScopedLock lock(m_mutex);
	m_quit = quit;
}

bool EventState::getQuitFlag (void)
{
	de::ScopedLock lock(m_mutex);
	return m_quit;
}

Display::Display (EventState& eventState, const char* name)
	: m_eventState	(eventState)
	, m_display		(DE_NULL)
	, m_deleteAtom	(DE_NULL)
{
	m_display = XOpenDisplay((char*)name); // Won't modify argument string.
	if (!m_display)
		throw ResourceError("Failed to open display", name, __FILE__, __LINE__);

	m_deleteAtom	= XInternAtom(m_display, "WM_DELETE_WINDOW", False);
}

Display::~Display (void)
{
	XCloseDisplay(m_display);
}

void Display::processEvents (void)
{
	XEvent	event;

	while (XPending(m_display))
	{
		XNextEvent(m_display, &event);

		// \todo [2010-10-27 pyry] Handle ConfigureNotify?
		if (event.type == ClientMessage && (unsigned)event.xclient.data.l[0] == m_deleteAtom)
			m_eventState.setQuitFlag(true);
	}
}

bool Display::getVisualInfo (VisualID visualID, XVisualInfo& dst)
{
	XVisualInfo		query;
	query.visualid = visualID;
	int				numVisuals	= 0;
	XVisualInfo*	response	= XGetVisualInfo(m_display, VisualIDMask, &query, &numVisuals);
	bool			succ		= false;

	if (response != DE_NULL)
	{
		if (numVisuals > 0) // should be 1, but you never know...
		{
			dst = response[0];
			succ = true;
		}
		XFree(response);
	}

	return succ;
}

::Visual* Display::getVisual (VisualID visualID)
{
	XVisualInfo		info;

	if (getVisualInfo(visualID, info))
		return info.visual;

	return DE_NULL;
}

Window::Window (Display& display, int width, int height, ::Visual* visual)
	: m_display		(display)
	, m_colormap	(None)
	, m_window		(None)
	, m_visible		(false)
{
	XSetWindowAttributes	swa;
	::Display* const		dpy					= m_display.getXDisplay();
	::Window				root				= DefaultRootWindow(dpy);
	unsigned long			mask				= CWBorderPixel | CWEventMask;

	// If redirect is enabled, window size can't be guaranteed and it is up to
	// the window manager to decide whether to honor sizing requests. However,
	// overriding that causes window to appear as an overlay, which causes
	// other issues, so this is disabled by default.
	const bool				overrideRedirect	= false;

	if (overrideRedirect)
	{
		mask |= CWOverrideRedirect;
		swa.override_redirect = true;
	}

	if (visual == DE_NULL)
		visual = CopyFromParent;
	else
	{
		XVisualInfo	info	= XVisualInfo();
		bool		succ	= display.getVisualInfo(XVisualIDFromVisual(visual), info);

		TCU_CHECK_INTERNAL(succ);

		root				= RootWindow(dpy, info.screen);
		m_colormap			= XCreateColormap(dpy, root, visual, AllocNone);
		swa.colormap		= m_colormap;
		mask |= CWColormap;
	}

	swa.border_pixel	= 0;
	swa.event_mask		= ExposureMask|KeyPressMask|KeyReleaseMask|StructureNotifyMask;

	if (width == glu::RenderConfig::DONT_CARE)
		width = DEFAULT_WINDOW_WIDTH;
	if (height == glu::RenderConfig::DONT_CARE)
		height = DEFAULT_WINDOW_HEIGHT;

	m_window = XCreateWindow(dpy, root, 0, 0, width, height, 0,
							 CopyFromParent, InputOutput, visual, mask, &swa);
	TCU_CHECK(m_window);

	Atom deleteAtom = m_display.getDeleteAtom();
	XSetWMProtocols(dpy, m_window, &deleteAtom, 1);
}

void Window::setVisibility (bool visible)
{
	::Display*	dpy			= m_display.getXDisplay();
	int			eventType	= None;
	XEvent		event;

	if (visible == m_visible)
		return;

	if (visible)
	{
		XMapWindow(dpy, m_window);
		eventType = MapNotify;
	}
	else
	{
		XUnmapWindow(dpy, m_window);
		eventType = UnmapNotify;
	}

	// We are only interested about exposure/structure notify events, not user input
	XSelectInput(dpy, m_window, ExposureMask | StructureNotifyMask);

	do
	{
		XNextEvent(dpy, &event);
	} while (event.type != eventType);

	m_visible = visible;
}

void Window::getDimensions (int* width, int* height) const
{
	int x, y;
	::Window root;
	unsigned width_, height_, borderWidth, depth;

	XGetGeometry(m_display.getXDisplay(), m_window, &root, &x, &y, &width_, &height_, &borderWidth, &depth);
	if (width != DE_NULL)
		*width = static_cast<int>(width_);
	if (height != DE_NULL)
		*height = static_cast<int>(height_);
}

void Window::setDimensions (int width, int height)
{
	const unsigned int	mask = CWWidth | CWHeight;
	XWindowChanges		changes;
	changes.width		= width;
	changes.height		= height;

	XConfigureWindow(m_display.getXDisplay(), m_window, mask, &changes);
}

void Window::processEvents (void)
{
	// A bit of a hack, since we don't really handle all the events.
	m_display.processEvents();
}

Window::~Window (void)
{
	XDestroyWindow(m_display.getXDisplay(), m_window);
	if (m_colormap != None)
		XFreeColormap(m_display.getXDisplay(), m_colormap);
}

} // x11
} // tcu
