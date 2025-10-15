#ifndef _VKTSHADEROBJECTUTIL_HPP
#define _VKTSHADEROBJECTUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Utility for shader objects
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"

#include "deUniquePtr.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkImageUtil.hpp"
#include "vkPrograms.hpp"
#include "vktTestCase.hpp"
#include "vkTypeUtil.hpp"
#include "rrRenderer.hpp"
#include <memory>

namespace vkt
{
namespace shaderobjutil
{

#ifndef CTS_USES_VULKANSC
// The original method from the context returns a vector of const char*. This transforms it into the preferred format.
std::vector<std::string> getDeviceCreationExtensions(vkt::Context &context);

// Helper function for tests using shader objects.
void bindShaderObjectState(const vk::DeviceInterface &vkd, const std::vector<std::string> &deviceExtensions,
                           const vk::VkCommandBuffer cmdBuffer, const std::vector<vk::VkViewport> &viewports,
                           const std::vector<vk::VkRect2D> &scissors, const vk::VkPrimitiveTopology topology,
                           const uint32_t patchControlPoints,
                           const vk::VkPipelineVertexInputStateCreateInfo *vertexInputStateCreateInfo,
                           const vk::VkPipelineRasterizationStateCreateInfo *rasterizationStateCreateInfo,
                           const vk::VkPipelineMultisampleStateCreateInfo *multisampleStateCreateInfo,
                           const vk::VkPipelineDepthStencilStateCreateInfo *depthStencilStateCreateInfo,
                           const vk::VkPipelineColorBlendStateCreateInfo *colorBlendStateCreateInfo);
#endif
} // namespace shaderobjutil
} // namespace vkt

#endif // _VKTSHADEROBJECTUTIL_HPP
