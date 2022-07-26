#ifndef _VKTDRAWGROUPPARAMS_HPP
#define _VKTDRAWGROUPPARAMS_HPP
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
 * \brief Structure with parameters shared by all tests in 'draw' group.
 *//*--------------------------------------------------------------------*/

#include "deSharedPtr.hpp"

namespace vkt
{
namespace Draw
{

// Structure containing parameters for all tests in draw group
struct GroupParams
{
	// When this flag is set tests use dynamic rendering, otherwise renderpass object is used.
	bool useDynamicRendering;

	// When this flag is true then secondary command buffer is created in test
	bool useSecondaryCmdBuffer;

	// When true begin/endRendering is in secondary command buffer, when false those
	// commands are recorded to primary command buffer. This flag is checked only when
	// useSecondaryCmdBuffer is true.
	bool secondaryCmdBufferCompletelyContainsDynamicRenderpass;
};

typedef de::SharedPtr<GroupParams> SharedGroupParams;

} // Draw
} // vkt

#endif // _VKTDRAWGROUPPARAMS_HPP
