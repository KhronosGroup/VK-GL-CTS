#ifndef _TCUX11EGLPLATFORM_HPP
#define _TCUX11EGLPLATFORM_HPP
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
 * \brief X11Egl Platform.
 *//*--------------------------------------------------------------------*/

#include "deUniquePtr.hpp"
#include "egluPlatform.hpp"
#include "gluContextFactory.hpp"
#include "tcuX11.hpp"

namespace tcu
{
namespace x11
{
namespace egl
{

class Platform : public eglu::Platform
{
public:
										Platform				(EventState& eventState);
										~Platform				(void) {}

	de::MovePtr<glu::ContextFactory>	createContextFactory	(void);
};

}
} // x11
} // tcu

#endif // _TCUX11EGLPLATFORM_HPP
