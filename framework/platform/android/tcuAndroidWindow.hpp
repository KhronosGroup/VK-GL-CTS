#ifndef _TCUANDROIDWINDOW_HPP
#define _TCUANDROIDWINDOW_HPP
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

#include "tcuDefs.hpp"
#include "tcuVector.hpp"
#include "deSemaphore.hpp"

#include <android/native_window.h>

namespace tcu
{
namespace Android
{

class Window
{
public:
						Window						(ANativeWindow* window);
						~Window						(void);

	void				acquire						(void)	{ m_semaphore.decrement();				}
	bool				tryAcquire					(void)	{ return m_semaphore.tryDecrement();	}
	void				release						(void)	{ m_semaphore.increment();				}

	ANativeWindow*		getNativeWindow				(void)	{ return m_window;						}

	void				setBuffersGeometry			(int width, int height, int32_t format);

	IVec2				getSize						(void) const;

private:
						Window						(const Window& other);
	Window&				operator=					(const Window& other);

	ANativeWindow*		m_window;
	de::Semaphore		m_semaphore;
};

} // Android
} // tcu

#endif // _TCUANDROIDWINDOW_HPP
