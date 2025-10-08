#ifndef _VKTPIPELINEBLENDTESTSCOMMON_HPP
#define _VKTPIPELINEBLENDTESTSCOMMON_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
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
 * \brief Blending Tests Common Things
 *//*--------------------------------------------------------------------*/

#include "vkTypeUtil.hpp"
#include <string>
#include <vector>

namespace vkt
{
namespace pipeline
{
namespace blending_common
{
using namespace vk;

// Formats that are dEQP-compatible, non-integer and uncompressed
const std::vector<VkFormat> &getBlendFormats();
const std::vector<VkBlendFactor> &getBlendFactors();
const std::vector<VkBlendFactor> &getBlendWithDualSourceFactors();
const std::vector<VkBlendFactor> getDualSourceBlendFactors();
const std::vector<VkBlendOp> &getBlendOps();

std::string getFormatCaseName(VkFormat format);
std::string getBlendStateName(const VkPipelineColorBlendAttachmentState &blendState);
bool isSupportedBlendFormat(const InstanceInterface &instanceInterface, VkPhysicalDevice device, VkFormat format);
bool isSupportedTransferFormat(const InstanceInterface &instanceInterface, VkPhysicalDevice device, VkFormat format);
bool isSrc1BlendFactor(vk::VkBlendFactor blendFactor);
bool isSrc1BlendFactor(const VkPipelineColorBlendAttachmentState &state);
bool isAlphaBlendFactor(vk::VkBlendFactor blendFactor);
bool isAlphaBlendFactor(const VkPipelineColorBlendAttachmentState &state);

} // namespace blending_common
} // namespace pipeline
} // namespace vkt

#endif // _VKTPIPELINEBLENDTESTSCOMMON_HPP
