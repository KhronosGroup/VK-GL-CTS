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

#include "vksCacheBuilder.hpp"
#include "pcreader.hpp"
#include "vksJson.hpp"

#include <fstream>
//    Currently CTS does not use C++17, so universal method of deleting files from directory has been commented out
//#include <filesystem>
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "deDirectoryIterator.hpp"
#include "deFile.h"
#include "vkSafetyCriticalUtil.hpp"

namespace vk
{

typedef VKAPI_ATTR VkResult(VKAPI_CALL *CreateSamplerYcbcrConversionFunc)(
    VkDevice device, const VkSamplerYcbcrConversionCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator,
    VkSamplerYcbcrConversion *pYcbcrConversion);
typedef VKAPI_ATTR void(VKAPI_CALL *DestroySamplerYcbcrConversionFunc)(VkDevice device,
                                                                       VkSamplerYcbcrConversion ycbcrConversion,
                                                                       const VkAllocationCallbacks *pAllocator);
typedef VKAPI_ATTR VkResult(VKAPI_CALL *CreateSamplerFunc)(VkDevice device, const VkSamplerCreateInfo *pCreateInfo,
                                                           const VkAllocationCallbacks *pAllocator,
                                                           VkSampler *pSampler);
typedef VKAPI_ATTR void(VKAPI_CALL *DestroySamplerFunc)(VkDevice device, VkSampler sampler,
                                                        const VkAllocationCallbacks *pAllocator);
typedef VKAPI_ATTR VkResult(VKAPI_CALL *CreateShaderModuleFunc)(VkDevice device,
                                                                const VkShaderModuleCreateInfo *pCreateInfo,
                                                                const VkAllocationCallbacks *pAllocator,
                                                                VkShaderModule *pShaderModule);
typedef VKAPI_ATTR void(VKAPI_CALL *DestroyShaderModuleFunc)(VkDevice device, VkShaderModule shaderModule,
                                                             const VkAllocationCallbacks *pAllocator);
typedef VKAPI_ATTR VkResult(VKAPI_CALL *CreateRenderPassFunc)(VkDevice device,
                                                              const VkRenderPassCreateInfo *pCreateInfo,
                                                              const VkAllocationCallbacks *pAllocator,
                                                              VkRenderPass *pRenderPass);
typedef VKAPI_ATTR VkResult(VKAPI_CALL *CreateRenderPass2Func)(VkDevice device,
                                                               const VkRenderPassCreateInfo2 *pCreateInfo,
                                                               const VkAllocationCallbacks *pAllocator,
                                                               VkRenderPass *pRenderPass);
typedef VKAPI_ATTR void(VKAPI_CALL *DestroyRenderPassFunc)(VkDevice device, VkRenderPass renderPass,
                                                           const VkAllocationCallbacks *pAllocator);
typedef VKAPI_ATTR VkResult(VKAPI_CALL *CreateDescriptorSetLayoutFunc)(
    VkDevice device, const VkDescriptorSetLayoutCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator,
    VkDescriptorSetLayout *pSetLayout);
typedef VKAPI_ATTR void(VKAPI_CALL *DestroyDescriptorSetLayoutFunc)(VkDevice device,
                                                                    VkDescriptorSetLayout descriptorSetLayout,
                                                                    const VkAllocationCallbacks *pAllocator);
typedef VKAPI_ATTR VkResult(VKAPI_CALL *CreatePipelineLayoutFunc)(VkDevice device,
                                                                  const VkPipelineLayoutCreateInfo *pCreateInfo,
                                                                  const VkAllocationCallbacks *pAllocator,
                                                                  VkPipelineLayout *pPipelineLayout);
typedef VKAPI_ATTR void(VKAPI_CALL *DestroyPipelineLayoutFunc)(VkDevice device, VkPipelineLayout pipelineLayout,
                                                               const VkAllocationCallbacks *pAllocator);
typedef VKAPI_ATTR VkResult(VKAPI_CALL *CreateGraphicsPipelinesFunc)(VkDevice device, VkPipelineCache pipelineCache,
                                                                     uint32_t createInfoCount,
                                                                     const VkGraphicsPipelineCreateInfo *pCreateInfos,
                                                                     const VkAllocationCallbacks *pAllocator,
                                                                     VkPipeline *pPipelines);
typedef VKAPI_ATTR VkResult(VKAPI_CALL *CreateComputePipelinesFunc)(VkDevice device, VkPipelineCache pipelineCache,
                                                                    uint32_t createInfoCount,
                                                                    const VkComputePipelineCreateInfo *pCreateInfos,
                                                                    const VkAllocationCallbacks *pAllocator,
                                                                    VkPipeline *pPipelines);
typedef VKAPI_ATTR void(VKAPI_CALL *DestroyPipelineFunc)(VkDevice device, VkPipeline pipeline,
                                                         const VkAllocationCallbacks *pAllocator);
typedef VKAPI_ATTR VkResult(VKAPI_CALL *CreatePipelineCacheFunc)(VkDevice device,
                                                                 const VkPipelineCacheCreateInfo *pCreateInfo,
                                                                 const VkAllocationCallbacks *pAllocator,
                                                                 VkPipelineCache *pPipelineCache);
typedef VKAPI_ATTR void(VKAPI_CALL *DestroyPipelineCacheFunc)(VkDevice device, VkPipelineCache pipelineCache,
                                                              const VkAllocationCallbacks *pAllocator);
typedef VKAPI_ATTR VkResult(VKAPI_CALL *GetPipelineCacheDataFunc)(VkDevice device, VkPipelineCache pipelineCache,
                                                                  uintptr_t *pDataSize, void *pData);

} // namespace vk

namespace vksc_server
{

const VkDeviceSize VKSC_DEFAULT_PIPELINE_POOL_SIZE = 2u * 1024u * 1024u;

void exportFilesForExternalCompiler(const VulkanPipelineCacheInput &input, const std::string &path,
                                    const std::string &filePrefix)
{
    // unpack JSON data to track relations between objects
    using namespace vk;
    using namespace json;
    Context jsonReader;

    std::map<VkSamplerYcbcrConversion, VkSamplerYcbcrConversionCreateInfo> allSamplerYcbcrConversions;
    for (auto &&samplerYcbcr : input.samplerYcbcrConversions)
    {
        VkSamplerYcbcrConversionCreateInfo sycCI{};
        readJSON_VkSamplerYcbcrConversionCreateInfo(jsonReader, samplerYcbcr.second, sycCI);
        allSamplerYcbcrConversions.insert({samplerYcbcr.first, sycCI});
    }

    std::map<VkSampler, VkSamplerCreateInfo> allSamplers;
    for (auto &&sampler : input.samplers)
    {
        VkSamplerCreateInfo sCI{};
        readJSON_VkSamplerCreateInfo(jsonReader, sampler.second, sCI);
        allSamplers.insert({sampler.first, sCI});
    }

    std::map<VkShaderModule, VkShaderModuleCreateInfo> allShaderModules;
    std::map<VkShaderModule, std::vector<uint8_t>> allSpirvShaders;
    for (auto &&shader : input.shaderModules)
    {
        VkShaderModuleCreateInfo smCI{};
        std::vector<uint8_t> spirvShader;
        readJSON_VkShaderModuleCreateInfo(jsonReader, shader.second, smCI, spirvShader);
        allShaderModules.insert({shader.first, smCI});
        allSpirvShaders.insert({shader.first, spirvShader});
    }

    std::map<VkRenderPass, VkRenderPassCreateInfo> allRenderPasses;
    std::map<VkRenderPass, VkRenderPassCreateInfo2> allRenderPasses2;
    for (auto &&renderPass : input.renderPasses)
    {
        if (renderPass.second.find("VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2") != std::string::npos)
        {
            VkRenderPassCreateInfo2 rpCI{};
            readJSON_VkRenderPassCreateInfo2(jsonReader, renderPass.second, rpCI);
            allRenderPasses2.insert({renderPass.first, rpCI});
        }
        else if (renderPass.second.find("VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO") != std::string::npos)
        {
            VkRenderPassCreateInfo rpCI{};
            readJSON_VkRenderPassCreateInfo(jsonReader, renderPass.second, rpCI);
            allRenderPasses.insert({renderPass.first, rpCI});
        }
        else
            TCU_THROW(InternalError, "Could not recognize render pass type");
    }

    std::map<VkDescriptorSetLayout, VkDescriptorSetLayoutCreateInfo> allDescriptorSetLayouts;
    for (auto &&descriptorSetLayout : input.descriptorSetLayouts)
    {
        VkDescriptorSetLayoutCreateInfo dsCI{};
        readJSON_VkDescriptorSetLayoutCreateInfo(jsonReader, descriptorSetLayout.second, dsCI);
        allDescriptorSetLayouts.insert({descriptorSetLayout.first, dsCI});
    }

    std::map<VkPipelineLayout, VkPipelineLayoutCreateInfo> allPipelineLayouts;
    for (auto &&pipelineLayout : input.pipelineLayouts)
    {
        VkPipelineLayoutCreateInfo plCI{};
        readJSON_VkPipelineLayoutCreateInfo(jsonReader, pipelineLayout.second, plCI);
        allPipelineLayouts.insert({pipelineLayout.first, plCI});
    }

    uint32_t exportedPipelines = 0;

    for (auto &&pipeline : input.pipelines)
    {
        // filter objects used for this specific pipeline ( graphics or compute )
        std::map<VkSamplerYcbcrConversion, VkSamplerYcbcrConversionCreateInfo> samplerYcbcrConversions;
        std::map<VkSampler, VkSamplerCreateInfo> samplers;
        std::map<VkShaderModule, VkShaderModuleCreateInfo> shaderModules;
        std::map<VkShaderModule, std::vector<uint8_t>> spirvShaders;
        std::map<VkRenderPass, VkRenderPassCreateInfo> renderPasses;
        std::map<VkRenderPass, VkRenderPassCreateInfo2> renderPasses2;
        std::map<VkDescriptorSetLayout, VkDescriptorSetLayoutCreateInfo> descriptorSetLayouts;
        std::map<VkPipelineLayout, VkPipelineLayoutCreateInfo> pipelineLayouts;

        if (pipeline.pipelineContents.find("VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO") != std::string::npos)
        {
            VkGraphicsPipelineCreateInfo gpCI{};
            readJSON_VkGraphicsPipelineCreateInfo(jsonReader, pipeline.pipelineContents, gpCI);

            // copy all used shaders
            for (uint32_t i = 0; i < gpCI.stageCount; ++i)
            {
                auto it = allShaderModules.find(gpCI.pStages[i].module);
                if (it == end(allShaderModules))
                    TCU_THROW(InternalError, "Could not find shader module");
                shaderModules.insert(*it);

                auto it2 = allSpirvShaders.find(gpCI.pStages[i].module);
                if (it2 == end(allSpirvShaders))
                    TCU_THROW(InternalError, "Could not find shader");
                spirvShaders.insert(*it2);
            }

            // copy render pass
            {
                auto it = allRenderPasses.find(gpCI.renderPass);
                if (it == end(allRenderPasses))
                {
                    auto it2 = allRenderPasses2.find(gpCI.renderPass);
                    if (it2 == end(allRenderPasses2))
                        TCU_THROW(InternalError, "Could not find render pass");
                    else
                        renderPasses2.insert(*it2);
                }
                else
                    renderPasses.insert(*it);
            }

            // copy pipeline layout
            {
                auto it = allPipelineLayouts.find(gpCI.layout);
                if (it == end(allPipelineLayouts))
                    TCU_THROW(InternalError, "Could not find pipeline layout");
                pipelineLayouts.insert(*it);

                // copy descriptor set layouts
                for (uint32_t i = 0; i < it->second.setLayoutCount; ++i)
                {
                    auto it2 = allDescriptorSetLayouts.find(it->second.pSetLayouts[i]);
                    if (it2 == end(allDescriptorSetLayouts))
                        TCU_THROW(InternalError, "Could not find descriptor set layout");
                    descriptorSetLayouts.insert({it2->first, it2->second});

                    // copy samplers
                    for (uint32_t j = 0; j < it2->second.bindingCount; ++j)
                    {
                        if (it2->second.pBindings[j].pImmutableSamplers != nullptr)
                        {
                            for (uint32_t k = 0; k < it2->second.pBindings[j].descriptorCount; ++k)
                            {
                                auto it3 = allSamplers.find(it2->second.pBindings[j].pImmutableSamplers[k]);
                                if (it3 == end(allSamplers))
                                    TCU_THROW(InternalError, "Could not find sampler");
                                samplers.insert({it3->first, it3->second});

                                // copy sampler YcbcrConversion
                                if (it3->second.pNext != nullptr)
                                {
                                    VkSamplerYcbcrConversionInfo *info =
                                        (VkSamplerYcbcrConversionInfo *)findStructureInChain(
                                            it3->second.pNext, VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO);
                                    if (info->sType == VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO)
                                    {
                                        auto it4 = allSamplerYcbcrConversions.find(info->conversion);
                                        if (it4 == end(allSamplerYcbcrConversions))
                                            TCU_THROW(InternalError, "Could not find VkSamplerYcbcrConversion");
                                        samplerYcbcrConversions.insert({it4->first, it4->second});
                                    }
                                }
                            }
                        }
                    }
                }
            }

            vk::VkPhysicalDeviceFeatures2 deviceFeatures2;
            readJSON_VkPhysicalDeviceFeatures2(jsonReader, pipeline.deviceFeatures, deviceFeatures2);

            // export shaders and objects to JSON compatible with https://schema.khronos.org/vulkan/vkpcc.json
            std::string gpTxt = writeJSON_GraphicsPipeline_vkpccjson(
                filePrefix, exportedPipelines, pipeline.id, gpCI, deviceFeatures2, pipeline.deviceExtensions,
                samplerYcbcrConversions, samplers, descriptorSetLayouts, renderPasses, renderPasses2, pipelineLayouts);
            std::stringstream fileName;
#ifdef _WIN32
            fileName << path << "\\" << filePrefix << "graphics_pipeline_" << exportedPipelines << ".json";
#else
            fileName << path << "/" << filePrefix << "graphics_pipeline_" << exportedPipelines << ".json";
#endif
            {
                std::ofstream oFile(fileName.str().c_str(), std::ios::out);
                oFile << gpTxt;
            }

            for (uint32_t j = 0; j < gpCI.stageCount; ++j)
            {
                std::stringstream shaderName;
#ifdef _WIN32
                shaderName << path << "\\" << filePrefix << "shader_" << exportedPipelines << "_"
                           << gpCI.pStages[j].module.getInternal() << ".";
#else
                shaderName << path << "/" << filePrefix << "shader_" << exportedPipelines << "_"
                           << gpCI.pStages[j].module.getInternal() << ".";
#endif
                switch (gpCI.pStages[j].stage)
                {
                case VK_SHADER_STAGE_VERTEX_BIT:
                    shaderName << "vert";
                    break;
                case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
                    shaderName << "tesc";
                    break;
                case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
                    shaderName << "tese";
                    break;
                case VK_SHADER_STAGE_GEOMETRY_BIT:
                    shaderName << "geom";
                    break;
                case VK_SHADER_STAGE_FRAGMENT_BIT:
                    shaderName << "frag";
                    break;
                default:
                    TCU_THROW(InternalError, "Unrecognized shader stage");
                }
                shaderName << ".spv";

                auto sit = spirvShaders.find(gpCI.pStages[j].module);
                if (sit == end(spirvShaders))
                    TCU_THROW(InternalError, "SPIR-V shader not found");

                std::ofstream oFile(shaderName.str().c_str(), std::ios::out | std::ios::binary);
                oFile.write((const char *)sit->second.data(), sit->second.size());
            }

            exportedPipelines++;
        }
        else if (pipeline.pipelineContents.find("VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO") != std::string::npos)
        {
            VkComputePipelineCreateInfo cpCI{};
            readJSON_VkComputePipelineCreateInfo(jsonReader, pipeline.pipelineContents, cpCI);

            // copy shader
            {
                auto it = allShaderModules.find(cpCI.stage.module);
                if (it == end(allShaderModules))
                    TCU_THROW(InternalError, "Could not find shader module");
                shaderModules.insert(*it);

                auto it2 = allSpirvShaders.find(cpCI.stage.module);
                if (it2 == end(allSpirvShaders))
                    TCU_THROW(InternalError, "Could not find shader");
                spirvShaders.insert(*it2);
            }

            // copy pipeline layout
            {
                auto it = allPipelineLayouts.find(cpCI.layout);
                if (it == end(allPipelineLayouts))
                    TCU_THROW(InternalError, "Could not find pipeline layout");
                pipelineLayouts.insert(*it);

                // copy descriptor set layouts
                for (uint32_t i = 0; i < it->second.setLayoutCount; ++i)
                {
                    auto it2 = allDescriptorSetLayouts.find(it->second.pSetLayouts[i]);
                    if (it2 == end(allDescriptorSetLayouts))
                        TCU_THROW(InternalError, "Could not find descriptor set layout");
                    descriptorSetLayouts.insert({it2->first, it2->second});

                    // copy samplers
                    for (uint32_t j = 0; j < it2->second.bindingCount; ++j)
                    {
                        if (it2->second.pBindings[j].pImmutableSamplers != nullptr)
                        {
                            for (uint32_t k = 0; k < it2->second.pBindings[j].descriptorCount; ++k)
                            {
                                auto it3 = allSamplers.find(it2->second.pBindings[j].pImmutableSamplers[k]);
                                if (it3 == end(allSamplers))
                                    TCU_THROW(InternalError, "Could not find sampler");
                                samplers.insert({it3->first, it3->second});

                                // copy sampler YcbcrConversion
                                if (it3->second.pNext != nullptr)
                                {
                                    VkSamplerYcbcrConversionInfo *info =
                                        (VkSamplerYcbcrConversionInfo *)(it3->second.pNext);
                                    if (info->sType == VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO)
                                    {
                                        auto it4 = allSamplerYcbcrConversions.find(info->conversion);
                                        if (it4 == end(allSamplerYcbcrConversions))
                                            TCU_THROW(InternalError, "Could not find VkSamplerYcbcrConversion");
                                        samplerYcbcrConversions.insert({it4->first, it4->second});
                                    }
                                }
                            }
                        }
                    }
                }
            }
            vk::VkPhysicalDeviceFeatures2 deviceFeatures2;
            readJSON_VkPhysicalDeviceFeatures2(jsonReader, pipeline.deviceFeatures, deviceFeatures2);

            // export shaders and objects to JSON compatible with https://schema.khronos.org/vulkan/vkpcc.json
            std::string cpTxt = writeJSON_ComputePipeline_vkpccjson(
                filePrefix, exportedPipelines, pipeline.id, cpCI, deviceFeatures2, pipeline.deviceExtensions,
                samplerYcbcrConversions, samplers, descriptorSetLayouts, pipelineLayouts);
            std::stringstream fileName;
#ifdef _WIN32
            fileName << path << "\\" << filePrefix << "compute_pipeline_" << exportedPipelines << ".json";
#else
            fileName << path << "/" << filePrefix << "compute_pipeline_" << exportedPipelines << ".json";
#endif
            {
                std::ofstream oFile(fileName.str().c_str(), std::ios::out);
                oFile << cpTxt;
            }

            {
                std::stringstream shaderName;
#ifdef _WIN32
                shaderName << path << "\\" << filePrefix << "shader_" << exportedPipelines << "_"
                           << cpCI.stage.module.getInternal() << ".";
#else
                shaderName << path << "/" << filePrefix << "shader_" << exportedPipelines << "_"
                           << cpCI.stage.module.getInternal() << ".";
#endif
                switch (cpCI.stage.stage)
                {
                case VK_SHADER_STAGE_COMPUTE_BIT:
                    shaderName << "comp";
                    break;
                default:
                    TCU_THROW(InternalError, "Unrecognized shader stage");
                }
                shaderName << ".spv";

                auto sit = spirvShaders.find(cpCI.stage.module);
                if (sit == end(spirvShaders))
                    TCU_THROW(InternalError, "SPIR-V shader not found");

                std::ofstream oFile(shaderName.str().c_str(), std::ios::out | std::ios::binary);
                oFile.write((const char *)sit->second.data(), sit->second.size());
            }

            exportedPipelines++;
        }
    }
}

// This is function prototype for creating pipeline cache using offline pipeline compiler

vector<u8> buildOfflinePipelineCache(const VulkanPipelineCacheInput &input, const std::string &pipelineCompilerPath,
                                     const std::string &pipelineCompilerDataDir,
                                     const std::string &pipelineCompilerArgs,
                                     const std::string &pipelineCompilerOutputFile,
                                     const std::string &pipelineCompilerLogFile,
                                     const std::string &pipelineCompilerFilePrefix)
{
    if (!deFileExists(pipelineCompilerPath.c_str()))
        TCU_THROW(InternalError, std::string("Can't find pipeline compiler") + pipelineCompilerPath);
    // Remove all files from output directory
    for (de::DirectoryIterator iter(pipelineCompilerDataDir); iter.hasItem(); iter.next())
    {
        const de::FilePath filePath = iter.getItem();
        if (filePath.getType() != de::FilePath::TYPE_FILE)
            continue;
        if (!pipelineCompilerFilePrefix.empty() && filePath.getBaseName().find(pipelineCompilerFilePrefix) != 0)
            continue;
        deDeleteFile(filePath.getPath());
    }

    // export new files
    exportFilesForExternalCompiler(input, pipelineCompilerDataDir, pipelineCompilerFilePrefix);
    if (input.pipelines.size() == 0)
        return vector<u8>();

    // run offline pipeline compiler
    {
        std::stringstream compilerCommand;
        compilerCommand << pipelineCompilerPath << " --path " << pipelineCompilerDataDir << " --out "
                        << pipelineCompilerOutputFile;
        if (!pipelineCompilerLogFile.empty())
            compilerCommand << " --log " << pipelineCompilerLogFile;
        if (!pipelineCompilerFilePrefix.empty())
            compilerCommand << " --prefix " << pipelineCompilerFilePrefix;
        if (!pipelineCompilerArgs.empty())
            compilerCommand << " " << pipelineCompilerArgs;

        std::string command = compilerCommand.str();
        int returnValue     = system(command.c_str());
        // offline pipeline compiler returns EXIT_SUCCESS on success
        if (returnValue != EXIT_SUCCESS)
        {
            TCU_THROW(InternalError, "offline pipeline compilation failed");
        }
    }

    // read created pipeline cache into result vector
    vector<u8> result;
    {
        std::ifstream iFile(pipelineCompilerOutputFile.c_str(), std::ios::in | std::ios::binary);
        if (!iFile)
            TCU_THROW(InternalError, (std::string("Cannot open file ") + pipelineCompilerOutputFile).c_str());

        auto fileBegin = iFile.tellg();
        iFile.seekg(0, std::ios::end);
        auto fileEnd = iFile.tellg();
        iFile.seekg(0, std::ios::beg);
        std::size_t fileSize = static_cast<std::size_t>(fileEnd - fileBegin);
        if (fileSize > 0)
        {
            result.resize(fileSize);
            iFile.read(reinterpret_cast<char *>(result.data()), fileSize);
            if (iFile.fail())
                TCU_THROW(InternalError, (std::string("Cannot load file ") + pipelineCompilerOutputFile).c_str());
        }
    }
    return result;
}

vector<u8> buildPipelineCache(const VulkanPipelineCacheInput &input, const vk::PlatformInterface &vkp,
                              vk::VkInstance instance, const vk::InstanceInterface &vki,
                              vk::VkPhysicalDevice physicalDevice, uint32_t queueIndex)
{
    using namespace vk;
    using namespace json;

    Context jsonReader;

    // sort pipelines by device features and extensions
    std::vector<VulkanJsonPipelineDescription> pipelines = input.pipelines;
    std::sort(begin(pipelines), end(pipelines),
              [](const VulkanJsonPipelineDescription &lhs, const VulkanJsonPipelineDescription &rhs)
              {
                  if (lhs.deviceExtensions != rhs.deviceExtensions)
                      return lhs.deviceExtensions < rhs.deviceExtensions;
                  return lhs.deviceFeatures < rhs.deviceFeatures;
              });

    std::string deviceFeatures                = "<empty>";
    std::vector<std::string> deviceExtensions = {"<empty>"};

    Move<VkDevice> pcDevice;
    VkPipelineCache pipelineCache;
    vector<u8> resultCacheData;

    GetDeviceProcAddrFunc getDeviceProcAddrFunc                         = nullptr;
    CreateSamplerYcbcrConversionFunc createSamplerYcbcrConversionFunc   = nullptr;
    DestroySamplerYcbcrConversionFunc destroySamplerYcbcrConversionFunc = nullptr;
    CreateSamplerFunc createSamplerFunc                                 = nullptr;
    DestroySamplerFunc destroySamplerFunc                               = nullptr;
    CreateShaderModuleFunc createShaderModuleFunc                       = nullptr;
    DestroyShaderModuleFunc destroyShaderModuleFunc                     = nullptr;
    CreateRenderPassFunc createRenderPassFunc                           = nullptr;
    CreateRenderPass2Func createRenderPass2Func                         = nullptr;
    DestroyRenderPassFunc destroyRenderPassFunc                         = nullptr;
    CreateDescriptorSetLayoutFunc createDescriptorSetLayoutFunc         = nullptr;
    DestroyDescriptorSetLayoutFunc destroyDescriptorSetLayoutFunc       = nullptr;
    CreatePipelineLayoutFunc createPipelineLayoutFunc                   = nullptr;
    DestroyPipelineLayoutFunc destroyPipelineLayoutFunc                 = nullptr;
    CreateGraphicsPipelinesFunc createGraphicsPipelinesFunc             = nullptr;
    CreateComputePipelinesFunc createComputePipelinesFunc               = nullptr;
    CreatePipelineCacheFunc createPipelineCacheFunc                     = nullptr;
    DestroyPipelineCacheFunc destroyPipelineCacheFunc                   = nullptr;
    DestroyPipelineFunc destroyPipelineFunc                             = nullptr;
    GetPipelineCacheDataFunc getPipelineCacheDataFunc                   = nullptr;

    std::map<VkSamplerYcbcrConversion, VkSamplerYcbcrConversion> falseToRealSamplerYcbcrConversions;
    std::map<VkSampler, VkSampler> falseToRealSamplers;
    std::map<VkShaderModule, VkShaderModule> falseToRealShaderModules;
    std::map<VkRenderPass, VkRenderPass> falseToRealRenderPasses;
    std::map<VkDescriptorSetLayout, VkDescriptorSetLayout> falseToRealDescriptorSetLayouts;
    std::map<VkPipelineLayout, VkPipelineLayout> falseToRealPipelineLayouts;

    // decode VkGraphicsPipelineCreateInfo and VkComputePipelineCreateInfo structs and create VkPipelines with a given pipeline cache
    for (auto &&pipeline : pipelines)
    {
        // check if we need to create new device
        if (pcDevice.get() == nullptr || deviceFeatures != pipeline.deviceFeatures ||
            deviceExtensions != pipeline.deviceExtensions)
        {
            // remove old device
            if (pcDevice.get() != nullptr)
            {
                // collect cache data
                std::size_t cacheSize;
                VK_CHECK(getPipelineCacheDataFunc(*pcDevice, pipelineCache, &cacheSize, nullptr));
                resultCacheData.resize(cacheSize);
                VK_CHECK(getPipelineCacheDataFunc(*pcDevice, pipelineCache, &cacheSize, resultCacheData.data()));

                // clean up resources - in ResourceInterfaceStandard we just simulate Vulkan SC driver after all...
                for (auto &&it : falseToRealPipelineLayouts)
                    destroyPipelineLayoutFunc(*pcDevice, it.second, nullptr);
                for (auto &&it : falseToRealDescriptorSetLayouts)
                    destroyDescriptorSetLayoutFunc(*pcDevice, it.second, nullptr);
                for (auto &&it : falseToRealRenderPasses)
                    destroyRenderPassFunc(*pcDevice, it.second, nullptr);
                for (auto &&it : falseToRealShaderModules)
                    destroyShaderModuleFunc(*pcDevice, it.second, nullptr);
                for (auto &&it : falseToRealSamplers)
                    destroySamplerFunc(*pcDevice, it.second, nullptr);
                for (auto &&it : falseToRealSamplerYcbcrConversions)
                    destroySamplerYcbcrConversionFunc(*pcDevice, it.second, nullptr);
                destroyPipelineCacheFunc(*pcDevice, pipelineCache, nullptr);

                // clear maps
                falseToRealSamplerYcbcrConversions.clear();
                falseToRealSamplers.clear();
                falseToRealShaderModules.clear();
                falseToRealRenderPasses.clear();
                falseToRealDescriptorSetLayouts.clear();
                falseToRealPipelineLayouts.clear();

                // remove device
                pcDevice = Move<VkDevice>();
            }

            // create new device with proper features and extensions
            const float queuePriority                           = 1.0f;
            const VkDeviceQueueCreateInfo deviceQueueCreateInfo = {
                VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                nullptr,
                (VkDeviceQueueCreateFlags)0u,
                queueIndex,     //queueFamilyIndex;
                1,              //queueCount;
                &queuePriority, //pQueuePriorities;
            };

            // recreate pNext chain. Add required Vulkan SC objects if they're missing
            void *pNextChain                           = readJSON_pNextChain(jsonReader, pipeline.deviceFeatures);
            VkPhysicalDeviceFeatures2 *chainedFeatures = (VkPhysicalDeviceFeatures2 *)findStructureInChain(
                pNextChain, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
            VkPhysicalDeviceFeatures2 localFeatures = initVulkanStructure();
            VkDeviceObjectReservationCreateInfo *chainedObjReservation =
                (VkDeviceObjectReservationCreateInfo *)findStructureInChain(
                    pNextChain, VK_STRUCTURE_TYPE_DEVICE_OBJECT_RESERVATION_CREATE_INFO);
            VkDeviceObjectReservationCreateInfo localObjReservation = resetDeviceObjectReservationCreateInfo();
            VkPhysicalDeviceVulkanSC10Features *chainedSC10Features =
                (VkPhysicalDeviceVulkanSC10Features *)findStructureInChain(
                    pNextChain, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_SC_1_0_FEATURES);
            VkPhysicalDeviceVulkanSC10Features localSC10Features = createDefaultSC10Features();

            void *pNext = pNextChain;
            if (chainedFeatures == nullptr)
            {
                chainedFeatures     = &localFeatures;
                localFeatures.pNext = pNext;
                pNext               = &localFeatures;
            }
            if (chainedObjReservation == nullptr)
            {
                chainedObjReservation     = &localObjReservation;
                localObjReservation.pNext = pNext;
                pNext                     = &localObjReservation;
            }
            if (chainedSC10Features == nullptr)
            {
                chainedSC10Features     = &localSC10Features;
                localSC10Features.pNext = pNext;
                pNext                   = &localSC10Features;
            }

            uint32_t gPipelineCount = 0U;
            uint32_t cPipelineCount = 0U;
            for (auto &&pipeline2 : pipelines)
            {
                if (pipeline2.deviceFeatures != pipeline.deviceFeatures ||
                    pipeline2.deviceExtensions != pipeline.deviceExtensions)
                    continue;
                if (pipeline2.pipelineContents.find("VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO") !=
                    std::string::npos)
                    gPipelineCount++;
                else if (pipeline2.pipelineContents.find("VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO") !=
                         std::string::npos)
                    cPipelineCount++;
            }

            // declare pipeline pool size
            VkPipelinePoolSize poolSize = {
                VK_STRUCTURE_TYPE_PIPELINE_POOL_SIZE, // VkStructureType sType;
                nullptr,                              // const void* pNext;
                VKSC_DEFAULT_PIPELINE_POOL_SIZE,      // VkDeviceSize poolEntrySize;
                gPipelineCount + cPipelineCount       // uint32_t poolEntryCount;
            };
            chainedObjReservation->pipelinePoolSizeCount = 1u;
            chainedObjReservation->pPipelinePoolSizes    = &poolSize;

            // declare pipeline cache
            VkPipelineCacheCreateInfo pcCI = {
                VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,              // VkStructureType sType;
                nullptr,                                                   // const void* pNext;
                (VkPipelineCacheCreateFlags)0u,                            // VkPipelineCacheCreateFlags flags;
                resultCacheData.size(),                                    // size_t initialDataSize;
                resultCacheData.empty() ? nullptr : resultCacheData.data() // const void* pInitialData;
            };
            chainedObjReservation->pipelineCacheCreateInfoCount = 1u;
            chainedObjReservation->pPipelineCacheCreateInfos    = &pcCI;

            chainedObjReservation->pipelineLayoutRequestCount =
                de::max(chainedObjReservation->pipelineLayoutRequestCount, uint32_t(input.pipelineLayouts.size()));
            chainedObjReservation->renderPassRequestCount =
                de::max(chainedObjReservation->renderPassRequestCount, uint32_t(input.renderPasses.size()));
            chainedObjReservation->graphicsPipelineRequestCount =
                de::max(chainedObjReservation->graphicsPipelineRequestCount, gPipelineCount);
            chainedObjReservation->computePipelineRequestCount =
                de::max(chainedObjReservation->computePipelineRequestCount, cPipelineCount);
            chainedObjReservation->descriptorSetLayoutRequestCount = de::max(
                chainedObjReservation->descriptorSetLayoutRequestCount, uint32_t(input.descriptorSetLayouts.size()));
            chainedObjReservation->samplerRequestCount =
                de::max(chainedObjReservation->samplerRequestCount, uint32_t(input.samplers.size()));
            chainedObjReservation->samplerYcbcrConversionRequestCount =
                de::max(chainedObjReservation->samplerYcbcrConversionRequestCount,
                        uint32_t(input.samplerYcbcrConversions.size()));
            chainedObjReservation->pipelineCacheRequestCount =
                de::max(chainedObjReservation->pipelineCacheRequestCount, 1u);

            // decode all VkDescriptorSetLayoutCreateInfo
            std::map<VkDescriptorSetLayout, VkDescriptorSetLayoutCreateInfo> descriptorSetLayoutCreateInfos;
            for (auto &&descriptorSetLayout : input.descriptorSetLayouts)
            {
                VkDescriptorSetLayoutCreateInfo dsCI{};
                readJSON_VkDescriptorSetLayoutCreateInfo(jsonReader, descriptorSetLayout.second, dsCI);
                descriptorSetLayoutCreateInfos.insert({descriptorSetLayout.first, dsCI});
            }

            chainedObjReservation->descriptorSetLayoutBindingLimit = 1u;
            for (auto &&dsCI : descriptorSetLayoutCreateInfos)
                for (uint32_t i = 0; i < dsCI.second.bindingCount; ++i)
                    chainedObjReservation->descriptorSetLayoutBindingLimit = de::max(
                        chainedObjReservation->descriptorSetLayoutBindingLimit, dsCI.second.pBindings[i].binding + 1u);

            // recreate device extensions
            vector<const char *> deviceExts;
            for (auto &&ext : pipeline.deviceExtensions)
                deviceExts.push_back(ext.data());

            const VkDeviceCreateInfo deviceCreateInfo = {
                VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,             // sType
                pNext,                                            // pNext
                (VkDeviceCreateFlags)0u,                          // flags
                1,                                                // queueRecordCount
                &deviceQueueCreateInfo,                           // pRequestedQueues
                0,                                                // layerCount
                nullptr,                                          // ppEnabledLayerNames
                (uint32_t)deviceExts.size(),                      // extensionCount
                deviceExts.empty() ? nullptr : deviceExts.data(), // ppEnabledExtensionNames
                &(chainedFeatures->features)                      // pEnabledFeatures
            };

            // create new device
            pcDevice         = createDevice(vkp, instance, vki, physicalDevice, &deviceCreateInfo);
            deviceFeatures   = pipeline.deviceFeatures;
            deviceExtensions = pipeline.deviceExtensions;

            // create local function pointers required to perform pipeline cache creation
            getDeviceProcAddrFunc = (GetDeviceProcAddrFunc)vkp.getInstanceProcAddr(instance, "vkGetDeviceProcAddr");
            createSamplerYcbcrConversionFunc =
                (CreateSamplerYcbcrConversionFunc)getDeviceProcAddrFunc(*pcDevice, "vkCreateSamplerYcbcrConversion");
            destroySamplerYcbcrConversionFunc =
                (DestroySamplerYcbcrConversionFunc)getDeviceProcAddrFunc(*pcDevice, "vkDestroySamplerYcbcrConversion");
            createSamplerFunc      = (CreateSamplerFunc)getDeviceProcAddrFunc(*pcDevice, "vkCreateSampler");
            destroySamplerFunc     = (DestroySamplerFunc)getDeviceProcAddrFunc(*pcDevice, "vkDestroySampler");
            createShaderModuleFunc = (CreateShaderModuleFunc)getDeviceProcAddrFunc(*pcDevice, "vkCreateShaderModule");
            destroyShaderModuleFunc =
                (DestroyShaderModuleFunc)getDeviceProcAddrFunc(*pcDevice, "vkDestroyShaderModule");
            createRenderPassFunc  = (CreateRenderPassFunc)getDeviceProcAddrFunc(*pcDevice, "vkCreateRenderPass");
            createRenderPass2Func = (CreateRenderPass2Func)getDeviceProcAddrFunc(*pcDevice, "vkCreateRenderPass2");
            destroyRenderPassFunc = (DestroyRenderPassFunc)getDeviceProcAddrFunc(*pcDevice, "vkDestroyRenderPass");
            createDescriptorSetLayoutFunc =
                (CreateDescriptorSetLayoutFunc)getDeviceProcAddrFunc(*pcDevice, "vkCreateDescriptorSetLayout");
            destroyDescriptorSetLayoutFunc =
                (DestroyDescriptorSetLayoutFunc)getDeviceProcAddrFunc(*pcDevice, "vkDestroyDescriptorSetLayout");
            createPipelineLayoutFunc =
                (CreatePipelineLayoutFunc)getDeviceProcAddrFunc(*pcDevice, "vkCreatePipelineLayout");
            destroyPipelineLayoutFunc =
                (DestroyPipelineLayoutFunc)getDeviceProcAddrFunc(*pcDevice, "vkDestroyPipelineLayout");
            createGraphicsPipelinesFunc =
                (CreateGraphicsPipelinesFunc)getDeviceProcAddrFunc(*pcDevice, "vkCreateGraphicsPipelines");
            createComputePipelinesFunc =
                (CreateComputePipelinesFunc)getDeviceProcAddrFunc(*pcDevice, "vkCreateComputePipelines");
            createPipelineCacheFunc =
                (CreatePipelineCacheFunc)getDeviceProcAddrFunc(*pcDevice, "vkCreatePipelineCache");
            destroyPipelineCacheFunc =
                (DestroyPipelineCacheFunc)getDeviceProcAddrFunc(*pcDevice, "vkDestroyPipelineCache");
            destroyPipelineFunc = (DestroyPipelineFunc)getDeviceProcAddrFunc(*pcDevice, "vkDestroyPipeline");
            getPipelineCacheDataFunc =
                (GetPipelineCacheDataFunc)getDeviceProcAddrFunc(*pcDevice, "vkGetPipelineCacheData");

            VK_CHECK(createPipelineCacheFunc(*pcDevice, &pcCI, nullptr, &pipelineCache));

            // decode VkSamplerYcbcrConversionCreateInfo structs and create VkSamplerYcbcrConversions
            for (auto &&samplerYcbcr : input.samplerYcbcrConversions)
            {
                VkSamplerYcbcrConversionCreateInfo sycCI{};
                readJSON_VkSamplerYcbcrConversionCreateInfo(jsonReader, samplerYcbcr.second, sycCI);
                VkSamplerYcbcrConversion realConversion;
                VK_CHECK(createSamplerYcbcrConversionFunc(*pcDevice, &sycCI, nullptr, &realConversion));
                falseToRealSamplerYcbcrConversions.insert({samplerYcbcr.first, realConversion});
            }

            // decode VkSamplerCreateInfo structs and create VkSamplers
            for (auto &&sampler : input.samplers)
            {
                VkSamplerCreateInfo sCI{};
                readJSON_VkSamplerCreateInfo(jsonReader, sampler.second, sCI);

                // replace ycbcr conversions if required
                if (sCI.pNext != NULL)
                {
                    if (sCI.pNext != nullptr)
                    {
                        VkSamplerYcbcrConversionInfo *info = (VkSamplerYcbcrConversionInfo *)(sCI.pNext);
                        if (info->sType == VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO)
                        {
                            auto jt = falseToRealSamplerYcbcrConversions.find(info->conversion);
                            if (jt == end(falseToRealSamplerYcbcrConversions))
                                TCU_THROW(InternalError, "VkSamplerYcbcrConversion not found");
                            info->conversion = jt->second;
                        }
                    }
                }

                VkSampler realSampler;
                VK_CHECK(createSamplerFunc(*pcDevice, &sCI, nullptr, &realSampler));
                falseToRealSamplers.insert({sampler.first, realSampler});
            }

            // decode VkShaderModuleCreateInfo structs and create VkShaderModules
            for (auto &&shader : input.shaderModules)
            {
                VkShaderModuleCreateInfo smCI{};
                std::vector<uint8_t> spirvShader;
                readJSON_VkShaderModuleCreateInfo(jsonReader, shader.second, smCI, spirvShader);
                VkShaderModule realShaderModule;
                VK_CHECK(createShaderModuleFunc(*pcDevice, &smCI, nullptr, &realShaderModule));
                falseToRealShaderModules.insert({shader.first, realShaderModule});
            }

            // decode renderPass structs and create VkRenderPasses
            for (auto &&renderPass : input.renderPasses)
            {
                if (renderPass.second.find("VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2") != std::string::npos)
                {
                    VkRenderPassCreateInfo2 rpCI{};
                    readJSON_VkRenderPassCreateInfo2(jsonReader, renderPass.second, rpCI);
                    VkRenderPass realRenderPass;
                    VK_CHECK(createRenderPass2Func(*pcDevice, &rpCI, nullptr, &realRenderPass));
                    falseToRealRenderPasses.insert({renderPass.first, realRenderPass});
                }
                else if (renderPass.second.find("VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO") != std::string::npos)
                {
                    VkRenderPassCreateInfo rpCI{};
                    readJSON_VkRenderPassCreateInfo(jsonReader, renderPass.second, rpCI);
                    VkRenderPass realRenderPass;
                    VK_CHECK(createRenderPassFunc(*pcDevice, &rpCI, nullptr, &realRenderPass));
                    falseToRealRenderPasses.insert({renderPass.first, realRenderPass});
                }
                else
                    TCU_THROW(InternalError, "Could not recognize render pass type");
            }

            // create VkDescriptorSetLayouts
            for (auto &&dsCI : descriptorSetLayoutCreateInfos)
            {
                std::vector<VkDescriptorSetLayoutBinding> newDescriptorBindings;
                std::vector<std::vector<VkSampler>> realSamplers;

                // we have to replace all bindings if there is any immutable sampler defined
                bool needReplaceSamplers = false;
                for (uint32_t i = 0; i < dsCI.second.bindingCount; ++i)
                {
                    if (dsCI.second.pBindings[i].pImmutableSamplers != nullptr)
                        needReplaceSamplers = true;
                }

                if (needReplaceSamplers)
                {
                    for (uint32_t i = 0; i < dsCI.second.bindingCount; ++i)
                    {
                        if (dsCI.second.pBindings[i].pImmutableSamplers == nullptr)
                        {
                            newDescriptorBindings.push_back(dsCI.second.pBindings[i]);
                            continue;
                        }

                        realSamplers.push_back(std::vector<VkSampler>(dsCI.second.pBindings[i].descriptorCount));
                        for (uint32_t j = 0; j < dsCI.second.pBindings[i].descriptorCount; ++j)
                        {
                            if (dsCI.second.pBindings[i].pImmutableSamplers[j] == VK_NULL_HANDLE)
                            {
                                realSamplers.back()[j] = VK_NULL_HANDLE;
                                continue;
                            }
                            else
                            {
                                auto jt = falseToRealSamplers.find(dsCI.second.pBindings[i].pImmutableSamplers[j]);
                                if (jt == end(falseToRealSamplers))
                                    TCU_THROW(InternalError, "VkSampler not found");
                                realSamplers.back()[j] = jt->second;
                            }
                        }
                        VkDescriptorSetLayoutBinding bCopy = {
                            dsCI.second.pBindings[i].binding,         // uint32_t binding;
                            dsCI.second.pBindings[i].descriptorType,  // VkDescriptorType descriptorType;
                            dsCI.second.pBindings[i].descriptorCount, // uint32_t descriptorCount;
                            dsCI.second.pBindings[i].stageFlags,      // VkShaderStageFlags stageFlags;
                            realSamplers.back().data()                // const VkSampler* pImmutableSamplers;
                        };
                        newDescriptorBindings.push_back(bCopy);
                    }
                    dsCI.second.pBindings = newDescriptorBindings.data();
                }

                VkDescriptorSetLayout realDescriptorSetLayout;
                VK_CHECK(createDescriptorSetLayoutFunc(*pcDevice, &dsCI.second, nullptr, &realDescriptorSetLayout));
                falseToRealDescriptorSetLayouts.insert({dsCI.first, realDescriptorSetLayout});
            }

            // decode pipeline layout structs and create VkPipelineLayouts. Requires creation of new pSetLayouts to bypass constness
            for (auto &&pipelineLayout : input.pipelineLayouts)
            {
                VkPipelineLayoutCreateInfo plCI{};
                readJSON_VkPipelineLayoutCreateInfo(jsonReader, pipelineLayout.second, plCI);
                // replace descriptor set layouts with real ones
                std::vector<VkDescriptorSetLayout> newSetLayouts;
                for (uint32_t i = 0; i < plCI.setLayoutCount; ++i)
                {
                    auto jt = falseToRealDescriptorSetLayouts.find(plCI.pSetLayouts[i]);
                    if (jt == end(falseToRealDescriptorSetLayouts))
                        TCU_THROW(InternalError, "VkDescriptorSetLayout not found");
                    newSetLayouts.push_back(jt->second);
                }
                plCI.pSetLayouts = newSetLayouts.data();

                VkPipelineLayout realPipelineLayout;
                VK_CHECK(createPipelineLayoutFunc(*pcDevice, &plCI, nullptr, &realPipelineLayout));
                falseToRealPipelineLayouts.insert({pipelineLayout.first, realPipelineLayout});
            }
        }

        // after device creation - start creating pipelines
        if (pipeline.pipelineContents.find("VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO") != std::string::npos)
        {
            VkGraphicsPipelineCreateInfo gpCI{};
            gpCI.basePipelineHandle = VK_NULL_HANDLE;
            readJSON_VkGraphicsPipelineCreateInfo(jsonReader, pipeline.pipelineContents, gpCI);

            // set poolEntrySize for pipeline
            VkPipelineOfflineCreateInfo *offlineCreateInfo = (VkPipelineOfflineCreateInfo *)findStructureInChain(
                gpCI.pNext, VK_STRUCTURE_TYPE_PIPELINE_OFFLINE_CREATE_INFO);
            if (offlineCreateInfo != nullptr)
                offlineCreateInfo->poolEntrySize = VKSC_DEFAULT_PIPELINE_POOL_SIZE;

            // replace VkShaderModules with real ones. Requires creation of new pStages to bypass constness
            std::vector<VkPipelineShaderStageCreateInfo> newStages;
            for (uint32_t i = 0; i < gpCI.stageCount; ++i)
            {
                VkPipelineShaderStageCreateInfo newStage = gpCI.pStages[i];
                auto jt                                  = falseToRealShaderModules.find(gpCI.pStages[i].module);
                if (jt == end(falseToRealShaderModules))
                    TCU_THROW(InternalError, "VkShaderModule not found");
                newStage.module = jt->second;
                newStages.push_back(newStage);
            }
            gpCI.pStages = newStages.data();

            // replace render pass with a real one
            {
                auto jt = falseToRealRenderPasses.find(gpCI.renderPass);
                if (jt == end(falseToRealRenderPasses))
                    TCU_THROW(InternalError, "VkRenderPass not found");
                gpCI.renderPass = jt->second;
            }

            // replace pipeline layout with a real one
            {
                auto jt = falseToRealPipelineLayouts.find(gpCI.layout);
                if (jt == end(falseToRealPipelineLayouts))
                    TCU_THROW(InternalError, "VkPipelineLayout not found");
                gpCI.layout = jt->second;
            }

            VkPipeline gPipeline = VK_NULL_HANDLE;
            VK_CHECK(createGraphicsPipelinesFunc(*pcDevice, pipelineCache, 1, &gpCI, nullptr, &gPipeline));
            // pipeline was added to cache. We may remove it immediately
            destroyPipelineFunc(*pcDevice, gPipeline, nullptr);
        }
        else if (pipeline.pipelineContents.find("VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO") != std::string::npos)
        {
            VkComputePipelineCreateInfo cpCI{};
            cpCI.basePipelineHandle = VK_NULL_HANDLE;
            readJSON_VkComputePipelineCreateInfo(jsonReader, pipeline.pipelineContents, cpCI);

            // set poolEntrySize for pipeline
            VkPipelineOfflineCreateInfo *offlineCreateInfo = (VkPipelineOfflineCreateInfo *)findStructureInChain(
                cpCI.pNext, VK_STRUCTURE_TYPE_PIPELINE_OFFLINE_CREATE_INFO);
            if (offlineCreateInfo != nullptr)
                offlineCreateInfo->poolEntrySize = VKSC_DEFAULT_PIPELINE_POOL_SIZE;

            // replace VkShaderModule with real one
            {
                auto jt = falseToRealShaderModules.find(cpCI.stage.module);
                if (jt == end(falseToRealShaderModules))
                    TCU_THROW(InternalError, "VkShaderModule not found");
                cpCI.stage.module = jt->second;
            }

            // replace pipeline layout with a real one
            {
                auto jt = falseToRealPipelineLayouts.find(cpCI.layout);
                if (jt == end(falseToRealPipelineLayouts))
                    TCU_THROW(InternalError, "VkPipelineLayout not found");
                cpCI.layout = jt->second;
            }

            VkPipeline cPipeline = VK_NULL_HANDLE;
            VK_CHECK(createComputePipelinesFunc(*pcDevice, pipelineCache, 1, &cpCI, nullptr, &cPipeline));
            // pipeline was added to cache. We may remove it immediately
            destroyPipelineFunc(*pcDevice, cPipeline, nullptr);
        }
        else
            TCU_THROW(InternalError, "Could not recognize pipeline type");
    }

    if (pcDevice.get() != nullptr)
    {
        // getPipelineCacheData() binary data, store it in m_cacheData
        std::size_t cacheSize;
        VK_CHECK(getPipelineCacheDataFunc(*pcDevice, pipelineCache, &cacheSize, nullptr));
        resultCacheData.resize(cacheSize);
        VK_CHECK(getPipelineCacheDataFunc(*pcDevice, pipelineCache, &cacheSize, resultCacheData.data()));

        // clean up resources - in ResourceInterfaceStandard we just simulate Vulkan SC driver after all...
        for (auto &&it : falseToRealPipelineLayouts)
            destroyPipelineLayoutFunc(*pcDevice, it.second, nullptr);
        for (auto &&it : falseToRealDescriptorSetLayouts)
            destroyDescriptorSetLayoutFunc(*pcDevice, it.second, nullptr);
        for (auto &&it : falseToRealRenderPasses)
            destroyRenderPassFunc(*pcDevice, it.second, nullptr);
        for (auto &&it : falseToRealShaderModules)
            destroyShaderModuleFunc(*pcDevice, it.second, nullptr);
        for (auto &&it : falseToRealSamplers)
            destroySamplerFunc(*pcDevice, it.second, nullptr);
        for (auto &&it : falseToRealSamplerYcbcrConversions)
            destroySamplerYcbcrConversionFunc(*pcDevice, it.second, nullptr);

        destroyPipelineCacheFunc(*pcDevice, pipelineCache, nullptr);
    }

    return resultCacheData;
}

std::vector<VulkanPipelineSize> extractSizesFromPipelineCache(const VulkanPipelineCacheInput &input,
                                                              const vector<u8> &pipelineCache,
                                                              uint32_t pipelineDefaultSize, bool recyclePipelineMemory)
{
    std::vector<VulkanPipelineSize> result;
    if (input.pipelines.empty())
        return result;
    VKSCPipelineCacheHeaderReader pcr(pipelineCache.size(), pipelineCache.data());
    if (pcr.isValid())
    {
        for (uint32_t p = 0; p < pcr.getPipelineIndexCount(); ++p)
        {
            const VkPipelineCacheSafetyCriticalIndexEntry *pie = pcr.getPipelineIndexEntry(p);
            if (nullptr != pie)
            {
                VulkanPipelineSize pipelineSize;
                pipelineSize.id = resetPipelineOfflineCreateInfo();
                for (uint32_t i = 0; i < VK_UUID_SIZE; ++i)
                    pipelineSize.id.pipelineIdentifier[i] = pie->pipelineIdentifier[i];
                pipelineSize.size  = uint32_t(pie->pipelineMemorySize);
                pipelineSize.count = 0u;
                auto it            = std::find_if(begin(input.pipelines), end(input.pipelines),
                                                  vksc_server::PipelineIdentifierEqual(pipelineSize.id));
                if (it != end(input.pipelines))
                {
                    if (recyclePipelineMemory)
                        pipelineSize.count = it->maxCount;
                    else // you'd better have enough memory...
                        pipelineSize.count = it->allCount;
                }
                result.emplace_back(pipelineSize);
            }
        }
    }
    else // ordinary Vulkan pipeline. Declare all pipeline sizes as equal to pipelineDefaultSize
    {
        for (uint32_t p = 0; p < input.pipelines.size(); ++p)
        {
            VulkanPipelineSize pipelineSize;
            pipelineSize.id = resetPipelineOfflineCreateInfo();
            for (uint32_t i = 0; i < VK_UUID_SIZE; ++i)
                pipelineSize.id.pipelineIdentifier[i] = input.pipelines[p].id.pipelineIdentifier[i];
            pipelineSize.size = pipelineDefaultSize;
            if (recyclePipelineMemory)
                pipelineSize.count = input.pipelines[p].maxCount;
            else // you'd better have enough memory...
                pipelineSize.count = input.pipelines[p].allCount;
            result.emplace_back(pipelineSize);
        }
    }

    return result;
}

} // namespace vksc_server
