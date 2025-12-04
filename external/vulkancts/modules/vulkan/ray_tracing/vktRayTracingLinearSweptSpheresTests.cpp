/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
 * Copyright (c) 2024 NVIDIA Corporation.
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
 *
 *//*!
* \file
* \brief Ray Tracing Linear Swept Spheres tests.
*//*--------------------------------------------------------------------*/

#include "vktRayTracingAccelerationStructuresTests.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vkDefs.hpp"
#include "deClock.h"
#include "deRandom.h"
#include "vkCmdUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkRayTracingUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTexture.hpp"
#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "tcuFloat.hpp"
#include "deModularCounter.hpp"

#include <cmath>
#include <cstddef>
#include <set>
#include <limits>
#include <iostream>

namespace vkt
{
namespace RayTracing
{
namespace
{

using namespace vk;
using namespace vkt;
using namespace tcu;

static const VkFlags ALL_RAY_TRACING_STAGES = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                                              VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
                                              VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR;

enum class GeometryType
{
    SPHERES = 0,
    LSS     = 1,
};

enum class TestType
{
    VERTICES,
    INDICES,
    INDEXING_MODE_LIST,
    INDEXING_MODE_SUCCESSIVE
};

enum class VertexFormat
{
    FLOAT3 = 0,
    FLOAT2 = 1,
    HALF3  = 2,
    HALF2  = 3,
};

enum class RadiusFormat
{
    R32 = 0,
    R16 = 1,
};

struct TestParams;

struct DeviceHelper
{
    Move<VkDevice> device;
    de::MovePtr<DeviceDriver> vkd;
    uint32_t queueFamilyIndex;
    VkQueue queue;
    de::MovePtr<SimpleAllocator> allocator;

    DeviceHelper(Context &context)
    {
        const auto &vkp           = context.getPlatformInterface();
        const auto &vki           = context.getInstanceInterface();
        const auto instance       = context.getInstance();
        const auto physicalDevice = context.getPhysicalDevice();

        queueFamilyIndex = context.getUniversalQueueFamilyIndex();

        // Get device features (these have to be checked in the test case)
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures = initVulkanStructure();
        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures =
            initVulkanStructure(&rayTracingPipelineFeatures);
        VkPhysicalDeviceRayTracingLinearSweptSpheresFeaturesNV rayTracingLinearSweptSpheresFeatures =
            initVulkanStructure(&accelerationStructureFeatures);
        VkPhysicalDeviceBufferDeviceAddressFeaturesKHR deviceAddressFeatures =
            initVulkanStructure(&rayTracingLinearSweptSpheresFeatures);
        VkPhysicalDeviceFeatures2 deviceFeatures = initVulkanStructure(&deviceAddressFeatures);

        vki.getPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures);

        // Make sure robust buffer access is disabled as in the default device
        deviceFeatures.features.robustBufferAccess = VK_FALSE;

        const auto queuePriority = 1.0f;
        const VkDeviceQueueCreateInfo queueInfo{
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            0u,                                         // VkDeviceQueueCreateFlags flags;
            queueFamilyIndex,                           // uint32_t queueFamilyIndex;
            1u,                                         // uint32_t queueCount;
            &queuePriority,                             // const float* pQueuePriorities;
        };

        // Required extensions - create device with VK_KHR_ray_tracing_pipeline but
        // without VK_KHR_pipeline_library to also test that that combination works
        std::vector<const char *> requiredExtensions{
            "VK_KHR_ray_tracing_pipeline",     "VK_KHR_acceleration_structure",
            "VK_KHR_deferred_host_operations", "VK_KHR_buffer_device_address",
            "VK_EXT_descriptor_indexing",      "VK_KHR_spirv_1_4",
            "VK_KHR_shader_float_controls",    "VK_NV_ray_tracing_linear_swept_spheres"};

        const VkDeviceCreateInfo createInfo{
            VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,             // VkStructureType sType;
            deviceFeatures.pNext,                             // const void* pNext;
            0u,                                               // VkDeviceCreateFlags flags;
            1u,                                               // uint32_t queueCreateInfoCount;
            &queueInfo,                                       // const VkDeviceQueueCreateInfo* pQueueCreateInfos;
            0u,                                               // uint32_t enabledLayerCount;
            nullptr,                                          // const char* const* ppEnabledLayerNames;
            static_cast<uint32_t>(requiredExtensions.size()), // uint32_t enabledExtensionCount;
            requiredExtensions.data(),                        // const char* const* ppEnabledExtensionNames;
            &deviceFeatures.features,                         // const VkPhysicalDeviceFeatures* pEnabledFeatures;
        };

        // Create custom device and related objects
        device = createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), vkp, instance, vki,
                                    physicalDevice, &createInfo);
        vkd    = de::MovePtr<DeviceDriver>(new DeviceDriver(vkp, instance, device.get(), context.getUsedApiVersion(),
                                                            context.getTestContext().getCommandLine()));
        queue  = getDeviceQueue(*vkd, *device, queueFamilyIndex, 0u);
        allocator = de::MovePtr<SimpleAllocator>(
            new SimpleAllocator(*vkd, device.get(), getPhysicalDeviceMemoryProperties(vki, physicalDevice)));
    }
};

struct TestParams
{
    GeometryType geometryType;
    TestType testType;
    bool doBlasCopy;
    bool useEndcaps;
    bool skipBuiltinPrimitives;
    bool useRayQuery;
    bool useHitObject;
    VertexFormat vertexFormat;
    RadiusFormat radiusFormat;
};

class LinearSweptSpheresTestInstance : public TestInstance
{
public:
    LinearSweptSpheresTestInstance(Context &context, const TestParams &data);
    ~LinearSweptSpheresTestInstance(void) = default;
    tcu::TestStatus iterate(void) override;

private:
    VkWriteDescriptorSetAccelerationStructureKHR makeASWriteDescriptorSet(
        const VkAccelerationStructureKHR *pAccelerationStructure);
    void clearBuffer(de::SharedPtr<BufferWithMemory> buffer, VkDeviceSize bufferSize);
    TestParams m_TestParams;

protected:
    DeviceHelper m_customDevice;
    Move<VkDescriptorSetLayout> m_descriptorSetLayout;
    de::MovePtr<RayTracingPipeline> m_rayTracingPipeline;
    std::vector<de::SharedPtr<BottomLevelAccelerationStructure>> m_blasVect;
    de::SharedPtr<TopLevelAccelerationStructure> m_referenceTLAS;
    de::SharedPtr<TopLevelAccelerationStructure> m_resultTLAS;
    de::MovePtr<BufferWithMemory> m_rgenShaderBT;
    de::MovePtr<BufferWithMemory> m_chitShaderBT;
    de::MovePtr<BufferWithMemory> m_missShaderBT;
    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBuffer;
    Move<VkPipelineLayout> m_pipelineLayout;
    Move<VkPipeline> m_pipeline;
    virtual void setupRayTracingPipeline()     = 0;
    virtual void setupAccelerationStructures() = 0;
};

VkWriteDescriptorSetAccelerationStructureKHR LinearSweptSpheresTestInstance::makeASWriteDescriptorSet(
    const VkAccelerationStructureKHR *pAccelerationStructure)
{
    return {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR, // VkStructureType                        sType
        nullptr,               // const void*                            pNext
        1u,                    // uint32_t                                accelerationStructureCount
        pAccelerationStructure // const VkAccelerationStructureKHR*    pAccelerationStructures
    };
}

LinearSweptSpheresTestInstance::LinearSweptSpheresTestInstance(Context &context, const TestParams &data)
    : TestInstance(context)
    , m_TestParams(data)
    , m_customDevice(context)
    , m_referenceTLAS(makeTopLevelAccelerationStructure().release())
    , m_resultTLAS(makeTopLevelAccelerationStructure().release())
{
}

void LinearSweptSpheresTestInstance::clearBuffer(de::SharedPtr<BufferWithMemory> buffer, VkDeviceSize bufferSize)
{
    const DeviceInterface &vkd = *m_customDevice.vkd;
    const VkDevice device      = *m_customDevice.device;
    auto &bufferAlloc          = buffer->getAllocation();
    void *bufferPtr            = bufferAlloc.getHostPtr();

    deMemset(bufferPtr, 1, static_cast<size_t>(bufferSize));
    vk::flushAlloc(vkd, device, bufferAlloc);
}

tcu::TestStatus LinearSweptSpheresTestInstance::iterate(void)
{
    const DeviceInterface &vkd      = *m_customDevice.vkd;
    const VkDevice device           = *m_customDevice.device;
    const uint32_t queueFamilyIndex = m_customDevice.queueFamilyIndex;
    const VkQueue queue             = m_customDevice.queue;
    Allocator &allocator            = *m_customDevice.allocator;
    const uint32_t sgHandleSize     = m_context.getRayTracingPipelineProperties().shaderGroupHandleSize;
    const uint32_t imageSize        = 64u;

    const Move<VkDescriptorPool> descriptorPool =
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 2u)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u)
            .build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 2u);

    m_descriptorSetLayout = DescriptorSetLayoutBuilder()
                                .addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                                                  ALL_RAY_TRACING_STAGES) // as with single/four aabb's
                                .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                  ALL_RAY_TRACING_STAGES) // ssbo with result/reference values
                                .build(vkd, device);

    const Move<VkDescriptorSet> referenceDescriptorSet =
        makeDescriptorSet(vkd, device, *descriptorPool, *m_descriptorSetLayout);
    const Move<VkDescriptorSet> resultDescriptorSet =
        makeDescriptorSet(vkd, device, *descriptorPool, *m_descriptorSetLayout);

    const VkDeviceSize resultBufferSize = imageSize * imageSize * sizeof(int);
    const VkBufferCreateInfo resultBufferCreateInfo =
        makeBufferCreateInfo(resultBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    de::SharedPtr<BufferWithMemory> referenceBuffer = de::SharedPtr<BufferWithMemory>(
        new BufferWithMemory(vkd, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));
    de::SharedPtr<BufferWithMemory> resultBuffer = de::SharedPtr<BufferWithMemory>(
        new BufferWithMemory(vkd, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));

    m_rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

    setupRayTracingPipeline();

    const VkStridedDeviceAddressRegionKHR rgenSBTR = makeStridedDeviceAddressRegionKHR(
        getBufferDeviceAddress(vkd, device, m_rgenShaderBT->get(), 0), sgHandleSize, sgHandleSize);
    const VkStridedDeviceAddressRegionKHR chitSBTR = makeStridedDeviceAddressRegionKHR(
        getBufferDeviceAddress(vkd, device, m_chitShaderBT->get(), 0), sgHandleSize, sgHandleSize);
    const VkStridedDeviceAddressRegionKHR missSBTR = makeStridedDeviceAddressRegionKHR(
        getBufferDeviceAddress(vkd, device, m_missShaderBT->get(), 0), sgHandleSize, sgHandleSize);
    const VkStridedDeviceAddressRegionKHR callableSBTR = makeStridedDeviceAddressRegionKHR(0, 0, 0);

    m_cmdPool   = createCommandPool(vkd, device, 0, queueFamilyIndex);
    m_cmdBuffer = allocateCommandBuffer(vkd, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    // clear result and reference buffers
    clearBuffer(resultBuffer, resultBufferSize);
    clearBuffer(referenceBuffer, resultBufferSize);

    beginCommandBuffer(vkd, *m_cmdBuffer, 0u);

    {
        setupAccelerationStructures();
        {
            typedef DescriptorSetUpdateBuilder::Location DSL;

            const VkWriteDescriptorSetAccelerationStructureKHR referenceAS =
                makeASWriteDescriptorSet(m_referenceTLAS->getPtr());
            const VkDescriptorBufferInfo referenceSSBO = makeDescriptorBufferInfo(**referenceBuffer, 0u, VK_WHOLE_SIZE);
            DescriptorSetUpdateBuilder()
                .writeSingle(*referenceDescriptorSet, DSL::binding(0u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                             &referenceAS)
                .writeSingle(*referenceDescriptorSet, DSL::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                             &referenceSSBO)
                .update(vkd, device);
        }

        // wait for data transfers
        const VkMemoryBarrier bufferUploadBarrier =
            makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        cmdPipelineMemoryBarrier(vkd, *m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, &bufferUploadBarrier, 1u);

        // wait for as build
        const VkMemoryBarrier asBuildBarrier = makeMemoryBarrier(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                                                                 VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR);
        cmdPipelineMemoryBarrier(vkd, *m_cmdBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                 VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, &asBuildBarrier, 1u);

        vkd.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *m_pipeline);

        // generate reference
        vkd.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *m_pipelineLayout, 0, 1,
                                  &referenceDescriptorSet.get(), 0, nullptr);
        cmdTraceRays(vkd, *m_cmdBuffer, &rgenSBTR, &missSBTR, &chitSBTR, &callableSBTR, imageSize, imageSize, 1);

        const VkMemoryBarrier postTraceMemoryBarrier =
            makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        cmdPipelineMemoryBarrier(vkd, *m_cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, &postTraceMemoryBarrier);
    }

    endCommandBuffer(vkd, *m_cmdBuffer);

    submitCommandsAndWait(vkd, device, queue, m_cmdBuffer.get());

    // verify result buffer
    auto referenceAllocation = referenceBuffer->getAllocation();
    invalidateMappedMemoryRange(vkd, device, referenceAllocation.getMemory(), referenceAllocation.getOffset(),
                                resultBufferSize);

    tcu::TextureFormat imageFormat(vk::mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM));
    tcu::PixelBufferAccess referenceAccess(imageFormat, imageSize, imageSize, 1, referenceAllocation.getHostPtr());

    const int width  = referenceAccess.getWidth();
    const int height = referenceAccess.getHeight();
    const int depth  = referenceAccess.getDepth();

    // Verify each pixel in the reference image
    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                const IVec4 refPix = referenceAccess.getPixelInt(x, y, z);

                // Handle sphere geometry type
                if (m_TestParams.geometryType == GeometryType::SPHERES)
                {
                    // SPHERES geometry should not have no_endcaps case
                    if (!m_TestParams.useEndcaps)
                    {
                        tcu::print("Wrong configuration for SPHERES geometry. Endcaps should be enabled.");
                        return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED,
                                               "SPHERES geometry should not have no_endcaps case");
                    }

                    // For SPHERES geometry:
                    // - If testType is VERTICES, we shoot rays at all 12 sphere vertices, so expect 12 hits.
                    // - If testType is INDICES, we shoot rays only at the 8 indexed vertices, so expect 8 hits.
                    if (m_TestParams.testType != TestType::VERTICES && m_TestParams.testType != TestType::INDICES)
                    {
                        tcu::print("Wrong test type for SPHERES geometry. Expected VERTICES or INDICES.");
                        return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED, "Invalid test type for spheres geometry");
                    }

                    const int expectedValue = (m_TestParams.testType == TestType::VERTICES) ? 12 : 8;
                    if (refPix[0] != expectedValue)
                    {
                        tcu::print("Found value: %d, Expected: %d", refPix[0], expectedValue);
                        return tcu::TestStatus::fail("Unexpected value for spheres geometry");
                    }
                    continue;
                }

                // Handle linear swept spheres geometry type
                if (m_TestParams.geometryType == GeometryType::LSS)
                {
                    // Handle case without endcaps
                    if (!m_TestParams.useEndcaps)
                    {
                        // For LSS (Linear Swept Spheres) geometry without endcaps:
                        // - If testType is INDEXING_MODE_SUCCESSIVE, we shoot rays at 3 segments (or vertices), so expect 3 hits.
                        // - Otherwise (e.g., INDEXING_MODE_LIST), we shoot rays at one segment (or vertices), so expect 1 hit.
                        const int expectedValue = (m_TestParams.testType == TestType::INDEXING_MODE_SUCCESSIVE) ? 3 : 1;
                        if (refPix[0] != expectedValue)
                        {
                            tcu::print("Found value: %d, Expected: %d", refPix[0], expectedValue);
                            return tcu::TestStatus::fail("Unexpected value for LSS without endcaps");
                        }
                        continue;
                    }

                    // Handle case with endcaps

                    // For LSS (Linear Swept Spheres) geometry with endcaps enabled:
                    // - If testType is VERTICES, we shoot rays at all 12 LSS vertices (including endcaps), so expect 12 hits.
                    // - If testType is INDEXING_MODE_LIST, we shoot rays at 6 segments (including endcaps), so expect 6 hits.
                    // - If testType is INDEXING_MODE_SUCCESSIVE, we shoot rays at 10 segments (including endcaps), so expect 10 hits.
                    int expectedValue;
                    if (m_TestParams.testType == TestType::VERTICES)
                    {
                        expectedValue = 12;
                    }
                    else if (m_TestParams.testType == TestType::INDEXING_MODE_LIST)
                    {
                        expectedValue = 6;
                    }
                    else if (m_TestParams.testType == TestType::INDEXING_MODE_SUCCESSIVE)
                    {
                        expectedValue = 10;
                    }
                    else
                    {
                        return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED, "Invalid test type for LSS with endcaps");
                    }

                    if (refPix[0] != expectedValue)
                    {
                        tcu::print("Found value: %d, Expected: %d", refPix[0], expectedValue);
                        return tcu::TestStatus::fail("Unexpected value for LSS with endcaps");
                    }
                }
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class SpheresTestInstance : public LinearSweptSpheresTestInstance
{
public:
    SpheresTestInstance(Context &context, const TestParams &data);

    void setupRayTracingPipeline() override;
    void setupAccelerationStructures() override;

protected:
    TestParams m_data;
};

SpheresTestInstance::SpheresTestInstance(Context &context, const TestParams &data)
    : LinearSweptSpheresTestInstance(context, data)
{
    m_data = data;
}

void SpheresTestInstance::setupRayTracingPipeline()
{
    const DeviceInterface &vkd     = *m_customDevice.vkd;
    const VkDevice device          = *m_customDevice.device;
    Allocator &allocator           = *m_customDevice.allocator;
    vk::BinaryCollection &bc       = m_context.getBinaryCollection();
    const uint32_t sgHandleSize    = m_context.getRayTracingPipelineProperties().shaderGroupHandleSize;
    const uint32_t sgBaseAlignment = m_context.getRayTracingPipelineProperties().shaderGroupBaseAlignment;
    m_rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, createShaderModule(vkd, device, bc.get("rgen"), 0),
                                    0);
    m_rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                                    createShaderModule(vkd, device, bc.get("chit"), 0), 1);
    m_rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, createShaderModule(vkd, device, bc.get("miss"), 0),
                                    2);
    m_pipelineLayout = makePipelineLayout(vkd, device, m_descriptorSetLayout.get());
    m_rayTracingPipeline->setCreateFlags2(
        VK_PIPELINE_CREATE_2_RAY_TRACING_ALLOW_SPHERES_AND_LINEAR_SWEPT_SPHERES_BIT_NV);
    if (m_data.skipBuiltinPrimitives)
    {
        m_rayTracingPipeline->setCreateFlags2(
            VK_PIPELINE_CREATE_2_RAY_TRACING_SKIP_BUILT_IN_PRIMITIVES_BIT_KHR |
            VK_PIPELINE_CREATE_2_RAY_TRACING_ALLOW_SPHERES_AND_LINEAR_SWEPT_SPHERES_BIT_NV);
    }
    m_pipeline     = m_rayTracingPipeline->createPipeline(vkd, device, *m_pipelineLayout);
    m_rgenShaderBT = m_rayTracingPipeline->createShaderBindingTable(vkd, device, *m_pipeline, allocator, sgHandleSize,
                                                                    sgBaseAlignment, 0, 1);
    m_chitShaderBT = m_rayTracingPipeline->createShaderBindingTable(vkd, device, *m_pipeline, allocator, sgHandleSize,
                                                                    sgBaseAlignment, 1, 1);
    m_missShaderBT = m_rayTracingPipeline->createShaderBindingTable(vkd, device, *m_pipeline, allocator, sgHandleSize,
                                                                    sgBaseAlignment, 2, 1);
}

void SpheresTestInstance::setupAccelerationStructures()
{
    const DeviceInterface &vkd = *m_customDevice.vkd;
    const VkDevice device      = *m_customDevice.device;
    Allocator &allocator       = *m_customDevice.allocator;
    AccelerationStructBufferProperties bufferProps;
    bufferProps.props.residency = ResourceResidency::TRADITIONAL;
    VkFormat vertexFormat       = VK_FORMAT_UNDEFINED;
    VkFormat radiusFormat       = VK_FORMAT_UNDEFINED;

    if (m_data.vertexFormat == VertexFormat::FLOAT3)
    {
        vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    }
    else if (m_data.vertexFormat == VertexFormat::FLOAT2)
    {
        vertexFormat = VK_FORMAT_R32G32_SFLOAT;
    }
    else if (m_data.vertexFormat == VertexFormat::HALF3)
    {
        vertexFormat = VK_FORMAT_R16G16B16_SFLOAT;
    }
    else if (m_data.vertexFormat == VertexFormat::HALF2)
    {
        vertexFormat = VK_FORMAT_R16G16_SFLOAT;
    }

    if (m_data.radiusFormat == RadiusFormat::R32)
    {
        radiusFormat = VK_FORMAT_R32_SFLOAT;
    }
    else if (m_data.radiusFormat == RadiusFormat::R16)
    {
        radiusFormat = VK_FORMAT_R16_SFLOAT;
    }
    de::SharedPtr<BottomLevelAccelerationStructure> sphereBLAS(makeBottomLevelAccelerationStructure().release());
    sphereBLAS->setGeometryCount(1u);

    VkIndexType indexType = VK_INDEX_TYPE_NONE_KHR;
    if (m_data.testType == TestType::INDICES)
    {
        indexType = VK_INDEX_TYPE_UINT32;
    }
    VkRayTracingLssIndexingModeNV indexingMode = VK_RAY_TRACING_LSS_INDEXING_MODE_LIST_NV;
    std::vector<tcu::Vec3> sphereVertexData    = {
        tcu::Vec3(-8, 7, -15),  tcu::Vec3(7, 7, -15),  tcu::Vec3(6, 6, -15),  tcu::Vec3(-7, 5, -15),
        tcu::Vec3(-8, 3, -15),  tcu::Vec3(4, 2, -15),  tcu::Vec3(6, 1, -15),  tcu::Vec3(-9, 1, -15),
        tcu::Vec3(-6, 0, -15),  tcu::Vec3(5, -1, -15), tcu::Vec3(8, -2, -15), tcu::Vec3(-8, -3, -15),
        tcu::Vec3(-6, -5, -15), tcu::Vec3(7, -6, -15), tcu::Vec3(5, -7, -15), tcu::Vec3(-8, -6, -15)};

    std::vector<float> sphereRadiusData   = {0.5f, 0.6f, 0.7f, 0.8f, 0.6f, 0.5f, 0.9f, 0.4f,
                                             0.7f, 0.6f, 0.9f, 0.5f, 0.9f, 0.6f, 0.8f, 0.5f};
    std::vector<uint32_t> sphereIndexData = {15, 13, 11, 9, 7, 5, 3, 1};

    sphereBLAS->addSphereGeometry(sphereVertexData, sphereRadiusData, sphereIndexData, false, indexType, indexingMode,
                                  m_data.useEndcaps, m_data.doBlasCopy, vertexFormat, radiusFormat);
    sphereBLAS->createAndBuild(vkd, device, *m_cmdBuffer, allocator, bufferProps);
    m_blasVect.push_back(sphereBLAS);
    m_referenceTLAS->setInstanceCount(1);

    m_referenceTLAS->addInstance(sphereBLAS);
    m_referenceTLAS->createAndBuild(vkd, device, *m_cmdBuffer, allocator, bufferProps);
}

class LSSpheresTestInstance : public LinearSweptSpheresTestInstance
{
public:
    LSSpheresTestInstance(Context &context, const TestParams &data);

    void setupRayTracingPipeline() override;
    void setupAccelerationStructures() override;

protected:
    TestParams m_data;
};

LSSpheresTestInstance::LSSpheresTestInstance(Context &context, const TestParams &data)
    : LinearSweptSpheresTestInstance(context, data)
{
    m_data = data;
}

void LSSpheresTestInstance::setupRayTracingPipeline()
{
    const DeviceInterface &vkd     = *m_customDevice.vkd;
    const VkDevice device          = *m_customDevice.device;
    Allocator &allocator           = *m_customDevice.allocator;
    vk::BinaryCollection &bc       = m_context.getBinaryCollection();
    const uint32_t sgHandleSize    = m_context.getRayTracingPipelineProperties().shaderGroupHandleSize;
    const uint32_t sgBaseAlignment = m_context.getRayTracingPipelineProperties().shaderGroupBaseAlignment;
    m_rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, createShaderModule(vkd, device, bc.get("rgen"), 0),
                                    0);
    m_rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                                    createShaderModule(vkd, device, bc.get("chit"), 0), 1);
    m_rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, createShaderModule(vkd, device, bc.get("miss"), 0),
                                    2);
    m_pipelineLayout = makePipelineLayout(vkd, device, m_descriptorSetLayout.get());
    m_rayTracingPipeline->setCreateFlags2(
        VK_PIPELINE_CREATE_2_RAY_TRACING_ALLOW_SPHERES_AND_LINEAR_SWEPT_SPHERES_BIT_NV);
    m_pipeline     = m_rayTracingPipeline->createPipeline(vkd, device, *m_pipelineLayout);
    m_rgenShaderBT = m_rayTracingPipeline->createShaderBindingTable(vkd, device, *m_pipeline, allocator, sgHandleSize,
                                                                    sgBaseAlignment, 0, 1);
    m_chitShaderBT = m_rayTracingPipeline->createShaderBindingTable(vkd, device, *m_pipeline, allocator, sgHandleSize,
                                                                    sgBaseAlignment, 1, 1);
    m_missShaderBT = m_rayTracingPipeline->createShaderBindingTable(vkd, device, *m_pipeline, allocator, sgHandleSize,
                                                                    sgBaseAlignment, 2, 1);
}

void LSSpheresTestInstance::setupAccelerationStructures()
{
    const DeviceInterface &vkd = *m_customDevice.vkd;
    const VkDevice device      = *m_customDevice.device;
    Allocator &allocator       = *m_customDevice.allocator;
    VkFormat vertexFormat      = VK_FORMAT_UNDEFINED;
    VkFormat radiusFormat      = VK_FORMAT_UNDEFINED;
    if (m_data.vertexFormat == VertexFormat::FLOAT3)
    {
        vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    }
    else if (m_data.vertexFormat == VertexFormat::FLOAT2)
    {
        vertexFormat = VK_FORMAT_R32G32_SFLOAT;
    }
    else if (m_data.vertexFormat == VertexFormat::HALF3)
    {
        vertexFormat = VK_FORMAT_R16G16B16_SFLOAT;
    }
    else if (m_data.vertexFormat == VertexFormat::HALF2)
    {
        vertexFormat = VK_FORMAT_R16G16_SFLOAT;
    }

    if (m_data.radiusFormat == RadiusFormat::R32)
    {
        radiusFormat = VK_FORMAT_R32_SFLOAT;
    }
    else if (m_data.radiusFormat == RadiusFormat::R16)
    {
        radiusFormat = VK_FORMAT_R16_SFLOAT;
    }

    de::SharedPtr<BottomLevelAccelerationStructure> LSSphereBLAS(makeBottomLevelAccelerationStructure().release());
    LSSphereBLAS->setGeometryCount(1u);
    AccelerationStructBufferProperties bufferProps;
    bufferProps.props.residency                = ResourceResidency::TRADITIONAL;
    VkIndexType indexType                      = VK_INDEX_TYPE_NONE_KHR;
    VkRayTracingLssIndexingModeNV indexingMode = VK_RAY_TRACING_LSS_INDEXING_MODE_LIST_NV;

    if (m_data.testType == TestType::INDEXING_MODE_SUCCESSIVE)
    {
        indexingMode = VK_RAY_TRACING_LSS_INDEXING_MODE_SUCCESSIVE_NV;
        indexType    = VK_INDEX_TYPE_UINT32;
    }
    else if (m_data.testType == TestType::INDEXING_MODE_LIST)
    {
        indexingMode = VK_RAY_TRACING_LSS_INDEXING_MODE_LIST_NV;
        indexType    = VK_INDEX_TYPE_UINT32;
    }

    std::vector<tcu::Vec3> lssSphereVertexData = {
        tcu::Vec3(-8, 7, -15),  tcu::Vec3(8, 7, -15),  tcu::Vec3(8, 5, -15),  tcu::Vec3(-8, 5, -15),
        tcu::Vec3(-8, 3, -15),  tcu::Vec3(8, 3, -15),  tcu::Vec3(8, 1, -15),  tcu::Vec3(-8, 1, -15),
        tcu::Vec3(-8, -1, -15), tcu::Vec3(8, -1, -15), tcu::Vec3(8, -3, -15), tcu::Vec3(-8, -3, -15),
        tcu::Vec3(-8, -5, -15), tcu::Vec3(8, -5, -15), tcu::Vec3(8, -7, -15), tcu::Vec3(-8, -7, -15)};

    std::vector<tcu::Vec3> lssSphereVertexDataNoEndcaps = {tcu::Vec3(2, 0, -15), tcu::Vec3(6, 0, -15),
                                                           tcu::Vec3(10, 0, -15)};

    std::vector<float> lssSphereRadiusData = {0.5f, 0.6f, 0.7f, 0.8f, 0.6f, 0.5f, 0.9f, 0.4f,
                                              0.7f, 0.6f, 0.9f, 0.5f, 0.9f, 0.6f, 0.8f, 0.5f};

    std::vector<float> lssSphereRadiusDataNoEndcaps = {2.0f, 2.0f, 2.0f};
    std::vector<uint32_t> lssSphereIndexData;
    if (indexingMode == VK_RAY_TRACING_LSS_INDEXING_MODE_SUCCESSIVE_NV)
    {
        lssSphereIndexData = {0, 1, 2, 3, 4, 8, 9, 10, 11, 12, 13, 14};
    }
    else
    {
        lssSphereIndexData = {0, 2, 2, 4, 4, 6, 8, 10, 10, 12, 12, 14};
    }

    std::vector<uint32_t> lssSphereIndexDNoEndcaps;

    if (indexingMode == VK_RAY_TRACING_LSS_INDEXING_MODE_SUCCESSIVE_NV)
    {
        lssSphereIndexDNoEndcaps = {0, 1};
    }
    else
    {
        lssSphereIndexDNoEndcaps = {0, 1};
    }

    if (m_data.useEndcaps)
    {
        LSSphereBLAS->addSphereGeometry(lssSphereVertexData, lssSphereRadiusData, lssSphereIndexData, true, indexType,
                                        indexingMode, true, m_data.doBlasCopy, vertexFormat, radiusFormat);
    }
    else
    {
        LSSphereBLAS->addSphereGeometry(lssSphereVertexDataNoEndcaps, lssSphereRadiusDataNoEndcaps,
                                        lssSphereIndexDNoEndcaps, true, indexType, indexingMode, false,
                                        m_data.doBlasCopy, vertexFormat, radiusFormat);
    }

    LSSphereBLAS->createAndBuild(vkd, device, *m_cmdBuffer, allocator, bufferProps);
    m_blasVect.push_back(LSSphereBLAS);
    m_referenceTLAS->setInstanceCount(1);

    m_referenceTLAS->addInstance(LSSphereBLAS);
    m_referenceTLAS->createAndBuild(vkd, device, *m_cmdBuffer, allocator, bufferProps);
}

class LinearSweptSpheresTestCase : public TestCase
{
public:
    LinearSweptSpheresTestCase(tcu::TestContext &context, const char *name, const TestParams &data)
        : vkt::TestCase(context, name)
        , m_data(data){};

    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override;
    void initPrograms(SourceCollections &programCollection) const override;

protected:
    TestParams m_data;
};

TestInstance *LinearSweptSpheresTestCase::createInstance(Context &context) const
{
    if (m_data.geometryType == GeometryType::SPHERES)
    {
        return new SpheresTestInstance(context, m_data);
    }
    else if (m_data.geometryType == GeometryType::LSS)
    {
        return new LSSpheresTestInstance(context, m_data);
    }
    return new LSSpheresTestInstance(context, m_data);
}

void LinearSweptSpheresTestCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
    context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");
    context.requireDeviceFunctionality("VK_NV_ray_tracing_linear_swept_spheres");

    const VkPhysicalDeviceRayTracingPipelineFeaturesKHR &rayTracingPipelineFeaturesKHR =
        context.getRayTracingPipelineFeatures();
    if (!rayTracingPipelineFeaturesKHR.rayTracingPipeline)
        TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");

    const VkPhysicalDeviceAccelerationStructureFeaturesKHR &accelerationStructureFeaturesKHR =
        context.getAccelerationStructureFeatures();
    if (!accelerationStructureFeaturesKHR.accelerationStructure)
        TCU_THROW(TestError, "VK_KHR_ray_tracing_pipeline requires "
                             "VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");
    const VkPhysicalDeviceRayTracingLinearSweptSpheresFeaturesNV &linearSweptSpheresFeaturesNV =
        context.getRayTracingLinearSweptSpheresFeaturesNV();
    if (!linearSweptSpheresFeaturesNV.linearSweptSpheres)
        TCU_THROW(TestError, "VK_NV_ray_tracing_linear_swept_spheres requires "
                             "VkPhysicalDeviceRayTracingLinearSweptSpheresFeaturesNV.linearSweptSpheres");
}

void LinearSweptSpheresTestCase::initPrograms(SourceCollections &programCollection) const
{
    const vk::ShaderBuildOptions glslBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

    // Create shader modules
    std::string raygenSource =
        "#version 460                                                                             \n"
        "#extension GL_EXT_ray_tracing : enable                                                   \n"
        "#extension GL_EXT_ray_query : enable                                                     \n"
        "#extension GL_NV_shader_invocation_reorder : enable                                      \n"
        "#extension GL_NV_linear_swept_spheres : enable                                           \n"
        "layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;                \n"
        "layout(set = 0, binding = 1, std430) writeonly buffer Result {\n"
        "    int value[];\n"
        "} result;\n"
        "layout(location = 0) rayPayloadEXT int hitValue;                                        \n"
        "                                                                                         \n"
        "void main()                                                                              \n"
        "{                                                                                        \n"

        "                                                                                         \n"
        "    float tmin = 0.001;                                                                  \n"
        "    float tmax = 1000.0;                                                                 \n"
        "                                                                                         \n"
        "    hitValue = 0;                                                                \n"
        "   int results =0;\n"
        "                                                                                         \n";

    if (m_data.geometryType == GeometryType::SPHERES)
    {

        raygenSource += "vec3 vertices[12] = vec3[12](\n"
                        "    vec3(-8, -6, 1), // Vertex 15\n"
                        "    vec3(7, -6, 1),  // Vertex 13\n"
                        "    vec3(-8, -3, 1), // Vertex 11\n"
                        "    vec3(5, -1, 1),  // Vertex 9\n"
                        "    vec3(-9, 1, 1),  // Vertex 7\n"
                        "    vec3(4, 2, 1),   // Vertex 5\n"
                        "    vec3(-7, 5, 1),  // Vertex 3\n"
                        "    vec3(7, 7, 1),    // Vertex 1\n"
                        "    vec3(6, 6, 1),    // Vertex 2\n"
                        "    vec3(-8, 3, 1),    // Vertex 4\n"
                        "    vec3(6, 1, 1),    // Vertex 6\n"
                        "    vec3(-6, 0, 1)    // Vertex 8\n"
                        ");\n\n";
    }
    else
    {

        raygenSource += "vec3 vertices[12] = vec3[12](\n"
                        "    vec3(-8, 7, 1),  // Vertex 1\n"
                        "    vec3(8, 7, 1),   // Vertex 2\n"
                        "    vec3(8, 5, 1),   // Vertex 3\n"
                        "    vec3(-8, 5, 1),  // Vertex 4\n"
                        "    vec3(-8, 3, 1),  // Vertex 5\n"
                        "    vec3(8, 3, 1),   // Vertex 6\n"
                        "    vec3(8, 1, 1),   // Vertex 7\n"
                        "    vec3(-8, 1, 1),  // Vertex 8\n"
                        "    vec3(-8, -1, 1), // Vertex 9\n"
                        "    vec3(8, -1, 1),  // Vertex 10\n"
                        "    vec3(8, -3, 1),  // Vertex 11\n"
                        "    vec3(-8, -3, 1)  // Vertex 12\n"
                        ");\n";
    }
    if (!m_data.useEndcaps)
    {
        raygenSource += "vec3 noendCapsVertices[5] = vec3[5](\n"
                        "    vec3(1, 0, 1),  // Endcap 1\n"
                        "    vec3(4, 1, 1),  // Endcap 2\n"
                        "    vec3(7, 1, 1),  // Endcap 2\n"
                        "    vec3(9, 1, 1),  // Endcap 2\n"
                        "    vec3(11,0, 1)  // Endcap 2\n"
                        ");\n\n";
        raygenSource += "// Shoot rays at the vertices\n"
                        "for (int i = 0; i < 5; i++) {\n"
                        "    vec3 vertex = noendCapsVertices[i];\n\n"
                        "    vec3 direction = vec3(0,0,-1);";
    }
    else
    {
        raygenSource += "// Shoot rays at the vertices\n"
                        "for (int i = 0; i < 12; i++) {\n"
                        "    vec3 vertex = vertices[i];\n\n"
                        "    vec3 direction = vec3(0,0,-1);";
    }

    if (m_data.useRayQuery)
    {
        raygenSource += "    bool cond  = false; \n"
                        "    rayQueryEXT rq; \n"
                        "    rayQueryInitializeEXT(rq, topLevelAS, gl_RayFlagsOpaqueEXT, 0xff, vertex, tmin, "
                        "vec3(0,0,-1), "
                        "tmax); \n"
                        "    rayQueryProceedEXT(rq); \n"
                        "    uint hit = rayQueryGetIntersectionTypeEXT(rq, true); \n"
                        "    if (hit != gl_RayQueryCommittedIntersectionNoneEXT) { \n";

        if (m_data.geometryType == GeometryType::SPHERES &&
            (m_data.testType == TestType::VERTICES || m_data.testType == TestType::INDICES))
        {
            // Test for sphere geometry
            raygenSource += "        cond = rayQueryIsSphereHitNV(rq, true); \n";
        }
        else
        {
            // Test for LSS geometry
            raygenSource +=

                "        cond = rayQueryIsLSSHitNV(rq, true); \n";
        }
        raygenSource += "        hitValue = int(cond); \n"
                        "    } else { \n"
                        "        hitValue = 0; \n"
                        "    } \n";
    }
    else if (m_data.useHitObject)
    {
        raygenSource += "    hitObjectNV hObj; \n"
                        "    hitObjectTraceRayNV(hObj, topLevelAS, gl_RayFlagsOpaqueEXT, 0xff, 0, 1, 0, vertex, tmin, "
                        "vec3(0,0,-1), tmax, 0); \n"
                        "    reorderThreadNV(hObj); \n"

                        "    if (hitObjectIsHitNV(hObj)) { \n"
                        "        bool cond = false; \n";

        if (m_data.geometryType == GeometryType::SPHERES)
        {
            raygenSource += "    cond = hitObjectIsSphereHitNV(hObj) && !hitObjectIsLSSHitNV(hObj); \n";
        }
        else
        {
            raygenSource +=

                "    cond = !hitObjectIsSphereHitNV(hObj) && hitObjectIsLSSHitNV(hObj); \n";
        }
        raygenSource += "        hitValue = int(cond); \n "
                        "    } \n"
                        // Miss
                        "    else { hitValue = 0; } \n";
    }
    else
    {
        raygenSource += "    // Trace a ray from 'origin' towards the 'vertex' in the direction \n"
                        "    traceRayEXT(topLevelAS, 0, 0xff, 0, 1, 0, vertex, tmin, vec3(0,0,-1), tmax, 0);\n\n";
    }
    raygenSource += "    // Store the result by adding the hit value with the constant 0xFF000000\n"
                    "     results+=hitValue;\n"
                    "}\n"
                    "  uint  resultIndex = gl_LaunchIDEXT.x + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x;\n"
                    "  result.value[resultIndex] =results+ 0xFF000000;\n"
                    "};\n";
    programCollection.glslSources.add("rgen") << glu::RaygenSource(raygenSource) << glslBuildOptions;

    std::string closestHitSource = "    #version 460                                        \n"
                                   "    #extension GL_EXT_ray_tracing : enable              \n"
                                   "    #extension GL_NV_linear_swept_spheres : enable      \n"
                                   "    #extension GL_EXT_ray_tracing : enable              \n"
                                   "    layout(location = 0) rayPayloadInEXT int hitValue; \n"
                                   "                                                        \n"
                                   "void main() {                                           \n"
                                   "    bool cond  = false;                                 \n";

    if (m_data.geometryType == GeometryType::SPHERES &&
        (m_data.testType == TestType::VERTICES || m_data.testType == TestType::INDICES))
    {
        closestHitSource += "    cond  =  gl_HitIsSphereNV && !gl_HitIsLSSNV; \n";
    }
    else
    {
        closestHitSource += "    cond  = gl_HitIsLSSNV && !gl_HitIsSphereNV; \n";
    }

    closestHitSource += "    hitValue =1; \n"
                        "}                                                         \n";

    programCollection.glslSources.add("chit") << glu::ClosestHitSource(closestHitSource) << glslBuildOptions;

    std::string missShaderSource = "#version 460                                        \n"
                                   "#extension GL_EXT_ray_tracing : enable              \n"
                                   "layout(location = 0) rayPayloadInEXT int hitValue; \n"
                                   "                                                    \n"
                                   "void main() {                                       \n"
                                   "    hitValue =0;            \n"
                                   "}                                                   \n";

    programCollection.glslSources.add("miss") << glu::MissSource(missShaderSource) << glslBuildOptions;
}

} //namespace

tcu::TestCaseGroup *createLinearSweptSpheresTests(tcu::TestContext &testCtx)
{

    typedef struct
    {
        GeometryType geometryMode;

        const char *name;
    } GeometryMode;

    typedef struct
    {
        TestType testMode;

        const char *name;
    } TestMode;

    typedef struct
    {
        VertexFormat vertexFormat;

        const char *name;
    } VertexMode;

    typedef struct
    {
        RadiusFormat radiusFormat;

        const char *name;
    } RadiusMode;

    struct
    {
        bool doBlasCopy;
        const char *name;
    } blasCopyType[] = {
        {false, "no_blascopy"},
        {true, "blascopy"},
    };

    struct
    {
        bool useEndcaps;
        const char *name;
    } useEndcapsType[] = {
        {false, "no_endcaps"},
        {true, "endcaps"},
    };

    struct
    {
        bool useRayQuery;
        const char *name;
    } useRayQueryType[] = {
        {false, "no_use_ray_query"},
        {true, "use_ray_query"},
    };

    struct
    {
        bool useHitObject;
        const char *name;
    } useHitObjectType[] = {
        {false, "no_use_hit_object"},
        {true, "use_hit_object"},
    };

    TestParams testParams = {};

    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "linear_swept_spheres"));
    const GeometryMode geometryModes[] = {
        {GeometryType::SPHERES, "spheres"},
        {GeometryType::LSS, "lss"},
    };

    const TestMode testModes[] = {{TestType::VERTICES, "vertices"},
                                  {TestType::INDICES, "indices"},
                                  {TestType::INDEXING_MODE_LIST, "indexing_mode_list"},
                                  {TestType::INDEXING_MODE_SUCCESSIVE, "indexing_mode_successive"}};

    const VertexMode vertexFormatType[] = {
        {VertexFormat::FLOAT3, "float3"},
        {VertexFormat::FLOAT2, "float2"},
        {VertexFormat::HALF3, "half3"},
        {VertexFormat::HALF2, "half2"},
    };

    const RadiusMode radiusFormatType[] = {
        {RadiusFormat::R32, "float"},
        {RadiusFormat::R16, "half"},
    };

    for (int geometryIndex = 0; geometryIndex < DE_LENGTH_OF_ARRAY(geometryModes); geometryIndex++)
    {
        de::MovePtr<tcu::TestCaseGroup> geometryGroup(
            new tcu::TestCaseGroup(testCtx, geometryModes[geometryIndex].name));

        for (int modeIndex = 0; modeIndex < DE_LENGTH_OF_ARRAY(testModes); modeIndex++)
        {
            de::MovePtr<tcu::TestCaseGroup> modeGroup(new tcu::TestCaseGroup(testCtx, testModes[modeIndex].name));

            for (int blasCopyIndex = 0; blasCopyIndex < DE_LENGTH_OF_ARRAY(blasCopyType); blasCopyIndex++)
            {
                de::MovePtr<tcu::TestCaseGroup> blasCopyGroup(
                    new tcu::TestCaseGroup(testCtx, blasCopyType[blasCopyIndex].name));

                for (int endcapsIndex = 0; endcapsIndex < DE_LENGTH_OF_ARRAY(useEndcapsType); endcapsIndex++)
                {
                    de::MovePtr<tcu::TestCaseGroup> endcapsGroup(
                        new tcu::TestCaseGroup(testCtx, useEndcapsType[endcapsIndex].name));

                    for (int useRayQueryIndex = 0; useRayQueryIndex < DE_LENGTH_OF_ARRAY(useRayQueryType);
                         useRayQueryIndex++)
                    {
                        de::MovePtr<tcu::TestCaseGroup> useRayQueryGroup(
                            new tcu::TestCaseGroup(testCtx, useRayQueryType[useRayQueryIndex].name));

                        for (int useHitObjectIndex = 0; useHitObjectIndex < DE_LENGTH_OF_ARRAY(useHitObjectType);
                             useHitObjectIndex++)
                        {
                            de::MovePtr<tcu::TestCaseGroup> useHitObjectGroup(
                                new tcu::TestCaseGroup(testCtx, useHitObjectType[useHitObjectIndex].name));
                            for (int vertexFormatIndex = 0; vertexFormatIndex < DE_LENGTH_OF_ARRAY(vertexFormatType);
                                 vertexFormatIndex++)
                            {
                                de::MovePtr<tcu::TestCaseGroup> vertexFormatGroup(
                                    new tcu::TestCaseGroup(testCtx, vertexFormatType[vertexFormatIndex].name));
                                for (int radiusFormatIndex = 0;
                                     radiusFormatIndex < DE_LENGTH_OF_ARRAY(radiusFormatType); radiusFormatIndex++)
                                {
                                    if (geometryModes[geometryIndex].geometryMode == GeometryType::LSS &&
                                        useEndcapsType[endcapsIndex].useEndcaps == false &&
                                        (vertexFormatType[vertexFormatIndex].vertexFormat == VertexFormat::FLOAT2 ||
                                         vertexFormatType[vertexFormatIndex].vertexFormat == VertexFormat::HALF2))
                                    {

                                        continue; // Skip tests with half2 and float2 vertex format for LSS without endcaps
                                    }

                                    // Skip SPHERES geometry with no_endcaps
                                    if (geometryModes[geometryIndex].geometryMode == GeometryType::SPHERES &&
                                        useEndcapsType[endcapsIndex].useEndcaps == false)
                                    {
                                        continue;
                                    }

                                    // Skip SPHERES geometry with test types other than VERTICES or INDICES
                                    if (geometryModes[geometryIndex].geometryMode == GeometryType::SPHERES &&
                                        testModes[modeIndex].testMode != TestType::VERTICES &&
                                        testModes[modeIndex].testMode != TestType::INDICES)
                                    {
                                        continue;
                                    }

                                    // Skip LSS geometry with INDICES test type
                                    if (geometryModes[geometryIndex].geometryMode == GeometryType::LSS &&
                                        testModes[modeIndex].testMode == TestType::INDICES)
                                    {
                                        continue;
                                    }
                                    testParams.geometryType = geometryModes[geometryIndex].geometryMode;
                                    testParams.testType     = testModes[modeIndex].testMode;
                                    testParams.doBlasCopy   = blasCopyType[blasCopyIndex].doBlasCopy;
                                    testParams.useEndcaps   = useEndcapsType[endcapsIndex].useEndcaps;
                                    testParams.useRayQuery  = useRayQueryType[useRayQueryIndex].useRayQuery;
                                    testParams.useHitObject = useHitObjectType[useHitObjectIndex].useHitObject;
                                    testParams.vertexFormat = vertexFormatType[vertexFormatIndex].vertexFormat;
                                    testParams.radiusFormat = radiusFormatType[radiusFormatIndex].radiusFormat;

                                    vertexFormatGroup->addChild(new LinearSweptSpheresTestCase(
                                        testCtx, radiusFormatType[radiusFormatIndex].name, testParams));
                                }

                                useHitObjectGroup->addChild(vertexFormatGroup.release());
                            }
                            useRayQueryGroup->addChild(useHitObjectGroup.release());
                        }

                        endcapsGroup->addChild(useRayQueryGroup.release());
                    }
                    blasCopyGroup->addChild(endcapsGroup.release());
                }
                modeGroup->addChild(blasCopyGroup.release());
            }
            geometryGroup->addChild(modeGroup.release());
        }
        group->addChild(geometryGroup.release());
    }
    return group.release();
}

} // namespace RayTracing
} // namespace vkt
