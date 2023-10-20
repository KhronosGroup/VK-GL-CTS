#ifndef _VKCOMPUTEPIPELINECONSTRUCTIONUTIL_HPP
#define _VKCOMPUTEPIPELINECONSTRUCTIONUTIL_HPP
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
 * \brief Wrapper that can construct monolithic pipeline or use
		  VK_EXT_shader_object for compute pipeline construction.
 *//*--------------------------------------------------------------------*/

#include "vkRef.hpp"
#include "deSharedPtr.hpp"
#include "vkPrograms.hpp"
#include "vkPipelineConstructionUtil.hpp"

namespace vk
{

enum ComputePipelineConstructionType
{
	COMPUTE_PIPELINE_CONSTRUCTION_TYPE_PIPELINE	= 0,				// Construct monolithic pipeline
	COMPUTE_PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_SPIRV,			// Use VK_EXT_shader_object and construct a shader object from spirv
	COMPUTE_PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_BINARY,		// Use VK_EXT_shader_object and construct a shader object from binary
};

inline ComputePipelineConstructionType graphicsToComputeConstructionType (PipelineConstructionType pipelineConstructionType)
{
	if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV || pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_LINKED_SPIRV)
		return COMPUTE_PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_SPIRV;
	if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_BINARY || pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_LINKED_BINARY)
		return COMPUTE_PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_BINARY;
	return COMPUTE_PIPELINE_CONSTRUCTION_TYPE_PIPELINE;
}

void checkShaderObjectRequirements (const InstanceInterface&		vki,
									VkPhysicalDevice				physicalDevice,
									ComputePipelineConstructionType	computePipelineConstructionType);

class ComputePipelineWrapper
{
public:
										ComputePipelineWrapper	() = default;
										ComputePipelineWrapper	(const DeviceInterface&					vk,
																 VkDevice								device,
																 const ComputePipelineConstructionType	pipelineConstructionType);
										ComputePipelineWrapper	(const DeviceInterface&					vk,
																 VkDevice								device,
																 const ComputePipelineConstructionType	pipelineConstructionType,
																 const ProgramBinary&					programBinary);

										ComputePipelineWrapper	(const ComputePipelineWrapper&) noexcept;
										ComputePipelineWrapper	(ComputePipelineWrapper&&) noexcept;
										~ComputePipelineWrapper	(void) = default;

	ComputePipelineWrapper&				operator=				(const ComputePipelineWrapper& rhs) noexcept;
	ComputePipelineWrapper&				operator=				(ComputePipelineWrapper&& rhs) noexcept;

	void								setDescriptorSetLayout	(VkDescriptorSetLayout descriptorSetLayout);
	void								setDescriptorSetLayouts	(deUint32 setLayoutCount, const VkDescriptorSetLayout* descriptorSetLayouts);
	void								setSpecializationInfo	(VkSpecializationInfo specializationInfo);
	void								setPipelineCreateFlags	(VkPipelineCreateFlags pipelineCreateFlags);
	void								setPipelineCreatePNext	(void* pipelineCreatePNext);
	void								setSubgroupSize			(uint32_t subgroupSize);
	void								buildPipeline			(void);
	void								bind					(VkCommandBuffer commandBuffer);

	VkPipelineLayout					getPipelineLayout		(void);

private:
	void								buildPipelineLayout		(void);

	struct InternalData;

	// Store internal data that is needed only for pipeline construction.
	de::SharedPtr<InternalData>			m_internalData;
	const ProgramBinary*				m_programBinary;
	std::vector<VkDescriptorSetLayout>	m_descriptorSetLayouts;
	VkSpecializationInfo				m_specializationInfo;
	VkPipelineCreateFlags				m_pipelineCreateFlags;
	void*								m_pipelineCreatePNext;
	uint32_t							m_subgroupSize;

	Move<VkPipeline>					m_pipeline;
	Move<VkPipelineLayout>				m_pipelineLayout;
#ifndef CTS_USES_VULKANSC
	Move<VkShaderEXT>					m_shader;
#endif
};

} // vk

#endif // _VKCOMPUTEPIPELINECONSTRUCTIONUTIL_HPP
