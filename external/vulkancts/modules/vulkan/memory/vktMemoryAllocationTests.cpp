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

#include "tcuMaybe.hpp"
#include "tcuResultCollector.hpp"
#include "tcuTestLog.hpp"

#include "vkPlatform.hpp"
#include "vkStrUtil.hpp"
#include "vkRef.hpp"
#include "vkDeviceUtil.hpp"
#include "vkQueryUtil.hpp"

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
enum
{
	// The min max for allocation count is 4096. Use 4000 to take into account
	// possible memory allocations made by layers etc.
	MAX_ALLOCATION_COUNT = 4000
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

	TestConfig (void)
		: memoryAllocationCount	((deUint32)-1)
		, order					(ORDER_LAST)
	{
	}
};

class AllocateFreeTestInstance : public TestInstance
{
public:
						AllocateFreeTestInstance		(Context& context, const TestConfig config)
		: TestInstance			(context)
		, m_config				(config)
		, m_result				(m_context.getTestContext().getLog())
		, m_memoryTypeIndex		(0)
		, m_memoryProperties	(getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()))
	{
		DE_ASSERT(!!m_config.memorySize != !!m_config.memoryPercentage);
	}

	tcu::TestStatus		iterate							(void);

private:
	const TestConfig						m_config;
	tcu::ResultCollector					m_result;
	deUint32								m_memoryTypeIndex;
	const VkPhysicalDeviceMemoryProperties	m_memoryProperties;
};

tcu::TestStatus AllocateFreeTestInstance::iterate (void)
{
	TestLog&								log					= m_context.getTestContext().getLog();
	const VkDevice							device				= m_context.getDevice();
	const DeviceInterface&					vkd					= m_context.getDeviceInterface();

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

	try
	{
		const VkMemoryType		memoryType		= m_memoryProperties.memoryTypes[m_memoryTypeIndex];
		const VkMemoryHeap		memoryHeap		= m_memoryProperties.memoryHeaps[memoryType.heapIndex];

		const VkDeviceSize		allocationSize	= (m_config.memorySize ? *m_config.memorySize : (VkDeviceSize)(*m_config.memoryPercentage * (float)memoryHeap.size));
		vector<VkDeviceMemory>	memoryObjects	(m_config.memoryAllocationCount, (VkDeviceMemory)0);

		log << TestLog::Message << "Memory type index: " << m_memoryTypeIndex << TestLog::EndMessage;

		if (memoryType.heapIndex >= m_memoryProperties.memoryHeapCount)
			m_result.fail("Invalid heap index defined for memory type.");

		{
			log << TestLog::Message << "Memory type: " << memoryType << TestLog::EndMessage;
			log << TestLog::Message << "Memory heap: " << memoryHeap << TestLog::EndMessage;

			if (allocationSize * m_config.memoryAllocationCount * 8 > memoryHeap.size)
				TCU_THROW(NotSupportedError, "Memory heap doesn't have enough memory.");

			try
			{
				if (m_config.order == TestConfig::ALLOC_FREE || m_config.order == TestConfig::ALLOC_REVERSE_FREE)
				{
					for (size_t ndx = 0; ndx < m_config.memoryAllocationCount; ndx++)
					{
						const VkMemoryAllocateInfo alloc =
						{
							VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,	// sType
							DE_NULL,								// pNext
							allocationSize,							// allocationSize
							m_memoryTypeIndex						// memoryTypeIndex;
						};

						VK_CHECK(vkd.allocateMemory(device, &alloc, (const VkAllocationCallbacks*)DE_NULL, &memoryObjects[ndx]));

						TCU_CHECK(!!memoryObjects[ndx]);
					}

					if (m_config.order == TestConfig::ALLOC_FREE)
					{
						for (size_t ndx = 0; ndx < m_config.memoryAllocationCount; ndx++)
						{
							const VkDeviceMemory mem = memoryObjects[memoryObjects.size() - 1 - ndx];

							vkd.freeMemory(device, mem, (const VkAllocationCallbacks*)DE_NULL);
							memoryObjects[memoryObjects.size() - 1 - ndx] = (VkDeviceMemory)0;
						}
					}
					else
					{
						for (size_t ndx = 0; ndx < m_config.memoryAllocationCount; ndx++)
						{
							const VkDeviceMemory mem = memoryObjects[ndx];

							vkd.freeMemory(device, mem, (const VkAllocationCallbacks*)DE_NULL);
							memoryObjects[ndx] = (VkDeviceMemory)0;
						}
					}
				}
				else
				{
					for (size_t ndx = 0; ndx < m_config.memoryAllocationCount; ndx++)
					{
						const VkMemoryAllocateInfo alloc =
						{
							VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,	// sType
							DE_NULL,								// pNext
							allocationSize,							// allocationSize
							m_memoryTypeIndex						// memoryTypeIndex;
						};

						VK_CHECK(vkd.allocateMemory(device, &alloc, (const VkAllocationCallbacks*)DE_NULL, &memoryObjects[ndx]));
						TCU_CHECK(!!memoryObjects[ndx]);

						vkd.freeMemory(device, memoryObjects[ndx], (const VkAllocationCallbacks*)DE_NULL);
						memoryObjects[ndx] = (VkDeviceMemory)0;
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
	catch (const tcu::TestError& error)
	{
		m_result.fail(error.getMessage());
	}

	m_memoryTypeIndex++;

	if (m_memoryTypeIndex < m_memoryProperties.memoryTypeCount)
		return tcu::TestStatus::incomplete();
	else
		return tcu::TestStatus(m_result.getResult(), m_result.getMessage());
}

struct MemoryType
{
	deUint32		index;
	VkMemoryType	type;
};

struct MemoryObject
{
	VkDeviceMemory	memory;
	VkDeviceSize	size;
};

struct Heap
{
	VkMemoryHeap			heap;
	VkDeviceSize			memoryUsage;
	VkDeviceSize			maxMemoryUsage;
	vector<MemoryType>		types;
	vector<MemoryObject>	objects;
};

class RandomAllocFreeTestInstance : public TestInstance
{
public:
						RandomAllocFreeTestInstance		(Context& context, deUint32 seed);
						~RandomAllocFreeTestInstance	(void);

	tcu::TestStatus		iterate							(void);

private:
	const size_t		m_opCount;
	deUint32			m_memoryObjectCount;
	size_t				m_opNdx;
	de::Random			m_rng;
	vector<Heap>		m_heaps;
	vector<size_t>		m_nonFullHeaps;
	vector<size_t>		m_nonEmptyHeaps;
};

RandomAllocFreeTestInstance::RandomAllocFreeTestInstance	(Context& context, deUint32 seed)
	: TestInstance			(context)
	, m_opCount				(128)
	, m_memoryObjectCount	(0)
	, m_opNdx				(0)
	, m_rng					(seed)
{
	const VkPhysicalDevice					physicalDevice		= context.getPhysicalDevice();
	const InstanceInterface&				vki					= context.getInstanceInterface();
	const VkPhysicalDeviceMemoryProperties	memoryProperties	= getPhysicalDeviceMemoryProperties(vki, physicalDevice);

	TCU_CHECK(memoryProperties.memoryHeapCount <= 32);
	TCU_CHECK(memoryProperties.memoryTypeCount <= 32);

	m_heaps.resize(memoryProperties.memoryHeapCount);

	m_nonFullHeaps.reserve(m_heaps.size());
	m_nonEmptyHeaps.reserve(m_heaps.size());

	for (deUint32 heapNdx = 0; heapNdx < memoryProperties.memoryHeapCount; heapNdx++)
	{
		m_heaps[heapNdx].heap			= memoryProperties.memoryHeaps[heapNdx];
		m_heaps[heapNdx].memoryUsage	= 0;
		m_heaps[heapNdx].maxMemoryUsage	= m_heaps[heapNdx].heap.size / 8;

		m_heaps[heapNdx].objects.reserve(100);

		m_nonFullHeaps.push_back((size_t)heapNdx);
	}

	for (deUint32 memoryTypeNdx = 0; memoryTypeNdx < memoryProperties.memoryTypeCount; memoryTypeNdx++)
	{
		const MemoryType type =
		{
			memoryTypeNdx,
			memoryProperties.memoryTypes[memoryTypeNdx]
		};

		TCU_CHECK(type.type.heapIndex < memoryProperties.memoryHeapCount);

		m_heaps[type.type.heapIndex].types.push_back(type);
	}
}

RandomAllocFreeTestInstance::~RandomAllocFreeTestInstance (void)
{
	const VkDevice							device				= m_context.getDevice();
	const DeviceInterface&					vkd					= m_context.getDeviceInterface();

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
	const VkDevice			device			= m_context.getDevice();
	const DeviceInterface&	vkd				= m_context.getDeviceInterface();
	TestLog&				log				= m_context.getTestContext().getLog();
	bool					allocateMore;

	if (m_opNdx == 0)
	{
		log << TestLog::Message << "Performing " << m_opCount << " random VkAllocMemory() / VkFreeMemory() calls before freeing all memory." << TestLog::EndMessage;
		log << TestLog::Message << "Using max 1/8 of the memory in each memory heap." << TestLog::EndMessage;
	}

	if (m_opNdx >= m_opCount)
	{
		if (m_nonEmptyHeaps.empty())
			return tcu::TestStatus::pass("Pass");
		else
			allocateMore = false;
	}
	else if (!m_nonEmptyHeaps.empty() && !m_nonFullHeaps.empty() && (m_memoryObjectCount < MAX_ALLOCATION_COUNT))
		allocateMore = m_rng.getBool(); // Randomize if both operations are doable.
	else if (m_nonEmptyHeaps.empty())
		allocateMore = true; // Allocate more if there are no objects to free.
	else if (m_nonFullHeaps.empty())
		allocateMore = false; // Free objects if there is no free space for new objects.
	else
	{
		allocateMore = false;
		DE_FATAL("Fail");
	}

	if (allocateMore)
	{
		const size_t		nonFullHeapNdx	= (size_t)(m_rng.getUint32() % (deUint32)m_nonFullHeaps.size());
		const size_t		heapNdx			= m_nonFullHeaps[nonFullHeapNdx];
		Heap&				heap			= m_heaps[heapNdx];
		const MemoryType&	memoryType		= m_rng.choose<MemoryType>(heap.types.begin(), heap.types.end());
		const VkDeviceSize	allocationSize	= 1 + (m_rng.getUint64() % (deUint64)(heap.maxMemoryUsage - heap.memoryUsage));


		if ((allocationSize > (deUint64)(heap.maxMemoryUsage - heap.memoryUsage)) && (allocationSize != 1))
			TCU_THROW(InternalError, "Test Error: trying to allocate memory more than the available heap size.");


		const MemoryObject object =
		{
			(VkDeviceMemory)0,
			allocationSize
		};

		heap.objects.push_back(object);

		const VkMemoryAllocateInfo alloc =
		{
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,	// sType
			DE_NULL,								// pNext
			object.size,							// allocationSize
			memoryType.index						// memoryTypeIndex;
		};

		VK_CHECK(vkd.allocateMemory(device, &alloc, (const VkAllocationCallbacks*)DE_NULL, &heap.objects.back().memory));
		TCU_CHECK(!!heap.objects.back().memory);
		m_memoryObjectCount++;

		// If heap was empty add to the non empty heaps.
		if (heap.memoryUsage == 0)
		{
			DE_ASSERT(heap.objects.size() == 1);
			m_nonEmptyHeaps.push_back(heapNdx);
		}
		else
			DE_ASSERT(heap.objects.size() > 1);

		heap.memoryUsage += allocationSize;

		// If heap became full, remove from non full heaps.
		if (heap.memoryUsage >= heap.maxMemoryUsage)
		{
			m_nonFullHeaps[nonFullHeapNdx] = m_nonFullHeaps.back();
			m_nonFullHeaps.pop_back();
		}
	}
	else
	{
		const size_t		nonEmptyHeapNdx	= (size_t)(m_rng.getUint32() % (deUint32)m_nonEmptyHeaps.size());
		const size_t		heapNdx			= m_nonEmptyHeaps[nonEmptyHeapNdx];
		Heap&				heap			= m_heaps[heapNdx];
		const size_t		memoryObjectNdx	= m_rng.getUint32() % heap.objects.size();
		MemoryObject&		memoryObject	= heap.objects[memoryObjectNdx];

		vkd.freeMemory(device, memoryObject.memory, (const VkAllocationCallbacks*)DE_NULL);
		memoryObject.memory = (VkDeviceMemory)0;
		m_memoryObjectCount--;

		if (heap.memoryUsage >= heap.maxMemoryUsage && heap.memoryUsage - memoryObject.size < heap.maxMemoryUsage)
			m_nonFullHeaps.push_back(heapNdx);

		heap.memoryUsage -= memoryObject.size;

		heap.objects[memoryObjectNdx] = heap.objects.back();
		heap.objects.pop_back();

		if (heap.memoryUsage == 0)
		{
			DE_ASSERT(heap.objects.empty());

			m_nonEmptyHeaps[nonEmptyHeapNdx] = m_nonEmptyHeaps.back();
			m_nonEmptyHeaps.pop_back();
		}
		else
			DE_ASSERT(!heap.objects.empty());
	}

	m_opNdx++;
	return tcu::TestStatus::incomplete();
}


} // anonymous

tcu::TestCaseGroup* createAllocationTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "allocation", "Memory allocation tests."));

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
		de::MovePtr<tcu::TestCaseGroup>	basicGroup	(new tcu::TestCaseGroup(testCtx, "basic", "Basic memory allocation and free tests"));

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
			const deUint32 seed = deInt32Hash(caseNdx ^ 32480);

			randomGroup->addChild(new InstanceFactory1<RandomAllocFreeTestInstance, deUint32>(testCtx, tcu::NODETYPE_SELF_VALIDATE, de::toString(caseNdx), "Random case", seed));
		}

		group->addChild(randomGroup.release());
	}

	return group.release();
}

} // memory
} // vkt
