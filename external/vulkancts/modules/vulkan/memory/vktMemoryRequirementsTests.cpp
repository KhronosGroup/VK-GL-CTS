/*-------------------------------------------------------------------------
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
 * \brief Buffer and image memory requirements tests.
 *//*--------------------------------------------------------------------*/

#include "vktMemoryRequirementsTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deSTLUtil.hpp"

#include "tcuResultCollector.hpp"
#include "tcuTestLog.hpp"

#include <set>

namespace vkt
{
namespace memory
{
namespace
{
using namespace vk;
using de::MovePtr;
using tcu::TestLog;

Move<VkBuffer> makeBuffer (const DeviceInterface& vk, const VkDevice device, const VkDeviceSize size, const VkBufferCreateFlags flags, const VkBufferUsageFlags usage)
{
	const VkBufferCreateInfo createInfo =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType        sType;
		DE_NULL,									// const void*            pNext;
		flags,										// VkBufferCreateFlags    flags;
		size,										// VkDeviceSize           size;
		usage,										// VkBufferUsageFlags     usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode          sharingMode;
		0u,											// uint32_t               queueFamilyIndexCount;
		DE_NULL,									// const uint32_t*        pQueueFamilyIndices;
	};
	return createBuffer(vk, device, &createInfo);
}

VkMemoryRequirements getBufferMemoryRequirements (const DeviceInterface& vk, const VkDevice device, const VkDeviceSize size, const VkBufferCreateFlags flags, const VkBufferUsageFlags usage)
{
	const Unique<VkBuffer> buffer(makeBuffer(vk, device, size, flags, usage));
	return getBufferMemoryRequirements(vk, device, *buffer);
}

VkMemoryRequirements getBufferMemoryRequirements2 (const DeviceInterface& vk, const VkDevice device, const VkDeviceSize size, const VkBufferCreateFlags flags, const VkBufferUsageFlags usage, void* next = DE_NULL)
{
	const Unique<VkBuffer>				buffer		(makeBuffer(vk, device, size, flags, usage));
	VkBufferMemoryRequirementsInfo2		info	=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,		// VkStructureType	sType
		DE_NULL,													// const void*		pNext
		*buffer														// VkBuffer			buffer
	};
	VkMemoryRequirements2				req2	=
	{
		VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,					// VkStructureType		sType
		next,														// void*				pNext
		{0, 0, 0}													// VkMemoryRequirements	memoryRequirements
	};

	vk.getBufferMemoryRequirements2(device, &info, &req2);

	return req2.memoryRequirements;
}

#ifndef CTS_USES_VULKANSC
VkMemoryRequirements getBufferCreateInfoMemoryRequirementsKHR (const DeviceInterface& vk, const VkDevice device, const VkDeviceSize size, const VkBufferCreateFlags flags, const VkBufferUsageFlags usage, void* next = DE_NULL)
{
	const VkBufferCreateInfo createInfo =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType        sType;
		DE_NULL,									// const void*            pNext;
		flags,										// VkBufferCreateFlags    flags;
		size,										// VkDeviceSize           size;
		usage,										// VkBufferUsageFlags     usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode          sharingMode;
		0u,											// uint32_t               queueFamilyIndexCount;
		DE_NULL,									// const uint32_t*        pQueueFamilyIndices;
	};
	const VkDeviceBufferMemoryRequirementsKHR memoryInfo =
	{
		VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS_KHR,
		DE_NULL,
		&createInfo
	};
	VkMemoryRequirements2				req2	=
	{
		VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR,				// VkStructureType		sType
		next,														// void*				pNext
		{0, 0, 0}													// VkMemoryRequirements	memoryRequirements
	};

	vk.getDeviceBufferMemoryRequirements(device, &memoryInfo, &req2);

	return req2.memoryRequirements;
}
#endif // CTS_USES_VULKANSC

VkMemoryRequirements getImageMemoryRequirements2 (const DeviceInterface& vk, const VkDevice device, const VkImageCreateInfo& createInfo, void* next = DE_NULL)
{
	const Unique<VkImage> image(createImage(vk, device, &createInfo));

	VkImageMemoryRequirementsInfo2		info	=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,			// VkStructureType	sType
		DE_NULL,													// const void*		pNext
		*image														// VkImage			image
	};
	VkMemoryRequirements2				req2	=
	{
		VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,					// VkStructureType		sType
		next,														// void*				pNext
		{0, 0, 0}													// VkMemoryRequirements	memoryRequirements
	};

	vk.getImageMemoryRequirements2(device, &info, &req2);

	return req2.memoryRequirements;
}

#ifndef CTS_USES_VULKANSC
VkMemoryRequirements getDeviceImageMemoryRequirements (const DeviceInterface& vk, const VkDevice device, const VkImageCreateInfo& createInfo, void* next = DE_NULL)
{
	VkDeviceImageMemoryRequirementsKHR info =
	{
		VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS_KHR,
		DE_NULL,
		&createInfo,
		VkImageAspectFlagBits(0)
	};
	VkMemoryRequirements2				req2	=
	{
		VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR,				// VkStructureType		sType
		next,														// void*				pNext
		{0, 0, 0}													// VkMemoryRequirements	memoryRequirements
	};

	vk.getDeviceImageMemoryRequirements(device, &info, &req2);

	return req2.memoryRequirements;
}
#endif // CTS_USES_VULKANSC

#ifndef CTS_USES_VULKANSC
std::vector<VkSparseImageMemoryRequirements> getImageCreateInfoSparseMemoryRequirements (const DeviceInterface& vk, VkDevice device, const VkImageCreateInfo& createInfo)
{
	VkDeviceImageMemoryRequirementsKHR info =
	{
		VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS_KHR,
		DE_NULL,
		&createInfo,
		VkImageAspectFlagBits(0)
	};
	deUint32										requirementsCount = 0;
	std::vector<VkSparseImageMemoryRequirements>	requirements;
	std::vector<VkSparseImageMemoryRequirements2>	requirements2;

	vk.getDeviceImageSparseMemoryRequirements(device, &info, &requirementsCount, DE_NULL);

	if (requirementsCount > 0)
	{
		requirements2.resize(requirementsCount);
		for (deUint32 ndx = 0; ndx < requirementsCount; ++ndx)
		{
			requirements2[ndx].sType = VK_STRUCTURE_TYPE_SPARSE_IMAGE_MEMORY_REQUIREMENTS_2;
			requirements2[ndx].pNext = DE_NULL;
		}

		vk.getDeviceImageSparseMemoryRequirements(device, &info, &requirementsCount, &requirements2[0]);

		if ((size_t)requirementsCount != requirements2.size())
			TCU_FAIL("Returned sparse image memory requirements count changes between queries");

		requirements.resize(requirementsCount);
		for (deUint32 ndx = 0; ndx < requirementsCount; ++ndx)
			requirements[ndx] = requirements2[ndx].memoryRequirements;
	}

	return requirements;
}
#endif // CTS_USES_VULKANSC

//! Get an index of each set bit, starting from the least significant bit.
std::vector<deUint32> bitsToIndices (deUint32 bits)
{
	std::vector<deUint32> indices;
	for (deUint32 i = 0u; bits != 0u; ++i, bits >>= 1)
	{
		if (bits & 1u)
			indices.push_back(i);
	}
	return indices;
}

template<typename T>
T nextEnum (T value)
{
	return static_cast<T>(static_cast<deUint32>(value) + 1);
}

template<typename T>
T nextFlag (T value)
{
	if (value)
		return static_cast<T>(static_cast<deUint32>(value) << 1);
	else
		return static_cast<T>(1);
}

template<typename T>
T nextFlagExcluding (T value, T excludedFlags)
{
	deUint32 tmp = static_cast<deUint32>(value);
	while ((tmp = nextFlag(tmp)) & static_cast<deUint32>(excludedFlags));
	return static_cast<T>(tmp);
}

bool validValueVkBool32 (const VkBool32 value)
{
	return (value == VK_FALSE || value == VK_TRUE);
}

class IBufferMemoryRequirements
{
public:
	virtual void populateTestGroup			(tcu::TestCaseGroup*						group) = 0;

protected:
	virtual void addFunctionTestCase		(tcu::TestCaseGroup*						group,
											 const std::string&							name,
											 const std::string&							desc,
											 VkBufferCreateFlags						arg0) = 0;

	virtual tcu::TestStatus execTest		(Context&									context,
											 const VkBufferCreateFlags					bufferFlags) = 0;

	virtual void updateMemoryRequirements	(const DeviceInterface&						vk,
											 const VkDevice								device,
											 const VkDeviceSize							size,
											 const VkBufferCreateFlags					flags,
											 const VkBufferUsageFlags					usage,
											 const bool									all) = 0;

	virtual void verifyMemoryRequirements	(tcu::ResultCollector&						result,
											 const VkPhysicalDeviceMemoryProperties&	deviceMemoryProperties,
											 const VkPhysicalDeviceLimits&				limits,
											 const VkBufferCreateFlags					bufferFlags,
											 const VkBufferUsageFlags					usage) = 0;
};

class BufferMemoryRequirementsOriginal : public IBufferMemoryRequirements
{
	static tcu::TestStatus testEntryPoint	(Context&									context,
											 const VkBufferCreateFlags					bufferFlags);

public:
	virtual void populateTestGroup			(tcu::TestCaseGroup*						group);

protected:
	virtual void addFunctionTestCase		(tcu::TestCaseGroup*						group,
											 const std::string&							name,
											 const std::string&							desc,
											 VkBufferCreateFlags						arg0);

	virtual tcu::TestStatus execTest		(Context&									context,
											 const VkBufferCreateFlags					bufferFlags);

	virtual void updateMemoryRequirements	(const DeviceInterface&						vk,
											 const VkDevice								device,
											 const VkDeviceSize							size,
											 const VkBufferCreateFlags					flags,
											 const VkBufferUsageFlags					usage,
											 const bool									all);

	virtual void verifyMemoryRequirements	(tcu::ResultCollector&						result,
											 const VkPhysicalDeviceMemoryProperties&	deviceMemoryProperties,
											 const VkPhysicalDeviceLimits&				limits,
											 const VkBufferCreateFlags					bufferFlags,
											 const VkBufferUsageFlags					usage);

protected:
	VkMemoryRequirements	m_allUsageFlagsRequirements;
	VkMemoryRequirements	m_currentTestRequirements;
};


tcu::TestStatus BufferMemoryRequirementsOriginal::testEntryPoint (Context& context, const VkBufferCreateFlags bufferFlags)
{
	BufferMemoryRequirementsOriginal test;

	return test.execTest(context, bufferFlags);
}

void BufferMemoryRequirementsOriginal::populateTestGroup (tcu::TestCaseGroup* group)
{
	const struct
	{
		VkBufferCreateFlags		flags;
		const char* const		name;
	} bufferCases[] =
	{
		{ (VkBufferCreateFlags)0,																								"regular"					},
#ifndef CTS_USES_VULKANSC
		{ VK_BUFFER_CREATE_SPARSE_BINDING_BIT,																					"sparse"					},
		{ VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT,											"sparse_residency"			},
		{ VK_BUFFER_CREATE_SPARSE_BINDING_BIT											| VK_BUFFER_CREATE_SPARSE_ALIASED_BIT,	"sparse_aliased"			},
		{ VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT	| VK_BUFFER_CREATE_SPARSE_ALIASED_BIT,	"sparse_residency_aliased"	},
#endif // CTS_USES_VULKANSC
	};

	de::MovePtr<tcu::TestCaseGroup> bufferGroup(new tcu::TestCaseGroup(group->getTestContext(), "buffer", ""));

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(bufferCases); ++ndx)
		addFunctionTestCase(bufferGroup.get(), bufferCases[ndx].name, "", bufferCases[ndx].flags);

	group->addChild(bufferGroup.release());
}

void checkSupportBufferMemoryRequirementsOriginal (Context& context, VkBufferCreateFlags flags)
{
	if (flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_BINDING);

	if (flags & VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_BUFFER);

	if (flags & VK_BUFFER_CREATE_SPARSE_ALIASED_BIT)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_ALIASED);
}

void BufferMemoryRequirementsOriginal::addFunctionTestCase (tcu::TestCaseGroup*	group,
															const std::string&	name,
															const std::string&	desc,
															VkBufferCreateFlags	arg0)
{
	addFunctionCase(group, name, desc, checkSupportBufferMemoryRequirementsOriginal, testEntryPoint, arg0);
}

tcu::TestStatus BufferMemoryRequirementsOriginal::execTest (Context& context, const VkBufferCreateFlags bufferFlags)
{
	const DeviceInterface&					vk			= context.getDeviceInterface();
	const InstanceInterface&				vki			= context.getInstanceInterface();
	const VkDevice							device		= context.getDevice();
	const VkPhysicalDevice					physDevice	= context.getPhysicalDevice();

	const VkPhysicalDeviceMemoryProperties	memoryProperties	= getPhysicalDeviceMemoryProperties(vki, physDevice);
	const VkPhysicalDeviceLimits			limits				= getPhysicalDeviceProperties(vki, physDevice).limits;
	const VkBufferUsageFlags				allUsageFlags		= static_cast<VkBufferUsageFlags>((VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT << 1) - 1);
	tcu::TestLog&							log					= context.getTestContext().getLog();
	bool									allPass				= true;

	const VkDeviceSize sizeCases[] =
	{
		1    * 1024,
		8    * 1024,
		64   * 1024,
		1024 * 1024,
	};

	// Updates m_allUsageFlags* fields
	updateMemoryRequirements(vk, device, 1024, bufferFlags, allUsageFlags, true); // doesn't depend on size

	for (VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT; usage <= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT; usage = nextFlag(usage))
	{
		deUint32		previousMemoryTypeBits	= 0u;
		VkDeviceSize	previousAlignment		= 0u;

		log << tcu::TestLog::Message << "Verify a buffer with usage flags: " << de::toString(getBufferUsageFlagsStr(usage)) << tcu::TestLog::EndMessage;

		for (const VkDeviceSize* pSize = sizeCases; pSize < sizeCases + DE_LENGTH_OF_ARRAY(sizeCases); ++pSize)
		{
			log << tcu::TestLog::Message << "- size " << *pSize << " bytes" << tcu::TestLog::EndMessage;

			tcu::ResultCollector result(log, "ERROR: ");

			// Updates m_allUsageFlags* fields
			updateMemoryRequirements(vk, device, *pSize, bufferFlags, usage, false);

			// Check:
			// - requirements for a particular buffer usage
			// - memoryTypeBits are a subset of bits for requirements with all usage flags combined
			verifyMemoryRequirements(result, memoryProperties, limits, bufferFlags, usage);

			// Check that for the same usage and create flags:
			// - memoryTypeBits are the same
			// - alignment is the same
			if (pSize > sizeCases)
			{
				result.check(m_currentTestRequirements.memoryTypeBits == previousMemoryTypeBits,
					"memoryTypeBits differ from the ones in the previous buffer size");

				result.check(m_currentTestRequirements.alignment == previousAlignment,
					"alignment differs from the one in the previous buffer size");
			}

			if (result.getResult() != QP_TEST_RESULT_PASS)
				allPass = false;

			previousMemoryTypeBits	= m_currentTestRequirements.memoryTypeBits;
			previousAlignment		= m_currentTestRequirements.alignment;
		}

		if (!allPass)
			break;
	}

	return allPass ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail("Some memory requirements were incorrect");
}

void BufferMemoryRequirementsOriginal::updateMemoryRequirements (const DeviceInterface&		vk,
																 const VkDevice				device,
																 const VkDeviceSize			size,
																 const VkBufferCreateFlags	flags,
																 const VkBufferUsageFlags	usage,
																 const bool					all)
{
	if (all)
	{
		m_allUsageFlagsRequirements	= getBufferMemoryRequirements(vk, device, size, flags, usage);
	}
	else
	{
		m_currentTestRequirements	= getBufferMemoryRequirements(vk, device, size, flags, usage);
	}
}

void BufferMemoryRequirementsOriginal::verifyMemoryRequirements (tcu::ResultCollector&						result,
																 const VkPhysicalDeviceMemoryProperties&	deviceMemoryProperties,
																 const VkPhysicalDeviceLimits&				limits,
																 const VkBufferCreateFlags					bufferFlags,
																 const VkBufferUsageFlags					usage)
{
	if (result.check(m_currentTestRequirements.memoryTypeBits != 0, "VkMemoryRequirements memoryTypeBits has no bits set"))
	{
		typedef std::vector<deUint32>::const_iterator	IndexIterator;
		const std::vector<deUint32>						usedMemoryTypeIndices			= bitsToIndices(m_currentTestRequirements.memoryTypeBits);
		bool											deviceLocalMemoryFound			= false;
		bool											hostVisibleCoherentMemoryFound	= false;

		for (IndexIterator memoryTypeNdx = usedMemoryTypeIndices.begin(); memoryTypeNdx != usedMemoryTypeIndices.end(); ++memoryTypeNdx)
		{
			if (*memoryTypeNdx >= deviceMemoryProperties.memoryTypeCount)
			{
				result.fail("VkMemoryRequirements memoryTypeBits contains bits for non-existing memory types");
				continue;
			}

			const VkMemoryPropertyFlags	memoryPropertyFlags = deviceMemoryProperties.memoryTypes[*memoryTypeNdx].propertyFlags;

			if (memoryPropertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
				deviceLocalMemoryFound = true;

			if (memoryPropertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
				hostVisibleCoherentMemoryFound = true;

			result.check((memoryPropertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) == 0u,
				"Memory type includes VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT");
		}

		result.check(deIsPowerOfTwo64(static_cast<deUint64>(m_currentTestRequirements.alignment)) == DE_TRUE,
			"VkMemoryRequirements alignment isn't power of two");

		if (usage & (VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT))
		{
			result.check(m_currentTestRequirements.alignment >= limits.minTexelBufferOffsetAlignment,
				"VkMemoryRequirements alignment doesn't respect minTexelBufferOffsetAlignment");
		}

		if (usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
		{
			result.check(m_currentTestRequirements.alignment >= limits.minUniformBufferOffsetAlignment,
				"VkMemoryRequirements alignment doesn't respect minUniformBufferOffsetAlignment");
		}

		if (usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
		{
			result.check(m_currentTestRequirements.alignment >= limits.minStorageBufferOffsetAlignment,
				"VkMemoryRequirements alignment doesn't respect minStorageBufferOffsetAlignment");
		}

		result.check(deviceLocalMemoryFound,
			"None of the required memory types included VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT");

		result.check((bufferFlags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT) || hostVisibleCoherentMemoryFound,
			"Required memory type doesn't include VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT and VK_MEMORY_PROPERTY_HOST_COHERENT_BIT");

		result.check((m_currentTestRequirements.memoryTypeBits & m_allUsageFlagsRequirements.memoryTypeBits) == m_allUsageFlagsRequirements.memoryTypeBits,
			"Memory type bits aren't a superset of memory type bits for all usage flags combined");
	}
}

class BufferMemoryRequirementsExtended : public BufferMemoryRequirementsOriginal
{
	static tcu::TestStatus testEntryPoint	(Context&					context,
											 const VkBufferCreateFlags	bufferFlags);

protected:
	virtual void addFunctionTestCase		(tcu::TestCaseGroup*		group,
											 const std::string&			name,
											 const std::string&			desc,
											 VkBufferCreateFlags		arg0);

	virtual void updateMemoryRequirements	(const DeviceInterface&		vk,
											 const VkDevice				device,
											 const VkDeviceSize			size,
											 const VkBufferCreateFlags	flags,
											 const VkBufferUsageFlags	usage,
											 const bool					all);
};

tcu::TestStatus BufferMemoryRequirementsExtended::testEntryPoint (Context& context, const VkBufferCreateFlags bufferFlags)
{
	BufferMemoryRequirementsExtended test;

	return test.execTest(context, bufferFlags);
}

void checkSupportBufferMemoryRequirementsExtended (Context& context, VkBufferCreateFlags flags)
{
	checkSupportBufferMemoryRequirementsOriginal(context, flags);

	context.requireDeviceFunctionality("VK_KHR_get_memory_requirements2");
}

void BufferMemoryRequirementsExtended::addFunctionTestCase (tcu::TestCaseGroup*	group,
															const std::string&	name,
															const std::string&	desc,
															VkBufferCreateFlags	arg0)
{
	addFunctionCase(group, name, desc, checkSupportBufferMemoryRequirementsExtended, testEntryPoint, arg0);
}

void BufferMemoryRequirementsExtended::updateMemoryRequirements (const DeviceInterface&		vk,
																 const VkDevice				device,
																 const VkDeviceSize			size,
																 const VkBufferCreateFlags	flags,
																 const VkBufferUsageFlags	usage,
																 const bool					all)
{
	if (all)
	{
		m_allUsageFlagsRequirements	= getBufferMemoryRequirements2(vk, device, size, flags, usage);
	}
	else
	{
		m_currentTestRequirements	= getBufferMemoryRequirements2(vk, device, size, flags, usage);
	}
}


class BufferMemoryRequirementsDedicatedAllocation : public BufferMemoryRequirementsExtended
{
	static tcu::TestStatus testEntryPoint	(Context&									context,
											 const VkBufferCreateFlags					bufferFlags);

protected:
	virtual void addFunctionTestCase		(tcu::TestCaseGroup*						group,
											 const std::string&							name,
											 const std::string&							desc,
											 VkBufferCreateFlags						arg0);

	virtual void updateMemoryRequirements	(const DeviceInterface&						vk,
											 const VkDevice								device,
											 const VkDeviceSize							size,
											 const VkBufferCreateFlags					flags,
											 const VkBufferUsageFlags					usage,
											 const bool									all);

	virtual void verifyMemoryRequirements	(tcu::ResultCollector&						result,
											 const VkPhysicalDeviceMemoryProperties&	deviceMemoryProperties,
											 const VkPhysicalDeviceLimits&				limits,
											 const VkBufferCreateFlags					bufferFlags,
											 const VkBufferUsageFlags					usage);

protected:
	VkBool32	m_allUsageFlagsPrefersDedicatedAllocation;
	VkBool32	m_allUsageFlagsRequiresDedicatedAllocation;

	VkBool32	m_currentTestPrefersDedicatedAllocation;
	VkBool32	m_currentTestRequiresDedicatedAllocation;
};


tcu::TestStatus BufferMemoryRequirementsDedicatedAllocation::testEntryPoint(Context& context, const VkBufferCreateFlags bufferFlags)
{
	BufferMemoryRequirementsDedicatedAllocation test;

	return test.execTest(context, bufferFlags);
}

void checkSupportBufferMemoryRequirementsDedicatedAllocation (Context& context, VkBufferCreateFlags flags)
{
	checkSupportBufferMemoryRequirementsExtended(context, flags);

	context.requireDeviceFunctionality("VK_KHR_dedicated_allocation");
}

void BufferMemoryRequirementsDedicatedAllocation::addFunctionTestCase (tcu::TestCaseGroup*	group,
																	   const std::string&	name,
																	   const std::string&	desc,
																	   VkBufferCreateFlags	arg0)
{
	addFunctionCase(group, name, desc, checkSupportBufferMemoryRequirementsDedicatedAllocation, testEntryPoint, arg0);
}

void BufferMemoryRequirementsDedicatedAllocation::updateMemoryRequirements (const DeviceInterface&		vk,
																			const VkDevice				device,
																			const VkDeviceSize			size,
																			const VkBufferCreateFlags	flags,
																			const VkBufferUsageFlags	usage,
																			const bool					all)
{
	const deUint32						invalidVkBool32			= static_cast<deUint32>(~0);

	VkMemoryDedicatedRequirements	dedicatedRequirements	=
	{
		VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS,		// VkStructureType	sType
		DE_NULL,												// void*			pNext
		invalidVkBool32,										// VkBool32			prefersDedicatedAllocation
		invalidVkBool32											// VkBool32			requiresDedicatedAllocation
	};

	if (all)
	{
		m_allUsageFlagsRequirements					= getBufferMemoryRequirements2(vk, device, size, flags, usage, &dedicatedRequirements);
		m_allUsageFlagsPrefersDedicatedAllocation	= dedicatedRequirements.prefersDedicatedAllocation;
		m_allUsageFlagsRequiresDedicatedAllocation	= dedicatedRequirements.requiresDedicatedAllocation;

		TCU_CHECK(validValueVkBool32(m_allUsageFlagsPrefersDedicatedAllocation));
		// Test design expects m_allUsageFlagsRequiresDedicatedAllocation to be false
		TCU_CHECK(m_allUsageFlagsRequiresDedicatedAllocation == VK_FALSE);
	}
	else
	{
		m_currentTestRequirements					= getBufferMemoryRequirements2(vk, device, size, flags, usage, &dedicatedRequirements);
		m_currentTestPrefersDedicatedAllocation		= dedicatedRequirements.prefersDedicatedAllocation;
		m_currentTestRequiresDedicatedAllocation	= dedicatedRequirements.requiresDedicatedAllocation;
	}
}

void BufferMemoryRequirementsDedicatedAllocation::verifyMemoryRequirements (tcu::ResultCollector&					result,
																			const VkPhysicalDeviceMemoryProperties&	deviceMemoryProperties,
																			const VkPhysicalDeviceLimits&			limits,
																			const VkBufferCreateFlags				bufferFlags,
																			const VkBufferUsageFlags				usage)
{
	BufferMemoryRequirementsExtended::verifyMemoryRequirements(result, deviceMemoryProperties, limits, bufferFlags, usage);

	result.check(validValueVkBool32(m_currentTestPrefersDedicatedAllocation),
		"Invalid VkBool32 value in m_currentTestPrefersDedicatedAllocation");

	result.check(m_currentTestRequiresDedicatedAllocation == VK_FALSE,
		"Regular (non-shared) objects must not require dedicated allocations");
}

#ifndef CTS_USES_VULKANSC
class BufferMemoryRequirementsCreateInfo : public BufferMemoryRequirementsOriginal
{
	static tcu::TestStatus testEntryPoint	(Context&									context,
											 const VkBufferCreateFlags					bufferFlags);

protected:
	virtual void addFunctionTestCase		(tcu::TestCaseGroup*						group,
											 const std::string&							name,
											 const std::string&							desc,
											 VkBufferCreateFlags						arg0);

	virtual void updateMemoryRequirements	(const DeviceInterface&						vk,
											 const VkDevice								device,
											 const VkDeviceSize							size,
											 const VkBufferCreateFlags					flags,
											 const VkBufferUsageFlags					usage,
											 const bool									all);

	virtual void verifyMemoryRequirements	(tcu::ResultCollector&						result,
											 const VkPhysicalDeviceMemoryProperties&	deviceMemoryProperties,
											 const VkPhysicalDeviceLimits&				limits,
											 const VkBufferCreateFlags					bufferFlags,
											 const VkBufferUsageFlags					usage);

protected:
	VkMemoryRequirements	m_currentTestOriginalRequirements;
	VkMemoryRequirements	m_currentTestHalfRequirements;
};


tcu::TestStatus BufferMemoryRequirementsCreateInfo::testEntryPoint(Context& context, const VkBufferCreateFlags bufferFlags)
{
	BufferMemoryRequirementsCreateInfo test;

	return test.execTest(context, bufferFlags);
}

void checkSupportBufferMemoryRequirementsCreateInfo (Context& context, VkBufferCreateFlags flags)
{
	checkSupportBufferMemoryRequirementsExtended(context, flags);

	context.requireDeviceFunctionality("VK_KHR_maintenance4");
}

void BufferMemoryRequirementsCreateInfo::addFunctionTestCase(tcu::TestCaseGroup*	group,
															 const std::string&		name,
															 const std::string&		desc,
															 VkBufferCreateFlags	arg0)
{
	addFunctionCase(group, name, desc, checkSupportBufferMemoryRequirementsCreateInfo, testEntryPoint, arg0);
}

void BufferMemoryRequirementsCreateInfo::updateMemoryRequirements	(const DeviceInterface&		vk,
																	 const VkDevice				device,
																	 const VkDeviceSize			size,
																	 const VkBufferCreateFlags	flags,
																	 const VkBufferUsageFlags	usage,
																	 const bool					all)
{
	if (all)
	{
		m_allUsageFlagsRequirements		= getBufferCreateInfoMemoryRequirementsKHR(vk, device, size, flags, usage);
	}
	else
	{
		m_currentTestRequirements		= getBufferCreateInfoMemoryRequirementsKHR(vk, device, size, flags, usage);
		m_currentTestHalfRequirements	= getBufferCreateInfoMemoryRequirementsKHR(vk, device, size / 2, flags, usage);
	}

	m_currentTestOriginalRequirements = getBufferMemoryRequirements(vk, device, size, flags, usage);
}

void BufferMemoryRequirementsCreateInfo::verifyMemoryRequirements	(tcu::ResultCollector&						result,
																	 const VkPhysicalDeviceMemoryProperties&	deviceMemoryProperties,
																	 const VkPhysicalDeviceLimits&				limits,
																	 const VkBufferCreateFlags					bufferFlags,
																	 const VkBufferUsageFlags					usage)
{
	BufferMemoryRequirementsOriginal::verifyMemoryRequirements(result, deviceMemoryProperties, limits, bufferFlags, usage);

	result.check(m_currentTestRequirements.alignment == m_currentTestOriginalRequirements.alignment,
			"VkMemoryRequirements alignment queried from vkBufferCreateInfo does not match alignment from equivalent object");

	result.check(m_currentTestRequirements.memoryTypeBits == m_currentTestOriginalRequirements.memoryTypeBits,
			"VkMemoryRequirements memoryTypeBits queried from vkBufferCreateInfo does not match memoryTypeBits from equivalent object");

	result.check(m_currentTestRequirements.size == m_currentTestOriginalRequirements.size,
			"VkMemoryRequirements size queried from vkBufferCreateInfo does not match size from equivalent object");

	result.check(m_currentTestRequirements.size >= m_currentTestHalfRequirements.size,
			"VkMemoryRequirements size queried from vkBufferCreateInfo is larger for a smaller buffer");
}
#endif // CTS_USES_VULKANSC

struct ImageTestParams
{
	ImageTestParams (VkImageCreateFlags		flags_,
					 VkImageTiling			tiling_,
					 bool					transient_,
					 bool					useMaint4_ = false)
	: flags		(flags_)
	, tiling	(tiling_)
	, transient	(transient_)
	, useMaint4	(useMaint4_)
	{
	}

	ImageTestParams (void)
	{
	}

	VkImageCreateFlags		flags;
	VkImageTiling			tiling;
	bool					transient;
	bool					useMaint4;
};

class IImageMemoryRequirements
{
public:
	virtual void populateTestGroup			(tcu::TestCaseGroup*						group) = 0;

protected:
	virtual void addFunctionTestCase		(tcu::TestCaseGroup*						group,
											 const std::string&							name,
											 const std::string&							desc,
											 const ImageTestParams						arg0) = 0;

	virtual tcu::TestStatus execTest		(Context&									context,
											 const ImageTestParams						bufferFlags) = 0;

	virtual void updateMemoryRequirements	(const DeviceInterface&						vk,
											 const VkDevice								device) = 0;

	virtual void verifyMemoryRequirements	(tcu::ResultCollector&						result,
											 const VkPhysicalDeviceMemoryProperties&	deviceMemoryProperties) = 0;
};

class ImageMemoryRequirementsOriginal : public IImageMemoryRequirements
{
	static tcu::TestStatus testEntryPoint	(Context&									context,
											 const ImageTestParams						params);

public:
	virtual void populateTestGroup			(tcu::TestCaseGroup*						group);

protected:
	virtual void addFunctionTestCase		(tcu::TestCaseGroup*						group,
											 const std::string&							name,
											 const std::string&							desc,
											 const ImageTestParams						arg0);

	virtual tcu::TestStatus execTest		(Context&									context,
											 const ImageTestParams						params);

	virtual void updateMemoryRequirements	(const DeviceInterface&						vk,
											 const VkDevice								device);

	virtual void verifyMemoryRequirements	(tcu::ResultCollector&						result,
											 const VkPhysicalDeviceMemoryProperties&	deviceMemoryProperties);

private:
	virtual bool isImageSupported			(const Context&								context,
											 const InstanceInterface&					vki,
											 const VkPhysicalDevice						physDevice,
											 const VkImageCreateInfo&					info);

	virtual bool isFormatMatchingAspect		(const VkFormat								format,
											 const VkImageAspectFlags					aspect);

protected:
	VkImageCreateInfo								m_currentTestImageInfo;
	VkMemoryRequirements							m_currentTestRequirements;
#ifndef CTS_USES_VULKANSC
	std::vector<VkSparseImageMemoryRequirements>	m_currentTestSparseRequirements;
#endif // CTS_USES_VULKANSC
};


tcu::TestStatus ImageMemoryRequirementsOriginal::testEntryPoint (Context& context, const ImageTestParams params)
{
	ImageMemoryRequirementsOriginal test;

	return test.execTest(context, params);
}

void ImageMemoryRequirementsOriginal::populateTestGroup (tcu::TestCaseGroup* group)
{
	const struct
	{
		VkImageCreateFlags		flags;
		bool					transient;
		const char* const		name;
	} imageFlagsCases[] =
	{
		{ (VkImageCreateFlags)0,																								false,	"regular"					},
		{ (VkImageCreateFlags)0,																								true,	"transient"					},
#ifndef CTS_USES_VULKANSC
		{ VK_IMAGE_CREATE_SPARSE_BINDING_BIT,																					false,	"sparse"					},
		{ VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT,											false,	"sparse_residency"			},
		{ VK_IMAGE_CREATE_SPARSE_BINDING_BIT											| VK_IMAGE_CREATE_SPARSE_ALIASED_BIT,	false,	"sparse_aliased"			},
		{ VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT		| VK_IMAGE_CREATE_SPARSE_ALIASED_BIT,	false,	"sparse_residency_aliased"	},
#endif // CTS_USES_VULKANSC
	};

	de::MovePtr<tcu::TestCaseGroup> imageGroup(new tcu::TestCaseGroup(group->getTestContext(), "image", ""));

	for (int flagsNdx = 0; flagsNdx < DE_LENGTH_OF_ARRAY(imageFlagsCases); ++flagsNdx)
	for (int tilingNdx = 0; tilingNdx <= 1; ++tilingNdx)
	{
		ImageTestParams		params;
		std::ostringstream	caseName;

		params.flags		=  imageFlagsCases[flagsNdx].flags;
		params.transient	=  imageFlagsCases[flagsNdx].transient;
		params.useMaint4	=  false;
		caseName			<< imageFlagsCases[flagsNdx].name;

		if (tilingNdx != 0)
		{
			params.tiling =  VK_IMAGE_TILING_OPTIMAL;
			caseName      << "_tiling_optimal";
		}
		else
		{
			params.tiling =  VK_IMAGE_TILING_LINEAR;
			caseName      << "_tiling_linear";
		}

		if ((params.flags & VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT) && (params.tiling == VK_IMAGE_TILING_LINEAR))
			continue;

		addFunctionTestCase(imageGroup.get(), caseName.str(), "", params);
	}

	group->addChild(imageGroup.release());
}

void checkSupportImageMemoryRequirementsOriginal (Context& context, ImageTestParams params)
{
	const VkPhysicalDeviceFeatures features = getPhysicalDeviceFeatures(context.getInstanceInterface(), context.getPhysicalDevice());

	if (params.flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_BINDING);

	if (params.flags & VK_BUFFER_CREATE_SPARSE_ALIASED_BIT)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_ALIASED);

	if ((params.flags & VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT) && !(features.sparseResidencyImage2D || features.sparseResidencyImage3D))
		TCU_THROW(NotSupportedError, "Feature not supported: sparseResidencyImage (2D and 3D)");
}

void ImageMemoryRequirementsOriginal::addFunctionTestCase (tcu::TestCaseGroup*		group,
														   const std::string&		name,
														   const std::string&		desc,
														   const ImageTestParams	arg0)
{
	addFunctionCase(group, name, desc, checkSupportImageMemoryRequirementsOriginal, testEntryPoint, arg0);
}

void ImageMemoryRequirementsOriginal::updateMemoryRequirements	(const DeviceInterface&		vk,
																 const VkDevice				device)
{
	const Unique<VkImage> image(createImage(vk, device, &m_currentTestImageInfo));

	m_currentTestRequirements = getImageMemoryRequirements(vk, device, *image);

#ifndef CTS_USES_VULKANSC
	if (m_currentTestImageInfo.flags & VK_IMAGE_CREATE_SPARSE_BINDING_BIT)
		m_currentTestSparseRequirements = getImageSparseMemoryRequirements(vk, device, *image);
#endif // CTS_USES_VULKANSC
}

void ImageMemoryRequirementsOriginal::verifyMemoryRequirements (tcu::ResultCollector&					result,
																const VkPhysicalDeviceMemoryProperties&	deviceMemoryProperties)
{
	if (result.check(m_currentTestRequirements.memoryTypeBits != 0, "VkMemoryRequirements memoryTypeBits has no bits set"))
	{
		typedef std::vector<deUint32>::const_iterator	IndexIterator;
		const std::vector<deUint32>						usedMemoryTypeIndices			= bitsToIndices(m_currentTestRequirements.memoryTypeBits);
		bool											deviceLocalMemoryFound			= false;
		bool											hostVisibleCoherentMemoryFound	= false;

		for (IndexIterator memoryTypeNdx = usedMemoryTypeIndices.begin(); memoryTypeNdx != usedMemoryTypeIndices.end(); ++memoryTypeNdx)
		{
			if (*memoryTypeNdx >= deviceMemoryProperties.memoryTypeCount)
			{
				result.fail("VkMemoryRequirements memoryTypeBits contains bits for non-existing memory types");
				continue;
			}

			const VkMemoryPropertyFlags	memoryPropertyFlags = deviceMemoryProperties.memoryTypes[*memoryTypeNdx].propertyFlags;

			if (memoryPropertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
				deviceLocalMemoryFound = true;

			if (memoryPropertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
				hostVisibleCoherentMemoryFound = true;

			if (memoryPropertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
			{
				result.check((m_currentTestImageInfo.usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) != 0u,
					"Memory type includes VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT for a non-transient attachment image");
			}
		}

		result.check(deIsPowerOfTwo64(static_cast<deUint64>(m_currentTestRequirements.alignment)) == DE_TRUE,
			"VkMemoryRequirements alignment isn't power of two");

		result.check(deviceLocalMemoryFound,
			"None of the required memory types included VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT");

		result.check(m_currentTestImageInfo.tiling == VK_IMAGE_TILING_OPTIMAL || hostVisibleCoherentMemoryFound,
			"Required memory type doesn't include VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT and VK_MEMORY_PROPERTY_HOST_COHERENT_BIT");

#ifndef CTS_USES_VULKANSC
		if (m_currentTestImageInfo.flags & VK_IMAGE_CREATE_SPARSE_BINDING_BIT)
		{
			for (const VkSparseImageMemoryRequirements &sparseRequirements : m_currentTestSparseRequirements)
			{
				result.check((sparseRequirements.imageMipTailSize % m_currentTestRequirements.alignment) == 0,
					"VkSparseImageMemoryRequirements imageMipTailSize is not aligned with sparse block size");
			}
		}
#endif // CTS_USES_VULKANSC
	}
}

bool isUsageMatchesFeatures (const VkImageUsageFlags usage, const VkFormatFeatureFlags featureFlags)
{
	if ((usage & VK_IMAGE_USAGE_SAMPLED_BIT) && (featureFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
		return true;
	if ((usage & VK_IMAGE_USAGE_STORAGE_BIT) && (featureFlags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
		return true;
	if ((usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)) && (featureFlags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
		return true;
	if ((usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) && (featureFlags & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
		return true;

	return false;
}

bool doesUsageSatisfyFeatures (const VkImageUsageFlags usage, const VkFormatFeatureFlags featureFlags)
{
	bool ans = true;

	if (usage & VK_IMAGE_USAGE_SAMPLED_BIT)
		ans &= ((featureFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0);
	if (usage & VK_IMAGE_USAGE_STORAGE_BIT)
		ans &= ((featureFlags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0);
	if (usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT))
		ans &= ((featureFlags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0);
	if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		ans &= ((featureFlags & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0);

	return ans;
}

//! This catches both invalid as well as legal but unsupported combinations of image parameters
bool ImageMemoryRequirementsOriginal::isImageSupported (const Context& context, const InstanceInterface& vki, const VkPhysicalDevice physDevice, const VkImageCreateInfo& info)
{
	DE_ASSERT(info.extent.width >= 1u && info.extent.height >= 1u && info.extent.depth >= 1u);

#ifndef CTS_USES_VULKANSC
	if (!context.isDeviceFunctionalitySupported("VK_KHR_maintenance5"))
	{
		if (info.format == VK_FORMAT_A8_UNORM_KHR || info.format == VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR)
			return false;
	}
#endif // CTS_USES_VULKANSC

	if (isYCbCrFormat(info.format)
		&& (info.imageType != VK_IMAGE_TYPE_2D
			|| info.mipLevels != 1
			|| info.arrayLayers != 1
			|| info.samples != VK_SAMPLE_COUNT_1_BIT
			|| !context.isDeviceFunctionalitySupported("VK_KHR_sampler_ycbcr_conversion")))
	{
		return false;
	}

	if (info.imageType == VK_IMAGE_TYPE_1D)
	{
		DE_ASSERT(info.extent.height == 1u && info.extent.depth == 1u);
	}
	else if (info.imageType == VK_IMAGE_TYPE_2D)
	{
		DE_ASSERT(info.extent.depth == 1u);

		if (info.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)
		{
			DE_ASSERT(info.extent.width == info.extent.height);
			DE_ASSERT(info.arrayLayers >= 6u && (info.arrayLayers % 6u) == 0u);
		}
	}

	if ((info.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) && info.imageType != VK_IMAGE_TYPE_2D)
		return false;

	if ((info.samples != VK_SAMPLE_COUNT_1_BIT) &&
		(info.imageType != VK_IMAGE_TYPE_2D || (info.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) || info.tiling != VK_IMAGE_TILING_OPTIMAL || info.mipLevels > 1u))
		return false;

	if ((info.usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) &&
		(info.usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)) == 0u)
		return false;

	const VkPhysicalDeviceFeatures features = getPhysicalDeviceFeatures(vki, physDevice);

	if (info.flags & VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT)
	{
		DE_ASSERT(info.tiling == VK_IMAGE_TILING_OPTIMAL);

		if (info.imageType == VK_IMAGE_TYPE_1D)
			return false;

		if (info.imageType == VK_IMAGE_TYPE_2D && !features.sparseResidencyImage2D)
			return false;
		if (info.imageType == VK_IMAGE_TYPE_3D && !features.sparseResidencyImage3D)
			return false;
		if (info.samples == VK_SAMPLE_COUNT_2_BIT && !features.sparseResidency2Samples)
			return false;
		if (info.samples == VK_SAMPLE_COUNT_4_BIT && !features.sparseResidency4Samples)
			return false;
		if (info.samples == VK_SAMPLE_COUNT_8_BIT && !features.sparseResidency8Samples)
			return false;
		if (info.samples == VK_SAMPLE_COUNT_16_BIT && !features.sparseResidency16Samples)
			return false;
		if (info.samples == VK_SAMPLE_COUNT_32_BIT || info.samples == VK_SAMPLE_COUNT_64_BIT)
			return false;
	}

	if (info.samples != VK_SAMPLE_COUNT_1_BIT && (info.usage & VK_IMAGE_USAGE_STORAGE_BIT) && !features.shaderStorageImageMultisample)
		return false;

	switch (info.format)
	{
		case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
		case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
		case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
		case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
		case VK_FORMAT_BC2_UNORM_BLOCK:
		case VK_FORMAT_BC2_SRGB_BLOCK:
		case VK_FORMAT_BC3_UNORM_BLOCK:
		case VK_FORMAT_BC3_SRGB_BLOCK:
		case VK_FORMAT_BC4_UNORM_BLOCK:
		case VK_FORMAT_BC4_SNORM_BLOCK:
		case VK_FORMAT_BC5_UNORM_BLOCK:
		case VK_FORMAT_BC5_SNORM_BLOCK:
		case VK_FORMAT_BC6H_UFLOAT_BLOCK:
		case VK_FORMAT_BC6H_SFLOAT_BLOCK:
		case VK_FORMAT_BC7_UNORM_BLOCK:
		case VK_FORMAT_BC7_SRGB_BLOCK:
			if (!features.textureCompressionBC)
				return false;
			break;

		case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
		case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
		case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
		case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
		case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
		case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
		case VK_FORMAT_EAC_R11_UNORM_BLOCK:
		case VK_FORMAT_EAC_R11_SNORM_BLOCK:
		case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
		case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
			if (!features.textureCompressionETC2)
				return false;
			break;

		case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
		case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
		case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
		case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
		case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
		case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
		case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
		case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
		case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
		case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
		case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
		case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
		case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
		case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
		case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
		case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
		case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
		case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
		case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
		case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
		case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
		case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
		case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
		case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
		case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
		case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
		case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
		case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
			if (!features.textureCompressionASTC_LDR)
				return false;
			break;

		default:
			break;
	}

	const VkFormatProperties	formatProperties	= getPhysicalDeviceFormatProperties(vki, physDevice, info.format);
	const VkFormatFeatureFlags	formatFeatures		= (info.tiling == VK_IMAGE_TILING_LINEAR ? formatProperties.linearTilingFeatures
																							 : formatProperties.optimalTilingFeatures);

	if (!isUsageMatchesFeatures(info.usage, formatFeatures))
		return false;

	VkImageFormatProperties		imageFormatProperties;
	const VkResult				result				= vki.getPhysicalDeviceImageFormatProperties(
														physDevice, info.format, info.imageType, info.tiling, info.usage, info.flags, &imageFormatProperties);

	if (result == VK_SUCCESS)
	{
		if (info.arrayLayers > imageFormatProperties.maxArrayLayers)
			return false;
		if (info.mipLevels > imageFormatProperties.maxMipLevels)
			return false;
		if ((info.samples & imageFormatProperties.sampleCounts) == 0u)
			return false;
	}

#ifndef CTS_USES_VULKANSC
	if ((info.flags & VK_IMAGE_CREATE_SPARSE_BINDING_BIT) && !checkSparseImageFormatSupport(physDevice, vki, info))
		return false;
#endif // CTS_USES_VULKANSC

	return result == VK_SUCCESS;
}

VkExtent3D makeExtentForImage (const VkImageType imageType)
{
	VkExtent3D extent = { 64u, 64u, 4u };

	if (imageType == VK_IMAGE_TYPE_1D)
		extent.height = extent.depth = 1u;
	else if (imageType == VK_IMAGE_TYPE_2D)
		extent.depth = 1u;

	return extent;
}

#ifndef CTS_USES_VULKANSC
VkExtent3D halfExtentForImage (const VkImageType imageType, VkExtent3D extent)
{
	VkExtent3D halfExtent = extent;

	if (imageType == VK_IMAGE_TYPE_1D)
		halfExtent.width /= 2;
	else if (imageType == VK_IMAGE_TYPE_2D)
		halfExtent.height /= 2;
	else
		halfExtent.depth /= 2;

	return halfExtent;
}
#endif // CTS_USES_VULKANSC

bool ImageMemoryRequirementsOriginal::isFormatMatchingAspect (const VkFormat format, const VkImageAspectFlags aspect)
{
	DE_ASSERT(aspect == VK_IMAGE_ASPECT_COLOR_BIT || aspect == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT));

	// D/S formats are laid out next to each other in the enum
	const bool isDepthStencilFormat = (format >= VK_FORMAT_D16_UNORM && format <= VK_FORMAT_D32_SFLOAT_S8_UINT);

	return (aspect == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) == isDepthStencilFormat;
}

std::string getImageInfoString (const VkImageCreateInfo& imageInfo)
{
	std::ostringstream str;

	switch (imageInfo.imageType)
	{
		case VK_IMAGE_TYPE_1D:			str << "1D "; break;
		case VK_IMAGE_TYPE_2D:			str << "2D "; break;
		case VK_IMAGE_TYPE_3D:			str << "3D "; break;
		default:						break;
	}

	switch (imageInfo.tiling)
	{
		case VK_IMAGE_TILING_OPTIMAL:	str << "(optimal) "; break;
		case VK_IMAGE_TILING_LINEAR:	str << "(linear) "; break;
		default:						break;
	}

	str << "extent:[" << imageInfo.extent.width << ", " << imageInfo.extent.height << ", " << imageInfo.extent.depth << "] ";
	str << imageInfo.format << " ";
	str << "samples:" << static_cast<deUint32>(imageInfo.samples) << " ";
	str << "flags:" << static_cast<deUint32>(imageInfo.flags) << " ";
	str << "usage:" << static_cast<deUint32>(imageInfo.usage) << " ";

	return str.str();
}

tcu::TestStatus ImageMemoryRequirementsOriginal::execTest (Context& context, const ImageTestParams params)
{
	const VkFormat				formats[]		=
	{
		VK_FORMAT_R4G4_UNORM_PACK8,
		VK_FORMAT_R4G4B4A4_UNORM_PACK16,
		VK_FORMAT_B4G4R4A4_UNORM_PACK16,
		VK_FORMAT_R5G6B5_UNORM_PACK16,
		VK_FORMAT_B5G6R5_UNORM_PACK16,
		VK_FORMAT_R5G5B5A1_UNORM_PACK16,
		VK_FORMAT_B5G5R5A1_UNORM_PACK16,
		VK_FORMAT_A1R5G5B5_UNORM_PACK16,
#ifndef CTS_USES_VULKANSC
		VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR,
#endif // CTS_USES_VULKANSC
		VK_FORMAT_R8_UNORM,
		VK_FORMAT_R8_SNORM,
		VK_FORMAT_R8_USCALED,
		VK_FORMAT_R8_SSCALED,
		VK_FORMAT_R8_UINT,
		VK_FORMAT_R8_SINT,
		VK_FORMAT_R8_SRGB,
#ifndef CTS_USES_VULKANSC
		VK_FORMAT_A8_UNORM_KHR,
#endif // CTS_USES_VULKANSC
		VK_FORMAT_R8G8_UNORM,
		VK_FORMAT_R8G8_SNORM,
		VK_FORMAT_R8G8_USCALED,
		VK_FORMAT_R8G8_SSCALED,
		VK_FORMAT_R8G8_UINT,
		VK_FORMAT_R8G8_SINT,
		VK_FORMAT_R8G8_SRGB,
		VK_FORMAT_R8G8B8_UNORM,
		VK_FORMAT_R8G8B8_SNORM,
		VK_FORMAT_R8G8B8_USCALED,
		VK_FORMAT_R8G8B8_SSCALED,
		VK_FORMAT_R8G8B8_UINT,
		VK_FORMAT_R8G8B8_SINT,
		VK_FORMAT_R8G8B8_SRGB,
		VK_FORMAT_B8G8R8_UNORM,
		VK_FORMAT_B8G8R8_SNORM,
		VK_FORMAT_B8G8R8_USCALED,
		VK_FORMAT_B8G8R8_SSCALED,
		VK_FORMAT_B8G8R8_UINT,
		VK_FORMAT_B8G8R8_SINT,
		VK_FORMAT_B8G8R8_SRGB,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R8G8B8A8_USCALED,
		VK_FORMAT_R8G8B8A8_SSCALED,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8B8A8_SINT,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_B8G8R8A8_SNORM,
		VK_FORMAT_B8G8R8A8_USCALED,
		VK_FORMAT_B8G8R8A8_SSCALED,
		VK_FORMAT_B8G8R8A8_UINT,
		VK_FORMAT_B8G8R8A8_SINT,
		VK_FORMAT_B8G8R8A8_SRGB,
		VK_FORMAT_A8B8G8R8_UNORM_PACK32,
		VK_FORMAT_A8B8G8R8_SNORM_PACK32,
		VK_FORMAT_A8B8G8R8_USCALED_PACK32,
		VK_FORMAT_A8B8G8R8_SSCALED_PACK32,
		VK_FORMAT_A8B8G8R8_UINT_PACK32,
		VK_FORMAT_A8B8G8R8_SINT_PACK32,
		VK_FORMAT_A8B8G8R8_SRGB_PACK32,
		VK_FORMAT_A2R10G10B10_UNORM_PACK32,
		VK_FORMAT_A2R10G10B10_SNORM_PACK32,
		VK_FORMAT_A2R10G10B10_USCALED_PACK32,
		VK_FORMAT_A2R10G10B10_SSCALED_PACK32,
		VK_FORMAT_A2R10G10B10_UINT_PACK32,
		VK_FORMAT_A2R10G10B10_SINT_PACK32,
		VK_FORMAT_A2B10G10R10_UNORM_PACK32,
		VK_FORMAT_A2B10G10R10_SNORM_PACK32,
		VK_FORMAT_A2B10G10R10_USCALED_PACK32,
		VK_FORMAT_A2B10G10R10_SSCALED_PACK32,
		VK_FORMAT_A2B10G10R10_UINT_PACK32,
		VK_FORMAT_A2B10G10R10_SINT_PACK32,
		VK_FORMAT_R16_UNORM,
		VK_FORMAT_R16_SNORM,
		VK_FORMAT_R16_USCALED,
		VK_FORMAT_R16_SSCALED,
		VK_FORMAT_R16_UINT,
		VK_FORMAT_R16_SINT,
		VK_FORMAT_R16_SFLOAT,
		VK_FORMAT_R16G16_UNORM,
		VK_FORMAT_R16G16_SNORM,
		VK_FORMAT_R16G16_USCALED,
		VK_FORMAT_R16G16_SSCALED,
		VK_FORMAT_R16G16_UINT,
		VK_FORMAT_R16G16_SINT,
		VK_FORMAT_R16G16_SFLOAT,
		VK_FORMAT_R16G16B16_UNORM,
		VK_FORMAT_R16G16B16_SNORM,
		VK_FORMAT_R16G16B16_USCALED,
		VK_FORMAT_R16G16B16_SSCALED,
		VK_FORMAT_R16G16B16_UINT,
		VK_FORMAT_R16G16B16_SINT,
		VK_FORMAT_R16G16B16_SFLOAT,
		VK_FORMAT_R16G16B16A16_UNORM,
		VK_FORMAT_R16G16B16A16_SNORM,
		VK_FORMAT_R16G16B16A16_USCALED,
		VK_FORMAT_R16G16B16A16_SSCALED,
		VK_FORMAT_R16G16B16A16_UINT,
		VK_FORMAT_R16G16B16A16_SINT,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R32_SINT,
		VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R32G32_UINT,
		VK_FORMAT_R32G32_SINT,
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32_UINT,
		VK_FORMAT_R32G32B32_SINT,
		VK_FORMAT_R32G32B32_SFLOAT,
		VK_FORMAT_R32G32B32A32_UINT,
		VK_FORMAT_R32G32B32A32_SINT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_FORMAT_R64_UINT,
		VK_FORMAT_R64_SINT,
		VK_FORMAT_R64_SFLOAT,
		VK_FORMAT_R64G64_UINT,
		VK_FORMAT_R64G64_SINT,
		VK_FORMAT_R64G64_SFLOAT,
		VK_FORMAT_R64G64B64_UINT,
		VK_FORMAT_R64G64B64_SINT,
		VK_FORMAT_R64G64B64_SFLOAT,
		VK_FORMAT_R64G64B64A64_UINT,
		VK_FORMAT_R64G64B64A64_SINT,
		VK_FORMAT_R64G64B64A64_SFLOAT,
		VK_FORMAT_B10G11R11_UFLOAT_PACK32,
		VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
		VK_FORMAT_D16_UNORM,
		VK_FORMAT_X8_D24_UNORM_PACK32,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_S8_UINT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_BC1_RGB_UNORM_BLOCK,
		VK_FORMAT_BC1_RGB_SRGB_BLOCK,
		VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
		VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
		VK_FORMAT_BC2_UNORM_BLOCK,
		VK_FORMAT_BC2_SRGB_BLOCK,
		VK_FORMAT_BC3_UNORM_BLOCK,
		VK_FORMAT_BC3_SRGB_BLOCK,
		VK_FORMAT_BC4_UNORM_BLOCK,
		VK_FORMAT_BC4_SNORM_BLOCK,
		VK_FORMAT_BC5_UNORM_BLOCK,
		VK_FORMAT_BC5_SNORM_BLOCK,
		VK_FORMAT_BC6H_UFLOAT_BLOCK,
		VK_FORMAT_BC6H_SFLOAT_BLOCK,
		VK_FORMAT_BC7_UNORM_BLOCK,
		VK_FORMAT_BC7_SRGB_BLOCK,
		VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,
		VK_FORMAT_EAC_R11_UNORM_BLOCK,
		VK_FORMAT_EAC_R11_SNORM_BLOCK,
		VK_FORMAT_EAC_R11G11_UNORM_BLOCK,
		VK_FORMAT_EAC_R11G11_SNORM_BLOCK,
		VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
		VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
		VK_FORMAT_ASTC_5x4_UNORM_BLOCK,
		VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
		VK_FORMAT_ASTC_5x5_UNORM_BLOCK,
		VK_FORMAT_ASTC_5x5_SRGB_BLOCK,
		VK_FORMAT_ASTC_6x5_UNORM_BLOCK,
		VK_FORMAT_ASTC_6x5_SRGB_BLOCK,
		VK_FORMAT_ASTC_6x6_UNORM_BLOCK,
		VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
		VK_FORMAT_ASTC_8x5_UNORM_BLOCK,
		VK_FORMAT_ASTC_8x5_SRGB_BLOCK,
		VK_FORMAT_ASTC_8x6_UNORM_BLOCK,
		VK_FORMAT_ASTC_8x6_SRGB_BLOCK,
		VK_FORMAT_ASTC_8x8_UNORM_BLOCK,
		VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
		VK_FORMAT_ASTC_10x5_UNORM_BLOCK,
		VK_FORMAT_ASTC_10x5_SRGB_BLOCK,
		VK_FORMAT_ASTC_10x6_UNORM_BLOCK,
		VK_FORMAT_ASTC_10x6_SRGB_BLOCK,
		VK_FORMAT_ASTC_10x8_UNORM_BLOCK,
		VK_FORMAT_ASTC_10x8_SRGB_BLOCK,
		VK_FORMAT_ASTC_10x10_UNORM_BLOCK,
		VK_FORMAT_ASTC_10x10_SRGB_BLOCK,
		VK_FORMAT_ASTC_12x10_UNORM_BLOCK,
		VK_FORMAT_ASTC_12x10_SRGB_BLOCK,
		VK_FORMAT_ASTC_12x12_UNORM_BLOCK,
		VK_FORMAT_ASTC_12x12_SRGB_BLOCK,
		VK_FORMAT_G8B8G8R8_422_UNORM,
		VK_FORMAT_B8G8R8G8_422_UNORM,
		VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
		VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
		VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
		VK_FORMAT_G8_B8R8_2PLANE_422_UNORM,
		VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM,
		VK_FORMAT_R10X6_UNORM_PACK16,
		VK_FORMAT_R10X6G10X6_UNORM_2PACK16,
		VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16,
		VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16,
		VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16,
		VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16,
		VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
		VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16,
		VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16,
		VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16,
		VK_FORMAT_R12X4_UNORM_PACK16,
		VK_FORMAT_R12X4G12X4_UNORM_2PACK16,
		VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16,
		VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16,
		VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16,
		VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16,
		VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16,
		VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16,
		VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16,
		VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16,
		VK_FORMAT_G16B16G16R16_422_UNORM,
		VK_FORMAT_B16G16R16G16_422_UNORM,
		VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM,
		VK_FORMAT_G16_B16R16_2PLANE_420_UNORM,
		VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM,
		VK_FORMAT_G16_B16R16_2PLANE_422_UNORM,
		VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM,
		VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT,
		VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT,
		VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT,
		VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT,
		VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT,
		VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT,
	};
	const DeviceInterface&		vk				= context.getDeviceInterface();
	const InstanceInterface&	vki				= context.getInstanceInterface();
	const VkDevice				device			= context.getDevice();
	const VkPhysicalDevice		physDevice		= context.getPhysicalDevice();
	const VkImageCreateFlags	sparseFlags		= VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT | VK_IMAGE_CREATE_SPARSE_ALIASED_BIT;
	const VkImageUsageFlags		transientFlags	= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

	const VkPhysicalDeviceMemoryProperties	memoryProperties		= getPhysicalDeviceMemoryProperties(vki, physDevice);
	const deUint32							notInitializedBits		= ~0u;
	const VkImageAspectFlags				colorAspect				= VK_IMAGE_ASPECT_COLOR_BIT;
	const VkImageAspectFlags				depthStencilAspect		= VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	const VkImageAspectFlags				allAspects[2]			= { colorAspect, depthStencilAspect };
	tcu::TestLog&							log						= context.getTestContext().getLog();
	bool									allPass					= true;
	deUint32								numCheckedImages		= 0u;

	log << tcu::TestLog::Message << "Verify memory requirements for the following parameter combinations:" << tcu::TestLog::EndMessage;

	for (deUint32 loopAspectNdx = 0u; loopAspectNdx < DE_LENGTH_OF_ARRAY(allAspects); ++loopAspectNdx)
	{
		const VkImageAspectFlags	aspect					= allAspects[loopAspectNdx];
		deUint32					previousMemoryTypeBits	= notInitializedBits;

		for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
		{
			const VkFormat format = formats[formatNdx];

			if  (isFormatMatchingAspect(format, aspect))
			{
				// memoryTypeBits may differ between depth/stencil formats
				if (aspect == depthStencilAspect)
					previousMemoryTypeBits = notInitializedBits;

				for (VkImageType			loopImageType	= VK_IMAGE_TYPE_1D;					loopImageType	!= VK_IMAGE_TYPE_LAST;					loopImageType	= nextEnum(loopImageType))
				for (VkImageCreateFlags		loopCreateFlags	= (VkImageCreateFlags)0;			loopCreateFlags	<= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;	loopCreateFlags	= nextFlagExcluding(loopCreateFlags, sparseFlags))
				for (VkImageUsageFlags		loopUsageFlags	= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;	loopUsageFlags	<= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;	loopUsageFlags	= nextFlagExcluding(loopUsageFlags, transientFlags))
				for (VkSampleCountFlagBits	loopSampleCount	= VK_SAMPLE_COUNT_1_BIT;			loopSampleCount	<= VK_SAMPLE_COUNT_16_BIT;				loopSampleCount	= nextFlag(loopSampleCount))
				{
					const VkImageCreateFlags	actualCreateFlags	= loopCreateFlags | params.flags;
					const VkImageUsageFlags		actualUsageFlags	= loopUsageFlags  | (params.transient ? VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT : (VkImageUsageFlagBits)0);
					const bool					isCube				= (actualCreateFlags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) != 0u;
					const VkImageCreateInfo		imageInfo			=
					{
						VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		// VkStructureType          sType;
						DE_NULL,									// const void*              pNext;
						actualCreateFlags,							// VkImageCreateFlags       flags;
						loopImageType,								// VkImageType              imageType;
						format,									// VkFormat                 format;
						makeExtentForImage(loopImageType),			// VkExtent3D               extent;
						1u,											// uint32_t                 mipLevels;
						(isCube ? 6u : 1u),							// uint32_t                 arrayLayers;
						loopSampleCount,							// VkSampleCountFlagBits    samples;
						params.tiling,								// VkImageTiling            tiling;
						actualUsageFlags,							// VkImageUsageFlags        usage;
						VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode            sharingMode;
						0u,											// uint32_t                 queueFamilyIndexCount;
						DE_NULL,									// const uint32_t*          pQueueFamilyIndices;
						VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout            initialLayout;
					};

					m_currentTestImageInfo = imageInfo;

					if (!isImageSupported(context, vki, physDevice, m_currentTestImageInfo))
						continue;

					log << tcu::TestLog::Message << "- " << getImageInfoString(m_currentTestImageInfo) << tcu::TestLog::EndMessage;
					++numCheckedImages;

					tcu::ResultCollector result(log, "ERROR: ");

					updateMemoryRequirements(vk, device);

					verifyMemoryRequirements(result, memoryProperties);

					// For the same tiling, transient usage, and sparse flags, (and format, if D/S) memoryTypeBits must be the same for all images
					result.check((previousMemoryTypeBits == notInitializedBits) || (m_currentTestRequirements.memoryTypeBits == previousMemoryTypeBits),
									"memoryTypeBits differ from the ones in the previous image configuration");

					if (result.getResult() != QP_TEST_RESULT_PASS)
						allPass = false;

					previousMemoryTypeBits = m_currentTestRequirements.memoryTypeBits;
				}
			}
		}
	}

	if (numCheckedImages == 0u)
		log << tcu::TestLog::Message << "NOTE: No supported image configurations -- nothing to check" << tcu::TestLog::EndMessage;

	return allPass ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail("Some memory requirements were incorrect");
}


class ImageMemoryRequirementsExtended : public ImageMemoryRequirementsOriginal
{
public:
	static tcu::TestStatus testEntryPoint	(Context&									context,
											 const ImageTestParams						params);

protected:
	virtual void addFunctionTestCase		(tcu::TestCaseGroup*						group,
											 const std::string&							name,
											 const std::string&							desc,
											 const ImageTestParams						arg0);

	virtual void updateMemoryRequirements	(const DeviceInterface&						vk,
											 const VkDevice								device);
};


tcu::TestStatus ImageMemoryRequirementsExtended::testEntryPoint (Context& context, const ImageTestParams params)
{
	ImageMemoryRequirementsExtended test;

	return test.execTest(context, params);
}

void checkSupportImageMemoryRequirementsExtended (Context& context, ImageTestParams params)
{
	checkSupportImageMemoryRequirementsOriginal(context, params);

	context.requireDeviceFunctionality("VK_KHR_get_memory_requirements2");
}

void ImageMemoryRequirementsExtended::addFunctionTestCase (tcu::TestCaseGroup*		group,
														   const std::string&		name,
														   const std::string&		desc,
														   const ImageTestParams	arg0)
{
	addFunctionCase(group, name, desc, checkSupportImageMemoryRequirementsExtended, testEntryPoint, arg0);
}

void ImageMemoryRequirementsExtended::updateMemoryRequirements (const DeviceInterface&		vk,
															    const VkDevice				device)
{
	m_currentTestRequirements = getImageMemoryRequirements2(vk, device, m_currentTestImageInfo);
}


class ImageMemoryRequirementsDedicatedAllocation : public ImageMemoryRequirementsExtended
{
public:
	static tcu::TestStatus testEntryPoint	(Context&									context,
											 const ImageTestParams						params);

protected:
	virtual void addFunctionTestCase		(tcu::TestCaseGroup*						group,
											 const std::string&							name,
											 const std::string&							desc,
											 const ImageTestParams						arg0);

	virtual void updateMemoryRequirements	(const DeviceInterface&						vk,
											 const VkDevice								device);

	virtual void verifyMemoryRequirements	(tcu::ResultCollector&						result,
											 const VkPhysicalDeviceMemoryProperties&	deviceMemoryProperties);

protected:
	VkBool32	m_currentTestPrefersDedicatedAllocation;
	VkBool32	m_currentTestRequiresDedicatedAllocation;
};


tcu::TestStatus ImageMemoryRequirementsDedicatedAllocation::testEntryPoint (Context& context, const ImageTestParams params)
{
	ImageMemoryRequirementsDedicatedAllocation test;

	return test.execTest(context, params);
}

void checkSupportImageMemoryRequirementsDedicatedAllocation (Context& context, ImageTestParams params)
{
	checkSupportImageMemoryRequirementsExtended(context, params);

	context.requireDeviceFunctionality("VK_KHR_dedicated_allocation");
}

void ImageMemoryRequirementsDedicatedAllocation::addFunctionTestCase (tcu::TestCaseGroup*		group,
																	  const std::string&		name,
																	  const std::string&		desc,
																	  const ImageTestParams		arg0)
{
	addFunctionCase(group, name, desc, checkSupportImageMemoryRequirementsDedicatedAllocation, testEntryPoint, arg0);
}

void ImageMemoryRequirementsDedicatedAllocation::updateMemoryRequirements (const DeviceInterface&	vk,
																		   const VkDevice			device)
{
	const deUint32						invalidVkBool32			= static_cast<deUint32>(~0);

	VkMemoryDedicatedRequirements	dedicatedRequirements	=
	{
		VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS,		// VkStructureType	sType
		DE_NULL,												// void*			pNext
		invalidVkBool32,										// VkBool32			prefersDedicatedAllocation
		invalidVkBool32											// VkBool32			requiresDedicatedAllocation
	};

	m_currentTestRequirements					= getImageMemoryRequirements2(vk, device, m_currentTestImageInfo, &dedicatedRequirements);
	m_currentTestPrefersDedicatedAllocation		= dedicatedRequirements.prefersDedicatedAllocation;
	m_currentTestRequiresDedicatedAllocation	= dedicatedRequirements.requiresDedicatedAllocation;
}

void ImageMemoryRequirementsDedicatedAllocation::verifyMemoryRequirements (tcu::ResultCollector&						result,
																		   const VkPhysicalDeviceMemoryProperties&	deviceMemoryProperties)
{
	ImageMemoryRequirementsExtended::verifyMemoryRequirements(result, deviceMemoryProperties);

	result.check(validValueVkBool32(m_currentTestPrefersDedicatedAllocation),
		"Non-bool value in m_currentTestPrefersDedicatedAllocation");

	result.check(m_currentTestRequiresDedicatedAllocation == VK_FALSE,
		"Test design expects m_currentTestRequiresDedicatedAllocation to be false");
}

#ifndef CTS_USES_VULKANSC
class ImageMemoryRequirementsCreateInfo : public ImageMemoryRequirementsExtended
{
public:
	static tcu::TestStatus testEntryPoint	(Context&									context,
											 const ImageTestParams						params);

protected:
	virtual void addFunctionTestCase		(tcu::TestCaseGroup*						group,
											 const std::string&							name,
											 const std::string&							desc,
											 const ImageTestParams						arg0);

	virtual void updateMemoryRequirements	(const DeviceInterface&						vk,
											 const VkDevice								device);

	virtual void verifyMemoryRequirements	(tcu::ResultCollector&						result,
											 const VkPhysicalDeviceMemoryProperties&	deviceMemoryProperties);

	VkMemoryRequirements							m_currentTestOriginalRequirements;
	VkMemoryRequirements							m_currentTestHalfRequirements;
	std::vector<VkSparseImageMemoryRequirements>	m_currentTestOriginalSparseRequirements;
};


tcu::TestStatus ImageMemoryRequirementsCreateInfo::testEntryPoint (Context& context, const ImageTestParams params)
{
	ImageMemoryRequirementsCreateInfo test;

	return test.execTest(context, params);
}

void checkSupportImageMemoryRequirementsCreateInfo (Context& context, ImageTestParams params)
{
	checkSupportImageMemoryRequirementsExtended(context, params);

	context.requireDeviceFunctionality("VK_KHR_maintenance4");
}

void ImageMemoryRequirementsCreateInfo::addFunctionTestCase (tcu::TestCaseGroup*	group,
															 const std::string&		name,
															 const std::string&		desc,
															 const ImageTestParams	arg0)
{
	addFunctionCase(group, name, desc, checkSupportImageMemoryRequirementsCreateInfo, testEntryPoint, arg0);
}

void ImageMemoryRequirementsCreateInfo::updateMemoryRequirements (const DeviceInterface&	vk,
																  const VkDevice			device)
{
	m_currentTestRequirements = getDeviceImageMemoryRequirements(vk, device, m_currentTestImageInfo);

	const Unique<VkImage> image(createImage(vk, device, &m_currentTestImageInfo));
	m_currentTestOriginalRequirements = getImageMemoryRequirements(vk, device, *image);

	VkImageCreateInfo halfImageCreateInfo = m_currentTestImageInfo;
	halfImageCreateInfo.extent = halfExtentForImage(m_currentTestImageInfo.imageType, m_currentTestImageInfo.extent);
	m_currentTestHalfRequirements = getDeviceImageMemoryRequirements(vk, device, halfImageCreateInfo);

	if (m_currentTestImageInfo.flags & VK_IMAGE_CREATE_SPARSE_BINDING_BIT)
	{
		m_currentTestSparseRequirements = getImageCreateInfoSparseMemoryRequirements(vk, device, m_currentTestImageInfo);
		m_currentTestOriginalSparseRequirements = getImageSparseMemoryRequirements(vk, device, *image);
	}
}

void ImageMemoryRequirementsCreateInfo::verifyMemoryRequirements (tcu::ResultCollector&					result,
																  const VkPhysicalDeviceMemoryProperties&	deviceMemoryProperties)
{
	ImageMemoryRequirementsOriginal::verifyMemoryRequirements(result, deviceMemoryProperties);

	result.check(m_currentTestRequirements.alignment == m_currentTestOriginalRequirements.alignment,
			"VkMemoryRequirements alignment queried from vkImageCreateInfo differs from equivalent object query");

	result.check(m_currentTestRequirements.memoryTypeBits == m_currentTestOriginalRequirements.memoryTypeBits,
			"VkMemoryRequirements memoryTypeBits queried from vkImageCreateInfo differs from equivalent object query");

	result.check(m_currentTestRequirements.size == m_currentTestOriginalRequirements.size,
			"VkMemoryRequirements size queried from vkImageCreateInfo differs from equivalent object query");

	result.check(m_currentTestRequirements.size >= m_currentTestHalfRequirements.size,
			"VkMemoryRequirements size queried from vkImageCreateInfo is larger for a smaller image");

	if (m_currentTestImageInfo.flags & VK_IMAGE_CREATE_SPARSE_BINDING_BIT)
	{
		result.check(m_currentTestSparseRequirements.size() == m_currentTestOriginalSparseRequirements.size(),
			"VkSparseImageMemoryRequirements count queried from vkImageCreateInfo differs from equivalent object query");

		for (deUint32 ndx = 0; ndx < m_currentTestSparseRequirements.size(); ++ndx)
		{
			const VkSparseImageMemoryRequirements &sparseRequirementsFromCreateInfo	= m_currentTestSparseRequirements[ndx];
			const VkSparseImageMemoryRequirements &sparseRequirementsFromObject		= m_currentTestOriginalSparseRequirements[ndx];

			result.check(sparseRequirementsFromCreateInfo.formatProperties.aspectMask == sparseRequirementsFromObject.formatProperties.aspectMask,
				"VkSparseImageMemoryRequirements aspectMask queried from vkImageCreateInfo differs from equivalent object query");

			result.check(sparseRequirementsFromCreateInfo.formatProperties.flags == sparseRequirementsFromObject.formatProperties.flags,
				"VkSparseImageMemoryRequirements flags queried from vkImageCreateInfo differs from equivalent object query");

			result.check(sparseRequirementsFromCreateInfo.formatProperties.imageGranularity.width == sparseRequirementsFromObject.formatProperties.imageGranularity.width,
				"VkSparseImageMemoryRequirements imageGranularity queried from vkImageCreateInfo differs from equivalent object query");
			result.check(sparseRequirementsFromCreateInfo.formatProperties.imageGranularity.height == sparseRequirementsFromObject.formatProperties.imageGranularity.height,
				"VkSparseImageMemoryRequirements imageGranularity queried from vkImageCreateInfo differs from equivalent object query");
			result.check(sparseRequirementsFromCreateInfo.formatProperties.imageGranularity.depth == sparseRequirementsFromObject.formatProperties.imageGranularity.depth,
				"VkSparseImageMemoryRequirements imageGranularity queried from vkImageCreateInfo differs from equivalent object query");

			result.check(sparseRequirementsFromCreateInfo.imageMipTailFirstLod == sparseRequirementsFromObject.imageMipTailFirstLod,
				"VkSparseImageMemoryRequirements imageMipTailFirstLod queried from vkImageCreateInfo differs from equivalent object query");

			result.check(sparseRequirementsFromCreateInfo.imageMipTailSize == sparseRequirementsFromObject.imageMipTailSize,
				"VkSparseImageMemoryRequirements imageMipTailSize queried from vkImageCreateInfo differs from equivalent object query");

			result.check(sparseRequirementsFromCreateInfo.imageMipTailOffset == sparseRequirementsFromObject.imageMipTailOffset,
				"VkSparseImageMemoryRequirements imageMipTailOffset queried from vkImageCreateInfo differs from equivalent object query");

			result.check(sparseRequirementsFromCreateInfo.imageMipTailStride == sparseRequirementsFromObject.imageMipTailStride,
				"VkSparseImageMemoryRequirements imageMipTailStride queried from vkImageCreateInfo differs from equivalent object query");
		}
	}
}
#endif // CTS_USES_VULKANSC

void populateCoreTestGroup (tcu::TestCaseGroup* group)
{
	BufferMemoryRequirementsOriginal	bufferTest;
	ImageMemoryRequirementsOriginal		imageTest;

	bufferTest.populateTestGroup(group);
	imageTest.populateTestGroup(group);
}

void populateExtendedTestGroup (tcu::TestCaseGroup* group)
{
	BufferMemoryRequirementsExtended	bufferTest;
	ImageMemoryRequirementsExtended		imageTest;

	bufferTest.populateTestGroup(group);
	imageTest.populateTestGroup(group);
}

void populateDedicatedAllocationTestGroup (tcu::TestCaseGroup* group)
{
	BufferMemoryRequirementsDedicatedAllocation	bufferTest;
	ImageMemoryRequirementsDedicatedAllocation	imageTest;

	bufferTest.populateTestGroup(group);
	imageTest.populateTestGroup(group);
}

bool isMultiplaneImageSupported (const InstanceInterface&	vki,
								 const VkPhysicalDevice		physicalDevice,
								 const VkImageCreateInfo&	info)
{
	// cubemap requires arrayLayers > 1, which multiplane doesn't support
	if (info.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)
		return false;

	if ((info.usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) &&
		(info.usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)) == 0u)
		return false;

	const VkPhysicalDeviceFeatures features = getPhysicalDeviceFeatures(vki, physicalDevice);

	if (info.flags & VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT)
	{
		DE_ASSERT(info.tiling == VK_IMAGE_TILING_OPTIMAL);

		if (info.imageType == VK_IMAGE_TYPE_2D && !features.sparseResidencyImage2D)
			return false;
	}

	const VkFormatProperties	formatProperties	= getPhysicalDeviceFormatProperties(vki, physicalDevice, info.format);
	const VkFormatFeatureFlags	formatFeatures		= (info.tiling == VK_IMAGE_TILING_LINEAR ? formatProperties.linearTilingFeatures
																							 : formatProperties.optimalTilingFeatures);
	if ((info.flags & VK_IMAGE_CREATE_DISJOINT_BIT) != 0 && (formatFeatures & VK_FORMAT_FEATURE_DISJOINT_BIT) == 0)
		return false;

	if (!doesUsageSatisfyFeatures(info.usage, formatFeatures))
		return false;

	VkImageFormatProperties		imageFormatProperties;
	const VkResult				result				= vki.getPhysicalDeviceImageFormatProperties(
														physicalDevice, info.format, info.imageType, info.tiling, info.usage, info.flags, &imageFormatProperties);

	if (VK_SUCCESS == result)
	{
		if (info.arrayLayers > imageFormatProperties.maxArrayLayers)
			return false;
		if (info.mipLevels > imageFormatProperties.maxMipLevels)
			return false;
		if ((info.samples & imageFormatProperties.sampleCounts) == 0u)
			return false;
	}

	return (VK_SUCCESS == result);
}

tcu::TestStatus testMultiplaneImages (Context& context, ImageTestParams params)
{
	const VkFormat multiplaneFormats[] =
	{
		VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
		VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
		VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
		VK_FORMAT_G8_B8R8_2PLANE_422_UNORM,
		VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM,
		VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16,
		VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
		VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16,
		VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16,
		VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16,
		VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16,
		VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16,
		VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16,
		VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16,
		VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16,
		VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM,
		VK_FORMAT_G16_B16R16_2PLANE_420_UNORM,
		VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM,
		VK_FORMAT_G16_B16R16_2PLANE_422_UNORM
	};

	std::vector<VkImageCreateFlags> nonSparseCreateFlags
	{
		  VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT
		, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
		, VK_IMAGE_CREATE_ALIAS_BIT
		/* VK_IMAGE_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT - present tests use only one physical device */
		, VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT
		/* VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT not apply with planar formats */
		, VK_IMAGE_CREATE_EXTENDED_USAGE_BIT
		, VK_IMAGE_CREATE_DISJOINT_BIT
		, VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT
	};
	if (context.getDeviceVulkan11Features().protectedMemory)
	{
		nonSparseCreateFlags.emplace_back(VK_IMAGE_CREATE_PROTECTED_BIT);
	}
#ifndef CTS_USES_VULKANSC
	std::vector<VkImageCreateFlags> nonSparseCreateFlagsNoSC;
	if (context.isDeviceFunctionalitySupported(VK_NV_CORNER_SAMPLED_IMAGE_EXTENSION_NAME))
	{
		nonSparseCreateFlagsNoSC.push_back(VK_IMAGE_CREATE_CORNER_SAMPLED_BIT_NV);
	}
	if (context.isDeviceFunctionalitySupported(VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME))
	{
		nonSparseCreateFlagsNoSC.push_back(VK_IMAGE_CREATE_SUBSAMPLED_BIT_EXT);
	}
	if (context.isDeviceFunctionalitySupported(VK_EXT_IMAGE_2D_VIEW_OF_3D_EXTENSION_NAME))
	{
		nonSparseCreateFlagsNoSC.push_back(VK_IMAGE_CREATE_2D_VIEW_COMPATIBLE_BIT_EXT);
	}
	if (context.isDeviceFunctionalitySupported(VK_QCOM_FRAGMENT_DENSITY_MAP_OFFSET_EXTENSION_NAME))
	{
		nonSparseCreateFlagsNoSC.push_back(VK_IMAGE_CREATE_FRAGMENT_DENSITY_MAP_OFFSET_BIT_QCOM);
	}

	auto isMultisampledRenderToSingleSampledEnabled = [&]() -> bool
	{
		bool enabled = false;
		if (context.isDeviceFunctionalitySupported(VK_EXT_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_EXTENSION_NAME))
		{
			const auto& feature = context.getMultisampledRenderToSingleSampledFeaturesEXT();
			enabled = feature.multisampledRenderToSingleSampled != VK_FALSE;
		}
		return enabled;
	};
	if (isMultisampledRenderToSingleSampledEnabled())
		nonSparseCreateFlagsNoSC.push_back(VK_IMAGE_CREATE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_BIT_EXT);
	nonSparseCreateFlags.insert(nonSparseCreateFlags.end(), nonSparseCreateFlagsNoSC.begin(), nonSparseCreateFlagsNoSC.end());
#endif // CTS_USES_VULKANSC

	const std::vector<VkImageCreateFlags> sparseCreateFlags
	{
		VK_IMAGE_CREATE_SPARSE_BINDING_BIT,
		VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT,
		VK_IMAGE_CREATE_SPARSE_ALIASED_BIT,
		VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT
	};

	const bool								isSparseTest		((params.flags & VK_IMAGE_CREATE_SPARSE_BINDING_BIT) != 0);
	const InstanceInterface&				vki					= context.getInstanceInterface();
	const VkPhysicalDevice					physicalDevice		= context.getPhysicalDevice();
	const DeviceInterface&					vk					= context.getDeviceInterface();
	const VkDevice							device				= context.getDevice();
	const VkImageUsageFlags					transientFlags		= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
	const VkPhysicalDeviceMemoryProperties	memoryProperties	= getPhysicalDeviceMemoryProperties(vki, physicalDevice);
	tcu::TestLog&							log					= context.getTestContext().getLog();
	tcu::ResultCollector					result				(log, "ERROR: ");
	std::set<VkFormat>						supportedFormats	{};

	log << TestLog::Message << "Memory properties: " << memoryProperties << TestLog::EndMessage;

	for (const VkFormat format : multiplaneFormats)
	{
		for (const VkImageCreateFlags loopCreateFlags : (isSparseTest ? sparseCreateFlags : nonSparseCreateFlags))
		{
			const VkImageType			imageType			= ((loopCreateFlags & VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT)
#ifndef CTS_USES_VULKANSC
																|| (loopCreateFlags & VK_IMAGE_CREATE_2D_VIEW_COMPATIBLE_BIT_EXT)
#endif // CTS_USES_VULKANSC
																																	)
																? isYCbCrConversionFormat(format)
																  ? VK_IMAGE_TYPE_MAX_ENUM
																  : VK_IMAGE_TYPE_3D
																: VK_IMAGE_TYPE_2D;
			if (imageType == VK_IMAGE_TYPE_MAX_ENUM) continue;

			if ((loopCreateFlags & VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT) && !isDepthStencilFormat(format)) continue;

			for (VkImageUsageFlags		loopUsageFlags	= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
										loopUsageFlags	<= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
										loopUsageFlags	= nextFlagExcluding(loopUsageFlags, transientFlags))
			{
				const VkImageCreateFlags	actualCreateFlags	= loopCreateFlags | (isSparseTest ? params.flags : 0);
				const VkImageUsageFlags		actualUsageFlags	= loopUsageFlags  | (params.transient ? VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT : (VkImageUsageFlagBits)0);
				const uint32_t				arrayLayers			= ((imageType == VK_IMAGE_TYPE_2D) && (loopCreateFlags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)) ? 6u : 1u;
				const VkImageCreateInfo		imageInfo			=
				{
					VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType          sType;
					DE_NULL,								// const void*              pNext;
					actualCreateFlags,						// VkImageCreateFlags       flags;
					imageType,								// VkImageType              imageType;
					format,									// VkFormat                 format;
					{ 64u, 64u, 1u, },						// VkExtent3D               extent;
					1u,										// uint32_t                 mipLevels;
					arrayLayers,							// uint32_t                 arrayLayers;
					VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits    samples;
					params.tiling,							// VkImageTiling            tiling;
					actualUsageFlags,						// VkImageUsageFlags        usage;
					VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode            sharingMode;
					0u,										// uint32_t                 queueFamilyIndexCount;
					DE_NULL,								// const uint32_t*          pQueueFamilyIndices;
					VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout            initialLayout;
				};

				if (isMultiplaneImageSupported(vki, physicalDevice, imageInfo))
				{
					const Unique<VkImage>			image			((params.useMaint4) ? Move<VkImage>() : createImage(vk, device, &imageInfo));

					log << tcu::TestLog::Message << "- " << getImageInfoString(imageInfo) << tcu::TestLog::EndMessage;

					supportedFormats.insert(format);

					for (deUint32 planeNdx = 0; planeNdx < (deUint32)getPlaneCount(format); planeNdx++)
					{
						VkMemoryRequirements2						requirements	=
						{
							VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
							DE_NULL,
							{ 0u, 0u, 0u }
						};
						const VkImageAspectFlagBits					aspect		= getPlaneAspect(planeNdx);

	#ifndef CTS_USES_VULKANSC
						if (params.useMaint4)
						{
							const VkDeviceImageMemoryRequirementsKHR	info	=
							{
								VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS_KHR,
								DE_NULL,
								&imageInfo,
								aspect
							};
							vk.getDeviceImageMemoryRequirements(device, &info, &requirements);
						}
						else
						{
	#endif // CTS_USES_VULKANSC
							const VkImagePlaneMemoryRequirementsInfo	aspectInfo	=
							{
								VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO,
								DE_NULL,
								aspect
							};
							const VkImageMemoryRequirementsInfo2		info		=
							{
								VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
								(actualCreateFlags & VK_IMAGE_CREATE_DISJOINT_BIT) == 0 ? DE_NULL : &aspectInfo,
								*image
							};

							vk.getImageMemoryRequirements2(device, &info, &requirements);
	#ifndef CTS_USES_VULKANSC
						}
	#endif // CTS_USES_VULKANSC

						log << TestLog::Message << "Aspect: " << getImageAspectFlagsStr(aspect) << ", Requirements: " << requirements << TestLog::EndMessage;

						result.check(deIsPowerOfTwo64(static_cast<deUint64>(requirements.memoryRequirements.alignment)), "VkMemoryRequirements alignment isn't power of two");

	#ifndef CTS_USES_VULKANSC
						if (actualCreateFlags & VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT)
						{
							std::vector<VkSparseImageMemoryRequirements> sparseRequirements;
							if ((*image == DE_NULL) || params.useMaint4)
								sparseRequirements = getDeviceImageSparseMemoryRequirements(vk, device, imageInfo, aspect);
							else
								sparseRequirements = getImageSparseMemoryRequirements(vk, device, *image);

							for (const VkSparseImageMemoryRequirements& sr : sparseRequirements)
							{
								if (sr.formatProperties.aspectMask == aspect)
								{
									result.check((sr.imageMipTailSize % requirements.memoryRequirements.alignment) == 0,
										"VkSparseImageMemoryRequirements::imageMipTailSize is not aligned with sparse block size");
									break;
								}
							}
						}
	#endif // CTS_USES_VULKANSC

						if (result.check(requirements.memoryRequirements.memoryTypeBits != 0, "No supported memory types"))
						{
							typedef std::vector<deUint32>::const_iterator	IndexIterator;
							const std::vector<deUint32>						usedMemoryTypeIndices	= bitsToIndices(requirements.memoryRequirements.memoryTypeBits);
							bool											hasHostVisibleType		= false;

							for (IndexIterator memoryTypeNdx = usedMemoryTypeIndices.begin(); memoryTypeNdx != usedMemoryTypeIndices.end(); ++memoryTypeNdx)
							{
								if (result.check(*memoryTypeNdx < memoryProperties.memoryTypeCount, "Unknown memory type bits set in memory requirements"))
								{
									const VkMemoryPropertyFlags	propertyFlags	(memoryProperties.memoryTypes[*memoryTypeNdx].propertyFlags);

									if (propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
										hasHostVisibleType = true;

									if (propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
									{
										result.check((imageInfo.usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) != 0u,
											"Memory type includes VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT for a non-transient attachment image");
									}
								}
								else
									break;
							}

							result.check(params.tiling != VK_IMAGE_TILING_LINEAR || (hasHostVisibleType || (loopCreateFlags & VK_IMAGE_CREATE_PROTECTED_BIT)),
								"Required memory type doesn't include VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT");
						}
					}
				}
			}
		}
	}

	if (supportedFormats.size() == 0)
	{
		return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED,
			"None of tested in given configuration multiplanar formats is supported by device.");
	}

	return tcu::TestStatus(result.getResult(), result.getMessage());
}

void checkSupportMultiplane (Context& context, ImageTestParams params)
{
	checkSupportImageMemoryRequirementsOriginal(context, params);

	context.requireDeviceFunctionality("VK_KHR_get_memory_requirements2");
	context.requireDeviceFunctionality("VK_KHR_sampler_ycbcr_conversion");

	if (params.useMaint4)
		context.requireDeviceFunctionality("VK_KHR_maintenance4");
}

void populateMultiplaneTestGroup (tcu::TestCaseGroup* group, bool useMaint4)
{
	const struct
	{
		VkImageCreateFlags		flags;
		bool					transient;
		const char* const		name;
	} imageFlagsCases[] =
	{
		{ (VkImageCreateFlags)0,																								false,	"regular"					},
		{ (VkImageCreateFlags)0,																								true,	"transient"					},
		{ VK_IMAGE_CREATE_SPARSE_BINDING_BIT,																					false,	"sparse"					},
		{ VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT,											false,	"sparse_residency"			},
		{ VK_IMAGE_CREATE_SPARSE_BINDING_BIT											| VK_IMAGE_CREATE_SPARSE_ALIASED_BIT,	false,	"sparse_aliased"			},
		{ VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT		| VK_IMAGE_CREATE_SPARSE_ALIASED_BIT,	false,	"sparse_residency_aliased"	},
	};
	const struct
	{
		VkImageTiling	value;
		const char*		name;
	} tilings[] =
	{
		{ VK_IMAGE_TILING_OPTIMAL,	"optimal"	},
		{ VK_IMAGE_TILING_LINEAR,	"linear"	}
	};

	for (size_t flagsNdx = 0; flagsNdx < DE_LENGTH_OF_ARRAY(imageFlagsCases); ++flagsNdx)
	for (size_t tilingNdx = 0; tilingNdx < DE_LENGTH_OF_ARRAY(tilings); ++tilingNdx)
	{
		const VkImageCreateFlags	flags		= imageFlagsCases[flagsNdx].flags;
		const bool					transient	= imageFlagsCases[flagsNdx].transient;
		const VkImageTiling			tiling		= tilings[tilingNdx].value;
		const ImageTestParams		params		(flags, tiling, transient, useMaint4);
		const std::string			name		= std::string(imageFlagsCases[flagsNdx].name) + "_" + tilings[tilingNdx].name;

		if (tiling == VK_IMAGE_TILING_LINEAR && (flags & VK_IMAGE_CREATE_SPARSE_BINDING_BIT) != 0)
			continue;

		addFunctionCase(group, name, name, checkSupportMultiplane, testMultiplaneImages, params);
	}
}

tcu::TestStatus testVkMemoryPropertyFlags(Context& context)
{
	VkMemoryPropertyFlags	propertyFlagSets[]	=
	{
		0,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT	| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT	| VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT	| VK_MEMORY_PROPERTY_HOST_CACHED_BIT			| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT	| VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT			| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT	| VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT			| VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT	| VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT			| VK_MEMORY_PROPERTY_HOST_CACHED_BIT			| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT	| VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT,
		VK_MEMORY_PROPERTY_PROTECTED_BIT,
		VK_MEMORY_PROPERTY_PROTECTED_BIT	| VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
#ifndef CTS_USES_VULKANSC
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT	| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT			| VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT	| VK_MEMORY_PROPERTY_HOST_CACHED_BIT			| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT			| VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT	| VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT	| VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT			| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT			| VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT	| VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT			| VK_MEMORY_PROPERTY_HOST_CACHED_BIT			| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT			| VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT	| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT			| VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD	| VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT	| VK_MEMORY_PROPERTY_HOST_CACHED_BIT			| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT			| VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD	| VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT	| VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD	| VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT	| VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT			| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT			| VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD	| VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT	| VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT			| VK_MEMORY_PROPERTY_HOST_CACHED_BIT			| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT			| VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD	| VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT	| VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV
#endif // CTS_USES_VULKANSC
	};

	const InstanceInterface&				vki					= context.getInstanceInterface();
	const VkPhysicalDevice					physicalDevice		= context.getPhysicalDevice();
	VkPhysicalDeviceMemoryProperties		memoryProperties;
	uint32_t								matchingTypes		= 0;
	tcu::TestLog&							log					= context.getTestContext().getLog();
	tcu::ResultCollector					result				(log, "ERROR: ");

	vki.getPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

	for (uint32_t ndx = 0; ndx < memoryProperties.memoryTypeCount; ndx++)
	{
		for (auto& flag : propertyFlagSets)
		{
			if (memoryProperties.memoryTypes[ndx].propertyFlags == flag) matchingTypes++;
		}
	}

	std::string								diffStr				= std::to_string(int(memoryProperties.memoryTypeCount - matchingTypes));

	result.check(matchingTypes == memoryProperties.memoryTypeCount, "Unknown memory type bits set in memory requirements: " + diffStr + " mismatch");

	return tcu::TestStatus(result.getResult(), result.getMessage());
}

void populateMemoryPropertyFlagsTestGroup(tcu::TestCaseGroup* group)
{
	addFunctionCase(group, "check_all", "", testVkMemoryPropertyFlags);
}

void populateMultiplaneTestGroup (tcu::TestCaseGroup* group)
{
	populateMultiplaneTestGroup(group, false);
}


#ifndef CTS_USES_VULKANSC
void populateCreateInfoTestGroup (tcu::TestCaseGroup* group)
{
	BufferMemoryRequirementsCreateInfo			bufferTest;
	ImageMemoryRequirementsCreateInfo			imageTest;

	bufferTest.populateTestGroup(group);
	imageTest.populateTestGroup(group);

	de::MovePtr<tcu::TestCaseGroup> multiplaneGroup(new tcu::TestCaseGroup(group->getTestContext(), "multiplane_image", ""));
	populateMultiplaneTestGroup(multiplaneGroup.get(), true);
	group->addChild(multiplaneGroup.release());
}
#endif // CTS_USES_VULKANSC

} // anonymous


tcu::TestCaseGroup* createRequirementsTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> requirementsGroup(new tcu::TestCaseGroup(testCtx, "requirements", "Buffer and image memory requirements"));

	requirementsGroup->addChild(createTestGroup(testCtx, "core",					"Memory requirements tests with core functionality",						populateCoreTestGroup));
	requirementsGroup->addChild(createTestGroup(testCtx, "extended",				"Memory requirements tests with extension VK_KHR_get_memory_requirements2",	populateExtendedTestGroup));
	requirementsGroup->addChild(createTestGroup(testCtx, "dedicated_allocation",	"Memory requirements tests with extension VK_KHR_dedicated_allocation",		populateDedicatedAllocationTestGroup));
	requirementsGroup->addChild(createTestGroup(testCtx, "multiplane_image",		"Memory requirements tests with vkGetImagePlaneMemoryRequirements",			populateMultiplaneTestGroup));
	requirementsGroup->addChild(createTestGroup(testCtx, "memory_property_flags",	"Memory requirements tests with vkGetPhysicalDeviceMemoryProperties",		populateMemoryPropertyFlagsTestGroup));
#ifndef CTS_USES_VULKANSC
	requirementsGroup->addChild(createTestGroup(testCtx, "create_info",				"Memory requirements tests with extension VK_KHR_maintenance4",				populateCreateInfoTestGroup));
#endif // CTS_USES_VULKANSC

	return requirementsGroup.release();
}

} // memory
} // vkt
