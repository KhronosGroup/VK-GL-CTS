#ifndef _TCUWAYLANDEGLPLATFORM_HPP
#define _TCUWAYLANDEGLPLATFORM_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright (c) 2014 The Android Open Source Project
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 Mun Gwan-gyeong <elongbug@gmail.com>
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
 * \brief wayland Egl Platform.
 *//*--------------------------------------------------------------------*/

#include "deUniquePtr.hpp"
#include "egluPlatform.hpp"
#include "gluContextFactory.hpp"
#include "tcuWayland.hpp"

namespace tcu
{
namespace wayland
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
} // wayland
} // tcu

#endif // _TCUWAYLANDEGLPLATFORM_HPP
