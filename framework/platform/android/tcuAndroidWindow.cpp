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
 * \brief Android window.
 *//*--------------------------------------------------------------------*/

#include "tcuAndroidWindow.hpp"

namespace tcu
{
namespace Android
{

Window::Window (ANativeWindow* window)
	: m_window		(window)
	, m_semaphore	(1)
{
}

Window::~Window (void)
{
}

void Window::setBuffersGeometry (int width, int height, int32_t format)
{
	ANativeWindow_setBuffersGeometry(m_window, width, height, format);
}

IVec2 Window::getSize (void) const
{
	const int32_t	width	= ANativeWindow_getWidth(m_window);
	const int32_t	height	= ANativeWindow_getHeight(m_window);
	return IVec2(width, height);
}

} // Android
} // tcu
