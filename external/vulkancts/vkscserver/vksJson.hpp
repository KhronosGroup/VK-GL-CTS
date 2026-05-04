#ifndef _VKSJSON_HPP
#define _VKSJSON_HPP

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

#include "vksCommon.hpp"

#include "vkPrograms.hpp"

#include "vkDefs.hpp"
#include <vulkan/pcjson/vksc_pipeline_json.h>
using namespace vk;

namespace Json
{
class CharReader;
class FastWriter;
} // namespace Json

namespace vksc_server
{

struct VulkanJsonPipelineDescription;

namespace json
{

struct OwningVpjShaderFilenames
{
    std::vector<std::string> storage;
    std::vector<VpjShaderFileName> filenames;
};

vk::VkShaderStageFlagBits stage_string_to_bit(const std::string &stage);
const char *stage_bit_to_string(const vk::VkShaderStageFlagBits stage);
OwningVpjShaderFilenames getShaderFilenames(const vk::VkComputePipelineCreateInfo &ci, const std::string &prefix,
                                            const uint32_t index);
OwningVpjShaderFilenames getShaderFilenames(const vk::VkGraphicsPipelineCreateInfo &ci, const std::string &prefix,
                                            const uint32_t index);

struct Context
{
    Context();
    ~Context();
    void *parser;
    void *gen;
    std::unique_ptr<Json::CharReader> reader;
    std::unique_ptr<Json::FastWriter> writer;
};

void runGarbageCollection(Context &context);

string writeJSON_VkGraphicsPipelineCreateInfo(const Context &context,
                                              const vk::VkGraphicsPipelineCreateInfo &pCreateInfo);
string writeJSON_VkComputePipelineCreateInfo(const Context &context,
                                             const vk::VkComputePipelineCreateInfo &pCreateInfo);
string writeJSON_VkRenderPassCreateInfo(const Context &context, const vk::VkRenderPassCreateInfo &pCreateInfo);
string writeJSON_VkRenderPassCreateInfo2(const Context &context, const vk::VkRenderPassCreateInfo2 &pCreateInfo);
string writeJSON_VkPipelineLayoutCreateInfo(const Context &context, const vk::VkPipelineLayoutCreateInfo &pCreateInfo);
string writeJSON_VkDescriptorSetLayoutCreateInfo(const Context &context,
                                                 const vk::VkDescriptorSetLayoutCreateInfo &pCreateInfo);
string writeJSON_VkSamplerCreateInfo(const Context &context, const vk::VkSamplerCreateInfo &pCreateInfo);
string writeJSON_VkSamplerYcbcrConversionCreateInfo(const Context &context,
                                                    const vk::VkSamplerYcbcrConversionCreateInfo &pCreateInfo);
string writeJSON_VkShaderModuleCreateInfo(const Context &context, const vk::VkShaderModuleCreateInfo &smCI);
string writeJSON_VkDeviceObjectReservationCreateInfo(const Context &context,
                                                     const vk::VkDeviceObjectReservationCreateInfo &dmrCI);
string writeJSON_VkPipelineOfflineCreateInfo(const Context &context, const vk::VkPipelineOfflineCreateInfo &piInfo);
string writeJSON_GraphicsPipeline_vkpccjson(Context &context,
                                            const vksc_server::VulkanJsonPipelineDescription &pipelineDescription,
                                            const std::string &filePrefix, const uint32_t pipelineIndex);
string writeJSON_ComputePipeline_vkpccjson(Context &context,
                                           const vksc_server::VulkanJsonPipelineDescription &pipelineDescription,
                                           const std::string &filePrefix, const uint32_t pipelineIndex);
string writeJSON_VkPhysicalDeviceFeatures2(const Context &context, const vk::VkPhysicalDeviceFeatures2 &features);
string writeJSON_VkPhysicalDeviceFeatures2(const Context &context, const vk::VkDeviceCreateInfo &device_create_info);

void readJSON_VkGraphicsPipelineCreateInfo(Context &context, const string &graphicsPipelineCreateInfo,
                                           vk::VkGraphicsPipelineCreateInfo &gpCI);
void readJSON_VkComputePipelineCreateInfo(Context &context, const string &computePipelineCreateInfo,
                                          vk::VkComputePipelineCreateInfo &cpCI);
void readJSON_VkRenderPassCreateInfo(Context &context, const string &renderPassCreateInfo,
                                     vk::VkRenderPassCreateInfo &rpCI);
void readJSON_VkRenderPassCreateInfo2(Context &context, const string &renderPassCreateInfo,
                                      vk::VkRenderPassCreateInfo2 &rpCI);
void readJSON_VkDescriptorSetLayoutCreateInfo(Context &context, const string &descriptorSetLayoutCreateInfo,
                                              vk::VkDescriptorSetLayoutCreateInfo &dsCI);
void readJSON_VkPipelineLayoutCreateInfo(Context &context, const string &pipelineLayoutCreateInfo,
                                         vk::VkPipelineLayoutCreateInfo &plCI);
void readJSON_VkShaderModuleCreateInfo(Context &context, const string &shaderModuleCreate,
                                       vk::VkShaderModuleCreateInfo &smCI);
void readJSON_VkDeviceObjectReservationCreateInfo(Context &context, const string &deviceMemoryReservation,
                                                  vk::VkDeviceObjectReservationCreateInfo &dmrCI);
void readJSON_VkPipelineOfflineCreateInfo(Context &context, const string &pipelineIdentifierInfo,
                                          vk::VkPipelineOfflineCreateInfo &piInfo);
void readJSON_VkSamplerCreateInfo(Context &context, const string &samplerCreateInfo, vk::VkSamplerCreateInfo &sCI);
void readJSON_VkSamplerYcbcrConversionCreateInfo(Context &context, const std::string &samplerYcbcrConversionCreateInfo,
                                                 vk::VkSamplerYcbcrConversionCreateInfo &sycCI);
void readJSON_VkPhysicalDeviceFeatures2(Context &context, const std::string &featuresJson,
                                        vk::VkPhysicalDeviceFeatures2 &features);

} // namespace json

} // namespace vksc_server

#endif // _VKSJSON_HPP
