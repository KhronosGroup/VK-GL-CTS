
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief Synchronization timeline semaphore tests
 *//*--------------------------------------------------------------------*/

#include "vktSynchronizationBasicSemaphoreTests.hpp"
#include "vktSynchronizationOperation.hpp"
#include "vktSynchronizationOperationTestData.hpp"
#include "vktSynchronizationOperationResources.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktSynchronizationUtil.hpp"
#include "vktExternalMemoryUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vkBarrierUtil.hpp"

#include "vkDefs.hpp"
#include "vkPlatform.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkRef.hpp"
#include "vkTypeUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkSafetyCriticalUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"

#include "deClock.h"
#include "deRandom.hpp"
#include "deThread.hpp"
#include "deUniquePtr.hpp"

#include <limits>
#include <set>
#include <iterator>
#include <algorithm>
#include <sstream>

namespace vkt
{
namespace synchronization
{
namespace
{

using namespace vk;
using tcu::TestLog;
using de::MovePtr;
using de::SharedPtr;

template<typename T>
inline SharedPtr<Move<T> > makeVkSharedPtr (Move<T> move)
{
	return SharedPtr<Move<T> >(new Move<T>(move));
}

template<typename T>
inline SharedPtr<T> makeSharedPtr (de::MovePtr<T> move)
{
	return SharedPtr<T>(move.release());
}

template<typename T>
inline SharedPtr<T> makeSharedPtr (T* ptr)
{
	return SharedPtr<T>(ptr);
}

deUint64 getMaxTimelineSemaphoreValueDifference(const InstanceInterface& vk,
												const VkPhysicalDevice physicalDevice)
{
	VkPhysicalDeviceTimelineSemaphoreProperties		timelineSemaphoreProperties;
	VkPhysicalDeviceProperties2						properties;

	deMemset(&timelineSemaphoreProperties, 0, sizeof(timelineSemaphoreProperties));
	timelineSemaphoreProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES;

	deMemset(&properties, 0, sizeof(properties));
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties.pNext = &timelineSemaphoreProperties;

	vk.getPhysicalDeviceProperties2(physicalDevice, &properties);

	return timelineSemaphoreProperties.maxTimelineSemaphoreValueDifference;
}

void deviceSignal (const DeviceInterface&		vk,
				   const VkDevice				device,
				   const VkQueue				queue,
				   const VkFence				fence,
				   const SynchronizationType	type,
				   const VkSemaphore			semaphore,
				   const deUint64				timelineValue)
{
	{
		VkSemaphoreSubmitInfoKHR	signalSemaphoreSubmitInfo	= makeCommonSemaphoreSubmitInfo(semaphore, timelineValue, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR);
		SynchronizationWrapperPtr	synchronizationWrapper		= getSynchronizationWrapper(type, vk, DE_TRUE);
		synchronizationWrapper->addSubmitInfo(
			0u,										// deUint32								waitSemaphoreInfoCount
			DE_NULL,								// const VkSemaphoreSubmitInfoKHR*		pWaitSemaphoreInfos
			0u,										// deUint32								commandBufferInfoCount
			DE_NULL,								// const VkCommandBufferSubmitInfoKHR*	pCommandBufferInfos
			1u,										// deUint32								signalSemaphoreInfoCount
			&signalSemaphoreSubmitInfo,				// const VkSemaphoreSubmitInfoKHR*		pSignalSemaphoreInfos
			DE_FALSE,
			DE_TRUE
		);
		VK_CHECK(synchronizationWrapper->queueSubmit(queue, DE_NULL));
	}

	if (fence != DE_NULL)
	{
		SynchronizationWrapperPtr synchronizationWrapper = getSynchronizationWrapper(type, vk, 1u);
		synchronizationWrapper->addSubmitInfo(
			0u,										// deUint32								waitSemaphoreInfoCount
			DE_NULL,								// const VkSemaphoreSubmitInfoKHR*		pWaitSemaphoreInfos
			0u,										// deUint32								commandBufferInfoCount
			DE_NULL,								// const VkCommandBufferSubmitInfoKHR*	pCommandBufferInfos
			0u,										// deUint32								signalSemaphoreInfoCount
			DE_NULL									// const VkSemaphoreSubmitInfoKHR*		pSignalSemaphoreInfos
		);
		VK_CHECK(synchronizationWrapper->queueSubmit(queue, fence));
		VK_CHECK(vk.waitForFences(device, 1u, &fence, VK_TRUE, ~(0ull)));
	}
}

void hostSignal (const DeviceInterface& vk, const VkDevice& device, VkSemaphore semaphore, const deUint64 timelineValue)
{
	VkSemaphoreSignalInfo	ssi	=
	{
		VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,	// VkStructureType				sType;
		DE_NULL,									// const void*					pNext;
		semaphore,									// VkSemaphore					semaphore;
		timelineValue,								// deUint64						value;
	};

	VK_CHECK(vk.signalSemaphore(device, &ssi));
}

class WaitTestInstance : public TestInstance
{
public:
	WaitTestInstance (Context& context, SynchronizationType type, bool waitAll, bool signalFromDevice)
		: TestInstance			(context)
		, m_type				(type)
		, m_waitAll				(waitAll)
		, m_signalFromDevice	(signalFromDevice)
	{
	}

	tcu::TestStatus iterate (void)
	{
		const DeviceInterface&								vk				= m_context.getDeviceInterface();
		const VkDevice&										device			= m_context.getDevice();
		const VkQueue										queue			= m_context.getUniversalQueue();
		Unique<VkFence>										fence			(createFence(vk, device));
		std::vector<SharedPtr<Move<VkSemaphore > > >		semaphorePtrs	(createTimelineSemaphores(vk, device, 100));
		de::Random											rng				(1234);
		std::vector<VkSemaphore>							semaphores;
		std::vector<deUint64>								timelineValues;

		for (deUint32 i = 0; i < semaphorePtrs.size(); i++)
		{
			semaphores.push_back((*semaphorePtrs[i]).get());
			timelineValues.push_back(rng.getInt(1, 10000));
		}

		if (m_waitAll)
		{

			for (deUint32 semIdx = 0; semIdx < semaphores.size(); semIdx++)
			{
				if (m_signalFromDevice)
				{
					deviceSignal(vk, device, queue, *fence, m_type, semaphores[semIdx], timelineValues[semIdx]);
					VK_CHECK(vk.resetFences(device, 1, &fence.get()));
				}
				else
					hostSignal(vk, device, semaphores[semIdx], timelineValues[semIdx]);
			}
		}
		else
		{
			deUint32	randomIdx	= rng.getInt(0, (deUint32)(semaphores.size() - 1));

			if (m_signalFromDevice)
				deviceSignal(vk, device, queue, *fence, m_type, semaphores[randomIdx], timelineValues[randomIdx]);
			else
				hostSignal(vk, device, semaphores[randomIdx], timelineValues[randomIdx]);
		}

		{
			const VkSemaphoreWaitInfo		waitInfo	=
			{
				VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,									// VkStructureType			sType;
				DE_NULL,																// const void*				pNext;
				m_waitAll ? 0u : (VkSemaphoreWaitFlags) VK_SEMAPHORE_WAIT_ANY_BIT,	// VkSemaphoreWaitFlagsKHR	flags;
				(deUint32) semaphores.size(),											// deUint32					semaphoreCount;
				&semaphores[0],															// const VkSemaphore*		pSemaphores;
				&timelineValues[0],														// const deUint64*			pValues;
			};

			VkResult result = vk.waitSemaphores(device, &waitInfo, 0ull);
			if (result != VK_SUCCESS)
				return tcu::TestStatus::fail("Wait failed");
		}

		VK_CHECK(vk.deviceWaitIdle(device));

		return tcu::TestStatus::pass("Wait success");
	}

private:

	std::vector<SharedPtr<Move<VkSemaphore > > > createTimelineSemaphores(const DeviceInterface& vk, const VkDevice& device, deUint32 count)
	{
		std::vector<SharedPtr<Move<VkSemaphore > > > semaphores;

		for (deUint32 i = 0; i < count; i++)
			semaphores.push_back(makeVkSharedPtr(createSemaphoreType(vk, device, VK_SEMAPHORE_TYPE_TIMELINE)));

		return semaphores;
	}

	const SynchronizationType	m_type;
	bool						m_waitAll;
	bool						m_signalFromDevice;
};

class WaitTestCase : public TestCase
{
public:
	WaitTestCase (tcu::TestContext& testCtx, const std::string& name, SynchronizationType type, bool waitAll, bool signalFromDevice)
		: TestCase				(testCtx, name.c_str(), "")
		, m_type				(type)
		, m_waitAll				(waitAll)
		, m_signalFromDevice	(signalFromDevice)
	{
	}

	void checkSupport(Context& context) const override
	{
		context.requireDeviceFunctionality("VK_KHR_timeline_semaphore");
		if (m_type == SynchronizationType::SYNCHRONIZATION2)
			context.requireDeviceFunctionality("VK_KHR_synchronization2");
	}

	TestInstance* createInstance (Context& context) const override
	{
		return new WaitTestInstance(context, m_type, m_waitAll, m_signalFromDevice);
	}

private:
	const SynchronizationType	m_type;
	bool						m_waitAll;
	bool						m_signalFromDevice;
};

// This test verifies that waiting from the host on a timeline point
// that is itself waiting for signaling works properly.
class HostWaitBeforeSignalTestInstance : public TestInstance
{
public:
	HostWaitBeforeSignalTestInstance (Context& context, SynchronizationType type)
		: TestInstance			(context)
		, m_type				(type)
	{
	}

	tcu::TestStatus iterate (void)
	{
		const DeviceInterface&	vk					= m_context.getDeviceInterface();
		const VkDevice&			device				= m_context.getDevice();
		const VkQueue			queue				= m_context.getUniversalQueue();
		Unique<VkSemaphore>		semaphore			(createSemaphoreType(vk, device, VK_SEMAPHORE_TYPE_TIMELINE));
		de::Random				rng					(1234);
		std::vector<deUint64>	timelineValues;

		// Host value we signal at the end.
		timelineValues.push_back(1 + rng.getInt(1, 10000));

		for (deUint32 i = 0; i < 12; i++)
		{
			const deUint64				newTimelineValue			= (timelineValues.back() + rng.getInt(1, 10000));
			VkSemaphoreSubmitInfoKHR	waitSemaphoreSubmitInfo		= makeCommonSemaphoreSubmitInfo(*semaphore, timelineValues.back(), VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR);
			VkSemaphoreSubmitInfoKHR	signalSemaphoreSubmitInfo	= makeCommonSemaphoreSubmitInfo(*semaphore, newTimelineValue, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR);
			SynchronizationWrapperPtr	synchronizationWrapper		= getSynchronizationWrapper(m_type, vk, DE_TRUE);

			synchronizationWrapper->addSubmitInfo(
				1u,										// deUint32								waitSemaphoreInfoCount
				&waitSemaphoreSubmitInfo,				// const VkSemaphoreSubmitInfoKHR*		pWaitSemaphoreInfos
				0u,										// deUint32								commandBufferInfoCount
				DE_NULL,								// const VkCommandBufferSubmitInfoKHR*	pCommandBufferInfos
				1u,										// deUint32								signalSemaphoreInfoCount
				&signalSemaphoreSubmitInfo,				// const VkSemaphoreSubmitInfoKHR*		pSignalSemaphoreInfos
				DE_TRUE,
				DE_TRUE
			);

			VK_CHECK(synchronizationWrapper->queueSubmit(queue, DE_NULL));

			timelineValues.push_back(newTimelineValue);
		}

		{
			const VkSemaphoreWaitInfo waitInfo =
			{
				VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,											// VkStructureType			sType;
				DE_NULL,																		// const void*				pNext;
				0u,																				// VkSemaphoreWaitFlagsKHR	flags;
				(deUint32) 1u,																	// deUint32					semaphoreCount;
				&semaphore.get(),																// const VkSemaphore*		pSemaphores;
				&timelineValues[rng.getInt(0, static_cast<int>(timelineValues.size() - 1))],	// const deUint64*			pValues;
			};

			VkResult result = vk.waitSemaphores(device, &waitInfo, 0ull);
			if (result != VK_TIMEOUT)
				return tcu::TestStatus::fail("Wait failed");
		}

		hostSignal(vk, device, *semaphore, timelineValues.front());

		{
			const VkSemaphoreWaitInfo waitInfo =
			{
				VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,		// VkStructureType			sType;
				DE_NULL,									// const void*				pNext;
				0u,											// VkSemaphoreWaitFlagsKHR	flags;
				(deUint32) 1u,								// deUint32					semaphoreCount;
				&semaphore.get(),							// const VkSemaphore*		pSemaphores;
				&timelineValues.back(),						// const deUint64*			pValues;
			};

			VkResult result = vk.waitSemaphores(device, &waitInfo, ~(0ull));
			if (result != VK_SUCCESS)
				return tcu::TestStatus::fail("Wait failed");
		}

		VK_CHECK(vk.deviceWaitIdle(device));

		return tcu::TestStatus::pass("Wait success");
	}

private:

	std::vector<SharedPtr<Move<VkSemaphore > > > createTimelineSemaphores(const DeviceInterface& vk, const VkDevice& device, deUint32 count)
	{
		std::vector<SharedPtr<Move<VkSemaphore > > > semaphores;

		for (deUint32 i = 0; i < count; i++)
			semaphores.push_back(makeVkSharedPtr(createSemaphoreType(vk, device, VK_SEMAPHORE_TYPE_TIMELINE)));

		return semaphores;
	}

protected:

	const SynchronizationType m_type;
};

class HostWaitBeforeSignalTestCase : public TestCase
{
public:
	HostWaitBeforeSignalTestCase(tcu::TestContext&		testCtx,
								 const std::string&		name,
								 SynchronizationType	type)
		: TestCase(testCtx, name.c_str(), "")
		, m_type(type)
	{
	}

	void checkSupport(Context& context) const override
	{
		context.requireDeviceFunctionality("VK_KHR_timeline_semaphore");
		if (m_type == SynchronizationType::SYNCHRONIZATION2)
			context.requireDeviceFunctionality("VK_KHR_synchronization2");
	}

	TestInstance* createInstance(Context& context) const override
	{
		return new HostWaitBeforeSignalTestInstance(context, m_type);
	}

protected:
	const SynchronizationType m_type;
};

class PollTestInstance : public TestInstance
{
public:
	PollTestInstance (Context& context, bool signalFromDevice)
		: TestInstance			(context)
		, m_signalFromDevice	(signalFromDevice)
	{
	}

	tcu::TestStatus iterate (void)
	{
		const DeviceInterface&								vk				= m_context.getDeviceInterface();
		const VkDevice&										device			= m_context.getDevice();
		const VkQueue										queue			= m_context.getUniversalQueue();
		Unique<VkFence>										fence			(createFence(vk, device));
		std::vector<SharedPtr<Move<VkSemaphore > > >		semaphorePtrs	(createTimelineSemaphores(vk, device, 100));
		de::Random											rng				(1234);
		std::vector<VkSemaphore>							semaphores;
		std::vector<deUint64>								timelineValues;
		const deUint64										secondInMicroSeconds	= 1000ull * 1000ull * 1000ull;
		deUint64											startTime;
		VkResult											result = VK_SUCCESS;

		for (deUint32 i = 0; i < semaphorePtrs.size(); i++)
		{
			semaphores.push_back((*semaphorePtrs[i]).get());
			timelineValues.push_back(rng.getInt(1, 10000));
		}

		for (deUint32 semIdx = 0; semIdx < semaphores.size(); semIdx++)
		{
			if (m_signalFromDevice)
			{
				deviceSignal(vk, device, queue, semIdx == (semaphores.size() - 1) ? *fence : DE_NULL, SynchronizationType::LEGACY, semaphores[semIdx], timelineValues[semIdx]);
			}
			else
				hostSignal(vk, device, semaphores[semIdx], timelineValues[semIdx]);
		}

		startTime = deGetMicroseconds();

		do
		{
			deUint64	value;

			result = vk.getSemaphoreCounterValue(device, semaphores.back(), &value);

			if (result != VK_SUCCESS)
				break;

			if (value == timelineValues.back())
			{
				if (m_signalFromDevice)
					VK_CHECK(vk.waitForFences(device, 1u, &fence.get(), VK_TRUE, ~(0ull)));
				VK_CHECK(vk.deviceWaitIdle(device));
				return tcu::TestStatus::pass("Poll on timeline value succeeded");
			}

			if (value > timelineValues.back())
			{
				result = VK_ERROR_UNKNOWN;
				break;
			}
		} while ((deGetMicroseconds() - startTime) < secondInMicroSeconds);

		VK_CHECK(vk.deviceWaitIdle(device));

		if (result != VK_SUCCESS)
			return tcu::TestStatus::fail("Fail");
		return tcu::TestStatus::fail("Timeout");
	}

private:

	std::vector<SharedPtr<Move<VkSemaphore > > > createTimelineSemaphores(const DeviceInterface& vk, const VkDevice& device, deUint32 count)
	{
		std::vector<SharedPtr<Move<VkSemaphore > > > semaphores;

		for (deUint32 i = 0; i < count; i++)
			semaphores.push_back(makeVkSharedPtr(createSemaphoreType(vk, device, VK_SEMAPHORE_TYPE_TIMELINE)));

		return semaphores;
	}

	bool m_signalFromDevice;
};

class PollTestCase : public TestCase
{
public:
	PollTestCase (tcu::TestContext& testCtx, const std::string& name, bool signalFromDevice)
		: TestCase				(testCtx, name.c_str(), "")
		, m_signalFromDevice	(signalFromDevice)
	{
	}

	virtual void checkSupport(Context& context) const
	{
		context.requireDeviceFunctionality("VK_KHR_timeline_semaphore");
	}

	TestInstance* createInstance (Context& context) const
	{
		return new PollTestInstance(context, m_signalFromDevice);
	}

private:
	bool m_signalFromDevice;
};

class MonotonicallyIncrementChecker : public de::Thread
{
public:
	MonotonicallyIncrementChecker					(const DeviceInterface& vkd, VkDevice device, VkSemaphore semaphore)
		: de::Thread()
		, m_vkd(vkd)
		, m_device(device)
		, m_semaphore(semaphore)
		, m_running(true)
		, m_status(tcu::TestStatus::incomplete())
	{}

	virtual			~MonotonicallyIncrementChecker	(void)	{}

	tcu::TestStatus	getStatus						() { return m_status; }
	void			stop							() { m_running = false; }
	virtual void	run								()
	{
		deUint64 lastValue = 0;

		while (m_running)
		{
			deUint64 value;

			VK_CHECK(m_vkd.getSemaphoreCounterValue(m_device, m_semaphore, &value));

			if (value < lastValue) {
				m_status = tcu::TestStatus::fail("Value not monotonically increasing");
				return;
			}

			lastValue = value;
			deYield();
		}

		m_status = tcu::TestStatus::pass("Value monotonically increasing");
	}

private:
	const DeviceInterface&		m_vkd;
	VkDevice					m_device;
	VkSemaphore					m_semaphore;
	bool						m_running;
	tcu::TestStatus				m_status;
};

void checkSupport (Context& context, SynchronizationType type)
{
	context.requireDeviceFunctionality("VK_KHR_timeline_semaphore");
	if (type == SynchronizationType::SYNCHRONIZATION2)
		context.requireDeviceFunctionality("VK_KHR_synchronization2");
}

// Queue device signaling close to the edges of the
// maxTimelineSemaphoreValueDifference value and verify that the value
// of the semaphore never goes backwards.
tcu::TestStatus maxDifferenceValueCase (Context& context, SynchronizationType type)
{
	const DeviceInterface&							vk							= context.getDeviceInterface();
	const VkDevice&									device						= context.getDevice();
	const VkQueue									queue						= context.getUniversalQueue();
	const deUint64									requiredMinValueDifference	= deIntMaxValue32(32);
	const deUint64									maxTimelineValueDifference	= getMaxTimelineSemaphoreValueDifference(context.getInstanceInterface(), context.getPhysicalDevice());
	const Unique<VkSemaphore>						semaphore					(createSemaphoreType(vk, device, VK_SEMAPHORE_TYPE_TIMELINE));
	const Unique<VkFence>							fence						(createFence(vk, device));
	tcu::TestLog&									log							= context.getTestContext().getLog();
	MonotonicallyIncrementChecker					checkerThread				(vk, device, *semaphore);
	deUint64										iterations;
	deUint64										timelineBackValue;
	deUint64										timelineFrontValue;

	if (maxTimelineValueDifference < requiredMinValueDifference)
		return tcu::TestStatus::fail("Timeline semaphore max value difference test failed");

	iterations = std::min<deUint64>(std::numeric_limits<deUint64>::max() / maxTimelineValueDifference, 100ull);

	log << TestLog::Message
		<< " maxTimelineSemaphoreValueDifference=" << maxTimelineValueDifference
		<< " maxExpected=" << requiredMinValueDifference
		<< " iterations=" << iterations
		<< TestLog::EndMessage;

	checkerThread.start();

	timelineBackValue = timelineFrontValue = 1;
	hostSignal(vk, device, *semaphore, timelineFrontValue);

	for (deUint64 i = 0; i < iterations; i++)
	{
		deUint64	fenceValue;

		for (deUint32 j = 1; j <= 10; j++)
			deviceSignal(vk, device, queue, DE_NULL, type, *semaphore, ++timelineFrontValue);

		timelineFrontValue = timelineBackValue + maxTimelineValueDifference - 10;
		fenceValue = timelineFrontValue;
		deviceSignal(vk, device, queue, *fence, type, *semaphore, fenceValue);
		for (deUint32 j = 1; j < 10; j++)
			deviceSignal(vk, device, queue, DE_NULL, type, *semaphore, ++timelineFrontValue);

		deUint64 value;
		VK_CHECK(vk.getSemaphoreCounterValue(device, *semaphore, &value));

		VK_CHECK(vk.waitForFences(device, 1, &fence.get(), VK_TRUE, ~(0ull)));
		VK_CHECK(vk.resetFences(device, 1, &fence.get()));

		timelineBackValue = fenceValue;
	}

	VK_CHECK(vk.deviceWaitIdle(device));

	checkerThread.stop();
	checkerThread.join();

	return checkerThread.getStatus();
}

tcu::TestStatus initialValueCase (Context& context, SynchronizationType type)
{
	DE_UNREF(type);

	const DeviceInterface&							vk							= context.getDeviceInterface();
	const VkDevice&									device						= context.getDevice();
	const VkQueue									queue						= context.getUniversalQueue();
	const deUint64									maxTimelineValueDifference	= getMaxTimelineSemaphoreValueDifference(context.getInstanceInterface(), context.getPhysicalDevice());
	de::Random										rng							(1234);
	const deUint64									nonZeroValue				= 1 + rng.getUint64() % (maxTimelineValueDifference - 1);
	const Unique<VkSemaphore>						semaphoreDefaultValue		(createSemaphoreType(vk, device, VK_SEMAPHORE_TYPE_TIMELINE));
	const Unique<VkSemaphore>						semaphoreInitialValue		(createSemaphoreType(vk, device, VK_SEMAPHORE_TYPE_TIMELINE, 0, nonZeroValue));
	deUint64										initialValue;
	VkSemaphoreWaitInfo								waitInfo					=
	{
		VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		0u,											// VkSemaphoreWaitFlagsKHR	flags;
		1u,											// deUint32					semaphoreCount;
		DE_NULL,									// const VkSemaphore*		pSemaphores;
		&initialValue,								// const deUint64*			pValues;
	};
	deUint64										value;
	VkResult										result;

	waitInfo.pSemaphores = &semaphoreDefaultValue.get();
	initialValue = 0;
	result = vk.waitSemaphores(device, &waitInfo, 0ull);
	if (result != VK_SUCCESS)
		return tcu::TestStatus::fail("Wait zero initial value failed");

	{
		VkSemaphoreSubmitInfoKHR	waitSemaphoreSubmitInfo		= makeCommonSemaphoreSubmitInfo(*semaphoreDefaultValue, initialValue, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR);
		SynchronizationWrapperPtr	synchronizationWrapper		= getSynchronizationWrapper(type, vk, DE_TRUE);

		synchronizationWrapper->addSubmitInfo(
			1u,										// deUint32								waitSemaphoreInfoCount
			&waitSemaphoreSubmitInfo,				// const VkSemaphoreSubmitInfoKHR*		pWaitSemaphoreInfos
			0u,										// deUint32								commandBufferInfoCount
			DE_NULL,								// const VkCommandBufferSubmitInfoKHR*	pCommandBufferInfos
			0u,										// deUint32								signalSemaphoreInfoCount
			DE_NULL,								// const VkSemaphoreSubmitInfoKHR*		pSignalSemaphoreInfos
			DE_TRUE,
			DE_FALSE
		);

		VK_CHECK(synchronizationWrapper->queueSubmit(queue, DE_NULL));

		VK_CHECK(vk.deviceWaitIdle(device));
	}

	VK_CHECK(vk.getSemaphoreCounterValue(device, *semaphoreDefaultValue, &value));
#ifdef CTS_USES_VULKANSC
	if (context.getTestContext().getCommandLine().isSubProcess())
#endif // CTS_USES_VULKANSC
	{
		if (value != initialValue)
			return tcu::TestStatus::fail("Invalid zero initial value");
	}

	waitInfo.pSemaphores = &semaphoreInitialValue.get();
	initialValue = nonZeroValue;
	result = vk.waitSemaphores(device, &waitInfo, 0ull);
	if (result != VK_SUCCESS)
		return tcu::TestStatus::fail("Wait non zero initial value failed");

	VK_CHECK(vk.getSemaphoreCounterValue(device, *semaphoreInitialValue, &value));
#ifdef CTS_USES_VULKANSC
	if (context.getTestContext().getCommandLine().isSubProcess())
#endif // CTS_USES_VULKANSC
	{
		if (value != nonZeroValue)
			return tcu::TestStatus::fail("Invalid non zero initial value");
	}

	if (maxTimelineValueDifference != std::numeric_limits<deUint64>::max())
	{
		const deUint64				nonZeroMaxValue		= maxTimelineValueDifference + 1;
		const Unique<VkSemaphore>	semaphoreMaxValue	(createSemaphoreType(vk, device, VK_SEMAPHORE_TYPE_TIMELINE, 0, nonZeroMaxValue));

		waitInfo.pSemaphores = &semaphoreMaxValue.get();
		initialValue = nonZeroMaxValue;
		result = vk.waitSemaphores(device, &waitInfo, 0ull);
		if (result != VK_SUCCESS)
			return tcu::TestStatus::fail("Wait max value failed");

		VK_CHECK(vk.getSemaphoreCounterValue(device, *semaphoreMaxValue, &value));
#ifdef CTS_USES_VULKANSC
		if (context.getTestContext().getCommandLine().isSubProcess())
#endif // CTS_USES_VULKANSC
		{
			if (value != nonZeroMaxValue)
				return tcu::TestStatus::fail("Invalid max value initial value");
		}
	}

	return tcu::TestStatus::pass("Initial value correct");
}

class WaitTests : public tcu::TestCaseGroup
{
public:
	WaitTests (tcu::TestContext& testCtx, SynchronizationType type)
		: tcu::TestCaseGroup(testCtx, "wait", "Various wait cases of timeline semaphores")
		, m_type(type)
	{
	}

	void init (void)
	{
		static const struct
		{
			std::string	name;
			bool		waitAll;
			bool		signalFromDevice;
		}													waitCases[]	=
		{
			{ "all_signal_from_device",	true,	true },
			{ "one_signal_from_device",	false,	true },
			{ "all_signal_from_host",	true,	false },
			{ "one_signal_from_host",	false,	false },
		};

		for (deUint32 caseIdx = 0; caseIdx < DE_LENGTH_OF_ARRAY(waitCases); caseIdx++)
			addChild(new WaitTestCase(m_testCtx, waitCases[caseIdx].name, m_type, waitCases[caseIdx].waitAll, waitCases[caseIdx].signalFromDevice));
		addChild(new HostWaitBeforeSignalTestCase(m_testCtx, "host_wait_before_signal", m_type));
		addChild(new PollTestCase(m_testCtx, "poll_signal_from_device", true));
		addChild(new PollTestCase(m_testCtx, "poll_signal_from_host", false));
	}

protected:
	SynchronizationType m_type;
};

struct TimelineIteration
{
	TimelineIteration(OperationContext&						opContext,
					  const ResourceDescription&			resourceDesc,
					  const SharedPtr<OperationSupport>&	writeOpSupport,
					  const SharedPtr<OperationSupport>&	readOpSupport,
					  deUint64								lastValue,
					  de::Random&							rng)
		: resource(makeSharedPtr(new Resource(opContext, resourceDesc, writeOpSupport->getOutResourceUsageFlags() | readOpSupport->getInResourceUsageFlags())))
		, writeOp(makeSharedPtr(writeOpSupport->build(opContext, *resource)))
		, readOp(makeSharedPtr(readOpSupport->build(opContext, *resource)))
	{
		writeValue	= lastValue + rng.getInt(1, 100);
		readValue	= writeValue + rng.getInt(1, 100);
		cpuValue	= readValue + rng.getInt(1, 100);
	}
	~TimelineIteration() {}

	SharedPtr<Resource>		resource;

	SharedPtr<Operation>	writeOp;
	SharedPtr<Operation>	readOp;

	deUint64				writeValue;
	deUint64				readValue;
	deUint64				cpuValue;
};

class HostCopyThread : public de::Thread
{
public:
	HostCopyThread	(const DeviceInterface& vkd, VkDevice device, VkSemaphore semaphore, const std::vector<SharedPtr<TimelineIteration> >& iterations)
		: de::Thread()
		, m_vkd(vkd)
		, m_device(device)
		, m_semaphore(semaphore)
		, m_iterations(iterations) {}
	virtual			~HostCopyThread	(void)			{}

	virtual void	run								()
	{
		for (deUint32 iterIdx = 0; iterIdx < m_iterations.size(); iterIdx++)
		{
			// Wait on the GPU read operation.
			{
				const VkSemaphoreWaitInfo	waitInfo	=
				{
					VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,	// VkStructureType			sType;
					DE_NULL,									// const void*				pNext;
					0u,											// VkSemaphoreWaitFlagsKHR	flags;
					1u,											// deUint32					semaphoreCount
					&m_semaphore,								// VkSemaphore*				pSemaphores;
					&m_iterations[iterIdx]->readValue,			// deUint64*				pValues;
				};
				VkResult						result;

				result = m_vkd.waitSemaphores(m_device, &waitInfo, ~(deUint64)0u);
				if (result != VK_SUCCESS)
					return;
			}

			// Copy the data read on the GPU into the next GPU write operation.
			if (iterIdx < (m_iterations.size() - 1))
				m_iterations[iterIdx + 1]->writeOp->setData(m_iterations[iterIdx]->readOp->getData());

			// Signal the next GPU write operation.
			{
				const VkSemaphoreSignalInfo	signalInfo	=
				{
					VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,	// VkStructureType			sType;
					DE_NULL,										// const void*				pNext;
					m_semaphore,									// VkSemaphore				semaphore;
					m_iterations[iterIdx]->cpuValue,				// deUint64					value;
				};
				VkResult						result;

				result = m_vkd.signalSemaphore(m_device, &signalInfo);
				if (result != VK_SUCCESS)
					return;
			}
		}
	}

private:
	const DeviceInterface&								m_vkd;
	VkDevice											m_device;
	VkSemaphore											m_semaphore;
	const std::vector<SharedPtr<TimelineIteration> >&	m_iterations;
};

void randomizeData(std::vector<deUint8>& outData, const ResourceDescription& desc)
{
	de::Random	rng	(1234);

	if (desc.type == RESOURCE_TYPE_BUFFER) {
		for (deUint32 i = 0; i < outData.size(); i++)
			outData[i] = rng.getUint8();
	} else {
		const PlanarFormatDescription	planeDesc	= getPlanarFormatDescription(desc.imageFormat);
		tcu::PixelBufferAccess			access		(mapVkFormat(desc.imageFormat),
													 desc.size.x(), desc.size.y(), desc.size.z(),
													 static_cast<void *>(&outData[0]));

		DE_ASSERT(desc.type == RESOURCE_TYPE_IMAGE);

		for (int z = 0; z < access.getDepth(); z++) {
			for (int y = 0; y < access.getHeight(); y++) {
				for (int x = 0; x < access.getWidth(); x++) {
					if (isFloatFormat(desc.imageFormat)) {
						tcu::Vec4	value(rng.getFloat(), rng.getFloat(), rng.getFloat(), 1.0f);
						access.setPixel(value, x, y, z);
					} else {
						tcu::IVec4	value(rng.getInt(0, deIntMaxValue32(planeDesc.channels[0].sizeBits)),
										  rng.getInt(0, deIntMaxValue32(planeDesc.channels[1].sizeBits)),
										  rng.getInt(0, deIntMaxValue32(planeDesc.channels[2].sizeBits)),
										  rng.getInt(0, deIntMaxValue32(planeDesc.channels[3].sizeBits)));
						access.setPixel(value, x, y, z);
					}
				}
			}
		}
	}
}

// Create a chain of operations with data copied over on the device
// and the host with each operation depending on the previous one and
// verifies that the data at the beginning & end of the chain is the
// same.
class DeviceHostTestInstance : public TestInstance
{
public:
	DeviceHostTestInstance (Context&							context,
							SynchronizationType					type,
							const ResourceDescription&			resourceDesc,
							const SharedPtr<OperationSupport>&	writeOp,
							const SharedPtr<OperationSupport>&	readOp,
							PipelineCacheData&					pipelineCacheData)
		: TestInstance		(context)
		, m_type			(type)
		, m_opContext		(context, type, pipelineCacheData)
		, m_resourceDesc	(resourceDesc)
	{
		de::Random	rng		(1234);

		// Create a dozen couple of operations and their associated
		// resource.
		for (deUint32 i = 0; i < 12; i++)
		{
			m_iterations.push_back(makeSharedPtr(new TimelineIteration(m_opContext, resourceDesc, writeOp, readOp,
																	   i == 0 ? 0 : m_iterations.back()->cpuValue, rng)));
		}
	}

	tcu::TestStatus	iterate (void)
	{
		const DeviceInterface&								vk						= m_context.getDeviceInterface();
		const VkDevice										device					= m_context.getDevice();
		const VkQueue										queue					= m_context.getUniversalQueue();
		const deUint32										queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
		const Unique<VkSemaphore>							semaphore				(createSemaphoreType(vk, device, VK_SEMAPHORE_TYPE_TIMELINE));
		const Unique<VkCommandPool>							cmdPool					(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
		HostCopyThread										hostCopyThread			(vk, device, *semaphore, m_iterations);
		std::vector<SharedPtr<Move<VkCommandBuffer> > >		ptrCmdBuffers;
		std::vector<VkCommandBufferSubmitInfoKHR>			commandBufferSubmitInfos(m_iterations.size() * 2, makeCommonCommandBufferSubmitInfo(0));

		hostCopyThread.start();

		for (deUint32 opNdx = 0; opNdx < (m_iterations.size() * 2); opNdx++)
		{
			ptrCmdBuffers.push_back(makeVkSharedPtr(makeCommandBuffer(vk, device, *cmdPool)));
			commandBufferSubmitInfos[opNdx].commandBuffer = **(ptrCmdBuffers.back());
		}

		// Randomize the data copied over.
		{
			const Data				startData		= m_iterations.front()->writeOp->getData();
			Data					randomizedData;
			std::vector<deUint8>	dataArray;

			dataArray.resize(startData.size);
			randomizeData(dataArray, m_resourceDesc);
			randomizedData.size = dataArray.size();
			randomizedData.data = &dataArray[0];
			m_iterations.front()->writeOp->setData(randomizedData);
		}

		SynchronizationWrapperPtr				synchronizationWrapper		= getSynchronizationWrapper(m_type, vk, DE_TRUE, (deUint32)m_iterations.size() * 2u);
		std::vector<VkSemaphoreSubmitInfoKHR>	waitSemaphoreSubmitInfos	(m_iterations.size() * 2, makeCommonSemaphoreSubmitInfo(*semaphore, 0u, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR));
		std::vector<VkSemaphoreSubmitInfoKHR>	signalSemaphoreSubmitInfos	(m_iterations.size() * 2, makeCommonSemaphoreSubmitInfo(*semaphore, 0u, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR));

		for (deUint32 iterIdx = 0; iterIdx < m_iterations.size(); iterIdx++)
		{
			// Write operation
			{
				deUint32 wIdx = 2 * iterIdx;

				waitSemaphoreSubmitInfos[wIdx].value	= wIdx == 0 ? 0u : m_iterations[iterIdx - 1]->cpuValue;
				signalSemaphoreSubmitInfos[wIdx].value	= m_iterations[iterIdx]->writeValue;

				synchronizationWrapper->addSubmitInfo(
					wIdx == 0 ? 0u : 1u,							// deUint32								waitSemaphoreInfoCount
					&waitSemaphoreSubmitInfos[wIdx],				// const VkSemaphoreSubmitInfoKHR*		pWaitSemaphoreInfos
					1u,												// deUint32								commandBufferInfoCount
					&commandBufferSubmitInfos[wIdx],				// const VkCommandBufferSubmitInfoKHR*	pCommandBufferInfos
					1u,												// deUint32								signalSemaphoreInfoCount
					&signalSemaphoreSubmitInfos[wIdx],				// const VkSemaphoreSubmitInfoKHR*		pSignalSemaphoreInfos
					wIdx == 0 ? DE_FALSE : DE_TRUE,
					DE_TRUE
				);

				VkCommandBuffer cmdBuffer = commandBufferSubmitInfos[wIdx].commandBuffer;
				beginCommandBuffer(vk, cmdBuffer);
				m_iterations[iterIdx]->writeOp->recordCommands(cmdBuffer);

				{
					const SyncInfo	writeSync	= m_iterations[iterIdx]->writeOp->getOutSyncInfo();
					const SyncInfo	readSync	= m_iterations[iterIdx]->readOp->getInSyncInfo();
					const Resource& resource	= *(m_iterations[iterIdx]->resource);

					if (resource.getType() == RESOURCE_TYPE_IMAGE)
					{
						DE_ASSERT(writeSync.imageLayout != VK_IMAGE_LAYOUT_UNDEFINED);
						DE_ASSERT(readSync.imageLayout != VK_IMAGE_LAYOUT_UNDEFINED);

						const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
							writeSync.stageMask,							// VkPipelineStageFlags2KHR			srcStageMask
							writeSync.accessMask,							// VkAccessFlags2KHR				srcAccessMask
							readSync.stageMask,								// VkPipelineStageFlags2KHR			dstStageMask
							readSync.accessMask,							// VkAccessFlags2KHR				dstAccessMask
							writeSync.imageLayout,							// VkImageLayout					oldLayout
							readSync.imageLayout,							// VkImageLayout					newLayout
							resource.getImage().handle,						// VkImage							image
							resource.getImage().subresourceRange			// VkImageSubresourceRange			subresourceRange
						);
						VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
						synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
					}
					else
					{
						const VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 = makeBufferMemoryBarrier2(
							writeSync.stageMask,							// VkPipelineStageFlags2KHR			srcStageMask
							writeSync.accessMask,							// VkAccessFlags2KHR				srcAccessMask
							readSync.stageMask,								// VkPipelineStageFlags2KHR			dstStageMask
							readSync.accessMask,							// VkAccessFlags2KHR				dstAccessMask
							resource.getBuffer().handle,					// VkBuffer							buffer
							0,												// VkDeviceSize						offset
							VK_WHOLE_SIZE									// VkDeviceSize						size
						);
						VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, &bufferMemoryBarrier2);
						synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
					}
				}

				endCommandBuffer(vk, cmdBuffer);
			}

			// Read operation
			{
				deUint32 rIdx = 2 * iterIdx + 1;

				waitSemaphoreSubmitInfos[rIdx].value = m_iterations[iterIdx]->writeValue;
				signalSemaphoreSubmitInfos[rIdx].value = m_iterations[iterIdx]->readValue;

				synchronizationWrapper->addSubmitInfo(
					1u,												// deUint32								waitSemaphoreInfoCount
					&waitSemaphoreSubmitInfos[rIdx],				// const VkSemaphoreSubmitInfoKHR*		pWaitSemaphoreInfos
					1u,												// deUint32								commandBufferInfoCount
					&commandBufferSubmitInfos[rIdx],				// const VkCommandBufferSubmitInfoKHR*	pCommandBufferInfos
					1u,												// deUint32								signalSemaphoreInfoCount
					&signalSemaphoreSubmitInfos[rIdx],				// const VkSemaphoreSubmitInfoKHR*		pSignalSemaphoreInfos
					rIdx == 0 ? DE_FALSE : DE_TRUE,
					DE_TRUE
				);

				VkCommandBuffer cmdBuffer = commandBufferSubmitInfos[rIdx].commandBuffer;
				beginCommandBuffer(vk, cmdBuffer);
				m_iterations[iterIdx]->readOp->recordCommands(cmdBuffer);
				endCommandBuffer(vk, cmdBuffer);
			}
		}

		VK_CHECK(synchronizationWrapper->queueSubmit(queue, DE_NULL));

		VK_CHECK(vk.deviceWaitIdle(device));

		hostCopyThread.join();

		{
			const Data	expected = m_iterations.front()->writeOp->getData();
			const Data	actual	 = m_iterations.back()->readOp->getData();

			if (0 != deMemCmp(expected.data, actual.data, expected.size))
				return tcu::TestStatus::fail("Memory contents don't match");
		}

		return tcu::TestStatus::pass("OK");
	}

protected:
	const SynchronizationType					m_type;
	OperationContext							m_opContext;
	const ResourceDescription					m_resourceDesc;
	std::vector<SharedPtr<TimelineIteration> >	m_iterations;
};

class DeviceHostSyncTestCase : public TestCase
{
public:
	DeviceHostSyncTestCase	(tcu::TestContext&			testCtx,
							 const std::string&			name,
							 const std::string&			description,
							 SynchronizationType		type,
							 const ResourceDescription	resourceDesc,
							 const OperationName		writeOp,
							 const OperationName		readOp,
							 PipelineCacheData&			pipelineCacheData)
		: TestCase				(testCtx, name, description)
		, m_type				(type)
		, m_resourceDesc		(resourceDesc)
		, m_writeOp				(makeOperationSupport(writeOp, resourceDesc).release())
		, m_readOp				(makeOperationSupport(readOp, resourceDesc).release())
		, m_pipelineCacheData	(pipelineCacheData)
	{
	}

	void checkSupport(Context& context) const override
	{
		context.requireDeviceFunctionality("VK_KHR_timeline_semaphore");
		if (m_type == SynchronizationType::SYNCHRONIZATION2)
			context.requireDeviceFunctionality("VK_KHR_synchronization2");
	}

	void initPrograms (SourceCollections& programCollection) const override
	{
		m_writeOp->initPrograms(programCollection);
		m_readOp->initPrograms(programCollection);
	}

	TestInstance* createInstance (Context& context) const override
	{
		return new DeviceHostTestInstance(context, m_type, m_resourceDesc, m_writeOp, m_readOp, m_pipelineCacheData);
	}

private:
	const SynchronizationType			m_type;
	const ResourceDescription			m_resourceDesc;
	const SharedPtr<OperationSupport>	m_writeOp;
	const SharedPtr<OperationSupport>	m_readOp;
	PipelineCacheData&					m_pipelineCacheData;
};

class DeviceHostTestsBase : public tcu::TestCaseGroup
{
public:
	DeviceHostTestsBase(tcu::TestContext& testCtx, SynchronizationType type)
		: tcu::TestCaseGroup(testCtx, "device_host", "Synchronization of serialized device/host operations")
		, m_type(type)
	{
	}

	void initCommonTests (void)
	{
		static const OperationName		writeOps[]	=
		{
			OPERATION_NAME_WRITE_COPY_BUFFER,
			OPERATION_NAME_WRITE_COPY_BUFFER_TO_IMAGE,
			OPERATION_NAME_WRITE_COPY_IMAGE_TO_BUFFER,
			OPERATION_NAME_WRITE_COPY_IMAGE,
			OPERATION_NAME_WRITE_BLIT_IMAGE,
			OPERATION_NAME_WRITE_SSBO_VERTEX,
			OPERATION_NAME_WRITE_SSBO_TESSELLATION_CONTROL,
			OPERATION_NAME_WRITE_SSBO_TESSELLATION_EVALUATION,
			OPERATION_NAME_WRITE_SSBO_GEOMETRY,
			OPERATION_NAME_WRITE_SSBO_FRAGMENT,
			OPERATION_NAME_WRITE_SSBO_COMPUTE,
			OPERATION_NAME_WRITE_SSBO_COMPUTE_INDIRECT,
			OPERATION_NAME_WRITE_IMAGE_VERTEX,
			OPERATION_NAME_WRITE_IMAGE_TESSELLATION_CONTROL,
			OPERATION_NAME_WRITE_IMAGE_TESSELLATION_EVALUATION,
			OPERATION_NAME_WRITE_IMAGE_GEOMETRY,
			OPERATION_NAME_WRITE_IMAGE_FRAGMENT,
			OPERATION_NAME_WRITE_IMAGE_COMPUTE,
			OPERATION_NAME_WRITE_IMAGE_COMPUTE_INDIRECT,
		};
		static const OperationName		readOps[]	=
		{
			OPERATION_NAME_READ_COPY_BUFFER,
			OPERATION_NAME_READ_COPY_BUFFER_TO_IMAGE,
			OPERATION_NAME_READ_COPY_IMAGE_TO_BUFFER,
			OPERATION_NAME_READ_COPY_IMAGE,
			OPERATION_NAME_READ_BLIT_IMAGE,
			OPERATION_NAME_READ_UBO_VERTEX,
			OPERATION_NAME_READ_UBO_TESSELLATION_CONTROL,
			OPERATION_NAME_READ_UBO_TESSELLATION_EVALUATION,
			OPERATION_NAME_READ_UBO_GEOMETRY,
			OPERATION_NAME_READ_UBO_FRAGMENT,
			OPERATION_NAME_READ_UBO_COMPUTE,
			OPERATION_NAME_READ_UBO_COMPUTE_INDIRECT,
			OPERATION_NAME_READ_SSBO_VERTEX,
			OPERATION_NAME_READ_SSBO_TESSELLATION_CONTROL,
			OPERATION_NAME_READ_SSBO_TESSELLATION_EVALUATION,
			OPERATION_NAME_READ_SSBO_GEOMETRY,
			OPERATION_NAME_READ_SSBO_FRAGMENT,
			OPERATION_NAME_READ_SSBO_COMPUTE,
			OPERATION_NAME_READ_SSBO_COMPUTE_INDIRECT,
			OPERATION_NAME_READ_IMAGE_VERTEX,
			OPERATION_NAME_READ_IMAGE_TESSELLATION_CONTROL,
			OPERATION_NAME_READ_IMAGE_TESSELLATION_EVALUATION,
			OPERATION_NAME_READ_IMAGE_GEOMETRY,
			OPERATION_NAME_READ_IMAGE_FRAGMENT,
			OPERATION_NAME_READ_IMAGE_COMPUTE,
			OPERATION_NAME_READ_IMAGE_COMPUTE_INDIRECT,
			OPERATION_NAME_READ_INDIRECT_BUFFER_DRAW,
			OPERATION_NAME_READ_INDIRECT_BUFFER_DRAW_INDEXED,
			OPERATION_NAME_READ_INDIRECT_BUFFER_DISPATCH,
			OPERATION_NAME_READ_VERTEX_INPUT,
		};

		for (int writeOpNdx = 0; writeOpNdx < DE_LENGTH_OF_ARRAY(writeOps); ++writeOpNdx)
		for (int readOpNdx = 0; readOpNdx < DE_LENGTH_OF_ARRAY(readOps); ++readOpNdx)
		{
			const OperationName	writeOp		= writeOps[writeOpNdx];
			const OperationName	readOp		= readOps[readOpNdx];
			const std::string	opGroupName = getOperationName(writeOp) + "_" + getOperationName(readOp);
			bool				empty		= true;

			de::MovePtr<tcu::TestCaseGroup> opGroup	(new tcu::TestCaseGroup(m_testCtx, opGroupName.c_str(), ""));

			for (int resourceNdx = 0; resourceNdx < DE_LENGTH_OF_ARRAY(s_resources); ++resourceNdx)
			{
				const ResourceDescription&	resource	= s_resources[resourceNdx];
				std::string					name		= getResourceName(resource);

				if (isResourceSupported(writeOp, resource) && isResourceSupported(readOp, resource))
				{
					opGroup->addChild(new DeviceHostSyncTestCase(m_testCtx, name, "", m_type, resource, writeOp, readOp, m_pipelineCacheData));
					empty = false;
				}
			}
			if (!empty)
				addChild(opGroup.release());
		}
	}

protected:
	SynchronizationType m_type;

private:
	// synchronization.op tests share pipeline cache data to speed up test
	// execution.
	PipelineCacheData	m_pipelineCacheData;
};

class LegacyDeviceHostTests : public DeviceHostTestsBase
{
public:
	LegacyDeviceHostTests(tcu::TestContext& testCtx)
		: DeviceHostTestsBase(testCtx, SynchronizationType::LEGACY)
	{
	}

	void init(void)
	{
		initCommonTests();

		de::MovePtr<tcu::TestCaseGroup> miscGroup(new tcu::TestCaseGroup(m_testCtx, "misc", ""));
		addFunctionCase(miscGroup.get(), "max_difference_value", "Timeline semaphore properties test", checkSupport, maxDifferenceValueCase, m_type);
		addFunctionCase(miscGroup.get(), "initial_value", "Timeline semaphore initial value test", checkSupport, initialValueCase, m_type);
		addChild(miscGroup.release());
	}
};

class Sytnchronization2DeviceHostTests : public DeviceHostTestsBase
{
public:
	Sytnchronization2DeviceHostTests(tcu::TestContext& testCtx)
		: DeviceHostTestsBase(testCtx, SynchronizationType::SYNCHRONIZATION2)
	{
	}

	void init(void)
	{
		initCommonTests();

		de::MovePtr<tcu::TestCaseGroup> miscGroup(new tcu::TestCaseGroup(m_testCtx, "misc", ""));
		addFunctionCase(miscGroup.get(), "max_difference_value", "Timeline semaphore properties test", checkSupport, maxDifferenceValueCase, m_type);
		addChild(miscGroup.release());
	}
};

struct QueueTimelineIteration
{
	QueueTimelineIteration(const SharedPtr<OperationSupport>&	_opSupport,
						   deUint64								lastValue,
						   VkQueue								_queue,
						   deUint32								_queueFamilyIdx,
						   de::Random&							rng)
		: opSupport(_opSupport)
		, queue(_queue)
		, queueFamilyIdx(_queueFamilyIdx)
	{
		timelineValue	= lastValue + rng.getInt(1, 100);
	}
	~QueueTimelineIteration() {}

	SharedPtr<OperationSupport>	opSupport;
	VkQueue						queue;
	deUint32					queueFamilyIdx;
	deUint64					timelineValue;
	SharedPtr<Operation>		op;
};

std::vector<VkDeviceQueueCreateInfo> getQueueCreateInfo(const std::vector<VkQueueFamilyProperties> queueFamilyProperties)
{
	std::vector<VkDeviceQueueCreateInfo> infos;

	for (deUint32 i = 0; i < queueFamilyProperties.size(); i++) {
		VkDeviceQueueCreateInfo info =
		{
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			DE_NULL,
			0,
			i,
			queueFamilyProperties[i].queueCount,
			DE_NULL
		};
		infos.push_back(info);
	}

	return infos;
}

Move<VkDevice> createTestDevice(Context& context, const VkInstance& instance, const InstanceInterface& vki, SynchronizationType type)
{
	const VkPhysicalDevice							physicalDevice				= chooseDevice(vki, instance, context.getTestContext().getCommandLine());
	const std::vector<VkQueueFamilyProperties>		queueFamilyProperties		= getPhysicalDeviceQueueFamilyProperties(vki, physicalDevice);
	std::vector<VkDeviceQueueCreateInfo>			queueCreateInfos			= getQueueCreateInfo(queueFamilyProperties);
	VkPhysicalDeviceSynchronization2FeaturesKHR		synchronization2Features	{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR, DE_NULL, DE_TRUE };
	VkPhysicalDeviceTimelineSemaphoreFeatures		timelineSemaphoreFeatures	{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES, DE_NULL, DE_TRUE };
	VkPhysicalDeviceFeatures2						createPhysicalFeatures		{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &timelineSemaphoreFeatures, context.getDeviceFeatures() };
	void**											nextPtr						= &timelineSemaphoreFeatures.pNext;

	std::vector<const char*> deviceExtensions;

	if (!isCoreDeviceExtension(context.getUsedApiVersion(), "VK_KHR_timeline_semaphore"))
		deviceExtensions.push_back("VK_KHR_timeline_semaphore");
	if (type == SynchronizationType::SYNCHRONIZATION2)
	{
		deviceExtensions.push_back("VK_KHR_synchronization2");
		addToChainVulkanStructure(&nextPtr, synchronization2Features);
	}

	void* pNext												= &createPhysicalFeatures;
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
			memReservationInfo.pipelinePoolSizeCount			= deUint32(poolSizes.size());
			memReservationInfo.pPipelinePoolSizes				= poolSizes.data();
		}
	}
#endif // CTS_USES_VULKANSC

	const VkDeviceCreateInfo						deviceInfo				=
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,							//VkStructureType					sType;
		pNext,															//const void*						pNext;
		0u,																//VkDeviceCreateFlags				flags;
		static_cast<deUint32>(queueCreateInfos.size()),					//deUint32							queueCreateInfoCount;
		&queueCreateInfos[0],											//const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
		0u,																//deUint32							enabledLayerCount;
		DE_NULL,														//const char* const*				ppEnabledLayerNames;
		static_cast<deUint32>(deviceExtensions.size()),					//deUint32							enabledExtensionCount;
		deviceExtensions.data(),										//const char* const*				ppEnabledExtensionNames;
		0u																//const VkPhysicalDeviceFeatures*	pEnabledFeatures;
	};
	std::vector<SharedPtr<std::vector<float> > >	queuePriorities;

	for (auto& queueCreateInfo : queueCreateInfos)
	{
		MovePtr<std::vector<float> > priorities(new std::vector<float>);

		for (deUint32 i = 0; i < queueCreateInfo.queueCount; i++)
			priorities->push_back(1.0f);

		queuePriorities.push_back(makeSharedPtr(priorities));

		queueCreateInfo.pQueuePriorities = &(*queuePriorities.back().get())[0];
	}

	const auto validation = context.getTestContext().getCommandLine().isValidationEnabled();

	return createCustomDevice(validation, context.getPlatformInterface(), instance,
							  vki, physicalDevice, &deviceInfo);
}


// Class to wrap a singleton instance and device
class SingletonDevice
{
	SingletonDevice	(Context& context, SynchronizationType type)
		: m_logicalDevice	(createTestDevice(context, context.getInstance(), context.getInstanceInterface(), type))
	{
	}

public:

	static const Unique<vk::VkDevice>& getDevice(Context& context, SynchronizationType type)
	{
		if (!m_singletonDevice)
			m_singletonDevice = SharedPtr<SingletonDevice>(new SingletonDevice(context, type));

		DE_ASSERT(m_singletonDevice);
		return m_singletonDevice->m_logicalDevice;
	}

	static void destroy()
	{
		m_singletonDevice.clear();
	}
private:
	const Unique<vk::VkDevice>			m_logicalDevice;

	static SharedPtr<SingletonDevice>	m_singletonDevice;
};
SharedPtr<SingletonDevice>		SingletonDevice::m_singletonDevice;

static void cleanupGroup ()
{
	// Destroy singleton object
	SingletonDevice::destroy();
}

// Create a chain of operations with data copied across queues & host
// and submit the operations out of order to verify that the queues
// are properly unblocked as the work progresses.
class WaitBeforeSignalTestInstance : public TestInstance
{
public:
	WaitBeforeSignalTestInstance (Context&								context,
								  SynchronizationType					type,
								  const ResourceDescription&			resourceDesc,
								  const SharedPtr<OperationSupport>&	writeOp,
								  const SharedPtr<OperationSupport>&	readOp,
								  PipelineCacheData&					pipelineCacheData)
		: TestInstance		(context)
		, m_type			(type)
		, m_resourceDesc	(resourceDesc)
		, m_device			(SingletonDevice::getDevice(context, type))
		, m_context			(context)
#ifndef CTS_USES_VULKANSC
		, m_deviceDriver	(de::MovePtr<DeviceDriver>(new DeviceDriver(context.getPlatformInterface(), context.getInstance(), *m_device, context.getUsedApiVersion())))
#else
		, m_deviceDriver	(de::MovePtr<DeviceDriverSC, DeinitDeviceDeleter>(new DeviceDriverSC(context.getPlatformInterface(), context.getInstance(), *m_device, context.getTestContext().getCommandLine(), context.getResourceInterface(), m_context.getDeviceVulkanSC10Properties(), m_context.getDeviceProperties(), context.getUsedApiVersion()), vk::DeinitDeviceDeleter(context.getResourceInterface().get(), *m_device)))
#endif // CTS_USES_VULKANSC
		, m_allocator		(new SimpleAllocator(*m_deviceDriver, *m_device,
												 getPhysicalDeviceMemoryProperties(context.getInstanceInterface(),
												 chooseDevice(context.getInstanceInterface(), context.getInstance(), context.getTestContext().getCommandLine()))))
		, m_opContext		(context, type, *m_deviceDriver, *m_device, *m_allocator, pipelineCacheData)
	{
		const auto&									vki							= m_context.getInstanceInterface();
		const auto									instance					= m_context.getInstance();
		const DeviceInterface&						vk							= *m_deviceDriver;
		const VkDevice								device						= *m_device;
		const VkPhysicalDevice						physicalDevice				= chooseDevice(vki, instance, context.getTestContext().getCommandLine());
		const std::vector<VkQueueFamilyProperties>	queueFamilyProperties		= getPhysicalDeviceQueueFamilyProperties(vki, physicalDevice);
		const deUint32								universalQueueFamilyIndex	= context.getUniversalQueueFamilyIndex();
		de::Random									rng							(1234);
		deUint32									lastCopyOpIdx				= 0;
		std::set<std::pair<deUint32, deUint32> >	used_queues;

		m_hostTimelineValue = rng.getInt(0, 1000);

		m_iterations.push_back(makeSharedPtr(new QueueTimelineIteration(writeOp, m_hostTimelineValue,
																		getDeviceQueue(vk, device,
																		universalQueueFamilyIndex, 0),
																		universalQueueFamilyIndex, rng)));
		used_queues.insert(std::make_pair(universalQueueFamilyIndex, 0));

		// Go through all the queues and try to use all the ones that
		// support the type of resource we're dealing with.
		for (deUint32 familyIdx = 0; familyIdx < queueFamilyProperties.size(); familyIdx++) {
			for (deUint32 instanceIdx = 0; instanceIdx < queueFamilyProperties[familyIdx].queueCount; instanceIdx++) {
				// Only add each queue once.
				if (used_queues.find(std::make_pair(familyIdx, instanceIdx)) != used_queues.end())
					continue;

				// Find an operation compatible with the queue
				for (deUint32 copyOpIdx = 0; copyOpIdx < DE_LENGTH_OF_ARRAY(s_copyOps); copyOpIdx++) {
					OperationName					copyOpName			= s_copyOps[(lastCopyOpIdx + copyOpIdx) % DE_LENGTH_OF_ARRAY(s_copyOps)];

					if (isResourceSupported(copyOpName, resourceDesc))
					{
						SharedPtr<OperationSupport>	copyOpSupport		(makeOperationSupport(copyOpName, resourceDesc).release());
						VkQueueFlags				copyOpQueueFlags	= copyOpSupport->getQueueFlags(m_opContext);

						if ((copyOpQueueFlags & queueFamilyProperties[familyIdx].queueFlags) != copyOpQueueFlags)
							continue;

						m_iterations.push_back(makeSharedPtr(new QueueTimelineIteration(copyOpSupport, m_iterations.back()->timelineValue,
																						getDeviceQueue(vk, device, familyIdx, instanceIdx),
																						familyIdx, rng)));
						used_queues.insert(std::make_pair(familyIdx, instanceIdx));
						break;
					}
				}
			}
		}

		// Add the read operation on the universal queue, it should be
		// submitted in order with regard to the write operation.
		m_iterations.push_back(makeSharedPtr(new QueueTimelineIteration(readOp, m_iterations.back()->timelineValue,
																		getDeviceQueue(vk, device,
																		universalQueueFamilyIndex, 0),
																		universalQueueFamilyIndex, rng)));

		// Now create the resources with the usage associated to the
		// operation performed on the resource.
		for (deUint32 opIdx = 0; opIdx < (m_iterations.size() - 1); opIdx++)
		{
			deUint32 usage = m_iterations[opIdx]->opSupport->getOutResourceUsageFlags() | m_iterations[opIdx + 1]->opSupport->getInResourceUsageFlags();

			m_resources.push_back(makeSharedPtr(new Resource(m_opContext, resourceDesc, usage)));
		}

		m_iterations.front()->op = makeSharedPtr(m_iterations.front()->opSupport->build(m_opContext, *m_resources.front()).release());
		for (deUint32 opIdx = 1; opIdx < (m_iterations.size() - 1); opIdx++)
		{
			m_iterations[opIdx]->op = makeSharedPtr(m_iterations[opIdx]->opSupport->build(m_opContext,
																						  *m_resources[opIdx - 1],
																						  *m_resources[opIdx]).release());
		}
		m_iterations.back()->op = makeSharedPtr(m_iterations.back()->opSupport->build(m_opContext, *m_resources.back()).release());
	}

	~WaitBeforeSignalTestInstance()
	{
	}

	tcu::TestStatus	iterate (void)
	{
		const DeviceInterface&							vk							= *m_deviceDriver;
		const VkDevice									device						= *m_device;
		const Unique<VkSemaphore>						semaphore					(createSemaphoreType(vk, device, VK_SEMAPHORE_TYPE_TIMELINE));
		std::vector<SharedPtr<Move<VkCommandPool> > >	cmdPools;
		std::vector<SharedPtr<Move<VkCommandBuffer> > >	ptrCmdBuffers;
		std::vector<VkCommandBufferSubmitInfoKHR>		commandBufferSubmitInfos	(m_iterations.size(), makeCommonCommandBufferSubmitInfo(0));
		VkSemaphoreSubmitInfoKHR						waitSemaphoreSubmitInfo		= makeCommonSemaphoreSubmitInfo(*semaphore, 0u, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR);
		VkSemaphoreSubmitInfoKHR						signalSemaphoreSubmitInfo	= makeCommonSemaphoreSubmitInfo(*semaphore, 0u, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR);

		for (deUint32 opNdx = 0; opNdx < m_iterations.size(); opNdx++)
		{
			cmdPools.push_back(makeVkSharedPtr(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
																 m_iterations[opNdx]->queueFamilyIdx)));
			ptrCmdBuffers.push_back(makeVkSharedPtr(makeCommandBuffer(vk, device, **cmdPools.back())));
			commandBufferSubmitInfos[opNdx].commandBuffer = **(ptrCmdBuffers.back());
		}

		// Randomize the data copied over.
		{
			const Data				startData		= m_iterations.front()->op->getData();
			Data					randomizedData;
			std::vector<deUint8>	dataArray;

			dataArray.resize(startData.size);
			randomizeData(dataArray, m_resourceDesc);
			randomizedData.size = dataArray.size();
			randomizedData.data = &dataArray[0];
			m_iterations.front()->op->setData(randomizedData);
		}

		for (deUint32 _iterIdx = 0; _iterIdx < (m_iterations.size() - 1); _iterIdx++)
		{
			// Submit in reverse order of the dependency order to
			// exercise the wait-before-submit behavior.
			deUint32					iterIdx					= (deUint32)(m_iterations.size() - 2 - _iterIdx);
			VkCommandBuffer				cmdBuffer				= commandBufferSubmitInfos[iterIdx].commandBuffer;
			SynchronizationWrapperPtr	synchronizationWrapper	= getSynchronizationWrapper(m_type, vk, DE_TRUE);

			waitSemaphoreSubmitInfo.value		= iterIdx == 0 ? m_hostTimelineValue : m_iterations[iterIdx - 1]->timelineValue;
			signalSemaphoreSubmitInfo.value		= m_iterations[iterIdx]->timelineValue;

			synchronizationWrapper->addSubmitInfo(
				1u,										// deUint32								waitSemaphoreInfoCount
				&waitSemaphoreSubmitInfo,				// const VkSemaphoreSubmitInfoKHR*		pWaitSemaphoreInfos
				1u,										// deUint32								commandBufferInfoCount
				&commandBufferSubmitInfos[iterIdx],		// const VkCommandBufferSubmitInfoKHR*	pCommandBufferInfos
				1u,										// deUint32								signalSemaphoreInfoCount
				&signalSemaphoreSubmitInfo,				// const VkSemaphoreSubmitInfoKHR*		pSignalSemaphoreInfos
				DE_TRUE,
				DE_TRUE
			);

			beginCommandBuffer(vk, cmdBuffer);
			m_iterations[iterIdx]->op->recordCommands(cmdBuffer);

			{
				const SyncInfo	writeSync	= m_iterations[iterIdx]->op->getOutSyncInfo();
				const SyncInfo	readSync	= m_iterations[iterIdx + 1]->op->getInSyncInfo();
				const Resource&	resource	= *m_resources[iterIdx];

				if (resource.getType() == RESOURCE_TYPE_IMAGE)
				{
					DE_ASSERT(writeSync.imageLayout != VK_IMAGE_LAYOUT_UNDEFINED);
					DE_ASSERT(readSync.imageLayout != VK_IMAGE_LAYOUT_UNDEFINED);

					const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
						writeSync.stageMask,							// VkPipelineStageFlags2KHR			srcStageMask
						writeSync.accessMask,							// VkAccessFlags2KHR				srcAccessMask
						readSync.stageMask,								// VkPipelineStageFlags2KHR			dstStageMask
						readSync.accessMask,							// VkAccessFlags2KHR				dstAccessMask
						writeSync.imageLayout,							// VkImageLayout					oldLayout
						readSync.imageLayout,							// VkImageLayout					newLayout
						resource.getImage().handle,						// VkImage							image
						resource.getImage().subresourceRange,			// VkImageSubresourceRange			subresourceRange
						m_iterations[iterIdx]->queueFamilyIdx,			// deUint32							srcQueueFamilyIndex
						m_iterations[iterIdx + 1]->queueFamilyIdx		// deUint32							destQueueFamilyIndex
					);
					VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
					synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
				}
				else
				{
					const VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 = makeBufferMemoryBarrier2(
						writeSync.stageMask,							// VkPipelineStageFlags2KHR			srcStageMask
						writeSync.accessMask,							// VkAccessFlags2KHR				srcAccessMask
						readSync.stageMask,								// VkPipelineStageFlags2KHR			dstStageMask
						readSync.accessMask,							// VkAccessFlags2KHR				dstAccessMask
						resource.getBuffer().handle,					// VkBuffer							buffer
						0,												// VkDeviceSize						offset
						VK_WHOLE_SIZE,									// VkDeviceSize						size
						m_iterations[iterIdx]->queueFamilyIdx,			// deUint32							srcQueueFamilyIndex
						m_iterations[iterIdx + 1]->queueFamilyIdx		// deUint32							dstQueueFamilyIndex
					);
					VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, &bufferMemoryBarrier2);
					synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
				}
			}

			endCommandBuffer(vk, cmdBuffer);

			VK_CHECK(synchronizationWrapper->queueSubmit(m_iterations[iterIdx]->queue, DE_NULL));
		}

		// Submit the last read operation in order.
		{
			const deUint32				iterIdx					= (deUint32) (m_iterations.size() - 1);
			SynchronizationWrapperPtr	synchronizationWrapper	= getSynchronizationWrapper(m_type, vk, DE_TRUE);

			waitSemaphoreSubmitInfo.value		= m_iterations[iterIdx - 1]->timelineValue;
			signalSemaphoreSubmitInfo.value		= m_iterations[iterIdx]->timelineValue;

			synchronizationWrapper->addSubmitInfo(
				1u,										// deUint32								waitSemaphoreInfoCount
				&waitSemaphoreSubmitInfo,				// const VkSemaphoreSubmitInfoKHR*		pWaitSemaphoreInfos
				1u,										// deUint32								commandBufferInfoCount
				&commandBufferSubmitInfos[iterIdx],		// const VkCommandBufferSubmitInfoKHR*	pCommandBufferInfos
				1u,										// deUint32								signalSemaphoreInfoCount
				&signalSemaphoreSubmitInfo,				// const VkSemaphoreSubmitInfoKHR*		pSignalSemaphoreInfos
				DE_TRUE,
				DE_TRUE
			);

			VkCommandBuffer cmdBuffer = commandBufferSubmitInfos[iterIdx].commandBuffer;
			beginCommandBuffer(vk, cmdBuffer);
			m_iterations[iterIdx]->op->recordCommands(cmdBuffer);
			endCommandBuffer(vk, cmdBuffer);

			VK_CHECK(synchronizationWrapper->queueSubmit(m_iterations[iterIdx]->queue, DE_NULL));
		}

		{
			// Kick off the whole chain from the host.
			hostSignal(vk, device, *semaphore, m_hostTimelineValue);
			VK_CHECK(vk.deviceWaitIdle(device));
		}

		{
			const Data	expected = m_iterations.front()->op->getData();
			const Data	actual	 = m_iterations.back()->op->getData();

			if (0 != deMemCmp(expected.data, actual.data, expected.size))
				return tcu::TestStatus::fail("Memory contents don't match");
		}

		return tcu::TestStatus::pass("OK");
	}

protected:
	const SynchronizationType						m_type;
	const ResourceDescription						m_resourceDesc;
	const Unique<VkDevice>&							m_device;
	const Context&									m_context;
#ifndef CTS_USES_VULKANSC
	de::MovePtr<vk::DeviceDriver>					m_deviceDriver;
#else
	de::MovePtr<DeviceDriverSC,DeinitDeviceDeleter>	m_deviceDriver;
#endif // CTS_USES_VULKANSC
	MovePtr<Allocator>								m_allocator;
	OperationContext								m_opContext;
	std::vector<SharedPtr<QueueTimelineIteration> >	m_iterations;
	std::vector<SharedPtr<Resource> >				m_resources;
	deUint64										m_hostTimelineValue;
};

class WaitBeforeSignalTestCase : public TestCase
{
public:
	WaitBeforeSignalTestCase	(tcu::TestContext&			testCtx,
								 const std::string&			name,
								 const std::string&			description,
								 SynchronizationType		type,
								 const ResourceDescription	resourceDesc,
								 const OperationName		writeOp,
								 const OperationName		readOp,
								 PipelineCacheData&			pipelineCacheData)
		: TestCase				(testCtx, name, description)
		, m_type				(type)
		, m_resourceDesc		(resourceDesc)
		, m_writeOp				(makeOperationSupport(writeOp, resourceDesc).release())
		, m_readOp				(makeOperationSupport(readOp, resourceDesc).release())
		, m_pipelineCacheData	(pipelineCacheData)
	{
	}

	void checkSupport(Context& context) const override
	{
		context.requireDeviceFunctionality("VK_KHR_timeline_semaphore");
		if (m_type == SynchronizationType::SYNCHRONIZATION2)
			context.requireDeviceFunctionality("VK_KHR_synchronization2");
	}

	void initPrograms (SourceCollections& programCollection) const override
	{
		m_writeOp->initPrograms(programCollection);
		m_readOp->initPrograms(programCollection);

		for (deUint32 copyOpNdx = 0; copyOpNdx < DE_LENGTH_OF_ARRAY(s_copyOps); copyOpNdx++)
		{
			if (isResourceSupported(s_copyOps[copyOpNdx], m_resourceDesc))
				makeOperationSupport(s_copyOps[copyOpNdx], m_resourceDesc)->initPrograms(programCollection);
		}
	}

	TestInstance* createInstance (Context& context) const override
	{
		return new WaitBeforeSignalTestInstance(context, m_type, m_resourceDesc, m_writeOp, m_readOp, m_pipelineCacheData);
	}

private:
	SynchronizationType					m_type;
	const ResourceDescription			m_resourceDesc;
	const SharedPtr<OperationSupport>	m_writeOp;
	const SharedPtr<OperationSupport>	m_readOp;
	PipelineCacheData&					m_pipelineCacheData;
};

class WaitBeforeSignalTests : public tcu::TestCaseGroup
{
public:
	WaitBeforeSignalTests (tcu::TestContext& testCtx, SynchronizationType type)
		: tcu::TestCaseGroup(testCtx, "wait_before_signal", "Synchronization of out of order submissions to queues")
		, m_type(type)
	{
	}

	void init (void)
	{
		static const OperationName		writeOps[]	=
		{
			OPERATION_NAME_WRITE_COPY_BUFFER,
			OPERATION_NAME_WRITE_COPY_BUFFER_TO_IMAGE,
			OPERATION_NAME_WRITE_COPY_IMAGE_TO_BUFFER,
			OPERATION_NAME_WRITE_COPY_IMAGE,
			OPERATION_NAME_WRITE_BLIT_IMAGE,
			OPERATION_NAME_WRITE_SSBO_VERTEX,
			OPERATION_NAME_WRITE_SSBO_TESSELLATION_CONTROL,
			OPERATION_NAME_WRITE_SSBO_TESSELLATION_EVALUATION,
			OPERATION_NAME_WRITE_SSBO_GEOMETRY,
			OPERATION_NAME_WRITE_SSBO_FRAGMENT,
			OPERATION_NAME_WRITE_SSBO_COMPUTE,
			OPERATION_NAME_WRITE_SSBO_COMPUTE_INDIRECT,
			OPERATION_NAME_WRITE_IMAGE_VERTEX,
			OPERATION_NAME_WRITE_IMAGE_TESSELLATION_CONTROL,
			OPERATION_NAME_WRITE_IMAGE_TESSELLATION_EVALUATION,
			OPERATION_NAME_WRITE_IMAGE_GEOMETRY,
			OPERATION_NAME_WRITE_IMAGE_FRAGMENT,
			OPERATION_NAME_WRITE_IMAGE_COMPUTE,
			OPERATION_NAME_WRITE_IMAGE_COMPUTE_INDIRECT,
		};
		static const OperationName		readOps[]	=
		{
			OPERATION_NAME_READ_COPY_BUFFER,
			OPERATION_NAME_READ_COPY_BUFFER_TO_IMAGE,
			OPERATION_NAME_READ_COPY_IMAGE_TO_BUFFER,
			OPERATION_NAME_READ_COPY_IMAGE,
			OPERATION_NAME_READ_BLIT_IMAGE,
			OPERATION_NAME_READ_UBO_VERTEX,
			OPERATION_NAME_READ_UBO_TESSELLATION_CONTROL,
			OPERATION_NAME_READ_UBO_TESSELLATION_EVALUATION,
			OPERATION_NAME_READ_UBO_GEOMETRY,
			OPERATION_NAME_READ_UBO_FRAGMENT,
			OPERATION_NAME_READ_UBO_COMPUTE,
			OPERATION_NAME_READ_UBO_COMPUTE_INDIRECT,
			OPERATION_NAME_READ_SSBO_VERTEX,
			OPERATION_NAME_READ_SSBO_TESSELLATION_CONTROL,
			OPERATION_NAME_READ_SSBO_TESSELLATION_EVALUATION,
			OPERATION_NAME_READ_SSBO_GEOMETRY,
			OPERATION_NAME_READ_SSBO_FRAGMENT,
			OPERATION_NAME_READ_SSBO_COMPUTE,
			OPERATION_NAME_READ_SSBO_COMPUTE_INDIRECT,
			OPERATION_NAME_READ_IMAGE_VERTEX,
			OPERATION_NAME_READ_IMAGE_TESSELLATION_CONTROL,
			OPERATION_NAME_READ_IMAGE_TESSELLATION_EVALUATION,
			OPERATION_NAME_READ_IMAGE_GEOMETRY,
			OPERATION_NAME_READ_IMAGE_FRAGMENT,
			OPERATION_NAME_READ_IMAGE_COMPUTE,
			OPERATION_NAME_READ_IMAGE_COMPUTE_INDIRECT,
			OPERATION_NAME_READ_INDIRECT_BUFFER_DRAW,
			OPERATION_NAME_READ_INDIRECT_BUFFER_DRAW_INDEXED,
			OPERATION_NAME_READ_INDIRECT_BUFFER_DISPATCH,
			OPERATION_NAME_READ_VERTEX_INPUT,
		};

		for (int writeOpNdx = 0; writeOpNdx < DE_LENGTH_OF_ARRAY(writeOps); ++writeOpNdx)
		for (int readOpNdx = 0; readOpNdx < DE_LENGTH_OF_ARRAY(readOps); ++readOpNdx)
		{
			const OperationName	writeOp		= writeOps[writeOpNdx];
			const OperationName	readOp		= readOps[readOpNdx];
			const std::string	opGroupName = getOperationName(writeOp) + "_" + getOperationName(readOp);
			bool				empty		= true;

			de::MovePtr<tcu::TestCaseGroup> opGroup	(new tcu::TestCaseGroup(m_testCtx, opGroupName.c_str(), ""));

			for (int resourceNdx = 0; resourceNdx < DE_LENGTH_OF_ARRAY(s_resources); ++resourceNdx)
			{
				const ResourceDescription&	resource	= s_resources[resourceNdx];
				std::string					name		= getResourceName(resource);

				if (isResourceSupported(writeOp, resource) && isResourceSupported(readOp, resource))
				{
					opGroup->addChild(new WaitBeforeSignalTestCase(m_testCtx, name, "", m_type, resource, writeOp, readOp, m_pipelineCacheData));
					empty = false;
				}
			}
			if (!empty)
				addChild(opGroup.release());
		}
	}

	void deinit (void)
	{
		cleanupGroup();
	}

private:
	SynchronizationType m_type;

	// synchronization.op tests share pipeline cache data to speed up test
	// execution.
	PipelineCacheData	m_pipelineCacheData;
};

// Creates a tree of operations like this :
//
// WriteOp1-Queue0 --> CopyOp2-Queue1 --> ReadOp-Queue4
//                 |
//                 --> CopyOp3-Queue3 --> ReadOp-Queue5
//
// Verifies that we get the data propagated properly.
class OneToNTestInstance : public TestInstance
{
public:
	OneToNTestInstance (Context&							context,
						SynchronizationType					type,
						const ResourceDescription&			resourceDesc,
						const SharedPtr<OperationSupport>&	writeOp,
						const SharedPtr<OperationSupport>&	readOp,
						PipelineCacheData&					pipelineCacheData)
		: TestInstance		(context)
		, m_type			(type)
		, m_resourceDesc	(resourceDesc)
		, m_device			(SingletonDevice::getDevice(context, type))
		, m_context			(context)
#ifndef CTS_USES_VULKANSC
		, m_deviceDriver(de::MovePtr<DeviceDriver>(new DeviceDriver(context.getPlatformInterface(), context.getInstance(), *m_device, context.getUsedApiVersion())))
#else
		, m_deviceDriver(de::MovePtr<DeviceDriverSC, DeinitDeviceDeleter>(new DeviceDriverSC(context.getPlatformInterface(), context.getInstance(), *m_device, context.getTestContext().getCommandLine(), context.getResourceInterface(), m_context.getDeviceVulkanSC10Properties(), m_context.getDeviceProperties(), context.getUsedApiVersion()), vk::DeinitDeviceDeleter(context.getResourceInterface().get(), *m_device)))
#endif // CTS_USES_VULKANSC
		, m_allocator		(new SimpleAllocator(*m_deviceDriver, *m_device,
												 getPhysicalDeviceMemoryProperties(context.getInstanceInterface(),
												 chooseDevice(context.getInstanceInterface(), context.getInstance(), context.getTestContext().getCommandLine()))))
		, m_opContext		(context, type, *m_deviceDriver, *m_device, *m_allocator, pipelineCacheData)
	{
		const auto&									vki							= m_context.getInstanceInterface();
		const auto									instance					= m_context.getInstance();
		const DeviceInterface&						vk							= *m_deviceDriver;
		const VkDevice								device						= *m_device;
		const VkPhysicalDevice						physicalDevice				= chooseDevice(vki, instance, context.getTestContext().getCommandLine());
		const std::vector<VkQueueFamilyProperties>	queueFamilyProperties		= getPhysicalDeviceQueueFamilyProperties(vki, physicalDevice);
		const deUint32								universalQueueFamilyIndex	= context.getUniversalQueueFamilyIndex();
		de::Random									rng							(1234);
		deUint32									lastCopyOpIdx				= 0;
		deUint64									lastSubmitValue;

		m_hostTimelineValue = rng.getInt(0, 1000);

		m_writeIteration = makeSharedPtr(new QueueTimelineIteration(writeOp, m_hostTimelineValue,
																	getDeviceQueue(vk, device,
																	universalQueueFamilyIndex, 0),
																	universalQueueFamilyIndex, rng));
		lastSubmitValue = m_writeIteration->timelineValue;

		// Go through all the queues and try to use all the ones that
		// support the type of resource we're dealing with.
		for (deUint32 familyIdx = 0; familyIdx < queueFamilyProperties.size(); familyIdx++) {
			for (deUint32 instanceIdx = 0; instanceIdx < queueFamilyProperties[familyIdx].queueCount; instanceIdx++) {
				// Find an operation compatible with the queue
				for (deUint32 copyOpIdx = 0; copyOpIdx < DE_LENGTH_OF_ARRAY(s_copyOps); copyOpIdx++) {
					OperationName					copyOpName			= s_copyOps[(lastCopyOpIdx + copyOpIdx) % DE_LENGTH_OF_ARRAY(s_copyOps)];

					if (isResourceSupported(copyOpName, resourceDesc))
					{
						SharedPtr<OperationSupport>	copyOpSupport		(makeOperationSupport(copyOpName, resourceDesc).release());
						VkQueueFlags				copyOpQueueFlags	= copyOpSupport->getQueueFlags(m_opContext);

						if ((copyOpQueueFlags & queueFamilyProperties[familyIdx].queueFlags) != copyOpQueueFlags)
							continue;

						m_copyIterations.push_back(makeSharedPtr(new QueueTimelineIteration(copyOpSupport, lastSubmitValue,
																							getDeviceQueue(vk, device, familyIdx, instanceIdx),
																							familyIdx, rng)));
						lastSubmitValue = m_copyIterations.back()->timelineValue;
						break;
					}
				}
			}
		}

		for (deUint32 copyOpIdx = 0; copyOpIdx < m_copyIterations.size(); copyOpIdx++) {
			bool added = false;

			for (deUint32 familyIdx = 0; familyIdx < queueFamilyProperties.size() && !added; familyIdx++) {
				for (deUint32 instanceIdx = 0; instanceIdx < queueFamilyProperties[familyIdx].queueCount && !added; instanceIdx++) {
					VkQueueFlags	readOpQueueFlags	= readOp->getQueueFlags(m_opContext);

					// If the readOpQueueFlags contain the transfer bit set then check if the queue supports graphics or compute operations before skipping this iteration.
					// Because reporting transfer functionality is optional if a queue supports graphics or compute operations.
					if (((readOpQueueFlags & queueFamilyProperties[familyIdx].queueFlags) != readOpQueueFlags) &&
						(((readOpQueueFlags & VK_QUEUE_TRANSFER_BIT) == 0) ||
						((queueFamilyProperties[familyIdx].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) == 0)))
						continue;

					// Add the read operation on the universal queue, it should be
					// submitted in order with regard to the write operation.
					m_readIterations.push_back(makeSharedPtr(new QueueTimelineIteration(readOp, lastSubmitValue,
																						getDeviceQueue(vk, device,
																									   universalQueueFamilyIndex, 0),
																						universalQueueFamilyIndex, rng)));
					lastSubmitValue = m_readIterations.back()->timelineValue;

					added = true;
				}
			}

			DE_ASSERT(added);
		}

		DE_ASSERT(m_copyIterations.size() == m_readIterations.size());

		// Now create the resources with the usage associated to the
		// operation performed on the resource.
		{
			deUint32 writeUsage = writeOp->getOutResourceUsageFlags();

			for (deUint32 copyOpIdx = 0; copyOpIdx < m_copyIterations.size(); copyOpIdx++) {
				writeUsage |= m_copyIterations[copyOpIdx]->opSupport->getInResourceUsageFlags();
			}
			m_writeResource = makeSharedPtr(new Resource(m_opContext, resourceDesc, writeUsage));
			m_writeIteration->op = makeSharedPtr(writeOp->build(m_opContext, *m_writeResource).release());

			for (deUint32 copyOpIdx = 0; copyOpIdx < m_copyIterations.size(); copyOpIdx++)
			{
				deUint32 usage = m_copyIterations[copyOpIdx]->opSupport->getOutResourceUsageFlags() |
					m_readIterations[copyOpIdx]->opSupport->getInResourceUsageFlags();

				m_copyResources.push_back(makeSharedPtr(new Resource(m_opContext, resourceDesc, usage)));

				m_copyIterations[copyOpIdx]->op = makeSharedPtr(m_copyIterations[copyOpIdx]->opSupport->build(m_opContext,
																											  *m_writeResource,
																											  *m_copyResources[copyOpIdx]).release());
				m_readIterations[copyOpIdx]->op = makeSharedPtr(readOp->build(m_opContext,
																			  *m_copyResources[copyOpIdx]).release());
			}
		}
	}

	~OneToNTestInstance ()
	{
	}

	void recordBarrier (const DeviceInterface&	vk, VkCommandBuffer cmdBuffer, const QueueTimelineIteration& inIter, const QueueTimelineIteration& outIter, const Resource& resource, bool originalLayout)
	{
		const SyncInfo				writeSync				= inIter.op->getOutSyncInfo();
		const SyncInfo				readSync				= outIter.op->getInSyncInfo();
		SynchronizationWrapperPtr	synchronizationWrapper	= getSynchronizationWrapper(m_type, vk, DE_TRUE);

		if (resource.getType() == RESOURCE_TYPE_IMAGE)
		{
			DE_ASSERT(writeSync.imageLayout != VK_IMAGE_LAYOUT_UNDEFINED);
			DE_ASSERT(readSync.imageLayout != VK_IMAGE_LAYOUT_UNDEFINED);

			const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
				writeSync.stageMask,											// VkPipelineStageFlags2KHR			srcStageMask
				writeSync.accessMask,											// VkAccessFlags2KHR				srcAccessMask
				readSync.stageMask,												// VkPipelineStageFlags2KHR			dstStageMask
				readSync.accessMask,											// VkAccessFlags2KHR				dstAccessMask
				originalLayout ? writeSync.imageLayout : readSync.imageLayout,	// VkImageLayout					oldLayout
				readSync.imageLayout,											// VkImageLayout					newLayout
				resource.getImage().handle,										// VkImage							image
				resource.getImage().subresourceRange,							// VkImageSubresourceRange			subresourceRange
				inIter.queueFamilyIdx,											// deUint32							srcQueueFamilyIndex
				outIter.queueFamilyIdx											// deUint32							destQueueFamilyIndex
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
		}
		else
		{
			const VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 = makeBufferMemoryBarrier2(
				writeSync.stageMask,							// VkPipelineStageFlags2KHR			srcStageMask
				writeSync.accessMask,							// VkAccessFlags2KHR				srcAccessMask
				readSync.stageMask,								// VkPipelineStageFlags2KHR			dstStageMask
				readSync.accessMask,							// VkAccessFlags2KHR				dstAccessMask
				resource.getBuffer().handle,					// VkBuffer							buffer
				0,												// VkDeviceSize						offset
				VK_WHOLE_SIZE,									// VkDeviceSize						size
				inIter.queueFamilyIdx,							// deUint32							srcQueueFamilyIndex
				outIter.queueFamilyIdx							// deUint32							dstQueueFamilyIndex
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, &bufferMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
		}
	}

	void submit (const DeviceInterface&	vk, VkCommandBuffer cmdBuffer, const QueueTimelineIteration& iter, VkSemaphore semaphore, const deUint64 *waitValues, const deUint32 waitValuesCount)
	{
		VkSemaphoreSubmitInfoKHR		waitSemaphoreSubmitInfo[] =
		{
			makeCommonSemaphoreSubmitInfo(semaphore, waitValues[0], VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR),
			makeCommonSemaphoreSubmitInfo(semaphore, waitValues[1], VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR)
		};
		VkSemaphoreSubmitInfoKHR		signalSemaphoreSubmitInfo =
			makeCommonSemaphoreSubmitInfo(semaphore, iter.timelineValue, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR);

		VkCommandBufferSubmitInfoKHR	commandBufferSubmitInfo	= makeCommonCommandBufferSubmitInfo(cmdBuffer);
		SynchronizationWrapperPtr		synchronizationWrapper	= getSynchronizationWrapper(m_type, vk, DE_TRUE);

		synchronizationWrapper->addSubmitInfo(
			waitValuesCount,						// deUint32								waitSemaphoreInfoCount
			waitSemaphoreSubmitInfo,				// const VkSemaphoreSubmitInfoKHR*		pWaitSemaphoreInfos
			1u,										// deUint32								commandBufferInfoCount
			&commandBufferSubmitInfo,				// const VkCommandBufferSubmitInfoKHR*	pCommandBufferInfos
			1u,										// deUint32								signalSemaphoreInfoCount
			&signalSemaphoreSubmitInfo,				// const VkSemaphoreSubmitInfoKHR*		pSignalSemaphoreInfos
			DE_TRUE,
			DE_TRUE
		);

		VK_CHECK(synchronizationWrapper->queueSubmit(iter.queue, DE_NULL));
	}

	tcu::TestStatus	iterate (void)
	{
		const DeviceInterface&								vk						= *m_deviceDriver;
		const VkDevice										device					= *m_device;
		const Unique<VkSemaphore>							semaphore				(createSemaphoreType(vk, device, VK_SEMAPHORE_TYPE_TIMELINE));
		Unique<VkCommandPool>								writeCmdPool			(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
																									   m_context.getUniversalQueueFamilyIndex()));
		Unique<VkCommandBuffer>								writeCmdBuffer			(makeCommandBuffer(vk, device, *writeCmdPool));
		std::vector<SharedPtr<Move<VkCommandPool> > >		copyCmdPools;
		std::vector<SharedPtr<Move<VkCommandBuffer> > >		copyPtrCmdBuffers;
		std::vector<SharedPtr<Move<VkCommandPool> > >		readCmdPools;
		std::vector<SharedPtr<Move<VkCommandBuffer> > >		readPtrCmdBuffers;

		for (deUint32 copyOpNdx = 0; copyOpNdx < m_copyIterations.size(); copyOpNdx++)
		{
			copyCmdPools.push_back(makeVkSharedPtr(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
																 m_copyIterations[copyOpNdx]->queueFamilyIdx)));
			copyPtrCmdBuffers.push_back(makeVkSharedPtr(makeCommandBuffer(vk, device, **copyCmdPools.back())));

			readCmdPools.push_back(makeVkSharedPtr(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
																 m_readIterations[copyOpNdx]->queueFamilyIdx)));
			readPtrCmdBuffers.push_back(makeVkSharedPtr(makeCommandBuffer(vk, device, **readCmdPools.back())));
		}

		// Randomize the data copied over.
		{
			const Data				startData		= m_writeIteration->op->getData();
			Data					randomizedData;
			std::vector<deUint8>	dataArray;

			dataArray.resize(startData.size);
			randomizeData(dataArray, m_resourceDesc);
			randomizedData.size = dataArray.size();
			randomizedData.data = &dataArray[0];
			m_writeIteration->op->setData(randomizedData);
		}

		// Record command buffers
		{
			beginCommandBuffer(vk, *writeCmdBuffer);
			m_writeIteration->op->recordCommands(*writeCmdBuffer);
			endCommandBuffer(vk, *writeCmdBuffer);

			for (deUint32 copyOpIdx = 0; copyOpIdx < m_copyIterations.size(); copyOpIdx++)
			{
				beginCommandBuffer(vk, **copyPtrCmdBuffers[copyOpIdx]);
				recordBarrier(vk, **copyPtrCmdBuffers[copyOpIdx], *m_writeIteration, *m_copyIterations[copyOpIdx], *m_writeResource, copyOpIdx == 0);
				m_copyIterations[copyOpIdx]->op->recordCommands(**copyPtrCmdBuffers[copyOpIdx]);
				endCommandBuffer(vk, **copyPtrCmdBuffers[copyOpIdx]);
			}

			for (deUint32 readOpIdx = 0; readOpIdx < m_readIterations.size(); readOpIdx++)
			{
				beginCommandBuffer(vk, **readPtrCmdBuffers[readOpIdx]);
				recordBarrier(vk, **readPtrCmdBuffers[readOpIdx], *m_copyIterations[readOpIdx], *m_readIterations[readOpIdx], *m_copyResources[readOpIdx], true);
				m_readIterations[readOpIdx]->op->recordCommands(**readPtrCmdBuffers[readOpIdx]);
				endCommandBuffer(vk, **readPtrCmdBuffers[readOpIdx]);
			}
		}

		// Submit
		{
			submit(vk, *writeCmdBuffer, *m_writeIteration, *semaphore, &m_hostTimelineValue, 1);
			for (deUint32 copyOpIdx = 0; copyOpIdx < m_copyIterations.size(); copyOpIdx++)
			{
				deUint64 waitValues[2] =
				{
					m_writeIteration->timelineValue,
					copyOpIdx > 0 ? m_copyIterations[copyOpIdx - 1]->timelineValue : 0,
				};

				submit(vk, **copyPtrCmdBuffers[copyOpIdx], *m_copyIterations[copyOpIdx],
					   *semaphore, waitValues, copyOpIdx > 0 ? 2 : 1);
			}
			for (deUint32 readOpIdx = 0; readOpIdx < m_readIterations.size(); readOpIdx++)
			{
				deUint64 waitValues[2] =
				{
					m_copyIterations[readOpIdx]->timelineValue,
					readOpIdx > 0 ? m_readIterations[readOpIdx - 1]->timelineValue : m_copyIterations.back()->timelineValue,
				};

				submit(vk, **readPtrCmdBuffers[readOpIdx], *m_readIterations[readOpIdx],
					   *semaphore, waitValues, 2);
			}

			// Kick off the whole chain from the host.
			hostSignal(vk, device, *semaphore, m_hostTimelineValue);
			VK_CHECK(vk.deviceWaitIdle(device));
		}

		{
			const Data	expected = m_writeIteration->op->getData();

			for (deUint32 readOpIdx = 0; readOpIdx < m_readIterations.size(); readOpIdx++)
			{
				const Data	actual	 = m_readIterations[readOpIdx]->op->getData();

				if (0 != deMemCmp(expected.data, actual.data, expected.size))
					return tcu::TestStatus::fail("Memory contents don't match");
			}
		}

		return tcu::TestStatus::pass("OK");
	}

protected:
	SynchronizationType								m_type;
	ResourceDescription								m_resourceDesc;
	const Unique<VkDevice>&							m_device;
	const Context&									m_context;
#ifndef CTS_USES_VULKANSC
	de::MovePtr<vk::DeviceDriver>					m_deviceDriver;
#else
	de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter>	m_deviceDriver;
#endif // CTS_USES_VULKANSC
	MovePtr<Allocator>								m_allocator;
	OperationContext								m_opContext;
	SharedPtr<QueueTimelineIteration>				m_writeIteration;
	std::vector<SharedPtr<QueueTimelineIteration> >	m_copyIterations;
	std::vector<SharedPtr<QueueTimelineIteration> >	m_readIterations;
	SharedPtr<Resource>								m_writeResource;
	std::vector<SharedPtr<Resource> >				m_copyResources;
	deUint64										m_hostTimelineValue;
};

class OneToNTestCase : public TestCase
{
public:
	OneToNTestCase	(tcu::TestContext&			testCtx,
					 const std::string&			name,
					 const std::string&			description,
					 SynchronizationType		type,
					 const ResourceDescription	resourceDesc,
					 const OperationName		writeOp,
					 const OperationName		readOp,
					 PipelineCacheData&			pipelineCacheData)
		: TestCase				(testCtx, name, description)
		, m_type				(type)
		, m_resourceDesc		(resourceDesc)
		, m_writeOp				(makeOperationSupport(writeOp, resourceDesc).release())
		, m_readOp				(makeOperationSupport(readOp, resourceDesc).release())
		, m_pipelineCacheData	(pipelineCacheData)
	{
	}

	void checkSupport(Context& context) const override
	{
		context.requireDeviceFunctionality("VK_KHR_timeline_semaphore");
		if (m_type == SynchronizationType::SYNCHRONIZATION2)
			context.requireDeviceFunctionality("VK_KHR_synchronization2");
	}

	void initPrograms (SourceCollections& programCollection) const override
	{
		m_writeOp->initPrograms(programCollection);
		m_readOp->initPrograms(programCollection);

		for (deUint32 copyOpNdx = 0; copyOpNdx < DE_LENGTH_OF_ARRAY(s_copyOps); copyOpNdx++)
		{
			if (isResourceSupported(s_copyOps[copyOpNdx], m_resourceDesc))
				makeOperationSupport(s_copyOps[copyOpNdx], m_resourceDesc)->initPrograms(programCollection);
		}
	}

	TestInstance* createInstance (Context& context) const override
	{
		return new OneToNTestInstance(context, m_type, m_resourceDesc, m_writeOp, m_readOp, m_pipelineCacheData);
	}

private:
	SynchronizationType					m_type;
	const ResourceDescription			m_resourceDesc;
	const SharedPtr<OperationSupport>	m_writeOp;
	const SharedPtr<OperationSupport>	m_readOp;
	PipelineCacheData&					m_pipelineCacheData;
};

class OneToNTests : public tcu::TestCaseGroup
{
public:
	OneToNTests (tcu::TestContext& testCtx, SynchronizationType type)
		: tcu::TestCaseGroup(testCtx, "one_to_n", "Synchronization multiple waiter on a signal producer")
		, m_type(type)
	{
	}

	void init (void)
	{
		static const OperationName		writeOps[]	=
		{
			OPERATION_NAME_WRITE_COPY_BUFFER,
			OPERATION_NAME_WRITE_COPY_BUFFER_TO_IMAGE,
			OPERATION_NAME_WRITE_COPY_IMAGE_TO_BUFFER,
			OPERATION_NAME_WRITE_COPY_IMAGE,
			OPERATION_NAME_WRITE_BLIT_IMAGE,
			OPERATION_NAME_WRITE_SSBO_VERTEX,
			OPERATION_NAME_WRITE_SSBO_TESSELLATION_CONTROL,
			OPERATION_NAME_WRITE_SSBO_TESSELLATION_EVALUATION,
			OPERATION_NAME_WRITE_SSBO_GEOMETRY,
			OPERATION_NAME_WRITE_SSBO_FRAGMENT,
			OPERATION_NAME_WRITE_SSBO_COMPUTE,
			OPERATION_NAME_WRITE_SSBO_COMPUTE_INDIRECT,
			OPERATION_NAME_WRITE_IMAGE_VERTEX,
			OPERATION_NAME_WRITE_IMAGE_TESSELLATION_CONTROL,
			OPERATION_NAME_WRITE_IMAGE_TESSELLATION_EVALUATION,
			OPERATION_NAME_WRITE_IMAGE_GEOMETRY,
			OPERATION_NAME_WRITE_IMAGE_FRAGMENT,
			OPERATION_NAME_WRITE_IMAGE_COMPUTE,
			OPERATION_NAME_WRITE_IMAGE_COMPUTE_INDIRECT,
		};
		static const OperationName		readOps[]	=
		{
			OPERATION_NAME_READ_COPY_BUFFER,
			OPERATION_NAME_READ_COPY_BUFFER_TO_IMAGE,
			OPERATION_NAME_READ_COPY_IMAGE_TO_BUFFER,
			OPERATION_NAME_READ_COPY_IMAGE,
			OPERATION_NAME_READ_BLIT_IMAGE,
			OPERATION_NAME_READ_UBO_VERTEX,
			OPERATION_NAME_READ_UBO_TESSELLATION_CONTROL,
			OPERATION_NAME_READ_UBO_TESSELLATION_EVALUATION,
			OPERATION_NAME_READ_UBO_GEOMETRY,
			OPERATION_NAME_READ_UBO_FRAGMENT,
			OPERATION_NAME_READ_UBO_COMPUTE,
			OPERATION_NAME_READ_UBO_COMPUTE_INDIRECT,
			OPERATION_NAME_READ_SSBO_VERTEX,
			OPERATION_NAME_READ_SSBO_TESSELLATION_CONTROL,
			OPERATION_NAME_READ_SSBO_TESSELLATION_EVALUATION,
			OPERATION_NAME_READ_SSBO_GEOMETRY,
			OPERATION_NAME_READ_SSBO_FRAGMENT,
			OPERATION_NAME_READ_SSBO_COMPUTE,
			OPERATION_NAME_READ_SSBO_COMPUTE_INDIRECT,
			OPERATION_NAME_READ_IMAGE_VERTEX,
			OPERATION_NAME_READ_IMAGE_TESSELLATION_CONTROL,
			OPERATION_NAME_READ_IMAGE_TESSELLATION_EVALUATION,
			OPERATION_NAME_READ_IMAGE_GEOMETRY,
			OPERATION_NAME_READ_IMAGE_FRAGMENT,
			OPERATION_NAME_READ_IMAGE_COMPUTE,
			OPERATION_NAME_READ_IMAGE_COMPUTE_INDIRECT,
			OPERATION_NAME_READ_INDIRECT_BUFFER_DRAW,
			OPERATION_NAME_READ_INDIRECT_BUFFER_DRAW_INDEXED,
			OPERATION_NAME_READ_INDIRECT_BUFFER_DISPATCH,
			OPERATION_NAME_READ_VERTEX_INPUT,
		};

		for (int writeOpNdx = 0; writeOpNdx < DE_LENGTH_OF_ARRAY(writeOps); ++writeOpNdx)
		for (int readOpNdx = 0; readOpNdx < DE_LENGTH_OF_ARRAY(readOps); ++readOpNdx)
		{
			const OperationName	writeOp		= writeOps[writeOpNdx];
			const OperationName	readOp		= readOps[readOpNdx];
			const std::string	opGroupName = getOperationName(writeOp) + "_" + getOperationName(readOp);
			bool				empty		= true;

			de::MovePtr<tcu::TestCaseGroup> opGroup	(new tcu::TestCaseGroup(m_testCtx, opGroupName.c_str(), ""));

			for (int resourceNdx = 0; resourceNdx < DE_LENGTH_OF_ARRAY(s_resources); ++resourceNdx)
			{
				const ResourceDescription&	resource	= s_resources[resourceNdx];
				std::string					name		= getResourceName(resource);

				if (isResourceSupported(writeOp, resource) && isResourceSupported(readOp, resource))
				{
					opGroup->addChild(new OneToNTestCase(m_testCtx, name, "", m_type, resource, writeOp, readOp, m_pipelineCacheData));
					empty = false;
				}
			}
			if (!empty)
				addChild(opGroup.release());
		}
	}

	void deinit (void)
	{
		cleanupGroup();
	}

private:
	SynchronizationType	m_type;

	// synchronization.op tests share pipeline cache data to speed up test
	// execution.
	PipelineCacheData	m_pipelineCacheData;
};

#ifndef CTS_USES_VULKANSC

// Make a nonzero initial value for a semaphore. semId is assigned to each semaphore by callers.
deUint64 getInitialValue(deUint32 semId)
{
	return (semId + 1ull) * 1000ull;
}

struct SparseBindParams
{
	deUint32 numWaitSems;
	deUint32 numSignalSems;
};

class SparseBindCase : public vkt::TestCase
{
public:
							SparseBindCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const SparseBindParams& params);
	virtual					~SparseBindCase	(void) {}

	virtual TestInstance*	createInstance	(Context& context) const;
	virtual void			checkSupport	(Context& context) const;

private:
	SparseBindParams m_params;
};

class SparseBindInstance : public vkt::TestInstance
{
public:
								SparseBindInstance	(Context& context, const SparseBindParams& params);
	virtual						~SparseBindInstance	(void) {}

	virtual tcu::TestStatus		iterate				(void);

private:
	SparseBindParams m_params;
};

SparseBindCase::SparseBindCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const SparseBindParams& params)
	: vkt::TestCase	(testCtx, name, description)
	, m_params		(params)
{}

TestInstance* SparseBindCase::createInstance (Context& context) const
{
	return new SparseBindInstance(context, m_params);
}

void SparseBindCase::checkSupport (Context& context) const
{
	// Check support for sparse binding and timeline semaphores.
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_BINDING);
	context.requireDeviceFunctionality("VK_KHR_timeline_semaphore");
}

SparseBindInstance::SparseBindInstance (Context& context, const SparseBindParams& params)
	: vkt::TestInstance	(context)
	, m_params			(params)
{
}

void queueBindSparse (const vk::DeviceInterface& vkd, vk::VkQueue queue, deUint32 bindInfoCount, const vk::VkBindSparseInfo *pBindInfo)
{
	VK_CHECK(vkd.queueBindSparse(queue, bindInfoCount, pBindInfo, DE_NULL));
}

#endif // CTS_USES_VULKANSC

struct SemaphoreWithInitial
{
	vk::Move<vk::VkSemaphore>	semaphore;
	deUint64					initialValue;

	SemaphoreWithInitial (vk::Move<vk::VkSemaphore>&& sem, deUint64 initVal)
		: semaphore		(sem)
		, initialValue	(initVal)
	{}

	SemaphoreWithInitial (SemaphoreWithInitial&& other)
		: semaphore		(other.semaphore)
		, initialValue	(other.initialValue)
	{}
};

using SemaphoreVec	= std::vector<SemaphoreWithInitial>;
using PlainSemVec	= std::vector<vk::VkSemaphore>;
using ValuesVec		= std::vector<deUint64>;

#ifndef CTS_USES_VULKANSC

PlainSemVec getHandles (const SemaphoreVec& semVec)
{
	PlainSemVec handlesVec;
	handlesVec.reserve(semVec.size());

	const auto conversion = [](const SemaphoreWithInitial& s) { return s.semaphore.get(); };
	std::transform(begin(semVec), end(semVec), std::back_inserter(handlesVec), conversion);

	return handlesVec;
}

ValuesVec getInitialValues (const SemaphoreVec& semVec)
{
	ValuesVec initialValues;
	initialValues.reserve(semVec.size());

	const auto conversion = [](const SemaphoreWithInitial& s) { return s.initialValue; };
	std::transform(begin(semVec), end(semVec), std::back_inserter(initialValues), conversion);

	return initialValues;
}

// Increases values in the vector by one.
ValuesVec getNextValues (const ValuesVec& values)
{
	ValuesVec nextValues;
	nextValues.reserve(values.size());

	std::transform(begin(values), end(values), std::back_inserter(nextValues), [](deUint64 v) { return v + 1ull; });
	return nextValues;
}

SemaphoreWithInitial createTimelineSemaphore (const vk::DeviceInterface& vkd, vk::VkDevice device, deUint32 semId)
{
	const auto initialValue = getInitialValue(semId);
	return SemaphoreWithInitial(createSemaphoreType(vkd, device, vk::VK_SEMAPHORE_TYPE_TIMELINE, 0u, initialValue), initialValue);
}

// Signal the given semaphores with the corresponding values.
void hostSignal (const vk::DeviceInterface& vkd, vk::VkDevice device, const PlainSemVec& semaphores, const ValuesVec& signalValues)
{
	DE_ASSERT(semaphores.size() == signalValues.size());

	for (size_t i = 0; i < semaphores.size(); ++i)
		hostSignal(vkd, device, semaphores[i], signalValues[i]);
}

// Wait for the given semaphores and their corresponding values.
void hostWait (const vk::DeviceInterface& vkd, vk::VkDevice device, const PlainSemVec& semaphores, const ValuesVec& waitValues)
{
	DE_ASSERT(semaphores.size() == waitValues.size() && !semaphores.empty());

	constexpr deUint64 kTimeout = 10000000000ull; // 10 seconds in nanoseconds.

	const vk::VkSemaphoreWaitInfo waitInfo =
	{
		vk::VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,	//	VkStructureType			sType;
		nullptr,									//	const void*				pNext;
		0u,											//	VkSemaphoreWaitFlags	flags;
		static_cast<deUint32>(semaphores.size()),	//	deUint32				semaphoreCount;
		semaphores.data(),							//	const VkSemaphore*		pSemaphores;
		waitValues.data(),							//	const deUint64*			pValues;
	};
	VK_CHECK(vkd.waitSemaphores(device, &waitInfo, kTimeout));
}

tcu::TestStatus SparseBindInstance::iterate (void)
{
	const auto&	vkd		= m_context.getDeviceInterface();
	const auto	device	= m_context.getDevice();
	const auto	queue	= m_context.getSparseQueue();

	SemaphoreVec waitSemaphores;
	SemaphoreVec signalSemaphores;

	// Create as many semaphores as needed to wait and signal.
	for (deUint32 i = 0; i < m_params.numWaitSems; ++i)
		waitSemaphores.emplace_back(createTimelineSemaphore(vkd, device, i));
	for (deUint32 i = 0; i < m_params.numSignalSems; ++i)
		signalSemaphores.emplace_back(createTimelineSemaphore(vkd, device, i + m_params.numWaitSems));

	// Get handles for all semaphores.
	const auto waitSemHandles	= getHandles(waitSemaphores);
	const auto signalSemHandles	= getHandles(signalSemaphores);

	// Get initial values for all semaphores.
	const auto waitSemValues	= getInitialValues(waitSemaphores);
	const auto signalSemValues	= getInitialValues(signalSemaphores);

	// Get next expected values for all semaphores.
	const auto waitNextValues	= getNextValues(waitSemValues);
	const auto signalNextValues	= getNextValues(signalSemValues);

	const vk::VkTimelineSemaphoreSubmitInfo timeLineSubmitInfo =
	{
		vk::VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,			//	VkStructureType	sType;
		nullptr,														//	const void*		pNext;
		static_cast<deUint32>(waitNextValues.size()),					//	deUint32		waitSemaphoreValueCount;
		(waitNextValues.empty() ? nullptr : waitNextValues.data()),		//	const deUint64*	pWaitSemaphoreValues;
		static_cast<deUint32>(signalNextValues.size()),					//	deUint32		signalSemaphoreValueCount;
		(signalNextValues.empty() ? nullptr : signalNextValues.data()),	//	const deUint64*	pSignalSemaphoreValues;
	};

	const vk::VkBindSparseInfo bindInfo =
	{
		vk::VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,							//	VkStructureType								sType;
		&timeLineSubmitInfo,											//	const void*									pNext;
		static_cast<deUint32>(waitSemHandles.size()),					//	deUint32									waitSemaphoreCount;
		(waitSemHandles.empty() ? nullptr : waitSemHandles.data()),		//	const VkSemaphore*							pWaitSemaphores;
		0u,																//	deUint32									bufferBindCount;
		nullptr,														//	const VkSparseBufferMemoryBindInfo*			pBufferBinds;
		0u,																//	deUint32									imageOpaqueBindCount;
		nullptr,														//	const VkSparseImageOpaqueMemoryBindInfo*	pImageOpaqueBinds;
		0u,																//	deUint32									imageBindCount;
		nullptr,														//	const VkSparseImageMemoryBindInfo*			pImageBinds;
		static_cast<deUint32>(signalSemHandles.size()),					//	deUint32									signalSemaphoreCount;
		(signalSemHandles.empty() ? nullptr : signalSemHandles.data()),	//	const VkSemaphore*							pSignalSemaphores;
	};
	queueBindSparse(vkd, queue, 1u, &bindInfo);

	// If the device needs to wait and signal, check the signal semaphores have not been signaled yet.
	if (!waitSemaphores.empty() && !signalSemaphores.empty())
	{
		deUint64 value;
		for (size_t i = 0; i < signalSemaphores.size(); ++i)
		{
			value = 0;
			VK_CHECK(vkd.getSemaphoreCounterValue(device, signalSemHandles[i], &value));

			if (!value)
				TCU_FAIL("Invalid value obtained from vkGetSemaphoreCounterValue()");

			if (value != signalSemValues[i])
			{
				std::ostringstream msg;
				msg << "vkQueueBindSparse() may not have waited before signaling semaphore " << i
					<< " (expected value " << signalSemValues[i] << " but obtained " << value << ")";
				TCU_FAIL(msg.str());
			}
		}
	}

	// Signal semaphores the sparse bind command is waiting on.
	hostSignal(vkd, device, waitSemHandles, waitNextValues);

	// Wait for semaphores the sparse bind command is supposed to signal.
	if (!signalSemaphores.empty())
		hostWait(vkd, device, signalSemHandles, signalNextValues);

	VK_CHECK(vkd.deviceWaitIdle(device));
	return tcu::TestStatus::pass("Pass");
}

class SparseBindGroup : public tcu::TestCaseGroup
{
public:
	SparseBindGroup (tcu::TestContext& testCtx)
		: tcu::TestCaseGroup (testCtx, "sparse_bind", "vkQueueBindSparse combined with timeline semaphores")
	{}

	virtual void init (void)
	{
		static const struct
		{
			deUint32	waitSems;
			deUint32	sigSems;
			std::string	name;
			std::string	desc;
		} kSparseBindCases[] =
		{
			{	0u,		0u,		"no_sems",			"No semaphores to wait for or signal"					},
			{	0u,		1u,		"no_wait_sig",		"Signal semaphore without waiting for any other"		},
			{	1u,		0u,		"wait_no_sig",		"Wait for semaphore but do not signal any other"		},
			{	1u,		1u,		"wait_and_sig",		"Wait for semaphore and signal a second one"			},
			{	2u,		2u,		"wait_and_sig_2",	"Wait for two semaphores and signal two other ones"		},
		};

		for (int i = 0; i < DE_LENGTH_OF_ARRAY(kSparseBindCases); ++i)
			addChild(new SparseBindCase(m_testCtx, kSparseBindCases[i].name, kSparseBindCases[i].desc, SparseBindParams{kSparseBindCases[i].waitSems, kSparseBindCases[i].sigSems}));
	}
};

#endif // CTS_USES_VULKANSC

} // anonymous

tcu::TestCaseGroup* createTimelineSemaphoreTests (tcu::TestContext& testCtx)
{
	const SynchronizationType			type		(SynchronizationType::LEGACY);
	de::MovePtr<tcu::TestCaseGroup>		basicTests	(new tcu::TestCaseGroup(testCtx, "timeline_semaphore", "Timeline semaphore tests"));

	basicTests->addChild(new LegacyDeviceHostTests(testCtx));
	basicTests->addChild(new OneToNTests(testCtx, type));
	basicTests->addChild(new WaitBeforeSignalTests(testCtx, type));
	basicTests->addChild(new WaitTests(testCtx, type));
#ifndef CTS_USES_VULKANSC
	basicTests->addChild(new SparseBindGroup(testCtx));
#endif // CTS_USES_VULKANSC

	return basicTests.release();
}

tcu::TestCaseGroup* createSynchronization2TimelineSemaphoreTests(tcu::TestContext& testCtx)
{
	const SynchronizationType			type		(SynchronizationType::SYNCHRONIZATION2);
	de::MovePtr<tcu::TestCaseGroup>		basicTests	(new tcu::TestCaseGroup(testCtx, "timeline_semaphore", "Timeline semaphore tests"));

	basicTests->addChild(new Sytnchronization2DeviceHostTests(testCtx));
	basicTests->addChild(new OneToNTests(testCtx, type));
	basicTests->addChild(new WaitBeforeSignalTests(testCtx, type));
	basicTests->addChild(new WaitTests(testCtx, type));

	return basicTests.release();
}

} // synchronization
} // vkt
