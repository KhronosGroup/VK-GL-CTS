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

#include "vkComputePipelineConstructionUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkObjUtil.hpp"

namespace vk
{

void checkShaderObjectRequirements (const InstanceInterface&		vki,
									VkPhysicalDevice				physicalDevice,
									ComputePipelineConstructionType	computePipelineConstructionType)
{
	if (computePipelineConstructionType == COMPUTE_PIPELINE_CONSTRUCTION_TYPE_PIPELINE)
		return;

	const auto& supportedExtensions = enumerateCachedDeviceExtensionProperties(vki, physicalDevice);
	if (!isExtensionStructSupported(supportedExtensions, RequiredExtension("VK_EXT_shader_object")))
		TCU_THROW(NotSupportedError, "VK_EXT_shader_object not supported");
}

struct ComputePipelineWrapper::InternalData
{
	const DeviceInterface&								vk;
	VkDevice											device;
	const ComputePipelineConstructionType				pipelineConstructionType;

	// initialize with most common values
	InternalData(const DeviceInterface& vkd, VkDevice vkDevice, const ComputePipelineConstructionType constructionType)
		: vk						(vkd)
		, device					(vkDevice)
		, pipelineConstructionType	(constructionType)
	{
	}
};

ComputePipelineWrapper::ComputePipelineWrapper (const	DeviceInterface&				vk,
												VkDevice								device,
												const ComputePipelineConstructionType	pipelineConstructionType)
	: m_internalData		(new ComputePipelineWrapper::InternalData(vk, device, pipelineConstructionType))
	, m_programBinary		(DE_NULL)
	, m_specializationInfo	{}
	, m_pipelineCreateFlags	((VkPipelineCreateFlags)0u)
	, m_pipelineCreatePNext	(DE_NULL)
	, m_subgroupSize		(0)
{

}

ComputePipelineWrapper::ComputePipelineWrapper (const DeviceInterface&					vk,
												VkDevice								device,
												const ComputePipelineConstructionType	pipelineConstructionType,
												const ProgramBinary&					programBinary)
	: m_internalData		(new ComputePipelineWrapper::InternalData(vk, device, pipelineConstructionType))
	, m_programBinary		(&programBinary)
	, m_specializationInfo	{}
	, m_pipelineCreateFlags	((VkPipelineCreateFlags)0u)
	, m_pipelineCreatePNext	(DE_NULL)
	, m_subgroupSize		(0)
{
}

ComputePipelineWrapper::ComputePipelineWrapper (const ComputePipelineWrapper& rhs) noexcept
	: m_internalData			(rhs.m_internalData)
	, m_programBinary			(rhs.m_programBinary)
	, m_descriptorSetLayouts	(rhs.m_descriptorSetLayouts)
	, m_specializationInfo		(rhs.m_specializationInfo)
	, m_pipelineCreateFlags		(rhs.m_pipelineCreateFlags)
	, m_pipelineCreatePNext		(rhs.m_pipelineCreatePNext)
	, m_subgroupSize			(rhs.m_subgroupSize)
{
	DE_ASSERT(rhs.m_pipeline.get() == DE_NULL);
#ifndef CTS_USES_VULKANSC
	DE_ASSERT(rhs.m_shader.get() == DE_NULL);
#endif
}

ComputePipelineWrapper::ComputePipelineWrapper (ComputePipelineWrapper&& rhs) noexcept
	: m_internalData			(rhs.m_internalData)
	, m_programBinary			(rhs.m_programBinary)
	, m_descriptorSetLayouts	(rhs.m_descriptorSetLayouts)
	, m_specializationInfo		(rhs.m_specializationInfo)
	, m_pipelineCreateFlags		(rhs.m_pipelineCreateFlags)
	, m_pipelineCreatePNext		(rhs.m_pipelineCreatePNext)
	, m_subgroupSize			(rhs.m_subgroupSize)
{
	DE_ASSERT(rhs.m_pipeline.get() == DE_NULL);
#ifndef CTS_USES_VULKANSC
	DE_ASSERT(rhs.m_shader.get() == DE_NULL);
#endif
}

ComputePipelineWrapper& ComputePipelineWrapper::operator= (const ComputePipelineWrapper& rhs) noexcept
{
	m_internalData = rhs.m_internalData;
	m_programBinary = rhs.m_programBinary;
	m_descriptorSetLayouts = rhs.m_descriptorSetLayouts;
	m_specializationInfo = rhs.m_specializationInfo;
	m_pipelineCreateFlags = rhs.m_pipelineCreateFlags;
	m_pipelineCreatePNext =	rhs.m_pipelineCreatePNext;
	DE_ASSERT(rhs.m_pipeline.get() == DE_NULL);
#ifndef CTS_USES_VULKANSC
	DE_ASSERT(rhs.m_shader.get() == DE_NULL);
#endif
	m_subgroupSize = rhs.m_subgroupSize;
	return *this;
}

ComputePipelineWrapper& ComputePipelineWrapper::operator= (ComputePipelineWrapper&& rhs) noexcept
{
	m_internalData = std::move(rhs.m_internalData);
	m_programBinary = rhs.m_programBinary;
	m_descriptorSetLayouts = std::move(rhs.m_descriptorSetLayouts);
	m_specializationInfo = rhs.m_specializationInfo;
	m_pipelineCreateFlags = rhs.m_pipelineCreateFlags;
	m_pipelineCreatePNext =	rhs.m_pipelineCreatePNext;
	DE_ASSERT(rhs.m_pipeline.get() == DE_NULL);
#ifndef CTS_USES_VULKANSC
	DE_ASSERT(rhs.m_shader.get() == DE_NULL);
#endif
	m_subgroupSize = rhs.m_subgroupSize;
	return *this;
}

void ComputePipelineWrapper::setDescriptorSetLayout (VkDescriptorSetLayout descriptorSetLayout)
{
	m_descriptorSetLayouts = { descriptorSetLayout };
}

void ComputePipelineWrapper::setDescriptorSetLayouts (deUint32 setLayoutCount, const VkDescriptorSetLayout* descriptorSetLayouts)
{
	m_descriptorSetLayouts.assign(descriptorSetLayouts, descriptorSetLayouts + setLayoutCount);
}

void ComputePipelineWrapper::setSpecializationInfo (VkSpecializationInfo specializationInfo)
{
	m_specializationInfo = specializationInfo;
}

void ComputePipelineWrapper::setPipelineCreateFlags (VkPipelineCreateFlags pipelineCreateFlags)
{
	m_pipelineCreateFlags = pipelineCreateFlags;
}

void ComputePipelineWrapper::setPipelineCreatePNext (void* pipelineCreatePNext)
{
	m_pipelineCreatePNext = pipelineCreatePNext;
}

void ComputePipelineWrapper::setSubgroupSize (uint32_t subgroupSize)
{
	m_subgroupSize = subgroupSize;
}
void ComputePipelineWrapper::buildPipeline (void)
{
	const auto& vk		= m_internalData->vk;
	const auto& device	= m_internalData->device;

	VkSpecializationInfo* specializationInfo = m_specializationInfo.mapEntryCount > 0 ? &m_specializationInfo : DE_NULL;
	if (m_internalData->pipelineConstructionType == COMPUTE_PIPELINE_CONSTRUCTION_TYPE_PIPELINE)
	{
		DE_ASSERT(m_pipeline.get() == DE_NULL);
		const Unique<VkShaderModule>	shaderModule	(createShaderModule(vk, device, *m_programBinary));
		buildPipelineLayout();
		m_pipeline = vk::makeComputePipeline(vk, device, *m_pipelineLayout, m_pipelineCreateFlags, m_pipelineCreatePNext, *shaderModule, 0u, specializationInfo, 0, m_subgroupSize);
	}
	else
	{
#ifndef CTS_USES_VULKANSC
		DE_ASSERT(m_shader.get() == DE_NULL);
		buildPipelineLayout();
		vk::VkShaderCreateInfoEXT		createInfo =
		{
			vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			0u,												// VkShaderCreateFlagsEXT		flags;
			vk::VK_SHADER_STAGE_COMPUTE_BIT,				// VkShaderStageFlagBits		stage;
			0u,												// VkShaderStageFlags			nextStage;
			vk::VK_SHADER_CODE_TYPE_SPIRV_EXT,				// VkShaderCodeTypeEXT			codeType;
			m_programBinary->getSize(),						// size_t						codeSize;
			m_programBinary->getBinary(),					// const void*					pCode;
			"main",											// const char*					pName;
			(deUint32)m_descriptorSetLayouts.size(),		// uint32_t						setLayoutCount;
			m_descriptorSetLayouts.data(),					// VkDescriptorSetLayout*		pSetLayouts;
			0u,												// uint32_t						pushConstantRangeCount;
			DE_NULL,										// const VkPushConstantRange*	pPushConstantRanges;
			specializationInfo,								// const VkSpecializationInfo*	pSpecializationInfo;
		};

		m_shader = createShader(vk, device, createInfo);

		if (m_internalData->pipelineConstructionType == COMPUTE_PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_BINARY)
		{
			size_t dataSize;
			vk.getShaderBinaryDataEXT(device, *m_shader, &dataSize, DE_NULL);
			std::vector<deUint8> data(dataSize);
			vk.getShaderBinaryDataEXT(device, *m_shader, &dataSize, data.data());

			createInfo.codeType = vk::VK_SHADER_CODE_TYPE_BINARY_EXT;
			createInfo.codeSize = dataSize;
			createInfo.pCode = data.data();

			m_shader = createShader(vk, device, createInfo);
		}
#endif
	}
}

void ComputePipelineWrapper::bind (VkCommandBuffer commandBuffer)
{
	if (m_internalData->pipelineConstructionType == COMPUTE_PIPELINE_CONSTRUCTION_TYPE_PIPELINE)
	{
		m_internalData->vk.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline.get());
	}
	else
	{
#ifndef CTS_USES_VULKANSC
		const vk::VkShaderStageFlagBits stage = vk::VK_SHADER_STAGE_COMPUTE_BIT;
		m_internalData->vk.cmdBindShadersEXT(commandBuffer, 1, &stage, &*m_shader);
#endif
	}
}

void ComputePipelineWrapper::buildPipelineLayout (void)
{
	m_pipelineLayout = makePipelineLayout(m_internalData->vk, m_internalData->device, m_descriptorSetLayouts);
}

VkPipelineLayout ComputePipelineWrapper::getPipelineLayout (void)
{
	return *m_pipelineLayout;
}

} // vk
