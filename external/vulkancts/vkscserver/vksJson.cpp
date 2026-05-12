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

#include "vksJson.hpp"

#define VULKAN_JSON_CTS
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wvolatile"
#endif // defined(__GNUC__) && !defined(__clang__)
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wpointer-bool-conversion"
#pragma clang diagnostic ignored "-Wdeprecated-volatile"
#endif

#include <json/json.h>
#include <vulkan/pcjson/vksc_pipeline_json.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif // defined(__GNUC__) && !defined(__clang__)
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "vksStructsVKSC.hpp"
#include "vkQueryUtil.hpp"

using namespace vk;

namespace vksc_server
{

namespace json
{

// Helper class to remap and store object names
template <typename T>
struct PipelineLayoutAndChildObjectInfo
{
    vk::VkPipelineLayoutCreateInfo pipelineLayout{};
    std::vector<vk::VkDescriptorSetLayoutCreateInfo> descriptorSetLayouts{};
    std::vector<vk::VkSamplerCreateInfo> immutableSamplers{};
    std::vector<vk::VkSamplerYcbcrConversionCreateInfo> ycbcrSamplers{};
    std::vector<std::string> namesStorage{};
    std::vector<const char *> descriptorSetLayoutNames{};
    std::vector<uint64_t> descriptorSetLayoutIds{};
    std::vector<const char *> immutableSamplerNames{};
    std::vector<uint64_t> immutableSamplerIds{};
    std::vector<const char *> ycbcrSamplerNames{};
    std::vector<uint64_t> ycbcrSamplerIds{};

    PipelineLayoutAndChildObjectInfo(Context &context, T &state,
                                     const vksc_server::PipelineLayoutData &pipelineLayoutData)
    {
        readJSON_VkPipelineLayoutCreateInfo(context, pipelineLayoutData.json, pipelineLayout);

        uint32_t namesRequired = 0;
        for (size_t i = 0; i < pipelineLayoutData.descriptorSetLayouts.size(); ++i)
        {
            ++namesRequired;
            for (size_t j = 0; j < pipelineLayoutData.descriptorSetLayouts[i].immutableSamplers.size(); ++j)
            {
                ++namesRequired;
                if (pipelineLayoutData.descriptorSetLayouts[i].immutableSamplers[j].samplerYcbcrConversion.has_value())
                {
                    ++namesRequired;
                }
            }
        }
        namesStorage.reserve(namesRequired);
        auto notContains = [](const auto &container, const uint64_t uniqueObjId)
        { return std::find(std::cbegin(container), std::cend(container), uniqueObjId) == std::cend(container); };
        for (size_t i = 0; i < pipelineLayoutData.descriptorSetLayouts.size(); ++i)
        {
            const auto &descriptorSetLayout = pipelineLayoutData.descriptorSetLayouts[i];
            if (notContains(descriptorSetLayoutIds, descriptorSetLayout.uniqueObjId))
            {
                namesStorage.push_back(std::to_string(descriptorSetLayoutIds.size() + 1));
                descriptorSetLayoutIds.push_back(descriptorSetLayout.uniqueObjId);
                descriptorSetLayoutNames.push_back(namesStorage.back().c_str());
                descriptorSetLayouts.push_back({});
                readJSON_VkDescriptorSetLayoutCreateInfo(context, descriptorSetLayout.json,
                                                         descriptorSetLayouts.back());
            }
            for (size_t j = 0; j < pipelineLayoutData.descriptorSetLayouts[i].immutableSamplers.size(); ++j)
            {
                const auto &immutableSampler = descriptorSetLayout.immutableSamplers[j];
                if (notContains(immutableSamplerIds, immutableSampler.uniqueObjId))
                {
                    namesStorage.push_back(std::to_string(immutableSamplerNames.size() + 1));
                    immutableSamplerIds.push_back(immutableSampler.uniqueObjId);
                    immutableSamplerNames.push_back(namesStorage.back().c_str());
                    immutableSamplers.push_back({});
                    readJSON_VkSamplerCreateInfo(context, immutableSampler.json, immutableSamplers.back());
                }
                if (immutableSampler.samplerYcbcrConversion.has_value())
                {
                    const auto &ycbcrSampler = immutableSampler.samplerYcbcrConversion.value();
                    if (notContains(ycbcrSamplerIds, ycbcrSampler.uniqueObjId))
                    {
                        namesStorage.push_back(std::to_string(ycbcrSamplerNames.size() + 1));
                        ycbcrSamplerIds.push_back(ycbcrSampler.uniqueObjId);
                        ycbcrSamplerNames.push_back(namesStorage.back().c_str());
                        ycbcrSamplers.push_back({});
                        readJSON_VkSamplerYcbcrConversionCreateInfo(context, ycbcrSampler.json, ycbcrSamplers.back());
                    }
                }
            }
        }
        // Rewrite autoinc ids to indices
        auto findId = [](const auto &container, const uint64_t uniqueObjId)
        { return std::find(std::cbegin(container), std::cend(container), uniqueObjId); };
        for (size_t i = 0; i < pipelineLayout.setLayoutCount; ++i)
        {
            auto pipelineLayoutSets = const_cast<VkDescriptorSetLayout *>(pipelineLayout.pSetLayouts);
            pipelineLayoutSets[i]   = (const void *)std::distance(
                std::cbegin(descriptorSetLayoutIds),
                findId(descriptorSetLayoutIds, pipelineLayout.pSetLayouts[i].getInternal()));
        }
        for (auto &descriptorSetLayout : descriptorSetLayouts)
        {
            for (size_t i = 0; i < descriptorSetLayout.bindingCount; ++i)
            {
                if (descriptorSetLayout.pBindings[i].pImmutableSamplers)
                {
                    for (size_t j = 0; j < descriptorSetLayout.pBindings[i].descriptorCount; ++j)
                    {
                        auto bindingImmutableSamplers =
                            const_cast<VkSampler *>(descriptorSetLayout.pBindings[i].pImmutableSamplers);
                        bindingImmutableSamplers[j] = (const void *)std::distance(
                            std::cbegin(immutableSamplerIds),
                            findId(immutableSamplerIds,
                                   descriptorSetLayout.pBindings[i].pImmutableSamplers[j].getInternal()));
                    }
                }
            }
        }
        for (auto &immutableSampler : immutableSamplers)
        {
            auto ycbcr = findStructure<VkSamplerYcbcrConversionInfo>(const_cast<void *>(immutableSampler.pNext));
            if (ycbcr)
            {
                ycbcr->conversion = (const void *)std::distance(
                    std::cbegin(ycbcrSamplerIds), findId(ycbcrSamplerIds, ycbcr->conversion.getInternal()));
            }
        }

        // Write to state
        state.pPipelineLayout = &pipelineLayout;

        state.descriptorSetLayoutCount   = static_cast<uint32_t>(descriptorSetLayouts.size());
        state.pDescriptorSetLayouts      = descriptorSetLayouts.size() ? descriptorSetLayouts.data() : nullptr;
        state.ppDescriptorSetLayoutNames = descriptorSetLayoutNames.size() ? descriptorSetLayoutNames.data() : nullptr;

        state.immutableSamplerCount   = static_cast<uint32_t>(immutableSamplers.size());
        state.pImmutableSamplers      = immutableSamplers.size() ? immutableSamplers.data() : 0;
        state.ppImmutableSamplerNames = immutableSamplerNames.size() ? immutableSamplerNames.data() : nullptr;

        state.ycbcrSamplerCount   = static_cast<uint32_t>(ycbcrSamplers.size());
        state.pYcbcrSamplers      = ycbcrSamplers.size() ? ycbcrSamplers.data() : nullptr;
        state.ppYcbcrSamplerNames = ycbcrSamplerNames.size() ? ycbcrSamplerNames.data() : nullptr;
    }
};

Context::Context() : parser{vpjCreateParser()}, gen{vpjCreateGenerator()}, reader{nullptr}, writer{new Json::FastWriter}
{
    Json::CharReaderBuilder builder;
    builder.settings_["allowSpecialFloats"] = 1;
    reader.reset(builder.newCharReader());
}

Context::~Context()
{
    vpjDestroyParser(parser);
    vpjDestroyGenerator(gen);
}

void runGarbageCollection(Context &context)
{
    vpjFreeParserOutputs(context.parser);
    Json::CharReaderBuilder builder;
    builder.settings_["allowSpecialFloats"] = 1;
    context.reader.reset(builder.newCharReader());
}

std::vector<const char *> serializeCStringPtrs(const std::vector<std::string> &v)
{
    std::vector<const char *> res;
    res.reserve(v.size());
    std::transform(v.begin(), v.end(), std::back_insert_iterator(res), [](const std::string &s) { return s.c_str(); });
    return res;
};
const char *stage_bit_to_string(const vk::VkShaderStageFlagBits stage)
{
    switch (stage)
    {
    case vk::VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT:
        return "vert";
        break;
    case vk::VkShaderStageFlagBits::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        return "tesc";
        break;
    case vk::VkShaderStageFlagBits::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        return "tese";
        break;
    case vk::VkShaderStageFlagBits::VK_SHADER_STAGE_GEOMETRY_BIT:
        return "geom";
        break;
    case vk::VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT:
        return "frag";
        break;
    case vk::VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT:
        return "comp";
        break;
    default:
        TCU_THROW(InternalError, "Unrecognized shader stage");
    }
};

OwningVpjShaderFilenames getShaderFilenames(const vk::VkComputePipelineCreateInfo &ci, const std::string &prefix,
                                            const uint32_t index)
{
    const auto shader_filename = [&](const vk::VkPipelineShaderStageCreateInfo &pss_ci) -> std::string
    {
        return prefix + "shader_" + std::to_string(index) + '_' + std::to_string(pss_ci.module.getInternal()) + '.' +
               stage_bit_to_string(pss_ci.stage) + ".spv";
    };
    OwningVpjShaderFilenames res;
    res.storage.emplace_back(shader_filename(ci.stage));
    res.filenames.push_back(
        VpjShaderFileName{static_cast<int32_t>(ci.stage.stage), res.storage.back().c_str(), 0, nullptr});
    return res;
};
OwningVpjShaderFilenames getShaderFilenames(const vk::VkGraphicsPipelineCreateInfo &ci, const std::string &prefix,
                                            const uint32_t index)
{
    const auto shader_filename = [&](const vk::VkPipelineShaderStageCreateInfo &pss_ci) -> std::string
    {
        return prefix + "shader_" + std::to_string(index) + '_' + std::to_string(pss_ci.module.getInternal()) + '.' +
               stage_bit_to_string(pss_ci.stage) + ".spv";
    };
    OwningVpjShaderFilenames acc;
    for (uint32_t i = 0u; i < ci.stageCount; ++i)
    {
        const auto &pss_ci = ci.pStages[i];
        acc.storage.emplace_back(shader_filename(pss_ci));
        acc.filenames.push_back(
            VpjShaderFileName{static_cast<int32_t>(pss_ci.stage), acc.storage.back().c_str(), 0, nullptr});
    }
    return acc;
};

string writeJSON_VkGraphicsPipelineCreateInfo(const Context &context,
                                              const vk::VkGraphicsPipelineCreateInfo &pCreateInfo)
{
    const char *str, *msg;
    if (!vpjGenerateSingleStructJson(context.gen, &pCreateInfo, &str, &msg))
    {
        TCU_THROW(InternalError, msg);
    }
    std::string res = str;
    vpjFreeGeneratorOutputs(context.gen);
    return res;
}

string writeJSON_VkComputePipelineCreateInfo(const Context &context, const vk::VkComputePipelineCreateInfo &pCreateInfo)
{
    const char *str, *msg;
    if (!vpjGenerateSingleStructJson(context.gen, &pCreateInfo, &str, &msg))
    {
        TCU_THROW(InternalError, msg);
    }
    std::string res = str;
    vpjFreeGeneratorOutputs(context.gen);
    return res;
}

string writeJSON_VkRenderPassCreateInfo(const Context &context, const vk::VkRenderPassCreateInfo &pCreateInfo)
{
    const char *str, *msg;
    if (!vpjGenerateSingleStructJson(context.gen, &pCreateInfo, &str, &msg))
    {
        TCU_THROW(InternalError, msg);
    }
    std::string res = str;
    vpjFreeGeneratorOutputs(context.gen);
    return res;
}

string writeJSON_VkRenderPassCreateInfo2(const Context &context, const vk::VkRenderPassCreateInfo2 &pCreateInfo)
{
    const char *str, *msg;
    if (!vpjGenerateSingleStructJson(context.gen, &pCreateInfo, &str, &msg))
    {
        TCU_THROW(InternalError, msg);
    }
    std::string res = str;
    vpjFreeGeneratorOutputs(context.gen);
    return res;
}

string writeJSON_VkPipelineLayoutCreateInfo(const Context &context, const vk::VkPipelineLayoutCreateInfo &pCreateInfo)
{
    const char *str, *msg;
    if (!vpjGenerateSingleStructJson(context.gen, &pCreateInfo, &str, &msg))
    {
        TCU_THROW(InternalError, msg);
    }
    std::string res = str;
    vpjFreeGeneratorOutputs(context.gen);
    return res;
}

string writeJSON_VkDescriptorSetLayoutCreateInfo(const Context &context,
                                                 const vk::VkDescriptorSetLayoutCreateInfo &pCreateInfo)
{
    const char *str, *msg;
    if (!vpjGenerateSingleStructJson(context.gen, &pCreateInfo, &str, &msg))
    {
        TCU_THROW(InternalError, msg);
    }
    std::string res = str;
    vpjFreeGeneratorOutputs(context.gen);
    return res;
}

string writeJSON_VkSamplerCreateInfo(const Context &context, const vk::VkSamplerCreateInfo &pCreateInfo)
{
    const char *str, *msg;
    if (!vpjGenerateSingleStructJson(context.gen, &pCreateInfo, &str, &msg))
    {
        TCU_THROW(InternalError, msg);
    }
    std::string res = str;
    vpjFreeGeneratorOutputs(context.gen);
    return res;
}

string writeJSON_VkSamplerYcbcrConversionCreateInfo(const Context &context,
                                                    const VkSamplerYcbcrConversionCreateInfo &pCreateInfo)
{
    const char *str, *msg;
    if (!vpjGenerateSingleStructJson(context.gen, &pCreateInfo, &str, &msg))
    {
        TCU_THROW(InternalError, msg);
    }
    std::string res = str;
    vpjFreeGeneratorOutputs(context.gen);
    return res;
}

string writeJSON_VkShaderModuleCreateInfo(const Context &context, const VkShaderModuleCreateInfo &smCI)
{
    const char *str, *msg;
    if (!vpjGenerateSingleStructJson(context.gen, &smCI, &str, &msg))
    {
        TCU_THROW(InternalError, msg);
    }
    std::string res = str;
    vpjFreeGeneratorOutputs(context.gen);
    return res;
}

string writeJSON_VkDeviceObjectReservationCreateInfo(const Context &context,
                                                     const vk::VkDeviceObjectReservationCreateInfo &dmrCI)
{
    const char *str, *msg;
    if (!vpjGenerateSingleStructJson(context.gen, &dmrCI, &str, &msg))
    {
        TCU_THROW(InternalError, msg);
    }
    std::string res = str;
    vpjFreeGeneratorOutputs(context.gen);
    return res;
}

string writeJSON_VkPipelineOfflineCreateInfo(const Context &context, const vk::VkPipelineOfflineCreateInfo &piInfo)
{
    const char *str, *msg;
    if (!vpjGenerateSingleStructJson(context.gen, &piInfo, &str, &msg))
    {
        TCU_THROW(InternalError, msg);
    }
    std::string res = str;
    vpjFreeGeneratorOutputs(context.gen);
    return res;
}

string writeJSON_GraphicsPipeline_vkpccjson(Context &context, const VulkanJsonPipelineDescription &pipelineDescription,
                                            const std::string &filePrefix, const uint32_t pipelineIndex)
{
    if (!std::holds_alternative<GraphicsPipelineData>(pipelineDescription.pipelineData))
    {
        TCU_THROW(InternalError, "Pipeline description holds wrong type of pipeline data.");
    }

    auto &pipelineData = std::get<GraphicsPipelineData>(pipelineDescription.pipelineData);
    vk::VkGraphicsPipelineCreateInfo graphicsPipeline{};
    readJSON_VkGraphicsPipelineCreateInfo(context, pipelineData.json, graphicsPipeline);

    VpjData data{};
    data.graphicsPipelineState.pGraphicsPipeline = &graphicsPipeline;

    PipelineLayoutAndChildObjectInfo objectInfo(context, data.graphicsPipelineState, pipelineData.pipelineLayoutData);

    auto shaderFilenames                           = getShaderFilenames(graphicsPipeline, filePrefix, pipelineIndex);
    data.graphicsPipelineState.shaderFileNameCount = static_cast<uint32_t>(shaderFilenames.filenames.size());
    data.graphicsPipelineState.pShaderFileNames    = shaderFilenames.filenames.data();

    vk::VkPhysicalDeviceFeatures2 deviceFeatures2{};
    readJSON_VkPhysicalDeviceFeatures2(context, pipelineDescription.deviceFeatures, deviceFeatures2);
    data.graphicsPipelineState.pPhysicalDeviceFeatures = &deviceFeatures2;

    auto deviceExtensionCstrs  = serializeCStringPtrs(pipelineDescription.deviceExtensions);
    data.enabledExtensionCount = static_cast<uint32_t>(deviceExtensionCstrs.size());
    data.ppEnabledExtensions   = deviceExtensionCstrs.data();

    std::variant<vk::VkRenderPassCreateInfo, vk::VkRenderPassCreateInfo2> renderPass;
    if (pipelineData.renderPassData.json.find("VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO"))
    {
        renderPass = vk::VkRenderPassCreateInfo{};
        readJSON_VkRenderPassCreateInfo(context, pipelineData.renderPassData.json,
                                        std::get<vk::VkRenderPassCreateInfo>(renderPass));
        data.graphicsPipelineState.pRenderPass =
            reinterpret_cast<const void *>(&std::get<vk::VkRenderPassCreateInfo>(renderPass));
    }
    else if (pipelineData.renderPassData.json.find("VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2"))
    {
        renderPass = vk::VkRenderPassCreateInfo2{};
        readJSON_VkRenderPassCreateInfo2(context, pipelineData.renderPassData.json,
                                         std::get<vk::VkRenderPassCreateInfo2>(renderPass));
        data.graphicsPipelineState.pRenderPass =
            reinterpret_cast<const void *>(&std::get<vk::VkRenderPassCreateInfo2>(renderPass));
    }
    else
    {
        TCU_THROW(InternalError, "Renderpass create info is of unkown type.");
    }

    std::copy(pipelineDescription.id.pipelineIdentifier, pipelineDescription.id.pipelineIdentifier + VK_UUID_SIZE,
              data.pipelineUUID);

    const char *str, *msg;
    if (!vpjGeneratePipelineJson(context.gen, &data, &str, &msg))
    {
        TCU_THROW(InternalError, msg);
    }

    std::string res = str;
    vpjFreeGeneratorOutputs(context.gen);

    return res;
}

string writeJSON_ComputePipeline_vkpccjson(Context &context, const VulkanJsonPipelineDescription &pipelineDescription,
                                           const std::string &filePrefix, const uint32_t pipelineIndex)
{
    if (!std::holds_alternative<ComputePipelineData>(pipelineDescription.pipelineData))
    {
        TCU_THROW(InternalError, "Pipeline description holds wrong type of pipeline data.");
    }

    auto &pipelineData = std::get<ComputePipelineData>(pipelineDescription.pipelineData);
    vk::VkComputePipelineCreateInfo computePipeline{};
    readJSON_VkComputePipelineCreateInfo(context, pipelineData.json, computePipeline);

    VpjData data{};
    data.computePipelineState.pComputePipeline = &computePipeline;

    PipelineLayoutAndChildObjectInfo objectInfo(context, data.computePipelineState, pipelineData.pipelineLayoutData);

    auto shaderFilenames                          = getShaderFilenames(computePipeline, filePrefix, pipelineIndex);
    data.computePipelineState.shaderFileNameCount = static_cast<uint32_t>(shaderFilenames.filenames.size());
    data.computePipelineState.pShaderFileNames    = shaderFilenames.filenames.data();

    vk::VkPhysicalDeviceFeatures2 deviceFeatures2{};
    readJSON_VkPhysicalDeviceFeatures2(context, pipelineDescription.deviceFeatures, deviceFeatures2);
    data.computePipelineState.pPhysicalDeviceFeatures = &deviceFeatures2;

    auto deviceExtensionCstrs  = serializeCStringPtrs(pipelineDescription.deviceExtensions);
    data.enabledExtensionCount = static_cast<uint32_t>(deviceExtensionCstrs.size());
    data.ppEnabledExtensions   = deviceExtensionCstrs.data();

    std::copy(pipelineDescription.id.pipelineIdentifier, pipelineDescription.id.pipelineIdentifier + VK_UUID_SIZE,
              data.pipelineUUID);

    const char *str, *msg;
    if (!vpjGeneratePipelineJson(context.gen, &data, &str, &msg))
    {
        TCU_THROW(InternalError, msg);
    }

    std::string res = str;
    vpjFreeGeneratorOutputs(context.gen);

    return res;
}

string writeJSON_VkPhysicalDeviceFeatures2(const Context &context, const vk::VkPhysicalDeviceFeatures2 &features)
{
    const char *str, *msg;
    if (!vpjGenerateSingleStructJson(context.gen, &features, &str, &msg))
    {
        TCU_THROW(InternalError, msg);
    }
    std::string res = str;
    vpjFreeGeneratorOutputs(context.gen);
    return res;
}

string writeJSON_VkPhysicalDeviceFeatures2(const Context &context, const vk::VkDeviceCreateInfo &pCreateInfo)
{
    const char *msg;
    const void *in_chain                      = pCreateInfo.pNext;
    const void *out_features                  = nullptr;
    VkPhysicalDeviceFeatures2 deviceFeatures2 = initVulkanStructure();

    if (!findStructureInChain(in_chain, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2) &&
        pCreateInfo.pEnabledFeatures != NULL)
    {
        deviceFeatures2.features = *(pCreateInfo.pEnabledFeatures);
        deviceFeatures2.pNext    = (void *)pCreateInfo.pNext;
    }

    if (!vpjFilterDeviceFeatures(context.gen, in_chain, &out_features, &msg))
    {
        TCU_THROW(InternalError, msg);
    }

    return writeJSON_VkPhysicalDeviceFeatures2(context,
                                               *reinterpret_cast<const VkPhysicalDeviceFeatures2 *>(out_features));
}

void readJSON_VkGraphicsPipelineCreateInfo(Context &context, const string &graphicsPipelineCreateInfo,
                                           vk::VkGraphicsPipelineCreateInfo &gpCI)
{
    const char *msg;
    if (!vpjParseSingleStructJson(context.parser, graphicsPipelineCreateInfo.c_str(), &gpCI, &msg))
    {
        TCU_THROW(InternalError, msg);
    }
}

void readJSON_VkComputePipelineCreateInfo(Context &context, const string &computePipelineCreateInfo,
                                          vk::VkComputePipelineCreateInfo &cpCI)
{
    const char *msg;
    if (!vpjParseSingleStructJson(context.parser, computePipelineCreateInfo.c_str(), &cpCI, &msg))
    {
        TCU_THROW(InternalError, msg);
    }
}

void readJSON_VkRenderPassCreateInfo(Context &context, const string &renderPassCreateInfo, VkRenderPassCreateInfo &rpCI)
{
    const char *msg;
    if (!vpjParseSingleStructJson(context.parser, renderPassCreateInfo.c_str(), &rpCI, &msg))
    {
        TCU_THROW(InternalError, msg);
    }
}

void readJSON_VkRenderPassCreateInfo2(Context &context, const string &renderPassCreateInfo,
                                      vk::VkRenderPassCreateInfo2 &rpCI)
{
    const char *msg;
    if (!vpjParseSingleStructJson(context.parser, renderPassCreateInfo.c_str(), &rpCI, &msg))
    {
        TCU_THROW(InternalError, msg);
    }
}

void readJSON_VkDescriptorSetLayoutCreateInfo(Context &context, const string &descriptorSetLayoutCreateInfo,
                                              vk::VkDescriptorSetLayoutCreateInfo &dsCI)
{
    const char *msg;
    if (!vpjParseSingleStructJson(context.parser, descriptorSetLayoutCreateInfo.c_str(), &dsCI, &msg))
    {
        TCU_THROW(InternalError, msg);
    }
}

void readJSON_VkPipelineLayoutCreateInfo(Context &context, const string &pipelineLayoutCreateInfo,
                                         vk::VkPipelineLayoutCreateInfo &plCI)
{
    const char *msg;
    if (!vpjParseSingleStructJson(context.parser, pipelineLayoutCreateInfo.c_str(), &plCI, &msg))
    {
        TCU_THROW(InternalError, msg);
    }
}

void readJSON_VkDeviceObjectReservationCreateInfo(Context &context, const string &deviceMemoryReservation,
                                                  VkDeviceObjectReservationCreateInfo &dmrCI)
{
    const char *msg;
    if (!vpjParseSingleStructJson(context.parser, deviceMemoryReservation.c_str(), &dmrCI, &msg))
    {
        TCU_THROW(InternalError, msg);
    }
}

void readJSON_VkPipelineOfflineCreateInfo(Context &context, const string &pipelineIdentifierInfo,
                                          vk::VkPipelineOfflineCreateInfo &piInfo)
{
    const char *msg;
    if (!vpjParseSingleStructJson(context.parser, pipelineIdentifierInfo.c_str(), &piInfo, &msg))
    {
        TCU_THROW(InternalError, msg);
    }
}

void readJSON_VkSamplerCreateInfo(Context &context, const string &samplerCreateInfo, VkSamplerCreateInfo &sCI)
{
    const char *msg;
    if (!vpjParseSingleStructJson(context.parser, samplerCreateInfo.c_str(), &sCI, &msg))
    {
        TCU_THROW(InternalError, msg);
    }
}

void readJSON_VkSamplerYcbcrConversionCreateInfo(Context &context, const std::string &samplerYcbcrConversionCreateInfo,
                                                 VkSamplerYcbcrConversionCreateInfo &sycCI)
{
    const char *msg;
    if (!vpjParseSingleStructJson(context.parser, samplerYcbcrConversionCreateInfo.c_str(), &sycCI, &msg))
    {
        TCU_THROW(InternalError, msg);
    }
}

void readJSON_VkPhysicalDeviceFeatures2(Context &context, const std::string &featuresJson,
                                        vk::VkPhysicalDeviceFeatures2 &features)
{
    const char *msg;
    if (!vpjParseSingleStructJson(context.parser, featuresJson.c_str(), &features, &msg))
    {
        TCU_THROW(InternalError, msg);
    }
}

void readJSON_VkShaderModuleCreateInfo(Context &context, const string &shaderModuleCreate,
                                       VkShaderModuleCreateInfo &smCI)
{
    const char *msg;
    if (!vpjParseSingleStructJson(context.parser, shaderModuleCreate.c_str(), &smCI, &msg))
    {
        TCU_THROW(InternalError, msg);
    }
}

} // namespace json

} // namespace vksc_server
