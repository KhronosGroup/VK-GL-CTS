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
 * \brief Protected content copy buffer to image tests
 *//*--------------------------------------------------------------------*/

#include "vktProtectedMemCopyBufferToImageTests.hpp"

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
	BUFFER_SIZE		= 256,
	RENDER_WIDTH	= 8,
	RENDER_HEIGHT	= 8,
};

class CopyBufferToImageTestInstance : public ProtectedTestInstance
{
public:
									CopyBufferToImageTestInstance	(Context&					ctx,
																	 const deUint32				fillValue,
																	 const ValidationData&		refData,
																	 const ImageValidator&		validator,
																	 const CmdBufferType		cmdBufferType);
virtual tcu::TestStatus				iterate							 (void);

private:
	const vk::VkFormat				m_imageFormat;
	const deUint32					m_fillValue;
	const ValidationData&			m_refData;
	const ImageValidator&			m_validator;
	const CmdBufferType				m_cmdBufferType;
};

class CopyBufferToImageTestCase : public TestCase
{
public:
								CopyBufferToImageTestCase	(tcu::TestContext&			testCtx,
															 const std::string&			name,
															 deUint32					fillValue,
															 ValidationData				data,
															 CmdBufferType				cmdBufferType)
									: TestCase				(testCtx, name, "Copy buffer to image.")
									, m_fillValue			(fillValue)
									, m_refData				(data)
									, m_validator			(vk::VK_FORMAT_R32G32B32A32_SFLOAT)
									, m_cmdBufferType		(cmdBufferType)
								{
								}

	virtual						~CopyBufferToImageTestCase	(void) {}
	virtual TestInstance*		createInstance				(Context& ctx) const
								{
									return new CopyBufferToImageTestInstance(ctx, m_fillValue, m_refData, m_validator, m_cmdBufferType);
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
#endif // CTS_USES_VULKANSC
								}
private:
	deUint32					m_fillValue;
	ValidationData				m_refData;
	ImageValidator				m_validator;
	CmdBufferType				m_cmdBufferType;
};

CopyBufferToImageTestInstance::CopyBufferToImageTestInstance	(Context&					ctx,
																 const deUint32				fillValue,
																 const ValidationData&		refData,
																 const ImageValidator&		validator,
																 const CmdBufferType		cmdBufferType)
	: ProtectedTestInstance		(ctx)
	, m_imageFormat				(vk::VK_FORMAT_R32G32B32A32_SFLOAT)
	, m_fillValue				(fillValue)
	, m_refData					(refData)
	, m_validator				(validator)
	, m_cmdBufferType			(cmdBufferType)
{
}

tcu::TestStatus CopyBufferToImageTestInstance::iterate()
{
	ProtectedContext&					ctx					(m_protectedContext);
	const vk::DeviceInterface&			vk					= ctx.getDeviceInterface();
	const vk::VkDevice					device				= ctx.getDevice();
	const vk::VkQueue					queue				= ctx.getQueue();
	const deUint32						queueFamilyIndex	= ctx.getQueueFamilyIndex();

	// Create destination image
	de::MovePtr<vk::ImageWithMemory>	colorImage			= createImage2D(ctx, PROTECTION_ENABLED, queueFamilyIndex,
																			RENDER_WIDTH, RENDER_HEIGHT,
																			m_imageFormat,
																			vk::VK_IMAGE_USAGE_SAMPLED_BIT|vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	de::MovePtr<vk::BufferWithMemory>	srcBuffer			(makeBuffer(ctx,
																		PROTECTION_ENABLED,
																		queueFamilyIndex,
																		(deUint32)(BUFFER_SIZE * sizeof(deUint32)),
																		vk::VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT
																			| vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT
																			| vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
																		vk::MemoryRequirement::Protected));

	vk::Unique<vk::VkCommandPool>		cmdPool				(makeCommandPool(vk, device, PROTECTION_ENABLED, queueFamilyIndex));
	vk::Unique<vk::VkCommandBuffer>		cmdBuffer			(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	vk::Unique<vk::VkCommandBuffer>		secondaryCmdBuffer	(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_SECONDARY));
	vk::VkCommandBuffer					targetCmdBuffer		= (m_cmdBufferType == CMD_BUFFER_SECONDARY) ? *secondaryCmdBuffer : *cmdBuffer;

	// Begin cmd buffer
	beginCommandBuffer(vk, *cmdBuffer);

	if (m_cmdBufferType == CMD_BUFFER_SECONDARY)
	{
		// Begin secondary command buffer
		const vk::VkCommandBufferInheritanceInfo	bufferInheritanceInfo	=
		{
			vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,		// sType
			DE_NULL,													// pNext
			DE_NULL,													// renderPass
			0u,															// subpass
			DE_NULL,													// framebuffer
			VK_FALSE,													// occlusionQueryEnable
			(vk::VkQueryControlFlags)0u,								// queryFlags
			(vk::VkQueryPipelineStatisticFlags)0u,						// pipelineStatistics
		};
		beginSecondaryCommandBuffer(vk, *secondaryCmdBuffer, bufferInheritanceInfo);
	}

	// Start src buffer barrier
	{
		const vk::VkBufferMemoryBarrier startBufferBarrier =
		{
			vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType		sType
			DE_NULL,										// const void*			pNext
			0,												// VkAccessFlags		srcAccessMask
			vk::VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags		dstAccessMask
			queueFamilyIndex,								// uint32_t				srcQueueFamilyIndex
			queueFamilyIndex,								// uint32_t				dstQueueFamilyIndex
			**srcBuffer,									// VkBuffer				buffer
			0u,												// VkDeviceSize			offset
			VK_WHOLE_SIZE,									// VkDeviceSize			size
		};
		vk.cmdPipelineBarrier(targetCmdBuffer,
							  vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
							  vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
							  (vk::VkDependencyFlags) 0,
							  0, (const vk::VkMemoryBarrier *) DE_NULL,
							  1, &startBufferBarrier,
							  0, (const vk::VkImageMemoryBarrier *) DE_NULL);
	}
	vk.cmdFillBuffer(targetCmdBuffer, **srcBuffer, 0u, VK_WHOLE_SIZE, m_fillValue);

	{
		// Barrier to change accessMask to transfer read bit for source buffer
		const vk::VkBufferMemoryBarrier startCopyBufferBarrier =
		{
			vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,		// VkStructureType		sType
			DE_NULL,											// const void*			pNext
			vk::VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags		srcAccessMask
			vk::VK_ACCESS_TRANSFER_READ_BIT,					// VkAccessFlags		dstAccessMask
			queueFamilyIndex,									// uint32_t				srcQueueFamilyIndex
			queueFamilyIndex,									// uint32_t				dstQueueFamilyIndex
			**srcBuffer,										// VkBuffer				buffer
			0u,													// VkDeviceSize			offset
			VK_WHOLE_SIZE,										// VkDeviceSize			size
		};

		// Start image barrier for destination image.
		const vk::VkImageMemoryBarrier	startImgBarrier		=
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType		sType
			DE_NULL,											// const void*			pNext
			0,													// VkAccessFlags		srcAccessMask
			vk::VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags		dstAccessMask
			vk::VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout		oldLayout
			vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout		newLayout
			queueFamilyIndex,									// uint32_t				srcQueueFamilyIndex
			queueFamilyIndex,									// uint32_t				dstQueueFamilyIndex
			**colorImage,										// VkImage				image
			{
				vk::VK_IMAGE_ASPECT_COLOR_BIT,					// VkImageAspectFlags	aspectMask
				0u,												// uint32_t				baseMipLevel
				1u,												// uint32_t				mipLevels
				0u,												// uint32_t				baseArraySlice
				1u,												// uint32_t				subresourceRange
			}
		};

		vk.cmdPipelineBarrier(targetCmdBuffer,
							  vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
							  vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
							  (vk::VkDependencyFlags)0,
							  0, (const vk::VkMemoryBarrier*)DE_NULL,
							  1, &startCopyBufferBarrier,
							  1, &startImgBarrier);
	}

	// Copy buffer to image
	const vk::VkImageSubresourceLayers	subresourceLayers	=
	{
		vk::VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags		aspectMask
		0u,								// uint32_t					mipLevel
		0u,								// uint32_t					baseArrayLayer
		1u,								// uint32_t					layerCount
	};
	const vk::VkOffset3D				nullOffset			= {0u, 0u, 0u};
	const vk::VkExtent3D				imageExtent			= {(deUint32)RENDER_WIDTH, (deUint32)RENDER_HEIGHT, 1u};
	const vk::VkBufferImageCopy			copyRegion			=
	{
		0ull,							// VkDeviceSize				srcOffset;
		0,								// uint32_t					bufferRowLength
		0,								// uint32_t					bufferImageHeight
		subresourceLayers,				// VkImageSubresourceLayers	imageSubresource
		nullOffset,						// VkOffset3D				imageOffset
		imageExtent,					// VkExtent3D				imageExtent
	};
	vk.cmdCopyBufferToImage(targetCmdBuffer, **srcBuffer, **colorImage, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);

	{
		const vk::VkImageMemoryBarrier	endImgBarrier		=
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType		sType
			DE_NULL,											// const void*			pNext
			vk::VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags		srcAccessMask
			vk::VK_ACCESS_SHADER_READ_BIT,						// VkAccessFlags		dstAccessMask
			vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout		oldLayout
			vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,		// VkImageLayout		newLayout
			queueFamilyIndex,									// uint32_t				srcQueueFamilyIndex
			queueFamilyIndex,									// uint32_t				dstQueueFamilyIndex
			**colorImage,										// VkImage				image
			{
				vk::VK_IMAGE_ASPECT_COLOR_BIT,					// VkImageAspectFlags	aspectMask
				0u,												// uint32_t				baseMipLevel
				1u,												// uint32_t				mipLevels
				0u,												// uint32_t				baseArraySlice
				1u,												// uint32_t				subresourceRange
			}
		};
		vk.cmdPipelineBarrier(targetCmdBuffer,
							  vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
							  vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
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
		<< tcu::TestLog::Message << "Fill value: " << m_fillValue << tcu::TestLog::EndMessage;

	// Validate resulting image
	if (m_validator.validateImage(ctx, m_refData, **colorImage, m_imageFormat, vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
		return tcu::TestStatus::pass("Everything went OK");
	else
		return tcu::TestStatus::fail("Something went really wrong");
}

tcu::TestCaseGroup*	createCopyBufferToImageTests (tcu::TestContext& testCtx, CmdBufferType cmdBufferType)
{
	struct {
		const union {
			float		flt;
			deUint32	uint;
		}						fillValue;
		const ValidationData	data;
	} testData[] = {
		{	{ 0.0f },
			{
				{ tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 0.0f),
				  tcu::Vec4(0.1f, 0.1f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.0f, 0.0f), },
				{ tcu::Vec4(0.0f), tcu::Vec4(0.0f),
				  tcu::Vec4(0.0f), tcu::Vec4(0.0f), }
			}
		},
		{	{ 1.0f },
			{
				{ tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 0.0f),
				  tcu::Vec4(0.1f, 0.1f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.0f, 0.0f), },
				{ tcu::Vec4(1.0f), tcu::Vec4(1.0f),
				  tcu::Vec4(1.0f), tcu::Vec4(1.0f), }
			}
		},
		{	{ 0.2f },
			{
				{ tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 0.0f),
				  tcu::Vec4(0.1f, 0.1f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.0f, 0.0f), },
				{ tcu::Vec4(0.2f), tcu::Vec4(0.2f),
				  tcu::Vec4(0.2f), tcu::Vec4(0.2f), }
			}
		},
		{	{ 0.55f },
			{
				{ tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 0.0f),
				  tcu::Vec4(0.1f, 0.1f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.0f, 0.0f), },
				{ tcu::Vec4(0.55f), tcu::Vec4(0.55f),
				  tcu::Vec4(0.55f), tcu::Vec4(0.55f), }
			}
		},
		{	{ 0.82f },
			{
				{ tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 0.0f),
				  tcu::Vec4(0.1f, 0.1f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.0f, 0.0f), },
				{ tcu::Vec4(0.82f), tcu::Vec4(0.82f),
				  tcu::Vec4(0.82f), tcu::Vec4(0.82f), }
			}
		},
		{	{ 0.96f },
			{
				{ tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 0.0f),
				  tcu::Vec4(0.1f, 0.1f, 0.0f, 0.0f), tcu::Vec4(0.5f, 0.5f, 0.0f, 0.0f), },
				{ tcu::Vec4(0.96f), tcu::Vec4(0.96f),
				  tcu::Vec4(0.96f), tcu::Vec4(0.96f), }
			}
		},
	};

	de::MovePtr<tcu::TestCaseGroup>	copyStaticTests		(new tcu::TestCaseGroup(testCtx, "static", "Copy Buffer To Image Tests with static input"));

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(testData); ++ndx)
	{
		const std::string name = "copy_" + de::toString(ndx + 1);
		copyStaticTests->addChild(new CopyBufferToImageTestCase(testCtx, name.c_str(), testData[ndx].fillValue.uint, testData[ndx].data, cmdBufferType));
	}

	/* Add a few randomized tests */
	de::MovePtr<tcu::TestCaseGroup>	copyRandomTests		(new tcu::TestCaseGroup(testCtx, "random", "Copy Buffer To Image Tests with random input"));
	const int						testCount			= 10;
	de::Random						rnd					(testCtx.getCommandLine().getBaseSeed());
	for (int ndx = 0; ndx < testCount; ++ndx)
	{
		const std::string	name		= "copy_" + de::toString(ndx + 1);

		const union {
			float		flt;
			deUint32	uint;
		}					fillValue	= { rnd.getFloat(0.0, 1.0f) };

		const tcu::Vec4		refValue	(fillValue.flt);
		const tcu::Vec4		vec0		= tcu::randomVec4(rnd);
		const tcu::Vec4		vec1		= tcu::randomVec4(rnd);
		const tcu::Vec4		vec2		= tcu::randomVec4(rnd);
		const tcu::Vec4		vec3		= tcu::randomVec4(rnd);

		ValidationData		data		=
		{
			{ vec0, vec1, vec2, vec3 },
			{ refValue, refValue, refValue, refValue }
		};
		copyRandomTests->addChild(new CopyBufferToImageTestCase(testCtx, name.c_str(), fillValue.uint, data, cmdBufferType));
	}

	std::string groupName = getCmdBufferTypeStr(cmdBufferType);
	std::string groupDesc = "Copy Buffer To Image Tests with " + groupName + " command buffer";
	de::MovePtr<tcu::TestCaseGroup> copyTests (new tcu::TestCaseGroup(testCtx, groupName.c_str(), groupDesc.c_str()));
	copyTests->addChild(copyStaticTests.release());
	copyTests->addChild(copyRandomTests.release());
	return copyTests.release();
}

} // anonymous

tcu::TestCaseGroup*	createCopyBufferToImageTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> clearTests (new tcu::TestCaseGroup(testCtx, "copy_buffer_to_image", "Copy Buffer To Image Tests"));

	clearTests->addChild(createCopyBufferToImageTests(testCtx, CMD_BUFFER_PRIMARY));
	clearTests->addChild(createCopyBufferToImageTests(testCtx, CMD_BUFFER_SECONDARY));

	return clearTests.release();
}

} // ProtectedMem
} // vkt
