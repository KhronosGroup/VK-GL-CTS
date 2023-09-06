/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Google Inc.
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
 * \brief Fragment Operations Occlusion Query Tests
 *//*--------------------------------------------------------------------*/

#include "vktFragmentOperationsOcclusionQueryTests.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deMath.h"

#include <string>

namespace vkt
{
namespace FragmentOperations
{
namespace
{
using namespace vk;
using de::UniquePtr;

//! Basic 2D image.
inline VkImageCreateInfo makeImageCreateInfo (const tcu::IVec2& size, const VkFormat format, const VkImageUsageFlags usage)
{
	const VkImageCreateInfo imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		(VkImageCreateFlags)0,					// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						// VkImageType				imageType;
		format,									// VkFormat					format;
		makeExtent3D(size.x(), size.y(), 1),	// VkExtent3D				extent;
		1u,										// deUint32					mipLevels;
		1u,										// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
		usage,									// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
		0u,										// deUint32					queueFamilyIndexCount;
		DE_NULL,								// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			initialLayout;
	};

	return imageParams;
}

Move<VkRenderPass> makeRenderPass	(const DeviceInterface&	vk,
									 const VkDevice			device,
									 const VkFormat			colorFormat,
									 const bool				useDepthStencilAttachment,
									 const VkFormat			depthStencilFormat)
{
	return makeRenderPass(vk, device, colorFormat, useDepthStencilAttachment ? depthStencilFormat : VK_FORMAT_UNDEFINED);
}

Move<VkPipeline> makeGraphicsPipeline	(const DeviceInterface&	vk,
										 const VkDevice			device,
										 const VkPipelineLayout	pipelineLayout,
										 const VkRenderPass		renderPass,
										 const VkShaderModule	vertexModule,
										 const VkShaderModule	fragmentModule,
										 const tcu::IVec2&		renderSize,
										 const bool				enableScissorTest,
										 const bool				enableDepthTest,
										 const bool				enableStencilTest,
										 const bool				enableStencilWrite)
{
	const std::vector<VkViewport>			viewports					(1, makeViewport(renderSize));
	const std::vector<VkRect2D>				scissors					(1, enableScissorTest
																		? makeRect2D(renderSize.x() / 4, renderSize.y() / 4, renderSize.x() / 4 * 2, renderSize.y() / 4 * 2)
																		: makeRect2D(renderSize));

	const VkStencilOpState					stencilOpState				= makeStencilOpState(VK_STENCIL_OP_KEEP,												// stencil fail
																							 enableStencilWrite ? VK_STENCIL_OP_REPLACE : VK_STENCIL_OP_KEEP,	// depth & stencil pass
																							 VK_STENCIL_OP_KEEP,												// depth only fail
																							 enableStencilWrite ? VK_COMPARE_OP_ALWAYS : VK_COMPARE_OP_EQUAL,	// compare op
																							 0xff,																// compare mask
																							 0xff,																// write mask
																							 enableStencilWrite ? 0u : 1u);										// reference

	VkPipelineDepthStencilStateCreateInfo	depthStencilStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,			// VkStructureType							sType
		DE_NULL,															// const void*								pNext
		0u,																	// VkPipelineDepthStencilStateCreateFlags	flags
		enableDepthTest ? VK_TRUE : VK_FALSE,								// VkBool32									depthTestEnable
		enableDepthTest ? VK_TRUE : VK_FALSE,								// VkBool32									depthWriteEnable
		VK_COMPARE_OP_LESS,													// VkCompareOp								depthCompareOp
		VK_FALSE,															// VkBool32									depthBoundsTestEnable
		enableStencilTest ? VK_TRUE : VK_FALSE,								// VkBool32									stencilTestEnable
		enableStencilTest ? stencilOpState : VkStencilOpState{},			// VkStencilOpState							front
		enableStencilTest ? stencilOpState : VkStencilOpState{},			// VkStencilOpState							back
		0.0f,																// float									minDepthBounds
		1.0f																// float									maxDepthBounds
	};

	return vk::makeGraphicsPipeline(vk,										// const DeviceInterface&							vk
									device,									// const VkDevice									device
									pipelineLayout,							// const VkPipelineLayout							pipelineLayout
									vertexModule,							// const VkShaderModule								vertexShaderModule
									DE_NULL,								// const VkShaderModule								tessellationControlModule
									DE_NULL,								// const VkShaderModule								tessellationEvalModule
									DE_NULL,								// const VkShaderModule								geometryShaderModule
									fragmentModule,							// const VkShaderModule								fragmentShaderModule
									renderPass,								// const VkRenderPass								renderPass
									viewports,								// const std::vector<VkViewport>&					viewports
									scissors,								// const std::vector<VkRect2D>&						scissors
									VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// const VkPrimitiveTopology						topology
									0u,										// const deUint32									subpass
									0u,										// const deUint32									patchControlPoints
									DE_NULL,								// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
									DE_NULL,								// const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
									DE_NULL,								// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
									&depthStencilStateCreateInfo);			// const VkPipelineDepthStencilStateCreateInfo*		depthStencilStateCreateInfo
}

void commandClearDepthAttachment	(const DeviceInterface&	vk,
									 const VkCommandBuffer	commandBuffer,
									 const VkOffset2D&		offset,
									 const VkExtent2D&		extent,
									 const deUint32			clearValue)
{
	const VkClearAttachment	depthAttachment	=
	{
		VK_IMAGE_ASPECT_DEPTH_BIT,						// VkImageAspectFlags	aspectMask;
		0u,												// uint32_t				colorAttachment;
		makeClearValueDepthStencil(0.0f, clearValue),	// VkClearValue			clearValue;
	};

	const VkClearRect		rect			=
	{
		{ offset, extent },		// VkRect2D		rect;
		0u,						// uint32_t		baseArrayLayer;
		1u,						// uint32_t		layerCount;
	};

	vk.cmdClearAttachments(commandBuffer, 1u, &depthAttachment, 1u, &rect);
}

void commandClearStencilAttachment (const DeviceInterface&	vk,
									const VkCommandBuffer	commandBuffer,
									const VkOffset2D&		offset,
									const VkExtent2D&		extent,
									const deUint32			clearValue)
{
	const VkClearAttachment	stencilAttachment	=
	{
		VK_IMAGE_ASPECT_STENCIL_BIT,					// VkImageAspectFlags	aspectMask;
		0u,												// uint32_t				colorAttachment;
		makeClearValueDepthStencil(0.0f, clearValue),	// VkClearValue			clearValue;
	};

	const VkClearRect		rect				=
	{
		{ offset, extent },		// VkRect2D		rect;
		0u,						// uint32_t		baseArrayLayer;
		1u,						// uint32_t		layerCount;
	};

	vk.cmdClearAttachments(commandBuffer, 1u, &stencilAttachment, 1u, &rect);
}

VkImageAspectFlags getImageAspectFlags (const VkFormat format)
{
	const tcu::TextureFormat tcuFormat = mapVkFormat(format);

	if		(tcuFormat.order == tcu::TextureFormat::DS)		return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	else if	(tcuFormat.order == tcu::TextureFormat::D)		return VK_IMAGE_ASPECT_DEPTH_BIT;
	else if	(tcuFormat.order == tcu::TextureFormat::S)		return VK_IMAGE_ASPECT_STENCIL_BIT;

	DE_ASSERT(false);
	return 0u;
}

bool isSupportedDepthStencilFormat (const InstanceInterface& instanceInterface, const VkPhysicalDevice device, const VkFormat format)
{
	VkFormatProperties formatProps;
	instanceInterface.getPhysicalDeviceFormatProperties(device, format, &formatProps);
	return (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
}

VkFormat pickSupportedDepthStencilFormat	(const InstanceInterface&	instanceInterface,
											 const VkPhysicalDevice		device)
{
	static const VkFormat dsFormats[] =
	{
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
	};

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(dsFormats); ++i)
		if (isSupportedDepthStencilFormat(instanceInterface, device, dsFormats[i]))
			return dsFormats[i];

	return VK_FORMAT_UNDEFINED;
}

enum Flags
{
	TEST_NO_FLAGS		= 0,
	TEST_SCISSOR		= 1u << 0,
	TEST_DEPTH_WRITE	= 1u << 1,
	TEST_DEPTH_CLEAR	= 1u << 2,
	TEST_STENCIL_WRITE	= 1u << 3,
	TEST_STENCIL_CLEAR	= 1u << 4,
	TEST_ALL			= 1u << 5,
	TEST_PRECISE_BIT	= 1u << 6
};

class OcclusionQueryTestInstance : public TestInstance
{
public:
							OcclusionQueryTestInstance	(Context& context,
														 const tcu::IVec2 renderSize,
														 const bool preciseBitEnabled,
														 const bool scissorTestEnabled,
														 const bool depthTestEnabled,
														 const bool stencilTestEnabled,
														 const bool depthWriteEnabled,
														 const bool stencilWriteEnabled);

	tcu::TestStatus			iterate						(void);

private:
	const tcu::IVec2		m_renderSize;
	const bool				m_preciseBitEnabled;
	const bool				m_scissorTestEnabled;
	const bool				m_depthClearTestEnabled;
	const bool				m_stencilClearTestEnabled;
	const bool				m_depthWriteTestEnabled;
	const bool				m_stencilWriteTestEnabled;
};

OcclusionQueryTestInstance::OcclusionQueryTestInstance	(Context& context,
														 const tcu::IVec2 renderSize,
														 const bool preciseBitEnabled,
														 const bool scissorTestEnabled,
														 const bool depthClearTestEnabled,
														 const bool stencilClearTestEnabled,
														 const bool depthWriteTestEnabled,
														 const bool stencilWriteTestEnabled)
	: TestInstance				(context)
	, m_renderSize				(renderSize)
	, m_preciseBitEnabled		(preciseBitEnabled)
	, m_scissorTestEnabled		(scissorTestEnabled)
	, m_depthClearTestEnabled	(depthClearTestEnabled)
	, m_stencilClearTestEnabled	(stencilClearTestEnabled)
	, m_depthWriteTestEnabled	(depthWriteTestEnabled)
	, m_stencilWriteTestEnabled	(stencilWriteTestEnabled)
{
}

tcu::TestStatus OcclusionQueryTestInstance::iterate (void)
{
	const DeviceInterface&			vk						= m_context.getDeviceInterface();
	const InstanceInterface&		vki						= m_context.getInstanceInterface();
	const VkDevice					device					= m_context.getDevice();
	const VkPhysicalDevice			physDevice				= m_context.getPhysicalDevice();
	const VkQueue					queue					= m_context.getUniversalQueue();
	const deUint32					queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	Allocator&						allocator				= m_context.getDefaultAllocator();
	VkQueryPool						queryPool;
	const deUint32					queryCount				= 1u;
	std::vector<VkDeviceSize>		sampleCounts			(queryCount);

	// Create a query pool for storing the occlusion query result
	{
		VkQueryPoolCreateInfo queryPoolInfo
		{
			VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,	// VkStructureType					sType;
			DE_NULL,									// const void*						pNext;
			(VkQueryPoolCreateFlags)0,					// VkQueryPoolCreateFlags			flags;
			VK_QUERY_TYPE_OCCLUSION,					// VkQueryType						queryType;
			queryCount,									// uint32_t							queryCount;
			0u,											// VkQueryPipelineStatisticFlags	pipelineStatistics;
		};
		VK_CHECK(vk.createQueryPool(device, &queryPoolInfo, NULL, &queryPool));
	}

	// Color attachment
	const VkFormat					colorFormat				= VK_FORMAT_R8G8B8A8_UNORM;
	const VkImageSubresourceRange	colorSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const Unique<VkImage>			colorImage				(makeImage(vk, device, makeImageCreateInfo(m_renderSize, colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)));
	const UniquePtr<Allocation>		colorImageAlloc			(bindImage(vk, device, allocator, *colorImage, MemoryRequirement::Any));
	const Unique<VkImageView>		colorImageView			(makeImageView(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresourceRange));

	std::vector<VkImageView>		attachmentImages		= { *colorImageView };

	bool							depthTestsEnabled		= (m_depthClearTestEnabled		|| m_depthWriteTestEnabled);
	bool							stencilTestsEnabled		= (m_stencilClearTestEnabled	|| m_stencilWriteTestEnabled);

	const VkFormat					testFormat				= (depthTestsEnabled	&& stencilTestsEnabled
															? pickSupportedDepthStencilFormat(vki, physDevice)
															: !depthTestsEnabled		&& stencilTestsEnabled
															? VK_FORMAT_S8_UINT
															: VK_FORMAT_D16_UNORM);

	m_context.getTestContext().getLog() << tcu::TestLog::Message << "Using depth/stencil format " << getFormatName(testFormat) << tcu::TestLog::EndMessage;

	const VkImageSubresourceRange	testSubresourceRange = makeImageSubresourceRange(getImageAspectFlags(testFormat), 0u, 1u, 0u, 1u);
	const Unique<VkImage>			testImage(makeImage(vk, device, makeImageCreateInfo(m_renderSize, testFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)));
	const UniquePtr<Allocation>		testImageAlloc(bindImage(vk, device, allocator, *testImage, MemoryRequirement::Any));
	const Unique<VkImageView>		testImageView(makeImageView(vk, device, *testImage, VK_IMAGE_VIEW_TYPE_2D, testFormat, testSubresourceRange));

	if (depthTestsEnabled || stencilTestsEnabled)
		attachmentImages.push_back(*testImageView);

	const deUint32					numUsedAttachmentImages	= deUint32(attachmentImages.size());

	// Depth occluder vertex buffer
	const deUint32					numDepthOccVertices		= 6;
	const VkDeviceSize				dOccVertBuffSizeBytes	= 256;
	const Unique<VkBuffer>			dOccluderVertexBuffer	(makeBuffer(vk, device, dOccVertBuffSizeBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
	const UniquePtr<Allocation>		dOccVertexBufferAlloc	(bindBuffer(vk, device, allocator, *dOccluderVertexBuffer, MemoryRequirement::HostVisible));

	{
		tcu::Vec4* const pVertices = reinterpret_cast<tcu::Vec4*>(dOccVertexBufferAlloc->getHostPtr());

		pVertices[0]	= tcu::Vec4(-0.25f, -0.50f , 0.0f, 1.0f);	// Top Right
		pVertices[1]	= tcu::Vec4(-0.50f, -0.50f , 0.0f, 1.0f);	// Top Left
		pVertices[2]	= tcu::Vec4(-0.25f, -0.25f, 0.0f, 1.0f);	// Bottom Right
		pVertices[3]	= tcu::Vec4(-0.50f, -0.25f, 0.0f, 1.0f);	// Bottom Left
		pVertices[4]	= tcu::Vec4(-0.25f, -0.25f, 0.0f, 1.0f);	// Bottom Right
		pVertices[5]	= tcu::Vec4(-0.50f, -0.50f, 0.0f, 1.0f);	// Top Left

		flushAlloc(vk, device, *dOccVertexBufferAlloc);
	}

	// Stencil occluder vertex buffer
	const deUint32					numStencilOccVertices	= 6;
	const VkDeviceSize				sOccVertBuffSizeBytes	= 256;
	const Unique<VkBuffer>			sOccluderVertexBuffer	(makeBuffer(vk, device, sOccVertBuffSizeBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
	const UniquePtr<Allocation>		sOccVertexBufferAlloc	(bindBuffer(vk, device, allocator, *sOccluderVertexBuffer, MemoryRequirement::HostVisible));

	{
		tcu::Vec4* const pVertices = reinterpret_cast<tcu::Vec4*>(sOccVertexBufferAlloc->getHostPtr());

		pVertices[0]	= tcu::Vec4(-0.25f, -0.25f, 0.0f, 1.0f);	// Top Right
		pVertices[1]	= tcu::Vec4(-0.50f, -0.25f, 0.0f, 1.0f);	// Top Left
		pVertices[2]	= tcu::Vec4(-0.25f,  0.00f, 0.0f, 1.0f);	// Bottom Right
		pVertices[3]	= tcu::Vec4(-0.50f,  0.00f, 0.0f, 1.0f);	// Bottom Left
		pVertices[4]	= tcu::Vec4(-0.25f,  0.00f, 0.0f, 1.0f);	// Bottom Right
		pVertices[5]	= tcu::Vec4(-0.50f, -0.25f, 0.0f, 1.0f);	// Top Left

		flushAlloc(vk, device, *sOccVertexBufferAlloc);
	}

	// Main vertex buffer
	const deUint32					numVertices				= 6;
	const VkDeviceSize				vertexBufferSizeBytes	= 256;
	const Unique<VkBuffer>			vertexBuffer			(makeBuffer(vk, device, vertexBufferSizeBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
	const UniquePtr<Allocation>		vertexBufferAlloc		(bindBuffer(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible));

	{
		tcu::Vec4* const pVertices = reinterpret_cast<tcu::Vec4*>(vertexBufferAlloc->getHostPtr());

		pVertices[0]	= tcu::Vec4( 1.0f, -1.0f, 0.0f, 1.0f);
		pVertices[1]	= tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f);
		pVertices[2]	= tcu::Vec4(-1.0f,  1.0f, 0.0f, 1.0f);
		pVertices[3]	= tcu::Vec4(-1.0f,  1.0f, 1.0f, 1.0f);
		pVertices[4]	= tcu::Vec4( 1.0f,  1.0f, 1.0f, 1.0f);
		pVertices[5]	= tcu::Vec4( 1.0f, -1.0f, 1.0f, 1.0f);

		flushAlloc(vk, device, *vertexBufferAlloc);
	}

	// Render result buffer (to retrieve color attachment contents)
	const VkDeviceSize				colorBufferSizeBytes	= tcu::getPixelSize(mapVkFormat(colorFormat)) * m_renderSize.x() * m_renderSize.y();
	const Unique<VkBuffer>			colorBuffer				(makeBuffer(vk, device, colorBufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>		colorBufferAlloc		(bindBuffer(vk, device, allocator, *colorBuffer, MemoryRequirement::HostVisible));

	// Pipeline
	const Unique<VkShaderModule>	vertexModule			(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>	fragmentModule			(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0u));
	const Unique<VkRenderPass>		renderPass				(makeRenderPass(vk, device, colorFormat, (depthTestsEnabled || stencilTestsEnabled), testFormat));
	const Unique<VkFramebuffer>		framebuffer				(makeFramebuffer(vk, device, *renderPass, numUsedAttachmentImages, attachmentImages.data(), m_renderSize.x(), m_renderSize.y()));
	const Unique<VkPipelineLayout>	pipelineLayout			(makePipelineLayout(vk, device, DE_NULL));
	const Unique<VkPipeline>		pipeline				(makeGraphicsPipeline(vk, device, *pipelineLayout, *renderPass, *vertexModule, *fragmentModule, m_renderSize,
															 m_scissorTestEnabled, depthTestsEnabled, stencilTestsEnabled, false));

	const Unique<VkPipeline>		pipelineStencilWrite	(makeGraphicsPipeline(vk, device, *pipelineLayout, *renderPass, *vertexModule, *fragmentModule, m_renderSize,
															 m_scissorTestEnabled, false, stencilTestsEnabled, true));

	// Command buffer
	const Unique<VkCommandPool>		cmdPool					(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer				(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	{
		const tcu::Vec4		clearColor			= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
		const float			clearDepth			= 0.5f;
		const deUint32		clearStencil		= 1u;
		const VkDeviceSize	vertexBufferOffset	= 0ull;

		const VkRect2D		renderArea			=
		{
			makeOffset2D(0, 0),
			makeExtent2D(m_renderSize.x(), m_renderSize.y()),
		};

		beginCommandBuffer(vk, *cmdBuffer);

		vk.cmdResetQueryPool(*cmdBuffer, queryPool, 0, queryCount);

		// Will clear the attachments with specified depth and stencil values.
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea, clearColor, clearDepth, clearStencil);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

		// Mask half of the attachment image with value that will pass the stencil test.
		if (m_depthClearTestEnabled)
			commandClearDepthAttachment(vk, *cmdBuffer, makeOffset2D(0, m_renderSize.y() / 2), makeExtent2D(m_renderSize.x(), m_renderSize.y() / 2), 1u);

		// Mask half of the attachment image with value that will pass the stencil test.
		if (m_stencilClearTestEnabled)
			commandClearStencilAttachment(vk, *cmdBuffer, makeOffset2D(m_renderSize.x() / 2, 0), makeExtent2D(m_renderSize.x() / 2, m_renderSize.y()), 0u);

		if (m_depthWriteTestEnabled)
		{
			vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &dOccluderVertexBuffer.get(), &vertexBufferOffset);
			vk.cmdDraw(*cmdBuffer, numDepthOccVertices, 1u, 0u, 0u);
		}

		if (m_stencilWriteTestEnabled)
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineStencilWrite);
			vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &sOccluderVertexBuffer.get(), &vertexBufferOffset);
			vk.cmdDraw(*cmdBuffer, numStencilOccVertices, 1u, 0u, 0u);
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
		}

		vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);

		if (m_preciseBitEnabled)
		{
			vk.cmdBeginQuery(cmdBuffer.get(), queryPool, 0, VK_QUERY_CONTROL_PRECISE_BIT);
		}
		else
		{
			vk.cmdBeginQuery(cmdBuffer.get(), queryPool, 0, DE_NULL);
		}

		vk.cmdDraw(*cmdBuffer, numVertices, 1u, 0u, 0u);
		vk.cmdEndQuery(cmdBuffer.get(), queryPool, 0);

		endRenderPass(vk, *cmdBuffer);

		copyImageToBuffer(vk, *cmdBuffer, *colorImage, *colorBuffer, m_renderSize, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

		endCommandBuffer(vk, *cmdBuffer);
		submitCommandsAndWait(vk, device, queue, *cmdBuffer);
	}

	// Check results
	{
		deUint64 expResult = 0;

		if (m_preciseBitEnabled)
		{
			const deUint64	imageSize = m_scissorTestEnabled	? deUint64(m_renderSize.x()) * deUint64(m_renderSize.y()) / 4u
																: deUint64(m_renderSize.x()) * deUint64(m_renderSize.y());

			const deUint64	renderHeight = m_scissorTestEnabled	? deUint64(m_renderSize.y() / 2u)
																: deUint64(m_renderSize.y());

			const deUint64	occluderWriteSize = deUint64(m_renderSize.x()) * deUint64(m_renderSize.y()) / 64u;

			if (m_depthClearTestEnabled || m_stencilClearTestEnabled)
			{
				if (m_depthClearTestEnabled && m_stencilClearTestEnabled)
				{
					expResult = imageSize / 4;
				}

				if (!m_depthClearTestEnabled && m_stencilClearTestEnabled)
				{
					expResult = imageSize / 2;
				}

				if (m_depthClearTestEnabled && !m_stencilClearTestEnabled)
				{
					expResult = imageSize / 2 - imageSize / 8 - renderHeight / 4;
				}
			}
			else if (m_depthWriteTestEnabled)
			{
				expResult = imageSize / 2 - renderHeight / 2;
			}
			else
			{
				expResult = imageSize;
			}

			if (m_depthWriteTestEnabled)
			{
				expResult -= occluderWriteSize;

				if (m_stencilClearTestEnabled && !m_depthClearTestEnabled)
				{
					expResult -= (imageSize / 8 + renderHeight / 4);
				}
			}

			if (m_stencilWriteTestEnabled)
			{
				expResult -= occluderWriteSize;
			}
		}

		VK_CHECK(vk.getQueryPoolResults(device, queryPool, 0u, queryCount, queryCount * sizeof(VkDeviceSize), sampleCounts.data(), sizeof(VkDeviceSize), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));

		// Log test results
		{
			tcu::TestLog& log = m_context.getTestContext().getLog();
			log << tcu::TestLog::Message << "Passed Samples : " << de::toString(sampleCounts[0]) << " / " << expResult << tcu::TestLog::EndMessage;
		}

#ifndef CTS_USES_VULKANSC
		vk.destroyQueryPool(device, queryPool, nullptr);
#endif // CTS_USES_VULKANSC

		if ((m_preciseBitEnabled && sampleCounts[0] == expResult) || (!m_preciseBitEnabled && sampleCounts[0] > 0))
		{
			return tcu::TestStatus::pass("Success");
		}
		else
		{

			invalidateAlloc(vk, device, *colorBufferAlloc);

			const tcu::ConstPixelBufferAccess	imagePixelAccess	(mapVkFormat(colorFormat), m_renderSize.x(), m_renderSize.y(), 1, colorBufferAlloc->getHostPtr());
			tcu::TestLog&						log					= m_context.getTestContext().getLog();

			log << tcu::TestLog::Image("color0", "Rendered image", imagePixelAccess);

			return tcu::TestStatus::fail("Failure");
		}
	}
}

class OcclusionQueryTest : public TestCase
{
public:
						OcclusionQueryTest	(tcu::TestContext&		testCtx,
											 const std::string		name,
											 const deUint32			flags,
											 const int				renderWidth,
											 const int				renderHeight);

	void				initPrograms		(SourceCollections&		programCollection) const;
	TestInstance*		createInstance		(Context&				context) const;
	virtual void		checkSupport		(Context&				context) const;

private:
	const bool		m_preciseBitEnabled;
	const bool		m_scissorTestEnabled;
	const bool		m_depthClearTestEnabled;
	const bool		m_stencilClearTestEnabled;
	const bool		m_depthWriteTestEnabled;
	const bool		m_stencilWriteTestEnabled;
	const int		m_renderWidth;
	const int		m_renderHeight;
};

OcclusionQueryTest::OcclusionQueryTest	(tcu::TestContext& testCtx, const std::string name, const deUint32 flags, const int renderWidth, const int renderHeight)
	: TestCase							(testCtx, name, "")
	, m_preciseBitEnabled				(flags & TEST_PRECISE_BIT)
	, m_scissorTestEnabled				(flags & TEST_SCISSOR)
	, m_depthClearTestEnabled			(flags & TEST_DEPTH_CLEAR	|| flags & TEST_ALL)
	, m_stencilClearTestEnabled			(flags & TEST_STENCIL_CLEAR	|| flags & TEST_ALL)
	, m_depthWriteTestEnabled			(flags & TEST_DEPTH_WRITE	|| flags & TEST_ALL)
	, m_stencilWriteTestEnabled			(flags & TEST_STENCIL_WRITE	|| flags & TEST_ALL)
	, m_renderWidth						(renderWidth)
	, m_renderHeight					(renderHeight)
{
}

void OcclusionQueryTest::initPrograms (SourceCollections& programCollection) const
{
	// Vertex
	{
		std::ostringstream src;

		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in highp vec4 position;\n"
			<< "\n"
			<< "out gl_PerVertex\n"
			<< "{\n"
			<< "   vec4 gl_Position;\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    gl_Position = position;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment
	{
		std::ostringstream src;

		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) out highp vec4 fragColor;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "	fragColor = vec4(gl_FragCoord.x / " << m_renderWidth << ", gl_FragCoord.y / " << m_renderHeight << ", 0.0, 1.0); \n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

TestInstance* OcclusionQueryTest::createInstance (Context& context) const
{
	return new OcclusionQueryTestInstance	(context, tcu::IVec2(m_renderWidth, m_renderHeight),
											 m_preciseBitEnabled, m_scissorTestEnabled,
											 m_depthClearTestEnabled, m_stencilClearTestEnabled,
											 m_depthWriteTestEnabled, m_stencilWriteTestEnabled);
}

void OcclusionQueryTest::checkSupport (Context& context) const
{
	const InstanceInterface&	vki					= context.getInstanceInterface();
	const VkPhysicalDevice		physDevice			= context.getPhysicalDevice();
	VkImageFormatProperties		formatProperties;

	bool						depthTestsEnabled	= (m_depthClearTestEnabled		|| m_depthWriteTestEnabled);
	bool						stencilTestsEnabled	= (m_stencilClearTestEnabled	|| m_stencilWriteTestEnabled);

	const VkFormat				testFormat			= (stencilTestsEnabled	&& depthTestsEnabled
													? pickSupportedDepthStencilFormat(vki, physDevice)
													: stencilTestsEnabled
													? VK_FORMAT_S8_UINT
													: VK_FORMAT_D16_UNORM);

	if (m_preciseBitEnabled)
	{
		vk::VkQueryControlFlags queryControlFlags = { VK_QUERY_CONTROL_PRECISE_BIT };

		if (queryControlFlags && vk::VK_QUERY_CONTROL_PRECISE_BIT != context.getDeviceFeatures().occlusionQueryPrecise)
			TCU_THROW(NotSupportedError, "Precise occlusion queries are not supported");
	}

	vki.getPhysicalDeviceImageFormatProperties(physDevice, testFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0u, &formatProperties);
	if (formatProperties.sampleCounts == 0 || testFormat == VK_FORMAT_UNDEFINED)
		TCU_THROW(NotSupportedError, de::toString(testFormat) + " not supported");
}

} // anonymous ns

tcu::TestCaseGroup* createOcclusionQueryTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "occlusion_query", "occlusion query test cases"));

	{
		static const struct
		{
			std::string	caseName;
			deUint32	flags;
		} cases[] =
		{
			{ "_test_scissors_clear_color",								TEST_SCISSOR																			},
			{ "_test_scissors_depth_clear",								TEST_SCISSOR	|	TEST_DEPTH_CLEAR													},
			{ "_test_scissors_depth_write",								TEST_SCISSOR	|	TEST_DEPTH_WRITE													},
			{ "_test_scissors_depth_clear_depth_write",					TEST_SCISSOR	|	TEST_DEPTH_CLEAR	|	TEST_DEPTH_WRITE							},
			{ "_test_scissors_stencil_clear",							TEST_SCISSOR	|	TEST_STENCIL_CLEAR													},
			{ "_test_scissors_stencil_write",							TEST_SCISSOR	|	TEST_STENCIL_WRITE													},
			{ "_test_scissors_stencil_clear_stencil_write",				TEST_SCISSOR	|	TEST_STENCIL_CLEAR	|	TEST_STENCIL_WRITE							},
			{ "_test_scissors_depth_clear_stencil_clear",				TEST_SCISSOR	|	TEST_DEPTH_CLEAR	|	TEST_STENCIL_CLEAR							},
			{ "_test_scissors_depth_write_stencil_clear",				TEST_SCISSOR	|	TEST_DEPTH_WRITE	|	TEST_STENCIL_CLEAR							},
			{ "_test_scissors_depth_clear_stencil_write",				TEST_SCISSOR	|	TEST_DEPTH_CLEAR	|	TEST_STENCIL_WRITE							},
			{ "_test_scissors_depth_write_stencil_write",				TEST_SCISSOR	|	TEST_DEPTH_WRITE	|	TEST_STENCIL_WRITE							},
			{ "_test_scissors_depth_clear_stencil_clear_depth_write",	TEST_SCISSOR	|	TEST_DEPTH_CLEAR	|	TEST_DEPTH_WRITE	|	TEST_STENCIL_CLEAR	},
			{ "_test_scissors_depth_clear_stencil_clear_stencil_write",	TEST_SCISSOR	|	TEST_DEPTH_CLEAR	|	TEST_STENCIL_CLEAR	|	TEST_STENCIL_WRITE	},
			{ "_test_scissors_depth_clear_depth_write_stencil_write",	TEST_SCISSOR	|	TEST_DEPTH_CLEAR	|	TEST_DEPTH_WRITE	|	TEST_STENCIL_WRITE	},
			{ "_test_scissors_depth_write_stencil_clear_stencil_write",	TEST_SCISSOR	|	TEST_DEPTH_WRITE	|	TEST_STENCIL_CLEAR	|	TEST_STENCIL_WRITE	},
			{ "_test_scissors_test_all",								TEST_SCISSOR	|	TEST_ALL															},
			{ "_test_clear_color",										TEST_NO_FLAGS																			},
			{ "_test_depth_clear",															TEST_DEPTH_CLEAR													},
			{ "_test_depth_write",															TEST_DEPTH_WRITE													},
			{ "_test_depth_clear_depth_write",												TEST_DEPTH_CLEAR	|	TEST_DEPTH_WRITE							},
			{ "_test_stencil_clear",														TEST_STENCIL_CLEAR													},
			{ "_test_stencil_write",														TEST_STENCIL_WRITE													},
			{ "_test_stencil_clear_stencil_write",											TEST_STENCIL_CLEAR	|	TEST_STENCIL_WRITE							},
			{ "_test_depth_clear_stencil_clear",											TEST_DEPTH_CLEAR	|	TEST_STENCIL_CLEAR							},
			{ "_test_depth_write_stencil_clear",											TEST_DEPTH_WRITE	|	TEST_STENCIL_CLEAR							},
			{ "_test_depth_clear_stencil_write",											TEST_DEPTH_CLEAR	|	TEST_STENCIL_WRITE							},
			{ "_test_depth_write_stencil_write",											TEST_DEPTH_WRITE	|	TEST_STENCIL_WRITE							},
			{ "_test_depth_clear_stencil_clear_depth_write",								TEST_DEPTH_CLEAR	|	TEST_DEPTH_WRITE	|	TEST_STENCIL_CLEAR	},
			{ "_test_depth_clear_stencil_clear_stencil_write",								TEST_DEPTH_CLEAR	|	TEST_STENCIL_CLEAR	|	TEST_STENCIL_WRITE	},
			{ "_test_depth_clear_depth_write_stencil_write",								TEST_DEPTH_CLEAR	|	TEST_DEPTH_WRITE	|	TEST_STENCIL_WRITE	},
			{ "_test_depth_write_stencil_clear_stencil_write",								TEST_DEPTH_WRITE	|	TEST_STENCIL_CLEAR	|	TEST_STENCIL_WRITE	},
			{ "_test_test_all",																TEST_ALL															}
		};

		// Conservative tests
		for (int i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
			testGroup->addChild(new OcclusionQueryTest(testCtx, "conservative" + cases[i].caseName, cases[i].flags, 32, 32));

		// Precise tests
		for (int i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
			testGroup->addChild(new OcclusionQueryTest(testCtx, "precise" + cases[i].caseName, cases[i].flags | TEST_PRECISE_BIT, 32, 32));
	}

	return testGroup.release();
}

} // FragmentOperations
} // vkt
