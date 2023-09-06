/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
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
 * \brief Protected memory blit image tests
 *//*--------------------------------------------------------------------*/

#include "vktProtectedMemBlitImageTests.hpp"

#include "deRandom.hpp"
#include "tcuTestLog.hpp"
#include "tcuVector.hpp"
#include "tcuVectorUtil.hpp"

#include "vkPrograms.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"

#include "vktProtectedMemContext.hpp"
#include "vktProtectedMemUtils.hpp"
#include "vktProtectedMemImageValidator.hpp"

namespace vkt
{
namespace ProtectedMem
{

namespace
{

enum {
	RENDER_WIDTH	= 128,
	RENDER_HEIGHT	= 128,
};

class BlitImageTestInstance : public ProtectedTestInstance
{
public:
									BlitImageTestInstance	(Context&						ctx,
															 const vk::VkClearColorValue&	clearColorValue,
															 const ValidationData&			refData,
															 const ImageValidator&			validator,
															 const CmdBufferType			cmdBufferType);
	virtual tcu::TestStatus			iterate					 (void);

private:
	const vk::VkFormat				m_imageFormat;
	const vk::VkClearColorValue&	m_clearColorValue;
	const ValidationData&			m_refData;
	const ImageValidator&			m_validator;
	const CmdBufferType				m_cmdBufferType;
};

class BlitImageTestCase : public TestCase
{
public:
								BlitImageTestCase			(tcu::TestContext&			testCtx,
															 const std::string&			name,
															 vk::VkClearColorValue		clearColorValue,
															 ValidationData				data,
															 CmdBufferType				cmdBufferType)
									: TestCase				(testCtx, name, "Clear and blit image.")
									, m_clearColorValue		(clearColorValue)
									, m_refData				(data)
									, m_cmdBufferType		(cmdBufferType)
								{
								}

	virtual						~BlitImageTestCase			(void) {}
	virtual TestInstance*		createInstance				(Context& ctx) const
								{
									return new BlitImageTestInstance(ctx, m_clearColorValue, m_refData, m_validator, m_cmdBufferType);
								}
	virtual void				initPrograms				(vk::SourceCollections& programCollection) const
								{
									m_validator.initPrograms(programCollection);
								}
	virtual void				checkSupport				(Context& context) const
								{
									checkProtectedQueueSupport(context);
#ifdef CTS_USES_VULKANSC
									if (m_cmdBufferType == CMD_BUFFER_SECONDARY && context.getDeviceVulkanSC10Properties().secondaryCommandBufferNullOrImagelessFramebuffer == VK_FALSE)
										TCU_THROW(NotSupportedError, "secondaryCommandBufferNullFramebuffer is not supported");
#endif
								}
private:
	vk::VkClearColorValue		m_clearColorValue;
	ValidationData				m_refData;
	ImageValidator				m_validator;
	CmdBufferType				m_cmdBufferType;
};

BlitImageTestInstance::BlitImageTestInstance	(Context&						ctx,
												 const vk::VkClearColorValue&	clearColorValue,
												 const ValidationData&			refData,
												 const ImageValidator&			validator,
															 const CmdBufferType			cmdBufferType)
	: ProtectedTestInstance		(ctx)
	, m_imageFormat				(vk::VK_FORMAT_R8G8B8A8_UNORM)
	, m_clearColorValue			(clearColorValue)
	, m_refData					(refData)
	, m_validator				(validator)
	, m_cmdBufferType			(cmdBufferType)
{
}

tcu::TestStatus BlitImageTestInstance::iterate()
{
	ProtectedContext&					ctx					(m_protectedContext);
	const vk::DeviceInterface&			vk					= ctx.getDeviceInterface();
	const vk::VkDevice					device				= ctx.getDevice();
	const vk::VkQueue					queue				= ctx.getQueue();
	const deUint32						queueFamilyIndex	= ctx.getQueueFamilyIndex();

	// Create images
	de::MovePtr<vk::ImageWithMemory>	colorImage			= createImage2D(ctx, PROTECTION_ENABLED, queueFamilyIndex,
																			RENDER_WIDTH, RENDER_HEIGHT,
																			m_imageFormat,
																			vk::VK_IMAGE_USAGE_SAMPLED_BIT
																			| vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	de::MovePtr<vk::ImageWithMemory>	colorImageSrc		= createImage2D(ctx, PROTECTION_ENABLED, queueFamilyIndex,
																			RENDER_WIDTH, RENDER_HEIGHT,
																			m_imageFormat,
																			vk::VK_IMAGE_USAGE_SAMPLED_BIT
																			| vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT
																			| vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	vk::Unique<vk::VkPipelineLayout>	pipelineLayout		(createPipelineLayout(ctx, 0u, DE_NULL));

	vk::Unique<vk::VkCommandPool>		cmdPool				(makeCommandPool(vk, device, PROTECTION_ENABLED, queueFamilyIndex));
	vk::Unique<vk::VkCommandBuffer>		cmdBuffer			(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	vk::Unique<vk::VkCommandBuffer>		secondaryCmdBuffer	(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_SECONDARY));
	vk::VkCommandBuffer					targetCmdBuffer		= (m_cmdBufferType == CMD_BUFFER_SECONDARY) ? *secondaryCmdBuffer : *cmdBuffer;

	// Begin cmd buffer
	beginCommandBuffer(vk, *cmdBuffer);

	if (m_cmdBufferType == CMD_BUFFER_SECONDARY)
	{
		// Begin secondary command buffer
		const vk::VkCommandBufferInheritanceInfo	secCmdBufInheritInfo	=
		{
			vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
			DE_NULL,
			(vk::VkRenderPass)0u,										// renderPass
			0u,															// subpass
			(vk::VkFramebuffer)0u,										// framebuffer
			VK_FALSE,													// occlusionQueryEnable
			(vk::VkQueryControlFlags)0u,								// queryFlags
			(vk::VkQueryPipelineStatisticFlags)0u,						// pipelineStatistics
		};
		beginSecondaryCommandBuffer(vk, *secondaryCmdBuffer, secCmdBufInheritInfo);
	}

	// Start image barrier for source image.
	{
		const vk::VkImageMemoryBarrier	startImgBarrier		=
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// sType
			DE_NULL,											// pNext
			0,													// srcAccessMask
			vk::VK_ACCESS_TRANSFER_WRITE_BIT,					// dstAccessMask
			vk::VK_IMAGE_LAYOUT_UNDEFINED,						// oldLayout
			vk::VK_IMAGE_LAYOUT_GENERAL,						// newLayout
			queueFamilyIndex,									// srcQueueFamilyIndex
			queueFamilyIndex,									// dstQueueFamilyIndex
			**colorImageSrc,									// image
			{
				vk::VK_IMAGE_ASPECT_COLOR_BIT,					// aspectMask
				0u,												// baseMipLevel
				1u,												// mipLevels
				0u,												// baseArraySlice
				1u,												// subresourceRange
			}
		};

		vk.cmdPipelineBarrier(targetCmdBuffer,									// commandBuffer
							  vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,			// srcStageMask
							  vk::VK_PIPELINE_STAGE_TRANSFER_BIT,				// dstStageMask
							  (vk::VkDependencyFlags)0,							// dependencyFlags
							  0, (const vk::VkMemoryBarrier*)DE_NULL,			// memoryBarrierCount, pMemoryBarriers
							  0, (const vk::VkBufferMemoryBarrier*)DE_NULL,		// bufferMemoryBarrierCount, pBufferMemoryBarriers
							  1, &startImgBarrier);								// imageMemoryBarrierCount, pImageMemoryBarriers
	}

	// Image clear
	const vk::VkImageSubresourceRange subresourceRange =
	{
		vk::VK_IMAGE_ASPECT_COLOR_BIT,							// VkImageAspectFlags			aspectMask
		0u,														// uint32_t						baseMipLevel
		1u,														// uint32_t						levelCount
		0u,														// uint32_t						baseArrayLayer
		1u,														// uint32_t						layerCount
	};
	vk.cmdClearColorImage(targetCmdBuffer, **colorImageSrc, vk::VK_IMAGE_LAYOUT_GENERAL, &m_clearColorValue, 1, &subresourceRange);

	// Image barrier to change accessMask to transfer read bit for source image.
	{
		const vk::VkImageMemoryBarrier initializeBarrier =
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// sType
			DE_NULL,											// pNext
			vk::VK_ACCESS_TRANSFER_WRITE_BIT,					// srcAccessMask
			vk::VK_ACCESS_TRANSFER_READ_BIT,					// dstAccessMask
			vk::VK_IMAGE_LAYOUT_GENERAL,						// oldLayout
			vk::VK_IMAGE_LAYOUT_GENERAL,						// newLayout
			queueFamilyIndex,									// srcQueueFamilyIndex
			queueFamilyIndex,									// dstQueueFamilyIndex
			**colorImageSrc,									// image
			{
				vk::VK_IMAGE_ASPECT_COLOR_BIT,					// aspectMask
				0u,												// baseMipLevel
				1u,												// mipLevels
				0u,												// baseArraySlice
				1u,												// subresourceRange
			}
		};

		vk.cmdPipelineBarrier(targetCmdBuffer,
							  vk::VK_PIPELINE_STAGE_TRANSFER_BIT,	// srcStageMask
							  vk::VK_PIPELINE_STAGE_TRANSFER_BIT,	// dstStageMask
							  (vk::VkDependencyFlags)0,
							  0, (const vk::VkMemoryBarrier*)DE_NULL,
							  0, (const vk::VkBufferMemoryBarrier*)DE_NULL,
							  1, &initializeBarrier);
	}

	// Image barrier for destination image.
	{
		const vk::VkImageMemoryBarrier	initializeBarrier	=
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// sType
			DE_NULL,											// pNext
			0,													// srcAccessMask
			vk::VK_ACCESS_TRANSFER_WRITE_BIT,					// dstAccessMask
			vk::VK_IMAGE_LAYOUT_UNDEFINED,						// oldLayout
			vk::VK_IMAGE_LAYOUT_GENERAL,						// newLayout
			queueFamilyIndex,									// srcQueueFamilyIndex
			queueFamilyIndex,									// dstQueueFamilyIndex
			**colorImage,										// image
			{
				vk::VK_IMAGE_ASPECT_COLOR_BIT,					// aspectMask
				0u,												// baseMipLevel
				1u,												// mipLevels
				0u,												// baseArraySlice
				1u,												// subresourceRange
			}
		};

		vk.cmdPipelineBarrier(targetCmdBuffer,
							  vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,		// srcStageMask
							  vk::VK_PIPELINE_STAGE_TRANSFER_BIT,			// dstStageMask
							  (vk::VkDependencyFlags)0,
							  0, (const vk::VkMemoryBarrier*)DE_NULL,
							  0, (const vk::VkBufferMemoryBarrier*)DE_NULL,
							  1, &initializeBarrier);
	}

	// Blit image
	const vk::VkImageSubresourceLayers	imgSubResCopy	=
	{
		vk::VK_IMAGE_ASPECT_COLOR_BIT,							// VkImageAspectFlags			aspectMask;
		0u,														// deUint32						mipLevel;
		0u,														// deUint32						baseArrayLayer;
		1u,														// deUint32						layerCount;
	};
	const vk::VkOffset3D				nullOffset		=	{0u, 0u, 0u};
	const vk::VkOffset3D				imageOffset		=	{RENDER_WIDTH, RENDER_HEIGHT, 1};
	const vk::VkImageBlit				imageBlit		=
	 {
		imgSubResCopy,											// VkImageSubresourceLayers		srcSubresource;
		{
			nullOffset,
			imageOffset,
		},														// VkOffset3D					srcOffsets[2];
		imgSubResCopy,											// VkImageSubresourceLayers		dstSubresource;
		{
			nullOffset,
			imageOffset,
		},														// VkOffset3D					dstOffsets[2];
	 };
	vk.cmdBlitImage(targetCmdBuffer, **colorImageSrc, vk::VK_IMAGE_LAYOUT_GENERAL,
					**colorImage, vk::VK_IMAGE_LAYOUT_GENERAL, 1u, &imageBlit, vk::VK_FILTER_NEAREST);

	// Image barrier to change accessMask to shader read bit for destination image.
	{
		const vk::VkImageMemoryBarrier	endImgBarrier	=
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// sType
			DE_NULL,											// pNext
			vk::VK_ACCESS_TRANSFER_WRITE_BIT,					// srcAccessMask
			vk::VK_ACCESS_SHADER_READ_BIT,						// dstAccessMask
			vk::VK_IMAGE_LAYOUT_GENERAL,						// oldLayout
			vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,		// newLayout
			queueFamilyIndex,									// srcQueueFamilyIndex
			queueFamilyIndex,									// dstQueueFamilyIndex
			**colorImage,										// image
			{
				vk::VK_IMAGE_ASPECT_COLOR_BIT,					// aspectMask
				0u,												// baseMipLevel
				1u,												// mipLevels
				0u,												// baseArraySlice
				1u,												// subresourceRange
			}
		};
		vk.cmdPipelineBarrier(targetCmdBuffer,
							  vk::VK_PIPELINE_STAGE_TRANSFER_BIT,		// srcStageMask
							  vk::VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,	// dstStageMask
							  (vk::VkDependencyFlags)0,
							  0, (const vk::VkMemoryBarrier*)DE_NULL,
							  0, (const vk::VkBufferMemoryBarrier*)DE_NULL,
							  1, &endImgBarrier);
	}

	if (m_cmdBufferType == CMD_BUFFER_SECONDARY)
	{
		endCommandBuffer(vk, *secondaryCmdBuffer);
		vk.cmdExecuteCommands(*cmdBuffer, 1u, &secondaryCmdBuffer.get());
	}

	endCommandBuffer(vk, *cmdBuffer);

	// Submit command buffer
	const vk::Unique<vk::VkFence>	fence		(vk::createFence(vk, device));
	VK_CHECK(queueSubmit(ctx, PROTECTION_ENABLED, queue, *cmdBuffer, *fence, ~0ull));

	// Log out test data
	ctx.getTestContext().getLog()
		<< tcu::TestLog::Message << "Color clear value: " << tcu::Vec4(m_clearColorValue.float32) << tcu::TestLog::EndMessage;

	// Validate resulting image
	if (m_validator.validateImage(ctx, m_refData, **colorImage, m_imageFormat, vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
		return tcu::TestStatus::pass("Everything went OK");
	else
		return tcu::TestStatus::fail("Something went really wrong");
}

tcu::TestCaseGroup*	createBlitImageTests (tcu::TestContext& testCtx, CmdBufferType cmdBufferType)
{
	struct {
		const vk::VkClearColorValue		clearColorValue;
		const ValidationData			data;
	} testData[] = {
		{	{ { 1.0f, 0.0f, 0.0f, 1.0f } },
			{
				{ tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 0.0f),
				  tcu::Vec4(0.1f, 0.1f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.0f, 0.0f), },
				{ tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
				  tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f), }
			}
		},
		{	{ { 0.0f, 1.0f, 0.0f, 1.0f } },
			{
				{ tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 0.0f),
				  tcu::Vec4(0.1f, 0.1f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.0f, 0.0f), },
				{ tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f),
				  tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), }
			}
		},
		{	{ { 0.0f, 0.0f, 1.0f, 1.0f } },
			{
				{ tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 0.0f),
				  tcu::Vec4(0.1f, 0.1f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.0f, 0.0f), },
				{ tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f),
				  tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), }
			}
		},
		{	{ { 0.0f, 0.0f, 0.0f, 1.0f } },
			{
				{ tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 0.0f),
				  tcu::Vec4(0.1f, 0.1f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.0f, 0.0f), },
				{ tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
				  tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), }
			}
		},
		{	{ { 1.0f, 0.0f, 0.0f, 1.0f } },
			{
				{ tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 0.0f),
				  tcu::Vec4(0.1f, 0.1f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.0f, 0.0f), },
				{ tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
				  tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f), }
			}
		},
		{	{ { 1.0f, 0.0f, 0.0f, 0.0f } },
			{
				{ tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 0.0f),
				  tcu::Vec4(0.1f, 0.1f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.0f, 0.0f), },
				{ tcu::Vec4(1.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 0.0f),
				  tcu::Vec4(1.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 0.0f), }
			}
		},
		{	{ { 0.1f, 0.2f, 0.3f, 0.0f } },
			{
				{ tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 0.0f),
				  tcu::Vec4(0.1f, 0.1f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.0f, 0.0f), },
				{ tcu::Vec4(0.1f, 0.2f, 0.3f, 0.0f), tcu::Vec4(0.1f, 0.2f, 0.3f, 0.0f),
				  tcu::Vec4(0.1f, 0.2f, 0.3f, 0.0f), tcu::Vec4(0.1f, 0.2f, 0.3f, 0.0f), }
			}
		},
	};

	de::MovePtr<tcu::TestCaseGroup> blitStaticTests (new tcu::TestCaseGroup(testCtx, "static", "Blit Image Tests with static input"));

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(testData); ++ndx)
	{
		const std::string name = "blit_" + de::toString(ndx + 1);
		blitStaticTests->addChild(new BlitImageTestCase(testCtx, name.c_str(), testData[ndx].clearColorValue, testData[ndx].data, cmdBufferType));
	}

	/* Add a few randomized tests */
	de::MovePtr<tcu::TestCaseGroup>	blitRandomTests	(new tcu::TestCaseGroup(testCtx, "random", "Blit Image Tests with random input"));
	const int						testCount			= 10;
	de::Random						rnd					(testCtx.getCommandLine().getBaseSeed());
	for (int ndx = 0; ndx < testCount; ++ndx)
	{
		const std::string	name		= "blit_" + de::toString(ndx + 1);
		vk::VkClearValue	clearValue	= vk::makeClearValueColorVec4(tcu::randomVec4(rnd));
		const tcu::Vec4		refValue	(clearValue.color.float32[0], clearValue.color.float32[1], clearValue.color.float32[2], clearValue.color.float32[3]);
		const tcu::Vec4		vec0		= tcu::randomVec4(rnd);
		const tcu::Vec4		vec1		= tcu::randomVec4(rnd);
		const tcu::Vec4		vec2		= tcu::randomVec4(rnd);
		const tcu::Vec4		vec3		= tcu::randomVec4(rnd);

		ValidationData		data		=
		{
			{ vec0, vec1, vec2, vec3 },
			{ refValue, refValue, refValue, refValue }
		};
		blitRandomTests->addChild(new BlitImageTestCase(testCtx, name.c_str(), clearValue.color, data, cmdBufferType));
	}

	std::string groupName = getCmdBufferTypeStr(cmdBufferType);
	std::string groupDesc = "Blit Image Tests with " + groupName + " command buffer";
	de::MovePtr<tcu::TestCaseGroup> blitTests (new tcu::TestCaseGroup(testCtx, groupName.c_str(), groupDesc.c_str()));
	blitTests->addChild(blitStaticTests.release());
	blitTests->addChild(blitRandomTests.release());
	return blitTests.release();
}

} // anonymous

tcu::TestCaseGroup*	createBlitImageTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> blitTests (new tcu::TestCaseGroup(testCtx, "blit", "Blit Image Tests"));

	blitTests->addChild(createBlitImageTests(testCtx, CMD_BUFFER_PRIMARY));
	blitTests->addChild(createBlitImageTests(testCtx, CMD_BUFFER_SECONDARY));

	return blitTests.release();
}

} // ProtectedMem
} // vkt
