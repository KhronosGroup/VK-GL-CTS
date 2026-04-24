#ifndef _VKTTENSORSHADERUTIL_HPP
#define _VKTTENSORSHADERUTIL_HPP

/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 ARM Ltd.
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
 */
/*!
 * \file
 * \brief Tensor Shader Utility Classes
 */
/*--------------------------------------------------------------------*/

#include "../vktTensorTestsUtil.hpp"
#include "../vktTestCase.hpp"

namespace vkt
{
namespace tensor
{

using namespace vk;

std::string getTensorFormat(VkFormat format);
std::string getBooleanOp(BooleanOperator op);

} // namespace tensor
} // namespace vkt
#endif // _VKTTENSORSHADERUTIL_HPP
