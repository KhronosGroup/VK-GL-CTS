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
#include "tcuPlatform.hpp"

#include "vkDeviceUtil.hpp"
#include "vkPlatform.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkAllocationCallbackUtil.hpp"

#include "deRandom.hpp"
#include "deSharedPtr.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "deSTLUtil.hpp"

#include <string>
#include <vector>
#include <algorithm>

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
enum
{
	REFERENCE_BYTES_PER_BYTE = 2
};

template<typename T>
T divRoundUp (const T& a, const T& b)
{
	return (a / b) + (a % b == 0 ? 0 : 1);
}

// \note Bit vector that guarantees that each value takes only one bit.
// std::vector<bool> is often optimized to only take one bit for each bool, but
// that is implementation detail and in this case we really need to known how much
// memory is used.
class BitVector
{
public:
	enum
	{
		BLOCK_BIT_SIZE = 8 * sizeof(deUint32)
	};

	BitVector (size_t size, bool value = false)
		: m_data(divRoundUp<size_t>(size, (size_t)BLOCK_BIT_SIZE), value ? ~0x0u : 0x0u)
	{
	}

	bool get (size_t ndx) const
	{
		return (m_data[ndx / BLOCK_BIT_SIZE] & (0x1u << (deUint32)(ndx % BLOCK_BIT_SIZE))) != 0;
	}

	void set (size_t ndx, bool value)
	{
		if (value)
			m_data[ndx / BLOCK_BIT_SIZE] |= 0x1u << (deUint32)(ndx % BLOCK_BIT_SIZE);
		else
			m_data[ndx / BLOCK_BIT_SIZE] &= ~(0x1u << (deUint32)(ndx % BLOCK_BIT_SIZE));
	}

private:
	vector<deUint32>	m_data;
};

class ReferenceMemory
{
public:
	ReferenceMemory (size_t size, size_t atomSize)
		: m_atomSize	(atomSize)
		, m_bytes		(size, 0xDEu)
		, m_defined		(size, false)
		, m_flushed		(size / atomSize, false)
	{
		DE_ASSERT(size % m_atomSize == 0);
	}

	void write (size_t pos, deUint8 value)
	{
		m_bytes[pos] = value;
		m_defined.set(pos, true);
		m_flushed.set(pos / m_atomSize, false);
	}

	bool read (size_t pos, deUint8 value)
	{
		const bool isOk = !m_defined.get(pos)
						|| m_bytes[pos] == value;

		m_bytes[pos] = value;
		m_defined.set(pos, true);

		return isOk;
	}

	bool modifyXor (size_t pos, deUint8 value, deUint8 mask)
	{
		const bool isOk = !m_defined.get(pos)
						|| m_bytes[pos] == value;

		m_bytes[pos] = value ^ mask;
		m_defined.set(pos, true);
		m_flushed.set(pos / m_atomSize, false);

		return isOk;
	}

	void flush (size_t offset, size_t size)
	{
		DE_ASSERT((offset % m_atomSize) == 0);
		DE_ASSERT((size % m_atomSize) == 0);

		for (size_t ndx = 0; ndx < size / m_atomSize; ndx++)
			m_flushed.set((offset / m_atomSize) + ndx, true);
	}

	void invalidate (size_t offset, size_t size)
	{
		DE_ASSERT((offset % m_atomSize) == 0);
		DE_ASSERT((size % m_atomSize) == 0);

		for (size_t ndx = 0; ndx < size / m_atomSize; ndx++)
		{
			if (!m_flushed.get((offset / m_atomSize) + ndx))
			{
				for (size_t i = 0; i < m_atomSize; i++)
					m_defined.set(offset + ndx * m_atomSize + i, false);
			}
		}
	}


private:
	const size_t	m_atomSize;
	vector<deUint8>	m_bytes;
	BitVector		m_defined;
	BitVector		m_flushed;
};

struct MemoryType
{
	MemoryType		(deUint32 index_, const VkMemoryType& type_)
		: index	(index_)
		, type	(type_)
	{
	}

	MemoryType		(void)
		: index	(~0u)
	{
	}

	deUint32		index;
	VkMemoryType	type;
};

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
		const Unique<VkDeviceMemory>			memory			(allocateMemory(vk, device, &allocInfo));
		AllocationCallbackValidationResults		validateRes;

		validateAllocationCallbacks(callbackRecorder, &validateRes);

		TCU_CHECK(validateRes.violations.empty());

		return getLiveSystemAllocationTotal(validateRes)
			   + sizeof(void*)*validateRes.liveAllocations.size(); // allocation overhead
	}
}

Move<VkDeviceMemory> allocMemory (const DeviceInterface& vk, VkDevice device, VkDeviceSize pAllocInfo_allocationSize, deUint32 pAllocInfo_memoryTypeIndex)
{
	const VkMemoryAllocateInfo pAllocInfo =
	{
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		DE_NULL,
		pAllocInfo_allocationSize,
		pAllocInfo_memoryTypeIndex,
	};
	return allocateMemory(vk, device, &pAllocInfo);
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
	// \todo [2016-05-27 misojarvi] Remove once drivers start reporting correctly nonCoherentAtomSize that is at least 1.
	const VkDeviceSize						nonCoherentAtomSize	= context.getDeviceProperties().limits.nonCoherentAtomSize != 0
																? context.getDeviceProperties().limits.nonCoherentAtomSize
																: 1;

	{
		const tcu::ScopedLogSection	section	(log, "TestCaseInfo", "TestCaseInfo");

		log << TestLog::Message << "Seed: " << config.seed << TestLog::EndMessage;
		log << TestLog::Message << "Allocation size: " << config.allocationSize << " * atom" <<  TestLog::EndMessage;
		log << TestLog::Message << "Mapping, offset: " << config.mapping.offset << " * atom, size: " << config.mapping.size << " * atom" << TestLog::EndMessage;

		if (!config.flushMappings.empty())
		{
			log << TestLog::Message << "Invalidating following ranges:" << TestLog::EndMessage;

			for (size_t ndx = 0; ndx < config.flushMappings.size(); ndx++)
				log << TestLog::Message << "\tOffset: " << config.flushMappings[ndx].offset << " * atom, Size: " << config.flushMappings[ndx].size << " * atom" << TestLog::EndMessage;
		}

		if (config.remap)
			log << TestLog::Message << "Remapping memory between flush and invalidation." << TestLog::EndMessage;

		if (!config.invalidateMappings.empty())
		{
			log << TestLog::Message << "Flushing following ranges:" << TestLog::EndMessage;

			for (size_t ndx = 0; ndx < config.invalidateMappings.size(); ndx++)
				log << TestLog::Message << "\tOffset: " << config.invalidateMappings[ndx].offset << " * atom, Size: " << config.invalidateMappings[ndx].size << " * atom" << TestLog::EndMessage;
		}
	}

	for (deUint32 memoryTypeIndex = 0; memoryTypeIndex < memoryProperties.memoryTypeCount; memoryTypeIndex++)
	{
		try
		{
			const tcu::ScopedLogSection		section		(log, "MemoryType" + de::toString(memoryTypeIndex), "MemoryType" + de::toString(memoryTypeIndex));
			const VkMemoryType&				memoryType	= memoryProperties.memoryTypes[memoryTypeIndex];
			const VkMemoryHeap&				memoryHeap	= memoryProperties.memoryHeaps[memoryType.heapIndex];
			const VkDeviceSize				atomSize	= (memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0
														? 1
														: nonCoherentAtomSize;

			log << TestLog::Message << "MemoryType: " << memoryType << TestLog::EndMessage;
			log << TestLog::Message << "MemoryHeap: " << memoryHeap << TestLog::EndMessage;
			log << TestLog::Message << "AtomSize: " << atomSize << TestLog::EndMessage;
			log << TestLog::Message << "AllocationSize: " << config.allocationSize * atomSize <<  TestLog::EndMessage;
			log << TestLog::Message << "Mapping, offset: " << config.mapping.offset * atomSize << ", size: " << config.mapping.size * atomSize << TestLog::EndMessage;

			if (!config.flushMappings.empty())
			{
				log << TestLog::Message << "Invalidating following ranges:" << TestLog::EndMessage;

				for (size_t ndx = 0; ndx < config.flushMappings.size(); ndx++)
					log << TestLog::Message << "\tOffset: " << config.flushMappings[ndx].offset * atomSize << ", Size: " << config.flushMappings[ndx].size * atomSize << TestLog::EndMessage;
			}

			if (!config.invalidateMappings.empty())
			{
				log << TestLog::Message << "Flushing following ranges:" << TestLog::EndMessage;

				for (size_t ndx = 0; ndx < config.invalidateMappings.size(); ndx++)
					log << TestLog::Message << "\tOffset: " << config.invalidateMappings[ndx].offset * atomSize << ", Size: " << config.invalidateMappings[ndx].size * atomSize << TestLog::EndMessage;
			}

			if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
			{
				log << TestLog::Message << "Memory type doesn't support mapping." << TestLog::EndMessage;
			}
			else if (memoryHeap.size <= 4 * atomSize * config.allocationSize)
			{
				log << TestLog::Message << "Memory types heap is too small." << TestLog::EndMessage;
			}
			else
			{
				const Unique<VkDeviceMemory>	memory				(allocMemory(vkd, device, config.allocationSize * atomSize, memoryTypeIndex));
				de::Random						rng					(config.seed);
				vector<deUint8>					reference			((size_t)(config.allocationSize * atomSize));
				deUint8*						mapping				= DE_NULL;

				{
					void* ptr;
					VK_CHECK(vkd.mapMemory(device, *memory, config.mapping.offset * atomSize, config.mapping.size * atomSize, 0u, &ptr));
					TCU_CHECK(ptr);

					mapping = (deUint8*)ptr;
				}

				for (VkDeviceSize ndx = 0; ndx < config.mapping.size * atomSize; ndx++)
				{
					const deUint8 val = rng.getUint8();

					mapping[ndx]												= val;
					reference[(size_t)(config.mapping.offset * atomSize + ndx)]	= val;
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
							config.flushMappings[ndx].offset * atomSize,
							config.flushMappings[ndx].size * atomSize
						};

						ranges.push_back(range);
					}

					VK_CHECK(vkd.flushMappedMemoryRanges(device, (deUint32)ranges.size(), &ranges[0]));
				}

				if (config.remap)
				{
					void* ptr;
					vkd.unmapMemory(device, *memory);
					VK_CHECK(vkd.mapMemory(device, *memory, config.mapping.offset * atomSize, config.mapping.size * atomSize, 0u, &ptr));
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
							config.invalidateMappings[ndx].offset * atomSize,
							config.invalidateMappings[ndx].size * atomSize
						};

						ranges.push_back(range);
					}

					VK_CHECK(vkd.invalidateMappedMemoryRanges(device, (deUint32)ranges.size(), &ranges[0]));
				}

				if (!compareAndLogBuffer(log, (size_t)(config.mapping.size * atomSize), mapping, &reference[(size_t)(config.mapping.offset * atomSize)]))
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
										 ReferenceMemory&	reference);

	void				randomRead		(de::Random& rng);
	void				randomWrite		(de::Random& rng);
	void				randomModify	(de::Random& rng);

	const MemoryRange&	getRange		(void) const { return m_range; }

private:
	MemoryRange			m_range;
	void*				m_ptr;
	ReferenceMemory&	m_reference;
};

MemoryMapping::MemoryMapping (const MemoryRange&	range,
							  void*					ptr,
							  ReferenceMemory&		reference)
	: m_range		(range)
	, m_ptr			(ptr)
	, m_reference	(reference)
{
	DE_ASSERT(range.size > 0);
}

void MemoryMapping::randomRead (de::Random& rng)
{
	const size_t count = (size_t)rng.getInt(0, 100);

	for (size_t ndx = 0; ndx < count; ndx++)
	{
		const size_t	pos	= (size_t)(rng.getUint64() % (deUint64)m_range.size);
		const deUint8	val	= ((deUint8*)m_ptr)[pos];

		TCU_CHECK(m_reference.read((size_t)(m_range.offset + pos), val));
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
		m_reference.write((size_t)(m_range.offset + pos), val);
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

		((deUint8*)m_ptr)[pos]	= val ^ mask;
		TCU_CHECK(m_reference.modifyXor((size_t)(m_range.offset + pos), val, mask));
	}
}

VkDeviceSize randomSize (de::Random& rng, VkDeviceSize atomSize, VkDeviceSize maxSize)
{
	const VkDeviceSize maxSizeInAtoms = maxSize / atomSize;

	DE_ASSERT(maxSizeInAtoms > 0);

	return maxSizeInAtoms > 1
			? atomSize * (1 + (VkDeviceSize)(rng.getUint64() % (deUint64)maxSizeInAtoms))
			: atomSize;
}

VkDeviceSize randomOffset (de::Random& rng, VkDeviceSize atomSize, VkDeviceSize maxOffset)
{
	const VkDeviceSize maxOffsetInAtoms = maxOffset / atomSize;

	return maxOffsetInAtoms > 0
			? atomSize * (VkDeviceSize)(rng.getUint64() % (deUint64)(maxOffsetInAtoms + 1))
			: 0;
}

void randomRanges (de::Random& rng, vector<VkMappedMemoryRange>& ranges, size_t count, VkDeviceMemory memory, VkDeviceSize minOffset, VkDeviceSize maxSize, VkDeviceSize atomSize)
{
	ranges.resize(count);

	for (size_t rangeNdx = 0; rangeNdx < count; rangeNdx++)
	{
		const VkDeviceSize	size	= randomSize(rng, atomSize, maxSize);
		const VkDeviceSize	offset	= minOffset + randomOffset(rng, atomSize, maxSize - size);

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
												 deUint32					memoryTypeIndex,
												 VkDeviceSize				atomSize);

							~MemoryObject		(void);

	MemoryMapping*			mapRandom			(const DeviceInterface& vkd, VkDevice device, de::Random& rng);
	void					unmap				(void);

	void					randomFlush			(const DeviceInterface& vkd, VkDevice device, de::Random& rng);
	void					randomInvalidate	(const DeviceInterface& vkd, VkDevice device, de::Random& rng);

	VkDeviceSize			getSize				(void) const { return m_size; }
	MemoryMapping*			getMapping			(void) { return m_mapping; }

private:
	const DeviceInterface&	m_vkd;
	const VkDevice			m_device;

	const deUint32			m_memoryTypeIndex;
	const VkDeviceSize		m_size;
	const VkDeviceSize		m_atomSize;

	Move<VkDeviceMemory>	m_memory;

	MemoryMapping*			m_mapping;
	ReferenceMemory			m_referenceMemory;
};

MemoryObject::MemoryObject (const DeviceInterface&		vkd,
							VkDevice					device,
							VkDeviceSize				size,
							deUint32					memoryTypeIndex,
							VkDeviceSize				atomSize)
	: m_vkd				(vkd)
	, m_device			(device)
	, m_memoryTypeIndex	(memoryTypeIndex)
	, m_size			(size)
	, m_atomSize		(atomSize)
	, m_mapping			(DE_NULL)
	, m_referenceMemory	((size_t)size, (size_t)m_atomSize)
{
	m_memory = allocMemory(m_vkd, m_device, m_size, m_memoryTypeIndex);
}

MemoryObject::~MemoryObject (void)
{
	delete m_mapping;
}

MemoryMapping* MemoryObject::mapRandom (const DeviceInterface& vkd, VkDevice device, de::Random& rng)
{
	const VkDeviceSize	size	= randomSize(rng, m_atomSize, m_size);
	const VkDeviceSize	offset	= randomOffset(rng, m_atomSize, m_size - size);
	void*				ptr;

	DE_ASSERT(!m_mapping);

	VK_CHECK(vkd.mapMemory(device, *m_memory, offset, size, 0u, &ptr));
	TCU_CHECK(ptr);
	m_mapping = new MemoryMapping(MemoryRange(offset, size), ptr, m_referenceMemory);

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

	randomRanges(rng, ranges, rangeCount, *m_memory, m_mapping->getRange().offset, m_mapping->getRange().size, m_atomSize);

	for (size_t rangeNdx = 0; rangeNdx < ranges.size(); rangeNdx++)
		m_referenceMemory.flush((size_t)ranges[rangeNdx].offset, (size_t)ranges[rangeNdx].size);

	VK_CHECK(vkd.flushMappedMemoryRanges(device, (deUint32)ranges.size(), ranges.empty() ? DE_NULL : &ranges[0]));
}

void MemoryObject::randomInvalidate (const DeviceInterface& vkd, VkDevice device, de::Random& rng)
{
	const size_t				rangeCount	= (size_t)rng.getInt(1, 10);
	vector<VkMappedMemoryRange>	ranges		(rangeCount);

	randomRanges(rng, ranges, rangeCount, *m_memory, m_mapping->getRange().offset, m_mapping->getRange().size, m_atomSize);

	for (size_t rangeNdx = 0; rangeNdx < ranges.size(); rangeNdx++)
		m_referenceMemory.invalidate((size_t)ranges[rangeNdx].offset, (size_t)ranges[rangeNdx].size);

	VK_CHECK(vkd.invalidateMappedMemoryRanges(device, (deUint32)ranges.size(), ranges.empty() ? DE_NULL : &ranges[0]));
}

enum
{
	// Use only 1/2 of each memory heap.
	MAX_MEMORY_USAGE_DIV = 2
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

enum MemoryClass
{
	MEMORY_CLASS_SYSTEM = 0,
	MEMORY_CLASS_DEVICE,

	MEMORY_CLASS_LAST
};

// \todo [2016-04-20 pyry] Consider estimating memory fragmentation
class TotalMemoryTracker
{
public:
					TotalMemoryTracker	(void)
	{
		std::fill(DE_ARRAY_BEGIN(m_usage), DE_ARRAY_END(m_usage), 0);
	}

	void			allocate			(MemoryClass memClass, VkDeviceSize size)
	{
		m_usage[memClass] += size;
	}

	void			free				(MemoryClass memClass, VkDeviceSize size)
	{
		DE_ASSERT(size <= m_usage[memClass]);
		m_usage[memClass] -= size;
	}

	VkDeviceSize	getUsage			(MemoryClass memClass) const
	{
		return m_usage[memClass];
	}

	VkDeviceSize	getTotalUsage		(void) const
	{
		VkDeviceSize total = 0;
		for (int ndx = 0; ndx < MEMORY_CLASS_LAST; ++ndx)
			total += getUsage((MemoryClass)ndx);
		return total;
	}

private:
	VkDeviceSize	m_usage[MEMORY_CLASS_LAST];
};

class MemoryHeap
{
public:
	MemoryHeap (const VkMemoryHeap&			heap,
				const vector<MemoryType>&	memoryTypes,
				const PlatformMemoryLimits&	memoryLimits,
				const VkDeviceSize			nonCoherentAtomSize,
				TotalMemoryTracker&			totalMemTracker)
		: m_heap				(heap)
		, m_memoryTypes			(memoryTypes)
		, m_limits				(memoryLimits)
		, m_nonCoherentAtomSize	(nonCoherentAtomSize)
		, m_totalMemTracker		(totalMemTracker)
		, m_usage				(0)
	{
	}

	~MemoryHeap (void)
	{
		for (vector<MemoryObject*>::iterator iter = m_objects.begin(); iter != m_objects.end(); ++iter)
			delete *iter;
	}

	bool								full			(void) const { return getAvailableMem() < m_nonCoherentAtomSize * (1 + REFERENCE_BYTES_PER_BYTE);	}
	bool								empty			(void) const { return m_usage == 0;																	}

	MemoryObject*						allocateRandom	(const DeviceInterface& vkd, VkDevice device, de::Random& rng)
	{
		const VkDeviceSize		availableMem	= getAvailableMem();

		DE_ASSERT(availableMem > 0);

		const MemoryType		type			= rng.choose<MemoryType>(m_memoryTypes.begin(), m_memoryTypes.end());
		const VkDeviceSize		atomSize		= (type.type.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0
												? 1
												: m_nonCoherentAtomSize;
		const VkDeviceSize		size			= randomSize(rng, atomSize, availableMem);

		DE_ASSERT(size <= availableMem);

		MemoryObject* const		object	= new MemoryObject(vkd, device, size, type.index, atomSize);

		m_usage += size;
		m_totalMemTracker.allocate(getMemoryClass(), size);
		m_totalMemTracker.allocate(MEMORY_CLASS_SYSTEM, size * REFERENCE_BYTES_PER_BYTE);
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
		m_totalMemTracker.free(MEMORY_CLASS_SYSTEM, object->getSize() * REFERENCE_BYTES_PER_BYTE);
		m_totalMemTracker.free(getMemoryClass(), object->getSize());
		delete object;
	}

private:
	MemoryClass							getMemoryClass	(void) const
	{
		if ((m_heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0)
			return MEMORY_CLASS_DEVICE;
		else
			return MEMORY_CLASS_SYSTEM;
	}

	VkDeviceSize						getAvailableMem	(void) const
	{
		DE_ASSERT(m_usage <= m_heap.size/MAX_MEMORY_USAGE_DIV);

		const VkDeviceSize	availableInHeap	= m_heap.size/MAX_MEMORY_USAGE_DIV - m_usage;
		const bool			isUMA			= m_limits.totalDeviceLocalMemory == 0;

		if (isUMA)
		{
			const VkDeviceSize	totalUsage	= m_totalMemTracker.getTotalUsage();
			const VkDeviceSize	totalSysMem	= (VkDeviceSize)m_limits.totalSystemMemory;

			DE_ASSERT(totalUsage <= totalSysMem);

			return de::min(availableInHeap, (totalSysMem-totalUsage) / (1 + REFERENCE_BYTES_PER_BYTE));
		}
		else
		{
			const VkDeviceSize	totalUsage		= m_totalMemTracker.getTotalUsage();
			const VkDeviceSize	totalSysMem		= (VkDeviceSize)m_limits.totalSystemMemory;

			const MemoryClass	memClass		= getMemoryClass();
			const VkDeviceSize	totalMemClass	= memClass == MEMORY_CLASS_SYSTEM
												? (VkDeviceSize)(m_limits.totalSystemMemory / (1 + REFERENCE_BYTES_PER_BYTE))
												: m_limits.totalDeviceLocalMemory;
			const VkDeviceSize	usedMemClass	= m_totalMemTracker.getUsage(memClass);

			DE_ASSERT(usedMemClass <= totalMemClass);

			return de::min(de::min(availableInHeap, totalMemClass-usedMemClass), (totalSysMem - totalUsage) / REFERENCE_BYTES_PER_BYTE);
		}
	}

	const VkMemoryHeap			m_heap;
	const vector<MemoryType>	m_memoryTypes;
	const PlatformMemoryLimits&	m_limits;
	const VkDeviceSize			m_nonCoherentAtomSize;
	TotalMemoryTracker&			m_totalMemTracker;

	VkDeviceSize				m_usage;
	vector<MemoryObject*>		m_objects;
};

size_t getMemoryObjectSystemSize (Context& context)
{
	return computeDeviceMemorySystemMemFootprint(context.getDeviceInterface(), context.getDevice())
		   + sizeof(MemoryObject)
		   + sizeof(de::SharedPtr<MemoryObject>);
}

size_t getMemoryMappingSystemSize (void)
{
	return sizeof(MemoryMapping) + sizeof(de::SharedPtr<MemoryMapping>);
}

class RandomMemoryMappingInstance : public TestInstance
{
public:
	RandomMemoryMappingInstance (Context& context, deUint32 seed)
		: TestInstance				(context)
		, m_memoryObjectSysMemSize	(getMemoryObjectSystemSize(context))
		, m_memoryMappingSysMemSize	(getMemoryMappingSystemSize())
		, m_memoryLimits			(getMemoryLimits(context.getTestContext().getPlatform().getVulkanPlatform()))
		, m_rng						(seed)
		, m_opNdx					(0)
	{
		const VkPhysicalDevice					physicalDevice		= context.getPhysicalDevice();
		const InstanceInterface&				vki					= context.getInstanceInterface();
		const VkPhysicalDeviceMemoryProperties	memoryProperties	= getPhysicalDeviceMemoryProperties(vki, physicalDevice);
		// \todo [2016-05-26 misojarvi] Remove zero check once drivers report correctly 1 instead of 0
		const VkDeviceSize						nonCoherentAtomSize	= context.getDeviceProperties().limits.nonCoherentAtomSize != 0
																	? context.getDeviceProperties().limits.nonCoherentAtomSize
																	: 1;

		// Initialize heaps
		{
			vector<vector<MemoryType> >	memoryTypes	(memoryProperties.memoryHeapCount);

			for (deUint32 memoryTypeNdx = 0; memoryTypeNdx < memoryProperties.memoryTypeCount; memoryTypeNdx++)
			{
				if (memoryProperties.memoryTypes[memoryTypeNdx].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
					memoryTypes[memoryProperties.memoryTypes[memoryTypeNdx].heapIndex].push_back(MemoryType(memoryTypeNdx, memoryProperties.memoryTypes[memoryTypeNdx]));
			}

			for (deUint32 heapIndex = 0; heapIndex < memoryProperties.memoryHeapCount; heapIndex++)
			{
				const VkMemoryHeap	heapInfo	= memoryProperties.memoryHeaps[heapIndex];

				if (!memoryTypes[heapIndex].empty())
				{
					const de::SharedPtr<MemoryHeap>	heap	(new MemoryHeap(heapInfo, memoryTypes[heapIndex], m_memoryLimits, nonCoherentAtomSize, m_totalMemTracker));

					TCU_CHECK_INTERNAL(!heap->full());

					m_memoryHeaps.push_back(heap);
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

		const VkDeviceSize		sysMemUsage					= (m_memoryLimits.totalDeviceLocalMemory == 0)
															? m_totalMemTracker.getTotalUsage()
															: m_totalMemTracker.getUsage(MEMORY_CLASS_SYSTEM);

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

			m_totalMemTracker.free(MEMORY_CLASS_SYSTEM, (VkDeviceSize)m_memoryMappingSysMemSize);
		}
		else if (!m_nonMappedMemoryObjects.empty() &&
				 (m_rng.getFloat() < mapProbability) &&
				 (sysMemUsage+m_memoryMappingSysMemSize <= (VkDeviceSize)m_memoryLimits.totalSystemMemory))
		{
			// Map memory object
			MemoryObject* const		object	= m_rng.choose<MemoryObject*>(m_nonMappedMemoryObjects.begin(), m_nonMappedMemoryObjects.end());
			MemoryMapping*			mapping	= object->mapRandom(vkd, device, m_rng);

			m_memoryMappings.push_back(mapping);
			m_mappedMemoryObjects.push_back(object);
			removeFirstEqual(m_nonMappedMemoryObjects, object);

			m_totalMemTracker.allocate(MEMORY_CLASS_SYSTEM, (VkDeviceSize)m_memoryMappingSysMemSize);
		}
		else
		{
			// Sort heaps based on capacity (full or not)
			vector<MemoryHeap*>		nonFullHeaps;
			vector<MemoryHeap*>		nonEmptyHeaps;

			if (sysMemUsage+m_memoryObjectSysMemSize <= (VkDeviceSize)m_memoryLimits.totalSystemMemory)
			{
				// For the duration of sorting reserve MemoryObject space from system memory
				m_totalMemTracker.allocate(MEMORY_CLASS_SYSTEM, (VkDeviceSize)m_memoryObjectSysMemSize);

				for (vector<de::SharedPtr<MemoryHeap> >::const_iterator heapIter = m_memoryHeaps.begin();
					 heapIter != m_memoryHeaps.end();
					 ++heapIter)
				{
					if (!(*heapIter)->full())
						nonFullHeaps.push_back(heapIter->get());

					if (!(*heapIter)->empty())
						nonEmptyHeaps.push_back(heapIter->get());
				}

				m_totalMemTracker.free(MEMORY_CLASS_SYSTEM, (VkDeviceSize)m_memoryObjectSysMemSize);
			}
			else
			{
				// Not possible to even allocate MemoryObject from system memory, look for non-empty heaps
				for (vector<de::SharedPtr<MemoryHeap> >::const_iterator heapIter = m_memoryHeaps.begin();
					 heapIter != m_memoryHeaps.end();
					 ++heapIter)
				{
					if (!(*heapIter)->empty())
						nonEmptyHeaps.push_back(heapIter->get());
				}
			}

			if (!nonFullHeaps.empty() && (nonEmptyHeaps.empty() || m_rng.getFloat() < allocProbability))
			{
				// Reserve MemoryObject from sys mem first
				m_totalMemTracker.allocate(MEMORY_CLASS_SYSTEM, (VkDeviceSize)m_memoryObjectSysMemSize);

				// Allocate more memory objects
				MemoryHeap* const	heap	= m_rng.choose<MemoryHeap*>(nonFullHeaps.begin(), nonFullHeaps.end());
				MemoryObject* const	object	= heap->allocateRandom(vkd, device, m_rng);

				m_nonMappedMemoryObjects.push_back(object);
			}
			else
			{
				// Free memory objects
				MemoryHeap* const		heap	= m_rng.choose<MemoryHeap*>(nonEmptyHeaps.begin(), nonEmptyHeaps.end());
				MemoryObject* const		object	= heap->getRandomObject(m_rng);

				// Remove mapping
				if (object->getMapping())
				{
					removeFirstEqual(m_memoryMappings, object->getMapping());
					m_totalMemTracker.free(MEMORY_CLASS_SYSTEM, m_memoryMappingSysMemSize);
				}

				removeFirstEqual(m_mappedMemoryObjects, object);
				removeFirstEqual(m_nonMappedMemoryObjects, object);

				heap->free(object);
				m_totalMemTracker.free(MEMORY_CLASS_SYSTEM, (VkDeviceSize)m_memoryObjectSysMemSize);
			}
		}

		m_opNdx += 1;
		if (m_opNdx == opCount)
			return tcu::TestStatus::pass("Pass");
		else
			return tcu::TestStatus::incomplete();
	}

private:
	const size_t						m_memoryObjectSysMemSize;
	const size_t						m_memoryMappingSysMemSize;
	const PlatformMemoryLimits			m_memoryLimits;

	de::Random							m_rng;
	size_t								m_opNdx;

	TotalMemoryTracker					m_totalMemTracker;
	vector<de::SharedPtr<MemoryHeap> >	m_memoryHeaps;

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
			config.flushMappings = vector<MemoryRange>(1, MemoryRange(mapping.offset, mapping.size));
			config.invalidateMappings = vector<MemoryRange>(1, MemoryRange(mapping.offset, mapping.size));
			return config;

		case OP_SUB_INVALIDATE:
			DE_ASSERT(mapping.size / 4 > 0);

			config.flushMappings = vector<MemoryRange>(1, MemoryRange(mapping.offset + mapping.size / 4, mapping.size / 2));
			config.invalidateMappings = vector<MemoryRange>(1, MemoryRange(mapping.offset + mapping.size / 4, mapping.size / 2));
			return config;

		case OP_SUB_INVALIDATE_SEPARATE:
			DE_ASSERT(mapping.size / 2 > 0);

			config.flushMappings.push_back(MemoryRange(mapping.offset + mapping.size /  2, mapping.size - (mapping.size / 2)));
			config.flushMappings.push_back(MemoryRange(mapping.offset, mapping.size / 2));

			config.invalidateMappings.push_back(MemoryRange(mapping.offset + mapping.size /  2, mapping.size - (mapping.size / 2)));
			config.invalidateMappings.push_back(MemoryRange(mapping.offset, mapping.size / 2));

			return config;

		case OP_SUB_INVALIDATE_OVERLAPPING:
			DE_ASSERT((mapping.size / 3) > 0);

			config.flushMappings.push_back(MemoryRange(mapping.offset + mapping.size /  3, mapping.size - (mapping.size / 2)));
			config.flushMappings.push_back(MemoryRange(mapping.offset, (2 * mapping.size) / 3));

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
