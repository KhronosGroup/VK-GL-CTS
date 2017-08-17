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
#include "vkPlatform.hpp"
#include "vktTestCase.hpp"
#include "tcuPlatform.hpp"

namespace vkt
{
namespace api
{
namespace
{
using namespace vk;

PlatformMemoryLimits getPlatformMemoryLimits (Context& context)
{
	PlatformMemoryLimits	memoryLimits;

	context.getTestContext().getPlatform().getVulkanPlatform().getMemoryLimits(memoryLimits);

	return memoryLimits;
}

VkDeviceSize getMaxBufferSize(const VkDeviceSize& bufferSize,
							  const VkDeviceSize& alignment,
							  const PlatformMemoryLimits& limits)
{
	VkDeviceSize size = bufferSize;

	if (limits.totalDeviceLocalMemory == 0)
	{
		// 'UMA' systems where device memory counts against system memory
		size = std::min(bufferSize, limits.totalSystemMemory - alignment);
	}
	else
	{
		// 'LMA' systems where device memory is local to the GPU
		size = std::min(bufferSize, limits.totalDeviceLocalMemory - alignment);
	}

	return size;
}

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
									: TestInstance		(ctx)
									, m_testCase		(testCase)
									, m_sparseContext	(createSparseContext())
								{}
	virtual tcu::TestStatus		iterate						(void);
	tcu::TestStatus				bufferCreateAndAllocTest	(VkDeviceSize		size);

private:
	BufferCaseParameters		m_testCase;

private:
	// Custom context for sparse cases
	struct SparseContext
	{
		SparseContext (Move<VkDevice>& device, const deUint32 queueFamilyIndex, const InstanceInterface& interface)
		: m_device				(device)
		, m_queueFamilyIndex	(queueFamilyIndex)
		, m_deviceInterface		(interface, *m_device)
		{}

		Unique<VkDevice>	m_device;
		const deUint32		m_queueFamilyIndex;
		DeviceDriver		m_deviceInterface;
	};

	de::UniquePtr<SparseContext>	m_sparseContext;

	// Wrapper functions around m_context calls to support sparse cases.
	VkPhysicalDevice				getPhysicalDevice (void) const
	{
		// Same in sparse and regular case
		return m_context.getPhysicalDevice();
	}

	VkDevice						getDevice (void) const
	{
		if (m_sparseContext)
			return *(m_sparseContext->m_device);

		return m_context.getDevice();
	}

	const InstanceInterface&		getInstanceInterface (void) const
	{
		// Same in sparse and regular case
		return m_context.getInstanceInterface();
	}

	const DeviceInterface&			getDeviceInterface (void) const
	{
		if (m_sparseContext)
			return m_sparseContext->m_deviceInterface;

		return m_context.getDeviceInterface();
	}

	deUint32						getUniversalQueueFamilyIndex (void) const
	{
		if (m_sparseContext)
			return m_sparseContext->m_queueFamilyIndex;

		return m_context.getUniversalQueueFamilyIndex();
	}

	static deUint32					findQueueFamilyIndexWithCaps (const InstanceInterface& vkInstance, VkPhysicalDevice physicalDevice, VkQueueFlags requiredCaps)
	{
		const std::vector<VkQueueFamilyProperties>	queueProps	= getPhysicalDeviceQueueFamilyProperties(vkInstance, physicalDevice);

		for (size_t queueNdx = 0; queueNdx < queueProps.size(); queueNdx++)
		{
			if ((queueProps[queueNdx].queueFlags & requiredCaps) == requiredCaps)
				return (deUint32)queueNdx;
		}

		TCU_THROW(NotSupportedError, "No matching queue found");
	}

	// Create the sparseContext
	SparseContext*					createSparseContext	(void) const
	{
		if ((m_testCase.flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT) ||
			(m_testCase.flags & VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT) ||
			(m_testCase.flags & VK_BUFFER_CREATE_SPARSE_ALIASED_BIT))
		{
			const InstanceInterface&		vk				= getInstanceInterface();
			const VkPhysicalDevice			physicalDevice	= getPhysicalDevice();
			const VkPhysicalDeviceFeatures	deviceFeatures	= getPhysicalDeviceFeatures(vk, physicalDevice);

			const deUint32 queueIndex = findQueueFamilyIndexWithCaps(vk, physicalDevice, VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_SPARSE_BINDING_BIT);

			VkDeviceQueueCreateInfo			queueInfo;
			VkDeviceCreateInfo				deviceInfo;
			const float						queuePriority	= 1.0f;

			deMemset(&queueInfo,	0, sizeof(queueInfo));
			deMemset(&deviceInfo,	0, sizeof(deviceInfo));

			queueInfo.sType							= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.pNext							= DE_NULL;
			queueInfo.flags							= (VkDeviceQueueCreateFlags)0u;
			queueInfo.queueFamilyIndex				= queueIndex;
			queueInfo.queueCount					= 1u;
			queueInfo.pQueuePriorities				= &queuePriority;

			deviceInfo.sType						= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			deviceInfo.pNext						= DE_NULL;
			deviceInfo.queueCreateInfoCount			= 1u;
			deviceInfo.pQueueCreateInfos			= &queueInfo;
			deviceInfo.enabledExtensionCount		= 0u;
			deviceInfo.ppEnabledExtensionNames		= DE_NULL;
			deviceInfo.enabledLayerCount			= 0u;
			deviceInfo.ppEnabledLayerNames			= DE_NULL;
			deviceInfo.pEnabledFeatures				= &deviceFeatures;

			Move<VkDevice>	device = createDevice(vk, physicalDevice, &deviceInfo);

			return new SparseContext(device, queueIndex, vk);
		}

		return DE_NULL;
	}
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

inline VkDeviceSize alignDeviceSize (VkDeviceSize val, VkDeviceSize align)
{
	DE_ASSERT(deIsPowerOfTwo64(align));
	DE_ASSERT(val + align >= val);				// crash on overflow
	return (val + align - 1) & ~(align - 1);
}

tcu::TestStatus BufferTestInstance::bufferCreateAndAllocTest (VkDeviceSize size)
{
	const VkPhysicalDevice					vkPhysicalDevice	= getPhysicalDevice();
	const InstanceInterface&				vkInstance			= getInstanceInterface();
	const VkDevice							vkDevice			= getDevice();
	const DeviceInterface&					vk					= getDeviceInterface();
	const deUint32							queueFamilyIndex	= getUniversalQueueFamilyIndex();
	const VkPhysicalDeviceMemoryProperties	memoryProperties	= getPhysicalDeviceMemoryProperties(vkInstance, vkPhysicalDevice);
	const VkPhysicalDeviceLimits			limits				= getPhysicalDeviceProperties(vkInstance, vkPhysicalDevice).limits;
	Move<VkBuffer>							buffer;
	Move<VkDeviceMemory>					memory;
	VkMemoryRequirements					memReqs;

	if ((m_testCase.flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT) != 0)
		size = std::min(size, limits.sparseAddressSpaceSize);

	// Create the test buffer and a memory allocation for it
	{
		// Create a minimal buffer first to get the supported memory types
		VkBufferCreateInfo bufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType        sType;
			DE_NULL,								// const void*            pNext;
			m_testCase.flags,						// VkBufferCreateFlags    flags;
			1u,										// VkDeviceSize           size;
			m_testCase.usage,						// VkBufferUsageFlags     usage;
			m_testCase.sharingMode,					// VkSharingMode          sharingMode;
			1u,										// uint32_t               queueFamilyIndexCount;
			&queueFamilyIndex,						// const uint32_t*        pQueueFamilyIndices;
		};

		buffer = createBuffer(vk, vkDevice, &bufferParams);
		vk.getBufferMemoryRequirements(vkDevice, *buffer, &memReqs);

		const deUint32		heapTypeIndex	= (deUint32)deCtz32(memReqs.memoryTypeBits);
		const VkMemoryType	memoryType		= memoryProperties.memoryTypes[heapTypeIndex];
		const VkMemoryHeap	memoryHeap		= memoryProperties.memoryHeaps[memoryType.heapIndex];
		const deUint32		shrinkBits		= 4;	// number of bits to shift when reducing the size with each iteration

		// Buffer size - Choose half of the reported heap size for the maximum buffer size, we
		// should attempt to test as large a portion as possible.
		//
		// However on a system where device memory is shared with the system, the maximum size
		// should be tested against the platform memory limits as significant portion of the heap
		// may already be in use by the operating system and other running processes.
		const VkDeviceSize  availableBufferSize	= getMaxBufferSize(memoryHeap.size,
																   memReqs.alignment,
																   getPlatformMemoryLimits(m_context));

		// For our test buffer size, halve the maximum available size and align
		const VkDeviceSize maxBufferSize = alignDeviceSize(availableBufferSize >> 1, memReqs.alignment);

		size = std::min(size, maxBufferSize);

		while (*memory == DE_NULL)
		{
			// Create the buffer
			{
				VkResult result		= VK_ERROR_OUT_OF_HOST_MEMORY;
				VkBuffer rawBuffer	= DE_NULL;

				bufferParams.size	= size;
				buffer				= Move<VkBuffer>();		// free the previous buffer, if any
				result				= vk.createBuffer(vkDevice, &bufferParams, (VkAllocationCallbacks*)DE_NULL, &rawBuffer);

				if (result != VK_SUCCESS)
				{
					size = alignDeviceSize(size >> shrinkBits, memReqs.alignment);

					if (size == 0 || bufferParams.size == memReqs.alignment)
						return tcu::TestStatus::fail("Buffer creation failed! (" + de::toString(getResultName(result)) + ")");

					continue;	// didn't work, try with a smaller buffer
				}

				buffer = Move<VkBuffer>(check<VkBuffer>(rawBuffer), Deleter<VkBuffer>(vk, vkDevice, DE_NULL));
			}

			vk.getBufferMemoryRequirements(vkDevice, *buffer, &memReqs);	// get the proper size requirement

			if (size > memReqs.size)
			{
				std::ostringstream errorMsg;
				errorMsg << "Requied memory size (" << memReqs.size << " bytes) smaller than the buffer's size (" << size << " bytes)!";
				return tcu::TestStatus::fail(errorMsg.str());
			}

			// Allocate the memory
			{
				VkResult		result			= VK_ERROR_OUT_OF_HOST_MEMORY;
				VkDeviceMemory	rawMemory		= DE_NULL;

				const VkMemoryAllocateInfo memAlloc =
				{
					VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,		// VkStructureType    sType;
					NULL,										// const void*        pNext;
					memReqs.size,								// VkDeviceSize       allocationSize;
					heapTypeIndex,								// uint32_t           memoryTypeIndex;
				};

				result = vk.allocateMemory(vkDevice, &memAlloc, (VkAllocationCallbacks*)DE_NULL, &rawMemory);

				if (result != VK_SUCCESS)
				{
					size = alignDeviceSize(size >> shrinkBits, memReqs.alignment);

					if (size == 0 || memReqs.size == memReqs.alignment)
						return tcu::TestStatus::fail("Unable to allocate " + de::toString(memReqs.size) + " bytes of memory");

					continue;	// didn't work, try with a smaller allocation (and a smaller buffer)
				}

				memory = Move<VkDeviceMemory>(check<VkDeviceMemory>(rawMemory), Deleter<VkDeviceMemory>(vk, vkDevice, DE_NULL));
			}
		} // while
	}

	// Bind the memory
	if ((m_testCase.flags & (VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT | VK_BUFFER_CREATE_SPARSE_ALIASED_BIT)) != 0)
	{
		VkQueue								queue					= DE_NULL;

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
			*buffer,							// VkBuffer									buffer;
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

		if (vk.queueBindSparse(queue, 1, &bindSparseInfo, *fence) != VK_SUCCESS)
			return tcu::TestStatus::fail("Bind sparse buffer memory failed! (requested memory size: " + de::toString(size) + ")");

		VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), VK_TRUE, ~(0ull) /* infinity */));
	}
	else
	{
		if (vk.bindBufferMemory(vkDevice, *buffer, *memory, 0) != VK_SUCCESS)
			return tcu::TestStatus::fail("Bind buffer memory failed! (requested memory size: " + de::toString(size) + ")");
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus BufferTestInstance::iterate (void)
{
	const VkPhysicalDeviceFeatures&	physicalDeviceFeatures	= getPhysicalDeviceFeatures(getInstanceInterface(), getPhysicalDevice());

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
		16384,
		~0ull,		// try to exercise a very large buffer too (will be clamped to a sensible size later)
	};

	for (int i = 0; i < DE_LENGTH_OF_ARRAY(testSizes); ++i)
	{
		const tcu::TestStatus testStatus = bufferCreateAndAllocTest(testSizes[i]);

		if (testStatus.getCode() != QP_TEST_RESULT_PASS)
			return testStatus;
	}

	return tcu::TestStatus::pass("Pass");
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

	// \note SPARSE_RESIDENCY and SPARSE_ALIASED have to be used together with the SPARSE_BINDING flag.
	const VkBufferCreateFlags bufferCreateFlags[] =
	{
		0,
		VK_BUFFER_CREATE_SPARSE_BINDING_BIT,
		VK_BUFFER_CREATE_SPARSE_BINDING_BIT	|	VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT,
		VK_BUFFER_CREATE_SPARSE_BINDING_BIT	|												VK_BUFFER_CREATE_SPARSE_ALIASED_BIT,
		VK_BUFFER_CREATE_SPARSE_BINDING_BIT	|	VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT	|	VK_BUFFER_CREATE_SPARSE_ALIASED_BIT,
	};

	de::MovePtr<tcu::TestCaseGroup>	buffersTests	(new tcu::TestCaseGroup(testCtx, "buffer", "Buffer Tests"));

	const deUint32 maximumValueOfBufferUsageFlags	= (1u << (DE_LENGTH_OF_ARRAY(bufferUsageModes) - 1)) - 1u;

	for (deUint32 bufferCreateFlagsNdx = 0u; bufferCreateFlagsNdx < DE_LENGTH_OF_ARRAY(bufferCreateFlags); bufferCreateFlagsNdx++)
	for (deUint32 combinedBufferUsageFlags = 1u; combinedBufferUsageFlags <= maximumValueOfBufferUsageFlags; combinedBufferUsageFlags++)
	{
		const BufferCaseParameters	testParams =
		{
			combinedBufferUsageFlags,
			bufferCreateFlags[bufferCreateFlagsNdx],
			VK_SHARING_MODE_EXCLUSIVE
		};
		std::ostringstream	testName;
		std::ostringstream	testDescription;
		testName << "create_buffer_" << combinedBufferUsageFlags << "_" << testParams.flags;
		testDescription << "vkCreateBuffer test " << combinedBufferUsageFlags << " " << testParams.flags;
		buffersTests->addChild(new BuffersTestCase(testCtx, testName.str(), testDescription.str(), testParams));
	}

	return buffersTests.release();
}

} // api
} // vk
