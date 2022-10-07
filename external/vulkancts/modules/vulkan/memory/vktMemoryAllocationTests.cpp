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
 * \brief Simple memory allocation tests.
 *//*--------------------------------------------------------------------*/

#include "vktMemoryAllocationTests.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "tcuMaybe.hpp"
#include "tcuResultCollector.hpp"
#include "tcuTestLog.hpp"
#include "tcuPlatform.hpp"
#include "tcuCommandLine.hpp"

#include "vkPlatform.hpp"
#include "vkStrUtil.hpp"
#include "vkRef.hpp"
#include "vkDeviceUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkAllocationCallbackUtil.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deRandom.hpp"

using tcu::Maybe;
using tcu::TestLog;

using std::string;
using std::vector;

using namespace vk;

namespace vkt
{
namespace memory
{
namespace
{

template<typename T>
T roundUpToMultiple(const T& a, const T& b)
{
	return b * (a / b + (a % b != 0 ? 1 : 0));
}

enum
{
	// The min max for allocation count is 4096. Use 4000 to take into account
	// possible memory allocations made by layers etc.
	MAX_ALLOCATION_COUNT = 4000
};

enum AllocationMode
{
	ALLOCATION_MODE_DEFAULT,
	ALLOCATION_MODE_DEVICE_GROUP,
	ALLOCATION_MODE_PAGEABLE
};

struct TestConfig
{
	enum Order
	{
		ALLOC_FREE,
		ALLOC_REVERSE_FREE,
		MIXED_ALLOC_FREE,
		ORDER_LAST
	};

	Maybe<VkDeviceSize>	memorySize;
	Maybe<float>		memoryPercentage;
	deUint32			memoryAllocationCount;
	Order				order;
	AllocationMode		allocationMode;

	TestConfig (void)
		: memoryAllocationCount	((deUint32)-1)
		, order					(ORDER_LAST)
		, allocationMode		(ALLOCATION_MODE_DEFAULT)
	{
	}
};

struct TestConfigRandom
{
	const deUint32			seed;
	const AllocationMode	allocationMode;

	TestConfigRandom (const deUint32 _seed, const AllocationMode _allocationMode)
		: seed				(_seed)
		, allocationMode	(_allocationMode)
	{
	}
};

template<typename T>
T roundUpToNextMultiple (T value, T multiple)
{
	if (value % multiple == 0)
		return value;
	else
		return value + multiple - (value % multiple);
}

class BaseAllocateTestInstance : public TestInstance
{
public:
	BaseAllocateTestInstance		(Context& context, AllocationMode allocationMode)
		: TestInstance				(context)
		, m_allocationMode			(allocationMode)
		, m_subsetAllocationAllowed	(false)
		, m_numPhysDevices			(1)
		, m_memoryProperties		(getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()))
		, m_deviceCoherentMemSupported  (false)
	{
		if (m_allocationMode == ALLOCATION_MODE_DEVICE_GROUP)
			createDeviceGroup();
		else
			createTestDevice();

		m_allocFlagsInfo.sType		= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
		m_allocFlagsInfo.pNext		= DE_NULL;
		m_allocFlagsInfo.flags		= VK_MEMORY_ALLOCATE_DEVICE_MASK_BIT;
		m_allocFlagsInfo.deviceMask	= 0;
	}

	void						createTestDevice	(void);
	void						createDeviceGroup	(void);
	const vk::DeviceInterface&	getDeviceInterface	(void) { return (m_allocationMode == ALLOCATION_MODE_DEVICE_GROUP) ? *m_deviceDriver : m_context.getDeviceInterface(); }
	vk::VkDevice				getDevice			(void) { return m_logicalDevice.get();}

protected:
	AllocationMode							m_allocationMode;
	bool									m_subsetAllocationAllowed;
	VkMemoryAllocateFlagsInfo				m_allocFlagsInfo;
	deUint32								m_numPhysDevices;
	VkPhysicalDeviceMemoryProperties		m_memoryProperties;
	bool									m_deviceCoherentMemSupported;

private:
	CustomInstance					m_deviceGroupInstance;
	vk::Move<vk::VkDevice>			m_logicalDevice;
	de::MovePtr<vk::DeviceDriver>	m_deviceDriver;
};

void BaseAllocateTestInstance::createTestDevice (void)
{
	VkInstance										instance				(m_context.getInstance());
	InstanceDriver									instanceDriver			(m_context.getPlatformInterface(), instance);
	const VkPhysicalDeviceFeatures					deviceFeatures			= getPhysicalDeviceFeatures(instanceDriver, m_context.getPhysicalDevice());
	const float										queuePriority			= 1.0f;
	deUint32										queueFamilyIndex		= 0;
	bool											protMemSupported		= false;
	const bool										usePageable				= m_allocationMode == ALLOCATION_MODE_PAGEABLE;

	void* pNext = DE_NULL;

	if (usePageable && !m_context.isDeviceFunctionalitySupported("VK_EXT_pageable_device_local_memory"))
		TCU_THROW(NotSupportedError, "VK_EXT_pageable_device_local_memory is not supported");

	VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT pageableDeviceLocalMemoryFeature =
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT,	// VkStructureType					sType
		pNext,																		// const void*						pNext
		VK_FALSE,																		// VkBool32							pageableDeviceLocalMemory;
	};
	pNext = (usePageable) ? &pageableDeviceLocalMemoryFeature : DE_NULL;

	VkPhysicalDeviceProtectedMemoryFeatures protectedMemoryFeature =
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES,	// VkStructureType					sType
		pNext,					// const void*						pNext
		VK_FALSE														// VkBool32							protectedMemory;
	};
	pNext = &protectedMemoryFeature;

	VkPhysicalDeviceCoherentMemoryFeaturesAMD coherentMemoryFeatures =
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD, // VkStructureType                                      sType
		pNext,															// const void*                                          pNext
		VK_FALSE                                                        // VkBool32                                             deviceCoherentMemory;
	};
	if (m_context.isDeviceFunctionalitySupported("VK_AMD_device_coherent_memory")) pNext = &coherentMemoryFeatures;

	VkPhysicalDeviceFeatures				features;
	deMemset(&features, 0, sizeof(vk::VkPhysicalDeviceFeatures));

	VkPhysicalDeviceFeatures2				features2		=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,				// VkStructureType					sType
		pNext,														// const void*						pNext
		features													// VkPhysicalDeviceFeatures			features
	};

	// Check if the physical device supports the protected memory feature
	instanceDriver.getPhysicalDeviceFeatures2(m_context.getPhysicalDevice(), &features2);
	protMemSupported				= protectedMemoryFeature.protectedMemory;
	m_deviceCoherentMemSupported	= coherentMemoryFeatures.deviceCoherentMemory;

	VkDeviceQueueCreateFlags queueCreateFlags = protMemSupported ? (vk::VkDeviceQueueCreateFlags)vk::VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT : 0u;

	if (usePageable && !pageableDeviceLocalMemoryFeature.pageableDeviceLocalMemory)
		TCU_FAIL("pageableDeviceLocalMemory feature not supported but VK_EXT_pageable_device_local_memory advertised");

	pageableDeviceLocalMemoryFeature.pageableDeviceLocalMemory = usePageable;

	std::vector<const char*>						deviceExtensions;
	if (usePageable)
	{
		deviceExtensions.push_back("VK_EXT_memory_priority");
		deviceExtensions.push_back("VK_EXT_pageable_device_local_memory");
	}

	VkDeviceQueueCreateInfo							queueInfo		=
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,					// VkStructureType					sType;
		DE_NULL,													// const void*						pNext;
		queueCreateFlags,											// VkDeviceQueueCreateFlags			flags;
		queueFamilyIndex,											// deUint32							queueFamilyIndex;
		1u,															// deUint32							queueCount;
		&queuePriority												// const float*						pQueuePriorities;
	};

	const VkDeviceCreateInfo						deviceInfo		=
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,							// VkStructureType					sType;
		(protMemSupported || usePageable || m_deviceCoherentMemSupported) ? &features2 : DE_NULL,		// const void*						pNext;
		(VkDeviceCreateFlags)0,											// VkDeviceCreateFlags				flags;
		1u,																// uint32_t							queueCreateInfoCount;
		&queueInfo,														// const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
		0u,																// uint32_t							enabledLayerCount;
		DE_NULL,														// const char* const*				ppEnabledLayerNames;
		deUint32(deviceExtensions.size()),								// uint32_t							enabledExtensionCount;
		(deviceExtensions.empty()) ? DE_NULL : deviceExtensions.data(),	// const char* const*				ppEnabledExtensionNames;
		(protMemSupported || usePageable || m_deviceCoherentMemSupported) ? DE_NULL : &deviceFeatures	// const VkPhysicalDeviceFeatures*	pEnabledFeatures;
	};

	m_logicalDevice		= createCustomDevice(m_context.getTestContext().getCommandLine().isValidationEnabled(), m_context.getPlatformInterface(), instance, instanceDriver, m_context.getPhysicalDevice(), &deviceInfo);
}

void BaseAllocateTestInstance::createDeviceGroup (void)
{
	const tcu::CommandLine&							cmdLine					= m_context.getTestContext().getCommandLine();
	const deUint32									devGroupIdx				= cmdLine.getVKDeviceGroupId() - 1;
	const deUint32									physDeviceIdx			= cmdLine.getVKDeviceId() - 1;
	const float										queuePriority			= 1.0f;
	deUint32										queueFamilyIndex		= 0;
	const std::vector<std::string>					requiredExtensions		(1, "VK_KHR_device_group_creation");
	m_deviceGroupInstance													= createCustomInstanceWithExtensions(m_context, requiredExtensions);
	std::vector<VkPhysicalDeviceGroupProperties>	devGroupProperties		= enumeratePhysicalDeviceGroups(m_context.getInstanceInterface(), m_deviceGroupInstance);
	m_numPhysDevices														= devGroupProperties[devGroupIdx].physicalDeviceCount;
	m_subsetAllocationAllowed												= devGroupProperties[devGroupIdx].subsetAllocation;
	if (m_numPhysDevices < 2)
		TCU_THROW(NotSupportedError, "Device group allocation tests not supported with 1 physical device");
	std::vector<const char*>						deviceExtensions;

	if (!isCoreDeviceExtension(m_context.getUsedApiVersion(), "VK_KHR_device_group"))
		deviceExtensions.push_back("VK_KHR_device_group");

	VkDeviceGroupDeviceCreateInfo					deviceGroupInfo =
	{
		VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO_KHR,								//stype
		DE_NULL,																			//pNext
		devGroupProperties[devGroupIdx].physicalDeviceCount,								//physicalDeviceCount
		devGroupProperties[devGroupIdx].physicalDevices										//physicalDevices
	};
	VkInstance										instance				(m_deviceGroupInstance);
	InstanceDriver									instanceDriver			(m_context.getPlatformInterface(), instance);
	const VkPhysicalDeviceFeatures					deviceFeatures	=		getPhysicalDeviceFeatures(instanceDriver, deviceGroupInfo.pPhysicalDevices[physDeviceIdx]);

	const std::vector<VkQueueFamilyProperties>		queueProps		=		getPhysicalDeviceQueueFamilyProperties(instanceDriver, devGroupProperties[devGroupIdx].physicalDevices[physDeviceIdx]);
	for (size_t queueNdx = 0; queueNdx < queueProps.size(); queueNdx++)
	{
		if (queueProps[queueNdx].queueFlags & VK_QUEUE_COMPUTE_BIT)
			queueFamilyIndex = (deUint32)queueNdx;
	}

	VkDeviceQueueCreateInfo							queueInfo		=
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,					// VkStructureType					sType;
		DE_NULL,													// const void*						pNext;
		(VkDeviceQueueCreateFlags)0u,								// VkDeviceQueueCreateFlags			flags;
		queueFamilyIndex,											// deUint32							queueFamilyIndex;
		1u,															// deUint32							queueCount;
		&queuePriority												// const float*						pQueuePriorities;
	};

	const VkDeviceCreateInfo						deviceInfo		=
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,						// VkStructureType					sType;
		&deviceGroupInfo,											// const void*						pNext;
		(VkDeviceCreateFlags)0,										// VkDeviceCreateFlags				flags;
		1u	,														// uint32_t							queueCreateInfoCount;
		&queueInfo,													// const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
		0u,															// uint32_t							enabledLayerCount;
		DE_NULL,													// const char* const*				ppEnabledLayerNames;
		deUint32(deviceExtensions.size()),							// uint32_t							enabledExtensionCount;
		deviceExtensions.empty() ? DE_NULL : &deviceExtensions[0],	// const char* const*	ppEnabledExtensionNames;
		&deviceFeatures,											// const VkPhysicalDeviceFeatures*	pEnabledFeatures;
	};

	m_logicalDevice		= createCustomDevice(m_context.getTestContext().getCommandLine().isValidationEnabled(), m_context.getPlatformInterface(), instance, instanceDriver, deviceGroupInfo.pPhysicalDevices[physDeviceIdx], &deviceInfo);
	m_deviceDriver		= de::MovePtr<DeviceDriver>(new DeviceDriver(m_context.getPlatformInterface(), instance, *m_logicalDevice));
	m_memoryProperties	= getPhysicalDeviceMemoryProperties(instanceDriver, deviceGroupInfo.pPhysicalDevices[physDeviceIdx]);
}

class AllocateFreeTestInstance : public BaseAllocateTestInstance
{
public:
	AllocateFreeTestInstance	(Context& context, const TestConfig config)
		: BaseAllocateTestInstance		(context, config.allocationMode)
		, m_config						(config)
		, m_result						(m_context.getTestContext().getLog())
		, m_memoryTypeIndex				(0)
                , m_memoryLimits                (tcu::getMemoryLimits(context.getTestContext().getPlatform()))
	{
		DE_ASSERT(!!m_config.memorySize != !!m_config.memoryPercentage);
	}

	tcu::TestStatus		iterate							(void);

private:
	const TestConfig						m_config;
	tcu::ResultCollector					m_result;
	deUint32								m_memoryTypeIndex;
	const tcu::PlatformMemoryLimits			m_memoryLimits;
};


tcu::TestStatus AllocateFreeTestInstance::iterate (void)
{
	TestLog&								log					= m_context.getTestContext().getLog();
	const VkDevice							device				= getDevice();
	const DeviceInterface&					vkd					= getDeviceInterface();
	VkMemoryRequirements					memReqs;
	const deUint32							queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	VkBufferCreateFlags						createFlags			= (vk::VkBufferCreateFlagBits)0u;
	VkBufferUsageFlags						usageFlags			= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	VkSharingMode							sharingMode			= vk::VK_SHARING_MODE_EXCLUSIVE;
	Move<VkBuffer>							buffer;

	if ((m_memoryProperties.memoryTypes[m_memoryTypeIndex].propertyFlags & vk::VK_MEMORY_PROPERTY_PROTECTED_BIT) == vk::VK_MEMORY_PROPERTY_PROTECTED_BIT)
	{
		createFlags |= vk::VK_BUFFER_CREATE_PROTECTED_BIT;
	}

	DE_ASSERT(m_config.memoryAllocationCount <= MAX_ALLOCATION_COUNT);

	if (m_memoryTypeIndex == 0)
	{
		log << TestLog::Message << "Memory allocation count: " << m_config.memoryAllocationCount << TestLog::EndMessage;
		log << TestLog::Message << "Single allocation size: " << (m_config.memorySize ? de::toString(*m_config.memorySize) : de::toString(100.0f * (*m_config.memoryPercentage)) + " percent of the heap size.") << TestLog::EndMessage;

		if (m_config.order == TestConfig::ALLOC_REVERSE_FREE)
			log << TestLog::Message << "Memory is freed in reversed order. " << TestLog::EndMessage;
		else if (m_config.order == TestConfig::ALLOC_FREE)
			log << TestLog::Message << "Memory is freed in same order as allocated. " << TestLog::EndMessage;
		else if (m_config.order == TestConfig::MIXED_ALLOC_FREE)
			log << TestLog::Message << "Memory is freed right after allocation. " << TestLog::EndMessage;
		else
			DE_FATAL("Unknown allocation order");
	}

	bool memoryTypeSupported = true;
#ifndef CTS_USES_VULKANSC
	memoryTypeSupported = !((m_memoryProperties.memoryTypes[m_memoryTypeIndex].propertyFlags & vk::VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD) > 0 && !m_deviceCoherentMemSupported);
#endif

	if (memoryTypeSupported)
	{
		try
		{
			const VkMemoryType		memoryType = m_memoryProperties.memoryTypes[m_memoryTypeIndex];
			const VkMemoryHeap		memoryHeap = m_memoryProperties.memoryHeaps[memoryType.heapIndex];

			// Create a buffer to get the required size
			{
				const VkDeviceSize bufferSize = m_config.memorySize ? *m_config.memorySize : (VkDeviceSize)(*m_config.memoryPercentage * (float)memoryHeap.size);

				VkBufferCreateInfo bufferParams =
				{
					VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,                       // VkStructureType          sType;
					DE_NULL,                                                    // const void*              pNext;
					createFlags,                                                // VkBufferCreateFlags      flags;
					bufferSize,                                                 // VkDeviceSize             size;
					usageFlags,                                                 // VkBufferUsageFlags       usage;
					sharingMode,                                                // VkSharingMode            sharingMode;
					1u,                                                         // uint32_t                 queueFamilyIndexCount;
					&queueFamilyIndex,                                          // const uint32_t*          pQueueFamilyIndices;
				};

				buffer = createBuffer(vkd, device, &bufferParams);
				vkd.getBufferMemoryRequirements(device, *buffer, &memReqs);
			}

			const VkDeviceSize		allocationSize = (m_config.memorySize ? memReqs.size : (VkDeviceSize)(*m_config.memoryPercentage * (float)memoryHeap.size));
			const VkDeviceSize		roundedUpAllocationSize = roundUpToNextMultiple(allocationSize, m_memoryLimits.deviceMemoryAllocationGranularity);
			vector<VkDeviceMemory>	memoryObjects(m_config.memoryAllocationCount, (VkDeviceMemory)0);

			log << TestLog::Message << "Memory type index: " << m_memoryTypeIndex << TestLog::EndMessage;

			if (memoryType.heapIndex >= m_memoryProperties.memoryHeapCount)
				m_result.fail("Invalid heap index defined for memory type.");

			{
				log << TestLog::Message << "Memory type: " << memoryType << TestLog::EndMessage;
				log << TestLog::Message << "Memory heap: " << memoryHeap << TestLog::EndMessage;

				if (roundedUpAllocationSize * m_config.memoryAllocationCount > memoryHeap.size)
					TCU_THROW(NotSupportedError, "Memory heap doesn't have enough memory.");

#if (DE_PTR_SIZE == 4)
				// For 32-bit binaries we cap the total host visible allocations to 1.5GB to
				// avoid exhausting CPU virtual address space and throwing a false negative result.
				if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
					allocationSize * m_config.memoryAllocationCount * (m_subsetAllocationAllowed ? 1 : m_numPhysDevices) >= 1610612736)

					log << TestLog::Message << "    Skipping: Not enough CPU virtual address space for all host visible allocations." << TestLog::EndMessage;
				else
				{
#else
					{
#endif

						try
						{
							const deUint32 totalDeviceMaskCombinations = m_subsetAllocationAllowed ? (1 << m_numPhysDevices) - 1 : 1;
							for (deUint32 deviceMask = 1; deviceMask <= totalDeviceMaskCombinations; deviceMask++)
							{
								// Allocate on all physical devices if subset allocation is not allowed, do only once.
								if (!m_subsetAllocationAllowed)
									deviceMask = (1 << m_numPhysDevices) - 1;
								m_allocFlagsInfo.deviceMask = deviceMask;

								if (m_config.order == TestConfig::ALLOC_FREE || m_config.order == TestConfig::ALLOC_REVERSE_FREE)
								{
									for (size_t ndx = 0; ndx < m_config.memoryAllocationCount; ndx++)
									{
										VkMemoryAllocateInfo	alloc =
										{
											VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,				// sType
											(m_allocationMode == ALLOCATION_MODE_DEVICE_GROUP) ? &m_allocFlagsInfo : DE_NULL,	// pNext
											allocationSize,										// allocationSize
											m_memoryTypeIndex									// memoryTypeIndex;
										};

										VkResult				res = vkd.allocateMemory(device, &alloc, (const VkAllocationCallbacks*)DE_NULL, &memoryObjects[ndx]);

										// Some implementations might have limitations on protected heap, and these limitations
										// don't show up in Vulkan queries. Use a hard coded threshold after which out of memory
										// is allowed.
										if (res == VK_ERROR_OUT_OF_DEVICE_MEMORY && memoryType.propertyFlags & vk::VK_MEMORY_PROPERTY_PROTECTED_BIT && ndx > 80)
											break;

										VK_CHECK(res);

										TCU_CHECK(!!memoryObjects[ndx]);
									}

									if (m_config.order == TestConfig::ALLOC_FREE)
									{
										for (size_t ndx = 0; ndx < m_config.memoryAllocationCount; ndx++)
										{
											const VkDeviceMemory mem = memoryObjects[memoryObjects.size() - 1 - ndx];

											if (!!mem)
											{
												vkd.freeMemory(device, mem, (const VkAllocationCallbacks*)DE_NULL);
												memoryObjects[memoryObjects.size() - 1 - ndx] = (VkDeviceMemory)0;
											}
										}
									}
									else
									{
										for (size_t ndx = 0; ndx < m_config.memoryAllocationCount; ndx++)
										{
											const VkDeviceMemory mem = memoryObjects[ndx];

											if (!!mem)
											{
												vkd.freeMemory(device, mem, (const VkAllocationCallbacks*)DE_NULL);
												memoryObjects[ndx] = (VkDeviceMemory)0;
											}
										}
									}
								}
								else
								{
									for (size_t ndx = 0; ndx < m_config.memoryAllocationCount; ndx++)
									{
										const VkMemoryAllocateInfo alloc =
										{
											VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,				// sType
											(m_allocationMode == ALLOCATION_MODE_DEVICE_GROUP) ? &m_allocFlagsInfo : DE_NULL,	// pNext
											allocationSize,										// allocationSize
											m_memoryTypeIndex									// memoryTypeIndex;
										};

										VK_CHECK(vkd.allocateMemory(device, &alloc, (const VkAllocationCallbacks*)DE_NULL, &memoryObjects[ndx]));
										TCU_CHECK(!!memoryObjects[ndx]);

										vkd.freeMemory(device, memoryObjects[ndx], (const VkAllocationCallbacks*)DE_NULL);
										memoryObjects[ndx] = (VkDeviceMemory)0;
									}
								}
							}
						}
						catch (...)
						{
							for (size_t ndx = 0; ndx < m_config.memoryAllocationCount; ndx++)
							{
								const VkDeviceMemory mem = memoryObjects[ndx];

								if (!!mem)
								{
									vkd.freeMemory(device, mem, (const VkAllocationCallbacks*)DE_NULL);
									memoryObjects[ndx] = (VkDeviceMemory)0;
								}
							}

							throw;
						}
					}
				}
			}
		catch (const tcu::TestError& error)
		{
			m_result.fail(error.getMessage());
		}
	}

	m_memoryTypeIndex++;

	if (m_memoryTypeIndex < m_memoryProperties.memoryTypeCount)
		return tcu::TestStatus::incomplete();
	else
		return tcu::TestStatus(m_result.getResult(), m_result.getMessage());
}

size_t computeDeviceMemorySystemMemFootprint (const DeviceInterface& vk, VkDevice device)
{
	AllocationCallbackRecorder	callbackRecorder	(getSystemAllocator());

	{
		// 1 B allocation from memory type 0
		const VkMemoryAllocateInfo	allocInfo	=
		{
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			DE_NULL,
			1u,
			0u,
		};
		const Unique<VkDeviceMemory>			memory			(allocateMemory(vk, device, &allocInfo, callbackRecorder.getCallbacks()));
		AllocationCallbackValidationResults		validateRes;

		validateAllocationCallbacks(callbackRecorder, &validateRes);

		TCU_CHECK(validateRes.violations.empty());

		return getLiveSystemAllocationTotal(validateRes)
			   + sizeof(void*)*validateRes.liveAllocations.size(); // allocation overhead
	}
}

struct MemoryType
{
	deUint32		index;
	VkMemoryType	type;
};

struct MemoryObject
{
	VkDeviceMemory			memory;
	VkDeviceSize			size;
	VkMemoryPropertyFlags	propertyFlags;
};

struct Heap
{
	VkMemoryHeap			heap;
	VkDeviceSize			memoryUsage;
	VkDeviceSize			maxMemoryUsage;
	vector<MemoryType>		types;
	vector<MemoryObject>	objects;
};

class RandomAllocFreeTestInstance : public BaseAllocateTestInstance
{
public:
								RandomAllocFreeTestInstance		(Context& context, TestConfigRandom config);
								~RandomAllocFreeTestInstance	(void);

	tcu::TestStatus				iterate							(void);

private:
	const size_t					m_opCount;
	const size_t					m_allocSysMemSize;
	const tcu::PlatformMemoryLimits	m_memoryLimits;
	const deUint32					m_totalDeviceMaskCombinations;

	deUint32						m_memoryObjectCount;
	deUint32						m_memoryProtectedObjectCount;
	deUint32						m_currentDeviceMask;
	size_t							m_opNdx;
	de::Random						m_rng;
	vector<Heap>					m_heaps;
	VkDeviceSize					m_totalSystemMem;
	VkDeviceSize					m_totalDeviceMem;
};

RandomAllocFreeTestInstance::RandomAllocFreeTestInstance (Context& context, TestConfigRandom config)
	: BaseAllocateTestInstance	(context, config.allocationMode)
	, m_opCount						(128)
	, m_allocSysMemSize				(computeDeviceMemorySystemMemFootprint(getDeviceInterface(), context.getDevice())
									 + sizeof(MemoryObject))
	, m_memoryLimits				(tcu::getMemoryLimits(context.getTestContext().getPlatform()))
	, m_totalDeviceMaskCombinations	(m_subsetAllocationAllowed ? (1 << m_numPhysDevices) - 1 : 1)
	, m_memoryObjectCount			(0)
	, m_memoryProtectedObjectCount	(0)
	, m_currentDeviceMask			(m_subsetAllocationAllowed ? 1 : (1 << m_numPhysDevices) - 1)
	, m_opNdx						(0)
	, m_rng							(config.seed)
	, m_totalSystemMem				(0)
	, m_totalDeviceMem				(0)
{
	TCU_CHECK(m_memoryProperties.memoryHeapCount <= 32);
	TCU_CHECK(m_memoryProperties.memoryTypeCount <= 32);

	m_heaps.resize(m_memoryProperties.memoryHeapCount);

	for (deUint32 heapNdx = 0; heapNdx < m_memoryProperties.memoryHeapCount; heapNdx++)
	{
		m_heaps[heapNdx].heap			= m_memoryProperties.memoryHeaps[heapNdx];
		m_heaps[heapNdx].memoryUsage	= 0;
		m_heaps[heapNdx].maxMemoryUsage	= m_heaps[heapNdx].heap.size / 8; /* Use at maximum 12.5% of heap */

		m_heaps[heapNdx].objects.reserve(100);
	}

	for (deUint32 memoryTypeNdx = 0; memoryTypeNdx < m_memoryProperties.memoryTypeCount; memoryTypeNdx++)
	{
		const MemoryType type =
		{
			memoryTypeNdx,
			m_memoryProperties.memoryTypes[memoryTypeNdx]
		};

		TCU_CHECK(type.type.heapIndex < m_memoryProperties.memoryHeapCount);

		if ((m_memoryProperties.memoryTypes[type.index].propertyFlags & vk::VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD) > 0 && !m_deviceCoherentMemSupported)
		{
			continue;
		}


		m_heaps[type.type.heapIndex].types.push_back(type);
	}
}

RandomAllocFreeTestInstance::~RandomAllocFreeTestInstance (void)
{
	const VkDevice							device				= getDevice();
	const DeviceInterface&					vkd					= getDeviceInterface();

	for (deUint32 heapNdx = 0; heapNdx < (deUint32)m_heaps.size(); heapNdx++)
	{
		const Heap&	heap	= m_heaps[heapNdx];

		for (size_t objectNdx = 0; objectNdx < heap.objects.size(); objectNdx++)
		{
			if (!!heap.objects[objectNdx].memory)
				vkd.freeMemory(device, heap.objects[objectNdx].memory, (const VkAllocationCallbacks*)DE_NULL);
		}
	}
}

tcu::TestStatus RandomAllocFreeTestInstance::iterate (void)
{
	const VkDevice			device			= getDevice();
	const DeviceInterface&	vkd				= getDeviceInterface();
	TestLog&				log				= m_context.getTestContext().getLog();
	const bool				isUMA			= m_memoryLimits.totalDeviceLocalMemory == 0;
	const VkDeviceSize		usedSysMem		= isUMA ? (m_totalDeviceMem+m_totalSystemMem) : m_totalSystemMem;
	const bool				canAllocateSys	= usedSysMem + m_allocSysMemSize + 1024 < m_memoryLimits.totalSystemMemory; // \note Always leave room for 1 KiB sys mem alloc
	const bool				canAllocateDev	= isUMA ? canAllocateSys : (m_totalDeviceMem + 16 < m_memoryLimits.totalDeviceLocalMemory);
	vector<size_t>			nonFullHeaps;
	vector<size_t>			nonEmptyHeaps;
	bool					allocateMore;

	if (m_opNdx == 0)
	{
		log << TestLog::Message << "Performing " << m_opCount << " random VkAllocMemory() / VkFreeMemory() calls before freeing all memory." << TestLog::EndMessage;
		log << TestLog::Message << "Using max 1/8 of the memory in each memory heap." << TestLog::EndMessage;
	}

	// Sort heaps based on whether allocations or frees are possible
	for (size_t heapNdx = 0; heapNdx < m_heaps.size(); ++heapNdx)
	{
		const bool	isDeviceLocal	= (m_heaps[heapNdx].heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0;
		const bool	isHeapFull		= m_heaps[heapNdx].memoryUsage >= m_heaps[heapNdx].maxMemoryUsage;
		const bool	isHeapEmpty		= m_heaps[heapNdx].memoryUsage == 0;

		if (!isHeapEmpty)
			nonEmptyHeaps.push_back(heapNdx);

		if (!isHeapFull && ((isUMA && canAllocateSys) ||
							(!isUMA && isDeviceLocal && canAllocateDev) ||
							(!isUMA && !isDeviceLocal && canAllocateSys)))
			nonFullHeaps.push_back(heapNdx);
	}

	if (m_opNdx >= m_opCount)
	{
		if (nonEmptyHeaps.empty())
		{
			m_currentDeviceMask++;
			if (m_currentDeviceMask > m_totalDeviceMaskCombinations)
				return tcu::TestStatus::pass("Pass");
			else
			{
				m_opNdx = 0;
				return tcu::TestStatus::incomplete();
			}
		}
		else
			allocateMore = false;
	}
	else if (!nonEmptyHeaps.empty() &&
			 !nonFullHeaps.empty() &&
			 (m_memoryObjectCount < MAX_ALLOCATION_COUNT) &&
			 canAllocateSys)
		allocateMore = m_rng.getBool(); // Randomize if both operations are doable.
	else if (nonEmptyHeaps.empty())
	{
		DE_ASSERT(canAllocateSys);
		allocateMore = true; // Allocate more if there are no objects to free.
	}
	else if (nonFullHeaps.empty() || !canAllocateSys)
		allocateMore = false; // Free objects if there is no free space for new objects.
	else
	{
		allocateMore = false;
		DE_FATAL("Fail");
	}

	if (allocateMore)
	{
		const size_t		nonFullHeapNdx	= (size_t)(m_rng.getUint32() % (deUint32)nonFullHeaps.size());
		const size_t		heapNdx			= nonFullHeaps[nonFullHeapNdx];
		Heap&				heap			= m_heaps[heapNdx];
		const MemoryType&	memoryType		= m_rng.choose<MemoryType>(heap.types.begin(), heap.types.end());
		const bool			isDeviceLocal	= (heap.heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0;
		const bool			isProtected		= memoryType.type.propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT;
		VkDeviceSize		maxAllocSize	= (isDeviceLocal && !isUMA)
											? de::min(heap.maxMemoryUsage - heap.memoryUsage, (VkDeviceSize)m_memoryLimits.totalDeviceLocalMemory - m_totalDeviceMem)
											: de::min(heap.maxMemoryUsage - heap.memoryUsage, (VkDeviceSize)m_memoryLimits.totalSystemMemory - usedSysMem - m_allocSysMemSize);
		const VkDeviceSize	maxProtectedAllocSize = 1 * 1024 * 1024;

		// Some implementations might have limitations on protected heap, and these
		// limitations don't show up in Vulkan queries. Use a hard coded limit for
		// allocations of arbitrarily selected size of 1MB as per Note at "Device
		// Memory Allocation" at the spec to use minimum-size allocations.
		if(isProtected)
			maxAllocSize = (maxAllocSize > maxProtectedAllocSize) ? maxProtectedAllocSize : maxAllocSize;

		const VkDeviceSize allocationSize = 1 + (m_rng.getUint64() % maxAllocSize);

		if ((allocationSize > (deUint64)(heap.maxMemoryUsage - heap.memoryUsage)) && (allocationSize != 1))
			TCU_THROW(InternalError, "Test Error: trying to allocate memory more than the available heap size.");

		const MemoryObject object =
		{
			(VkDeviceMemory)0,
			allocationSize,
			memoryType.type.propertyFlags
		};

		heap.objects.push_back(object);

		m_allocFlagsInfo.deviceMask = m_currentDeviceMask;
		const VkMemoryAllocateInfo alloc =
		{
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,												// sType
			(m_allocationMode == ALLOCATION_MODE_DEVICE_GROUP) ? &m_allocFlagsInfo : DE_NULL,	// pNext
			object.size,																		// allocationSize
			memoryType.index																	// memoryTypeIndex;
		};

		VkResult	res	= vkd.allocateMemory(device, &alloc, (const VkAllocationCallbacks*)DE_NULL, &heap.objects.back().memory);

		// Some implementations might have limitations on protected heap, and these
		// limitations don't show up in Vulkan queries. Use a hard coded threshold
		// after which out of memory is allowed as per Note at "Device Memory Allocation"
		// at the spec to support at least 80 allocations concurrently.
		if (res == VK_ERROR_OUT_OF_DEVICE_MEMORY && isProtected && m_memoryProtectedObjectCount > 80)
		{
			heap.objects.pop_back();
		}
		else
		{
			VK_CHECK(res);

			TCU_CHECK(!!heap.objects.back().memory);
			m_memoryObjectCount++;

			if (isProtected)
				m_memoryProtectedObjectCount++;

			heap.memoryUsage										+= allocationSize;
			(isDeviceLocal ? m_totalDeviceMem : m_totalSystemMem)	+= allocationSize;
			m_totalSystemMem										+= m_allocSysMemSize;
		}
	}
	else
	{
		const size_t		nonEmptyHeapNdx	= (size_t)(m_rng.getUint32() % (deUint32)nonEmptyHeaps.size());
		const size_t		heapNdx			= nonEmptyHeaps[nonEmptyHeapNdx];
		Heap&				heap			= m_heaps[heapNdx];
		const size_t		memoryObjectNdx	= m_rng.getUint32() % heap.objects.size();
		MemoryObject&		memoryObject	= heap.objects[memoryObjectNdx];
		const bool			isDeviceLocal	= (heap.heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0;

		vkd.freeMemory(device, memoryObject.memory, (const VkAllocationCallbacks*)DE_NULL);

		memoryObject.memory = (VkDeviceMemory)0;
		m_memoryObjectCount--;

		if (memoryObject.propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT)
		{
			m_memoryProtectedObjectCount--;
			memoryObject.propertyFlags = (VkMemoryPropertyFlags)0;
		}

		heap.memoryUsage										-= memoryObject.size;
		(isDeviceLocal ? m_totalDeviceMem : m_totalSystemMem)	-= memoryObject.size;
		m_totalSystemMem										-= m_allocSysMemSize;

		heap.objects[memoryObjectNdx] = heap.objects.back();
		heap.objects.pop_back();

		DE_ASSERT(heap.memoryUsage == 0 || !heap.objects.empty());
	}

	m_opNdx++;
	return tcu::TestStatus::incomplete();
}


} // anonymous

tcu::TestCaseGroup* createAllocationTestsCommon (tcu::TestContext& testCtx, AllocationMode allocationMode)
{
	const char* name = [&]{
		switch (allocationMode)
		{
			case ALLOCATION_MODE_DEFAULT:
				return "allocation";
			case ALLOCATION_MODE_DEVICE_GROUP:
				return "device_group_allocation";
			case ALLOCATION_MODE_PAGEABLE:
				return "pageable_allocation";
			default:
				TCU_THROW(InternalError, "Unknown allocation mode");
		}
	} ();
	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, name, "Memory allocation tests."));

	const VkDeviceSize	KiB	= 1024;
	const VkDeviceSize	MiB	= 1024 * KiB;

	const struct
	{
		const char* const	str;
		VkDeviceSize		size;
	} allocationSizes[] =
	{
		{   "64", 64 },
		{  "128", 128 },
		{  "256", 256 },
		{  "512", 512 },
		{ "1KiB", 1*KiB },
		{ "4KiB", 4*KiB },
		{ "8KiB", 8*KiB },
		{ "1MiB", 1*MiB }
	};

	const int allocationPercents[] =
	{
		1
	};

	const int allocationCounts[] =
	{
		1, 10, 100, 1000, -1
	};

	const struct
	{
		const char* const		str;
		const TestConfig::Order	order;
	} orders[] =
	{
		{ "forward",	TestConfig::ALLOC_FREE },
		{ "reverse",	TestConfig::ALLOC_REVERSE_FREE },
		{ "mixed",		TestConfig::MIXED_ALLOC_FREE }
	};

	{
		de::MovePtr<tcu::TestCaseGroup>	basicGroup(new tcu::TestCaseGroup(testCtx, "basic", "Basic memory allocation and free tests"));

		for (size_t allocationSizeNdx = 0; allocationSizeNdx < DE_LENGTH_OF_ARRAY(allocationSizes); allocationSizeNdx++)
		{
			const VkDeviceSize				allocationSize		= allocationSizes[allocationSizeNdx].size;
			const char* const				allocationSizeName	= allocationSizes[allocationSizeNdx].str;
			de::MovePtr<tcu::TestCaseGroup>	sizeGroup			(new tcu::TestCaseGroup(testCtx, ("size_" + string(allocationSizeName)).c_str(), ("Test different allocation sizes " + de::toString(allocationSize)).c_str()));

			for (size_t orderNdx = 0; orderNdx < DE_LENGTH_OF_ARRAY(orders); orderNdx++)
			{
				const TestConfig::Order			order				= orders[orderNdx].order;
				const char* const				orderName			= orders[orderNdx].str;
				const char* const				orderDescription	= orderName;
				de::MovePtr<tcu::TestCaseGroup>	orderGroup			(new tcu::TestCaseGroup(testCtx, orderName, orderDescription));

				for (size_t allocationCountNdx = 0; allocationCountNdx < DE_LENGTH_OF_ARRAY(allocationCounts); allocationCountNdx++)
				{
					const int allocationCount = allocationCounts[allocationCountNdx];

					if (allocationCount != -1 && allocationCount * allocationSize > 50 * MiB)
						continue;

					TestConfig config;

					config.memorySize				= allocationSize;
					config.order					= order;
					config.allocationMode			= allocationMode;
					if (allocationCount == -1)
					{
						if (allocationSize < 4096)
							continue;

						config.memoryAllocationCount	= de::min((deUint32)(50 * MiB / allocationSize), (deUint32)MAX_ALLOCATION_COUNT);

						if (config.memoryAllocationCount == 0
							|| config.memoryAllocationCount == 1
							|| config.memoryAllocationCount == 10
							|| config.memoryAllocationCount == 100
							|| config.memoryAllocationCount == 1000)
						continue;
					}
					else
						config.memoryAllocationCount	= allocationCount;

					orderGroup->addChild(new InstanceFactory1<AllocateFreeTestInstance, TestConfig>(testCtx, tcu::NODETYPE_SELF_VALIDATE, "count_" + de::toString(config.memoryAllocationCount), "", config));
				}

				sizeGroup->addChild(orderGroup.release());
			}

			basicGroup->addChild(sizeGroup.release());
		}

		for (size_t allocationPercentNdx = 0; allocationPercentNdx < DE_LENGTH_OF_ARRAY(allocationPercents); allocationPercentNdx++)
		{
			const int						allocationPercent	= allocationPercents[allocationPercentNdx];
			de::MovePtr<tcu::TestCaseGroup>	percentGroup		(new tcu::TestCaseGroup(testCtx, ("percent_" + de::toString(allocationPercent)).c_str(), ("Test different allocation percents " + de::toString(allocationPercent)).c_str()));

			for (size_t orderNdx = 0; orderNdx < DE_LENGTH_OF_ARRAY(orders); orderNdx++)
			{
				const TestConfig::Order			order				= orders[orderNdx].order;
				const char* const				orderName			= orders[orderNdx].str;
				const char* const				orderDescription	= orderName;
				de::MovePtr<tcu::TestCaseGroup>	orderGroup			(new tcu::TestCaseGroup(testCtx, orderName, orderDescription));

				for (size_t allocationCountNdx = 0; allocationCountNdx < DE_LENGTH_OF_ARRAY(allocationCounts); allocationCountNdx++)
				{
					const int allocationCount = allocationCounts[allocationCountNdx];

					if ((allocationCount != -1) && ((float)allocationCount * (float)allocationPercent >= 1.00f / 8.00f))
						continue;

					TestConfig config;

					config.memoryPercentage			= (float)allocationPercent / 100.0f;
					config.order					= order;
					config.allocationMode			= allocationMode;

					if (allocationCount == -1)
					{
						config.memoryAllocationCount	= de::min((deUint32)((1.00f / 8.00f) / ((float)allocationPercent / 100.0f)), (deUint32)MAX_ALLOCATION_COUNT);

						if (config.memoryAllocationCount == 0
							|| config.memoryAllocationCount == 1
							|| config.memoryAllocationCount == 10
							|| config.memoryAllocationCount == 100
							|| config.memoryAllocationCount == 1000)
						continue;
					}
					else
						config.memoryAllocationCount	= allocationCount;

					orderGroup->addChild(new InstanceFactory1<AllocateFreeTestInstance, TestConfig>(testCtx, tcu::NODETYPE_SELF_VALIDATE, "count_" + de::toString(config.memoryAllocationCount), "", config));
				}

				percentGroup->addChild(orderGroup.release());
			}

			basicGroup->addChild(percentGroup.release());
		}

		group->addChild(basicGroup.release());
	}

	{
		const deUint32					caseCount	= 100;
		de::MovePtr<tcu::TestCaseGroup>	randomGroup	(new tcu::TestCaseGroup(testCtx, "random", "Random memory allocation tests."));

		for (deUint32 caseNdx = 0; caseNdx < caseCount; caseNdx++)
		{
			TestConfigRandom config(deInt32Hash(caseNdx ^ 32480), allocationMode);

			randomGroup->addChild(new InstanceFactory1<RandomAllocFreeTestInstance, TestConfigRandom>(testCtx, tcu::NODETYPE_SELF_VALIDATE, de::toString(caseNdx), "Random case", config));
		}

		group->addChild(randomGroup.release());
	}

	return group.release();
}

tcu::TestCaseGroup* createAllocationTests (tcu::TestContext& testCtx)
{
	return createAllocationTestsCommon(testCtx, ALLOCATION_MODE_DEFAULT);
}

tcu::TestCaseGroup* createDeviceGroupAllocationTests (tcu::TestContext& testCtx)
{
	return createAllocationTestsCommon(testCtx, ALLOCATION_MODE_DEVICE_GROUP);
}

tcu::TestCaseGroup* createPageableAllocationTests (tcu::TestContext& testCtx)
{
	return createAllocationTestsCommon(testCtx, ALLOCATION_MODE_PAGEABLE);
}

} // memory
} // vkt
