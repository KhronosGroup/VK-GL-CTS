/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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
 * \brief Memory Commitment tests
 *//*--------------------------------------------------------------------*/

#include "vktApiGetMemoryCommitment.hpp"

#include "vkDeviceUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vktTestCase.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuTestLog.hpp"

using namespace vk;
using tcu::TestLog;

namespace vkt
{
namespace api
{

struct MemoryCommitmentCaseParams
{
	deUint32	bufferSize;
	deUint32	bufferViewSize;
	deUint32	elementOffset;
};

namespace
{

std::vector<deUint32> getMemoryTypeIndices (VkMemoryPropertyFlags propertyFlag, const VkPhysicalDeviceMemoryProperties& pMemoryProperties)
{
	std::vector<deUint32> indices;
	for (deUint32 typeIndex = 0u; typeIndex < pMemoryProperties.memoryTypeCount; ++typeIndex)
	{
		if ((pMemoryProperties.memoryTypes[typeIndex].propertyFlags & propertyFlag) == propertyFlag)
			indices.push_back(typeIndex);
	}
	return indices;
}

}

class MemoryCommitmentTestInstance : public vkt::TestInstance
{
public:
									MemoryCommitmentTestInstance	(Context& context, MemoryCommitmentCaseParams testCase);
	tcu::TestStatus					iterate							(void);
	Move<VkCommandPool>				createCommandPool				() const;
	Move<VkCommandBuffer>			allocatePrimaryCommandBuffer	(VkCommandPool commandPool) const;
	bool							isDeviceMemoryCommitmentOk		(const VkMemoryRequirements memoryRequirements);

private:
	const tcu::IVec2				m_renderSize;
};

MemoryCommitmentTestInstance::MemoryCommitmentTestInstance(Context& context, MemoryCommitmentCaseParams testCase)
	: vkt::TestInstance		(context)
	, m_renderSize			(testCase.bufferViewSize, testCase.bufferViewSize)
{
}

class MemoryCommitmentTestCase : public vkt::TestCase
{
public:
							MemoryCommitmentTestCase	(tcu::TestContext&				testCtx,
														const std::string&				name,
														MemoryCommitmentCaseParams		memoryCommitmentTestInfo)
							: vkt::TestCase					(testCtx, name)
							, m_memoryCommitmentTestInfo	(memoryCommitmentTestInfo)
							{}
	virtual					~MemoryCommitmentTestCase(void){}
	virtual	void			initPrograms	(SourceCollections&	programCollection)	const;
	virtual TestInstance*	createInstance	(Context&			context)			const
							{
								return new MemoryCommitmentTestInstance(context, m_memoryCommitmentTestInfo);
							}
private:
	MemoryCommitmentCaseParams m_memoryCommitmentTestInfo;
};

tcu::TestStatus MemoryCommitmentTestInstance::iterate(void)
{
	const VkMemoryPropertyFlags				propertyFlag			= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
	const VkPhysicalDevice					physicalDevice			= m_context.getPhysicalDevice();
	const InstanceInterface&				vki						= m_context.getInstanceInterface();
	const VkPhysicalDeviceMemoryProperties	pMemoryProperties		= getPhysicalDeviceMemoryProperties(vki,physicalDevice);
	const std::vector<deUint32>				memoryTypeIndices		= getMemoryTypeIndices(propertyFlag, pMemoryProperties);
	Allocator&								memAlloc				= m_context.getDefaultAllocator();
	bool									isMemoryAllocationOK	= false;
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	const VkComponentMapping				componentMappingRGBA	= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	const DeviceInterface&					vkd						= m_context.getDeviceInterface();
	const Move<VkCommandPool>				cmdPool					= createCommandPool();
	const Move<VkCommandBuffer>				cmdBuffer				= allocatePrimaryCommandBuffer(*cmdPool);
	const VkDevice							device					= m_context.getDevice();
	Move<VkImageView>						colorAttachmentView;
	Move<VkRenderPass>						renderPass;
	Move<VkFramebuffer>						framebuffer;
	Move<VkDescriptorSetLayout>				descriptorSetLayout;
	Move<VkPipelineLayout>					pipelineLayout;
	Move<VkShaderModule>					vertexShaderModule;
	Move<VkShaderModule>					fragmentShaderModule;
	Move<VkPipeline>						graphicsPipelines;

	// Note we can still fail later if none of lazily allocated memory types can be used with the image below.
	if (memoryTypeIndices.empty())
		TCU_THROW(NotSupportedError, "Lazily allocated bit is not supported by any memory type");

	const VkImageCreateInfo	imageParams			=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType		sType;
		DE_NULL,										// const void*			pNext;
		0u,												// VkImageCreateFlags	flags;
		VK_IMAGE_TYPE_2D,								// VkImageType			imageType;
		VK_FORMAT_R32_UINT,								// VkFormat				format;
		{256u, 256u, 1},								// VkExtent3D			extent;
		1u,												// deUint32				mipLevels;
		1u,												// deUint32				arraySize;
		VK_SAMPLE_COUNT_1_BIT,							// deUint32				samples;
		VK_IMAGE_TILING_OPTIMAL,						// VkImageTiling		tiling;
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
			VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,	// VkImageUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode		sharingMode;
		1u,												// deUint32				queueFamilyCount;
		&queueFamilyIndex,								// const deUint32*		pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout		initialLayout;
	};

	Move<VkImage>				image				= createImage(vkd, device, &imageParams);
	const VkMemoryRequirements	memoryRequirements	= getImageMemoryRequirements(vkd, device, *image);
	de::MovePtr<Allocation>		imageAlloc			= memAlloc.allocate(memoryRequirements, MemoryRequirement::LazilyAllocated);

	VK_CHECK(vkd.bindImageMemory(device, *image, imageAlloc->getMemory(), imageAlloc->getOffset()));

	const VkImageViewCreateInfo colorAttachmentViewParams	=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,			// VkStructureType			sType;
		DE_NULL,											// const void*				pNext;
		0u,													// VkImageViewCreateFlags	flags;
		*image,												// VkImage					image;
		VK_IMAGE_VIEW_TYPE_2D,								// VkImageViewType			viewType;
		VK_FORMAT_R32_UINT,									// VkFormat					format;
		componentMappingRGBA,								// VkComponentMapping		components;
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }		// VkImageSubresourceRange	subresourceRange;
	};

	colorAttachmentView = createImageView(vkd, device, &colorAttachmentViewParams);

	// Create render pass
	renderPass = makeRenderPass(vkd, device, VK_FORMAT_R32_UINT);

	// Create framebuffer
	{
		const VkImageView attachmentBindInfos[1] =
		{
			*colorAttachmentView,
		};

		const VkFramebufferCreateInfo framebufferParams =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			(VkFramebufferCreateFlags)0,
			*renderPass,										// VkRenderPass					renderPass;
			1u,													// deUint32						attachmentCount;
			attachmentBindInfos,								// const VkImageView*			pAttachments;
			(deUint32)m_renderSize.x(),							// deUint32						width;
			(deUint32)m_renderSize.y(),							// deUint32						height;
			1u													// deUint32						layers;
		};

		framebuffer = createFramebuffer(vkd, device, &framebufferParams);
	}

	// Create descriptors
	{
		const VkDescriptorSetLayoutBinding layoutBindings[1] =
		{
			{
				0u,											// deUint32				binding;
				VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,	// VkDescriptorType		descriptorType;
				1u,											// deUint32				arraySize;
				VK_SHADER_STAGE_ALL,						// VkShaderStageFlags	stageFlags;
				DE_NULL										// const VkSampler*		pImmutableSamplers;
			},
		};

		const VkDescriptorSetLayoutCreateInfo descriptorLayoutParams =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	// VkStructureType						sType;
			DE_NULL,												// const void*							pNext;
			(VkDescriptorSetLayoutCreateFlags)0,
			DE_LENGTH_OF_ARRAY(layoutBindings),						// deUint32								count;
			layoutBindings											// const VkDescriptorSetLayoutBinding	pBinding;
		};

		descriptorSetLayout = createDescriptorSetLayout(vkd, device, &descriptorLayoutParams);
	}

	// Create pipeline layout
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			(VkPipelineLayoutCreateFlags)0,
			1u,													// deUint32						descriptorSetCount;
			&*descriptorSetLayout,								// const VkDescriptorSetLayout*	pSetLayouts;
			0u,													// deUint32						pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*	pPushConstantRanges;
		};

		pipelineLayout = createPipelineLayout(vkd, device, &pipelineLayoutParams);
	}

	// Create shaders
	{
		vertexShaderModule		= createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0);
		fragmentShaderModule	= createShaderModule(vkd, device, m_context.getBinaryCollection().get("frag"), 0);
	}

	// Create pipeline
	{
		const std::vector<VkViewport>	viewports	(1, makeViewport(m_renderSize));
		const std::vector<VkRect2D>		scissors	(1, makeRect2D(m_renderSize));

		graphicsPipelines = makeGraphicsPipeline(vkd,					// const DeviceInterface&            vk
												 device,				// const VkDevice                    device
												 *pipelineLayout,		// const VkPipelineLayout            pipelineLayout
												 *vertexShaderModule,	// const VkShaderModule              vertexShaderModule
												 DE_NULL,				// const VkShaderModule              tessellationControlModule
												 DE_NULL,				// const VkShaderModule              tessellationEvalModule
												 DE_NULL,				// const VkShaderModule              geometryShaderModule
												 *fragmentShaderModule,	// const VkShaderModule              fragmentShaderModule
												 *renderPass,			// const VkRenderPass                renderPass
												 viewports,				// const std::vector<VkViewport>&    viewports
												 scissors);				// const std::vector<VkRect2D>&      scissors
	}

	// getMemoryCommitment
	isMemoryAllocationOK = isDeviceMemoryCommitmentOk(memoryRequirements);

	const deUint32			clearColor[4]	= { 1u, 1u, 1u, 1u };
	const VkClearAttachment	clearAttachment	=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,									// VkImageAspectFlags	aspectMask;
		0u,															// deUint32				colorAttachment;
		makeClearValueColorU32(clearColor[0],
							   clearColor[1],
							   clearColor[2],
							   clearColor[3])						// VkClearValue			clearValue;
	};

	const VkOffset2D offset =
	{
		0,
		0
	};

	const VkExtent2D extent =
	{
		256u,
		256u
	};

	const VkRect2D rect =
	{
		offset,
		extent
	};

	const VkClearRect clearRect =
	{
		rect,
		0u, // baseArrayLayer
		1u	// layerCount
	};

	// beginCommandBuffer
	beginCommandBuffer(vkd, *cmdBuffer);

	const VkImageMemoryBarrier initialImageBarrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		0,											// VkMemoryOutputFlags		outputMask;
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// VkMemoryInputFlags		inputMask;
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					destQueueFamilyIndex;
		image.get(),								// VkImage					image;
		{											// VkImageSubresourceRange	subresourceRange;
			VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspectFlags	aspectMask;
			0u,										// deUint32				baseMipLevel;
			1u,										// deUint32				mipLevels;
			0u,										// deUint32				baseArraySlice;
			1u										// deUint32				arraySize;
		}
	};

	vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &initialImageBarrier);
	beginRenderPass(vkd, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, 256u, 256u), tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
	vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipelines);
	// clearAttachments
	vkd.cmdClearAttachments(*cmdBuffer, 1, &clearAttachment, 1u, &clearRect);
	endRenderPass(vkd, *cmdBuffer);
	endCommandBuffer(vkd, *cmdBuffer);

	// queueSubmit
	const VkQueue	queue	= m_context.getUniversalQueue();
	submitCommandsAndWait(vkd, device, queue, *cmdBuffer);

	// getMemoryCommitment
	isMemoryAllocationOK = (isMemoryAllocationOK && isDeviceMemoryCommitmentOk(memoryRequirements)) ? true : false;

	if (isMemoryAllocationOK)
		return tcu::TestStatus::pass("Pass");

	return tcu::TestStatus::fail("Fail");
}

class MemoryCommitmentAllocateOnlyTestInstance : public vkt::TestInstance
{
public:
									MemoryCommitmentAllocateOnlyTestInstance	(Context& context);
	tcu::TestStatus					iterate										(void);
};

class MemoryCommitmentAllocateOnlyTestCase : public vkt::TestCase
{
public:
							MemoryCommitmentAllocateOnlyTestCase	(tcu::TestContext&				testCtx,
																	const std::string&				name)
							: vkt::TestCase							(testCtx, name)
							{}
	virtual					~MemoryCommitmentAllocateOnlyTestCase(void){}
	virtual TestInstance*	createInstance	(Context&			context)			const
							{
								return new MemoryCommitmentAllocateOnlyTestInstance(context);
							}
};

MemoryCommitmentAllocateOnlyTestInstance::MemoryCommitmentAllocateOnlyTestInstance(Context& context)
	: vkt::TestInstance		(context)
{
}

tcu::TestStatus MemoryCommitmentAllocateOnlyTestInstance::iterate(void)
{
	const VkPhysicalDevice					physicalDevice			= m_context.getPhysicalDevice();
	const VkDevice							device					= m_context.getDevice();
	const InstanceInterface&				vki						= m_context.getInstanceInterface();
	const DeviceInterface&					vkd						= m_context.getDeviceInterface();
	const VkPhysicalDeviceMemoryProperties	pMemoryProperties		= getPhysicalDeviceMemoryProperties(vki,physicalDevice);
	const VkMemoryPropertyFlags				propertyFlag			= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
	const std::vector<deUint32>				memoryTypeIndices		= getMemoryTypeIndices(propertyFlag, pMemoryProperties);
	const int								arrayLength				= 10;
	VkDeviceSize							pCommittedMemoryInBytes = 0u;
	VkDeviceSize							allocSize[arrayLength];

	if (memoryTypeIndices.empty())
		TCU_THROW(NotSupportedError, "Lazily allocated bit is not supported by any memory type");

	// generating random allocation sizes
	for (int i = 0; i < arrayLength; ++i)
	{
		allocSize[i] = rand() % 1000 + 1;
	}

	for (const auto memoryTypeIndex : memoryTypeIndices)
	{
		for (int i = 0; i < arrayLength; ++i)
		{
			const VkMemoryAllocateInfo	memAllocInfo =
			{
				VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,	//	VkStructureType		sType
				NULL,									//	const void*			pNext
				allocSize[i],							//	VkDeviceSize		allocationSize
				memoryTypeIndex							//	deUint32			memoryTypeIndex
			};

			Move<VkDeviceMemory> memory = allocateMemory(vkd, device, &memAllocInfo, (const VkAllocationCallbacks*)DE_NULL);

			vkd.getDeviceMemoryCommitment(device, memory.get(), &pCommittedMemoryInBytes);
			if(pCommittedMemoryInBytes != 0)
			{
				tcu::TestLog& log = m_context.getTestContext().getLog();
				log << TestLog::Message << "Warning: Memory commitment not null before binding." << TestLog::EndMessage;
			}
			if(pCommittedMemoryInBytes > allocSize[i])
				return tcu::TestStatus::fail("Fail");

		}
	}
	return tcu::TestStatus::pass("Pass");
}

void MemoryCommitmentTestCase::initPrograms (SourceCollections& programCollection) const
{
	programCollection.glslSources.add("vert") << glu::VertexSource(
		"#version 310 es\n"
		"layout (location = 0) in highp vec4 a_position;\n"
		"void main()\n"
		"{\n"
		"	gl_Position = a_position;\n"
		"}\n");

	programCollection.glslSources.add("frag") << glu::FragmentSource(
		"#version 310 es\n"
		"#extension GL_EXT_texture_buffer : enable\n"
		"layout (set=0, binding=0) uniform highp usamplerBuffer u_buffer;\n"
		"layout (location = 0) out highp uint o_color;\n"
		"void main()\n"
		"{\n"
		"	o_color = texelFetch(u_buffer, int(gl_FragCoord.x)).x;\n"
		"}\n");
}

Move<VkCommandPool> MemoryCommitmentTestInstance::createCommandPool() const
{
	const VkDevice			device				= m_context.getDevice();
	const DeviceInterface&	vkd					= m_context.getDeviceInterface();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();

	return vk::createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
}

Move<VkCommandBuffer> MemoryCommitmentTestInstance::allocatePrimaryCommandBuffer (VkCommandPool commandPool) const
{
	const VkDevice						device					= m_context.getDevice();
	const DeviceInterface&				vkd						= m_context.getDeviceInterface();

	return vk::allocateCommandBuffer(vkd, device, commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
}

bool MemoryCommitmentTestInstance::isDeviceMemoryCommitmentOk(const VkMemoryRequirements memoryRequirements)
{
	const VkFormat							colorFormat			= VK_FORMAT_R32_UINT;
	const VkPhysicalDevice					physicalDevice		= m_context.getPhysicalDevice();
	const InstanceInterface&				vki					= m_context.getInstanceInterface();
	const VkMemoryPropertyFlags				propertyFlag		= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
	const VkPhysicalDeviceMemoryProperties	pMemoryProperties	= getPhysicalDeviceMemoryProperties(vki,physicalDevice);
	const VkDeviceSize						pixelDataSize		= m_renderSize.x() * m_renderSize.y() * mapVkFormat(colorFormat).getPixelSize();

	for (deUint32 memTypeNdx = 0u; memTypeNdx < VK_MAX_MEMORY_TYPES; ++memTypeNdx)
	{
		if((pMemoryProperties.memoryTypes[memTypeNdx].propertyFlags & propertyFlag) == propertyFlag) //if supports Lazy allocation
		{
			const VkMemoryAllocateInfo	memAllocInfo =
			{
				VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,		//	VkStructureType		sType
				NULL,										//	const void*			pNext
				pixelDataSize,								//	VkDeviceSize		allocationSize
				memTypeNdx									//	deUint32			memoryTypeIndex
			};
			const VkDevice			device					= m_context.getDevice();
			const DeviceInterface&	vkd						= m_context.getDeviceInterface();
			Move<VkDeviceMemory>	memory					= allocateMemory(vkd, device, &memAllocInfo, (const VkAllocationCallbacks*)DE_NULL);
			VkDeviceSize			pCommittedMemoryInBytes = 0u;
			vkd.getDeviceMemoryCommitment(device, memory.get(), &pCommittedMemoryInBytes);
			if(pCommittedMemoryInBytes <= memoryRequirements.size)
				return true;
		}
	}
	return false;
}

tcu::TestCaseGroup* createMemoryCommitmentTests (tcu::TestContext& testCtx)
{
	static const MemoryCommitmentCaseParams info =
	{
		2048u,	// deUint32	bufferSize
		256u,	// deUint32	bufferViewSize
		0u,		// deUint32	elementOffset
	};

	de::MovePtr<tcu::TestCaseGroup>	getMemoryCommitmentTests	(new tcu::TestCaseGroup(testCtx, "get_memory_commitment"));

	{
		getMemoryCommitmentTests->addChild(new MemoryCommitmentTestCase(testCtx, "memory_commitment", info));
		getMemoryCommitmentTests->addChild(new MemoryCommitmentAllocateOnlyTestCase(testCtx, "memory_commitment_allocate_only"));
	}

	return getMemoryCommitmentTests.release();
}

} //api
} //vkt
