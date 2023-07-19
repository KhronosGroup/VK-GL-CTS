#ifndef _VKTSYNCHRONIZATIONDEFS_HPP
#define _VKTSYNCHRONIZATIONDEFS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
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
 * \brief Synchronization definitions
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"

namespace vkt
{
namespace synchronization
{

#ifndef CTS_USES_VULKANSC
	typedef vk::VkVideoCodecOperationFlagsKHR VideoCodecOperationFlags;
#else
	// Can be replaced when/if video extension will be promoted into SC
	typedef uint32_t VideoCodecOperationFlags;
#endif

} // synchronization
} // vkt

#endif // _VKTSYNCHRONIZATIONDEFS_HPP
