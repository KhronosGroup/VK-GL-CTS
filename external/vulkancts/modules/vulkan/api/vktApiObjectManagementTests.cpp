/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief Object management tests
 *//*--------------------------------------------------------------------*/

#include "vktApiObjectManagementTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkPlatform.hpp"
#include "vkStrUtil.hpp"
#include "vkAllocationCallbackUtil.hpp"
#include "vkObjTypeImpl.inl"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkSafetyCriticalUtil.hpp"

#include "vktTestGroupUtil.hpp"

#include "tcuVector.hpp"
#include "tcuResultCollector.hpp"
#include "tcuCommandLine.hpp"
#include "tcuTestLog.hpp"
#include "tcuPlatform.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "deArrayUtil.hpp"
#include "deSpinBarrier.hpp"
#include "deThread.hpp"
#include "deInt32.h"

#include <limits>
#include <algorithm>

#define VK_DESCRIPTOR_TYPE_LAST (VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT + 1)

namespace vkt
{
namespace api
{

namespace
{

using namespace vk;

using de::UniquePtr;
using de::MovePtr;
using de::SharedPtr;

using tcu::IVec3;
using tcu::UVec3;
using tcu::ResultCollector;
using tcu::TestStatus;
using tcu::TestLog;

using std::string;
using std::vector;

typedef SharedPtr<Move<VkPipeline> >			VkPipelineSp;		// Move so it's possible to disown the handle
typedef SharedPtr<Move<VkDescriptorSet> >		VkDescriptorSetSp;
typedef SharedPtr<Move<VkCommandBuffer> >		VkCommandBufferSp;

class ThreadGroupThread;

/*--------------------------------------------------------------------*//*!
 * \brief Thread group
 *
 * Thread group manages collection of threads that are expected to be
 * launched simultaneously as a group.
 *
 * Shared barrier is provided for synchronizing execution. Terminating thread
 * early either by returning from ThreadGroupThread::runThread() or throwing
 * an exception is safe, and other threads will continue execution. The
 * thread that has been terminated is simply removed from the synchronization
 * group.
 *
 * TestException-based exceptions are collected and translated into a
 * tcu::TestStatus by using tcu::ResultCollector.
 *
 * Use cases for ThreadGroup include for example testing thread-safety of
 * certain API operations by poking API simultaneously from multiple
 * threads.
 *//*--------------------------------------------------------------------*/

class ThreadGroup
{
public:
							ThreadGroup			(void);
							~ThreadGroup		(void);

	void					add					(de::MovePtr<ThreadGroupThread> thread);
	TestStatus				run					(void);

private:
	typedef std::vector<de::SharedPtr<ThreadGroupThread> >	ThreadVector;

	ThreadVector			m_threads;
	de::SpinBarrier			m_barrier;
} DE_WARN_UNUSED_TYPE;

class ThreadGroupThread : private de::Thread
{
public:
							ThreadGroupThread	(void);
	virtual					~ThreadGroupThread	(void);

	void					start				(de::SpinBarrier* groupBarrier);

	ResultCollector&		getResultCollector	(void) { return m_resultCollector; }

	using de::Thread::join;

protected:
	virtual void			runThread			(void) = 0;

	void					barrier				(void);

private:
							ThreadGroupThread	(const ThreadGroupThread&);
	ThreadGroupThread&		operator=			(const ThreadGroupThread&);

	void					run					(void);

	ResultCollector			m_resultCollector;
	de::SpinBarrier*		m_barrier;
};

// ThreadGroup

ThreadGroup::ThreadGroup (void)
	: m_barrier(1)
{
}

ThreadGroup::~ThreadGroup (void)
{
}

void ThreadGroup::add (de::MovePtr<ThreadGroupThread> thread)
{
	m_threads.push_back(de::SharedPtr<ThreadGroupThread>(thread.release()));
}

tcu::TestStatus ThreadGroup::run (void)
{
	tcu::ResultCollector	resultCollector;

	m_barrier.reset((int)m_threads.size());

	for (ThreadVector::iterator threadIter = m_threads.begin(); threadIter != m_threads.end(); ++threadIter)
		(*threadIter)->start(&m_barrier);

	for (ThreadVector::iterator threadIter = m_threads.begin(); threadIter != m_threads.end(); ++threadIter)
	{
		tcu::ResultCollector&	threadResult	= (*threadIter)->getResultCollector();
		(*threadIter)->join();
		resultCollector.addResult(threadResult.getResult(), threadResult.getMessage());
	}

	return tcu::TestStatus(resultCollector.getResult(), resultCollector.getMessage());
}

// ThreadGroupThread

ThreadGroupThread::ThreadGroupThread (void)
	: m_barrier(DE_NULL)
{
}

ThreadGroupThread::~ThreadGroupThread (void)
{
}

void ThreadGroupThread::start (de::SpinBarrier* groupBarrier)
{
	m_barrier = groupBarrier;
	de::Thread::start();
}

void ThreadGroupThread::run (void)
{
	try
	{
		runThread();
	}
	catch (const tcu::TestException& e)
	{
		getResultCollector().addResult(e.getTestResult(), e.getMessage());
	}
	catch (const std::exception& e)
	{
		getResultCollector().addResult(QP_TEST_RESULT_FAIL, e.what());
	}
	catch (...)
	{
		getResultCollector().addResult(QP_TEST_RESULT_FAIL, "Exception");
	}

	m_barrier->removeThread(de::SpinBarrier::WAIT_MODE_AUTO);
}

inline void ThreadGroupThread::barrier (void)
{
	m_barrier->sync(de::SpinBarrier::WAIT_MODE_AUTO);
}

deUint32 getDefaultTestThreadCount (void)
{
#ifndef CTS_USES_VULKANSC
	return de::clamp(deGetNumAvailableLogicalCores(), 2u, 8u);
#else
	return 2u;
#endif // CTS_USES_VULKANSC

}

// Utilities

struct Environment
{
	const PlatformInterface&			vkp;
	deUint32							apiVersion;
	const InstanceInterface&			instanceInterface;
	VkInstance							instance;
	const DeviceInterface&				vkd;
	VkDevice							device;
	deUint32							queueFamilyIndex;
	const BinaryCollection&				programBinaries;
	const VkAllocationCallbacks*		allocationCallbacks;
	deUint32							maxResourceConsumers;		// Maximum number of objects using same Object::Resources concurrently
#ifdef CTS_USES_VULKANSC
	de::SharedPtr<ResourceInterface>		resourceInterface;
	VkPhysicalDeviceVulkanSC10Properties	vulkanSC10Properties;
	VkPhysicalDeviceProperties				properties;
#endif // CTS_USES_VULKANSC
	const tcu::CommandLine&			commandLine;

	Environment (Context& context, deUint32 maxResourceConsumers_)
		: vkp					(context.getPlatformInterface())
		, apiVersion			(context.getUsedApiVersion())
		, instanceInterface		(context.getInstanceInterface())
		, instance				(context.getInstance())
		, vkd					(context.getDeviceInterface())
		, device				(context.getDevice())
		, queueFamilyIndex		(context.getUniversalQueueFamilyIndex())
		, programBinaries		(context.getBinaryCollection())
		, allocationCallbacks	(DE_NULL)
		, maxResourceConsumers	(maxResourceConsumers_)
#ifdef CTS_USES_VULKANSC
		, resourceInterface		(context.getResourceInterface())
		, vulkanSC10Properties	(context.getDeviceVulkanSC10Properties())
		, properties			(context.getDeviceProperties())
#endif // CTS_USES_VULKANSC
		, commandLine			(context.getTestContext().getCommandLine())
	{
	}

	Environment (const PlatformInterface&		vkp_,
				 deUint32						apiVersion_,
				 const InstanceInterface&		instanceInterface_,
				 VkInstance						instance_,
				 const DeviceInterface&			vkd_,
				 VkDevice						device_,
				 deUint32						queueFamilyIndex_,
				 const BinaryCollection&		programBinaries_,
				 const VkAllocationCallbacks*	allocationCallbacks_,
				 deUint32						maxResourceConsumers_,
#ifdef CTS_USES_VULKANSC
				 de::SharedPtr<ResourceInterface>	resourceInterface_,
				 const VkPhysicalDeviceVulkanSC10Properties& vulkanSC10Properties_,
#endif // CTS_USES_VULKANSC
				 const tcu::CommandLine&		commandLine_)
		: vkp					(vkp_)
		, apiVersion			(apiVersion_)
		, instanceInterface		(instanceInterface_)
		, instance				(instance_)
		, vkd					(vkd_)
		, device				(device_)
		, queueFamilyIndex		(queueFamilyIndex_)
		, programBinaries		(programBinaries_)
#ifdef CTS_USES_VULKANSC
		, allocationCallbacks(DE_NULL)
#else
		, allocationCallbacks(allocationCallbacks_)
#endif // CTS_USES_VULKANSC
		, maxResourceConsumers	(maxResourceConsumers_)
#ifdef CTS_USES_VULKANSC
		, resourceInterface		(resourceInterface_)
		, vulkanSC10Properties	(vulkanSC10Properties_)
#endif // CTS_USES_VULKANSC
		, commandLine			(commandLine_)
	{
#ifdef CTS_USES_VULKANSC
		DE_UNREF(allocationCallbacks_);
#endif // CTS_USES_VULKANSC
	}
};

template<typename Case>
struct Dependency
{
	typename Case::Resources		resources;
	Unique<typename Case::Type>		object;

	Dependency (const Environment& env, const typename Case::Parameters& params)
		: resources	(env, params)
		, object	(Case::create(env, resources, params))
	{}
};

template<typename T>
T roundUpToNextMultiple (T value, T multiple)
{
	if (value % multiple == 0)
		return value;
	else
		return value + multiple - (value % multiple);
}

#if defined(DE_DEBUG)
template<typename T>
bool isPowerOfTwo (T value)
{
	return ((value & (value - T(1))) == 0);
}
#endif

template<typename T>
T alignToPowerOfTwo (T value, T align)
{
	DE_ASSERT(isPowerOfTwo(align));
	return (value + align - T(1)) & ~(align - T(1));
}
#ifndef CTS_USES_VULKANSC
inline bool hasDeviceExtension (Context& context, const string name)
{
	return context.isDeviceFunctionalitySupported(name);
}
#endif

VkDeviceSize getPageTableSize (const tcu::PlatformMemoryLimits& limits, VkDeviceSize allocationSize)
{
	VkDeviceSize	totalSize	= 0;

	for (size_t levelNdx = 0; levelNdx < limits.devicePageTableHierarchyLevels; ++levelNdx)
	{
		const VkDeviceSize	coveredAddressSpaceSize	= limits.devicePageSize<<levelNdx;
		const VkDeviceSize	numPagesNeeded			= alignToPowerOfTwo(allocationSize, coveredAddressSpaceSize) / coveredAddressSpaceSize;

		totalSize += numPagesNeeded*limits.devicePageTableEntrySize;
	}

	return totalSize;
}

size_t getCurrentSystemMemoryUsage (const AllocationCallbackRecorder& allocRecoder)
{
	const size_t						systemAllocationOverhead	= sizeof(void*)*2;
	AllocationCallbackValidationResults	validationResults;

	validateAllocationCallbacks(allocRecoder, &validationResults);
	TCU_CHECK(validationResults.violations.empty());

	return getLiveSystemAllocationTotal(validationResults) + systemAllocationOverhead*validationResults.liveAllocations.size();
}

template<typename Object>
size_t computeSystemMemoryUsage (Context& context, const typename Object::Parameters& params)
{
	AllocationCallbackRecorder			allocRecorder		(getSystemAllocator());
	const Environment					env					(context.getPlatformInterface(),
															 context.getUsedApiVersion(),
															 context.getInstanceInterface(),
															 context.getInstance(),
															 context.getDeviceInterface(),
															 context.getDevice(),
															 context.getUniversalQueueFamilyIndex(),
															 context.getBinaryCollection(),
															 allocRecorder.getCallbacks(),
															 1u,
#ifdef CTS_USES_VULKANSC
															context.getResourceInterface(),
															context.getDeviceVulkanSC10Properties(),
#endif // CTS_USES_VULKANSC
															 context.getTestContext().getCommandLine());
	const typename Object::Resources	res					(env, params);
	const size_t						resourceMemoryUsage	= getCurrentSystemMemoryUsage(allocRecorder);

	{
		Unique<typename Object::Type>	obj					(Object::create(env, res, params));
		const size_t					totalMemoryUsage	= getCurrentSystemMemoryUsage(allocRecorder);

		return totalMemoryUsage - resourceMemoryUsage;
	}
}

size_t getSafeObjectCount (const tcu::PlatformMemoryLimits&	memoryLimits,
						   size_t							objectSystemMemoryUsage,
						   VkDeviceSize						objectDeviceMemoryUsage = 0)
{
	const VkDeviceSize	roundedUpDeviceMemory	= roundUpToNextMultiple(objectDeviceMemoryUsage, memoryLimits.deviceMemoryAllocationGranularity);

	if (memoryLimits.totalDeviceLocalMemory > 0 && roundedUpDeviceMemory > 0)
	{
		if (objectSystemMemoryUsage > 0)
			return de::min(memoryLimits.totalSystemMemory / objectSystemMemoryUsage,
						   (size_t)(memoryLimits.totalDeviceLocalMemory / roundedUpDeviceMemory));
		else
			return (size_t)(memoryLimits.totalDeviceLocalMemory / roundedUpDeviceMemory);
	}
	else if (objectSystemMemoryUsage + roundedUpDeviceMemory > 0)
	{
		DE_ASSERT(roundedUpDeviceMemory <= std::numeric_limits<size_t>::max() - objectSystemMemoryUsage);
		return memoryLimits.totalSystemMemory / (objectSystemMemoryUsage + (size_t)roundedUpDeviceMemory);
	}
	else
	{
		// Warning: at this point driver has probably not implemented allocation callbacks correctly
		return std::numeric_limits<size_t>::max();
	}
}

tcu::PlatformMemoryLimits getPlatformMemoryLimits (Context& context)
{
	tcu::PlatformMemoryLimits	memoryLimits;

	context.getTestContext().getPlatform().getMemoryLimits(memoryLimits);

	return memoryLimits;
}

size_t getSafeObjectCount (Context& context, size_t objectSystemMemoryUsage, VkDeviceSize objectDeviceMemorySize = 0)
{
	return getSafeObjectCount(getPlatformMemoryLimits(context), objectSystemMemoryUsage, objectDeviceMemorySize);
}

VkDeviceSize getPageTableSize (Context& context, VkDeviceSize allocationSize)
{
	return getPageTableSize(getPlatformMemoryLimits(context), allocationSize);
}

template<typename Object>
deUint32 getSafeObjectCount (Context&							context,
							 const typename Object::Parameters&	params,
							 deUint32							hardCountLimit,
							 VkDeviceSize						deviceMemoryUsage = 0)
{
	return (deUint32)de::min((size_t)hardCountLimit,
							 getSafeObjectCount(context,
												computeSystemMemoryUsage<Object>(context, params),
												deviceMemoryUsage));
}

// Object definitions

enum
{
	MAX_CONCURRENT_INSTANCES		= 32,
	MAX_CONCURRENT_DEVICES			= 32,
	MAX_CONCURRENT_SYNC_PRIMITIVES	= 100,
	MAX_CONCURRENT_PIPELINE_CACHES	= 128,
	MAX_CONCURRENT_QUERY_POOLS		= 8192,
	DEFAULT_MAX_CONCURRENT_OBJECTS	= 16*1024,
};

struct Instance
{
	typedef VkInstance Type;

	struct Parameters
	{
		const vector<string>	instanceExtensions;

		Parameters (void) {}

		Parameters (vector<string>& extensions)
			: instanceExtensions	(extensions)
		{}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static deUint32 getMaxConcurrent (Context& context, const Parameters& params)
	{
		return getSafeObjectCount<Instance>(context, params, MAX_CONCURRENT_INSTANCES);
	}

	static Move<VkInstance> create (const Environment& env, const Resources&, const Parameters& params)
	{
		vector<const char*>					extensionNamePtrs;
		const vector<VkExtensionProperties>	instanceExts = enumerateInstanceExtensionProperties(env.vkp, DE_NULL);
		for (const auto& extName : params.instanceExtensions)
		{
			bool extNotInCore = !isCoreInstanceExtension(env.apiVersion, extName);
			if (extNotInCore && !isExtensionStructSupported(instanceExts.begin(), instanceExts.end(), RequiredExtension(extName)))
				TCU_THROW(NotSupportedError, (extName + " is not supported").c_str());

			if (extNotInCore)
				extensionNamePtrs.push_back(extName.c_str());
		}

		const VkApplicationInfo		appInfo			=
		{
			VK_STRUCTURE_TYPE_APPLICATION_INFO,
			DE_NULL,
			DE_NULL,							// pApplicationName
			0u,									// applicationVersion
			DE_NULL,							// pEngineName
			0u,									// engineVersion
			env.apiVersion
		};

		const VkInstanceCreateInfo	instanceInfo	=
		{
			VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			DE_NULL,
			(VkInstanceCreateFlags)0,
			&appInfo,
			0u,																// enabledLayerNameCount
			DE_NULL,														// ppEnabledLayerNames
			(deUint32)extensionNamePtrs.size(),								// enabledExtensionNameCount
			extensionNamePtrs.empty() ? DE_NULL : &extensionNamePtrs[0],	// ppEnabledExtensionNames
		};

		return createInstance(env.vkp, &instanceInfo, env.allocationCallbacks);
	}
};

struct Device
{
	typedef VkDevice Type;

	struct Parameters
	{
		deUint32		deviceIndex;
		VkQueueFlags	queueFlags;

		Parameters (deUint32 deviceIndex_, VkQueueFlags queueFlags_)
			: deviceIndex	(deviceIndex_)
			, queueFlags	(queueFlags_)
		{}
	};

	struct Resources
	{
		Dependency<Instance>	instance;
#ifndef CTS_USES_VULKANSC
		InstanceDriver			vki;
#else
		InstanceDriverSC		vki;
#endif // CTS_USES_VULKANSC

		VkPhysicalDevice		physicalDevice;
		deUint32				queueFamilyIndex;

		Resources (const Environment& env, const Parameters& params)
			: instance			(env, Instance::Parameters())
#ifndef CTS_USES_VULKANSC
			, vki(env.vkp, *instance.object)
#else
			, vki(env.vkp, *instance.object, env.commandLine, env.resourceInterface)
#endif // CTS_USES_VULKANSC
			, physicalDevice	(0)
			, queueFamilyIndex	(~0u)
		{
			{
				const vector<VkPhysicalDevice>	physicalDevices	= enumeratePhysicalDevices(vki, *instance.object);

				if (physicalDevices.size() <= (size_t)params.deviceIndex)
					TCU_THROW(NotSupportedError, "Device not found");

				physicalDevice = physicalDevices[params.deviceIndex];
			}

			{
				const vector<VkQueueFamilyProperties>	queueProps		= getPhysicalDeviceQueueFamilyProperties(vki, physicalDevice);
				bool									foundMatching	= false;

				for (size_t curQueueNdx = 0; curQueueNdx < queueProps.size(); curQueueNdx++)
				{
					if ((queueProps[curQueueNdx].queueFlags & params.queueFlags) == params.queueFlags)
					{
						queueFamilyIndex	= (deUint32)curQueueNdx;
						foundMatching		= true;
					}
				}

				if (!foundMatching)
					TCU_THROW(NotSupportedError, "Matching queue not found");
			}
		}
	};

	static deUint32 getMaxConcurrent (Context& context, const Parameters& params)
	{
		return getSafeObjectCount<Device>(context, params, MAX_CONCURRENT_DEVICES);
	}

	static Move<VkDevice> create (const Environment& env, const Resources& res, const Parameters&)
	{
		const float	queuePriority	= 1.0;

		const VkDeviceQueueCreateInfo	queues[]	=
		{
			{
				VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				DE_NULL,
				(VkDeviceQueueCreateFlags)0,
				res.queueFamilyIndex,
				1u,									// queueCount
				&queuePriority,						// pQueuePriorities
			}
		};

		void* pNext									= DE_NULL;
#ifdef CTS_USES_VULKANSC
		VkDeviceObjectReservationCreateInfo memReservationInfo	= env.commandLine.isSubProcess() ? env.resourceInterface->getStatMax() : resetDeviceObjectReservationCreateInfo();
		memReservationInfo.pNext								= pNext;
		pNext													= &memReservationInfo;

		VkPhysicalDeviceVulkanSC10Features sc10Features			= createDefaultSC10Features();
		sc10Features.pNext										= pNext;
		pNext													= &sc10Features;

		VkPipelineCacheCreateInfo			pcCI;
		std::vector<VkPipelinePoolSize>		poolSizes;
		if (env.commandLine.isSubProcess())
		{
			if (env.resourceInterface->getCacheDataSize() > 0)
			{
				pcCI =
				{
					VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,		// VkStructureType				sType;
					DE_NULL,											// const void*					pNext;
					VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
						VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT,	// VkPipelineCacheCreateFlags	flags;
					env.resourceInterface->getCacheDataSize(),			// deUintptr					initialDataSize;
					env.resourceInterface->getCacheData()				// const void*					pInitialData;
				};
				memReservationInfo.pipelineCacheCreateInfoCount		= 1;
				memReservationInfo.pPipelineCacheCreateInfos		= &pcCI;
			}

			poolSizes							= env.resourceInterface->getPipelinePoolSizes();
			if (!poolSizes.empty())
			{
				memReservationInfo.pipelinePoolSizeCount			= deUint32(poolSizes.size());
				memReservationInfo.pPipelinePoolSizes				= poolSizes.data();
			}
		}
#endif // CTS_USES_VULKANSC

		VkPhysicalDeviceFeatures enabledFeatures = getPhysicalDeviceFeatures(res.vki, res.physicalDevice);

		const VkDeviceCreateInfo		deviceInfo	=
		{
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			pNext,
			(VkDeviceCreateFlags)0,
			DE_LENGTH_OF_ARRAY(queues),
			queues,
			0u,										// enabledLayerNameCount
			DE_NULL,								// ppEnabledLayerNames
			0u,										// enabledExtensionNameCount
			DE_NULL,								// ppEnabledExtensionNames
			&enabledFeatures,						// pEnabledFeatures
		};

		return createCustomDevice(env.commandLine.isValidationEnabled(), env.vkp, env.instance, res.vki, res.physicalDevice, &deviceInfo, env.allocationCallbacks);
	}
};


struct DeviceGroup
{
	typedef VkDevice Type;

	struct Parameters
	{
		deUint32		deviceGroupIndex;
		deUint32		deviceIndex;
		VkQueueFlags	queueFlags;

		Parameters (deUint32 deviceGroupIndex_, deUint32 deviceIndex_, VkQueueFlags queueFlags_)
			: deviceGroupIndex	(deviceGroupIndex_)
			, deviceIndex		(deviceIndex_)
			, queueFlags		(queueFlags_)
		{}
	};

	struct Resources
	{
		vector<string>				extensions;
		Dependency<Instance>		instance;
#ifndef CTS_USES_VULKANSC
		InstanceDriver				vki;
#else
		InstanceDriverSC			vki;
#endif
		vector<VkPhysicalDevice>	physicalDevices;
		deUint32					physicalDeviceCount;
		deUint32					queueFamilyIndex;

		Resources (const Environment& env, const Parameters& params)
			: extensions			(1, "VK_KHR_device_group_creation")
			, instance				(env, Instance::Parameters(extensions))
#ifndef CTS_USES_VULKANSC
			, vki					(env.vkp, *instance.object)
#else
			, vki					(env.vkp, *instance.object, env.commandLine, env.resourceInterface)
#endif
			, physicalDeviceCount	(0)
			, queueFamilyIndex		(~0u)
		{
			{
				const vector<VkPhysicalDeviceGroupProperties> devGroupProperties = enumeratePhysicalDeviceGroups(vki, *instance.object);

				if (devGroupProperties.size() <= (size_t)params.deviceGroupIndex)
					TCU_THROW(NotSupportedError, "Device Group not found");

				physicalDeviceCount	= devGroupProperties[params.deviceGroupIndex].physicalDeviceCount;
				physicalDevices.resize(physicalDeviceCount);

				for (deUint32 physicalDeviceIdx = 0; physicalDeviceIdx < physicalDeviceCount; physicalDeviceIdx++)
					physicalDevices[physicalDeviceIdx] = devGroupProperties[params.deviceGroupIndex].physicalDevices[physicalDeviceIdx];
			}

			{
				const vector<VkQueueFamilyProperties>	queueProps = getPhysicalDeviceQueueFamilyProperties(vki, physicalDevices[params.deviceIndex]);
				bool									foundMatching = false;

				for (size_t curQueueNdx = 0; curQueueNdx < queueProps.size(); curQueueNdx++)
				{
					if ((queueProps[curQueueNdx].queueFlags & params.queueFlags) == params.queueFlags)
					{
						queueFamilyIndex = (deUint32)curQueueNdx;
						foundMatching = true;
					}
				}

				if (!foundMatching)
					TCU_THROW(NotSupportedError, "Matching queue not found");
			}
		}
	};

	static deUint32 getMaxConcurrent (Context& context, const Parameters& params)
	{
		return getSafeObjectCount<DeviceGroup>(context, params, MAX_CONCURRENT_DEVICES);
	}

	static Move<VkDevice> create (const Environment& env, const Resources& res, const Parameters& params)
	{
		const float	queuePriority = 1.0;

		const VkDeviceQueueCreateInfo	queues[] =
		{
			{
				VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				DE_NULL,							// pNext
				(VkDeviceQueueCreateFlags)0,		// flags
				res.queueFamilyIndex,				// queueFamilyIndex
				1u,									// queueCount
				&queuePriority,						// pQueuePriorities
			}
		};

		VkDeviceGroupDeviceCreateInfo deviceGroupInfo =
		{
			VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO,	//stype
			DE_NULL,											//pNext
			res.physicalDeviceCount,							//physicalDeviceCount
			res.physicalDevices.data()							//physicalDevices
		};

		void* pNext									= &deviceGroupInfo;
#ifdef CTS_USES_VULKANSC
		VkDeviceObjectReservationCreateInfo memReservationInfo	= env.commandLine.isSubProcess() ? env.resourceInterface->getStatMax() : resetDeviceObjectReservationCreateInfo();
		memReservationInfo.pNext								= pNext;
		pNext													= &memReservationInfo;

		VkPhysicalDeviceVulkanSC10Features sc10Features			= createDefaultSC10Features();
		sc10Features.pNext										= pNext;
		pNext													= &sc10Features;

		VkPipelineCacheCreateInfo			pcCI;
		std::vector<VkPipelinePoolSize>		poolSizes;
		if (env.commandLine.isSubProcess())
		{
			if (env.resourceInterface->getCacheDataSize() > 0)
			{
				pcCI =
				{
					VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,		// VkStructureType				sType;
					DE_NULL,											// const void*					pNext;
					VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
						VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT,	// VkPipelineCacheCreateFlags	flags;
					env.resourceInterface->getCacheDataSize(),			// deUintptr					initialDataSize;
					env.resourceInterface->getCacheData()				// const void*					pInitialData;
				};
				memReservationInfo.pipelineCacheCreateInfoCount		= 1;
				memReservationInfo.pPipelineCacheCreateInfos		= &pcCI;
			}

			poolSizes							= env.resourceInterface->getPipelinePoolSizes();
			if (!poolSizes.empty())
			{
				memReservationInfo.pipelinePoolSizeCount			= deUint32(poolSizes.size());
				memReservationInfo.pPipelinePoolSizes				= poolSizes.data();
			}
		}
#endif // CTS_USES_VULKANSC

		VkPhysicalDeviceFeatures enabledFeatures = getPhysicalDeviceFeatures(res.vki, res.physicalDevices[params.deviceIndex]);

		const VkDeviceCreateInfo			deviceGroupCreateInfo =
		{
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			pNext,
			(VkDeviceCreateFlags)0,
			DE_LENGTH_OF_ARRAY(queues),
			queues,
			0u,													// enabledLayerNameCount
			DE_NULL,											// ppEnabledLayerNames
			0u,													// enabledExtensionNameCount
			DE_NULL,											// ppEnabledExtensionNames
			&enabledFeatures,									// pEnabledFeatures
		};

		return createCustomDevice(env.commandLine.isValidationEnabled(), env.vkp, env.instance, res.vki, res.physicalDevices[params.deviceIndex], &deviceGroupCreateInfo, env.allocationCallbacks);
	}
};

struct DeviceMemory
{
	typedef VkDeviceMemory Type;

	struct Parameters
	{
		VkDeviceSize	size;
		deUint32		memoryTypeIndex;

		Parameters (VkDeviceSize size_, deUint32 memoryTypeIndex_)
			: size				(size_)
			, memoryTypeIndex	(memoryTypeIndex_)
		{
			DE_ASSERT(memoryTypeIndex < VK_MAX_MEMORY_TYPES);
		}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static deUint32 getMaxConcurrent (Context& context, const Parameters& params)
	{
		const VkDeviceSize	deviceMemoryUsage	= params.size + getPageTableSize(context, params.size);

		return getSafeObjectCount<DeviceMemory>(context,
												params,
												de::min(context.getDeviceProperties().limits.maxMemoryAllocationCount,
														(deUint32)DEFAULT_MAX_CONCURRENT_OBJECTS),
												deviceMemoryUsage);
	}

	static Move<VkDeviceMemory> create (const Environment& env, const Resources&, const Parameters& params)
	{
		const VkMemoryAllocateInfo	allocInfo	=
		{
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			DE_NULL,
			params.size,
			params.memoryTypeIndex
		};

		return allocateMemory(env.vkd, env.device, &allocInfo, env.allocationCallbacks);
	}
};

DeviceMemory::Parameters getDeviceMemoryParameters (const VkMemoryRequirements& memReqs)
{
	return DeviceMemory::Parameters(memReqs.size, deCtz32(memReqs.memoryTypeBits));
}

DeviceMemory::Parameters getDeviceMemoryParameters (const Environment& env, VkImage image)
{
	return getDeviceMemoryParameters(getImageMemoryRequirements(env.vkd, env.device, image));
}

DeviceMemory::Parameters getDeviceMemoryParameters (const Environment& env, VkBuffer image)
{
	return getDeviceMemoryParameters(getBufferMemoryRequirements(env.vkd, env.device, image));
}

struct Buffer
{
	typedef VkBuffer Type;

	struct Parameters
	{
		VkDeviceSize		size;
		VkBufferUsageFlags	usage;

		Parameters (VkDeviceSize		size_,
					VkBufferUsageFlags	usage_)
			: size	(size_)
			, usage	(usage_)
		{}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static deUint32 getMaxConcurrent (Context& context, const Parameters& params)
	{
		const Environment			env		(context, 1u);
		const Resources				res		(env, params);
		const Unique<VkBuffer>		buffer	(create(env, res, params));
		const VkMemoryRequirements	memReqs	= getBufferMemoryRequirements(env.vkd, env.device, *buffer);

		return getSafeObjectCount<Buffer>(context,
										  params,
										  DEFAULT_MAX_CONCURRENT_OBJECTS,
										  getPageTableSize(context, memReqs.size));
	}

	static Move<VkBuffer> create (const Environment& env, const Resources&, const Parameters& params)
	{
		const VkBufferCreateInfo	bufferInfo	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			DE_NULL,
			(VkBufferCreateFlags)0,
			params.size,
			params.usage,
			VK_SHARING_MODE_EXCLUSIVE,
			1u,
			&env.queueFamilyIndex
		};

		return createBuffer(env.vkd, env.device, &bufferInfo, env.allocationCallbacks);
	}
};

struct BufferView
{
	typedef VkBufferView Type;

	struct Parameters
	{
		Buffer::Parameters	buffer;
		VkFormat			format;
		VkDeviceSize		offset;
		VkDeviceSize		range;

		Parameters (const Buffer::Parameters&	buffer_,
					VkFormat					format_,
					VkDeviceSize				offset_,
					VkDeviceSize				range_)
			: buffer	(buffer_)
			, format	(format_)
			, offset	(offset_)
			, range		(range_)
		{}
	};

	struct Resources
	{
		Dependency<Buffer>			buffer;
		Dependency<DeviceMemory>	memory;

		Resources (const Environment& env, const Parameters& params)
			: buffer(env, params.buffer)
			, memory(env, getDeviceMemoryParameters(env, *buffer.object))
		{
			VK_CHECK(env.vkd.bindBufferMemory(env.device, *buffer.object, *memory.object, 0));
		}
	};

	static deUint32 getMaxConcurrent (Context& context, const Parameters& params)
	{
		return getSafeObjectCount<BufferView>(context, params, DEFAULT_MAX_CONCURRENT_OBJECTS);
	}

	static Move<VkBufferView> create (const Environment& env, const Resources& res, const Parameters& params)
	{
		const VkBufferViewCreateInfo	bufferViewInfo	=
		{
			VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
			DE_NULL,
			(VkBufferViewCreateFlags)0,
			*res.buffer.object,
			params.format,
			params.offset,
			params.range
		};

		return createBufferView(env.vkd, env.device, &bufferViewInfo, env.allocationCallbacks);
	}
};

struct Image
{
	typedef VkImage Type;

	struct Parameters
	{
		VkImageCreateFlags		flags;
		VkImageType				imageType;
		VkFormat				format;
		VkExtent3D				extent;
		deUint32				mipLevels;
		deUint32				arraySize;
		VkSampleCountFlagBits	samples;
		VkImageTiling			tiling;
		VkImageUsageFlags		usage;
		VkImageLayout			initialLayout;

		Parameters (VkImageCreateFlags		flags_,
					VkImageType				imageType_,
					VkFormat				format_,
					VkExtent3D				extent_,
					deUint32				mipLevels_,
					deUint32				arraySize_,
					VkSampleCountFlagBits	samples_,
					VkImageTiling			tiling_,
					VkImageUsageFlags		usage_,
					VkImageLayout			initialLayout_)
			: flags			(flags_)
			, imageType		(imageType_)
			, format		(format_)
			, extent		(extent_)
			, mipLevels		(mipLevels_)
			, arraySize		(arraySize_)
			, samples		(samples_)
			, tiling		(tiling_)
			, usage			(usage_)
			, initialLayout	(initialLayout_)
		{}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static deUint32 getMaxConcurrent (Context& context, const Parameters& params)
	{
		const Environment			env		(context, 1u);
		const Resources				res		(env, params);
		const Unique<VkImage>		image	(create(env, res, params));
		const VkMemoryRequirements	memReqs	= getImageMemoryRequirements(env.vkd, env.device, *image);

		return getSafeObjectCount<Image>(context,
										 params,
										 DEFAULT_MAX_CONCURRENT_OBJECTS,
										 getPageTableSize(context, memReqs.size));
	}

	static Move<VkImage> create (const Environment& env, const Resources&, const Parameters& params)
	{
		const VkImageCreateInfo		imageInfo	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			DE_NULL,
			params.flags,
			params.imageType,
			params.format,
			params.extent,
			params.mipLevels,
			params.arraySize,
			params.samples,
			params.tiling,
			params.usage,
			VK_SHARING_MODE_EXCLUSIVE,		// sharingMode
			1u,								// queueFamilyIndexCount
			&env.queueFamilyIndex,			// pQueueFamilyIndices
			params.initialLayout
		};

		return createImage(env.vkd, env.device, &imageInfo, env.allocationCallbacks);
	}
};

struct ImageView
{
	typedef VkImageView Type;

	struct Parameters
	{
		Image::Parameters		image;
		VkImageViewType			viewType;
		VkFormat				format;
		VkComponentMapping		components;
		VkImageSubresourceRange	subresourceRange;

		Parameters (const Image::Parameters&	image_,
					VkImageViewType				viewType_,
					VkFormat					format_,
					VkComponentMapping			components_,
					VkImageSubresourceRange		subresourceRange_)
			: image				(image_)
			, viewType			(viewType_)
			, format			(format_)
			, components		(components_)
			, subresourceRange	(subresourceRange_)
		{}
	};

	struct Resources
	{
		Dependency<Image>			image;
		Dependency<DeviceMemory>	memory;

		Resources (const Environment& env, const Parameters& params)
			: image	(env, params.image)
			, memory(env, getDeviceMemoryParameters(env, *image.object))
		{
			VK_CHECK(env.vkd.bindImageMemory(env.device, *image.object, *memory.object, 0));
		}
	};

	static deUint32 getMaxConcurrent (Context& context, const Parameters& params)
	{
		return getSafeObjectCount<ImageView>(context, params, DEFAULT_MAX_CONCURRENT_OBJECTS);
	}

	static Move<VkImageView> create (const Environment& env, const Resources& res, const Parameters& params)
	{
		const VkImageViewCreateInfo	imageViewInfo	=
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			DE_NULL,
			(VkImageViewCreateFlags)0,
			*res.image.object,
			params.viewType,
			params.format,
			params.components,
			params.subresourceRange,
		};

		return createImageView(env.vkd, env.device, &imageViewInfo, env.allocationCallbacks);
	}
};

struct Semaphore
{
	typedef VkSemaphore Type;

	struct Parameters
	{
		VkSemaphoreCreateFlags	flags;

		Parameters (VkSemaphoreCreateFlags flags_)
			: flags(flags_)
		{}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static deUint32 getMaxConcurrent (Context& context, const Parameters& params)
	{
		return getSafeObjectCount<Semaphore>(context, params, MAX_CONCURRENT_SYNC_PRIMITIVES);
	}

	static Move<VkSemaphore> create (const Environment& env, const Resources&, const Parameters& params)
	{
		const VkSemaphoreCreateInfo	semaphoreInfo	=
		{
			VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			DE_NULL,
			params.flags
		};

		return createSemaphore(env.vkd, env.device, &semaphoreInfo, env.allocationCallbacks);
	}
};

struct Fence
{
	typedef VkFence Type;

	struct Parameters
	{
		VkFenceCreateFlags	flags;

		Parameters (VkFenceCreateFlags flags_)
			: flags(flags_)
		{}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static deUint32 getMaxConcurrent (Context& context, const Parameters& params)
	{
		return getSafeObjectCount<Fence>(context, params, MAX_CONCURRENT_SYNC_PRIMITIVES);
	}

	static Move<VkFence> create (const Environment& env, const Resources&, const Parameters& params)
	{
		const VkFenceCreateInfo	fenceInfo	=
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			DE_NULL,
			params.flags
		};

		return createFence(env.vkd, env.device, &fenceInfo, env.allocationCallbacks);
	}
};

struct Event
{
	typedef VkEvent Type;

	struct Parameters
	{
		VkEventCreateFlags	flags;

		Parameters (VkEventCreateFlags flags_)
			: flags(flags_)
		{}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static deUint32 getMaxConcurrent (Context& context, const Parameters& params)
	{
		return getSafeObjectCount<Event>(context, params, MAX_CONCURRENT_SYNC_PRIMITIVES);
	}

	static Move<VkEvent> create (const Environment& env, const Resources&, const Parameters& params)
	{
		const VkEventCreateInfo	eventInfo	=
		{
			VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
			DE_NULL,
			params.flags
		};

		return createEvent(env.vkd, env.device, &eventInfo, env.allocationCallbacks);
	}
};

struct QueryPool
{
	typedef VkQueryPool Type;

	struct Parameters
	{
		VkQueryType						queryType;
		deUint32						entryCount;
		VkQueryPipelineStatisticFlags	pipelineStatistics;

		Parameters (VkQueryType						queryType_,
					deUint32						entryCount_,
					VkQueryPipelineStatisticFlags	pipelineStatistics_)
			: queryType				(queryType_)
			, entryCount			(entryCount_)
			, pipelineStatistics	(pipelineStatistics_)
		{}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static deUint32 getMaxConcurrent (Context& context, const Parameters& params)
	{
		return getSafeObjectCount<QueryPool>(context, params, MAX_CONCURRENT_QUERY_POOLS);
	}

	static Move<VkQueryPool> create (const Environment& env, const Resources&, const Parameters& params)
	{
		const VkQueryPoolCreateInfo	queryPoolInfo	=
		{
			VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
			DE_NULL,
			(VkQueryPoolCreateFlags)0,
			params.queryType,
			params.entryCount,
			params.pipelineStatistics
		};

		return createQueryPool(env.vkd, env.device, &queryPoolInfo, env.allocationCallbacks);
	}
};

struct ShaderModule
{
	typedef VkShaderModule Type;

	struct Parameters
	{
		VkShaderStageFlagBits	shaderStage;
		string					binaryName;

		Parameters (VkShaderStageFlagBits	shaderStage_,
					const std::string&		binaryName_)
			: shaderStage	(shaderStage_)
			, binaryName	(binaryName_)
		{}
	};

	struct Resources
	{
		const ProgramBinary&	binary;

		Resources (const Environment& env, const Parameters& params)
			: binary(env.programBinaries.get(params.binaryName))
		{}
	};

	static deUint32 getMaxConcurrent (Context& context, const Parameters& params)
	{
		return getSafeObjectCount<ShaderModule>(context, params, DEFAULT_MAX_CONCURRENT_OBJECTS);
	}

	static const char* getSource (VkShaderStageFlagBits stage)
	{
		switch (stage)
		{
			case VK_SHADER_STAGE_VERTEX_BIT:
				return "#version 310 es\n"
					   "layout(location = 0) in highp vec4 a_position;\n"
					   "void main () { gl_Position = a_position; }\n";

			case VK_SHADER_STAGE_FRAGMENT_BIT:
				return "#version 310 es\n"
					   "layout(location = 0) out mediump vec4 o_color;\n"
					   "void main () { o_color = vec4(1.0, 0.5, 0.25, 1.0); }";

			case VK_SHADER_STAGE_COMPUTE_BIT:
				return "#version 310 es\n"
					   "layout(binding = 0) buffer Input { highp uint dataIn[]; };\n"
					   "layout(binding = 1) buffer Output { highp uint dataOut[]; };\n"
					   "void main (void)\n"
					   "{\n"
					   "	dataOut[gl_GlobalInvocationID.x] = ~dataIn[gl_GlobalInvocationID.x];\n"
					   "}\n";

			default:
				DE_FATAL("Not implemented");
				return DE_NULL;
		}
	}

	static void initPrograms (SourceCollections& dst, Parameters params)
	{
		const char* const	source	= getSource(params.shaderStage);

		DE_ASSERT(source);

		dst.glslSources.add(params.binaryName)
			<< glu::ShaderSource(getGluShaderType(params.shaderStage), source);
	}

	static Move<VkShaderModule> create (const Environment& env, const Resources& res, const Parameters&)
	{
		const VkShaderModuleCreateInfo	shaderModuleInfo	=
		{
			VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			DE_NULL,
			(VkShaderModuleCreateFlags)0,
			res.binary.getSize(),
			(const deUint32*)res.binary.getBinary(),
		};

		return createShaderModule(env.vkd, env.device, &shaderModuleInfo, env.allocationCallbacks);
	}
};

struct PipelineCache
{
	typedef VkPipelineCache Type;

	struct Parameters
	{
		Parameters (void) {}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static deUint32 getMaxConcurrent (Context& context, const Parameters& params)
	{
		return getSafeObjectCount<PipelineCache>(context, params, MAX_CONCURRENT_PIPELINE_CACHES);
	}

	static void initPrograms(SourceCollections& dst, Parameters)
	{
		ShaderModule::initPrograms(dst, ShaderModule::Parameters(VK_SHADER_STAGE_COMPUTE_BIT, "comp"));
	}

	static Move<VkPipelineCache> create (const Environment& env, const Resources&, const Parameters&)
	{
#ifdef CTS_USES_VULKANSC
		// creating dummy compute pipeline to ensure pipeline cache is not empty
		if (!env.commandLine.isSubProcess())
		{
			const Unique<VkShaderModule>			shaderModule				(createShaderModule(env.vkd, env.device, env.programBinaries.get("comp"), 0u));

			const Move<VkDescriptorSetLayout>		descriptorSetLayout			(DescriptorSetLayoutBuilder()
																					.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
																					.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
																					.build(env.vkd, env.device));

			const Move<VkPipelineLayout>			pipelineLayout				(makePipelineLayout(env.vkd, env.device, *descriptorSetLayout));

			const VkPipelineShaderStageCreateInfo	stageCreateInfo				=
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,			// VkStructureType                     sType;
				DE_NULL,														// const void*                         pNext;
				0u,																// VkPipelineShaderStageCreateFlags    flags;
				VK_SHADER_STAGE_COMPUTE_BIT,									// VkShaderStageFlagBits               stage;
				*shaderModule,													// VkShaderModule                      module;
				"main",															// const char*                         pName;
				DE_NULL,														// const VkSpecializationInfo*         pSpecializationInfo;
			};

			const VkComputePipelineCreateInfo		pipelineInfo				=
			{
				VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,					// VkStructureType					sType
				DE_NULL,														// const void*						pNext
				(VkPipelineCreateFlags)0,										// VkPipelineCreateFlags			flags
				stageCreateInfo,												// VkPipelineShaderStageCreateInfo	stage
				*pipelineLayout,												// VkPipelineLayout					layout
				DE_NULL,														// VkPipeline						basePipelineHandle
				0u																// deInt32							basePipelineIndex
			};

			Move<VkPipeline>						pipeline					= createComputePipeline(env.vkd, env.device, DE_NULL, &pipelineInfo);
		}
#endif // CTS_USES_VULKANSC
		const VkPipelineCacheCreateInfo	pipelineCacheInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,				// VkStructureType				sType;
			DE_NULL,													// const void*					pNext;
#ifndef CTS_USES_VULKANSC
			(VkPipelineCacheCreateFlags)0u,								// VkPipelineCacheCreateFlags	flags;
			0u,															// size_t						initialDataSize;
			DE_NULL,													// const void*					pInitialData;
#else
			VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
				VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT,	// VkPipelineCacheCreateFlags	flags;
			env.resourceInterface->getCacheDataSize(),					// deUintptr					initialDataSize;
			env.resourceInterface->getCacheData()						// const void*					pInitialData;
#endif // CTS_USES_VULKANSC
		};

		return createPipelineCache(env.vkd, env.device, &pipelineCacheInfo, env.allocationCallbacks);
	}
};

struct Sampler
{
	typedef VkSampler Type;

	struct Parameters
	{
		VkFilter				magFilter;
		VkFilter				minFilter;
		VkSamplerMipmapMode		mipmapMode;
		VkSamplerAddressMode	addressModeU;
		VkSamplerAddressMode	addressModeV;
		VkSamplerAddressMode	addressModeW;
		float					mipLodBias;
		VkBool32				anisotropyEnable;
		float					maxAnisotropy;
		VkBool32				compareEnable;
		VkCompareOp				compareOp;
		float					minLod;
		float					maxLod;
		VkBorderColor			borderColor;
		VkBool32				unnormalizedCoordinates;

		// \todo [2015-09-17 pyry] Other configurations
		Parameters (void)
			: magFilter					(VK_FILTER_NEAREST)
			, minFilter					(VK_FILTER_NEAREST)
			, mipmapMode				(VK_SAMPLER_MIPMAP_MODE_NEAREST)
			, addressModeU				(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
			, addressModeV				(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
			, addressModeW				(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
			, mipLodBias				(0.0f)
			, anisotropyEnable			(VK_FALSE)
			, maxAnisotropy				(1.0f)
			, compareEnable				(VK_FALSE)
			, compareOp					(VK_COMPARE_OP_ALWAYS)
			, minLod					(-1000.f)
			, maxLod					(+1000.f)
			, borderColor				(VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK)
			, unnormalizedCoordinates	(VK_FALSE)
		{}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static deUint32 getMaxConcurrent (Context& context, const Parameters& params)
	{
		return getSafeObjectCount<Sampler>(context,
										   params,
										   de::min(context.getDeviceProperties().limits.maxSamplerAllocationCount,
												   (deUint32)DEFAULT_MAX_CONCURRENT_OBJECTS));
	}

	static Move<VkSampler> create (const Environment& env, const Resources&, const Parameters& params)
	{
		const VkSamplerCreateInfo	samplerInfo	=
		{
			VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			DE_NULL,
			(VkSamplerCreateFlags)0,
			params.magFilter,
			params.minFilter,
			params.mipmapMode,
			params.addressModeU,
			params.addressModeV,
			params.addressModeW,
			params.mipLodBias,
			params.anisotropyEnable,
			params.maxAnisotropy,
			params.compareEnable,
			params.compareOp,
			params.minLod,
			params.maxLod,
			params.borderColor,
			params.unnormalizedCoordinates
		};

		return createSampler(env.vkd, env.device, &samplerInfo, env.allocationCallbacks);
	}
};

struct DescriptorSetLayout
{
	typedef VkDescriptorSetLayout Type;

	struct Parameters
	{
		struct Binding
		{
			deUint32			binding;
			VkDescriptorType	descriptorType;
			deUint32			descriptorCount;
			VkShaderStageFlags	stageFlags;
			bool				useImmutableSampler;

			Binding (deUint32			binding_,
					 VkDescriptorType	descriptorType_,
					 deUint32			descriptorCount_,
					 VkShaderStageFlags	stageFlags_,
					 bool				useImmutableSampler_)
				: binding				(binding_)
				, descriptorType		(descriptorType_)
				, descriptorCount		(descriptorCount_)
				, stageFlags			(stageFlags_)
				, useImmutableSampler	(useImmutableSampler_)
			{}

			Binding (void) {}
		};

		vector<Binding>	bindings;

		Parameters (const vector<Binding>& bindings_)
			: bindings(bindings_)
		{}

		static Parameters empty (void)
		{
			return Parameters(vector<Binding>());
		}

		static Parameters single (deUint32				binding,
								  VkDescriptorType		descriptorType,
								  deUint32				descriptorCount,
								  VkShaderStageFlags	stageFlags,
								  bool					useImmutableSampler = false)
		{
			vector<Binding> bindings;
			bindings.push_back(Binding(binding, descriptorType, descriptorCount, stageFlags, useImmutableSampler));
			return Parameters(bindings);
		}
	};

	struct Resources
	{
		vector<VkDescriptorSetLayoutBinding>	bindings;
		MovePtr<Dependency<Sampler> >			immutableSampler;
		vector<VkSampler>						immutableSamplersPtr;

		Resources (const Environment& env, const Parameters& params)
		{
			// Create immutable sampler if needed
			for (vector<Parameters::Binding>::const_iterator cur = params.bindings.begin(); cur != params.bindings.end(); cur++)
			{
				if (cur->useImmutableSampler && !immutableSampler)
				{
					immutableSampler = de::newMovePtr<Dependency<Sampler> >(env, Sampler::Parameters());

					if (cur->useImmutableSampler && immutableSamplersPtr.size() < (size_t)cur->descriptorCount)
						immutableSamplersPtr.resize(cur->descriptorCount, *immutableSampler->object);
				}
			}

			for (vector<Parameters::Binding>::const_iterator cur = params.bindings.begin(); cur != params.bindings.end(); cur++)
			{
				const VkDescriptorSetLayoutBinding	binding	=
				{
					cur->binding,
					cur->descriptorType,
					cur->descriptorCount,
					cur->stageFlags,
					(cur->useImmutableSampler ? &immutableSamplersPtr[0] : DE_NULL)
				};

				bindings.push_back(binding);
			}
		}
	};

	static deUint32 getMaxConcurrent (Context& context, const Parameters& params)
	{
		return getSafeObjectCount<DescriptorSetLayout>(context, params, DEFAULT_MAX_CONCURRENT_OBJECTS);
	}

	static Move<VkDescriptorSetLayout> create (const Environment& env, const Resources& res, const Parameters&)
	{
		const VkDescriptorSetLayoutCreateInfo	descriptorSetLayoutInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			DE_NULL,
			(VkDescriptorSetLayoutCreateFlags)0,
			(deUint32)res.bindings.size(),
			(res.bindings.empty() ? DE_NULL : &res.bindings[0])
		};

		return createDescriptorSetLayout(env.vkd, env.device, &descriptorSetLayoutInfo, env.allocationCallbacks);
	}
};

struct PipelineLayout
{
	typedef VkPipelineLayout Type;

	struct Parameters
	{
		vector<DescriptorSetLayout::Parameters>	descriptorSetLayouts;
		vector<VkPushConstantRange>				pushConstantRanges;

		Parameters (void) {}

		static Parameters empty (void)
		{
			return Parameters();
		}

		static Parameters singleDescriptorSet (const DescriptorSetLayout::Parameters& descriptorSetLayout)
		{
			Parameters params;
			params.descriptorSetLayouts.push_back(descriptorSetLayout);
			return params;
		}
	};

	struct Resources
	{
		typedef SharedPtr<Dependency<DescriptorSetLayout> >	DescriptorSetLayoutDepSp;
		typedef vector<DescriptorSetLayoutDepSp>			DescriptorSetLayouts;

		DescriptorSetLayouts			descriptorSetLayouts;
		vector<VkDescriptorSetLayout>	pSetLayouts;

		Resources (const Environment& env, const Parameters& params)
		{
			for (vector<DescriptorSetLayout::Parameters>::const_iterator dsParams = params.descriptorSetLayouts.begin();
				 dsParams != params.descriptorSetLayouts.end();
				 ++dsParams)
			{
				descriptorSetLayouts.push_back(DescriptorSetLayoutDepSp(new Dependency<DescriptorSetLayout>(env, *dsParams)));
				pSetLayouts.push_back(*descriptorSetLayouts.back()->object);
			}
		}
	};

	static deUint32 getMaxConcurrent (Context& context, const Parameters& params)
	{
		return getSafeObjectCount<PipelineLayout>(context, params, DEFAULT_MAX_CONCURRENT_OBJECTS);
	}

	static Move<VkPipelineLayout> create (const Environment& env, const Resources& res, const Parameters& params)
	{
		const VkPipelineLayoutCreateInfo	pipelineLayoutInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			DE_NULL,
			(VkPipelineLayoutCreateFlags)0,
			(deUint32)res.pSetLayouts.size(),
			(res.pSetLayouts.empty() ? DE_NULL : &res.pSetLayouts[0]),
			(deUint32)params.pushConstantRanges.size(),
			(params.pushConstantRanges.empty() ? DE_NULL : &params.pushConstantRanges[0]),
		};

		return createPipelineLayout(env.vkd, env.device, &pipelineLayoutInfo, env.allocationCallbacks);
	}
};

struct RenderPass
{
	typedef VkRenderPass Type;

	// \todo [2015-09-17 pyry] More interesting configurations
	struct Parameters
	{
		Parameters (void) {}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static deUint32 getMaxConcurrent (Context& context, const Parameters& params)
	{
		return getSafeObjectCount<RenderPass>(context, params, DEFAULT_MAX_CONCURRENT_OBJECTS);
	}

	static Move<VkRenderPass> create (const Environment& env, const Resources&, const Parameters&)
	{
		return makeRenderPass(env.vkd, env.device, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_D16_UNORM,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			env.allocationCallbacks);
	}
};

struct GraphicsPipeline
{
	typedef VkPipeline Type;

	// \todo [2015-09-17 pyry] More interesting configurations
	struct Parameters
	{
		Parameters (void) {}
	};

	struct Resources
	{
		Dependency<ShaderModule>	vertexShader;
		Dependency<ShaderModule>	fragmentShader;
		Dependency<PipelineLayout>	layout;
		Dependency<RenderPass>		renderPass;
		Dependency<PipelineCache>	pipelineCache;

		Resources (const Environment& env, const Parameters&)
			: vertexShader		(env, ShaderModule::Parameters(VK_SHADER_STAGE_VERTEX_BIT, "vert"))
			, fragmentShader	(env, ShaderModule::Parameters(VK_SHADER_STAGE_FRAGMENT_BIT, "frag"))
			, layout			(env, PipelineLayout::Parameters::singleDescriptorSet(
										DescriptorSetLayout::Parameters::single(0u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u, VK_SHADER_STAGE_FRAGMENT_BIT, true)))
			, renderPass		(env, RenderPass::Parameters())
			, pipelineCache		(env, PipelineCache::Parameters())
		{}
	};

	static deUint32 getMaxConcurrent (Context& context, const Parameters& params)
	{
		return getSafeObjectCount<GraphicsPipeline>(context, params, DEFAULT_MAX_CONCURRENT_OBJECTS);
	}

	static void initPrograms (SourceCollections& dst, Parameters)
	{
#ifdef CTS_USES_VULKANSC
		// Pipeline cache dependency uses compute shader to ensure that pipeline cache is not empty in subprocess.
		// We have to add this shader even if we don't plan to use it later in any *.graphics_pipeline test
		ShaderModule::initPrograms(dst, ShaderModule::Parameters(VK_SHADER_STAGE_COMPUTE_BIT, "comp"));
#endif // CTS_USES_VULKANSC
		ShaderModule::initPrograms(dst, ShaderModule::Parameters(VK_SHADER_STAGE_VERTEX_BIT, "vert"));
		ShaderModule::initPrograms(dst, ShaderModule::Parameters(VK_SHADER_STAGE_FRAGMENT_BIT, "frag"));
	}

	static vector<VkPipelineSp> createMultiple (const Environment& env, const Resources& res, const Parameters&, vector<VkPipeline>* const pOutHandles, VkResult* const pOutResult)
	{
		DE_ASSERT(pOutResult);
		DE_ASSERT(pOutHandles);
		DE_ASSERT(pOutHandles->size() != 0);

		const VkPipelineShaderStageCreateInfo			stages[]			=
		{
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				DE_NULL,
				(VkPipelineShaderStageCreateFlags)0,
				VK_SHADER_STAGE_VERTEX_BIT,
				*res.vertexShader.object,
				"main",
				DE_NULL,							// pSpecializationInfo
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				DE_NULL,
				(VkPipelineShaderStageCreateFlags)0,
				VK_SHADER_STAGE_FRAGMENT_BIT,
				*res.fragmentShader.object,
				"main",
				DE_NULL,							// pSpecializationInfo
			}
		};
		const VkVertexInputBindingDescription			vertexBindings[]	=
		{
			{
				0u,									// binding
				16u,								// stride
				VK_VERTEX_INPUT_RATE_VERTEX
			}
		};
		const VkVertexInputAttributeDescription			vertexAttribs[]		=
		{
			{
				0u,									// location
				0u,									// binding
				VK_FORMAT_R32G32B32A32_SFLOAT,
				0u,									// offset
			}
		};
		const VkPipelineVertexInputStateCreateInfo		vertexInputState	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			DE_NULL,
			(VkPipelineVertexInputStateCreateFlags)0,
			DE_LENGTH_OF_ARRAY(vertexBindings),
			vertexBindings,
			DE_LENGTH_OF_ARRAY(vertexAttribs),
			vertexAttribs
		};
		const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyState	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			DE_NULL,
			(VkPipelineInputAssemblyStateCreateFlags)0,
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			VK_FALSE								// primitiveRestartEnable
		};
		const VkViewport								viewport			= makeViewport(tcu::UVec2(64));
		const VkRect2D									scissor				= makeRect2D(tcu::UVec2(64));

		const VkPipelineViewportStateCreateInfo			viewportState		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			DE_NULL,
			(VkPipelineViewportStateCreateFlags)0,
			1u,
			&viewport,
			1u,
			&scissor,
		};
		const VkPipelineRasterizationStateCreateInfo	rasterState			=
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			DE_NULL,
			(VkPipelineRasterizationStateCreateFlags)0,
			VK_FALSE,								// depthClampEnable
			VK_FALSE,								// rasterizerDiscardEnable
			VK_POLYGON_MODE_FILL,
			VK_CULL_MODE_BACK_BIT,
			VK_FRONT_FACE_COUNTER_CLOCKWISE,
			VK_FALSE,								// depthBiasEnable
			0.0f,									// depthBiasConstantFactor
			0.0f,									// depthBiasClamp
			0.0f,									// depthBiasSlopeFactor
			1.0f,									// lineWidth
		};
		const VkPipelineMultisampleStateCreateInfo		multisampleState	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			DE_NULL,
			(VkPipelineMultisampleStateCreateFlags)0,
			VK_SAMPLE_COUNT_1_BIT,
			VK_FALSE,								// sampleShadingEnable
			1.0f,									// minSampleShading
			DE_NULL,								// pSampleMask
			VK_FALSE,								// alphaToCoverageEnable
			VK_FALSE,								// alphaToOneEnable
		};
		const VkPipelineDepthStencilStateCreateInfo		depthStencilState	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			DE_NULL,
			(VkPipelineDepthStencilStateCreateFlags)0,
			VK_TRUE,								// depthTestEnable
			VK_TRUE,								// depthWriteEnable
			VK_COMPARE_OP_LESS,						// depthCompareOp
			VK_FALSE,								// depthBoundsTestEnable
			VK_FALSE,								// stencilTestEnable
			{ VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0u, 0u, 0u },
			{ VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0u, 0u, 0u },
			0.0f,									// minDepthBounds
			1.0f,									// maxDepthBounds
		};
		const VkPipelineColorBlendAttachmentState		colorBlendAttState[]=
		{
			{
				VK_FALSE,							// blendEnable
				VK_BLEND_FACTOR_ONE,
				VK_BLEND_FACTOR_ZERO,
				VK_BLEND_OP_ADD,
				VK_BLEND_FACTOR_ONE,
				VK_BLEND_FACTOR_ZERO,
				VK_BLEND_OP_ADD,
				VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT
			}
		};
		const VkPipelineColorBlendStateCreateInfo		colorBlendState		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			DE_NULL,
			(VkPipelineColorBlendStateCreateFlags)0,
			VK_FALSE,								// logicOpEnable
			VK_LOGIC_OP_COPY,
			DE_LENGTH_OF_ARRAY(colorBlendAttState),
			colorBlendAttState,
			{ 0.0f, 0.0f, 0.0f, 0.0f }				// blendConstants
		};
		const VkGraphicsPipelineCreateInfo				pipelineInfo		=
		{
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			DE_NULL,
			(VkPipelineCreateFlags)0,
			DE_LENGTH_OF_ARRAY(stages),
			stages,
			&vertexInputState,
			&inputAssemblyState,
			DE_NULL,								// pTessellationState
			&viewportState,
			&rasterState,
			&multisampleState,
			&depthStencilState,
			&colorBlendState,
			(const VkPipelineDynamicStateCreateInfo*)DE_NULL,
			*res.layout.object,
			*res.renderPass.object,
			0u,										// subpass
			(VkPipeline)0,							// basePipelineHandle
			0,										// basePipelineIndex
		};

		const deUint32							numPipelines	= static_cast<deUint32>(pOutHandles->size());
		VkPipeline*	const						pHandles		= &(*pOutHandles)[0];
		vector<VkGraphicsPipelineCreateInfo>	pipelineInfos	(numPipelines, pipelineInfo);

		*pOutResult = env.vkd.createGraphicsPipelines(env.device, *res.pipelineCache.object, numPipelines, &pipelineInfos[0], env.allocationCallbacks, pHandles);

		vector<VkPipelineSp>	pipelines;

		// Even if an error is returned, some pipelines may have been created successfully
		for (deUint32 i = 0; i < numPipelines; ++i)
		{
			if (pHandles[i] != DE_NULL)
				pipelines.push_back(VkPipelineSp(new Move<VkPipeline>(check<VkPipeline>(pHandles[i]), Deleter<VkPipeline>(env.vkd, env.device, env.allocationCallbacks))));
		}

		return pipelines;
	}

	static Move<VkPipeline> create (const Environment& env, const Resources& res, const Parameters&)
	{
		vector<VkPipeline>		handles			(1, DE_NULL);
		VkResult				result			= VK_NOT_READY;
		vector<VkPipelineSp>	scopedHandles	= createMultiple(env, res, Parameters(), &handles, &result);

		VK_CHECK(result);
		return Move<VkPipeline>(check<VkPipeline>(scopedHandles.front()->disown()), Deleter<VkPipeline>(env.vkd, env.device, env.allocationCallbacks));
	}
};

struct ComputePipeline
{
	typedef VkPipeline Type;

	// \todo [2015-09-17 pyry] More interesting configurations
	struct Parameters
	{
		Parameters (void) {}
	};

	struct Resources
	{
		Dependency<ShaderModule>	shaderModule;
		Dependency<PipelineLayout>	layout;
		Dependency<PipelineCache>	pipelineCache;

		static DescriptorSetLayout::Parameters getDescriptorSetLayout (void)
		{
			typedef DescriptorSetLayout::Parameters::Binding Binding;

			vector<Binding> bindings;

			bindings.push_back(Binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, false));
			bindings.push_back(Binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, false));

			return DescriptorSetLayout::Parameters(bindings);
		}

		Resources (const Environment& env, const Parameters&)
			: shaderModule		(env, ShaderModule::Parameters(VK_SHADER_STAGE_COMPUTE_BIT, "comp"))
			, layout			(env, PipelineLayout::Parameters::singleDescriptorSet(getDescriptorSetLayout()))
			, pipelineCache		(env, PipelineCache::Parameters())
		{}
	};

	static deUint32 getMaxConcurrent (Context& context, const Parameters& params)
	{
		return getSafeObjectCount<ComputePipeline>(context, params, DEFAULT_MAX_CONCURRENT_OBJECTS);
	}

	static void initPrograms (SourceCollections& dst, Parameters)
	{
		ShaderModule::initPrograms(dst, ShaderModule::Parameters(VK_SHADER_STAGE_COMPUTE_BIT, "comp"));
	}

	static Move<VkPipeline> create (const Environment& env, const Resources& res, const Parameters&)
	{
		const VkComputePipelineCreateInfo	pipelineInfo	=
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			DE_NULL,
			(VkPipelineCreateFlags)0,
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				DE_NULL,
				(VkPipelineShaderStageCreateFlags)0,
				VK_SHADER_STAGE_COMPUTE_BIT,
				*res.shaderModule.object,
				"main",
				DE_NULL					// pSpecializationInfo
			},
			*res.layout.object,
			(VkPipeline)0,				// basePipelineHandle
			0u,							// basePipelineIndex
		};

		return createComputePipeline(env.vkd, env.device, *res.pipelineCache.object, &pipelineInfo, env.allocationCallbacks);
	}

	static vector<VkPipelineSp> createMultiple (const Environment& env, const Resources& res, const Parameters&, vector<VkPipeline>* const pOutHandles, VkResult* const pOutResult)
	{
		DE_ASSERT(pOutResult);
		DE_ASSERT(pOutHandles);
		DE_ASSERT(pOutHandles->size() != 0);

		const VkComputePipelineCreateInfo	commonPipelineInfo	=
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			DE_NULL,
			(VkPipelineCreateFlags)0,
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				DE_NULL,
				(VkPipelineShaderStageCreateFlags)0,
				VK_SHADER_STAGE_COMPUTE_BIT,
				*res.shaderModule.object,
				"main",
				DE_NULL					// pSpecializationInfo
			},
			*res.layout.object,
			(VkPipeline)0,				// basePipelineHandle
			0u,							// basePipelineIndex
		};

		const deUint32						numPipelines	= static_cast<deUint32>(pOutHandles->size());
		VkPipeline*	const					pHandles		= &(*pOutHandles)[0];
		vector<VkComputePipelineCreateInfo>	pipelineInfos	(numPipelines, commonPipelineInfo);

		*pOutResult = env.vkd.createComputePipelines(env.device, *res.pipelineCache.object, numPipelines, &pipelineInfos[0], env.allocationCallbacks, pHandles);

		vector<VkPipelineSp>	pipelines;

		// Even if an error is returned, some pipelines may have been created successfully
		for (deUint32 i = 0; i < numPipelines; ++i)
		{
			if (pHandles[i] != DE_NULL)
				pipelines.push_back(VkPipelineSp(new Move<VkPipeline>(check<VkPipeline>(pHandles[i]), Deleter<VkPipeline>(env.vkd, env.device, env.allocationCallbacks))));
		}

		return pipelines;
	}
};

struct DescriptorPool
{
	typedef VkDescriptorPool Type;

	struct Parameters
	{
		VkDescriptorPoolCreateFlags		flags;
		deUint32						maxSets;
		vector<VkDescriptorPoolSize>	poolSizes;

		Parameters (VkDescriptorPoolCreateFlags				flags_,
					deUint32								maxSets_,
					const vector<VkDescriptorPoolSize>&		poolSizes_)
			: flags		(flags_)
			, maxSets	(maxSets_)
			, poolSizes	(poolSizes_)
		{}

		static Parameters singleType (VkDescriptorPoolCreateFlags	flags,
									  deUint32						maxSets,
									  VkDescriptorType				type,
									  deUint32						count)
		{
			vector<VkDescriptorPoolSize> poolSizes;
			poolSizes.push_back(makeDescriptorPoolSize(type, count));
			return Parameters(flags, maxSets, poolSizes);
		}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static deUint32 getMaxConcurrent (Context& context, const Parameters& params)
	{
		return getSafeObjectCount<DescriptorPool>(context, params, DEFAULT_MAX_CONCURRENT_OBJECTS);
	}

	static Move<VkDescriptorPool> create (const Environment& env, const Resources&, const Parameters& params)
	{
		const VkDescriptorPoolCreateInfo	descriptorPoolInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			DE_NULL,
			params.flags,
			params.maxSets,
			(deUint32)params.poolSizes.size(),
			(params.poolSizes.empty() ? DE_NULL : &params.poolSizes[0])
		};

		return createDescriptorPool(env.vkd, env.device, &descriptorPoolInfo, env.allocationCallbacks);
	}
};

struct DescriptorSet
{
	typedef VkDescriptorSet Type;

	struct Parameters
	{
		DescriptorSetLayout::Parameters	descriptorSetLayout;

		Parameters (const DescriptorSetLayout::Parameters& descriptorSetLayout_)
			: descriptorSetLayout(descriptorSetLayout_)
		{}
	};

	struct Resources
	{
		Dependency<DescriptorPool>		descriptorPool;
		Dependency<DescriptorSetLayout>	descriptorSetLayout;

		static vector<VkDescriptorPoolSize> computePoolSizes (const DescriptorSetLayout::Parameters& layout, int maxSets)
		{
			deUint32						countByType[VK_DESCRIPTOR_TYPE_LAST];
			vector<VkDescriptorPoolSize>	typeCounts;

			std::fill(DE_ARRAY_BEGIN(countByType), DE_ARRAY_END(countByType), 0u);

			for (vector<DescriptorSetLayout::Parameters::Binding>::const_iterator cur = layout.bindings.begin();
				 cur != layout.bindings.end();
				 ++cur)
			{
				DE_ASSERT((deUint32)cur->descriptorType < VK_DESCRIPTOR_TYPE_LAST);
				countByType[cur->descriptorType] += cur->descriptorCount * maxSets;
			}

			for (deUint32 type = 0; type < VK_DESCRIPTOR_TYPE_LAST; ++type)
			{
				if (countByType[type] > 0)
					typeCounts.push_back(makeDescriptorPoolSize((VkDescriptorType)type, countByType[type]));
			}

			return typeCounts;
		}

		Resources (const Environment& env, const Parameters& params)
			: descriptorPool		(env, DescriptorPool::Parameters(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, env.maxResourceConsumers, computePoolSizes(params.descriptorSetLayout, env.maxResourceConsumers)))
			, descriptorSetLayout	(env, params.descriptorSetLayout)
		{
		}
	};

	static deUint32 getMaxConcurrent (Context& context, const Parameters& params)
	{
		return getSafeObjectCount<DescriptorSet>(context, params, DEFAULT_MAX_CONCURRENT_OBJECTS);
	}

	static Move<VkDescriptorSet> create (const Environment& env, const Resources& res, const Parameters&)
	{
		const VkDescriptorSetAllocateInfo	allocateInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			DE_NULL,
			*res.descriptorPool.object,
			1u,
			&res.descriptorSetLayout.object.get(),
		};

		return allocateDescriptorSet(env.vkd, env.device, &allocateInfo);
	}

	static vector<VkDescriptorSetSp> createMultiple (const Environment& env, const Resources& res, const Parameters&, vector<VkDescriptorSet>* const pOutHandles, VkResult* const pOutResult)
	{
		DE_ASSERT(pOutResult);
		DE_ASSERT(pOutHandles);
		DE_ASSERT(pOutHandles->size() != 0);

		const deUint32						numDescriptorSets		= static_cast<deUint32>(pOutHandles->size());
		VkDescriptorSet* const				pHandles				= &(*pOutHandles)[0];
		const vector<VkDescriptorSetLayout>	descriptorSetLayouts	(numDescriptorSets, res.descriptorSetLayout.object.get());

		const VkDescriptorSetAllocateInfo	allocateInfo			=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			DE_NULL,
			*res.descriptorPool.object,
			numDescriptorSets,
			&descriptorSetLayouts[0],
		};

		*pOutResult = env.vkd.allocateDescriptorSets(env.device, &allocateInfo, pHandles);

		vector<VkDescriptorSetSp>	descriptorSets;

		if (*pOutResult == VK_SUCCESS)
		{
			for (deUint32 i = 0; i < numDescriptorSets; ++i)
				descriptorSets.push_back(VkDescriptorSetSp(new Move<VkDescriptorSet>(check<VkDescriptorSet>(pHandles[i]), Deleter<VkDescriptorSet>(env.vkd, env.device, *res.descriptorPool.object))));
		}

		return descriptorSets;
	}
};

struct Framebuffer
{
	typedef VkFramebuffer Type;

	struct Parameters
	{
		Parameters (void)
		{}
	};

	struct Resources
	{
		Dependency<ImageView>	colorAttachment;
		Dependency<ImageView>	depthStencilAttachment;
		Dependency<RenderPass>	renderPass;

		Resources (const Environment& env, const Parameters&)
			: colorAttachment			(env, ImageView::Parameters(Image::Parameters(0u, VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM,
																					  makeExtent3D(256, 256, 1),
																					  1u, 1u,
																					  VK_SAMPLE_COUNT_1_BIT,
																					  VK_IMAGE_TILING_OPTIMAL,
																					  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
																					  VK_IMAGE_LAYOUT_UNDEFINED),
																		 VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM,
																		 makeComponentMappingRGBA(),
																		 makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u)))
			, depthStencilAttachment	(env, ImageView::Parameters(Image::Parameters(0u, VK_IMAGE_TYPE_2D, VK_FORMAT_D16_UNORM,
																					  makeExtent3D(256, 256, 1),
																					  1u, 1u,
																					  VK_SAMPLE_COUNT_1_BIT,
																					  VK_IMAGE_TILING_OPTIMAL,
																					  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
																					  VK_IMAGE_LAYOUT_UNDEFINED),
																		 VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D16_UNORM,
																		 makeComponentMappingRGBA(),
																		 makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u)))
			, renderPass				(env, RenderPass::Parameters())
		{}
	};

	static deUint32 getMaxConcurrent (Context& context, const Parameters& params)
	{
		// \todo [2016-03-23 pyry] Take into account attachment sizes
		return getSafeObjectCount<Framebuffer>(context, params, DEFAULT_MAX_CONCURRENT_OBJECTS);
	}

	static Move<VkFramebuffer> create (const Environment& env, const Resources& res, const Parameters&)
	{
		const VkImageView				attachments[]	=
		{
			*res.colorAttachment.object,
			*res.depthStencilAttachment.object,
		};
		const VkFramebufferCreateInfo	framebufferInfo	=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			DE_NULL,
			(VkFramebufferCreateFlags)0,
			*res.renderPass.object,
			(deUint32)DE_LENGTH_OF_ARRAY(attachments),
			attachments,
			256u,										// width
			256u,										// height
			1u											// layers
		};

		return createFramebuffer(env.vkd, env.device, &framebufferInfo, env.allocationCallbacks);
	}
};

struct CommandPool
{
	typedef VkCommandPool Type;

	struct Parameters
	{
		VkCommandPoolCreateFlags	flags;

		Parameters (VkCommandPoolCreateFlags flags_)
			: flags(flags_)
		{}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static deUint32 getMaxConcurrent (Context& context, const Parameters& params)
	{
		return getSafeObjectCount<CommandPool>(context, params, DEFAULT_MAX_CONCURRENT_OBJECTS);
	}

	static Move<VkCommandPool> create (const Environment& env, const Resources&, const Parameters& params)
	{
		const VkCommandPoolCreateInfo	cmdPoolInfo	=
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			DE_NULL,
			params.flags,
			env.queueFamilyIndex,
		};

		return createCommandPool(env.vkd, env.device, &cmdPoolInfo, env.allocationCallbacks);
	}
};

struct CommandBuffer
{
	typedef VkCommandBuffer Type;

	struct Parameters
	{
		CommandPool::Parameters		commandPool;
		VkCommandBufferLevel		level;

		Parameters (const CommandPool::Parameters&	commandPool_,
					VkCommandBufferLevel			level_)
			: commandPool	(commandPool_)
			, level			(level_)
		{}
	};

	struct Resources
	{
		Dependency<CommandPool>	commandPool;

		Resources (const Environment& env, const Parameters& params)
			: commandPool(env, params.commandPool)
		{}
	};

	static deUint32 getMaxConcurrent (Context& context, const Parameters& params)
	{
		return getSafeObjectCount<CommandBuffer>(context, params, DEFAULT_MAX_CONCURRENT_OBJECTS);
	}

	static Move<VkCommandBuffer> create (const Environment& env, const Resources& res, const Parameters& params)
	{
		const VkCommandBufferAllocateInfo	cmdBufferInfo	=
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			DE_NULL,
			*res.commandPool.object,
			params.level,
			1,							// bufferCount
		};

		return allocateCommandBuffer(env.vkd, env.device, &cmdBufferInfo);
	}

	static vector<VkCommandBufferSp> createMultiple (const Environment& env, const Resources& res, const Parameters& params, vector<VkCommandBuffer>* const pOutHandles, VkResult* const pOutResult)
	{
		DE_ASSERT(pOutResult);
		DE_ASSERT(pOutHandles);
		DE_ASSERT(pOutHandles->size() != 0);

		const deUint32						numCommandBuffers	= static_cast<deUint32>(pOutHandles->size());
		VkCommandBuffer* const				pHandles			= &(*pOutHandles)[0];

		const VkCommandBufferAllocateInfo	cmdBufferInfo		=
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			DE_NULL,
			*res.commandPool.object,
			params.level,
			numCommandBuffers,
		};

		*pOutResult = env.vkd.allocateCommandBuffers(env.device, &cmdBufferInfo, pHandles);

		vector<VkCommandBufferSp>	commandBuffers;

		if (*pOutResult == VK_SUCCESS)
		{
			for (deUint32 i = 0; i < numCommandBuffers; ++i)
				commandBuffers.push_back(VkCommandBufferSp(new Move<VkCommandBuffer>(check<VkCommandBuffer>(pHandles[i]), Deleter<VkCommandBuffer>(env.vkd, env.device, *res.commandPool.object))));
		}

		return commandBuffers;
	}
};

// Test cases

template<typename Object>
tcu::TestStatus createSingleTest (Context& context, typename Object::Parameters params)
{
	const Environment					env	(context, 1u);
	const typename Object::Resources	res	(env, params);

	{
		Unique<typename Object::Type>	obj	(Object::create(env, res, params));
	}

	return tcu::TestStatus::pass("Ok");
}

template<typename Object>
tcu::TestStatus createMultipleUniqueResourcesTest (Context& context, typename Object::Parameters params)
{
	const Environment					env		(context, 1u);
	const typename Object::Resources	res0	(env, params);
	const typename Object::Resources	res1	(env, params);
	const typename Object::Resources	res2	(env, params);
	const typename Object::Resources	res3	(env, params);

	{
		Unique<typename Object::Type>	obj0	(Object::create(env, res0, params));
		Unique<typename Object::Type>	obj1	(Object::create(env, res1, params));
		Unique<typename Object::Type>	obj2	(Object::create(env, res2, params));
		Unique<typename Object::Type>	obj3	(Object::create(env, res3, params));
	}

	return tcu::TestStatus::pass("Ok");
}

#ifdef CTS_USES_VULKANSC
template<>
tcu::TestStatus createMultipleUniqueResourcesTest<Instance> (Context& context, Instance::Parameters params)
{
	const Environment					env(context, 1u);
	const typename Instance::Resources	res0(env, params);
	const typename Instance::Resources	res1(env, params);

	{
		Unique<typename Instance::Type>	obj0(Instance::create(env, res0, params));
		Unique<typename Instance::Type>	obj1(Instance::create(env, res1, params));
	}

	return tcu::TestStatus::pass("Ok");
}
#endif // CTS_USES_VULKANSC

template<typename Object>
tcu::TestStatus createMultipleSharedResourcesTest (Context& context, typename Object::Parameters params)
{
	const Environment					env	(context, 4u);
	const typename Object::Resources	res	(env, params);

	{
		Unique<typename Object::Type>	obj0	(Object::create(env, res, params));
		Unique<typename Object::Type>	obj1	(Object::create(env, res, params));
		Unique<typename Object::Type>	obj2	(Object::create(env, res, params));
		Unique<typename Object::Type>	obj3	(Object::create(env, res, params));
	}

	return tcu::TestStatus::pass("Ok");
}

#ifndef CTS_USES_VULKANSC

// Class to wrap singleton devices used by private_data tests
class SingletonDevice
{
	Move<VkDevice> createPrivateDataDevice(const Context &context, int idx)
	{
		const int requestedSlots[NUM_DEVICES][2] =
		{
			{0, 0},
			{1, 0},
			{1, 1},
			{4, 4},
			{1, 100},
		};

		const float	queuePriority					= 1.0;
		const VkDeviceQueueCreateInfo	queues[]	=
		{
			{
				VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				DE_NULL,
				(VkDeviceQueueCreateFlags)0,
				context.getUniversalQueueFamilyIndex(),
				1u,									// queueCount
				&queuePriority,						// pQueuePriorities
			}
		};

		VkDevicePrivateDataCreateInfoEXT pdci0 =
		{
			VK_STRUCTURE_TYPE_DEVICE_PRIVATE_DATA_CREATE_INFO_EXT,	// VkStructureType                       sType;
			DE_NULL,												// const void*                           pNext;
			0u,														// uint32_t                              privateDataSlotRequestCount;
		};
		VkDevicePrivateDataCreateInfoEXT pdci1 =
		{
			VK_STRUCTURE_TYPE_DEVICE_PRIVATE_DATA_CREATE_INFO_EXT,	// VkStructureType                       sType;
			DE_NULL,												// const void*                           pNext;
			0u,														// uint32_t                              privateDataSlotRequestCount;
		};
		void *pNext = DE_NULL;

		if (requestedSlots[idx][0])
		{
			pNext = &pdci0;
			pdci0.privateDataSlotRequestCount = requestedSlots[idx][0];
			if (requestedSlots[idx][1])
			{
				pdci0.pNext = &pdci1;
				pdci1.privateDataSlotRequestCount = requestedSlots[idx][1];
			}
		}

		VkPhysicalDevicePrivateDataFeaturesEXT privateDataFeatures =
		{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIVATE_DATA_FEATURES_EXT,	// VkStructureType    sType;
			pNext,															// void*              pNext;
			VK_TRUE,														// VkBool32           privateData;
		};
		pNext = &privateDataFeatures;

		const char *extName = "VK_EXT_private_data";

		VkPhysicalDeviceFeatures enabledFeatures = getPhysicalDeviceFeatures(context.getInstanceInterface(), context.getPhysicalDevice());

		const VkDeviceCreateInfo		deviceInfo	=
		{
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			pNext,
			(VkDeviceCreateFlags)0,
			DE_LENGTH_OF_ARRAY(queues),
			queues,
			0u,										// enabledLayerNameCount
			DE_NULL,								// ppEnabledLayerNames
			1u,										// enabledExtensionNameCount
			&extName,								// ppEnabledExtensionNames
			&enabledFeatures,						// pEnabledFeatures
		};

		Move<VkDevice> device = createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(),
												   context.getPlatformInterface(), context.getInstance(), context.getInstanceInterface(), context.getPhysicalDevice(), &deviceInfo, DE_NULL);
		return device;
	}

	SingletonDevice	(const Context& context, int idx)
		: m_logicalDevice	(createPrivateDataDevice(context, idx))
	{
	}


public:

	static const int NUM_DEVICES = 5;

	static const Unique<vk::VkDevice>& getDevice(const Context& context, int idx)
	{
		if (!m_singletonDevice[idx])
			m_singletonDevice[idx] = SharedPtr<SingletonDevice>(new SingletonDevice(context, idx));

		DE_ASSERT(m_singletonDevice[idx]);
		return m_singletonDevice[idx]->m_logicalDevice;
	}

	static void destroy()
	{
		for (int idx = 0; idx < NUM_DEVICES; ++idx)
			m_singletonDevice[idx].clear();
	}

private:
	const Unique<vk::VkDevice>					m_logicalDevice;
	static SharedPtr<SingletonDevice>			m_singletonDevice[NUM_DEVICES];
};

SharedPtr<SingletonDevice>		SingletonDevice::m_singletonDevice[NUM_DEVICES];

template<typename T> static deUint64 HandleToInt(T t) { return t.getInternal(); }
template<typename T> static deUint64 HandleToInt(T *t) { return (deUint64)(deUintptr)(t); }

template<typename Object>
tcu::TestStatus createPrivateDataTest (Context& context, typename Object::Parameters params)
{
	if (!context.getPrivateDataFeatures().privateData)
		TCU_THROW(NotSupportedError, "privateData not supported");

	for (int d = 0; d < SingletonDevice::NUM_DEVICES; ++d)
	{
		const Unique<vk::VkDevice>&			device =			SingletonDevice::getDevice(context, d);
		const Environment					env					(context.getPlatformInterface(),
																 context.getUsedApiVersion(),
																 context.getInstanceInterface(),
																 context.getInstance(),
																 context.getDeviceInterface(),
																 *device,
																 context.getUniversalQueueFamilyIndex(),
																 context.getBinaryCollection(),
																 DE_NULL,
																 4u,
																 context.getTestContext().getCommandLine());

		const typename Object::Resources	res	(env, params);

		const VkPrivateDataSlotCreateInfoEXT createInfo =
		{
			VK_STRUCTURE_TYPE_PRIVATE_DATA_SLOT_CREATE_INFO_EXT,	// VkStructureType                    sType;
			DE_NULL,												// const void*                        pNext;
			0u,														// VkPrivateDataSlotCreateFlagsEXT    flags;
		};

		const int numSlots = 100;

		typedef Unique<VkPrivateDataSlot>					PrivateDataSlotUp;
		typedef SharedPtr<PrivateDataSlotUp>				PrivateDataSlotSp;
		vector<PrivateDataSlotSp> slots;

		// interleave allocating objects and slots
		for (int i = 0; i < numSlots / 2; ++i)
		{
			Move<VkPrivateDataSlot> s = createPrivateDataSlot(env.vkd, *device, &createInfo, DE_NULL);
			slots.push_back(PrivateDataSlotSp(new PrivateDataSlotUp(s)));
		}

		Unique<typename Object::Type>	obj0	(Object::create(env, res, params));
		Unique<typename Object::Type>	obj1	(Object::create(env, res, params));

		for (int i = numSlots / 2; i < numSlots; ++i)
		{
			Move<VkPrivateDataSlot> s = createPrivateDataSlot(env.vkd, *device, &createInfo, DE_NULL);
			slots.push_back(PrivateDataSlotSp(new PrivateDataSlotUp(s)));
		}

		Unique<typename Object::Type>	obj2	(Object::create(env, res, params));
		Unique<typename Object::Type>	obj3	(Object::create(env, res, params));

		Unique<typename Object::Type> *objs[4] = { &obj0, &obj1, &obj2, &obj3 };

		for (int r = 0; r < 3; ++r)
		{
			deUint64 data;

			// Test private data for the objects
			for (int o = 0; o < 4; ++o)
			{
				auto &obj = *objs[o];
				for (int i = 0; i < numSlots; ++i)
				{
					data = 1234;
					env.vkd.getPrivateData(*device, getObjectType<typename Object::Type>(), HandleToInt(obj.get()), **slots[i], &data);
					if (data != 0)
						return tcu::TestStatus::fail("Expected initial value of zero");
				}
			}
			for (int o = 0; o < 4; ++o)
			{
				auto &obj = *objs[o];
				for (int i = 0; i < numSlots; ++i)
					VK_CHECK(env.vkd.setPrivateData(*device, getObjectType<typename Object::Type>(), HandleToInt(obj.get()), **slots[i], i*i*i + o*o + 1));
			}
			for (int o = 0; o < 4; ++o)
			{
				auto &obj = *objs[o];
				for (int i = 0; i < numSlots; ++i)
				{
					data = 1234;
					env.vkd.getPrivateData(*device, getObjectType<typename Object::Type>(), HandleToInt(obj.get()), **slots[i], &data);
					if (data != (deUint64)(i*i*i + o*o + 1))
						return tcu::TestStatus::fail("Didn't read back set value");
				}
			}


			// Test private data for the private data objects
			for (int o = 0; o < numSlots; ++o)
			{
				auto &obj = **slots[o];
				for (int i = 0; i < numSlots; ++i)
				{
					data = 1234;
					env.vkd.getPrivateData(*device, VK_OBJECT_TYPE_PRIVATE_DATA_SLOT_EXT, HandleToInt<VkPrivateDataSlotEXT>(obj), **slots[i], &data);
					if (data != 0)
						return tcu::TestStatus::fail("Expected initial value of zero");
				}
			}
			for (int o = 0; o < numSlots; ++o)
			{
				auto &obj = **slots[o];
				for (int i = 0; i < numSlots; ++i)
					VK_CHECK(env.vkd.setPrivateData(*device, VK_OBJECT_TYPE_PRIVATE_DATA_SLOT_EXT, HandleToInt<VkPrivateDataSlotEXT>(obj), **slots[i], i*i*i + o*o + 1));
			}
			for (int o = 0; o < numSlots; ++o)
			{
				auto &obj = **slots[o];
				for (int i = 0; i < numSlots; ++i)
				{
					data = 1234;
					env.vkd.getPrivateData(*device, VK_OBJECT_TYPE_PRIVATE_DATA_SLOT_EXT, HandleToInt<VkPrivateDataSlotEXT>(obj), **slots[i], &data);
					if (data != (deUint64)(i*i*i + o*o + 1))
						return tcu::TestStatus::fail("Didn't read back set value");
				}
			}

			// Test private data for the device
			for (int i = 0; i < numSlots; ++i)
			{
				data = 1234;
				env.vkd.getPrivateData(*device, VK_OBJECT_TYPE_DEVICE, (deUint64)(deUintptr)(*device), **slots[i], &data);
				if (data != 0)
					return tcu::TestStatus::fail("Expected initial value of zero for device");
			}
			for (int i = 0; i < numSlots; ++i)
				VK_CHECK(env.vkd.setPrivateData(*device, VK_OBJECT_TYPE_DEVICE, (deUint64)(deUintptr)(*device), **slots[i], i*i*i + r*r + 1));
			for (int i = 0; i < numSlots; ++i)
			{
				data = 1234;
				env.vkd.getPrivateData(*device, VK_OBJECT_TYPE_DEVICE, (deUint64)(deUintptr)(*device), **slots[i], &data);
				if (data != (deUint64)(i*i*i + r*r + 1))
					return tcu::TestStatus::fail("Didn't read back set value from device");
			}

			// Destroy and realloc slots for the next iteration
			slots.clear();
			for (int i = 0; i < numSlots; ++i)
			{
				Move<VkPrivateDataSlotEXT> s = createPrivateDataSlot(env.vkd, *device, &createInfo, DE_NULL);
				slots.push_back(PrivateDataSlotSp(new PrivateDataSlotUp(s)));
			}
		}
	}

	return tcu::TestStatus::pass("Ok");
}

template<typename Object>
tcu::TestStatus createMaxConcurrentTest (Context& context, typename Object::Parameters params)
{
	typedef Unique<typename Object::Type>	UniqueObject;
	typedef SharedPtr<UniqueObject>			ObjectPtr;

	const deUint32						numObjects			= Object::getMaxConcurrent(context, params);
	const Environment					env					(context, numObjects);
	const typename Object::Resources	res					(env, params);
	vector<ObjectPtr>					objects				(numObjects);
	const deUint32						watchdogInterval	= 1024;

	context.getTestContext().getLog()
		<< TestLog::Message << "Creating " << numObjects << " " << getTypeName<typename Object::Type>() << " objects" << TestLog::EndMessage;

	for (deUint32 ndx = 0; ndx < numObjects; ndx++)
	{
		objects[ndx] = ObjectPtr(new UniqueObject(Object::create(env, res, params)));

		if ((ndx > 0) && ((ndx % watchdogInterval) == 0))
			context.getTestContext().touchWatchdog();
	}

	context.getTestContext().touchWatchdog();
	objects.clear();

	return tcu::TestStatus::pass("Ok");
}

#endif // CTS_USES_VULKANSC

// How many objects to create per thread
template<typename Object>	int getCreateCount				(void) { return 100;	}

// Creating VkDevice and VkInstance can take significantly longer than other object types

#ifndef CTS_USES_VULKANSC
template<>					int getCreateCount<Instance>	(void) { return 20;		}
template<>					int getCreateCount<Device>		(void) { return 20;		}
template<>					int getCreateCount<DeviceGroup>	(void) { return 20;		}
#else
template<>					int getCreateCount<Instance>	(void) { return 2;		}
template<>					int getCreateCount<Device>		(void) { return 2;		}
template<>					int getCreateCount<DeviceGroup>	(void) { return 2;		}
#endif // CTS_USES_VULKANSC

template<typename Object>
class CreateThread : public ThreadGroupThread
{
public:
	CreateThread (const Environment& env, const typename Object::Resources& resources, const typename Object::Parameters& params)
		: m_env			(env)
		, m_resources	(resources)
		, m_params		(params)
	{}

	void runThread (void)
	{
		const int	numIters			= getCreateCount<Object>();
#ifndef CTS_USES_VULKANSC
		const int	itersBetweenSyncs	= numIters / 5;
#else
		const int	itersBetweenSyncs	= 1;
#endif // CTS_USES_VULKANSC

		DE_ASSERT(itersBetweenSyncs > 0);

		for (int iterNdx = 0; iterNdx < numIters; iterNdx++)
		{
			// Sync every Nth iteration to make entering driver at the same time more likely
			if ((iterNdx % itersBetweenSyncs) == 0)
				barrier();

			{
				Unique<typename Object::Type>	obj	(Object::create(m_env, m_resources, m_params));
#ifdef CTS_USES_VULKANSC
				if (iterNdx == 0)
				{
					barrier();
				}
#endif
			}
		}
	}

private:
	const Environment&					m_env;
	const typename Object::Resources&	m_resources;
	const typename Object::Parameters&	m_params;
};

template<typename Object>
tcu::TestStatus multithreadedCreateSharedResourcesTest (Context& context, typename Object::Parameters params)
{
#ifdef CTS_USES_VULKANSC
	MultithreadedDestroyGuard			mdGuard		(context.getResourceInterface());
#endif // CTS_USES_VULKANSC
	TestLog&							log			= context.getTestContext().getLog();
	const deUint32						numThreads	= getDefaultTestThreadCount();
	const Environment					env			(context, numThreads);
	const typename Object::Resources	res			(env, params);
	ThreadGroup							threads;

	log << TestLog::Message << "numThreads = " << numThreads << TestLog::EndMessage;

	for (deUint32 ndx = 0; ndx < numThreads; ndx++)
		threads.add(MovePtr<ThreadGroupThread>(new CreateThread<Object>(env, res, params)));

	return threads.run();
}

template<typename Object>
tcu::TestStatus multithreadedCreatePerThreadResourcesTest (Context& context, typename Object::Parameters params)
{
	typedef SharedPtr<typename Object::Resources>	ResPtr;
#ifdef CTS_USES_VULKANSC
	MultithreadedDestroyGuard	mdGuard	(context.getResourceInterface());
#endif // CTS_USES_VULKANSC
	TestLog&					log			= context.getTestContext().getLog();
	const deUint32				numThreads	= getDefaultTestThreadCount();
	const Environment			env			(context, 1u);
	vector<ResPtr>				resources	(numThreads);
	ThreadGroup					threads;

	log << TestLog::Message << "numThreads = " << numThreads << TestLog::EndMessage;

	for (deUint32 ndx = 0; ndx < numThreads; ndx++)
	{
		resources[ndx] = ResPtr(new typename Object::Resources(env, params));
		threads.add(MovePtr<ThreadGroupThread>(new CreateThread<Object>(env, *resources[ndx], params)));
	}

	return threads.run();
}

struct EnvClone
{
	Device::Resources											deviceRes;
	Unique<VkDevice>											device;
#ifndef CTS_USES_VULKANSC
	de::MovePtr<vk::DeviceDriver>								vkd;
#else
	de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter>	vkd;
#endif // CTS_USES_VULKANSC
	Environment													env;

	EnvClone (const Environment& parent, const Device::Parameters& deviceParams, deUint32 maxResourceConsumers)
		: deviceRes	(parent, deviceParams)
		, device	(Device::create(parent, deviceRes, deviceParams))
#ifndef CTS_USES_VULKANSC
		, vkd(de::MovePtr<DeviceDriver>(new DeviceDriver(parent.vkp, parent.instance, *device, parent.apiVersion)))
		, env(parent.vkp, parent.apiVersion, parent.instanceInterface, parent.instance, *vkd, *device, deviceRes.queueFamilyIndex, parent.programBinaries, parent.allocationCallbacks, maxResourceConsumers, parent.commandLine)
#else
		, vkd(de::MovePtr<DeviceDriverSC, DeinitDeviceDeleter>(new DeviceDriverSC(parent.vkp, parent.instance, *device, parent.commandLine, parent.resourceInterface, parent.vulkanSC10Properties, parent.properties, parent.apiVersion), vk::DeinitDeviceDeleter(parent.resourceInterface.get(), *device)))
		, env(parent.vkp, parent.apiVersion, parent.instanceInterface, parent.instance, *vkd, *device, deviceRes.queueFamilyIndex, parent.programBinaries, parent.allocationCallbacks, maxResourceConsumers, parent.resourceInterface, parent.vulkanSC10Properties, parent.commandLine)
#endif // CTS_USES_VULKANSC
	{
	}
};

Device::Parameters getDefaulDeviceParameters (Context& context)
{
	return Device::Parameters(context.getTestContext().getCommandLine().getVKDeviceId()-1u,
							  VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT);
}

template<typename Object>
tcu::TestStatus multithreadedCreatePerThreadDeviceTest (Context& context, typename Object::Parameters params)
{
#ifdef CTS_USES_VULKANSC
	MultithreadedDestroyGuard						mdGuard(context.getResourceInterface());
#endif // CTS_USES_VULKANSC
	typedef SharedPtr<EnvClone>						EnvPtr;
	typedef SharedPtr<typename Object::Resources>	ResPtr;

	TestLog&					log				= context.getTestContext().getLog();
	const deUint32				numThreads		= getDefaultTestThreadCount();
	const Device::Parameters	deviceParams	= getDefaulDeviceParameters(context);
	const Environment			sharedEnv		(context, numThreads);			// For creating Device's
	vector<EnvPtr>				perThreadEnv	(numThreads);
	vector<ResPtr>				resources		(numThreads);
	ThreadGroup					threads;

	log << TestLog::Message << "numThreads = " << numThreads << TestLog::EndMessage;

	for (deUint32 ndx = 0; ndx < numThreads; ndx++)
	{
		perThreadEnv[ndx]	= EnvPtr(new EnvClone(sharedEnv, deviceParams, 1u));
		resources[ndx]		= ResPtr(new typename Object::Resources(perThreadEnv[ndx]->env, params));

		threads.add(MovePtr<ThreadGroupThread>(new CreateThread<Object>(perThreadEnv[ndx]->env, *resources[ndx], params)));
	}

	return threads.run();
}

#ifndef CTS_USES_VULKANSC

template<typename Object>
tcu::TestStatus createSingleAllocCallbacksTest (Context& context, typename Object::Parameters params)
{
	const deUint32						noCmdScope		= (1u << VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE)
														| (1u << VK_SYSTEM_ALLOCATION_SCOPE_DEVICE)
														| (1u << VK_SYSTEM_ALLOCATION_SCOPE_CACHE)
														| (1u << VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

	// Callbacks used by resources
	AllocationCallbackRecorder			resCallbacks	(getSystemAllocator(), 128);

	// Root environment still uses default instance and device, created without callbacks
	const Environment					rootEnv			(context.getPlatformInterface(),
														 context.getUsedApiVersion(),
														 context.getInstanceInterface(),
														 context.getInstance(),
														 context.getDeviceInterface(),
														 context.getDevice(),
														 context.getUniversalQueueFamilyIndex(),
														 context.getBinaryCollection(),
														 resCallbacks.getCallbacks(),
														 1u,
#ifdef CTS_USES_VULKANSC
														 context.getResourceInterface(),
														 context.getDeviceVulkanSC10Properties(),
#endif // CTS_USES_VULKANSC
														 context.getTestContext().getCommandLine());

	{
		// Test env has instance & device created with callbacks
		const EnvClone						resEnv		(rootEnv, getDefaulDeviceParameters(context), 1u);
		const typename Object::Resources	res			(resEnv.env, params);

		// Supply a separate callback recorder just for object construction
		AllocationCallbackRecorder			objCallbacks(getSystemAllocator(), 128);
		const Environment					objEnv		(resEnv.env.vkp,
														 resEnv.env.apiVersion,
														 resEnv.env.instanceInterface,
														 resEnv.env.instance,
														 resEnv.env.vkd,
														 resEnv.env.device,
														 resEnv.env.queueFamilyIndex,
														 resEnv.env.programBinaries,
														 objCallbacks.getCallbacks(),
														 resEnv.env.maxResourceConsumers,
#ifdef CTS_USES_VULKANSC
														 resEnv.env.resourceInterface,
														 resEnv.env.vulkanSC10Properties,
#endif // CTS_USES_VULKANSC
														 resEnv.env.commandLine);

		{
			Unique<typename Object::Type>	obj	(Object::create(objEnv, res, params));

			// Validate that no command-level allocations are live
			if (!validateAndLog(context.getTestContext().getLog(), objCallbacks, noCmdScope))
				return tcu::TestStatus::fail("Invalid allocation callback");
		}

		// At this point all allocations made against object callbacks must have been freed
		if (!validateAndLog(context.getTestContext().getLog(), objCallbacks, 0u))
			return tcu::TestStatus::fail("Invalid allocation callback");
	}

	if (!validateAndLog(context.getTestContext().getLog(), resCallbacks, 0u))
		return tcu::TestStatus::fail("Invalid allocation callback");

	return tcu::TestStatus::pass("Ok");
}

#endif // CTS_USES_VULKANSC

template<typename Object>	deUint32	getOomIterLimit					(void) { return 40;		}
#ifndef CTS_USES_VULKANSC
template<>					deUint32	getOomIterLimit<Device>			(void) { return 20;		}
template<>					deUint32	getOomIterLimit<DeviceGroup>	(void) { return 20;		}
#endif // CTS_USES_VULKANSC

#ifndef CTS_USES_VULKANSC

template<typename Object>
tcu::TestStatus allocCallbackFailTest (Context& context, typename Object::Parameters params)
{
	AllocationCallbackRecorder			resCallbacks		(getSystemAllocator(), 128);
	const Environment					rootEnv				(context.getPlatformInterface(),
															 context.getUsedApiVersion(),
															 context.getInstanceInterface(),
															 context.getInstance(),
															 context.getDeviceInterface(),
															 context.getDevice(),
															 context.getUniversalQueueFamilyIndex(),
															 context.getBinaryCollection(),
															 resCallbacks.getCallbacks(),
															 1u,
#ifdef CTS_USES_VULKANSC
															 context.getResourceInterface(),
															 context.getDeviceVulkanSC10Properties(),
#endif // CTS_USES_VULKANSC
															 context.getTestContext().getCommandLine());
	deUint32							numPassingAllocs	= 0;
	const deUint32						cmdLineIterCount	= (deUint32)context.getTestContext().getCommandLine().getTestIterationCount();
	const deUint32						maxTries			= cmdLineIterCount != 0 ? cmdLineIterCount : getOomIterLimit<Object>();
	const deUint32						finalLimit			= std::max(maxTries, 10000u);
	bool								createOk			= false;

	{
		const EnvClone						resEnv	(rootEnv, getDefaulDeviceParameters(context), 1u);
		const typename Object::Resources	res		(resEnv.env, params);

		// Iterate over test until object allocation succeeds
		while(true)
		{
			DeterministicFailAllocator			objAllocator(getSystemAllocator(),
															 DeterministicFailAllocator::MODE_COUNT_AND_FAIL,
															 numPassingAllocs);
			AllocationCallbackRecorder			recorder	(objAllocator.getCallbacks(), 128);
			const Environment					objEnv		(resEnv.env.vkp,
															 resEnv.env.apiVersion,
															 resEnv.env.instanceInterface,
															 resEnv.env.instance,
															 resEnv.env.vkd,
															 resEnv.env.device,
															 resEnv.env.queueFamilyIndex,
															 resEnv.env.programBinaries,
															 recorder.getCallbacks(),
															 resEnv.env.maxResourceConsumers,
#ifdef CTS_USES_VULKANSC
															 resEnv.env.resourceInterface,
															 resEnv.env.vulkanSC10Properties,
#endif // CTS_USES_VULKANSC
															 resEnv.env.commandLine);

			context.getTestContext().getLog()
				<< TestLog::Message
				<< "Trying to create object with " << numPassingAllocs << " allocation" << (numPassingAllocs != 1 ? "s" : "") << " passing"
				<< TestLog::EndMessage;

			createOk = false;
			try
			{
				Unique<typename Object::Type>	obj	(Object::create(objEnv, res, params));
				createOk = true;
			}
			catch (const vk::OutOfMemoryError& e)
			{
				if (e.getError() != VK_ERROR_OUT_OF_HOST_MEMORY)
				{
					context.getTestContext().getLog() << e;
					return tcu::TestStatus::fail("Got invalid error code");
				}
			}

			if (!validateAndLog(context.getTestContext().getLog(), recorder, 0u))
				return tcu::TestStatus::fail("Invalid allocation callback");

			if (createOk)
			{
				context.getTestContext().getLog()
					<< TestLog::Message << "Object construction succeeded! " << TestLog::EndMessage;
				break;
			}

			++numPassingAllocs;
			// if allocation didn't succeed with huge limit then stop trying
			if (numPassingAllocs >= finalLimit)
				break;
			// if we reached maxTries but didn't create object, try doing it with huge limit
			if (numPassingAllocs >= maxTries)
				numPassingAllocs = finalLimit;
		}
	}

	if (!validateAndLog(context.getTestContext().getLog(), resCallbacks, 0u))
		return tcu::TestStatus::fail("Invalid allocation callback");

	if (numPassingAllocs == 0)
		return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Allocation callbacks not called");
	else if (numPassingAllocs >= finalLimit)
	{
		if (createOk)
		{
			context.getTestContext().getLog()
				<< TestLog::Message << "Maximum iteration count (" << maxTries << ") reached without object construction passing. "
				<< "Object was succesfully constructed with " << numPassingAllocs << " iterations limit." << TestLog::EndMessage;
			return tcu::TestStatus(QP_TEST_RESULT_PASS, "Construction passed but not all iterations were checked");
		}

		context.getTestContext().getLog()
			<< TestLog::Message << "WARNING: Maximum iteration count (" << finalLimit << ") reached without object construction passing. "
			<< "OOM testing incomplete, use --deqp-test-iteration-count= to test with higher limit." << TestLog::EndMessage;
		return tcu::TestStatus(QP_TEST_RESULT_PASS, "Max iter count reached");
	}
	else
		return tcu::TestStatus::pass("Ok");
}

#endif // CTS_USES_VULKANSC

// Determine whether an API call sets the invalid handles to NULL (true) or leaves them undefined or not modified (false)
template<typename T> inline bool isNullHandleOnAllocationFailure				  (Context&)		 { return false; }

#ifndef CTS_USES_VULKANSC
template<>			 inline bool isNullHandleOnAllocationFailure<VkCommandBuffer> (Context& context) { return hasDeviceExtension(context, "VK_KHR_maintenance1"); }
template<>			 inline bool isNullHandleOnAllocationFailure<VkDescriptorSet> (Context& context) { return hasDeviceExtension(context, "VK_KHR_maintenance1"); }
template<>			 inline bool isNullHandleOnAllocationFailure<VkPipeline>	  (Context&)		 { return true;  }
#endif // CTS_USES_VULKANSC

template<typename T> inline bool isPooledObject					 (void) { return false; };
#ifndef CTS_USES_VULKANSC
template<>			 inline bool isPooledObject<VkCommandBuffer> (void) { return true;  };
template<>			 inline bool isPooledObject<VkDescriptorSet> (void) { return true;  };
#endif // CTS_USES_VULKANSC

template<typename Object>
tcu::TestStatus allocCallbackFailMultipleObjectsTest (Context& context, typename Object::Parameters params)
{
	typedef SharedPtr<Move<typename Object::Type> >	ObjectTypeSp;

	static const deUint32	numObjects			= 4;
	const bool				expectNullHandles	= isNullHandleOnAllocationFailure<typename Object::Type>(context);
	deUint32				numPassingAllocs	= 0;

	{
		vector<typename Object::Type>	handles	(numObjects);
		VkResult						result	= VK_NOT_READY;

		for (; numPassingAllocs <= numObjects; ++numPassingAllocs)
		{
			ValidateQueryBits::fillBits(handles.begin(), handles.end());	// fill with garbage

			// \note We have to use the same allocator for both resource dependencies and the object under test,
			//       because pooled objects take memory from the pool.
			DeterministicFailAllocator			objAllocator(getSystemAllocator(), DeterministicFailAllocator::MODE_DO_NOT_COUNT, 0);
			AllocationCallbackRecorder			recorder	(objAllocator.getCallbacks(), 128);
			const Environment					objEnv		(context.getPlatformInterface(),
															 context.getUsedApiVersion(),
															 context.getInstanceInterface(),
															 context.getInstance(),
															 context.getDeviceInterface(),
															 context.getDevice(),
															 context.getUniversalQueueFamilyIndex(),
															 context.getBinaryCollection(),
															 recorder.getCallbacks(),
															 numObjects,
#ifdef CTS_USES_VULKANSC
															 context.getResourceInterface(),
															 context.getDeviceVulkanSC10Properties(),
#endif // CTS_USES_VULKANSC
															 context.getTestContext().getCommandLine());

			context.getTestContext().getLog()
				<< TestLog::Message
				<< "Trying to create " << numObjects << " objects with " << numPassingAllocs << " allocation" << (numPassingAllocs != 1 ? "s" : "") << " passing"
				<< TestLog::EndMessage;

			{
				const typename Object::Resources res (objEnv, params);

				objAllocator.reset(DeterministicFailAllocator::MODE_COUNT_AND_FAIL, numPassingAllocs);
				const vector<ObjectTypeSp> scopedHandles = Object::createMultiple(objEnv, res, params, &handles, &result);
			}

			if (result == VK_SUCCESS)
			{
				context.getTestContext().getLog() << TestLog::Message << "Construction of all objects succeeded! " << TestLog::EndMessage;
				break;
			}
			else
			{
				if (expectNullHandles)
				{
					for (deUint32 nullNdx = numPassingAllocs; nullNdx < numObjects; ++nullNdx)
					{
						if (handles[nullNdx] != DE_NULL)
							return tcu::TestStatus::fail("Some object handles weren't set to NULL");
					}
				}

				if (result != VK_ERROR_OUT_OF_HOST_MEMORY)
					return tcu::TestStatus::fail("Got invalid error code: " + de::toString(getResultName(result)));

				if (!validateAndLog(context.getTestContext().getLog(), recorder, 0u))
					return tcu::TestStatus::fail("Invalid allocation callback");
			}
		}
	}

	if (numPassingAllocs == 0)
	{
		if (isPooledObject<typename Object::Type>())
			return tcu::TestStatus::pass("Not validated: pooled objects didn't seem to use host memory");
		else
			return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Allocation callbacks not called");
	}
	else
		return tcu::TestStatus::pass("Ok");
}

// Utilities for creating groups

template<typename Object>
struct NamedParameters
{
	const char*						name;
	typename Object::Parameters		parameters;
};

template<typename Object>
struct CaseDescription
{
	typename FunctionInstance1<typename Object::Parameters>::Function	function;
	const NamedParameters<Object>*										paramsBegin;
	const NamedParameters<Object>*										paramsEnd;
	typename FunctionSupport1<typename Object::Parameters>::Function	supportFunction;
};

#define EMPTY_CASE_DESC(OBJECT)	\
	{ (FunctionInstance1<OBJECT::Parameters>::Function)DE_NULL, DE_NULL, DE_NULL, DE_NULL }

#define CASE_DESC(FUNCTION, CASES, SUPPORT)	\
	{ FUNCTION, DE_ARRAY_BEGIN(CASES), DE_ARRAY_END(CASES), SUPPORT	}

struct CaseDescriptions
{
	CaseDescription<Instance>				instance;
	CaseDescription<Device>					device;
	CaseDescription<DeviceGroup>			deviceGroup;
	CaseDescription<DeviceMemory>			deviceMemory;
	CaseDescription<Buffer>					buffer;
	CaseDescription<BufferView>				bufferView;
	CaseDescription<Image>					image;
	CaseDescription<ImageView>				imageView;
	CaseDescription<Semaphore>				semaphore;
	CaseDescription<Event>					event;
	CaseDescription<Fence>					fence;
	CaseDescription<QueryPool>				queryPool;
	CaseDescription<ShaderModule>			shaderModule;
	CaseDescription<PipelineCache>			pipelineCache;
	CaseDescription<PipelineLayout>			pipelineLayout;
	CaseDescription<RenderPass>				renderPass;
	CaseDescription<GraphicsPipeline>		graphicsPipeline;
	CaseDescription<ComputePipeline>		computePipeline;
	CaseDescription<DescriptorSetLayout>	descriptorSetLayout;
	CaseDescription<Sampler>				sampler;
	CaseDescription<DescriptorPool>			descriptorPool;
	CaseDescription<DescriptorSet>			descriptorSet;
	CaseDescription<Framebuffer>			framebuffer;
	CaseDescription<CommandPool>			commandPool;
	CaseDescription<CommandBuffer>			commandBuffer;
};

template<typename Object>
void addCases (tcu::TestCaseGroup *group, const CaseDescription<Object>& cases)
{
	for (const NamedParameters<Object>* cur = cases.paramsBegin; cur != cases.paramsEnd; ++cur)
	{
		if(cases.supportFunction == DE_NULL)
			addFunctionCase(group, cur->name, "", cases.function, cur->parameters);
		else
			addFunctionCase(group, cur->name, "", cases.supportFunction, cases.function, cur->parameters);
	}
}

void checkImageCubeArraySupport (Context& context, const ImageView::Parameters params) {
	if (params.viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY && !context.getDeviceFeatures().imageCubeArray)
		TCU_THROW(NotSupportedError, "imageCubeArray feature is not supported by this implementation");
}

void checkEventSupport (Context& context, const Event::Parameters)
{
#ifndef CTS_USES_VULKANSC
	if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") && !context.getPortabilitySubsetFeatures().events)
		TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Events are not supported by this implementation");
#else
	DE_UNREF(context);
#endif // CTS_USES_VULKANSC
}

void checkRecycleDescriptorSetMemorySupport (Context& context, const DescriptorSet::Parameters)
{
#ifdef CTS_USES_VULKANSC
	if (!context.getDeviceVulkanSC10Properties().recycleDescriptorSetMemory)
		TCU_THROW(NotSupportedError, "VkPhysicalDeviceVulkanSC10Properties::recycleDescriptorSetMemory not supported by this implementation");
#else
	DE_UNREF(context);
#endif // CTS_USES_VULKANSC
}

template<typename Object>
void addCasesWithProgs (tcu::TestCaseGroup *group, const CaseDescription<Object>& cases)
{
	for (const NamedParameters<Object>* cur = cases.paramsBegin; cur != cases.paramsEnd; ++cur)
	{
		if (cases.supportFunction == DE_NULL)
			addFunctionCaseWithPrograms(group, cur->name, "", Object::initPrograms, cases.function, cur->parameters);
		else
			addFunctionCaseWithPrograms(group, cur->name, "", cases.supportFunction, Object::initPrograms, cases.function, cur->parameters);
	}
}

static void createTests (tcu::TestCaseGroup* group, CaseDescriptions cases)
{
	addCases			(group, cases.instance);
	addCases			(group, cases.device);
	addCases			(group, cases.deviceGroup);
	addCases			(group, cases.deviceMemory);
	addCases			(group, cases.buffer);
	addCases			(group, cases.bufferView);
	addCases			(group, cases.image);
	addCases			(group, cases.imageView);
	addCases			(group, cases.semaphore);
	addCases			(group, cases.event);
	addCases			(group, cases.fence);
	addCases			(group, cases.queryPool);
	addCases			(group, cases.sampler);
	addCasesWithProgs	(group, cases.shaderModule);
#ifndef CTS_USES_VULKANSC
	addCases			(group, cases.pipelineCache);
#else
	addCasesWithProgs	(group, cases.pipelineCache);
#endif // CTS_USES_VULKANSC
	addCases			(group, cases.pipelineLayout);
	addCases			(group, cases.renderPass);
	addCasesWithProgs	(group, cases.graphicsPipeline);
	addCasesWithProgs	(group, cases.computePipeline);
	addCases			(group, cases.descriptorSetLayout);
	addCases			(group, cases.descriptorPool);
	addCases			(group, cases.descriptorSet);
	addCases			(group, cases.framebuffer);
	addCases			(group, cases.commandPool);
	addCases			(group, cases.commandBuffer);
}

#ifndef CTS_USES_VULKANSC
static void cleanupGroup (tcu::TestCaseGroup* group, CaseDescriptions cases)
{
	DE_UNREF(group);
	DE_UNREF(cases);
	// Destroy singleton object

	SingletonDevice::destroy();
}
#endif // CTS_USES_VULKANSC

tcu::TestCaseGroup* createGroup (tcu::TestContext& testCtx, const char* name, const char* desc, const CaseDescriptions& cases)
{
	MovePtr<tcu::TestCaseGroup>	group	(new tcu::TestCaseGroup(testCtx, name, desc));
	createTests(group.get(), cases);
	return group.release();
}

} // anonymous

tcu::TestCaseGroup* createObjectManagementTests (tcu::TestContext& testCtx)
{
	MovePtr<tcu::TestCaseGroup>	objectMgmtTests	(new tcu::TestCaseGroup(testCtx, "object_management", "Object management tests"));

	const Image::Parameters		img1D			(0u, VK_IMAGE_TYPE_1D, VK_FORMAT_R8G8B8A8_UNORM, makeExtent3D(256,   1, 1), 1u,  4u, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_UNDEFINED);
	const Image::Parameters		img2D			(0u, VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, makeExtent3D( 64,  64, 1), 1u, 12u, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_LAYOUT_UNDEFINED);
	const Image::Parameters		imgCube			(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, makeExtent3D( 64,  64, 1), 1u, 12u, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_LAYOUT_UNDEFINED);
	const Image::Parameters		img3D			(0u, VK_IMAGE_TYPE_3D, VK_FORMAT_R8G8B8A8_UNORM, makeExtent3D( 64,  64, 4), 1u,  1u, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_UNDEFINED);
	const ImageView::Parameters	imgView1D		(img1D, VK_IMAGE_VIEW_TYPE_1D,			img1D.format, makeComponentMappingRGBA(), makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));
	const ImageView::Parameters	imgView1DArr	(img1D, VK_IMAGE_VIEW_TYPE_1D_ARRAY,	img1D.format, makeComponentMappingRGBA(), makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 4u));
	const ImageView::Parameters	imgView2D		(img2D, VK_IMAGE_VIEW_TYPE_2D,			img2D.format, makeComponentMappingRGBA(), makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));
	const ImageView::Parameters	imgView2DArr	(img2D, VK_IMAGE_VIEW_TYPE_2D_ARRAY,	img2D.format, makeComponentMappingRGBA(), makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 8u));
	const ImageView::Parameters	imgViewCube		(imgCube, VK_IMAGE_VIEW_TYPE_CUBE,		img2D.format, makeComponentMappingRGBA(), makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 6u));
	const ImageView::Parameters	imgViewCubeArr	(imgCube, VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,	img2D.format, makeComponentMappingRGBA(), makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 12u));
	const ImageView::Parameters	imgView3D		(img3D, VK_IMAGE_VIEW_TYPE_3D,			img3D.format, makeComponentMappingRGBA(), makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));

	const DescriptorSetLayout::Parameters	singleUboDescLayout	= DescriptorSetLayout::Parameters::single(0u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u, VK_SHADER_STAGE_VERTEX_BIT);

	static const NamedParameters<Instance>					s_instanceCases[]				=
	{
		{ "instance",					Instance::Parameters() },
	};
	// \note Device index may change - must not be static

	const NamedParameters<Device>				s_deviceCases[]					=
	{
		{ "device",						Device::Parameters(testCtx.getCommandLine().getVKDeviceId()-1u, VK_QUEUE_GRAPHICS_BIT)	},
	};
	const NamedParameters<DeviceGroup>					s_deviceGroupCases[]			=
	{
		{ "device_group",				DeviceGroup::Parameters(testCtx.getCommandLine().getVKDeviceGroupId() - 1u, testCtx.getCommandLine().getVKDeviceId() - 1u, VK_QUEUE_GRAPHICS_BIT) },
	};
	static const NamedParameters<DeviceMemory>			s_deviceMemCases[]				=
	{
		{ "device_memory_small",		DeviceMemory::Parameters(1024, 0u)	},
	};
	static const NamedParameters<Buffer>				s_bufferCases[]					=
	{
		{ "buffer_uniform_small",		Buffer::Parameters(1024u,			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),	},
		{ "buffer_uniform_large",		Buffer::Parameters(1024u*1024u*16u,	VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),	},
		{ "buffer_storage_small",		Buffer::Parameters(1024u,			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),	},
		{ "buffer_storage_large",		Buffer::Parameters(1024u*1024u*16u,	VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),	},
	};
	static const NamedParameters<BufferView>			s_bufferViewCases[]				=
	{
		{ "buffer_view_uniform_r8g8b8a8_unorm",	BufferView::Parameters(Buffer::Parameters(8192u, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT), VK_FORMAT_R8G8B8A8_UNORM, 0u, 4096u)	},
		{ "buffer_view_storage_r8g8b8a8_unorm",	BufferView::Parameters(Buffer::Parameters(8192u, VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT), VK_FORMAT_R8G8B8A8_UNORM, 0u, 4096u)	},
	};
	static const NamedParameters<Image>					s_imageCases[]					=
	{
		{ "image_1d",					img1D		},
		{ "image_2d",					img2D		},
		{ "image_3d",					img3D		},
	};
	static const NamedParameters<ImageView>				s_imageViewCases[]				=
	{
		{ "image_view_1d",				imgView1D		},
		{ "image_view_1d_arr",			imgView1DArr	},
		{ "image_view_2d",				imgView2D		},
		{ "image_view_2d_arr",			imgView2DArr	},
		{ "image_view_cube",			imgViewCube		},
		{ "image_view_cube_arr",		imgViewCubeArr	},
		{ "image_view_3d",				imgView3D		},
	};
	static const NamedParameters<Semaphore>				s_semaphoreCases[]				=
	{
		{ "semaphore",					Semaphore::Parameters(0u),	}
	};
	static const NamedParameters<Event>					s_eventCases[]					=
	{
		{ "event",						Event::Parameters(0u)		}
	};
	static const NamedParameters<Fence>					s_fenceCases[]					=
	{
		{ "fence",						Fence::Parameters(0u)								},
		{ "fence_signaled",				Fence::Parameters(VK_FENCE_CREATE_SIGNALED_BIT)		}
	};
	static const NamedParameters<QueryPool>				s_queryPoolCases[]				=
	{
		{ "query_pool",					QueryPool::Parameters(VK_QUERY_TYPE_OCCLUSION, 1u, 0u)	}
	};
	static const NamedParameters<ShaderModule>			s_shaderModuleCases[]			=
	{
		{ "shader_module",				ShaderModule::Parameters(VK_SHADER_STAGE_COMPUTE_BIT, "test")	}
	};
	static const NamedParameters<PipelineCache>			s_pipelineCacheCases[]			=
	{
		{ "pipeline_cache",				PipelineCache::Parameters()		}
	};
	static const NamedParameters<PipelineLayout>		s_pipelineLayoutCases[]			=
	{
		{ "pipeline_layout_empty",		PipelineLayout::Parameters::empty()										},
		{ "pipeline_layout_single",		PipelineLayout::Parameters::singleDescriptorSet(singleUboDescLayout)	}
	};
	static const NamedParameters<RenderPass>			s_renderPassCases[]				=
	{
		{ "render_pass",				RenderPass::Parameters()		}
	};
	static const NamedParameters<GraphicsPipeline>		s_graphicsPipelineCases[]		=
	{
		{ "graphics_pipeline",			GraphicsPipeline::Parameters()	}
	};
	static const NamedParameters<ComputePipeline>		s_computePipelineCases[]		=
	{
		{ "compute_pipeline",			ComputePipeline::Parameters()	}
	};
	static const NamedParameters<DescriptorSetLayout>	s_descriptorSetLayoutCases[]	=
	{
		{ "descriptor_set_layout_empty",	DescriptorSetLayout::Parameters::empty()	},
		{ "descriptor_set_layout_single",	singleUboDescLayout							}
	};
	static const NamedParameters<Sampler>				s_samplerCases[]				=
	{
		{ "sampler",					Sampler::Parameters()	}
	};
	static const NamedParameters<DescriptorPool>		s_descriptorPoolCases[]			=
	{
		{ "descriptor_pool",						DescriptorPool::Parameters::singleType((VkDescriptorPoolCreateFlags)0,						4u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3u)	},
		{ "descriptor_pool_free_descriptor_set",	DescriptorPool::Parameters::singleType(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,	4u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3u)	}
	};
	static const NamedParameters<DescriptorSet>			s_descriptorSetCases[]			=
	{
		{ "descriptor_set",				DescriptorSet::Parameters(singleUboDescLayout)	}
	};
	static const NamedParameters<Framebuffer>			s_framebufferCases[]			=
	{
		{ "framebuffer",				Framebuffer::Parameters()	}
	};
	static const NamedParameters<CommandPool>			s_commandPoolCases[]			=
	{
		{ "command_pool",				CommandPool::Parameters((VkCommandPoolCreateFlags)0)			},
		{ "command_pool_transient",		CommandPool::Parameters(VK_COMMAND_POOL_CREATE_TRANSIENT_BIT)	}
	};
	static const NamedParameters<CommandBuffer>			s_commandBufferCases[]			=
	{
		{ "command_buffer_primary",		CommandBuffer::Parameters(CommandPool::Parameters((VkCommandPoolCreateFlags)0u), VK_COMMAND_BUFFER_LEVEL_PRIMARY)	},
		{ "command_buffer_secondary",	CommandBuffer::Parameters(CommandPool::Parameters((VkCommandPoolCreateFlags)0u), VK_COMMAND_BUFFER_LEVEL_SECONDARY)	}
	};

	const CaseDescriptions	s_createSingleGroup	=
	{
		CASE_DESC(createSingleTest	<Instance>,					s_instanceCases,			DE_NULL),
		CASE_DESC(createSingleTest	<Device>,					s_deviceCases,				DE_NULL),
		CASE_DESC(createSingleTest	<DeviceGroup>,				s_deviceGroupCases,			DE_NULL),
		CASE_DESC(createSingleTest	<DeviceMemory>,				s_deviceMemCases,			DE_NULL),
		CASE_DESC(createSingleTest	<Buffer>,					s_bufferCases,				DE_NULL),
		CASE_DESC(createSingleTest	<BufferView>,				s_bufferViewCases,			DE_NULL),
		CASE_DESC(createSingleTest	<Image>,					s_imageCases,				DE_NULL),
		CASE_DESC(createSingleTest	<ImageView>,				s_imageViewCases,			checkImageCubeArraySupport),
		CASE_DESC(createSingleTest	<Semaphore>,				s_semaphoreCases,			DE_NULL),
		CASE_DESC(createSingleTest	<Event>,					s_eventCases,				checkEventSupport),
		CASE_DESC(createSingleTest	<Fence>,					s_fenceCases,				DE_NULL),
		CASE_DESC(createSingleTest	<QueryPool>,				s_queryPoolCases,			DE_NULL),
		CASE_DESC(createSingleTest	<ShaderModule>,				s_shaderModuleCases,		DE_NULL),
		CASE_DESC(createSingleTest	<PipelineCache>,			s_pipelineCacheCases,		DE_NULL),
		CASE_DESC(createSingleTest	<PipelineLayout>,			s_pipelineLayoutCases,		DE_NULL),
		CASE_DESC(createSingleTest	<RenderPass>,				s_renderPassCases,			DE_NULL),
		CASE_DESC(createSingleTest	<GraphicsPipeline>,			s_graphicsPipelineCases,	DE_NULL),
		CASE_DESC(createSingleTest	<ComputePipeline>,			s_computePipelineCases,		DE_NULL),
		CASE_DESC(createSingleTest	<DescriptorSetLayout>,		s_descriptorSetLayoutCases,	DE_NULL),
		CASE_DESC(createSingleTest	<Sampler>,					s_samplerCases,				DE_NULL),
		CASE_DESC(createSingleTest	<DescriptorPool>,			s_descriptorPoolCases,		DE_NULL),
		CASE_DESC(createSingleTest	<DescriptorSet>,			s_descriptorSetCases,		DE_NULL),
		CASE_DESC(createSingleTest	<Framebuffer>,				s_framebufferCases,			DE_NULL),
		CASE_DESC(createSingleTest	<CommandPool>,				s_commandPoolCases,			DE_NULL),
		CASE_DESC(createSingleTest	<CommandBuffer>,			s_commandBufferCases,		DE_NULL),
	};
	objectMgmtTests->addChild(createGroup(testCtx, "single", "Create single object", s_createSingleGroup));

	const CaseDescriptions	s_createMultipleUniqueResourcesGroup	=
	{
		CASE_DESC(createMultipleUniqueResourcesTest	<Instance>,					s_instanceCases,			DE_NULL),
#ifndef CTS_USES_VULKANSC
		CASE_DESC(createMultipleUniqueResourcesTest	<Device>,					s_deviceCases,				DE_NULL),
		CASE_DESC(createMultipleUniqueResourcesTest	<DeviceGroup>,				s_deviceGroupCases,			DE_NULL),
#else
		EMPTY_CASE_DESC(Device),
		EMPTY_CASE_DESC(DeviceGroup),
#endif
		CASE_DESC(createMultipleUniqueResourcesTest	<DeviceMemory>,				s_deviceMemCases,			DE_NULL),
		CASE_DESC(createMultipleUniqueResourcesTest	<Buffer>,					s_bufferCases,				DE_NULL),
		CASE_DESC(createMultipleUniqueResourcesTest	<BufferView>,				s_bufferViewCases,			DE_NULL),
		CASE_DESC(createMultipleUniqueResourcesTest	<Image>,					s_imageCases,				DE_NULL),
		CASE_DESC(createMultipleUniqueResourcesTest	<ImageView>,				s_imageViewCases,			checkImageCubeArraySupport),
		CASE_DESC(createMultipleUniqueResourcesTest	<Semaphore>,				s_semaphoreCases,			DE_NULL),
		CASE_DESC(createMultipleUniqueResourcesTest	<Event>,					s_eventCases,				checkEventSupport),
		CASE_DESC(createMultipleUniqueResourcesTest	<Fence>,					s_fenceCases,				DE_NULL),
		CASE_DESC(createMultipleUniqueResourcesTest	<QueryPool>,				s_queryPoolCases,			DE_NULL),
		CASE_DESC(createMultipleUniqueResourcesTest	<ShaderModule>,				s_shaderModuleCases,		DE_NULL),
		CASE_DESC(createMultipleUniqueResourcesTest	<PipelineCache>,			s_pipelineCacheCases,		DE_NULL),
		CASE_DESC(createMultipleUniqueResourcesTest	<PipelineLayout>,			s_pipelineLayoutCases,		DE_NULL),
		CASE_DESC(createMultipleUniqueResourcesTest	<RenderPass>,				s_renderPassCases,			DE_NULL),
		CASE_DESC(createMultipleUniqueResourcesTest	<GraphicsPipeline>,			s_graphicsPipelineCases,	DE_NULL),
		CASE_DESC(createMultipleUniqueResourcesTest	<ComputePipeline>,			s_computePipelineCases,		DE_NULL),
		CASE_DESC(createMultipleUniqueResourcesTest	<DescriptorSetLayout>,		s_descriptorSetLayoutCases,	DE_NULL),
		CASE_DESC(createMultipleUniqueResourcesTest	<Sampler>,					s_samplerCases,				DE_NULL),
		CASE_DESC(createMultipleUniqueResourcesTest	<DescriptorPool>,			s_descriptorPoolCases,		DE_NULL),
		CASE_DESC(createMultipleUniqueResourcesTest	<DescriptorSet>,			s_descriptorSetCases,		DE_NULL),
		CASE_DESC(createMultipleUniqueResourcesTest	<Framebuffer>,				s_framebufferCases,			DE_NULL),
		CASE_DESC(createMultipleUniqueResourcesTest	<CommandPool>,				s_commandPoolCases,			DE_NULL),
		CASE_DESC(createMultipleUniqueResourcesTest	<CommandBuffer>,			s_commandBufferCases,		DE_NULL),
	};
	objectMgmtTests->addChild(createGroup(testCtx, "multiple_unique_resources", "Multiple objects with per-object unique resources", s_createMultipleUniqueResourcesGroup));

	const CaseDescriptions	s_createMultipleSharedResourcesGroup	=
	{
		EMPTY_CASE_DESC(Instance), // No resources used
#ifndef CTS_USES_VULKANSC
		CASE_DESC(createMultipleSharedResourcesTest	<Device>,					s_deviceCases,				DE_NULL),
		CASE_DESC(createMultipleSharedResourcesTest	<DeviceGroup>,				s_deviceGroupCases,			DE_NULL),
#else
		EMPTY_CASE_DESC(Device),
		EMPTY_CASE_DESC(DeviceGroup),
#endif
		CASE_DESC(createMultipleSharedResourcesTest	<DeviceMemory>,				s_deviceMemCases,			DE_NULL),
		CASE_DESC(createMultipleSharedResourcesTest	<Buffer>,					s_bufferCases,				DE_NULL),
		CASE_DESC(createMultipleSharedResourcesTest	<BufferView>,				s_bufferViewCases,			DE_NULL),
		CASE_DESC(createMultipleSharedResourcesTest	<Image>,					s_imageCases,				DE_NULL),
		CASE_DESC(createMultipleSharedResourcesTest	<ImageView>,				s_imageViewCases,			checkImageCubeArraySupport),
		CASE_DESC(createMultipleSharedResourcesTest	<Semaphore>,				s_semaphoreCases,			DE_NULL),
		CASE_DESC(createMultipleSharedResourcesTest	<Event>,					s_eventCases,				checkEventSupport),
		CASE_DESC(createMultipleSharedResourcesTest	<Fence>,					s_fenceCases,				DE_NULL),
		CASE_DESC(createMultipleSharedResourcesTest	<QueryPool>,				s_queryPoolCases,			DE_NULL),
		CASE_DESC(createMultipleSharedResourcesTest	<ShaderModule>,				s_shaderModuleCases,		DE_NULL),
		CASE_DESC(createMultipleSharedResourcesTest	<PipelineCache>,			s_pipelineCacheCases,		DE_NULL),
		CASE_DESC(createMultipleSharedResourcesTest	<PipelineLayout>,			s_pipelineLayoutCases,		DE_NULL),
		CASE_DESC(createMultipleSharedResourcesTest	<RenderPass>,				s_renderPassCases,			DE_NULL),
		CASE_DESC(createMultipleSharedResourcesTest	<GraphicsPipeline>,			s_graphicsPipelineCases,	DE_NULL),
		CASE_DESC(createMultipleSharedResourcesTest	<ComputePipeline>,			s_computePipelineCases,		DE_NULL),
		CASE_DESC(createMultipleSharedResourcesTest	<DescriptorSetLayout>,		s_descriptorSetLayoutCases,	DE_NULL),
		CASE_DESC(createMultipleSharedResourcesTest	<Sampler>,					s_samplerCases,				DE_NULL),
		CASE_DESC(createMultipleSharedResourcesTest	<DescriptorPool>,			s_descriptorPoolCases,		DE_NULL),
		CASE_DESC(createMultipleSharedResourcesTest	<DescriptorSet>,			s_descriptorSetCases,		DE_NULL),
		CASE_DESC(createMultipleSharedResourcesTest	<Framebuffer>,				s_framebufferCases,			DE_NULL),
		CASE_DESC(createMultipleSharedResourcesTest	<CommandPool>,				s_commandPoolCases,			DE_NULL),
		CASE_DESC(createMultipleSharedResourcesTest	<CommandBuffer>,			s_commandBufferCases,		DE_NULL),
	};
	objectMgmtTests->addChild(createGroup(testCtx, "multiple_shared_resources", "Multiple objects with shared resources", s_createMultipleSharedResourcesGroup));

#ifndef CTS_USES_VULKANSC
// Removed from Vulkan SC test set: VkAllocationCallbacks is not supported and pointers to this type must be NULL
	const CaseDescriptions	s_createMaxConcurrentGroup	=
	{
		CASE_DESC(createMaxConcurrentTest	<Instance>,					s_instanceCases,			DE_NULL),
		CASE_DESC(createMaxConcurrentTest	<Device>,					s_deviceCases,				DE_NULL),
		CASE_DESC(createMaxConcurrentTest	<DeviceGroup>,				s_deviceGroupCases,			DE_NULL),
		CASE_DESC(createMaxConcurrentTest	<DeviceMemory>,				s_deviceMemCases,			DE_NULL),
		CASE_DESC(createMaxConcurrentTest	<Buffer>,					s_bufferCases,				DE_NULL),
		CASE_DESC(createMaxConcurrentTest	<BufferView>,				s_bufferViewCases,			DE_NULL),
		CASE_DESC(createMaxConcurrentTest	<Image>,					s_imageCases,				DE_NULL),
		CASE_DESC(createMaxConcurrentTest	<ImageView>,				s_imageViewCases,			checkImageCubeArraySupport),
		CASE_DESC(createMaxConcurrentTest	<Semaphore>,				s_semaphoreCases,			DE_NULL),
		CASE_DESC(createMaxConcurrentTest	<Event>,					s_eventCases,				checkEventSupport),
		CASE_DESC(createMaxConcurrentTest	<Fence>,					s_fenceCases,				DE_NULL),
		CASE_DESC(createMaxConcurrentTest	<QueryPool>,				s_queryPoolCases,			DE_NULL),
		CASE_DESC(createMaxConcurrentTest	<ShaderModule>,				s_shaderModuleCases,		DE_NULL),
		CASE_DESC(createMaxConcurrentTest	<PipelineCache>,			s_pipelineCacheCases,		DE_NULL),
		CASE_DESC(createMaxConcurrentTest	<PipelineLayout>,			s_pipelineLayoutCases,		DE_NULL),
		CASE_DESC(createMaxConcurrentTest	<RenderPass>,				s_renderPassCases,			DE_NULL),
		CASE_DESC(createMaxConcurrentTest	<GraphicsPipeline>,			s_graphicsPipelineCases,	DE_NULL),
		CASE_DESC(createMaxConcurrentTest	<ComputePipeline>,			s_computePipelineCases,		DE_NULL),
		CASE_DESC(createMaxConcurrentTest	<DescriptorSetLayout>,		s_descriptorSetLayoutCases,	DE_NULL),
		CASE_DESC(createMaxConcurrentTest	<Sampler>,					s_samplerCases,				DE_NULL),
		CASE_DESC(createMaxConcurrentTest	<DescriptorPool>,			s_descriptorPoolCases,		DE_NULL),
		CASE_DESC(createMaxConcurrentTest	<DescriptorSet>,			s_descriptorSetCases,		DE_NULL),
		CASE_DESC(createMaxConcurrentTest	<Framebuffer>,				s_framebufferCases,			DE_NULL),
		CASE_DESC(createMaxConcurrentTest	<CommandPool>,				s_commandPoolCases,			DE_NULL),
		CASE_DESC(createMaxConcurrentTest	<CommandBuffer>,			s_commandBufferCases,		DE_NULL),
	};
	objectMgmtTests->addChild(createGroup(testCtx, "max_concurrent", "Maximum number of concurrently live objects", s_createMaxConcurrentGroup));
#endif // CTS_USES_VULKANSC

	const CaseDescriptions	s_multithreadedCreatePerThreadDeviceGroup	=
	{
		EMPTY_CASE_DESC(Instance),		// Does not make sense
		EMPTY_CASE_DESC(Device),		// Does not make sense
		EMPTY_CASE_DESC(DeviceGroup),	// Does not make sense
		CASE_DESC(multithreadedCreatePerThreadDeviceTest	<DeviceMemory>,				s_deviceMemCases,			DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadDeviceTest	<Buffer>,					s_bufferCases,				DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadDeviceTest	<BufferView>,				s_bufferViewCases,			DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadDeviceTest	<Image>,					s_imageCases,				DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadDeviceTest	<ImageView>,				s_imageViewCases,			checkImageCubeArraySupport),
		CASE_DESC(multithreadedCreatePerThreadDeviceTest	<Semaphore>,				s_semaphoreCases,			DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadDeviceTest	<Event>,					s_eventCases,				checkEventSupport),
		CASE_DESC(multithreadedCreatePerThreadDeviceTest	<Fence>,					s_fenceCases,				DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadDeviceTest	<QueryPool>,				s_queryPoolCases,			DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadDeviceTest	<ShaderModule>,				s_shaderModuleCases,		DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadDeviceTest	<PipelineCache>,			s_pipelineCacheCases,		DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadDeviceTest	<PipelineLayout>,			s_pipelineLayoutCases,		DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadDeviceTest	<RenderPass>,				s_renderPassCases,			DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadDeviceTest	<GraphicsPipeline>,			s_graphicsPipelineCases,	DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadDeviceTest	<ComputePipeline>,			s_computePipelineCases,		DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadDeviceTest	<DescriptorSetLayout>,		s_descriptorSetLayoutCases,	DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadDeviceTest	<Sampler>,					s_samplerCases,				DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadDeviceTest	<DescriptorPool>,			s_descriptorPoolCases,		DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadDeviceTest	<DescriptorSet>,			s_descriptorSetCases,		checkRecycleDescriptorSetMemorySupport),
		CASE_DESC(multithreadedCreatePerThreadDeviceTest	<Framebuffer>,				s_framebufferCases,			DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadDeviceTest	<CommandPool>,				s_commandPoolCases,			DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadDeviceTest	<CommandBuffer>,			s_commandBufferCases,		DE_NULL),
	};
	objectMgmtTests->addChild(createGroup(testCtx, "multithreaded_per_thread_device", "Multithreaded object construction with per-thread device ", s_multithreadedCreatePerThreadDeviceGroup));

	const CaseDescriptions	s_multithreadedCreatePerThreadResourcesGroup	=
	{
		CASE_DESC(multithreadedCreatePerThreadResourcesTest	<Instance>,					s_instanceCases,			DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadResourcesTest	<Device>,					s_deviceCases,				DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadResourcesTest	<DeviceGroup>,				s_deviceGroupCases,			DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadResourcesTest	<DeviceMemory>,				s_deviceMemCases,			DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadResourcesTest	<Buffer>,					s_bufferCases,				DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadResourcesTest	<BufferView>,				s_bufferViewCases,			DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadResourcesTest	<Image>,					s_imageCases,				DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadResourcesTest	<ImageView>,				s_imageViewCases,			checkImageCubeArraySupport),
		CASE_DESC(multithreadedCreatePerThreadResourcesTest	<Semaphore>,				s_semaphoreCases,			DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadResourcesTest	<Event>,					s_eventCases,				checkEventSupport),
		CASE_DESC(multithreadedCreatePerThreadResourcesTest	<Fence>,					s_fenceCases,				DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadResourcesTest	<QueryPool>,				s_queryPoolCases,			DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadResourcesTest	<ShaderModule>,				s_shaderModuleCases,		DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadResourcesTest	<PipelineCache>,			s_pipelineCacheCases,		DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadResourcesTest	<PipelineLayout>,			s_pipelineLayoutCases,		DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadResourcesTest	<RenderPass>,				s_renderPassCases,			DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadResourcesTest	<GraphicsPipeline>,			s_graphicsPipelineCases,	DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadResourcesTest	<ComputePipeline>,			s_computePipelineCases,		DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadResourcesTest	<DescriptorSetLayout>,		s_descriptorSetLayoutCases,	DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadResourcesTest	<Sampler>,					s_samplerCases,				DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadResourcesTest	<DescriptorPool>,			s_descriptorPoolCases,		DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadResourcesTest	<DescriptorSet>,			s_descriptorSetCases,		checkRecycleDescriptorSetMemorySupport),
		CASE_DESC(multithreadedCreatePerThreadResourcesTest	<Framebuffer>,				s_framebufferCases,			DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadResourcesTest	<CommandPool>,				s_commandPoolCases,			DE_NULL),
		CASE_DESC(multithreadedCreatePerThreadResourcesTest	<CommandBuffer>,			s_commandBufferCases,		DE_NULL),
	};
	objectMgmtTests->addChild(createGroup(testCtx, "multithreaded_per_thread_resources", "Multithreaded object construction with per-thread resources", s_multithreadedCreatePerThreadResourcesGroup));

	const CaseDescriptions	s_multithreadedCreateSharedResourcesGroup	=
	{
		EMPTY_CASE_DESC(Instance),
#ifndef CTS_USES_VULKANSC
		CASE_DESC(multithreadedCreateSharedResourcesTest	<Device>,					s_deviceCases,				DE_NULL),
		CASE_DESC(multithreadedCreateSharedResourcesTest	<DeviceGroup>,				s_deviceGroupCases,			DE_NULL),
#else
		EMPTY_CASE_DESC(Device),
		EMPTY_CASE_DESC(DeviceGroup),
#endif
		CASE_DESC(multithreadedCreateSharedResourcesTest	<DeviceMemory>,				s_deviceMemCases,			DE_NULL),
		CASE_DESC(multithreadedCreateSharedResourcesTest	<Buffer>,					s_bufferCases,				DE_NULL),
		CASE_DESC(multithreadedCreateSharedResourcesTest	<BufferView>,				s_bufferViewCases,			DE_NULL),
		CASE_DESC(multithreadedCreateSharedResourcesTest	<Image>,					s_imageCases,				DE_NULL),
		CASE_DESC(multithreadedCreateSharedResourcesTest	<ImageView>,				s_imageViewCases,			checkImageCubeArraySupport),
		CASE_DESC(multithreadedCreateSharedResourcesTest	<Semaphore>,				s_semaphoreCases,			DE_NULL),
		CASE_DESC(multithreadedCreateSharedResourcesTest	<Event>,					s_eventCases,				checkEventSupport),
		CASE_DESC(multithreadedCreateSharedResourcesTest	<Fence>,					s_fenceCases,				DE_NULL),
		CASE_DESC(multithreadedCreateSharedResourcesTest	<QueryPool>,				s_queryPoolCases,			DE_NULL),
		CASE_DESC(multithreadedCreateSharedResourcesTest	<ShaderModule>,				s_shaderModuleCases,		DE_NULL),
		CASE_DESC(multithreadedCreateSharedResourcesTest	<PipelineCache>,			s_pipelineCacheCases,		DE_NULL),
		CASE_DESC(multithreadedCreateSharedResourcesTest	<PipelineLayout>,			s_pipelineLayoutCases,		DE_NULL),
		CASE_DESC(multithreadedCreateSharedResourcesTest	<RenderPass>,				s_renderPassCases,			DE_NULL),
		CASE_DESC(multithreadedCreateSharedResourcesTest	<GraphicsPipeline>,			s_graphicsPipelineCases,	DE_NULL),
		CASE_DESC(multithreadedCreateSharedResourcesTest	<ComputePipeline>,			s_computePipelineCases,		DE_NULL),
		CASE_DESC(multithreadedCreateSharedResourcesTest	<DescriptorSetLayout>,		s_descriptorSetLayoutCases,	DE_NULL),
		CASE_DESC(multithreadedCreateSharedResourcesTest	<Sampler>,					s_samplerCases,				DE_NULL),
		CASE_DESC(multithreadedCreateSharedResourcesTest	<DescriptorPool>,			s_descriptorPoolCases,		DE_NULL),
		EMPTY_CASE_DESC(DescriptorSet),		// \note Needs per-thread DescriptorPool
		CASE_DESC(multithreadedCreateSharedResourcesTest	<Framebuffer>,				s_framebufferCases,			DE_NULL),
		CASE_DESC(multithreadedCreateSharedResourcesTest	<CommandPool>,				s_commandPoolCases,			DE_NULL),
		EMPTY_CASE_DESC(CommandBuffer),			// \note Needs per-thread CommandPool
	};
	objectMgmtTests->addChild(createGroup(testCtx, "multithreaded_shared_resources", "Multithreaded object construction with shared resources", s_multithreadedCreateSharedResourcesGroup));

#ifndef CTS_USES_VULKANSC

// Removed from Vulkan SC test set: VkAllocationCallbacks is not supported and pointers to this type must be NULL
	const CaseDescriptions	s_createSingleAllocCallbacksGroup	=
	{
		CASE_DESC(createSingleAllocCallbacksTest	<Instance>,					s_instanceCases,			DE_NULL),
		CASE_DESC(createSingleAllocCallbacksTest	<Device>,					s_deviceCases,				DE_NULL),
		CASE_DESC(createSingleAllocCallbacksTest	<DeviceGroup>,				s_deviceGroupCases,			DE_NULL),
		CASE_DESC(createSingleAllocCallbacksTest	<DeviceMemory>,				s_deviceMemCases,			DE_NULL),
		CASE_DESC(createSingleAllocCallbacksTest	<Buffer>,					s_bufferCases,				DE_NULL),
		CASE_DESC(createSingleAllocCallbacksTest	<BufferView>,				s_bufferViewCases,			DE_NULL),
		CASE_DESC(createSingleAllocCallbacksTest	<Image>,					s_imageCases,				DE_NULL),
		CASE_DESC(createSingleAllocCallbacksTest	<ImageView>,				s_imageViewCases,			checkImageCubeArraySupport),
		CASE_DESC(createSingleAllocCallbacksTest	<Semaphore>,				s_semaphoreCases,			DE_NULL),
		CASE_DESC(createSingleAllocCallbacksTest	<Event>,					s_eventCases,				checkEventSupport),
		CASE_DESC(createSingleAllocCallbacksTest	<Fence>,					s_fenceCases,				DE_NULL),
		CASE_DESC(createSingleAllocCallbacksTest	<QueryPool>,				s_queryPoolCases,			DE_NULL),
		CASE_DESC(createSingleAllocCallbacksTest	<ShaderModule>,				s_shaderModuleCases,		DE_NULL),
		CASE_DESC(createSingleAllocCallbacksTest	<PipelineCache>,			s_pipelineCacheCases,		DE_NULL),
		CASE_DESC(createSingleAllocCallbacksTest	<PipelineLayout>,			s_pipelineLayoutCases,		DE_NULL),
		CASE_DESC(createSingleAllocCallbacksTest	<RenderPass>,				s_renderPassCases,			DE_NULL),
		CASE_DESC(createSingleAllocCallbacksTest	<GraphicsPipeline>,			s_graphicsPipelineCases,	DE_NULL),
		CASE_DESC(createSingleAllocCallbacksTest	<ComputePipeline>,			s_computePipelineCases,		DE_NULL),
		CASE_DESC(createSingleAllocCallbacksTest	<DescriptorSetLayout>,		s_descriptorSetLayoutCases,	DE_NULL),
		CASE_DESC(createSingleAllocCallbacksTest	<Sampler>,					s_samplerCases,				DE_NULL),
		CASE_DESC(createSingleAllocCallbacksTest	<DescriptorPool>,			s_descriptorPoolCases,		DE_NULL),
		CASE_DESC(createSingleAllocCallbacksTest	<DescriptorSet>,			s_descriptorSetCases,		DE_NULL),
		CASE_DESC(createSingleAllocCallbacksTest	<Framebuffer>,				s_framebufferCases,			DE_NULL),
		CASE_DESC(createSingleAllocCallbacksTest	<CommandPool>,				s_commandPoolCases,			DE_NULL),
		CASE_DESC(createSingleAllocCallbacksTest	<CommandBuffer>,			s_commandBufferCases,		DE_NULL),
	};
	objectMgmtTests->addChild(createGroup(testCtx, "single_alloc_callbacks", "Create single object", s_createSingleAllocCallbacksGroup));
#endif // CTS_USES_VULKANSC


#ifndef CTS_USES_VULKANSC
	// Removed from Vulkan SC test set: VkAllocationCallbacks is not supported and pointers to this type must be NULL
	// \note Skip pooled objects in this test group. They are properly handled by the "multiple" group farther down below.
	const CaseDescriptions	s_allocCallbackFailGroup	=
	{
		CASE_DESC(allocCallbackFailTest	<Instance>,					s_instanceCases,			DE_NULL),
		CASE_DESC(allocCallbackFailTest	<Device>,					s_deviceCases,				DE_NULL),
		CASE_DESC(allocCallbackFailTest	<DeviceGroup>,				s_deviceGroupCases,			DE_NULL),
		CASE_DESC(allocCallbackFailTest	<DeviceMemory>,				s_deviceMemCases,			DE_NULL),
		CASE_DESC(allocCallbackFailTest	<Buffer>,					s_bufferCases,				DE_NULL),
		CASE_DESC(allocCallbackFailTest	<BufferView>,				s_bufferViewCases,			DE_NULL),
		CASE_DESC(allocCallbackFailTest	<Image>,					s_imageCases,				DE_NULL),
		CASE_DESC(allocCallbackFailTest	<ImageView>,				s_imageViewCases,			checkImageCubeArraySupport),
		CASE_DESC(allocCallbackFailTest	<Semaphore>,				s_semaphoreCases,			DE_NULL),
		CASE_DESC(allocCallbackFailTest	<Event>,					s_eventCases,				checkEventSupport),
		CASE_DESC(allocCallbackFailTest	<Fence>,					s_fenceCases,				DE_NULL),
		CASE_DESC(allocCallbackFailTest	<QueryPool>,				s_queryPoolCases,			DE_NULL),
		CASE_DESC(allocCallbackFailTest	<ShaderModule>,				s_shaderModuleCases,		DE_NULL),
		CASE_DESC(allocCallbackFailTest	<PipelineCache>,			s_pipelineCacheCases,		DE_NULL),
		CASE_DESC(allocCallbackFailTest	<PipelineLayout>,			s_pipelineLayoutCases,		DE_NULL),
		CASE_DESC(allocCallbackFailTest	<RenderPass>,				s_renderPassCases,			DE_NULL),
		CASE_DESC(allocCallbackFailTest	<GraphicsPipeline>,			s_graphicsPipelineCases,	DE_NULL),
		CASE_DESC(allocCallbackFailTest	<ComputePipeline>,			s_computePipelineCases,		DE_NULL),
		CASE_DESC(allocCallbackFailTest	<DescriptorSetLayout>,		s_descriptorSetLayoutCases,	DE_NULL),
		CASE_DESC(allocCallbackFailTest	<Sampler>,					s_samplerCases,				DE_NULL),
		CASE_DESC(allocCallbackFailTest	<DescriptorPool>,			s_descriptorPoolCases,		DE_NULL),
		EMPTY_CASE_DESC(DescriptorSet),
		CASE_DESC(allocCallbackFailTest	<Framebuffer>,				s_framebufferCases,			DE_NULL),
		CASE_DESC(allocCallbackFailTest	<CommandPool>,				s_commandPoolCases,			DE_NULL),
		EMPTY_CASE_DESC(CommandBuffer),
	};
	objectMgmtTests->addChild(createGroup(testCtx, "alloc_callback_fail", "Allocation callback failure", s_allocCallbackFailGroup));
#endif // CTS_USES_VULKANSC

#ifndef CTS_USES_VULKANSC
	// Removed from Vulkan SC test set: VkAllocationCallbacks is not supported and pointers to this type must be NULL
	// \note Test objects that can be created in bulk
	const CaseDescriptions	s_allocCallbackFailMultipleObjectsGroup	=
	{
		EMPTY_CASE_DESC(Instance),			// most objects can be created one at a time only
		EMPTY_CASE_DESC(Device),
		EMPTY_CASE_DESC(DeviceGroup),
		EMPTY_CASE_DESC(DeviceMemory),
		EMPTY_CASE_DESC(Buffer),
		EMPTY_CASE_DESC(BufferView),
		EMPTY_CASE_DESC(Image),
		EMPTY_CASE_DESC(ImageView),
		EMPTY_CASE_DESC(Semaphore),
		EMPTY_CASE_DESC(Event),
		EMPTY_CASE_DESC(Fence),
		EMPTY_CASE_DESC(QueryPool),
		EMPTY_CASE_DESC(ShaderModule),
		EMPTY_CASE_DESC(PipelineCache),
		EMPTY_CASE_DESC(PipelineLayout),
		EMPTY_CASE_DESC(RenderPass),
		CASE_DESC(allocCallbackFailMultipleObjectsTest <GraphicsPipeline>,		s_graphicsPipelineCases,	DE_NULL),
		CASE_DESC(allocCallbackFailMultipleObjectsTest <ComputePipeline>,		s_computePipelineCases,		DE_NULL),
		EMPTY_CASE_DESC(DescriptorSetLayout),
		EMPTY_CASE_DESC(Sampler),
		EMPTY_CASE_DESC(DescriptorPool),
		CASE_DESC(allocCallbackFailMultipleObjectsTest <DescriptorSet>,			s_descriptorSetCases,		DE_NULL),
		EMPTY_CASE_DESC(Framebuffer),
		EMPTY_CASE_DESC(CommandPool),
		CASE_DESC(allocCallbackFailMultipleObjectsTest <CommandBuffer>,			s_commandBufferCases,		DE_NULL),
	};
	objectMgmtTests->addChild(createGroup(testCtx, "alloc_callback_fail_multiple", "Allocation callback failure creating multiple objects with one call", s_allocCallbackFailMultipleObjectsGroup));
#endif // CTS_USES_VULKANSC

#ifndef CTS_USES_VULKANSC
	// Removed from Vulkan SC test set: VK_EXT_private_data extension does not exist in Vulkan SC
	const CaseDescriptions	s_privateDataResourcesGroup	=
	{
		EMPTY_CASE_DESC(Instance),		// Does not make sense
		EMPTY_CASE_DESC(Device),		// Device is tested in each object test
		EMPTY_CASE_DESC(DeviceGroup),	// Device is tested in each object test
		CASE_DESC(createPrivateDataTest	<DeviceMemory>,				s_deviceMemCases,			DE_NULL),
		CASE_DESC(createPrivateDataTest	<Buffer>,					s_bufferCases,				DE_NULL),
		CASE_DESC(createPrivateDataTest	<BufferView>,				s_bufferViewCases,			DE_NULL),
		CASE_DESC(createPrivateDataTest	<Image>,					s_imageCases,				DE_NULL),
		CASE_DESC(createPrivateDataTest	<ImageView>,				s_imageViewCases,			checkImageCubeArraySupport),
		CASE_DESC(createPrivateDataTest	<Semaphore>,				s_semaphoreCases,			DE_NULL),
		CASE_DESC(createPrivateDataTest	<Event>,					s_eventCases,				checkEventSupport),
		CASE_DESC(createPrivateDataTest	<Fence>,					s_fenceCases,				DE_NULL),
		CASE_DESC(createPrivateDataTest	<QueryPool>,				s_queryPoolCases,			DE_NULL),
		CASE_DESC(createPrivateDataTest	<ShaderModule>,				s_shaderModuleCases,		DE_NULL),
		CASE_DESC(createPrivateDataTest	<PipelineCache>,			s_pipelineCacheCases,		DE_NULL),
		CASE_DESC(createPrivateDataTest	<PipelineLayout>,			s_pipelineLayoutCases,		DE_NULL),
		CASE_DESC(createPrivateDataTest	<RenderPass>,				s_renderPassCases,			DE_NULL),
		CASE_DESC(createPrivateDataTest	<GraphicsPipeline>,			s_graphicsPipelineCases,	DE_NULL),
		CASE_DESC(createPrivateDataTest	<ComputePipeline>,			s_computePipelineCases,		DE_NULL),
		CASE_DESC(createPrivateDataTest	<DescriptorSetLayout>,		s_descriptorSetLayoutCases,	DE_NULL),
		CASE_DESC(createPrivateDataTest	<Sampler>,					s_samplerCases,				DE_NULL),
		CASE_DESC(createPrivateDataTest	<DescriptorPool>,			s_descriptorPoolCases,		DE_NULL),
		CASE_DESC(createPrivateDataTest	<DescriptorSet>,			s_descriptorSetCases,		DE_NULL),
		CASE_DESC(createPrivateDataTest	<Framebuffer>,				s_framebufferCases,			DE_NULL),
		CASE_DESC(createPrivateDataTest	<CommandPool>,				s_commandPoolCases,			DE_NULL),
		CASE_DESC(createPrivateDataTest	<CommandBuffer>,			s_commandBufferCases,		DE_NULL),
	};
	objectMgmtTests->addChild(createTestGroup(testCtx, "private_data", "Multiple objects with private data", createTests, s_privateDataResourcesGroup, cleanupGroup));
#endif // CTS_USES_VULKANSC

	return objectMgmtTests.release();
}

} // api
} // vkt
