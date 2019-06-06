/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 * Copyright (c) 2019 Intel Corporation
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
 * \brief Stub no-op version of RenderDocUtil class
 *//*--------------------------------------------------------------------*/

#include "vkRenderDocUtil.hpp"

namespace vk
{

struct RenderDocPrivate
{
};

RenderDocUtil::RenderDocUtil (void)
{
}

RenderDocUtil::~RenderDocUtil (void)
{
}

bool RenderDocUtil::isValid (void)
{
	return false;
}

void RenderDocUtil::startFrame (vk::VkInstance instance)
{
	DE_UNREF(instance);
}

void RenderDocUtil::endFrame (vk::VkInstance instance)
{
	DE_UNREF(instance);
}

} // vk
