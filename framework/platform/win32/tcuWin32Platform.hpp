#ifndef _TCUWIN32PLATFORM_HPP
#define _TCUWIN32PLATFORM_HPP
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
 * \brief Win32 platform port.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuPlatform.hpp"
#include "gluPlatform.hpp"
#include "tcuWin32API.h"

#if defined(DEQP_SUPPORT_EGL)
#	ifndef _EGLUPLATFORM_HPP
#		include "egluPlatform.hpp"
#	endif
#endif

namespace tcu
{

class Win32Platform : public tcu::Platform, private glu::Platform
#if defined(DEQP_SUPPORT_EGL)
	, private eglu::Platform
#endif
{
public:
							Win32Platform		(void);
							~Win32Platform		(void);

	bool					processEvents		(void);

	const glu::Platform&	getGLPlatform		(void) const { return static_cast<const glu::Platform&>(*this);		}

#if defined(DEQP_SUPPORT_EGL)
	const eglu::Platform&	getEGLPlatform		(void) const { return static_cast<const eglu::Platform&>(*this);	}
#endif

private:
	HINSTANCE				m_instance;
};

} // tcu

#endif // _TCUWIN32PLATFORM_HPP
