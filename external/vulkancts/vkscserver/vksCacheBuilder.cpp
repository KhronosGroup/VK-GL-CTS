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
    Context jsonContext{};
#ifdef _WIN32
    const char *path_sep = "\\";
#else
    const char *path_sep = "/";
#endif
    uint32_t exportedPipelines = 0;
    for (const auto &pipeline : input.pipelines)
    {
        if (std::holds_alternative<GraphicsPipelineData>(pipeline.pipelineData))
        {
            // export shaders and objects to JSON compatible with https://schema.khronos.org/vulkan/vkpcc.json
            auto &pipelineData = std::get<GraphicsPipelineData>(pipeline.pipelineData);
            std::string gpTxt =
                writeJSON_GraphicsPipeline_vkpccjson(jsonContext, pipeline, filePrefix, exportedPipelines);
            std::stringstream fileName;
            fileName << path << path_sep << filePrefix << "graphics_pipeline_" << exportedPipelines << ".json";
            {
                std::ofstream oFile(fileName.str().c_str(), std::ios::out);
                oFile << gpTxt;
            }

            vk::VkGraphicsPipelineCreateInfo gpCI{};
            readJSON_VkGraphicsPipelineCreateInfo(jsonContext, pipelineData.json, gpCI);
            auto shaderFilenames = getShaderFilenames(gpCI, filePrefix, exportedPipelines);
            for (uint32_t i = 0; i < gpCI.stageCount; ++i)
            {
                vk::VkShaderModuleCreateInfo smCI{};
                readJSON_VkShaderModuleCreateInfo(jsonContext, pipelineData.shaderModuleData[i].json, smCI);

                std::stringstream shaderName;
                shaderName << path << path_sep << shaderFilenames.filenames[i].pFilename;

                std::ofstream oFile(shaderName.str().c_str(), std::ios::out | std::ios::binary);
                oFile.write((const char *)smCI.pCode, smCI.codeSize);
            }

            exportedPipelines++;
        }
        else if (std::holds_alternative<ComputePipelineData>(pipeline.pipelineData))
        {
            // export shaders and objects to JSON compatible with https://schema.khronos.org/vulkan/vkpcc.json
            auto &pipelineData = std::get<ComputePipelineData>(pipeline.pipelineData);
            std::string cpTxt =
                writeJSON_ComputePipeline_vkpccjson(jsonContext, pipeline, filePrefix, exportedPipelines);
            std::stringstream fileName;
            fileName << path << path_sep << filePrefix << "compute_pipeline_" << exportedPipelines << ".json";
            {
                std::ofstream oFile(fileName.str().c_str(), std::ios::out);
                oFile << cpTxt;
            }

            {
                vk::VkComputePipelineCreateInfo cpCI{};
                readJSON_VkComputePipelineCreateInfo(jsonContext, pipelineData.json, cpCI);
                auto shaderFilenames = getShaderFilenames(cpCI, filePrefix, exportedPipelines);

                vk::VkShaderModuleCreateInfo smCI{};
                readJSON_VkShaderModuleCreateInfo(jsonContext, pipelineData.shaderModuleData.json, smCI);

                std::stringstream shaderName;
                shaderName << path << path_sep << shaderFilenames.filenames[0].pFilename;

                std::ofstream oFile(shaderName.str().c_str(), std::ios::out | std::ios::binary);
                oFile.write((const char *)smCI.pCode, smCI.codeSize);
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

    // At this point, testing for gfx is enough, the pipelineData variant can't be empty.
    auto isGfx = [](vksc_server::VulkanJsonPipelineDescription &pipeline)
    { return std::holds_alternative<GraphicsPipelineData>(pipeline.pipelineData); };
    auto gfxData = [](vksc_server::VulkanJsonPipelineDescription &pipeline)
    { return std::get<GraphicsPipelineData>(pipeline.pipelineData); };
    auto compData = [](vksc_server::VulkanJsonPipelineDescription &pipeline)
    { return std::get<ComputePipelineData>(pipeline.pipelineData); };

    // Calculate VkDeviceObjectReservationCreateInfo
    auto writeDeviceObjectReservationCreateInfo =
        [&, poolSize = vk::VkPipelinePoolSize{vk::VK_STRUCTURE_TYPE_PIPELINE_POOL_SIZE, nullptr, 0, 0}](
            vk::VkDeviceObjectReservationCreateInfo &chainedObjReservation, vk::VkPipelineCacheCreateInfo &pcCI) mutable
    {
        uint32_t gPipelineCount = 0U;
        uint32_t cPipelineCount = 0U;
        for (auto &&pipeline : pipelines)
        {
            if (isGfx(pipeline))
            {
                gPipelineCount++;
                if (gfxData(pipeline).renderPassData.json.find("VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2") !=
                    std::string::npos)
                {
                    vk::VkRenderPassCreateInfo2 rpCI{};
                    readJSON_VkRenderPassCreateInfo2(jsonReader, gfxData(pipeline).renderPassData.json, rpCI);
                    chainedObjReservation.subpassDescriptionRequestCount =
                        de::max(rpCI.subpassCount, chainedObjReservation.subpassDescriptionRequestCount);
                }
                else if (gfxData(pipeline).renderPassData.json.find("VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO") !=
                         std::string::npos)
                {
                    vk::VkRenderPassCreateInfo rpCI{};
                    readJSON_VkRenderPassCreateInfo(jsonReader, gfxData(pipeline).renderPassData.json, rpCI);
                    chainedObjReservation.subpassDescriptionRequestCount =
                        de::max(rpCI.subpassCount, chainedObjReservation.subpassDescriptionRequestCount);
                }
                else
                    TCU_THROW(InternalError, "Could not recognize render pass type");
                chainedObjReservation.renderPassRequestCount++;
            }
            else
            {
                cPipelineCount++;
            }
            const auto &pipelineLayoutData =
                isGfx(pipeline) ? gfxData(pipeline).pipelineLayoutData : compData(pipeline).pipelineLayoutData;
            chainedObjReservation.descriptorSetLayoutRequestCount =
                de::max(chainedObjReservation.descriptorSetLayoutRequestCount,
                        uint32_t(pipelineLayoutData.descriptorSetLayouts.size()));
            uint32_t samplerCount      = 0;
            uint32_t ycbcrSamplerCount = 0;
            for (size_t i = 0; i < pipelineLayoutData.descriptorSetLayouts.size(); ++i)
            {
                vk::VkDescriptorSetLayoutCreateInfo dsCI{};
                readJSON_VkDescriptorSetLayoutCreateInfo(jsonReader, pipelineLayoutData.descriptorSetLayouts[i].json,
                                                         dsCI);
                chainedObjReservation.descriptorSetLayoutBindingLimit =
                    de::max(chainedObjReservation.descriptorSetLayoutBindingLimit, dsCI.bindingCount + 1u);
                for (uint32_t j = 0; j < dsCI.bindingCount; ++j)
                {
                    if (dsCI.pBindings[j].descriptorType == vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
                        dsCI.pBindings[j].descriptorType == vk::VK_DESCRIPTOR_TYPE_SAMPLER)
                        samplerCount++;
                }
                for (size_t j = 0; j < pipelineLayoutData.descriptorSetLayouts[i].immutableSamplers.size(); ++j)
                {
                    if (pipelineLayoutData.descriptorSetLayouts[i].immutableSamplers[j].samplerYcbcrConversion)
                        ycbcrSamplerCount++;
                }
            }
            chainedObjReservation.samplerRequestCount =
                de::max(chainedObjReservation.samplerRequestCount, samplerCount);
            chainedObjReservation.samplerYcbcrConversionRequestCount =
                de::max(chainedObjReservation.samplerYcbcrConversionRequestCount, ycbcrSamplerCount);
            chainedObjReservation.pipelineLayoutRequestCount++;
        }
        chainedObjReservation.graphicsPipelineRequestCount = gPipelineCount;
        chainedObjReservation.computePipelineRequestCount  = cPipelineCount;
        chainedObjReservation.pipelineCacheRequestCount = de::max(chainedObjReservation.pipelineCacheRequestCount, 1u);

        poolSize.poolEntrySize                             = vksc_server::VKSC_DEFAULT_PIPELINE_POOL_SIZE;
        poolSize.poolEntryCount                            = gPipelineCount + cPipelineCount;
        chainedObjReservation.pipelinePoolSizeCount        = 1u;
        chainedObjReservation.pPipelinePoolSizes           = &poolSize;
        pcCI.initialDataSize                               = resultCacheData.size();
        pcCI.pInitialData                                  = resultCacheData.empty() ? nullptr : resultCacheData.data();
        chainedObjReservation.pipelineCacheCreateInfoCount = 1u;
        chainedObjReservation.pPipelineCacheCreateInfos    = &pcCI;
    };

    // decode VkGraphicsPipelineCreateInfo and VkComputePipelineCreateInfo structs and create VkPipelines with a given pipeline cache
    for (auto &&pipeline : pipelines)
    {
        std::map<VkSamplerYcbcrConversion, VkSamplerYcbcrConversion> falseToRealSamplerYcbcrConversions;
        std::map<VkSampler, VkSampler> falseToRealSamplers;
        std::map<VkShaderModule, VkShaderModule> falseToRealShaderModules;
        std::map<VkRenderPass, VkRenderPass> falseToRealRenderPasses;
        std::map<VkDescriptorSetLayout, VkDescriptorSetLayout> falseToRealDescriptorSetLayouts;
        std::map<VkPipelineLayout, VkPipelineLayout> falseToRealPipelineLayouts;

        // check if we need to create new device
        if (pcDevice.get() == nullptr || deviceFeatures != pipeline.deviceFeatures ||
            deviceExtensions != pipeline.deviceExtensions)
        {
            // remove old device
            if (pcDevice.get() != nullptr)
            {
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
            VkPhysicalDeviceFeatures2 deviceFeatures2;
            readJSON_VkPhysicalDeviceFeatures2(jsonReader, pipeline.deviceFeatures, deviceFeatures2);
            void *pNextChain                           = deviceFeatures2.pNext;
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

            vk::VkPipelineCacheCreateInfo pcCI{vk::VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, nullptr, 0, 0,
                                               nullptr};
            writeDeviceObjectReservationCreateInfo(*chainedObjReservation, pcCI);

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
        }

        // decode pipeline layout structs and create VkPipelineLayouts. Requires creation of new pSetLayouts to bypass constness
        const auto &pipelineLayoutData =
            isGfx(pipeline) ? gfxData(pipeline).pipelineLayoutData : compData(pipeline).pipelineLayoutData;

        // decode VkPipelineLayoutCreateInfo
        vk::VkPipelineLayoutCreateInfo plCI{};
        readJSON_VkPipelineLayoutCreateInfo(jsonReader, pipelineLayoutData.json, plCI);

        for (auto &&descriptorSetLayout : isGfx(pipeline) ? gfxData(pipeline).pipelineLayoutData.descriptorSetLayouts :
                                                            compData(pipeline).pipelineLayoutData.descriptorSetLayouts)
        {
            // decode VkDescriptorSetLayoutCreateInfo
            vk::VkDescriptorSetLayoutCreateInfo dsCI{};
            readJSON_VkDescriptorSetLayoutCreateInfo(jsonReader, descriptorSetLayout.json, dsCI);

            for (auto &&immutableSampler : descriptorSetLayout.immutableSamplers)
            {
                // decode VkSamplerCreateInfo
                vk::VkSamplerCreateInfo sCI{};
                readJSON_VkSamplerCreateInfo(jsonReader, immutableSampler.json, sCI);

                if (immutableSampler.samplerYcbcrConversion)
                {
                    // decode VkSamplerYcbcrConversionCreateInfo
                    vk::VkSamplerYcbcrConversionCreateInfo sycCI{};
                    readJSON_VkSamplerYcbcrConversionCreateInfo(
                        jsonReader, immutableSampler.samplerYcbcrConversion.value().json, sycCI);

                    // create VkSamplerYcbcrConversions
                    vk::VkSamplerYcbcrConversion realConversion;
                    VK_CHECK(createSamplerYcbcrConversionFunc(*pcDevice, &sycCI, nullptr, &realConversion));
                    falseToRealSamplerYcbcrConversions.insert(
                        {reinterpret_cast<void *>(immutableSampler.samplerYcbcrConversion.value().uniqueObjId),
                         realConversion});

                    // Patch VkSamplerCreateInfo
                    if (auto ycbcrInfo = (const vk::VkSamplerYcbcrConversionInfo *)findStructureInChain(
                            sCI.pNext, vk::VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO);
                        ycbcrInfo != nullptr)
                        const_cast<vk::VkSamplerYcbcrConversionInfo *>(ycbcrInfo)->conversion = realConversion;
                    else
                        TCU_THROW(InternalError, "VkSamplerYcbcrConversion not found");
                }

                // create VkSampler
                vk::VkSampler realSampler;
                VK_CHECK(createSamplerFunc(*pcDevice, &sCI, nullptr, &realSampler));
                falseToRealSamplers.insert({reinterpret_cast<void *>(immutableSampler.uniqueObjId), realSampler});

                // Patch VkDescriptorSetLayoutCreateInfo
                for (uint32_t i = 0; i < dsCI.bindingCount; ++i)
                    if (dsCI.pBindings[i].pImmutableSamplers)
                        for (uint32_t j = 0; j < dsCI.pBindings[i].descriptorCount; ++j)
                            if (dsCI.pBindings[i].pImmutableSamplers[j] ==
                                reinterpret_cast<const void *>(immutableSampler.uniqueObjId))
                                const_cast<vk::VkSampler *>(dsCI.pBindings[i].pImmutableSamplers)[j] = realSampler;
            }

            // create VkDescriptorSetLayout
            vk::VkDescriptorSetLayout realDescriptorSetLayout;
            VK_CHECK(createDescriptorSetLayoutFunc(*pcDevice, &dsCI, nullptr, &realDescriptorSetLayout));
            falseToRealDescriptorSetLayouts.insert(
                {reinterpret_cast<void *>(descriptorSetLayout.uniqueObjId), realDescriptorSetLayout});
        }

        // patch VkPipelineLayoutCreateInfo
        for (uint32_t i = 0; i < plCI.setLayoutCount; ++i)
        {
            if (auto jt = falseToRealDescriptorSetLayouts.find(plCI.pSetLayouts[i]);
                jt != end(falseToRealDescriptorSetLayouts))
                const_cast<vk::VkDescriptorSetLayout *>(plCI.pSetLayouts)[i] = jt->second;
            else
                TCU_THROW(InternalError, "VkDescriptorSetLayout not found");
        }

        // create VkPipelineLayout
        vk::VkPipelineLayout realPipelineLayout;
        VK_CHECK(createPipelineLayoutFunc(*pcDevice, &plCI, nullptr, &realPipelineLayout));
        falseToRealPipelineLayouts.insert(
            {reinterpret_cast<void *>(pipelineLayoutData.uniqueObjId), realPipelineLayout});

        // decode VkShaderModuleCreateInfo structs and create VkShaderModules
        for (auto &&shader : isGfx(pipeline) ?
                                 gfxData(pipeline).shaderModuleData :
                                 std::vector<vksc_server::ShaderModuleData>{compData(pipeline).shaderModuleData})
        {
            vk::VkShaderModuleCreateInfo smCI{};
            readJSON_VkShaderModuleCreateInfo(jsonReader, shader.json, smCI);
            VkShaderModule realShaderModule;
            VK_CHECK(createShaderModuleFunc(*pcDevice, &smCI, nullptr, &realShaderModule));
            falseToRealShaderModules.insert({reinterpret_cast<void *>(shader.uniqueObjId), realShaderModule});
        }

        // decode renderPass structs and create VkRenderPasses
        if (isGfx(pipeline))
        {
            const auto &renderPass = gfxData(pipeline).renderPassData;
            if (renderPass.json.find("VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2") != std::string::npos)
            {
                vk::VkRenderPassCreateInfo2 rpCI{};
                readJSON_VkRenderPassCreateInfo2(jsonReader, renderPass.json, rpCI);
                vk::VkRenderPass realRenderPass;
                VK_CHECK(createRenderPass2Func(*pcDevice, &rpCI, nullptr, &realRenderPass));
                falseToRealRenderPasses.insert({reinterpret_cast<void *>(renderPass.uniqueObjId), realRenderPass});
            }
            else if (renderPass.json.find("VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO") != std::string::npos)
            {
                vk::VkRenderPassCreateInfo rpCI{};
                readJSON_VkRenderPassCreateInfo(jsonReader, renderPass.json, rpCI);
                vk::VkRenderPass realRenderPass;
                VK_CHECK(createRenderPassFunc(*pcDevice, &rpCI, nullptr, &realRenderPass));
                falseToRealRenderPasses.insert({reinterpret_cast<void *>(renderPass.uniqueObjId), realRenderPass});
            }
            else
                TCU_THROW(InternalError, "Could not recognize render pass type");
        }

        // after device creation - start creating pipelines
        if (isGfx(pipeline))
        {
            auto &pipelineData = std::get<GraphicsPipelineData>(pipeline.pipelineData);
            VkGraphicsPipelineCreateInfo gpCI{};
            gpCI.basePipelineHandle = VK_NULL_HANDLE;
            readJSON_VkGraphicsPipelineCreateInfo(jsonReader, pipelineData.json, gpCI);

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
        else
        {
            auto &pipelineData = std::get<GraphicsPipelineData>(pipeline.pipelineData);
            VkComputePipelineCreateInfo cpCI{};
            cpCI.basePipelineHandle = VK_NULL_HANDLE;
            readJSON_VkComputePipelineCreateInfo(jsonReader, pipelineData.json, cpCI);

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
    }

    if (pcDevice.get() != nullptr)
    {
        // getPipelineCacheData() binary data, store it in m_cacheData
        std::size_t cacheSize;
        VK_CHECK(getPipelineCacheDataFunc(*pcDevice, pipelineCache, &cacheSize, nullptr));
        resultCacheData.resize(cacheSize);
        VK_CHECK(getPipelineCacheDataFunc(*pcDevice, pipelineCache, &cacheSize, resultCacheData.data()));

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
