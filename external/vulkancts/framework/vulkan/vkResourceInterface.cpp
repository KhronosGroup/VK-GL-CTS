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
 *//*!
 * \file
 * \brief Defines class for handling resources ( programs, pipelines, files, etc. )
 *//*--------------------------------------------------------------------*/

#include "vkResourceInterface.hpp"
#include "vkQueryUtil.hpp"

#ifdef CTS_USES_VULKANSC
#include <functional>
#include <fstream>
#include "vkSafetyCriticalUtil.hpp"
#include "vkRefUtil.hpp"
#include "tcuCommandLine.hpp"
#include "vksCacheBuilder.hpp"
#include "vksSerializer.hpp"
#include "vkApiVersion.hpp"
#include <json/json.h>
using namespace vksc_server::json;
#endif // CTS_USES_VULKANSC

namespace vk
{

ResourceInterface::ResourceInterface(tcu::TestContext &testCtx)
    : m_testCtx(testCtx)
#ifdef CTS_USES_VULKANSC
    , m_commandPoolIndex(0u)
    , m_resourceCounter(0u)
    , m_uniqueObjIdCounter(0u)
    , m_statCurrent(resetDeviceObjectReservationCreateInfo())
    , m_statMax(resetDeviceObjectReservationCreateInfo())
    , m_enabledHandleDestroy(true)
#endif // CTS_USES_VULKANSC
{
#ifdef CTS_USES_VULKANSC
    // pipelineCacheRequestCount does not contain one instance of createPipelineCache call that happens only in subprocess
    m_statCurrent.pipelineCacheRequestCount = 1u;
    m_statMax.pipelineCacheRequestCount     = 1u;
#endif // CTS_USES_VULKANSC
}

ResourceInterface::~ResourceInterface()
{
}

void ResourceInterface::initTestCase(const std::string &casePath)
{
    m_currentTestPath = casePath;
}

const std::string &ResourceInterface::getCasePath() const
{
    return m_currentTestPath;
}

#ifdef CTS_USES_VULKANSC
Json::Value &findStructureInJson(Json::Value &first, const char *type)
{
    Json::Value *cur = &first;

    while (!(cur->isString() && cur->asString() == "NULL"))
    {
        if ((*cur)["sType"] == type)
            break;
        else
            cur = &(*cur)["pNext"];
    }

    return *cur;
}

void ResourceInterface::initApiVersion(const uint32_t version)
{
    const ApiVersion apiVersion = unpackVersion(version);
    const bool vulkanSC         = (apiVersion.variantNum == 1);

    m_version  = tcu::Maybe<uint32_t>(version);
    m_vulkanSC = vulkanSC;
}

bool ResourceInterface::isVulkanSC(void) const
{
    return m_vulkanSC.get();
}

std::mutex &ResourceInterface::getStatMutex()
{
    return m_mutex;
}

VkDeviceObjectReservationCreateInfo &ResourceInterface::getStatCurrent()
{
    return m_statCurrent;
}

VkDeviceObjectReservationCreateInfo &ResourceInterface::getStatMax()
{
    return m_statMax;
}

const VkDeviceObjectReservationCreateInfo &ResourceInterface::getStatMax() const
{
    return m_statMax;
}

void ResourceInterface::setHandleDestroy(bool value)
{
    m_enabledHandleDestroy = value;
}

bool ResourceInterface::isEnabledHandleDestroy() const
{
    return m_enabledHandleDestroy;
}

void ResourceInterface::finalizeCommandBuffers()
{
    // We have information about command buffer sizes
    // Now we have to convert it into command pool sizes
    std::map<uint64_t, std::size_t> cpToIndex;
    for (std::size_t i = 0; i < m_commandPoolMemoryConsumption.size(); ++i)
        cpToIndex.insert({m_commandPoolMemoryConsumption[i].commandPool, i});
    for (const auto &memC : m_commandBufferMemoryConsumption)
    {
        std::size_t j = cpToIndex[memC.second.commandPool];
        m_commandPoolMemoryConsumption[j].updateValues(memC.second.maxCommandPoolAllocated,
                                                       memC.second.maxCommandPoolReservedSize,
                                                       memC.second.maxCommandBufferAllocated);
        m_commandPoolMemoryConsumption[j].commandBufferCount++;
    }
    // Each m_commandPoolMemoryConsumption element must have at least one command buffer ( see DeviceDriverSC::createCommandPoolHandlerNorm() )
    // As a result we have to ensure that commandBufferRequestCount is not less than the number of command pools
    m_statMax.commandBufferRequestCount =
        de::max(uint32_t(m_commandPoolMemoryConsumption.size()), m_statMax.commandBufferRequestCount);
}

std::vector<uint8_t> ResourceInterface::exportData() const
{
    vksc_server::VulkanDataTransmittedFromMainToSubprocess vdtfmtsp(m_pipelineInput, m_statMax,
                                                                    m_commandPoolMemoryConsumption, m_pipelineSizes);

    return vksc_server::Serialize(vdtfmtsp);
}

void ResourceInterface::importData(std::vector<uint8_t> &importText) const
{
    vksc_server::VulkanDataTransmittedFromMainToSubprocess vdtfmtsp =
        vksc_server::Deserialize<vksc_server::VulkanDataTransmittedFromMainToSubprocess>(importText);

    m_pipelineInput                = vdtfmtsp.pipelineCacheInput;
    m_statMax                      = vdtfmtsp.memoryReservation;
    m_commandPoolMemoryConsumption = vdtfmtsp.commandPoolMemoryConsumption;
    m_pipelineSizes                = vdtfmtsp.pipelineSizes;
}

void ResourceInterface::registerObjectHash(uint64_t handle, std::size_t hashValue) const
{
    m_objectHashes[handle] = hashValue;
}

const std::map<uint64_t, std::size_t> &ResourceInterface::getObjectHashes() const
{
    return m_objectHashes;
}

struct PipelinePoolSizeInfo
{
    uint32_t maxTestCount;
    uint32_t size;
};

void ResourceInterface::preparePipelinePoolSizes()
{
    std::map<std::string, std::vector<PipelinePoolSizeInfo>> pipelineInfoPerTest;

    // Step 1: collect information about all pipelines in each test, group by size
    for (const auto &pipeline : m_pipelineInput.pipelines)
    {
        auto it = std::find_if(begin(m_pipelineSizes), end(m_pipelineSizes),
                               vksc_server::PipelineIdentifierEqual(pipeline.id));
        if (it == end(m_pipelineSizes))
            TCU_THROW(InternalError, "Pipeline size information not found for pipeline identifier");

        PipelinePoolSizeInfo ppsi{it->count, it->size};

        for (const auto &test : pipeline.tests)
        {
            auto pit = pipelineInfoPerTest.find(test);
            if (pit == end(pipelineInfoPerTest))
                pit = pipelineInfoPerTest.insert({test, std::vector<PipelinePoolSizeInfo>()}).first;
            // group by the same sizes in a test
            bool found = false;
            for (size_t i = 0; i < pit->second.size(); ++i)
            {
                if (pit->second[i].size == ppsi.size)
                {
                    pit->second[i].maxTestCount += ppsi.maxTestCount;
                    found = true;
                    break;
                }
            }
            if (!found)
                pit->second.push_back(ppsi);
        }
    }

    // Step 2: choose pipeline pool sizes
    std::vector<PipelinePoolSizeInfo> finalPoolSizes;
    for (const auto &pInfo : pipelineInfoPerTest)
    {
        for (const auto &ppsi1 : pInfo.second)
        {
            auto it = std::find_if(begin(finalPoolSizes), end(finalPoolSizes),
                                   [&ppsi1](const PipelinePoolSizeInfo &x) { return (x.size == ppsi1.size); });
            if (it != end(finalPoolSizes))
                it->maxTestCount = de::max(it->maxTestCount, ppsi1.maxTestCount);
            else
                finalPoolSizes.push_back(ppsi1);
        }
    }

    // Step 3: convert results to VkPipelinePoolSize
    m_pipelinePoolSizes.clear();
    for (const auto &ppsi : finalPoolSizes)
    {
        VkPipelinePoolSize poolSize = {
            VK_STRUCTURE_TYPE_PIPELINE_POOL_SIZE, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            ppsi.size,                            // VkDeviceSize poolEntrySize;
            ppsi.maxTestCount                     // uint32_t poolEntryCount;
        };
        m_pipelinePoolSizes.emplace_back(poolSize);
    }
}

std::vector<VkPipelinePoolSize> ResourceInterface::getPipelinePoolSizes() const
{
    return m_pipelinePoolSizes;
}

void ResourceInterface::fillPoolEntrySize(vk::VkPipelineOfflineCreateInfo &pipelineIdentifier) const
{
    auto it = std::find_if(begin(m_pipelineSizes), end(m_pipelineSizes),
                           vksc_server::PipelineIdentifierEqual(pipelineIdentifier));
    if (it == end(m_pipelineSizes))
        TCU_THROW(InternalError, "Pipeline size information not found for pipeline identifier");
    pipelineIdentifier.poolEntrySize = it->size;
}

vksc_server::VulkanCommandMemoryConsumption ResourceInterface::getNextCommandPoolSize()
{
    if (m_commandPoolMemoryConsumption.empty())
        return vksc_server::VulkanCommandMemoryConsumption();

    vksc_server::VulkanCommandMemoryConsumption result = m_commandPoolMemoryConsumption[m_commandPoolIndex];
    // modulo operation is just a safeguard against excessive number of requests
    m_commandPoolIndex = (m_commandPoolIndex + 1) % uint32_t(m_commandPoolMemoryConsumption.size());
    return result;
}

std::size_t ResourceInterface::getCacheDataSize() const
{
    return m_cacheData.size();
}

const uint8_t *ResourceInterface::getCacheData() const
{
    return m_cacheData.data();
}

VkPipelineCache ResourceInterface::getPipelineCache(VkDevice device) const
{
    auto pit = m_devicePipelineCaches.find(device);
    if (pit == end(m_devicePipelineCaches))
        TCU_THROW(InternalError, "Pipeline cache not found for this device");
    return pit->second.pipelineCache.get()->get();
}

#endif // CTS_USES_VULKANSC

ResourceInterfaceStandard::ResourceInterfaceStandard(tcu::TestContext &testCtx) : ResourceInterface(testCtx)
{
}

void ResourceInterfaceStandard::initDevice(DeviceInterface &deviceInterface, VkDevice device)
{
    // ResourceInterfaceStandard is a class for running VulkanSC tests on normal Vulkan driver.
    // CTS does not have vkCreateShaderModule function defined for Vulkan SC driver, but we need this function
    // So ResourceInterfaceStandard class must have its own vkCreateShaderModule function pointer
    // Moreover - we create additional function pointers for vkCreateGraphicsPipelines, vkCreateComputePipelines, etc.
    // BTW: although ResourceInterfaceStandard exists in normal Vulkan tests - only initDevice and buildProgram functions are used by Vulkan tests
    // Other functions are called from within DeviceDriverSC which does not exist in these tests ( DeviceDriver class is used instead )
    m_createShaderModuleFunc[device] =
        (CreateShaderModuleFunc)deviceInterface.getDeviceProcAddr(device, "vkCreateShaderModule");
    m_createGraphicsPipelinesFunc[device] =
        (CreateGraphicsPipelinesFunc)deviceInterface.getDeviceProcAddr(device, "vkCreateGraphicsPipelines");
    m_createComputePipelinesFunc[device] =
        (CreateComputePipelinesFunc)deviceInterface.getDeviceProcAddr(device, "vkCreateComputePipelines");
#ifdef CTS_USES_VULKANSC
    if (m_testCtx.getCommandLine().isSubProcess())
    {
        if (m_cacheData.size() > 0)
        {
            VkPipelineCacheCreateInfo pCreateInfo = {
                VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType sType;
                nullptr,                                      // const void* pNext;
                VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
                    VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT, // VkPipelineCacheCreateFlags flags;
                m_cacheData.size(),                                       // uintptr_t initialDataSize;
                m_cacheData.data()                                        // const void* pInitialData;
            };
            auto &pipelineCacheInfo = m_devicePipelineCaches[device];
            if (pipelineCacheInfo.refCount == 0)
            {
                pipelineCacheInfo.pipelineCache = de::SharedPtr<Move<VkPipelineCache>>(
                    new Move<VkPipelineCache>(createPipelineCache(deviceInterface, device, &pCreateInfo, nullptr)));
                pipelineCacheInfo.refCount = 1;
            }
            else
            {
                DE_ASSERT(pipelineCacheInfo.pipelineCache);
                // Increment reference count
                pipelineCacheInfo.refCount++;
            }
        }
    }
#endif // CTS_USES_VULKANSC
}

void ResourceInterfaceStandard::deinitDevice(VkDevice device)
{
#ifdef CTS_USES_VULKANSC
    if (m_testCtx.getCommandLine().isSubProcess())
    {
        auto it = m_devicePipelineCaches.find(device);
        if (it != m_devicePipelineCaches.end())
        {
            if (it->second.refCount == 1)
            {
                // Last reference so ready to delete pipeline cache
                m_devicePipelineCaches.erase(it);
            }
            else
            {
                // Only decrement reference count
                it->second.refCount--;
            }
        }
    }
#else
    DE_UNREF(device);
#endif // CTS_USES_VULKANSC
}

#ifdef CTS_USES_VULKANSC

void ResourceInterfaceStandard::registerDeviceFeatures(VkDevice device, const VkDeviceCreateInfo *pCreateInfo) const
{
    m_deviceFeatures[device] = writeJSON_VkPhysicalDeviceFeatures2(m_jsonContext, *pCreateInfo);

    std::vector<std::string> extensions;
    for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; ++i)
        extensions.push_back(pCreateInfo->ppEnabledExtensionNames[i]);
    m_deviceExtensions[device] = extensions;
}

void ResourceInterfaceStandard::unregisterDeviceFeatures(VkDevice device) const
{
    m_deviceFeatures.erase(device);
    m_deviceExtensions.erase(device);
}

VkResult ResourceInterfaceStandard::createShaderModule(VkDevice device, const VkShaderModuleCreateInfo *pCreateInfo,
                                                       const VkAllocationCallbacks *pAllocator,
                                                       VkShaderModule *pShaderModule, bool normalMode) const
{
    if (normalMode)
    {
        if (isVulkanSC())
        {
            *pShaderModule = incResourceCounter<VkShaderModule>();
            registerObjectHash(pShaderModule->getInternal(),
                               calculateShaderModuleHash(*pCreateInfo, getObjectHashes()));
            return VK_SUCCESS;
        }
        else
        {
            const auto it = m_createShaderModuleFunc.find(device);
            if (it != end(m_createShaderModuleFunc))
            {
                VkResult result = it->second(device, pCreateInfo, pAllocator, pShaderModule);
                registerObjectHash(pShaderModule->getInternal(),
                                   calculateShaderModuleHash(*pCreateInfo, getObjectHashes()));
                return result;
            }
            TCU_THROW(InternalError, "vkCreateShaderModule not defined");
        }
    }

    // main process: store VkShaderModuleCreateInfo in JSON format. Shaders will be sent later for device pipeline cache creation ( and sent through file to another process )
    *pShaderModule = incResourceCounter<VkShaderModule>();
    registerObjectHash(pShaderModule->getInternal(), calculateShaderModuleHash(*pCreateInfo, getObjectHashes()));
    m_pipelineInput.shaderModules.insert(
        {*pShaderModule,
         {writeJSON_VkShaderModuleCreateInfo(m_jsonContext, *pCreateInfo), pShaderModule->getInternal()}});
    return VK_SUCCESS;
}

VkPipelineOfflineCreateInfo makeGraphicsPipelineIdentifier(const std::string &testPath,
                                                           const VkGraphicsPipelineCreateInfo &gpCI,
                                                           const std::map<uint64_t, std::size_t> &objectHashes)
{
    DE_UNREF(testPath);
    VkPipelineOfflineCreateInfo pipelineID = resetPipelineOfflineCreateInfo();
    std::size_t hashValue                  = calculateGraphicsPipelineHash(gpCI, objectHashes);
    memcpy(pipelineID.pipelineIdentifier, &hashValue, sizeof(std::size_t));
    return pipelineID;
}

VkPipelineOfflineCreateInfo makeComputePipelineIdentifier(const std::string &testPath,
                                                          const VkComputePipelineCreateInfo &cpCI,
                                                          const std::map<uint64_t, std::size_t> &objectHashes)
{
    DE_UNREF(testPath);
    VkPipelineOfflineCreateInfo pipelineID = resetPipelineOfflineCreateInfo();
    std::size_t hashValue                  = calculateComputePipelineHash(cpCI, objectHashes);
    memcpy(pipelineID.pipelineIdentifier, &hashValue, sizeof(std::size_t));
    return pipelineID;
}

VkResult ResourceInterfaceStandard::createGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache,
                                                            uint32_t createInfoCount,
                                                            const VkGraphicsPipelineCreateInfo *pCreateInfos,
                                                            const VkAllocationCallbacks *pAllocator,
                                                            VkPipeline *pPipelines, bool normalMode) const
{
    DE_UNREF(pipelineCache);

    // build pipeline identifiers (if required), make a copy of pCreateInfos
    std::vector<VkPipelineOfflineCreateInfo> pipelineIDs;
    std::vector<uint8_t> idInPNextChain;
    std::vector<VkGraphicsPipelineCreateInfo> pCreateInfoCopies;

    for (uint32_t i = 0; i < createInfoCount; ++i)
    {
        pCreateInfoCopies.push_back(pCreateInfos[i]);

        // Check if test added pipeline identifier on its own
        VkPipelineOfflineCreateInfo *idInfo = (VkPipelineOfflineCreateInfo *)findStructureInChain(
            pCreateInfos[i].pNext, VK_STRUCTURE_TYPE_PIPELINE_OFFLINE_CREATE_INFO);
        if (idInfo == nullptr)
        {
            pipelineIDs.push_back(
                makeGraphicsPipelineIdentifier(m_currentTestPath, pCreateInfos[i], getObjectHashes()));
            idInPNextChain.push_back(0);
        }
        else
        {
            pipelineIDs.push_back(*idInfo);
            idInPNextChain.push_back(1);
        }

        if (normalMode)
            fillPoolEntrySize(pipelineIDs.back());
    }

    // Include pipelineIdentifiers into pNext chain of pCreateInfoCopies - skip this operation if pipeline identifier was created inside test
    for (uint32_t i = 0; i < createInfoCount; ++i)
    {
        if (idInPNextChain[i] == 0)
        {
            pipelineIDs[i].pNext       = pCreateInfoCopies[i].pNext;
            pCreateInfoCopies[i].pNext = &pipelineIDs[i];
        }
    }

    // subprocess: load graphics pipelines from OUR device pipeline cache
    if (normalMode)
    {
        const auto it = m_createGraphicsPipelinesFunc.find(device);
        if (it != end(m_createGraphicsPipelinesFunc))
        {
            auto pit = m_devicePipelineCaches.find(device);
            if (pit != end(m_devicePipelineCaches))
            {
                VkPipelineCache pCache = pit->second.pipelineCache->get();
                return it->second(device, pCache, createInfoCount, pCreateInfoCopies.data(), pAllocator, pPipelines);
            }
            TCU_THROW(InternalError, "Pipeline cache not initialized for this device");
        }
        TCU_THROW(InternalError, "vkCreateGraphicsPipelines not defined");
    }

    // main process: store pipelines in JSON format. Pipelines will be sent later for device pipeline cache creation ( and sent through file to another process )
    for (uint32_t i = 0; i < createInfoCount; ++i)
    {
        m_pipelineIdentifiers.insert({pPipelines[i], pipelineIDs[i]});

        auto it              = std::find_if(begin(m_pipelineInput.pipelines), end(m_pipelineInput.pipelines),
                                            vksc_server::PipelineIdentifierEqual(pipelineIDs[i]));
        pipelineIDs[i].pNext = nullptr;
        if (it == end(m_pipelineInput.pipelines))
        {
            const auto &featIt = m_deviceFeatures.find(device);
            if (featIt == end(m_deviceFeatures))
                TCU_THROW(InternalError, "Failed to find device features for graphics pipeline");
            const auto &extIt = m_deviceExtensions.find(device);
            if (extIt == end(m_deviceExtensions))
                TCU_THROW(InternalError, "Failed to find device extensions for graphics pipeline");
            auto renderPassResult = m_pipelineInput.renderPasses.find(pCreateInfos[i].renderPass);
            if (renderPassResult == end(m_pipelineInput.renderPasses))
                TCU_THROW(InternalError, "Failed to find renderpass for graphics pipeline");

            auto layoutResult = m_pipelineInput.pipelineLayouts.find(pCreateInfos[i].layout);
            std::vector<vksc_server::ShaderModuleData> shaderModuleData;
            for (uint32_t j = 0; j < pCreateInfos[i].stageCount; ++j)
            {
                auto shaderModuleResult = m_pipelineInput.shaderModules.find(pCreateInfos[i].pStages[j].module);
                if (shaderModuleResult == end(m_pipelineInput.shaderModules))
                    TCU_THROW(InternalError, "Failed to find a shader module for graphics pipeline");
                else
                    shaderModuleData.push_back({shaderModuleResult->second});
            }

            m_pipelineInput.pipelines.push_back(vksc_server::VulkanJsonPipelineDescription(
                pipelineIDs[i],
                vksc_server::GraphicsPipelineData{
                    writeJSON_VkGraphicsPipelineCreateInfo(m_jsonContext, pCreateInfoCopies[i]), layoutResult->second,
                    renderPassResult->second, std::move(shaderModuleData), pPipelines[i].getInternal()},
                featIt->second, extIt->second, m_currentTestPath));
        }
        else
            it->add(m_currentTestPath);
    }
    return VK_SUCCESS;
}

VkResult ResourceInterfaceStandard::createComputePipelines(VkDevice device, VkPipelineCache pipelineCache,
                                                           uint32_t createInfoCount,
                                                           const VkComputePipelineCreateInfo *pCreateInfos,
                                                           const VkAllocationCallbacks *pAllocator,
                                                           VkPipeline *pPipelines, bool normalMode) const
{
    DE_UNREF(pipelineCache);

    // build pipeline identifiers (if required), make a copy of pCreateInfos
    std::vector<VkPipelineOfflineCreateInfo> pipelineIDs;
    std::vector<uint8_t> idInPNextChain;
    std::vector<VkComputePipelineCreateInfo> pCreateInfoCopies;

    for (uint32_t i = 0; i < createInfoCount; ++i)
    {
        pCreateInfoCopies.push_back(pCreateInfos[i]);

        // Check if test added pipeline identifier on its own
        VkPipelineOfflineCreateInfo *idInfo = (VkPipelineOfflineCreateInfo *)findStructureInChain(
            pCreateInfos[i].pNext, VK_STRUCTURE_TYPE_PIPELINE_OFFLINE_CREATE_INFO);
        if (idInfo == nullptr)
        {
            pipelineIDs.push_back(makeComputePipelineIdentifier(m_currentTestPath, pCreateInfos[i], getObjectHashes()));
            idInPNextChain.push_back(0);
        }
        else
        {
            pipelineIDs.push_back(*idInfo);
            idInPNextChain.push_back(1);
        }

        if (normalMode)
            fillPoolEntrySize(pipelineIDs.back());
    }

    // Include pipelineIdentifiers into pNext chain of pCreateInfoCopies - skip this operation if pipeline identifier was created inside test
    for (uint32_t i = 0; i < createInfoCount; ++i)
    {
        if (idInPNextChain[i] == 0)
        {
            pipelineIDs[i].pNext       = pCreateInfoCopies[i].pNext;
            pCreateInfoCopies[i].pNext = &pipelineIDs[i];
        }
    }

    // subprocess: load compute pipelines from OUR pipeline cache
    if (normalMode)
    {
        const auto it = m_createComputePipelinesFunc.find(device);
        if (it != end(m_createComputePipelinesFunc))
        {
            auto pit = m_devicePipelineCaches.find(device);
            if (pit != end(m_devicePipelineCaches))
            {
                VkPipelineCache pCache = pit->second.pipelineCache->get();
                return it->second(device, pCache, createInfoCount, pCreateInfoCopies.data(), pAllocator, pPipelines);
            }
            TCU_THROW(InternalError, "Pipeline cache not initialized for this device");
        }
        TCU_THROW(InternalError, "vkCreateComputePipelines not defined");
    }

    // main process: store pipelines in JSON format. Pipelines will be sent later for device pipeline cache creation ( and sent through file to another process )
    for (uint32_t i = 0; i < createInfoCount; ++i)
    {
        m_pipelineIdentifiers.insert({pPipelines[i], pipelineIDs[i]});

        auto it              = std::find_if(begin(m_pipelineInput.pipelines), end(m_pipelineInput.pipelines),
                                            vksc_server::PipelineIdentifierEqual(pipelineIDs[i]));
        pipelineIDs[i].pNext = nullptr;
        if (it == end(m_pipelineInput.pipelines))
        {
            const auto &featIt = m_deviceFeatures.find(device);
            if (featIt == end(m_deviceFeatures))
                TCU_THROW(InternalError, "Failed to find device features for compute pipeline");
            const auto &extIt = m_deviceExtensions.find(device);
            if (extIt == end(m_deviceExtensions))
                TCU_THROW(InternalError, "Failed to find device extensions for compute pipeline");

            auto layoutResult       = m_pipelineInput.pipelineLayouts.find(pCreateInfos[i].layout);
            auto shaderModuleResult = m_pipelineInput.shaderModules.find(pCreateInfos[i].stage.module);
            if (layoutResult == std::end(m_pipelineInput.pipelineLayouts))
            {
                TCU_THROW(InternalError, "Failed to find pipeline layout for compute pipeline");
            }

            if (shaderModuleResult == std::end(m_pipelineInput.shaderModules))
            {
                TCU_THROW(InternalError, "Failed to find shader module for compute pipeline");
            }

            // Rewrite handle of pipeline layout in graphics pipeline with autoinc unique_obj_id
            pCreateInfoCopies[i].layout = reinterpret_cast<vk::VkPipelineLayout *>(layoutResult->second.uniqueObjId);

            m_pipelineInput.pipelines.push_back(vksc_server::VulkanJsonPipelineDescription(
                pipelineIDs[i],
                vksc_server::ComputePipelineData{
                    writeJSON_VkComputePipelineCreateInfo(m_jsonContext, pCreateInfoCopies[i]), layoutResult->second,
                    shaderModuleResult->second, pPipelines[i].getInternal()},
                featIt->second, extIt->second, m_currentTestPath));
        }
        else
            it->add(m_currentTestPath);
    }
    return VK_SUCCESS;
}

void ResourceInterfaceStandard::destroyPipeline(VkDevice device, VkPipeline pipeline,
                                                const VkAllocationCallbacks *pAllocator) const
{
    DE_UNREF(device);
    DE_UNREF(pAllocator);

    auto it = m_pipelineIdentifiers.find(pipeline);
    if (it == end(m_pipelineIdentifiers))
        TCU_THROW(InternalError, "Failed to find pipeline");

    auto pit = std::find_if(begin(m_pipelineInput.pipelines), end(m_pipelineInput.pipelines),
                            vksc_server::PipelineIdentifierEqual(it->second));
    if (pit == end(m_pipelineInput.pipelines))
        TCU_THROW(InternalError, "Failed to find pipeline identifier");
    pit->remove();
}

void ResourceInterfaceStandard::createRenderPass(VkDevice device, const VkRenderPassCreateInfo *pCreateInfo,
                                                 const VkAllocationCallbacks *pAllocator,
                                                 VkRenderPass *pRenderPass) const
{
    DE_UNREF(device);
    DE_UNREF(pAllocator);
    m_pipelineInput.renderPasses.insert(
        {*pRenderPass, {writeJSON_VkRenderPassCreateInfo(m_jsonContext, *pCreateInfo), pRenderPass->getInternal()}});
}

void ResourceInterfaceStandard::createRenderPass2(VkDevice device, const VkRenderPassCreateInfo2 *pCreateInfo,
                                                  const VkAllocationCallbacks *pAllocator,
                                                  VkRenderPass *pRenderPass) const
{
    DE_UNREF(device);
    DE_UNREF(pAllocator);
    m_pipelineInput.renderPasses.insert(
        {*pRenderPass, {writeJSON_VkRenderPassCreateInfo2(m_jsonContext, *pCreateInfo), pRenderPass->getInternal()}});
}

void ResourceInterfaceStandard::createPipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo *pCreateInfo,
                                                     const VkAllocationCallbacks *pAllocator,
                                                     VkPipelineLayout *pPipelineLayout) const
{
    DE_UNREF(device);
    DE_UNREF(pAllocator);
    std::vector<vksc_server::DescriptorSetLayoutData> descriptorSetLayouts;
    descriptorSetLayouts.reserve(pCreateInfo->setLayoutCount);
    auto pCreateInfoString = vksc_server::json::writeJSON_VkPipelineLayoutCreateInfo(m_jsonContext, *pCreateInfo);
    Json::Value pCreateInfoJson;
    m_jsonContext.reader->parse(pCreateInfoString.c_str(), pCreateInfoString.c_str() + pCreateInfoString.size(),
                                &pCreateInfoJson, nullptr);
    for (uint32_t i = 0; i < pCreateInfo->setLayoutCount; ++i)
    {
        if (auto result = m_pipelineInput.descriptorSetLayouts.find(pCreateInfo->pSetLayouts[i]);
            result != std::end(m_pipelineInput.descriptorSetLayouts))
        {
            // Rewrite handle(s) in create info to unique_obj_id(s)
            pCreateInfoJson["pSetLayouts"][i] = result->second.uniqueObjId;
            // Store local copy of dependent create info
            descriptorSetLayouts.emplace_back(result->second);
        }
        else
        {
            TCU_THROW(InternalError, "Failed to find descriptor set layout referenced by pipeline layout");
        }
    }
    m_pipelineInput.pipelineLayouts.insert(
        {*pPipelineLayout,
         {m_jsonContext.writer->write(pCreateInfoJson), std::move(descriptorSetLayouts), ++m_uniqueObjIdCounter}});
}

void ResourceInterfaceStandard::createDescriptorSetLayout(VkDevice device,
                                                          const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
                                                          const VkAllocationCallbacks *pAllocator,
                                                          VkDescriptorSetLayout *pSetLayout) const
{
    DE_UNREF(device);
    DE_UNREF(pAllocator);
    std::vector<vksc_server::SamplerData> immutableSamplers{};
    auto pCreateInfoString = vksc_server::json::writeJSON_VkDescriptorSetLayoutCreateInfo(m_jsonContext, *pCreateInfo);
    Json::Value pCreateInfoJson;
    m_jsonContext.reader->parse(pCreateInfoString.c_str(), pCreateInfoString.c_str() + pCreateInfoString.size(),
                                &pCreateInfoJson, nullptr);
    for (uint32_t i = 0; i < pCreateInfo->bindingCount; ++i)
    {
        if (pCreateInfo->pBindings[i].pImmutableSamplers)
        {
            immutableSamplers.reserve(immutableSamplers.size() + pCreateInfo->pBindings[i].descriptorCount);
            for (uint32_t j = 0; j < pCreateInfo->pBindings[i].descriptorCount; ++j)
            {
                if (auto result = m_pipelineInput.samplers.find(pCreateInfo->pBindings[i].pImmutableSamplers[j]);
                    result != std::end(m_pipelineInput.samplers))
                {
                    // Rewrite handle(s) in create info to unique_obj_id(s)
                    pCreateInfoJson["pBindings"][i]["pImmutableSamplers"][j] = result->second.uniqueObjId;
                    // Store local copy of dependent create info
                    immutableSamplers.emplace_back(result->second);
                }
                else
                {
                    TCU_THROW(InternalError, "Failed to find sampler referenced by descriptor set layout");
                }
            }
        }
    }
    m_pipelineInput.descriptorSetLayouts.insert(
        {*pSetLayout,
         {m_jsonContext.writer->write(pCreateInfoJson), std::move(immutableSamplers), ++m_uniqueObjIdCounter}});
}

void ResourceInterfaceStandard::createSampler(VkDevice device, const VkSamplerCreateInfo *pCreateInfo,
                                              const VkAllocationCallbacks *pAllocator, VkSampler *pSampler) const
{
    DE_UNREF(device);
    DE_UNREF(pAllocator);
    auto ycbcr = reinterpret_cast<const VkSamplerYcbcrConversionInfo *>(
        findStructureInChain(pCreateInfo->pNext, VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO));
    if (ycbcr)
    {
        if (auto result = m_pipelineInput.samplerYcbcrConversions.find(ycbcr->conversion);
            result != std::end(m_pipelineInput.samplerYcbcrConversions))
        {
            // Rewrite handle(s) in create info to unique object id(s)
            auto pCreateInfoString = vksc_server::json::writeJSON_VkSamplerCreateInfo(m_jsonContext, *pCreateInfo);
            Json::Value pCreateInfoJson;
            m_jsonContext.reader->parse(pCreateInfoString.c_str(), pCreateInfoString.c_str() + pCreateInfoString.size(),
                                        &pCreateInfoJson, nullptr);
            auto &ycbcrJson = findStructureInJson(pCreateInfoJson, "VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO");
            ycbcrJson["conversion"] = result->second.uniqueObjId;
            // Store local copy of dependent create info
            m_pipelineInput.samplers.insert(
                {*pSampler, {m_jsonContext.writer->write(pCreateInfoJson), {result->second}, ++m_uniqueObjIdCounter}});
        }
        else
        {
            TCU_THROW(InternalError, "Failed to find sampler ycbcr conversion referenced by sampler");
        }
    }
    else
    {
        m_pipelineInput.samplers.insert(
            {*pSampler, {writeJSON_VkSamplerCreateInfo(m_jsonContext, *pCreateInfo), {}, ++m_uniqueObjIdCounter}});
    }
}

void ResourceInterfaceStandard::createSamplerYcbcrConversion(VkDevice device,
                                                             const VkSamplerYcbcrConversionCreateInfo *pCreateInfo,
                                                             const VkAllocationCallbacks *pAllocator,
                                                             VkSamplerYcbcrConversion *pYcbcrConversion) const
{
    DE_UNREF(device);
    DE_UNREF(pAllocator);
    m_pipelineInput.samplerYcbcrConversions.insert(
        {*pYcbcrConversion,
         {writeJSON_VkSamplerYcbcrConversionCreateInfo(m_jsonContext, *pCreateInfo), ++m_uniqueObjIdCounter}});
}

void ResourceInterfaceStandard::createCommandPool(VkDevice device, const VkCommandPoolCreateInfo *pCreateInfo,
                                                  const VkAllocationCallbacks *pAllocator,
                                                  VkCommandPool *pCommandPool) const
{
    DE_UNREF(device);
    DE_UNREF(pCreateInfo);
    DE_UNREF(pAllocator);
    m_commandPoolMemoryConsumption.push_back(vksc_server::VulkanCommandMemoryConsumption(pCommandPool->getInternal()));
}

void ResourceInterfaceStandard::allocateCommandBuffers(VkDevice device,
                                                       const VkCommandBufferAllocateInfo *pAllocateInfo,
                                                       VkCommandBuffer *pCommandBuffers) const
{
    DE_UNREF(device);
    for (uint32_t i = 0; i < pAllocateInfo->commandBufferCount; ++i)
    {
        m_commandBufferMemoryConsumption.insert({pCommandBuffers[i], vksc_server::VulkanCommandMemoryConsumption(
                                                                         pAllocateInfo->commandPool.getInternal())});
    }
}

void ResourceInterfaceStandard::increaseCommandBufferSize(VkCommandBuffer commandBuffer, VkDeviceSize commandSize) const
{
    auto it = m_commandBufferMemoryConsumption.find(commandBuffer);
    if (it == end(m_commandBufferMemoryConsumption))
        TCU_THROW(InternalError, "Unregistered command buffer");

    it->second.updateValues(commandSize, commandSize, commandSize);
}

void ResourceInterfaceStandard::resetCommandPool(VkDevice device, VkCommandPool commandPool,
                                                 VkCommandPoolResetFlags flags) const
{
    DE_UNREF(device);
    DE_UNREF(flags);

    for (auto &memC : m_commandBufferMemoryConsumption)
    {
        if (memC.second.commandPool == commandPool.getInternal())
            memC.second.resetValues();
    }
}

void ResourceInterfaceStandard::importPipelineCacheData(const PlatformInterface &vkp, VkInstance instance,
                                                        const InstanceInterface &vki, VkPhysicalDevice physicalDevice,
                                                        uint32_t queueIndex)
{
    if (!std::string(m_testCtx.getCommandLine().getPipelineCompilerPath()).empty())
    {
        m_cacheData = vksc_server::buildOfflinePipelineCache(
            m_pipelineInput, std::string(m_testCtx.getCommandLine().getPipelineCompilerPath()),
            std::string(m_testCtx.getCommandLine().getPipelineCompilerDataDir()),
            std::string(m_testCtx.getCommandLine().getPipelineCompilerArgs()),
            std::string(m_testCtx.getCommandLine().getPipelineCompilerOutputFile()),
            std::string(m_testCtx.getCommandLine().getPipelineCompilerLogFile()),
            std::string(m_testCtx.getCommandLine().getPipelineCompilerFilePrefix()));
    }
    else
    {
        m_cacheData = vksc_server::buildPipelineCache(m_pipelineInput, vkp, instance, vki, physicalDevice, queueIndex);
    }

    VkPhysicalDeviceVulkanSC10Properties vulkanSC10Properties = initVulkanStructure();
    VkPhysicalDeviceProperties2 deviceProperties2             = initVulkanStructure(&vulkanSC10Properties);
    vki.getPhysicalDeviceProperties2(physicalDevice, &deviceProperties2);

    m_pipelineSizes = vksc_server::extractSizesFromPipelineCache(
        m_pipelineInput, m_cacheData, uint32_t(m_testCtx.getCommandLine().getPipelineDefaultSize()),
        vulkanSC10Properties.recyclePipelineMemory == VK_TRUE);
    preparePipelinePoolSizes();
}

void ResourceInterfaceStandard::resetObjects()
{
    m_pipelineInput = {};
    m_objectHashes.clear();
    m_commandPoolMemoryConsumption.clear();
    m_commandPoolIndex = 0u;
    m_commandBufferMemoryConsumption.clear();
    m_resourceCounter = 0u;
    m_statCurrent     = resetDeviceObjectReservationCreateInfo();
    m_statMax         = resetDeviceObjectReservationCreateInfo();
    // pipelineCacheRequestCount does not contain one instance of createPipelineCache call that happens only in subprocess
    m_statCurrent.pipelineCacheRequestCount = 1u;
    m_statMax.pipelineCacheRequestCount     = 1u;
    m_cacheData.clear();
    m_pipelineIdentifiers.clear();
    m_pipelineSizes.clear();
    m_pipelinePoolSizes.clear();
    runGarbageCollection(m_jsonContext);
}

void ResourceInterfaceStandard::resetPipelineCaches()
{
    if (m_testCtx.getCommandLine().isSubProcess())
    {
        m_devicePipelineCaches.clear();
    }
}

bool ResourceInterfaceStandard::resetPipelineCache(VkDevice device, bool onlyIfInSubprocess)
{
    if (auto it = m_devicePipelineCaches.find(device);
        it != m_devicePipelineCaches.end() && (!onlyIfInSubprocess || m_testCtx.getCommandLine().isSubProcess()))
    {
        if (it->second.refCount == 1)
        {
            // Last reference so ready to delete pipeline cache
            m_devicePipelineCaches.erase(it);
        }
        else
        {
            // Only decrement reference count
            it->second.refCount--;
        }
    }
    return false;
}

#endif // CTS_USES_VULKANSC

vk::ProgramBinary *ResourceInterfaceStandard::compileProgram(const vk::ProgramIdentifier &progId,
                                                             const vk::GlslSource &source,
                                                             glu::ShaderProgramInfo *buildInfo,
                                                             const tcu::CommandLine &commandLine)
{
    DE_UNREF(progId);
    return vk::buildProgram(source, buildInfo, commandLine);
}

vk::ProgramBinary *ResourceInterfaceStandard::compileProgram(const vk::ProgramIdentifier &progId,
                                                             const vk::HlslSource &source,
                                                             glu::ShaderProgramInfo *buildInfo,
                                                             const tcu::CommandLine &commandLine)
{
    DE_UNREF(progId);
    return vk::buildProgram(source, buildInfo, commandLine);
}

vk::ProgramBinary *ResourceInterfaceStandard::compileProgram(const vk::ProgramIdentifier &progId,
                                                             const vk::SpirVAsmSource &source,
                                                             vk::SpirVProgramInfo *buildInfo,
                                                             const tcu::CommandLine &commandLine)
{
    DE_UNREF(progId);
    return vk::assembleProgram(source, buildInfo, commandLine);
}

#ifdef CTS_USES_VULKANSC

ResourceInterfaceVKSC::ResourceInterfaceVKSC(tcu::TestContext &testCtx) : ResourceInterfaceStandard(testCtx)
{
    m_address = std::string(testCtx.getCommandLine().getServerAddress());
}

vksc_server::Server *ResourceInterfaceVKSC::getServer()
{
    if (!m_server)
    {
        m_server = std::make_shared<vksc_server::Server>(m_address);
    }
    return m_server.get();
}

bool ResourceInterfaceVKSC::noServer() const
{
    return m_address.empty();
}

vk::ProgramBinary *ResourceInterfaceVKSC::compileProgram(const vk::ProgramIdentifier &progId,
                                                         const vk::GlslSource &source,
                                                         glu::ShaderProgramInfo *buildInfo,
                                                         const tcu::CommandLine &commandLine)
{
    if (noServer())
        return ResourceInterfaceStandard::compileProgram(progId, source, buildInfo, commandLine);

    DE_UNREF(progId);
    DE_UNREF(buildInfo);

    vksc_server::CompileShaderRequest request;
    request.source.active = "glsl";
    request.source.glsl   = source;
    request.commandLine   = commandLine.getInitialCmdLine();
    vksc_server::CompileShaderResponse response;
    getServer()->SendRequest(request, response);

    return new ProgramBinary(PROGRAM_FORMAT_SPIRV, response.binary.size(), response.binary.data());
}

vk::ProgramBinary *ResourceInterfaceVKSC::compileProgram(const vk::ProgramIdentifier &progId,
                                                         const vk::HlslSource &source,
                                                         glu::ShaderProgramInfo *buildInfo,
                                                         const tcu::CommandLine &commandLine)
{
    if (noServer())
        return ResourceInterfaceStandard::compileProgram(progId, source, buildInfo, commandLine);

    DE_UNREF(progId);
    DE_UNREF(buildInfo);

    vksc_server::CompileShaderRequest request;
    request.source.active = "hlsl";
    request.source.hlsl   = source;
    request.commandLine   = commandLine.getInitialCmdLine();
    vksc_server::CompileShaderResponse response;
    getServer()->SendRequest(request, response);

    return new ProgramBinary(PROGRAM_FORMAT_SPIRV, response.binary.size(), response.binary.data());
}

vk::ProgramBinary *ResourceInterfaceVKSC::compileProgram(const vk::ProgramIdentifier &progId,
                                                         const vk::SpirVAsmSource &source,
                                                         vk::SpirVProgramInfo *buildInfo,
                                                         const tcu::CommandLine &commandLine)
{
    if (noServer())
        return ResourceInterfaceStandard::compileProgram(progId, source, buildInfo, commandLine);

    DE_UNREF(progId);
    DE_UNREF(buildInfo);

    vksc_server::CompileShaderRequest request;
    request.source.active = "spirv";
    request.source.spirv  = source;
    request.commandLine   = commandLine.getInitialCmdLine();
    vksc_server::CompileShaderResponse response;
    getServer()->SendRequest(request, response);

    return new ProgramBinary(PROGRAM_FORMAT_SPIRV, response.binary.size(), response.binary.data());
}

VkResult ResourceInterfaceVKSC::createShaderModule(VkDevice device, const VkShaderModuleCreateInfo *pCreateInfo,
                                                   const VkAllocationCallbacks *pAllocator,
                                                   VkShaderModule *pShaderModule, bool normalMode) const
{
    if (noServer() || !normalMode || !isVulkanSC())
        return ResourceInterfaceStandard::createShaderModule(device, pCreateInfo, pAllocator, pShaderModule,
                                                             normalMode);

    // We will reach this place only in one case:
    // - server exists
    // - subprocess asks for creation of VkShaderModule which will be later ignored, because it will receive the whole pipeline from server
    // ( Are there any tests which receive VkShaderModule and do not use it in any pipeline ? )
    *pShaderModule = incResourceCounter<VkShaderModule>();
    registerObjectHash(pShaderModule->getInternal(), calculateShaderModuleHash(*pCreateInfo, getObjectHashes()));
    return VK_SUCCESS;
}

void ResourceInterfaceVKSC::importPipelineCacheData(const PlatformInterface &vkp, VkInstance instance,
                                                    const InstanceInterface &vki, VkPhysicalDevice physicalDevice,
                                                    uint32_t queueIndex)
{
    if (noServer())
    {
        ResourceInterfaceStandard::importPipelineCacheData(vkp, instance, vki, physicalDevice, queueIndex);
        return;
    }

    vksc_server::CreateCacheRequest request;
    request.input                 = m_pipelineInput;
    std::vector<int> caseFraction = m_testCtx.getCommandLine().getCaseFraction();
    request.caseFraction          = caseFraction.empty() ? -1 : caseFraction[0];

    vksc_server::CreateCacheResponse response;
    getServer()->SendRequest(request, response);

    if (response.status)
    {
        m_cacheData = std::move(response.binary);

        VkPhysicalDeviceVulkanSC10Properties vulkanSC10Properties = initVulkanStructure();
        VkPhysicalDeviceProperties2 deviceProperties2             = initVulkanStructure(&vulkanSC10Properties);
        vki.getPhysicalDeviceProperties2(physicalDevice, &deviceProperties2);

        m_pipelineSizes = vksc_server::extractSizesFromPipelineCache(
            m_pipelineInput, m_cacheData, uint32_t(m_testCtx.getCommandLine().getPipelineDefaultSize()),
            vulkanSC10Properties.recyclePipelineMemory == VK_TRUE);
        preparePipelinePoolSizes();
    }
    else
    {
        TCU_THROW(InternalError,
                  "Server did not return pipeline cache data when requested (check server log for details)");
    }
}

MultithreadedDestroyGuard::MultithreadedDestroyGuard(de::SharedPtr<vk::ResourceInterface> resourceInterface)
    : m_resourceInterface{resourceInterface}
{
    if (m_resourceInterface.get() != nullptr)
        m_resourceInterface->setHandleDestroy(false);
}

MultithreadedDestroyGuard::~MultithreadedDestroyGuard()
{
    if (m_resourceInterface.get() != nullptr)
        m_resourceInterface->setHandleDestroy(true);
}

#endif // CTS_USES_VULKANSC

} // namespace vk
