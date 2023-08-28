/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
 * Copyright (c) 2023 Google LLC.
 * Copyright (c) 2023 LunarG, Inc.
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
 * \brief Test robustness with pipeline cache
 *//*--------------------------------------------------------------------*/

#include "vktPipelineRobustnessCacheTests.hpp"

#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkPipelineConstructionUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"

namespace vkt
{
namespace pipeline
{

enum RobustnessBehaviour
{
	ROBUSTNESS = 0,
	ROBUSTNESS_2 = 1,
};

enum RobustnessType
{
	STORAGE = 0,
	UNIFORM,
	VERTEX_INPUT,
	IMAGE,
};

namespace
{

std::unique_ptr<vk::BufferWithMemory> makeBufferForImage(const vk::DeviceInterface& vk, const vk::VkDevice device, vk::Allocator& allocator, vk::VkFormat imageFormat, vk::VkExtent2D imageExtent)
{
	const auto	tcuFormat = mapVkFormat(imageFormat);
	const auto	outBufferSize = static_cast<vk::VkDeviceSize>(static_cast<uint32_t>(tcu::getPixelSize(tcuFormat)) * imageExtent.width * imageExtent.height);
	const auto	outBufferUsage = vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const auto	outBufferInfo = vk::makeBufferCreateInfo(outBufferSize, outBufferUsage);

	auto outBuffer = std::unique_ptr<vk::BufferWithMemory>(new vk::BufferWithMemory(vk, device, allocator, outBufferInfo, vk::MemoryRequirement::HostVisible));

	return outBuffer;
}

vk::VkImageCreateInfo makeImageCreateInfo(vk::VkFormat format, vk::VkExtent3D extent, vk::VkImageUsageFlags usage)
{
	const vk::VkImageCreateInfo imageCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,									//	const void*				pNext;
		(vk::VkImageCreateFlags)0u,						//	VkImageCreateFlags		flags;
		vk::VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		format,										//	VkFormat				format;
		extent,										//	VkExtent3D				extent;
		1u,											//	deUint32				mipLevels;
		1u,											//	deUint32				arrayLayers;
		vk::VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		vk::VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		usage,										//	VkImageUsageFlags		usage;
		vk::VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,											//	deUint32				queueFamilyIndexCount;
		nullptr,									//	const deUint32*			pQueueFamilyIndices;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};

	return imageCreateInfo;
}

vk::Move<vk::VkSampler>	makeSampler(const vk::DeviceInterface& vk, const vk::VkDevice device)
{
	const vk::VkSamplerCreateInfo		samplerInfo =
	{
		vk::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,	// sType
		DE_NULL,									// pNext
		0u,											// flags
		vk::VK_FILTER_NEAREST,						// magFilter
		vk::VK_FILTER_NEAREST,						// minFilter
		vk::VK_SAMPLER_MIPMAP_MODE_NEAREST,			// mipmapMode
		vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,	// addressModeU
		vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,	// addressModeV
		vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,	// addressModeW
		0.0f,										// mipLodBias
		VK_FALSE,									// anisotropyEnable
		1.0f,										// maxAnisotropy
		DE_FALSE,									// compareEnable
		vk::VK_COMPARE_OP_ALWAYS,					// compareOp
		0.0f,										// minLod
		1.0f,										// maxLod
		vk::VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,// borderColor
		VK_FALSE,									// unnormalizedCoords
	};

	return createSampler(vk, device, &samplerInfo);
}

class PipelineCacheTestInstance : public vkt::TestInstance
{
public:
	PipelineCacheTestInstance (vkt::Context& context, vk::PipelineConstructionType pipelineConstructionType, RobustnessBehaviour robustnessBufferBehaviour, RobustnessType type)
		: vkt::TestInstance				(context)
		, m_pipelineConstructionType	(pipelineConstructionType)
		, m_robustnessBufferBehaviour	(robustnessBufferBehaviour)
		, m_type						(type)
		, m_extent						()
	{
	}

private:
	void						draw		(const vk::GraphicsPipelineWrapper& pipeline);
	bool						verifyImage	(tcu::Vec4 value, bool oob);
	tcu::TestStatus				iterate		(void);

	const vk::PipelineConstructionType		m_pipelineConstructionType;
	const RobustnessBehaviour				m_robustnessBufferBehaviour;
	const RobustnessType					m_type;

	vk::VkExtent2D							m_extent;
	vk::Move	<vk::VkCommandPool>			m_cmdPool;
	vk::Move<vk::VkCommandBuffer>			m_cmdBuffer;
	de::MovePtr<vk::BufferWithMemory>		m_buffer;
	vk::RenderPassWrapper					m_renderPass;
	vk::PipelineLayoutWrapper				m_pipelineLayout;
	vk::Move<vk::VkDescriptorPool>			m_descriptorPool;
	vk::Move<vk::VkDescriptorSet>			m_descriptorSet;
	de::MovePtr<vk::ImageWithMemory>		m_colorAttachment;
	std::unique_ptr<vk::BufferWithMemory>	m_outBuffer;
};

void PipelineCacheTestInstance::draw (const vk::GraphicsPipelineWrapper& pipeline)
{
	const vk::DeviceInterface&					vk		= m_context.getDeviceInterface();
	const vk::VkDevice							device	= m_context.getDevice();
	const vk::VkQueue							queue	= m_context.getUniversalQueue();

	const tcu::Vec4								clearColor(0.0f, 0.0f, 0.0f, 1.0f);

	vk::beginCommandBuffer(vk, *m_cmdBuffer);
	if (m_type == VERTEX_INPUT)
	{
		vk::VkDeviceSize offset = 0u;
		vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &**m_buffer, &offset);
	}
	m_renderPass.begin(vk, *m_cmdBuffer, makeRect2D(m_extent), clearColor);
	vk.cmdBindDescriptorSets(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0, 1, &*m_descriptorSet, 0, DE_NULL);
	pipeline.bind(*m_cmdBuffer);
	vk.cmdDraw(*m_cmdBuffer, 4, 1, 0, 0);
	m_renderPass.end(vk, *m_cmdBuffer);
	vk::endCommandBuffer(vk, *m_cmdBuffer);

	vk::submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

	vk::beginCommandBuffer(vk, *m_cmdBuffer);
	vk::copyImageToBuffer(vk, *m_cmdBuffer, m_colorAttachment->get(), (*m_outBuffer).get(), tcu::IVec2(m_extent.width, m_extent.height));
	vk::endCommandBuffer(vk, *m_cmdBuffer);
	vk::submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);
}

bool PipelineCacheTestInstance::verifyImage (tcu::Vec4 value, bool oob)
{
	const vk::DeviceInterface&					vk				= m_context.getDeviceInterface();
	const vk::VkDevice							device			= m_context.getDevice();

	auto&										outBufferAlloc	= m_outBuffer->getAllocation();

	invalidateAlloc(vk, device, outBufferAlloc);
	const tcu::ConstPixelBufferAccess result(vk::mapVkFormat(vk::VK_FORMAT_R32G32B32A32_SFLOAT), tcu::IVec3(m_extent.width, m_extent.height, 1), (const char*)outBufferAlloc.getHostPtr());

	const deUint32 h = result.getHeight();
	const deUint32 w = result.getWidth();
	for (deUint32 y = 0; y < h; y++)
	{
		for (deUint32 x = 0; x < w; x++)
		{
			tcu::Vec4	pix = result.getPixel(x, y);

			if (oob && m_type == IMAGE)
			{
				for (deUint32 i = 0; i < 4; ++i)
					if (pix[i] != 0.0f && pix[i] != 1.0f)
						return false;
			}
			else if (pix != value)
				return false;
		}
	}
	return true;
}

tcu::TestStatus PipelineCacheTestInstance::iterate (void)
{
	const vk::InstanceInterface&				vki						= m_context.getInstanceInterface();
	const vk::DeviceInterface&					vk						= m_context.getDeviceInterface();
	const vk::VkPhysicalDevice					physicalDevice			= m_context.getPhysicalDevice();
	const vk::VkDevice							device					= m_context.getDevice();
	const deUint32								queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	const vk::VkQueue							queue					= m_context.getUniversalQueue();
	auto&										alloc					= m_context.getDefaultAllocator();
	const auto&									deviceExtensions		= m_context.getDeviceExtensions();

	m_extent		= {32, 32};
	const deUint32								bufferSize				= sizeof(float) * 4u;
	const deUint32								indexBufferSize			= sizeof(deUint32);

	const auto									subresourceRange		= vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

	m_cmdPool		= createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
	m_cmdBuffer		= (allocateCommandBuffer(vk, device, *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	m_buffer		= de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(vk, device, alloc, vk::makeBufferCreateInfo(bufferSize, vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT | vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT | vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | vk::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), vk::MemoryRequirement::HostVisible));

	de::MovePtr<vk::BufferWithMemory>			indexBuffer				= de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(vk, device, alloc, vk::makeBufferCreateInfo(indexBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT | vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), vk::MemoryRequirement::HostVisible));
	de::MovePtr<vk::ImageWithMemory>			image					= de::MovePtr<vk::ImageWithMemory>(new vk::ImageWithMemory(vk, device, alloc, makeImageCreateInfo(vk::VK_FORMAT_R32G32B32A32_SFLOAT, {1, 1, 1}, vk::VK_IMAGE_USAGE_STORAGE_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT), vk::MemoryRequirement::Any));
	const auto									imageView				= makeImageView(vk, device, image->get(), vk::VK_IMAGE_VIEW_TYPE_2D, vk::VK_FORMAT_R32G32B32A32_SFLOAT, subresourceRange);
	const auto									sampler					= makeSampler(vk, device);

	auto&										bufferAlloc				= m_buffer->getAllocation();
	auto&										indexBufferAlloc		= indexBuffer->getAllocation();
	const float									values[4]				= { 0.5f, 0.5f, 0.5f, 0.5f };
	deMemcpy(bufferAlloc.getHostPtr(), values, sizeof(float) * 4);
	flushAlloc(vk, device, bufferAlloc);
	const deUint32								index					= 0u;
	deMemcpy(indexBufferAlloc.getHostPtr(), &index, sizeof(deUint32));
	flushAlloc(vk, device, indexBufferAlloc);

	const vk::VkDescriptorBufferInfo			descriptorBufferInfo	(makeDescriptorBufferInfo(m_buffer->get(), 0, bufferSize));
	const vk::VkDescriptorImageInfo				descriptorImageInfo		(makeDescriptorImageInfo(sampler.get(), imageView.get(), vk::VK_IMAGE_LAYOUT_GENERAL));
	const vk::VkDescriptorBufferInfo			indexBufferInfo			(makeDescriptorBufferInfo(indexBuffer->get(), 0, indexBufferSize));

	const std::vector<vk::VkViewport>			viewports				{ makeViewport(m_extent) };
	const std::vector<vk::VkRect2D>				scissors				{ makeRect2D(m_extent) };

	vk::ShaderWrapper							vert					= vk::ShaderWrapper(vk, device, m_context.getBinaryCollection().get("vert"));
	vk::ShaderWrapper							frag					= vk::ShaderWrapper(vk, device, m_context.getBinaryCollection().get("frag"));

	vk::VkDescriptorType						descriptorType			= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	if (m_type == STORAGE)
	{
		descriptorType = vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	}
	else if (m_type == UNIFORM)
	{
		descriptorType = vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	}
	else if (m_type == IMAGE)
	{
		descriptorType = vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	}

	const auto									descriptorSetLayout		(vk::DescriptorSetLayoutBuilder()
		.addSingleBinding(descriptorType, vk::VK_SHADER_STAGE_FRAGMENT_BIT)
		.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_VERTEX_BIT | vk::VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(vk, device));

	m_pipelineLayout		= vk::PipelineLayoutWrapper(m_pipelineConstructionType, vk, device, *descriptorSetLayout);

	m_descriptorPool		= (vk::DescriptorPoolBuilder()
		.addType(descriptorType)
		.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1));
												m_descriptorSet			= makeDescriptorSet(vk, device, *m_descriptorPool, *descriptorSetLayout);
	vk::DescriptorSetUpdateBuilder				builder;
	if (m_type == STORAGE || m_type == UNIFORM)
		builder.writeSingle(*m_descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType, &descriptorBufferInfo);
	if (m_type == IMAGE)
		builder.writeSingle(*m_descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType, &descriptorImageInfo);
	builder.writeSingle(*m_descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &indexBufferInfo);
	builder.update(vk, device);;

	//buffer to read the output image
	m_outBuffer				= makeBufferForImage(vk, device, alloc, vk::VK_FORMAT_R32G32B32A32_SFLOAT, m_extent);

	const auto									vertModule				= vk::createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"));
	const auto									fragModule				= vk::createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"));

	// Color attachment.
	const vk::VkImageCreateInfo					imageCreateInfo			=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,									//	const void*				pNext;
		0u,											//	VkImageCreateFlags		flags;
		vk::VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		vk::VK_FORMAT_R32G32B32A32_SFLOAT,			//	VkFormat				format;
		{ m_extent.width, m_extent.height, 1},		//	VkExtent3D				extent;
		1u,											//	deUint32				mipLevels;
		1u,											//	deUint32				arrayLayers;
		vk::VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		vk::VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT,	//	VkImageUsageFlags		usage;
		vk::VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,											//	deUint32				queueFamilyIndexCount;
		DE_NULL,									//	const deUint32*			pQueueFamilyIndices;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};

	m_colorAttachment		= de::MovePtr<vk::ImageWithMemory>(new vk::ImageWithMemory(vk, device, alloc, imageCreateInfo, vk::MemoryRequirement::Any));
	const auto									colorAttachmentView		= makeImageView(vk, device, m_colorAttachment->get(), vk::VK_IMAGE_VIEW_TYPE_2D, vk::VK_FORMAT_R32G32B32A32_SFLOAT, subresourceRange);

	m_renderPass			= vk::RenderPassWrapper(m_pipelineConstructionType, vk, device, vk::VK_FORMAT_R32G32B32A32_SFLOAT);
	m_renderPass.createFramebuffer(vk, device, **m_colorAttachment, *colorAttachmentView, 32, 32);

	vk::VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	//	VkStructureType								sType;
		nullptr,														//	const void*									pNext;
		0u,																//	VkPipelineVertexInputStateCreateFlags		flags;
		0u,																//	deUint32									vertexBindingDescriptionCount;
		DE_NULL,														//	const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
		0u,																//	deUint32									vertexAttributeDescriptionCount;
		DE_NULL,														//	const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	vk::VkVertexInputBindingDescription bindingDescription;
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(float);
	bindingDescription.inputRate = vk::VK_VERTEX_INPUT_RATE_INSTANCE;

	std::vector<vk::VkVertexInputAttributeDescription> attributeDescriptions(16);
	for (deUint32 i = 0; i < (deUint32)attributeDescriptions.size(); ++i)
	{
		attributeDescriptions[i].location = i;
		attributeDescriptions[i].binding = 0;
		attributeDescriptions[i].format = vk::VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[i].offset = (deUint32)(sizeof(float) * i);
	}

	if (m_type == VERTEX_INPUT)
	{
		vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1u;
		vertexInputStateCreateInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputStateCreateInfo.vertexAttributeDescriptionCount = (deUint32)attributeDescriptions.size();
		vertexInputStateCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
	}

	// Input assembly.
	const vk::VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,															//	const void*								pNext;
		0u,																	//	VkPipelineInputAssemblyStateCreateFlags	flags;
		vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,							//	VkPrimitiveTopology						topology;
		DE_FALSE,															//	VkBool32								primitiveRestartEnable;
	};

	const vk::VkPipelineCacheCreateInfo			pipelineCacheCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,		// VkStructureType				sType;
		DE_NULL,												// const void*					pNext;
		0u,														// VkPipelineCacheCreateFlags	flags;
		0u,														// deUintptr					initialDataSize;
		DE_NULL,												// const void*					pInitialData;
	};

	vk::Move<vk::VkPipelineCache>				pipelineCache			= createPipelineCache(vk, device, &pipelineCacheCreateInfo);

	vk::GraphicsPipelineWrapper					graphicsPipeline		(vki, vk, physicalDevice, device, deviceExtensions, m_pipelineConstructionType);
	graphicsPipeline.setDefaultTopology(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
					.setDefaultRasterizationState()
					.setDefaultMultisampleState()
					.setDefaultDepthStencilState()
					.setDefaultColorBlendState()
					.setupVertexInputState(&vertexInputStateCreateInfo, &inputAssemblyStateCreateInfo)
					.setupPreRasterizationShaderState(viewports,
						scissors,
						m_pipelineLayout,
						*m_renderPass,
						0u,
						vert)
					.setupFragmentShaderState(m_pipelineLayout, *m_renderPass, 0u, frag)
					.setupFragmentOutputState(*m_renderPass)
					.setMonolithicPipelineLayout(m_pipelineLayout)
					.buildPipeline(*pipelineCache);

	vk::VkPipelineRobustnessCreateInfoEXT		pipelineRobustnessInfo = vk::initVulkanStructure();

	if (m_robustnessBufferBehaviour == ROBUSTNESS)
	{
		if (m_type == STORAGE) pipelineRobustnessInfo.storageBuffers = vk::VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_EXT;
		else if (m_type == UNIFORM) pipelineRobustnessInfo.uniformBuffers = vk::VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_EXT;
		else if (m_type == VERTEX_INPUT) pipelineRobustnessInfo.vertexInputs = vk::VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_EXT;
		else if (m_type == IMAGE) pipelineRobustnessInfo.images = vk::VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_ROBUST_IMAGE_ACCESS_EXT;
	}
	else
	{
		if (m_type == STORAGE) pipelineRobustnessInfo.storageBuffers = vk::VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT;
		else if (m_type == UNIFORM) pipelineRobustnessInfo.uniformBuffers = vk::VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT;
		else if (m_type == VERTEX_INPUT) pipelineRobustnessInfo.vertexInputs = vk::VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT;
		else if (m_type == IMAGE) pipelineRobustnessInfo.images = vk::VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_ROBUST_IMAGE_ACCESS_2_EXT;
	}

	vk::GraphicsPipelineWrapper					robustPipeline			(vki, vk, physicalDevice, device, deviceExtensions, m_pipelineConstructionType);
	robustPipeline.setDefaultTopology(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
					.setDefaultRasterizationState()
					.setDefaultMultisampleState()
					.setDefaultDepthStencilState()
					.setDefaultColorBlendState()
					.setupVertexInputState(&vertexInputStateCreateInfo, &inputAssemblyStateCreateInfo)
					.setupPreRasterizationShaderState(viewports,
						scissors,
						m_pipelineLayout,
						*m_renderPass,
						0u,
						vert)
					.setupFragmentShaderState(m_pipelineLayout, *m_renderPass, 0u, frag)
					.setupFragmentOutputState(*m_renderPass)
					.setMonolithicPipelineLayout(m_pipelineLayout)
					.buildPipeline(*pipelineCache, 0, 0, vk::PipelineCreationFeedbackCreateInfoWrapper(), &pipelineRobustnessInfo);

	if (m_type == IMAGE)
	{
		// Initialize image
		const vk::VkImageMemoryBarrier		preImageBarrier =
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkAccessFlags			srcAccessMask;
			vk::VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
			vk::VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout			oldLayout;
			vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,						// deUint32					dstQueueFamilyIndex;
			**image,										// VkImage					image;
			{												// VkImageSubresourceRange	subresourceRange;
				vk::VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspect;
				0u,								// deUint32				baseMipLevel;
				1u,								// deUint32				mipLevels;
				0u,								// deUint32				baseArraySlice;
				1u,								// deUint32				arraySize;
			}
		};

		const vk::VkImageMemoryBarrier		postImageBarrier =
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			vk::VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
			vk::VK_ACCESS_SHADER_READ_BIT,					// VkAccessFlags			dstAccessMask;
			vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
			vk::VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,						// deUint32					dstQueueFamilyIndex;
			**image,										// VkImage					image;
			{												// VkImageSubresourceRange	subresourceRange;
				vk::VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspect;
				0u,								// deUint32				baseMipLevel;
				1u,								// deUint32				mipLevels;
				0u,								// deUint32				baseArraySlice;
				1u,								// deUint32				arraySize;
			}
		};

		const vk::VkBufferImageCopy	copyRegion =
		{
			0u,											// VkDeviceSize				bufferOffset;
			0u,											// deUint32					bufferRowLength;
			0u,											// deUint32					bufferImageHeight;
			{
				vk::VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspectFlags	aspect;
				0u,											// deUint32				mipLevel;
				0u,											// deUint32				baseArrayLayer;
				1u,											// deUint32				layerCount;
			},											// VkImageSubresourceLayers	imageSubresource;
			{ 0, 0, 0 },								// VkOffset3D				imageOffset;
			{ 1, 1, 1 },								// VkExtent3D				imageExtent;
		};

		vk::beginCommandBuffer(vk, *m_cmdBuffer);
		vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (vk::VkDependencyFlags)0, 0, (const vk::VkMemoryBarrier*)DE_NULL,
			0u, DE_NULL, 1u, &preImageBarrier);
		vk.cmdCopyBufferToImage(*m_cmdBuffer, **m_buffer, **image, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);
		vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, (vk::VkDependencyFlags)0, 0, (const vk::VkMemoryBarrier*)DE_NULL, 0, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1, &postImageBarrier);
		vk::endCommandBuffer(vk, *m_cmdBuffer);
		vk::submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);
	}

	draw(graphicsPipeline);

	if (!verifyImage(tcu::Vec4(values[0]), false))
		return tcu::TestStatus::fail("Fail");

	deUint32 invalidIndex = m_type == VERTEX_INPUT ? 15u : 999u;
	deMemcpy(indexBufferAlloc.getHostPtr(), &invalidIndex, sizeof(deUint32));
	flushAlloc(vk, device, indexBufferAlloc);

	draw(robustPipeline);

	if (m_robustnessBufferBehaviour == ROBUSTNESS_2)
	{
		if (!verifyImage(tcu::Vec4(0.0f), true))
			return tcu::TestStatus::fail("Fail");
	}

	return tcu::TestStatus::pass("Pass");
}

class PipelineCacheTestCase : public vkt::TestCase
{
public:
	PipelineCacheTestCase (tcu::TestContext& context, const char* name, const char* description, vk::PipelineConstructionType pipelineConstructionType, RobustnessBehaviour robustnessBufferBehaviour, RobustnessType type)
		: TestCase						(context, name, description)
		, m_pipelineConstructionType	(pipelineConstructionType)
		, m_robustnessBufferBehaviour	(robustnessBufferBehaviour)
		, m_type						(type)
	{
	}
private:
	void					checkSupport	(vkt::Context& context) const;
	void					initPrograms	(vk::SourceCollections& programCollection) const;
	vkt::TestInstance*		createInstance	(vkt::Context& context) const { return new PipelineCacheTestInstance(context, m_pipelineConstructionType, m_robustnessBufferBehaviour, m_type); }

	const vk::PipelineConstructionType		m_pipelineConstructionType;
	const RobustnessBehaviour				m_robustnessBufferBehaviour;
	const RobustnessType					m_type;
};

void PipelineCacheTestCase::checkSupport (vkt::Context& context) const
{
	if (m_robustnessBufferBehaviour == ROBUSTNESS)
		context.requireDeviceFunctionality("VK_EXT_pipeline_robustness");
	if (m_robustnessBufferBehaviour == ROBUSTNESS_2)
		context.requireDeviceFunctionality("VK_EXT_robustness2");

	vk::VkPhysicalDevicePipelineRobustnessFeaturesEXT pipelineRobustnessFeatures = vk::initVulkanStructure();
	vk::VkPhysicalDeviceRobustness2FeaturesEXT robustness2Features = vk::initVulkanStructure(&pipelineRobustnessFeatures);
	vk::VkPhysicalDeviceFeatures2 features2;

	features2.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &robustness2Features;

	context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

	if (pipelineRobustnessFeatures.pipelineRobustness == DE_FALSE)
		TCU_THROW(NotSupportedError, "VkPhysicalDevicePipelineRobustnessFeaturesEXT::pipelineRobustness feature not supported");

	if (m_robustnessBufferBehaviour == ROBUSTNESS_2)
	{
		if (m_type == IMAGE)
		{
			if (robustness2Features.robustImageAccess2 == DE_FALSE)
				TCU_THROW(NotSupportedError, "VkPhysicalDeviceRobustness2FeaturesEXT::robustImageAccess2 feature not supported");
		}
		else
		{
			if (robustness2Features.robustBufferAccess2 == DE_FALSE)
				TCU_THROW(NotSupportedError, "VkPhysicalDeviceRobustness2FeaturesEXT::robustBufferAccess2 feature not supported");
		}
	}

	vk::checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_pipelineConstructionType);
}

void PipelineCacheTestCase::initPrograms(vk::SourceCollections& programCollection) const
{
	if (m_type == VERTEX_INPUT)
	{
		{
			std::ostringstream vert;
			vert
				<< "#version 450\n"
				<< "layout(location = 0) in float in_values[16];\n"
				<< "layout(location = 0) out float out_value;\n"
				<< "layout (set=0, binding=1) restrict readonly buffer IndexBuffer {\n"
				<< "    uint index;\n"
				<< "};\n"
				<< "void main()\n"
				<< "{\n"
				<< "    vec2 vertex = vec2(gl_VertexIndex & 1u, (gl_VertexIndex >> 1u) & 1u);\n"
				<< "    gl_Position = vec4(vertex * 2.0f - 1.0f, 0.0f, 1.0f);\n"
				<< "    out_value = in_values[index];\n"
				<< "}\n"
				;

			programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
		}
		{
			std::ostringstream frag;
			frag
				<< "#version 450\n"
				<< "layout (location=0) in float in_value;\n"
				<< "layout (location=0) out vec4 out_color;\n"
				<< "void main()\n"
				<< "{\n"
				<< "    out_color = vec4(in_value);\n"
				<< "}\n"
				;

			programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
		}
	}
	else
	{
		{
			std::ostringstream vert;
			vert
				<< "#version 450\n"
				<< "void main()\n"
				<< "{\n"
				<< "    vec2 vertex = vec2(gl_VertexIndex & 1u, (gl_VertexIndex >> 1u) & 1u);\n"
				<< "    gl_Position = vec4(vertex * 2.0f - 1.0f, 0.0f, 1.0f);\n"
				<< "}\n"
				;

			programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
		}
		{
			std::string descriptor = {};
			std::string write = {};
			if (m_type == STORAGE)
			{
				descriptor =
					"layout (set=0, binding=0) restrict readonly buffer StorageBuffer {\n"
					"    float values[];\n"
					"};\n";
				write = "    out_color = vec4(values[index]);\n";
			}
			else if (m_type == UNIFORM)
			{
				descriptor =
					"layout (std140, set=0, binding=0) restrict uniform UniformBuffer {\n"
					"	float values[1000];\n"
					"};\n";
				write = "    out_color = vec4(values[index]);\n";
			}
			else if (m_type == IMAGE)
			{
				descriptor = "layout (set=0, binding=0, rgba32f) uniform image2D tex;\n";
				write = "    out_color = imageLoad(tex, ivec2(index, 0));\n";
			}

			std::ostringstream frag;
			frag
				<< "#version 450\n"
				<< "layout (location=0) out vec4 out_color;\n"
				<< descriptor
				<< "layout (set=0, binding=1) restrict readonly buffer IndexBuffer {\n"
				<< "    uint index;\n"
				<< "};\n"
				<< "void main()\n"
				<< "{\n"
				<< write
				<< "}\n"
				;

			programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
		}
	}
}

} // anonymous

tcu::TestCaseGroup* createPipelineRobustnessCacheTests (tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup	(new tcu::TestCaseGroup(testCtx, "pipeline_cache", "Test pipeline cache with different robustness enabled"));

	const struct
	{
		RobustnessBehaviour	robustnessBehaviour;
		const char* name;
	} robustnessTests[] =
	{
		{ ROBUSTNESS,			"robustness"		},
		{ ROBUSTNESS_2,			"robustness2"		},
	};

	const struct
	{
		RobustnessType	type;
		const char* name;
	} typeTests[] =
	{
		{ STORAGE,			"storage"		},
		{ UNIFORM,			"uniform"		},
		{ VERTEX_INPUT,		"vertex_input"	},
		{ IMAGE,			"image"		},
	};

	for (const auto& robustnessTest : robustnessTests)
	{
		de::MovePtr<tcu::TestCaseGroup> robustnessGroup(new tcu::TestCaseGroup(testCtx, robustnessTest.name, ""));
		for (const auto& typeTest : typeTests)
		{
			robustnessGroup->addChild(new PipelineCacheTestCase(testCtx, typeTest.name, "", pipelineConstructionType, robustnessTest.robustnessBehaviour, typeTest.type));
		}
		testGroup->addChild(robustnessGroup.release());
	}

	return testGroup.release();
}

} // pipeline
} // vkt
