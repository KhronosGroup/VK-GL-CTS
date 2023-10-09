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
#include "vktApiBufferAndImageAllocationUtil.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkSafetyCriticalUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuCommandLine.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorType.hpp"
#include "deSharedPtr.hpp"
#include <limits>

namespace vkt
{

namespace api
{

using namespace vk;

namespace
{

struct TestParams
{
	enum
	{
		TEST_DATA_SIZE													= 256
	};

	VkDeviceSize					dstSize;
	VkDeviceSize					dstOffset;
	VkDeviceSize					size;
	deUint32						testData[TEST_DATA_SIZE];
	de::SharedPtr<IBufferAllocator>	bufferAllocator;
	bool							useTransferOnlyQueue;
};

// Creates a device that has transfer only operations
Move<VkDevice> createCustomDevice(Context& context, uint32_t& queueFamilyIndex)
{
	const InstanceInterface&	instanceDriver = context.getInstanceInterface();
	const VkPhysicalDevice		physicalDevice = context.getPhysicalDevice();

	queueFamilyIndex = findQueueFamilyIndexWithCaps(instanceDriver, physicalDevice, VK_QUEUE_TRANSFER_BIT, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

	const std::vector<VkQueueFamilyProperties>	queueFamilies = getPhysicalDeviceQueueFamilyProperties(instanceDriver, physicalDevice);

	// This must be found, findQueueFamilyIndexWithCaps would have
	// thrown a NotSupported exception if the requested queue type did
	// not exist. Similarly, this was written with the assumption the
	// "alternative" queue would be different to the universal queue.
	DE_ASSERT(queueFamilyIndex < queueFamilies.size() && queueFamilyIndex != context.getUniversalQueueFamilyIndex());
	const float queuePriority = 1.0f;
	const VkDeviceQueueCreateInfo deviceQueueCreateInfos
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,		// VkStructureType				sType;
		nullptr,										// const void*					pNext;
		(VkDeviceQueueCreateFlags)0u,					// VkDeviceQueueCreateFlags		flags;
		queueFamilyIndex,								// uint32_t						queueFamilyIndex;
		1u,												// uint32_t						queueCount;
		&queuePriority,									// const float*					pQueuePriorities;
	};

	// Replicate default device extension list.
	const auto	extensionNames				= context.getDeviceCreationExtensions();
	auto		synchronization2Features	= context.getSynchronization2Features();
	auto		deviceFeatures2				= context.getDeviceFeatures2();
	const void*	pNext						= &deviceFeatures2;

	if (context.isDeviceFunctionalitySupported("VK_KHR_synchronization2"))
	{
		if (context.getUsedApiVersion() < VK_API_VERSION_1_3)
		{
			synchronization2Features.pNext = &deviceFeatures2;
			pNext = &synchronization2Features;
		}
	}

#ifdef CTS_USES_VULKANSC
	VkDeviceObjectReservationCreateInfo memReservationInfo = context.getTestContext().getCommandLine().isSubProcess() ? context.getResourceInterface()->getStatMax() : resetDeviceObjectReservationCreateInfo();
	memReservationInfo.pNext = pNext;
	pNext = &memReservationInfo;

	VkPipelineCacheCreateInfo			pcCI;
	std::vector<VkPipelinePoolSize>		poolSizes;
	if (context.getTestContext().getCommandLine().isSubProcess())
	{
		if (context.getResourceInterface()->getCacheDataSize() > 0)
		{
			pcCI =
			{
				VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,			// VkStructureType				sType;
				DE_NULL,												// const void*					pNext;
				VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
					VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT,	// VkPipelineCacheCreateFlags	flags;
				context.getResourceInterface()->getCacheDataSize(),	// deUintptr					initialDataSize;
				context.getResourceInterface()->getCacheData()		// const void*					pInitialData;
			};
			memReservationInfo.pipelineCacheCreateInfoCount = 1;
			memReservationInfo.pPipelineCacheCreateInfos = &pcCI;
		}
		poolSizes = context.getResourceInterface()->getPipelinePoolSizes();
		if (!poolSizes.empty())
		{
			memReservationInfo.pipelinePoolSizeCount = deUint32(poolSizes.size());
			memReservationInfo.pPipelinePoolSizes = poolSizes.data();
		}
	}
#endif // CTS_USES_VULKANSC

	const VkDeviceCreateInfo deviceCreateInfo
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,			// VkStructureType					sType;
		pNext,											// const void*						pNext;
		(VkDeviceCreateFlags)0u,						// VkDeviceCreateFlags				flags;
		1u,												// uint32_t							queueCreateInfoCount;
		&deviceQueueCreateInfos,						// const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
		0u,												// uint32_t							enabledLayerCount;
		DE_NULL,										// const char* const*				ppEnabledLayerNames;
		static_cast<uint32_t>(extensionNames.size()),	// uint32_t							enabledExtensionCount;
		extensionNames.data(),							// const char* const*				ppEnabledExtensionNames;
		DE_NULL,										// const VkPhysicalDeviceFeatures*	pEnabledFeatures;
	};

	return vkt::createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), context.getPlatformInterface(), context.getInstance(), instanceDriver, physicalDevice, &deviceCreateInfo);
}

class FillWholeBufferTestInstance : public vkt::TestInstance
{
public:
							FillWholeBufferTestInstance	(Context& context, const TestParams& testParams);
	virtual tcu::TestStatus iterate						(void) override;
protected:
	// dstSize will be used as the buffer size.
	// dstOffset will be used as the offset for vkCmdFillBuffer.
	// size in vkCmdFillBuffer will always be VK_WHOLE_SIZE.
	const TestParams		m_params;

	Move<VkDevice>			m_customDevice;
	de::MovePtr<Allocator>	m_customAllocator;

	VkDevice				m_device;
	Allocator*				m_allocator;
	uint32_t				m_queueFamilyIndex;

	Move<VkCommandPool>		m_cmdPool;
	Move<VkCommandBuffer>	m_cmdBuffer;

	Move<VkBuffer>			m_destination;
	de::MovePtr<Allocation>	m_destinationBufferAlloc;
};

FillWholeBufferTestInstance::FillWholeBufferTestInstance(Context& context, const TestParams& testParams)
	: vkt::TestInstance(context), m_params(testParams)
{
	const InstanceInterface&	vki			= m_context.getInstanceInterface();
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkPhysicalDevice		physDevice	= m_context.getPhysicalDevice();

	if (testParams.useTransferOnlyQueue)
	{
		m_customDevice		= createCustomDevice(context, m_queueFamilyIndex);
		m_customAllocator	= de::MovePtr<Allocator>(new SimpleAllocator(vk, *m_customDevice, getPhysicalDeviceMemoryProperties(vki, physDevice)));

		m_device			= *m_customDevice;
		m_allocator			= &(*m_customAllocator);
	}
	else
	{
		m_device			= context.getDevice();
		m_allocator			= &context.getDefaultAllocator();
		m_queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	}

	m_cmdPool = createCommandPool(vk, m_device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, m_queueFamilyIndex);
	m_cmdBuffer = allocateCommandBuffer(vk, m_device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	testParams.bufferAllocator->createTestBuffer(vk, m_device, m_queueFamilyIndex, m_params.dstSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, context, *m_allocator, m_destination, MemoryRequirement::HostVisible, m_destinationBufferAlloc);
}

tcu::TestStatus FillWholeBufferTestInstance::iterate(void)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkQueue			queue	= getDeviceQueue(vk, m_device, m_queueFamilyIndex, 0);

	// if posible use synchronization2 when testing transfer only queue
	const bool useSynchronization2 = m_context.isDeviceFunctionalitySupported("VK_KHR_synchronization2") && m_params.useTransferOnlyQueue;

	// Make sure some stuff below will work.
	DE_ASSERT(m_params.dstSize >= sizeof(deUint32));
	DE_ASSERT(m_params.dstSize <  static_cast<VkDeviceSize>(std::numeric_limits<size_t>::max()));
	DE_ASSERT(m_params.dstOffset < m_params.dstSize);

	// Fill buffer from the host and flush buffer memory.
	deUint8* bytes = reinterpret_cast<deUint8*>(m_destinationBufferAlloc->getHostPtr());
	deMemset(bytes, 0xff, static_cast<size_t>(m_params.dstSize));
	flushAlloc(vk, m_device, *m_destinationBufferAlloc);

	const VkBufferMemoryBarrier	gpuToHostBarrier
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags			dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
		*m_destination,								// VkBuffer					buffer;
		0u,											// VkDeviceSize				offset;
		VK_WHOLE_SIZE								// VkDeviceSize				size;
	};

#ifndef CTS_USES_VULKANSC
	using BufferMemoryBarrier2		= VkBufferMemoryBarrier2;
	using DependencyInfo			= VkDependencyInfo;
	using CommandBufferSubmitInfo	= VkCommandBufferSubmitInfo;
	using SubmitInfo2				= VkSubmitInfo2;
	auto cmdPipelineBarrier2Fun		= &DeviceInterface::cmdPipelineBarrier2;
	auto queueSubmit2Fun			= &DeviceInterface::queueSubmit2;
#else
	using BufferMemoryBarrier2		= VkBufferMemoryBarrier2KHR;
	using DependencyInfo			= VkDependencyInfoKHR;
	using CommandBufferSubmitInfo	= VkCommandBufferSubmitInfoKHR;
	using SubmitInfo2				= VkSubmitInfo2KHR;
	auto cmdPipelineBarrier2Fun		= &DeviceInterface::cmdPipelineBarrier2KHR;
	auto queueSubmit2Fun			= &DeviceInterface::queueSubmit2KHR;
#endif // CTS_USES_VULKANSC

	BufferMemoryBarrier2 gpuToHostBarrier2	= initVulkanStructure();
	gpuToHostBarrier2.srcStageMask			= VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;
	gpuToHostBarrier2.srcAccessMask			= VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
	gpuToHostBarrier2.dstStageMask			= VK_PIPELINE_STAGE_2_HOST_BIT_KHR;
	gpuToHostBarrier2.dstAccessMask			= VK_ACCESS_2_HOST_READ_BIT_KHR;
	gpuToHostBarrier2.srcQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	gpuToHostBarrier2.dstQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	gpuToHostBarrier2.buffer				= *m_destination;
	gpuToHostBarrier2.size					= VK_WHOLE_SIZE;

	DependencyInfo depInfo = initVulkanStructure();
	depInfo.bufferMemoryBarrierCount	= 1;
	depInfo.pBufferMemoryBarriers		= &gpuToHostBarrier2;

	// Fill buffer using VK_WHOLE_SIZE.
	beginCommandBuffer(vk, *m_cmdBuffer);
	vk.cmdFillBuffer(*m_cmdBuffer, *m_destination, m_params.dstOffset, VK_WHOLE_SIZE, deUint32{0x01010101});

	if (useSynchronization2)
		(vk.*(cmdPipelineBarrier2Fun))(*m_cmdBuffer, &depInfo);
	else
		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, DE_NULL, 1, &gpuToHostBarrier, 0, DE_NULL);

	endCommandBuffer(vk, *m_cmdBuffer);

	Move<VkFence> fence(createFence(vk, m_device));
	if (useSynchronization2)
	{
		CommandBufferSubmitInfo commandBufferInfos	= initVulkanStructure();
		commandBufferInfos.commandBuffer			= *m_cmdBuffer;

		SubmitInfo2 submitInfo2						= initVulkanStructure();
		submitInfo2.commandBufferInfoCount			= 1u;
		submitInfo2.pCommandBufferInfos				= &commandBufferInfos;

		(vk.*(queueSubmit2Fun))(queue, 1u, &submitInfo2, *fence);
	}
	else
	{
		VkSubmitInfo submitInfo			= initVulkanStructure();
		submitInfo.commandBufferCount	= 1u;
		submitInfo.pCommandBuffers		= &m_cmdBuffer.get();

		VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, *fence));
	}
	waitForFence(vk, m_device, *fence);

	// Invalidate buffer memory and check the buffer contains the expected results.
	invalidateAlloc(vk, m_device, *m_destinationBufferAlloc);

	const VkDeviceSize startOfExtra = (m_params.dstSize / sizeof(deUint32)) * sizeof(deUint32);
	for (VkDeviceSize i = 0; i < m_params.dstSize; ++i)
	{
		const deUint8 expectedByte = ((i >= m_params.dstOffset && i < startOfExtra)? 0x01 : 0xff);
		if (bytes[i] != expectedByte)
		{
			std::ostringstream msg;
			msg << "Invalid byte at position " << i << " in the buffer (found 0x"
				<< std::hex << static_cast<int>(bytes[i]) << " but expected 0x" << static_cast<int>(expectedByte) << ")";
			return tcu::TestStatus::fail(msg.str());
		}
	}

	return tcu::TestStatus::pass("Pass");
}

class FillWholeBufferTestCase : public vkt::TestCase
{
public:
							FillWholeBufferTestCase	(tcu::TestContext&	testCtx,
													 const std::string&	name,
													 const std::string&	description,
													 const TestParams	params)
		: vkt::TestCase(testCtx, name, description), m_params(params)
	{}

	virtual TestInstance*	createInstance			(Context&			context) const override
	{
		return static_cast<TestInstance*>(new FillWholeBufferTestInstance(context, m_params));
	}
private:
	const TestParams		m_params;
};


class FillBufferTestInstance : public vkt::TestInstance
{
public:
									FillBufferTestInstance				(Context&					context,
																		 TestParams					testParams);
	virtual tcu::TestStatus			iterate								(void);
protected:
	const TestParams				m_params;

	Move<VkDevice>					m_customDevice;
	de::MovePtr<Allocator>			m_customAllocator;

	VkDevice						m_device;
	Allocator*						m_allocator;
	uint32_t						m_queueFamilyIndex;

	Move<VkCommandPool>				m_cmdPool;
	Move<VkCommandBuffer>			m_cmdBuffer;
	de::MovePtr<tcu::TextureLevel>	m_destinationTextureLevel;
	de::MovePtr<tcu::TextureLevel>	m_expectedTextureLevel;

	VkCommandBufferBeginInfo		m_cmdBufferBeginInfo;

	Move<VkBuffer>					m_destination;
	de::MovePtr<Allocation>			m_destinationBufferAlloc;

	void							generateBuffer						(tcu::PixelBufferAccess		buffer,
																		 int						width,
																		 int						height,
																		 int						depth = 1);
	virtual void					generateExpectedResult				(void);
	void							uploadBuffer						(tcu::ConstPixelBufferAccess
																									bufferAccess,
																		 const Allocation&			bufferAlloc);
	virtual tcu::TestStatus			checkTestResult						(tcu::ConstPixelBufferAccess
																									result);
	deUint32						calculateSize						(tcu::ConstPixelBufferAccess
																									src) const
	{
		return src.getWidth() * src.getHeight() * src.getDepth() * tcu::getPixelSize(src.getFormat());
	}
};

									FillBufferTestInstance::FillBufferTestInstance
																		(Context&					context,
																		 TestParams					testParams)
									: vkt::TestInstance					(context)
									, m_params							(testParams)
{
	const InstanceInterface&	vki			= m_context.getInstanceInterface();
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkPhysicalDevice		physDevice	= m_context.getPhysicalDevice();

	if (testParams.useTransferOnlyQueue)
	{
		m_customDevice		= createCustomDevice(context, m_queueFamilyIndex);
		m_customAllocator	= de::MovePtr<Allocator>(new SimpleAllocator(vk, *m_customDevice, getPhysicalDeviceMemoryProperties(vki, physDevice)));

		m_device			= *m_customDevice;
		m_allocator			= &(*m_customAllocator);
	}
	else
	{
		m_device			= context.getDevice();
		m_allocator			= &context.getDefaultAllocator();
		m_queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	}

	// Create command pool
	m_cmdPool = createCommandPool(vk, m_device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, m_queueFamilyIndex);

	// Create command buffer
	m_cmdBuffer = allocateCommandBuffer(vk, m_device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	testParams.bufferAllocator->createTestBuffer(vk, m_device, m_queueFamilyIndex, m_params.dstSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, context, *m_allocator, m_destination, MemoryRequirement::HostVisible, m_destinationBufferAlloc);
}

tcu::TestStatus						FillBufferTestInstance::iterate		(void)
{
	const int						dstLevelWidth						= (int)(m_params.dstSize / 4);
	m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(mapVkFormat(VK_FORMAT_R8G8B8A8_UINT), dstLevelWidth, 1));

	generateBuffer(m_destinationTextureLevel->getAccess(), dstLevelWidth, 1, 1);

	generateExpectedResult();

	uploadBuffer(m_destinationTextureLevel->getAccess(), *m_destinationBufferAlloc);

	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkQueue			queue	= getDeviceQueue(vk, m_device, m_queueFamilyIndex, 0);

	const VkBufferMemoryBarrier		dstBufferBarrier					=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,						// VkStructureType			sType;
		DE_NULL,														// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,									// VkAccessFlags			srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,										// VkAccessFlags			dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,										// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,										// deUint32					dstQueueFamilyIndex;
		*m_destination,													// VkBuffer					buffer;
		m_params.dstOffset,												// VkDeviceSize				offset;
		VK_WHOLE_SIZE													// VkDeviceSize				size;
	};

	beginCommandBuffer(vk, *m_cmdBuffer);
	vk.cmdFillBuffer(*m_cmdBuffer, *m_destination, m_params.dstOffset, m_params.size, m_params.testData[0]);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &dstBufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	endCommandBuffer(vk, *m_cmdBuffer);

	submitCommandsAndWait(vk, m_device, queue, m_cmdBuffer.get());

	// Read buffer data
	de::MovePtr<tcu::TextureLevel>	resultLevel	(new tcu::TextureLevel(m_destinationTextureLevel->getAccess().getFormat(), dstLevelWidth, 1));
	invalidateAlloc(vk, m_device, *m_destinationBufferAlloc);
	tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(resultLevel->getFormat(), resultLevel->getSize(), m_destinationBufferAlloc->getHostPtr()));

	return checkTestResult(resultLevel->getAccess());
}

void								FillBufferTestInstance::generateBuffer
																		(tcu::PixelBufferAccess		buffer,
																		 int						width,
																		 int						height,
																		 int						depth)
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

void								FillBufferTestInstance::uploadBuffer
																		(tcu::ConstPixelBufferAccess
																									bufferAccess,
																		 const Allocation&			bufferAlloc)
{
	const DeviceInterface&			vk									= m_context.getDeviceInterface();
	const deUint32					bufferSize							= calculateSize(bufferAccess);

	// Write buffer data
	deMemcpy(bufferAlloc.getHostPtr(), bufferAccess.getDataPtr(), bufferSize);
	flushAlloc(vk, m_device, bufferAlloc);
}

tcu::TestStatus						FillBufferTestInstance::checkTestResult
																		(tcu::ConstPixelBufferAccess
																									result)
{
	const tcu::ConstPixelBufferAccess
									expected							= m_expectedTextureLevel->getAccess();
	const tcu::UVec4				threshold							(0, 0, 0, 0);

	if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparsion", expected, result, threshold, tcu::COMPARE_LOG_RESULT))
	{
		return tcu::TestStatus::fail("Fill and Update Buffer test");
	}

	return tcu::TestStatus::pass("Fill and Update Buffer test");
}

void								FillBufferTestInstance::generateExpectedResult
																		(void)
{
	const tcu::ConstPixelBufferAccess
									dst									= m_destinationTextureLevel->getAccess();

	m_expectedTextureLevel	= de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(dst.getFormat(), dst.getWidth(), dst.getHeight(), dst.getDepth()));
	tcu::copy(m_expectedTextureLevel->getAccess(), dst);

	deUint32*						currentPtr							= (deUint32*) m_expectedTextureLevel->getAccess().getDataPtr() + m_params.dstOffset / 4;
	deUint32*						endPtr								= currentPtr + m_params.size / 4;

	while (currentPtr < endPtr)
	{
		*currentPtr = m_params.testData[0];
		currentPtr++;
	}
}

class FillBufferTestCase : public vkt::TestCase
{
public:
									FillBufferTestCase					(tcu::TestContext&			testCtx,
																		 const std::string&			name,
																		 const std::string&			description,
																		 const TestParams			params)
									: vkt::TestCase						(testCtx, name, description)
									, m_params							(params)
	{}

	virtual TestInstance*			createInstance						(Context&					context) const
	{
		return static_cast<TestInstance*>(new FillBufferTestInstance(context, m_params));
	}
private:
	const TestParams				m_params;
};

// Update Buffer

class UpdateBufferTestInstance : public FillBufferTestInstance
{
public:
									UpdateBufferTestInstance			(Context&					context,
																		 TestParams					testParams)
									: FillBufferTestInstance			(context, testParams)
	{}
	virtual tcu::TestStatus			iterate								(void);

protected:
	virtual void					generateExpectedResult				(void);
};

tcu::TestStatus						UpdateBufferTestInstance::iterate	(void)
{
	const int						dstLevelWidth						= (int)(m_params.dstSize / 4);
	m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(mapVkFormat(VK_FORMAT_R8G8B8A8_UINT), dstLevelWidth, 1));

	generateBuffer(m_destinationTextureLevel->getAccess(), dstLevelWidth, 1, 1);

	generateExpectedResult();

	uploadBuffer(m_destinationTextureLevel->getAccess(), *m_destinationBufferAlloc);

	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkQueue			queue	= getDeviceQueue(vk, m_device, m_queueFamilyIndex, 0);

	const VkBufferMemoryBarrier		dstBufferBarrier					=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,						// VkStructureType			sType;
		DE_NULL,														// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,									// VkAccessFlags			srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,										// VkAccessFlags			dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,										// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,										// deUint32					dstQueueFamilyIndex;
		*m_destination,													// VkBuffer					buffer;
		m_params.dstOffset,												// VkDeviceSize				offset;
		VK_WHOLE_SIZE													// VkDeviceSize				size;
	};

	beginCommandBuffer(vk, *m_cmdBuffer);
	vk.cmdUpdateBuffer(*m_cmdBuffer, *m_destination, m_params.dstOffset, m_params.size, m_params.testData);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &dstBufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	endCommandBuffer(vk, *m_cmdBuffer);

	submitCommandsAndWait(vk, m_device, queue, m_cmdBuffer.get());

	// Read buffer data
	de::MovePtr<tcu::TextureLevel>	resultLevel	(new tcu::TextureLevel(m_destinationTextureLevel->getAccess().getFormat(), dstLevelWidth, 1));
	invalidateAlloc(vk, m_device, *m_destinationBufferAlloc);
	tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(resultLevel->getFormat(), resultLevel->getSize(), m_destinationBufferAlloc->getHostPtr()));

	return checkTestResult(resultLevel->getAccess());
}

void								UpdateBufferTestInstance::generateExpectedResult
																		(void)
{
	const tcu::ConstPixelBufferAccess
									dst									= m_destinationTextureLevel->getAccess();

	m_expectedTextureLevel	= de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(dst.getFormat(), dst.getWidth(), dst.getHeight(), dst.getDepth()));
	tcu::copy(m_expectedTextureLevel->getAccess(), dst);

	deUint32*						currentPtr							= (deUint32*) m_expectedTextureLevel->getAccess().getDataPtr() + m_params.dstOffset / 4;

	deMemcpy(currentPtr, m_params.testData, (size_t)m_params.size);
}

class UpdateBufferTestCase : public vkt::TestCase
{
public:
									UpdateBufferTestCase				(tcu::TestContext&			testCtx,
																		 const std::string&			name,
																		 const std::string&			description,
																		 const TestParams			params)
									: vkt::TestCase						(testCtx, name, description)
									, m_params							(params)
	{}

	virtual TestInstance*			createInstance						(Context&					context) const
	{
		return (TestInstance*) new UpdateBufferTestInstance(context, m_params);
	}
private:
	TestParams						m_params;
};

} // anonymous

tcu::TestCaseGroup*					createFillAndUpdateBufferTests	(tcu::TestContext&			testCtx)
{
	const de::SharedPtr<IBufferAllocator> bufferAllocators[]
	{
		de::SharedPtr<BufferSuballocation>(new BufferSuballocation()),
		de::SharedPtr<BufferDedicatedAllocation>(new BufferDedicatedAllocation())
	};

	de::MovePtr<tcu::TestCaseGroup> fillAndUpdateBufferTests(new tcu::TestCaseGroup(testCtx, "fill_and_update_buffer", "Fill and Update Buffer Tests"));

	struct TestGroupData
	{
		const char*		name;
		const char*		description;
		bool			useDedicatedAllocation;
		bool			useTransferOnlyQueue;
	};
	const TestGroupData testGroupData[]
	{
		{ "suballocation",					"BufferView Fill and Update Tests for Suballocated Objects",						false,	false },
		{ "suballocation_transfer_queue",	"BufferView Fill and Update Tests for Suballocated Objects on transfer only queue",	false,	true },
		{ "dedicated_alloc",				"BufferView Fill and Update Tests for Dedicatedly Allocated Objects",				true,	false },
	};

	TestParams params;
	for (const auto& groupData : testGroupData)
	{
		de::MovePtr<tcu::TestCaseGroup> currentTestsGroup(new tcu::TestCaseGroup(testCtx, groupData.name, groupData.description));

		params.dstSize				= TestParams::TEST_DATA_SIZE;
		params.bufferAllocator		= bufferAllocators[groupData.useDedicatedAllocation];
		params.useTransferOnlyQueue	= groupData.useTransferOnlyQueue;

		deUint8* data = (deUint8*) params.testData;
		for (deUint32 b = 0u; b < (params.dstSize * sizeof(params.testData[0])); b++)
			data[b] = (deUint8) (b % 255);

		{
			const std::string		description							("whole buffer");
			const std::string		testName							("buffer_whole");

			params.dstOffset = 0;
			params.size = params.dstSize;

			currentTestsGroup->addChild(new FillBufferTestCase(testCtx, "fill_" + testName, "Fill " + description, params));
			currentTestsGroup->addChild(new UpdateBufferTestCase(testCtx, "update_" + testName, "Update " + description, params));
		}

		{
			const std::string		description							("first word in buffer");
			const std::string		testName							("buffer_first_one");

			params.dstOffset = 0;
			params.size = 4;

			currentTestsGroup->addChild(new FillBufferTestCase(testCtx, "fill_" + testName, "Fill " + description, params));
			currentTestsGroup->addChild(new UpdateBufferTestCase(testCtx, "update_" + testName, "Update " + description, params));
		}

		{
			const std::string		description							("second word in buffer");
			const std::string		testName							("buffer_second_one");

			params.dstOffset = 4;
			params.size = 4;

			currentTestsGroup->addChild(new FillBufferTestCase(testCtx, "fill_" + testName, "Fill " + description, params));
			currentTestsGroup->addChild(new UpdateBufferTestCase(testCtx, "update_" + testName, "Update " + description, params));
		}

		{
			const std::string		description							("buffer second part");
			const std::string		testName							("buffer_second_part");

			params.dstOffset = params.dstSize / 2;
			params.size = params.dstSize / 2;

			currentTestsGroup->addChild(new FillBufferTestCase(testCtx, "fill_" + testName, "Fill " + description, params));
			currentTestsGroup->addChild(new UpdateBufferTestCase(testCtx, "update_" + testName, "Update " + description, params));
		}

		// VK_WHOLE_SIZE tests.
		{
			for (VkDeviceSize i = 0; i < sizeof(deUint32); ++i)
			{
				for (VkDeviceSize j = 0; j < sizeof(deUint32); ++j)
				{
					params.dstSize		= TestParams::TEST_DATA_SIZE + i;
					params.dstOffset	= j * sizeof(deUint32);
					params.size			= VK_WHOLE_SIZE;

					const VkDeviceSize	extraBytes	= params.dstSize % sizeof(deUint32);
					const std::string	name		= "fill_buffer_vk_whole_size_" + de::toString(extraBytes) + "_extra_bytes_offset_" + de::toString(params.dstOffset);
					const std::string	description	= "vkCmdFillBuffer with VK_WHOLE_SIZE, " + de::toString(extraBytes) + " extra bytes and offset " + de::toString(params.dstOffset);

					currentTestsGroup->addChild(new FillWholeBufferTestCase{testCtx, name, description, params});
				}
			}
		}

		fillAndUpdateBufferTests->addChild(currentTestsGroup.release());
	}

	return fillAndUpdateBufferTests.release();
}

} // api
} // vkt
