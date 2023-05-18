#ifndef _VKSSTRUCTSVKSC_HPP
#define _VKSSTRUCTSVKSC_HPP

/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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
 *-------------------------------------------------------------------------*/

#include "vksSerializerVKSC.hpp"

namespace vksc_server
{

struct SourceVariant
{
	string												active;
	vk::GlslSource										glsl;
	vk::HlslSource										hlsl;
	vk::SpirVAsmSource									spirv;

	template <typename TYPE>
	void Serialize (Serializer<TYPE>& archive)
	{
		archive.Serialize(active);
		if (active == "glsl") archive.Serialize(glsl);
		else if (active == "hlsl") archive.Serialize(hlsl);
		else if (active == "spirv") archive.Serialize(spirv);
		else throw std::runtime_error("incorrect shader type");
	}
};

struct VulkanJsonPipelineDescription
{
	VulkanJsonPipelineDescription	()
		: currentCount	(0u)
		, maxCount		(0u)
		, allCount		(0u)
	{
	}
	VulkanJsonPipelineDescription	(const vk::VkPipelineOfflineCreateInfo&	id_,
									 const string&							pipelineContents_,
									 const string&							deviceFeatures_,
									 const vector<string>&					deviceExtensions_,
									 const std::string&						test)
		: id				(id_)
		, pipelineContents	(pipelineContents_)
		, deviceFeatures	(deviceFeatures_)
		, deviceExtensions	(deviceExtensions_)
		, currentCount	(1u)
		, maxCount		(1u)
		, allCount		(1u)
	{
		tests.insert(test);
	}

	void add	(const std::string& test)
	{
		tests.insert(test);
		allCount++;
		currentCount++;
		maxCount = de::max(maxCount, currentCount);
	}

	void remove	()
	{
		currentCount--;
	}

	vk::VkPipelineOfflineCreateInfo						id;
	string												pipelineContents;
	string												deviceFeatures;
	vector<string>										deviceExtensions;
	deUint32											currentCount;
	deUint32											maxCount;
	deUint32											allCount;
	std::set<string>									tests;
};

inline void SerializeItem(Serializer<ToRead>& serializer, VulkanJsonPipelineDescription& v)
{
	serializer.Serialize(v.id, v.pipelineContents, v.deviceFeatures, v.deviceExtensions, v.currentCount, v.maxCount, v.allCount, v.tests);
}

inline void SerializeItem(Serializer<ToWrite>& serializer, VulkanJsonPipelineDescription& v)
{
	serializer.Serialize(v.id, v.pipelineContents, v.deviceFeatures, v.deviceExtensions, v.currentCount, v.maxCount, v.allCount, v.tests);
}

struct VulkanPipelineSize
{
	vk::VkPipelineOfflineCreateInfo						id;
	deUint32											count;
	deUint32											size;
};

inline void SerializeItem(Serializer<ToRead>& serializer, VulkanPipelineSize& v)
{
	serializer.Serialize(v.id, v.count, v.size);
}

inline void SerializeItem(Serializer<ToWrite>& serializer, VulkanPipelineSize& v)
{
	serializer.Serialize(v.id, v.count, v.size);
}

struct PipelineIdentifierEqual
{
	PipelineIdentifierEqual(const vk::VkPipelineOfflineCreateInfo& p)
		: searched(p)
	{
	}
	bool operator() (const vksc_server::VulkanJsonPipelineDescription& item) const
	{
		for (deUint32 i = 0; i < VK_UUID_SIZE; ++i)
			if (searched.pipelineIdentifier[i] != item.id.pipelineIdentifier[i])
				return false;
		return true;
	}
	bool operator() (const vksc_server::VulkanPipelineSize& item) const
	{
		for (deUint32 i = 0; i < VK_UUID_SIZE; ++i)
			if (searched.pipelineIdentifier[i] != item.id.pipelineIdentifier[i])
				return false;
		return true;
	}

	const vk::VkPipelineOfflineCreateInfo& searched;
};

struct VulkanPipelineCacheInput
{
	std::map<vk::VkSamplerYcbcrConversion, string>		samplerYcbcrConversions;
	std::map<vk::VkSampler, string>						samplers;
	std::map<vk::VkShaderModule, string>				shaderModules;
	std::map<vk::VkRenderPass, string>					renderPasses;
	std::map<vk::VkPipelineLayout, string>				pipelineLayouts;
	std::map<vk::VkDescriptorSetLayout, string>			descriptorSetLayouts;
	std::vector<VulkanJsonPipelineDescription>			pipelines;

	template <typename TYPE>
	void Serialize (Serializer<TYPE>& archive)
	{
		archive.Serialize(samplerYcbcrConversions, samplers, shaderModules, renderPasses, pipelineLayouts, descriptorSetLayouts, pipelines);
	}
};

inline void SerializeItem(Serializer<ToRead>& serializer, VulkanPipelineCacheInput& v)
{
	serializer.Serialize(v.samplerYcbcrConversions, v.samplers, v.shaderModules, v.renderPasses, v.pipelineLayouts, v.descriptorSetLayouts, v.pipelines);
}

inline void SerializeItem(Serializer<ToWrite>& serializer, VulkanPipelineCacheInput& v)
{
	serializer.Serialize(v.samplerYcbcrConversions, v.samplers, v.shaderModules, v.renderPasses, v.pipelineLayouts, v.descriptorSetLayouts, v.pipelines);
}

struct VulkanCommandMemoryConsumption
{
	VulkanCommandMemoryConsumption()
		: commandPool						(0u)
		, commandBufferCount				(0u)
		, currentCommandPoolAllocated		(0u)
		, maxCommandPoolAllocated			(0u)
		, currentCommandPoolReservedSize	(0u)
		, maxCommandPoolReservedSize		(0u)
		, currentCommandBufferAllocated		(0u)
		, maxCommandBufferAllocated			(0u)
	{
	}

	VulkanCommandMemoryConsumption (deUint64 commandPool_)
		: commandPool						(commandPool_)
		, commandBufferCount				(0u)
		, currentCommandPoolAllocated		(0u)
		, maxCommandPoolAllocated			(0u)
		, currentCommandPoolReservedSize	(0u)
		, maxCommandPoolReservedSize		(0u)
		, currentCommandBufferAllocated		(0u)
		, maxCommandBufferAllocated			(0u)
	{
	}
	void updateValues(vk::VkDeviceSize cpAlloc, vk::VkDeviceSize cpReserved, vk::VkDeviceSize cbAlloc)
	{
		currentCommandPoolAllocated		+= cpAlloc;		maxCommandPoolAllocated		= de::max(currentCommandPoolAllocated, maxCommandPoolAllocated);
		currentCommandPoolReservedSize	+= cpReserved;	maxCommandPoolReservedSize	= de::max(currentCommandPoolReservedSize, maxCommandPoolReservedSize);
		currentCommandBufferAllocated	+= cbAlloc;		maxCommandBufferAllocated	= de::max(currentCommandBufferAllocated, maxCommandBufferAllocated);
	}
	void resetValues()
	{
		currentCommandPoolAllocated		= 0u;
		currentCommandPoolReservedSize	= 0u;
		currentCommandBufferAllocated	= 0u;
	}

	deUint64			commandPool;
	deUint32			commandBufferCount;
	vk::VkDeviceSize	currentCommandPoolAllocated;
	vk::VkDeviceSize	maxCommandPoolAllocated;
	vk::VkDeviceSize	currentCommandPoolReservedSize;
	vk::VkDeviceSize	maxCommandPoolReservedSize;
	vk::VkDeviceSize	currentCommandBufferAllocated;
	vk::VkDeviceSize	maxCommandBufferAllocated;
};

inline void SerializeItem(Serializer<ToRead>& serializer, VulkanCommandMemoryConsumption& v)
{
	serializer.Serialize(v.commandPool, v.commandBufferCount, v.currentCommandPoolAllocated, v.maxCommandPoolAllocated, v.currentCommandPoolReservedSize, v.maxCommandPoolReservedSize, v.currentCommandBufferAllocated, v.maxCommandBufferAllocated);
}

inline void SerializeItem(Serializer<ToWrite>& serializer, VulkanCommandMemoryConsumption& v)
{
	serializer.Serialize(v.commandPool, v.commandBufferCount, v.currentCommandPoolAllocated, v.maxCommandPoolAllocated, v.currentCommandPoolReservedSize, v.maxCommandPoolReservedSize, v.currentCommandBufferAllocated, v.maxCommandBufferAllocated);
}

struct VulkanDataTransmittedFromMainToSubprocess
{
	VulkanDataTransmittedFromMainToSubprocess ()
	{
	}
	VulkanDataTransmittedFromMainToSubprocess (const VulkanPipelineCacheInput&						pipelineCacheInput_,
											   const vk::VkDeviceObjectReservationCreateInfo&		memoryReservation_,
											   const std::vector<VulkanCommandMemoryConsumption>&	commandPoolMemoryConsumption_,
											   const std::vector<VulkanPipelineSize>&				pipelineSizes_)
		: pipelineCacheInput			(pipelineCacheInput_)
		, memoryReservation				(memoryReservation_)
		, commandPoolMemoryConsumption	(commandPoolMemoryConsumption_)
		, pipelineSizes					(pipelineSizes_)
	{
	}

	VulkanPipelineCacheInput					pipelineCacheInput;
	vk::VkDeviceObjectReservationCreateInfo		memoryReservation;
	std::vector<VulkanCommandMemoryConsumption>	commandPoolMemoryConsumption;
	std::vector<VulkanPipelineSize>				pipelineSizes;

	template <typename TYPE>
	void Serialize(Serializer<TYPE>& archive)
	{
		archive.Serialize(pipelineCacheInput, memoryReservation, commandPoolMemoryConsumption, pipelineSizes);
	}
};

struct CmdLineParams
{
	std::string											compilerPath;
	std::string											compilerDataDir;
	std::string											compilerPipelineCacheFile;
	std::string											compilerLogFile;
	std::string											compilerArgs;
};

}

#endif // _VKSSTRUCTSVKSC_HPP
