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
 * \brief Protected memory attachment clear tests
 *//*--------------------------------------------------------------------*/

#include "vktProtectedMemAttachmentClearTests.hpp"

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

class AttachmentClearTestInstance : public ProtectedTestInstance
{
public:
								AttachmentClearTestInstance	(Context&					ctx,
															 const vk::VkClearValue&	clearValue,
															 const ValidationData&		refData,
															 const ImageValidator&		validator,
															 const CmdBufferType		cmdBufferType);
	virtual tcu::TestStatus		iterate						(void);

private:
	const vk::VkFormat			m_imageFormat;
	const vk::VkClearValue&		m_clearValue;
	const ValidationData&		m_refData;
	const ImageValidator&		m_validator;
	const CmdBufferType			m_cmdBufferType;
};


class AttachmentClearTestCase : public TestCase
{
public:
							AttachmentClearTestCase		(tcu::TestContext&		testCtx,
														 const std::string&		name,
														 vk::VkClearValue		clearValue,
														 ValidationData			data,
														 CmdBufferType			cmdBufferType)
								: TestCase			(testCtx, name, "Clear attachment.")
								, m_clearValue		(clearValue)
								, m_refData			(data)
								, m_cmdBufferType	(cmdBufferType)
							{
							}

	virtual					~AttachmentClearTestCase	(void) {}
	virtual TestInstance*	createInstance				(Context& ctx) const
							{
								return new AttachmentClearTestInstance(ctx, m_clearValue, m_refData, m_validator, m_cmdBufferType);
							}
	virtual void			initPrograms				(vk::SourceCollections&	programCollection) const
							{
								m_validator.initPrograms(programCollection);
							}
	virtual void			checkSupport				(Context& context) const
							{
								checkProtectedQueueSupport(context);
							}
private:
	vk::VkClearValue		m_clearValue;
	ValidationData			m_refData;
	ImageValidator			m_validator;
	CmdBufferType			m_cmdBufferType;
};

AttachmentClearTestInstance::AttachmentClearTestInstance	(Context&					ctx,
															 const vk::VkClearValue&	clearValue,
															 const ValidationData&		refData,
															 const ImageValidator&		validator,
															 const CmdBufferType		cmdBufferType)
	: ProtectedTestInstance	(ctx)
	, m_imageFormat			(vk::VK_FORMAT_R8G8B8A8_UNORM)
	, m_clearValue			(clearValue)
	, m_refData				(refData)
	, m_validator			(validator)
	, m_cmdBufferType		(cmdBufferType)
{
}

tcu::TestStatus AttachmentClearTestInstance::iterate()
{
	ProtectedContext&					ctx					(m_protectedContext);
	const vk::DeviceInterface&			vk					= ctx.getDeviceInterface();
	const vk::VkDevice					device				= ctx.getDevice();
	const vk::VkQueue					queue				= ctx.getQueue();
	const deUint32						queueFamilyIndex	= ctx.getQueueFamilyIndex();

	// Create output image
	de::MovePtr<vk::ImageWithMemory>	colorImage			(createImage2D(ctx, PROTECTION_ENABLED, queueFamilyIndex,
																			RENDER_WIDTH, RENDER_HEIGHT,
																			m_imageFormat,
																			vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|vk::VK_IMAGE_USAGE_SAMPLED_BIT));
	vk::Unique<vk::VkImageView>			colorImageView		(createImageView(ctx, **colorImage, m_imageFormat));

	vk::Unique<vk::VkRenderPass>		renderPass			(createRenderPass(ctx, m_imageFormat));
	vk::Unique<vk::VkFramebuffer>		framebuffer			(createFramebuffer(ctx, RENDER_WIDTH, RENDER_HEIGHT, *renderPass, *colorImageView));
	vk::Unique<vk::VkPipelineLayout>	pipelineLayout		(createPipelineLayout(ctx, 0u, DE_NULL));

	vk::Unique<vk::VkCommandPool>		cmdPool				(makeCommandPool(vk, device, PROTECTION_ENABLED, queueFamilyIndex));
	vk::Unique<vk::VkCommandBuffer>		cmdBuffer			(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	vk::Unique<vk::VkCommandBuffer>		secondaryCmdBuffer	(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_SECONDARY));
	vk::VkCommandBuffer					targetCmdBuffer		= (m_cmdBufferType == CMD_BUFFER_SECONDARY) ? *secondaryCmdBuffer : *cmdBuffer;

	// Begin cmd buffer
	beginCommandBuffer(vk, *cmdBuffer);

	// Start image barrier
	{
		const vk::VkImageMemoryBarrier	startImgBarrier		=
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// sType
			DE_NULL,											// pNext
			0,													// srcAccessMask
			vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// dstAccessMask
			vk::VK_IMAGE_LAYOUT_UNDEFINED,						// oldLayout
			vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// newLayout
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

		vk.cmdPipelineBarrier(*cmdBuffer,
							  vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
							  vk::VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
							  (vk::VkDependencyFlags)0,
							  0, (const vk::VkMemoryBarrier*)DE_NULL,
							  0, (const vk::VkBufferMemoryBarrier*)DE_NULL,
							  1, &startImgBarrier);
	}

	// Image clear to different from input color

	const tcu::Vec4						clearValue			(m_clearValue.color.float32[0] < 0.5f ? 1.0f : 0.0f,
															 m_clearValue.color.float32[1] < 0.5f ? 1.0f : 0.0f,
															 m_clearValue.color.float32[2] < 0.5f ? 1.0f : 0.0f,
															 m_clearValue.color.float32[3] < 0.5f ? 1.0f : 0.0f);

	const vk::VkSubpassContents			subpassContents		= m_cmdBufferType == CMD_BUFFER_SECONDARY
															  ? vk::VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS
															  : vk::VK_SUBPASS_CONTENTS_INLINE;
	beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, vk::makeRect2D(RENDER_WIDTH, RENDER_HEIGHT), clearValue, subpassContents);

	if (m_cmdBufferType == CMD_BUFFER_SECONDARY)
	{
		// Begin secondary command buffer
		const vk::VkCommandBufferInheritanceInfo	bufferInheritanceInfo	=
		{
			vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,		// sType
			DE_NULL,													// pNext
			*renderPass,												// renderPass
			0u,															// subpass
			*framebuffer,												// framebuffer
			VK_FALSE,													// occlusionQueryEnable
			(vk::VkQueryControlFlags)0u,								// queryFlags
			(vk::VkQueryPipelineStatisticFlags)0u,						// pipelineStatistics
		};
		beginSecondaryCommandBuffer(vk, *secondaryCmdBuffer, bufferInheritanceInfo);
	}

	{
		const vk::VkClearAttachment		pAttachments		=
		{
			vk::VK_IMAGE_ASPECT_COLOR_BIT,						//VkImageAspectFlags	aspectMask
			0u,													//uint32_t				colorAttachment
			m_clearValue										// VkClearValue			clearValue;
		};
		const vk::VkRect2D				rect2D				= vk::makeRect2D(RENDER_WIDTH, RENDER_HEIGHT);
		const vk::VkClearRect			clearRect			=
		{
			rect2D,												// VkRect2D				rect;
			0u,													// deUint32				baseArrayLayer;
			1u													// deUint32				layerCount;
		};
		vk.cmdClearAttachments(targetCmdBuffer, 1u, &pAttachments, 1u, &clearRect);
	}

	if (m_cmdBufferType == CMD_BUFFER_SECONDARY)
	{
		endCommandBuffer(vk, *secondaryCmdBuffer);
		vk.cmdExecuteCommands(*cmdBuffer, 1u, &secondaryCmdBuffer.get());
	}

	endRenderPass(vk, *cmdBuffer);

	{
		// Image validator reads image in compute shader
		const vk::VkImageMemoryBarrier	endImgBarrier		=
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// sType
			DE_NULL,											// pNext
			vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// srcAccessMask
			vk::VK_ACCESS_SHADER_READ_BIT,						// dstAccessMask
			vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// oldLayout
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
		vk.cmdPipelineBarrier(*cmdBuffer,
							  vk::VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
							  vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							  (vk::VkDependencyFlags)0,
							  0, (const vk::VkMemoryBarrier*)DE_NULL,
							  0, (const vk::VkBufferMemoryBarrier*)DE_NULL,
							  1, &endImgBarrier);
	}

	endCommandBuffer(vk, *cmdBuffer);

	// Submit command buffer
	const vk::Unique<vk::VkFence>	fence		(vk::createFence(vk, device));
	VK_CHECK(queueSubmit(ctx, PROTECTION_ENABLED, queue, *cmdBuffer, *fence, ~0ull));

	// Log out test data
	ctx.getTestContext().getLog()
		<< tcu::TestLog::Message << "Color clear value: " << tcu::Vec4(m_clearValue.color.float32) << tcu::TestLog::EndMessage
		<< tcu::TestLog::Message << "Depth clear value: " << m_clearValue.depthStencil.depth << tcu::TestLog::EndMessage
		<< tcu::TestLog::Message << "Stencil clear value: " << m_clearValue.depthStencil.stencil << tcu::TestLog::EndMessage;

	// Validate resulting image
	if (m_validator.validateImage(ctx, m_refData, **colorImage, m_imageFormat, vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
		return tcu::TestStatus::pass("Everything went OK");
	else
		return tcu::TestStatus::fail("Something went really wrong");
}

tcu::TestCaseGroup*	createAttachmentClearTests (tcu::TestContext& testCtx, CmdBufferType cmdBufferType)
{
	struct {
		const vk::VkClearValue		clearValue;
		const ValidationData		data;
	} testData[] = {
		{	vk::makeClearValueColorF32(1.0f, 0.0f, 0.0f, 1.0f),
			{
				{ tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 0.0f),
				  tcu::Vec4(0.1f, 0.1f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.0f, 0.0f), },
				{ tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
				  tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f), }
			}
		},
		{	vk::makeClearValueColorF32(0.0f, 1.0f, 0.0f, 1.0f),
			{
				{ tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 0.0f),
				  tcu::Vec4(0.1f, 0.1f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.0f, 0.0f), },
				{ tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f),
				  tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), }
			}
		},
		{	vk::makeClearValueColorF32(0.0f, 0.0f, 1.0f, 1.0f),
			{
				{ tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 0.0f),
				  tcu::Vec4(0.1f, 0.1f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.0f, 0.0f), },
				{ tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f),
				  tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), }
			}
		},
		{	vk::makeClearValueColorF32(0.0f, 0.0f, 0.0f, 1.0f),
			{
				{ tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 0.0f),
				  tcu::Vec4(0.1f, 0.1f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.0f, 0.0f), },
				{ tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
				  tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), }
			}
		},
		{	vk::makeClearValueColorF32(1.0f, 0.0f, 0.0f, 1.0f),
			{
				{ tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 0.0f),
				  tcu::Vec4(0.1f, 0.1f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.0f, 0.0f), },
				{ tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
				  tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f), }
			}
		},
		{	vk::makeClearValueColorF32(1.0f, 0.0f, 0.0f, 0.0f),
			{
				{ tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 0.0f),
				  tcu::Vec4(0.1f, 0.1f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.0f, 0.0f), },
				{ tcu::Vec4(1.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 0.0f),
				  tcu::Vec4(1.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 0.0f), }
			}
		},
		{	vk::makeClearValueColorF32(0.1f, 0.2f, 0.3f, 0.0f),
			{
				{ tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 0.0f),
				  tcu::Vec4(0.1f, 0.1f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.0f, 0.0f), },
				{ tcu::Vec4(0.1f, 0.2f, 0.3f, 0.0f), tcu::Vec4(0.1f, 0.2f, 0.3f, 0.0f),
				  tcu::Vec4(0.1f, 0.2f, 0.3f, 0.0f), tcu::Vec4(0.1f, 0.2f, 0.3f, 0.0f), }
			}
		},
	};

	de::MovePtr<tcu::TestCaseGroup>	clearStaticTests	(new tcu::TestCaseGroup(testCtx, "static", "Attachment Clear Op Tests with static input"));

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(testData); ++ndx)
	{
		const std::string name = "clear_" +  de::toString(ndx + 1);
		clearStaticTests->addChild(new AttachmentClearTestCase(testCtx, name.c_str(), testData[ndx].clearValue, testData[ndx].data, cmdBufferType));
	}

	/* Add a few randomized tests */
	de::MovePtr<tcu::TestCaseGroup>	clearRandomTests	(new tcu::TestCaseGroup(testCtx, "random", "Attachment Clear Op Tests with random input"));
	const int						testCount			= 10;
	de::Random						rnd					(testCtx.getCommandLine().getBaseSeed());
	for (int ndx = 0; ndx < testCount; ++ndx)
	{
		const std::string	name		= "clear_" +  de::toString(ndx + 1);
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

		clearRandomTests->addChild(new AttachmentClearTestCase(testCtx, name.c_str(), clearValue, data, cmdBufferType));
	}

	std::string groupName = getCmdBufferTypeStr(cmdBufferType);
	std::string groupDesc = "Attachment Clear Op Tests with " + groupName + " command buffer";
	de::MovePtr<tcu::TestCaseGroup> clearTests (new tcu::TestCaseGroup(testCtx, groupName.c_str(), groupDesc.c_str()));
	clearTests->addChild(clearStaticTests.release());
	clearTests->addChild(clearRandomTests.release());
	return clearTests.release();
}

} // anonymous

tcu::TestCaseGroup*	createAttachmentClearTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> clearTests (new tcu::TestCaseGroup(testCtx, "clear_op", "Attachment Clear Op Tests"));

	clearTests->addChild(createAttachmentClearTests(testCtx, CMD_BUFFER_PRIMARY));
	clearTests->addChild(createAttachmentClearTests(testCtx, CMD_BUFFER_SECONDARY));

	return clearTests.release();
}

} // ProtectedMem
} // vkt
