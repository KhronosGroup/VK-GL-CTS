/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
  * \brief  VkDeviceObjectReservationCreateInfo tests
*//*--------------------------------------------------------------------*/

#include "vktDeviceObjectReservationTests.hpp"

#include <vector>
#include <string>

#include "tcuTestCase.hpp"

#include "vkDefs.hpp"
#include "vkDeviceUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkSafetyCriticalUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktCustomInstancesDevices.hpp"

using namespace vk;

namespace vkt
{
namespace sc
{
namespace
{

enum TestMaxValues
{
	TMV_UNDEFINED = 0,
	TMV_DESCRIPTOR_SET_LAYOUT_BINDING_LIMIT,
	TMV_MAX_IMAGEVIEW_MIPLEVELS,
	TMV_MAX_IMAGEVIEW_ARRAYLAYERS,
	TMV_MAX_LAYEREDIMAGEVIEW_MIPLEVELS,
	TMV_MAX_OCCLUSION_QUERIES_PER_POOL,
	TMV_MAX_PIPELINESTATISTICS_QUERIES_PER_POOL,
	TMV_MAX_TIMESTAMP_QUERIES_PER_POOL
};

const deUint32	VERIFYMAXVALUES_OBJECT_COUNT	= 5U;
const deUint32	VERIFYMAXVALUES_ARRAYLAYERS		= 8U;
const deUint32	VERIFYMAXVALUES_MIPLEVELS		= 5U;

enum TestRequestCounts
{
	TRC_UNDEFINED = 0,
	TRC_SEMAPHORE,
	TRC_COMMAND_BUFFER,
	TRC_FENCE,
	TRC_DEVICE_MEMORY,
	TRC_BUFFER,
	TRC_IMAGE,
	TRC_EVENT,
	TRC_QUERY_POOL,
	TRC_BUFFER_VIEW,
	TRC_IMAGE_VIEW,
	TRC_LAYERED_IMAGE_VIEW,
	TRC_PIPELINE_LAYOUT,
	TRC_RENDER_PASS,
	TRC_GRAPHICS_PIPELINE,
	TRC_COMPUTE_PIPELINE,
	TRC_DESCRIPTORSET_LAYOUT,
	TRC_SAMPLER,
	TRC_DESCRIPTOR_POOL,
	TRC_DESCRIPTORSET,
	TRC_FRAMEBUFFER,
	TRC_COMMANDPOOL,
	TRC_SAMPLERYCBCRCONVERSION,
	TRC_SURFACE,
	TRC_SWAPCHAIN,
	TRC_DISPLAY_MODE,
};

enum TestPoolSizes
{
	PST_UNDEFINED = 0,
	PST_NONE,
	PST_ZERO,
	PST_TOO_SMALL_SIZE,
	PST_ONE_FITS,
	PST_MULTIPLE_FIT,
};

struct TestParams
{
	TestParams (const TestMaxValues& testMaxValues_ = TMV_UNDEFINED, const TestRequestCounts& testRequestCounts_ = TRC_UNDEFINED, const TestPoolSizes& testPoolSizeType_ = PST_UNDEFINED)
		: testMaxValues		{ testMaxValues_ }
		, testRequestCounts	{ testRequestCounts_ }
		, testPoolSizeType	{ testPoolSizeType_ }
	{
	}
	TestMaxValues			testMaxValues;
	TestRequestCounts		testRequestCounts;
	TestPoolSizes			testPoolSizeType;
};

typedef de::SharedPtr<Unique<VkSemaphore>>					SemaphoreSp;
typedef de::SharedPtr<Unique<VkCommandBuffer>>				CommandBufferSp;
typedef de::SharedPtr<Unique<VkFence>>						FenceSp;
typedef de::SharedPtr<Unique<VkDeviceMemory> >				DeviceMemorySp;
typedef de::SharedPtr<Unique<VkBuffer>>						BufferSp;
typedef de::SharedPtr<Unique<VkImage>>						ImageSp;
typedef de::SharedPtr<Unique<VkEvent>>						EventSp;
typedef de::SharedPtr<Unique<VkQueryPool>>					QueryPoolSp;
typedef de::SharedPtr<Unique<VkBufferView>>					BufferViewSp;
typedef de::SharedPtr<Unique<VkImageView>>					ImageViewSp;
typedef de::SharedPtr<Unique<VkPipelineLayout>>				PipelineLayoutSp;
typedef de::SharedPtr<Unique<VkRenderPass>>					RenderPassSp;
typedef de::SharedPtr<Unique<VkPipeline>>					PipelineSp;
typedef de::SharedPtr<Unique<VkDescriptorSetLayout>>		DescriptorSetLayoutSp;
typedef de::SharedPtr<Unique<VkSampler>>					SamplerSp;
typedef de::SharedPtr<Unique<VkDescriptorPool>>				DescriptorPoolSp;
typedef de::SharedPtr<Unique<VkDescriptorSet>>				DescriptorSetSp;
typedef de::SharedPtr<Unique<VkFramebuffer>>				FramebufferSp;
typedef de::SharedPtr<Unique<VkCommandPool>>				CommandPoolSp;
typedef de::SharedPtr<Unique<VkSamplerYcbcrConversion>>		SamplerYcbcrConversionSp;
//typedef de::SharedPtr<Unique<VkSurfaceKHR>>					SurfaceSp;
//typedef de::SharedPtr<Unique<VkSwapchainKHR>>					SwapchainSp;
//typedef de::SharedPtr<Unique<VkDisplayModeKHR>>				DisplayModeSp;
typedef de::SharedPtr<Unique<VkSubpassDescription>>			SubpassDescriptionSp;
typedef de::SharedPtr<Unique<VkAttachmentDescription>>		AttachmentDescriptionSp;


void createSemaphores		(const DeviceInterface&					vkd,
							 const VkDevice							device,
							 std::vector<SemaphoreSp>::iterator		begin,
							 std::vector<SemaphoreSp>::iterator		end)
{
	for(std::vector<SemaphoreSp>::iterator it=begin; it!=end; ++it)
		*it = SemaphoreSp(new Unique<VkSemaphore>(createSemaphore(vkd, device)));
}

void createCommandBuffers	(const DeviceInterface&					vkd,
							 const VkDevice							device,
							 const VkCommandPool					commandPool,
							 std::vector<CommandBufferSp>::iterator	begin,
							 std::vector<CommandBufferSp>::iterator	end)
{
	for (std::vector<CommandBufferSp>::iterator it = begin; it != end; ++it)
	{
		const vk::VkCommandBufferAllocateInfo commandBufferAI =
		{
			vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				// sType
			DE_NULL,														// pNext
			commandPool,													// commandPool
			vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY,							// level
			1u																// commandBufferCount
		};
		*it = CommandBufferSp(new Unique<VkCommandBuffer>(allocateCommandBuffer(vkd, device, &commandBufferAI)));
	}
}

void createFences			(const DeviceInterface&					vkd,
							 const VkDevice							device,
							 std::vector<FenceSp>::iterator			begin,
							 std::vector<FenceSp>::iterator			end)
{
	for (std::vector<FenceSp>::iterator it = begin; it != end; ++it)
	{
		const VkFenceCreateInfo fenceCI =
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,							// VkStructureType		sType
			DE_NULL,														// const void*			pNext
			0u																// VkFenceCreateFlags	flags
		};
		*it = FenceSp(new Unique<VkFence>(createFence(vkd, device, &fenceCI)));
	}
}

void allocateDeviceMemory	(const DeviceInterface&					vkd,
							 const VkDevice							device,
							 VkDeviceSize							size,
							 std::vector<DeviceMemorySp>::iterator	begin,
							 std::vector<DeviceMemorySp>::iterator	end)
{
	for (std::vector<DeviceMemorySp>::iterator it = begin; it != end; ++it)
	{
		VkMemoryAllocateInfo alloc =
		{
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,							// sType
			DE_NULL,														// pNext
			size,															// allocationSize
			0U																// memoryTypeIndex;
		};
		*it = DeviceMemorySp(new Unique<VkDeviceMemory>(allocateMemory(vkd, device, &alloc)));
	}
}

void createBuffers			(const DeviceInterface&					vkd,
							 const VkDevice							device,
							 VkDeviceSize							size,
							 std::vector<BufferSp>::iterator		begin,
							 std::vector<BufferSp>::iterator		end)
{
	deUint32 queueFamilyIndex = 0u;
	for (std::vector<BufferSp>::iterator it = begin; it != end; ++it)
	{
		const VkBufferCreateInfo	bufferCI =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,							// sType
			DE_NULL,														// pNext
			0u,																// flags
			size,															// size
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,								// usage
			VK_SHARING_MODE_EXCLUSIVE,										// sharingMode
			1u,																// queueFamilyIndexCount
			&queueFamilyIndex,												// pQueueFamilyIndices
		};
		*it = BufferSp(new Unique<VkBuffer>(createBuffer(vkd, device, &bufferCI)));
	}
}

void createImages			(const DeviceInterface&					vkd,
							 const VkDevice							device,
							 deUint32								size,
							 std::vector<ImageSp>::iterator			begin,
							 std::vector<ImageSp>::iterator			end)
{
	deUint32 queueFamilyIndex = 0u;
	for (std::vector<ImageSp>::iterator it = begin; it != end; ++it)
	{
		const VkImageCreateInfo			imageCI =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,							// VkStructureType			sType
			DE_NULL,														// const void*				pNext
			(VkImageCreateFlags)0u,											// VkImageCreateFlags		flags
			VK_IMAGE_TYPE_2D,												// VkImageType				imageType
			VK_FORMAT_R8_UNORM,												// VkFormat					format
			{
				size,														// deUint32	width
				size,														// deUint32	height
				1u															// deUint32	depth
			},																// VkExtent3D				extent
			1u,																// deUint32					mipLevels
			1u,																// deUint32					arrayLayers
			VK_SAMPLE_COUNT_1_BIT,											// VkSampleCountFlagBits	samples
			VK_IMAGE_TILING_OPTIMAL,										// VkImageTiling			tiling
			VK_IMAGE_USAGE_SAMPLED_BIT,										// VkImageUsageFlags		usage
			VK_SHARING_MODE_EXCLUSIVE,										// VkSharingMode			sharingMode
			1u,																// deUint32					queueFamilyIndexCount
			&queueFamilyIndex,												// const deUint32*			pQueueFamilyIndices
			VK_IMAGE_LAYOUT_UNDEFINED										// VkImageLayout			initialLayout
		};
		*it = ImageSp(new Unique<VkImage>(createImage(vkd, device, &imageCI)));
	}
}

void createEvents			(const DeviceInterface&					vkd,
							 const VkDevice							device,
							 std::vector<EventSp>::iterator			begin,
							 std::vector<EventSp>::iterator			end)
{
	for(std::vector<EventSp>::iterator it=begin; it!=end; ++it)
		*it = EventSp(new Unique<VkEvent>(createEvent(vkd, device)));
}

void createQueryPools		(const DeviceInterface&					vkd,
							 const VkDevice							device,
							 std::vector<QueryPoolSp>::iterator		begin,
							 std::vector<QueryPoolSp>::iterator		end)
{
	for (std::vector<QueryPoolSp>::iterator it = begin; it != end; ++it)
	{
		const VkQueryPoolCreateInfo	queryPoolCI =
		{
			VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,						//  VkStructureType					sType
			DE_NULL,														//  const void*						pNext
			(VkQueryPoolCreateFlags)0,										//  VkQueryPoolCreateFlags			flags
			VK_QUERY_TYPE_OCCLUSION,										//  VkQueryType						queryType
			1u,																//  deUint32						queryCount
			0u,																//  VkQueryPipelineStatisticFlags	pipelineStatistics
		};
		*it = QueryPoolSp(new Unique<VkQueryPool>(createQueryPool(vkd, device, &queryPoolCI)));
	}
}

void createBufferViews		(const DeviceInterface&					vkd,
							 const VkDevice							device,
							 const VkBuffer							buffer,
							 const VkDeviceSize						size,
							 std::vector<BufferViewSp>::iterator	begin,
							 std::vector<BufferViewSp>::iterator	end)
{
	for (std::vector<BufferViewSp>::iterator it = begin; it != end; ++it)
	{
		const VkBufferViewCreateInfo bufferViewCI =
		{
			VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,						// VkStructureType			sType
			DE_NULL,														// const void*				pNext
			0u,																// VkBufferViewCreateFlags	flags
			buffer,															// VkBuffer					buffer
			VK_FORMAT_R8_UNORM,												// VkFormat					format
			0ull,															// VkDeviceSize				offset
			size															// VkDeviceSize				range
		};
		*it = BufferViewSp(new Unique<VkBufferView>(createBufferView(vkd, device, &bufferViewCI)));
	}
}

void createImageViews		(const DeviceInterface&					vkd,
							 const VkDevice							device,
							 const VkImage							image,
							 const VkFormat							format,
							 std::vector<ImageViewSp>::iterator		begin,
							 std::vector<ImageViewSp>::iterator		end)
{
	for (std::vector<ImageViewSp>::iterator it = begin; it != end; ++it)
	{
		VkComponentMapping componentMapping { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		VkImageViewCreateInfo imageViewCI =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,						// VkStructureType          sType
			DE_NULL,														// const void*              pNext
			0u,																// VkImageViewCreateFlags   flags
			image,															// VkImage                  image
			VK_IMAGE_VIEW_TYPE_2D,											// VkImageViewType          viewType
			format,															// VkFormat                 format
			componentMapping,												// VkComponentMapping       components
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },					// VkImageSubresourceRange  subresourceRange
		};
		*it = ImageViewSp(new Unique<VkImageView>(createImageView(vkd, device, &imageViewCI)));
	}
}

void createPipelineLayouts	(const DeviceInterface&						vkd,
							 const VkDevice								device,
							 std::vector<PipelineLayoutSp>::iterator	begin,
							 std::vector<PipelineLayoutSp>::iterator	end)
{
	for (std::vector<PipelineLayoutSp>::iterator it = begin; it != end; ++it)
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutCI =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,					// VkStructureType					sType
			DE_NULL,														// const void*						pNext
			0u,																// VkPipelineLayoutCreateFlags		flags
			0u,																// deUint32							setLayoutCount
			DE_NULL,														// const VkDescriptorSetLayout*		pSetLayouts
			0u,																// deUint32							pushConstantRangeCount
			DE_NULL															// const VkPushConstantRange*		pPushConstantRanges
		};
		*it = PipelineLayoutSp(new Unique<VkPipelineLayout>(createPipelineLayout(vkd, device, &pipelineLayoutCI)));
	}
}

void createRenderPasses		(const DeviceInterface&					vkd,
							 const VkDevice							device,
							 VkAttachmentDescription*				colorAttachment,
							 std::vector<RenderPassSp>::iterator	begin,
							 std::vector<RenderPassSp>::iterator	end)
{
	for (std::vector<RenderPassSp>::iterator it = begin; it != end; ++it)
	{
		const VkAttachmentReference		colorAttachmentRef =
		{
			0u,																// deUint32			attachment
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL						// VkImageLayout	layout
		};

		const VkSubpassDescription		subpassDescription =
		{
			0u,																// VkSubpassDescriptionFlags	flags
			VK_PIPELINE_BIND_POINT_GRAPHICS,								// VkPipelineBindPoint			pipelineBindPoint
			0u,																// deUint32						inputAttachmentCount
			DE_NULL,														// const VkAttachmentReference*	pInputAttachments
			1u,																// deUint32						colorAttachmentCount
			&colorAttachmentRef,											// const VkAttachmentReference*	pColorAttachments
			DE_NULL,														// const VkAttachmentReference*	pResolveAttachments
			DE_NULL,														// const VkAttachmentReference*	pDepthStencilAttachment
			0u,																// deUint32						preserveAttachmentCount
			DE_NULL															// const deUint32*				pPreserveAttachments
		};

		const VkRenderPassCreateInfo	renderPassCI =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType					sType
			DE_NULL,									// const void*						pNext
			0u,											// VkRenderPassCreateFlags			flags
			1u,											// deUint32							attachmentCount
			colorAttachment,							// const VkAttachmentDescription*	pAttachments
			1u,											// deUint32							subpassCount
			&subpassDescription,						// const VkSubpassDescription*		pSubpasses
			0u,											// deUint32							dependencyCount
			DE_NULL										// const VkSubpassDependency*		pDependencies
		};
		*it = RenderPassSp(new Unique<VkRenderPass>(createRenderPass(vkd, device, &renderPassCI)));
	}
}

void createGraphicsPipelines (const DeviceInterface&				vkd,
							  const VkDevice						device,
							  VkShaderModule						vertexShaderModule,
							  VkShaderModule						fragmentShaderModule,
							  VkRenderPass							renderPass,
							  VkPipelineLayout						pipelineLayout,
							  VkDeviceSize							poolEntrySize,
							  de::SharedPtr<vk::ResourceInterface>	resourceInterface,
							  std::vector<PipelineSp>::iterator		begin,
							  std::vector<PipelineSp>::iterator		end)
{
	std::vector<VkPipelineShaderStageCreateInfo> shaderStageCreateInfos;
	shaderStageCreateInfos.push_back
	(
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,			// VkStructureType                     sType;
			DE_NULL,														// const void*                         pNext;
			(VkPipelineShaderStageCreateFlags)0,							// VkPipelineShaderStageCreateFlags    flags;
			VK_SHADER_STAGE_VERTEX_BIT,										// VkShaderStageFlagBits               stage;
			vertexShaderModule,												// VkShaderModule                      shader;
			"main",															// const char*                         pName;
			DE_NULL,														// const VkSpecializationInfo*         pSpecializationInfo;
		}
	);

	shaderStageCreateInfos.push_back
	(
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,			// VkStructureType                     sType;
			DE_NULL,														// const void*                         pNext;
			(VkPipelineShaderStageCreateFlags)0,							// VkPipelineShaderStageCreateFlags    flags;
			VK_SHADER_STAGE_FRAGMENT_BIT,									// VkShaderStageFlagBits               stage;
			fragmentShaderModule,											// VkShaderModule                      shader;
			"main",															// const char*                         pName;
			DE_NULL,														// const VkSpecializationInfo*         pSpecializationInfo;
		}
	);

	for (std::vector<PipelineSp>::iterator it = begin; it != end; ++it)
	{
		const VkPipelineVertexInputStateCreateInfo		vertexInputStateCreateInfo		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType                             sType;
			DE_NULL,														// const void*                                 pNext;
			(VkPipelineVertexInputStateCreateFlags)0,						// VkPipelineVertexInputStateCreateFlags       flags;
			0u,																// deUint32                                    vertexBindingDescriptionCount;
			DE_NULL,														// const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
			0u,																// deUint32                                    vertexAttributeDescriptionCount;
			DE_NULL															// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
		};

		const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyStateCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType                            sType;
			DE_NULL,														// const void*                                pNext;
			(VkPipelineInputAssemblyStateCreateFlags)0,						// VkPipelineInputAssemblyStateCreateFlags    flags;
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,							// VkPrimitiveTopology                        topology;
			VK_FALSE														// VkBool32                                   primitiveRestartEnable;
		};

		const VkPipelineViewportStateCreateInfo			viewPortStateCreateInfo			=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,			// VkStructureType                       sType;
			DE_NULL,														// const void*                           pNext;
			(VkPipelineViewportStateCreateFlags)0,							// VkPipelineViewportStateCreateFlags    flags;
			1,																// deUint32                              viewportCount;
			DE_NULL,														// const VkViewport*                     pViewports;
			1,																// deUint32                              scissorCount;
			DE_NULL															// const VkRect2D*                       pScissors;
		};

		const VkPipelineRasterizationStateCreateInfo	rasterizationStateCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType                            sType;
			DE_NULL,														// const void*                                pNext;
			(VkPipelineRasterizationStateCreateFlags)0,						// VkPipelineRasterizationStateCreateFlags    flags;
			VK_FALSE,														// VkBool32                                   depthClampEnable;
			VK_FALSE,														// VkBool32                                   rasterizerDiscardEnable;
			VK_POLYGON_MODE_FILL,											// VkPolygonMode                              polygonMode;
			VK_CULL_MODE_BACK_BIT,											// VkCullModeFlags                            cullMode;
			VK_FRONT_FACE_CLOCKWISE,										// VkFrontFace                                frontFace;
			VK_FALSE,														// VkBool32                                   depthBiasEnable;
			0.0f,															// float                                      depthBiasConstantFactor;
			0.0f,															// float                                      depthBiasClamp;
			0.0f,															// float                                      depthBiasSlopeFactor;
			1.0f															// float                                      lineWidth;
		};

		const VkPipelineMultisampleStateCreateInfo		multisampleStateCreateInfo		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,		// VkStructureType                          sType;
			DE_NULL,														// const void*                              pNext;
			(VkPipelineMultisampleStateCreateFlags)0,						// VkPipelineMultisampleStateCreateFlags    flags;
			VK_SAMPLE_COUNT_1_BIT,											// VkSampleCountFlagBits                    rasterizationSamples;
			VK_FALSE,														// VkBool32                                 sampleShadingEnable;
			0.0f,															// float                                    minSampleShading;
			DE_NULL,														// const VkSampleMask*                      pSampleMask;
			VK_FALSE,														// VkBool32                                 alphaToCoverageEnable;
			VK_FALSE														// VkBool32                                 alphaToOneEnable;
		};

		const VkPipelineColorBlendAttachmentState		colorBlendAttachmentState		=
		{
			VK_FALSE,														// VkBool32                 blendEnable;
			VK_BLEND_FACTOR_ZERO,											// VkBlendFactor            srcColorBlendFactor;
			VK_BLEND_FACTOR_ZERO,											// VkBlendFactor            dstColorBlendFactor;
			VK_BLEND_OP_ADD,												// VkBlendOp                colorBlendOp;
			VK_BLEND_FACTOR_ZERO,											// VkBlendFactor            srcAlphaBlendFactor;
			VK_BLEND_FACTOR_ZERO,											// VkBlendFactor            dstAlphaBlendFactor;
			VK_BLEND_OP_ADD,												// VkBlendOp                alphaBlendOp;
			(VkColorComponentFlags)0xFu										// VkColorComponentFlags    colorWriteMask;
		};

		const VkPipelineColorBlendStateCreateInfo		colorBlendStateCreateInfo		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,		// VkStructureType                               sType;
			DE_NULL,														// const void*                                   pNext;
			(VkPipelineColorBlendStateCreateFlags)0,						// VkPipelineColorBlendStateCreateFlags          flags;
			DE_FALSE,														// VkBool32                                      logicOpEnable;
			VK_LOGIC_OP_CLEAR,												// VkLogicOp                                     logicOp;
			1,																// deUint32                                      attachmentCount;
			&colorBlendAttachmentState,										// const VkPipelineColorBlendAttachmentState*    pAttachments;
			{ 1.0f, 1.0f, 1.0f, 1.0f }										// float                                         blendConstants[4];
		};

		const VkDynamicState							dynamicStates[]					=
		{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		const VkPipelineDynamicStateCreateInfo			dynamicStateCreateInfo			=
		{
			VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,			// VkStructureType                      sType;
			DE_NULL,														// const void*                          pNext;
			(VkPipelineDynamicStateCreateFlags)0u,							// VkPipelineDynamicStateCreateFlags    flags;
			DE_LENGTH_OF_ARRAY(dynamicStates),								// deUint32                             dynamicStateCount;
			dynamicStates													// const VkDynamicState*                pDynamicStates;
		};

		VkGraphicsPipelineCreateInfo					graphicsPipelineCI				=
		{
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,				// VkStructureType                                  sType;
			DE_NULL,														// const void*                                      pNext;
			(VkPipelineCreateFlags)0,										// VkPipelineCreateFlags                            flags;
			deUint32(shaderStageCreateInfos.size()),						// deUint32                                         stageCount;
			shaderStageCreateInfos.data(),									// const VkPipelineShaderStageCreateInfo*           pStages;
			&vertexInputStateCreateInfo,									// const VkPipelineVertexInputStateCreateInfo*      pVertexInputState;
			&inputAssemblyStateCreateInfo,									// const VkPipelineInputAssemblyStateCreateInfo*    pInputAssemblyState;
			DE_NULL,														// const VkPipelineTessellationStateCreateInfo*     pTessellationState;
			&viewPortStateCreateInfo,										// const VkPipelineViewportStateCreateInfo*         pViewportState;
			&rasterizationStateCreateInfo,									// const VkPipelineRasterizationStateCreateInfo*    pRasterizationState;
			&multisampleStateCreateInfo,									// const VkPipelineMultisampleStateCreateInfo*      pMultisampleState;
			DE_NULL,														// const VkPipelineDepthStencilStateCreateInfo*     pDepthStencilState;
			&colorBlendStateCreateInfo,										// const VkPipelineColorBlendStateCreateInfo*       pColorBlendState;
			&dynamicStateCreateInfo,										// const VkPipelineDynamicStateCreateInfo*          pDynamicState;
			pipelineLayout,													// VkPipelineLayout                                 layout;
			renderPass,														// VkRenderPass                                     renderPass;
			0u,																// deUint32                                         subpass;
			DE_NULL,														// VkPipeline                                       basePipelineHandle;
			0																// int                                              basePipelineIndex;
		};

		// we have to ensure that proper poolEntrySize is used
		VkPipelineOfflineCreateInfo						pipelineOfflineCreateInfo;
		if (poolEntrySize != 0u)
		{
			pipelineOfflineCreateInfo				= resetPipelineOfflineCreateInfo();
			std::size_t					hashValue	= calculateGraphicsPipelineHash(graphicsPipelineCI, resourceInterface->getObjectHashes());
			memcpy(pipelineOfflineCreateInfo.pipelineIdentifier, &hashValue, sizeof(std::size_t));
			pipelineOfflineCreateInfo.poolEntrySize = poolEntrySize;
			graphicsPipelineCI.pNext				= &pipelineOfflineCreateInfo;
		}

		*it = PipelineSp(new Unique<VkPipeline>(createGraphicsPipeline(vkd, device, (VkPipelineCache)0u, &graphicsPipelineCI)));
	}
}

void createComputePipelines (const DeviceInterface&					vkd,
							 const VkDevice							device,
							 VkShaderModule							shaderModule,
							 VkPipelineLayout						pipelineLayout,
							 VkDeviceSize							poolEntrySize,
							 de::SharedPtr<vk::ResourceInterface>	resourceInterface,
							 std::vector<PipelineSp>::iterator		begin,
							 std::vector<PipelineSp>::iterator		end)
{
	for (std::vector<PipelineSp>::iterator it = begin; it != end; ++it)
	{
		VkPipelineShaderStageCreateInfo				shaderStageCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,			// VkStructureType                     sType;
			DE_NULL,														// const void*                         pNext;
			(VkPipelineShaderStageCreateFlags)0,							// VkPipelineShaderStageCreateFlags    flags;
			VK_SHADER_STAGE_COMPUTE_BIT,									// VkShaderStageFlagBits               stage;
			shaderModule,													// VkShaderModule                      shader;
			"main",															// const char*                         pName;
			DE_NULL,														// const VkSpecializationInfo*         pSpecializationInfo;
		};

		VkComputePipelineCreateInfo						computePipelineCI	=
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,					// VkStructureType					sType
			DE_NULL,														// const void*						pNext
			0u,																// VkPipelineCreateFlags			flags
			shaderStageCreateInfo,											// VkPipelineShaderStageCreateInfo	stage
			pipelineLayout,													// VkPipelineLayout					layout
			(vk::VkPipeline)0,												// VkPipeline						basePipelineHandle
			0u,																// deInt32							basePipelineIndex
		};

		// we have to ensure that proper poolEntrySize is used
		VkPipelineOfflineCreateInfo						pipelineOfflineCreateInfo;
		if (poolEntrySize != 0u)
		{
			pipelineOfflineCreateInfo				= resetPipelineOfflineCreateInfo();
			std::size_t					hashValue	= calculateComputePipelineHash(computePipelineCI, resourceInterface->getObjectHashes());
			memcpy(pipelineOfflineCreateInfo.pipelineIdentifier, &hashValue, sizeof(std::size_t));
			pipelineOfflineCreateInfo.poolEntrySize = poolEntrySize;
			computePipelineCI.pNext					= &pipelineOfflineCreateInfo;
		}

		*it = PipelineSp(new Unique<VkPipeline>(createComputePipeline(vkd, device, (VkPipelineCache)0u, &computePipelineCI)));
	}
}

void createDescriptorSetLayouts	(const DeviceInterface&							vkd,
								 const VkDevice									device,
								 std::vector<DescriptorSetLayoutSp>::iterator	begin,
								 std::vector<DescriptorSetLayoutSp>::iterator	end)
{
	for (std::vector<DescriptorSetLayoutSp>::iterator it = begin; it != end; ++it)
	{
		const VkDescriptorSetLayoutBinding descriptorSetLayoutBinding =
		{
			0,																// binding
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,								// descriptorType
			1u,																// descriptorCount
			VK_SHADER_STAGE_ALL,											// stageFlags
			NULL															// pImmutableSamplers
		};

		const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,			// sType
			NULL,															// pNext
			(VkDescriptorSetLayoutCreateFlags)0u,							// flags
			1u,																// bindingCount
			&descriptorSetLayoutBinding										// pBindings
		};
		*it = DescriptorSetLayoutSp(new Unique<VkDescriptorSetLayout>(createDescriptorSetLayout(vkd, device, &descriptorSetLayoutCI)));
	}
}

void createSamplers			(const DeviceInterface&					vkd,
							 const VkDevice							device,
							 std::vector<SamplerSp>::iterator		begin,
							 std::vector<SamplerSp>::iterator		end)
{
	for (std::vector<SamplerSp>::iterator it = begin; it != end; ++it)
	{
		const VkSamplerCreateInfo samplerCI =
		{
			VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,							//VkStructureType		sType
			DE_NULL,														//const void*			pNext
			0u,																//VkSamplerCreateFlags	flags
			VK_FILTER_NEAREST,												//VkFilter				magFilter
			VK_FILTER_NEAREST,												//VkFilter				minFilter
			VK_SAMPLER_MIPMAP_MODE_NEAREST,									//VkSamplerMipmapMode	mipmapMode
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,							//VkSamplerAddressMode	addressModeU
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,							//VkSamplerAddressMode	addressModeV
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,							//VkSamplerAddressMode	addressModeW
			0.0f,															//float					mipLodBias
			VK_FALSE,														//VkBool32				anisotropyEnable
			1.0f,															//float					maxAnisotropy
			VK_FALSE,														//VkBool32				compareEnable
			VK_COMPARE_OP_EQUAL,											//VkCompareOp			compareOp
			0.0f,															//float					minLod
			0.0f,															//float					maxLod
			VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,						//VkBorderColor			borderColor
			VK_TRUE,														//VkBool32				unnormalizedCoordinates
		};
		*it = SamplerSp(new Unique<VkSampler>(createSampler(vkd, device, &samplerCI)));
	}
}

void createDescriptorPools	(const DeviceInterface&						vkd,
							 const VkDevice								device,
							 deUint32									maxSets,
							 std::vector<DescriptorPoolSp>::iterator	begin,
							 std::vector<DescriptorPoolSp>::iterator	end)
{
	for (std::vector<DescriptorPoolSp>::iterator it = begin; it != end; ++it)
	{
		const VkDescriptorPoolSize			poolSizes =
		{
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			1u
		};
		const VkDescriptorPoolCreateInfo	descriptorPoolCI =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,					// sType
			DE_NULL,														// pNext
			VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,				// flags
			maxSets,														// maxSets
			1u,																// poolSizeCount
			&poolSizes,														// pPoolSizes
		};
		*it = DescriptorPoolSp(new Unique<VkDescriptorPool>(createDescriptorPool(vkd, device, &descriptorPoolCI)));
	}
}

void createDescriptorSets	(const DeviceInterface&						vkd,
							 const VkDevice								device,
							 const VkDescriptorPool						descriptorPool,
							 const VkDescriptorSetLayout				setLayout,
							 std::vector<DescriptorSetSp>::iterator		begin,
							 std::vector<DescriptorSetSp>::iterator		end)
{
	for (std::vector<DescriptorSetSp>::iterator it = begin; it != end; ++it)
	{
		const VkDescriptorSetAllocateInfo descriptorSetAI =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,					// VkStructureType                 sType
			DE_NULL,														// const void*                     pNext
			descriptorPool,													// VkDescriptorPool                descriptorPool
			1u,																// deUint32                        descriptorSetCount
			&setLayout														// const VkDescriptorSetLayout*    pSetLayouts
		};
		*it = DescriptorSetSp(new Unique<VkDescriptorSet>(allocateDescriptorSet(vkd, device, &descriptorSetAI)));
	}
}

void createFramebuffers		(const DeviceInterface&						vkd,
							 const VkDevice								device,
							 const VkRenderPass							renderPass,
							 const VkImageView							imageView,
							 std::vector<FramebufferSp>::iterator		begin,
							 std::vector<FramebufferSp>::iterator		end)
{
	for (std::vector<FramebufferSp>::iterator it = begin; it != end; ++it)
	{
		const VkFramebufferCreateInfo framebufferCi =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,						// VkStructureType			sType
			DE_NULL,														// const void*				pNext
			0u,																// VkFramebufferCreateFlags	flags
			renderPass,														// VkRenderPass				renderPass
			1u,																// uint32_t					attachmentCount
			&imageView,														// const VkImageView*		pAttachments
			8u,																// uint32_t					width
			8u,																// uint32_t					height
			1u																// uint32_t					layers
		};

		*it = FramebufferSp(new Unique<VkFramebuffer>(createFramebuffer(vkd, device, &framebufferCi)));
	}
}

void createCommandPools		(const DeviceInterface&					vkd,
							 const VkDevice							device,
							 std::vector<CommandPoolSp>::iterator	begin,
							 std::vector<CommandPoolSp>::iterator	end)
{
	for (std::vector<CommandPoolSp>::iterator it = begin; it != end; ++it)
	{
		const VkCommandPoolCreateInfo			commandPoolCI =
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,						// sType
			DE_NULL,														// pNext
			0u,																// flags
			0u,																// queueFamilyIndex
		};
		*it = CommandPoolSp(new Unique<VkCommandPool>(createCommandPool(vkd, device, &commandPoolCI)));
	}
}

void createSamplerYcbcrConversions (const DeviceInterface&							vkd,
									const VkDevice									device,
									std::vector<SamplerYcbcrConversionSp>::iterator	begin,
									std::vector<SamplerYcbcrConversionSp>::iterator	end)
{
	for (std::vector<SamplerYcbcrConversionSp>::iterator it = begin; it != end; ++it)
	{
		const VkSamplerYcbcrConversionCreateInfo	ycbcrConversionCI =
		{
			VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,			// sType
			DE_NULL,														// pNext
			VK_FORMAT_G8B8G8R8_422_UNORM,									// format
			VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY,					// ycbcrModel
			VK_SAMPLER_YCBCR_RANGE_ITU_FULL,								// ycbcrRange
			{
				VK_COMPONENT_SWIZZLE_IDENTITY,								// r
				VK_COMPONENT_SWIZZLE_IDENTITY,								// g
				VK_COMPONENT_SWIZZLE_IDENTITY,								// b
				VK_COMPONENT_SWIZZLE_IDENTITY,								// a
			},																// components
			VK_CHROMA_LOCATION_MIDPOINT,									// xChromaOffset
			VK_CHROMA_LOCATION_MIDPOINT,									// yChromaOffset
			VK_FILTER_NEAREST,												// chromaFilter
			VK_FALSE,														// forceExplicitReconstruction
			};
		*it = SamplerYcbcrConversionSp(new Unique<VkSamplerYcbcrConversion>(createSamplerYcbcrConversion(vkd, device, &ycbcrConversionCI)));
	}
}

// Base class for all VkDeviceObjectReservationCreateInfo tests.
// Creates a device with 0 for all "max" values / and "RequestCounts"
class DeviceObjectReservationInstance : public vkt::TestInstance
{
public:
										DeviceObjectReservationInstance		(Context&								context,
																			 const TestParams&						testParams_);
	tcu::TestStatus						iterate								(void) override;

	virtual Move<VkDevice>				createTestDevice					(VkDeviceCreateInfo&					deviceCreateInfo,
																			 VkDeviceObjectReservationCreateInfo&	objectInfo,
																			 VkPhysicalDeviceVulkanSC10Features&	sc10Features);
	virtual void						performTest							(const DeviceInterface&					vkd,
																			 VkDevice								device);
	virtual bool						verifyTestResults					(const DeviceInterface&					vkd,
																			 VkDevice								device);

protected:
	TestParams							testParams;
	vkt::CustomInstance					instance;
	VkPhysicalDevice					physicalDevice;
};

DeviceObjectReservationInstance::DeviceObjectReservationInstance (Context&			context,
																  const TestParams&	testParams_)
	: vkt::TestInstance				( context )
	, testParams					( testParams_ )
	, instance						( vkt::createCustomInstanceFromContext(context) )
	, physicalDevice				( chooseDevice(instance.getDriver(), instance, context.getTestContext().getCommandLine()) )
{
}

tcu::TestStatus DeviceObjectReservationInstance::iterate (void)
{
	const float						queuePriority		= 1.0f;

	const VkDeviceQueueCreateInfo	deviceQueueCI		=
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,							// sType
		DE_NULL,															// pNext
		(VkDeviceQueueCreateFlags)0u,										// flags
		0,																	//queueFamilyIndex;
		1,																	//queueCount;
		&queuePriority,														//pQueuePriorities;
	};

	VkDeviceCreateInfo				deviceCreateInfo	=
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,								// sType;
		DE_NULL,															// pNext;
		(VkDeviceCreateFlags)0u,											// flags
		1,																	// queueRecordCount;
		&deviceQueueCI,														// pRequestedQueues;
		0,																	// layerCount;
		DE_NULL,															// ppEnabledLayerNames;
		0,																	// extensionCount;
		DE_NULL,															// ppEnabledExtensionNames;
		DE_NULL,															// pEnabledFeatures;
	};

	void* pNext = DE_NULL;

	VkDeviceObjectReservationCreateInfo	objectInfo		= resetDeviceObjectReservationCreateInfo();
	objectInfo.pipelineCacheRequestCount				= 1u;
	objectInfo.pNext									= pNext;
	pNext												= &objectInfo;

	VkPhysicalDeviceVulkanSC10Features	sc10Features	= createDefaultSC10Features();
	sc10Features.pNext									= pNext;
	pNext												= &sc10Features;

	deviceCreateInfo.pNext								= pNext;

	Move<VkDevice>					device				= createTestDevice(deviceCreateInfo, objectInfo, sc10Features);
	de::MovePtr<DeviceDriverSC, DeinitDeviceDeleter>
									deviceDriver		= de::MovePtr<DeviceDriverSC, DeinitDeviceDeleter>(new DeviceDriverSC(m_context.getPlatformInterface(), instance, *device, m_context.getTestContext().getCommandLine(), m_context.getResourceInterface(), m_context.getDeviceVulkanSC10Properties(), m_context.getDeviceProperties(), m_context.getUsedApiVersion()),
															DeinitDeviceDeleter(m_context.getResourceInterface().get(), *device));

	performTest(*deviceDriver, *device);

	const VkQueue					queue				= getDeviceQueue(*deviceDriver, *device,  0, 0);
	VK_CHECK(deviceDriver->queueWaitIdle(queue));

	if (!verifyTestResults(*deviceDriver, *device))
		return tcu::TestStatus::fail("Failed");
	return tcu::TestStatus::pass("Pass");
}

Move<VkDevice> DeviceObjectReservationInstance::createTestDevice (VkDeviceCreateInfo&					deviceCreateInfo,
																  VkDeviceObjectReservationCreateInfo&	objectInfo,
																  VkPhysicalDeviceVulkanSC10Features&	sc10Features)
{
	DE_UNREF(sc10Features);

	// perform any non pipeline operations - create 2 semaphores
	objectInfo.semaphoreRequestCount = 2u;

	return createCustomDevice(m_context.getTestContext().getCommandLine().isValidationEnabled(), m_context.getPlatformInterface(), instance, instance.getDriver(), physicalDevice, &deviceCreateInfo);
}

void DeviceObjectReservationInstance::performTest (const DeviceInterface&				vkd,
												   VkDevice								device)
{
	std::vector<SemaphoreSp> semaphores(2u);
	createSemaphores(vkd, device, begin(semaphores), end(semaphores));
}

bool DeviceObjectReservationInstance::verifyTestResults (const DeviceInterface&			vkd,
														 VkDevice						device)
{
	DE_UNREF(vkd);
	DE_UNREF(device);
	return true;
}

// Creates device with multiple VkDeviceObjectReservationCreateInfo and ensures that the limits of an individual VkDeviceObjectReservationCreateInfo can be exceeded.
class MultipleReservation : public DeviceObjectReservationInstance
{
public:
	MultipleReservation (Context&			context,
						 const TestParams&	testParams_)
		: DeviceObjectReservationInstance(context, testParams_)
	{
	}
	Move<VkDevice> createTestDevice (VkDeviceCreateInfo&					deviceCreateInfo,
									 VkDeviceObjectReservationCreateInfo&	objectInfo,
									 VkPhysicalDeviceVulkanSC10Features&	sc10Features) override
	{
		DE_UNREF(sc10Features);

		VkDeviceObjectReservationCreateInfo  thirdObjectInfo	= resetDeviceObjectReservationCreateInfo();
		thirdObjectInfo.deviceMemoryRequestCount				= 2;

		VkDeviceObjectReservationCreateInfo  secondObjectInfo	= resetDeviceObjectReservationCreateInfo();
		secondObjectInfo.deviceMemoryRequestCount				= 2;
		secondObjectInfo.pNext									= &thirdObjectInfo;

		objectInfo.deviceMemoryRequestCount						= 2;
		objectInfo.pNext										= &secondObjectInfo;

		return createCustomDevice(m_context.getTestContext().getCommandLine().isValidationEnabled(), m_context.getPlatformInterface(), instance, instance.getDriver(), physicalDevice, &deviceCreateInfo);
	}

	void performTest (const DeviceInterface&				vkd,
					  VkDevice								device) override
	{
		std::vector<VkDeviceMemory>	memoryObjects(6, (VkDeviceMemory)0);
		for (size_t ndx = 0; ndx < 6; ndx++)
		{
			VkMemoryAllocateInfo alloc =
			{
				VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,						// sType
				DE_NULL,													// pNext
				128U,														// allocationSize
				0U															// memoryTypeIndex;
			};

			VK_CHECK(vkd.allocateMemory(device, &alloc, (const VkAllocationCallbacks*)DE_NULL, &memoryObjects[ndx]));

			TCU_CHECK(!!memoryObjects[ndx]);
		}
	}
};

void checkSupportVerifyMaxValues (vkt::Context& context, TestParams testParams)
{
	if (testParams.testMaxValues == TMV_MAX_PIPELINESTATISTICS_QUERIES_PER_POOL && context.getDeviceFeatures().pipelineStatisticsQuery == VK_FALSE)
		TCU_THROW(NotSupportedError, "pipelineStatisticsQuery is not supported");
}

// For each of the various resource "max" values, create resources that exercise the maximum values requested

class VerifyMaxValues : public DeviceObjectReservationInstance
{
public:
	VerifyMaxValues					(Context&								context,
									 const TestParams&						testParams_)
		: DeviceObjectReservationInstance(context, testParams_)
	{
	}
	Move<VkDevice> createTestDevice (VkDeviceCreateInfo&					deviceCreateInfo,
									 VkDeviceObjectReservationCreateInfo&	objectInfo,
									 VkPhysicalDeviceVulkanSC10Features&	sc10Features) override
	{
		DE_UNREF(sc10Features);
		switch (testParams.testMaxValues)
		{
			case TMV_DESCRIPTOR_SET_LAYOUT_BINDING_LIMIT:
				objectInfo.descriptorSetLayoutBindingLimit			= VERIFYMAXVALUES_OBJECT_COUNT+1u;
				objectInfo.descriptorSetLayoutBindingRequestCount	= VERIFYMAXVALUES_OBJECT_COUNT;
				objectInfo.descriptorSetLayoutRequestCount			= 1u;
				break;
			case TMV_MAX_IMAGEVIEW_MIPLEVELS:
				objectInfo.maxImageViewMipLevels					= VERIFYMAXVALUES_MIPLEVELS;
				objectInfo.maxImageViewArrayLayers					= 1u;
				objectInfo.imageRequestCount						= 1u;
				objectInfo.deviceMemoryRequestCount					= 1u;
				break;
			case TMV_MAX_IMAGEVIEW_ARRAYLAYERS:
				objectInfo.maxImageViewMipLevels					= 1u;
				objectInfo.maxImageViewArrayLayers					= VERIFYMAXVALUES_ARRAYLAYERS;
				objectInfo.imageRequestCount						= 1u;
				objectInfo.deviceMemoryRequestCount					= 1u;
				break;
			case TMV_MAX_LAYEREDIMAGEVIEW_MIPLEVELS:
				objectInfo.maxLayeredImageViewMipLevels				= VERIFYMAXVALUES_MIPLEVELS;
				objectInfo.maxImageViewArrayLayers					= VERIFYMAXVALUES_ARRAYLAYERS;
				objectInfo.imageRequestCount						= 1u;
				objectInfo.deviceMemoryRequestCount					= 1u;
				break;
			case TMV_MAX_OCCLUSION_QUERIES_PER_POOL:
				objectInfo.maxOcclusionQueriesPerPool				= VERIFYMAXVALUES_OBJECT_COUNT;
				objectInfo.queryPoolRequestCount					= 1u;
				break;
			case TMV_MAX_PIPELINESTATISTICS_QUERIES_PER_POOL:
				objectInfo.maxPipelineStatisticsQueriesPerPool		= VERIFYMAXVALUES_OBJECT_COUNT;
				objectInfo.queryPoolRequestCount					= 1u;
				break;
			case TMV_MAX_TIMESTAMP_QUERIES_PER_POOL:
				objectInfo.maxTimestampQueriesPerPool				= VERIFYMAXVALUES_OBJECT_COUNT;
				objectInfo.queryPoolRequestCount					= 1u;
				break;
			default:
				TCU_THROW(InternalError, "Unsupported max value");
		}

		return createCustomDevice(m_context.getTestContext().getCommandLine().isValidationEnabled(), m_context.getPlatformInterface(), instance, instance.getDriver(), physicalDevice, &deviceCreateInfo);
	}

	void performTest (const DeviceInterface&				vkd,
					  VkDevice								device) override
	{
		SimpleAllocator					allocator(vkd, device, getPhysicalDeviceMemoryProperties(instance.getDriver(), physicalDevice));
		de::MovePtr<ImageWithMemory>	image;
		Move<VkQueryPool>				queryPool;
		Move<VkDescriptorSetLayout>		descriptorSetLayout;
		deUint32						queueFamilyIndex = 0u;

		switch (testParams.testMaxValues)
		{
			case TMV_DESCRIPTOR_SET_LAYOUT_BINDING_LIMIT:
			{
				const VkDescriptorSetLayoutBinding	binding =
				{
					VERIFYMAXVALUES_OBJECT_COUNT,							// binding
					VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,						// descriptorType
					1u,														// descriptorCount
					VK_SHADER_STAGE_ALL,									// stageFlags
					DE_NULL,												// pImmutableSamplers
				};

				const VkDescriptorSetLayoutCreateInfo	layoutCreateInfo =
				{
					VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
					DE_NULL,												// pNext
					0u,														// flags
					1u,														// bindingCount
					&binding,												// pBindings
				};
				descriptorSetLayout = createDescriptorSetLayout(vkd, device, &layoutCreateInfo);
				break;
			}
			case TMV_MAX_IMAGEVIEW_MIPLEVELS:
			{
				const VkImageCreateInfo			imageCreateInfo =
				{
					VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,					// VkStructureType			sType;
					DE_NULL,												// const void*				pNext;
					(VkImageCreateFlags)0u,									// VkImageCreateFlags		flags;
					VK_IMAGE_TYPE_2D,										// VkImageType				imageType;
					VK_FORMAT_R8_UNORM,										// VkFormat					format;
					{
						1 << VERIFYMAXVALUES_MIPLEVELS,						// deUint32	width;
						1 << VERIFYMAXVALUES_MIPLEVELS,						// deUint32	height;
						1u													// deUint32	depth;
					},														// VkExtent3D				extent;
					VERIFYMAXVALUES_MIPLEVELS,								// deUint32					mipLevels;
					1u,														// deUint32					arrayLayers;
					VK_SAMPLE_COUNT_1_BIT,									// VkSampleCountFlagBits	samples;
					VK_IMAGE_TILING_OPTIMAL,								// VkImageTiling			tiling;
					VK_IMAGE_USAGE_SAMPLED_BIT,								// VkImageUsageFlags		usage;
					VK_SHARING_MODE_EXCLUSIVE,								// VkSharingMode			sharingMode;
					1u,														// deUint32					queueFamilyIndexCount;
					&queueFamilyIndex,										// const deUint32*			pQueueFamilyIndices;
					VK_IMAGE_LAYOUT_UNDEFINED								// VkImageLayout			initialLayout;
				};
				image = de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
				break;
			}
			case TMV_MAX_IMAGEVIEW_ARRAYLAYERS:
			{
				const VkImageCreateInfo			imageCreateInfo =
				{
					VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,					// VkStructureType			sType;
					DE_NULL,												// const void*				pNext;
					(VkImageCreateFlags)0u,									// VkImageCreateFlags		flags;
					VK_IMAGE_TYPE_2D,										// VkImageType				imageType;
					VK_FORMAT_R8_UNORM,										// VkFormat					format;
					{
						16U,												// deUint32	width;
						16U,												// deUint32	height;
						1u													// deUint32	depth;
					},														// VkExtent3D				extent;
					1u,														// deUint32					mipLevels;
					VERIFYMAXVALUES_ARRAYLAYERS,							// deUint32					arrayLayers;
					VK_SAMPLE_COUNT_1_BIT,									// VkSampleCountFlagBits	samples;
					VK_IMAGE_TILING_OPTIMAL,								// VkImageTiling			tiling;
					VK_IMAGE_USAGE_SAMPLED_BIT,								// VkImageUsageFlags		usage;
					VK_SHARING_MODE_EXCLUSIVE,								// VkSharingMode			sharingMode;
					1u,														// deUint32					queueFamilyIndexCount;
					&queueFamilyIndex,										// const deUint32*			pQueueFamilyIndices;
					VK_IMAGE_LAYOUT_UNDEFINED								// VkImageLayout			initialLayout;
				};
				image = de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
				break;
			}
			case TMV_MAX_LAYEREDIMAGEVIEW_MIPLEVELS:
			{
				const VkImageCreateInfo			imageCreateInfo =
				{
					VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,					// VkStructureType			sType;
					DE_NULL,												// const void*				pNext;
					(VkImageCreateFlags)0u,									// VkImageCreateFlags		flags;
					VK_IMAGE_TYPE_2D,										// VkImageType				imageType;
					VK_FORMAT_R8_UNORM,										// VkFormat					format;
					{
						1 << VERIFYMAXVALUES_MIPLEVELS,						// deUint32	width;
						1 << VERIFYMAXVALUES_MIPLEVELS,						// deUint32	height;
						1u													// deUint32	depth;
					},														// VkExtent3D				extent;
					VERIFYMAXVALUES_MIPLEVELS,								// deUint32					mipLevels;
					VERIFYMAXVALUES_ARRAYLAYERS,							// deUint32					arrayLayers;
					VK_SAMPLE_COUNT_1_BIT,									// VkSampleCountFlagBits	samples;
					VK_IMAGE_TILING_OPTIMAL,								// VkImageTiling			tiling;
					VK_IMAGE_USAGE_SAMPLED_BIT,								// VkImageUsageFlags		usage;
					VK_SHARING_MODE_EXCLUSIVE,								// VkSharingMode			sharingMode;
					1u,														// deUint32					queueFamilyIndexCount;
					&queueFamilyIndex,										// const deUint32*			pQueueFamilyIndices;
					VK_IMAGE_LAYOUT_UNDEFINED								// VkImageLayout			initialLayout;
				};
				image = de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
				break;
			}
			case TMV_MAX_OCCLUSION_QUERIES_PER_POOL:
			{
				const VkQueryPoolCreateInfo	queryPoolCreateInfo =
				{
					VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,				//  VkStructureType					sType;
					DE_NULL,												//  const void*						pNext;
					(VkQueryPoolCreateFlags)0,								//  VkQueryPoolCreateFlags			flags;
					VK_QUERY_TYPE_OCCLUSION,								//  VkQueryType						queryType;
					VERIFYMAXVALUES_OBJECT_COUNT,							//  deUint32						queryCount;
					0u,														//  VkQueryPipelineStatisticFlags	pipelineStatistics;
				};
				queryPool = createQueryPool(vkd, device, &queryPoolCreateInfo);
				break;
			}
			case TMV_MAX_PIPELINESTATISTICS_QUERIES_PER_POOL:
			{
				const VkQueryPoolCreateInfo	queryPoolCreateInfo =
				{
					VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,				//  VkStructureType					sType;
					DE_NULL,												//  const void*						pNext;
					(VkQueryPoolCreateFlags)0,								//  VkQueryPoolCreateFlags			flags;
					VK_QUERY_TYPE_PIPELINE_STATISTICS,						//  VkQueryType						queryType;
					VERIFYMAXVALUES_OBJECT_COUNT,							//  deUint32						queryCount;
					0u,														//  VkQueryPipelineStatisticFlags	pipelineStatistics;
				};
				queryPool = createQueryPool(vkd, device, &queryPoolCreateInfo);
				break;
			}
			case TMV_MAX_TIMESTAMP_QUERIES_PER_POOL:
			{
				const VkQueryPoolCreateInfo	queryPoolCreateInfo =
				{
					VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,				//  VkStructureType					sType;
					DE_NULL,												//  const void*						pNext;
					(VkQueryPoolCreateFlags)0,								//  VkQueryPoolCreateFlags			flags;
					VK_QUERY_TYPE_TIMESTAMP,								//  VkQueryType						queryType;
					VERIFYMAXVALUES_OBJECT_COUNT,							//  deUint32						queryCount;
					0u,														//  VkQueryPipelineStatisticFlags	pipelineStatistics;
				};
				queryPool = createQueryPool(vkd, device, &queryPoolCreateInfo);
				break;
			}
			default:
				TCU_THROW(InternalError, "Unsupported max value");
		}
	}
};

void checkSupportVerifyRequestCounts (vkt::Context& context, TestParams testParams)
{
	if (testParams.testRequestCounts == TRC_SAMPLERYCBCRCONVERSION && context.getDeviceVulkan11Features().samplerYcbcrConversion == VK_FALSE)
		TCU_THROW(NotSupportedError, "samplerYcbcrConversion is not supported");
}

// create programs for VerifyRequestCounts tests
struct ProgramsVerifyLimits
{
	void init(SourceCollections& dst, TestParams testParams) const
	{
		if (testParams.testRequestCounts == TRC_GRAPHICS_PIPELINE || testParams.testPoolSizeType != PST_UNDEFINED)
		{
			dst.glslSources.add("vertex") << glu::VertexSource(
				"#version 450\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"   gl_Position = vec4(0);\n"
				"}\n");
			dst.glslSources.add("fragment") << glu::FragmentSource(
				"#version 450\n"
				"\n"
				"layout(location=0) out vec4 x;\n"
				"void main (void)\n"
				"{\n"
				"   x = vec4(1);\n"
				"}\n");
		}
		else if (testParams.testRequestCounts == TRC_COMPUTE_PIPELINE)
		{
			dst.glslSources.add("compute") << glu::ComputeSource(
				"#version 450\n"
				"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
				"void main (void)\n"
				"{\n"
				"	uvec4 x = uvec4(0);\n"
				"}\n");
		}
	}
};

// For each of the various resource "max" values, create resources that exercise the maximum values requested
class VerifyRequestCounts : public DeviceObjectReservationInstance
{
public:
	VerifyRequestCounts				(Context&								context,
									 const TestParams&						testParams_)
		: DeviceObjectReservationInstance(context, testParams_)
	{
	}


	Move<VkDevice> createTestDevice (VkDeviceCreateInfo&					deviceCreateInfo,
									 VkDeviceObjectReservationCreateInfo&	objectInfo,
									 VkPhysicalDeviceVulkanSC10Features&	sc10Features) override
	{
		DE_UNREF(sc10Features);

		std::vector<VkPipelinePoolSize>	poolSizes;
		VkDeviceSize					pipelineDefaultSize			= VkDeviceSize(m_context.getTestContext().getCommandLine().getPipelineDefaultSize());

		switch (testParams.testRequestCounts)
		{
			case TRC_SEMAPHORE:
				objectInfo.semaphoreRequestCount					= VERIFYMAXVALUES_OBJECT_COUNT;
				break;
			case TRC_COMMAND_BUFFER:
				objectInfo.commandPoolRequestCount					= 1u;
				objectInfo.commandBufferRequestCount				= 2 * VERIFYMAXVALUES_OBJECT_COUNT + (VERIFYMAXVALUES_OBJECT_COUNT - VERIFYMAXVALUES_OBJECT_COUNT / 2) ;
				break;
			case TRC_FENCE:
				objectInfo.fenceRequestCount						= VERIFYMAXVALUES_OBJECT_COUNT;
				break;
			case TRC_DEVICE_MEMORY:
				objectInfo.deviceMemoryRequestCount					= VERIFYMAXVALUES_OBJECT_COUNT;
				break;
			case TRC_BUFFER:
				objectInfo.bufferRequestCount						= VERIFYMAXVALUES_OBJECT_COUNT;
				break;
			case TRC_IMAGE:
				objectInfo.imageRequestCount						= VERIFYMAXVALUES_OBJECT_COUNT;
				objectInfo.maxImageViewMipLevels					= 1u;
				objectInfo.maxImageViewArrayLayers					= 1u;
				break;
			case TRC_EVENT:
				objectInfo.eventRequestCount						= VERIFYMAXVALUES_OBJECT_COUNT;
				break;
			case TRC_QUERY_POOL:
				objectInfo.queryPoolRequestCount					= VERIFYMAXVALUES_OBJECT_COUNT;
				break;
			case TRC_BUFFER_VIEW:
				objectInfo.deviceMemoryRequestCount					= 1u;
				objectInfo.bufferRequestCount						= 1u;
				objectInfo.bufferViewRequestCount					= VERIFYMAXVALUES_OBJECT_COUNT;
				break;
			case TRC_IMAGE_VIEW:
				objectInfo.deviceMemoryRequestCount					= 1u;
				objectInfo.imageViewRequestCount					= VERIFYMAXVALUES_OBJECT_COUNT;
				objectInfo.imageRequestCount						= 1u;
				objectInfo.maxImageViewMipLevels					= 1u;
				objectInfo.maxImageViewArrayLayers					= 1u;
				break;
			case TRC_LAYERED_IMAGE_VIEW:
				objectInfo.deviceMemoryRequestCount					= 1u;
				objectInfo.imageViewRequestCount					= VERIFYMAXVALUES_OBJECT_COUNT;
				objectInfo.layeredImageViewRequestCount				= VERIFYMAXVALUES_OBJECT_COUNT;
				objectInfo.imageRequestCount						= 1u;
				objectInfo.maxImageViewMipLevels					= 1u;
				objectInfo.maxImageViewArrayLayers					= VERIFYMAXVALUES_ARRAYLAYERS;
				break;
			case TRC_PIPELINE_LAYOUT:
				objectInfo.pipelineLayoutRequestCount				= VERIFYMAXVALUES_OBJECT_COUNT;
				break;
			case TRC_RENDER_PASS:
				objectInfo.renderPassRequestCount					= VERIFYMAXVALUES_OBJECT_COUNT;
				objectInfo.subpassDescriptionRequestCount			= VERIFYMAXVALUES_OBJECT_COUNT;
				objectInfo.attachmentDescriptionRequestCount		= VERIFYMAXVALUES_OBJECT_COUNT;
				break;
			case TRC_GRAPHICS_PIPELINE:
				objectInfo.pipelineLayoutRequestCount				= 1u;
				objectInfo.renderPassRequestCount					= 1u;
				objectInfo.subpassDescriptionRequestCount			= 1u;
				objectInfo.attachmentDescriptionRequestCount		= 1u;
				objectInfo.graphicsPipelineRequestCount				= VERIFYMAXVALUES_OBJECT_COUNT;
				poolSizes.push_back({ VK_STRUCTURE_TYPE_PIPELINE_POOL_SIZE, DE_NULL, pipelineDefaultSize, VERIFYMAXVALUES_OBJECT_COUNT });
				break;
			case TRC_COMPUTE_PIPELINE:
				objectInfo.pipelineLayoutRequestCount				= 1u;
				objectInfo.computePipelineRequestCount				= VERIFYMAXVALUES_OBJECT_COUNT;
				poolSizes.push_back({ VK_STRUCTURE_TYPE_PIPELINE_POOL_SIZE, DE_NULL, pipelineDefaultSize, VERIFYMAXVALUES_OBJECT_COUNT });
				break;
			case TRC_DESCRIPTORSET_LAYOUT:
				objectInfo.descriptorSetLayoutRequestCount			= VERIFYMAXVALUES_OBJECT_COUNT;
				objectInfo.descriptorSetLayoutBindingRequestCount	= VERIFYMAXVALUES_OBJECT_COUNT;
				objectInfo.descriptorSetLayoutBindingLimit			= 2u;
				break;
			case TRC_SAMPLER:
				objectInfo.samplerRequestCount						= VERIFYMAXVALUES_OBJECT_COUNT;
				break;
			case TRC_DESCRIPTOR_POOL:
				objectInfo.descriptorPoolRequestCount				= VERIFYMAXVALUES_OBJECT_COUNT;
				break;
			case TRC_DESCRIPTORSET:
				objectInfo.descriptorSetLayoutRequestCount			= 1u;
				objectInfo.descriptorSetLayoutBindingRequestCount	= 1u;
				objectInfo.descriptorSetLayoutBindingLimit			= 2u;
				objectInfo.descriptorPoolRequestCount				= 1u;
				objectInfo.descriptorSetRequestCount				= VERIFYMAXVALUES_OBJECT_COUNT;
				break;
			case TRC_FRAMEBUFFER:
				objectInfo.deviceMemoryRequestCount					= 1u;
				objectInfo.imageViewRequestCount					= 1u;
				objectInfo.imageRequestCount						= 1u;
				objectInfo.maxImageViewMipLevels					= 1u;
				objectInfo.maxImageViewArrayLayers					= 1u;
				objectInfo.renderPassRequestCount					= 1u;
				objectInfo.subpassDescriptionRequestCount			= 1u;
				objectInfo.attachmentDescriptionRequestCount		= 1u;
				objectInfo.framebufferRequestCount					= VERIFYMAXVALUES_OBJECT_COUNT;
				break;
			case TRC_COMMANDPOOL:
				objectInfo.commandPoolRequestCount					= VERIFYMAXVALUES_OBJECT_COUNT;
				objectInfo.commandBufferRequestCount				= VERIFYMAXVALUES_OBJECT_COUNT;
				break;
			case TRC_SAMPLERYCBCRCONVERSION:
				objectInfo.samplerYcbcrConversionRequestCount		= VERIFYMAXVALUES_OBJECT_COUNT;
				break;
//			case TRC_SURFACE:
//				objectInfo.surfaceRequestCount						= VERIFYMAXVALUES_OBJECT_COUNT;
//				break;
//			case TRC_SWAPCHAIN:
//				objectInfo.swapchainRequestCount					= VERIFYMAXVALUES_OBJECT_COUNT;
//				break;
//			case TRC_DISPLAY_MODE:
//				objectInfo.displayModeRequestCount					= VERIFYMAXVALUES_OBJECT_COUNT;
//				break;
			default:
				TCU_THROW(InternalError, "Unsupported request count");
		}

		objectInfo.pipelinePoolSizeCount = deUint32(poolSizes.size());
		objectInfo.pPipelinePoolSizes = poolSizes.empty() ? DE_NULL : poolSizes.data();

		return createCustomDevice(m_context.getTestContext().getCommandLine().isValidationEnabled(), m_context.getPlatformInterface(), instance, instance.getDriver(), physicalDevice, &deviceCreateInfo);
	}


	void performTest (const DeviceInterface&				vkd,
					  VkDevice								device) override
	{
		SimpleAllocator	allocator			(vkd, device, getPhysicalDeviceMemoryProperties(instance.getDriver(), physicalDevice));
		VkDeviceSize	pipelineDefaultSize	= VkDeviceSize(m_context.getTestContext().getCommandLine().getPipelineDefaultSize());
		deUint32		queueFamilyIndex	= 0u;

		switch (testParams.testRequestCounts)
		{
			case TRC_SEMAPHORE:
			{
				std::vector<SemaphoreSp> semaphores(VERIFYMAXVALUES_OBJECT_COUNT);
				createSemaphores(vkd, device, begin(semaphores), end(semaphores));
				std::fill(begin(semaphores) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(semaphores), SemaphoreSp());
				createSemaphores(vkd, device, begin(semaphores) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(semaphores));
				std::fill(begin(semaphores), end(semaphores), SemaphoreSp());
				createSemaphores(vkd, device, begin(semaphores), end(semaphores));
				break;
			}
			case TRC_COMMAND_BUFFER:
			{
				std::vector<CommandPoolSp> commandPools(1u);
				createCommandPools(vkd, device, begin(commandPools), end(commandPools));

				std::vector<CommandBufferSp> commandBuffers(VERIFYMAXVALUES_OBJECT_COUNT);
				createCommandBuffers(vkd, device, commandPools[0]->get(), begin(commandBuffers), end(commandBuffers));
				std::fill(begin(commandBuffers) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(commandBuffers), CommandBufferSp());
				createCommandBuffers(vkd, device, commandPools[0]->get(), begin(commandBuffers) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(commandBuffers));
				std::fill(begin(commandBuffers), end(commandBuffers), CommandBufferSp());
				createCommandBuffers(vkd, device, commandPools[0]->get(), begin(commandBuffers), end(commandBuffers));
				break;
			}
			case TRC_FENCE:
			{
				std::vector<FenceSp> fences(VERIFYMAXVALUES_OBJECT_COUNT);
				createFences(vkd, device, begin(fences), end(fences));
				std::fill(begin(fences) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(fences), FenceSp());
				createFences(vkd, device, begin(fences) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(fences));
				std::fill(begin(fences), end(fences), FenceSp());
				createFences(vkd, device, begin(fences), end(fences));
				break;
			}
			case TRC_DEVICE_MEMORY:
			{
				std::vector<DeviceMemorySp> mems(VERIFYMAXVALUES_OBJECT_COUNT);
				allocateDeviceMemory(vkd, device, 16U, begin(mems), end(mems));
				break;
			}
			case TRC_BUFFER:
			{
				std::vector<BufferSp> buffers(VERIFYMAXVALUES_OBJECT_COUNT);
				createBuffers(vkd, device, 32ull, begin(buffers), end(buffers));
				std::fill(begin(buffers) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(buffers), BufferSp());
				createBuffers(vkd, device, 32ull, begin(buffers) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(buffers));
				std::fill(begin(buffers), end(buffers), BufferSp());
				createBuffers(vkd, device, 32ull, begin(buffers), end(buffers));
				break;
			}
			case TRC_IMAGE:
			{
				std::vector<ImageSp> images(VERIFYMAXVALUES_OBJECT_COUNT);
				createImages(vkd, device, 16u, begin(images), end(images));
				std::fill(begin(images) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(images), ImageSp());
				createImages(vkd, device, 16u, begin(images) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(images));
				std::fill(begin(images), end(images), ImageSp());
				createImages(vkd, device, 16u, begin(images), end(images));
				break;
			}
			case TRC_EVENT:
			{
				std::vector<EventSp> events(VERIFYMAXVALUES_OBJECT_COUNT);
				createEvents(vkd, device, begin(events), end(events));
				std::fill(begin(events) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(events), EventSp());
				createEvents(vkd, device, begin(events) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(events));
				std::fill(begin(events), end(events), EventSp());
				createEvents(vkd, device, begin(events), end(events));
				break;
			}
			case TRC_QUERY_POOL:
			{
				std::vector<QueryPoolSp> queryPools(VERIFYMAXVALUES_OBJECT_COUNT);
				createQueryPools(vkd, device, begin(queryPools), end(queryPools));
				break;
			}
			case TRC_BUFFER_VIEW:
			{
				const VkBufferCreateInfo bufferCI = makeBufferCreateInfo(128ull, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT);
				BufferWithMemory buffer(vkd, device, allocator, bufferCI, MemoryRequirement::HostVisible);

				std::vector<BufferViewSp> bufferViews(VERIFYMAXVALUES_OBJECT_COUNT);
				createBufferViews(vkd, device, buffer.get(), 128ull, begin(bufferViews), end(bufferViews));
				std::fill(begin(bufferViews) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(bufferViews), BufferViewSp());
				createBufferViews(vkd, device, buffer.get(), 128ull, begin(bufferViews) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(bufferViews));
				std::fill(begin(bufferViews), end(bufferViews), BufferViewSp());
				createBufferViews(vkd, device, buffer.get(), 128ull, begin(bufferViews), end(bufferViews));
				break;
			}
			case TRC_IMAGE_VIEW:
			{
				const VkImageCreateInfo			imageCI =
				{
					VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,					// VkStructureType			sType;
					DE_NULL,												// const void*				pNext;
					(VkImageCreateFlags)0u,									// VkImageCreateFlags		flags;
					VK_IMAGE_TYPE_2D,										// VkImageType				imageType;
					VK_FORMAT_R8_UNORM,										// VkFormat					format;
					{
						8u,													// deUint32	width;
						8u,													// deUint32	height;
						1u													// deUint32	depth;
					},														// VkExtent3D				extent;
					1u,														// deUint32					mipLevels;
					1u,														// deUint32					arrayLayers;
					VK_SAMPLE_COUNT_1_BIT,									// VkSampleCountFlagBits	samples;
					VK_IMAGE_TILING_OPTIMAL,								// VkImageTiling			tiling;
					VK_IMAGE_USAGE_SAMPLED_BIT,								// VkImageUsageFlags		usage;
					VK_SHARING_MODE_EXCLUSIVE,								// VkSharingMode			sharingMode;
					1u,														// deUint32					queueFamilyIndexCount;
					&queueFamilyIndex,										// const deUint32*			pQueueFamilyIndices;
					VK_IMAGE_LAYOUT_UNDEFINED								// VkImageLayout			initialLayout;
				};
				ImageWithMemory image(vkd, device, allocator, imageCI, MemoryRequirement::Any);

				std::vector<ImageViewSp> imageViews(VERIFYMAXVALUES_OBJECT_COUNT);
				createImageViews(vkd, device, image.get(), VK_FORMAT_R8_UNORM, begin(imageViews), end(imageViews));
				std::fill(begin(imageViews) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(imageViews), ImageViewSp());
				createImageViews(vkd, device, image.get(), VK_FORMAT_R8_UNORM, begin(imageViews) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(imageViews));
				std::fill(begin(imageViews), end(imageViews), ImageViewSp());
				createImageViews(vkd, device, image.get(), VK_FORMAT_R8_UNORM, begin(imageViews), end(imageViews));
				break;
			}
			case TRC_LAYERED_IMAGE_VIEW:
			{
				const VkImageCreateInfo			imageCI =
				{
					VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,					// VkStructureType			sType;
					DE_NULL,												// const void*				pNext;
					(VkImageCreateFlags)0u,									// VkImageCreateFlags		flags;
					VK_IMAGE_TYPE_2D,										// VkImageType				imageType;
					VK_FORMAT_R8_UNORM,										// VkFormat					format;
					{
						8u,													// deUint32	width;
						8u,													// deUint32	height;
						1u													// deUint32	depth;
					},														// VkExtent3D				extent;
					1u,														// deUint32					mipLevels;
					VERIFYMAXVALUES_ARRAYLAYERS,							// deUint32					arrayLayers;
					VK_SAMPLE_COUNT_1_BIT,									// VkSampleCountFlagBits	samples;
					VK_IMAGE_TILING_OPTIMAL,								// VkImageTiling			tiling;
					VK_IMAGE_USAGE_SAMPLED_BIT,								// VkImageUsageFlags		usage;
					VK_SHARING_MODE_EXCLUSIVE,								// VkSharingMode			sharingMode;
					1u,														// deUint32					queueFamilyIndexCount;
					&queueFamilyIndex,										// const deUint32*			pQueueFamilyIndices;
					VK_IMAGE_LAYOUT_UNDEFINED								// VkImageLayout			initialLayout;
				};
				ImageWithMemory image(vkd, device, allocator, imageCI, MemoryRequirement::Any);

				std::vector<ImageViewSp> imageViews(VERIFYMAXVALUES_OBJECT_COUNT);
				createImageViews(vkd, device, image.get(), VK_FORMAT_R8_UNORM, begin(imageViews), end(imageViews));
				std::fill(begin(imageViews) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(imageViews), ImageViewSp());
				createImageViews(vkd, device, image.get(), VK_FORMAT_R8_UNORM, begin(imageViews) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(imageViews));
				std::fill(begin(imageViews), end(imageViews), ImageViewSp());
				createImageViews(vkd, device, image.get(), VK_FORMAT_R8_UNORM, begin(imageViews), end(imageViews));
				break;
			}
			case TRC_PIPELINE_LAYOUT:
			{
				std::vector<PipelineLayoutSp> pipelineLayouts(VERIFYMAXVALUES_OBJECT_COUNT);
				createPipelineLayouts(vkd, device, begin(pipelineLayouts), end(pipelineLayouts));
				std::fill(begin(pipelineLayouts) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(pipelineLayouts), PipelineLayoutSp());
				createPipelineLayouts(vkd, device, begin(pipelineLayouts) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(pipelineLayouts));
				std::fill(begin(pipelineLayouts), end(pipelineLayouts), PipelineLayoutSp());
				createPipelineLayouts(vkd, device, begin(pipelineLayouts), end(pipelineLayouts));
				break;
			}
			case TRC_RENDER_PASS:
			{
				VkAttachmentDescription	attachmentDescription =
				{
					0u,														// VkAttachmentDescriptionFlags	flags;
					VK_FORMAT_R8G8B8A8_UNORM,								// VkFormat						format;
					VK_SAMPLE_COUNT_1_BIT,									// VkSampleCountFlagBits		samples;
					VK_ATTACHMENT_LOAD_OP_DONT_CARE,						// VkAttachmentLoadOp			loadOp;
					VK_ATTACHMENT_STORE_OP_DONT_CARE,						// VkAttachmentStoreOp			storeOp;
					VK_ATTACHMENT_LOAD_OP_DONT_CARE,						// VkAttachmentLoadOp			stencilLoadOp;
					VK_ATTACHMENT_STORE_OP_DONT_CARE,						// VkAttachmentStoreOp			stencilStoreOp;
					VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout				initialLayout;
					VK_IMAGE_LAYOUT_GENERAL,								// VkImageLayout				finalLayout;
				};

				std::vector<RenderPassSp> renderPasses(VERIFYMAXVALUES_OBJECT_COUNT);
				createRenderPasses(vkd, device, &attachmentDescription, begin(renderPasses), end(renderPasses));
				std::fill(begin(renderPasses) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(renderPasses), RenderPassSp());
				createRenderPasses(vkd, device, &attachmentDescription, begin(renderPasses) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(renderPasses));
				std::fill(begin(renderPasses), end(renderPasses), RenderPassSp());
				createRenderPasses(vkd, device, &attachmentDescription, begin(renderPasses), end(renderPasses));
				break;
			}
			case TRC_GRAPHICS_PIPELINE:
			{
				VkAttachmentDescription	attachmentDescription =
				{
					0u,														// VkAttachmentDescriptionFlags	flags;
					VK_FORMAT_R8G8B8A8_UNORM,								// VkFormat						format;
					VK_SAMPLE_COUNT_1_BIT,									// VkSampleCountFlagBits		samples;
					VK_ATTACHMENT_LOAD_OP_DONT_CARE,						// VkAttachmentLoadOp			loadOp;
					VK_ATTACHMENT_STORE_OP_DONT_CARE,						// VkAttachmentStoreOp			storeOp;
					VK_ATTACHMENT_LOAD_OP_DONT_CARE,						// VkAttachmentLoadOp			stencilLoadOp;
					VK_ATTACHMENT_STORE_OP_DONT_CARE,						// VkAttachmentStoreOp			stencilStoreOp;
					VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout				initialLayout;
					VK_IMAGE_LAYOUT_GENERAL,								// VkImageLayout				finalLayout;
				};
				std::vector<RenderPassSp> renderPasses(1u);
				createRenderPasses(vkd, device, &attachmentDescription, begin(renderPasses), end(renderPasses));
				std::vector<PipelineLayoutSp>	pipelineLayouts(1u);
				createPipelineLayouts(vkd, device, begin(pipelineLayouts), end(pipelineLayouts));
				Move<VkShaderModule>			vertexShaderModule		= createShaderModule(vkd, device, m_context.getBinaryCollection().get("vertex"), 0u);
				Move<VkShaderModule>			fragmentShaderModule	= createShaderModule(vkd, device, m_context.getBinaryCollection().get("fragment"), 0u);

				std::vector<PipelineSp> pipelines(VERIFYMAXVALUES_OBJECT_COUNT);
				createGraphicsPipelines(vkd, device, vertexShaderModule.get(), fragmentShaderModule.get(), renderPasses[0]->get(), pipelineLayouts[0]->get(), pipelineDefaultSize, m_context.getResourceInterface(), begin(pipelines), end(pipelines));

				if (m_context.getDeviceVulkanSC10Properties().recyclePipelineMemory)
				{
					std::fill(begin(pipelines) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(pipelines), PipelineSp());
					createGraphicsPipelines(vkd, device, vertexShaderModule.get(), fragmentShaderModule.get(), renderPasses[0]->get(), pipelineLayouts[0]->get(), pipelineDefaultSize, m_context.getResourceInterface(), begin(pipelines) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(pipelines));
					std::fill(begin(pipelines), end(pipelines), PipelineSp());
					createGraphicsPipelines(vkd, device, vertexShaderModule.get(), fragmentShaderModule.get(), renderPasses[0]->get(), pipelineLayouts[0]->get(), pipelineDefaultSize, m_context.getResourceInterface(), begin(pipelines), end(pipelines));
				}

				break;
			}
			case TRC_COMPUTE_PIPELINE:
			{
				std::vector<PipelineLayoutSp>	pipelineLayouts	(1u);
				createPipelineLayouts(vkd, device, begin(pipelineLayouts), end(pipelineLayouts));
				Move<VkShaderModule>			shaderModule	= createShaderModule(vkd, device, m_context.getBinaryCollection().get("compute"), 0u);

				std::vector<PipelineSp> pipelines(VERIFYMAXVALUES_OBJECT_COUNT);
				createComputePipelines(vkd, device, shaderModule.get(), pipelineLayouts[0]->get(), pipelineDefaultSize, m_context.getResourceInterface(), begin(pipelines), end(pipelines));

				if (m_context.getDeviceVulkanSC10Properties().recyclePipelineMemory)
				{
					std::fill(begin(pipelines) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(pipelines), PipelineSp());
					createComputePipelines(vkd, device, shaderModule.get(), pipelineLayouts[0]->get(), pipelineDefaultSize, m_context.getResourceInterface(), begin(pipelines) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(pipelines));
					std::fill(begin(pipelines), end(pipelines), PipelineSp());
					createComputePipelines(vkd, device, shaderModule.get(), pipelineLayouts[0]->get(), pipelineDefaultSize, m_context.getResourceInterface(), begin(pipelines), end(pipelines));
				}
				break;
			}
			case TRC_DESCRIPTORSET_LAYOUT:
			{
				std::vector<DescriptorSetLayoutSp> descriptorSetLayouts(VERIFYMAXVALUES_OBJECT_COUNT);
				createDescriptorSetLayouts(vkd, device, begin(descriptorSetLayouts), end(descriptorSetLayouts));
				std::fill(begin(descriptorSetLayouts) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(descriptorSetLayouts), DescriptorSetLayoutSp());
				createDescriptorSetLayouts(vkd, device, begin(descriptorSetLayouts) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(descriptorSetLayouts));
				std::fill(begin(descriptorSetLayouts), end(descriptorSetLayouts), DescriptorSetLayoutSp());
				createDescriptorSetLayouts(vkd, device, begin(descriptorSetLayouts), end(descriptorSetLayouts));
				break;
			}
			case TRC_SAMPLER:
			{
				std::vector<SamplerSp> samplers(VERIFYMAXVALUES_OBJECT_COUNT);
				createSamplers(vkd, device, begin(samplers), end(samplers));
				std::fill(begin(samplers) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(samplers), SamplerSp());
				createSamplers(vkd, device, begin(samplers) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(samplers));
				std::fill(begin(samplers), end(samplers), SamplerSp());
				createSamplers(vkd, device, begin(samplers), end(samplers));
				break;
			}
			case TRC_DESCRIPTOR_POOL:
			{
				std::vector<DescriptorPoolSp> descriptorPools(VERIFYMAXVALUES_OBJECT_COUNT);
				createDescriptorPools(vkd, device, 1u, begin(descriptorPools), end(descriptorPools));
				break;
			}
			case TRC_DESCRIPTORSET:
			{
				std::vector<DescriptorSetLayoutSp> descriptorSetLayouts(1u);
				createDescriptorSetLayouts(vkd, device, begin(descriptorSetLayouts), end(descriptorSetLayouts));
				std::vector<DescriptorPoolSp> descriptorPools(1u);
				createDescriptorPools(vkd, device, VERIFYMAXVALUES_OBJECT_COUNT, begin(descriptorPools), end(descriptorPools));

				std::vector<DescriptorSetSp> descriptorSets(VERIFYMAXVALUES_OBJECT_COUNT);
				createDescriptorSets(vkd, device, descriptorPools[0]->get(), descriptorSetLayouts[0]->get(), begin(descriptorSets), end(descriptorSets));
				if (m_context.getDeviceVulkanSC10Properties().recycleDescriptorSetMemory)
				{
					std::fill(begin(descriptorSets) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(descriptorSets), DescriptorSetSp());
					createDescriptorSets(vkd, device, descriptorPools[0]->get(), descriptorSetLayouts[0]->get(), begin(descriptorSets) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(descriptorSets));
					std::fill(begin(descriptorSets), end(descriptorSets), DescriptorSetSp());
					createDescriptorSets(vkd, device, descriptorPools[0]->get(), descriptorSetLayouts[0]->get(), begin(descriptorSets), end(descriptorSets));
				}
				break;
			}
			case TRC_FRAMEBUFFER:
			{
				VkImageCreateInfo	imageCI	=
				{
					VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,					// VkStructureType			sType
					DE_NULL,												// const void*				pNext
					(VkImageCreateFlags)0u,									// VkImageCreateFlags		flags
					VK_IMAGE_TYPE_2D,										// VkImageType				imageType
					VK_FORMAT_R8G8B8A8_UNORM,								// VkFormat					format
					{
						8u,													// deUint32	width
						8u,													// deUint32	height
						1u													// deUint32	depth
					},														// VkExtent3D				extent
					1u,														// deUint32					mipLevels
					1u,														// deUint32					arrayLayers
					VK_SAMPLE_COUNT_1_BIT,									// VkSampleCountFlagBits	samples
					VK_IMAGE_TILING_OPTIMAL,								// VkImageTiling			tiling
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,					// VkImageUsageFlags		usage;
					VK_SHARING_MODE_EXCLUSIVE,								// VkSharingMode			sharingMode;
					1u,														// deUint32					queueFamilyIndexCount;
					&queueFamilyIndex,										// const deUint32*			pQueueFamilyIndices;
					VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout			initialLayout;
				};
				ImageWithMemory image(vkd, device, allocator, imageCI, MemoryRequirement::Any);

				VkAttachmentDescription	attachmentDescription =
				{
					0u,														// VkAttachmentDescriptionFlags	flags;
					VK_FORMAT_R8G8B8A8_UNORM,								// VkFormat						format;
					VK_SAMPLE_COUNT_1_BIT,									// VkSampleCountFlagBits		samples;
					VK_ATTACHMENT_LOAD_OP_DONT_CARE,						// VkAttachmentLoadOp			loadOp;
					VK_ATTACHMENT_STORE_OP_DONT_CARE,						// VkAttachmentStoreOp			storeOp;
					VK_ATTACHMENT_LOAD_OP_DONT_CARE,						// VkAttachmentLoadOp			stencilLoadOp;
					VK_ATTACHMENT_STORE_OP_DONT_CARE,						// VkAttachmentStoreOp			stencilStoreOp;
					VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout				initialLayout;
					VK_IMAGE_LAYOUT_GENERAL,								// VkImageLayout				finalLayout;
				};

				std::vector<RenderPassSp> renderPasses(1u);
				createRenderPasses(vkd, device, &attachmentDescription, begin(renderPasses), end(renderPasses));

				std::vector<ImageViewSp> imageViews(1u);
				createImageViews(vkd, device, image.get(), VK_FORMAT_R8G8B8A8_UNORM, begin(imageViews), end(imageViews));

				std::vector<FramebufferSp> framebuffers(VERIFYMAXVALUES_OBJECT_COUNT);
				createFramebuffers(vkd, device, renderPasses[0]->get(), imageViews[0]->get(), begin(framebuffers), end(framebuffers));
				std::fill(begin(framebuffers) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(framebuffers), FramebufferSp());
				createFramebuffers(vkd, device, renderPasses[0]->get(), imageViews[0]->get(), begin(framebuffers) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(framebuffers));
				std::fill(begin(framebuffers), end(framebuffers), FramebufferSp());
				createFramebuffers(vkd, device, renderPasses[0]->get(), imageViews[0]->get(), begin(framebuffers), end(framebuffers));
				break;
			}
			case TRC_COMMANDPOOL:
			{
				std::vector<CommandPoolSp> commandPools(VERIFYMAXVALUES_OBJECT_COUNT);
				createCommandPools(vkd, device, begin(commandPools), end(commandPools));
				break;
			}
			case TRC_SAMPLERYCBCRCONVERSION:
			{
				std::vector<SamplerYcbcrConversionSp> samplers(VERIFYMAXVALUES_OBJECT_COUNT);
				createSamplerYcbcrConversions(vkd, device, begin(samplers), end(samplers));
				std::fill(begin(samplers) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(samplers), SamplerYcbcrConversionSp());
				createSamplerYcbcrConversions(vkd, device, begin(samplers) + VERIFYMAXVALUES_OBJECT_COUNT / 2, end(samplers));
				std::fill(begin(samplers), end(samplers), SamplerYcbcrConversionSp());
				createSamplerYcbcrConversions(vkd, device, begin(samplers), end(samplers));
				break;
			}
//			case TRC_SURFACE:
//				break;
//			case TRC_SWAPCHAIN:
//				break;
//			case TRC_DISPLAY_MODE:
//				break;
			default:
				TCU_THROW(InternalError, "Unsupported max value");
		}
	}
};

// test pipeline pool sizes
class VerifyPipelinePoolSizes : public DeviceObjectReservationInstance
{
public:
	VerifyPipelinePoolSizes			(Context&								context,
									 const TestParams&						testParams_)
		: DeviceObjectReservationInstance(context, testParams_)
	{
	}

	Move<VkDevice> createTestDevice (VkDeviceCreateInfo&					deviceCreateInfo,
									 VkDeviceObjectReservationCreateInfo&	objectInfo,
									 VkPhysicalDeviceVulkanSC10Features&	sc10Features) override
	{
		DE_UNREF(sc10Features);

		std::vector<VkPipelinePoolSize> poolSizes;

		const VkDeviceSize psTooSmall		= 64u;
		const VkDeviceSize psForOnePipeline	= VkDeviceSize(m_context.getTestContext().getCommandLine().getPipelineDefaultSize());

		switch (testParams.testPoolSizeType)
		{
		case PST_NONE:
			objectInfo.graphicsPipelineRequestCount = 1u;
			break;
		case PST_ZERO:
			objectInfo.graphicsPipelineRequestCount = 1u;
			poolSizes.push_back({ VK_STRUCTURE_TYPE_PIPELINE_POOL_SIZE, DE_NULL, 0u, 1u });
			break;
		case PST_TOO_SMALL_SIZE:
			poolSizes.push_back({ VK_STRUCTURE_TYPE_PIPELINE_POOL_SIZE, DE_NULL, psTooSmall, 1u });
			objectInfo.graphicsPipelineRequestCount = 1u;
			break;
		case PST_ONE_FITS:
			poolSizes.push_back({ VK_STRUCTURE_TYPE_PIPELINE_POOL_SIZE, DE_NULL, psForOnePipeline, 1u });
			objectInfo.graphicsPipelineRequestCount = 1u;
			break;
		case PST_MULTIPLE_FIT:
			poolSizes.push_back({ VK_STRUCTURE_TYPE_PIPELINE_POOL_SIZE, DE_NULL, psForOnePipeline, 16u });
			objectInfo.graphicsPipelineRequestCount = 16u;
			break;
		default:
			TCU_THROW(InternalError, "Unsupported pool size type");
		}

		objectInfo.pipelinePoolSizeCount					= deUint32(poolSizes.size());
		objectInfo.pPipelinePoolSizes						= poolSizes.empty() ? DE_NULL : poolSizes.data();
		objectInfo.pipelineLayoutRequestCount				= 1u;
		objectInfo.renderPassRequestCount					= 1u;
		objectInfo.subpassDescriptionRequestCount			= 1u;
		objectInfo.attachmentDescriptionRequestCount		= 1u;

		return createCustomDevice(m_context.getTestContext().getCommandLine().isValidationEnabled(), m_context.getPlatformInterface(), instance, instance.getDriver(), physicalDevice, &deviceCreateInfo);
	}

	void performTest (const DeviceInterface&				vk,
					  VkDevice								device) override
	{
		const vk::PlatformInterface&					vkp								= m_context.getPlatformInterface();
		const InstanceInterface&						vki								= instance.getDriver();

		Move<VkShaderModule>							vertexShader					= createShaderModule(vk, device, m_context.getBinaryCollection().get("vertex"), 0);
		Move<VkShaderModule>							fragmentShader					= createShaderModule(vk, device, m_context.getBinaryCollection().get("fragment"), 0);

		std::vector<VkPipelineShaderStageCreateInfo>	shaderStageCreateInfos			=
		{
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,						// VkStructureType                     sType;
				DE_NULL,																	// const void*                         pNext;
				(VkPipelineShaderStageCreateFlags)0,										// VkPipelineShaderStageCreateFlags    flags;
				VK_SHADER_STAGE_VERTEX_BIT,													// VkShaderStageFlagBits               stage;
				*vertexShader,																// VkShaderModule                      shader;
				"main",																		// const char*                         pName;
				DE_NULL,																	// const VkSpecializationInfo*         pSpecializationInfo;
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,						// VkStructureType                     sType;
				DE_NULL,																	// const void*                         pNext;
				(VkPipelineShaderStageCreateFlags)0,										// VkPipelineShaderStageCreateFlags    flags;
				VK_SHADER_STAGE_FRAGMENT_BIT,												// VkShaderStageFlagBits               stage;
				*fragmentShader,																// VkShaderModule                      shader;
				"main",																		// const char*                         pName;
				DE_NULL,																	// const VkSpecializationInfo*         pSpecializationInfo;
			}
		};

		VkPipelineVertexInputStateCreateInfo			vertexInputStateCreateInfo;
		VkPipelineInputAssemblyStateCreateInfo			inputAssemblyStateCreateInfo;
		VkPipelineViewportStateCreateInfo				viewPortStateCreateInfo;
		VkPipelineRasterizationStateCreateInfo			rasterizationStateCreateInfo;
		VkPipelineMultisampleStateCreateInfo			multisampleStateCreateInfo;
		VkPipelineColorBlendAttachmentState				colorBlendAttachmentState;
		VkPipelineColorBlendStateCreateInfo				colorBlendStateCreateInfo;
		VkPipelineDynamicStateCreateInfo				dynamicStateCreateInfo;
		std::vector<VkDynamicState>						dynamicStates					= { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

		const VkPipelineLayoutCreateInfo				pipelineLayoutCreateInfo		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,										// VkStructureType                     sType;
			DE_NULL,																			// const void*                         pNext;
			(VkPipelineLayoutCreateFlags)0u,													// VkPipelineLayoutCreateFlags         flags;
			0u,																					// deUint32                            setLayoutCount;
			DE_NULL,																			// const VkDescriptorSetLayout*        pSetLayouts;
			0u,																					// deUint32                            pushConstantRangeCount;
			DE_NULL																				// const VkPushConstantRange*          pPushConstantRanges;
		};
		Move<VkPipelineLayout>							pipelineLayout					= createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

		const VkFormat									format							= getRenderTargetFormat(vki, physicalDevice);

		VkAttachmentDescription							attachmentDescription;
		VkAttachmentReference							attachmentReference;
		VkSubpassDescription							subpassDescription;
		VkRenderPassCreateInfo							renderPassCreateInfo			= prepareSimpleRenderPassCI(format, attachmentDescription, attachmentReference, subpassDescription);
		Move<VkRenderPass>								renderPass						= createRenderPass(vk, device, &renderPassCreateInfo);

		VkGraphicsPipelineCreateInfo					graphicsPipelineCreateInfo		= prepareSimpleGraphicsPipelineCI(vertexInputStateCreateInfo, shaderStageCreateInfos,
				inputAssemblyStateCreateInfo, viewPortStateCreateInfo, rasterizationStateCreateInfo, multisampleStateCreateInfo, colorBlendAttachmentState, colorBlendStateCreateInfo,
				dynamicStateCreateInfo, dynamicStates, *pipelineLayout, *renderPass);

		// create custom VkPipelineIdentifierInfo
		VkPipelineOfflineCreateInfo						pipelineID						= resetPipelineOfflineCreateInfo();
		applyPipelineIdentifier(pipelineID, "ID_DR_PS_00");
		pipelineID.pNext																= graphicsPipelineCreateInfo.pNext;
		graphicsPipelineCreateInfo.pNext												= &pipelineID;

		if (m_context.getTestContext().getCommandLine().isSubProcess())
		{
			pipelineID.poolEntrySize = VkDeviceSize(m_context.getTestContext().getCommandLine().getPipelineDefaultSize());
		}

		std::size_t										pipelineCount					= 0u;
		switch (testParams.testPoolSizeType)
		{
		case PST_NONE:
		case PST_ZERO:
		case PST_TOO_SMALL_SIZE:
		case PST_ONE_FITS:
			pipelineCount = 1u;
			break;
		case PST_MULTIPLE_FIT:
			pipelineCount = 16u;
			break;
		default:
			TCU_THROW(InternalError, "Unsupported pool size type");
		};

		if (!m_context.getTestContext().getCommandLine().isSubProcess())
		{
			std::vector<Move<VkPipeline>> pipelines;
			for (deUint32 i = 0; i < pipelineCount; ++i)
				pipelines.emplace_back(createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineCreateInfo));
			return;
		}

		GetDeviceProcAddrFunc				getDeviceProcAddrFunc			= (GetDeviceProcAddrFunc)vkp.getInstanceProcAddr(instance, "vkGetDeviceProcAddr");
		CreateGraphicsPipelinesFunc			createGraphicsPipelinesFunc		= (CreateGraphicsPipelinesFunc)getDeviceProcAddrFunc(device, "vkCreateGraphicsPipelines");
		DestroyPipelineFunc					destroyPipelineFunc				= (DestroyPipelineFunc)getDeviceProcAddrFunc(device, "vkDestroyPipeline");
		VkPipelineCache						pipelineCache					= m_context.getResourceInterface()->getPipelineCache(device);
		std::vector<VkPipeline>				pipelines						(pipelineCount, VkPipeline(DE_NULL));
		deUint32							iterations						= m_context.getDeviceVulkanSC10Properties().recyclePipelineMemory ? 1u : 4u;

		// if recyclePipelineMemory is set then we are able to create the same pipelines again
		for (deUint32 iter = 0; iter < iterations; ++iter)
		{
			for (deUint32 i = 0; i < pipelineCount; ++i)
			{
				VkResult result = createGraphicsPipelinesFunc(device, pipelineCache, 1u, &graphicsPipelineCreateInfo, DE_NULL, &pipelines[i]);
				results.push_back(result);
				if (result != VK_SUCCESS)
				{
					for (deUint32 j = 0; j < pipelineCount; ++j)
						if (pipelines[j].getInternal() != DE_NULL)
							destroyPipelineFunc(device, pipelines[j], DE_NULL);
					return;
				}
			}

			for (deUint32 i = 0; i < pipelineCount; ++i)
			{
				destroyPipelineFunc(device, pipelines[i], DE_NULL);
				pipelines[i] = VkPipeline(DE_NULL);
			}
		}
	}

	bool verifyTestResults	(const DeviceInterface&					vkd,
							 VkDevice								device) override
	{
		DE_UNREF(vkd);
		DE_UNREF(device);

		if (!m_context.getTestContext().getCommandLine().isSubProcess())
			return true;

		switch (testParams.testPoolSizeType)
		{
		case PST_NONE:
		case PST_ZERO:
		case PST_TOO_SMALL_SIZE:
			return (results.back() == VK_ERROR_OUT_OF_POOL_MEMORY);
		case PST_ONE_FITS:
			return (results.back() == VK_SUCCESS);
		case PST_MULTIPLE_FIT:
			return (results.back() == VK_SUCCESS);
		default:
			TCU_THROW(InternalError, "Unsupported pool size type");
		};
		return true;
	}

	std::vector<VkResult> results;
};

} // anonymous

tcu::TestCaseGroup*	createDeviceObjectReservationTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "device_object_reservation", "Tests verifying VkDeviceObjectReservationCreateInfo"));

	// add basic tests
	{
		de::MovePtr<tcu::TestCaseGroup> basicGroup(new tcu::TestCaseGroup(group->getTestContext(), "basic", ""));

		basicGroup->addChild(new InstanceFactory1<DeviceObjectReservationInstance, TestParams>(testCtx, "create_device", "", TestParams()));
		basicGroup->addChild(new InstanceFactory1<MultipleReservation, TestParams>(testCtx, "multiple_device_object_reservation", "", TestParams()));

		group->addChild(basicGroup.release());
	}

	// add tests verifying device limits
	{
		de::MovePtr<tcu::TestCaseGroup> limitGroup(new tcu::TestCaseGroup(group->getTestContext(), "limits", ""));
		struct TestMaxValuesData
		{
			TestMaxValues							testMaxValues;
			const char*								name;
		} testMaxValues[] =
		{
			{ TMV_DESCRIPTOR_SET_LAYOUT_BINDING_LIMIT,		"descriptor_set_layout_binding_limit" },
			{ TMV_MAX_IMAGEVIEW_MIPLEVELS,					"max_imageview_miplevels" },
			{ TMV_MAX_IMAGEVIEW_ARRAYLAYERS,				"max_imageview_arraylayers" },
			{ TMV_MAX_LAYEREDIMAGEVIEW_MIPLEVELS,			"max_layeredimageview_miplevels" },
			{ TMV_MAX_OCCLUSION_QUERIES_PER_POOL,			"max_occlusion_queries_per_pool" },
			{ TMV_MAX_PIPELINESTATISTICS_QUERIES_PER_POOL,	"max_pipelinestatistics_queries_per_pool" },
			{ TMV_MAX_TIMESTAMP_QUERIES_PER_POOL,			"max_timestamp_queries_per_pool" },
		};
		{
			de::MovePtr<tcu::TestCaseGroup> maxValGroup(new tcu::TestCaseGroup(group->getTestContext(), "max_values", ""));

			for (deInt32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(testMaxValues); ndx++)
			{
				TestParams testParams
				{
					testMaxValues[ndx].testMaxValues,
					TRC_UNDEFINED
				};
				maxValGroup->addChild(new InstanceFactory1WithSupport<VerifyMaxValues, TestParams, FunctionSupport1<TestParams>>(testCtx, testMaxValues[ndx].name, "", testParams,
					typename FunctionSupport1<TestParams>::Args(checkSupportVerifyMaxValues, testParams)));
			}

			limitGroup->addChild(maxValGroup.release());
		}

		struct TestRequestCountData
		{
			TestRequestCounts						requestCount;
			const char*								name;
		} testRequestCounts[] =
		{
			{ TRC_SEMAPHORE,					"semaphore" },
			{ TRC_COMMAND_BUFFER,				"command_buffer" },
			{ TRC_FENCE,						"fence" },
			{ TRC_DEVICE_MEMORY,				"device_memory" },
			{ TRC_BUFFER,						"buffer" },
			{ TRC_IMAGE,						"image" },
			{ TRC_EVENT,						"event" },
			{ TRC_QUERY_POOL,					"query_pool" },
			{ TRC_BUFFER_VIEW,					"buffer_view" },
			{ TRC_IMAGE_VIEW,					"image_view" },
			{ TRC_LAYERED_IMAGE_VIEW,			"layered_image_view" },
			{ TRC_PIPELINE_LAYOUT,				"pipeline_layout" },
			{ TRC_RENDER_PASS,					"render_pass" },
			{ TRC_GRAPHICS_PIPELINE,			"graphics_pipeline" },
			{ TRC_COMPUTE_PIPELINE,				"compute_pipeline" },
			{ TRC_DESCRIPTORSET_LAYOUT,			"descriptorset_layout" },
			{ TRC_SAMPLER,						"sampler" },
			{ TRC_DESCRIPTOR_POOL,				"descriptor_pool" },
			{ TRC_DESCRIPTORSET,				"descriptorset" },
			{ TRC_FRAMEBUFFER,					"framebuffer" },
			{ TRC_COMMANDPOOL,					"commandpool" },
			{ TRC_SAMPLERYCBCRCONVERSION,		"samplerycbcrconversion" },
//			{ TRC_SURFACE,						"surface" },
//			{ TRC_SWAPCHAIN,					"swapchain" },
//			{ TRC_DISPLAY_MODE,					"display_mode" },
		};
		{
			de::MovePtr<tcu::TestCaseGroup> requestCountGroup(new tcu::TestCaseGroup(group->getTestContext(), "request_count", ""));

			for (deInt32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(testRequestCounts); ndx++)
			{
				TestParams testParams
				{
					TMV_UNDEFINED,
					testRequestCounts[ndx].requestCount
				};
				requestCountGroup->addChild(new InstanceFactory1WithSupport<VerifyRequestCounts, TestParams, FunctionSupport1<TestParams>, ProgramsVerifyLimits>(testCtx, testRequestCounts[ndx].name, "",
					ProgramsVerifyLimits(), testParams, typename FunctionSupport1<TestParams>::Args(checkSupportVerifyRequestCounts, testParams)));
			}

			limitGroup->addChild(requestCountGroup.release());
		}

		group->addChild(limitGroup.release());
	}

	// add tests verifying pipeline pool sizes
	{
		de::MovePtr<tcu::TestCaseGroup> ppsGroup(new tcu::TestCaseGroup(group->getTestContext(), "pipeline_pool_size", ""));


		struct PoolSizesData
		{
			TestPoolSizes					type;
			const char*						name;
		} poolSizes[] =
		{
			{ PST_NONE,					"none" },
			{ PST_ZERO,					"zero" },
			{ PST_TOO_SMALL_SIZE,		"too_small_size" },
			{ PST_ONE_FITS,				"one_fits" },
			{ PST_MULTIPLE_FIT,			"multiple_fit" },
		};

		for (deInt32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(poolSizes); ndx++)
		{
			TestParams testParams(TMV_UNDEFINED, TRC_UNDEFINED, poolSizes[ndx].type);

			ppsGroup->addChild(new InstanceFactory1<VerifyPipelinePoolSizes, TestParams, ProgramsVerifyLimits>(testCtx, poolSizes[ndx].name, "", ProgramsVerifyLimits(), testParams));
		}


		group->addChild(ppsGroup.release());
	}

	return group.release();
}

}	// sc

}	// vkt
