/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 Google Inc.
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
 * \brief Descriptor set tests
 *//*--------------------------------------------------------------------*/

#include "amber/vktAmberTestCase.hpp"
#include "vktApiDescriptorSetTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkMemUtil.hpp"
#include "vktApiBufferComputeInstance.hpp"
#include "vktApiComputeInstanceResultBuffer.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"

#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkPrograms.hpp"

namespace vkt
{
namespace api
{

namespace
{

using namespace std;
using namespace vk;

// Descriptor set layout used to create a pipeline layout is destroyed prior to creating a pipeline
Move<VkPipelineLayout> createPipelineLayoutDestroyDescriptorSetLayout (Context& context)
{
	const DeviceInterface&					vk							= context.getDeviceInterface();
	const VkDevice							device						= context.getDevice();
	Unique<VkDescriptorSetLayout>			descriptorSetLayout			(createDescriptorSetLayout(context));

	const VkPipelineLayoutCreateInfo		pipelineLayoutCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,			// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		(VkPipelineLayoutCreateFlags)0,							// VkPipelineLayoutCreateFlags		flags;
		1u,														// deUint32							setLayoutCount;
		&descriptorSetLayout.get(),								// const VkDescriptorSetLayout*		pSetLayouts;
		0u,														// deUint32							pushConstantRangeCount;
		DE_NULL													// const VkPushConstantRange*		pPushConstantRanges;
	};

	return createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);
}

tcu::TestStatus descriptorSetLayoutLifetimeGraphicsTest (Context& context)
{
	const DeviceInterface&							vk								= context.getDeviceInterface();
	const VkDevice									device							= context.getDevice();
    deUint32					                    queueFamilyIndex                = context.getUniversalQueueFamilyIndex();
    const VkQueue					                queue				            = context.getUniversalQueue();

	Unique<VkPipelineLayout>						pipelineLayout					(createPipelineLayoutDestroyDescriptorSetLayout(context));

	const Unique<VkShaderModule>					vertexShaderModule				(createShaderModule(vk, device, context.getBinaryCollection().get("vertex"), 0));

	const VkPipelineShaderStageCreateInfo			shaderStageCreateInfo			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags	flags;
		VK_SHADER_STAGE_VERTEX_BIT,								// VkShaderStageFlagBits			stage;
		vertexShaderModule.get(),								// VkShaderModule					shader;
		"main",													// const char*						pName;
		DE_NULL,												// const VkSpecializationInfo*		pSpecializationInfo;
	};

	const VkPipelineVertexInputStateCreateInfo		vertexInputStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		(VkPipelineVertexInputStateCreateFlags)0,					// VkPipelineVertexInputStateCreateFlags	flags;
		0u,															// deUint32									vertexBindingDescriptionCount;
		DE_NULL,													// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
		0u,															// deUint32									vertexAttributeDescriptionCount;
		DE_NULL														// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		(VkPipelineInputAssemblyStateCreateFlags)0,						// VkPipelineInputAssemblyStateCreateFlags	flags;
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,							// VkPrimitiveTopology						topology;
		VK_FALSE														// VkBool32									primitiveRestartEnable;
	};

	const VkPipelineRasterizationStateCreateInfo	rasterizationStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		(VkPipelineRasterizationStateCreateFlags)0,						// VkPipelineRasterizationStateCreateFlags	flags;
		VK_FALSE,														// VkBool32									depthClampEnable;
		VK_TRUE,														// VkBool32									rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,											// VkPolygonMode							polygonMode;
		VK_CULL_MODE_NONE,												// VkCullModeFlags							cullMode;
		VK_FRONT_FACE_CLOCKWISE,										// VkFrontFace								frontFace;
		VK_FALSE,														// VkBool32									depthBiasEnable;
		0.0f,															// float									depthBiasConstantFactor;
		0.0f,															// float									depthBiasClamp;
		0.0f,															// float									depthBiasSlopeFactor;
		1.0f															// float									lineWidth;
	};

	const VkSubpassDescription						subpassDescription				=
	{
		(VkSubpassDescriptionFlags)0,		// VkSubpassDescriptionFlags		flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint				pipelineBindPoint
		0u,									// deUint32							inputAttachmentCount
		DE_NULL,							// const VkAttachmentReference*		pInputAttachments
		0u,									// deUint32							colorAttachmentCount
		DE_NULL,							// const VkAttachmentReference*		pColorAttachments
		DE_NULL,							// const VkAttachmentReference*		pResolveAttachments
		DE_NULL,							// const VkAttachmentReference*		pDepthStencilAttachment
		0u,									// deUint32							preserveAttachmentCount
		DE_NULL								// const deUint32*					pPreserveAttachments
	};

	const VkRenderPassCreateInfo					renderPassCreateInfo			=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,		// VkStructureType					sType;
		DE_NULL,										// const void*						pNext;
		(VkRenderPassCreateFlags)0,						// VkRenderPassCreateFlags			flags;
		0u,												// deUint32							attachmentCount
		DE_NULL,										// const VkAttachmentDescription*	pAttachments
		1u,												// deUint32							subpassCount
		&subpassDescription,							// const VkSubpassDescription*		pSubpasses
		0u,												// deUint32							dependencyCount
		DE_NULL											// const VkSubpassDependency*		pDependencies
	};

	Unique<VkRenderPass>							renderPass						(createRenderPass(vk, device, &renderPassCreateInfo));

	const VkGraphicsPipelineCreateInfo				graphicsPipelineCreateInfo		=
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType									sType;
		DE_NULL,											// const void*										pNext;
		(VkPipelineCreateFlags)0,							// VkPipelineCreateFlags							flags;
		1u,													// deUint32											stageCount;
		&shaderStageCreateInfo,								// const VkPipelineShaderStageCreateInfo*			pStages;
		&vertexInputStateCreateInfo,						// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
		&inputAssemblyStateCreateInfo,						// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
		DE_NULL,											// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
		DE_NULL,											// const VkPipelineViewportStateCreateInfo*			pViewportState;
		&rasterizationStateCreateInfo,						// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
		DE_NULL,											// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
		DE_NULL,											// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
		DE_NULL,											// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
		DE_NULL,											// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
		pipelineLayout.get(),								// VkPipelineLayout									layout;
		renderPass.get(),									// VkRenderPass										renderPass;
		0u,													// deUint32											subpass;
		DE_NULL,											// VkPipeline										basePipelineHandle;
		0													// int												basePipelineIndex;
	};

	Unique<VkPipeline>								graphicsPipeline				(createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineCreateInfo));


	VkFramebufferCreateInfo framebufferCreateInfo
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,		// VkStructureType			sType
		DE_NULL,										// const void*				pNext
		0,												// VkFramebufferCreateFlags	flags
		*renderPass,									// VkRenderPass				renderPass
		0,												// uint32_t					attachmentCount
		DE_NULL,										// const VkImageView*		pAttachments
		16,												// uint32_t					width
		16,												// uint32_t					height
		1												// uint32_t					layers
	};

	Move <VkFramebuffer> framebuffer = createFramebuffer(vk, device, &framebufferCreateInfo);

	const VkCommandPoolCreateInfo cmdPoolInfo			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,		// Stype
		DE_NULL,										// PNext
		DE_NULL,										// flags
		queueFamilyIndex,								// queuefamilyindex
	};

	const Unique<VkCommandPool>				cmdPool(createCommandPool(vk, device, &cmdPoolInfo));

	const VkCommandBufferAllocateInfo		cmdBufParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	//	VkStructureType			sType;
		DE_NULL,										//	const void*				pNext;
		*cmdPool,										//	VkCommandPool			pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,				//	VkCommandBufferLevel	level;
		1u,												//	uint32_t				bufferCount;
	};

	const Unique<VkCommandBuffer>			cmdBuf(allocateCommandBuffer(vk, device, &cmdBufParams));

	const VkRenderPassBeginInfo renderPassBeginInfo		=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		DE_NULL,
		*renderPass,
		*framebuffer,
		{{0, 0}, {16, 16}},
		0,
		DE_NULL
	};

	beginCommandBuffer(vk, *cmdBuf, 0u);
	{
		vk.cmdBeginRenderPass(*cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vk.cmdBindPipeline(*cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
		vk.cmdDraw(*cmdBuf, 3u, 1u, 0, 0);
		vk.cmdEndRenderPass(*cmdBuf);
    }
    endCommandBuffer(vk, *cmdBuf);

    submitCommandsAndWait(vk, device, queue, *cmdBuf);

	// Test should always pass
	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus descriptorSetLayoutLifetimeComputeTest (Context& context)
{
	const DeviceInterface&					vk							= context.getDeviceInterface();
	const VkDevice							device						= context.getDevice();
    deUint32					            queueFamilyIndex            = context.getUniversalQueueFamilyIndex();
    const VkQueue					        queue				        = context.getUniversalQueue();
    Allocator&								allocator = context.getDefaultAllocator();
    const ComputeInstanceResultBuffer		result(vk, device, allocator, 0.0f);


    Unique<VkPipelineLayout>				pipelineLayout				(createPipelineLayoutDestroyDescriptorSetLayout(context));

	const Unique<VkShaderModule>			computeShaderModule			(createShaderModule(vk, device, context.getBinaryCollection().get("compute"), 0));

	const VkPipelineShaderStageCreateInfo	shaderStageCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags	flags;
		VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits			stage;
		computeShaderModule.get(),								// VkShaderModule					shader;
		"main",													// const char*						pName;
		DE_NULL													// const VkSpecializationInfo*		pSpecializationInfo;
	};

	const VkComputePipelineCreateInfo		computePipelineCreateInfo	=
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,			// VkStructureType					sType
		DE_NULL,												// const void*						pNext
		(VkPipelineCreateFlags)0,								// VkPipelineCreateFlags			flags
		shaderStageCreateInfo,									// VkPipelineShaderStageCreateInfo	stage
		pipelineLayout.get(),									// VkPipelineLayout					layout
		DE_NULL,												// VkPipeline						basePipelineHandle
		0														// int								basePipelineIndex
	};

	const deUint32							offset = (0u);
	const deUint32							addressableSize = 256;
	const deUint32							dataSize = 8;
	de::MovePtr<Allocation>					bufferMem;
	const Unique<VkBuffer>					buffer						(createDataBuffer(context, offset, addressableSize, 0x00, dataSize, 0x5A, &bufferMem));
	const Unique<VkDescriptorSetLayout>		descriptorSetLayout			(createDescriptorSetLayout(context));
	const Unique<VkDescriptorPool>			descriptorPool				(createDescriptorPool(context));
	const Unique<VkDescriptorSet>			descriptorSet				(createDescriptorSet(context, *descriptorPool, *descriptorSetLayout, *buffer, offset, result.getBuffer()));

	Unique<VkPipeline>						computePipeline				(createComputePipeline(vk, device, DE_NULL, &computePipelineCreateInfo));

	const VkCommandPoolCreateInfo cmdPoolInfo				=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,				// Stype
		DE_NULL,												// PNext
		DE_NULL,												// flags
		queueFamilyIndex,										// queuefamilyindex
	};

	const Unique<VkCommandPool>				cmdPool(createCommandPool(vk, device, &cmdPoolInfo));

	const VkCommandBufferAllocateInfo		cmdBufParams	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,			//	VkStructureType			sType;
		DE_NULL,												//	const void*				pNext;
		*cmdPool,												//	VkCommandPool			pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,						//	VkCommandBufferLevel	level;
		1u,														//	uint32_t				bufferCount;
	};

	const Unique<VkCommandBuffer>			cmdBuf(allocateCommandBuffer(vk, device, &cmdBufParams));

	beginCommandBuffer(vk, *cmdBuf, 0u);
	{
		vk.cmdBindPipeline(*cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
		vk.cmdBindDescriptorSets(*cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, 1u, &*descriptorSet, 0, 0);
		vk.cmdDispatch(*cmdBuf, 1u, 1u, 1u);
	}
	endCommandBuffer(vk, *cmdBuf);

	submitCommandsAndWait(vk, device, queue, *cmdBuf);

	// Test should always pass
	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus emptyDescriptorSetLayoutTest (Context& context, VkDescriptorSetLayoutCreateFlags descriptorSetLayoutCreateFlags)
{
	const DeviceInterface&					vk								= context.getDeviceInterface();
	const VkDevice							device							= context.getDevice();

#ifndef CTS_USES_VULKANSC
	if (descriptorSetLayoutCreateFlags == VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR)
		if (!context.isDeviceFunctionalitySupported("VK_KHR_push_descriptor"))
			TCU_THROW(NotSupportedError, "VK_KHR_push_descriptor extension not supported");
#endif // CTS_USES_VULKANSC

	const VkDescriptorSetLayoutCreateInfo	descriptorSetLayoutCreateInfo	=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	// VkStructureType                        sType;
		DE_NULL,												// const void*                            pNext;
		descriptorSetLayoutCreateFlags,							// VkDescriptorSetLayoutCreateFlags       flags;
		0u,														// deUint32                               bindingCount;
		DE_NULL													// const VkDescriptorSetLayoutBinding*    pBindings;
	};

	Unique<VkDescriptorSetLayout>			descriptorSetLayout				(createDescriptorSetLayout(vk, device, &descriptorSetLayoutCreateInfo));

	// Test should always pass
	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus descriptorSetLayoutBindingOrderingTest (Context& context)
{
	/*
		This test tests that if dstBinding has fewer than
		descriptorCount array elements remaining starting from dstArrayElement,
		then the remainder will be used to update the subsequent binding.
	*/

	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkDevice				device				= context.getDevice();
	deUint32					queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	const VkQueue				queue				= context.getUniversalQueue();

	const Unique<VkShaderModule> computeShaderModule (createShaderModule(vk, device, context.getBinaryCollection().get("compute"), 0));

	de::MovePtr<BufferWithMemory> buffer;
	buffer			= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vk,
																		device,
																		context.getDefaultAllocator(),
																		makeBufferCreateInfo(4u, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
																		MemoryRequirement::HostVisible));
	deUint32 *bufferPtr = (deUint32 *)buffer->getAllocation().getHostPtr();
	*bufferPtr = 5;

	de::MovePtr<BufferWithMemory> resultBuffer;
	resultBuffer	= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vk,
																		device,
																		context.getDefaultAllocator(),
																		makeBufferCreateInfo(4u * 3, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
																		MemoryRequirement::HostVisible));

	const VkDescriptorBufferInfo	descriptorBufferInfos[]		=
	{
		{
			buffer->get(),		// VkBuffer			buffer
			0u,					// VkDeviceSize		offset
			VK_WHOLE_SIZE		// VkDeviceSize		range
		},
		{
			buffer->get(),		// VkBuffer			buffer
			0u,					// VkDeviceSize		offset
			VK_WHOLE_SIZE		// VkDeviceSize		range
		},
		{
			buffer->get(),		// VkBuffer			buffer
			0u,					// VkDeviceSize		offset
			VK_WHOLE_SIZE		// VkDeviceSize		range
		},
	};

	const VkDescriptorBufferInfo	descriptorBufferInfoResult	=
	{
		resultBuffer->get(),	// VkBuffer			buffer
		0u,						// VkDeviceSize		offset
		VK_WHOLE_SIZE			// VkDeviceSize		range
	};

	const VkDescriptorSetLayoutBinding layoutBindings[] =
	{
		{
			0u,										// deUint32				binding;
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,		// VkDescriptorType		descriptorType;
			2u,										// deUint32				descriptorCount;
			VK_SHADER_STAGE_ALL,					// VkShaderStageFlags	stageFlags;
			DE_NULL									// const VkSampler*		pImmutableSamplers;
		},
		{
			1u,										// deUint32				binding;
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,		// VkDescriptorType		descriptorType;
			1u,										// deUint32				descriptorCount;
			VK_SHADER_STAGE_ALL,					// VkShaderStageFlags	stageFlags;
			DE_NULL									// const VkSampler*		pImmutableSamplers;
		},
		{
			2u,										// deUint32				binding;
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,		// VkDescriptorType		descriptorType;
			1u,										// deUint32				descriptorCount;
			VK_SHADER_STAGE_ALL,					// VkShaderStageFlags	stageFlags;
			DE_NULL									// const VkSampler*		pImmutableSamplers;
		}
	};

	const VkDescriptorSetLayoutCreateInfo	descriptorSetLayoutCreateInfo	=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,												// const void*								pNext;
		0u,														// VkDescriptorSetLayoutCreateFlags			flags;
		DE_LENGTH_OF_ARRAY(layoutBindings),						// deUint32									bindingCount;
		layoutBindings											// const VkDescriptorSetLayoutBinding*		pBindings;
	};

	Move<VkDescriptorSetLayout> descriptorSetLayout(createDescriptorSetLayout(vk, device, &descriptorSetLayoutCreateInfo));

	const VkDescriptorPoolSize				poolSize[]						=
	{
		{
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,			// VkDescriptorType				type
			3u											// uint32_t						descriptorCount
		},
		{
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,			// VkDescriptorType				type
			1u											// uint32_t						descriptorCount
		}
	};

	const VkDescriptorPoolCreateInfo		descriptorPoolCreateInfo		=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,	// VkStructureType				sType
		DE_NULL,										// const void*					pNext
		0u,												// VkDescriptorPoolCreateFlags	flags
		1u,												// uint32_t						maxSets
		2u,												// uint32_t						poolSizeCount
		poolSize										// const VkDescriptorPoolSize*	pPoolSizes
	};

	Move<VkDescriptorPool> descriptorPool(createDescriptorPool(vk, device, &descriptorPoolCreateInfo));

	VkDescriptorSet descriptorSet;
	{
		const VkDescriptorSetAllocateInfo allocInfo =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,		// VkStructure						sType
			DE_NULL,											// const void*						pNext
			*descriptorPool,									// VkDescriptorPool					descriptorPool
			1u,													// uint32_t							descriptorSetCount
			&*descriptorSetLayout								// const VkDescriptorSetLayout*		pSetLayouts
		};

		VK_CHECK(vk.allocateDescriptorSets(device, &allocInfo, &descriptorSet));
	}

	const VkWriteDescriptorSet				descriptorWrite			=
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,					// VkStructureType					sType
		DE_NULL,												// const void*						pNext
		descriptorSet,											// VkDescriptorSet					dstSet
		0u,														// deUint32							dstBinding
		0u,														// deUint32							dstArrayElement
		3u,														// deUint32							descriptorCount
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,						// VkDescriptorType					descriptorType
		DE_NULL,												// const VkDescriptorImageInfo		pImageInfo
		descriptorBufferInfos,									// const VkDescriptorBufferInfo*	pBufferInfo
		DE_NULL													// const VkBufferView*				pTexelBufferView
	};

	const VkWriteDescriptorSet				descriptorWriteResult	=
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,					// VkStructureType					sType
		DE_NULL,												// const void*						pNext
		descriptorSet,											// VkDescriptorSet					dstSet
		2u,														// deUint32							dstBinding
		0u,														// deUint32							dstArrayElement
		1u,														// deUint32							descriptorCount
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,						// VkDescriptorType					descriptorType
		DE_NULL,												// const VkDescriptorImageInfo		pImageInfo
		&descriptorBufferInfoResult,							// const VkDescriptorBufferInfo*	pBufferInfo
		DE_NULL													// const VkBufferView*				pTexelBufferView
	};

	const VkCommandPoolCreateInfo			cmdPoolInfo				=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,				// VkStructureType					Stype
		DE_NULL,												// const void*						PNext
		DE_NULL,												// VkCommandPoolCreateFlags			flags
		queueFamilyIndex,										// uint32_t							queuefamilyindex
	};

	const Unique<VkCommandPool>				cmdPool(createCommandPool(vk, device, &cmdPoolInfo));

	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,			// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		*cmdPool,												// VkCommandPool					pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,						// VkCommandBufferLevel				level;
		1u,														// uint32_t							bufferCount;
	};

	const Unique<VkCommandBuffer>			cmdBuf(allocateCommandBuffer(vk, device, &cmdBufParams));

	const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,			// VkStructureType					sType
		DE_NULL,												// const void*						pNext
		0,														// VkPipelineLayoutCreateFlags		flags
		1u,														// uint32_t							setLayoutCount
		&*descriptorSetLayout,									// const VkDescriptorSetLayout*		pSetLayouts
		0u,														// uint32_t							pushConstantRangeCount
		nullptr													// const VkPushConstantRange*		pPushConstantRanges
	};

	Move<VkPipelineLayout> pipelineLayout(createPipelineLayout(vk, device, &pipelineLayoutCreateInfo));

	const VkPipelineShaderStageCreateInfo	shaderStageCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags	flags;
		VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits			stage;
		computeShaderModule.get(),								// VkShaderModule					shader;
		"main",													// const char*						pName;
		DE_NULL													// const VkSpecializationInfo*		pSpecializationInfo;
	};

	const VkComputePipelineCreateInfo		computePipelineCreateInfo	=
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,			// VkStructureType					sType
		DE_NULL,												// const void*						pNext
		(VkPipelineCreateFlags)0,								// VkPipelineCreateFlags			flags
		shaderStageCreateInfo,									// VkPipelineShaderStageCreateInfo	stage
		*pipelineLayout,										// VkPipelineLayout					layout
		DE_NULL,												// VkPipeline						basePipelineHandle
		0														// int								basePipelineIndex
	};

	Unique<VkPipeline> computePipeline(createComputePipeline(vk, device, DE_NULL, &computePipelineCreateInfo));

	beginCommandBuffer(vk, *cmdBuf, 0u);
	{
		vk.cmdBindPipeline(*cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
		vk.updateDescriptorSets(device, 1u, &descriptorWrite, 0u, DE_NULL);
		vk.updateDescriptorSets(device, 1u, &descriptorWriteResult, 0u, DE_NULL);
		vk.cmdBindDescriptorSets(*cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, 1u, &descriptorSet, 0, nullptr);
		flushAlloc(vk, device, buffer->getAllocation());
		vk.cmdDispatch(*cmdBuf, 1u, 1u, 1u);
	}

	endCommandBuffer(vk, *cmdBuf);
	submitCommandsAndWait(vk, device, queue, *cmdBuf);

	const Allocation& bufferAllocationResult = resultBuffer->getAllocation();
	invalidateAlloc(vk, device, bufferAllocationResult);

	const deUint32* resultPtr = static_cast<deUint32*>(bufferAllocationResult.getHostPtr());

	if (resultPtr[0] == 5 && resultPtr[1] == 5 && resultPtr[2] == 5)
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("Fail");
}
} // anonymous

void createDescriptorSetLayoutLifetimeGraphicsSource (SourceCollections& dst)
{
	dst.glslSources.add("vertex") << glu::VertexSource(
		"#version 310 es\n"
		"void main (void)\n"
		"{\n"
		"    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
		"}\n");
}

void createDescriptorSetLayoutLifetimeComputeSource (SourceCollections& dst)
{
	dst.glslSources.add("compute") << glu::ComputeSource(
		"#version 310 es\n"
		"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
		"void main (void)\n"
		"{\n"
		"}\n");
}

void createDescriptorSetLayoutBindingOrderingSource (SourceCollections& dst)
{
	dst.glslSources.add("compute") << glu::ComputeSource(
		"#version 310 es\n"
		"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
		"layout (set = 0, binding = 0) uniform UniformBuffer0 {\n"
		"	int data;\n"
		"} uniformbufferarray[2];\n"
		"layout (set = 0, binding = 1) uniform UniformBuffer2 {\n"
		"	int data;\n"
		"} uniformbuffer2;\n"
		"layout (set = 0, binding = 2) buffer StorageBuffer {\n"
		"	int result0;\n"
		"	int result1;\n"
		"	int result2;\n"
		"} results;\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"	results.result0 = uniformbufferarray[0].data;\n"
		"	results.result1 = uniformbufferarray[1].data;\n"
		"	results.result2 = uniformbuffer2.data;\n"
		"}\n");
}

tcu::TestCaseGroup* createDescriptorSetLayoutLifetimeTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> descriptorSetLayoutLifetimeTests(new tcu::TestCaseGroup(testCtx, "descriptor_set_layout_lifetime", "Descriptor set layout lifetime tests"));

	addFunctionCaseWithPrograms(descriptorSetLayoutLifetimeTests.get(), "graphics", "Test descriptor set layout lifetime in graphics pipeline", createDescriptorSetLayoutLifetimeGraphicsSource, descriptorSetLayoutLifetimeGraphicsTest);
	addFunctionCaseWithPrograms(descriptorSetLayoutLifetimeTests.get(), "compute", "Test descriptor set layout lifetime in compute pipeline", createDescriptorSetLayoutLifetimeComputeSource,  descriptorSetLayoutLifetimeComputeTest);

	return descriptorSetLayoutLifetimeTests.release();
}

tcu::TestCaseGroup* createEmptyDescriptorSetLayoutTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> emptyDescriptorSetLayoutTests(new tcu::TestCaseGroup(testCtx, "empty_set", "Create empty descriptor set layout tests"));

	addFunctionCase(emptyDescriptorSetLayoutTests.get(), "normal", "Create empty desciptor set layout", emptyDescriptorSetLayoutTest, (VkDescriptorSetLayoutCreateFlags)0u);
#ifndef CTS_USES_VULKANSC
	// Removed from Vulkan SC test set: VK_KHR_push_descriptor extension removed from Vulkan SC
	addFunctionCase(emptyDescriptorSetLayoutTests.get(), "push_descriptor", "Create empty push descriptor set layout", emptyDescriptorSetLayoutTest, (VkDescriptorSetLayoutCreateFlags)VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
#endif // CTS_USES_VULKANSC
	return emptyDescriptorSetLayoutTests.release();
}

tcu::TestCaseGroup* createDescriptorSetLayoutBindingOrderingTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> descriptorSetLayoutBindingOrderingTests(new tcu::TestCaseGroup(testCtx, "descriptor_set_layout_binding", "Create descriptor set layout ordering tests"));
	addFunctionCaseWithPrograms(descriptorSetLayoutBindingOrderingTests.get(), "update_subsequent_binding", "Test subsequent binding update with remaining elements", createDescriptorSetLayoutBindingOrderingSource, descriptorSetLayoutBindingOrderingTest);

#ifndef CTS_USES_VULKANSC
	static const char dataDir[] = "api/descriptor_set/descriptor_set_layout_binding";
	descriptorSetLayoutBindingOrderingTests->addChild(cts_amber::createAmberTestCase(testCtx, "layout_binding_order", "Test descriptor set layout binding order", dataDir, "layout_binding_order.amber"));
#endif // CTS_USES_VULKANSC

	return descriptorSetLayoutBindingOrderingTests.release();
}

tcu::TestCaseGroup* createDescriptorSetLayoutTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> descriptorSetLayoutTests(new tcu::TestCaseGroup(testCtx, "descriptor_set_layout", "Descriptor set layout tests"));

	descriptorSetLayoutTests->addChild(createEmptyDescriptorSetLayoutTests(testCtx));

	return descriptorSetLayoutTests.release();
}

tcu::TestCaseGroup* createDescriptorSetTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> descriptorSetTests(new tcu::TestCaseGroup(testCtx, "descriptor_set", "Descriptor set tests"));

	descriptorSetTests->addChild(createDescriptorSetLayoutLifetimeTests(testCtx));
	descriptorSetTests->addChild(createDescriptorSetLayoutTests(testCtx));
	descriptorSetTests->addChild(createDescriptorSetLayoutBindingOrderingTests(testCtx));

	return descriptorSetTests.release();
}

} // api
} // vkt
