/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Valve Corporation.
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
 * \brief Tests vkCmdClearAttachments with unused attachments.
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassMultipleSubpassesMultipleCommandBuffersTests.hpp"
#include "pipeline/vktPipelineImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkImageUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include <sstream>
#include <functional>
#include <vector>
#include <string>
#include <memory>

namespace vkt
{
namespace renderpass
{

namespace
{

struct Vertex
{
	tcu::Vec4	position;
	tcu::Vec4	color;
};

template<typename T>
inline VkDeviceSize sizeInBytes(const std::vector<T>& vec)
{
	return vec.size() * sizeof(vec[0]);
}

std::vector<Vertex> genVertices (void)
{
	std::vector<Vertex>		vectorData;
	const tcu::Vec4			red			= {1.0f, 0.0f, 0.0f, 1.0f};
	const tcu::Vec4			green		= {0.0f, 1.0f, 0.0f, 1.0f};
	const tcu::Vec4			blue		= {0.0f, 0.0f, 1.0f, 1.0f};
	const tcu::Vec4			yellow		= {1.0f, 1.0f, 0.0f, 1.0f};

	vectorData.push_back({tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f) , red});
	vectorData.push_back({tcu::Vec4( 0.0f, -1.0f, 0.0f, 1.0f) , red});
	vectorData.push_back({tcu::Vec4(-1.0f,  1.0f, 0.0f, 1.0f) , red});
	vectorData.push_back({tcu::Vec4( 0.0f,  1.0f, 0.0f, 1.0f) , red});

	vectorData.push_back({tcu::Vec4( 0.0f, -1.0f, 0.0f, 1.0f) , green});
	vectorData.push_back({tcu::Vec4( 1.0f, -1.0f, 0.0f, 1.0f) , green});
	vectorData.push_back({tcu::Vec4( 0.0f,  1.0f, 0.0f, 1.0f) , green});
	vectorData.push_back({tcu::Vec4( 1.0f,  1.0f, 0.0f, 1.0f) , green});

	vectorData.push_back({tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f) , blue});
	vectorData.push_back({tcu::Vec4( 0.0f, -1.0f, 0.0f, 1.0f) , blue});
	vectorData.push_back({tcu::Vec4(-1.0f,  1.0f, 0.0f, 1.0f) , blue});
	vectorData.push_back({tcu::Vec4( 0.0f,  1.0f, 0.0f, 1.0f) , blue});

	vectorData.push_back({tcu::Vec4( 0.0f, -1.0f, 0.0f, 1.0f) , yellow});
	vectorData.push_back({tcu::Vec4( 1.0f, -1.0f, 0.0f, 1.0f) , yellow});
	vectorData.push_back({tcu::Vec4( 0.0f,  1.0f, 0.0f, 1.0f) , yellow});
	vectorData.push_back({tcu::Vec4( 1.0f,  1.0f, 0.0f, 1.0f) , yellow});

	return vectorData;
}

class MultipleSubpassesMultipleCommandBuffersTestInstance : public TestInstance
{
public:
											MultipleSubpassesMultipleCommandBuffersTestInstance	(Context&			context);
	virtual									~MultipleSubpassesMultipleCommandBuffersTestInstance	(void) {}
	virtual tcu::TestStatus					iterate								(void);
	void									createCommandBuffer					(const DeviceInterface&	vk,
																				 VkDevice				vkDevice);
private:
	static constexpr deUint32				kImageWidth			= 32;
	static constexpr deUint32				kImageHeight		= 32;
	const tcu::UVec2						m_renderSize		= { kImageWidth, kImageHeight };

	VkClearValue							m_initialColor;
	VkClearValue							m_clearColor;

	Move<VkImage>							m_colorImageA;
	de::MovePtr<Allocation>					m_colorImageAllocA;
	Move<VkImageView>						m_colorAttachmentViewA;

	Move<VkImage>							m_colorImageB;
	de::MovePtr<Allocation>					m_colorImageAllocB;
	Move<VkImageView>						m_colorAttachmentViewB;

	Move<VkRenderPass>						m_renderPass;
	Move<VkFramebuffer>						m_framebufferA;
	Move<VkFramebuffer>						m_framebufferB;
	Move<VkShaderModule>					m_vertexShaderModule;
	Move<VkShaderModule>					m_fragmentShaderModule;
	Move<VkDescriptorSetLayout>				m_descriptorSetLayout;
	Move<VkPipelineLayout>					m_pipelineLayout;
	Move<VkPipeline>						m_graphicsPipeline0;
	Move<VkPipeline>						m_graphicsPipeline1;
	Move<VkPipeline>						m_graphicsPipeline2;
	Move<VkCommandPool>						m_cmdPool;
	Move<VkCommandBuffer>					m_cmdBufferA;
	Move<VkCommandBuffer>					m_cmdBufferB;

	Move<VkBuffer>							m_vertexBuffer;
	de::MovePtr<Allocation>					m_vertexBufferAlloc;
};

class MultipleSubpassesMultipleCommandBuffersTest : public vkt::TestCase
{
public:
										MultipleSubpassesMultipleCommandBuffersTest	(tcu::TestContext&	testContext,
																	 const std::string&	name,
																	 const std::string&	description)
											: vkt::TestCase(testContext, name, description)
											{}
	virtual								~MultipleSubpassesMultipleCommandBuffersTest	(void) {}
	virtual void						initPrograms				(SourceCollections&	sourceCollections) const;
	virtual TestInstance*				createInstance				(Context&			context) const;
};

TestInstance* MultipleSubpassesMultipleCommandBuffersTest::createInstance (Context& context) const
{
	return new MultipleSubpassesMultipleCommandBuffersTestInstance(context);
}

void MultipleSubpassesMultipleCommandBuffersTest::initPrograms (SourceCollections& sourceCollections) const
{
	// Vertex shader.
	sourceCollections.glslSources.add("vert_shader") << glu::VertexSource(
		"#version 450\n"
		"layout(location = 0) in vec4 position;\n"
		"layout(location = 1) in vec4 color;\n"
		"layout(location = 0) out vec4 vtxColor;\n"
		"void main (void)\n"
		"{\n"
		"\tgl_Position = position;\n"
		"\tvtxColor = color;\n"
		"}\n");

	// Fragment shader.
	std::ostringstream fragmentSource;

	fragmentSource	<< "#version 450\n"
					<< "layout(location = 0) in vec4 vtxColor;\n"
					<< "layout(location = 0) out vec4 fragColor;\n"
					<< "void main (void)\n"
					<< "{\n"
					<< "\tfragColor = vtxColor;\n"
					<< "}\n";

	sourceCollections.glslSources.add("frag_shader") << glu::FragmentSource(fragmentSource.str());
}

// Create a render pass for this use case.
Move<VkRenderPass> createRenderPass (const DeviceInterface&	vk, VkDevice vkDevice)
{
	// Create attachment descriptions.
	const VkAttachmentDescription		attachmentDescription	=
	{
		(VkAttachmentDescriptionFlags)0,			// VkAttachmentDescriptionFlags		flags
		VK_FORMAT_R32G32B32A32_SFLOAT,				// VkFormat							format
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits			samples
		VK_ATTACHMENT_LOAD_OP_LOAD,					// VkAttachmentLoadOp				loadOp
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp				storeOp
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp				stencilLoadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp				stencilStoreOp
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout					initialLayout
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout					finalLayout
	};

	// Mark attachments as used or not depending on the test parameters.
	const VkAttachmentReference			attachmentReference
	{
		0u,																	// deUint32				attachment
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,							// VkImageLayout		layout
	};

	// Create subpass description with the previous color attachment references.
	std::vector<vk::VkSubpassDescription> subpassDescriptions;
	{
		const vk::VkSubpassDescription			subpassDescription =
		{
			(VkSubpassDescriptionFlags)0,											// VkSubpassDescriptionFlags		flags
			VK_PIPELINE_BIND_POINT_GRAPHICS,										// VkPipelineBindPoint				pipelineBindPoint
			0u,																		// deUint32							inputAttachmentCount
			DE_NULL,																// const VkAttachmentReference*		pInputAttachments
			1u,																		// deUint32							colorAttachmentCount
			&attachmentReference,													// const VkAttachmentReference*		pColorAttachments
			DE_NULL,																// const VkAttachmentReference*		pResolveAttachments
			DE_NULL,																// const VkAttachmentReference*		pDepthStencilAttachment
			0u,																		// deUint32							preserveAttachmentCount
			DE_NULL																	// const deUint32*					pPreserveAttachments
		};
		subpassDescriptions.emplace_back(subpassDescription);
		subpassDescriptions.emplace_back(subpassDescription);
		subpassDescriptions.emplace_back(subpassDescription);
	}

	std::vector<vk::VkSubpassDependency> subpassDependencies;
	{
		vk::VkSubpassDependency			subpassDependency =
		{
			0u,												// deUint32				srcSubpass
			1u,												// deUint32					dstSubpass
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,	// VkPipelineStageFlags		srcStageMask
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,	// VkPipelineStageFlags		dstStageMask
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// VkAccessFlags			srcAccessMask
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// VkAccessFlags			dstAccessMask
			0u												// VkDependencyFlags		dependencyFlags
		};
		subpassDependencies.emplace_back(subpassDependency);
		subpassDependency.srcSubpass = 1u;
		subpassDependency.dstSubpass = 2u;
		subpassDependencies.emplace_back(subpassDependency);
	}


	const vk::VkRenderPassCreateInfo	renderPassInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,								// VkStructureType					sType
		DE_NULL,																// const void*						pNext
		(VkRenderPassCreateFlags)0,												// VkRenderPassCreateFlags			flags
		1u,																		// deUint32							attachmentCount
		&attachmentDescription,													// const VkAttachmentDescription*	pAttachments
		static_cast<deUint32>(subpassDescriptions.size()),						// deUint32							subpassCount
		subpassDescriptions.data(),												// const VkSubpassDescription*		pSubpasses
		static_cast<deUint32>(subpassDependencies.size()),						// deUint32							dependencyCount
		subpassDependencies.data(),												// const VkSubpassDependency*		pDependencies
	};

	return createRenderPass(vk, vkDevice, &renderPassInfo);
}

MultipleSubpassesMultipleCommandBuffersTestInstance::MultipleSubpassesMultipleCommandBuffersTestInstance(Context&	context)
	: vkt::TestInstance(context)
{
	// Initial color for all images.
	m_initialColor.color.float32[0] = 0.0f;
	m_initialColor.color.float32[1] = 0.0f;
	m_initialColor.color.float32[2] = 0.0f;
	m_initialColor.color.float32[3] = 1.0f;

	// Clear color for used attachments.
	m_clearColor.color.float32[0] = 1.0f;
	m_clearColor.color.float32[1] = 1.0f;
	m_clearColor.color.float32[2] = 1.0f;
	m_clearColor.color.float32[3] = 1.0f;

	const DeviceInterface&		vk						= m_context.getDeviceInterface();
	const VkDevice				vkDevice				= m_context.getDevice();
	const deUint32				queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	SimpleAllocator				memAlloc				(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
	const VkComponentMapping	componentMapping		= { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };

	// Create color images.
	{
		const VkImageCreateInfo	colorImageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,									// VkStructureType			sType;
			DE_NULL,																// const void*				pNext;
			0u,																		// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,														// VkImageType				imageType;
			VK_FORMAT_R32G32B32A32_SFLOAT,											// VkFormat					format;
			{ kImageWidth, kImageHeight, 1u },										// VkExtent3D				extent;
			1u,																		// deUint32					mipLevels;
			1u,																		// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,													// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,												// VkImageTiling			tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
				| VK_IMAGE_USAGE_TRANSFER_DST_BIT,									// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,												// VkSharingMode			sharingMode;
			1u,																		// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,														// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED												// VkImageLayout			initialLayout;
		};
		// Create, allocate and bind image memory.
		m_colorImageA = createImage(vk, vkDevice, &colorImageParams);
		m_colorImageAllocA = memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImageA), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImageA, m_colorImageAllocA->getMemory(), m_colorImageAllocA->getOffset()));

		m_colorImageB = createImage(vk, vkDevice, &colorImageParams);
		m_colorImageAllocB = memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImageB), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImageB, m_colorImageAllocB->getMemory(), m_colorImageAllocB->getOffset()));

		// Create image view.
		{
			const VkImageViewCreateInfo colorAttachmentViewParamsA =
			{
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,			// VkStructureType			sType;
				DE_NULL,											// const void*				pNext;
				0u,													// VkImageViewCreateFlags	flags;
				*m_colorImageA,										// VkImage					image;
				VK_IMAGE_VIEW_TYPE_2D,								// VkImageViewType			viewType;
				VK_FORMAT_R32G32B32A32_SFLOAT,						// VkFormat					format;
				componentMapping,									// VkChannelMapping			channels;
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }		// VkImageSubresourceRange	subresourceRange;
			};
			m_colorAttachmentViewA = createImageView(vk, vkDevice, &colorAttachmentViewParamsA);

			const VkImageViewCreateInfo colorAttachmentViewParamsB =
			{
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,			// VkStructureType			sType;
				DE_NULL,											// const void*				pNext;
				0u,													// VkImageViewCreateFlags	flags;
				*m_colorImageB,										// VkImage					image;
				VK_IMAGE_VIEW_TYPE_2D,								// VkImageViewType			viewType;
				VK_FORMAT_R32G32B32A32_SFLOAT,						// VkFormat					format;
				componentMapping,									// VkChannelMapping			channels;
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }		// VkImageSubresourceRange	subresourceRange;
			};
			m_colorAttachmentViewB = createImageView(vk, vkDevice, &colorAttachmentViewParamsB);
		}

		// Clear image and leave it prepared to be used as a color attachment.
		{
			const VkImageAspectFlags		aspectMask	= VK_IMAGE_ASPECT_COLOR_BIT;
			Move<VkCommandPool>				cmdPool;
			Move<VkCommandBuffer>			cmdBuffer;
			std::vector<VkImageMemoryBarrier> preImageBarriers;
			std::vector<VkImageMemoryBarrier> postImageBarriers;

			// Create command pool and buffer
			cmdPool		= createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
			cmdBuffer	= allocateCommandBuffer(vk, vkDevice, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

			// From undefined layout to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL.
			const VkImageMemoryBarrier preImageBarrierA =
			{
				 VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,	// VkStructureType			sType;
				 DE_NULL,									// const void*				pNext;
				 0u,										// VkAccessFlags			srcAccessMask;
				 VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
				 VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout			oldLayout;
				 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			newLayout;
				 VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
				 VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
				 *m_colorImageA,							// VkImage					image;
				 {											// VkImageSubresourceRange	subresourceRange;
					aspectMask,								// VkImageAspect			aspect;
					0u,										// deUint32					baseMipLevel;
					1u,										// deUint32					mipLevels;
					0u,										// deUint32					baseArraySlice;
					1u										// deUint32					arraySize;
				 }
			};

			preImageBarriers.emplace_back(preImageBarrierA);

			// From VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL.
			const VkImageMemoryBarrier postImageBarrierA =
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
				DE_NULL,									// const void*				pNext;
				VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
				VK_ACCESS_SHADER_READ_BIT,					// VkAccessFlags			dstAccessMask;
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout			newLayout;
				VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
				*m_colorImageA,								// VkImage					image;
				{											// VkImageSubresourceRange	subresourceRange;
					aspectMask,								// VkImageAspect			aspect;
					0u,										// deUint32					baseMipLevel;
					1u,										// deUint32					mipLevels;
					0u,										// deUint32					baseArraySlice;
					1u										// deUint32					arraySize;
				}
			};

			postImageBarriers.emplace_back(postImageBarrierA);

			// From undefined layout to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL.
			const VkImageMemoryBarrier preImageBarrierB =
			{
				 VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,	// VkStructureType			sType;
				 DE_NULL,									// const void*				pNext;
				 0u,										// VkAccessFlags			srcAccessMask;
				 VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
				 VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout			oldLayout;
				 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			newLayout;
				 VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
				 VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
				 *m_colorImageB,							// VkImage					image;
				 {											// VkImageSubresourceRange	subresourceRange;
					aspectMask,								// VkImageAspect			aspect;
					0u,										// deUint32					baseMipLevel;
					1u,										// deUint32					mipLevels;
					0u,										// deUint32					baseArraySlice;
					1u										// deUint32					arraySize;
				 }
			};

			preImageBarriers.emplace_back(preImageBarrierB);

			// From VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL.
			const VkImageMemoryBarrier postImageBarrierB =
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
				DE_NULL,									// const void*				pNext;
				VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
				VK_ACCESS_SHADER_READ_BIT,					// VkAccessFlags			dstAccessMask;
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout			newLayout;
				VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
				*m_colorImageB,								// VkImage					image;
				{											// VkImageSubresourceRange	subresourceRange;
					aspectMask,								// VkImageAspect			aspect;
					0u,										// deUint32					baseMipLevel;
					1u,										// deUint32					mipLevels;
					0u,										// deUint32					baseArraySlice;
					1u										// deUint32					arraySize;
				}
			};

			postImageBarriers.emplace_back(postImageBarrierB);

			const VkImageSubresourceRange clearRange	=
			{
				aspectMask,	// VkImageAspectFlags	aspectMask;
				0u,			// deUint32				baseMipLevel;
				1u,			// deUint32				levelCount;
				0u,			// deUint32				baseArrayLayer;
				1u			// deUint32				layerCount;
			};

			// Clear image and transfer layout.
			beginCommandBuffer(vk, *cmdBuffer);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, static_cast<deUint32>(preImageBarriers.size()), preImageBarriers.data());
			vk.cmdClearColorImage(*cmdBuffer, *m_colorImageA, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &m_initialColor.color, 1, &clearRange);
			vk.cmdClearColorImage(*cmdBuffer, *m_colorImageB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &m_initialColor.color, 1, &clearRange);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, static_cast<deUint32>(postImageBarriers.size()), postImageBarriers.data());
			endCommandBuffer(vk, *cmdBuffer);

			submitCommandsAndWait(vk, vkDevice, m_context.getUniversalQueue(), cmdBuffer.get());
		}
	}

	// Create render pass.
	m_renderPass = createRenderPass(vk, vkDevice);

	// Create framebuffer
	{
		const VkImageView				attachmentBindInfosA[1]			=
		{
			*m_colorAttachmentViewA,
		};
		const VkFramebufferCreateInfo	framebufferParamsA	=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			0u,													// VkFramebufferCreateFlags	flags;
			*m_renderPass,										// VkRenderPass				renderPass;
			1u,													// deUint32					attachmentCount;
			attachmentBindInfosA,								// const VkImageView*		pAttachments;
			kImageWidth,										// deUint32					width;
			kImageHeight,										// deUint32					height;
			1u													// deUint32					layers;
		};

		m_framebufferA = createFramebuffer(vk, vkDevice, &framebufferParamsA);

		const VkImageView				attachmentBindInfosB[1]			=
		{
			*m_colorAttachmentViewB,
		};
		const VkFramebufferCreateInfo	framebufferParamsB	=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			0u,													// VkFramebufferCreateFlags	flags;
			*m_renderPass,										// VkRenderPass				renderPass;
			1u,													// deUint32					attachmentCount;
			attachmentBindInfosB,								// const VkImageView*		pAttachments;
			kImageWidth,										// deUint32					width;
			kImageHeight,										// deUint32					height;
			1u													// deUint32					layers;
		};

		m_framebufferB = createFramebuffer(vk, vkDevice, &framebufferParamsB);
	}

	// Create pipeline layout.
	{
		const VkDescriptorSetLayoutCreateInfo	descriptorSetLayoutParams	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	// VkStructureType						sType
			DE_NULL,												// const void*							pNext
			0u,														// VkDescriptorSetLayoutCreateFlags		flags
			0u,														// deUint32								bindingCount
			DE_NULL													// const VkDescriptorSetLayoutBinding*	pBindings
		};
		m_descriptorSetLayout = createDescriptorSetLayout(vk, vkDevice, &descriptorSetLayoutParams);

		const VkPipelineLayoutCreateInfo		pipelineLayoutParams		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			0u,													// VkPipelineLayoutCreateFlags		flags;
			1u,													// deUint32							setLayoutCount;
			&m_descriptorSetLayout.get(),						// const VkDescriptorSetLayout*		pSetLayouts;
			0u,													// deUint32							pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*		pPushConstantRanges;
		};

		m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// Create Vertex buffer
	{
		const std::vector<Vertex>		vertexValues		= genVertices();
		const VkDeviceSize				vertexBufferSize	= sizeInBytes(vertexValues);

		const vk::VkBufferCreateInfo	bufferCreateInfo	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,				// VkStructureType		sType
			DE_NULL,											// const void*			pNext
			0u,													// VkBufferCreateFlags	flags
			vertexBufferSize,									// VkDeviceSize			size
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,					// VkBufferUsageFlags	usage
			VK_SHARING_MODE_EXCLUSIVE,							// VkSharingMode		sharingMode
			1u,													// deUint32				queueFamilyIndexCount
			&queueFamilyIndex									// const deUint32*		pQueueFamilyIndices
		};

		m_vertexBuffer		= createBuffer(vk, vkDevice, &bufferCreateInfo);
		m_vertexBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_vertexBuffer), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset()));
		// Load vertices into vertex buffer
		deMemcpy(m_vertexBufferAlloc->getHostPtr(), vertexValues.data(), static_cast<size_t>(vertexBufferSize));
		flushAlloc(vk, vkDevice, *m_vertexBufferAlloc);
	}

	// Vertex buffer description
	const vk::VkVertexInputBindingDescription bindingDescription =
	{
		0u,													// deUint32				binding
		sizeof(Vertex),										// deUint32				stride
		VK_VERTEX_INPUT_RATE_VERTEX							// VkVertexInputRate	inputRate
	};

	std::vector<vk::VkVertexInputAttributeDescription> attributeDescriptions;
	{
		vk::VkVertexInputAttributeDescription attributeDescriptionVertex =
		{
			0u,									// deUint32		location
			0u,									// deUint32		binding
			VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat		format
			offsetof(Vertex, position)			// deUint32		offset
		};

		vk::VkVertexInputAttributeDescription attributeDescriptionColor =
		{
			1u,									// deUint32		location
			0u,									// deUint32		binding
			VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat		format
			offsetof(Vertex, color)				// deUint32		offset
		};
		attributeDescriptions.emplace_back(attributeDescriptionVertex);
		attributeDescriptions.emplace_back(attributeDescriptionColor);
	}

	const vk::VkPipelineVertexInputStateCreateInfo vertexInputState =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType
		DE_NULL,														// const void*								pNext
		0u,																// VkPipelineVertexInputStateCreateFlags	flags
		1u,																// deUint32									vertexBindingDescriptionCount
		&bindingDescription,											// const VkVertexInputBindingDescription*	pVertexBindingDescriptions
		static_cast<deUint32>(attributeDescriptions.size()),			// deUint32									vertexAttributeDescriptionCount
		attributeDescriptions.data(),									// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions
	};

	m_vertexShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("vert_shader"), 0);
	m_fragmentShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("frag_shader"), 0);

	// Create pipeline.
	{
		const std::vector<VkViewport>						viewports						(1, makeViewport(m_renderSize));
		const std::vector<VkRect2D>							scissors						(1, makeRect2D(m_renderSize));

		const VkPipelineColorBlendAttachmentState			colorBlendAttachmentState		=
		{
			VK_FALSE,					// VkBool32					blendEnable
			VK_BLEND_FACTOR_ZERO,		// VkBlendFactor			srcColorBlendFactor
			VK_BLEND_FACTOR_ZERO,		// VkBlendFactor			dstColorBlendFactor
			VK_BLEND_OP_ADD,			// VkBlendOp				colorBlendOp
			VK_BLEND_FACTOR_ZERO,		// VkBlendFactor			srcAlphaBlendFactor
			VK_BLEND_FACTOR_ZERO,		// VkBlendFactor			dstAlphaBlendFactor
			VK_BLEND_OP_ADD,			// VkBlendOp				alphaBlendOp
			VK_COLOR_COMPONENT_R_BIT	// VkColorComponentFlags	colorWriteMask
			| VK_COLOR_COMPONENT_G_BIT
			| VK_COLOR_COMPONENT_B_BIT
			| VK_COLOR_COMPONENT_A_BIT
		};

		const VkPipelineColorBlendStateCreateInfo			colorBlendStateCreateInfo		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,							// VkStructureType								sType
			DE_NULL,																			// const void*									pNext
			0u,																					// VkPipelineColorBlendStateCreateFlags			flags
			VK_FALSE,																			// VkBool32										logicOpEnable
			VK_LOGIC_OP_CLEAR,																	// VkLogicOp									logicOp
		    1u,																					// deUint32										attachmentCount
		    &colorBlendAttachmentState,															// const VkPipelineColorBlendAttachmentState*	pAttachments
			{ 0.0f, 0.0f, 0.0f, 0.0f }															// float										blendConstants[4]
		};

		m_graphicsPipeline0 = makeGraphicsPipeline(vk,									// const DeviceInterface&							vk
												   vkDevice,								// const VkDevice									device
												   *m_pipelineLayout,					// const VkPipelineLayout							pipelineLayout
												   *m_vertexShaderModule,				// const VkShaderModule								vertexShaderModule
												   DE_NULL,								// const VkShaderModule								tessellationControlModule
												   DE_NULL,								// const VkShaderModule								tessellationEvalModule
												   DE_NULL,								// const VkShaderModule								geometryShaderModule
												   *m_fragmentShaderModule,				// const VkShaderModule								fragmentShaderModule
												   *m_renderPass,						// const VkRenderPass								renderPass
												   viewports,							// const std::vector<VkViewport>&					viewports
												   scissors,								// const std::vector<VkRect2D>&						scissors
												   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,	// const VkPrimitiveTopology						topology
												   0u,									// const deUint32									subpass
												   0u,									// const deUint32									patchControlPoints
												   &vertexInputState,					// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
												   DE_NULL,								// const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
												   DE_NULL,								// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
												   DE_NULL,								// const VkPipelineDepthStencilStateCreateInfo*		depthStencilStateCreateInfo
												   &colorBlendStateCreateInfo);			// const VkPipelineColorBlendStateCreateInfo*		colorBlendStateCreateInfo

		m_graphicsPipeline1 = makeGraphicsPipeline(vk,									// const DeviceInterface&							vk
												   vkDevice,								// const VkDevice									device
												   *m_pipelineLayout,					// const VkPipelineLayout							pipelineLayout
												   *m_vertexShaderModule,				// const VkShaderModule								vertexShaderModule
												   DE_NULL,								// const VkShaderModule								tessellationControlModule
												   DE_NULL,								// const VkShaderModule								tessellationEvalModule
												   DE_NULL,								// const VkShaderModule								geometryShaderModule
												   *m_fragmentShaderModule,				// const VkShaderModule								fragmentShaderModule
												   *m_renderPass,						// const VkRenderPass								renderPass
												   viewports,							// const std::vector<VkViewport>&					viewports
												   scissors,								// const std::vector<VkRect2D>&						scissors
												   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,	// const VkPrimitiveTopology						topology
												   1u,									// const deUint32									subpass
												   0u,									// const deUint32									patchControlPoints
												   &vertexInputState,					// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
												   DE_NULL,								// const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
												   DE_NULL,								// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
												   DE_NULL,								// const VkPipelineDepthStencilStateCreateInfo*		depthStencilStateCreateInfo
												   &colorBlendStateCreateInfo);			// const VkPipelineColorBlendStateCreateInfo*		colorBlendStateCreateInfo

		m_graphicsPipeline2 = makeGraphicsPipeline(vk,									// const DeviceInterface&							vk
												   vkDevice,								// const VkDevice									device
												   *m_pipelineLayout,					// const VkPipelineLayout							pipelineLayout
												   *m_vertexShaderModule,				// const VkShaderModule								vertexShaderModule
												   DE_NULL,								// const VkShaderModule								tessellationControlModule
												   DE_NULL,								// const VkShaderModule								tessellationEvalModule
												   DE_NULL,								// const VkShaderModule								geometryShaderModule
												   *m_fragmentShaderModule,				// const VkShaderModule								fragmentShaderModule
												   *m_renderPass,						// const VkRenderPass								renderPass
												   viewports,							// const std::vector<VkViewport>&					viewports
												   scissors,								// const std::vector<VkRect2D>&						scissors
												   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,	// const VkPrimitiveTopology						topology
												   2u,									// const deUint32									subpass
												   0u,									// const deUint32									patchControlPoints
												   &vertexInputState,					// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
												   DE_NULL,								// const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
												   DE_NULL,								// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
												   DE_NULL,								// const VkPipelineDepthStencilStateCreateInfo*		depthStencilStateCreateInfo
												   &colorBlendStateCreateInfo);			// const VkPipelineColorBlendStateCreateInfo*		colorBlendStateCreateInfo

	}

	// Create command pool
	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

	// Create command buffer
	createCommandBuffer(vk, vkDevice);
}

void MultipleSubpassesMultipleCommandBuffersTestInstance::createCommandBuffer (const DeviceInterface&	vk,
																		  VkDevice					vkDevice)
{
	const VkRenderPassBeginInfo							renderPassBeginInfoA	=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		*m_renderPass,								// VkRenderPass			renderPass;
		*m_framebufferA,							// VkFramebuffer		framebuffer;
		makeRect2D(m_renderSize),					// VkRect2D				renderArea;
		0u,											// uint32_t				clearValueCount;
		DE_NULL										// const VkClearValue*	pClearValues;
	};
	const VkRenderPassBeginInfo							renderPassBeginInfoB	=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		*m_renderPass,								// VkRenderPass			renderPass;
		*m_framebufferB,							// VkFramebuffer		framebuffer;
		makeRect2D(m_renderSize),					// VkRect2D				renderArea;
		0u,											// uint32_t				clearValueCount;
		DE_NULL										// const VkClearValue*	pClearValues;
	};

	m_cmdBufferA = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	m_cmdBufferB = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	const VkClearRect									clearRect			=
	{
		{												// VkRect2D		rect;
			{ 0, 0, },									//	VkOffset2D	offset;
			{ kImageWidth, kImageHeight }				//	VkExtent2D	extent;
		},
		0u,												// uint32_t		baseArrayLayer;
		1u												// uint32_t		layerCount;
	};

	const VkClearAttachment clearAttachment =
	{
		VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
		0u,							// uint32_t				colorAttachment;
		m_clearColor				// VkClearValue			clearValue;
	};

	VkDeviceSize	vertexBufferOffset	= 0u;

	// Command Buffer A will set its own event but wait for the B's event before continuing to the next subpass.
	beginCommandBuffer(vk, *m_cmdBufferA, 0u);
	beginCommandBuffer(vk, *m_cmdBufferB, 0u);
		vk.cmdBeginRenderPass(*m_cmdBufferA, &renderPassBeginInfoA, VK_SUBPASS_CONTENTS_INLINE);
		vk.cmdBindPipeline(*m_cmdBufferA, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipeline0);
		vk.cmdBindVertexBuffers(*m_cmdBufferA, 0u, 1u, &m_vertexBuffer.get(), &vertexBufferOffset);
		vk.cmdClearAttachments(*m_cmdBufferA, 1u, &clearAttachment, 1u, &clearRect);

		vk.cmdBeginRenderPass(*m_cmdBufferB, &renderPassBeginInfoB, VK_SUBPASS_CONTENTS_INLINE);
		vk.cmdBindPipeline(*m_cmdBufferB, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipeline0);
		vk.cmdClearAttachments(*m_cmdBufferB, 1u, &clearAttachment, 1u, &clearRect);
		vk.cmdNextSubpass(*m_cmdBufferB, VK_SUBPASS_CONTENTS_INLINE);

		vk.cmdNextSubpass(*m_cmdBufferA, VK_SUBPASS_CONTENTS_INLINE);
		vk.cmdBindPipeline(*m_cmdBufferA, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipeline1);
		vk.cmdBindVertexBuffers(*m_cmdBufferA, 0u, 1u, &m_vertexBuffer.get(), &vertexBufferOffset);
		vk.cmdDraw(*m_cmdBufferA, 4u, 1u, 0u, 0u);

		vertexBufferOffset = 8 * sizeof(Vertex);
		vk.cmdBindPipeline(*m_cmdBufferB, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipeline1);
		vk.cmdBindVertexBuffers(*m_cmdBufferB, 0u, 1u, &m_vertexBuffer.get(), &vertexBufferOffset);
		vk.cmdDraw(*m_cmdBufferB, 4u, 1u, 0u, 0u);
		vk.cmdNextSubpass(*m_cmdBufferB, VK_SUBPASS_CONTENTS_INLINE);

		vertexBufferOffset = 0u;
		vk.cmdNextSubpass(*m_cmdBufferA, VK_SUBPASS_CONTENTS_INLINE);
		vk.cmdBindPipeline(*m_cmdBufferA, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipeline2);
		vk.cmdBindVertexBuffers(*m_cmdBufferA, 0u, 1u, &m_vertexBuffer.get(), &vertexBufferOffset);
		vk.cmdDraw(*m_cmdBufferA, 4u, 1u, 4u, 0u);

		vertexBufferOffset = 8 * sizeof(Vertex);
		vk.cmdBindPipeline(*m_cmdBufferB, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipeline2);
		vk.cmdDraw(*m_cmdBufferB, 4u, 1u, 4u, 0u);
		vk.cmdEndRenderPass(*m_cmdBufferB);
		vk.cmdEndRenderPass(*m_cmdBufferA);
	endCommandBuffer(vk, *m_cmdBufferA);
	endCommandBuffer(vk, *m_cmdBufferB);
}

tcu::TestStatus	MultipleSubpassesMultipleCommandBuffersTestInstance::iterate (void)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			vkDevice			= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	SimpleAllocator			allocator			(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));

	{
		const Unique<VkFence>				fence				(createFence(vk, vkDevice));
		std::vector<VkCommandBuffer>	commandBuffers;
		commandBuffers.emplace_back(m_cmdBufferA.get());
		commandBuffers.emplace_back(m_cmdBufferB.get());

		const VkSubmitInfo		submitInfo				=
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,						// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			0u,													// deUint32						waitSemaphoreCount;
			DE_NULL,											// const VkSemaphore*			pWaitSemaphores;
			(const VkPipelineStageFlags*)DE_NULL,				// const VkPipelineStageFlags*	pWaitDstStageMask;
			static_cast<deUint32>(commandBuffers.size()),		// deUint32						commandBufferCount;
			commandBuffers.data(),								// const VkCommandBuffer*		pCommandBuffers;
			0u,													// deUint32						signalSemaphoreCount;
			DE_NULL,											// const VkSemaphore*			pSignalSemaphores;
		};

		VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, *fence));
		VK_CHECK(vk.waitForFences(vkDevice, 1u, &fence.get(), DE_TRUE, ~0ull));
	}

	{
		// Colors to compare to.
		const tcu::Vec4			red			= {1.0f, 0.0f, 0.0f, 1.0f};
		const tcu::Vec4			green		= {0.0f, 1.0f, 0.0f, 1.0f};
		const tcu::Vec4			blue		= {0.0f, 0.0f, 1.0f, 1.0f};
		const tcu::Vec4			yellow		= {1.0f, 1.0f, 0.0f, 1.0f};

		// Read result images.
		de::MovePtr<tcu::TextureLevel> imagePixelsA = pipeline::readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *m_colorImageA, VK_FORMAT_R32G32B32A32_SFLOAT, m_renderSize);
		de::MovePtr<tcu::TextureLevel> imagePixelsB = pipeline::readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *m_colorImageB, VK_FORMAT_R32G32B32A32_SFLOAT, m_renderSize);

		// Verify pixel colors match.
		const tcu::ConstPixelBufferAccess&	imageAccessA		= imagePixelsA->getAccess();
		const tcu::ConstPixelBufferAccess&	imageAccessB		= imagePixelsB->getAccess();


		tcu::TextureLevel	referenceImageA(mapVkFormat(VK_FORMAT_R32G32B32A32_SFLOAT), m_renderSize.x(), m_renderSize.y());
		tcu::TextureLevel	referenceImageB(mapVkFormat(VK_FORMAT_R32G32B32A32_SFLOAT), m_renderSize.x(), m_renderSize.y());

		tcu::clear(tcu::getSubregion(referenceImageA.getAccess(), 0u, 0u,
									 imageAccessA.getWidth() / 2, imageAccessA.getHeight()),
					   red);
		tcu::clear(tcu::getSubregion(referenceImageA.getAccess(), imageAccessA.getWidth() / 2, 0u,
									 imageAccessA.getWidth() / 2, imageAccessA.getHeight()),
				   green);

		if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", referenceImageA.getAccess(), imageAccessA, tcu::Vec4(0.02f), tcu::COMPARE_LOG_RESULT))
			TCU_FAIL("[A] Rendered image is not correct");

		tcu::clear(tcu::getSubregion(referenceImageB.getAccess(), 0u, 0u,
									 imageAccessB.getWidth() / 2, imageAccessB.getHeight()),
					   blue);
		tcu::clear(tcu::getSubregion(referenceImageB.getAccess(), imageAccessB.getWidth() / 2, 0u,
									 imageAccessA.getWidth() / 2, imageAccessB.getHeight()),
				   yellow);

		if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", referenceImageB.getAccess(), imageAccessB, tcu::Vec4(0.02f), tcu::COMPARE_LOG_RESULT))
			TCU_FAIL("[B] Rendered image is not correct");

	}

	return tcu::TestStatus::pass("Pass");
}
} // anonymous

tcu::TestCaseGroup* createRenderPassMultipleSubpassesMultipleCommandBuffersTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	testGroup (new tcu::TestCaseGroup(testCtx, "multiple_subpasses_multiple_command_buffers", "Multiple subpasses multiple command buffers"));

	testGroup->addChild(new MultipleSubpassesMultipleCommandBuffersTest(testCtx, "test", ""));

	return testGroup.release();
}

} // renderpass
} // vkt
