/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 The Khronos Group Inc.
 * Copyright (c) 2026 NVIDIA Corporation.
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
 * \file vktBindingPushConstantBankTests.cpp
 * \brief VK_NV_push_constant_bank extension tests
 *//*--------------------------------------------------------------------*/

#include "vktBindingPushConstantBankTests.hpp"

#include <array>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>

#include "deRandom.hpp"
#include "deUniquePtr.hpp"
#include "vkCmdUtil.hpp"
#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

namespace vkt
{
namespace BindingModel
{
namespace
{

using namespace vk;
using de::MovePtr;

// Maximum number of push constant banks to test
static const uint32_t kMaxTestBanks = 8u;

enum class TestType
{
    COMPUTE_BASIC,                 // Basic compute test with multiple banks (non-descriptor heap)
    GRAPHICS_BASIC,                // Basic graphics test with multiple banks (non-descriptor heap)
    COMPUTE_WITH_DESCRIPTOR_HEAP,  // Compute test with descriptor heap integration
    GRAPHICS_WITH_DESCRIPTOR_HEAP, // Graphics test with descriptor heap integration
    COMPUTE_MEMBER_OFFSET,         // Compute test with member_offset qualifier
    GRAPHICS_MEMBER_OFFSET,        // Graphics test with member_offset qualifier
};

struct TestParams
{
    TestType testType;
    uint32_t numBanks;     // Number of banks to use (up to device limit)
    uint32_t memberOffset; // Offset in bytes for member_offset tests (0 for basic tests)
};

inline bool usesDescriptorHeap(TestType type)
{
    return type == TestType::COMPUTE_WITH_DESCRIPTOR_HEAP || type == TestType::GRAPHICS_WITH_DESCRIPTOR_HEAP;
}

inline bool usesMemberOffset(TestType type)
{
    return type == TestType::COMPUTE_MEMBER_OFFSET || type == TestType::GRAPHICS_MEMBER_OFFSET;
}

inline bool isComputeTest(TestType type)
{
    return type == TestType::COMPUTE_BASIC || type == TestType::COMPUTE_WITH_DESCRIPTOR_HEAP ||
           type == TestType::COMPUTE_MEMBER_OFFSET;
}

VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

// Helper struct for result buffer resources
struct ResultBufferResources
{
    Move<VkBuffer> buffer;
    MovePtr<Allocation> memory;
    uint32_t *ptr;
    VkDeviceSize size;
};

using ResultBufferResourcesPtr = std::unique_ptr<ResultBufferResources>;

// Helper struct for descriptor set resources (non-heap tests)
struct DescriptorResources
{
    Move<VkDescriptorSetLayout> layout;
    Move<VkDescriptorPool> pool;
    Move<VkDescriptorSet> set;
};

using DescriptorResourcesPtr = std::unique_ptr<DescriptorResources>;

// Helper: Create and initialize result buffer
ResultBufferResourcesPtr createResultBuffer(const DeviceInterface &vkd, VkDevice device, Allocator &allocator)
{
    ResultBufferResourcesPtr resPtr(new ResultBufferResources);
    auto &res = *resPtr;

    res.size = sizeof(uint32_t) * kMaxTestBanks;

    const VkBufferCreateInfo bufferInfo = makeBufferCreateInfo(res.size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    res.buffer                          = createBuffer(vkd, device, &bufferInfo);
    res.memory =
        allocator.allocate(getBufferMemoryRequirements(vkd, device, *res.buffer), MemoryRequirement::HostVisible);
    VK_CHECK(vkd.bindBufferMemory(device, *res.buffer, res.memory->getMemory(), res.memory->getOffset()));

    res.ptr = static_cast<uint32_t *>(res.memory->getHostPtr());
    for (uint32_t i = 0; i < kMaxTestBanks; ++i)
        res.ptr[i] = ~0u;
    flushAlloc(vkd, device, *res.memory);

    return resPtr;
}

// Helper: Create descriptor set layout, pool, and set for SSBO
DescriptorResourcesPtr createDescriptorResources(const DeviceInterface &vkd, VkDevice device,
                                                 VkShaderStageFlags stageFlags, VkBuffer resultBuffer)
{
    DescriptorResourcesPtr resPtr(new DescriptorResources);
    auto &res = *resPtr;

    const VkDescriptorSetLayoutBinding binding       = {0u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, stageFlags, nullptr};
    const VkDescriptorSetLayoutCreateInfo layoutInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr,
                                                        0u, 1u, &binding};
    res.layout                                       = createDescriptorSetLayout(vkd, device, &layoutInfo);

    const VkDescriptorPoolSize poolSize       = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u};
    const VkDescriptorPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                 nullptr,
                                                 VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                                                 1u,
                                                 1u,
                                                 &poolSize};
    res.pool                                  = createDescriptorPool(vkd, device, &poolInfo);

    const VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, *res.pool,
                                                   1u, &*res.layout};
    res.set                                     = allocateDescriptorSet(vkd, device, &allocInfo);

    const VkDescriptorBufferInfo bufferDescInfo = {resultBuffer, 0ull, VK_WHOLE_SIZE};
    const VkWriteDescriptorSet writeDescSet     = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, *res.set,        0u,     0u, 1u,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,      nullptr, &bufferDescInfo, nullptr};
    vkd.updateDescriptorSets(device, 1u, &writeDescSet, 0u, nullptr);

    return resPtr;
}

// Helper: Create push constant ranges for banks
std::vector<VkPushConstantRange> createPushConstantRanges(uint32_t numBanks, VkShaderStageFlags stageFlags,
                                                          uint32_t size)
{
    std::vector<VkPushConstantRange> ranges(numBanks);
    for (uint32_t bank = 0; bank < numBanks; ++bank)
    {
        ranges[bank].stageFlags = stageFlags;
        ranges[bank].offset     = 0;
        ranges[bank].size       = size;
    }
    return ranges;
}

// Helper: Push constants for all banks using vkCmdPushConstants2
void pushConstantsForBanks(const DeviceInterface &vkd, VkCommandBuffer cmdBuffer, VkPipelineLayout layout,
                           VkShaderStageFlags stageFlags, uint32_t numBanks, uint32_t memberOffset)
{
    const uint32_t pushConstantSize = memberOffset + sizeof(uint32_t);
    std::vector<uint8_t> pushData(pushConstantSize);

    for (uint32_t bank = 0; bank < numBanks; ++bank)
    {
        std::memset(pushData.data(), 0, pushConstantSize);
        uint32_t dataValue = bank;
        std::memcpy(pushData.data(), &dataValue, sizeof(dataValue));

        VkPushConstantBankInfoNV bankInfo = {};
        bankInfo.sType                    = VK_STRUCTURE_TYPE_PUSH_CONSTANT_BANK_INFO_NV;
        bankInfo.pNext                    = nullptr;
        bankInfo.bank                     = bank;

        VkPushConstantsInfo pushInfo = initVulkanStructure();
        pushInfo.pNext               = &bankInfo;
        pushInfo.layout              = layout;
        pushInfo.stageFlags          = stageFlags;
        pushInfo.offset              = memberOffset;
        pushInfo.size                = pushConstantSize;
        pushInfo.pValues             = pushData.data();

        vkd.cmdPushConstants2(cmdBuffer, &pushInfo);
    }
}

// Helper: Verify result buffer contents
tcu::TestStatus verifyResults(tcu::TestContext &testCtx, const DeviceInterface &vkd, VkDevice device,
                              Allocation &resultMemory, uint32_t *resultPtr, uint32_t numBanks,
                              uint32_t memberOffset = 0)
{
    invalidateAlloc(vkd, device, resultMemory);

    bool pass = true;
    for (uint32_t bank = 0; bank < numBanks; ++bank)
    {
        if (resultPtr[bank] != bank)
        {
            testCtx.getLog() << tcu::TestLog::Message << "Bank " << bank
                             << (memberOffset > 0 ? " with member_offset " + std::to_string(memberOffset) : "")
                             << ": expected " << bank << ", got " << resultPtr[bank] << tcu::TestLog::EndMessage;
            pass = false;
        }
    }

    return pass ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail("Result mismatch");
}

// Helper: Push data for all banks using vkCmdPushDataEXT (descriptor heap mode)
void pushDataForBanks(const DeviceInterface &vkd, VkCommandBuffer cmdBuffer, uint32_t numBanks)
{
    for (uint32_t bank = 0; bank < numBanks; ++bank)
    {
        uint32_t pushData = bank;

        VkPushConstantBankInfoNV bankInfo = {};
        bankInfo.sType                    = VK_STRUCTURE_TYPE_PUSH_CONSTANT_BANK_INFO_NV;
        bankInfo.pNext                    = nullptr;
        bankInfo.bank                     = bank;

        VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
        pushDataInfo.pNext             = &bankInfo;
        pushDataInfo.offset            = 0;
        pushDataInfo.data.address      = &pushData;
        pushDataInfo.data.size         = sizeof(pushData);

        vkd.cmdPushDataEXT(cmdBuffer, &pushDataInfo);
    }
}

// Helper: Create graphics pipeline with rasterizer discard
Move<VkPipeline> createGraphicsPipelineWithDiscard(const DeviceInterface &vkd, VkDevice device,
                                                   VkShaderModule vertexModule, VkPipelineLayout layout)
{
    VkPipelineShaderStageCreateInfo vertexStage = initVulkanStructure();
    vertexStage.stage                           = VK_SHADER_STAGE_VERTEX_BIT;
    vertexStage.module                          = vertexModule;
    vertexStage.pName                           = "main";

    VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = initVulkanStructure();
    inputAssemblyState.topology                               = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    VkViewport viewport = makeViewport(1u, 1u);
    VkRect2D scissor    = makeRect2D(1u, 1u);

    VkPipelineViewportStateCreateInfo viewportState = initVulkanStructure();
    viewportState.viewportCount                     = 1;
    viewportState.pViewports                        = &viewport;
    viewportState.scissorCount                      = 1;
    viewportState.pScissors                         = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizationState = initVulkanStructure();
    rasterizationState.rasterizerDiscardEnable                = VK_TRUE;
    rasterizationState.polygonMode                            = VK_POLYGON_MODE_FILL;
    rasterizationState.lineWidth                              = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampleState = initVulkanStructure();
    multisampleState.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendState = initVulkanStructure();

    VkGraphicsPipelineCreateInfo pipelineInfo = initVulkanStructure();
    pipelineInfo.stageCount                   = 1;
    pipelineInfo.pStages                      = &vertexStage;
    pipelineInfo.pVertexInputState            = &vertexInputState;
    pipelineInfo.pInputAssemblyState          = &inputAssemblyState;
    pipelineInfo.pViewportState               = &viewportState;
    pipelineInfo.pRasterizationState          = &rasterizationState;
    pipelineInfo.pMultisampleState            = &multisampleState;
    pipelineInfo.pColorBlendState             = &colorBlendState;
    pipelineInfo.layout                       = layout;

    return createGraphicsPipeline(vkd, device, VK_NULL_HANDLE, &pipelineInfo);
}

class PushConstantBankTestInstance : public TestInstance
{
public:
    PushConstantBankTestInstance(Context &context, const TestParams &params);
    tcu::TestStatus iterate() override;

private:
    tcu::TestStatus runComputeTest();
    tcu::TestStatus runGraphicsTest();
    tcu::TestStatus runComputeDescriptorHeapTest();
    tcu::TestStatus runGraphicsDescriptorHeapTest();
    tcu::TestStatus runComputeMemberOffsetTest();
    tcu::TestStatus runGraphicsMemberOffsetTest();

    TestParams m_params;

    VkPhysicalDevice m_physDevice;
    Move<VkDevice> m_device;
    MovePtr<DeviceDriver> m_deviceInterface;
    VkQueue m_queue;
    uint32_t m_queueFamilyIndex;
    MovePtr<Allocator> m_allocatorPtr;

    VkPhysicalDevicePushConstantBankPropertiesNV m_pushConstantBankProperties;
    VkPhysicalDeviceDescriptorHeapPropertiesEXT m_descriptorHeapProperties;
};

PushConstantBankTestInstance::PushConstantBankTestInstance(Context &context, const TestParams &params)
    : TestInstance(context)
    , m_params(params)
    , m_physDevice(context.getPhysicalDevice())
    , m_pushConstantBankProperties()
    , m_descriptorHeapProperties()
{
    const auto &vki              = context.getInstanceInterface();
    const bool useDescriptorHeap = usesDescriptorHeap(m_params.testType);

    // Query push constant bank properties
    m_pushConstantBankProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_CONSTANT_BANK_PROPERTIES_NV;
    m_pushConstantBankProperties.pNext = nullptr;

    VkPhysicalDeviceProperties2 properties2 = initVulkanStructure();
    properties2.pNext                       = &m_pushConstantBankProperties;

    if (useDescriptorHeap)
    {
        m_descriptorHeapProperties.sType   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_PROPERTIES_EXT;
        m_descriptorHeapProperties.pNext   = nullptr;
        m_pushConstantBankProperties.pNext = &m_descriptorHeapProperties;
    }

    vki.getPhysicalDeviceProperties2(m_physDevice, &properties2);

    // Create custom device with required extensions and features
    const float queuePriority         = 1.0f;
    VkDeviceQueueCreateInfo queueInfo = initVulkanStructure();

    // Find appropriate queue family
    const auto queueFamilyProperties = getPhysicalDeviceQueueFamilyProperties(vki, m_physDevice);
    VkQueueFlags requiredQueueFlags  = isComputeTest(m_params.testType) ? VK_QUEUE_COMPUTE_BIT : VK_QUEUE_GRAPHICS_BIT;

    m_queueFamilyIndex = 0;
    for (uint32_t i = 0; i < queueFamilyProperties.size(); ++i)
    {
        if ((queueFamilyProperties[i].queueFlags & requiredQueueFlags) == requiredQueueFlags)
        {
            m_queueFamilyIndex = i;
            break;
        }
    }

    queueInfo.queueFamilyIndex = m_queueFamilyIndex;
    queueInfo.queueCount       = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    // Build extension list
    std::vector<const char *> deviceExtensions;
    deviceExtensions.push_back(VK_NV_PUSH_CONSTANT_BANK_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_MAINTENANCE_5_EXTENSION_NAME);

    if (useDescriptorHeap)
    {
        deviceExtensions.push_back(VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME);
        deviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
        deviceExtensions.push_back(VK_KHR_SHADER_UNTYPED_POINTERS_EXTENSION_NAME);
        deviceExtensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    }

    // Build feature chain
    VkPhysicalDevicePushConstantBankFeaturesNV pushConstantBankFeatures = {};
    pushConstantBankFeatures.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_CONSTANT_BANK_FEATURES_NV;
    pushConstantBankFeatures.pNext            = nullptr;
    pushConstantBankFeatures.pushConstantBank = VK_TRUE;

    VkPhysicalDeviceMaintenance5FeaturesKHR maintenance5Features = initVulkanStructure();
    maintenance5Features.pNext                                   = &pushConstantBankFeatures;
    maintenance5Features.maintenance5                            = VK_TRUE;

    VkPhysicalDeviceVulkan12Features vulkan12Features = initVulkanStructure();
    vulkan12Features.pNext                            = &maintenance5Features;
    vulkan12Features.bufferDeviceAddress              = useDescriptorHeap ? VK_TRUE : VK_FALSE;

    VkPhysicalDeviceVulkan13Features vulkan13Features = initVulkanStructure();
    vulkan13Features.pNext                            = &vulkan12Features;
    vulkan13Features.dynamicRendering                 = VK_TRUE;
    vulkan13Features.synchronization2                 = useDescriptorHeap ? VK_TRUE : VK_FALSE;

    VkPhysicalDeviceDescriptorHeapFeaturesEXT descriptorHeapFeatures = initVulkanStructure();
    descriptorHeapFeatures.pNext                                     = &vulkan13Features;
    descriptorHeapFeatures.descriptorHeap                            = VK_TRUE;

    VkPhysicalDeviceFeatures2 features2 = initVulkanStructure();
    features2.pNext =
        useDescriptorHeap ? static_cast<void *>(&descriptorHeapFeatures) : static_cast<void *>(&vulkan13Features);
    features2.features.vertexPipelineStoresAndAtomics = VK_TRUE;

    VkDeviceCreateInfo deviceInfo      = initVulkanStructure();
    deviceInfo.pNext                   = &features2;
    deviceInfo.queueCreateInfoCount    = 1;
    deviceInfo.pQueueCreateInfos       = &queueInfo;
    deviceInfo.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();

    m_device =
        createCustomDevice(context.getPlatformInterface(), context.getInstance(), vki, m_physDevice, &deviceInfo);

    m_deviceInterface =
        MovePtr<DeviceDriver>(new DeviceDriver(context.getPlatformInterface(), context.getInstance(), *m_device,
                                               context.getUsedApiVersion(), context.getTestContext().getCommandLine()));

    m_deviceInterface->getDeviceQueue(*m_device, m_queueFamilyIndex, 0, &m_queue);

    m_allocatorPtr = MovePtr<Allocator>(
        new SimpleAllocator(*m_deviceInterface, *m_device, getPhysicalDeviceMemoryProperties(vki, m_physDevice)));
}

tcu::TestStatus PushConstantBankTestInstance::iterate()
{
    switch (m_params.testType)
    {
    case TestType::COMPUTE_BASIC:
        return runComputeTest();
    case TestType::GRAPHICS_BASIC:
        return runGraphicsTest();
    case TestType::COMPUTE_WITH_DESCRIPTOR_HEAP:
        return runComputeDescriptorHeapTest();
    case TestType::GRAPHICS_WITH_DESCRIPTOR_HEAP:
        return runGraphicsDescriptorHeapTest();
    case TestType::COMPUTE_MEMBER_OFFSET:
        return runComputeMemberOffsetTest();
    case TestType::GRAPHICS_MEMBER_OFFSET:
        return runGraphicsMemberOffsetTest();
    default:
        TCU_FAIL("Unknown test type");
    }
}

//
// Non-descriptor heap compute test using vkCmdPushConstants2 with VkPushConstantBankInfoNV
//
tcu::TestStatus PushConstantBankTestInstance::runComputeTest()
{
    const auto &vkd = *m_deviceInterface;

    const uint32_t maxBanks = m_pushConstantBankProperties.maxComputePushConstantBanks;
    const uint32_t numBanks = de::min(m_params.numBanks, de::min(maxBanks, kMaxTestBanks));

    if (numBanks < 1)
        TCU_THROW(NotSupportedError, "No compute push constant banks available");

    auto resultResPtr = createResultBuffer(vkd, *m_device, *m_allocatorPtr);
    auto &resultRes   = *resultResPtr;
    auto descResPtr   = createDescriptorResources(vkd, *m_device, VK_SHADER_STAGE_COMPUTE_BIT, *resultRes.buffer);
    auto &descRes     = *descResPtr;

    auto pushConstantRanges = createPushConstantRanges(numBanks, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(uint32_t));
    const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                           nullptr,
                                                           0u,
                                                           1u,
                                                           &*descRes.layout,
                                                           numBanks,
                                                           pushConstantRanges.data()};
    Move<VkPipelineLayout> pipelineLayout               = createPipelineLayout(vkd, *m_device, &pipelineLayoutInfo);

    const auto shaderModule = createShaderModule(vkd, *m_device, m_context.getBinaryCollection().get("compute"));
    const VkPipelineShaderStageCreateInfo shaderStageInfo = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                             nullptr,
                                                             0u,
                                                             VK_SHADER_STAGE_COMPUTE_BIT,
                                                             *shaderModule,
                                                             "main",
                                                             nullptr};
    const VkComputePipelineCreateInfo computePipelineInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                                             nullptr,
                                                             0u,
                                                             shaderStageInfo,
                                                             *pipelineLayout,
                                                             VK_NULL_HANDLE,
                                                             0};
    Move<VkPipeline> pipeline = createComputePipeline(vkd, *m_device, VK_NULL_HANDLE, &computePipelineInfo);

    const auto cmdPool   = makeCommandPool(vkd, *m_device, m_queueFamilyIndex);
    const auto cmdBuffer = allocateCommandBuffer(vkd, *m_device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vkd, *cmdBuffer);
    vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    vkd.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &*descRes.set, 0u,
                              nullptr);

    pushConstantsForBanks(vkd, *cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, numBanks, 0);

    vkd.cmdDispatch(*cmdBuffer, 1u, 1u, 1u);

    const VkMemoryBarrier memoryBarrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT,
                                           VK_ACCESS_HOST_READ_BIT};
    vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                           &memoryBarrier, 0u, nullptr, 0u, nullptr);
    endCommandBuffer(vkd, *cmdBuffer);

    submitCommandsAndWait(vkd, *m_device, m_queue, *cmdBuffer);

    return verifyResults(m_context.getTestContext(), vkd, *m_device, *resultRes.memory, resultRes.ptr, numBanks);
}

//
// Non-descriptor heap graphics test using vkCmdPushConstants2 with VkPushConstantBankInfoNV
//
tcu::TestStatus PushConstantBankTestInstance::runGraphicsTest()
{
    const auto &vkd = *m_deviceInterface;

    const uint32_t maxBanks = m_pushConstantBankProperties.maxGraphicsPushConstantBanks;
    const uint32_t numBanks = de::min(m_params.numBanks, de::min(maxBanks, kMaxTestBanks));

    if (numBanks < 1)
        TCU_THROW(NotSupportedError, "No graphics push constant banks available");

    auto resultResPtr = createResultBuffer(vkd, *m_device, *m_allocatorPtr);
    auto &resultRes   = *resultResPtr;
    auto descResPtr   = createDescriptorResources(vkd, *m_device, VK_SHADER_STAGE_VERTEX_BIT, *resultRes.buffer);
    auto &descRes     = *descResPtr;

    auto pushConstantRanges = createPushConstantRanges(numBanks, VK_SHADER_STAGE_VERTEX_BIT, sizeof(uint32_t));
    const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                           nullptr,
                                                           0u,
                                                           1u,
                                                           &*descRes.layout,
                                                           numBanks,
                                                           pushConstantRanges.data()};
    Move<VkPipelineLayout> pipelineLayout               = createPipelineLayout(vkd, *m_device, &pipelineLayoutInfo);

    const auto vertexModule   = createShaderModule(vkd, *m_device, m_context.getBinaryCollection().get("vertex"));
    Move<VkPipeline> pipeline = createGraphicsPipelineWithDiscard(vkd, *m_device, *vertexModule, *pipelineLayout);

    const auto cmdPool   = makeCommandPool(vkd, *m_device, m_queueFamilyIndex);
    const auto cmdBuffer = allocateCommandBuffer(vkd, *m_device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vkd, *cmdBuffer);

    VkRenderingInfo renderingInfo = initVulkanStructure();
    renderingInfo.renderArea      = makeRect2D(1u, 1u);
    renderingInfo.layerCount      = 1u;
    vkd.cmdBeginRendering(*cmdBuffer, &renderingInfo);

    vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
    vkd.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &*descRes.set, 0u,
                              nullptr);

    pushConstantsForBanks(vkd, *cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, numBanks, 0);

    vkd.cmdDraw(*cmdBuffer, 1u, 1u, 0u, 0u);
    vkd.cmdEndRendering(*cmdBuffer);

    const VkMemoryBarrier memoryBarrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT,
                                           VK_ACCESS_HOST_READ_BIT};
    vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                           &memoryBarrier, 0u, nullptr, 0u, nullptr);
    endCommandBuffer(vkd, *cmdBuffer);

    submitCommandsAndWait(vkd, *m_device, m_queue, *cmdBuffer);

    return verifyResults(m_context.getTestContext(), vkd, *m_device, *resultRes.memory, resultRes.ptr, numBanks);
}

//
// Descriptor heap compute test using vkCmdPushDataEXT with VkPushDataInfoEXT and VkPushConstantBankInfoNV
//
tcu::TestStatus PushConstantBankTestInstance::runComputeDescriptorHeapTest()
{
    const auto &vkd = *m_deviceInterface;

    // Determine number of banks to test - use push data banks for descriptor heap mode
    const uint32_t maxBanks = m_pushConstantBankProperties.maxComputePushDataBanks;
    const uint32_t numBanks = de::min(m_params.numBanks, de::min(maxBanks, kMaxTestBanks));

    if (numBanks < 1)
    {
        TCU_THROW(NotSupportedError, "No compute push data banks available");
    }

    // Calculate heap sizes
    const VkDeviceSize bufferDescriptorStride =
        alignUp(m_descriptorHeapProperties.bufferDescriptorSize, m_descriptorHeapProperties.bufferDescriptorAlignment);
    const VkDeviceSize resourceHeapUserSize = bufferDescriptorStride;
    const VkDeviceSize resourceHeapReservedOffset =
        alignUp(resourceHeapUserSize, m_descriptorHeapProperties.resourceHeapAlignment);
    const VkDeviceSize resourceHeapSize =
        resourceHeapReservedOffset + m_descriptorHeapProperties.minResourceHeapReservedRange;

    // Create resource heap buffer
    VkBufferUsageFlags2KHR heapUsage =
        VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR;

    VkBufferUsageFlags2CreateInfoKHR heapUsageFlags2 = initVulkanStructure();
    heapUsageFlags2.usage                            = heapUsage;

    VkBufferCreateInfo resourceHeapBufferInfo = initVulkanStructure();
    resourceHeapBufferInfo.pNext              = &heapUsageFlags2;
    resourceHeapBufferInfo.size               = resourceHeapSize;
    resourceHeapBufferInfo.usage              = static_cast<VkBufferUsageFlags>(heapUsage);

    Move<VkBuffer> resourceHeapBuffer = createBuffer(vkd, *m_device, &resourceHeapBufferInfo);
    MovePtr<Allocation> resourceHeapMemory =
        m_allocatorPtr->allocate(getBufferMemoryRequirements(vkd, *m_device, *resourceHeapBuffer),
                                 MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress);
    VK_CHECK(vkd.bindBufferMemory(*m_device, *resourceHeapBuffer, resourceHeapMemory->getMemory(),
                                  resourceHeapMemory->getOffset()));

    VkBufferDeviceAddressInfo resourceHeapAddrInfo = initVulkanStructure();
    resourceHeapAddrInfo.buffer                    = *resourceHeapBuffer;
    VkDeviceAddress resourceHeapAddress            = vkd.getBufferDeviceAddress(*m_device, &resourceHeapAddrInfo);

    // Create result buffer
    const VkDeviceSize resultBufferSize = sizeof(uint32_t) * kMaxTestBanks;

    VkBufferUsageFlags2KHR resultUsage =
        VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR;
    VkBufferUsageFlags2CreateInfoKHR resultUsageFlags2 = initVulkanStructure();
    resultUsageFlags2.usage                            = resultUsage;

    VkBufferCreateInfo resultBufferInfo = initVulkanStructure();
    resultBufferInfo.pNext              = &resultUsageFlags2;
    resultBufferInfo.size               = resultBufferSize;
    resultBufferInfo.usage              = static_cast<VkBufferUsageFlags>(resultUsage);

    Move<VkBuffer> resultBuffer = createBuffer(vkd, *m_device, &resultBufferInfo);
    MovePtr<Allocation> resultMemory =
        m_allocatorPtr->allocate(getBufferMemoryRequirements(vkd, *m_device, *resultBuffer),
                                 MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress);
    VK_CHECK(vkd.bindBufferMemory(*m_device, *resultBuffer, resultMemory->getMemory(), resultMemory->getOffset()));

    VkBufferDeviceAddressInfo resultAddrInfo = initVulkanStructure();
    resultAddrInfo.buffer                    = *resultBuffer;
    VkDeviceAddress resultBufferAddress      = vkd.getBufferDeviceAddress(*m_device, &resultAddrInfo);

    // Initialize result buffer
    uint32_t *resultPtr = static_cast<uint32_t *>(resultMemory->getHostPtr());
    for (uint32_t i = 0; i < kMaxTestBanks; ++i)
    {
        resultPtr[i] = ~0u;
    }
    flushAlloc(vkd, *m_device, *resultMemory);

    // Write descriptor to resource heap
    VkDeviceAddressRangeEXT resultBufferAddressRange = {resultBufferAddress, resultBufferSize};
    VkResourceDescriptorInfoEXT resourceDescInfo     = initVulkanStructure();
    resourceDescInfo.type                            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    resourceDescInfo.data.pAddressRange              = &resultBufferAddressRange;

    VkHostAddressRangeEXT resourceHeapHostRange = {resourceHeapMemory->getHostPtr(),
                                                   static_cast<size_t>(bufferDescriptorStride)};
    VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1u, &resourceDescInfo, &resourceHeapHostRange));
    flushAlloc(vkd, *m_device, *resourceHeapMemory);

    // Create compute shader module
    const auto shaderModule = createShaderModule(vkd, *m_device, m_context.getBinaryCollection().get("compute_heap"));

    // Set up descriptor mapping for the SSBO
    VkDescriptorSetAndBindingMappingEXT ssboMapping  = initVulkanStructure();
    ssboMapping.descriptorSet                        = 0;
    ssboMapping.firstBinding                         = 0;
    ssboMapping.bindingCount                         = 1;
    ssboMapping.resourceMask                         = VK_SPIRV_RESOURCE_TYPE_READ_WRITE_STORAGE_BUFFER_BIT_EXT;
    ssboMapping.source                               = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    ssboMapping.sourceData.constantOffset.heapOffset = 0;

    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo = initVulkanStructure();
    mappingInfo.mappingCount                                  = 1;
    mappingInfo.pMappings                                     = &ssboMapping;

    // Create pipeline with descriptor heap flag
    VkPipelineCreateFlags2CreateInfoKHR pipelineFlags2 = initVulkanStructure();
    pipelineFlags2.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    VkPipelineShaderStageCreateInfo shaderStageInfo = initVulkanStructure();
    shaderStageInfo.pNext                           = &mappingInfo;
    shaderStageInfo.stage                           = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module                          = *shaderModule;
    shaderStageInfo.pName                           = "main";

    VkComputePipelineCreateInfo computePipelineInfo = initVulkanStructure();
    computePipelineInfo.pNext                       = &pipelineFlags2;
    computePipelineInfo.stage                       = shaderStageInfo;
    // No layout needed for descriptor heap pipelines

    Move<VkPipeline> pipeline = createComputePipeline(vkd, *m_device, VK_NULL_HANDLE, &computePipelineInfo);

    // Create command pool and buffer
    const auto cmdPool   = makeCommandPool(vkd, *m_device, m_queueFamilyIndex);
    const auto cmdBuffer = allocateCommandBuffer(vkd, *m_device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    // Record command buffer
    beginCommandBuffer(vkd, *cmdBuffer);

    // Memory barrier for heap buffer
    VkBufferMemoryBarrier2 heapBarrier = initVulkanStructure();
    heapBarrier.srcStageMask           = VK_PIPELINE_STAGE_2_HOST_BIT;
    heapBarrier.srcAccessMask          = VK_ACCESS_2_HOST_WRITE_BIT;
    heapBarrier.dstStageMask           = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    heapBarrier.dstAccessMask          = VK_ACCESS_2_RESOURCE_HEAP_READ_BIT_EXT;
    heapBarrier.buffer                 = *resourceHeapBuffer;
    heapBarrier.offset                 = 0;
    heapBarrier.size                   = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 resultBarrier = initVulkanStructure();
    resultBarrier.srcStageMask           = VK_PIPELINE_STAGE_2_HOST_BIT;
    resultBarrier.srcAccessMask          = VK_ACCESS_2_HOST_WRITE_BIT;
    resultBarrier.dstStageMask           = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    resultBarrier.dstAccessMask          = VK_ACCESS_2_SHADER_WRITE_BIT;
    resultBarrier.buffer                 = *resultBuffer;
    resultBarrier.offset                 = 0;
    resultBarrier.size                   = VK_WHOLE_SIZE;

    std::array<VkBufferMemoryBarrier2, 2> barriers = {heapBarrier, resultBarrier};
    VkDependencyInfo depInfo                       = initVulkanStructure();
    depInfo.bufferMemoryBarrierCount               = static_cast<uint32_t>(barriers.size());
    depInfo.pBufferMemoryBarriers                  = barriers.data();
    vkd.cmdPipelineBarrier2(*cmdBuffer, &depInfo);

    vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);

    // Bind resource heap
    VkBindHeapInfoEXT bindHeapInfo   = initVulkanStructure();
    bindHeapInfo.heapRange.address   = resourceHeapAddress;
    bindHeapInfo.heapRange.size      = resourceHeapSize;
    bindHeapInfo.reservedRangeOffset = resourceHeapReservedOffset;
    bindHeapInfo.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;
    vkd.cmdBindResourceHeapEXT(*cmdBuffer, &bindHeapInfo);

    pushDataForBanks(vkd, *cmdBuffer, numBanks);

    vkd.cmdDispatch(*cmdBuffer, 1u, 1u, 1u);

    VkBufferMemoryBarrier resultMemBarrier = initVulkanStructure();
    resultMemBarrier.srcAccessMask         = VK_ACCESS_SHADER_WRITE_BIT;
    resultMemBarrier.dstAccessMask         = VK_ACCESS_HOST_READ_BIT;
    resultMemBarrier.buffer                = *resultBuffer;
    resultMemBarrier.offset                = 0;
    resultMemBarrier.size                  = VK_WHOLE_SIZE;
    vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u,
                           nullptr, 1u, &resultMemBarrier, 0u, nullptr);
    endCommandBuffer(vkd, *cmdBuffer);

    submitCommandsAndWait(vkd, *m_device, m_queue, *cmdBuffer);

    return verifyResults(m_context.getTestContext(), vkd, *m_device, *resultMemory, resultPtr, numBanks);
}

//
// Descriptor heap graphics test using vkCmdPushDataEXT with VkPushDataInfoEXT and VkPushConstantBankInfoNV
//
tcu::TestStatus PushConstantBankTestInstance::runGraphicsDescriptorHeapTest()
{
    const auto &vkd = *m_deviceInterface;

    // Determine number of banks to test - use push data banks for descriptor heap mode
    const uint32_t maxBanks = m_pushConstantBankProperties.maxGraphicsPushDataBanks;
    const uint32_t numBanks = de::min(m_params.numBanks, de::min(maxBanks, kMaxTestBanks));

    if (numBanks < 1)
    {
        TCU_THROW(NotSupportedError, "No graphics push data banks available");
    }

    // Calculate heap sizes
    const VkDeviceSize bufferDescriptorStride =
        alignUp(m_descriptorHeapProperties.bufferDescriptorSize, m_descriptorHeapProperties.bufferDescriptorAlignment);
    const VkDeviceSize resourceHeapUserSize = bufferDescriptorStride;
    const VkDeviceSize resourceHeapReservedOffset =
        alignUp(resourceHeapUserSize, m_descriptorHeapProperties.resourceHeapAlignment);
    const VkDeviceSize resourceHeapSize =
        resourceHeapReservedOffset + m_descriptorHeapProperties.minResourceHeapReservedRange;

    // Create resource heap buffer
    VkBufferUsageFlags2KHR heapUsage =
        VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR;

    VkBufferUsageFlags2CreateInfoKHR heapUsageFlags2 = initVulkanStructure();
    heapUsageFlags2.usage                            = heapUsage;

    VkBufferCreateInfo resourceHeapBufferInfo = initVulkanStructure();
    resourceHeapBufferInfo.pNext              = &heapUsageFlags2;
    resourceHeapBufferInfo.size               = resourceHeapSize;
    resourceHeapBufferInfo.usage              = static_cast<VkBufferUsageFlags>(heapUsage);

    Move<VkBuffer> resourceHeapBuffer = createBuffer(vkd, *m_device, &resourceHeapBufferInfo);
    MovePtr<Allocation> resourceHeapMemory =
        m_allocatorPtr->allocate(getBufferMemoryRequirements(vkd, *m_device, *resourceHeapBuffer),
                                 MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress);
    VK_CHECK(vkd.bindBufferMemory(*m_device, *resourceHeapBuffer, resourceHeapMemory->getMemory(),
                                  resourceHeapMemory->getOffset()));

    VkBufferDeviceAddressInfo resourceHeapAddrInfo = initVulkanStructure();
    resourceHeapAddrInfo.buffer                    = *resourceHeapBuffer;
    VkDeviceAddress resourceHeapAddress            = vkd.getBufferDeviceAddress(*m_device, &resourceHeapAddrInfo);

    // Create result buffer
    const VkDeviceSize resultBufferSize = sizeof(uint32_t) * kMaxTestBanks;

    VkBufferUsageFlags2KHR resultUsage =
        VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR;
    VkBufferUsageFlags2CreateInfoKHR resultUsageFlags2 = initVulkanStructure();
    resultUsageFlags2.usage                            = resultUsage;

    VkBufferCreateInfo resultBufferInfo = initVulkanStructure();
    resultBufferInfo.pNext              = &resultUsageFlags2;
    resultBufferInfo.size               = resultBufferSize;
    resultBufferInfo.usage              = static_cast<VkBufferUsageFlags>(resultUsage);

    Move<VkBuffer> resultBuffer = createBuffer(vkd, *m_device, &resultBufferInfo);
    MovePtr<Allocation> resultMemory =
        m_allocatorPtr->allocate(getBufferMemoryRequirements(vkd, *m_device, *resultBuffer),
                                 MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress);
    VK_CHECK(vkd.bindBufferMemory(*m_device, *resultBuffer, resultMemory->getMemory(), resultMemory->getOffset()));

    VkBufferDeviceAddressInfo resultAddrInfo = initVulkanStructure();
    resultAddrInfo.buffer                    = *resultBuffer;
    VkDeviceAddress resultBufferAddress      = vkd.getBufferDeviceAddress(*m_device, &resultAddrInfo);

    // Initialize result buffer
    uint32_t *resultPtr = static_cast<uint32_t *>(resultMemory->getHostPtr());
    for (uint32_t i = 0; i < kMaxTestBanks; ++i)
    {
        resultPtr[i] = ~0u;
    }
    flushAlloc(vkd, *m_device, *resultMemory);

    // Write descriptor to resource heap
    VkDeviceAddressRangeEXT resultBufferAddressRange = {resultBufferAddress, resultBufferSize};
    VkResourceDescriptorInfoEXT resourceDescInfo     = initVulkanStructure();
    resourceDescInfo.type                            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    resourceDescInfo.data.pAddressRange              = &resultBufferAddressRange;

    VkHostAddressRangeEXT resourceHeapHostRange = {resourceHeapMemory->getHostPtr(),
                                                   static_cast<size_t>(bufferDescriptorStride)};
    VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1u, &resourceDescInfo, &resourceHeapHostRange));
    flushAlloc(vkd, *m_device, *resourceHeapMemory);

    // Create vertex shader module
    const auto vertexModule = createShaderModule(vkd, *m_device, m_context.getBinaryCollection().get("vertex_heap"));

    // Set up descriptor mapping for the SSBO
    VkDescriptorSetAndBindingMappingEXT ssboMapping  = initVulkanStructure();
    ssboMapping.descriptorSet                        = 0;
    ssboMapping.firstBinding                         = 0;
    ssboMapping.bindingCount                         = 1;
    ssboMapping.resourceMask                         = VK_SPIRV_RESOURCE_TYPE_READ_WRITE_STORAGE_BUFFER_BIT_EXT;
    ssboMapping.source                               = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    ssboMapping.sourceData.constantOffset.heapOffset = 0;

    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo = initVulkanStructure();
    mappingInfo.mappingCount                                  = 1;
    mappingInfo.pMappings                                     = &ssboMapping;

    // Create pipeline with descriptor heap flag
    VkPipelineCreateFlags2CreateInfoKHR pipelineFlags2 = initVulkanStructure();
    pipelineFlags2.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    VkPipelineShaderStageCreateInfo vertexStage = initVulkanStructure();
    vertexStage.pNext                           = &mappingInfo;
    vertexStage.stage                           = VK_SHADER_STAGE_VERTEX_BIT;
    vertexStage.module                          = *vertexModule;
    vertexStage.pName                           = "main";

    VkPipelineVertexInputStateCreateInfo vertexInputState     = initVulkanStructure();
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = initVulkanStructure();
    inputAssemblyState.topology                               = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    VkViewport viewport = makeViewport(1u, 1u);
    VkRect2D scissor    = makeRect2D(1u, 1u);

    VkPipelineViewportStateCreateInfo viewportState = initVulkanStructure();
    viewportState.viewportCount                     = 1;
    viewportState.pViewports                        = &viewport;
    viewportState.scissorCount                      = 1;
    viewportState.pScissors                         = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizationState = initVulkanStructure();
    rasterizationState.rasterizerDiscardEnable                = VK_TRUE;
    rasterizationState.polygonMode                            = VK_POLYGON_MODE_FILL;
    rasterizationState.lineWidth                              = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampleState = initVulkanStructure();
    multisampleState.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendState = initVulkanStructure();

    VkGraphicsPipelineCreateInfo graphicsPipelineInfo = initVulkanStructure();
    graphicsPipelineInfo.pNext                        = &pipelineFlags2;
    graphicsPipelineInfo.stageCount                   = 1;
    graphicsPipelineInfo.pStages                      = &vertexStage;
    graphicsPipelineInfo.pVertexInputState            = &vertexInputState;
    graphicsPipelineInfo.pInputAssemblyState          = &inputAssemblyState;
    graphicsPipelineInfo.pViewportState               = &viewportState;
    graphicsPipelineInfo.pRasterizationState          = &rasterizationState;
    graphicsPipelineInfo.pMultisampleState            = &multisampleState;
    graphicsPipelineInfo.pColorBlendState             = &colorBlendState;
    // No layout needed for descriptor heap pipelines

    Move<VkPipeline> pipeline = createGraphicsPipeline(vkd, *m_device, VK_NULL_HANDLE, &graphicsPipelineInfo);

    // Command buffer
    const auto cmdPool   = makeCommandPool(vkd, *m_device, m_queueFamilyIndex);
    const auto cmdBuffer = allocateCommandBuffer(vkd, *m_device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vkd, *cmdBuffer);

    // Memory barriers
    VkBufferMemoryBarrier2 heapBarrier = initVulkanStructure();
    heapBarrier.srcStageMask           = VK_PIPELINE_STAGE_2_HOST_BIT;
    heapBarrier.srcAccessMask          = VK_ACCESS_2_HOST_WRITE_BIT;
    heapBarrier.dstStageMask           = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    heapBarrier.dstAccessMask          = VK_ACCESS_2_RESOURCE_HEAP_READ_BIT_EXT;
    heapBarrier.buffer                 = *resourceHeapBuffer;
    heapBarrier.offset                 = 0;
    heapBarrier.size                   = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 resultBarrier = initVulkanStructure();
    resultBarrier.srcStageMask           = VK_PIPELINE_STAGE_2_HOST_BIT;
    resultBarrier.srcAccessMask          = VK_ACCESS_2_HOST_WRITE_BIT;
    resultBarrier.dstStageMask           = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    resultBarrier.dstAccessMask          = VK_ACCESS_2_SHADER_WRITE_BIT;
    resultBarrier.buffer                 = *resultBuffer;
    resultBarrier.offset                 = 0;
    resultBarrier.size                   = VK_WHOLE_SIZE;

    std::array<VkBufferMemoryBarrier2, 2> barriers = {heapBarrier, resultBarrier};
    VkDependencyInfo depInfo                       = initVulkanStructure();
    depInfo.bufferMemoryBarrierCount               = static_cast<uint32_t>(barriers.size());
    depInfo.pBufferMemoryBarriers                  = barriers.data();
    vkd.cmdPipelineBarrier2(*cmdBuffer, &depInfo);

    // Use dynamic rendering with no attachments
    VkRenderingInfo renderingInfo = initVulkanStructure();
    renderingInfo.renderArea      = makeRect2D(1u, 1u);
    renderingInfo.layerCount      = 1u;

    vkd.cmdBeginRendering(*cmdBuffer, &renderingInfo);

    vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

    // Bind resource heap
    VkBindHeapInfoEXT bindHeapInfo   = initVulkanStructure();
    bindHeapInfo.heapRange.address   = resourceHeapAddress;
    bindHeapInfo.heapRange.size      = resourceHeapSize;
    bindHeapInfo.reservedRangeOffset = resourceHeapReservedOffset;
    bindHeapInfo.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;
    vkd.cmdBindResourceHeapEXT(*cmdBuffer, &bindHeapInfo);

    pushDataForBanks(vkd, *cmdBuffer, numBanks);

    vkd.cmdDraw(*cmdBuffer, 1u, 1u, 0u, 0u);
    vkd.cmdEndRendering(*cmdBuffer);

    VkBufferMemoryBarrier resultMemBarrier = initVulkanStructure();
    resultMemBarrier.srcAccessMask         = VK_ACCESS_SHADER_WRITE_BIT;
    resultMemBarrier.dstAccessMask         = VK_ACCESS_HOST_READ_BIT;
    resultMemBarrier.buffer                = *resultBuffer;
    resultMemBarrier.offset                = 0;
    resultMemBarrier.size                  = VK_WHOLE_SIZE;
    vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr,
                           1u, &resultMemBarrier, 0u, nullptr);
    endCommandBuffer(vkd, *cmdBuffer);

    submitCommandsAndWait(vkd, *m_device, m_queue, *cmdBuffer);

    return verifyResults(m_context.getTestContext(), vkd, *m_device, *resultMemory, resultPtr, numBanks);
}

//
// Compute test with member_offset qualifier
//
tcu::TestStatus PushConstantBankTestInstance::runComputeMemberOffsetTest()
{
    const auto &vkd = *m_deviceInterface;

    const uint32_t maxBanks     = m_pushConstantBankProperties.maxComputePushConstantBanks;
    const uint32_t numBanks     = de::min(m_params.numBanks, de::min(maxBanks, kMaxTestBanks));
    const uint32_t memberOffset = m_params.memberOffset;

    if (numBanks < 1)
        TCU_THROW(NotSupportedError, "No compute push constant banks available");

    auto resultResPtr = createResultBuffer(vkd, *m_device, *m_allocatorPtr);
    auto &resultRes   = *resultResPtr;
    auto descResPtr   = createDescriptorResources(vkd, *m_device, VK_SHADER_STAGE_COMPUTE_BIT, *resultRes.buffer);
    auto &descRes     = *descResPtr;

    const uint32_t pushConstantSize = memberOffset + sizeof(uint32_t);
    auto pushConstantRanges         = createPushConstantRanges(numBanks, VK_SHADER_STAGE_COMPUTE_BIT, pushConstantSize);
    const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                           nullptr,
                                                           0u,
                                                           1u,
                                                           &*descRes.layout,
                                                           numBanks,
                                                           pushConstantRanges.data()};
    Move<VkPipelineLayout> pipelineLayout               = createPipelineLayout(vkd, *m_device, &pipelineLayoutInfo);

    const auto shaderModule = createShaderModule(vkd, *m_device, m_context.getBinaryCollection().get("compute_offset"));
    const VkPipelineShaderStageCreateInfo shaderStageInfo = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                             nullptr,
                                                             0u,
                                                             VK_SHADER_STAGE_COMPUTE_BIT,
                                                             *shaderModule,
                                                             "main",
                                                             nullptr};
    const VkComputePipelineCreateInfo computePipelineInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                                             nullptr,
                                                             0u,
                                                             shaderStageInfo,
                                                             *pipelineLayout,
                                                             VK_NULL_HANDLE,
                                                             0};
    Move<VkPipeline> pipeline = createComputePipeline(vkd, *m_device, VK_NULL_HANDLE, &computePipelineInfo);

    const auto cmdPool   = makeCommandPool(vkd, *m_device, m_queueFamilyIndex);
    const auto cmdBuffer = allocateCommandBuffer(vkd, *m_device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vkd, *cmdBuffer);
    vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    vkd.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &*descRes.set, 0u,
                              nullptr);

    pushConstantsForBanks(vkd, *cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, numBanks, memberOffset);

    vkd.cmdDispatch(*cmdBuffer, 1u, 1u, 1u);

    const VkMemoryBarrier memoryBarrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT,
                                           VK_ACCESS_HOST_READ_BIT};
    vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                           &memoryBarrier, 0u, nullptr, 0u, nullptr);
    endCommandBuffer(vkd, *cmdBuffer);

    submitCommandsAndWait(vkd, *m_device, m_queue, *cmdBuffer);

    return verifyResults(m_context.getTestContext(), vkd, *m_device, *resultRes.memory, resultRes.ptr, numBanks,
                         memberOffset);
}

//
// Graphics test with member_offset qualifier
//
tcu::TestStatus PushConstantBankTestInstance::runGraphicsMemberOffsetTest()
{
    const auto &vkd = *m_deviceInterface;

    const uint32_t maxBanks     = m_pushConstantBankProperties.maxGraphicsPushConstantBanks;
    const uint32_t numBanks     = de::min(m_params.numBanks, de::min(maxBanks, kMaxTestBanks));
    const uint32_t memberOffset = m_params.memberOffset;

    if (numBanks < 1)
        TCU_THROW(NotSupportedError, "No graphics push constant banks available");

    auto resultResPtr = createResultBuffer(vkd, *m_device, *m_allocatorPtr);
    auto &resultRes   = *resultResPtr;
    auto descResPtr   = createDescriptorResources(vkd, *m_device, VK_SHADER_STAGE_VERTEX_BIT, *resultRes.buffer);
    auto &descRes     = *descResPtr;

    const uint32_t pushConstantSize = memberOffset + sizeof(uint32_t);
    auto pushConstantRanges         = createPushConstantRanges(numBanks, VK_SHADER_STAGE_VERTEX_BIT, pushConstantSize);
    const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                           nullptr,
                                                           0u,
                                                           1u,
                                                           &*descRes.layout,
                                                           numBanks,
                                                           pushConstantRanges.data()};
    Move<VkPipelineLayout> pipelineLayout               = createPipelineLayout(vkd, *m_device, &pipelineLayoutInfo);

    const auto vertexModule = createShaderModule(vkd, *m_device, m_context.getBinaryCollection().get("vertex_offset"));
    Move<VkPipeline> pipeline = createGraphicsPipelineWithDiscard(vkd, *m_device, *vertexModule, *pipelineLayout);

    const auto cmdPool   = makeCommandPool(vkd, *m_device, m_queueFamilyIndex);
    const auto cmdBuffer = allocateCommandBuffer(vkd, *m_device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vkd, *cmdBuffer);

    VkRenderingInfo renderingInfo = initVulkanStructure();
    renderingInfo.renderArea      = makeRect2D(1u, 1u);
    renderingInfo.layerCount      = 1u;
    vkd.cmdBeginRendering(*cmdBuffer, &renderingInfo);

    vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
    vkd.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &*descRes.set, 0u,
                              nullptr);

    pushConstantsForBanks(vkd, *cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, numBanks, memberOffset);

    vkd.cmdDraw(*cmdBuffer, 1u, 1u, 0u, 0u);
    vkd.cmdEndRendering(*cmdBuffer);

    const VkMemoryBarrier memoryBarrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT,
                                           VK_ACCESS_HOST_READ_BIT};
    vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                           &memoryBarrier, 0u, nullptr, 0u, nullptr);
    endCommandBuffer(vkd, *cmdBuffer);

    submitCommandsAndWait(vkd, *m_device, m_queue, *cmdBuffer);

    return verifyResults(m_context.getTestContext(), vkd, *m_device, *resultRes.memory, resultRes.ptr, numBanks,
                         memberOffset);
}

class PushConstantBankTestCase : public TestCase
{
public:
    PushConstantBankTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : TestCase(testCtx, name)
        , m_params(params)
    {
    }

    void checkSupport(Context &context) const override
    {
        context.requireDeviceFunctionality(VK_KHR_MAINTENANCE_5_EXTENSION_NAME);
        context.requireDeviceFunctionality(VK_NV_PUSH_CONSTANT_BANK_EXTENSION_NAME);

        // Check feature support
        VkPhysicalDevicePushConstantBankFeaturesNV pushConstantBankFeatures = {};
        pushConstantBankFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_CONSTANT_BANK_FEATURES_NV;
        pushConstantBankFeatures.pNext = nullptr;

        VkPhysicalDeviceFeatures2 features2 = initVulkanStructure();
        features2.pNext                     = &pushConstantBankFeatures;
        context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

        if (!pushConstantBankFeatures.pushConstantBank)
        {
            TCU_THROW(NotSupportedError, "pushConstantBank feature is not supported");
        }

        // Check properties
        VkPhysicalDevicePushConstantBankPropertiesNV pushConstantBankProperties = {};
        pushConstantBankProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_CONSTANT_BANK_PROPERTIES_NV;
        pushConstantBankProperties.pNext = nullptr;

        VkPhysicalDeviceProperties2 properties2 = initVulkanStructure();
        properties2.pNext                       = &pushConstantBankProperties;
        context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties2);

        const bool useDescriptorHeap = usesDescriptorHeap(m_params.testType);
        const bool isCompute         = isComputeTest(m_params.testType);

        uint32_t maxBanks;
        if (useDescriptorHeap)
        {
            maxBanks = isCompute ? pushConstantBankProperties.maxComputePushDataBanks :
                                   pushConstantBankProperties.maxGraphicsPushDataBanks;
        }
        else
        {
            maxBanks = isCompute ? pushConstantBankProperties.maxComputePushConstantBanks :
                                   pushConstantBankProperties.maxGraphicsPushConstantBanks;
        }

        if (maxBanks < 1)
        {
            TCU_THROW(NotSupportedError, "No push constant banks available");
        }

        if (useDescriptorHeap)
        {
            context.requireDeviceFunctionality(VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME);
            context.requireDeviceFunctionality(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
            context.requireDeviceFunctionality(VK_KHR_SHADER_UNTYPED_POINTERS_EXTENSION_NAME);
            context.requireDeviceFunctionality(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
        }

        // Graphics tests need vertexPipelineStoresAndAtomics
        if (!isCompute)
        {
            context.requireDeviceCoreFeature(DeviceCoreFeature::DEVICE_CORE_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS);
        }
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new PushConstantBankTestInstance(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

private:
    TestParams m_params;
};

void PushConstantBankTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
    const bool isCompute         = isComputeTest(m_params.testType);
    const bool useDescriptorHeap = usesDescriptorHeap(m_params.testType);
    const bool useMemberOffset   = usesMemberOffset(m_params.testType);

    // Generate shader source with push constant banks
    std::ostringstream src;
    src << "#version 460\n";
    src << "#extension GL_NV_push_constant_bank : require\n\n";

    if (isCompute)
    {
        src << "layout(local_size_x = 1) in;\n\n";
    }

    src << "layout(std430, binding = 0) writeonly buffer ResultData {\n";
    src << "    uint bank[" << kMaxTestBanks << "];\n";
    src << "} resultData;\n\n";

    // Declare push constant blocks for each bank
    for (uint32_t bank = 0; bank < kMaxTestBanks; ++bank)
    {
        if (useMemberOffset)
        {
            // Use member_offset qualifier to read data at specified offset
            src << "layout(push_constant, bank = " << bank << ", member_offset = " << m_params.memberOffset
                << ") uniform PushConstantBank" << bank << " {\n";
        }
        else
        {
            src << "layout(push_constant, bank = " << bank << ") uniform PushConstantBank" << bank << " {\n";
        }
        src << "    uint data;\n";
        src << "} bank" << bank << ";\n\n";
    }

    src << "void main() {\n";
    for (uint32_t bank = 0; bank < kMaxTestBanks; ++bank)
    {
        src << "    resultData.bank[" << bank << "] = bank" << bank << ".data;\n";
    }

    if (!isCompute)
    {
        src << "    gl_Position = vec4(0, 0, 0, 1);\n";
    }

    src << "}\n";

    // Add shaders with appropriate names based on test type
    if (isCompute)
    {
        if (useDescriptorHeap)
        {
            programCollection.glslSources.add("compute_heap") << glu::ComputeSource(src.str());
        }
        else if (useMemberOffset)
        {
            programCollection.glslSources.add("compute_offset") << glu::ComputeSource(src.str());
        }
        else
        {
            programCollection.glslSources.add("compute") << glu::ComputeSource(src.str());
        }
    }
    else
    {
        if (useDescriptorHeap)
        {
            programCollection.glslSources.add("vertex_heap") << glu::VertexSource(src.str());
        }
        else if (useMemberOffset)
        {
            programCollection.glslSources.add("vertex_offset") << glu::VertexSource(src.str());
        }
        else
        {
            programCollection.glslSources.add("vertex") << glu::VertexSource(src.str());
        }
    }
}

void populateBasicTests(tcu::TestCaseGroup *group)
{
    tcu::TestContext &testCtx = group->getTestContext();

    // Compute tests with different bank counts (non-descriptor heap)
    for (uint32_t numBanks : {1u, 2u, 4u, 8u})
    {
        std::string testName = "compute_" + std::to_string(numBanks) + "_banks";
        TestParams params{};
        params.testType     = TestType::COMPUTE_BASIC;
        params.numBanks     = numBanks;
        params.memberOffset = 0;

        group->addChild(new PushConstantBankTestCase(testCtx, testName, params));
    }

    // Graphics tests with different bank counts (non-descriptor heap)
    for (uint32_t numBanks : {1u, 2u, 4u, 8u})
    {
        std::string testName = "graphics_" + std::to_string(numBanks) + "_banks";
        TestParams params{};
        params.testType     = TestType::GRAPHICS_BASIC;
        params.numBanks     = numBanks;
        params.memberOffset = 0;

        group->addChild(new PushConstantBankTestCase(testCtx, testName, params));
    }

    // Compute tests with member_offset
    for (uint32_t memberOffset : {4u, 16u})
    {
        std::string testName = "compute_member_offset_" + std::to_string(memberOffset);
        TestParams params{};
        params.testType     = TestType::COMPUTE_MEMBER_OFFSET;
        params.numBanks     = 4u;
        params.memberOffset = memberOffset;

        group->addChild(new PushConstantBankTestCase(testCtx, testName, params));
    }

    // Graphics tests with member_offset
    for (uint32_t memberOffset : {4u, 16u})
    {
        std::string testName = "graphics_member_offset_" + std::to_string(memberOffset);
        TestParams params{};
        params.testType     = TestType::GRAPHICS_MEMBER_OFFSET;
        params.numBanks     = 4u;
        params.memberOffset = memberOffset;

        group->addChild(new PushConstantBankTestCase(testCtx, testName, params));
    }
}

void populateDescriptorHeapTests(tcu::TestCaseGroup *group)
{
    tcu::TestContext &testCtx = group->getTestContext();

    // Compute tests with descriptor heap
    for (uint32_t numBanks : {1u, 4u, 8u})
    {
        std::string testName = "compute_" + std::to_string(numBanks) + "_banks";
        TestParams params{};
        params.testType     = TestType::COMPUTE_WITH_DESCRIPTOR_HEAP;
        params.numBanks     = numBanks;
        params.memberOffset = 0;

        group->addChild(new PushConstantBankTestCase(testCtx, testName, params));
    }

    // Graphics tests with descriptor heap
    for (uint32_t numBanks : {1u, 4u, 8u})
    {
        std::string testName = "graphics_" + std::to_string(numBanks) + "_banks";
        TestParams params{};
        params.testType     = TestType::GRAPHICS_WITH_DESCRIPTOR_HEAP;
        params.numBanks     = numBanks;
        params.memberOffset = 0;

        group->addChild(new PushConstantBankTestCase(testCtx, testName, params));
    }
}

} // anonymous namespace

tcu::TestCaseGroup *createPushConstantBankTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "push_constant_bank"));

    // Basic tests without descriptor heap (using vkCmdPushConstants2 + VkPushConstantBankInfoNV)
    de::MovePtr<tcu::TestCaseGroup> basicGroup(new tcu::TestCaseGroup(testCtx, "basic"));
    populateBasicTests(basicGroup.get());
    group->addChild(basicGroup.release());

    // Tests with descriptor heap integration (using vkCmdPushDataEXT + VkPushConstantBankInfoNV)
    de::MovePtr<tcu::TestCaseGroup> heapGroup(new tcu::TestCaseGroup(testCtx, "descriptor_heap"));
    populateDescriptorHeapTests(heapGroup.get());
    group->addChild(heapGroup.release());

    return group.release();
}

} // namespace BindingModel
} // namespace vkt
