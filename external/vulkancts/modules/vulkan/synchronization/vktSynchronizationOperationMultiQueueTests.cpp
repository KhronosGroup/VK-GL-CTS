/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Synchronization primitive tests with multi queue
 *//*--------------------------------------------------------------------*/

#include "vktSynchronizationOperationMultiQueueTests.hpp"
#include "vkDefs.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "deUniquePtr.hpp"
#include "tcuTestLog.hpp"
#include "vktSynchronizationUtil.hpp"
#include "vktSynchronizationOperation.hpp"
#include "vktSynchronizationOperationTestData.hpp"
#include "vktTestGroupUtil.hpp"

namespace vkt
{
namespace synchronization
{
namespace
{
using namespace vk;

enum QueueType {WRITE, READ, COUNT};
enum {NO_MATCH_FOUND = ~((deUint32)0)};

class MultiQueues
{
public:
	MultiQueues	(MultiQueues &obj)
		: m_logicalDevice	(obj.m_logicalDevice)
		, m_allocator		(obj.m_allocator)
	{
		m_queues.resize(COUNT);
		m_queueFamilyIndex.resize(COUNT);
		for (int ndx = 0; ndx < COUNT; ++ndx)
		{
			m_queues[ndx]			= obj.m_queues[ndx];
			m_queueFamilyIndex[ndx]	= obj.m_queueFamilyIndex[ndx];
		}
	}

	MultiQueues	()
	{
		m_queues.resize(COUNT);
		m_queueFamilyIndex.resize(COUNT);
		for (int ndx = 0; ndx < COUNT; ++ndx)
			m_queueFamilyIndex[ndx] = NO_MATCH_FOUND;
	}

	Move<VkDevice>			m_logicalDevice;
	de::MovePtr<Allocator>	m_allocator;
	std::vector<VkQueue>	m_queues;
	std::vector<deUint32>	m_queueFamilyIndex;
};

de::MovePtr<Allocator> createAllocator (Context& context, VkDevice device)
{
	const DeviceInterface&					deviceInterface			= context.getDeviceInterface();
	const InstanceInterface&				instance				= context.getInstanceInterface();
	const VkPhysicalDevice					physicalDevice			= context.getPhysicalDevice();
	const VkPhysicalDeviceMemoryProperties	deviceMemoryProperties	= getPhysicalDeviceMemoryProperties(instance, physicalDevice);

	// Create memory allocator for device
	return de::MovePtr<Allocator> (new SimpleAllocator(deviceInterface, device, deviceMemoryProperties));
}

bool checkQueueFlags (const vk::VkQueueFlags& availableFlag, const vk::VkQueueFlags& neededFlag)
{
	if (VK_QUEUE_TRANSFER_BIT == neededFlag)
	{
		if ( (availableFlag & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT ||
			 (availableFlag & VK_QUEUE_COMPUTE_BIT)	 == VK_QUEUE_COMPUTE_BIT  ||
			 (availableFlag & VK_QUEUE_TRANSFER_BIT) == VK_QUEUE_TRANSFER_BIT
		   )
			return true;
	}
	else if ((availableFlag & neededFlag) == neededFlag)
	{
		return true;
	}
	return false;
}

de::MovePtr<MultiQueues> createQueues (Context& context, const vk::VkQueueFlags& queueFlagWrite, const vk::VkQueueFlags& queueFlagRead)
{
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const InstanceInterface&				instance				= context.getInstanceInterface();
	const VkPhysicalDevice					physicalDevice			= context.getPhysicalDevice();
	const float								queuePriorities[COUNT]	= {1.0f, 1.0f};
	VkDeviceCreateInfo						deviceInfo;
	VkPhysicalDeviceFeatures				deviceFeatures;
	std::vector<VkQueueFamilyProperties>	queueFamilyProperties;
	VkDeviceQueueCreateInfo					queueInfos[COUNT];
	VkQueueFlags							queueFlags[COUNT]		= {queueFlagWrite,queueFlagRead};
	int										fisrtQueueToFind		= WRITE;
	int										secondQueueToFind		= READ;
	MultiQueues								queues;

	queueFamilyProperties = getPhysicalDeviceQueueFamilyProperties(instance, physicalDevice);

	{
		int	counterWrite = 0;
		int	counterRead	 = 0;

		for (deUint32 queuePropertiesNdx = 0; queuePropertiesNdx < queueFamilyProperties.size(); ++queuePropertiesNdx)
		{
			if (checkQueueFlags(queueFamilyProperties[queuePropertiesNdx].queueFlags, queueFlagWrite))
				counterWrite++;

			if (checkQueueFlags(queueFamilyProperties[queuePropertiesNdx].queueFlags, queueFlagRead))
				counterRead++;
		}

		if (counterRead < counterWrite)
		{
			fisrtQueueToFind	= READ;
			secondQueueToFind	= WRITE;
		}
	}

	for (deUint32 queuePropertiesNdx = 0; queuePropertiesNdx < queueFamilyProperties.size(); ++queuePropertiesNdx)
	{
		if (NO_MATCH_FOUND == queues.m_queueFamilyIndex[fisrtQueueToFind])
		if (checkQueueFlags(queueFamilyProperties[queuePropertiesNdx].queueFlags, queueFlags[fisrtQueueToFind]))
		{
			queues.m_queueFamilyIndex[fisrtQueueToFind] = queuePropertiesNdx;
		}

		if (NO_MATCH_FOUND == queues.m_queueFamilyIndex[secondQueueToFind])
		if (checkQueueFlags(queueFamilyProperties[queuePropertiesNdx].queueFlags, queueFlags[secondQueueToFind]))
		{
			if (queuePropertiesNdx != queues.m_queueFamilyIndex[fisrtQueueToFind] || queueFamilyProperties[queuePropertiesNdx].queueCount > 1u)
			{
				queues.m_queueFamilyIndex[secondQueueToFind] = queuePropertiesNdx;
			}
		}
	}

	if (NO_MATCH_FOUND == queues.m_queueFamilyIndex[WRITE] || NO_MATCH_FOUND == queues.m_queueFamilyIndex[READ])
	{
		TCU_THROW(NotSupportedError, "Queue not found");
	}

	for (int queueNdx = 0; queueNdx < COUNT; ++queueNdx)
	{
		VkDeviceQueueCreateInfo queueInfo;
		deMemset(&queueInfo, 0, sizeof(queueInfo));

		queueInfo.sType				= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.pNext				= DE_NULL;
		queueInfo.flags				= (VkDeviceQueueCreateFlags)0u;
		queueInfo.queueFamilyIndex	= queues.m_queueFamilyIndex[queueNdx];
		queueInfo.queueCount		= (queues.m_queueFamilyIndex[WRITE] == queues.m_queueFamilyIndex[READ]) ? static_cast<deUint32>(COUNT) : 1u;
		queueInfo.pQueuePriorities	= queuePriorities;

		queueInfos[queueNdx]	= queueInfo;

		if (queues.m_queueFamilyIndex[WRITE] == queues.m_queueFamilyIndex[READ])
			break;
	}

	deMemset(&deviceInfo, 0, sizeof(deviceInfo));
	instance.getPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

	deviceInfo.sType					= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.pNext					= DE_NULL;
	deviceInfo.enabledExtensionCount	= 0u;
	deviceInfo.ppEnabledExtensionNames	= DE_NULL;
	deviceInfo.enabledLayerCount		= 0u;
	deviceInfo.ppEnabledLayerNames		= DE_NULL;
	deviceInfo.pEnabledFeatures			= &deviceFeatures;
	deviceInfo.queueCreateInfoCount		= (queues.m_queueFamilyIndex[WRITE] == queues.m_queueFamilyIndex[READ]) ? 1u : static_cast<deUint32>(COUNT);
	deviceInfo.pQueueCreateInfos		= queueInfos;

	queues.m_logicalDevice = vk::createDevice(instance, physicalDevice, &deviceInfo);


	for (deUint32 queueReqNdx = 0; queueReqNdx < COUNT; ++queueReqNdx)
	{
		if (queues.m_queueFamilyIndex[WRITE] == queues.m_queueFamilyIndex[READ])
			vk.getDeviceQueue(*queues.m_logicalDevice, queues.m_queueFamilyIndex[queueReqNdx], queueReqNdx, &queues.m_queues[queueReqNdx]);
		else
			vk.getDeviceQueue(*queues.m_logicalDevice, queues.m_queueFamilyIndex[queueReqNdx], 0u, &queues.m_queues[queueReqNdx]);
	}
	queues.m_allocator = createAllocator (context, *queues.m_logicalDevice);

	return de::MovePtr<MultiQueues> (new MultiQueues(queues));
}

void createBarrierMultiQueue (const DeviceInterface& vk, const VkCommandBuffer& cmdBuffer, const SyncInfo& writeSync, const SyncInfo& readSync,
							  const Resource& resource, const MultiQueues& queues, const vk::VkSharingMode sharingMode, const bool secondQueue=false)
{
	if (resource.getType() == RESOURCE_TYPE_IMAGE)
	{
		VkImageMemoryBarrier barrier = makeImageMemoryBarrier(writeSync.accessMask, readSync.accessMask,
			writeSync.imageLayout, readSync.imageLayout, resource.getImage().handle, resource.getImage().subresourceRange);

		if (queues.m_queueFamilyIndex[WRITE] != queues.m_queueFamilyIndex[READ] && vk::VK_SHARING_MODE_EXCLUSIVE == sharingMode)
		{
			barrier.srcQueueFamilyIndex = queues.m_queueFamilyIndex[WRITE];
			barrier.dstQueueFamilyIndex = queues.m_queueFamilyIndex[READ];
			if (secondQueue)
			{
				barrier.oldLayout		= barrier.newLayout;
				barrier.srcAccessMask	= barrier.dstAccessMask;
			}
			vk.cmdPipelineBarrier(cmdBuffer, writeSync.stageMask, readSync.stageMask, (VkDependencyFlags)0, 0u, (const VkMemoryBarrier*)DE_NULL, 0u, (const VkBufferMemoryBarrier*)DE_NULL, 1u, &barrier);
		}
		else if (!secondQueue)
			vk.cmdPipelineBarrier(cmdBuffer, writeSync.stageMask, readSync.stageMask, (VkDependencyFlags)0, 0u, (const VkMemoryBarrier*)DE_NULL, 0u, (const VkBufferMemoryBarrier*)DE_NULL, 1u, &barrier);
	}
	else if ((resource.getType() == RESOURCE_TYPE_BUFFER || isIndirectBuffer(resource.getType())) &&
			 queues.m_queueFamilyIndex[WRITE] != queues.m_queueFamilyIndex[READ] &&
			 vk::VK_SHARING_MODE_EXCLUSIVE == sharingMode)
	{
		const VkBufferMemoryBarrier barrier =
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
				DE_NULL,									// const void*		pNext;
				writeSync.accessMask ,						// VkAccessFlags	srcAccessMask;
				readSync.accessMask,						// VkAccessFlags	dstAccessMask;
				queues.m_queueFamilyIndex[WRITE],			// deUint32			srcQueueFamilyIndex;
				queues.m_queueFamilyIndex[READ],			// deUint32			destQueueFamilyIndex;
				resource.getBuffer().handle,				// VkBuffer			buffer;
				resource.getBuffer().offset,				// VkDeviceSize		offset;
				resource.getBuffer().size,					// VkDeviceSize		size;
			};
		vk.cmdPipelineBarrier(cmdBuffer, writeSync.stageMask, readSync.stageMask, (VkDependencyFlags)0, 0u, (const VkMemoryBarrier*)DE_NULL, 1u, (const VkBufferMemoryBarrier*)&barrier, 0u, (const VkImageMemoryBarrier *)DE_NULL);
	}
}

class BaseTestInstance : public TestInstance
{
public:
	BaseTestInstance (Context& context, const ResourceDescription& resourceDesc, const OperationSupport& writeOp, const OperationSupport& readOp, PipelineCacheData& pipelineCacheData)
		: TestInstance	(context)
		, m_queues		(createQueues(context, writeOp.getQueueFlags(), readOp.getQueueFlags()))
		, m_opContext	(new OperationContext(context, pipelineCacheData, m_context.getDeviceInterface(), (*(*m_queues).m_logicalDevice), (*(*m_queues).m_allocator)))
		, m_resource	(new Resource(*m_opContext, resourceDesc, writeOp.getResourceUsageFlags() | readOp.getResourceUsageFlags()))
		, m_writeOp		(writeOp.build(*m_opContext, *m_resource))
		, m_readOp		(readOp.build(*m_opContext, *m_resource))
	{
	}

protected:
	de::UniquePtr<MultiQueues>		m_queues;
	de::UniquePtr<OperationContext>	m_opContext;
	de::UniquePtr<Resource>			m_resource;
	de::MovePtr<Operation>			m_writeOp;
	de::MovePtr<Operation>			m_readOp;
};

class SemaphoreTestInstance : public BaseTestInstance
{
public:
	SemaphoreTestInstance (Context& context, const ResourceDescription& resourceDesc, const OperationSupport& writeOp, const OperationSupport& readOp, PipelineCacheData& pipelineCacheData, const vk::VkSharingMode sharingMode)
		: BaseTestInstance	(context, resourceDesc, writeOp, readOp, pipelineCacheData)
		, m_sharingMode		(sharingMode)
	{
	}

	tcu::TestStatus	iterate (void)
	{
		const DeviceInterface&			vk					= (*m_opContext).getDeviceInterface();
		const VkDevice					device				= (*m_opContext).getDevice();
		const Move<VkCommandPool>		cmdPool[COUNT]		= {makeCommandPool(vk, device, (*m_queues).m_queueFamilyIndex[WRITE]), makeCommandPool(vk, device, (*m_queues).m_queueFamilyIndex[READ])};
		const Move<VkCommandBuffer>		ptrCmdBuffer[COUNT]	= {makeCommandBuffer(vk, device, *cmdPool[WRITE]), makeCommandBuffer(vk, device, *cmdPool[READ])};
		VkCommandBuffer					cmdBuffers[COUNT]	= {*ptrCmdBuffer[WRITE], *ptrCmdBuffer[READ]};
		const VkSemaphoreCreateInfo		semaphoreInfo		=
															{
																VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,	//VkStructureType			sType;
																DE_NULL,									//const void*				pNext;
																0u											//VkSemaphoreCreateFlags	flags;
															};
		const Unique<VkSemaphore>		semaphore			(createSemaphore(vk, device, &semaphoreInfo, DE_NULL));
		const VkPipelineStageFlags		stageBits[]			= { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };
		const VkSubmitInfo				submitInfo[COUNT]	=
															{
																{
																	VK_STRUCTURE_TYPE_SUBMIT_INFO,		// VkStructureType			sType;
																	DE_NULL,							// const void*				pNext;
																	0u,									// deUint32					waitSemaphoreCount;
																	DE_NULL,							// const VkSemaphore*		pWaitSemaphores;
																	(const VkPipelineStageFlags*)DE_NULL,
																	1u,									// deUint32					commandBufferCount;
																	&cmdBuffers[WRITE],					// const VkCommandBuffer*	pCommandBuffers;
																	1u,									// deUint32					signalSemaphoreCount;
																	&semaphore.get(),					// const VkSemaphore*		pSignalSemaphores;
																},
																{
																	VK_STRUCTURE_TYPE_SUBMIT_INFO,		// VkStructureType				sType;
																	DE_NULL,							// const void*					pNext;
																	1u,									// deUint32						waitSemaphoreCount;
																	&semaphore.get(),					// const VkSemaphore*			pWaitSemaphores;
																	stageBits,							// const VkPipelineStageFlags*	pWaitDstStageMask;
																	1u,									// deUint32						commandBufferCount;
																	&cmdBuffers[READ],					// const VkCommandBuffer*		pCommandBuffers;
																	0u,									// deUint32						signalSemaphoreCount;
																	DE_NULL,							// const VkSemaphore*			pSignalSemaphores;
																}
															};
		const SyncInfo					writeSync			= m_writeOp->getSyncInfo();
		const SyncInfo					readSync			= m_readOp->getSyncInfo();

		beginCommandBuffer(vk, cmdBuffers[WRITE]);
		m_writeOp->recordCommands(cmdBuffers[WRITE]);
		createBarrierMultiQueue(vk,cmdBuffers[WRITE], writeSync, readSync, (*m_resource), (*m_queues), m_sharingMode);
		endCommandBuffer(vk, cmdBuffers[WRITE]);

		beginCommandBuffer(vk, cmdBuffers[READ]);
		createBarrierMultiQueue(vk,cmdBuffers[READ], writeSync, readSync, (*m_resource), (*m_queues), m_sharingMode, true);
		m_readOp->recordCommands(cmdBuffers[READ]);
		endCommandBuffer(vk, cmdBuffers[READ]);

		VK_CHECK(vk.queueSubmit((*m_queues).m_queues[WRITE], 1u, &submitInfo[WRITE], DE_NULL));
		VK_CHECK(vk.queueSubmit((*m_queues).m_queues[READ], 1u, &submitInfo[READ], DE_NULL));
		VK_CHECK(vk.queueWaitIdle((*m_queues).m_queues[WRITE]));
		VK_CHECK(vk.queueWaitIdle((*m_queues).m_queues[READ]));

		{
			const Data	expected = m_writeOp->getData();
			const Data	actual	 = m_readOp->getData();

			if (0 != deMemCmp(expected.data, actual.data, expected.size))
				return tcu::TestStatus::fail("Memory contents don't match");
		}
		return tcu::TestStatus::pass("OK");
	}
private:
	const vk::VkSharingMode		m_sharingMode;
};

class FenceTestInstance : public BaseTestInstance
{
public:
	FenceTestInstance (Context& context, const ResourceDescription& resourceDesc, const OperationSupport& writeOp, const OperationSupport& readOp, PipelineCacheData& pipelineCacheData, const vk::VkSharingMode sharingMode)
		: BaseTestInstance	(context, resourceDesc, writeOp, readOp, pipelineCacheData)
		, m_sharingMode		(sharingMode)
	{
	}

	tcu::TestStatus	iterate (void)
	{
		const DeviceInterface&			vk					= (*m_opContext).getDeviceInterface();
		const VkDevice					device				= (*m_opContext).getDevice();
		const Move<VkCommandPool>		cmdPool[COUNT]		= {makeCommandPool(vk, device, (*m_queues).m_queueFamilyIndex[WRITE]), makeCommandPool(vk, device, (*m_queues).m_queueFamilyIndex[READ])};
		const Move<VkCommandBuffer>		ptrCmdBuffer[COUNT]	= {makeCommandBuffer(vk, device, *cmdPool[WRITE]), makeCommandBuffer(vk, device, *cmdPool[READ])};
		VkCommandBuffer					cmdBuffers[COUNT]	= {*ptrCmdBuffer[WRITE], *ptrCmdBuffer[READ]};
		const SyncInfo					writeSync			= m_writeOp->getSyncInfo();
		const SyncInfo					readSync			= m_readOp->getSyncInfo();

		beginCommandBuffer(vk, cmdBuffers[WRITE]);
		m_writeOp->recordCommands(cmdBuffers[WRITE]);
		createBarrierMultiQueue(vk,cmdBuffers[WRITE], writeSync, readSync, (*m_resource), (*m_queues), m_sharingMode);
		endCommandBuffer(vk, cmdBuffers[WRITE]);

		submitCommandsAndWait(vk, device, (*m_queues).m_queues[WRITE], cmdBuffers[WRITE]);

		beginCommandBuffer(vk, cmdBuffers[READ]);
		createBarrierMultiQueue(vk,cmdBuffers[READ], writeSync, readSync, (*m_resource), (*m_queues), m_sharingMode, true);
		m_readOp->recordCommands(cmdBuffers[READ]);
		endCommandBuffer(vk, cmdBuffers[READ]);

		submitCommandsAndWait(vk, device, (*m_queues).m_queues[READ], cmdBuffers[READ]);

		{
			const Data	expected = m_writeOp->getData();
			const Data	actual	 = m_readOp->getData();

			if (0 != deMemCmp(expected.data, actual.data, expected.size))
				return tcu::TestStatus::fail("Memory contents don't match");
		}
		return tcu::TestStatus::pass("OK");
	}

private:
	const vk::VkSharingMode		m_sharingMode;
};

class BaseTestCase : public TestCase
{
public:
	BaseTestCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description,
					const ResourceDescription resourceDesc, const OperationName writeOp, const OperationName readOp)
		: TestCase				(testCtx, name, description)
		, m_resourceDesc		(resourceDesc)
		, m_writeOp				(makeOperationSupport(writeOp, resourceDesc))
		, m_readOp				(makeOperationSupport(readOp, resourceDesc))
	{
	}

	void initPrograms (SourceCollections& programCollection) const
	{
		m_writeOp->initPrograms(programCollection);
		m_readOp->initPrograms(programCollection);
	}

protected:
	const ResourceDescription				m_resourceDesc;
	const de::UniquePtr<OperationSupport>	m_writeOp;
	const de::UniquePtr<OperationSupport>	m_readOp;
};

class SyncTestCase : public TestCase
{
public:
	SyncTestCase	(tcu::TestContext&			testCtx,
					const std::string&			name,
					const std::string&			description,
					const SyncPrimitive			syncPrimitive,
					const ResourceDescription	resourceDesc,
					const OperationName			writeOp,
					const OperationName			readOp,
					const vk::VkSharingMode		sharingMode,
					PipelineCacheData&			pipelineCacheData)
		: TestCase				(testCtx, name, description)
		, m_resourceDesc		(resourceDesc)
		, m_writeOp				(makeOperationSupport(writeOp, resourceDesc))
		, m_readOp				(makeOperationSupport(readOp, resourceDesc))
		, m_syncPrimitive		(syncPrimitive)
		, m_sharingMode			(sharingMode)
		, m_pipelineCacheData	(pipelineCacheData)
	{
	}

	void initPrograms (SourceCollections& programCollection) const
	{
		m_writeOp->initPrograms(programCollection);
		m_readOp->initPrograms(programCollection);
	}

	TestInstance* createInstance (Context& context) const
	{
		switch (m_syncPrimitive)
		{
			case SYNC_PRIMITIVE_FENCE:
				return new FenceTestInstance(context, m_resourceDesc, *m_writeOp, *m_readOp, m_pipelineCacheData, m_sharingMode);
			case SYNC_PRIMITIVE_SEMAPHORE:
				return new SemaphoreTestInstance(context, m_resourceDesc, *m_writeOp, *m_readOp, m_pipelineCacheData, m_sharingMode);
			default :
				DE_ASSERT(0);
				return DE_NULL;
		}
	}

private:
	const ResourceDescription				m_resourceDesc;
	const de::UniquePtr<OperationSupport>	m_writeOp;
	const de::UniquePtr<OperationSupport>	m_readOp;
	const SyncPrimitive						m_syncPrimitive;
	const vk::VkSharingMode					m_sharingMode;
	PipelineCacheData&						m_pipelineCacheData;
};

void createTests (tcu::TestCaseGroup* group, PipelineCacheData* pipelineCacheData)
{
	tcu::TestContext& testCtx = group->getTestContext();

	static const struct
	{
		const char*		name;
		SyncPrimitive	syncPrimitive;
		int				numOptions;
	} groups[] =
	{
		{ "fence",		SYNC_PRIMITIVE_FENCE,		1 },
		{ "semaphore",	SYNC_PRIMITIVE_SEMAPHORE,	1 }
	};

	for (int groupNdx = 0; groupNdx < DE_LENGTH_OF_ARRAY(groups); ++groupNdx)
	{
		de::MovePtr<tcu::TestCaseGroup> synchGroup (new tcu::TestCaseGroup(testCtx, groups[groupNdx].name, ""));

		for (int writeOpNdx = 0; writeOpNdx < DE_LENGTH_OF_ARRAY(s_writeOps); ++writeOpNdx)
		for (int readOpNdx = 0; readOpNdx < DE_LENGTH_OF_ARRAY(s_readOps); ++readOpNdx)
		{
			const OperationName	writeOp		= s_writeOps[writeOpNdx];
			const OperationName	readOp		= s_readOps[readOpNdx];
			const std::string	opGroupName = getOperationName(writeOp) + "_" + getOperationName(readOp);
			bool				empty		= true;

			de::MovePtr<tcu::TestCaseGroup> opGroup	(new tcu::TestCaseGroup(testCtx, opGroupName.c_str(), ""));

			for (int optionNdx = 0; optionNdx <= groups[groupNdx].numOptions; ++optionNdx)
			for (int resourceNdx = 0; resourceNdx < DE_LENGTH_OF_ARRAY(s_resources); ++resourceNdx)
			{
				const ResourceDescription&	resource	= s_resources[resourceNdx];
				std::string					name		= getResourceName(resource);

				vk::VkSharingMode sharingMode = vk::VK_SHARING_MODE_EXCLUSIVE;
				// queue family sharing mode used for resource
				if (optionNdx)
				{
					name += "_concurrent";
					sharingMode = vk::VK_SHARING_MODE_CONCURRENT;
				}
				else
					name += "_exclusive";

				if (isResourceSupported(writeOp, resource) && isResourceSupported(readOp, resource))
				{
					opGroup->addChild(new SyncTestCase(testCtx, name, "", groups[groupNdx].syncPrimitive, resource, writeOp, readOp, sharingMode, *pipelineCacheData));
					empty = false;
				}
			}
			if (!empty)
				synchGroup->addChild(opGroup.release());
		}

		group->addChild(synchGroup.release());
	}
}

} // anonymous

tcu::TestCaseGroup* createSynchronizedOperationMultiQueueTests (tcu::TestContext& testCtx, PipelineCacheData& pipelineCacheData)
{
	return createTestGroup(testCtx, "multi_queue", "Synchronization of a memory-modifying operation", createTests, &pipelineCacheData);
}

} // synchronization
} // vkt
