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

#include "vksJson.hpp"

#include <variant>
#include <optional>

namespace vksc_server
{

struct SourceVariant
{
    string active;
    vk::GlslSource glsl;
    vk::HlslSource hlsl;
    vk::SpirVAsmSource spirv;

    template <typename TYPE>
    void Serialize(Serializer<TYPE> &archive)
    {
        archive.Serialize(active);
        if (active == "glsl")
            archive.Serialize(glsl);
        else if (active == "hlsl")
            archive.Serialize(hlsl);
        else if (active == "spirv")
            archive.Serialize(spirv);
        else
            throw std::runtime_error("incorrect shader type");
    }
};

struct YcbcrData
{
    std::string json;
    std::uint64_t uniqueObjId;
};

template <typename TYPE>
inline void SerializeItem(Serializer<TYPE> &serializer, YcbcrData &v)
{
    serializer.Serialize(v.json, v.uniqueObjId);
}

struct SamplerData
{
    std::string json;
    std::optional<YcbcrData> samplerYcbcrConversion;
    std::uint64_t uniqueObjId;
};

inline void SerializeItem(Serializer<ToRead> &serializer, SamplerData &v)
{
    std::vector<YcbcrData> samplerYcbcrConversion{};

    serializer.Serialize(v.json, samplerYcbcrConversion, v.uniqueObjId);

    if (!samplerYcbcrConversion.empty())
    {
        v.samplerYcbcrConversion = samplerYcbcrConversion[0];
    }
    else
    {
        v.samplerYcbcrConversion = std::nullopt;
    }
}

inline void SerializeItem(Serializer<ToWrite> &serializer, SamplerData &v)
{
    std::vector<YcbcrData> samplerYcbcrConversion{};

    if (v.samplerYcbcrConversion)
    {
        samplerYcbcrConversion.push_back(v.samplerYcbcrConversion.value());
    }

    serializer.Serialize(v.json, samplerYcbcrConversion, v.uniqueObjId);
}

struct DescriptorSetLayoutData
{
    std::string json;
    std::vector<SamplerData> immutableSamplers;
    std::uint64_t uniqueObjId;
};

template <typename TYPE>
inline void SerializeItem(Serializer<TYPE> &serializer, DescriptorSetLayoutData &v)
{
    serializer.Serialize(v.json, v.immutableSamplers, v.uniqueObjId);
}

struct PipelineLayoutData
{
    std::string json;
    std::vector<DescriptorSetLayoutData> descriptorSetLayouts;
    std::uint64_t uniqueObjId;
};

template <typename TYPE>
inline void SerializeItem(Serializer<TYPE> &serializer, PipelineLayoutData &v)
{
    serializer.Serialize(v.json, v.descriptorSetLayouts, v.uniqueObjId);
}

struct ShaderModuleData
{
    std::string json;
    std::uint64_t uniqueObjId;
};

template <typename TYPE>
inline void SerializeItem(Serializer<TYPE> &serializer, ShaderModuleData &v)
{
    serializer.Serialize(v.json);
}

struct RenderPassData
{
    std::string json;
    std::uint64_t uniqueObjId;
};

template <typename TYPE>
inline void SerializeItem(Serializer<TYPE> &serializer, RenderPassData &v)
{
    serializer.Serialize(v.json);
}

struct GraphicsPipelineData
{
    std::string json;
    PipelineLayoutData pipelineLayoutData;
    RenderPassData renderPassData;
    std::vector<ShaderModuleData> shaderModuleData;
    std::uint64_t uniqueObjId;
};

template <typename TYPE>
inline void SerializeItem(Serializer<TYPE> &serializer, GraphicsPipelineData &v)
{
    serializer.Serialize(v.json, v.pipelineLayoutData, v.renderPassData, v.shaderModuleData, v.uniqueObjId);
}

struct ComputePipelineData
{
    std::string json;
    PipelineLayoutData pipelineLayoutData;
    ShaderModuleData shaderModuleData;
    std::uint64_t uniqueObjId;
};

template <typename TYPE>
inline void SerializeItem(Serializer<TYPE> &serializer, ComputePipelineData &v)
{
    serializer.Serialize(v.json, v.pipelineLayoutData, v.shaderModuleData, v.uniqueObjId);
}

struct VulkanJsonPipelineDescription
{
    VulkanJsonPipelineDescription() : currentCount(0u), maxCount(0u), allCount(0u)
    {
    }
    VulkanJsonPipelineDescription(const vk::VkPipelineOfflineCreateInfo &id_,
                                  const std::variant<ComputePipelineData, GraphicsPipelineData> &pipelineData_,
                                  const string &deviceFeatures_, const vector<string> &deviceExtensions_,
                                  const std::string &test)
        : id(id_)
        , pipelineData(pipelineData_)
        , deviceFeatures(deviceFeatures_)
        , deviceExtensions(deviceExtensions_)
        , currentCount(1u)
        , maxCount(1u)
        , allCount(1u)
    {
        tests.insert(test);
    }

    void add(const std::string &test)
    {
        tests.insert(test);
        allCount++;
        currentCount++;
        maxCount = de::max(maxCount, currentCount);
    }

    void remove()
    {
        currentCount--;
    }

    vk::VkPipelineOfflineCreateInfo id;
    std::variant<ComputePipelineData, GraphicsPipelineData> pipelineData;
    std::string deviceFeatures;
    vector<std::string> deviceExtensions;

    uint32_t currentCount;
    uint32_t maxCount;
    uint32_t allCount;
    std::set<string> tests;
};

inline void SerializeItem(Serializer<ToRead> &serializer, VulkanJsonPipelineDescription &v)
{
    std::vector<ComputePipelineData> computePipelineData{};
    std::vector<GraphicsPipelineData> graphicsPipelineData{};

    serializer.Serialize(v.id, computePipelineData, graphicsPipelineData, v.deviceFeatures, v.deviceExtensions,
                         v.currentCount, v.maxCount, v.allCount, v.tests);

    if (!computePipelineData.empty())
    {
        v.pipelineData = computePipelineData[0];
    }
    else if (!graphicsPipelineData.empty())
    {
        v.pipelineData = graphicsPipelineData[0];
    }
    else
    {
        TCU_THROW(InternalError, "Reading empty variant. This should not have happened.");
    }
}

inline void SerializeItem(Serializer<ToWrite> &serializer, VulkanJsonPipelineDescription &v)
{
    std::vector<ComputePipelineData> computePipelineData{};
    std::vector<GraphicsPipelineData> graphicsPipelineData{};

    if (std::holds_alternative<ComputePipelineData>(v.pipelineData))
    {
        computePipelineData.push_back(std::get<ComputePipelineData>(v.pipelineData));
    }
    else if (std::holds_alternative<GraphicsPipelineData>(v.pipelineData))
    {
        graphicsPipelineData.push_back(std::get<GraphicsPipelineData>(v.pipelineData));
    }
    else
    {
        TCU_THROW(InternalError, "Writing empty variant. This should not have happened.");
    }

    serializer.Serialize(v.id, computePipelineData, graphicsPipelineData, v.deviceFeatures, v.deviceExtensions,
                         v.currentCount, v.maxCount, v.allCount, v.tests);
}

struct VulkanPipelineSize
{
    vk::VkPipelineOfflineCreateInfo id;
    uint32_t count;
    uint32_t size;
};

inline void SerializeItem(Serializer<ToRead> &serializer, VulkanPipelineSize &v)
{
    serializer.Serialize(v.id, v.count, v.size);
}

inline void SerializeItem(Serializer<ToWrite> &serializer, VulkanPipelineSize &v)
{
    serializer.Serialize(v.id, v.count, v.size);
}

struct PipelineIdentifierEqual
{
    PipelineIdentifierEqual(const vk::VkPipelineOfflineCreateInfo &p) : searched(p)
    {
    }
    bool operator()(const vksc_server::VulkanJsonPipelineDescription &item) const
    {
        for (uint32_t i = 0; i < VK_UUID_SIZE; ++i)
            if (searched.pipelineIdentifier[i] != item.id.pipelineIdentifier[i])
                return false;
        return true;
    }
    bool operator()(const vksc_server::VulkanPipelineSize &item) const
    {
        for (uint32_t i = 0; i < VK_UUID_SIZE; ++i)
            if (searched.pipelineIdentifier[i] != item.id.pipelineIdentifier[i])
                return false;
        return true;
    }

    const vk::VkPipelineOfflineCreateInfo &searched;
};

struct VulkanPipelineCacheInput
{
    std::map<vk::VkSamplerYcbcrConversion, YcbcrData> samplerYcbcrConversions;
    std::map<vk::VkSampler, SamplerData> samplers;
    std::map<vk::VkDescriptorSetLayout, DescriptorSetLayoutData> descriptorSetLayouts;
    std::map<vk::VkPipelineLayout, PipelineLayoutData> pipelineLayouts;
    std::map<vk::VkShaderModule, ShaderModuleData> shaderModules;
    std::map<vk::VkRenderPass, RenderPassData> renderPasses;
    std::vector<VulkanJsonPipelineDescription> pipelines;

    template <typename TYPE>
    void Serialize(Serializer<TYPE> &archive)
    {
        archive.Serialize(pipelines);
    }
};

inline void SerializeItem(Serializer<ToRead> &serializer, VulkanPipelineCacheInput &v)
{
    serializer.Serialize(v.pipelines);
}

inline void SerializeItem(Serializer<ToWrite> &serializer, VulkanPipelineCacheInput &v)
{
    serializer.Serialize(v.pipelines);
}

struct VulkanCommandMemoryConsumption
{
    VulkanCommandMemoryConsumption()
        : commandPool(0u)
        , commandBufferCount(0u)
        , currentCommandPoolAllocated(0u)
        , maxCommandPoolAllocated(0u)
        , currentCommandPoolReservedSize(0u)
        , maxCommandPoolReservedSize(0u)
        , currentCommandBufferAllocated(0u)
        , maxCommandBufferAllocated(0u)
    {
    }

    VulkanCommandMemoryConsumption(uint64_t commandPool_)
        : commandPool(commandPool_)
        , commandBufferCount(0u)
        , currentCommandPoolAllocated(0u)
        , maxCommandPoolAllocated(0u)
        , currentCommandPoolReservedSize(0u)
        , maxCommandPoolReservedSize(0u)
        , currentCommandBufferAllocated(0u)
        , maxCommandBufferAllocated(0u)
    {
    }
    void updateValues(vk::VkDeviceSize cpAlloc, vk::VkDeviceSize cpReserved, vk::VkDeviceSize cbAlloc)
    {
        currentCommandPoolAllocated += cpAlloc;
        maxCommandPoolAllocated = de::max(currentCommandPoolAllocated, maxCommandPoolAllocated);
        currentCommandPoolReservedSize += cpReserved;
        maxCommandPoolReservedSize = de::max(currentCommandPoolReservedSize, maxCommandPoolReservedSize);
        currentCommandBufferAllocated += cbAlloc;
        maxCommandBufferAllocated = de::max(currentCommandBufferAllocated, maxCommandBufferAllocated);
    }
    void resetValues()
    {
        currentCommandPoolAllocated    = 0u;
        currentCommandPoolReservedSize = 0u;
        currentCommandBufferAllocated  = 0u;
    }

    uint64_t commandPool;
    uint32_t commandBufferCount;
    vk::VkDeviceSize currentCommandPoolAllocated;
    vk::VkDeviceSize maxCommandPoolAllocated;
    vk::VkDeviceSize currentCommandPoolReservedSize;
    vk::VkDeviceSize maxCommandPoolReservedSize;
    vk::VkDeviceSize currentCommandBufferAllocated;
    vk::VkDeviceSize maxCommandBufferAllocated;
};

inline void SerializeItem(Serializer<ToRead> &serializer, VulkanCommandMemoryConsumption &v)
{
    serializer.Serialize(v.commandPool, v.commandBufferCount, v.currentCommandPoolAllocated, v.maxCommandPoolAllocated,
                         v.currentCommandPoolReservedSize, v.maxCommandPoolReservedSize,
                         v.currentCommandBufferAllocated, v.maxCommandBufferAllocated);
}

inline void SerializeItem(Serializer<ToWrite> &serializer, VulkanCommandMemoryConsumption &v)
{
    serializer.Serialize(v.commandPool, v.commandBufferCount, v.currentCommandPoolAllocated, v.maxCommandPoolAllocated,
                         v.currentCommandPoolReservedSize, v.maxCommandPoolReservedSize,
                         v.currentCommandBufferAllocated, v.maxCommandBufferAllocated);
}

struct VulkanDataTransmittedFromMainToSubprocess
{
    VulkanDataTransmittedFromMainToSubprocess()
    {
    }
    VulkanDataTransmittedFromMainToSubprocess(
        const VulkanPipelineCacheInput &pipelineCacheInput_,
        const vk::VkDeviceObjectReservationCreateInfo &memoryReservation_,
        const std::vector<VulkanCommandMemoryConsumption> &commandPoolMemoryConsumption_,
        const std::vector<VulkanPipelineSize> &pipelineSizes_)
        : pipelineCacheInput(pipelineCacheInput_)
        , memoryReservation(memoryReservation_)
        , commandPoolMemoryConsumption(commandPoolMemoryConsumption_)
        , pipelineSizes(pipelineSizes_)
    {
    }

    VulkanPipelineCacheInput pipelineCacheInput;
    vk::VkDeviceObjectReservationCreateInfo memoryReservation;
    std::vector<VulkanCommandMemoryConsumption> commandPoolMemoryConsumption;
    std::vector<VulkanPipelineSize> pipelineSizes;

    template <typename TYPE>
    void Serialize(Serializer<TYPE> &archive)
    {
        archive.Serialize(pipelineCacheInput, memoryReservation, commandPoolMemoryConsumption, pipelineSizes);
    }
};

struct CmdLineParams
{
    std::string compilerPath;
    std::string compilerDataDir;
    std::string compilerPipelineCacheFile;
    std::string compilerLogFile;
    std::string compilerArgs;
};

} // namespace vksc_server

#endif // _VKSSTRUCTSVKSC_HPP
