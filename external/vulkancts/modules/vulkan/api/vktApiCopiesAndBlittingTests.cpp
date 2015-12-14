/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by Khronos,
 * at which point this condition clause shall be removed.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief Vulkan Copies And Blitting Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiCopiesAndBlittingTests.hpp"

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "vkMemUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "tcuVectorType.hpp"

namespace vkt
{

namespace api
{

using namespace vk;

namespace
{

class CopiesAndBlittingTestInstance : public vkt::TestInstance
{
public:
										CopiesAndBlittingTestInstance		(Context&	context);
	virtual								~CopiesAndBlittingTestInstance		(void);
	virtual tcu::TestStatus				iterate								(void);
protected:
	Move<VkCommandPool>					m_cmdPool;
	Move<VkCommandBuffer>				m_cmdBuffer;
	Move<VkFence>						m_fence;

	virtual void						generateTestBuffer					()
										{};
	virtual void						generateExpectedResult				()
										{};
	virtual deBool						checkTestResult						(std::vector<deUint32> expected, std::vector<deUint32> actual);

	const VkCommandBufferBeginInfo		m_cmdBufferBeginInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,			// VkCommandBufferUsageFlags		flags;
		DE_NULL,												// VkRenderPass						renderPass;
		0u,														// deUint32							subpass;
		DE_NULL,												// VkFramebuffer					framebuffer;
		false,													// VkBool32							occlusionQueryEnable;
		0u,														// VkQueryControlFlags				queryFlags;
		0u														// VkQueryPipelineStatisticFlags	pipelineStatistics;
	};
};

CopiesAndBlittingTestInstance::~CopiesAndBlittingTestInstance	(void)
{
}

CopiesAndBlittingTestInstance::CopiesAndBlittingTestInstance	(Context& context)
	: vkt::TestInstance		(context)
{
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkDevice				vkDevice			= context.getDevice();
	const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();

	// Create command pool
	{
		const VkCommandPoolCreateInfo cmdPoolParams =
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,										// const void*			pNext;
			VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,			// VkCmdPoolCreateFlags	flags;
			queueFamilyIndex,								// deUint32				queueFamilyIndex;
		};

		m_cmdPool = createCommandPool(vk, vkDevice, &cmdPoolParams);
	}

	// Create command buffer
	{
		const VkCommandBufferAllocateInfo cmdBufferAllocateInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			*m_cmdPool,										// VkCommandPool			commandPool;
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCommandBufferLevel		level;
			1u												// deUint32					bufferCount;
		};

		m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, &cmdBufferAllocateInfo);
	}

	// Create fence
	{
		const VkFenceCreateInfo fenceParams =
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u										// VkFenceCreateFlags	flags;
		};

		m_fence = createFence(vk, vkDevice, &fenceParams);
	}
}

deBool CopiesAndBlittingTestInstance::checkTestResult(std::vector<deUint32> expected, std::vector<deUint32> actual)
{
	if (expected.size() != actual.size())
		return DE_FALSE;

	for (deUint32 i = 0; i < expected.size(); i++) {
		if (expected[i] != actual[i])
			return DE_FALSE;
	}
	return DE_TRUE;
}

tcu::TestStatus CopiesAndBlittingTestInstance::iterate (void)
{
	return tcu::TestStatus::pass("CopiesAndBlitting test");
}

class CopiesAndBlittingTestCase : public vkt::TestCase
{
public:
							CopiesAndBlittingTestCase	(tcu::TestContext&			testCtx,
														 const std::string&			name,
														 const std::string&			description)
								: vkt::TestCase			(testCtx, name, description)
							{}

	virtual					~CopiesAndBlittingTestCase	(void) {}
	virtual	void			initPrograms				(SourceCollections&			programCollection) const
							{}

	virtual TestInstance*	createInstance				(Context&					context) const
							{
								return new CopiesAndBlittingTestInstance(context);
							}
};

struct ImageToImageCaseParams {
	const VkExtent3D	srcExtent;
	const VkFormat		srcColorFormat;
	const VkExtent3D	dstExtent;
	const VkFormat		dstColorFormat;
};

//
class ImageToImageCopies : public CopiesAndBlittingTestInstance
{
public:
										ImageToImageCopies		(Context&	context, ImageToImageCaseParams params);
	//virtual								~ImageToImageCopies		(void);
	virtual tcu::TestStatus				iterate					(void);
private:
	Move<VkImage>						m_source;
	de::MovePtr<Allocation>				m_sourceImageAlloc;
	Move<VkImage>						m_destination;
	de::MovePtr<Allocation>				m_destinationImageAlloc;
	ImageToImageCaseParams				m_testParams;
};

ImageToImageCopies::ImageToImageCopies (Context &context, ImageToImageCaseParams params)
	: CopiesAndBlittingTestInstance(context)
	, m_testParams(params)
{
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkDevice				vkDevice			= context.getDevice();
	const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	SimpleAllocator				memAlloc			(vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));

	const VkImageCreateInfo sourceImageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType		sType;
		DE_NULL,								// const void*			pNext;
		0u,										// VkImageCreateFlags	flags;
		VK_IMAGE_TYPE_2D,						// VkImageType			imageType;
		m_testParams.srcColorFormat,			// VkFormat				format;
		m_testParams.srcExtent,					// VkExtent3D			extent;
		1u,										// deUint32				mipLevels;
		1u,										// deUint32				arraySize;
		VK_SAMPLE_COUNT_1_BIT,					// deUint32				samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling		tiling;
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT,		// VkImageUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
		1u,										// deUint32				queueFamilyCount;
		&queueFamilyIndex,						// const deUint32*		pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,	// VkImageLayout		initialLayout;
	};

	m_source = createImage(vk, vkDevice, &sourceImageParams);
	m_sourceImageAlloc = memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_source), MemoryRequirement::Any);
	VK_CHECK(vk.bindImageMemory(vkDevice, *m_source, m_sourceImageAlloc->getMemory(), m_sourceImageAlloc->getOffset()));

	const VkImageCreateInfo destinationImageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType		sType;
		DE_NULL,								// const void*			pNext;
		0u,										// VkImageCreateFlags	flags;
		VK_IMAGE_TYPE_2D,						// VkImageType			imageType;
		m_testParams.dstColorFormat,			// VkFormat				format;
		m_testParams.dstExtent,					// VkExtent3D			extent;
		1u,										// deUint32				mipLevels;
		1u,										// deUint32				arraySize;
		VK_SAMPLE_COUNT_1_BIT,					// deUint32				samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling		tiling;
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,		// VkImageUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
		1u,										// deUint32				queueFamilyCount;
		&queueFamilyIndex,						// const deUint32*		pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,	// VkImageLayout		initialLayout;
	};

	m_destination = createImage(vk, vkDevice, &destinationImageParams);
	m_destinationImageAlloc = memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_destination), MemoryRequirement::Any);
	VK_CHECK(vk.bindImageMemory(vkDevice, *m_destination, m_destinationImageAlloc->getMemory(), m_destinationImageAlloc->getOffset()));
}

tcu::TestStatus ImageToImageCopies::iterate()
{
	generateTestBuffer();
	generateExpectedResult();

	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();

	const VkImageSubresourceLayers sourceLayer {
		VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
		0u,							// uint32_t				mipLevel;
		0u,							// uint32_t				baseArrayLayer;
		1u,							// uint32_t				layerCount;
	};

	const VkImageCopy imageCopy =
	{
		sourceLayer,	// VkImageSubresourceLayers	srcSubresource;
		{0, 0, 0},		// VkOffset3D				srcOffset;
		sourceLayer,	// VkImageSubresourceLayers	dstSubresource;
		{0, 0, 0},		// VkOffset3D				dstOffset;
		{16, 16, 1},	// VkExtent3D				extent;
	};

	VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &m_cmdBufferBeginInfo));
	vk.cmdCopyImage(*m_cmdBuffer, m_source.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_destination.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);
	VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

	const VkSubmitInfo submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,						// const void*				pNext;
		0u,								// deUint32					waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*		pWaitSemaphores;
		1u,								// deUint32					commandBufferCount;
		&m_cmdBuffer.get(),				// const VkCommandBuffer*	pCommandBuffers;
		0u,								// deUint32					signalSemaphoreCount;
		DE_NULL							// const VkSemaphore*		pSignalSemaphores;
	};

	VK_CHECK(vk.resetFences(vkDevice, 1, &m_fence.get()));
	VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *m_fence));
	VK_CHECK(vk.waitForFences(vkDevice, 1, &m_fence.get(), true, ~(0ull) /* infinity */));

	// checkTestResult();

	return tcu::TestStatus::fail("Unimplemented!");
}

class ImageToImageTestCase : public vkt::TestCase
{
public:
							ImageToImageTestCase		(tcu::TestContext&				testCtx,
														 const std::string&				name,
														 const std::string&				description,
														 const ImageToImageCaseParams	params)
								: vkt::TestCase			(testCtx, name, description)
								, m_testParams			(params)
							{}

	virtual					~ImageToImageTestCase		(void) {}
	virtual	void			initPrograms				(SourceCollections&				programCollection) const
							{}

	virtual TestInstance*	createInstance				(Context&						context) const
							{
								return new ImageToImageCopies(context, m_testParams);
							}
private:
	ImageToImageCaseParams		m_testParams;
};

} // anonymous

tcu::TestCaseGroup* createCopiesAndBlittingTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	copiesAndBlittingTests	(new tcu::TestCaseGroup(testCtx, "access", "Copies And Blitting Tests"));

	{
		std::ostringstream description;
		description << " Copies And Blitting ";
		copiesAndBlittingTests->addChild(new CopiesAndBlittingTestCase(testCtx, "copies_and_blitting_tests_complete", description.str()));
	}

	{
		std::ostringstream description;
		const ImageToImageCaseParams params =
		{
			{256, 256, 1},
			VK_FORMAT_R32_UINT,
			{256, 256, 1},
			VK_FORMAT_R8G8B8A8_UINT,
		};
		description << "Copy from image to image";
		copiesAndBlittingTests->addChild(new ImageToImageTestCase(testCtx, "imageToImageWhole", description.str(), params));
	}

	return copiesAndBlittingTests.release();
}

} // api
} // vkt
