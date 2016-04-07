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
 * \brief Simple memory mapping tests.
 *//*--------------------------------------------------------------------*/

#include "vktMemoryMappingTests.hpp"

#include "vktTestCaseUtil.hpp"

#include "tcuMaybe.hpp"
#include "tcuResultCollector.hpp"
#include "tcuTestLog.hpp"

#include "vkDeviceUtil.hpp"
#include "vkPlatform.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkStrUtil.hpp"

#include "deRandom.hpp"
#include "deSharedPtr.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include <string>
#include <vector>

using tcu::Maybe;
using tcu::TestLog;

using de::SharedPtr;

using std::string;
using std::vector;

using namespace vk;

namespace vkt
{
namespace memory
{
namespace
{
Move<VkDeviceMemory> allocMemory (const DeviceInterface& vk, VkDevice device, VkDeviceSize pAllocInfo_allocationSize, deUint32 pAllocInfo_memoryTypeIndex)
{
	VkDeviceMemory object = 0;
	const VkMemoryAllocateInfo pAllocInfo =
	{
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		DE_NULL,
		pAllocInfo_allocationSize,
		pAllocInfo_memoryTypeIndex,
	};
	VK_CHECK(vk.allocateMemory(device, &pAllocInfo, (const VkAllocationCallbacks*)DE_NULL, &object));
	return Move<VkDeviceMemory>(check<VkDeviceMemory>(object), Deleter<VkDeviceMemory>(vk, device, (const VkAllocationCallbacks*)DE_NULL));
}

struct MemoryRange
{
	MemoryRange (VkDeviceSize offset_ = ~(VkDeviceSize)0, VkDeviceSize size_ = ~(VkDeviceSize)0)
		: offset	(offset_)
		, size		(size_)
	{
	}

	VkDeviceSize	offset;
	VkDeviceSize	size;
};

struct TestConfig
{
	TestConfig (void)
		: allocationSize	(~(VkDeviceSize)0)
	{
	}

	VkDeviceSize		allocationSize;
	deUint32			seed;

	MemoryRange			mapping;
	vector<MemoryRange>	flushMappings;
	vector<MemoryRange>	invalidateMappings;
	bool				remap;
};

bool compareAndLogBuffer (TestLog& log, size_t size, const deUint8* result, const deUint8* reference)
{
	size_t	failedBytes	= 0;
	size_t	firstFailed	= (size_t)-1;

	for (size_t ndx = 0; ndx < size; ndx++)
	{
		if (result[ndx] != reference[ndx])
		{
			failedBytes++;

			if (firstFailed == (size_t)-1)
				firstFailed = ndx;
		}
	}

	if (failedBytes > 0)
	{
		log << TestLog::Message << "Comparison failed. Failed bytes " << failedBytes << ". First failed at offset " << firstFailed << "." << TestLog::EndMessage;

		std::ostringstream	expectedValues;
		std::ostringstream	resultValues;

		for (size_t ndx = firstFailed; ndx < firstFailed + 10 && ndx < size; ndx++)
		{
			if (ndx != firstFailed)
			{
				expectedValues << ", ";
				resultValues << ", ";
			}

			expectedValues << reference[ndx];
			resultValues << result[ndx];
		}

		if (firstFailed + 10 < size)
		{
			expectedValues << "...";
			resultValues << "...";
		}

		log << TestLog::Message << "Expected values at offset: " << firstFailed << ", " << expectedValues.str() << TestLog::EndMessage;
		log << TestLog::Message << "Result values at offset: " << firstFailed << ", " << resultValues.str() << TestLog::EndMessage;

		return false;
	}
	else
		return true;
}

tcu::TestStatus testMemoryMapping (Context& context, const TestConfig config)
{
	TestLog&								log					= context.getTestContext().getLog();
	tcu::ResultCollector					result				(log);
	const VkPhysicalDevice					physicalDevice		= context.getPhysicalDevice();
	const VkDevice							device				= context.getDevice();
	const InstanceInterface&				vki					= context.getInstanceInterface();
	const DeviceInterface&					vkd					= context.getDeviceInterface();
	const VkPhysicalDeviceMemoryProperties	memoryProperties	= getPhysicalDeviceMemoryProperties(vki, physicalDevice);

	{
		const tcu::ScopedLogSection	section	(log, "TestCaseInfo", "TestCaseInfo");

		log << TestLog::Message << "Seed: " << config.seed << TestLog::EndMessage;
		log << TestLog::Message << "Allocation size: " << config.allocationSize << TestLog::EndMessage;
		log << TestLog::Message << "Mapping, offset: " << config.mapping.offset << ", size: " << config.mapping.size << TestLog::EndMessage;

		if (!config.flushMappings.empty())
		{
			log << TestLog::Message << "Invalidating following ranges:" << TestLog::EndMessage;

			for (size_t ndx = 0; ndx < config.flushMappings.size(); ndx++)
				log << TestLog::Message << "\tOffset: " << config.flushMappings[ndx].offset << ", Size: " << config.flushMappings[ndx].size << TestLog::EndMessage;
		}

		if (config.remap)
			log << TestLog::Message << "Remapping memory between flush and invalidation." << TestLog::EndMessage;

		if (!config.invalidateMappings.empty())
		{
			log << TestLog::Message << "Flushing following ranges:" << TestLog::EndMessage;

			for (size_t ndx = 0; ndx < config.invalidateMappings.size(); ndx++)
				log << TestLog::Message << "\tOffset: " << config.invalidateMappings[ndx].offset << ", Size: " << config.invalidateMappings[ndx].size << TestLog::EndMessage;
		}
	}

	for (deUint32 memoryTypeIndex = 0; memoryTypeIndex < memoryProperties.memoryTypeCount; memoryTypeIndex++)
	{
		try
		{
			const tcu::ScopedLogSection		section		(log, "MemoryType" + de::toString(memoryTypeIndex), "MemoryType" + de::toString(memoryTypeIndex));
			const VkMemoryType&				memoryType	= memoryProperties.memoryTypes[memoryTypeIndex];
			const VkMemoryHeap&				memoryHeap	= memoryProperties.memoryHeaps[memoryType.heapIndex];

			log << TestLog::Message << "MemoryType: " << memoryType << TestLog::EndMessage;
			log << TestLog::Message << "MemoryHeap: " << memoryHeap << TestLog::EndMessage;

			if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
			{
				log << TestLog::Message << "Memory type doesn't support mapping." << TestLog::EndMessage;
			}
			else
			{
				const Unique<VkDeviceMemory>	memory				(allocMemory(vkd, device, config.allocationSize, memoryTypeIndex));
				de::Random						rng					(config.seed);
				vector<deUint8>					reference			((size_t)config.allocationSize);
				deUint8*						mapping				= DE_NULL;

				{
					void* ptr;
					VK_CHECK(vkd.mapMemory(device, *memory, config.mapping.offset, config.mapping.size, 0u, &ptr));
					TCU_CHECK(ptr);

					mapping = (deUint8*)ptr;
				}

				for (VkDeviceSize ndx = 0; ndx < config.mapping.size; ndx++)
				{
					const deUint8 val = rng.getUint8();

					mapping[ndx]										= val;
					reference[(size_t)(config.mapping.offset + ndx)]	= val;
				}

				if (!config.flushMappings.empty())
				{
					vector<VkMappedMemoryRange> ranges;

					for (size_t ndx = 0; ndx < config.flushMappings.size(); ndx++)
					{
						const VkMappedMemoryRange range =
						{
							VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
							DE_NULL,

							*memory,
							config.flushMappings[ndx].offset,
							config.flushMappings[ndx].size
						};

						ranges.push_back(range);
					}

					VK_CHECK(vkd.flushMappedMemoryRanges(device, (deUint32)ranges.size(), &ranges[0]));
				}

				if (config.remap)
				{
					void* ptr;
					vkd.unmapMemory(device, *memory);
					VK_CHECK(vkd.mapMemory(device, *memory, config.mapping.offset, config.mapping.size, 0u, &ptr));
					TCU_CHECK(ptr);

					mapping = (deUint8*)ptr;
				}

				if (!config.invalidateMappings.empty())
				{
					vector<VkMappedMemoryRange> ranges;

					for (size_t ndx = 0; ndx < config.invalidateMappings.size(); ndx++)
					{
						const VkMappedMemoryRange range =
						{
							VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
							DE_NULL,

							*memory,
							config.invalidateMappings[ndx].offset,
							config.invalidateMappings[ndx].size
						};

						ranges.push_back(range);
					}

					VK_CHECK(vkd.invalidateMappedMemoryRanges(device, (deUint32)ranges.size(), &ranges[0]));
				}

				if (!compareAndLogBuffer(log, (size_t)config.mapping.size, mapping, &reference[(size_t)config.mapping.offset]))
					result.fail("Unexpected values read from mapped memory.");

				vkd.unmapMemory(device, *memory);
			}
		}
		catch (const tcu::TestError& error)
		{
			result.fail(error.getMessage());
		}
	}

	return tcu::TestStatus(result.getResult(), result.getMessage());
}

class MemoryMapping
{
public:
						MemoryMapping	(const MemoryRange&	range,
										 void*				ptr,
										 deUint16*			refPtr);

	void				randomRead		(de::Random& rng);
	void				randomWrite		(de::Random& rng);
	void				randomModify	(de::Random& rng);

	const MemoryRange&	getRange		(void) const { return m_range; }

private:
	MemoryRange	m_range;
	void*		m_ptr;
	deUint16*	m_refPtr;
};

MemoryMapping::MemoryMapping (const MemoryRange&	range,
							  void*					ptr,
							  deUint16*				refPtr)
	: m_range	(range)
	, m_ptr		(ptr)
	, m_refPtr	(refPtr)
{
	DE_ASSERT(range.size > 0);
}

void MemoryMapping::randomRead (de::Random& rng)
{
	const size_t count = (size_t)rng.getInt(0, 100);

	for (size_t ndx = 0; ndx < count; ndx++)
	{
		const size_t	pos	= (size_t)(rng.getUint64() % (deUint64)m_range.size);
		const deUint8	val	= ((deUint8*) m_ptr)[pos];

		if (m_refPtr[pos] < 256)
			TCU_CHECK((deUint16)val == m_refPtr[pos]);
		else
			m_refPtr[pos] = (deUint16)val;
	}
}

void MemoryMapping::randomWrite (de::Random& rng)
{
	const size_t count = (size_t)rng.getInt(0, 100);

	for (size_t ndx = 0; ndx < count; ndx++)
	{
		const size_t	pos	= (size_t)(rng.getUint64() % (deUint64)m_range.size);
		const deUint8	val	= rng.getUint8();

		((deUint8*)m_ptr)[pos]	= val;
		m_refPtr[pos]			= (deUint16)val;
	}
}

void MemoryMapping::randomModify (de::Random& rng)
{
	const size_t count = (size_t)rng.getInt(0, 100);

	for (size_t ndx = 0; ndx < count; ndx++)
	{
		const size_t	pos		= (size_t)(rng.getUint64() % (deUint64)m_range.size);
		const deUint8	val		= ((deUint8*)m_ptr)[pos];
		const deUint8	mask	= rng.getUint8();

		if (m_refPtr[pos] < 256)
			TCU_CHECK((deUint16)val == m_refPtr[pos]);

		((deUint8*)m_ptr)[pos]	= val ^ mask;
		m_refPtr[pos]			= (deUint16)(val ^ mask);
	}
}

void randomRanges (de::Random& rng, vector<VkMappedMemoryRange>& ranges, size_t count, VkDeviceMemory memory, VkDeviceSize minOffset, VkDeviceSize maxSize)
{
	ranges.resize(count);

	for (size_t rangeNdx = 0; rangeNdx < count; rangeNdx++)
	{
		const VkDeviceSize	size	= (maxSize > 1 ? (VkDeviceSize)(1 + (rng.getUint64() % (deUint64)(maxSize - 1))) : 1);
		const VkDeviceSize	offset	= minOffset + (VkDeviceSize)(rng.getUint64() % (deUint64)(maxSize - size + 1));

		const VkMappedMemoryRange range =
		{
			VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
			DE_NULL,

			memory,
			offset,
			size
		};
		ranges[rangeNdx] = range;
	}
}

class MemoryObject
{
public:
							MemoryObject		(const DeviceInterface&		vkd,
												 VkDevice					device,
												 VkDeviceSize				size,
												 deUint32					memoryTypeIndex);

							~MemoryObject		(void);

	MemoryMapping*			mapRandom			(const DeviceInterface& vkd, VkDevice device, de::Random& rng);
	void					unmap				(void);

	void					randomFlush			(const DeviceInterface& vkd, VkDevice device, de::Random& rng);
	void					randomInvalidate	(const DeviceInterface& vkd, VkDevice device, de::Random& rng);

	VkDeviceSize			getSize				(void) const { return m_size; }
	MemoryMapping*			getMapping			(void) { return m_mapping; }

private:
	const DeviceInterface&	m_vkd;
	VkDevice				m_device;

	deUint32				m_memoryTypeIndex;
	VkDeviceSize			m_size;

	Move<VkDeviceMemory>	m_memory;

	MemoryMapping*			m_mapping;
	vector<deUint16>		m_reference;
};

MemoryObject::MemoryObject (const DeviceInterface&		vkd,
							VkDevice					device,
							VkDeviceSize				size,
							deUint32					memoryTypeIndex)
	: m_vkd				(vkd)
	, m_device			(device)
	, m_memoryTypeIndex	(memoryTypeIndex)
	, m_size			(size)
	, m_mapping			(DE_NULL)
{
	m_memory = allocMemory(m_vkd, m_device, m_size, m_memoryTypeIndex);
	m_reference.resize((size_t)m_size, 0xFFFFu);
}

MemoryObject::~MemoryObject (void)
{
	delete m_mapping;
}

MemoryMapping* MemoryObject::mapRandom (const DeviceInterface& vkd, VkDevice device, de::Random& rng)
{
	const VkDeviceSize	size	= (m_size > 1 ? (VkDeviceSize)(1 + (rng.getUint64() % (deUint64)(m_size - 1))) : 1);
	const VkDeviceSize	offset	= (VkDeviceSize)(rng.getUint64() % (deUint64)(m_size - size + 1));
	void*				ptr;

	DE_ASSERT(!m_mapping);

	VK_CHECK(vkd.mapMemory(device, *m_memory, offset, size, 0u, &ptr));
	TCU_CHECK(ptr);
	m_mapping = new MemoryMapping(MemoryRange(offset, size), ptr, &(m_reference[(size_t)offset]));

	return m_mapping;
}

void MemoryObject::unmap (void)
{
	m_vkd.unmapMemory(m_device, *m_memory);

	delete m_mapping;
	m_mapping = DE_NULL;
}

void MemoryObject::randomFlush (const DeviceInterface& vkd, VkDevice device, de::Random& rng)
{
	const size_t				rangeCount	= (size_t)rng.getInt(1, 10);
	vector<VkMappedMemoryRange>	ranges		(rangeCount);

	randomRanges(rng, ranges, rangeCount, *m_memory, m_mapping->getRange().offset, m_mapping->getRange().size);

	VK_CHECK(vkd.flushMappedMemoryRanges(device, (deUint32)ranges.size(), ranges.empty() ? DE_NULL : &ranges[0]));
}

void MemoryObject::randomInvalidate (const DeviceInterface& vkd, VkDevice device, de::Random& rng)
{
	const size_t				rangeCount	= (size_t)rng.getInt(1, 10);
	vector<VkMappedMemoryRange>	ranges		(rangeCount);

	randomRanges(rng, ranges, rangeCount, *m_memory, m_mapping->getRange().offset, m_mapping->getRange().size);

	VK_CHECK(vkd.invalidateMappedMemoryRanges(device, (deUint32)ranges.size(), ranges.empty() ? DE_NULL : &ranges[0]));
}

enum
{
	// Use only 1/16 of each memory heap.
	MAX_MEMORY_USAGE_DIV = 16
};

template<typename T>
void removeFirstEqual (vector<T>& vec, const T& val)
{
	for (size_t ndx = 0; ndx < vec.size(); ndx++)
	{
		if (vec[ndx] == val)
		{
			vec[ndx] = vec.back();
			vec.pop_back();
			return;
		}
	}
}

class MemoryHeap
{
public:
	MemoryHeap (const VkMemoryHeap&			heap,
				const vector<deUint32>&		memoryTypes)
		: m_heap		(heap)
		, m_memoryTypes	(memoryTypes)
		, m_usage		(0)
	{
	}

	~MemoryHeap (void)
	{
		for (vector<MemoryObject*>::iterator iter = m_objects.begin(); iter != m_objects.end(); ++iter)
			delete *iter;
	}

	bool								full			(void) const { return m_usage * MAX_MEMORY_USAGE_DIV >= m_heap.size; }
	bool								empty			(void) const { return m_usage == 0; }

	MemoryObject*						allocateRandom	(const DeviceInterface& vkd, VkDevice device, de::Random& rng)
	{
		const VkDeviceSize		size	= 1 + (rng.getUint64() % (de::max((deInt64)((m_heap.size / MAX_MEMORY_USAGE_DIV) - m_usage - 1ull), (deInt64)1)));
		const deUint32			type	= rng.choose<deUint32>(m_memoryTypes.begin(), m_memoryTypes.end());

		if ( (size > (VkDeviceSize)((m_heap.size / MAX_MEMORY_USAGE_DIV) - m_usage)) && (size != 1))
			TCU_THROW(InternalError, "Test Error: trying to allocate memory more than the available heap size.");

		MemoryObject* const		object	= new MemoryObject(vkd, device, size, type);

		m_usage += size;
		m_objects.push_back(object);

		return object;
	}

	MemoryObject*						getRandomObject	(de::Random& rng) const
	{
		return rng.choose<MemoryObject*>(m_objects.begin(), m_objects.end());
	}

	void								free			(MemoryObject* object)
	{
		removeFirstEqual(m_objects, object);
		m_usage -= object->getSize();
		delete object;
	}

private:
	VkMemoryHeap			m_heap;
	vector<deUint32>		m_memoryTypes;

	VkDeviceSize			m_usage;
	vector<MemoryObject*>	m_objects;
};

class RandomMemoryMappingInstance : public TestInstance
{
public:
	RandomMemoryMappingInstance (Context& context, deUint32 seed)
		: TestInstance	(context)
		, m_rng			(seed)
		, m_opNdx		(0)
	{
		const VkPhysicalDevice					physicalDevice		= context.getPhysicalDevice();
		const InstanceInterface&				vki					= context.getInstanceInterface();
		const VkPhysicalDeviceMemoryProperties	memoryProperties	= getPhysicalDeviceMemoryProperties(vki, physicalDevice);

		// Initialize heaps
		{
			vector<vector<deUint32> >	memoryTypes	(memoryProperties.memoryHeapCount);

			for (deUint32 memoryTypeNdx = 0; memoryTypeNdx < memoryProperties.memoryTypeCount; memoryTypeNdx++)
			{
				if (memoryProperties.memoryTypes[memoryTypeNdx].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
					memoryTypes[memoryProperties.memoryTypes[memoryTypeNdx].heapIndex].push_back(memoryTypeNdx);
			}

			for (deUint32 heapIndex = 0; heapIndex < memoryProperties.memoryHeapCount; heapIndex++)
			{
				const VkMemoryHeap	heapInfo	= memoryProperties.memoryHeaps[heapIndex];

				if (!memoryTypes[heapIndex].empty())
				{
					const de::SharedPtr<MemoryHeap>	heap	(new MemoryHeap(heapInfo, memoryTypes[heapIndex]));

					if (!heap->full())
						m_nonFullHeaps.push_back(heap);
				}
			}
		}
	}

	~RandomMemoryMappingInstance (void)
	{
	}

	tcu::TestStatus iterate (void)
	{
		const size_t			opCount						= 100;
		const float				memoryOpProbability			= 0.5f;		// 0.50
		const float				flushInvalidateProbability	= 0.4f;		// 0.20
		const float				mapProbability				= 0.50f;	// 0.15
		const float				unmapProbability			= 0.25f;	// 0.075

		const float				allocProbability			= 0.75f; // Versun free

		const VkDevice			device						= m_context.getDevice();
		const DeviceInterface&	vkd							= m_context.getDeviceInterface();

		if (m_opNdx < opCount)
		{
			if (!m_memoryMappings.empty() && m_rng.getFloat() < memoryOpProbability)
			{
				// Perform operations on mapped memory
				MemoryMapping* const	mapping	= m_rng.choose<MemoryMapping*>(m_memoryMappings.begin(), m_memoryMappings.end());

				enum Op
				{
					OP_READ = 0,
					OP_WRITE,
					OP_MODIFY,
					OP_LAST
				};

				const Op op = (Op)(m_rng.getUint32() % OP_LAST);

				switch (op)
				{
					case OP_READ:
						mapping->randomRead(m_rng);
						break;

					case OP_WRITE:
						mapping->randomWrite(m_rng);
						break;

					case OP_MODIFY:
						mapping->randomModify(m_rng);
						break;

					default:
						DE_FATAL("Invalid operation");
				}
			}
			else if (!m_mappedMemoryObjects.empty() && m_rng.getFloat() < flushInvalidateProbability)
			{
				MemoryObject* const	object	= m_rng.choose<MemoryObject*>(m_mappedMemoryObjects.begin(), m_mappedMemoryObjects.end());

				if (m_rng.getBool())
					object->randomFlush(vkd, device, m_rng);
				else
					object->randomInvalidate(vkd, device, m_rng);
			}
			else if (!m_mappedMemoryObjects.empty() && m_rng.getFloat() < unmapProbability)
			{
				// Unmap memory object
				MemoryObject* const	object	= m_rng.choose<MemoryObject*>(m_mappedMemoryObjects.begin(), m_mappedMemoryObjects.end());

				// Remove mapping
				removeFirstEqual(m_memoryMappings, object->getMapping());

				object->unmap();
				removeFirstEqual(m_mappedMemoryObjects, object);
				m_nonMappedMemoryObjects.push_back(object);
			}
			else if (!m_nonMappedMemoryObjects.empty() && m_rng.getFloat() < mapProbability)
			{
				// Map memory object
				MemoryObject* const		object	= m_rng.choose<MemoryObject*>(m_nonMappedMemoryObjects.begin(), m_nonMappedMemoryObjects.end());
				MemoryMapping*			mapping	= object->mapRandom(vkd, device, m_rng);

				m_memoryMappings.push_back(mapping);
				m_mappedMemoryObjects.push_back(object);
				removeFirstEqual(m_nonMappedMemoryObjects, object);
			}
			else
			{
				if (!m_nonFullHeaps.empty() && (m_nonEmptyHeaps.empty() || m_rng.getFloat() < allocProbability))
				{
					// Allocate more memory objects
					de::SharedPtr<MemoryHeap> const heap = m_rng.choose<de::SharedPtr<MemoryHeap> >(m_nonFullHeaps.begin(), m_nonFullHeaps.end());

					if (heap->empty())
						m_nonEmptyHeaps.push_back(heap);

					{
						MemoryObject* const	object = heap->allocateRandom(vkd, device, m_rng);

						if (heap->full())
							removeFirstEqual(m_nonFullHeaps, heap);

						m_nonMappedMemoryObjects.push_back(object);
					}
				}
				else
				{
					// Free memory objects
					de::SharedPtr<MemoryHeap> const		heap	= m_rng.choose<de::SharedPtr<MemoryHeap> >(m_nonEmptyHeaps.begin(), m_nonEmptyHeaps.end());
					MemoryObject* const					object	= heap->getRandomObject(m_rng);

					// Remove mapping
					if (object->getMapping())
						removeFirstEqual(m_memoryMappings, object->getMapping());

					removeFirstEqual(m_mappedMemoryObjects, object);
					removeFirstEqual(m_nonMappedMemoryObjects, object);

					if (heap->full())
						m_nonFullHeaps.push_back(heap);

					heap->free(object);

					if (heap->empty())
						removeFirstEqual(m_nonEmptyHeaps, heap);
				}
			}

			m_opNdx++;
			return tcu::TestStatus::incomplete();
		}
		else
			return tcu::TestStatus::pass("Pass");
	}

private:
	de::Random							m_rng;
	size_t								m_opNdx;

	vector<de::SharedPtr<MemoryHeap> >	m_nonEmptyHeaps;
	vector<de::SharedPtr<MemoryHeap> >	m_nonFullHeaps;

	vector<MemoryObject*>				m_mappedMemoryObjects;
	vector<MemoryObject*>				m_nonMappedMemoryObjects;
	vector<MemoryMapping*>				m_memoryMappings;
};

enum Op
{
	OP_NONE = 0,

	OP_FLUSH,
	OP_SUB_FLUSH,
	OP_SUB_FLUSH_SEPARATE,
	OP_SUB_FLUSH_OVERLAPPING,

	OP_INVALIDATE,
	OP_SUB_INVALIDATE,
	OP_SUB_INVALIDATE_SEPARATE,
	OP_SUB_INVALIDATE_OVERLAPPING,

	OP_REMAP,

	OP_LAST
};

TestConfig subMappedConfig (VkDeviceSize				allocationSize,
							const MemoryRange&			mapping,
							Op							op,
							deUint32					seed)
{
	TestConfig config;

	config.allocationSize	= allocationSize;
	config.seed				= seed;
	config.mapping			= mapping;
	config.remap			= false;

	switch (op)
	{
		case OP_NONE:
			return config;

		case OP_REMAP:
			config.remap = true;
			return config;

		case OP_FLUSH:
			config.flushMappings = vector<MemoryRange>(1, MemoryRange(mapping.offset, mapping.size));
			return config;

		case OP_SUB_FLUSH:
			DE_ASSERT(mapping.size / 4 > 0);

			config.flushMappings = vector<MemoryRange>(1, MemoryRange(mapping.offset + mapping.size / 4, mapping.size / 2));
			return config;

		case OP_SUB_FLUSH_SEPARATE:
			DE_ASSERT(mapping.size / 2 > 0);

			config.flushMappings.push_back(MemoryRange(mapping.offset + mapping.size /  2, mapping.size - (mapping.size / 2)));
			config.flushMappings.push_back(MemoryRange(mapping.offset, mapping.size / 2));

			return config;

		case OP_SUB_FLUSH_OVERLAPPING:
			DE_ASSERT((mapping.size / 3) > 0);

			config.flushMappings.push_back(MemoryRange(mapping.offset + mapping.size /  3, mapping.size - (mapping.size / 2)));
			config.flushMappings.push_back(MemoryRange(mapping.offset, (2 * mapping.size) / 3));

			return config;

		case OP_INVALIDATE:
			config.invalidateMappings = vector<MemoryRange>(1, MemoryRange(mapping.offset, mapping.size));
			return config;

		case OP_SUB_INVALIDATE:
			DE_ASSERT(mapping.size / 4 > 0);

			config.invalidateMappings = vector<MemoryRange>(1, MemoryRange(mapping.offset + mapping.size / 4, mapping.size / 2));
			return config;

		case OP_SUB_INVALIDATE_SEPARATE:
			DE_ASSERT(mapping.size / 2 > 0);

			config.invalidateMappings.push_back(MemoryRange(mapping.offset + mapping.size /  2, mapping.size - (mapping.size / 2)));
			config.invalidateMappings.push_back(MemoryRange(mapping.offset, mapping.size / 2));

			return config;

		case OP_SUB_INVALIDATE_OVERLAPPING:
			DE_ASSERT((mapping.size / 3) > 0);

			config.invalidateMappings.push_back(MemoryRange(mapping.offset + mapping.size /  3, mapping.size - (mapping.size / 2)));
			config.invalidateMappings.push_back(MemoryRange(mapping.offset, (2 * mapping.size) / 3));

			return config;

		default:
			DE_FATAL("Unknown Op");
			return TestConfig();
	}
}

TestConfig fullMappedConfig (VkDeviceSize	allocationSize,
							 Op				op,
							 deUint32		seed)
{
	return subMappedConfig(allocationSize, MemoryRange(0, allocationSize), op, seed);
}

} // anonymous

tcu::TestCaseGroup* createMappingTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "mapping", "Memory mapping tests."));

	const VkDeviceSize allocationSizes[] =
	{
		33, 257, 4087, 8095, 1*1024*1024 + 1
	};

	const VkDeviceSize offsets[] =
	{
		0, 17, 129, 255, 1025, 32*1024+1
	};

	const VkDeviceSize sizes[] =
	{
		31, 255, 1025, 4085, 1*1024*1024 - 1
	};

	const struct
	{
		const Op			op;
		const char* const	name;
	} ops[] =
	{
		{ OP_NONE,						"simple"					},
		{ OP_REMAP,						"remap"						},
		{ OP_FLUSH,						"flush"						},
		{ OP_SUB_FLUSH,					"subflush"					},
		{ OP_SUB_FLUSH_SEPARATE,		"subflush_separate"			},
		{ OP_SUB_FLUSH_SEPARATE,		"subflush_overlapping"		},

		{ OP_INVALIDATE,				"invalidate"				},
		{ OP_SUB_INVALIDATE,			"subinvalidate"				},
		{ OP_SUB_INVALIDATE_SEPARATE,	"subinvalidate_separate"	},
		{ OP_SUB_INVALIDATE_SEPARATE,	"subinvalidate_overlapping"	}
	};

	// .full
	{
		de::MovePtr<tcu::TestCaseGroup> fullGroup (new tcu::TestCaseGroup(testCtx, "full", "Map memory completely."));

		for (size_t allocationSizeNdx = 0; allocationSizeNdx < DE_LENGTH_OF_ARRAY(allocationSizes); allocationSizeNdx++)
		{
			const VkDeviceSize				allocationSize		= allocationSizes[allocationSizeNdx];
			de::MovePtr<tcu::TestCaseGroup>	allocationSizeGroup	(new tcu::TestCaseGroup(testCtx, de::toString(allocationSize).c_str(), ""));

			for (size_t opNdx = 0; opNdx < DE_LENGTH_OF_ARRAY(ops); opNdx++)
			{
				const Op			op		= ops[opNdx].op;
				const char* const	name	= ops[opNdx].name;
				const deUint32		seed	= (deUint32)(opNdx * allocationSizeNdx);
				const TestConfig	config	= fullMappedConfig(allocationSize, op, seed);

				addFunctionCase(allocationSizeGroup.get(), name, name, testMemoryMapping, config);
			}

			fullGroup->addChild(allocationSizeGroup.release());
		}

		group->addChild(fullGroup.release());
	}

	// .sub
	{
		de::MovePtr<tcu::TestCaseGroup> subGroup (new tcu::TestCaseGroup(testCtx, "sub", "Map part of the memory."));

		for (size_t allocationSizeNdx = 0; allocationSizeNdx < DE_LENGTH_OF_ARRAY(allocationSizes); allocationSizeNdx++)
		{
			const VkDeviceSize				allocationSize		= allocationSizes[allocationSizeNdx];
			de::MovePtr<tcu::TestCaseGroup>	allocationSizeGroup	(new tcu::TestCaseGroup(testCtx, de::toString(allocationSize).c_str(), ""));

			for (size_t offsetNdx = 0; offsetNdx < DE_LENGTH_OF_ARRAY(offsets); offsetNdx++)
			{
				const VkDeviceSize				offset			= offsets[offsetNdx];

				if (offset >= allocationSize)
					continue;

				de::MovePtr<tcu::TestCaseGroup>	offsetGroup		(new tcu::TestCaseGroup(testCtx, ("offset_" + de::toString(offset)).c_str(), ""));

				for (size_t sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(sizes); sizeNdx++)
				{
					const VkDeviceSize				size		= sizes[sizeNdx];

					if (offset + size > allocationSize)
						continue;

					if (offset == 0 && size == allocationSize)
						continue;

					de::MovePtr<tcu::TestCaseGroup>	sizeGroup	(new tcu::TestCaseGroup(testCtx, ("size_" + de::toString(size)).c_str(), ""));

					for (size_t opNdx = 0; opNdx < DE_LENGTH_OF_ARRAY(ops); opNdx++)
					{
						const deUint32		seed	= (deUint32)(opNdx * allocationSizeNdx);
						const Op			op		= ops[opNdx].op;
						const char* const	name	= ops[opNdx].name;
						const TestConfig	config	= subMappedConfig(allocationSize, MemoryRange(offset, size), op, seed);

						addFunctionCase(sizeGroup.get(), name, name, testMemoryMapping, config);
					}

					offsetGroup->addChild(sizeGroup.release());
				}

				allocationSizeGroup->addChild(offsetGroup.release());
			}

			subGroup->addChild(allocationSizeGroup.release());
		}

		group->addChild(subGroup.release());
	}

	// .random
	{
		de::MovePtr<tcu::TestCaseGroup>	randomGroup	(new tcu::TestCaseGroup(testCtx, "random", "Random memory mapping tests."));
		de::Random						rng			(3927960301u);

		for (size_t ndx = 0; ndx < 100; ndx++)
		{
			const deUint32		seed	= rng.getUint32();
			const std::string	name	= de::toString(ndx);

			randomGroup->addChild(new InstanceFactory1<RandomMemoryMappingInstance, deUint32>(testCtx, tcu::NODETYPE_SELF_VALIDATE, de::toString(ndx), "Random case", seed));
		}

		group->addChild(randomGroup.release());
	}

	return group.release();
}

} // memory
} // vkt
