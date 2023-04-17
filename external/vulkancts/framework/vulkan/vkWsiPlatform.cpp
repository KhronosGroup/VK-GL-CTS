/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2016 Google Inc.
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
 * \brief WSI Platform Abstraction.
 *//*--------------------------------------------------------------------*/

#include "vkWsiPlatform.hpp"

namespace vk
{
namespace wsi
{

void Window::setVisible (bool visible)
{
	DE_UNREF(visible);
	TCU_THROW(InternalError, "setVisible() called on window not supporting it");
}

void Window::setForeground(void)
{
}

void Window::resize (const tcu::UVec2&)
{
	TCU_THROW(InternalError, "resize() called on window not supporting it");
}

void Window::setMinimized (bool minimized)
{
	DE_UNREF(minimized);
	TCU_THROW(InternalError, "setMinimized() called on window not supporting it");
}

} // wsi
} // vk
