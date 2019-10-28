/*-------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2017 Khronos Group
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
* \brief API Maintenance3 Check test - checks structs and function from VK_KHR_maintenance3
*//*--------------------------------------------------------------------*/

#include "tcuTestLog.hpp"

#include "vkQueryUtil.hpp"

#include "vktApiMaintenance3Check.hpp"
#include "vktTestCase.hpp"

#include <sstream>
#include <limits>
#include <utility>
#include <algorithm>
#include <map>
#include <set>

using namespace vk;

namespace vkt
{

namespace api
{

namespace
{
using ::std::string;
using ::std::vector;
using ::std::map;
using ::std::set;
using ::std::ostringstream;
using ::std::make_pair;

typedef vk::VkPhysicalDeviceProperties						DevProp1;
typedef vk::VkPhysicalDeviceProperties2						DevProp2;
typedef vk::VkPhysicalDeviceMaintenance3Properties			MaintDevProp3;
typedef vk::VkPhysicalDeviceFeatures2						DevFeat2;
typedef vk::VkPhysicalDeviceInlineUniformBlockFeaturesEXT	DevIubFeat;
typedef vk::VkPhysicalDeviceInlineUniformBlockPropertiesEXT	DevIubProp;

// These variables are equal to minimal values for maxMemoryAllocationSize and maxPerSetDescriptors
constexpr deUint32 maxMemoryAllocationSize			= 1073741824u;
constexpr deUint32 maxDescriptorsInSet				= 1024u;
constexpr deUint32 maxReasonableInlineUniformBlocks	= 64u;

using TypeSet		= set<vk::VkDescriptorType>;

// Structure representing an implementation limit, like maxPerStageDescriptorSamplers. It has a maximum value
// obtained at runtime and a remaining number of descriptors, which starts with the same count and decreases
// as we assign descriptor counts to the different types. A limit is affected by (or itself affects) one or more
// descriptor types. Note a type may be involved in several limits, and a limit may affect several types.
struct Limit
{
	Limit(const string& name_, deUint32 maxValue_, const TypeSet& affectedTypes_)
		: name(name_), maxValue(maxValue_), remaining(maxValue_), affectedTypes(affectedTypes_)
		{}

	const string	name;
	const deUint32	maxValue;
	deUint32		remaining;
	const TypeSet	affectedTypes;
};

// Structure representing how many descriptors have been assigned to the given type. The type is "alive" during
// descriptor count assignment if more descriptors can be added to the type without hitting any limit affected
// by the type. Once at least one of the limits is reached, no more descriptors can be assigned to the type and
// the type is no longer considered "alive".
struct TypeState
{
	TypeState(vk::VkDescriptorType type_)
		: type(type_), alive(true), count(0u)
		{}

	const vk::VkDescriptorType	type;
	bool						alive;
	deUint32					count;
};

using TypeCounts	= map<vk::VkDescriptorType, TypeState>;
using LimitsVector	= vector<Limit>;

// Get the subset of alive types from the given map.
TypeSet getAliveTypes (const TypeCounts& typeCounts)
{
	TypeSet aliveTypes;
	for (const auto& typeCount : typeCounts)
	{
		if (typeCount.second.alive)
			aliveTypes.insert(typeCount.first);
	}
	return aliveTypes;
}

// Get the subset of alive types for a specific limit, among the set of types affected by the limit.
TypeSet getAliveTypesForLimit (const Limit& limit, const TypeSet& aliveTypes)
{
	TypeSet subset;
	for (const auto& type : limit.affectedTypes)
	{
		if (aliveTypes.find(type) != aliveTypes.end())
			subset.insert(type);
	}
	return subset;
}

// Distribute descriptor counts as evenly as possible among the given set of types, taking into account the
// given limits.
void distributeCounts (LimitsVector& limits, TypeCounts& typeCounts)
{
	using IncrementsMap = map<vk::VkDescriptorType, deUint32>;
	TypeSet aliveTypes;

	while ((aliveTypes = getAliveTypes(typeCounts)).size() > 0u)
	{
		// Calculate the maximum increment per alive descriptor type. This involves iterating over the limits and
		// finding out how many more descriptors can be distributed among the affected types that are still alive
		// for the limit. For each type, remember the lowest possible increment.
		IncrementsMap increments;
		for (const auto& type : aliveTypes)
			increments[type] = std::numeric_limits<deUint32>::max();

		TypeSet aliveTypesForLimit;

		for (const auto& limit : limits)
		{
			if (limit.remaining == 0u)
				continue;

			aliveTypesForLimit = getAliveTypesForLimit(limit, aliveTypes);
			if (aliveTypesForLimit.empty())
				continue;

			// Distribute remaining count evenly among alive types.
			deUint32 maxIncrement = limit.remaining / static_cast<deUint32>(aliveTypesForLimit.size());
			if (maxIncrement == 0u)
			{
				// More types than remaining descriptors. Assign 1 to the first affected types and 0 to the rest.
				deUint32 remaining = limit.remaining;
				for (const auto& type : aliveTypesForLimit)
				{
					if (remaining > 0u && increments[type] > 0u)
					{
						increments[type] = 1u;
						--remaining;
					}
					else
					{
						increments[type] = 0u;
					}
				}
			}
			else
			{
				// Find the lowest possible increment taking into account all limits.
				for (const auto& type : aliveTypesForLimit)
				{
					if (increments[type] > maxIncrement)
						increments[type] = maxIncrement;
				}
			}
		}

		// Apply the calculated increments per descriptor type, decreasing the remaining descriptors for each
		// limit affected by the type, and switching types to the not-alive state when a limit is hit.
		for (const auto& inc : increments)
		{
			const vk::VkDescriptorType& type = inc.first;
			const deUint32& increment = inc.second;

			// Increase type count.
			auto iter = typeCounts.find(type);
			DE_ASSERT(iter != typeCounts.end());
			iter->second.count += increment;

			for (auto& limit : limits)
			{
				// Decrease remaining descriptors for affected limits.
				if (limit.affectedTypes.find(type) != limit.affectedTypes.end())
				{
					DE_ASSERT(increment <= limit.remaining);
					limit.remaining -= increment;
				}
				if (limit.remaining == 0u)
				{
					// Limit hit, switch affected types to not-alive.
					for (const auto& affectedType : limit.affectedTypes)
					{
						auto tc = typeCounts.find(affectedType);
						if (tc != typeCounts.end())
							tc->second.alive = false;
					}
				}
			}
		}
	}
}

// Create a limits vector based on runtime limit information for the device.
LimitsVector buildLimitsVector (const DevProp1& prop1, const DevIubProp& iubProp, const MaintDevProp3& maintProp3)
{
	static const TypeSet samplerTypes				= { vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vk::VK_DESCRIPTOR_TYPE_SAMPLER };
	static const TypeSet sampledImageTypes			= { vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vk::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, vk::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER };
	static const TypeSet uniformBufferTypes			= { vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC };
	static const TypeSet storageBufferTypes			= { vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC };
	static const TypeSet storageImageTypes			= { vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, vk::VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER };
	static const TypeSet inputAttachmentTypes		= { vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT };
	static const TypeSet inlineUniformBlockTypes	= { vk::VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT };
	static const TypeSet dynamicUniformBuffer		= { vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC };
	static const TypeSet dynamicStorageBuffer		= { vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC };
	static const TypeSet allTypesButIUB				= {
														vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
														vk::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
														vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
														vk::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
														vk::VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
														vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
														vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
														vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
														vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
														vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
													};
	static const TypeSet allTypes					= {
														vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
														vk::VK_DESCRIPTOR_TYPE_SAMPLER,
														vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
														vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
														vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
														vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
														vk::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
														vk::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
														vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
														vk::VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
														vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
														vk::VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT,
													};

	LimitsVector limits = {
		{
			"maxPerStageDescriptorSamplers",
			prop1.limits.maxPerStageDescriptorSamplers,
			samplerTypes
		},
		{
			"maxDescriptorSetSamplers",
			prop1.limits.maxDescriptorSetSamplers,
			samplerTypes
		},
		{
			"maxPerStageDescriptorSampledImages",
			prop1.limits.maxPerStageDescriptorSampledImages,
			sampledImageTypes
		},
		{
			"maxDescriptorSetSampledImages",
			prop1.limits.maxDescriptorSetSampledImages,
			sampledImageTypes
		},
		{
			"maxPerStageDescriptorUniformBuffers",
			prop1.limits.maxPerStageDescriptorUniformBuffers,
			uniformBufferTypes
		},
		{
			"maxDescriptorSetUniformBuffers",
			prop1.limits.maxDescriptorSetUniformBuffers,
			uniformBufferTypes
		},
		{
			"maxPerStageDescriptorStorageBuffers",
			prop1.limits.maxPerStageDescriptorStorageBuffers,
			storageBufferTypes
		},
		{
			"maxDescriptorSetStorageBuffers",
			prop1.limits.maxDescriptorSetStorageBuffers,
			storageBufferTypes
		},
		{
			"maxPerStageDescriptorStorageImages",
			prop1.limits.maxPerStageDescriptorStorageImages,
			storageImageTypes
		},
		{
			"maxDescriptorSetStorageImages",
			prop1.limits.maxDescriptorSetStorageImages,
			storageImageTypes
		},
		{
			"maxPerStageDescriptorInputAttachments",
			prop1.limits.maxPerStageDescriptorInputAttachments,
			inputAttachmentTypes
		},
		{
			"maxDescriptorSetInputAttachments",
			prop1.limits.maxDescriptorSetInputAttachments,
			inputAttachmentTypes
		},
		{
			"maxDescriptorSetUniformBuffersDynamic",
			prop1.limits.maxDescriptorSetUniformBuffersDynamic,
			dynamicUniformBuffer
		},
		{
			"maxDescriptorSetStorageBuffersDynamic",
			prop1.limits.maxDescriptorSetStorageBuffersDynamic,
			dynamicStorageBuffer
		},
		{
			"maxPerStageDescriptorInlineUniformBlocks",
			iubProp.maxPerStageDescriptorInlineUniformBlocks,
			inlineUniformBlockTypes
		},
		{
			"maxDescriptorSetInlineUniformBlocks",
			iubProp.maxDescriptorSetInlineUniformBlocks,
			inlineUniformBlockTypes
		},
		{
			"maxPerStageResources",
			prop1.limits.maxPerStageResources,
			allTypesButIUB
		},
		{
			"maxPerSetDescriptors",
			maintProp3.maxPerSetDescriptors,
			allTypes
		},
	};

	return limits;
}

// Create a vector of bindings by constructing the system limits and distributing descriptor counts.
vector<vk::VkDescriptorSetLayoutBinding> calculateBindings(const DevProp1& prop1, const DevIubProp& iubProp, const MaintDevProp3& maintProp3, const vector<vk::VkDescriptorType> &types)
{
	LimitsVector limits = buildLimitsVector(prop1, iubProp, maintProp3);
	TypeCounts typeCounts;

	for (const auto& type : types)
		typeCounts.emplace(make_pair(type, TypeState(type)));

	distributeCounts(limits, typeCounts);

	deUint32 bindingNumber = 0u;
	vector<vk::VkDescriptorSetLayoutBinding> bindings;
	for (const auto& tc : typeCounts)
	{
		if (tc.first != VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
		{
			vk::VkDescriptorSetLayoutBinding b;
			b.binding = bindingNumber;
			b.descriptorCount = tc.second.count;
			b.descriptorType = tc.first;
			b.pImmutableSamplers = DE_NULL;
			b.stageFlags = vk::VK_SHADER_STAGE_ALL;

			bindings.push_back(b);
		}
		else
		{
			// Inline uniform blocks are special because descriptorCount represents the size of that block.
			// The only way of creating several blocks is by adding more structures to the list instead of creating an array.
			size_t firstAdded = bindings.size();
			bindings.resize(firstAdded + tc.second.count);
			for (deUint32 i = 0u; i < tc.second.count; ++i)
			{
				vk::VkDescriptorSetLayoutBinding& b = bindings[firstAdded + i];
				b.binding = bindingNumber + i;
				b.descriptorCount = 4u;	// For inline uniform blocks, this must be a multiple of 4 according to the spec.
				b.descriptorType = tc.first;
				b.pImmutableSamplers = DE_NULL;
				b.stageFlags = vk::VK_SHADER_STAGE_ALL;
			}
		}
		bindingNumber += tc.second.count;
	}

	return bindings;
}

// Get a textual description with descriptor counts per type.
string getBindingsDescription (const vector<VkDescriptorSetLayoutBinding>& bindings)
{
	map<vk::VkDescriptorType, deUint32> typeCount;
	deUint32 totalCount = 0u;
	deUint32 count;
	for (const auto& b : bindings)
	{
		auto iter = typeCount.find(b.descriptorType);
		if (iter == typeCount.end())
			iter = typeCount.insert(make_pair(b.descriptorType, (deUint32)0)).first;
		count = ((b.descriptorType == vk::VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT) ? 1u : b.descriptorCount);
		iter->second += count;
		totalCount += count;
	}

	deUint32 i = 0;
	ostringstream combStr;

	combStr << "{ Descriptors: " << totalCount << ", [";
	for (const auto& tc : typeCount)
		combStr << (i++ ? ", " : " ") << tc.first << ": " << tc.second;
	combStr << " ] }";

	return combStr.str();
}

class Maintenance3StructTestInstance : public TestInstance
{
public:
											Maintenance3StructTestInstance			(Context& ctx)
												: TestInstance						(ctx)
	{}
	virtual tcu::TestStatus					iterate									(void)
	{
		tcu::TestLog&						log										= m_context.getTestContext().getLog();

		// set values to be a bit smaller than required minimum values
		MaintDevProp3 maintProp3 =
		{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES,				//VkStructureType						sType;
			DE_NULL,																//void*									pNext;
			maxDescriptorsInSet - 1u,												//deUint32								maxPerSetDescriptors;
			maxMemoryAllocationSize - 1u											//VkDeviceSize							maxMemoryAllocationSize;
		};

		DevProp2 prop2;
		deMemset(&prop2, 0, sizeof(prop2)); // zero the structure
		prop2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		prop2.pNext = &maintProp3;

		m_context.getInstanceInterface().getPhysicalDeviceProperties2(m_context.getPhysicalDevice(), &prop2);

		if (maintProp3.maxMemoryAllocationSize < maxMemoryAllocationSize)
			return tcu::TestStatus::fail("Fail");

		if (maintProp3.maxPerSetDescriptors < maxDescriptorsInSet)
			return tcu::TestStatus::fail("Fail");

		log << tcu::TestLog::Message << "maxMemoryAllocationSize: "	<< maintProp3.maxMemoryAllocationSize	<< tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "maxPerSetDescriptors: "	<< maintProp3.maxPerSetDescriptors		<< tcu::TestLog::EndMessage;
		return tcu::TestStatus::pass("Pass");
	}
};

class Maintenance3StructTestCase : public TestCase
{
public:
											Maintenance3StructTestCase				(tcu::TestContext&	testCtx)
												: TestCase(testCtx, "maintenance3_properties", "tests VkPhysicalDeviceMaintenance3Properties struct")
	{}

	virtual									~Maintenance3StructTestCase				(void)
	{}
	virtual void							checkSupport							(Context&	ctx) const
	{
		ctx.requireDeviceFunctionality("VK_KHR_maintenance3");
	}
	virtual TestInstance*					createInstance							(Context&	ctx) const
	{
		return new Maintenance3StructTestInstance(ctx);
	}

private:
};

class Maintenance3DescriptorTestInstance : public TestInstance
{
public:
											Maintenance3DescriptorTestInstance		(Context&	ctx)
												: TestInstance(ctx)
	{}
	virtual tcu::TestStatus					iterate									(void)
	{
		const auto& vki				= m_context.getInstanceInterface();
		const auto& vkd				= m_context.getDeviceInterface();
		const auto& physicalDevice	= m_context.getPhysicalDevice();
		const auto&	device			= m_context.getDevice();
		auto&		log				= m_context.getTestContext().getLog();
		bool		iubSupported	= false;

		if (m_context.isDeviceFunctionalitySupported("VK_EXT_inline_uniform_block"))
		{
			DevIubFeat	iubFeatures	=
			{
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES_EXT,
				DE_NULL,
				0u,
				0u
			};

			DevFeat2	features2	=
			{
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
				&iubFeatures,
				VkPhysicalDeviceFeatures()
			};

			vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);
			iubSupported = (iubFeatures.inlineUniformBlock == VK_TRUE);
		}

		DevIubProp							devIubProp								=
		{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_PROPERTIES_EXT,	// VkStructureType	sType;
			DE_NULL,																// void*			pNext;
			0u,																		// deUint32			maxInlineUniformBlockSize;
			0u,																		// deUint32			maxPerStageDescriptorInlineUniformBlocks;
			0u,																		// deUint32			maxPerStageDescriptorUpdateAfterBindInlineUniformBlocks;
			0u,																		// deUint32			maxDescriptorSetInlineUniformBlocks;
			0u																		// deUint32			maxDescriptorSetUpdateAfterBindInlineUniformBlocks;
		};

		MaintDevProp3						maintProp3								=
		{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES,				//VkStructureType						sType;
			(iubSupported ? &devIubProp : DE_NULL),									//void*									pNext;
			maxDescriptorsInSet,													//deUint32								maxPerSetDescriptors;
			maxMemoryAllocationSize													//VkDeviceSize							maxMemoryAllocationSize;
		};

		DevProp2							prop2									=
		{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,							//VkStructureType						sType;
			&maintProp3,															//void*									pNext;
			VkPhysicalDeviceProperties()											//VkPhysicalDeviceProperties			properties;
		};

		vki.getPhysicalDeviceProperties2(physicalDevice, &prop2);

		vector<VkDescriptorType> descriptorTypes = {
			VK_DESCRIPTOR_TYPE_SAMPLER,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
			VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
			VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
		};
		if (iubSupported)
			descriptorTypes.push_back(VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT);

		// VkDescriptorSetLayoutCreateInfo setup
		vk::VkDescriptorSetLayoutCreateInfo	pCreateInfo								=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,					//VkStructureType						sType;
			DE_NULL,																//const void*							pNext;
			0u,																		//VkDescriptorSetLayoutCreateFlags		flags;
			0u,																		//deUint32								bindingCount;
			DE_NULL																	//const VkDescriptorSetLayoutBinding*	pBindings;
		};

		// VkDescriptorSetLayoutSupport setup
		vk::VkDescriptorSetLayoutSupport	pSupport								=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT,						//VkStructureType						sType;
			DE_NULL,																//void*									pNext;
			VK_FALSE																//VkBool32								supported;
		};

		// Check every combination maximizing descriptor counts.
		for (size_t combSize = 1; combSize <= descriptorTypes.size(); ++combSize)
		{
			// Create a vector of selectors with combSize elements set to true.
			vector<bool> selectors(descriptorTypes.size(), false);
			std::fill(begin(selectors), begin(selectors) + combSize, true);

			// Iterate over every permutation of selectors for that combination size.
			do
			{
				vector<vk::VkDescriptorType> types;
				for (size_t i = 0; i < selectors.size(); ++i)
				{
					if (selectors[i])
						types.push_back(descriptorTypes[i]);
				}

				// Due to inline uniform blocks being unable to form arrays and each one of them needing its own
				// VkDescriptorSetLayoutBinding structure, we will limit when to test them.
				if (std::find(begin(types), end(types), VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT) != types.end() &&
					devIubProp.maxPerStageDescriptorInlineUniformBlocks > maxReasonableInlineUniformBlocks &&
					combSize > 1u && combSize < descriptorTypes.size())
				{
					continue;
				}

				vector<vk::VkDescriptorSetLayoutBinding> bindings = calculateBindings(prop2.properties, devIubProp, maintProp3, types);

				string description = getBindingsDescription(bindings);
				log << tcu::TestLog::Message << "Testing combination: " << description << tcu::TestLog::EndMessage;

				pCreateInfo.bindingCount = static_cast<deUint32>(bindings.size());
				pCreateInfo.pBindings = bindings.data();

				vkd.getDescriptorSetLayoutSupport(device, &pCreateInfo, &pSupport);
				if (pSupport.supported == VK_FALSE)
				{
					ostringstream msg;
					msg << "Failed to use the following descriptor type counts: " << description;
					return tcu::TestStatus::fail(msg.str());
				}
			} while (std::prev_permutation(begin(selectors), end(selectors)));
		}

		return tcu::TestStatus::pass("Pass");
	}

};

class Maintenance3DescriptorTestCase : public TestCase
{

public:
											Maintenance3DescriptorTestCase			(tcu::TestContext&	testCtx)
												: TestCase(testCtx, "descriptor_set", "tests vkGetDescriptorSetLayoutSupport struct")
	{}
	virtual									~Maintenance3DescriptorTestCase			(void)
	{}
	virtual void							checkSupport							(Context&	ctx) const
	{
		ctx.requireDeviceFunctionality("VK_KHR_maintenance3");
	}
	virtual TestInstance*					createInstance							(Context&	ctx) const
	{
		return new Maintenance3DescriptorTestInstance(ctx);
	}
};

} // anonymous

tcu::TestCaseGroup*							createMaintenance3Tests					(tcu::TestContext&	testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	main3Tests(new tcu::TestCaseGroup(testCtx, "maintenance3_check", "Maintenance3 Tests"));
	main3Tests->addChild(new Maintenance3StructTestCase(testCtx));
	main3Tests->addChild(new Maintenance3DescriptorTestCase(testCtx));

	return main3Tests.release();
}

} // api
} // vkt
