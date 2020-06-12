
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
#include "vkBarrierUtil.hpp"

#include "vkDefs.hpp"
#include "vkPlatform.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkRef.hpp"
#include "vkTypeUtil.hpp"
#include "vkBufferWithMemory.hpp"

#include "tcuTestLog.hpp"

#include "deRandom.hpp"
#include "deThread.hpp"
#include "deUniquePtr.hpp"

#include <limits>
#include <set>

namespace vkt
{
namespace synchronization
{
namespace
{

using namespace vk;
using namespace vkt::ExternalMemoryUtil;
using tcu::TestLog;
using de::MovePtr;
using de::SharedPtr;
using de::UniquePtr;

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

void deviceSignal (const DeviceInterface&	vk,
				   const VkDevice			device,
				   const VkQueue			queue,
				   const VkFence			fence,
				   const VkSemaphore		semaphore,
				   const deUint64			timelineValue)
{
	VkTimelineSemaphoreSubmitInfo		tsi			=
	{
		VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,		// VkStructureType				sType;
		DE_NULL,												// const void*					pNext;
		0u,														// deUint32						waitSemaphoreValueCount
		DE_NULL,												// const deUint64*				pWaitSemaphoreValues
		1u,														// deUint32						signalSemaphoreValueCount
		&timelineValue,											// const deUint64*				pSignalSemaphoreValues
	};
	VkSubmitInfo						si[2]		=
	{
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,						// VkStructureType				sType;
			&tsi,												// const void*					pNext;
			0,													// deUint32						waitSemaphoreCount;
			DE_NULL,											// const VkSemaphore*			pWaitSemaphores;
			DE_NULL,											// const VkPipelineStageFlags*	pWaitDstStageMask;
			0,													// deUint32						commandBufferCount;
			DE_NULL,											// const VkCommandBuffer*		pCommandBuffers;
			1,													// deUint32						signalSemaphoreCount;
			&semaphore,											// const VkSemaphore*			pSignalSemaphores;
		},
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,						// VkStructureType				sType;
			&tsi,												// const void*					pNext;
			0,													// deUint32						waitSemaphoreCount;
			DE_NULL,											// const VkSemaphore*			pWaitSemaphores;
			DE_NULL,											// const VkPipelineStageFlags*	pWaitDstStageMask;
			0,													// deUint32						commandBufferCount;
			DE_NULL,											// const VkCommandBuffer*		pCommandBuffers;
			0,													// deUint32						signalSemaphoreCount;
			DE_NULL,											// const VkSemaphore*			pSignalSemaphores;
		}
	};

	VK_CHECK(vk.queueSubmit(queue, 1u, &si[0], DE_NULL));
	if (fence != DE_NULL) {
		VK_CHECK(vk.queueSubmit(queue, 1u, &si[1], fence));
		VK_CHECK(vk.waitForFences(device, 1u, &fence, VK_TRUE, ~(0ull)));
	}
}

void hostSignal (const DeviceInterface& vk, const VkDevice& device, VkSemaphore semaphore, const deUint64 timelineValue)
{
	VkSemaphoreSignalInfoKHR	ssi	=
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
	WaitTestInstance (Context& context, bool waitAll, bool signalFromDevice)
		: TestInstance			(context)
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
					deviceSignal(vk, device, queue, *fence, semaphores[semIdx], timelineValues[semIdx]);
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
				deviceSignal(vk, device, queue, *fence, semaphores[randomIdx], timelineValues[randomIdx]);
			else
				hostSignal(vk, device, semaphores[randomIdx], timelineValues[randomIdx]);
		}

		{
			const VkSemaphoreWaitInfo		waitInfo	=
			{
				VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,									// VkStructureType			sType;
				DE_NULL,																// const void*				pNext;
				m_waitAll ? 0u : (VkSemaphoreWaitFlags) VK_SEMAPHORE_WAIT_ANY_BIT_KHR,	// VkSemaphoreWaitFlagsKHR	flags;
				(deUint32) semaphores.size(),											// deUint32					semaphoreCount;
				&semaphores[0],															// const VkSemaphore*		pSemaphores;
				&timelineValues[0],														// const deUint64*			pValues;
			};
			VkResult						result;

			result = vk.waitSemaphores(device, &waitInfo, 0ull);

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
			semaphores.push_back(makeVkSharedPtr(createSemaphoreType(vk, device, VK_SEMAPHORE_TYPE_TIMELINE_KHR)));

		return semaphores;
	}

	bool m_waitAll;
	bool m_signalFromDevice;
};

class WaitTestCase : public TestCase
{
public:
	WaitTestCase (tcu::TestContext& testCtx, const std::string& name, bool waitAll, bool signalFromDevice)
		: TestCase				(testCtx, name.c_str(), "")
		, m_waitAll				(waitAll)
		, m_signalFromDevice	(signalFromDevice)
	{
	}

	virtual void checkSupport(Context& context) const
	{
		context.requireDeviceFunctionality("VK_KHR_timeline_semaphore");
	}

	TestInstance* createInstance (Context& context) const
	{
		return new WaitTestInstance(context, m_waitAll, m_signalFromDevice);
	}

private:
	bool m_waitAll;
	bool m_signalFromDevice;
};

// This test verifies that waiting from the host on a timeline point
// that is itself waiting for signaling works properly.
class HostWaitBeforeSignalTestInstance : public TestInstance
{
public:
	HostWaitBeforeSignalTestInstance (Context& context)
		: TestInstance			(context)
	{
	}

	tcu::TestStatus iterate (void)
	{
		const DeviceInterface&	vk					= m_context.getDeviceInterface();
		const VkDevice&			device				= m_context.getDevice();
		const VkQueue			queue				= m_context.getUniversalQueue();
		Unique<VkSemaphore>		semaphore			(createSemaphoreType(vk, device, VK_SEMAPHORE_TYPE_TIMELINE_KHR));
		de::Random				rng					(1234);
		std::vector<deUint64>	timelineValues;

		// Host value we signal at the end.
		timelineValues.push_back(1 + rng.getInt(1, 10000));

		for (deUint32 i = 0; i < 12; i++)
		{
			const deUint64							newTimelineValue	= (timelineValues.back() + rng.getInt(1, 10000));
			const VkTimelineSemaphoreSubmitInfo		timelineSubmitInfo	=
			{
				VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,		// VkStructureType	sType;
				DE_NULL,												// const void*		pNext;
				1u,														// deUint32			waitSemaphoreValueCount
				&timelineValues.back(),									// const deUint64*	pWaitSemaphoreValues
				1u,														// deUint32			signalSemaphoreValueCount
				&newTimelineValue,										// const deUint64*	pSignalSemaphoreValues
			};
			const VkPipelineStageFlags				stageBits[]			= { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };
			const VkSubmitInfo						submitInfo			=
			{
				VK_STRUCTURE_TYPE_SUBMIT_INFO,							// VkStructureType			sType;
				&timelineSubmitInfo,									// const void*				pNext;
				1u,														// deUint32					waitSemaphoreCount;
				&semaphore.get(),										// const VkSemaphore*		pWaitSemaphores;
				stageBits,
				0u,														// deUint32					commandBufferCount;
				DE_NULL,												// const VkCommandBuffer*	pCommandBuffers;
				1u,														// deUint32					signalSemaphoreCount;
				&semaphore.get(),										// const VkSemaphore*		pSignalSemaphores;
			};

			VK_CHECK(vk.queueSubmit(queue, (deUint32) 1u, &submitInfo, DE_NULL));

			timelineValues.push_back(newTimelineValue);
		}

		{
			const VkSemaphoreWaitInfoKHR	waitInfo	=
			{
				VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR,										// VkStructureType			sType;
				DE_NULL,																		// const void*				pNext;
				0u,																				// VkSemaphoreWaitFlagsKHR	flags;
				(deUint32) 1u,																	// deUint32					semaphoreCount;
				&semaphore.get(),																// const VkSemaphore*		pSemaphores;
				&timelineValues[rng.getInt(0, static_cast<int>(timelineValues.size() - 1))],	// const deUint64*			pValues;
			};
			VkResult						result;

			result = vk.waitSemaphores(device, &waitInfo, 0ull);

			if (result != VK_TIMEOUT)
				return tcu::TestStatus::fail("Wait failed");
		}

		hostSignal(vk, device, *semaphore, timelineValues.front());

		{
			const VkSemaphoreWaitInfoKHR	waitInfo	=
			{
				VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR,	// VkStructureType			sType;
				DE_NULL,									// const void*				pNext;
				0u,											// VkSemaphoreWaitFlagsKHR	flags;
				(deUint32) 1u,								// deUint32					semaphoreCount;
				&semaphore.get(),							// const VkSemaphore*		pSemaphores;
				&timelineValues.back(),						// const deUint64*			pValues;
			};
			VkResult						result;

			result = vk.waitSemaphores(device, &waitInfo, ~(0ull));

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
			semaphores.push_back(makeVkSharedPtr(createSemaphoreType(vk, device, VK_SEMAPHORE_TYPE_TIMELINE_KHR)));

		return semaphores;
	}
};

class HostWaitBeforeSignalTestCase : public TestCase
{
public:
	HostWaitBeforeSignalTestCase (tcu::TestContext& testCtx, const std::string& name)
		: TestCase				(testCtx, name.c_str(), "")
	{
	}

	virtual void checkSupport(Context& context) const
	{
		context.requireDeviceFunctionality("VK_KHR_timeline_semaphore");
	}

	TestInstance* createInstance (Context& context) const
	{
		return new HostWaitBeforeSignalTestInstance(context);
	}
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

void checkTimelineSupport (Context& context)
{
	if (!context.getTimelineSemaphoreFeatures().timelineSemaphore)
		TCU_THROW(NotSupportedError, "Timeline semaphore not supported");
}

// Queue device signaling close to the edges of the
// maxTimelineSemaphoreValueDifference value and verify that the value
// of the semaphore never goes backwards.
tcu::TestStatus maxDifferenceValueCase (Context& context)
{
	const DeviceInterface&							vk							= context.getDeviceInterface();
	const VkDevice&									device						= context.getDevice();
	const VkQueue									queue						= context.getUniversalQueue();
	const deUint64									requiredMinValueDifference	= deIntMaxValue32(32);
	const deUint64									maxTimelineValueDifference	= getMaxTimelineSemaphoreValueDifference(context.getInstanceInterface(), context.getPhysicalDevice());
	const Unique<VkSemaphore>						semaphore					(createSemaphoreType(vk, device, VK_SEMAPHORE_TYPE_TIMELINE_KHR));
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
			deviceSignal(vk, device, queue, DE_NULL, *semaphore, ++timelineFrontValue);

		timelineFrontValue = timelineBackValue + maxTimelineValueDifference - 10;
		fenceValue = timelineFrontValue;
		deviceSignal(vk, device, queue, *fence, *semaphore, fenceValue);
		for (deUint32 j = 1; j < 10; j++)
			deviceSignal(vk, device, queue, DE_NULL, *semaphore, ++timelineFrontValue);

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

tcu::TestStatus initialValueCase (Context& context)
{
	const DeviceInterface&							vk							= context.getDeviceInterface();
	const VkDevice&									device						= context.getDevice();
	const deUint64									maxTimelineValueDifference	= getMaxTimelineSemaphoreValueDifference(context.getInstanceInterface(), context.getPhysicalDevice());
	de::Random										rng							(1234);
	const deUint64									nonZeroValue				= 1 + rng.getUint64() % (maxTimelineValueDifference - 1);
	const Unique<VkSemaphore>						semaphoreDefaultValue		(createSemaphoreType(vk, device, VK_SEMAPHORE_TYPE_TIMELINE_KHR));
	const Unique<VkSemaphore>						semaphoreInitialValue		(createSemaphoreType(vk, device, VK_SEMAPHORE_TYPE_TIMELINE_KHR, 0, nonZeroValue));
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

	VK_CHECK(vk.getSemaphoreCounterValue(device, *semaphoreDefaultValue, &value));
	if (value != initialValue)
		return tcu::TestStatus::fail("Invalid zero initial value");

	waitInfo.pSemaphores = &semaphoreInitialValue.get();
	initialValue = nonZeroValue;
	result = vk.waitSemaphores(device, &waitInfo, 0ull);
	if (result != VK_SUCCESS)
		return tcu::TestStatus::fail("Wait non zero initial value failed");

	VK_CHECK(vk.getSemaphoreCounterValue(device, *semaphoreInitialValue, &value));
	if (value != nonZeroValue)
		return tcu::TestStatus::fail("Invalid non zero initial value");

	if (maxTimelineValueDifference != std::numeric_limits<deUint64>::max())
	{
		const deUint64				nonZeroMaxValue		= maxTimelineValueDifference + 1;
		const Unique<VkSemaphore>	semaphoreMaxValue	(createSemaphoreType(vk, device, VK_SEMAPHORE_TYPE_TIMELINE_KHR, 0, nonZeroMaxValue));

		waitInfo.pSemaphores = &semaphoreMaxValue.get();
		initialValue = nonZeroMaxValue;
		result = vk.waitSemaphores(device, &waitInfo, 0ull);
		if (result != VK_SUCCESS)
			return tcu::TestStatus::fail("Wait max value failed");

		VK_CHECK(vk.getSemaphoreCounterValue(device, *semaphoreMaxValue, &value));
		if (value != nonZeroMaxValue)
			return tcu::TestStatus::fail("Invalid max value initial value");
	}

	return tcu::TestStatus::pass("Initial value correct");
}

class WaitTests : public tcu::TestCaseGroup
{
public:
	WaitTests (tcu::TestContext& testCtx)
		: tcu::TestCaseGroup(testCtx, "wait", "Various wait cases of timeline semaphores")
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
			addChild(new WaitTestCase(m_testCtx, waitCases[caseIdx].name, waitCases[caseIdx].waitAll, waitCases[caseIdx].signalFromDevice));
		addChild(new HostWaitBeforeSignalTestCase(m_testCtx, "host_wait_before_signal"));
	}
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
				const VkSemaphoreWaitInfoKHR	waitInfo	=
				{
					VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR,	// VkStructureType			sType;
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
				const VkSemaphoreSignalInfoKHR	signalInfo	=
				{
					VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO_KHR,	// VkStructureType			sType;
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
							const ResourceDescription&			resourceDesc,
							const SharedPtr<OperationSupport>&	writeOp,
							const SharedPtr<OperationSupport>&	readOp,
							PipelineCacheData&					pipelineCacheData)
		: TestInstance		(context)
		, m_opContext		(context, pipelineCacheData)
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
		const Unique<VkSemaphore>							semaphore				(createSemaphoreType(vk, device, VK_SEMAPHORE_TYPE_TIMELINE_KHR));
		const Unique<VkCommandPool>							cmdPool					(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
		const VkPipelineStageFlags							stageBits[]				= { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };
		HostCopyThread										hostCopyThread			(vk, device, *semaphore, m_iterations);
		std::vector<SharedPtr<Move<VkCommandBuffer> > >		ptrCmdBuffers;
		std::vector<VkCommandBuffer>						cmdBuffers;
		std::vector<VkTimelineSemaphoreSubmitInfo>			timelineSubmitInfos;
		std::vector<VkSubmitInfo>							submitInfos;

		hostCopyThread.start();

		for (deUint32 opNdx = 0; opNdx < (m_iterations.size() * 2); opNdx++)
		{
			ptrCmdBuffers.push_back(makeVkSharedPtr(makeCommandBuffer(vk, device, *cmdPool)));
			cmdBuffers.push_back(**(ptrCmdBuffers.back()));
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

		timelineSubmitInfos.resize(m_iterations.size() * 2);
		submitInfos.resize(m_iterations.size() * 2);

		for (deUint32 iterIdx = 0; iterIdx < m_iterations.size(); iterIdx++)
		{
			// Write operation
			{
				const VkTimelineSemaphoreSubmitInfo		timelineSubmitInfo	=
				{
					VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,				// VkStructureType	sType;
					DE_NULL,														// const void*		pNext;
					iterIdx == 0 ? 0u : 1u,											// deUint32			waitSemaphoreValueCount
					iterIdx == 0 ? DE_NULL : &m_iterations[iterIdx - 1]->cpuValue,	// const deUint64*	pWaitSemaphoreValues
					1u,																// deUint32			signalSemaphoreValueCount
					&m_iterations[iterIdx]->writeValue,								// const deUint64*	pSignalSemaphoreValues
				};
				const VkSubmitInfo						submitInfo			=
				{
					VK_STRUCTURE_TYPE_SUBMIT_INFO,							// VkStructureType			sType;
					&timelineSubmitInfos[2 * iterIdx],						// const void*				pNext;
					iterIdx == 0 ? 0u : 1u,									// deUint32					waitSemaphoreCount;
					&semaphore.get(),										// const VkSemaphore*		pWaitSemaphores;
					stageBits,
					1u,														// deUint32					commandBufferCount;
					&cmdBuffers[2 * iterIdx],								// const VkCommandBuffer*	pCommandBuffers;
					1u,														// deUint32					signalSemaphoreCount;
					&semaphore.get(),										// const VkSemaphore*		pSignalSemaphores;
				};

				timelineSubmitInfos[2 * iterIdx]	=	timelineSubmitInfo;
				submitInfos[2 * iterIdx]			=	submitInfo;

				beginCommandBuffer(vk, cmdBuffers[2 * iterIdx]);
				m_iterations[iterIdx]->writeOp->recordCommands(cmdBuffers[2 * iterIdx]);

				{
					const SyncInfo	writeSync	= m_iterations[iterIdx]->writeOp->getOutSyncInfo();
					const SyncInfo	readSync	= m_iterations[iterIdx]->readOp->getInSyncInfo();
					const Resource&	resource	= *(m_iterations[iterIdx]->resource);

					if (resource.getType() == RESOURCE_TYPE_IMAGE)
					{
						DE_ASSERT(writeSync.imageLayout != VK_IMAGE_LAYOUT_UNDEFINED);
						DE_ASSERT(readSync.imageLayout != VK_IMAGE_LAYOUT_UNDEFINED);
						const VkImageMemoryBarrier barrier =  makeImageMemoryBarrier(writeSync.accessMask, readSync.accessMask,
																					 writeSync.imageLayout, readSync.imageLayout,
																					 resource.getImage().handle,
																					 resource.getImage().subresourceRange);
						vk.cmdPipelineBarrier(cmdBuffers[2 * iterIdx], writeSync.stageMask, readSync.stageMask, (VkDependencyFlags)0,
											  0u, (const VkMemoryBarrier*)DE_NULL, 0u, (const VkBufferMemoryBarrier*)DE_NULL, 1u, &barrier);
					}
					else
					{
						const VkBufferMemoryBarrier barrier = makeBufferMemoryBarrier(writeSync.accessMask, readSync.accessMask,
																					  resource.getBuffer().handle, 0, VK_WHOLE_SIZE);
						vk.cmdPipelineBarrier(cmdBuffers[2 * iterIdx], writeSync.stageMask, readSync.stageMask, (VkDependencyFlags)0,
											  0u, (const VkMemoryBarrier*)DE_NULL, 1u, &barrier, 0u, (const VkImageMemoryBarrier*)DE_NULL);
					}
				}

				endCommandBuffer(vk, cmdBuffers[2 * iterIdx]);
			}

			// Read operation
			{
				const VkTimelineSemaphoreSubmitInfo		timelineSubmitInfo	=
				{
					VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,		// VkStructureType	sType;
					DE_NULL,												// const void*		pNext;
					1u,														// deUint32			waitSemaphoreValueCount
					&m_iterations[iterIdx]->writeValue,						// const deUint64*	pWaitSemaphoreValues
					1u,														// deUint32			signalSemaphoreValueCount
					&m_iterations[iterIdx]->readValue,						// const deUint64*	pSignalSemaphoreValues
				};
				const VkSubmitInfo						submitInfo			=
				{
					VK_STRUCTURE_TYPE_SUBMIT_INFO,							// VkStructureType			sType;
					&timelineSubmitInfos[2 * iterIdx + 1],					// const void*				pNext;
					1u,														// deUint32					waitSemaphoreCount;
					&semaphore.get(),										// const VkSemaphore*		pWaitSemaphores;
					stageBits,
					1u,														// deUint32					commandBufferCount;
					&cmdBuffers[2 * iterIdx + 1],							// const VkCommandBuffer*	pCommandBuffers;
					1u,														// deUint32					signalSemaphoreCount;
					&semaphore.get(),										// const VkSemaphore*		pSignalSemaphores;
				};

				timelineSubmitInfos[2 * iterIdx + 1]	=	timelineSubmitInfo;
				submitInfos[2 * iterIdx + 1]			=	submitInfo;

				beginCommandBuffer(vk, cmdBuffers[2 * iterIdx + 1]);
				m_iterations[iterIdx]->readOp->recordCommands(cmdBuffers[2 * iterIdx + 1]);
				endCommandBuffer(vk, cmdBuffers[2 * iterIdx + 1]);
			}
		}

		VK_CHECK(vk.queueSubmit(queue, (deUint32) submitInfos.size(), &submitInfos[0], DE_NULL));

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
							 const ResourceDescription	resourceDesc,
							 const OperationName		writeOp,
							 const OperationName		readOp,
							 PipelineCacheData&			pipelineCacheData)
		: TestCase				(testCtx, name, description)
		, m_resourceDesc		(resourceDesc)
		, m_writeOp				(makeOperationSupport(writeOp, resourceDesc).release())
		, m_readOp				(makeOperationSupport(readOp, resourceDesc).release())
		, m_pipelineCacheData	(pipelineCacheData)
	{
	}

	virtual void checkSupport(Context& context) const
	{
		context.requireDeviceFunctionality("VK_KHR_timeline_semaphore");
	}

	void initPrograms (SourceCollections& programCollection) const
	{
		m_writeOp->initPrograms(programCollection);
		m_readOp->initPrograms(programCollection);
	}

	TestInstance* createInstance (Context& context) const
	{
		return new DeviceHostTestInstance(context, m_resourceDesc, m_writeOp, m_readOp, m_pipelineCacheData);
	}

private:
	const ResourceDescription			m_resourceDesc;
	const SharedPtr<OperationSupport>	m_writeOp;
	const SharedPtr<OperationSupport>	m_readOp;
	PipelineCacheData&					m_pipelineCacheData;
};

class DeviceHostTests : public tcu::TestCaseGroup
{
public:
	DeviceHostTests (tcu::TestContext& testCtx)
		: tcu::TestCaseGroup(testCtx, "device_host", "Synchronization of serialized device/host operations")
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
					opGroup->addChild(new DeviceHostSyncTestCase(m_testCtx, name, "", resource, writeOp, readOp, m_pipelineCacheData));
					empty = false;
				}
			}
			if (!empty)
				addChild(opGroup.release());
		}

		{
			de::MovePtr<tcu::TestCaseGroup> miscGroup	(new tcu::TestCaseGroup(m_testCtx, "misc", ""));
			addFunctionCase(miscGroup.get(), "max_difference_value", "Timeline semaphore properties test", checkTimelineSupport, maxDifferenceValueCase);
			addFunctionCase(miscGroup.get(), "initial_value", "Timeline semaphore initial value test", checkTimelineSupport, initialValueCase);
			addChild(miscGroup.release());
		}
	}

private:
	// synchronization.op tests share pipeline cache data to speed up test
	// execution.
	PipelineCacheData	m_pipelineCacheData;
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

Move<VkDevice> createDevice(const Context& context)
{
	const std::vector<VkQueueFamilyProperties>		queueFamilyProperties	= getPhysicalDeviceQueueFamilyProperties(context.getInstanceInterface(), context.getPhysicalDevice());
	std::vector<VkDeviceQueueCreateInfo>			queueCreateInfos		= getQueueCreateInfo(queueFamilyProperties);
	const char *									extensions[]			=
	{
		"VK_KHR_timeline_semaphore"
	};
	const VkDeviceCreateInfo						deviceInfo				=
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,							//VkStructureType					sType;
		DE_NULL,														//const void*						pNext;
		0u,																//VkDeviceCreateFlags				flags;
		static_cast<deUint32>(queueCreateInfos.size()),					//deUint32							queueCreateInfoCount;
		&queueCreateInfos[0],											//const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
		0u,																//deUint32							enabledLayerCount;
		DE_NULL,														//const char* const*				ppEnabledLayerNames;
		1u,																//deUint32							enabledExtensionCount;
		extensions,														//const char* const*				ppEnabledExtensionNames;
		&context.getDeviceFeatures()									//const VkPhysicalDeviceFeatures*	pEnabledFeatures;
	};
	std::vector<SharedPtr<std::vector<float> > >	queuePriorities;

	for (auto& queueCreateInfo : queueCreateInfos) {
		MovePtr<std::vector<float> > priorities(new std::vector<float>);

		for (deUint32 i = 0; i < queueCreateInfo.queueCount; i++)
			priorities->push_back(1.0f);

		queuePriorities.push_back(makeSharedPtr(priorities));

		queueCreateInfo.pQueuePriorities = &(*queuePriorities.back().get())[0];
	}

	return createDevice(context.getPlatformInterface(), context.getInstance(),
						context.getInstanceInterface(), context.getPhysicalDevice(), &deviceInfo);
}


// Class to wrap a singleton instance and device
class SingletonDevice
{
	SingletonDevice	(const Context& context)
		: m_logicalDevice	(createDevice(context))
	{
	}

public:

	static const Unique<vk::VkDevice>& getDevice(const Context& context)
	{
		if (!m_singletonDevice)
			m_singletonDevice = SharedPtr<SingletonDevice>(new SingletonDevice(context));

		DE_ASSERT(m_singletonDevice);
		return m_singletonDevice->m_logicalDevice;
	}

	static void destroy()
	{
		m_singletonDevice.clear();
	}

private:
	const Unique<vk::VkDevice>					m_logicalDevice;

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
								  const ResourceDescription&			resourceDesc,
								  const SharedPtr<OperationSupport>&	writeOp,
								  const SharedPtr<OperationSupport>&	readOp,
								  PipelineCacheData&					pipelineCacheData)
		: TestInstance		(context)
		, m_resourceDesc	(resourceDesc)
		, m_device			(SingletonDevice::getDevice(context))
		, m_deviceDriver	(MovePtr<DeviceDriver>(new DeviceDriver(context.getPlatformInterface(), context.getInstance(), *m_device)))
		, m_allocator		(new SimpleAllocator(*m_deviceDriver, *m_device,
												 getPhysicalDeviceMemoryProperties(context.getInstanceInterface(),
																				   context.getPhysicalDevice())))
		, m_opContext		(context, pipelineCacheData, *m_deviceDriver, *m_device, *m_allocator)
	{
		const DeviceInterface&						vk							= *m_deviceDriver;
		const VkDevice								device						= *m_device;
		const std::vector<VkQueueFamilyProperties>	queueFamilyProperties		= getPhysicalDeviceQueueFamilyProperties(context.getInstanceInterface(), context.getPhysicalDevice());
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

	tcu::TestStatus	iterate (void)
	{
		const DeviceInterface&								vk						= *m_deviceDriver;
		const VkDevice										device					= *m_device;
		const Unique<VkSemaphore>							semaphore				(createSemaphoreType(vk, device, VK_SEMAPHORE_TYPE_TIMELINE_KHR));
		const VkPipelineStageFlags							stageBits[]				= { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };
		std::vector<SharedPtr<Move<VkCommandPool> > >		cmdPools;
		std::vector<SharedPtr<Move<VkCommandBuffer> > >		ptrCmdBuffers;
		std::vector<VkCommandBuffer>						cmdBuffers;

		for (deUint32 opNdx = 0; opNdx < m_iterations.size(); opNdx++)
		{
			cmdPools.push_back(makeVkSharedPtr(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
																 m_iterations[opNdx]->queueFamilyIdx)));
			ptrCmdBuffers.push_back(makeVkSharedPtr(makeCommandBuffer(vk, device, **cmdPools.back())));
			cmdBuffers.push_back(**(ptrCmdBuffers.back()));
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
			deUint32 iterIdx = (deUint32)(m_iterations.size() - 2 - _iterIdx);

			const VkTimelineSemaphoreSubmitInfo		timelineSubmitInfo	=
			{
				VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,									// VkStructureType	sType;
				DE_NULL,																			// const void*		pNext;
				1u,																					// deUint32			waitSemaphoreValueCount
				iterIdx == 0 ? &m_hostTimelineValue : &m_iterations[iterIdx - 1]->timelineValue,	// const deUint64*	pWaitSemaphoreValues
				1u,																					// deUint32			signalSemaphoreValueCount
				&m_iterations[iterIdx]->timelineValue,												// const deUint64*	pSignalSemaphoreValues
			};
			const VkSubmitInfo						submitInfo			=
			{
				VK_STRUCTURE_TYPE_SUBMIT_INFO,														// VkStructureType			sType;
				&timelineSubmitInfo,																// const void*				pNext;
				1u,																					// deUint32					waitSemaphoreCount;
				&semaphore.get(),																	// const VkSemaphore*		pWaitSemaphores;
				stageBits,
				1u,																					// deUint32					commandBufferCount;
				&cmdBuffers[iterIdx],																// const VkCommandBuffer*	pCommandBuffers;
				1u,																					// deUint32					signalSemaphoreCount;
				&semaphore.get(),																	// const VkSemaphore*		pSignalSemaphores;
			};

			beginCommandBuffer(vk, cmdBuffers[iterIdx]);
			m_iterations[iterIdx]->op->recordCommands(cmdBuffers[iterIdx]);

			{
				const SyncInfo	writeSync	= m_iterations[iterIdx]->op->getOutSyncInfo();
				const SyncInfo	readSync	= m_iterations[iterIdx + 1]->op->getInSyncInfo();
				const Resource&	resource	= *m_resources[iterIdx];

				if (resource.getType() == RESOURCE_TYPE_IMAGE)
				{
					DE_ASSERT(writeSync.imageLayout != VK_IMAGE_LAYOUT_UNDEFINED);
					DE_ASSERT(readSync.imageLayout != VK_IMAGE_LAYOUT_UNDEFINED);
					const VkImageMemoryBarrier barrier =  makeImageMemoryBarrier(writeSync.accessMask, readSync.accessMask,
																				 writeSync.imageLayout, readSync.imageLayout,
																				 resource.getImage().handle,
																				 resource.getImage().subresourceRange,
																				 m_iterations[iterIdx]->queueFamilyIdx,
																				 m_iterations[iterIdx + 1]->queueFamilyIdx);
					vk.cmdPipelineBarrier(cmdBuffers[iterIdx], writeSync.stageMask, readSync.stageMask, (VkDependencyFlags)0,
										  0u, (const VkMemoryBarrier*)DE_NULL, 0u, (const VkBufferMemoryBarrier*)DE_NULL, 1u, &barrier);
				}
				else
				{
					const VkBufferMemoryBarrier barrier = makeBufferMemoryBarrier(writeSync.accessMask, readSync.accessMask,
																				  resource.getBuffer().handle, 0, VK_WHOLE_SIZE,
																				  m_iterations[iterIdx]->queueFamilyIdx,
																				  m_iterations[iterIdx + 1]->queueFamilyIdx);
					vk.cmdPipelineBarrier(cmdBuffers[iterIdx], writeSync.stageMask, readSync.stageMask, (VkDependencyFlags)0,
										  0u, (const VkMemoryBarrier*)DE_NULL, 1u, &barrier, 0u, (const VkImageMemoryBarrier*)DE_NULL);
				}
			}

			endCommandBuffer(vk, cmdBuffers[iterIdx]);

			VK_CHECK(vk.queueSubmit(m_iterations[iterIdx]->queue, 1u, &submitInfo, DE_NULL));
		}

		// Submit the last read operation in order.
		{
			const deUint32							iterIdx				= (deUint32) (m_iterations.size() - 1);
			const VkTimelineSemaphoreSubmitInfo		timelineSubmitInfo	=
			{
				VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,					// VkStructureType	sType;
				DE_NULL,															// const void*		pNext;
				1u,																	// deUint32			waitSemaphoreValueCount
				&m_iterations[iterIdx - 1]->timelineValue,							// const deUint64*	pWaitSemaphoreValues
				1u,																	// deUint32			signalSemaphoreValueCount
				&m_iterations[iterIdx]->timelineValue,								// const deUint64*	pSignalSemaphoreValues
			};
			const VkSubmitInfo						submitInfo			=
			{
				VK_STRUCTURE_TYPE_SUBMIT_INFO,							// VkStructureType			sType;
				&timelineSubmitInfo,									// const void*				pNext;
				1u,														// deUint32					waitSemaphoreCount;
				&semaphore.get(),										// const VkSemaphore*		pWaitSemaphores;
				stageBits,
				1u,														// deUint32					commandBufferCount;
				&cmdBuffers[iterIdx],									// const VkCommandBuffer*	pCommandBuffers;
				1u,														// deUint32					signalSemaphoreCount;
				&semaphore.get(),										// const VkSemaphore*		pSignalSemaphores;
			};

			beginCommandBuffer(vk, cmdBuffers[iterIdx]);
			m_iterations[iterIdx]->op->recordCommands(cmdBuffers[iterIdx]);
			endCommandBuffer(vk, cmdBuffers[iterIdx]);

			VK_CHECK(vk.queueSubmit(m_iterations[iterIdx]->queue, 1u, &submitInfo, DE_NULL));
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
	const ResourceDescription						m_resourceDesc;
	const Unique<VkDevice>&							m_device;
	MovePtr<DeviceDriver>							m_deviceDriver;
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
								 const ResourceDescription	resourceDesc,
								 const OperationName		writeOp,
								 const OperationName		readOp,
								 PipelineCacheData&			pipelineCacheData)
		: TestCase				(testCtx, name, description)
		, m_resourceDesc		(resourceDesc)
		, m_writeOp				(makeOperationSupport(writeOp, resourceDesc).release())
		, m_readOp				(makeOperationSupport(readOp, resourceDesc).release())
		, m_pipelineCacheData	(pipelineCacheData)
	{
	}

	virtual void checkSupport(Context& context) const
	{
		context.requireDeviceFunctionality("VK_KHR_timeline_semaphore");
	}

	void initPrograms (SourceCollections& programCollection) const
	{
		m_writeOp->initPrograms(programCollection);
		m_readOp->initPrograms(programCollection);

		for (deUint32 copyOpNdx = 0; copyOpNdx < DE_LENGTH_OF_ARRAY(s_copyOps); copyOpNdx++)
		{
			if (isResourceSupported(s_copyOps[copyOpNdx], m_resourceDesc))
				makeOperationSupport(s_copyOps[copyOpNdx], m_resourceDesc)->initPrograms(programCollection);
		}
	}

	TestInstance* createInstance (Context& context) const
	{
		return new WaitBeforeSignalTestInstance(context, m_resourceDesc, m_writeOp, m_readOp, m_pipelineCacheData);
	}

private:
	const ResourceDescription			m_resourceDesc;
	const SharedPtr<OperationSupport>	m_writeOp;
	const SharedPtr<OperationSupport>	m_readOp;
	PipelineCacheData&					m_pipelineCacheData;
};

class WaitBeforeSignalTests : public tcu::TestCaseGroup
{
public:
	WaitBeforeSignalTests (tcu::TestContext& testCtx)
		: tcu::TestCaseGroup(testCtx, "wait_before_signal", "Synchronization of out of order submissions to queues")
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
					opGroup->addChild(new WaitBeforeSignalTestCase(m_testCtx, name, "", resource, writeOp, readOp, m_pipelineCacheData));
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
						const ResourceDescription&			resourceDesc,
						const SharedPtr<OperationSupport>&	writeOp,
						const SharedPtr<OperationSupport>&	readOp,
						PipelineCacheData&					pipelineCacheData)
		: TestInstance		(context)
		, m_resourceDesc	(resourceDesc)
		, m_device			(SingletonDevice::getDevice(context))
		, m_deviceDriver	(MovePtr<DeviceDriver>(new DeviceDriver(context.getPlatformInterface(), context.getInstance(), *m_device)))
		, m_allocator		(new SimpleAllocator(*m_deviceDriver, *m_device,
												 getPhysicalDeviceMemoryProperties(context.getInstanceInterface(),
																				   context.getPhysicalDevice())))
		, m_opContext		(context, pipelineCacheData, *m_deviceDriver, *m_device, *m_allocator)
	{
		const DeviceInterface&									vk				= *m_deviceDriver;
		const VkDevice											device			= *m_device;
		const std::vector<VkQueueFamilyProperties>	queueFamilyProperties		= getPhysicalDeviceQueueFamilyProperties(context.getInstanceInterface(), context.getPhysicalDevice());
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

	void recordBarrier (const DeviceInterface&	vk, VkCommandBuffer cmdBuffer, const QueueTimelineIteration& inIter, const QueueTimelineIteration& outIter, const Resource& resource)
	{
		const SyncInfo	writeSync	= inIter.op->getOutSyncInfo();
		const SyncInfo	readSync	= outIter.op->getInSyncInfo();

		if (resource.getType() == RESOURCE_TYPE_IMAGE)
		{
			DE_ASSERT(writeSync.imageLayout != VK_IMAGE_LAYOUT_UNDEFINED);
			DE_ASSERT(readSync.imageLayout != VK_IMAGE_LAYOUT_UNDEFINED);
			const VkImageMemoryBarrier barrier =  makeImageMemoryBarrier(writeSync.accessMask, readSync.accessMask,
																		 writeSync.imageLayout, readSync.imageLayout,
																		 resource.getImage().handle,
																		 resource.getImage().subresourceRange,
																		 inIter.queueFamilyIdx,
																		 outIter.queueFamilyIdx);
			vk.cmdPipelineBarrier(cmdBuffer, writeSync.stageMask, readSync.stageMask, (VkDependencyFlags)0,
								  0u, (const VkMemoryBarrier*)DE_NULL, 0u, (const VkBufferMemoryBarrier*)DE_NULL, 1u, &barrier);
		}
		else
		{
			const VkBufferMemoryBarrier barrier = makeBufferMemoryBarrier(writeSync.accessMask, readSync.accessMask,
																		  resource.getBuffer().handle, 0, VK_WHOLE_SIZE,
																		  inIter.queueFamilyIdx,
																		  outIter.queueFamilyIdx);
			vk.cmdPipelineBarrier(cmdBuffer, writeSync.stageMask, readSync.stageMask, (VkDependencyFlags)0,
								  0u, (const VkMemoryBarrier*)DE_NULL, 1u, &barrier, 0u, (const VkImageMemoryBarrier*)DE_NULL);
		}
	}

	void submit (const DeviceInterface&	vk, VkCommandBuffer cmdBuffer, const QueueTimelineIteration& iter, VkSemaphore semaphore, const deUint64 *waitValues, const deUint32 waitValuesCount)
	{
		const VkPipelineStageFlags				stageBits[2]		=
		{
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		};
		const VkTimelineSemaphoreSubmitInfo		timelineSubmitInfo	=
		{
			VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,				// VkStructureType	sType;
			DE_NULL,														// const void*		pNext;
			waitValuesCount,												// deUint32			waitSemaphoreValueCount
			waitValues,														// const deUint64*	pWaitSemaphoreValues
			1u,																// deUint32			signalSemaphoreValueCount
			&iter.timelineValue,											// const deUint64*	pSignalSemaphoreValues
		};
		const VkSemaphore						waitSemaphores[2]	=
		{
			semaphore,
			semaphore,
		};
		const VkSubmitInfo						submitInfo			=
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,									// VkStructureType			sType;
			&timelineSubmitInfo,											// const void*				pNext;
			waitValuesCount,												// deUint32					waitSemaphoreCount;
			waitSemaphores,													// const VkSemaphore*		pWaitSemaphores;
			stageBits,
			1u,																// deUint32					commandBufferCount;
			&cmdBuffer,														// const VkCommandBuffer*	pCommandBuffers;
			1u,																// deUint32					signalSemaphoreCount;
			&semaphore,														// const VkSemaphore*		pSignalSemaphores;
		};

		VK_CHECK(vk.queueSubmit(iter.queue, 1u, &submitInfo, DE_NULL));
	}

	tcu::TestStatus	iterate (void)
	{
		const DeviceInterface&								vk						= *m_deviceDriver;
		const VkDevice										device					= *m_device;
		const Unique<VkSemaphore>							semaphore				(createSemaphoreType(vk, device, VK_SEMAPHORE_TYPE_TIMELINE_KHR));
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
				recordBarrier(vk, **copyPtrCmdBuffers[copyOpIdx], *m_writeIteration, *m_copyIterations[copyOpIdx], *m_writeResource);
				m_copyIterations[copyOpIdx]->op->recordCommands(**copyPtrCmdBuffers[copyOpIdx]);
				endCommandBuffer(vk, **copyPtrCmdBuffers[copyOpIdx]);
			}

			for (deUint32 readOpIdx = 0; readOpIdx < m_readIterations.size(); readOpIdx++)
			{
				beginCommandBuffer(vk, **readPtrCmdBuffers[readOpIdx]);
				recordBarrier(vk, **readPtrCmdBuffers[readOpIdx], *m_copyIterations[readOpIdx], *m_readIterations[readOpIdx], *m_copyResources[readOpIdx]);
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
	ResourceDescription								m_resourceDesc;
	const Unique<VkDevice>&							m_device;
	MovePtr<DeviceDriver>							m_deviceDriver;
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
					 const ResourceDescription	resourceDesc,
					 const OperationName		writeOp,
					 const OperationName		readOp,
					 PipelineCacheData&			pipelineCacheData)
		: TestCase				(testCtx, name, description)
		, m_resourceDesc		(resourceDesc)
		, m_writeOp				(makeOperationSupport(writeOp, resourceDesc).release())
		, m_readOp				(makeOperationSupport(readOp, resourceDesc).release())
		, m_pipelineCacheData	(pipelineCacheData)
	{
	}

	virtual void checkSupport(Context& context) const
	{
		context.requireDeviceFunctionality("VK_KHR_timeline_semaphore");
	}

	void initPrograms (SourceCollections& programCollection) const
	{
		m_writeOp->initPrograms(programCollection);
		m_readOp->initPrograms(programCollection);

		for (deUint32 copyOpNdx = 0; copyOpNdx < DE_LENGTH_OF_ARRAY(s_copyOps); copyOpNdx++)
		{
			if (isResourceSupported(s_copyOps[copyOpNdx], m_resourceDesc))
				makeOperationSupport(s_copyOps[copyOpNdx], m_resourceDesc)->initPrograms(programCollection);
		}
	}

	TestInstance* createInstance (Context& context) const
	{
		return new OneToNTestInstance(context, m_resourceDesc, m_writeOp, m_readOp, m_pipelineCacheData);
	}

private:
	const ResourceDescription			m_resourceDesc;
	const SharedPtr<OperationSupport>	m_writeOp;
	const SharedPtr<OperationSupport>	m_readOp;
	PipelineCacheData&					m_pipelineCacheData;
};

class OneToNTests : public tcu::TestCaseGroup
{
public:
	OneToNTests (tcu::TestContext& testCtx)
		: tcu::TestCaseGroup(testCtx, "one_to_n", "Synchronization multiple waiter on a signal producer")
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
					opGroup->addChild(new OneToNTestCase(m_testCtx, name, "", resource, writeOp, readOp, m_pipelineCacheData));
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
	// synchronization.op tests share pipeline cache data to speed up test
	// execution.
	PipelineCacheData	m_pipelineCacheData;
};

} // anonymous

tcu::TestCaseGroup* createTimelineSemaphoreTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> basicTests(new tcu::TestCaseGroup(testCtx, "timeline_semaphore", "Timeline semaphore tests"));

	basicTests->addChild(new DeviceHostTests(testCtx));
	basicTests->addChild(new OneToNTests(testCtx));
	basicTests->addChild(new WaitBeforeSignalTests(testCtx));
	basicTests->addChild(new WaitTests(testCtx));

	return basicTests.release();
}

} // synchronization
} // vkt
