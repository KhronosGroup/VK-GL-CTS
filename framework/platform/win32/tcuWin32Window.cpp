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
 * \brief Generic Win32 window class.
 *//*--------------------------------------------------------------------*/

#include "tcuWin32Window.hpp"

namespace tcu
{

static LRESULT CALLBACK win32WindowProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	Win32Window* window = reinterpret_cast<Win32Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
	if (window)
		return window->windowProc(uMsg, wParam, lParam);
	else
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

Win32Window::Win32Window (HINSTANCE instance, int width, int height)
	: m_window		(DE_NULL)
{
	try
	{
		static const char	s_className[]	= "dEQP Test Process Class";
		static const char	s_windowName[]	= "dEQP Test Process";

		{
			WNDCLASS wndClass;
			memset(&wndClass, 0, sizeof(wndClass));
			wndClass.style			= CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
			wndClass.lpfnWndProc	= win32WindowProc;
			wndClass.cbClsExtra		= 0;
			wndClass.cbWndExtra		= 0;
			wndClass.hInstance		= instance;
			wndClass.hIcon			= LoadIcon(NULL, IDI_APPLICATION);
			wndClass.hCursor		= LoadCursor(NULL, IDC_ARROW);
			wndClass.hbrBackground	= CreateSolidBrush(RGB(0, 0, 0));
			wndClass.lpszMenuName	= NULL;
			wndClass.lpszClassName	= s_className;

			RegisterClass(&wndClass);
		}

		m_window = CreateWindow(s_className, s_windowName,
								WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW,
								CW_USEDEFAULT, CW_USEDEFAULT,
								width, height,
								NULL, NULL, instance, NULL);

		if (!m_window)
			TCU_THROW(ResourceError, "Failed to create Win32 window");

		// Store this as userdata
		SetWindowLongPtr(m_window, GWLP_USERDATA, (LONG_PTR)this);

		setSize(width, height);
	}
	catch (...)
	{
		if (m_window)
			DestroyWindow(m_window);

		throw;
	}
}

Win32Window::~Win32Window (void)
{
	if (m_window)
	{
		// Clear this pointer from windowproc
		SetWindowLongPtr(m_window, GWLP_USERDATA, 0);
	}

	DestroyWindow(m_window);
}

void Win32Window::setVisible (bool visible)
{
	ShowWindow(m_window, visible ? SW_SHOW : SW_HIDE);
}

void Win32Window::setSize (int width, int height)
{
	RECT rc;

	rc.left		= 0;
	rc.top		= 0;
	rc.right	= width;
	rc.bottom	= height;

	if (!AdjustWindowRect(&rc, GetWindowLong(m_window, GWL_STYLE), GetMenu(m_window) != NULL))
		TCU_THROW(TestError, "AdjustWindowRect() failed");

	if (!SetWindowPos(m_window, NULL, 0, 0,
					  rc.right - rc.left, rc.bottom - rc.top,
					  SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOZORDER))
		TCU_THROW(TestError, "SetWindowPos() failed");
}

IVec2 Win32Window::getSize (void) const
{
	RECT rc;
	if (!GetClientRect(m_window, &rc))
		TCU_THROW(TestError, "GetClientRect() failed");

	return IVec2(rc.right - rc.left,
				 rc.bottom - rc.top);
}

void Win32Window::processEvents (void)
{
	MSG msg;
	while (PeekMessage(&msg, m_window, 0, 0, PM_REMOVE))
		DispatchMessage(&msg);
}

LRESULT Win32Window::windowProc (UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		// \todo [2014-03-12 pyry] Handle WM_SIZE?

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		case WM_KEYDOWN:
			if (wParam == VK_ESCAPE)
			{
				PostQuitMessage(0);
				return 0;
			}
			// fall-through

		default:
			return DefWindowProc(m_window, uMsg, wParam, lParam);
	}
}

} // tcu
