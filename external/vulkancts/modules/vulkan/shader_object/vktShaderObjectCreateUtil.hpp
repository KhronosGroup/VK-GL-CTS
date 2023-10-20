#ifndef _VKTSHADEROBJECTCREATEUTIL_HPP
#define _VKTSHADEROBJECTCREATEUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Utilities for vertex buffers.
 *//*--------------------------------------------------------------------*/

#include "vkRef.hpp"
#include "vkResourceInterface.hpp"

namespace vk
{

std::string				getShaderName						(vk::VkShaderStageFlagBits stage);
vk::VkShaderStageFlags	getShaderObjectNextStages			(vk::VkShaderStageFlagBits shaderStage, bool tessellationShaderFeature, bool geometryShaderFeature);

Move<VkShaderEXT>		createShaderFromBinary				(const DeviceInterface& vk, VkDevice device, vk::VkShaderStageFlagBits shaderStage, size_t codeSize, const void* pCode, bool tessellationShaderFeature, bool geometryShaderFeature, vk::VkDescriptorSetLayout descriptorSetLayout);
Move<VkShaderEXT>		createShader						(const DeviceInterface& vk, VkDevice device, const vk::VkShaderCreateInfoEXT& shaderCreateInfo);

void					addBasicShaderObjectShaders	(vk::SourceCollections& programCollection);

vk::VkShaderCreateInfoEXT makeShaderCreateInfo				(vk::VkShaderStageFlagBits stage, const vk::ProgramBinary& programBinary, bool tessellationShaderFeature, bool geometryShaderFeature, const vk::VkDescriptorSetLayout* descriptorSetLayout = DE_NULL);
void					setDefaultShaderObjectDynamicStates	(const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, const std::vector<std::string>& deviceExtensions, vk::VkPrimitiveTopology topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, bool meshShader = false, bool setViewport = false);
void					bindGraphicsShaders					(const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, vk::VkShaderEXT vertShader, vk::VkShaderEXT tescShader, vk::VkShaderEXT teseShader, vk::VkShaderEXT geomShader, vk::VkShaderEXT fragShader, bool taskShaderSupported, bool meshShaderSupported);
void					bindComputeShader					(const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, vk::VkShaderEXT compShader);
void					bindNullTaskMeshShaders				(const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, vk::VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures);
void					bindNullRasterizationShaders		(const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, vk::VkPhysicalDeviceFeatures features);

} // vkt

#endif // _VKTSHADEROBJECTCREATEUTIL_HPP
