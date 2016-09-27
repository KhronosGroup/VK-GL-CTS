/*------------------------------------------------------------------------
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
 * \brief Vulkan Fill Buffer Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiFillBufferTests.hpp"

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorType.hpp"

namespace vkt
{

namespace api
{

using namespace vk;

namespace
{

struct TestParams
{
	enum { TEST_DATA_SIZE = 256 };

	VkDeviceSize	dstSize;
	VkDeviceSize	dstOffset;
	VkDeviceSize	size;
	deUint32		testData[TEST_DATA_SIZE];
};

class FillBufferTestInstance : public vkt::TestInstance
{
public:
										FillBufferTestInstance		(Context&	context,
																	 TestParams	testParams);
	virtual tcu::TestStatus				iterate						(void);
protected:
	const TestParams					m_params;

	Move<VkCommandPool>					m_cmdPool;
	Move<VkCommandBuffer>				m_cmdBuffer;
	Move<VkFence>						m_fence;
	de::MovePtr<tcu::TextureLevel>		m_destinationTextureLevel;
	de::MovePtr<tcu::TextureLevel>		m_expectedTextureLevel;

	VkCommandBufferBeginInfo			m_cmdBufferBeginInfo;

	Move<VkBuffer>						m_destination;
	de::MovePtr<Allocation>				m_destinationBufferAlloc;

	void								generateBuffer				(tcu::PixelBufferAccess buffer, int width, int height, int depth = 1);
	virtual void						generateExpectedResult		(void);
	void								uploadBuffer				(tcu::ConstPixelBufferAccess bufferAccess, const Allocation& bufferAlloc);
	virtual tcu::TestStatus				checkTestResult				(tcu::ConstPixelBufferAccess result);
	deUint32							calculateSize				(tcu::ConstPixelBufferAccess src) const
										{
											return src.getWidth() * src.getHeight() * src.getDepth() * tcu::getPixelSize(src.getFormat());
										}
};

FillBufferTestInstance::FillBufferTestInstance (Context& context, TestParams testParams)
	: vkt::TestInstance	(context)
	, m_params			(testParams)
{
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkDevice				vkDevice			= context.getDevice();
	const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	Allocator&					memAlloc			= context.getDefaultAllocator();

	// Create command pool
	{
		const VkCommandPoolCreateInfo		cmdPoolParams			=
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
		const VkCommandBufferAllocateInfo	cmdBufferAllocateInfo	=
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
		const VkFenceCreateInfo				fenceParams				=
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u										// VkFenceCreateFlags	flags;
		};

		m_fence = createFence(vk, vkDevice, &fenceParams);
	}

	// Create desctination buffer
	{
		const VkBufferCreateInfo			destinationBufferParams	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			m_params.dstSize,							// VkDeviceSize			size;
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyIndexCount;
			&queueFamilyIndex,							// const deUint32*		pQueueFamilyIndices;
		};

		m_destination				= createBuffer(vk, vkDevice, &destinationBufferParams);
		m_destinationBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_destination), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_destination, m_destinationBufferAlloc->getMemory(), m_destinationBufferAlloc->getOffset()));
	}
}

tcu::TestStatus FillBufferTestInstance::iterate (void)
{
	const int	dstLevelWidth		= (int)(m_params.dstSize / 4);
	m_destinationTextureLevel		= de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(mapVkFormat(VK_FORMAT_R8G8B8A8_UINT), dstLevelWidth, 1));

	generateBuffer(m_destinationTextureLevel->getAccess(), dstLevelWidth, 1, 1);

	generateExpectedResult();

	uploadBuffer(m_destinationTextureLevel->getAccess(), *m_destinationBufferAlloc);

	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();

	const VkBufferMemoryBarrier		dstBufferBarrier	=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		DE_NULL,									// const void*		pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex;
		*m_destination,								// VkBuffer			buffer;
		0u,											// VkDeviceSize		offset;
		m_params.dstOffset							// VkDeviceSize		size;
	};

	const VkCommandBufferBeginInfo	cmdBufferBeginInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,			// VkCommandBufferUsageFlags		flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));
	vk.cmdFillBuffer(*m_cmdBuffer, *m_destination, m_params.dstOffset, m_params.size, m_params.testData[0]);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &dstBufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

	const VkSubmitInfo				submitInfo			=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,						// const void*				pNext;
		0u,								// deUint32					waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*		pWaitSemaphores;
		(const VkPipelineStageFlags*)DE_NULL,
		1u,								// deUint32					commandBufferCount;
		&m_cmdBuffer.get(),				// const VkCommandBuffer*	pCommandBuffers;
		0u,								// deUint32					signalSemaphoreCount;
		DE_NULL							// const VkSemaphore*		pSignalSemaphores;
	};

	VK_CHECK(vk.resetFences(vkDevice, 1, &m_fence.get()));
	VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *m_fence));
	VK_CHECK(vk.waitForFences(vkDevice, 1, &m_fence.get(), true, ~(0ull) /* infinity */));

	// Read buffer data
	de::MovePtr<tcu::TextureLevel>	resultLevel	(new tcu::TextureLevel(m_destinationTextureLevel->getAccess().getFormat(), dstLevelWidth, 1));
	invalidateMappedMemoryRange(vk, vkDevice, m_destinationBufferAlloc->getMemory(), m_destinationBufferAlloc->getOffset(), m_params.dstOffset);
	tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(resultLevel->getFormat(), resultLevel->getSize(), m_destinationBufferAlloc->getHostPtr()));

	return checkTestResult(resultLevel->getAccess());
}

void FillBufferTestInstance::generateBuffer (tcu::PixelBufferAccess buffer, int width, int height, int depth)
{
	for (int z = 0; z < depth; z++)
	{
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
				buffer.setPixel(tcu::UVec4(x, y, z, 255), x, y, z);
		}
	}
}

void FillBufferTestInstance::uploadBuffer (tcu::ConstPixelBufferAccess bufferAccess, const Allocation& bufferAlloc)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const deUint32				bufferSize	= calculateSize(bufferAccess);

	// Write buffer data
	deMemcpy(bufferAlloc.getHostPtr(), bufferAccess.getDataPtr(), bufferSize);
	flushMappedMemoryRange(vk, vkDevice, bufferAlloc.getMemory(), bufferAlloc.getOffset(), bufferSize);
}

tcu::TestStatus FillBufferTestInstance::checkTestResult (tcu::ConstPixelBufferAccess result)
{
	const tcu::ConstPixelBufferAccess	expected	= m_expectedTextureLevel->getAccess();
	const tcu::UVec4					threshold	(0, 0, 0, 0);

	if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparsion", expected, result, threshold, tcu::COMPARE_LOG_RESULT))
		return tcu::TestStatus::fail("Fill and Update Buffer test");

	return tcu::TestStatus::pass("Fill and Update Buffer test");
}

void FillBufferTestInstance::generateExpectedResult (void)
{
	const tcu::ConstPixelBufferAccess	dst	= m_destinationTextureLevel->getAccess();

	m_expectedTextureLevel	= de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(dst.getFormat(), dst.getWidth(), dst.getHeight(), dst.getDepth()));
	tcu::copy(m_expectedTextureLevel->getAccess(), dst);

	deUint32*	currentPtr	= (deUint32*) m_expectedTextureLevel->getAccess().getDataPtr() + m_params.dstOffset / 4;
	deUint32*	endPtr		= currentPtr + m_params.size / 4;

	while (currentPtr < endPtr)
	{
		*currentPtr = m_params.testData[0];
		currentPtr++;
	}
}

class FillBufferTestCase : public vkt::TestCase
{
public:
							FillBufferTestCase	(tcu::TestContext&	testCtx,
												 const std::string&	name,
												 const std::string&	description,
												 const TestParams	params)
								: vkt::TestCase	(testCtx, name, description)
								, m_params		(params)
							{}

	virtual TestInstance*	createInstance		(Context&			context) const
							{
								return (TestInstance*) new FillBufferTestInstance(context, m_params);
							}
private:
	const TestParams		m_params;
};

// Update Buffer

class UpdateBufferTestInstance : public FillBufferTestInstance
{
public:
										UpdateBufferTestInstance		(Context&	context,
																		 TestParams	testParams)
											: FillBufferTestInstance(context, testParams)
										{}
	virtual tcu::TestStatus				iterate							(void);

protected:
	virtual void						generateExpectedResult			(void);
};

tcu::TestStatus UpdateBufferTestInstance::iterate (void)
{
	const int	dstLevelWidth		= (int)(m_params.dstSize / 4);
	m_destinationTextureLevel		= de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(mapVkFormat(VK_FORMAT_R8G8B8A8_UINT), dstLevelWidth, 1));

	generateBuffer(m_destinationTextureLevel->getAccess(), dstLevelWidth, 1, 1);

	generateExpectedResult();

	uploadBuffer(m_destinationTextureLevel->getAccess(), *m_destinationBufferAlloc);

	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();

	const VkBufferMemoryBarrier		dstBufferBarrier	=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		DE_NULL,									// const void*		pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex;
		*m_destination,								// VkBuffer			buffer;
		0u,											// VkDeviceSize		offset;
		m_params.dstOffset							// VkDeviceSize		size;
	};

	const VkCommandBufferBeginInfo	cmdBufferBeginInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,			// VkCommandBufferUsageFlags		flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));
	vk.cmdUpdateBuffer(*m_cmdBuffer, *m_destination, m_params.dstOffset, m_params.size, m_params.testData);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &dstBufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

	const VkSubmitInfo				submitInfo			=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,						// const void*				pNext;
		0u,								// deUint32					waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*		pWaitSemaphores;
		(const VkPipelineStageFlags*)DE_NULL,
		1u,								// deUint32					commandBufferCount;
		&m_cmdBuffer.get(),				// const VkCommandBuffer*	pCommandBuffers;
		0u,								// deUint32					signalSemaphoreCount;
		DE_NULL							// const VkSemaphore*		pSignalSemaphores;
	};

	VK_CHECK(vk.resetFences(vkDevice, 1, &m_fence.get()));
	VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *m_fence));
	VK_CHECK(vk.waitForFences(vkDevice, 1, &m_fence.get(), true, ~(0ull) /* infinity */));

	// Read buffer data
	de::MovePtr<tcu::TextureLevel>	resultLevel	(new tcu::TextureLevel(m_destinationTextureLevel->getAccess().getFormat(), dstLevelWidth, 1));
	invalidateMappedMemoryRange(vk, vkDevice, m_destinationBufferAlloc->getMemory(), m_destinationBufferAlloc->getOffset(), m_params.dstOffset);
	tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(resultLevel->getFormat(), resultLevel->getSize(), m_destinationBufferAlloc->getHostPtr()));

	return checkTestResult(resultLevel->getAccess());
}

void UpdateBufferTestInstance::generateExpectedResult (void)
{
	const tcu::ConstPixelBufferAccess	dst	= m_destinationTextureLevel->getAccess();

	m_expectedTextureLevel	= de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(dst.getFormat(), dst.getWidth(), dst.getHeight(), dst.getDepth()));
	tcu::copy(m_expectedTextureLevel->getAccess(), dst);

	deUint32*	currentPtr	= (deUint32*) m_expectedTextureLevel->getAccess().getDataPtr() + m_params.dstOffset / 4;

	deMemcpy(currentPtr, m_params.testData, (size_t)m_params.size);
}

class UpdateBufferTestCase : public vkt::TestCase
{
public:
							UpdateBufferTestCase	(tcu::TestContext&	testCtx,
													 const std::string&	name,
													 const std::string&	description,
													 const TestParams	params)
								: vkt::TestCase	(testCtx, name, description)
								, m_params		(params)
							{}

	virtual TestInstance*	createInstance			(Context&			context) const
							{
								return (TestInstance*) new UpdateBufferTestInstance(context, m_params);
							}
private:
	TestParams				m_params;
};

} // anonymous

tcu::TestCaseGroup* createFillAndUpdateBufferTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	fillAndUpdateBufferTests	(new tcu::TestCaseGroup(testCtx, "fill_and_update_buffer", "Fill and Update Buffer Tests"));
	TestParams						params;
	params.dstSize = TestParams::TEST_DATA_SIZE;

	DE_ASSERT(params.dstSize <= TestParams::TEST_DATA_SIZE);
	deMemset(params.testData, 0xFFu, (size_t)params.dstSize);

	{
		const std::string	description	("whole buffer");
		const std::string	testName	("buffer_whole");

		params.dstOffset	= 0;
		params.size			= params.dstSize;

		fillAndUpdateBufferTests->addChild(new FillBufferTestCase(testCtx, "fill_" + testName, "Fill " + description, params));
		fillAndUpdateBufferTests->addChild(new UpdateBufferTestCase(testCtx, "update_" + testName, "Update " + description, params));
	}

	{
		const std::string	description	("first word in buffer");
		const std::string	testName	("buffer_first_one");

		params.dstOffset	= 0;
		params.size			= 4;

		fillAndUpdateBufferTests->addChild(new FillBufferTestCase(testCtx, "fill_" + testName, "Fill " + description, params));
		fillAndUpdateBufferTests->addChild(new UpdateBufferTestCase(testCtx, "update_" + testName, "Update " + description, params));
	}

	{
		const std::string	description	("second word in buffer");
		const std::string	testName	("buffer_second_one");

		params.dstOffset	= 4;
		params.size			= 4;

		fillAndUpdateBufferTests->addChild(new FillBufferTestCase(testCtx, "fill_" + testName, "Fill " + description, params));
		fillAndUpdateBufferTests->addChild(new UpdateBufferTestCase(testCtx, "update_" + testName, "Update " + description, params));
	}

	{
		const std::string	description	("buffer second part");
		const std::string	testName	("buffer_second_part");

		params.dstOffset	= params.dstSize / 2;
		params.size			= params.dstSize / 2;

		fillAndUpdateBufferTests->addChild(new FillBufferTestCase(testCtx, "fill_" + testName, "Fill " + description, params));
		fillAndUpdateBufferTests->addChild(new UpdateBufferTestCase(testCtx, "update_" + testName, "Update " + description, params));
	}

	return fillAndUpdateBufferTests.release();
}

} // api
} // vkt
