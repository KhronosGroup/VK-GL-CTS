/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
 * Copyright (c) 2017 Codeplay Software Ltd.
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
 */ /*!
 * \file
 * \brief Subgroups Tests Utils
 */ /*--------------------------------------------------------------------*/

#include "vktSubgroupsTestsUtils.hpp"
#include "deRandom.hpp"
#include "tcuCommandLine.hpp"

using namespace tcu;
using namespace std;
using namespace vk;
using namespace vkt;

namespace
{
deUint32 getFormatSizeInBytes(const VkFormat format)
{
	switch (format)
	{
		default:
			DE_FATAL("Unhandled format!");
		case VK_FORMAT_R32_SINT:
		case VK_FORMAT_R32_UINT:
			return sizeof(deInt32);
		case VK_FORMAT_R32G32_SINT:
		case VK_FORMAT_R32G32_UINT:
			return static_cast<deUint32>(sizeof(deInt32) * 2);
		case VK_FORMAT_R32G32B32_SINT:
		case VK_FORMAT_R32G32B32_UINT:
		case VK_FORMAT_R32G32B32A32_SINT:
		case VK_FORMAT_R32G32B32A32_UINT:
			return static_cast<deUint32>(sizeof(deInt32) * 4);
		case VK_FORMAT_R32_SFLOAT:
			return 4;
		case VK_FORMAT_R32G32_SFLOAT:
			return 8;
		case VK_FORMAT_R32G32B32_SFLOAT:
			return 16;
		case VK_FORMAT_R32G32B32A32_SFLOAT:
			return 16;
		case VK_FORMAT_R64_SFLOAT:
			return 8;
		case VK_FORMAT_R64G64_SFLOAT:
			return 16;
		case VK_FORMAT_R64G64B64_SFLOAT:
			return 32;
		case VK_FORMAT_R64G64B64A64_SFLOAT:
			return 32;
		// The below formats are used to represent bool and bvec* types. These
		// types are passed to the shader as int and ivec* types, before the
		// calculations are done as booleans. We need a distinct type here so
		// that the shader generators can switch on it and generate the correct
		// shader source for testing.
		case VK_FORMAT_R8_USCALED:
			return sizeof(deInt32);
		case VK_FORMAT_R8G8_USCALED:
			return static_cast<deUint32>(sizeof(deInt32) * 2);
		case VK_FORMAT_R8G8B8_USCALED:
		case VK_FORMAT_R8G8B8A8_USCALED:
			return static_cast<deUint32>(sizeof(deInt32) * 4);
	}
}

Move<VkPipelineLayout> makePipelineLayout(
	Context& context, const VkDescriptorSetLayout descriptorSetLayout)
{
	const vk::VkPipelineLayoutCreateInfo pipelineLayoutParams = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
		DE_NULL,			  // const void*            pNext;
		0u,					  // VkPipelineLayoutCreateFlags    flags;
		1u,					  // deUint32             setLayoutCount;
		&descriptorSetLayout, // const VkDescriptorSetLayout*   pSetLayouts;
		0u,					  // deUint32             pushConstantRangeCount;
		DE_NULL, // const VkPushConstantRange*   pPushConstantRanges;
	};
	return createPipelineLayout(context.getDeviceInterface(),
								context.getDevice(), &pipelineLayoutParams);
}

Move<VkRenderPass> makeRenderPass(Context& context, VkFormat format)
{
	VkAttachmentReference colorReference = {
		0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	const VkSubpassDescription subpassDescription = {0u,
													 VK_PIPELINE_BIND_POINT_GRAPHICS, 0, DE_NULL, 1, &colorReference,
													 DE_NULL, DE_NULL, 0, DE_NULL
													};

	const VkSubpassDependency subpassDependencies[2] = {
		{   VK_SUBPASS_EXTERNAL, 0u, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_DEPENDENCY_BY_REGION_BIT
		},
		{   0u, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_MEMORY_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT
		},
	};

	VkAttachmentDescription attachmentDescription = {0u, format,
													 VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR,
													 VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
													 VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
													 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
													};

	const VkRenderPassCreateInfo renderPassCreateInfo = {
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, DE_NULL, 0u, 1,
		&attachmentDescription, 1, &subpassDescription, 2, subpassDependencies
	};

	return createRenderPass(context.getDeviceInterface(), context.getDevice(),
							&renderPassCreateInfo);
}

Move<VkFramebuffer> makeFramebuffer(Context& context,
									const VkRenderPass renderPass, const VkImageView imageView, deUint32 width,
									deUint32 height)
{
	const VkFramebufferCreateInfo framebufferCreateInfo = {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, DE_NULL, 0u, renderPass, 1,
		&imageView, width, height, 1
	};

	return createFramebuffer(context.getDeviceInterface(), context.getDevice(),
							 &framebufferCreateInfo);
}

Move<VkPipeline> makeGraphicsPipeline(Context& context,
									  const VkPipelineLayout pipelineLayout,
									  const VkShaderStageFlags stages,
									  const VkShaderModule vertexShaderModule,
									  const VkShaderModule fragmentShaderModule,
									  const VkShaderModule geometryShaderModule,
									  const VkShaderModule tessellationControlModule,
									  const VkShaderModule tessellationEvaluationModule,
									  const VkRenderPass renderPass,
									  const VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
{
	const VkBool32 disableRasterization = !(VK_SHADER_STAGE_FRAGMENT_BIT & stages);
	std::vector<vk::VkPipelineShaderStageCreateInfo> pipelineShaderStageParams;
	{
		const vk::VkPipelineShaderStageCreateInfo	stageCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType					sType
				DE_NULL,											// const void*						pNext
				0u,													// VkPipelineShaderStageCreateFlags	flags
				VK_SHADER_STAGE_VERTEX_BIT,							// VkShaderStageFlagBits			stage
				vertexShaderModule,									// VkShaderModule					module
				"main",												// const char*						pName
				DE_NULL												// const VkSpecializationInfo*      pSpecializationInfo
		};
		pipelineShaderStageParams.push_back(stageCreateInfo);
	}

	if (VK_SHADER_STAGE_FRAGMENT_BIT & stages)
	{
		const vk::VkPipelineShaderStageCreateInfo	stageCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType
			DE_NULL,												// const void*							pNext
			0u,														// VkPipelineShaderStageCreateFlags		flags
			VK_SHADER_STAGE_FRAGMENT_BIT,							// VkShaderStageFlagBits				stage
			fragmentShaderModule,									// VkShaderModule						module
			"main",													// const char*							pName
			DE_NULL													// const VkSpecializationInfo*			pSpecializationInfo
		};
		pipelineShaderStageParams.push_back(stageCreateInfo);
	}

	if (VK_SHADER_STAGE_GEOMETRY_BIT & stages)
	{
		const vk::VkPipelineShaderStageCreateInfo	stageCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType
			DE_NULL,												// const void*							pNext
			0u,														// VkPipelineShaderStageCreateFlags		flags
			VK_SHADER_STAGE_GEOMETRY_BIT,							// VkShaderStageFlagBits				stage
			geometryShaderModule,									// VkShaderModule						module
			"main",													// const char*							pName
			DE_NULL,												// const VkSpecializationInfo*			pSpecializationInfo
		};
		pipelineShaderStageParams.push_back(stageCreateInfo);
	}

	if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT & stages)
	{
		const vk::VkPipelineShaderStageCreateInfo	stageCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType
			DE_NULL,												// const void*							pNext
			0u,														// VkPipelineShaderStageCreateFlags		flags
			VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,				// VkShaderStageFlagBits				stage
			tessellationControlModule,								// VkShaderModule						module
			"main",													// const char*							pName
			DE_NULL													// const VkSpecializationInfo*			pSpecializationInfo
		};
		pipelineShaderStageParams.push_back(stageCreateInfo);
	}

	if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT & stages)
	{
		const vk::VkPipelineShaderStageCreateInfo	stageCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType
			DE_NULL,												// const void*							pNext
			0u,														// VkPipelineShaderStageCreateFlags		flags
			VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,			// VkShaderStageFlagBits				stage
			tessellationEvaluationModule,							// VkShaderModule						module
			"main",													// const char*							pName
			DE_NULL													// const VkSpecializationInfo*			pSpecializationInfo
		};
		pipelineShaderStageParams.push_back(stageCreateInfo);
	}

	const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, DE_NULL, 0u,
		0u, DE_NULL, 0u, DE_NULL,
	};

	const VkPipelineTessellationStateCreateInfo tessellationStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
		DE_NULL,
		0,
		1
	};

	const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, DE_NULL,
		0u, topology, VK_FALSE
	};

	const VkPipelineViewportStateCreateInfo viewportStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, DE_NULL, 0u, 1u,
		DE_NULL, 1u, DE_NULL,
	};

	const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, DE_NULL,
		0u, VK_FALSE, disableRasterization, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE,
		VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f
	};

	const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, DE_NULL, 0u,
		VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0.0f, DE_NULL, VK_FALSE, VK_FALSE
	};

	const VkStencilOpState stencilOpState =
	{
		VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_NEVER,
		0, 0, 0
	};

	const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, DE_NULL, 0u,
		VK_FALSE, VK_FALSE, VK_COMPARE_OP_NEVER, VK_FALSE, VK_FALSE, stencilOpState,
		stencilOpState, 0.0f, 0.0f
	};

	const VkPipelineColorBlendAttachmentState colorBlendAttachmentState =
	{
		VK_FALSE, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
		VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
		VK_COLOR_COMPONENT_R_BIT
	};

	const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, DE_NULL, 0u,
		VK_FALSE, VK_LOGIC_OP_CLEAR, 1, &colorBlendAttachmentState,
		{ 0.0f, 0.0f, 0.0f, 0.0f }
	};

	const VkDynamicState dynamicState[2] =
	{
		VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
	};

	const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, DE_NULL, 0u, 2,
		dynamicState,
	};

	const bool usingTessellation = (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT & stages)
								   || (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT & stages);

	const VkGraphicsPipelineCreateInfo pipelineCreateInfo =
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, DE_NULL, 0u,
		static_cast<deUint32>(pipelineShaderStageParams.size()),
		&pipelineShaderStageParams[0], &vertexInputStateCreateInfo,
		&inputAssemblyStateCreateInfo, usingTessellation ? &tessellationStateCreateInfo : DE_NULL, &viewportStateCreateInfo,
		&rasterizationStateCreateInfo, &multisampleStateCreateInfo,
		&depthStencilStateCreateInfo, &colorBlendStateCreateInfo,
		&dynamicStateCreateInfo, pipelineLayout, renderPass, 0, DE_NULL, 0
	};

	return createGraphicsPipeline(context.getDeviceInterface(),
								  context.getDevice(), DE_NULL, &pipelineCreateInfo);
}

Move<VkPipeline> makeComputePipeline(Context& context,
									 const VkPipelineLayout pipelineLayout, const VkShaderModule shaderModule,
									 deUint32 localSizeX, deUint32 localSizeY, deUint32 localSizeZ)
{
	const deUint32 localSize[3] = {localSizeX, localSizeY, localSizeZ};

	const vk::VkSpecializationMapEntry entries[3] =
	{
		{0, sizeof(deUint32) * 0, sizeof(deUint32)},
		{1, sizeof(deUint32) * 1, sizeof(deUint32)},
		{2, static_cast<deUint32>(sizeof(deUint32) * 2), sizeof(deUint32)},
	};

	const vk::VkSpecializationInfo info =
	{
		/* mapEntryCount = */ 3,
		/* pMapEntries   = */ entries,
		/* dataSize      = */ sizeof(localSize),
		/* pData         = */ localSize
	};

	const vk::VkPipelineShaderStageCreateInfo pipelineShaderStageParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType
		// sType;
		DE_NULL, // const void*              pNext;
		0u,		 // VkPipelineShaderStageCreateFlags   flags;
		VK_SHADER_STAGE_COMPUTE_BIT, // VkShaderStageFlagBits        stage;
		shaderModule,				 // VkShaderModule           module;
		"main",						 // const char*              pName;
		&info, // const VkSpecializationInfo*      pSpecializationInfo;
	};

	const vk::VkComputePipelineCreateInfo pipelineCreateInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType
		// sType;
		DE_NULL,				   // const void*            pNext;
		0u,						   // VkPipelineCreateFlags      flags;
		pipelineShaderStageParams, // VkPipelineShaderStageCreateInfo  stage;
		pipelineLayout,			   // VkPipelineLayout         layout;
		DE_NULL,				   // VkPipeline           basePipelineHandle;
		0,						   // deInt32              basePipelineIndex;
	};

	return createComputePipeline(context.getDeviceInterface(),
								 context.getDevice(), DE_NULL, &pipelineCreateInfo);
}

Move<VkDescriptorSet> makeDescriptorSet(Context& context,
										const VkDescriptorPool descriptorPool,
										const VkDescriptorSetLayout setLayout)
{
	const VkDescriptorSetAllocateInfo allocateParams =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType
		// sType;
		DE_NULL,		// const void*          pNext;
		descriptorPool, // VkDescriptorPool       descriptorPool;
		1u,				// deUint32           setLayoutCount;
		&setLayout,		// const VkDescriptorSetLayout* pSetLayouts;
	};
	return allocateDescriptorSet(
			   context.getDeviceInterface(), context.getDevice(), &allocateParams);
}

Move<VkCommandPool> makeCommandPool(Context& context)
{
	const VkCommandPoolCreateInfo commandPoolParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, // VkStructureType sType;
		DE_NULL,									// const void*        pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // VkCommandPoolCreateFlags
		// flags;
		context.getUniversalQueueFamilyIndex(), // deUint32 queueFamilyIndex;
	};

	return createCommandPool(
			   context.getDeviceInterface(), context.getDevice(), &commandPoolParams);
}

Move<VkCommandBuffer> makeCommandBuffer(
	Context& context, const VkCommandPool commandPool)
{
	const VkCommandBufferAllocateInfo bufferAllocateParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType
		// sType;
		DE_NULL,						 // const void*        pNext;
		commandPool,					 // VkCommandPool      commandPool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY, // VkCommandBufferLevel   level;
		1u,								 // deUint32         bufferCount;
	};
	return allocateCommandBuffer(context.getDeviceInterface(),
								 context.getDevice(), &bufferAllocateParams);
}

void beginCommandBuffer(Context& context, const VkCommandBuffer commandBuffer)
{
	const VkCommandBufferBeginInfo commandBufBeginParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType sType;
		DE_NULL, // const void*            pNext;
		0u,		 // VkCommandBufferUsageFlags    flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};
	VK_CHECK(context.getDeviceInterface().beginCommandBuffer(
				 commandBuffer, &commandBufBeginParams));
}

void endCommandBuffer(Context& context, const VkCommandBuffer commandBuffer)
{
	VK_CHECK(context.getDeviceInterface().endCommandBuffer(commandBuffer));
}

Move<VkFence> submitCommandBuffer(
	Context& context, const VkCommandBuffer commandBuffer)
{
	const VkFenceCreateInfo fenceParams =
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, // VkStructureType    sType;
		DE_NULL,							 // const void*      pNext;
		0u,									 // VkFenceCreateFlags flags;
	};

	Move<VkFence> fence(createFence(
							context.getDeviceInterface(), context.getDevice(), &fenceParams));

	const VkSubmitInfo submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType      sType;
		DE_NULL,					   // const void*        pNext;
		0u,							   // deUint32         waitSemaphoreCount;
		DE_NULL,					   // const VkSemaphore*   pWaitSemaphores;
		(const VkPipelineStageFlags*)DE_NULL,
		1u,				// deUint32         commandBufferCount;
		&commandBuffer, // const VkCommandBuffer* pCommandBuffers;
		0u,				// deUint32         signalSemaphoreCount;
		DE_NULL,		// const VkSemaphore*   pSignalSemaphores;
	};

	vk::VkResult result = (context.getDeviceInterface().queueSubmit(
							   context.getUniversalQueue(), 1u, &submitInfo, *fence));
	VK_CHECK(result);

	return Move<VkFence>(fence);
}

void waitFence(Context& context, Move<VkFence> fence)
{
	VK_CHECK(context.getDeviceInterface().waitForFences(
				 context.getDevice(), 1u, &fence.get(), DE_TRUE, ~0ull));
}

struct Buffer;
struct Image;

struct BufferOrImage
{
	bool isImage() const
	{
		return m_isImage;
	}

	Buffer* getAsBuffer()
	{
		if (m_isImage) DE_FATAL("Trying to get a buffer as an image!");
		return reinterpret_cast<Buffer* >(this);
	}

	Image* getAsImage()
	{
		if (!m_isImage) DE_FATAL("Trying to get an image as a buffer!");
		return reinterpret_cast<Image*>(this);
	}

	VkDescriptorType getType() const
	{
		if (m_isImage)
		{
			return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		}
		else
		{
			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		}
	}

	Allocation& getAllocation() const
	{
		return *m_allocation;
	}

	virtual ~BufferOrImage() {}

protected:
	explicit BufferOrImage(bool image) : m_isImage(image) {}

	bool m_isImage;
	de::details::MovePtr<Allocation> m_allocation;
};

struct Buffer : public BufferOrImage
{
	explicit Buffer(
		Context& context, VkDeviceSize sizeInBytes, VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
		: BufferOrImage(false), m_sizeInBytes(sizeInBytes)
	{
		const vk::VkBufferCreateInfo bufferCreateInfo =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			DE_NULL,
			0u,
			sizeInBytes,
			usage,
			VK_SHARING_MODE_EXCLUSIVE,
			0u,
			DE_NULL,
		};
		m_buffer = createBuffer(context.getDeviceInterface(),
								context.getDevice(), &bufferCreateInfo);
		vk::VkMemoryRequirements req = getBufferMemoryRequirements(
										   context.getDeviceInterface(), context.getDevice(), *m_buffer);
		req.size *= 2;
		m_allocation = context.getDefaultAllocator().allocate(
						   req, MemoryRequirement::HostVisible);
		VK_CHECK(context.getDeviceInterface().bindBufferMemory(
					 context.getDevice(), *m_buffer, m_allocation->getMemory(),
					 m_allocation->getOffset()));
	}

	VkBuffer getBuffer() const {
		return *m_buffer;
	}


	VkDeviceSize getSize() const {
		return m_sizeInBytes;
	}

private:
	Move<VkBuffer> m_buffer;
	VkDeviceSize m_sizeInBytes;
};

struct Image : public BufferOrImage
{
	explicit Image(Context& context, deUint32 width, deUint32 height,
				   VkFormat format, VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT)
		: BufferOrImage(true)
	{
		const VkImageCreateInfo imageCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, DE_NULL, 0, VK_IMAGE_TYPE_2D,
			format, {width, height, 1}, 1, 1, VK_SAMPLE_COUNT_1_BIT,
			VK_IMAGE_TILING_OPTIMAL, usage,
			VK_SHARING_MODE_EXCLUSIVE, 0u, DE_NULL,
			VK_IMAGE_LAYOUT_UNDEFINED
		};
		m_image = createImage(context.getDeviceInterface(), context.getDevice(),
							  &imageCreateInfo);
		vk::VkMemoryRequirements req = getImageMemoryRequirements(
										   context.getDeviceInterface(), context.getDevice(), *m_image);
		req.size *= 2;
		m_allocation =
			context.getDefaultAllocator().allocate(req, MemoryRequirement::Any);
		VK_CHECK(context.getDeviceInterface().bindImageMemory(
					 context.getDevice(), *m_image, m_allocation->getMemory(),
					 m_allocation->getOffset()));

		const VkComponentMapping componentMapping =
		{
			VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY
		};

		const VkImageViewCreateInfo imageViewCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, DE_NULL, 0, *m_image,
			VK_IMAGE_VIEW_TYPE_2D, imageCreateInfo.format, componentMapping,
			{
				VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1,
			}
		};

		m_imageView = createImageView(context.getDeviceInterface(),
									  context.getDevice(), &imageViewCreateInfo);

		const struct VkSamplerCreateInfo samplerCreateInfo =
		{
			VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			DE_NULL,
			0u,
			VK_FILTER_NEAREST,
			VK_FILTER_NEAREST,
			VK_SAMPLER_MIPMAP_MODE_NEAREST,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			0.0f,
			VK_FALSE,
			1.0f,
			DE_FALSE,
			VK_COMPARE_OP_ALWAYS,
			0.0f,
			0.0f,
			VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
			VK_FALSE,
		};

		m_sampler = createSampler(context.getDeviceInterface(), context.getDevice(), &samplerCreateInfo);
	}

	VkImage getImage() const {
		return *m_image;
	}

	VkImageView getImageView() const {
		return *m_imageView;
	}

	VkSampler getSampler() const {
		return *m_sampler;
	}

private:
	Move<VkImage> m_image;
	Move<VkImageView> m_imageView;
	Move<VkSampler> m_sampler;
};
}

std::string vkt::subgroups::getSharedMemoryBallotHelper()
{
	return	"shared uvec4 superSecretComputeShaderHelper[gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z];\n"
			"uvec4 sharedMemoryBallot(bool vote)\n"
			"{\n"
			"  uint groupOffset = gl_SubgroupID;\n"
			"  // One invocation in the group 0's the whole group's data\n"
			"  if (subgroupElect())\n"
			"  {\n"
			"    superSecretComputeShaderHelper[groupOffset] = uvec4(0);\n"
			"  }\n"
			"  subgroupMemoryBarrierShared();\n"
			"  if (vote)\n"
			"  {\n"
			"    const highp uint bitToSet = 1u << (gl_SubgroupInvocationID % 32);\n"
			"    switch (gl_SubgroupInvocationID / 32)\n"
			"    {\n"
			"    case 0: atomicOr(superSecretComputeShaderHelper[groupOffset].x, bitToSet); break;\n"
			"    case 1: atomicOr(superSecretComputeShaderHelper[groupOffset].y, bitToSet); break;\n"
			"    case 2: atomicOr(superSecretComputeShaderHelper[groupOffset].z, bitToSet); break;\n"
			"    case 3: atomicOr(superSecretComputeShaderHelper[groupOffset].w, bitToSet); break;\n"
			"    }\n"
			"  }\n"
			"  subgroupMemoryBarrierShared();\n"
			"  return superSecretComputeShaderHelper[groupOffset];\n"
			"}\n";
}

deUint32 vkt::subgroups::getSubgroupSize(Context& context)
{
	VkPhysicalDeviceSubgroupProperties subgroupProperties;
	subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
	subgroupProperties.pNext = DE_NULL;

	VkPhysicalDeviceProperties2KHR properties;
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
	properties.pNext = &subgroupProperties;

	context.getInstanceInterface().getPhysicalDeviceProperties2KHR(context.getPhysicalDevice(), &properties);

	return subgroupProperties.subgroupSize;
}

VkDeviceSize vkt::subgroups::maxSupportedSubgroupSize() {
	return 128u;
}

std::string vkt::subgroups::getShaderStageName(VkShaderStageFlags stage)
{
	switch (stage)
	{
		default:
			DE_FATAL("Unhandled stage!");
		case VK_SHADER_STAGE_COMPUTE_BIT:
			return "compute";
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			return "fragment";
		case VK_SHADER_STAGE_VERTEX_BIT:
			return "vertex";
		case VK_SHADER_STAGE_GEOMETRY_BIT:
			return "geometry";
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
			return "tess_control";
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
			return "tess_eval";
	}
}

std::string vkt::subgroups::getSubgroupFeatureName(vk::VkSubgroupFeatureFlagBits bit)
{
	switch (bit)
	{
		default:
			DE_FATAL("Unknown subgroup feature category!");
		case VK_SUBGROUP_FEATURE_BASIC_BIT:
			return "VK_SUBGROUP_FEATURE_BASIC_BIT";
		case VK_SUBGROUP_FEATURE_VOTE_BIT:
			return "VK_SUBGROUP_FEATURE_VOTE_BIT";
		case VK_SUBGROUP_FEATURE_ARITHMETIC_BIT:
			return "VK_SUBGROUP_FEATURE_ARITHMETIC_BIT";
		case VK_SUBGROUP_FEATURE_BALLOT_BIT:
			return "VK_SUBGROUP_FEATURE_BALLOT_BIT";
		case VK_SUBGROUP_FEATURE_SHUFFLE_BIT:
			return "VK_SUBGROUP_FEATURE_SHUFFLE_BIT";
		case VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT:
			return "VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT";
		case VK_SUBGROUP_FEATURE_CLUSTERED_BIT:
			return "VK_SUBGROUP_FEATURE_CLUSTERED_BIT";
		case VK_SUBGROUP_FEATURE_QUAD_BIT:
			return "VK_SUBGROUP_FEATURE_QUAD_BIT";
	}
}

std::string vkt::subgroups::getVertShaderForStage(vk::VkShaderStageFlags stage)
{
	switch (stage)
	{
		default:
			DE_FATAL("Unhandled stage!");
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			return
				"#version 450\n"
				"void main (void)\n"
				"{\n"
				"  vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);\n"
				"  gl_Position = vec4(uv * 2.0f + -1.0f, 0.0f, 1.0f);\n"
				"}\n";
		case VK_SHADER_STAGE_GEOMETRY_BIT:
			return
				"#version 450\n"
				"void main (void)\n"
				"{\n"
				"}\n";
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
			return
				"#version 450\n"
				"void main (void)\n"
				"{\n"
				"}\n";
	}
}

bool vkt::subgroups::isSubgroupSupported(Context& context)
{
	VkPhysicalDeviceProperties properties;
	context.getInstanceInterface().getPhysicalDeviceProperties(context.getPhysicalDevice(), &properties);
	return (properties.apiVersion < VK_MAKE_VERSION(1, 1, 0)) ? false : true;
}

bool vkt::subgroups::areSubgroupOperationsSupportedForStage(
	Context& context, const VkShaderStageFlags stage)
{
	VkPhysicalDeviceSubgroupProperties subgroupProperties;
	subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
	subgroupProperties.pNext = DE_NULL;

	VkPhysicalDeviceProperties2KHR properties;
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
	properties.pNext = &subgroupProperties;

	context.getInstanceInterface().getPhysicalDeviceProperties2KHR(context.getPhysicalDevice(), &properties);

	return (stage & subgroupProperties.supportedStages) ? true : false;
}

bool vkt::subgroups::areSubgroupOperationsRequiredForStage(
	VkShaderStageFlags stage)
{
	switch (stage)
	{
		default:
			return false;
		case VK_SHADER_STAGE_COMPUTE_BIT:
			return true;
	}
}

bool vkt::subgroups::isSubgroupFeatureSupportedForDevice(
	Context& context,
	VkSubgroupFeatureFlagBits bit) {
	VkPhysicalDeviceSubgroupProperties subgroupProperties;
	subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
	subgroupProperties.pNext = DE_NULL;

	VkPhysicalDeviceProperties2KHR properties;
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
	properties.pNext = &subgroupProperties;

	context.getInstanceInterface().getPhysicalDeviceProperties2KHR(context.getPhysicalDevice(), &properties);

	return (bit & subgroupProperties.supportedOperations) ? true : false;
}

bool vkt::subgroups::isFragmentSSBOSupportedForDevice(Context& context)
{
	const VkPhysicalDeviceFeatures features = getPhysicalDeviceFeatures(
				context.getInstanceInterface(), context.getPhysicalDevice());
	return features.fragmentStoresAndAtomics ? true : false;
}

bool vkt::subgroups::isVertexSSBOSupportedForDevice(Context& context)
{
	const VkPhysicalDeviceFeatures features = getPhysicalDeviceFeatures(
				context.getInstanceInterface(), context.getPhysicalDevice());
	return features.vertexPipelineStoresAndAtomics ? true : false;
}

bool vkt::subgroups::isDoubleSupportedForDevice(Context& context)
{
	const VkPhysicalDeviceFeatures features = getPhysicalDeviceFeatures(
				context.getInstanceInterface(), context.getPhysicalDevice());
	return features.shaderFloat64 ? true : false;
}

bool vkt::subgroups::isDoubleFormat(VkFormat format)
{
	switch (format)
	{
		default:
			return false;
		case VK_FORMAT_R64_SFLOAT:
		case VK_FORMAT_R64G64_SFLOAT:
		case VK_FORMAT_R64G64B64_SFLOAT:
		case VK_FORMAT_R64G64B64A64_SFLOAT:
			return true;
	}
}

std::string vkt::subgroups::getFormatNameForGLSL(VkFormat format)
{
	switch (format)
	{
		default:
			DE_FATAL("Unhandled format!");
		case VK_FORMAT_R32_SINT:
			return "int";
		case VK_FORMAT_R32G32_SINT:
			return "ivec2";
		case VK_FORMAT_R32G32B32_SINT:
			return "ivec3";
		case VK_FORMAT_R32G32B32A32_SINT:
			return "ivec4";
		case VK_FORMAT_R32_UINT:
			return "uint";
		case VK_FORMAT_R32G32_UINT:
			return "uvec2";
		case VK_FORMAT_R32G32B32_UINT:
			return "uvec3";
		case VK_FORMAT_R32G32B32A32_UINT:
			return "uvec4";
		case VK_FORMAT_R32_SFLOAT:
			return "float";
		case VK_FORMAT_R32G32_SFLOAT:
			return "vec2";
		case VK_FORMAT_R32G32B32_SFLOAT:
			return "vec3";
		case VK_FORMAT_R32G32B32A32_SFLOAT:
			return "vec4";
		case VK_FORMAT_R64_SFLOAT:
			return "double";
		case VK_FORMAT_R64G64_SFLOAT:
			return "dvec2";
		case VK_FORMAT_R64G64B64_SFLOAT:
			return "dvec3";
		case VK_FORMAT_R64G64B64A64_SFLOAT:
			return "dvec4";
		case VK_FORMAT_R8_USCALED:
			return "bool";
		case VK_FORMAT_R8G8_USCALED:
			return "bvec2";
		case VK_FORMAT_R8G8B8_USCALED:
			return "bvec3";
		case VK_FORMAT_R8G8B8A8_USCALED:
			return "bvec4";
	}
}

void initializeMemory(Context& context, const Allocation& alloc, subgroups::SSBOData& data)
{
	const vk::VkFormat format = data.format;
	const vk::VkDeviceSize size = getFormatSizeInBytes(format) * data.numElements;
	if (subgroups::SSBOData::InitializeNonZero == data.initializeType)
	{
		de::Random rnd(context.getTestContext().getCommandLine().getBaseSeed());

		switch (format)
		{
			default:
				DE_FATAL("Illegal buffer format");
			case VK_FORMAT_R8_USCALED:
			case VK_FORMAT_R8G8_USCALED:
			case VK_FORMAT_R8G8B8_USCALED:
			case VK_FORMAT_R8G8B8A8_USCALED:
			case VK_FORMAT_R32_SINT:
			case VK_FORMAT_R32G32_SINT:
			case VK_FORMAT_R32G32B32_SINT:
			case VK_FORMAT_R32G32B32A32_SINT:
			case VK_FORMAT_R32_UINT:
			case VK_FORMAT_R32G32_UINT:
			case VK_FORMAT_R32G32B32_UINT:
			case VK_FORMAT_R32G32B32A32_UINT:
			{
				deUint32* ptr = reinterpret_cast<deUint32*>(alloc.getHostPtr());

				for (vk::VkDeviceSize k = 0; k < (size / 4); k++)
				{
					ptr[k] = rnd.getUint32();
				}
			}
			break;
			case VK_FORMAT_R32_SFLOAT:
			case VK_FORMAT_R32G32_SFLOAT:
			case VK_FORMAT_R32G32B32_SFLOAT:
			case VK_FORMAT_R32G32B32A32_SFLOAT:
			{
				float* ptr = reinterpret_cast<float*>(alloc.getHostPtr());

				for (vk::VkDeviceSize k = 0; k < (size / 4); k++)
				{
					ptr[k] = rnd.getFloat();
				}
			}
			break;
			case VK_FORMAT_R64_SFLOAT:
			case VK_FORMAT_R64G64_SFLOAT:
			case VK_FORMAT_R64G64B64_SFLOAT:
			case VK_FORMAT_R64G64B64A64_SFLOAT:
			{
				double* ptr = reinterpret_cast<double*>(alloc.getHostPtr());

				for (vk::VkDeviceSize k = 0; k < (size / 4); k++)
				{
					ptr[k] = rnd.getDouble();
				}
			}
			break;
		}
	}
	else if (subgroups::SSBOData::InitializeZero == data.initializeType)
	{
		deUint32* ptr = reinterpret_cast<deUint32*>(alloc.getHostPtr());

		for (vk::VkDeviceSize k = 0; k < size / 4; k++)
		{
			ptr[k] = 0;
		}
	}

	if (subgroups::SSBOData::InitializeNone != data.initializeType)
	{
		flushMappedMemoryRange(context.getDeviceInterface(),
							   context.getDevice(), alloc.getMemory(), alloc.getOffset(), size);
	}
}

tcu::TestStatus vkt::subgroups::makeTessellationEvaluationTest(
	Context& context, VkFormat format, SSBOData* extraDatas,
	deUint32 extraDatasCount,
	bool (*checkResult)(std::vector<const void*> datas, deUint32 width, deUint32 subgroupSize))
{
	const deUint32 maxWidth = 1024;

	const Unique<VkShaderModule> vertexShaderModule(
		createShaderModule(context.getDeviceInterface(), context.getDevice(),
						   context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule> tessellationControlShaderModule(
		createShaderModule(context.getDeviceInterface(), context.getDevice(),
						   context.getBinaryCollection().get("tesc"), 0u));
	const Unique<VkShaderModule> tessellationEvaluationShaderModule(
		createShaderModule(context.getDeviceInterface(), context.getDevice(),
						   context.getBinaryCollection().get("tese"), 0u));

	std::vector< de::SharedPtr<BufferOrImage> > inputBuffers(extraDatasCount + 1);

	// The implicit result SSBO we use to store our outputs from the shader
	{
		vk::VkDeviceSize size = getFormatSizeInBytes(format) * maxWidth * 2;
		inputBuffers[0] = de::SharedPtr<BufferOrImage>(new Buffer(context, size));
	}

	for (deUint32 i = 0; i < (inputBuffers.size() - 1); i++)
	{
		if (extraDatas[i].isImage)
		{
			inputBuffers[i + 1] = de::SharedPtr<BufferOrImage>(new Image(context,
											static_cast<deUint32>(extraDatas[i].numElements), 1, extraDatas[i].format));
		}
		else
		{
			vk::VkDeviceSize size =
				getFormatSizeInBytes(extraDatas[i].format) * extraDatas[i].numElements;
			inputBuffers[i + 1] = de::SharedPtr<BufferOrImage>(new Buffer(context, size));
		}

		const Allocation& alloc = inputBuffers[i + 1]->getAllocation();
		initializeMemory(context, alloc, extraDatas[i]);
	}

	DescriptorSetLayoutBuilder layoutBuilder;

	for (deUint32 i = 0; i < inputBuffers.size(); i++)
	{
		layoutBuilder.addBinding(inputBuffers[i]->getType(), 1,
								 VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, DE_NULL);
	}

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		layoutBuilder.build(context.getDeviceInterface(), context.getDevice()));

	const Unique<VkPipelineLayout> pipelineLayout(
		makePipelineLayout(context, *descriptorSetLayout));

	const Unique<VkRenderPass> renderPass(makeRenderPass(context, VK_FORMAT_R32_SFLOAT));
	const Unique<VkPipeline> pipeline(makeGraphicsPipeline(context, *pipelineLayout,
									  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
									  *vertexShaderModule, DE_NULL, DE_NULL, *tessellationControlShaderModule, *tessellationEvaluationShaderModule,
									  *renderPass, VK_PRIMITIVE_TOPOLOGY_PATCH_LIST));

	DescriptorPoolBuilder poolBuilder;

	for (deUint32 i = 0; i < inputBuffers.size(); i++)
	{
		poolBuilder.addType(inputBuffers[i]->getType());
	}

	const Unique<VkDescriptorPool> descriptorPool(
		poolBuilder.build(context.getDeviceInterface(), context.getDevice(),
						  VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	// Create descriptor set
	const Unique<VkDescriptorSet> descriptorSet(
		makeDescriptorSet(context, *descriptorPool, *descriptorSetLayout));

	DescriptorSetUpdateBuilder updateBuilder;

	for (deUint32 i = 0; i < inputBuffers.size(); i++)
	{
		if (inputBuffers[i]->isImage())
		{
			VkDescriptorImageInfo info =
				makeDescriptorImageInfo(inputBuffers[i]->getAsImage()->getSampler(),
										inputBuffers[i]->getAsImage()->getImageView(), VK_IMAGE_LAYOUT_GENERAL);

			updateBuilder.writeSingle(*descriptorSet,
									  DescriptorSetUpdateBuilder::Location::binding(i),
									  inputBuffers[i]->getType(), &info);
		}
		else
		{
			VkDescriptorBufferInfo info =
				makeDescriptorBufferInfo(inputBuffers[i]->getAsBuffer()->getBuffer(),
										 0ull, inputBuffers[i]->getAsBuffer()->getSize());

			updateBuilder.writeSingle(*descriptorSet,
									  DescriptorSetUpdateBuilder::Location::binding(i),
									  inputBuffers[i]->getType(), &info);
		}
	}

	updateBuilder.update(context.getDeviceInterface(), context.getDevice());

	const Unique<VkCommandPool> cmdPool(makeCommandPool(context));

	const deUint32 subgroupSize = getSubgroupSize(context);

	const Unique<VkCommandBuffer> cmdBuffer(
		makeCommandBuffer(context, *cmdPool));

	unsigned totalIterations = 0;
	unsigned failedIterations = 0;

	Image discardableImage(context, 1, 1, VK_FORMAT_R32_SFLOAT,
						   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
						   VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	for (deUint32 width = 1; width < maxWidth; width++)
	{
		for (deUint32 i = 1; i < inputBuffers.size(); i++)
		{
			// re-init the data
			const Allocation& alloc = inputBuffers[i]->getAllocation();
			initializeMemory(context, alloc, extraDatas[i - 1]);
		}

		totalIterations++;

		const Unique<VkFramebuffer> framebuffer(makeFramebuffer(context,
												*renderPass, discardableImage.getImageView(), 1, 1));

		const VkClearValue clearValue = {{{0.0f, 0.0f, 0.0f, 0.0f}}};

		const VkRenderPassBeginInfo renderPassBeginInfo = {
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, DE_NULL, *renderPass,
			*framebuffer, {{0, 0}, {1, 1}}, 1, &clearValue,
		};

		beginCommandBuffer(context, *cmdBuffer);

		VkViewport viewport = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};

		context.getDeviceInterface().cmdSetViewport(
			*cmdBuffer, 0, 1, &viewport);

		VkRect2D scissor = {{0, 0}, {1, 1}};

		context.getDeviceInterface().cmdSetScissor(
			*cmdBuffer, 0, 1, &scissor);

		context.getDeviceInterface().cmdBeginRenderPass(
			*cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		context.getDeviceInterface().cmdBindPipeline(
			*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

		context.getDeviceInterface().cmdBindDescriptorSets(*cmdBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
				&descriptorSet.get(), 0u, DE_NULL);

		context.getDeviceInterface().cmdDraw(*cmdBuffer, width, 1, 0, 0);

		context.getDeviceInterface().cmdEndRenderPass(*cmdBuffer);

		endCommandBuffer(context, *cmdBuffer);

		Move<VkFence> fence(submitCommandBuffer(context, *cmdBuffer));

		waitFence(context, fence);

		std::vector<const void*> datas;

		for (deUint32 i = 0; i < inputBuffers.size(); i++)
		{
			if (!inputBuffers[i]->isImage())
			{
				const Allocation& resultAlloc = inputBuffers[i]->getAllocation();
				invalidateMappedMemoryRange(context.getDeviceInterface(),
											context.getDevice(), resultAlloc.getMemory(),
											resultAlloc.getOffset(), inputBuffers[i]->getAsBuffer()->getSize());

				// we always have our result data first
				datas.push_back(resultAlloc.getHostPtr());
			}
		}

		if (!checkResult(datas, width * 2, subgroupSize))
		{
			failedIterations++;
		}

		context.getDeviceInterface().resetCommandBuffer(*cmdBuffer, 0);
	}

	if (0 < failedIterations)
	{
		context.getTestContext().getLog()
				<< TestLog::Message << (totalIterations - failedIterations) << " / "
				<< totalIterations << " values passed" << TestLog::EndMessage;
		return tcu::TestStatus::fail("Failed!");
	}

	return tcu::TestStatus::pass("OK");
}

tcu::TestStatus vkt::subgroups::makeTessellationControlTest(
	Context& context, VkFormat format, SSBOData* extraDatas,
	deUint32 extraDatasCount,
	bool (*checkResult)(std::vector<const void*> datas, deUint32 width, deUint32 subgroupSize))
{
	const deUint32 maxWidth = 1024;

	const Unique<VkShaderModule> vertexShaderModule(
		createShaderModule(context.getDeviceInterface(), context.getDevice(),
						   context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule> tessellationControlShaderModule(
		createShaderModule(context.getDeviceInterface(), context.getDevice(),
						   context.getBinaryCollection().get("tesc"), 0u));
	const Unique<VkShaderModule> tessellationEvaluationShaderModule(
		createShaderModule(context.getDeviceInterface(), context.getDevice(),
						   context.getBinaryCollection().get("tese"), 0u));

	std::vector< de::SharedPtr<BufferOrImage> > inputBuffers(extraDatasCount + 1);

	// The implicit result SSBO we use to store our outputs from the vertex shader
	{
		vk::VkDeviceSize size = getFormatSizeInBytes(format) * maxWidth;
		inputBuffers[0] = de::SharedPtr<BufferOrImage>(new Buffer(context, size));
	}

	for (deUint32 i = 0; i < (inputBuffers.size() - 1); i++)
	{
		if (extraDatas[i].isImage)
		{
			inputBuffers[i + 1] = de::SharedPtr<BufferOrImage>(new Image(context,
											static_cast<deUint32>(extraDatas[i].numElements), 1, extraDatas[i].format));
		}
		else
		{
			vk::VkDeviceSize size =
				getFormatSizeInBytes(extraDatas[i].format) * extraDatas[i].numElements;
			inputBuffers[i + 1] = de::SharedPtr<BufferOrImage>(new Buffer(context, size));
		}

		const Allocation& alloc = inputBuffers[i + 1]->getAllocation();
		initializeMemory(context, alloc, extraDatas[i]);
	}

	DescriptorSetLayoutBuilder layoutBuilder;

	for (deUint32 i = 0; i < inputBuffers.size(); i++)
	{
		layoutBuilder.addBinding(inputBuffers[i]->getType(), 1,
								 VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, DE_NULL);
	}

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		layoutBuilder.build(context.getDeviceInterface(), context.getDevice()));

	const Unique<VkPipelineLayout> pipelineLayout(
		makePipelineLayout(context, *descriptorSetLayout));

	const Unique<VkRenderPass> renderPass(makeRenderPass(context, VK_FORMAT_R32_SFLOAT));
	const Unique<VkPipeline> pipeline(makeGraphicsPipeline(context, *pipelineLayout,
									  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
									  *vertexShaderModule, DE_NULL, DE_NULL, *tessellationControlShaderModule, *tessellationEvaluationShaderModule,
									  *renderPass, VK_PRIMITIVE_TOPOLOGY_PATCH_LIST));

	DescriptorPoolBuilder poolBuilder;

	for (deUint32 i = 0; i < inputBuffers.size(); i++)
	{
		poolBuilder.addType(inputBuffers[i]->getType());
	}

	const Unique<VkDescriptorPool> descriptorPool(
		poolBuilder.build(context.getDeviceInterface(), context.getDevice(),
						  VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	// Create descriptor set
	const Unique<VkDescriptorSet> descriptorSet(
		makeDescriptorSet(context, *descriptorPool, *descriptorSetLayout));

	DescriptorSetUpdateBuilder updateBuilder;

	for (deUint32 i = 0; i < inputBuffers.size(); i++)
	{
		if (inputBuffers[i]->isImage())
		{
			VkDescriptorImageInfo info =
				makeDescriptorImageInfo(inputBuffers[i]->getAsImage()->getSampler(),
										inputBuffers[i]->getAsImage()->getImageView(), VK_IMAGE_LAYOUT_GENERAL);

			updateBuilder.writeSingle(*descriptorSet,
									  DescriptorSetUpdateBuilder::Location::binding(i),
									  inputBuffers[i]->getType(), &info);
		}
		else
		{
			VkDescriptorBufferInfo info =
				makeDescriptorBufferInfo(inputBuffers[i]->getAsBuffer()->getBuffer(),
										 0ull, inputBuffers[i]->getAsBuffer()->getSize());

			updateBuilder.writeSingle(*descriptorSet,
									  DescriptorSetUpdateBuilder::Location::binding(i),
									  inputBuffers[i]->getType(), &info);
		}
	}

	updateBuilder.update(context.getDeviceInterface(), context.getDevice());

	const Unique<VkCommandPool> cmdPool(makeCommandPool(context));

	const deUint32 subgroupSize = getSubgroupSize(context);

	const Unique<VkCommandBuffer> cmdBuffer(
		makeCommandBuffer(context, *cmdPool));

	unsigned totalIterations = 0;
	unsigned failedIterations = 0;

	Image discardableImage(context, 1, 1, VK_FORMAT_R32_SFLOAT,
						   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
						   VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	for (deUint32 width = 1; width < maxWidth; width++)
	{
		for (deUint32 i = 1; i < inputBuffers.size(); i++)
		{
			// re-init the data
			const Allocation& alloc = inputBuffers[i]->getAllocation();
			initializeMemory(context, alloc, extraDatas[i - 1]);
		}

		totalIterations++;

		const Unique<VkFramebuffer> framebuffer(makeFramebuffer(context,
												*renderPass, discardableImage.getImageView(), 1, 1));

		const VkClearValue clearValue = {{{0.0f, 0.0f, 0.0f, 0.0f}}};

		const VkRenderPassBeginInfo renderPassBeginInfo = {
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, DE_NULL, *renderPass,
			*framebuffer, {{0, 0}, {1, 1}}, 1, &clearValue,
		};

		beginCommandBuffer(context, *cmdBuffer);

		VkViewport viewport = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};

		context.getDeviceInterface().cmdSetViewport(
			*cmdBuffer, 0, 1, &viewport);

		VkRect2D scissor = {{0, 0}, {1, 1}};

		context.getDeviceInterface().cmdSetScissor(
			*cmdBuffer, 0, 1, &scissor);

		context.getDeviceInterface().cmdBeginRenderPass(
			*cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		context.getDeviceInterface().cmdBindPipeline(
			*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

		context.getDeviceInterface().cmdBindDescriptorSets(*cmdBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
				&descriptorSet.get(), 0u, DE_NULL);

		context.getDeviceInterface().cmdDraw(*cmdBuffer, width, 1, 0, 0);

		context.getDeviceInterface().cmdEndRenderPass(*cmdBuffer);

		endCommandBuffer(context, *cmdBuffer);

		Move<VkFence> fence(submitCommandBuffer(context, *cmdBuffer));

		waitFence(context, fence);

		std::vector<const void*> datas;

		for (deUint32 i = 0; i < inputBuffers.size(); i++)
		{
			if (!inputBuffers[i]->isImage())
			{
				const Allocation& resultAlloc = inputBuffers[i]->getAllocation();
				invalidateMappedMemoryRange(context.getDeviceInterface(),
											context.getDevice(), resultAlloc.getMemory(),
											resultAlloc.getOffset(), inputBuffers[i]->getAsBuffer()->getSize());

				// we always have our result data first
				datas.push_back(resultAlloc.getHostPtr());
			}
		}

		if (!checkResult(datas, width, subgroupSize))
		{
			failedIterations++;
		}

		context.getDeviceInterface().resetCommandBuffer(*cmdBuffer, 0);
	}

	if (0 < failedIterations)
	{
		context.getTestContext().getLog()
				<< TestLog::Message << (totalIterations - failedIterations) << " / "
				<< totalIterations << " values passed" << TestLog::EndMessage;
		return tcu::TestStatus::fail("Failed!");
	}

	return tcu::TestStatus::pass("OK");
}

tcu::TestStatus vkt::subgroups::makeGeometryTest(
	Context& context, VkFormat format, SSBOData* extraDatas,
	deUint32 extraDatasCount,
	bool (*checkResult)(std::vector<const void*> datas, deUint32 width, deUint32 subgroupSize))
{
	const deUint32 maxWidth = 1024;

	const Unique<VkShaderModule> vertexShaderModule(
		createShaderModule(context.getDeviceInterface(), context.getDevice(),
						   context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule> geometryShaderModule(
		createShaderModule(context.getDeviceInterface(), context.getDevice(),
						   context.getBinaryCollection().get("geom"), 0u));

	std::vector< de::SharedPtr<BufferOrImage> > inputBuffers(extraDatasCount + 1);

	// The implicit result SSBO we use to store our outputs from the vertex shader
	{
		vk::VkDeviceSize size = getFormatSizeInBytes(format) * maxWidth;
		inputBuffers[0] = de::SharedPtr<BufferOrImage>(new Buffer(context, size));
	}

	for (deUint32 i = 0; i < (inputBuffers.size() - 1); i++)
	{
		if (extraDatas[i].isImage)
		{
			inputBuffers[i + 1] = de::SharedPtr<BufferOrImage>(new Image(context,
											static_cast<deUint32>(extraDatas[i].numElements), 1, extraDatas[i].format));
		}
		else
		{
			vk::VkDeviceSize size =
				getFormatSizeInBytes(extraDatas[i].format) * extraDatas[i].numElements;
			inputBuffers[i + 1] = de::SharedPtr<BufferOrImage>(new Buffer(context, size));
		}

		const Allocation& alloc = inputBuffers[i + 1]->getAllocation();
		initializeMemory(context, alloc, extraDatas[i]);
	}

	DescriptorSetLayoutBuilder layoutBuilder;

	for (deUint32 i = 0; i < inputBuffers.size(); i++)
	{
		layoutBuilder.addBinding(inputBuffers[i]->getType(), 1,
								 VK_SHADER_STAGE_GEOMETRY_BIT, DE_NULL);
	}

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		layoutBuilder.build(context.getDeviceInterface(), context.getDevice()));

	const Unique<VkPipelineLayout> pipelineLayout(
		makePipelineLayout(context, *descriptorSetLayout));

	const Unique<VkRenderPass> renderPass(makeRenderPass(context, VK_FORMAT_R32_SFLOAT));
	const Unique<VkPipeline> pipeline(makeGraphicsPipeline(context, *pipelineLayout,
									  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT,
									  *vertexShaderModule, DE_NULL, *geometryShaderModule, DE_NULL, DE_NULL,
									  *renderPass, VK_PRIMITIVE_TOPOLOGY_POINT_LIST));

	DescriptorPoolBuilder poolBuilder;

	for (deUint32 i = 0; i < inputBuffers.size(); i++)
	{
		poolBuilder.addType(inputBuffers[i]->getType());
	}

	const Unique<VkDescriptorPool> descriptorPool(
		poolBuilder.build(context.getDeviceInterface(), context.getDevice(),
						  VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	// Create descriptor set
	const Unique<VkDescriptorSet> descriptorSet(
		makeDescriptorSet(context, *descriptorPool, *descriptorSetLayout));

	DescriptorSetUpdateBuilder updateBuilder;

	for (deUint32 i = 0; i < inputBuffers.size(); i++)
	{
		if (inputBuffers[i]->isImage())
		{
			VkDescriptorImageInfo info =
				makeDescriptorImageInfo(inputBuffers[i]->getAsImage()->getSampler(),
										inputBuffers[i]->getAsImage()->getImageView(), VK_IMAGE_LAYOUT_GENERAL);

			updateBuilder.writeSingle(*descriptorSet,
									  DescriptorSetUpdateBuilder::Location::binding(i),
									  inputBuffers[i]->getType(), &info);
		}
		else
		{
			VkDescriptorBufferInfo info =
				makeDescriptorBufferInfo(inputBuffers[i]->getAsBuffer()->getBuffer(),
										 0ull, inputBuffers[i]->getAsBuffer()->getSize());

			updateBuilder.writeSingle(*descriptorSet,
									  DescriptorSetUpdateBuilder::Location::binding(i),
									  inputBuffers[i]->getType(), &info);
		}
	}

	updateBuilder.update(context.getDeviceInterface(), context.getDevice());

	const Unique<VkCommandPool> cmdPool(makeCommandPool(context));

	const deUint32 subgroupSize = getSubgroupSize(context);

	const Unique<VkCommandBuffer> cmdBuffer(
		makeCommandBuffer(context, *cmdPool));

	unsigned totalIterations = 0;
	unsigned failedIterations = 0;

	Image discardableImage(context, 1, 1, VK_FORMAT_R32_SFLOAT,
						   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
						   VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	for (deUint32 width = 1; width < maxWidth; width++)
	{
		for (deUint32 i = 1; i < inputBuffers.size(); i++)
		{
			// re-init the data
			const Allocation& alloc = inputBuffers[i]->getAllocation();
			initializeMemory(context, alloc, extraDatas[i - 1]);
		}

		totalIterations++;

		const Unique<VkFramebuffer> framebuffer(makeFramebuffer(context,
												*renderPass, discardableImage.getImageView(), 1, 1));

		const VkClearValue clearValue = {{{0.0f, 0.0f, 0.0f, 0.0f}}};

		const VkRenderPassBeginInfo renderPassBeginInfo = {
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, DE_NULL, *renderPass,
			*framebuffer, {{0, 0}, {1, 1}}, 1, &clearValue,
		};

		beginCommandBuffer(context, *cmdBuffer);

		VkViewport viewport = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};

		context.getDeviceInterface().cmdSetViewport(
			*cmdBuffer, 0, 1, &viewport);

		VkRect2D scissor = {{0, 0}, {1, 1}};

		context.getDeviceInterface().cmdSetScissor(
			*cmdBuffer, 0, 1, &scissor);

		context.getDeviceInterface().cmdBeginRenderPass(
			*cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		context.getDeviceInterface().cmdBindPipeline(
			*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

		context.getDeviceInterface().cmdBindDescriptorSets(*cmdBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
				&descriptorSet.get(), 0u, DE_NULL);

		context.getDeviceInterface().cmdDraw(*cmdBuffer, width, 1, 0, 0);

		context.getDeviceInterface().cmdEndRenderPass(*cmdBuffer);

		endCommandBuffer(context, *cmdBuffer);

		Move<VkFence> fence(submitCommandBuffer(context, *cmdBuffer));

		waitFence(context, fence);

		std::vector<const void*> datas;

		for (deUint32 i = 0; i < inputBuffers.size(); i++)
		{
			if (!inputBuffers[i]->isImage())
			{
				const Allocation& resultAlloc = inputBuffers[i]->getAllocation();
				invalidateMappedMemoryRange(context.getDeviceInterface(),
											context.getDevice(), resultAlloc.getMemory(),
											resultAlloc.getOffset(), inputBuffers[i]->getAsBuffer()->getSize());

				// we always have our result data first
				datas.push_back(resultAlloc.getHostPtr());
			}
		}

		if (!checkResult(datas, width, subgroupSize))
		{
			failedIterations++;
		}

		context.getDeviceInterface().resetCommandBuffer(*cmdBuffer, 0);
	}

	if (0 < failedIterations)
	{
		context.getTestContext().getLog()
				<< TestLog::Message << (totalIterations - failedIterations) << " / "
				<< totalIterations << " values passed" << TestLog::EndMessage;
		return tcu::TestStatus::fail("Failed!");
	}

	return tcu::TestStatus::pass("OK");
}

tcu::TestStatus vkt::subgroups::makeVertexTest(
	Context& context, VkFormat format, SSBOData* extraDatas,
	deUint32 extraDatasCount,
	bool (*checkResult)(std::vector<const void*> datas, deUint32 width, deUint32 subgroupSize))
{
	const deUint32 maxWidth = 1024;

	const Unique<VkShaderModule> vertexShaderModule(
		createShaderModule(context.getDeviceInterface(), context.getDevice(),
						   context.getBinaryCollection().get("vert"), 0u));

	std::vector< de::SharedPtr<BufferOrImage> > inputBuffers(extraDatasCount + 1);

	// The implicit result SSBO we use to store our outputs from the vertex shader
	{
		vk::VkDeviceSize size = getFormatSizeInBytes(format) * maxWidth;
		inputBuffers[0] = de::SharedPtr<BufferOrImage>(new Buffer(context, size));
	}

	for (deUint32 i = 0; i < (inputBuffers.size() - 1); i++)
	{
		if (extraDatas[i].isImage)
		{
			inputBuffers[i + 1] = de::SharedPtr<BufferOrImage>(new Image(context,
											static_cast<deUint32>(extraDatas[i].numElements), 1, extraDatas[i].format));
		}
		else
		{
			vk::VkDeviceSize size =
				getFormatSizeInBytes(extraDatas[i].format) * extraDatas[i].numElements;
			inputBuffers[i + 1] = de::SharedPtr<BufferOrImage>(new Buffer(context, size));
		}

		const Allocation& alloc = inputBuffers[i + 1]->getAllocation();
		initializeMemory(context, alloc, extraDatas[i]);
	}

	DescriptorSetLayoutBuilder layoutBuilder;

	for (deUint32 i = 0; i < inputBuffers.size(); i++)
	{
		layoutBuilder.addBinding(inputBuffers[i]->getType(), 1,
								 VK_SHADER_STAGE_VERTEX_BIT, DE_NULL);
	}

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		layoutBuilder.build(context.getDeviceInterface(), context.getDevice()));

	const Unique<VkPipelineLayout> pipelineLayout(
		makePipelineLayout(context, *descriptorSetLayout));

	const Unique<VkRenderPass> renderPass(makeRenderPass(context, VK_FORMAT_R32_SFLOAT));
	const Unique<VkPipeline> pipeline(makeGraphicsPipeline(context, *pipelineLayout,
									  VK_SHADER_STAGE_VERTEX_BIT, *vertexShaderModule, DE_NULL, DE_NULL, DE_NULL, DE_NULL,
									  *renderPass, VK_PRIMITIVE_TOPOLOGY_POINT_LIST));

	DescriptorPoolBuilder poolBuilder;

	for (deUint32 i = 0; i < inputBuffers.size(); i++)
	{
		poolBuilder.addType(inputBuffers[i]->getType());
	}

	const Unique<VkDescriptorPool> descriptorPool(
		poolBuilder.build(context.getDeviceInterface(), context.getDevice(),
						  VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	// Create descriptor set
	const Unique<VkDescriptorSet> descriptorSet(
		makeDescriptorSet(context, *descriptorPool, *descriptorSetLayout));

	DescriptorSetUpdateBuilder updateBuilder;

	for (deUint32 i = 0; i < inputBuffers.size(); i++)
	{
		if (inputBuffers[i]->isImage())
		{
			VkDescriptorImageInfo info =
				makeDescriptorImageInfo(inputBuffers[i]->getAsImage()->getSampler(),
										inputBuffers[i]->getAsImage()->getImageView(), VK_IMAGE_LAYOUT_GENERAL);

			updateBuilder.writeSingle(*descriptorSet,
									  DescriptorSetUpdateBuilder::Location::binding(i),
									  inputBuffers[i]->getType(), &info);
		}
		else
		{
			VkDescriptorBufferInfo info =
				makeDescriptorBufferInfo(inputBuffers[i]->getAsBuffer()->getBuffer(),
										 0ull, inputBuffers[i]->getAsBuffer()->getSize());

			updateBuilder.writeSingle(*descriptorSet,
									  DescriptorSetUpdateBuilder::Location::binding(i),
									  inputBuffers[i]->getType(), &info);
		}
	}

	updateBuilder.update(context.getDeviceInterface(), context.getDevice());

	const Unique<VkCommandPool> cmdPool(makeCommandPool(context));

	const deUint32 subgroupSize = getSubgroupSize(context);

	const Unique<VkCommandBuffer> cmdBuffer(
		makeCommandBuffer(context, *cmdPool));

	unsigned totalIterations = 0;
	unsigned failedIterations = 0;

	Image discardableImage(context, 1, 1, VK_FORMAT_R32_SFLOAT,
						   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
						   VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	for (deUint32 width = 1; width < maxWidth; width++)
	{
		for (deUint32 i = 1; i < inputBuffers.size(); i++)
		{
			// re-init the data
			const Allocation& alloc = inputBuffers[i]->getAllocation();
			initializeMemory(context, alloc, extraDatas[i - 1]);
		}

		totalIterations++;

		const Unique<VkFramebuffer> framebuffer(makeFramebuffer(context,
												*renderPass, discardableImage.getImageView(), 1, 1));

		const VkClearValue clearValue = {{{0.0f, 0.0f, 0.0f, 0.0f}}};

		const VkRenderPassBeginInfo renderPassBeginInfo = {
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, DE_NULL, *renderPass,
			*framebuffer, {{0, 0}, {1, 1}}, 1, &clearValue,
		};

		beginCommandBuffer(context, *cmdBuffer);

		VkViewport viewport = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};

		context.getDeviceInterface().cmdSetViewport(
			*cmdBuffer, 0, 1, &viewport);

		VkRect2D scissor = {{0, 0}, {1, 1}};

		context.getDeviceInterface().cmdSetScissor(
			*cmdBuffer, 0, 1, &scissor);

		context.getDeviceInterface().cmdBeginRenderPass(
			*cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		context.getDeviceInterface().cmdBindPipeline(
			*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

		context.getDeviceInterface().cmdBindDescriptorSets(*cmdBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
				&descriptorSet.get(), 0u, DE_NULL);

		context.getDeviceInterface().cmdDraw(*cmdBuffer, width, 1, 0, 0);

		context.getDeviceInterface().cmdEndRenderPass(*cmdBuffer);

		endCommandBuffer(context, *cmdBuffer);

		Move<VkFence> fence(submitCommandBuffer(context, *cmdBuffer));

		waitFence(context, fence);

		std::vector<const void*> datas;

		for (deUint32 i = 0; i < inputBuffers.size(); i++)
		{
			if (!inputBuffers[i]->isImage())
			{
				const Allocation& resultAlloc = inputBuffers[i]->getAllocation();
				invalidateMappedMemoryRange(context.getDeviceInterface(),
											context.getDevice(), resultAlloc.getMemory(),
											resultAlloc.getOffset(), inputBuffers[i]->getAsBuffer()->getSize());

				// we always have our result data first
				datas.push_back(resultAlloc.getHostPtr());
			}
		}

		if (!checkResult(datas, width, subgroupSize))
		{
			failedIterations++;
		}

		context.getDeviceInterface().resetCommandBuffer(*cmdBuffer, 0);
	}

	if (0 < failedIterations)
	{
		context.getTestContext().getLog()
				<< TestLog::Message << (totalIterations - failedIterations) << " / "
				<< totalIterations << " values passed" << TestLog::EndMessage;
		return tcu::TestStatus::fail("Failed!");
	}

	return tcu::TestStatus::pass("OK");
}

tcu::TestStatus vkt::subgroups::makeFragmentTest(
	Context& context, VkFormat format, SSBOData* extraDatas,
	deUint32 extraDatasCount,
	bool (*checkResult)(std::vector<const void*> datas, deUint32 width,
						deUint32 height, deUint32 subgroupSize))
{
	const Unique<VkShaderModule> vertexShaderModule(
		createShaderModule(context.getDeviceInterface(), context.getDevice(),
						   context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule> fragmentShaderModule(
		createShaderModule(context.getDeviceInterface(), context.getDevice(),
						   context.getBinaryCollection().get("frag"), 0u));

	std::vector< de::SharedPtr<BufferOrImage> > inputBuffers(extraDatasCount);

	for (deUint32 i = 0; i < extraDatasCount; i++)
	{
		if (extraDatas[i].isImage)
		{
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Image(context,
										static_cast<deUint32>(extraDatas[i].numElements), 1, extraDatas[i].format));
		}
		else
		{
			vk::VkDeviceSize size =
				getFormatSizeInBytes(extraDatas[i].format) * extraDatas[i].numElements;
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Buffer(context, size));
		}

		const Allocation& alloc = inputBuffers[i]->getAllocation();
		initializeMemory(context, alloc, extraDatas[i]);
	}

	DescriptorSetLayoutBuilder layoutBuilder;

	for (deUint32 i = 0; i < extraDatasCount; i++)
	{
		layoutBuilder.addBinding(inputBuffers[i]->getType(), 1,
								 VK_SHADER_STAGE_FRAGMENT_BIT, DE_NULL);
	}

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		layoutBuilder.build(context.getDeviceInterface(), context.getDevice()));

	const Unique<VkPipelineLayout> pipelineLayout(
		makePipelineLayout(context, *descriptorSetLayout));

	const Unique<VkRenderPass> renderPass(makeRenderPass(context, format));
	const Unique<VkPipeline> pipeline(makeGraphicsPipeline(context, *pipelineLayout,
									  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
									  *vertexShaderModule, *fragmentShaderModule, DE_NULL, DE_NULL, DE_NULL, *renderPass));

	DescriptorPoolBuilder poolBuilder;

	// To stop validation complaining, always add at least one type to pool.
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	for (deUint32 i = 0; i < extraDatasCount; i++)
	{
		poolBuilder.addType(inputBuffers[i]->getType());
	}

	const Unique<VkDescriptorPool> descriptorPool(
		poolBuilder.build(context.getDeviceInterface(), context.getDevice(),
						  VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	// Create descriptor set
	const Unique<VkDescriptorSet> descriptorSet(
		makeDescriptorSet(context, *descriptorPool, *descriptorSetLayout));

	DescriptorSetUpdateBuilder updateBuilder;

	for (deUint32 i = 0; i < extraDatasCount; i++)
	{
		if (inputBuffers[i]->isImage())
		{
			VkDescriptorImageInfo info =
				makeDescriptorImageInfo(inputBuffers[i]->getAsImage()->getSampler(),
										inputBuffers[i]->getAsImage()->getImageView(), VK_IMAGE_LAYOUT_GENERAL);

			updateBuilder.writeSingle(*descriptorSet,
									  DescriptorSetUpdateBuilder::Location::binding(i),
									  inputBuffers[i]->getType(), &info);
		}
		else
		{
			VkDescriptorBufferInfo info =
				makeDescriptorBufferInfo(inputBuffers[i]->getAsBuffer()->getBuffer(),
										 0ull, inputBuffers[i]->getAsBuffer()->getSize());

			updateBuilder.writeSingle(*descriptorSet,
									  DescriptorSetUpdateBuilder::Location::binding(i),
									  inputBuffers[i]->getType(), &info);
		}
	}

	updateBuilder.update(context.getDeviceInterface(), context.getDevice());

	const Unique<VkCommandPool> cmdPool(makeCommandPool(context));

	const deUint32 subgroupSize = getSubgroupSize(context);

	const Unique<VkCommandBuffer> cmdBuffer(
		makeCommandBuffer(context, *cmdPool));

	unsigned totalIterations = 0;
	unsigned failedIterations = 0;

	for (deUint32 width = 8; width <= subgroupSize; width *= 2)
	{
		for (deUint32 height = 8; height <= subgroupSize; height *= 2)
		{
			totalIterations++;

			// re-init the data
			for (deUint32 i = 0; i < extraDatasCount; i++)
			{
				const Allocation& alloc = inputBuffers[i]->getAllocation();
				initializeMemory(context, alloc, extraDatas[i]);
			}

			VkDeviceSize formatSize = getFormatSizeInBytes(format);
			const VkDeviceSize resultImageSizeInBytes =
				width * height * formatSize;

			Image resultImage(context, width, height, format,
							  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
							  VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

			Buffer resultBuffer(context, resultImageSizeInBytes,
								VK_IMAGE_USAGE_TRANSFER_DST_BIT);

			const Unique<VkFramebuffer> framebuffer(makeFramebuffer(context,
													*renderPass, resultImage.getImageView(), width, height));

			const VkClearValue clearValue = {{{0.0f, 0.0f, 0.0f, 0.0f}}};

			const VkRenderPassBeginInfo renderPassBeginInfo = {
				VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, DE_NULL, *renderPass,
				*framebuffer, {{0, 0}, {width, height}}, 1, &clearValue,
			};

			beginCommandBuffer(context, *cmdBuffer);

			VkViewport viewport = {0.0f, 0.0f, static_cast<float>(width),
								   static_cast<float>(height), 0.0f, 1.0f
								  };

			context.getDeviceInterface().cmdSetViewport(
				*cmdBuffer, 0, 1, &viewport);

			VkRect2D scissor = {{0, 0}, {width, height}};

			context.getDeviceInterface().cmdSetScissor(
				*cmdBuffer, 0, 1, &scissor);

			context.getDeviceInterface().cmdBeginRenderPass(
				*cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			context.getDeviceInterface().cmdBindPipeline(
				*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			context.getDeviceInterface().cmdBindDescriptorSets(*cmdBuffer,
					VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
					&descriptorSet.get(), 0u, DE_NULL);

			context.getDeviceInterface().cmdDraw(*cmdBuffer, 3, 1, 0, 0);

			context.getDeviceInterface().cmdEndRenderPass(*cmdBuffer);

			vk::VkBufferImageCopy region = {0, 0, 0,
				{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0},
				{width, height, 1}
			};
			context.getDeviceInterface().cmdCopyImageToBuffer(*cmdBuffer,
					resultImage.getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					resultBuffer.getBuffer(), 1, &region);

			endCommandBuffer(context, *cmdBuffer);

			Move<VkFence> fence(submitCommandBuffer(context, *cmdBuffer));

			waitFence(context, fence);

			std::vector<const void*> datas;
			{
				const Allocation& resultAlloc = resultBuffer.getAllocation();
				invalidateMappedMemoryRange(context.getDeviceInterface(),
											context.getDevice(), resultAlloc.getMemory(),
											resultAlloc.getOffset(), resultImageSizeInBytes);

				// we always have our result data first
				datas.push_back(resultAlloc.getHostPtr());
			}

			for (deUint32 i = 0; i < extraDatasCount; i++)
			{
				if (!inputBuffers[i]->isImage())
				{
					const Allocation& resultAlloc = inputBuffers[i]->getAllocation();
					invalidateMappedMemoryRange(context.getDeviceInterface(),
												context.getDevice(), resultAlloc.getMemory(),
												resultAlloc.getOffset(), inputBuffers[i]->getAsBuffer()->getSize());

					// we always have our result data first
					datas.push_back(resultAlloc.getHostPtr());
				}
			}

			if (!checkResult(datas, width, height, subgroupSize))
			{
				failedIterations++;
			}

			context.getDeviceInterface().resetCommandBuffer(*cmdBuffer, 0);
		}
	}

	if (0 < failedIterations)
	{
		context.getTestContext().getLog()
				<< TestLog::Message << (totalIterations - failedIterations) << " / "
				<< totalIterations << " values passed" << TestLog::EndMessage;
		return tcu::TestStatus::fail("Failed!");
	}

	return tcu::TestStatus::pass("OK");
}

tcu::TestStatus vkt::subgroups::makeComputeTest(
	Context& context, VkFormat format, SSBOData* inputs, deUint32 inputsCount,
	bool (*checkResult)(std::vector<const void*> datas,
						const deUint32 numWorkgroups[3], const deUint32 localSize[3],
						deUint32 subgroupSize))
{
	VkDeviceSize elementSize = getFormatSizeInBytes(format);

	const VkDeviceSize resultBufferSize = maxSupportedSubgroupSize() *
										  maxSupportedSubgroupSize() *
										  maxSupportedSubgroupSize();
	const VkDeviceSize resultBufferSizeInBytes = resultBufferSize * elementSize;

	Buffer resultBuffer(
		context, resultBufferSizeInBytes);

	std::vector< de::SharedPtr<BufferOrImage> > inputBuffers(inputsCount);

	for (deUint32 i = 0; i < inputsCount; i++)
	{
		if (inputs[i].isImage)
		{
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Image(context,
										static_cast<deUint32>(inputs[i].numElements), 1, inputs[i].format));
		}
		else
		{
			vk::VkDeviceSize size =
				getFormatSizeInBytes(inputs[i].format) * inputs[i].numElements;
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Buffer(context, size));
		}

		const Allocation& alloc = inputBuffers[i]->getAllocation();
		initializeMemory(context, alloc, inputs[i]);
	}

	DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addBinding(
		resultBuffer.getType(), 1, VK_SHADER_STAGE_COMPUTE_BIT, DE_NULL);

	for (deUint32 i = 0; i < inputsCount; i++)
	{
		layoutBuilder.addBinding(
			inputBuffers[i]->getType(), 1, VK_SHADER_STAGE_COMPUTE_BIT, DE_NULL);
	}

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		layoutBuilder.build(context.getDeviceInterface(), context.getDevice()));

	const Unique<VkShaderModule> shaderModule(
		createShaderModule(context.getDeviceInterface(), context.getDevice(),
						   context.getBinaryCollection().get("comp"), 0u));
	const Unique<VkPipelineLayout> pipelineLayout(
		makePipelineLayout(context, *descriptorSetLayout));

	DescriptorPoolBuilder poolBuilder;

	poolBuilder.addType(resultBuffer.getType());

	for (deUint32 i = 0; i < inputsCount; i++)
	{
		poolBuilder.addType(inputBuffers[i]->getType());
	}

	const Unique<VkDescriptorPool> descriptorPool(
		poolBuilder.build(context.getDeviceInterface(), context.getDevice(),
						  VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	// Create descriptor set
	const Unique<VkDescriptorSet> descriptorSet(
		makeDescriptorSet(context, *descriptorPool, *descriptorSetLayout));

	DescriptorSetUpdateBuilder updateBuilder;

	const VkDescriptorBufferInfo resultDescriptorInfo =
		makeDescriptorBufferInfo(
			resultBuffer.getBuffer(), 0ull, resultBufferSizeInBytes);

	updateBuilder.writeSingle(*descriptorSet,
							  DescriptorSetUpdateBuilder::Location::binding(0u),
							  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultDescriptorInfo);

	for (deUint32 i = 0; i < inputsCount; i++)
	{
		if (inputBuffers[i]->isImage())
		{
			VkDescriptorImageInfo info =
				makeDescriptorImageInfo(inputBuffers[i]->getAsImage()->getSampler(),
										inputBuffers[i]->getAsImage()->getImageView(), VK_IMAGE_LAYOUT_GENERAL);

			updateBuilder.writeSingle(*descriptorSet,
									  DescriptorSetUpdateBuilder::Location::binding(i + 1),
									  inputBuffers[i]->getType(), &info);
		}
		else
		{
			vk::VkDeviceSize size =
				getFormatSizeInBytes(inputs[i].format) * inputs[i].numElements;
			VkDescriptorBufferInfo info =
				makeDescriptorBufferInfo(inputBuffers[i]->getAsBuffer()->getBuffer(), 0ull, size);

			updateBuilder.writeSingle(*descriptorSet,
									  DescriptorSetUpdateBuilder::Location::binding(i + 1),
									  inputBuffers[i]->getType(), &info);
		}
	}

	updateBuilder.update(context.getDeviceInterface(), context.getDevice());

	const Unique<VkCommandPool> cmdPool(makeCommandPool(context));

	unsigned totalIterations = 0;
	unsigned failedIterations = 0;

	const deUint32 subgroupSize = getSubgroupSize(context);

	const Unique<VkCommandBuffer> cmdBuffer(
		makeCommandBuffer(context, *cmdPool));

	const deUint32 numWorkgroups[3] = {4, 4, 4};

	const deUint32 localSizesToTestCount = 15;
	deUint32 localSizesToTest[localSizesToTestCount][3] =
	{
		{1, 1, 1},
		{32, 4, 1},
		{32, 1, 4},
		{1, 32, 4},
		{1, 4, 32},
		{4, 1, 32},
		{4, 32, 1},
		{subgroupSize, 1, 1},
		{1, subgroupSize, 1},
		{1, 1, subgroupSize},
		{3, 5, 7},
		{128, 1, 1},
		{1, 128, 1},
		{1, 1, 64},
		{1, 1, 1} // Isn't used, just here to make double buffering checks easier
	};

	Move<VkPipeline> lastPipeline(
		makeComputePipeline(context, *pipelineLayout, *shaderModule,
							localSizesToTest[0][0], localSizesToTest[0][1], localSizesToTest[0][2]));

	for (deUint32 index = 0; index < (localSizesToTestCount - 1); index++)
	{
		const deUint32 nextX = localSizesToTest[index + 1][0];
		const deUint32 nextY = localSizesToTest[index + 1][1];
		const deUint32 nextZ = localSizesToTest[index + 1][2];

		// we are running one test
		totalIterations++;

		beginCommandBuffer(context, *cmdBuffer);

		context.getDeviceInterface().cmdBindPipeline(
			*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *lastPipeline);

		context.getDeviceInterface().cmdBindDescriptorSets(*cmdBuffer,
				VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
				&descriptorSet.get(), 0u, DE_NULL);

		context.getDeviceInterface().cmdDispatch(*cmdBuffer,
				numWorkgroups[0], numWorkgroups[1], numWorkgroups[2]);

		endCommandBuffer(context, *cmdBuffer);

		Move<VkFence> fence(submitCommandBuffer(context, *cmdBuffer));

		Move<VkPipeline> nextPipeline(
			makeComputePipeline(context, *pipelineLayout, *shaderModule,
								nextX, nextY, nextZ));

		waitFence(context, fence);

		std::vector<const void*> datas;

		{
			const Allocation& resultAlloc = resultBuffer.getAllocation();
			invalidateMappedMemoryRange(context.getDeviceInterface(),
										context.getDevice(), resultAlloc.getMemory(),
										resultAlloc.getOffset(), resultBufferSizeInBytes);

			// we always have our result data first
			datas.push_back(resultAlloc.getHostPtr());
		}

		for (deUint32 i = 0; i < inputsCount; i++)
		{
			if (!inputBuffers[i]->isImage())
			{
				vk::VkDeviceSize size =
					getFormatSizeInBytes(inputs[i].format) *
					inputs[i].numElements;
				const Allocation& resultAlloc = inputBuffers[i]->getAllocation();
				invalidateMappedMemoryRange(context.getDeviceInterface(),
											context.getDevice(), resultAlloc.getMemory(),
											resultAlloc.getOffset(), size);

				// we always have our result data first
				datas.push_back(resultAlloc.getHostPtr());
			}
		}

		if (!checkResult(datas, numWorkgroups, localSizesToTest[index], subgroupSize))
		{
			failedIterations++;
		}

		context.getDeviceInterface().resetCommandBuffer(*cmdBuffer, 0);

		lastPipeline = nextPipeline;
	}

	if (0 < failedIterations)
	{
		context.getTestContext().getLog()
				<< TestLog::Message << (totalIterations - failedIterations) << " / "
				<< totalIterations << " values passed" << TestLog::EndMessage;
		return tcu::TestStatus::fail("Failed!");
	}

	return tcu::TestStatus::pass("OK");
}
