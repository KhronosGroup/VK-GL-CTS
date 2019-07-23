/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Object creation utilities
 *//*--------------------------------------------------------------------*/

#include "vktFragmentOperationsMakeUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include <vector>

namespace vkt
{
namespace FragmentOperations
{
using namespace vk;
using de::MovePtr;

Move<VkPipeline> makeComputePipeline (const DeviceInterface&		vk,
									  const VkDevice				device,
									  const VkPipelineLayout		pipelineLayout,
									  const VkShaderModule			shaderModule,
									  const VkSpecializationInfo*	specInfo)
{
	const VkPipelineShaderStageCreateInfo shaderStageInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags	flags;
		VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits			stage;
		shaderModule,											// VkShaderModule					module;
		"main",													// const char*						pName;
		specInfo,												// const VkSpecializationInfo*		pSpecializationInfo;
	};
	const VkComputePipelineCreateInfo pipelineInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,		// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		(VkPipelineCreateFlags)0,							// VkPipelineCreateFlags			flags;
		shaderStageInfo,									// VkPipelineShaderStageCreateInfo	stage;
		pipelineLayout,										// VkPipelineLayout					layout;
		DE_NULL,											// VkPipeline						basePipelineHandle;
		0,													// deInt32							basePipelineIndex;
	};
	return createComputePipeline(vk, device, DE_NULL , &pipelineInfo);
}

} // FragmentOperations
} // vkt
