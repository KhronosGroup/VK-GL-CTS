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
 * \brief Synchronization primitive tests with single queue
 *//*--------------------------------------------------------------------*/

#include "vktSynchronizationOperationSingleQueueTests.hpp"
#include "vkDefs.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "deRandom.hpp"
#include "deUniquePtr.hpp"
#include "tcuTestLog.hpp"
#include "vktSynchronizationUtil.hpp"
#include "vktSynchronizationOperation.hpp"
#include "vktSynchronizationOperationTestData.hpp"
#include "vktSynchronizationOperationResources.hpp"

namespace vkt
{
namespace synchronization
{
namespace
{
using namespace vk;
using tcu::TestLog;

class BaseTestInstance : public TestInstance
{
public:
	BaseTestInstance (Context& context, SynchronizationType type, const ResourceDescription& resourceDesc, const OperationSupport& writeOp, const OperationSupport& readOp, PipelineCacheData& pipelineCacheData)
		: TestInstance	(context)
		, m_type		(type)
		, m_opContext	(context, type, pipelineCacheData)
		, m_resource	(new Resource(m_opContext, resourceDesc, writeOp.getOutResourceUsageFlags() | readOp.getInResourceUsageFlags()))
		, m_writeOp		(writeOp.build(m_opContext, *m_resource))
		, m_readOp		(readOp.build(m_opContext, *m_resource))
	{
	}

protected:
	SynchronizationType					m_type;
	OperationContext					m_opContext;
	const de::UniquePtr<Resource>		m_resource;
	const de::UniquePtr<Operation>		m_writeOp;
	const de::UniquePtr<Operation>		m_readOp;
};

class EventTestInstance : public BaseTestInstance
{
public:
	EventTestInstance (Context& context, SynchronizationType type, const ResourceDescription& resourceDesc, const OperationSupport& writeOp, const OperationSupport& readOp, PipelineCacheData& pipelineCacheData)
		: BaseTestInstance		(context, type, resourceDesc, writeOp, readOp, pipelineCacheData)
	{
	}

	tcu::TestStatus iterate (void)
	{
		const DeviceInterface&			vk						= m_context.getDeviceInterface();
		const VkDevice					device					= m_context.getDevice();
		const VkQueue					queue					= m_context.getUniversalQueue();
		const deUint32					queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
		const Unique<VkCommandPool>		cmdPool					(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
		const Unique<VkCommandBuffer>	cmdBuffer				(makeCommandBuffer(vk, device, *cmdPool));
		const Unique<VkEvent>			event					(createEvent(vk, device));
		const SyncInfo					writeSync				= m_writeOp->getOutSyncInfo();
		const SyncInfo					readSync				= m_readOp->getInSyncInfo();
		SynchronizationWrapperPtr		synchronizationWrapper	= getSynchronizationWrapper(m_type, vk, DE_FALSE);

		beginCommandBuffer(vk, *cmdBuffer);

		m_writeOp->recordCommands(*cmdBuffer);

		if (m_resource->getType() == RESOURCE_TYPE_IMAGE)
		{
			const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
				writeSync.stageMask,							// VkPipelineStageFlags2KHR			srcStageMask
				writeSync.accessMask,							// VkAccessFlags2KHR				srcAccessMask
				readSync.stageMask,								// VkPipelineStageFlags2KHR			dstStageMask
				readSync.accessMask,							// VkAccessFlags2KHR				dstAccessMask
				writeSync.imageLayout,							// VkImageLayout					oldLayout
				readSync.imageLayout,							// VkImageLayout					newLayout
				m_resource->getImage().handle,					// VkImage							image
				m_resource->getImage().subresourceRange			// VkImageSubresourceRange			subresourceRange
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2, DE_TRUE);
			synchronizationWrapper->cmdSetEvent(*cmdBuffer, *event, &dependencyInfo);
			synchronizationWrapper->cmdWaitEvents(*cmdBuffer, 1u, &event.get(), &dependencyInfo);
		}
		else
		{
			const VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 = makeBufferMemoryBarrier2(
				writeSync.stageMask,							// VkPipelineStageFlags2KHR			srcStageMask
				writeSync.accessMask,							// VkAccessFlags2KHR				srcAccessMask
				readSync.stageMask,								// VkPipelineStageFlags2KHR			dstStageMask
				readSync.accessMask,							// VkAccessFlags2KHR				dstAccessMask
				m_resource->getBuffer().handle,					// VkBuffer							buffer
				m_resource->getBuffer().offset,					// VkDeviceSize						offset
				m_resource->getBuffer().size					// VkDeviceSize						size
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, &bufferMemoryBarrier2, DE_NULL, DE_TRUE);
			synchronizationWrapper->cmdSetEvent(*cmdBuffer, *event, &dependencyInfo);
			synchronizationWrapper->cmdWaitEvents(*cmdBuffer, 1u, &event.get(), &dependencyInfo);
		}

		m_readOp->recordCommands(*cmdBuffer);

		endCommandBuffer(vk, *cmdBuffer);
		submitCommandsAndWait(synchronizationWrapper, vk, device, queue, *cmdBuffer);

		{
			const Data	expected = m_writeOp->getData();
			const Data	actual	 = m_readOp->getData();

			if (isIndirectBuffer(m_resource->getType()))
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

		return tcu::TestStatus::pass("OK");
	}
};

class BarrierTestInstance : public BaseTestInstance
{
public:
	BarrierTestInstance	(Context& context, SynchronizationType type, const ResourceDescription& resourceDesc, const OperationSupport& writeOp, const OperationSupport& readOp, PipelineCacheData& pipelineCacheData)
		: BaseTestInstance		(context, type, resourceDesc, writeOp, readOp, pipelineCacheData)
	{
	}

	tcu::TestStatus iterate (void)
	{
		const DeviceInterface&			vk						= m_context.getDeviceInterface();
		const VkDevice					device					= m_context.getDevice();
		const VkQueue					queue					= m_context.getUniversalQueue();
		const deUint32					queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
		const Unique<VkCommandPool>		cmdPool					(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
		const Move<VkCommandBuffer>		cmdBuffer				(makeCommandBuffer(vk, device, *cmdPool));
		const SyncInfo					writeSync				= m_writeOp->getOutSyncInfo();
		const SyncInfo					readSync				= m_readOp->getInSyncInfo();
		SynchronizationWrapperPtr		synchronizationWrapper	= getSynchronizationWrapper(m_type, vk, DE_FALSE);

		beginCommandBuffer(vk, *cmdBuffer);

		m_writeOp->recordCommands(*cmdBuffer);

		if (m_resource->getType() == RESOURCE_TYPE_IMAGE)
		{
			const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
				writeSync.stageMask,							// VkPipelineStageFlags2KHR			srcStageMask
				writeSync.accessMask,							// VkAccessFlags2KHR				srcAccessMask
				readSync.stageMask,								// VkPipelineStageFlags2KHR			dstStageMask
				readSync.accessMask,							// VkAccessFlags2KHR				dstAccessMask
				writeSync.imageLayout,							// VkImageLayout					oldLayout
				readSync.imageLayout,							// VkImageLayout					newLayout
				m_resource->getImage().handle,					// VkImage							image
				m_resource->getImage().subresourceRange			// VkImageSubresourceRange			subresourceRange
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(*cmdBuffer, &dependencyInfo);
		}
		else
		{
			const VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 = makeBufferMemoryBarrier2(
				writeSync.stageMask,							// VkPipelineStageFlags2KHR			srcStageMask
				writeSync.accessMask,							// VkAccessFlags2KHR				srcAccessMask
				readSync.stageMask,								// VkPipelineStageFlags2KHR			dstStageMask
				readSync.accessMask,							// VkAccessFlags2KHR				dstAccessMask
				m_resource->getBuffer().handle,					// VkBuffer							buffer
				m_resource->getBuffer().offset,					// VkDeviceSize						offset
				m_resource->getBuffer().size					// VkDeviceSize						size
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, &bufferMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(*cmdBuffer, &dependencyInfo);
		}

		m_readOp->recordCommands(*cmdBuffer);

		endCommandBuffer(vk, *cmdBuffer);

		submitCommandsAndWait(synchronizationWrapper, vk, device, queue, *cmdBuffer);

		{
			const Data	expected = m_writeOp->getData();
			const Data	actual	 = m_readOp->getData();

			if (isIndirectBuffer(m_resource->getType()))
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

		return tcu::TestStatus::pass("OK");
	}
};

class BinarySemaphoreTestInstance : public BaseTestInstance
{
public:
	BinarySemaphoreTestInstance (Context& context, SynchronizationType type, const ResourceDescription& resourceDesc, const OperationSupport& writeOp, const OperationSupport& readOp, PipelineCacheData& pipelineCacheData)
		: BaseTestInstance	(context, type, resourceDesc, writeOp, readOp, pipelineCacheData)
	{
	}

	tcu::TestStatus	iterate (void)
	{
		enum {WRITE=0, READ, COUNT};
		const DeviceInterface&			vk						= m_context.getDeviceInterface();
		const VkDevice					device					= m_context.getDevice();
		const VkQueue					queue					= m_context.getUniversalQueue();
		const deUint32					queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
		const Unique<VkSemaphore>		semaphore				(createSemaphore (vk, device));
		const Unique<VkCommandPool>		cmdPool					(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
		const Move<VkCommandBuffer>		ptrCmdBuffer[COUNT]		= {makeCommandBuffer(vk, device, *cmdPool), makeCommandBuffer(vk, device, *cmdPool)};
		VkCommandBuffer					cmdBuffers[COUNT]		= {*ptrCmdBuffer[WRITE], *ptrCmdBuffer[READ]};
		SynchronizationWrapperPtr		synchronizationWrapper	= getSynchronizationWrapper(m_type, vk, DE_FALSE, 2u);
		const SyncInfo					writeSync				= m_writeOp->getOutSyncInfo();
		const SyncInfo					readSync				= m_readOp->getInSyncInfo();
		VkSemaphoreSubmitInfoKHR		signalSemaphoreSubmitInfo =
			makeCommonSemaphoreSubmitInfo(*semaphore, 0u, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR);
		VkSemaphoreSubmitInfoKHR		waitSemaphoreSubmitInfo =
			makeCommonSemaphoreSubmitInfo(*semaphore, 0u, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR);
		VkCommandBufferSubmitInfoKHR	commandBufferSubmitInfo[]
		{
			makeCommonCommandBufferSubmitInfo(cmdBuffers[WRITE]),
			makeCommonCommandBufferSubmitInfo(cmdBuffers[READ])
		};

		synchronizationWrapper->addSubmitInfo(
			0u,
			DE_NULL,
			1u,
			&commandBufferSubmitInfo[WRITE],
			1u,
			&signalSemaphoreSubmitInfo
		);
		synchronizationWrapper->addSubmitInfo(
			1u,
			&waitSemaphoreSubmitInfo,
			1u,
			&commandBufferSubmitInfo[READ],
			0u,
			DE_NULL
		);

		beginCommandBuffer(vk, cmdBuffers[WRITE]);

		m_writeOp->recordCommands(cmdBuffers[WRITE]);

		if (m_resource->getType() == RESOURCE_TYPE_IMAGE)
		{
			const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
				writeSync.stageMask,							// VkPipelineStageFlags2KHR			srcStageMask
				writeSync.accessMask,							// VkAccessFlags2KHR				srcAccessMask
				readSync.stageMask,								// VkPipelineStageFlags2KHR			dstStageMask
				readSync.accessMask,							// VkAccessFlags2KHR				dstAccessMask
				writeSync.imageLayout,							// VkImageLayout					oldLayout
				readSync.imageLayout,							// VkImageLayout					newLayout
				m_resource->getImage().handle,					// VkImage							image
				m_resource->getImage().subresourceRange			// VkImageSubresourceRange			subresourceRange
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffers[WRITE], &dependencyInfo);
		}
		else
		{
			const VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 = makeBufferMemoryBarrier2(
				writeSync.stageMask,							// VkPipelineStageFlags2KHR			srcStageMask
				writeSync.accessMask,							// VkAccessFlags2KHR				srcAccessMask
				readSync.stageMask,								// VkPipelineStageFlags2KHR			dstStageMask
				readSync.accessMask,							// VkAccessFlags2KHR				dstAccessMask
				m_resource->getBuffer().handle,					// VkBuffer							buffer
				0,												// VkDeviceSize						offset
				VK_WHOLE_SIZE									// VkDeviceSize						size
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, &bufferMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffers[WRITE], &dependencyInfo);
		}

		endCommandBuffer(vk, cmdBuffers[WRITE]);

		beginCommandBuffer(vk, cmdBuffers[READ]);

		m_readOp->recordCommands(cmdBuffers[READ]);

		endCommandBuffer(vk, cmdBuffers[READ]);

		VK_CHECK(synchronizationWrapper->queueSubmit(queue, DE_NULL));
		VK_CHECK(vk.queueWaitIdle(queue));

		{
			const Data	expected = m_writeOp->getData();
			const Data	actual	 = m_readOp->getData();

			if (isIndirectBuffer(m_resource->getType()))
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

		return tcu::TestStatus::pass("OK");
	}
};

template<typename T>
inline de::SharedPtr<Move<T> > makeVkSharedPtr (Move<T> move)
{
	return de::SharedPtr<Move<T> >(new Move<T>(move));
}

class TimelineSemaphoreTestInstance : public TestInstance
{
public:
	TimelineSemaphoreTestInstance (Context& context, SynchronizationType type, const ResourceDescription& resourceDesc, const de::SharedPtr<OperationSupport>& writeOp, const de::SharedPtr<OperationSupport>& readOp, PipelineCacheData& pipelineCacheData)
		: TestInstance	(context)
		, m_type		(type)
		, m_opContext	(context, type, pipelineCacheData)
	{
		if (!context.getTimelineSemaphoreFeatures().timelineSemaphore)
			TCU_THROW(NotSupportedError, "Timeline semaphore not supported");

		// Create a chain operation copying data from one resource to
		// another, each of the operation will be executing with a
		// dependency on the previous using timeline points.
		m_opSupports.push_back(writeOp);
		for (deUint32 copyOpNdx = 0; copyOpNdx < DE_LENGTH_OF_ARRAY(s_copyOps); copyOpNdx++)
		{
			if (isResourceSupported(s_copyOps[copyOpNdx], resourceDesc))
				m_opSupports.push_back(de::SharedPtr<OperationSupport>(makeOperationSupport(s_copyOps[copyOpNdx], resourceDesc).release()));
		}
		m_opSupports.push_back(readOp);

		for (deUint32 opNdx = 0; opNdx < (m_opSupports.size() - 1); opNdx++)
		{
			deUint32 usage = m_opSupports[opNdx]->getOutResourceUsageFlags() | m_opSupports[opNdx + 1]->getInResourceUsageFlags();

			m_resources.push_back(de::SharedPtr<Resource>(new Resource(m_opContext, resourceDesc, usage)));
		}

		m_ops.push_back(de::SharedPtr<Operation>(m_opSupports[0]->build(m_opContext, *m_resources[0]).release()));
		for (deUint32 opNdx = 1; opNdx < (m_opSupports.size() - 1); opNdx++)
			m_ops.push_back(de::SharedPtr<Operation>(m_opSupports[opNdx]->build(m_opContext, *m_resources[opNdx - 1], *m_resources[opNdx]).release()));
		m_ops.push_back(de::SharedPtr<Operation>(m_opSupports[m_opSupports.size() - 1]->build(m_opContext, *m_resources.back()).release()));
	}

	tcu::TestStatus	iterate (void)
	{
		const DeviceInterface&									vk							= m_context.getDeviceInterface();
		const VkDevice											device						= m_context.getDevice();
		const VkQueue											queue						= m_context.getUniversalQueue();
		const deUint32											queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();
		de::Random												rng							(1234);
		const Unique<VkSemaphore>								semaphore					(createSemaphoreType(vk, device, VK_SEMAPHORE_TYPE_TIMELINE_KHR));
		const Unique<VkCommandPool>								cmdPool						(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
		std::vector<de::SharedPtr<Move<VkCommandBuffer> > >		ptrCmdBuffers;
		std::vector<VkCommandBufferSubmitInfoKHR>				cmdBuffersInfo				(m_ops.size(), makeCommonCommandBufferSubmitInfo(0u));
		std::vector<VkSemaphoreSubmitInfoKHR>					waitSemaphoreSubmitInfos	(m_ops.size(), makeCommonSemaphoreSubmitInfo(*semaphore, 0u, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR));
		std::vector<VkSemaphoreSubmitInfoKHR>					signalSemaphoreSubmitInfos	(m_ops.size(), makeCommonSemaphoreSubmitInfo(*semaphore, 0u, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR));
		SynchronizationWrapperPtr								synchronizationWrapper		= getSynchronizationWrapper(m_type, vk, DE_TRUE, static_cast<deUint32>(m_ops.size()));
		deUint64												increment					= 0u;

		for (deUint32 opNdx = 0; opNdx < m_ops.size(); opNdx++)
		{
			ptrCmdBuffers.push_back(makeVkSharedPtr(makeCommandBuffer(vk, device, *cmdPool)));
			cmdBuffersInfo[opNdx].commandBuffer = **(ptrCmdBuffers.back());
		}

		for (deUint32 opNdx = 0; opNdx < m_ops.size(); opNdx++)
		{
			increment								+= (1 + rng.getUint8());
			signalSemaphoreSubmitInfos[opNdx].value = increment;
			waitSemaphoreSubmitInfos[opNdx].value	= increment;

			synchronizationWrapper->addSubmitInfo(
				opNdx == 0 ? 0u : 1u,
				opNdx == 0 ? DE_NULL : &waitSemaphoreSubmitInfos[opNdx-1],
				1u,
				&cmdBuffersInfo[opNdx],
				1u,
				&signalSemaphoreSubmitInfos[opNdx],
				opNdx == 0 ? DE_FALSE : DE_TRUE,
				DE_TRUE
			);

			VkCommandBuffer cmdBuffer = cmdBuffersInfo[opNdx].commandBuffer;
			beginCommandBuffer(vk, cmdBuffer);

			if (opNdx > 0)
			{
				const SyncInfo	lastSync	= m_ops[opNdx - 1]->getOutSyncInfo();
				const SyncInfo	currentSync	= m_ops[opNdx]->getInSyncInfo();
				const Resource&	resource	= *m_resources[opNdx - 1].get();

				if (resource.getType() == RESOURCE_TYPE_IMAGE)
				{
					DE_ASSERT(lastSync.imageLayout != VK_IMAGE_LAYOUT_UNDEFINED);
					DE_ASSERT(currentSync.imageLayout != VK_IMAGE_LAYOUT_UNDEFINED);

					const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
						lastSync.stageMask,									// VkPipelineStageFlags2KHR			srcStageMask
						lastSync.accessMask,								// VkAccessFlags2KHR				srcAccessMask
						currentSync.stageMask,								// VkPipelineStageFlags2KHR			dstStageMask
						currentSync.accessMask,								// VkAccessFlags2KHR				dstAccessMask
						lastSync.imageLayout,								// VkImageLayout					oldLayout
						currentSync.imageLayout,							// VkImageLayout					newLayout
						resource.getImage().handle,							// VkImage							image
						resource.getImage().subresourceRange				// VkImageSubresourceRange			subresourceRange
					);
					VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
					synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
				}
				else
				{
					const VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 = makeBufferMemoryBarrier2(
						lastSync.stageMask,									// VkPipelineStageFlags2KHR			srcStageMask
						lastSync.accessMask,								// VkAccessFlags2KHR				srcAccessMask
						currentSync.stageMask,								// VkPipelineStageFlags2KHR			dstStageMask
						currentSync.accessMask,								// VkAccessFlags2KHR				dstAccessMask
						resource.getBuffer().handle,						// VkBuffer							buffer
						0,													// VkDeviceSize						offset
						VK_WHOLE_SIZE										// VkDeviceSize						size
					);
					VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, &bufferMemoryBarrier2);
					synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
				}
			}

			m_ops[opNdx]->recordCommands(cmdBuffer);

			endCommandBuffer(vk, cmdBuffer);
		}

		VK_CHECK(synchronizationWrapper->queueSubmit(queue, DE_NULL));
		VK_CHECK(vk.queueWaitIdle(queue));

		{
			const Data	expected = m_ops.front()->getData();
			const Data	actual	 = m_ops.back()->getData();

			if (isIndirectBuffer(m_resources[0]->getType()))
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

		return tcu::TestStatus::pass("OK");
	}

protected:
	SynchronizationType								m_type;
	OperationContext								m_opContext;
	std::vector<de::SharedPtr<OperationSupport> >	m_opSupports;
	std::vector<de::SharedPtr<Operation> >			m_ops;
	std::vector<de::SharedPtr<Resource> >			m_resources;
};

class FenceTestInstance : public BaseTestInstance
{
public:
	FenceTestInstance (Context& context, SynchronizationType type, const ResourceDescription& resourceDesc, const OperationSupport& writeOp, const OperationSupport& readOp, PipelineCacheData& pipelineCacheData)
		: BaseTestInstance	(context, type, resourceDesc, writeOp, readOp, pipelineCacheData)
	{
	}

	tcu::TestStatus	iterate (void)
	{
		enum {WRITE=0, READ, COUNT};
		const DeviceInterface&			vk								= m_context.getDeviceInterface();
		const VkDevice					device							= m_context.getDevice();
		const VkQueue					queue							= m_context.getUniversalQueue();
		const deUint32					queueFamilyIndex				= m_context.getUniversalQueueFamilyIndex();
		const Unique<VkCommandPool>		cmdPool							(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
		const Move<VkCommandBuffer>		ptrCmdBuffer[COUNT]				= {makeCommandBuffer(vk, device, *cmdPool), makeCommandBuffer(vk, device, *cmdPool)};
		VkCommandBuffer					cmdBuffers[COUNT]				= {*ptrCmdBuffer[WRITE], *ptrCmdBuffer[READ]};
		const SyncInfo					writeSync						= m_writeOp->getOutSyncInfo();
		const SyncInfo					readSync						= m_readOp->getInSyncInfo();
		SynchronizationWrapperPtr		synchronizationWrapper[COUNT]
		{
			getSynchronizationWrapper(m_type, vk, DE_FALSE),
			getSynchronizationWrapper(m_type, vk, DE_FALSE)
		};

		beginCommandBuffer(vk, cmdBuffers[WRITE]);

		m_writeOp->recordCommands(cmdBuffers[WRITE]);

		if (m_resource->getType() == RESOURCE_TYPE_IMAGE)
		{
			const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
				writeSync.stageMask,							// VkPipelineStageFlags2KHR			srcStageMask
				writeSync.accessMask,							// VkAccessFlags2KHR				srcAccessMask
				readSync.stageMask,								// VkPipelineStageFlags2KHR			dstStageMask
				readSync.accessMask,							// VkAccessFlags2KHR				dstAccessMask
				writeSync.imageLayout,							// VkImageLayout					oldLayout
				readSync.imageLayout,							// VkImageLayout					newLayout
				m_resource->getImage().handle,					// VkImage							image
				m_resource->getImage().subresourceRange			// VkImageSubresourceRange			subresourceRange
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
			synchronizationWrapper[WRITE]->cmdPipelineBarrier(cmdBuffers[WRITE], &dependencyInfo);
		}

		endCommandBuffer(vk, cmdBuffers[WRITE]);

		submitCommandsAndWait(synchronizationWrapper[WRITE], vk, device, queue, cmdBuffers[WRITE]);

		beginCommandBuffer(vk, cmdBuffers[READ]);

		m_readOp->recordCommands(cmdBuffers[READ]);

		endCommandBuffer(vk, cmdBuffers[READ]);

		submitCommandsAndWait(synchronizationWrapper[READ], vk, device, queue, cmdBuffers[READ]);

		{
			const Data	expected = m_writeOp->getData();
			const Data	actual	 = m_readOp->getData();

			if (isIndirectBuffer(m_resource->getType()))
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

		return tcu::TestStatus::pass("OK");
	}
};

class SyncTestCase : public TestCase
{
public:
	SyncTestCase	(tcu::TestContext&			testCtx,
					 const std::string&			name,
					 const std::string&			description,
					 SynchronizationType		type,
					 const SyncPrimitive		syncPrimitive,
					 const ResourceDescription	resourceDesc,
					 const OperationName		writeOp,
					 const OperationName		readOp,
					 PipelineCacheData&			pipelineCacheData)
		: TestCase				(testCtx, name, description)
		, m_type				(type)
		, m_resourceDesc		(resourceDesc)
		, m_writeOp				(makeOperationSupport(writeOp, resourceDesc).release())
		, m_readOp				(makeOperationSupport(readOp, resourceDesc).release())
		, m_syncPrimitive		(syncPrimitive)
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

		if (SYNC_PRIMITIVE_EVENT == m_syncPrimitive &&
			context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
			!context.getPortabilitySubsetFeatures().events)
		{
			TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Events are not supported by this implementation");
		}
	}

	TestInstance* createInstance (Context& context) const
	{
		switch (m_syncPrimitive)
		{
			case SYNC_PRIMITIVE_FENCE:
				return new FenceTestInstance(context, m_type, m_resourceDesc, *m_writeOp, *m_readOp, m_pipelineCacheData);
			case SYNC_PRIMITIVE_BINARY_SEMAPHORE:
				return new BinarySemaphoreTestInstance(context, m_type, m_resourceDesc, *m_writeOp, *m_readOp, m_pipelineCacheData);
			case SYNC_PRIMITIVE_TIMELINE_SEMAPHORE:
				return new TimelineSemaphoreTestInstance(context, m_type, m_resourceDesc, m_writeOp, m_readOp, m_pipelineCacheData);
			case SYNC_PRIMITIVE_BARRIER:
				return new BarrierTestInstance(context, m_type, m_resourceDesc, *m_writeOp, *m_readOp, m_pipelineCacheData);
			case SYNC_PRIMITIVE_EVENT:
				return new EventTestInstance(context, m_type, m_resourceDesc, *m_writeOp, *m_readOp, m_pipelineCacheData);
		}

		DE_ASSERT(0);
		return DE_NULL;
	}

private:
	SynchronizationType						m_type;
	const ResourceDescription				m_resourceDesc;
	const de::SharedPtr<OperationSupport>	m_writeOp;
	const de::SharedPtr<OperationSupport>	m_readOp;
	const SyncPrimitive						m_syncPrimitive;
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
		{ "fence",				SYNC_PRIMITIVE_FENCE,				0, },
		{ "binary_semaphore",	SYNC_PRIMITIVE_BINARY_SEMAPHORE,	0, },
		{ "timeline_semaphore",	SYNC_PRIMITIVE_TIMELINE_SEMAPHORE,	0, },
		{ "barrier",			SYNC_PRIMITIVE_BARRIER,				1, },
		{ "event",				SYNC_PRIMITIVE_EVENT,				1, },
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

			for (int resourceNdx = 0; resourceNdx < DE_LENGTH_OF_ARRAY(s_resources); ++resourceNdx)
			{
				const ResourceDescription&	resource	= s_resources[resourceNdx];
				std::string					name		= getResourceName(resource);

				if (isResourceSupported(writeOp, resource) && isResourceSupported(readOp, resource))
				{
					opGroup->addChild(new SyncTestCase(testCtx, name, "", data.type, groups[groupNdx].syncPrimitive, resource, writeOp, readOp, *data.pipelineCacheData));
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

tcu::TestCaseGroup* createSynchronizedOperationSingleQueueTests (tcu::TestContext& testCtx, SynchronizationType type, PipelineCacheData& pipelineCacheData)
{
	TestData data
	{
		type,
		&pipelineCacheData
	};

	return createTestGroup(testCtx, "single_queue", "Synchronization of a memory-modifying operation", createTests, data);
}

} // synchronization
} // vkt
