#ifndef _VKRENDERDOCUTIL_HPP
#define _VKRENDERDOCUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 * Copyright (c) 2018 Intel Corporation
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
 * \brief VK_EXT_debug_report utilities
 *//*--------------------------------------------------------------------*/

#include <vkDefs.hpp>

namespace vk
{

struct RenderDocPrivate;

class RenderDocUtil
{
public:
											RenderDocUtil		(void);
											~RenderDocUtil		(void);

	bool									isValid				(void);

	void									startFrame			(vk::VkInstance);
	void									endFrame			(vk::VkInstance);

private:
	RenderDocPrivate *						m_priv;
};

} // vk

#endif // _VKRENDERDOCUTIL_HPP
