/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
 * Copyright (c) 2022 Valve Corporation.
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
 * \brief Test for conditional rendering with commands that ignore conditions
 *//*--------------------------------------------------------------------*/

#include "vktConditionalIgnoreTests.hpp"
#include "vktConditionalRenderingTestUtil.hpp"

#include "vktTestCase.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkImageWithMemory.hpp"

#include "vktTestCase.hpp"

#include "vkDefs.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuDefs.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"

#include <vector>
#include <sstream>
#include <algorithm>
#include <utility>
#include <iterator>
#include <string>
#include <limits>
#include <memory>
#include <functional>
#include <cstddef>
#include <set>


namespace vkt
{
namespace conditional
{
namespace
{

using namespace vk;

class ConditionalIgnoreClearTestCase : public vkt::TestCase
{
public:
	ConditionalIgnoreClearTestCase(tcu::TestContext& context, const std::string& name, const std::string& description, const ConditionalData& data);
	void            initPrograms            (SourceCollections&) const override { }
	TestInstance*   createInstance          (Context& context) const override;
	void            checkSupport            (Context& context) const override
	{
		context.requireDeviceFunctionality("VK_EXT_conditional_rendering");
		if (m_data.conditionInherited && !context.getConditionalRenderingFeaturesEXT().inheritedConditionalRendering)
			TCU_THROW(NotSupportedError, "Device does not support inherited conditional rendering");
	}
private:
	const ConditionalData m_data;
};


class ConditionalIgnoreClearTestInstance : public vkt::TestInstance
{
public:
	ConditionalIgnoreClearTestInstance(Context& context, const ConditionalData& data)
		: vkt::TestInstance (context)
		, m_data(data)
	{ };
	virtual tcu::TestStatus iterate (void);
private:
	const ConditionalData m_data;
};


ConditionalIgnoreClearTestCase::ConditionalIgnoreClearTestCase(tcu::TestContext& context, const std::string& name, const std::string& description, const ConditionalData& data)
	: vkt::TestCase (context, name, description)
	, m_data(data)
{ }

TestInstance* ConditionalIgnoreClearTestCase::createInstance(Context& context) const
{
	return new ConditionalIgnoreClearTestInstance(context, m_data);
}

//make a buffer to read an image back after rendering
std::unique_ptr<BufferWithMemory> makeBufferForImage(const DeviceInterface& vkd, const VkDevice device, Allocator& allocator, VkFormat imageFormat, VkExtent3D imageExtent)
{
	const auto	tcuFormat			= mapVkFormat(imageFormat);
	const auto	outBufferSize		= static_cast<VkDeviceSize>(static_cast<uint32_t>(tcu::getPixelSize(tcuFormat)) * imageExtent.width * imageExtent.height);
	const auto	outBufferUsage		= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const auto	outBufferInfo		= makeBufferCreateInfo(outBufferSize, outBufferUsage);

	auto outBuffer = std::unique_ptr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, outBufferInfo, MemoryRequirement::HostVisible));

	return outBuffer;
}

tcu::TestStatus ConditionalIgnoreClearTestInstance::iterate(void)
{
	const auto& vkd				= m_context.getDeviceInterface();
	const auto  device			= m_context.getDevice();
	auto&		alloc			= m_context.getDefaultAllocator();
	const auto	imageFormat		= VK_FORMAT_R8G8B8A8_UNORM;
	const auto	depthFormat		= VK_FORMAT_D16_UNORM;
	const auto	imageExtent		= makeExtent3D(2, 2, 1u);
	const auto	qIndex			= m_context.getUniversalQueueFamilyIndex();

	const auto							expected			= tcu::Vec4(0.0, 0.0, 0.0, 1.0);

	const VkClearColorValue				clearColor			= { { 0.0, 0.0, 0.0, 1.0 } };
	const VkClearColorValue				clearColorWrong		= { { 1.0, 0.0, 0.0, 1.0 } };

	const VkClearDepthStencilValue	depthClear			= {0.0, 0};
	const VkClearDepthStencilValue	depthClearWrong		= {1.0, 0};

	const tcu::IVec3 imageDim	(static_cast<int>(imageExtent.width), static_cast<int>(imageExtent.height), static_cast<int>(imageExtent.depth));
	const tcu::IVec2 imageSize	(imageDim.x(), imageDim.y());

	de::MovePtr<ImageWithMemory>  colorAttachment;
	de::MovePtr<ImageWithMemory>  depthAttachment;

	//create color image
	const auto  imageUsage      = static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	const VkImageCreateInfo imageCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType				sType;
		nullptr,								//	const void*					pNext;
		0u,										//	VkImageCreateFlags			flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType					imageType;
		imageFormat,							//	VkFormat					format;
		imageExtent,							//	VkExtent3D					extent;
		1u,										//	deUint32					mipLevels;
		1u,										//	deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits		samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling				tiling;
		imageUsage,								//	VkImageUsageFlags			usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode				sharingMode;
		0,										//	deUint32					queueFamilyIndexCount;
		nullptr,								//	const deUint32*				pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout				initialLayout;
	};

	const auto colorSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	colorAttachment						= de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, alloc, imageCreateInfo, MemoryRequirement::Any));
	auto colorAttachmentView			= makeImageView(vkd, device, colorAttachment->get(), VK_IMAGE_VIEW_TYPE_2D, imageFormat, colorSubresourceRange);

	//create depth image
	const auto  depthImageUsage =
		static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	const VkImageCreateInfo depthImageCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType			sType;
		nullptr,										// const void*				pNext;
		0u,												// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,								// VkImageType				imageType;
		depthFormat,									// VkFormat					format;
		imageExtent,									// VkExtent3D				extent;
		1u,												// deUint32					mipLevels;
		1u,												// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,							// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,						// VkImageTiling			tiling;
		depthImageUsage,								// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode			sharingMode;
		0u,												// deUint32					queueFamilyIndexCount;
		nullptr,										// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			initialLayout;
	};

	const auto depthSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u);
	depthAttachment						= de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, alloc, depthImageCreateInfo, MemoryRequirement::Any));
	auto depthAttachmentView			= makeImageView(vkd, device, depthAttachment->get(), VK_IMAGE_VIEW_TYPE_2D, depthFormat, depthSubresourceRange);

	//buffers to read the outputs
	const auto outBuffer			= makeBufferForImage(vkd, device, alloc, imageFormat, imageExtent);
	const auto& outBufferAlloc	= outBuffer->getAllocation();
	const void* outBufferData		= outBufferAlloc.getHostPtr();

	const auto outDepthBuffer		= makeBufferForImage(vkd, device, alloc, depthFormat, imageExtent);
	const auto& outDepthBufferAlloc	= outDepthBuffer->getAllocation();
	const void* outDepthBufferData	= outDepthBufferAlloc.getHostPtr();

	const auto commandPool	= createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, qIndex);
	auto commandBuffer		= allocateCommandBuffer(vkd, device, commandPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	auto commandBuffer2		= allocateCommandBuffer(vkd, device, commandPool.get(), VK_COMMAND_BUFFER_LEVEL_SECONDARY);

	auto conditionalBuffer = createConditionalRenderingBuffer(m_context, m_data);
	//prepare command buffers
	const bool useSecondaryCmdBuffer = m_data.conditionInherited || m_data.conditionInSecondaryCommandBuffer;

	VkCommandBufferInheritanceConditionalRenderingInfoEXT conditionalRenderingInheritanceInfo = initVulkanStructure();
	conditionalRenderingInheritanceInfo.conditionalRenderingEnable = m_data.conditionInherited ? VK_TRUE : VK_FALSE;

	const VkCommandBufferInheritanceInfo inheritanceInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		&conditionalRenderingInheritanceInfo,
		VK_NULL_HANDLE,									        // renderPass
		0u,														// subpass
		VK_NULL_HANDLE,									        // framebuffer
		VK_FALSE,												// occlusionQueryEnable
		(VkQueryControlFlags)0u,								// queryFlags
		(VkQueryPipelineStatisticFlags)0u,						// pipelineStatistics
	};

	const VkCommandBufferBeginInfo commandBufferBeginInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		nullptr,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		&inheritanceInfo
	};

	beginCommandBuffer(vkd, commandBuffer.get());
	//transition color and depth images
	VkImageMemoryBarrier colorTransition = makeImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, colorAttachment.get()->get(), colorSubresourceRange);
	VkImageMemoryBarrier depthTransition = makeImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, depthAttachment.get()->get(), depthSubresourceRange);
	VkImageMemoryBarrier barriers[] = {colorTransition, depthTransition};
	cmdPipelineImageMemoryBarrier(vkd, commandBuffer.get(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		barriers, DE_LENGTH_OF_ARRAY(barriers));

	//clear to the incorrect color
	vkd.cmdClearColorImage(commandBuffer.get(), colorAttachment.get()->get(), VK_IMAGE_LAYOUT_GENERAL, &clearColorWrong, 1, &colorSubresourceRange);
	vkd.cmdClearDepthStencilImage(commandBuffer.get(), depthAttachment.get()->get(), VK_IMAGE_LAYOUT_GENERAL, &depthClearWrong, 1, &depthSubresourceRange);

	const auto barrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
	cmdPipelineMemoryBarrier(vkd, commandBuffer.get(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &barrier);

	//do all combinations of clears
	if (useSecondaryCmdBuffer)
	{
		vkd.beginCommandBuffer(*commandBuffer2, &commandBufferBeginInfo);
		if (m_data.conditionInSecondaryCommandBuffer)
		{
			beginConditionalRendering(vkd, commandBuffer2.get(), *conditionalBuffer, m_data);
		}
		else
		{
			beginConditionalRendering(vkd, commandBuffer.get(), *conditionalBuffer, m_data);
		}

		//clear to the correct colors
		vkd.cmdClearColorImage(commandBuffer2.get(), colorAttachment.get()->get(), VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &colorSubresourceRange);
		vkd.cmdClearDepthStencilImage(commandBuffer2.get(), depthAttachment.get()->get(), VK_IMAGE_LAYOUT_GENERAL, &depthClear, 1, &depthSubresourceRange);

		if (m_data.conditionInSecondaryCommandBuffer)
		{
			vkd.cmdEndConditionalRenderingEXT(commandBuffer2.get());
		}
		else
		{
			vkd.cmdEndConditionalRenderingEXT(commandBuffer.get());
		}

		vkd.endCommandBuffer(*commandBuffer2);
		vkd.cmdExecuteCommands(commandBuffer.get(), 1, &commandBuffer2.get());
	}
	else
	{
		beginConditionalRendering(vkd, commandBuffer.get(), *conditionalBuffer, m_data);

		//clear to the correct colors
		vkd.cmdClearColorImage(commandBuffer.get(), colorAttachment.get()->get(), VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &colorSubresourceRange);
		vkd.cmdClearDepthStencilImage(commandBuffer.get(), depthAttachment.get()->get(), VK_IMAGE_LAYOUT_GENERAL, &depthClear, 1, &depthSubresourceRange);

		vkd.cmdEndConditionalRenderingEXT(commandBuffer.get());
	}
	copyImageToBuffer(vkd, commandBuffer.get(), colorAttachment.get()->get(), (*outBuffer).get(), imageSize,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);
	copyImageToBuffer(vkd, commandBuffer.get(), depthAttachment.get()->get(), (*outDepthBuffer).get(), imageSize,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, 1, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

	endCommandBuffer(vkd, commandBuffer.get());
	submitCommandsAndWait(vkd, device, m_context.getUniversalQueue(), commandBuffer.get());

	invalidateAlloc(vkd, device, outBufferAlloc);
	invalidateAlloc(vkd, device, outDepthBufferAlloc);
	tcu::ConstPixelBufferAccess outPixels(mapVkFormat(imageFormat), imageDim, outBufferData);
	tcu::ConstPixelBufferAccess outDepth(mapVkFormat(depthFormat), imageDim, outDepthBufferData);

	//the clears should happen in every case
	if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare color", "color image comparison", expected, outPixels, tcu::Vec4(0.0), tcu::COMPARE_LOG_ON_ERROR))
		return tcu::TestStatus::fail("Color image verification failed, check log for details");
	if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare depth", "depth image comparison", expected, outDepth, tcu::Vec4(0.0), tcu::COMPARE_LOG_ON_ERROR))
		return tcu::TestStatus::fail("Depth image verification failed, check log for details");

	return tcu::TestStatus::pass("Pass");
}

}	// anonymous

ConditionalIgnoreTests::ConditionalIgnoreTests(tcu::TestContext &testCtx)
	: TestCaseGroup	(testCtx, "conditional_ignore", "operations that ignore conditions")
{ }

ConditionalIgnoreTests::~ConditionalIgnoreTests(void)
{}

void ConditionalIgnoreTests::init (void)
{
	for (int conditionNdx = 0; conditionNdx < DE_LENGTH_OF_ARRAY(conditional::s_testsData); conditionNdx++)
	{
		const ConditionalData& conditionData = conditional::s_testsData[conditionNdx];

		if (conditionData.clearInRenderPass)
			continue;

		addChild(new ConditionalIgnoreClearTestCase(m_testCtx, std::string("clear_") + de::toString(conditionData).c_str(),
			"tests that some clear operations always happen", conditionData));
	}
}

}	// conditional
}	// vkt
