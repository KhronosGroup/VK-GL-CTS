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
 * \brief Synchronization internally synchronized objects tests
 *//*--------------------------------------------------------------------*/

#include "vktSynchronizationInternallySynchronizedObjectsTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktSynchronizationUtil.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "vkRef.hpp"
#include "tcuDefs.hpp"
#include "vkTypeUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkPlatform.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkSafetyCriticalUtil.hpp"

#include "tcuResultCollector.hpp"
#include "tcuCommandLine.hpp"

#include "deThread.hpp"
#include "deMutex.hpp"
#include "deSharedPtr.hpp"
#include "deSpinBarrier.hpp"


#include <limits>
#include <iterator>

namespace vkt
{
namespace synchronization
{
namespace
{
using namespace vk;

using std::vector;
using std::string;
using std::map;
using std::exception;
using std::ostringstream;

using tcu::TestStatus;
using tcu::TestContext;
using tcu::ResultCollector;
using tcu::TestException;

using de::UniquePtr;
using de::MovePtr;
using de::SharedPtr;
using de::Mutex;
using de::Thread;
using de::clamp;

template<typename T>
inline SharedPtr<Move<T> > makeVkSharedPtr(Move<T> move)
{
	return SharedPtr<Move<T> >(new Move<T>(move));
}

#ifndef CTS_USES_VULKANSC
enum
{
	EXECUTION_PER_THREAD	= 100,
	BUFFER_ELEMENT_COUNT	= 16,
	BUFFER_SIZE				= BUFFER_ELEMENT_COUNT*4
};
#else
enum
{
	EXECUTION_PER_THREAD	= 10,
	BUFFER_ELEMENT_COUNT	= 16,
	BUFFER_SIZE				= BUFFER_ELEMENT_COUNT*4
};
#endif // CTS_USES_VULKANSC

class MultiQueues
{
	typedef struct QueueType
	{
		vector<VkQueue>							queues;
		vector<bool>							available;
		vector<SharedPtr<Move<VkCommandPool>>>	commandPools;
	} Queues;

public:
	inline void		addQueueFamilyIndex		(const deUint32& queueFamilyIndex, const deUint32& count)
	{
		Queues temp;
		vector<bool>::iterator it;
		it = temp.available.begin();
		temp.available.insert(it, count, false);

		temp.queues.resize(count);

		m_queues[queueFamilyIndex] = temp;
	}

	deUint32 getQueueFamilyIndex (const int index) const
	{
		map<deUint32,Queues>::const_iterator it = begin(m_queues);
		std::advance(it, index);
		return it->first;
	}

	inline size_t	countQueueFamilyIndex	(void)
	{
		return m_queues.size();
	}

	Queues &		getQueues				(int index)
	{
		map<deUint32,Queues>::iterator it = m_queues.begin();
		advance (it, index);
		return it->second;
	}

	bool			getFreeQueue			(const DeviceInterface& vk, const VkDevice device, deUint32& returnQueueFamilyIndex, VkQueue& returnQueues, Move<VkCommandBuffer>& commandBuffer, int& returnQueueIndex)
	{
		for (int queueFamilyIndexNdx = 0 ; queueFamilyIndexNdx < static_cast<int>(m_queues.size()); ++queueFamilyIndexNdx)
		{
			Queues& queue = m_queues[getQueueFamilyIndex(queueFamilyIndexNdx)];
			for (int queueNdx = 0; queueNdx < static_cast<int>(queue.queues.size()); ++queueNdx)
			{
				m_mutex.lock();
				if (queue.available[queueNdx])
				{
					queue.available[queueNdx]	= false;
					returnQueueFamilyIndex		= getQueueFamilyIndex(queueFamilyIndexNdx);
					returnQueues				= queue.queues[queueNdx];
					commandBuffer				= makeCommandBuffer(vk, device, queue.commandPools[queueNdx]->get());
					returnQueueIndex			= queueNdx;
					m_mutex.unlock();
					return true;
				}
				m_mutex.unlock();
			}
		}
		return false;
	}

	void			releaseQueue			(const deUint32& queueFamilyIndex, const int& queueIndex, Move<VkCommandBuffer>& commandBuffer)
	{
		m_mutex.lock();
		commandBuffer = Move<VkCommandBuffer>();
		m_queues[queueFamilyIndex].available[queueIndex] = true;
		m_mutex.unlock();
	}

	inline void		setDevice				(Move<VkDevice> device, const Context& context)
	{
		m_logicalDevice = device;
#ifndef CTS_USES_VULKANSC
		m_deviceDriver = de::MovePtr<DeviceDriver>		(new DeviceDriver(context.getPlatformInterface(), context.getInstance(), *m_logicalDevice, context.getUsedApiVersion()));
#else
		m_deviceDriver = de::MovePtr<DeviceDriverSC, DeinitDeviceDeleter>(new DeviceDriverSC(context.getPlatformInterface(), context.getInstance(), *m_logicalDevice, context.getTestContext().getCommandLine(), context.getResourceInterface(), context.getDeviceVulkanSC10Properties(), context.getDeviceProperties(), context.getUsedApiVersion()), vk::DeinitDeviceDeleter(context.getResourceInterface().get(), *m_logicalDevice));
#endif // CTS_USES_VULKANSC
	}

	inline VkDevice	getDevice				(void)
	{
		return *m_logicalDevice;
	}

	inline DeviceInterface&	getDeviceInterface(void)
	{
		return *m_deviceDriver;
	}

	MovePtr<Allocator>				m_allocator;
protected:
	Move<VkDevice>					m_logicalDevice;
#ifndef CTS_USES_VULKANSC
	de::MovePtr<vk::DeviceDriver>	m_deviceDriver;
#else
	de::MovePtr<DeviceDriverSC, DeinitDeviceDeleter>	m_deviceDriver;
#endif // CTS_USES_VULKANSC
	map<deUint32,Queues>			m_queues;
	Mutex							m_mutex;
};

MovePtr<Allocator> createAllocator (const Context& context, const VkDevice& device)
{
	const DeviceInterface&					deviceInterface			= context.getDeviceInterface();
	const InstanceInterface&				instance				= context.getInstanceInterface();
	const VkPhysicalDevice					physicalDevice			= context.getPhysicalDevice();
	const VkPhysicalDeviceMemoryProperties	deviceMemoryProperties	= getPhysicalDeviceMemoryProperties(instance, physicalDevice);

	// Create memory allocator for device
	return MovePtr<Allocator> (new SimpleAllocator(deviceInterface, device, deviceMemoryProperties));
}

bool checkQueueFlags (const VkQueueFlags& availableFlag, const VkQueueFlags& neededFlag)
{
	if (VK_QUEUE_TRANSFER_BIT == neededFlag)
	{
		if ( (availableFlag & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT ||
			 (availableFlag & VK_QUEUE_COMPUTE_BIT)  == VK_QUEUE_COMPUTE_BIT  ||
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

MovePtr<MultiQueues> createQueues (Context& context, const VkQueueFlags& queueFlag, const VkInstance& instance, const InstanceInterface& vki)
{
	const VkPhysicalDevice					physicalDevice			= chooseDevice(vki, instance, context.getTestContext().getCommandLine());
	MovePtr<MultiQueues>					moveQueues				(new MultiQueues());
	MultiQueues&							queues					= *moveQueues;
	VkDeviceCreateInfo						deviceInfo;
	VkPhysicalDeviceFeatures				deviceFeatures;
	vector<VkQueueFamilyProperties>			queueFamilyProperties;
	vector<float>							queuePriorities;
	vector<VkDeviceQueueCreateInfo>			queueInfos;

	queueFamilyProperties = getPhysicalDeviceQueueFamilyProperties(vki, physicalDevice);

	for (deUint32 queuePropertiesNdx = 0; queuePropertiesNdx < queueFamilyProperties.size(); ++queuePropertiesNdx)
	{
		if (checkQueueFlags(queueFamilyProperties[queuePropertiesNdx].queueFlags, queueFlag))
		{
			queues.addQueueFamilyIndex(queuePropertiesNdx, queueFamilyProperties[queuePropertiesNdx].queueCount);
		}
	}

	if (queues.countQueueFamilyIndex() == 0)
	{
		TCU_THROW(NotSupportedError, "Queue not found");
	}

	{
		vector<float>::iterator it				= queuePriorities.begin();
		unsigned int			maxQueueCount	= 0;
		for (int queueFamilyIndexNdx = 0; queueFamilyIndexNdx < static_cast<int>(queues.countQueueFamilyIndex()); ++queueFamilyIndexNdx)
		{
			if (queues.getQueues(queueFamilyIndexNdx).queues.size() > maxQueueCount)
				maxQueueCount = static_cast<unsigned int>(queues.getQueues(queueFamilyIndexNdx).queues.size());
		}
		queuePriorities.insert(it, maxQueueCount, 1.0);
	}

	for (int queueFamilyIndexNdx = 0; queueFamilyIndexNdx < static_cast<int>(queues.countQueueFamilyIndex()); ++queueFamilyIndexNdx)
	{
		VkDeviceQueueCreateInfo	queueInfo;
		const deUint32			queueCount	= static_cast<deUint32>(queues.getQueues(queueFamilyIndexNdx).queues.size());

		deMemset(&queueInfo, 0, sizeof(queueInfo));

		queueInfo.sType				= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.pNext				= DE_NULL;
		queueInfo.flags				= (VkDeviceQueueCreateFlags)0u;
		queueInfo.queueFamilyIndex	= queues.getQueueFamilyIndex(queueFamilyIndexNdx);
		queueInfo.queueCount		= queueCount;
		queueInfo.pQueuePriorities	= &queuePriorities[0];

		queueInfos.push_back(queueInfo);
	}

	deMemset(&deviceInfo, 0, sizeof(deviceInfo));
	vki.getPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

	void* pNext												= DE_NULL;
#ifdef CTS_USES_VULKANSC
	VkDeviceObjectReservationCreateInfo memReservationInfo	= context.getTestContext().getCommandLine().isSubProcess() ? context.getResourceInterface()->getStatMax() : resetDeviceObjectReservationCreateInfo();
	memReservationInfo.pNext								= pNext;
	pNext													= &memReservationInfo;

	VkPhysicalDeviceVulkanSC10Features sc10Features			= createDefaultSC10Features();
	sc10Features.pNext										= pNext;
	pNext													= &sc10Features;

	VkPipelineCacheCreateInfo			pcCI;
	std::vector<VkPipelinePoolSize>		poolSizes;
	if (context.getTestContext().getCommandLine().isSubProcess())
	{
		if (context.getResourceInterface()->getCacheDataSize() > 0)
		{
			pcCI =
			{
				VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,		// VkStructureType				sType;
				DE_NULL,											// const void*					pNext;
				VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
					VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT,	// VkPipelineCacheCreateFlags	flags;
				context.getResourceInterface()->getCacheDataSize(),	// deUintptr					initialDataSize;
				context.getResourceInterface()->getCacheData()		// const void*					pInitialData;
			};
			memReservationInfo.pipelineCacheCreateInfoCount		= 1;
			memReservationInfo.pPipelineCacheCreateInfos		= &pcCI;
		}

		poolSizes							= context.getResourceInterface()->getPipelinePoolSizes();
		if (!poolSizes.empty())
		{
			memReservationInfo.pipelinePoolSizeCount		= deUint32(poolSizes.size());
			memReservationInfo.pPipelinePoolSizes			= poolSizes.data();
		}
	}
#endif // CTS_USES_VULKANSC

	deviceInfo.sType					= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.pNext					= pNext;
	deviceInfo.enabledExtensionCount	= 0u;
	deviceInfo.ppEnabledExtensionNames	= DE_NULL;
	deviceInfo.enabledLayerCount		= 0u;
	deviceInfo.ppEnabledLayerNames		= DE_NULL;
	deviceInfo.pEnabledFeatures			= &deviceFeatures;
	deviceInfo.queueCreateInfoCount		= static_cast<deUint32>(queues.countQueueFamilyIndex());
	deviceInfo.pQueueCreateInfos		= &queueInfos[0];

	queues.setDevice(createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), context.getPlatformInterface(), instance, vki, physicalDevice, &deviceInfo), context);
	vk::DeviceInterface& vk = queues.getDeviceInterface();

	for (deUint32 queueFamilyIndex = 0; queueFamilyIndex < queues.countQueueFamilyIndex(); ++queueFamilyIndex)
	{
		for (deUint32 queueReqNdx = 0; queueReqNdx < queues.getQueues(queueFamilyIndex).queues.size(); ++queueReqNdx)
		{
			vk.getDeviceQueue(queues.getDevice(), queues.getQueueFamilyIndex(queueFamilyIndex), queueReqNdx, &queues.getQueues(queueFamilyIndex).queues[queueReqNdx]);
			queues.getQueues(queueFamilyIndex).available[queueReqNdx]=true;
			queues.getQueues(queueFamilyIndex).commandPools.push_back(makeVkSharedPtr(createCommandPool(vk, queues.getDevice(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queues.getQueueFamilyIndex(queueFamilyIndex))));
		}
	}

	queues.m_allocator = createAllocator(context, queues.getDevice());
	return moveQueues;
}

TestStatus executeComputePipeline (const Context& context, const VkPipeline& pipeline, const VkPipelineLayout& pipelineLayout,
									const VkDescriptorSetLayout& descriptorSetLayout, MultiQueues& queues, const deUint32& shadersExecutions)
{
	DE_UNREF(context);
	const DeviceInterface&			vk					= queues.getDeviceInterface();
	const VkDevice					device				= queues.getDevice();
	deUint32						queueFamilyIndex;
	VkQueue							queue;
	int								queueIndex;
	Move<VkCommandBuffer>			cmdBuffer;
	while(!queues.getFreeQueue(vk, device, queueFamilyIndex, queue, cmdBuffer, queueIndex)){}

	{
		const Unique<VkDescriptorPool>	descriptorPool		(DescriptorPoolBuilder()
																.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
																.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
		Buffer							resultBuffer		(vk, device, *queues.m_allocator, makeBufferCreateInfo(BUFFER_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);
		const VkBufferMemoryBarrier		bufferBarrier		= makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *resultBuffer, 0ull, BUFFER_SIZE);

		{
			const Allocation& alloc = resultBuffer.getAllocation();
			deMemset(alloc.getHostPtr(), 0, BUFFER_SIZE);
			flushAlloc(vk, device, alloc);
		}

		// Start recording commands
		beginCommandBuffer(vk, *cmdBuffer);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

		// Create descriptor set
		const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, descriptorSetLayout));

		const VkDescriptorBufferInfo resultDescriptorInfo = makeDescriptorBufferInfo(*resultBuffer, 0ull, BUFFER_SIZE);

		DescriptorSetUpdateBuilder()
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultDescriptorInfo)
			.update(vk, device);

		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

		// Dispatch indirect compute command
		vk.cmdDispatch(*cmdBuffer, shadersExecutions, 1u, 1u);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0,
								 0, (const VkMemoryBarrier*)DE_NULL,
								 1, &bufferBarrier,
								 0, (const VkImageMemoryBarrier*)DE_NULL);

		// End recording commands
		endCommandBuffer(vk, *cmdBuffer);

		// Wait for command buffer execution finish
		submitCommandsAndWait(vk, device, queue, *cmdBuffer);
		queues.releaseQueue(queueFamilyIndex, queueIndex, cmdBuffer);

		{
			const Allocation& resultAlloc = resultBuffer.getAllocation();
			invalidateAlloc(vk, device, resultAlloc);

			const deInt32*	ptr = reinterpret_cast<deInt32*>(resultAlloc.getHostPtr());
			for (deInt32 ndx = 0; ndx < BUFFER_ELEMENT_COUNT; ++ndx)
			{
				if (ptr[ndx] != ndx)
				{
					return TestStatus::fail("The data don't match");
				}
			}
		}
		return TestStatus::pass("Passed");
	}
}


TestStatus executeGraphicPipeline (const Context& context, const VkPipeline& pipeline, const VkPipelineLayout& pipelineLayout,
									const VkDescriptorSetLayout& descriptorSetLayout, MultiQueues& queues, const VkRenderPass& renderPass, const deUint32 shadersExecutions)
{
	DE_UNREF(context);
	const DeviceInterface&			vk					= queues.getDeviceInterface();
	const VkDevice					device				= queues.getDevice();
	deUint32						queueFamilyIndex;
	VkQueue							queue;
	int								queueIndex;
	Move<VkCommandBuffer>			cmdBuffer;
	while (!queues.getFreeQueue(vk, device, queueFamilyIndex, queue, cmdBuffer, queueIndex)) {}

	{
		const Unique<VkDescriptorPool>	descriptorPool				(DescriptorPoolBuilder()
																		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
																		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
		Move<VkDescriptorSet>			descriptorSet				= makeDescriptorSet(vk, device, *descriptorPool, descriptorSetLayout);
		Buffer							resultBuffer				(vk, device, *queues.m_allocator, makeBufferCreateInfo(BUFFER_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);
		const VkBufferMemoryBarrier		bufferBarrier				= makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *resultBuffer, 0ull, BUFFER_SIZE);
		const VkFormat					colorFormat					= VK_FORMAT_R8G8B8A8_UNORM;
		const VkExtent3D				colorImageExtent			= makeExtent3D(1u, 1u, 1u);
		const VkImageSubresourceRange	colorImageSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
		de::MovePtr<Image>				colorAttachmentImage		= de::MovePtr<Image>(new Image(vk, device, *queues.m_allocator,
																		makeImageCreateInfo(VK_IMAGE_TYPE_2D, colorImageExtent, colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL),
																		MemoryRequirement::Any));
		Move<VkImageView>				colorAttachmentView			= makeImageView(vk, device, **colorAttachmentImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorImageSubresourceRange);
		Move<VkFramebuffer>				framebuffer					= makeFramebuffer(vk, device, renderPass, *colorAttachmentView, colorImageExtent.width, colorImageExtent.height);
		const VkDescriptorBufferInfo	outputBufferDescriptorInfo	= makeDescriptorBufferInfo(*resultBuffer, 0ull, BUFFER_SIZE);

		DescriptorSetUpdateBuilder()
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputBufferDescriptorInfo)
			.update		(vk, device);

		{
			const Allocation& alloc = resultBuffer.getAllocation();
			deMemset(alloc.getHostPtr(), 0, BUFFER_SIZE);
			flushAlloc(vk, device, alloc);
		}

		// Start recording commands
		beginCommandBuffer(vk, *cmdBuffer);
		// Change color attachment image layout
		{
			const VkImageMemoryBarrier colorAttachmentLayoutBarrier = makeImageMemoryBarrier(
				(VkAccessFlags)0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				**colorAttachmentImage, colorImageSubresourceRange);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (VkDependencyFlags)0,
				0u, DE_NULL, 0u, DE_NULL, 1u, &colorAttachmentLayoutBarrier);
		}

		{
			const VkRect2D	renderArea	= makeRect2D(1u, 1u);
			const tcu::Vec4	clearColor	= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
			beginRenderPass(vk, *cmdBuffer, renderPass, *framebuffer, renderArea, clearColor);
		}

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

		vk.cmdDraw(*cmdBuffer, shadersExecutions, 1u, 0u, 0u);
		endRenderPass(vk, *cmdBuffer);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0,
						0, (const VkMemoryBarrier*)DE_NULL,
						1, &bufferBarrier,
						0, (const VkImageMemoryBarrier*)DE_NULL);

		// End recording commands
		endCommandBuffer(vk, *cmdBuffer);

		// Wait for command buffer execution finish
		submitCommandsAndWait(vk, device, queue, *cmdBuffer);
		queues.releaseQueue(queueFamilyIndex, queueIndex, cmdBuffer);

		{
			const Allocation& resultAlloc = resultBuffer.getAllocation();
			invalidateAlloc(vk, device, resultAlloc);

			const deInt32*	ptr = reinterpret_cast<deInt32*>(resultAlloc.getHostPtr());
			for (deInt32 ndx = 0; ndx < BUFFER_ELEMENT_COUNT; ++ndx)
			{
				if (ptr[ndx] != ndx)
				{
					return TestStatus::fail("The data don't match");
				}
			}
		}
		return TestStatus::pass("Passed");
	}
}

class ThreadGroupThread : private Thread
{
public:
							ThreadGroupThread	(const Context& context, VkPipelineCache pipelineCache, const VkPipelineLayout& pipelineLayout,
												const VkDescriptorSetLayout& descriptorSetLayout, MultiQueues& queues, const vector<deUint32>& shadersExecutions)
								: m_context				(context)
								, m_pipelineCache		(pipelineCache)
								, m_pipelineLayout		(pipelineLayout)
								, m_descriptorSetLayout	(descriptorSetLayout)
								, m_queues				(queues)
								, m_shadersExecutions	(shadersExecutions)
								, m_barrier				(DE_NULL)
	{
	}

	virtual					~ThreadGroupThread	(void)
	{
	}

	ResultCollector&		getResultCollector	(void)
	{
		return m_resultCollector;
	}

	void					start				(de::SpinBarrier* groupBarrier);
	using Thread::join;

protected:
	virtual TestStatus		runThread		() = 0;
	const Context&							m_context;
	VkPipelineCache							m_pipelineCache;
	const VkPipelineLayout&					m_pipelineLayout;
	const VkDescriptorSetLayout&			m_descriptorSetLayout;
	MultiQueues&							m_queues;
	const vector<deUint32>&					m_shadersExecutions;

	void					barrier				(void);

private:
							ThreadGroupThread	(const ThreadGroupThread&);
	ThreadGroupThread&		operator=			(const ThreadGroupThread&);

	void					run					(void)
	{
		try
		{
			TestStatus result = runThread();
			m_resultCollector.addResult(result.getCode(), result.getDescription());
		}
		catch (const TestException& e)
		{
			m_resultCollector.addResult(e.getTestResult(), e.getMessage());
		}
		catch (const exception& e)
		{
			m_resultCollector.addResult(QP_TEST_RESULT_FAIL, e.what());
		}
		catch (...)
		{
			m_resultCollector.addResult(QP_TEST_RESULT_FAIL, "Exception");
		}

		m_barrier->removeThread(de::SpinBarrier::WAIT_MODE_AUTO);
	}

	ResultCollector							m_resultCollector;
	de::SpinBarrier*						m_barrier;
};

void ThreadGroupThread::start (de::SpinBarrier* groupBarrier)
{
	m_barrier = groupBarrier;
	de::Thread::start();
}

inline void ThreadGroupThread::barrier (void)
{
	m_barrier->sync(de::SpinBarrier::WAIT_MODE_AUTO);
}

class ThreadGroup
{
	typedef vector<SharedPtr<ThreadGroupThread> >	ThreadVector;
public:
							ThreadGroup			(void)
								: m_barrier(1)
	{
	}
							~ThreadGroup		(void)
	{
	}

	void					add					(MovePtr<ThreadGroupThread> thread)
	{
		m_threads.push_back(SharedPtr<ThreadGroupThread>(thread.release()));
	}

	TestStatus				run					(void)
	{
		ResultCollector	resultCollector;

		m_barrier.reset((int)m_threads.size());

		for (ThreadVector::iterator threadIter = m_threads.begin(); threadIter != m_threads.end(); ++threadIter)
			(*threadIter)->start(&m_barrier);

		for (ThreadVector::iterator threadIter = m_threads.begin(); threadIter != m_threads.end(); ++threadIter)
		{
			ResultCollector&	threadResult	= (*threadIter)->getResultCollector();
			(*threadIter)->join();
			resultCollector.addResult(threadResult.getResult(), threadResult.getMessage());
		}

		return TestStatus(resultCollector.getResult(), resultCollector.getMessage());
	}

private:
	ThreadVector							m_threads;
	de::SpinBarrier							m_barrier;
};


class CreateComputeThread : public ThreadGroupThread
{
public:
			CreateComputeThread	(const Context& context, VkPipelineCache pipelineCache, vector<VkComputePipelineCreateInfo>& pipelineInfo,
								const VkPipelineLayout& pipelineLayout, const VkDescriptorSetLayout& descriptorSetLayout,
								MultiQueues& queues, const vector<deUint32>& shadersExecutions)
				: ThreadGroupThread		(context, pipelineCache, pipelineLayout, descriptorSetLayout, queues, shadersExecutions)
				, m_pipelineInfo		(pipelineInfo)
	{
	}

	TestStatus	runThread		(void)
	{
		ResultCollector		resultCollector;
		for (int executionNdx = 0; executionNdx < EXECUTION_PER_THREAD; ++executionNdx)
		{
			const int shaderNdx					= executionNdx % (int)m_pipelineInfo.size();
			const DeviceInterface&	vk			= m_context.getDeviceInterface();
			const VkDevice			device		= m_queues.getDevice();
			Move<VkPipeline>		pipeline	= createComputePipeline(vk,device,m_pipelineCache, &m_pipelineInfo[shaderNdx]);

			TestStatus result = executeComputePipeline(m_context, *pipeline, m_pipelineLayout, m_descriptorSetLayout, m_queues, m_shadersExecutions[shaderNdx]);

#ifdef CTS_USES_VULKANSC
			// While collecting pipelines, synchronize between all threads for each pipeline that gets
			// created, so we will reserve the maximum amount of pipeline pool space that could need.
			if (!m_context.getTestContext().getCommandLine().isSubProcess()) {
				barrier();
			}
#endif

			resultCollector.addResult(result.getCode(), result.getDescription());
		}
		return TestStatus(resultCollector.getResult(), resultCollector.getMessage());
	}
private:
	vector<VkComputePipelineCreateInfo>&	m_pipelineInfo;
};

class CreateGraphicThread : public ThreadGroupThread
{
public:
			CreateGraphicThread	(const Context& context, VkPipelineCache pipelineCache, vector<VkGraphicsPipelineCreateInfo>& pipelineInfo,
								const VkPipelineLayout& pipelineLayout, const VkDescriptorSetLayout& descriptorSetLayout,
								MultiQueues& queues, const VkRenderPass& renderPass, const vector<deUint32>& shadersExecutions)
				: ThreadGroupThread		(context, pipelineCache, pipelineLayout, descriptorSetLayout, queues, shadersExecutions)
				, m_pipelineInfo		(pipelineInfo)
				, m_renderPass			(renderPass)
	{}

	TestStatus	runThread		(void)
	{
		ResultCollector		resultCollector;
		for (int executionNdx = 0; executionNdx < EXECUTION_PER_THREAD; ++executionNdx)
		{
			const int shaderNdx					= executionNdx % (int)m_pipelineInfo.size();
			const DeviceInterface&	vk			= m_context.getDeviceInterface();
			const VkDevice			device		= m_queues.getDevice();
			Move<VkPipeline>		pipeline	= createGraphicsPipeline(vk,device, m_pipelineCache, &m_pipelineInfo[shaderNdx]);

			TestStatus result = executeGraphicPipeline(m_context, *pipeline, m_pipelineLayout, m_descriptorSetLayout, m_queues, m_renderPass, m_shadersExecutions[shaderNdx]);

#ifdef CTS_USES_VULKANSC
			// While collecting pipelines, synchronize between all threads for each pipeline that gets
			// created, so we will reserve the maximum amount of pipeline pool space that could need.
			if (!m_context.getTestContext().getCommandLine().isSubProcess()) {
				barrier();
			}
#endif

			resultCollector.addResult(result.getCode(), result.getDescription());
		}
		return TestStatus(resultCollector.getResult(), resultCollector.getMessage());
	}

private:
	vector<VkGraphicsPipelineCreateInfo>&	m_pipelineInfo;
	const VkRenderPass&						m_renderPass;
};

class PipelineCacheComputeTestInstance  : public TestInstance
{
	typedef vector<SharedPtr<Unique<VkShaderModule> > > ShaderModuleVector;
public:
				PipelineCacheComputeTestInstance	(Context& context, const vector<deUint32>& shadersExecutions)
					: TestInstance			(context)
					, m_shadersExecutions	(shadersExecutions)

	{
	}

	TestStatus	iterate								(void)
	{
#ifdef CTS_USES_VULKANSC
		MultithreadedDestroyGuard				mdGuard				(m_context.getResourceInterface());
#endif // CTS_USES_VULKANSC
		const CustomInstance					instance			(createCustomInstanceFromContext(m_context));
		const InstanceDriver&					instanceDriver		(instance.getDriver());

		MovePtr<MultiQueues>					queues				= createQueues(m_context, VK_QUEUE_COMPUTE_BIT, instance, instanceDriver);
		const DeviceInterface&					vk					= queues->getDeviceInterface();
		const VkDevice							device				= queues->getDevice();
		ShaderModuleVector						shaderCompModules	= addShaderModules(device);
		Buffer									resultBuffer		(vk, device, *queues->m_allocator, makeBufferCreateInfo(BUFFER_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);
		const Move<VkDescriptorSetLayout>		descriptorSetLayout	(DescriptorSetLayoutBuilder()
																		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
																		.build(vk, device));
		const Move<VkPipelineLayout>			pipelineLayout		(makePipelineLayout(vk, device, *descriptorSetLayout));
		vector<VkPipelineShaderStageCreateInfo>	shaderStageInfos	= addShaderStageInfo(shaderCompModules);
		vector<VkComputePipelineCreateInfo>		pipelineInfo		= addPipelineInfo(*pipelineLayout, shaderStageInfos);
		const VkPipelineCacheCreateInfo			pipelineCacheInfo	=
																	{
																		VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,	// VkStructureType             sType;
																		DE_NULL,										// const void*                 pNext;
#ifndef CTS_USES_VULKANSC
																		0u,												// VkPipelineCacheCreateFlags  flags;
																		0u,												// deUintptr                   initialDataSize;
																		DE_NULL,										// const void*                 pInitialData;
#else
																		VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
																			VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT,	// VkPipelineCacheCreateFlags	flags;
																		m_context.getResourceInterface()->getCacheDataSize(),		// deUintptr					initialDataSize;
																		m_context.getResourceInterface()->getCacheData()			// const void*					pInitialData;
#endif // CTS_USES_VULKANSC
																	};
		Move<VkPipelineCache>					pipelineCache		= createPipelineCache(vk, device, &pipelineCacheInfo);
		Move<VkPipeline>						pipeline			= createComputePipeline(vk, device, *pipelineCache, &pipelineInfo[0]);
#ifndef CTS_USES_VULKANSC
		const deUint32							numThreads			= clamp(deGetNumAvailableLogicalCores(), 4u, 32u);
#else
		const deUint32							numThreads			= 2u;
#endif // CTS_USES_VULKANSC
		ThreadGroup								threads;

		executeComputePipeline(m_context, *pipeline, *pipelineLayout, *descriptorSetLayout, *queues, m_shadersExecutions[0]);

		for (deUint32 ndx = 0; ndx < numThreads; ++ndx)
			threads.add(MovePtr<ThreadGroupThread>(new CreateComputeThread(
				m_context, *pipelineCache, pipelineInfo, *pipelineLayout, *descriptorSetLayout, *queues, m_shadersExecutions)));

		{
			TestStatus thread_result = threads.run();
			if(thread_result.getCode())
			{
				return thread_result;
			}
		}
		return TestStatus::pass("Passed");
	}

private:
	ShaderModuleVector							addShaderModules					(const VkDevice& device)
	{
		const DeviceInterface&	vk	= m_context.getDeviceInterface();
		ShaderModuleVector		shaderCompModules;
		shaderCompModules.resize(m_shadersExecutions.size());
		for (int shaderNdx = 0; shaderNdx <  static_cast<int>(m_shadersExecutions.size()); ++shaderNdx)
		{
			ostringstream shaderName;
			shaderName<<"compute_"<<shaderNdx;
			shaderCompModules[shaderNdx] = SharedPtr<Unique<VkShaderModule> > (new Unique<VkShaderModule>(createShaderModule(vk, device, m_context.getBinaryCollection().get(shaderName.str()), (VkShaderModuleCreateFlags)0)));
		}
		return shaderCompModules;
	}

	vector<VkPipelineShaderStageCreateInfo>		addShaderStageInfo					(const ShaderModuleVector& shaderCompModules)
	{
		VkPipelineShaderStageCreateInfo			shaderStageInfo;
		vector<VkPipelineShaderStageCreateInfo>	shaderStageInfos;
		shaderStageInfo.sType				=	VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo.pNext				=	DE_NULL;
		shaderStageInfo.flags				=	(VkPipelineShaderStageCreateFlags)0;
		shaderStageInfo.stage				=	VK_SHADER_STAGE_COMPUTE_BIT;
		shaderStageInfo.pName				=	"main";
		shaderStageInfo.pSpecializationInfo	=	DE_NULL;

		for (int shaderNdx = 0; shaderNdx <  static_cast<int>(m_shadersExecutions.size()); ++shaderNdx)
		{
			shaderStageInfo.module = *(*shaderCompModules[shaderNdx]);
			shaderStageInfos.push_back(shaderStageInfo);
		}
		return shaderStageInfos;
	}

	vector<VkComputePipelineCreateInfo>		addPipelineInfo						(VkPipelineLayout pipelineLayout, const vector<VkPipelineShaderStageCreateInfo>& shaderStageInfos)
	{
		vector<VkComputePipelineCreateInfo> pipelineInfos;
		VkComputePipelineCreateInfo	computePipelineInfo;
									computePipelineInfo.sType				= VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
									computePipelineInfo.pNext				= DE_NULL;
									computePipelineInfo.flags				= (VkPipelineCreateFlags)0;
									computePipelineInfo.layout				= pipelineLayout;
									computePipelineInfo.basePipelineHandle	= DE_NULL;
									computePipelineInfo.basePipelineIndex	= 0;

		for (int shaderNdx = 0; shaderNdx < static_cast<int>(m_shadersExecutions.size()); ++shaderNdx)
		{
			computePipelineInfo.stage = shaderStageInfos[shaderNdx];
			pipelineInfos.push_back(computePipelineInfo);
		}
		return pipelineInfos;
	}

	const vector<deUint32>	m_shadersExecutions;
};

class PipelineCacheGraphicTestInstance  : public TestInstance
{
	typedef vector<SharedPtr<Unique<VkShaderModule> > > ShaderModuleVector;
public:
											PipelineCacheGraphicTestInstance	(Context& context, const vector<deUint32>& shadersExecutions)
								: TestInstance			(context)
								, m_shadersExecutions	(shadersExecutions)

	{
	}

	TestStatus								iterate								(void)
	{
#ifdef CTS_USES_VULKANSC
		MultithreadedDestroyGuard				mdGuard					(m_context.getResourceInterface());
#endif // CTS_USES_VULKANSC
		const CustomInstance					instance				(createCustomInstanceFromContext(m_context));
		const InstanceDriver&					instanceDriver			(instance.getDriver());
		const VkPhysicalDevice					physicalDevice			= chooseDevice(instanceDriver, instance, m_context.getTestContext().getCommandLine());
		requireFeatures(instanceDriver, physicalDevice, FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS);

		MovePtr<MultiQueues>					queues					= createQueues(m_context, VK_QUEUE_GRAPHICS_BIT, instance, instanceDriver);
		const DeviceInterface&					vk						= m_context.getDeviceInterface();
		const VkDevice							device					= queues->getDevice();
		VkFormat								colorFormat				= VK_FORMAT_R8G8B8A8_UNORM;
		Move<VkRenderPass>						renderPass				= makeRenderPass(vk, device, colorFormat);
		const Move<VkDescriptorSetLayout>		descriptorSetLayout		(DescriptorSetLayoutBuilder()
																			.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
																			.build(vk, device));
		ShaderModuleVector						shaderGraphicModules	= addShaderModules(device);
		const Move<VkPipelineLayout>			pipelineLayout			(makePipelineLayout(vk, device, *descriptorSetLayout));
		vector<VkPipelineShaderStageCreateInfo>	shaderStageInfos		= addShaderStageInfo(shaderGraphicModules);
		vector<VkGraphicsPipelineCreateInfo>	pipelineInfo			= addPipelineInfo(*pipelineLayout, shaderStageInfos, *renderPass);
		const VkPipelineCacheCreateInfo			pipelineCacheInfo		=
																		{
																			VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,	// VkStructureType             sType;
																			DE_NULL,										// const void*                 pNext;
#ifndef CTS_USES_VULKANSC
																			0u,												// VkPipelineCacheCreateFlags  flags;
																			0u,												// deUintptr                   initialDataSize;
																			DE_NULL											// const void*                 pInitialData;
#else
																			VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
																				VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT,	// VkPipelineCacheCreateFlags	flags;
																			m_context.getResourceInterface()->getCacheDataSize(),		// deUintptr					initialDataSize;
																			m_context.getResourceInterface()->getCacheData()			// const void*					pInitialData;
#endif // CTS_USES_VULKANSC
																		};
		Move<VkPipelineCache>					pipelineCache			= createPipelineCache(vk, device, &pipelineCacheInfo);
		Move<VkPipeline>						pipeline				= createGraphicsPipeline(vk, device, *pipelineCache, &pipelineInfo[0]);
#ifndef CTS_USES_VULKANSC
		const deUint32							numThreads				= clamp(deGetNumAvailableLogicalCores(), 4u, 32u);
#else
		const deUint32							numThreads				= 2u;
#endif // CTS_USES_VULKANSC
		ThreadGroup								threads;

		executeGraphicPipeline(m_context, *pipeline, *pipelineLayout, *descriptorSetLayout, *queues, *renderPass, m_shadersExecutions[0]);

		for (deUint32 ndx = 0; ndx < numThreads; ++ndx)
			threads.add(MovePtr<ThreadGroupThread>(new CreateGraphicThread(
				m_context, *pipelineCache, pipelineInfo, *pipelineLayout, *descriptorSetLayout, *queues, *renderPass, m_shadersExecutions)));

		{
			TestStatus thread_result = threads.run();
			if(thread_result.getCode())
			{
				return thread_result;
			}
		}
		return TestStatus::pass("Passed");
	}

private:
	ShaderModuleVector						addShaderModules					(const VkDevice& device)
	{
		const DeviceInterface&	vk					= m_context.getDeviceInterface();
		ShaderModuleVector		shaderModules;
		shaderModules.resize(m_shadersExecutions.size() + 1);
		for (int shaderNdx = 0; shaderNdx <  static_cast<int>(m_shadersExecutions.size()); ++shaderNdx)
		{
			ostringstream shaderName;
			shaderName<<"vert_"<<shaderNdx;
			shaderModules[shaderNdx] = SharedPtr<Unique<VkShaderModule> > (new Unique<VkShaderModule>(createShaderModule(vk, device, m_context.getBinaryCollection().get(shaderName.str()), (VkShaderModuleCreateFlags)0)));
		}
		shaderModules[m_shadersExecutions.size()] = SharedPtr<Unique<VkShaderModule> > (new Unique<VkShaderModule>(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), (VkShaderModuleCreateFlags)0)));
		return shaderModules;
	}

	vector<VkPipelineShaderStageCreateInfo>	addShaderStageInfo					(const ShaderModuleVector& shaderCompModules)
	{
		VkPipelineShaderStageCreateInfo			shaderStageInfo;
		vector<VkPipelineShaderStageCreateInfo>	shaderStageInfos;
		shaderStageInfo.sType				=	VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo.pNext				=	DE_NULL;
		shaderStageInfo.flags				=	(VkPipelineShaderStageCreateFlags)0;
		shaderStageInfo.pName				=	"main";
		shaderStageInfo.pSpecializationInfo	=	DE_NULL;

		for (int shaderNdx = 0; shaderNdx <  static_cast<int>(m_shadersExecutions.size()); ++shaderNdx)
		{
			shaderStageInfo.stage	=	VK_SHADER_STAGE_VERTEX_BIT;
			shaderStageInfo.module	= *(*shaderCompModules[shaderNdx]);
			shaderStageInfos.push_back(shaderStageInfo);

			shaderStageInfo.stage	=	VK_SHADER_STAGE_FRAGMENT_BIT;
			shaderStageInfo.module	= *(*shaderCompModules[m_shadersExecutions.size()]);
			shaderStageInfos.push_back(shaderStageInfo);
		}
		return shaderStageInfos;
	}

	vector<VkGraphicsPipelineCreateInfo>	addPipelineInfo						(VkPipelineLayout pipelineLayout, const vector<VkPipelineShaderStageCreateInfo>& shaderStageInfos, const VkRenderPass& renderPass)
	{
		VkExtent3D								colorImageExtent	= makeExtent3D(1u, 1u, 1u);
		vector<VkGraphicsPipelineCreateInfo>	pipelineInfo;

		m_vertexInputStateParams.sType								= VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		m_vertexInputStateParams.pNext								= DE_NULL;
		m_vertexInputStateParams.flags								= 0u;
		m_vertexInputStateParams.vertexBindingDescriptionCount		= 0u;
		m_vertexInputStateParams.pVertexBindingDescriptions			= DE_NULL;
		m_vertexInputStateParams.vertexAttributeDescriptionCount	= 0u;
		m_vertexInputStateParams.pVertexAttributeDescriptions		= DE_NULL;

		m_inputAssemblyStateParams.sType					= VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		m_inputAssemblyStateParams.pNext					= DE_NULL;
		m_inputAssemblyStateParams.flags					= 0u;
		m_inputAssemblyStateParams.topology					= VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		m_inputAssemblyStateParams.primitiveRestartEnable	= VK_FALSE;

		m_viewport.x			= 0.0f;
		m_viewport.y			= 0.0f;
		m_viewport.width		= (float)colorImageExtent.width;
		m_viewport.height		= (float)colorImageExtent.height;
		m_viewport.minDepth		= 0.0f;
		m_viewport.maxDepth		= 1.0f;

		//TODO
		m_scissor.offset.x		= 0;
		m_scissor.offset.y		= 0;
		m_scissor.extent.width	= colorImageExtent.width;
		m_scissor.extent.height	= colorImageExtent.height;

		m_viewportStateParams.sType			= VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		m_viewportStateParams.pNext			= DE_NULL;
		m_viewportStateParams.flags			= 0u;
		m_viewportStateParams.viewportCount	= 1u;
		m_viewportStateParams.pViewports	= &m_viewport;
		m_viewportStateParams.scissorCount	= 1u;
		m_viewportStateParams.pScissors		= &m_scissor;

		m_rasterStateParams.sType					= VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		m_rasterStateParams.pNext					= DE_NULL;
		m_rasterStateParams.flags					= 0u;
		m_rasterStateParams.depthClampEnable		= VK_FALSE;
		m_rasterStateParams.rasterizerDiscardEnable	= VK_FALSE;
		m_rasterStateParams.polygonMode				= VK_POLYGON_MODE_FILL;
		m_rasterStateParams.cullMode				= VK_CULL_MODE_NONE;
		m_rasterStateParams.frontFace				= VK_FRONT_FACE_COUNTER_CLOCKWISE;
		m_rasterStateParams.depthBiasEnable			= VK_FALSE;
		m_rasterStateParams.depthBiasConstantFactor	= 0.0f;
		m_rasterStateParams.depthBiasClamp			= 0.0f;
		m_rasterStateParams.depthBiasSlopeFactor	= 0.0f;
		m_rasterStateParams.lineWidth				= 1.0f;

		m_colorBlendAttachmentState.blendEnable			= VK_FALSE;
		m_colorBlendAttachmentState.srcColorBlendFactor	= VK_BLEND_FACTOR_ONE;
		m_colorBlendAttachmentState.dstColorBlendFactor	= VK_BLEND_FACTOR_ZERO;
		m_colorBlendAttachmentState.colorBlendOp		= VK_BLEND_OP_ADD;
		m_colorBlendAttachmentState.srcAlphaBlendFactor	= VK_BLEND_FACTOR_ONE;
		m_colorBlendAttachmentState.dstAlphaBlendFactor	= VK_BLEND_FACTOR_ZERO;
		m_colorBlendAttachmentState.alphaBlendOp		= VK_BLEND_OP_ADD;
		m_colorBlendAttachmentState.colorWriteMask		= VK_COLOR_COMPONENT_R_BIT |
														  VK_COLOR_COMPONENT_G_BIT |
														  VK_COLOR_COMPONENT_B_BIT |
														  VK_COLOR_COMPONENT_A_BIT;

		m_colorBlendStateParams.sType				= VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		m_colorBlendStateParams.pNext				= DE_NULL;
		m_colorBlendStateParams.flags				= 0u;
		m_colorBlendStateParams.logicOpEnable		= VK_FALSE;
		m_colorBlendStateParams.logicOp				= VK_LOGIC_OP_COPY;
		m_colorBlendStateParams.attachmentCount		= 1u;
		m_colorBlendStateParams.pAttachments		= &m_colorBlendAttachmentState;
		m_colorBlendStateParams.blendConstants[0]	= 0.0f;
		m_colorBlendStateParams.blendConstants[1]	= 0.0f;
		m_colorBlendStateParams.blendConstants[2]	= 0.0f;
		m_colorBlendStateParams.blendConstants[3]	= 0.0f;

		m_multisampleStateParams.sType					= VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		m_multisampleStateParams.pNext					= DE_NULL;
		m_multisampleStateParams.flags					= 0u;
		m_multisampleStateParams.rasterizationSamples	= VK_SAMPLE_COUNT_1_BIT;
		m_multisampleStateParams.sampleShadingEnable	= VK_FALSE;
		m_multisampleStateParams.minSampleShading		= 0.0f;
		m_multisampleStateParams.pSampleMask			= DE_NULL;
		m_multisampleStateParams.alphaToCoverageEnable	= VK_FALSE;
		m_multisampleStateParams.alphaToOneEnable		= VK_FALSE;

		m_depthStencilStateParams.sType					= VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		m_depthStencilStateParams.pNext					= DE_NULL;
		m_depthStencilStateParams.flags					= 0u;
		m_depthStencilStateParams.depthTestEnable		= VK_TRUE;
		m_depthStencilStateParams.depthWriteEnable		= VK_TRUE;
		m_depthStencilStateParams.depthCompareOp		= VK_COMPARE_OP_LESS_OR_EQUAL;
		m_depthStencilStateParams.depthBoundsTestEnable	= VK_FALSE;
		m_depthStencilStateParams.stencilTestEnable		= VK_FALSE;
		m_depthStencilStateParams.front.failOp			= VK_STENCIL_OP_KEEP;
		m_depthStencilStateParams.front.passOp			= VK_STENCIL_OP_KEEP;
		m_depthStencilStateParams.front.depthFailOp		= VK_STENCIL_OP_KEEP;
		m_depthStencilStateParams.front.compareOp		= VK_COMPARE_OP_NEVER;
		m_depthStencilStateParams.front.compareMask		= 0u;
		m_depthStencilStateParams.front.writeMask		= 0u;
		m_depthStencilStateParams.front.reference		= 0u;
		m_depthStencilStateParams.back.failOp			= VK_STENCIL_OP_KEEP;
		m_depthStencilStateParams.back.passOp			= VK_STENCIL_OP_KEEP;
		m_depthStencilStateParams.back.depthFailOp		= VK_STENCIL_OP_KEEP;
		m_depthStencilStateParams.back.compareOp		= VK_COMPARE_OP_NEVER;
		m_depthStencilStateParams.back.compareMask		= 0u;
		m_depthStencilStateParams.back.writeMask		= 0u;
		m_depthStencilStateParams.back.reference		= 0u;
		m_depthStencilStateParams.minDepthBounds		= 0.0f;
		m_depthStencilStateParams.maxDepthBounds		= 1.0f;

		VkGraphicsPipelineCreateInfo	graphicsPipelineParams	=
																{
																	VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType									sType;
																	DE_NULL,											// const void*										pNext;
																	0u,													// VkPipelineCreateFlags							flags;
																	2u,													// deUint32											stageCount;
																	DE_NULL,											// const VkPipelineShaderStageCreateInfo*			pStages;
																	&m_vertexInputStateParams,							// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
																	&m_inputAssemblyStateParams,						// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
																	DE_NULL,											// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
																	&m_viewportStateParams,								// const VkPipelineViewportStateCreateInfo*			pViewportState;
																	&m_rasterStateParams,								// const VkPipelineRasterizationStateCreateInfo*	pRasterState;
																	&m_multisampleStateParams,							// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
																	&m_depthStencilStateParams,							// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
																	&m_colorBlendStateParams,							// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
																	(const VkPipelineDynamicStateCreateInfo*)DE_NULL,	// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
																	pipelineLayout,										// VkPipelineLayout									layout;
																	renderPass,											// VkRenderPass										renderPass;
																	0u,													// deUint32											subpass;
																	DE_NULL,											// VkPipeline										basePipelineHandle;
																	0,													// deInt32											basePipelineIndex;
																};
		for (int shaderNdx = 0; shaderNdx < static_cast<int>(m_shadersExecutions.size()) * 2; shaderNdx+=2)
		{
			graphicsPipelineParams.pStages = &shaderStageInfos[shaderNdx];
			pipelineInfo.push_back(graphicsPipelineParams);
		}
		return pipelineInfo;
	}

	const vector<deUint32>					m_shadersExecutions;
	VkPipelineVertexInputStateCreateInfo	m_vertexInputStateParams;
	VkPipelineInputAssemblyStateCreateInfo	m_inputAssemblyStateParams;
	VkViewport								m_viewport;
	VkRect2D								m_scissor;
	VkPipelineViewportStateCreateInfo		m_viewportStateParams;
	VkPipelineRasterizationStateCreateInfo	m_rasterStateParams;
	VkPipelineColorBlendAttachmentState		m_colorBlendAttachmentState;
	VkPipelineColorBlendStateCreateInfo		m_colorBlendStateParams;
	VkPipelineMultisampleStateCreateInfo	m_multisampleStateParams;
	VkPipelineDepthStencilStateCreateInfo	m_depthStencilStateParams;
};

class PipelineCacheComputeTest : public TestCase
{
public:
							PipelineCacheComputeTest	(TestContext&		testCtx,
														const string&		name,
														const string&		description)
								:TestCase	(testCtx, name, description)
	{
	}

	void					initPrograms				(SourceCollections&	programCollection) const
	{
		ostringstream buffer;
		buffer	<< "layout(set = 0, binding = 0, std430) buffer Output\n"
				<< "{\n"
				<< "	int result[];\n"
				<< "} sb_out;\n";
		{
			ostringstream src;
			src	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
				<< "\n"
				<< "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
				<< "\n"
				<< buffer.str()
				<< "void main (void)\n"
				<< "{\n"
				<< "	highp uint ndx = gl_GlobalInvocationID.x;\n"
				<< "	sb_out.result[ndx] = int(ndx);\n"
				<< "}\n";
			programCollection.glslSources.add("compute_0") << glu::ComputeSource(src.str());
		}
		{
			ostringstream src;
			src	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
				<< "\n"
				<< "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
				<< "\n"
				<< buffer.str()
				<< "void main (void)\n"
				<< "{\n"
				<< "	for (highp uint ndx = 0u; ndx < "<<BUFFER_ELEMENT_COUNT<<"u; ndx++)\n"
				<< "	{\n"
				<< "		sb_out.result[ndx] = int(ndx);\n"
				<< "	}\n"
				<< "}\n";
			programCollection.glslSources.add("compute_1") << glu::ComputeSource(src.str());
		}
		{
			ostringstream src;
			src	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
				<< "\n"
				<< "layout(local_size_x = "<<BUFFER_ELEMENT_COUNT<<", local_size_y = 1, local_size_z = 1) in;\n"
				<< "\n"
				<< buffer.str()
				<< "void main (void)\n"
				<< "{\n"
				<< "	highp uint ndx = gl_LocalInvocationID.x;\n"
				<< "	sb_out.result[ndx] = int(ndx);\n"
				<< "}\n";
			programCollection.glslSources.add("compute_2") << glu::ComputeSource(src.str());
		}
	}

	TestInstance*			createInstance				(Context& context) const
	{
		vector<deUint32>	shadersExecutions;
		shadersExecutions.push_back(16u);	//compute_0
		shadersExecutions.push_back(1u);	//compute_1
		shadersExecutions.push_back(1u);	//compute_2
		return new PipelineCacheComputeTestInstance(context, shadersExecutions);
	}
};

class PipelineCacheGraphicTest : public TestCase
{
public:
							PipelineCacheGraphicTest	(TestContext&		testCtx,
														const string&		name,
														const string&		description)
								:TestCase	(testCtx, name, description)
	{

	}

	void					initPrograms				(SourceCollections&	programCollection) const
	{
		ostringstream buffer;
		buffer	<< "layout(set = 0, binding = 0, std430) buffer Output\n"
				<< "{\n"
				<< "	int result[];\n"
				<< "} sb_out;\n";

		// Vertex
		{
			std::ostringstream src;
			src	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
				<< "\n"
				<< buffer.str()
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "   sb_out.result[gl_VertexIndex] = int(gl_VertexIndex);\n"
				<< "   gl_PointSize = 1.0f;\n"
				<< "}\n";
			programCollection.glslSources.add("vert_0") << glu::VertexSource(src.str());
		}
		// Vertex
		{
			std::ostringstream src;
			src	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
				<< "\n"
				<< buffer.str()
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "	for (highp uint ndx = 0u; ndx < "<<BUFFER_ELEMENT_COUNT<<"u; ndx++)\n"
				<< "	{\n"
				<< "		sb_out.result[ndx] = int(ndx);\n"
				<< "	}\n"
				<< "	gl_PointSize = 1.0f;\n"
				<< "}\n";
			programCollection.glslSources.add("vert_1") << glu::VertexSource(src.str());
		}
		// Vertex
		{
			std::ostringstream src;
			src	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
				<< "\n"
				<< buffer.str()
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "	for (int ndx = "<<BUFFER_ELEMENT_COUNT-1<<"; ndx >= 0; ndx--)\n"
				<< "	{\n"
				<< "		sb_out.result[uint(ndx)] = ndx;\n"
				<< "	}\n"
				<< "	gl_PointSize = 1.0f;\n"
				<< "}\n";
			programCollection.glslSources.add("vert_2") << glu::VertexSource(src.str());
		}
		// Fragment
		{
			std::ostringstream src;
			src	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
				<< "\n"
				<< "layout(location = 0) out vec4 o_color;\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "    o_color = vec4(1.0);\n"
				<< "}\n";
			programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
		}
	}

	TestInstance*			createInstance				(Context& context) const
	{
		vector<deUint32>	shadersExecutions;
		shadersExecutions.push_back(16u);	//vert_0
		shadersExecutions.push_back(1u);	//vert_1
		shadersExecutions.push_back(1u);	//vert_2
		return new PipelineCacheGraphicTestInstance(context, shadersExecutions);
	}
};


} // anonymous

tcu::TestCaseGroup* createInternallySynchronizedObjects (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> tests(new tcu::TestCaseGroup(testCtx, "internally_synchronized_objects", "Internally synchronized objects"));
	tests->addChild(new PipelineCacheComputeTest(testCtx, "pipeline_cache_compute", "Internally synchronized object VkPipelineCache for compute pipeline is tested"));
	tests->addChild(new PipelineCacheGraphicTest(testCtx, "pipeline_cache_graphics", "Internally synchronized object VkPipelineCache for graphics pipeline is tested"));
	return tests.release();
}

} // synchronization
} // vkt
