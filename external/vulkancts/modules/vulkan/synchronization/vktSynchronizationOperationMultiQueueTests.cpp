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
#include "vktCustomInstancesDevices.hpp"
#include "vkDefs.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkPlatform.hpp"
#include "vkCmdUtil.hpp"
#include "deRandom.hpp"
#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "tcuTestLog.hpp"
#include "vktSynchronizationUtil.hpp"
#include "vktSynchronizationOperation.hpp"
#include "vktSynchronizationOperationTestData.hpp"
#include "vktSynchronizationOperationResources.hpp"
#include "vktTestGroupUtil.hpp"
#include "tcuCommandLine.hpp"

#include <set>

namespace vkt
{

namespace synchronization
{

namespace
{
using namespace vk;
using de::MovePtr;
using de::SharedPtr;
using de::UniquePtr;
using de::SharedPtr;

enum QueueType
{
	QUEUETYPE_WRITE,
	QUEUETYPE_READ
};

struct QueuePair
{
	QueuePair	(const deUint32 familyWrite, const deUint32 familyRead, const VkQueue write, const VkQueue read)
		: familyIndexWrite	(familyWrite)
		, familyIndexRead	(familyRead)
		, queueWrite		(write)
		, queueRead			(read)
	{}

	deUint32	familyIndexWrite;
	deUint32	familyIndexRead;
	VkQueue		queueWrite;
	VkQueue		queueRead;
};

struct Queue
{
	Queue	(const deUint32 familyOp, const VkQueue queueOp)
		:	family	(familyOp)
		,	queue	(queueOp)
	{}

	deUint32	family;
	VkQueue		queue;
};

bool checkQueueFlags (VkQueueFlags availableFlags, const VkQueueFlags neededFlags)
{
	if ((availableFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) != 0)
		availableFlags |= VK_QUEUE_TRANSFER_BIT;

	return (availableFlags & neededFlags) != 0;
}

class MultiQueues
{
	struct QueueData
	{
		VkQueueFlags			flags;
		std::vector<VkQueue>	queue;
	};

	MultiQueues	(const Context& context, SynchronizationType type, bool timelineSemaphore)
		: m_queueCount	(0)
	{
		const InstanceInterface&					instance				= context.getInstanceInterface();
		const VkPhysicalDevice						physicalDevice			= context.getPhysicalDevice();
		const std::vector<VkQueueFamilyProperties>	queueFamilyProperties	= getPhysicalDeviceQueueFamilyProperties(instance, physicalDevice);

		for (deUint32 queuePropertiesNdx = 0; queuePropertiesNdx < queueFamilyProperties.size(); ++queuePropertiesNdx)
		{
			addQueueIndex(queuePropertiesNdx,
						  std::min(2u, queueFamilyProperties[queuePropertiesNdx].queueCount),
						  queueFamilyProperties[queuePropertiesNdx].queueFlags);
		}

		std::vector<VkDeviceQueueCreateInfo>	queueInfos;
		const float								queuePriorities[2] = { 1.0f, 1.0f };	//get max 2 queues from one family

		for (std::map<deUint32, QueueData>::iterator it = m_queues.begin(); it!= m_queues.end(); ++it)
		{
			const VkDeviceQueueCreateInfo queueInfo	=
			{
				VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,		//VkStructureType			sType;
				DE_NULL,										//const void*				pNext;
				(VkDeviceQueueCreateFlags)0u,					//VkDeviceQueueCreateFlags	flags;
				it->first,										//deUint32					queueFamilyIndex;
				static_cast<deUint32>(it->second.queue.size()),	//deUint32					queueCount;
				&queuePriorities[0]								//const float*				pQueuePriorities;
			};
			queueInfos.push_back(queueInfo);
		}

		{
			VkPhysicalDeviceFeatures2					createPhysicalFeature		{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, DE_NULL, context.getDeviceFeatures() };
			VkPhysicalDeviceTimelineSemaphoreFeatures	timelineSemaphoreFeatures	{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES, DE_NULL, DE_TRUE };
			VkPhysicalDeviceSynchronization2FeaturesKHR	synchronization2Features	{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR, DE_NULL, DE_TRUE };
			void**										nextPtr						= &createPhysicalFeature.pNext;

			std::vector<const char*> deviceExtensions;
			if (timelineSemaphore)
			{
				deviceExtensions.push_back("VK_KHR_timeline_semaphore");
				addToChainVulkanStructure(&nextPtr, timelineSemaphoreFeatures);
			}
			if (type == SynchronizationType::SYNCHRONIZATION2)
			{
				deviceExtensions.push_back("VK_KHR_synchronization2");
				addToChainVulkanStructure(&nextPtr, synchronization2Features);
			}

			const VkDeviceCreateInfo deviceInfo =
			{
				VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,							//VkStructureType					sType;
				&createPhysicalFeature,											//const void*						pNext;
				0u,																//VkDeviceCreateFlags				flags;
				static_cast<deUint32>(queueInfos.size()),						//deUint32							queueCreateInfoCount;
				&queueInfos[0],													//const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
				0u,																//deUint32							enabledLayerCount;
				DE_NULL,														//const char* const*				ppEnabledLayerNames;
				static_cast<deUint32>(deviceExtensions.size()),					//deUint32							enabledExtensionCount;
				deviceExtensions.empty() ? DE_NULL : &deviceExtensions[0],		//const char* const*				ppEnabledExtensionNames;
				DE_NULL															//const VkPhysicalDeviceFeatures*	pEnabledFeatures;
			};

			m_logicalDevice	= createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), context.getPlatformInterface(), context.getInstance(), instance, physicalDevice, &deviceInfo);
			m_deviceDriver	= MovePtr<DeviceDriver>(new DeviceDriver(context.getPlatformInterface(), context.getInstance(), *m_logicalDevice));
			m_allocator		= MovePtr<Allocator>(new SimpleAllocator(*m_deviceDriver, *m_logicalDevice, getPhysicalDeviceMemoryProperties(instance, physicalDevice)));

			for (std::map<deUint32, QueueData>::iterator it = m_queues.begin(); it != m_queues.end(); ++it)
			for (int queueNdx = 0; queueNdx < static_cast<int>(it->second.queue.size()); ++queueNdx)
				m_deviceDriver->getDeviceQueue(*m_logicalDevice, it->first, queueNdx, &it->second.queue[queueNdx]);
		}
	}

	void addQueueIndex (const deUint32 queueFamilyIndex, const deUint32 count, const VkQueueFlags flags)
	{
		QueueData dataToPush;
		dataToPush.flags = flags;
		dataToPush.queue.resize(count);
		m_queues[queueFamilyIndex] = dataToPush;

		m_queueCount++;
	}

public:
	std::vector<QueuePair> getQueuesPairs (const VkQueueFlags flagsWrite, const VkQueueFlags flagsRead) const
	{
		std::map<deUint32, QueueData>	queuesWrite;
		std::map<deUint32, QueueData>	queuesRead;
		std::vector<QueuePair>			queuesPairs;

		for (std::map<deUint32, QueueData>::const_iterator it = m_queues.begin(); it != m_queues.end(); ++it)
		{
			const bool writeQueue	= checkQueueFlags(it->second.flags, flagsWrite);
			const bool readQueue	= checkQueueFlags(it->second.flags, flagsRead);

			if (!(writeQueue || readQueue))
				continue;

			if (writeQueue && readQueue)
			{
				queuesWrite[it->first]	= it->second;
				queuesRead[it->first]	= it->second;
			}
			else if (writeQueue)
				queuesWrite[it->first]	= it->second;
			else if (readQueue)
				queuesRead[it->first]	= it->second;
		}

		for (std::map<deUint32, QueueData>::iterator write = queuesWrite.begin(); write != queuesWrite.end(); ++write)
		for (std::map<deUint32, QueueData>::iterator read  = queuesRead.begin();  read  != queuesRead.end();  ++read)
		{
			const int writeSize	= static_cast<int>(write->second.queue.size());
			const int readSize	= static_cast<int>(read->second.queue.size());

			for (int writeNdx = 0; writeNdx < writeSize; ++writeNdx)
			for (int readNdx  = 0; readNdx  < readSize;  ++readNdx)
			{
				if (write->second.queue[writeNdx] != read->second.queue[readNdx])
				{
					queuesPairs.push_back(QueuePair(write->first, read->first, write->second.queue[writeNdx], read->second.queue[readNdx]));
					writeNdx = readNdx = std::max(writeSize, readSize);	//exit from the loops
				}
			}
		}

		if (queuesPairs.empty())
			TCU_THROW(NotSupportedError, "Queue not found");

		return queuesPairs;
	}

	Queue getDefaultQueue(const VkQueueFlags flagsOp) const
	{
		for (std::map<deUint32, QueueData>::const_iterator it = m_queues.begin(); it!= m_queues.end(); ++it)
		{
			if (checkQueueFlags(it->second.flags, flagsOp))
				return Queue(it->first, it->second.queue[0]);
		}

		TCU_THROW(NotSupportedError, "Queue not found");
	}

	Queue getQueue (const deUint32 familyIdx, const deUint32 queueIdx)
	{
		return Queue(familyIdx, m_queues[familyIdx].queue[queueIdx]);
	}

	VkQueueFlags getQueueFamilyFlags (const deUint32 familyIdx)
	{
		return m_queues[familyIdx].flags;
	}

	deUint32 queueFamilyCount (const deUint32 familyIdx)
	{
		return (deUint32) m_queues[familyIdx].queue.size();
	}

	deUint32 familyCount (void) const
	{
		return (deUint32) m_queues.size();
	}

	deUint32 totalQueueCount (void)
	{
		deUint32	count	= 0;

		for (deUint32 familyIdx = 0; familyIdx < familyCount(); familyIdx++)
		{
			count	+= queueFamilyCount(familyIdx);
		}

		return count;
	}

	VkDevice getDevice (void) const
	{
		return *m_logicalDevice;
	}

	const DeviceInterface& getDeviceInterface (void) const
	{
		return *m_deviceDriver;
	}

	Allocator& getAllocator (void)
	{
		return *m_allocator;
	}

	static SharedPtr<MultiQueues> getInstance(const Context& context, SynchronizationType type, bool timelineSemaphore)
	{
		if (!m_multiQueues)
			m_multiQueues = SharedPtr<MultiQueues>(new MultiQueues(context, type, timelineSemaphore));

		return m_multiQueues;
	}
	static void destroy()
	{
		m_multiQueues.clear();
	}

private:
	Move<VkDevice>					m_logicalDevice;
	MovePtr<DeviceDriver>			m_deviceDriver;
	MovePtr<Allocator>				m_allocator;
	std::map<deUint32, QueueData>	m_queues;
	deUint32						m_queueCount;

	static SharedPtr<MultiQueues>	m_multiQueues;
};
SharedPtr<MultiQueues>				MultiQueues::m_multiQueues;

void createBarrierMultiQueue (SynchronizationWrapperPtr synchronizationWrapper,
							  const VkCommandBuffer&	cmdBuffer,
							  const SyncInfo&			writeSync,
							  const SyncInfo&			readSync,
							  const Resource&			resource,
							  const deUint32			writeFamily,
							  const deUint32			readFamily,
							  const VkSharingMode		sharingMode,
							  const bool				secondQueue = false)
{
	if (resource.getType() == RESOURCE_TYPE_IMAGE)
	{
		VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
			secondQueue ? VkPipelineStageFlags(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT) : writeSync.stageMask,
			secondQueue ? 0u : writeSync.accessMask,
			!secondQueue ? VkPipelineStageFlags(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT) : readSync.stageMask,
			!secondQueue ? 0u : readSync.accessMask,
			writeSync.imageLayout,
			readSync.imageLayout,
			resource.getImage().handle,
			resource.getImage().subresourceRange
		);

		if (writeFamily != readFamily && VK_SHARING_MODE_EXCLUSIVE == sharingMode)
		{
			imageMemoryBarrier2.srcQueueFamilyIndex = writeFamily;
			imageMemoryBarrier2.dstQueueFamilyIndex = readFamily;

			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
		}
		else if (!secondQueue)
		{
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
		}
	}
	else
	{
		VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 = makeBufferMemoryBarrier2(
			secondQueue ? VkPipelineStageFlags(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT) : writeSync.stageMask,
			secondQueue ? 0u : writeSync.accessMask,
			!secondQueue ? VkPipelineStageFlags(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT) : readSync.stageMask,
			!secondQueue ? 0u : readSync.accessMask,
			resource.getBuffer().handle,
			resource.getBuffer().offset,
			resource.getBuffer().size
		);

		if (writeFamily != readFamily && VK_SHARING_MODE_EXCLUSIVE == sharingMode)
		{
			bufferMemoryBarrier2.srcQueueFamilyIndex = writeFamily;
			bufferMemoryBarrier2.dstQueueFamilyIndex = readFamily;
		}

		VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, &bufferMemoryBarrier2);
		synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
	}
}

class BaseTestInstance : public TestInstance
{
public:
	BaseTestInstance (Context& context, SynchronizationType type, const ResourceDescription& resourceDesc, const OperationSupport& writeOp, const OperationSupport& readOp, PipelineCacheData& pipelineCacheData, bool timelineSemaphore)
		: TestInstance		(context)
		, m_type			(type)
		, m_queues			(MultiQueues::getInstance(context, type, timelineSemaphore))
		, m_opContext		(new OperationContext(context, type, m_queues->getDeviceInterface(), m_queues->getDevice(), m_queues->getAllocator(), pipelineCacheData))
		, m_resourceDesc	(resourceDesc)
		, m_writeOp			(writeOp)
		, m_readOp			(readOp)
	{
	}

protected:
	const SynchronizationType			m_type;
	const SharedPtr<MultiQueues>		m_queues;
	const UniquePtr<OperationContext>	m_opContext;
	const ResourceDescription			m_resourceDesc;
	const OperationSupport&				m_writeOp;
	const OperationSupport&				m_readOp;
};

class BinarySemaphoreTestInstance : public BaseTestInstance
{
public:
	BinarySemaphoreTestInstance (Context& context, SynchronizationType type, const ResourceDescription& resourceDesc, const OperationSupport& writeOp, const OperationSupport& readOp, PipelineCacheData& pipelineCacheData, const VkSharingMode sharingMode)
		: BaseTestInstance	(context, type, resourceDesc, writeOp, readOp, pipelineCacheData, false)
		, m_sharingMode		(sharingMode)
	{
	}

	tcu::TestStatus	iterate (void)
	{
		const DeviceInterface&			vk			= m_opContext->getDeviceInterface();
		const VkDevice					device		= m_opContext->getDevice();
		const std::vector<QueuePair>	queuePairs	= m_queues->getQueuesPairs(m_writeOp.getQueueFlags(*m_opContext), m_readOp.getQueueFlags(*m_opContext));

		for (deUint32 pairNdx = 0; pairNdx < static_cast<deUint32>(queuePairs.size()); ++pairNdx)
		{
			const UniquePtr<Resource>		resource		(new Resource(*m_opContext, m_resourceDesc, m_writeOp.getOutResourceUsageFlags() | m_readOp.getInResourceUsageFlags()));
			const UniquePtr<Operation>		writeOp			(m_writeOp.build(*m_opContext, *resource));
			const UniquePtr<Operation>		readOp			(m_readOp.build (*m_opContext, *resource));

			const Move<VkCommandPool>			cmdPool[]		=
			{
				createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queuePairs[pairNdx].familyIndexWrite),
				createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queuePairs[pairNdx].familyIndexRead)
			};
			const Move<VkCommandBuffer>			ptrCmdBuffer[]	=
			{
				makeCommandBuffer(vk, device, *cmdPool[QUEUETYPE_WRITE]),
				makeCommandBuffer(vk, device, *cmdPool[QUEUETYPE_READ])
			};
			const VkCommandBufferSubmitInfoKHR	cmdBufferInfos[]	=
			{
				makeCommonCommandBufferSubmitInfo(*ptrCmdBuffer[QUEUETYPE_WRITE]),
				makeCommonCommandBufferSubmitInfo(*ptrCmdBuffer[QUEUETYPE_READ]),
			};
			const Unique<VkSemaphore>			semaphore		(createSemaphore(vk, device));
			VkSemaphoreSubmitInfoKHR			waitSemaphoreSubmitInfo =
				makeCommonSemaphoreSubmitInfo(*semaphore, 0u, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR);
			VkSemaphoreSubmitInfoKHR			signalSemaphoreSubmitInfo =
				makeCommonSemaphoreSubmitInfo(*semaphore, 0u, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR);
			SynchronizationWrapperPtr			synchronizationWrapper[]
			{
				getSynchronizationWrapper(m_type, vk, DE_FALSE),
				getSynchronizationWrapper(m_type, vk, DE_FALSE),
			};

			synchronizationWrapper[QUEUETYPE_WRITE]->addSubmitInfo(
				0u,
				DE_NULL,
				1u,
				&cmdBufferInfos[QUEUETYPE_WRITE],
				1u,
				&signalSemaphoreSubmitInfo
			);
			synchronizationWrapper[QUEUETYPE_READ]->addSubmitInfo(
				1u,
				&waitSemaphoreSubmitInfo,
				1u,
				&cmdBufferInfos[QUEUETYPE_READ],
				0u,
				DE_NULL
			);

			const SyncInfo					writeSync		= writeOp->getOutSyncInfo();
			const SyncInfo					readSync		= readOp->getInSyncInfo();
			VkCommandBuffer					writeCmdBuffer	= cmdBufferInfos[QUEUETYPE_WRITE].commandBuffer;
			VkCommandBuffer					readCmdBuffer	= cmdBufferInfos[QUEUETYPE_READ].commandBuffer;

			beginCommandBuffer		(vk, writeCmdBuffer);
			writeOp->recordCommands	(writeCmdBuffer);
			createBarrierMultiQueue	(synchronizationWrapper[QUEUETYPE_WRITE], writeCmdBuffer, writeSync, readSync, *resource, queuePairs[pairNdx].familyIndexWrite, queuePairs[pairNdx].familyIndexRead, m_sharingMode);
			endCommandBuffer		(vk, writeCmdBuffer);

			beginCommandBuffer		(vk, readCmdBuffer);
			createBarrierMultiQueue	(synchronizationWrapper[QUEUETYPE_READ], readCmdBuffer, writeSync, readSync, *resource, queuePairs[pairNdx].familyIndexWrite, queuePairs[pairNdx].familyIndexRead, m_sharingMode, true);
			readOp->recordCommands	(readCmdBuffer);
			endCommandBuffer		(vk, readCmdBuffer);

			VK_CHECK(synchronizationWrapper[QUEUETYPE_WRITE]->queueSubmit(queuePairs[pairNdx].queueWrite, DE_NULL));
			VK_CHECK(synchronizationWrapper[QUEUETYPE_READ]->queueSubmit(queuePairs[pairNdx].queueRead, DE_NULL));
			VK_CHECK(vk.queueWaitIdle(queuePairs[pairNdx].queueWrite));
			VK_CHECK(vk.queueWaitIdle(queuePairs[pairNdx].queueRead));

			{
				const Data	expected	= writeOp->getData();
				const Data	actual		= readOp->getData();

				if (isIndirectBuffer(m_resourceDesc.type))
				{
					const deUint32 expectedValue = reinterpret_cast<const deUint32*>(expected.data)[0];
					const deUint32 actualValue   = reinterpret_cast<const deUint32*>(actual.data)[0];

					if (actualValue < expectedValue)
						return tcu::TestStatus::fail("Counter value is smaller than expected");
				}
				else
				{
					if (0 != deMemCmp(expected.data, actual.data, expected.size))
						return tcu::TestStatus::fail("Memory contents don't match");
				}
			}
		}
		return tcu::TestStatus::pass("OK");
	}

private:
	const VkSharingMode	m_sharingMode;
};

template<typename T>
inline SharedPtr<Move<T> > makeVkSharedPtr (Move<T> move)
{
	return SharedPtr<Move<T> >(new Move<T>(move));
}

class TimelineSemaphoreTestInstance : public BaseTestInstance
{
public:
	TimelineSemaphoreTestInstance (Context& context, SynchronizationType type, const ResourceDescription& resourceDesc, const SharedPtr<OperationSupport>& writeOp, const SharedPtr<OperationSupport>& readOp, PipelineCacheData& pipelineCacheData, const VkSharingMode sharingMode)
		: BaseTestInstance	(context, type, resourceDesc, *writeOp, *readOp, pipelineCacheData, true)
		, m_sharingMode		(sharingMode)
	{
		deUint32				maxQueues		= 0;
		std::vector<deUint32>	queueFamilies;

		if (!context.getTimelineSemaphoreFeatures().timelineSemaphore)
			TCU_THROW(NotSupportedError, "Timeline semaphore not supported");

		if (m_queues->totalQueueCount() < 2)
			TCU_THROW(NotSupportedError, "Not enough queues");

		for (deUint32 familyNdx = 0; familyNdx < m_queues->familyCount(); familyNdx++)
		{
			maxQueues = std::max(m_queues->queueFamilyCount(familyNdx), maxQueues);
			queueFamilies.push_back(familyNdx);
		}

		// Create a chain of operations copying data from one resource
		// to another across at least every single queue of the system
		// at least once. Each of the operation will be executing with
		// a dependency on the previous using timeline points.
		m_opSupports.push_back(writeOp);
		m_opQueues.push_back(m_queues->getDefaultQueue(writeOp->getQueueFlags(*m_opContext)));

		for (deUint32 queueIdx = 0; queueIdx < maxQueues; queueIdx++)
		{
			for (deUint32 familyIdx = 0; familyIdx < m_queues->familyCount(); familyIdx++)
			{
				for (deUint32 copyOpIdx = 0; copyOpIdx < DE_LENGTH_OF_ARRAY(s_copyOps); copyOpIdx++)
				{
					if (isResourceSupported(s_copyOps[copyOpIdx], resourceDesc))
					{
						SharedPtr<OperationSupport>	opSupport	(makeOperationSupport(s_copyOps[copyOpIdx], m_resourceDesc).release());

						if (!checkQueueFlags(opSupport->getQueueFlags(*m_opContext), m_queues->getQueueFamilyFlags(familyIdx)))
							continue;

						m_opSupports.push_back(opSupport);
						m_opQueues.push_back(m_queues->getQueue(familyIdx, queueIdx % m_queues->queueFamilyCount(familyIdx)));
						break;
					}
				}
			}
		}

		m_opSupports.push_back(readOp);
		m_opQueues.push_back(m_queues->getDefaultQueue(readOp->getQueueFlags(*m_opContext)));

		// Now create the resources with the usage associated to the
		// operation performed on the resource.
		for (deUint32 opIdx = 0; opIdx < (m_opSupports.size() - 1); opIdx++)
		{
			deUint32 usage = m_opSupports[opIdx]->getOutResourceUsageFlags() | m_opSupports[opIdx + 1]->getInResourceUsageFlags();

			m_resources.push_back(SharedPtr<Resource>(new Resource(*m_opContext, m_resourceDesc, usage, m_sharingMode, queueFamilies)));
		}

		// Finally create the operations using the resources.
		m_ops.push_back(SharedPtr<Operation>(m_opSupports[0]->build(*m_opContext, *m_resources[0]).release()));
		for (deUint32 opIdx = 1; opIdx < (m_opSupports.size() - 1); opIdx++)
			m_ops.push_back(SharedPtr<Operation>(m_opSupports[opIdx]->build(*m_opContext, *m_resources[opIdx - 1], *m_resources[opIdx]).release()));
		m_ops.push_back(SharedPtr<Operation>(m_opSupports[m_opSupports.size() - 1]->build(*m_opContext, *m_resources.back()).release()));
	}

	tcu::TestStatus	iterate (void)
	{
		const DeviceInterface&							vk				= m_opContext->getDeviceInterface();
		const VkDevice									device			= m_opContext->getDevice();
		de::Random										rng				(1234);
		const Unique<VkSemaphore>						semaphore		(createSemaphoreType(vk, device, VK_SEMAPHORE_TYPE_TIMELINE_KHR));
		std::vector<SharedPtr<Move<VkCommandPool> > >	cmdPools;
		std::vector<SharedPtr<Move<VkCommandBuffer> > >	ptrCmdBuffers;
		std::vector<VkCommandBufferSubmitInfoKHR>		cmdBufferInfos;
		std::vector<deUint64>							timelineValues;

		cmdPools.resize(m_queues->familyCount());
		for (deUint32 familyIdx = 0; familyIdx < m_queues->familyCount(); familyIdx++)
			cmdPools[familyIdx] = makeVkSharedPtr(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, familyIdx));

		ptrCmdBuffers.resize(m_ops.size());
		cmdBufferInfos.resize(m_ops.size());
		for (deUint32 opIdx = 0; opIdx < m_ops.size(); opIdx++)
		{
			deUint64	increment	= 1 + rng.getUint8();

			ptrCmdBuffers[opIdx] = makeVkSharedPtr(makeCommandBuffer(vk, device, **cmdPools[m_opQueues[opIdx].family]));
			cmdBufferInfos[opIdx] = makeCommonCommandBufferSubmitInfo(**ptrCmdBuffers[opIdx]);

			timelineValues.push_back(timelineValues.empty() ? increment : (timelineValues.back() + increment));
		}

		for (deUint32 opIdx = 0; opIdx < m_ops.size(); opIdx++)
		{
			VkCommandBuffer				cmdBuffer = cmdBufferInfos[opIdx].commandBuffer;
			VkSemaphoreSubmitInfoKHR	waitSemaphoreSubmitInfo =
				makeCommonSemaphoreSubmitInfo(*semaphore, (opIdx == 0 ? 0u : timelineValues[opIdx - 1]), VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR);
			VkSemaphoreSubmitInfoKHR	signalSemaphoreSubmitInfo =
				makeCommonSemaphoreSubmitInfo(*semaphore, timelineValues[opIdx], VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR);
			SynchronizationWrapperPtr	synchronizationWrapper = getSynchronizationWrapper(m_type, vk, DE_TRUE);

			synchronizationWrapper->addSubmitInfo(
				opIdx == 0 ? 0u : 1u,
				&waitSemaphoreSubmitInfo,
				1u,
				&cmdBufferInfos[opIdx],
				1u,
				&signalSemaphoreSubmitInfo,
				opIdx == 0 ? DE_FALSE : DE_TRUE,
				DE_TRUE
			);

			beginCommandBuffer(vk, cmdBuffer);

			if (opIdx > 0)
			{
				const SyncInfo	writeSync	= m_ops[opIdx - 1]->getOutSyncInfo();
				const SyncInfo	readSync	= m_ops[opIdx]->getInSyncInfo();
				const Resource&	resource	= *m_resources[opIdx - 1].get();

				createBarrierMultiQueue(synchronizationWrapper, cmdBuffer, writeSync, readSync, resource, m_opQueues[opIdx - 1].family, m_opQueues[opIdx].family, m_sharingMode, true);
			}

			m_ops[opIdx]->recordCommands(cmdBuffer);

			if (opIdx < (m_ops.size() - 1))
			{
				const SyncInfo	writeSync	= m_ops[opIdx]->getOutSyncInfo();
				const SyncInfo	readSync	= m_ops[opIdx + 1]->getInSyncInfo();
				const Resource&	resource	= *m_resources[opIdx].get();

				createBarrierMultiQueue(synchronizationWrapper, cmdBuffer, writeSync, readSync, resource, m_opQueues[opIdx].family, m_opQueues[opIdx + 1].family, m_sharingMode);
			}

			endCommandBuffer(vk, cmdBuffer);

			VK_CHECK(synchronizationWrapper->queueSubmit(m_opQueues[opIdx].queue, DE_NULL));
		}


		VK_CHECK(vk.queueWaitIdle(m_opQueues.back().queue));

		{
			const Data	expected	= m_ops.front()->getData();
			const Data	actual		= m_ops.back()->getData();

			if (isIndirectBuffer(m_resourceDesc.type))
			{
				const deUint32 expectedValue = reinterpret_cast<const deUint32*>(expected.data)[0];
				const deUint32 actualValue   = reinterpret_cast<const deUint32*>(actual.data)[0];

				if (actualValue < expectedValue)
					return tcu::TestStatus::fail("Counter value is smaller than expected");
			}
			else
			{
				if (0 != deMemCmp(expected.data, actual.data, expected.size))
					return tcu::TestStatus::fail("Memory contents don't match");
			}
		}

		// Make the validation layers happy.
		for (deUint32 opIdx = 0; opIdx < m_opQueues.size(); opIdx++)
			VK_CHECK(vk.queueWaitIdle(m_opQueues[opIdx].queue));

		return tcu::TestStatus::pass("OK");
	}

private:
	const VkSharingMode							m_sharingMode;
	std::vector<SharedPtr<OperationSupport> >	m_opSupports;
	std::vector<SharedPtr<Operation> >			m_ops;
	std::vector<SharedPtr<Resource> >			m_resources;
	std::vector<Queue>							m_opQueues;
};

class FenceTestInstance : public BaseTestInstance
{
public:
	FenceTestInstance (Context& context, SynchronizationType type, const ResourceDescription& resourceDesc, const OperationSupport& writeOp, const OperationSupport& readOp, PipelineCacheData& pipelineCacheData, const VkSharingMode sharingMode)
		: BaseTestInstance	(context, type, resourceDesc, writeOp, readOp, pipelineCacheData, false)
		, m_sharingMode		(sharingMode)
	{
	}

	tcu::TestStatus	iterate (void)
	{
		const DeviceInterface&			vk			= m_opContext->getDeviceInterface();
		const VkDevice					device		= m_opContext->getDevice();
		const std::vector<QueuePair>	queuePairs	= m_queues->getQueuesPairs(m_writeOp.getQueueFlags(*m_opContext), m_readOp.getQueueFlags(*m_opContext));

		for (deUint32 pairNdx = 0; pairNdx < static_cast<deUint32>(queuePairs.size()); ++pairNdx)
		{
			const UniquePtr<Resource>		resource		(new Resource(*m_opContext, m_resourceDesc, m_writeOp.getOutResourceUsageFlags() | m_readOp.getInResourceUsageFlags()));
			const UniquePtr<Operation>		writeOp			(m_writeOp.build(*m_opContext, *resource));
			const UniquePtr<Operation>		readOp			(m_readOp.build(*m_opContext, *resource));
			const Move<VkCommandPool>		cmdPool[]
			{
				createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queuePairs[pairNdx].familyIndexWrite),
				createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queuePairs[pairNdx].familyIndexRead)
			};
			const Move<VkCommandBuffer>		ptrCmdBuffer[]
			{
				makeCommandBuffer(vk, device, *cmdPool[QUEUETYPE_WRITE]),
				makeCommandBuffer(vk, device, *cmdPool[QUEUETYPE_READ])
			};
			const VkCommandBufferSubmitInfoKHR	cmdBufferInfos[]
			{
				makeCommonCommandBufferSubmitInfo(*ptrCmdBuffer[QUEUETYPE_WRITE]),
				makeCommonCommandBufferSubmitInfo(*ptrCmdBuffer[QUEUETYPE_READ])
			};
			SynchronizationWrapperPtr		synchronizationWrapper[]
			{
				getSynchronizationWrapper(m_type, vk, DE_FALSE),
				getSynchronizationWrapper(m_type, vk, DE_FALSE),
			};
			const SyncInfo					writeSync		= writeOp->getOutSyncInfo();
			const SyncInfo					readSync		= readOp->getInSyncInfo();
			VkCommandBuffer					writeCmdBuffer	= cmdBufferInfos[QUEUETYPE_WRITE].commandBuffer;
			VkCommandBuffer					readCmdBuffer	= cmdBufferInfos[QUEUETYPE_READ].commandBuffer;

			beginCommandBuffer		(vk, writeCmdBuffer);
			writeOp->recordCommands	(writeCmdBuffer);
			createBarrierMultiQueue	(synchronizationWrapper[QUEUETYPE_WRITE], writeCmdBuffer, writeSync, readSync, *resource, queuePairs[pairNdx].familyIndexWrite, queuePairs[pairNdx].familyIndexRead, m_sharingMode);
			endCommandBuffer		(vk, writeCmdBuffer);

			submitCommandsAndWait	(synchronizationWrapper[QUEUETYPE_WRITE], vk, device, queuePairs[pairNdx].queueWrite, writeCmdBuffer);

			beginCommandBuffer		(vk, readCmdBuffer);
			createBarrierMultiQueue	(synchronizationWrapper[QUEUETYPE_READ], readCmdBuffer, writeSync, readSync, *resource, queuePairs[pairNdx].familyIndexWrite, queuePairs[pairNdx].familyIndexRead, m_sharingMode, true);
			readOp->recordCommands	(readCmdBuffer);
			endCommandBuffer		(vk, readCmdBuffer);

			submitCommandsAndWait(synchronizationWrapper[QUEUETYPE_READ], vk, device, queuePairs[pairNdx].queueRead, readCmdBuffer);

			{
				const Data	expected = writeOp->getData();
				const Data	actual	 = readOp->getData();

				if (isIndirectBuffer(m_resourceDesc.type))
				{
					const deUint32 expectedValue = reinterpret_cast<const deUint32*>(expected.data)[0];
					const deUint32 actualValue   = reinterpret_cast<const deUint32*>(actual.data)[0];

					if (actualValue < expectedValue)
						return tcu::TestStatus::fail("Counter value is smaller than expected");
				}
				else
				{
					if (0 != deMemCmp(expected.data, actual.data, expected.size))
						return tcu::TestStatus::fail("Memory contents don't match");
				}
			}
		}
		return tcu::TestStatus::pass("OK");
	}

private:
	const VkSharingMode	m_sharingMode;
};

class BaseTestCase : public TestCase
{
public:
	BaseTestCase (tcu::TestContext&			testCtx,
				  const std::string&		name,
				  const std::string&		description,
				  SynchronizationType		type,
				  const SyncPrimitive		syncPrimitive,
				  const ResourceDescription	resourceDesc,
				  const OperationName		writeOp,
				  const OperationName		readOp,
				  const VkSharingMode		sharingMode,
				  PipelineCacheData&		pipelineCacheData)
		: TestCase				(testCtx, name, description)
		, m_type				(type)
		, m_resourceDesc		(resourceDesc)
		, m_writeOp				(makeOperationSupport(writeOp, resourceDesc).release())
		, m_readOp				(makeOperationSupport(readOp, resourceDesc).release())
		, m_syncPrimitive		(syncPrimitive)
		, m_sharingMode			(sharingMode)
		, m_pipelineCacheData	(pipelineCacheData)
	{
	}

	void initPrograms (SourceCollections& programCollection) const
	{
		m_writeOp->initPrograms(programCollection);
		m_readOp->initPrograms(programCollection);

		if (m_syncPrimitive == SYNC_PRIMITIVE_TIMELINE_SEMAPHORE)
		{
			for (deUint32 copyOpNdx = 0; copyOpNdx < DE_LENGTH_OF_ARRAY(s_copyOps); copyOpNdx++)
			{
				if (isResourceSupported(s_copyOps[copyOpNdx], m_resourceDesc))
					makeOperationSupport(s_copyOps[copyOpNdx], m_resourceDesc)->initPrograms(programCollection);
			}
		}
	}

	void checkSupport(Context& context) const
	{
		if (m_type == SynchronizationType::SYNCHRONIZATION2)
			context.requireDeviceFunctionality("VK_KHR_synchronization2");
		if (m_syncPrimitive == SYNC_PRIMITIVE_TIMELINE_SEMAPHORE)
			context.requireDeviceFunctionality("VK_KHR_timeline_semaphore");

		const InstanceInterface&					instance				= context.getInstanceInterface();
		const VkPhysicalDevice						physicalDevice			= context.getPhysicalDevice();
		const std::vector<VkQueueFamilyProperties>	queueFamilyProperties	= getPhysicalDeviceQueueFamilyProperties(instance, physicalDevice);
		if (m_sharingMode == VK_SHARING_MODE_CONCURRENT && queueFamilyProperties.size() < 2)
			TCU_THROW(NotSupportedError, "Concurrent requires more than 1 queue family");
	}

	TestInstance* createInstance (Context& context) const
	{
		switch (m_syncPrimitive)
		{
			case SYNC_PRIMITIVE_FENCE:
				return new FenceTestInstance(context, m_type, m_resourceDesc, *m_writeOp, *m_readOp, m_pipelineCacheData, m_sharingMode);
			case SYNC_PRIMITIVE_BINARY_SEMAPHORE:
				return new BinarySemaphoreTestInstance(context, m_type, m_resourceDesc, *m_writeOp, *m_readOp, m_pipelineCacheData, m_sharingMode);
			case SYNC_PRIMITIVE_TIMELINE_SEMAPHORE:
				return new TimelineSemaphoreTestInstance(context, m_type, m_resourceDesc, m_writeOp, m_readOp, m_pipelineCacheData, m_sharingMode);
			default:
				DE_ASSERT(0);
				return DE_NULL;
		}
	}

private:
	const SynchronizationType				m_type;
	const ResourceDescription				m_resourceDesc;
	const SharedPtr<OperationSupport>		m_writeOp;
	const SharedPtr<OperationSupport>		m_readOp;
	const SyncPrimitive						m_syncPrimitive;
	const VkSharingMode						m_sharingMode;
	PipelineCacheData&						m_pipelineCacheData;
};

struct TestData
{
	SynchronizationType		type;
	PipelineCacheData*		pipelineCacheData;
};

void createTests (tcu::TestCaseGroup* group, TestData data)
{
	tcu::TestContext& testCtx = group->getTestContext();

	static const struct
	{
		const char*		name;
		SyncPrimitive	syncPrimitive;
		int				numOptions;
	} groups[] =
	{
		{ "fence",				SYNC_PRIMITIVE_FENCE,				1 },
		{ "binary_semaphore",	SYNC_PRIMITIVE_BINARY_SEMAPHORE,	1 },
		{ "timeline_semaphore",	SYNC_PRIMITIVE_TIMELINE_SEMAPHORE,	1 }
	};

	for (int groupNdx = 0; groupNdx < DE_LENGTH_OF_ARRAY(groups); ++groupNdx)
	{
		MovePtr<tcu::TestCaseGroup> synchGroup (new tcu::TestCaseGroup(testCtx, groups[groupNdx].name, ""));

		for (int writeOpNdx = 0; writeOpNdx < DE_LENGTH_OF_ARRAY(s_writeOps); ++writeOpNdx)
		for (int readOpNdx  = 0; readOpNdx  < DE_LENGTH_OF_ARRAY(s_readOps);  ++readOpNdx)
		{
			const OperationName	writeOp		= s_writeOps[writeOpNdx];
			const OperationName	readOp		= s_readOps[readOpNdx];
			const std::string	opGroupName = getOperationName(writeOp) + "_" + getOperationName(readOp);
			bool				empty		= true;

			MovePtr<tcu::TestCaseGroup> opGroup		(new tcu::TestCaseGroup(testCtx, opGroupName.c_str(), ""));

			for (int optionNdx = 0; optionNdx <= groups[groupNdx].numOptions; ++optionNdx)
			for (int resourceNdx = 0; resourceNdx < DE_LENGTH_OF_ARRAY(s_resources); ++resourceNdx)
			{
				const ResourceDescription&	resource	= s_resources[resourceNdx];
				if (isResourceSupported(writeOp, resource) && isResourceSupported(readOp, resource))
				{
					std::string					name		= getResourceName(resource);
					VkSharingMode				sharingMode = VK_SHARING_MODE_EXCLUSIVE;

					// queue family sharing mode used for resource
					if (optionNdx)
					{
						name += "_concurrent";
						sharingMode = VK_SHARING_MODE_CONCURRENT;
					}
					else
						name += "_exclusive";

					opGroup->addChild(new BaseTestCase(testCtx, name, "", data.type, groups[groupNdx].syncPrimitive, resource, writeOp, readOp, sharingMode, *data.pipelineCacheData));
					empty = false;
				}
			}
			if (!empty)
				synchGroup->addChild(opGroup.release());
		}
		group->addChild(synchGroup.release());
	}
}

void cleanupGroup (tcu::TestCaseGroup* group, TestData data)
{
	DE_UNREF(group);
	DE_UNREF(data.pipelineCacheData);
	// Destroy singleton object
	MultiQueues::destroy();
}

} // anonymous

tcu::TestCaseGroup* createSynchronizedOperationMultiQueueTests (tcu::TestContext& testCtx, SynchronizationType type, PipelineCacheData& pipelineCacheData)
{
	TestData data
	{
		type,
		&pipelineCacheData
	};

	return createTestGroup(testCtx, "multi_queue", "Synchronization of a memory-modifying operation", createTests, data, cleanupGroup);
}

} // synchronization
} // vkt
