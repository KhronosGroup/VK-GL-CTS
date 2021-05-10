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
		: count	(0u)
	{
	}
	VulkanJsonPipelineDescription	(const vk::VkPipelineIdentifierInfo&	id_,
									 const string&							pipelineContents_,
									 const string&							deviceFeatures_,
									 const vector<string>&					deviceExtensions_,
									 const std::string&						test)
		: id				(id_)
		, pipelineContents	(pipelineContents_)
		, deviceFeatures	(deviceFeatures_)
		, deviceExtensions	(deviceExtensions_)
		, count				(1u)
	{
		tests.insert(test);
	}

	void add	(const std::string& test)
	{
		tests.insert(test);
		count++;
	}

	vk::VkPipelineIdentifierInfo						id;
	string												pipelineContents;
	string												deviceFeatures;
	vector<string>										deviceExtensions;
	deUint32											count;
	std::set<string>									tests;
};

inline void SerializeItem(Serializer<ToRead>& serializer, VulkanJsonPipelineDescription& v)
{
	serializer.Serialize(v.id, v.pipelineContents, v.deviceFeatures, v.deviceExtensions, v.count, v.tests);
}

inline void SerializeItem(Serializer<ToWrite>& serializer, VulkanJsonPipelineDescription& v)
{
	serializer.Serialize(v.id, v.pipelineContents, v.deviceFeatures, v.deviceExtensions, v.count, v.tests);
}

struct PipelineIdentifierEqual
{
	PipelineIdentifierEqual(const vk::VkPipelineIdentifierInfo& p)
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

	const vk::VkPipelineIdentifierInfo& searched;
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
		: commandPool				(0u)
		, commandBufferCount		(0u)
		, commandPoolAllocated		(0u)
		, commandPoolReservedSize	(0u)
		, commandBufferAllocated	(0u)
	{
	}

	VulkanCommandMemoryConsumption (deUint64 commandPool_)
		: commandPool				(commandPool_)
		, commandBufferCount		(0u)
		, commandPoolAllocated		(0u)
		, commandPoolReservedSize	(0u)
		, commandBufferAllocated	(0u)
	{
	}
	void updateValues(vk::VkDeviceSize cpAlloc, vk::VkDeviceSize cpReserved, vk::VkDeviceSize cbAlloc)
	{
		commandPoolAllocated	+= cpAlloc;
		commandPoolReservedSize	+= cpReserved;
		commandBufferAllocated	+= cbAlloc;
	}
	deUint64			commandPool;
	deUint32			commandBufferCount;
	vk::VkDeviceSize	commandPoolAllocated;
	vk::VkDeviceSize	commandPoolReservedSize;
	vk::VkDeviceSize	commandBufferAllocated;
};

inline void SerializeItem(Serializer<ToRead>& serializer, VulkanCommandMemoryConsumption& v)
{
	serializer.Serialize(v.commandPool, v.commandBufferCount, v.commandPoolAllocated, v.commandPoolReservedSize, v.commandBufferAllocated);
}

inline void SerializeItem(Serializer<ToWrite>& serializer, VulkanCommandMemoryConsumption& v)
{
	serializer.Serialize(v.commandPool, v.commandBufferCount, v.commandPoolAllocated, v.commandPoolReservedSize, v.commandBufferAllocated);
}

struct VulkanPipelineSize
{
	vk::VkPipelineIdentifierInfo						id;
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
	std::string											compilerArgs;
};

}

#endif // _VKSSTRUCTSVKSC_HPP
