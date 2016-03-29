/*------------------------------------------------------------------------
 *  Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
 * \brief Vulkan Buffers Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiBufferTests.hpp"

#include "deStringUtil.hpp"
#include "gluVarType.hpp"
#include "tcuTestLog.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vktTestCase.hpp"

namespace vkt
{

using namespace vk;

namespace api
{

namespace
{

static const deUint32	MAX_BUFFER_SIZE_DIVISOR	= 16;

struct BufferCaseParameters
{
	VkBufferUsageFlags	usage;
	VkBufferCreateFlags	flags;
	VkSharingMode		sharingMode;
};

class BufferTestInstance : public TestInstance
{
public:
								BufferTestInstance			(Context&				ctx,
															 BufferCaseParameters	testCase)
									: TestInstance	(ctx)
									, m_testCase	(testCase)
								{}
	virtual tcu::TestStatus		iterate						(void);
	tcu::TestStatus				bufferCreateAndAllocTest	(VkDeviceSize		size);

private:
	BufferCaseParameters		m_testCase;
};

class BuffersTestCase : public TestCase
{
public:
							BuffersTestCase		(tcu::TestContext&		testCtx,
												 const std::string&		name,
												 const std::string&		description,
												 BufferCaseParameters	testCase)
								: TestCase(testCtx, name, description)
								, m_testCase(testCase)
							{}

	virtual					~BuffersTestCase	(void) {}
	virtual TestInstance*	createInstance		(Context&				ctx) const
							{
								tcu::TestLog& log	= m_testCtx.getLog();
								log << tcu::TestLog::Message << getBufferUsageFlagsStr(m_testCase.usage) << tcu::TestLog::EndMessage;
								return new BufferTestInstance(ctx, m_testCase);
							}

private:
	BufferCaseParameters		m_testCase;
};

 tcu::TestStatus BufferTestInstance::bufferCreateAndAllocTest (VkDeviceSize size)
{
	const VkPhysicalDevice		vkPhysicalDevice	= m_context.getPhysicalDevice();
	const InstanceInterface&	vkInstance			= m_context.getInstanceInterface();
	const VkDevice				vkDevice			= m_context.getDevice();
	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	Move<VkBuffer>				testBuffer;
	VkMemoryRequirements		memReqs;
	Move<VkDeviceMemory>		memory;
	const deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const VkPhysicalDeviceMemoryProperties	memoryProperties = getPhysicalDeviceMemoryProperties(vkInstance, vkPhysicalDevice);

	// Create buffer
	{
		VkBufferCreateInfo		bufferParams		=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			DE_NULL,
			m_testCase.flags,
			size,
			m_testCase.usage,
			m_testCase.sharingMode,
			1u,										//	deUint32			queueFamilyCount;
			&queueFamilyIndex,
		};

		try
		{
			testBuffer = createBuffer(vk, vkDevice, &bufferParams);
		}
		catch (const vk::Error& error)
		{
			return tcu::TestStatus::fail("Buffer creation failed! (requested memory size: " + de::toString(size) + ", Error code: " + de::toString(error.getMessage()) + ")");
		}

		vk.getBufferMemoryRequirements(vkDevice, *testBuffer, &memReqs);

		const deUint32		heapTypeIndex	= (deUint32)deCtz32(memReqs.memoryTypeBits);
		const VkMemoryType	memoryType		= memoryProperties.memoryTypes[heapTypeIndex];
		const VkMemoryHeap	memoryHeap		= memoryProperties.memoryHeaps[memoryType.heapIndex];
		const VkDeviceSize	maxBufferSize	= memoryHeap.size / MAX_BUFFER_SIZE_DIVISOR;
		// If the requested size is too large, clamp it based on the selected heap size
		if (size > maxBufferSize)
		{
			size = maxBufferSize;
			bufferParams.size = size;
			try
			{
				// allocate a new buffer with the adjusted size, the old one will be destroyed by the smart pointer
				testBuffer = createBuffer(vk, vkDevice, &bufferParams);
			}
			catch (const vk::Error& error)
			{
				return tcu::TestStatus::fail("Buffer creation failed! (requested memory size: " + de::toString(size) + ", Error code: " + de::toString(error.getMessage()) + ")");
			}
			vk.getBufferMemoryRequirements(vkDevice, *testBuffer, &memReqs);
		}

		if (size > memReqs.size)
		{
			std::ostringstream errorMsg;
			errorMsg << "Requied memory size (" << memReqs.size << " bytes) smaller than the buffer's size (" << size << " bytes)!";
			return tcu::TestStatus::fail(errorMsg.str());
		}
	}

	// Allocate and bind memory
	{
		const VkMemoryAllocateInfo memAlloc =
		{
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			NULL,
			memReqs.size,
			(deUint32)deCtz32(memReqs.memoryTypeBits)	//	deUint32		memoryTypeIndex
		};

		try
		{
			memory = allocateMemory(vk, vkDevice, &memAlloc, (const VkAllocationCallbacks*)DE_NULL);
		}
		catch (const vk::Error& error)
		{
			return tcu::TestStatus::fail("Alloc memory failed! (requested memory size: " + de::toString(size) + ", Error code: " + de::toString(error.getMessage()) + ")");
		}

		if ((m_testCase.flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT) ||
			(m_testCase.flags & VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT) ||
			(m_testCase.flags & VK_BUFFER_CREATE_SPARSE_ALIASED_BIT))
		{
			VkQueue queue												= 0;

			vk.getDeviceQueue(vkDevice, queueFamilyIndex, 0, &queue);

			const VkSparseMemoryBind			sparseMemoryBind		=
			{
				0,										// VkDeviceSize								resourceOffset;
				memReqs.size,							// VkDeviceSize								size;
				*memory,								// VkDeviceMemory							memory;
				0,										// VkDeviceSize								memoryOffset;
				0										// VkSparseMemoryBindFlags					flags;
			};

			const VkSparseBufferMemoryBindInfo	sparseBufferMemoryBindInfo	=
			{
				*testBuffer,							// VkBuffer									buffer;
				1u,										// deUint32									bindCount;
				&sparseMemoryBind						// const VkSparseMemoryBind*				pBinds;
			};

			const VkBindSparseInfo				bindSparseInfo			=
			{
				VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,		// VkStructureType							sType;
				DE_NULL,								// const void*								pNext;
				0,										// deUint32									waitSemaphoreCount;
				DE_NULL,								// const VkSemaphore*						pWaitSemaphores;
				1u,										// deUint32									bufferBindCount;
				&sparseBufferMemoryBindInfo,			// const VkSparseBufferMemoryBindInfo*		pBufferBinds;
				0,										// deUint32									imageOpaqueBindCount;
				DE_NULL,								// const VkSparseImageOpaqueMemoryBindInfo*	pImageOpaqueBinds;
				0,										// deUint32									imageBindCount;
				DE_NULL,								// const VkSparseImageMemoryBindInfo*		pImageBinds;
				0,										// deUint32									signalSemaphoreCount;
				DE_NULL,								// const VkSemaphore*						pSignalSemaphores;
			};

			const VkFenceCreateInfo fenceParams =
			{
				VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// VkStructureType		sType;
				DE_NULL,								// const void*			pNext;
				0u										// VkFenceCreateFlags	flags;
			};

			const vk::Unique<vk::VkFence> fence(vk::createFence(vk, vkDevice, &fenceParams));

			VK_CHECK(vk.resetFences(vkDevice, 1, &fence.get()));
			if (vk.queueBindSparse(queue, 1, &bindSparseInfo, *fence) != VK_SUCCESS)
				return tcu::TestStatus::fail("Bind sparse buffer memory failed! (requested memory size: " + de::toString(size) + ")");

			VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), VK_TRUE, ~(0ull) /* infinity */));
		} else
			if (vk.bindBufferMemory(vkDevice, *testBuffer, *memory, 0) != VK_SUCCESS)
				return tcu::TestStatus::fail("Bind buffer memory failed! (requested memory size: " + de::toString(size) + ")");
	}

	return tcu::TestStatus::pass("Buffer test");
}

tcu::TestStatus BufferTestInstance::iterate (void)
{
	const VkPhysicalDeviceFeatures&	physicalDeviceFeatures	= m_context.getDeviceFeatures();

	if ((m_testCase.flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT ) && !physicalDeviceFeatures.sparseBinding)
		TCU_THROW(NotSupportedError, "Sparse bindings feature is not supported");

	if ((m_testCase.flags & VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT ) && !physicalDeviceFeatures.sparseResidencyBuffer)
		TCU_THROW(NotSupportedError, "Sparse buffer residency feature is not supported");

	if ((m_testCase.flags & VK_BUFFER_CREATE_SPARSE_ALIASED_BIT ) && !physicalDeviceFeatures.sparseResidencyAliased)
		TCU_THROW(NotSupportedError, "Sparse aliased residency feature is not supported");

	const VkDeviceSize testSizes[] =
	{
		1,
		1181,
		15991,
		16384
	};
	tcu::TestStatus					testStatus			= tcu::TestStatus::pass("Buffer test");

	for (int i = 0; i < DE_LENGTH_OF_ARRAY(testSizes); i++)
	{
		if ((testStatus = bufferCreateAndAllocTest(testSizes[i])).getCode() != QP_TEST_RESULT_PASS)
			return testStatus;
	}

	if (m_testCase.usage & (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
	{
		const VkPhysicalDevice					vkPhysicalDevice	= m_context.getPhysicalDevice();
		const InstanceInterface&				vkInstance			= m_context.getInstanceInterface();
		VkPhysicalDeviceProperties	props;

		vkInstance.getPhysicalDeviceProperties(vkPhysicalDevice, &props);
		testStatus = bufferCreateAndAllocTest((VkDeviceSize) props.limits.maxTexelBufferElements);
	}

	return testStatus;
}

} // anonymous

 tcu::TestCaseGroup* createBufferTests (tcu::TestContext& testCtx)
{
	const VkBufferUsageFlags bufferUsageModes[] =
	{
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,
		VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
	};

	const VkBufferCreateFlags bufferCreateFlags[] =
	{
		VK_BUFFER_CREATE_SPARSE_BINDING_BIT,
		VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT,
		VK_BUFFER_CREATE_SPARSE_ALIASED_BIT
	};

	de::MovePtr<tcu::TestCaseGroup>	buffersTests	(new tcu::TestCaseGroup(testCtx, "buffer", "Buffer Tests"));

	deUint32	numberOfBufferUsageFlags			= DE_LENGTH_OF_ARRAY(bufferUsageModes);
	deUint32	numberOfBufferCreateFlags			= DE_LENGTH_OF_ARRAY(bufferCreateFlags);
	deUint32	maximumValueOfBufferUsageFlags		= (1 << (numberOfBufferUsageFlags - 1)) - 1;
	deUint32	maximumValueOfBufferCreateFlags		= (1 << (numberOfBufferCreateFlags)) - 1;

	for (deUint32 combinedBufferCreateFlags = 0; combinedBufferCreateFlags <= maximumValueOfBufferCreateFlags; combinedBufferCreateFlags++)
	{
		for (deUint32 combinedBufferUsageFlags = 1; combinedBufferUsageFlags <= maximumValueOfBufferUsageFlags; combinedBufferUsageFlags++)
		{
			if (combinedBufferCreateFlags == VK_BUFFER_CREATE_SPARSE_ALIASED_BIT)
			{
				// spec says: If flags contains VK_BUFFER_CREATE_SPARSE_ALIASED_BIT, it must also contain at least one of
				// VK_BUFFER_CREATE_SPARSE_BINDING_BIT or VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT
				continue;
			}
			BufferCaseParameters	testParams =
			{
				combinedBufferUsageFlags,
				combinedBufferCreateFlags,
				VK_SHARING_MODE_EXCLUSIVE
			};
			std::ostringstream	testName;
			std::ostringstream	testDescription;
			testName << "createBuffer_" << combinedBufferUsageFlags << "_" << combinedBufferCreateFlags;
			testDescription << "vkCreateBuffer test " << combinedBufferUsageFlags << " " << combinedBufferCreateFlags;
			buffersTests->addChild(new BuffersTestCase(testCtx, testName.str(), testDescription.str(), testParams));
		}
	}

	return buffersTests.release();
}

} // api
} // vk
