/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
 * Copyright (c) 2025 Qualcomm Technologies, Inc.
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
 * \brief Ray Tracing Opacity Micromap Tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingOpacityMicromapTestsKHR.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkRayTracingUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "deUniquePtr.hpp"
#include "deRandom.hpp"

#include <sstream>
#include <vector>
#include <iostream>

namespace vkt
{
namespace RayTracing
{

namespace
{

using namespace vk;

enum TestFlagBits
{
    TEST_FLAG_BIT_FORCE_OPAQUE_INSTANCE             = 1U << 0,
    TEST_FLAG_BIT_FORCE_OPAQUE_RAY_FLAG             = 1U << 1,
    TEST_FLAG_BIT_DISABLE_OPACITY_MICROMAP_INSTANCE = 1U << 2,
    TEST_FLAG_BIT_FORCE_2_STATE_INSTANCE            = 1U << 3,
    TEST_FLAG_BIT_FORCE_2_STATE_RAY_FLAG            = 1U << 4,
    TEST_FLAG_BIT_CULL_NON_OPAQUE_RAY_FLAG          = 1U << 5,
    TEST_FLAG_BIT_CULL_OPAQUE_RAY_FLAG              = 1U << 6,
    TEST_FLAG_BIT_LAST                              = 1U << 7,
};

std::vector<std::string> testFlagBitNames = {
    "force_opaque_instance",  "force_opaque_ray_flag",  "disable_opacity_micromap_instance",
    "force_2_state_instance", "force_2_state_ray_flag", "cull_non_opaque_ray_flag",
    "cull_opaque_ray_flag",
};

struct TestParams
{
    bool useSpecialIndex; // Must be 1 for useNullHandleForSpecialIndex
    bool useNullHandleForSpecialIndex;
    bool nonZeroBase;
    bool lossy; // Lossy OMM
    bool serialize;
    uint32_t testFlagMask;
    uint32_t subdivisionLevel; // Must be 0 for useSpecialIndex
    uint32_t mode;             // Special index value if useSpecialIndex, 2 or 4 for number of states otherwise
    uint32_t seed;
};

Move<VkQueryPool> makeQueryPool(const DeviceInterface &vk, const VkDevice device, const VkQueryType queryType,
                                uint32_t queryCount)
{
    const VkQueryPoolCreateInfo queryPoolCreateInfo = {
        VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, // sType
        nullptr,                                  // pNext
        (VkQueryPoolCreateFlags)0,                // flags
        queryType,                                // queryType
        queryCount,                               // queryCount
        0u,                                       // pipelineStatistics
    };
    return createQueryPool(vk, device, &queryPoolCreateInfo);
}

class OpacityMicromapCase : public TestCase
{
public:
    OpacityMicromapCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params);
    virtual ~OpacityMicromapCase(void)
    {
    }

    virtual void checkSupport(Context &context) const;
    virtual void initPrograms(vk::SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;

protected:
    TestParams m_params;
};

class OpacityMicromapInstance : public TestInstance
{
public:
    OpacityMicromapInstance(Context &context, const TestParams &params);
    virtual ~OpacityMicromapInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void);

protected:
    TestParams m_params;
};

OpacityMicromapCase::OpacityMicromapCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
    : TestCase(testCtx, name)
    , m_params(params)
{
}

void OpacityMicromapCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
    context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");
    context.requireDeviceFunctionality("VK_KHR_opacity_micromap");
    context.requireDeviceFunctionality("VK_KHR_device_address_commands");

    const VkPhysicalDeviceAccelerationStructureFeaturesKHR &accelerationStructureFeaturesKHR =
        context.getAccelerationStructureFeatures();
    if (accelerationStructureFeaturesKHR.accelerationStructure == false)
        TCU_THROW(TestError, "VK_KHR_ray_tracing_pipeline requires "
                             "VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");

    const VkPhysicalDeviceOpacityMicromapFeaturesKHR &opacityMicromapFeaturesKHR = context.getOpacityMicromapFeatures();
    if (opacityMicromapFeaturesKHR.micromap == false)
        TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceOpacityMicromapFeaturesKHR.micromap");

    const VkPhysicalDeviceOpacityMicromapPropertiesKHR &opacityMicromapPropertiesKHR =
        context.getOpacityMicromapProperties();

    const VkPhysicalDeviceDeviceAddressCommandsFeaturesKHR &deviceAddressCommandsFeaturesKHR =
        context.getDeviceAddressCommandsFeatures();
    if (deviceAddressCommandsFeaturesKHR.deviceAddressCommands == false)
        TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceDeviceAddressCommandsFeaturesKHR.deviceAddressCommands");

    if (!m_params.useSpecialIndex)
    {
        switch (m_params.mode)
        {
        case 2:
            if (m_params.subdivisionLevel > opacityMicromapPropertiesKHR.maxOpacity2StateSubdivisionLevel)
                TCU_THROW(NotSupportedError, "Requires a higher supported 2 state subdivision level");
            break;
        case 4:
            if (m_params.lossy)
            {
                if (m_params.subdivisionLevel > opacityMicromapPropertiesKHR.maxOpacityLossy4StateSubdivisionLevel)
                    TCU_THROW(NotSupportedError, "Requires a higher supported lossy 4 state subdivision level");
            }
            else
            {
                if (m_params.subdivisionLevel > opacityMicromapPropertiesKHR.maxOpacity4StateSubdivisionLevel)
                    TCU_THROW(NotSupportedError, "Requires a higher supported 4 state subdivision level");
            }
            break;
        default:
            DE_ASSERT(false);
            break;
        }
    }
}

void OpacityMicromapCase::initPrograms(vk::SourceCollections &programCollection) const
{
    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

    uint32_t numRays = levelToSubtriangles(m_params.subdivisionLevel);

    std::ostringstream layoutDecls;
    layoutDecls << "layout(set=0, binding=0) uniform accelerationStructureEXT topLevelAS;\n"
                << "layout(set=0, binding=1, std430) buffer RayOrigins {\n"
                << "  vec4 values[" << numRays << "];\n"
                << "} origins;\n"
                << "layout(set=0, binding=2, std430) buffer OutputModes {\n"
                << "  uint values[" << numRays << "];\n"
                << "} modes;\n";
    const auto layoutDeclsStr = layoutDecls.str();

    std::string flagsString = "gl_RayFlagsNoneEXT";

    if (m_params.testFlagMask & TEST_FLAG_BIT_FORCE_OPAQUE_RAY_FLAG)
    {
        flagsString = "gl_RayFlagsOpaqueEXT";
    }
    else if (m_params.testFlagMask & TEST_FLAG_BIT_CULL_NON_OPAQUE_RAY_FLAG)
    {
        flagsString = "gl_RayFlagsCullNoOpaqueEXT";
    }
    else if (m_params.testFlagMask & TEST_FLAG_BIT_CULL_OPAQUE_RAY_FLAG)
    {
        flagsString = "gl_RayFlagsCullOpaqueEXT";
    }

    if (m_params.testFlagMask & TEST_FLAG_BIT_FORCE_2_STATE_RAY_FLAG)
        flagsString += " | gl_RayFlagsForceOpacityMicromap2StateEXT";

    std::ostringstream rgen;
    rgen << "#version 460 core\n"
         << "#extension GL_EXT_ray_tracing : require\n"
         << "#extension GL_EXT_opacity_micromap : require\n"
         << "\n"
         << "layout(location=0) rayPayloadEXT uint value;\n"
         << "\n"
         << layoutDeclsStr << "\n"
         << "void main()\n"
         << "{\n"
         << "  const uint  cullMask  = 0xFF;\n"
         << "  const vec3  origin    = origins.values[gl_LaunchIDEXT.x].xyz;\n"
         << "  const vec3  direction = vec3(0.0, 0.0, -1.0);\n"
         << "  const float tMin      = 0.0;\n"
         << "  const float tMax      = 2.0;\n"
         << "  value                 = 0xFFFFFFFF;\n"
         << "  traceRayEXT(topLevelAS, " << flagsString << ", cullMask, 0, 0, 0, origin, tMin, direction, tMax, 0);\n"
         << "  modes.values[gl_LaunchIDEXT.x] = value;\n"
         << "}\n";

    std::ostringstream ah;
    ah << "#version 460 core\n"
       << "#extension GL_EXT_ray_tracing : require\n"
       << "\n"
       << layoutDeclsStr << "\n"
       << "layout(location=0) rayPayloadInEXT uint value;\n"
       << "\n"
       << "void main()\n"
       << "{\n"
       << "  value = 1;\n"
       << "  terminateRayEXT;\n"
       << "}\n";

    std::ostringstream ch;
    ch << "#version 460 core\n"
       << "#extension GL_EXT_ray_tracing : require\n"
       << "\n"
       << layoutDeclsStr << "\n"
       << "layout(location=0) rayPayloadInEXT uint value;\n"
       << "\n"
       << "void main()\n"
       << "{\n"
       << "  if (value != 1) {\n" // If we didn't already run AH mark as CH
       << "    value = 2;\n"
       << "  }\n"
       << "}\n";

    std::ostringstream miss;
    miss << "#version 460 core\n"
         << "#extension GL_EXT_ray_tracing : require\n"
         << layoutDeclsStr << "\n"
         << "layout(location=0) rayPayloadInEXT uint value;\n"
         << "\n"
         << "void main()\n"
         << "{\n"
         << "  value = 0;\n"
         << "}\n";

    programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(rgen.str())) << buildOptions;
    programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(miss.str())) << buildOptions;
    programCollection.glslSources.add("ah") << glu::AnyHitSource(updateRayTracingGLSL(ah.str())) << buildOptions;
    programCollection.glslSources.add("ch") << glu::ClosestHitSource(updateRayTracingGLSL(ch.str())) << buildOptions;
}

TestInstance *OpacityMicromapCase::createInstance(Context &context) const
{
    return new OpacityMicromapInstance(context, m_params);
}

OpacityMicromapInstance::OpacityMicromapInstance(Context &context, const TestParams &params)
    : TestInstance(context)
    , m_params(params)
{
}

tcu::Vec2 calcSubtriangleCentroid(const uint32_t index, const uint32_t subdivisionLevel)
{
    if (subdivisionLevel == 0)
    {
        return tcu::Vec2(1.0f / 3.0f, 1.0f / 3.0f);
    }

    uint32_t d = index;

    d = ((d >> 1) & 0x22222222u) | ((d << 1) & 0x44444444u) | (d & 0x99999999u);
    d = ((d >> 2) & 0x0c0c0c0cu) | ((d << 2) & 0x30303030u) | (d & 0xc3c3c3c3u);
    d = ((d >> 4) & 0x00f000f0u) | ((d << 4) & 0x0f000f00u) | (d & 0xf00ff00fu);
    d = ((d >> 8) & 0x0000ff00u) | ((d << 8) & 0x00ff0000u) | (d & 0xff0000ffu);

    uint32_t f = (d & 0xffffu) | ((d << 16) & ~d);

    f ^= (f >> 1) & 0x7fff7fffu;
    f ^= (f >> 2) & 0x3fff3fffu;
    f ^= (f >> 4) & 0x0fff0fffu;
    f ^= (f >> 8) & 0x00ff00ffu;

    uint32_t t = (f ^ d) >> 16;

    uint32_t iu = ((f & ~t) | (d & ~t) | (~d & ~f & t)) & 0xffffu;
    uint32_t iv = ((f >> 16) ^ d) & 0xffffu;
    uint32_t iw = ((~f & ~t) | (d & ~t) | (~d & f & t)) & ((1 << subdivisionLevel) - 1);

    const float scale = 1.0f / float(1 << subdivisionLevel);

    float u = (1.0f / 3.0f) * scale;
    float v = (1.0f / 3.0f) * scale;

    // we need to only look at "subdivisionLevel" bits
    iu = iu & ((1 << subdivisionLevel) - 1);
    iv = iv & ((1 << subdivisionLevel) - 1);
    iw = iw & ((1 << subdivisionLevel) - 1);

    bool upright = (iu & 1) ^ (iv & 1) ^ (iw & 1);
    if (!upright)
    {
        iu = iu + 1;
        iv = iv + 1;
    }

    if (upright)
    {
        return tcu::Vec2(u + (float)iu * scale, v + (float)iv * scale);
    }
    else
    {
        return tcu::Vec2((float)iu * scale - u, (float)iv * scale - v);
    }
}
void getFinalStateValue(std::vector<uint32_t> &expectOut, uint32_t state, uint32_t testFlagMask)
{
    if (state != uint32_t(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_TRANSPARENT_EXT))
    {
        if (testFlagMask & (TEST_FLAG_BIT_FORCE_OPAQUE_INSTANCE | TEST_FLAG_BIT_FORCE_OPAQUE_RAY_FLAG))
        {
            state = uint32_t(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_OPAQUE_EXT);
        }
        else if (state != uint32_t(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_OPAQUE_EXT))
        {
            state = uint32_t(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_UNKNOWN_OPAQUE_EXT);
        }
    }

    if (state == uint32_t(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_TRANSPARENT_EXT))
    {
        expectOut.push_back(0);
    }
    else if (state == uint32_t(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_UNKNOWN_OPAQUE_EXT))
    {
        expectOut.push_back((testFlagMask & TEST_FLAG_BIT_CULL_NON_OPAQUE_RAY_FLAG) ? 0 : 1);
    }
    else if (state == uint32_t(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_OPAQUE_EXT))
    {
        expectOut.push_back((testFlagMask & TEST_FLAG_BIT_CULL_OPAQUE_RAY_FLAG) ? 0 : 2);
    }
    else
    {
        DE_ASSERT(false);
    }
}
tcu::TestStatus OpacityMicromapInstance::iterate(void)
{
    const auto &vki    = m_context.getInstanceInterface();
    const auto physDev = m_context.getPhysicalDevice();
    const auto &vkd    = m_context.getDeviceInterface();
    const auto device  = m_context.getDevice();
    auto &alloc        = m_context.getDefaultAllocator();
    const auto qIndex  = m_context.getUniversalQueueFamilyIndex();
    const auto queue   = m_context.getUniversalQueue();
    const auto stages  = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
                        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

    // Command pool and buffer.
    const auto cmdPool      = makeCommandPool(vkd, device, qIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    beginCommandBuffer(vkd, cmdBuffer);

    // Build acceleration structures.
    auto topLevelAS     = makeTopLevelAccelerationStructure();
    auto bottomLevelAS  = makeBottomLevelAccelerationStructure();
    auto micromapAS     = makeMicromapAccelerationStructure();
    auto micromapASCopy = makeMicromapAccelerationStructure();
    std::vector<de::SharedPtr<SerialStorage>> micromapSerialized;

    const auto triangleCount       = (m_params.nonZeroBase ? 2u : 1u);
    uint32_t numSubtriangles       = levelToSubtriangles(m_params.subdivisionLevel);
    uint32_t triangleMicromapBytes = (m_params.mode == 2) ? (numSubtriangles + 7) / 8 : (numSubtriangles + 3) / 4;
    uint32_t opacityMicromapBytes  = triangleMicromapBytes * triangleCount;

    // Generate random micromap data
    std::vector<uint8_t> opacityMicromapData;
    opacityMicromapData.reserve(opacityMicromapBytes);

    de::Random rnd(m_params.seed);

    while (opacityMicromapData.size() < opacityMicromapBytes)
    {
        opacityMicromapData.push_back(rnd.getUint8());
    }

    if (m_params.lossy)
        micromapAS->setBuildFlags(VK_BUILD_ACCELERATION_STRUCTURE_MICROMAP_LOSSY_BIT_KHR);

    // Build a micromap (ignore infrastructure for now)
    // Create the buffer with the mask and index data
    // Allocate a fairly conservative bound for now
    micromapAS->addOpacityMicromap(
        opacityMicromapData, triangleCount, m_params.subdivisionLevel,
        (m_params.mode == 2 ? VK_OPACITY_MICROMAP_FORMAT_2_STATE_KHR : VK_OPACITY_MICROMAP_FORMAT_4_STATE_KHR),
        m_params.useSpecialIndex, m_params.mode);
    micromapAS->createAndBuild(vkd, device, cmdBuffer, alloc, 0);

    if (m_params.serialize)
    {
        std::vector<VkDeviceSize> micromapSerializationSize(1);
        const auto queryPoolSerialization =
            makeQueryPool(vkd, device, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR, 1);

        queryAccelerationStructureSize(vkd, device, cmdBuffer, {*micromapAS->getPtr()},
                                       VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, *queryPoolSerialization,
                                       VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR, 0u,
                                       micromapSerializationSize);
        endCommandBuffer(vkd, cmdBuffer);
        submitCommandsAndWait(vkd, device, queue, cmdBuffer);

        VK_CHECK(vkd.getQueryPoolResults(device, *queryPoolSerialization, 0u, 1, sizeof(VkDeviceSize),
                                         micromapSerializationSize.data(), sizeof(VkDeviceSize),
                                         VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));

        vkd.resetCommandPool(device, *cmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
        beginCommandBuffer(vkd, cmdBuffer);

        de::SharedPtr<SerialStorage> storage(new SerialStorage(
            vkd, device, alloc, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, micromapSerializationSize[0]));

        micromapAS->serialize(vkd, device, cmdBuffer, storage.get());
        micromapSerialized.push_back(storage);
        endCommandBuffer(vkd, cmdBuffer);
        submitCommandsAndWait(vkd, device, queue, cmdBuffer);

        vkd.resetCommandPool(device, *cmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
        beginCommandBuffer(vkd, cmdBuffer);
        micromapASCopy->createAndDeserializeFrom(vkd, device, cmdBuffer, alloc, storage.get());
    }

    // Attach the micromap to the geometry
    VkAccelerationStructureTrianglesOpacityMicromapKHR opacityGeometryMicromap = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_OPACITY_MICROMAP_KHR, //VkStructureType             sType;
        nullptr,                                                                 // void*                      pNext;
        VK_INDEX_TYPE_UINT32,                        // VkIndexType                indexType;
        micromapAS->getIndexBufferAddr(vkd, device), // VkDeviceAddress            indexBuffer;
        0,                                           // VkDeviceSize               indexStride;
        (m_params.nonZeroBase ? 1u : 0u),            // uint32_t                   baseTriangle;
        (m_params.useNullHandleForSpecialIndex ?
             VK_NULL_HANDLE :
             (m_params.serialize ? *micromapASCopy->getPtr() :
                                   *micromapAS->getPtr())), // VkAccelerationStructureKHR micromap;
    };

    const std::vector<tcu::Vec3> triangle = {
        tcu::Vec3(0.0f, 0.0f, 0.0f),
        tcu::Vec3(1.0f, 0.0f, 0.0f),
        tcu::Vec3(0.0f, 1.0f, 0.0f),
    };

    AccelerationStructBufferProperties bufferProps;
    bufferProps.props.residency = ResourceResidency::TRADITIONAL;

    bottomLevelAS->addGeometry(triangle, true /*is triangles*/, 0, nullptr, nullptr, &opacityGeometryMicromap);
    if (m_params.testFlagMask & TEST_FLAG_BIT_DISABLE_OPACITY_MICROMAP_INSTANCE)
        bottomLevelAS->setBuildFlags(VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DISABLE_OPACITY_MICROMAPS_EXT);
    bottomLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc, bufferProps);
    de::SharedPtr<BottomLevelAccelerationStructure> blasSharedPtr(bottomLevelAS.release());

    VkGeometryInstanceFlagsKHR instanceFlags = 0;

    if (m_params.testFlagMask & TEST_FLAG_BIT_FORCE_2_STATE_INSTANCE)
        instanceFlags |= VK_GEOMETRY_INSTANCE_FORCE_OPACITY_MICROMAP_2_STATE_EXT;
    if (m_params.testFlagMask & TEST_FLAG_BIT_FORCE_OPAQUE_INSTANCE)
        instanceFlags |= VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
    if (m_params.testFlagMask & TEST_FLAG_BIT_DISABLE_OPACITY_MICROMAP_INSTANCE)
        instanceFlags |= VK_GEOMETRY_INSTANCE_DISABLE_OPACITY_MICROMAPS_EXT;

    topLevelAS->setInstanceCount(1);
    topLevelAS->addInstance(blasSharedPtr, identityMatrix3x4, 0, 0xFFu, 0u, instanceFlags);
    topLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc, bufferProps);

    // One ray per subtriangle for this test
    uint32_t numRays = numSubtriangles;

    // SSBO buffer for origins.
    const auto originsBufferSize = static_cast<VkDeviceSize>(sizeof(tcu::Vec4) * numRays);
    const auto originsBufferInfo = makeBufferCreateInfo(originsBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory originsBuffer(vkd, device, alloc, originsBufferInfo, MemoryRequirement::HostVisible);
    auto &originsBufferAlloc = originsBuffer.getAllocation();
    void *originsBufferData  = originsBufferAlloc.getHostPtr();

    std::vector<tcu::Vec4> origins;
    std::vector<uint32_t> expectedOutputModes;
    std::vector<std::vector<uint32_t>> expectedLossyOutputModes;
    origins.reserve(numRays);
    expectedOutputModes.reserve(numRays);
    expectedLossyOutputModes.reserve(numRays);

    const auto micromapDataOffset = (m_params.nonZeroBase ? triangleMicromapBytes : 0u);
    const auto numOfLossyOptions  = 2u;

    // Fill in vector of expected outputs
    for (uint32_t index = 0; index < numRays; index++)
    {
        uint32_t state =
            m_params.testFlagMask & (TEST_FLAG_BIT_FORCE_OPAQUE_INSTANCE | TEST_FLAG_BIT_FORCE_OPAQUE_RAY_FLAG) ?
                VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_OPAQUE_EXT :
                VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_UNKNOWN_OPAQUE_EXT;
        uint32_t lossyState1 = VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_UNKNOWN_OPAQUE_EXT;
        uint32_t lossyState2 = VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_UNKNOWN_TRANSPARENT_EXT;

        expectedLossyOutputModes.emplace_back();
        auto &expectedLossyOutput = expectedLossyOutputModes.back();
        expectedLossyOutput.reserve(numOfLossyOptions);

        if (!(m_params.testFlagMask & TEST_FLAG_BIT_DISABLE_OPACITY_MICROMAP_INSTANCE))
        {
            if (m_params.useSpecialIndex)
            {
                state = m_params.mode;
            }
            else
            {
                if (m_params.mode == 2)
                {
                    uint8_t byte = opacityMicromapData[index / 8 + micromapDataOffset];
                    state        = (byte >> (index % 8)) & 0x1;
                }
                else
                {
                    DE_ASSERT(m_params.mode == 4);
                    uint8_t byte = opacityMicromapData[index / 4 + micromapDataOffset];
                    state        = (byte >> 2 * (index % 4)) & 0x3;
                }
                // Process in SPECIAL_INDEX number space
                state = ~state;
            }

            if (m_params.testFlagMask & (TEST_FLAG_BIT_FORCE_2_STATE_INSTANCE | TEST_FLAG_BIT_FORCE_2_STATE_RAY_FLAG))
            {
                if (state == uint32_t(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_UNKNOWN_TRANSPARENT_EXT))
                    state = uint32_t(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_TRANSPARENT_EXT);
                if (state == uint32_t(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_UNKNOWN_OPAQUE_EXT))
                    state = uint32_t(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_OPAQUE_EXT);
                lossyState1 = VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_OPAQUE_EXT;
                lossyState2 = VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_TRANSPARENT_EXT;
            }
        }

        getFinalStateValue(expectedOutputModes, state, m_params.testFlagMask);
        getFinalStateValue(expectedLossyOutput, lossyState1, m_params.testFlagMask);
        getFinalStateValue(expectedLossyOutput, lossyState2, m_params.testFlagMask);
    }

    for (uint32_t index = 0; index < numRays; index++)
    {
        tcu::Vec2 centroid = calcSubtriangleCentroid(index, m_params.subdivisionLevel);
        origins.push_back(tcu::Vec4(centroid.x(), centroid.y(), 1.0, 0.0));
    }

    const auto originsBufferSizeSz = static_cast<size_t>(originsBufferSize);
    deMemcpy(originsBufferData, origins.data(), originsBufferSizeSz);
    flushAlloc(vkd, device, originsBufferAlloc);

    // Storage buffer for output modes
    const auto outputModesBufferSize = static_cast<VkDeviceSize>(sizeof(uint32_t) * numRays);
    const auto outputModesBufferInfo = makeBufferCreateInfo(outputModesBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory outputModesBuffer(vkd, device, alloc, outputModesBufferInfo, MemoryRequirement::HostVisible);
    auto &outputModesBufferAlloc = outputModesBuffer.getAllocation();
    void *outputModesBufferData  = outputModesBufferAlloc.getHostPtr();
    deMemset(outputModesBufferData, 0xFF, static_cast<size_t>(outputModesBufferSize));
    flushAlloc(vkd, device, outputModesBufferAlloc);

    // Descriptor set layout.
    DescriptorSetLayoutBuilder dsLayoutBuilder;
    dsLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, stages);
    dsLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stages);
    dsLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stages);
    const auto setLayout = dsLayoutBuilder.build(vkd, device);

    // Pipeline layout.
    const auto pipelineLayout = makePipelineLayout(vkd, device, setLayout.get());

    // Descriptor pool and set.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    const auto descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descriptorSet  = makeDescriptorSet(vkd, device, descriptorPool.get(), setLayout.get());

    // Update descriptor set.
    {
        const VkWriteDescriptorSetAccelerationStructureKHR accelDescInfo = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            nullptr,
            1u,
            topLevelAS.get()->getPtr(),
        };
        const auto inStorageBufferInfo = makeDescriptorBufferInfo(originsBuffer.get(), 0ull, VK_WHOLE_SIZE);
        const auto storageBufferInfo   = makeDescriptorBufferInfo(outputModesBuffer.get(), 0ull, VK_WHOLE_SIZE);

        DescriptorSetUpdateBuilder updateBuilder;
        updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                                  VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelDescInfo);
        updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u),
                                  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &inStorageBufferInfo);
        updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(2u),
                                  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &storageBufferInfo);
        updateBuilder.update(vkd, device);
    }

    // Shader modules.
    auto rgenModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("rgen"), 0);
    auto missModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("miss"), 0);
    auto ahModule   = createShaderModule(vkd, device, m_context.getBinaryCollection().get("ah"), 0);
    auto chModule   = createShaderModule(vkd, device, m_context.getBinaryCollection().get("ch"), 0);

    // Get some ray tracing properties.
    uint32_t shaderGroupHandleSize    = 0u;
    uint32_t shaderGroupBaseAlignment = 1u;
    {
        const auto rayTracingPropertiesKHR = makeRayTracingProperties(vki, physDev);
        shaderGroupHandleSize              = rayTracingPropertiesKHR->getShaderGroupHandleSize();
        shaderGroupBaseAlignment           = rayTracingPropertiesKHR->getShaderGroupBaseAlignment();
    }

    // Create raytracing pipeline and shader binding tables.
    Move<VkPipeline> pipeline;
    de::MovePtr<BufferWithMemory> raygenSBT;
    de::MovePtr<BufferWithMemory> missSBT;
    de::MovePtr<BufferWithMemory> hitSBT;
    de::MovePtr<BufferWithMemory> callableSBT;

    auto raygenSBTRegion   = makeStridedDeviceAddressRegionKHR(0, 0, 0);
    auto missSBTRegion     = makeStridedDeviceAddressRegionKHR(0, 0, 0);
    auto hitSBTRegion      = makeStridedDeviceAddressRegionKHR(0, 0, 0);
    auto callableSBTRegion = makeStridedDeviceAddressRegionKHR(0, 0, 0);

    {
        const auto rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();
        rayTracingPipeline->setCreateFlags(VK_PIPELINE_CREATE_RAY_TRACING_OPACITY_MICROMAP_BIT_KHR);
        rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgenModule, 0);
        rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, missModule, 1);
        rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR, ahModule, 2);
        rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, chModule, 2);

        pipeline = rayTracingPipeline->createPipeline(vkd, device, pipelineLayout.get());

        raygenSBT       = rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc,
                                                                       shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
        raygenSBTRegion = makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenSBT->get(), 0),
                                                            shaderGroupHandleSize, shaderGroupHandleSize);

        missSBT       = rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc,
                                                                     shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
        missSBTRegion = makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missSBT->get(), 0),
                                                          shaderGroupHandleSize, shaderGroupHandleSize);

        hitSBT = rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize,
                                                              shaderGroupBaseAlignment, 2, 1);
        hitSBTRegion = makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitSBT->get(), 0),
                                                         shaderGroupHandleSize, shaderGroupHandleSize);
    }

    // Trace rays.
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.get());
    vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout.get(), 0u, 1u,
                              &descriptorSet.get(), 0u, nullptr);
    vkd.cmdTraceRaysKHR(cmdBuffer, &raygenSBTRegion, &missSBTRegion, &hitSBTRegion, &callableSBTRegion, numRays, 1u,
                        1u);

    // Barrier for the output buffer.
    const auto bufferBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                           &bufferBarrier, 0u, nullptr, 0u, nullptr);

    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    // Verify results.
    std::vector<uint32_t> outputData(expectedOutputModes.size());
    const auto outputModesBufferSizeSz = static_cast<size_t>(outputModesBufferSize);

    invalidateAlloc(vkd, device, outputModesBufferAlloc);
    DE_ASSERT(de::dataSize(outputData) == outputModesBufferSizeSz);
    deMemcpy(outputData.data(), outputModesBufferData, outputModesBufferSizeSz);

    bool fail = false;
    auto &log = m_context.getTestContext().getLog();

    const auto logValues = [&](uint32_t ref, uint32_t res, size_t idx)
    {
        std::ostringstream msg;
        msg << "Ray " << idx << ": expected " << ref << " and found " << res;
        log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
    };

    const auto lossyLogValues = [&](uint32_t ref1, uint32_t ref2, uint32_t ref3, uint32_t res, size_t idx)
    {
        std::ostringstream msg;
        msg << "Ray " << idx << ": expected " << ref1 << " and may lossy substitute to " << ref2 << " or " << ref3
            << " and found " << res;
        log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
    };

    for (size_t i = 0; i < outputData.size(); ++i)
    {
        const auto &outVal      = outputData[i];
        const auto &expectedVal = expectedOutputModes[i];

        if (outVal != expectedVal)
        {
            if ((m_params.lossy == true) &&
                (!(m_params.testFlagMask & TEST_FLAG_BIT_DISABLE_OPACITY_MICROMAP_INSTANCE)) && (m_params.mode == 4))
            {
                const auto &expectedLossyVal = expectedLossyOutputModes.at(i);

                DE_ASSERT(de::sizeU32(expectedLossyVal) == numOfLossyOptions);

                if ((outVal != expectedLossyVal[0]) && (outVal != expectedLossyVal[1]))
                {
                    lossyLogValues(expectedVal, expectedLossyVal[0], expectedLossyVal[1], outVal, i);
                    fail = true;
                }
            }
            else
            {
                logValues(expectedVal, outVal, i);
                fail = true;
            }
        }
    }

    if (fail)
        TCU_FAIL("Unexpected values found in output buffer; check log for details --");
    return tcu::TestStatus::pass("Pass");
}

} // namespace

bool isIlliegalRayFlagsComb(uint32_t mask)
{
    // Extract ray flags bits
    uint32_t bit1 = (mask & TEST_FLAG_BIT_FORCE_OPAQUE_RAY_FLAG) ? 1 : 0;
    uint32_t bit2 = (mask & TEST_FLAG_BIT_CULL_NON_OPAQUE_RAY_FLAG) ? 1 : 0;
    uint32_t bit3 = (mask & TEST_FLAG_BIT_CULL_OPAQUE_RAY_FLAG) ? 1 : 0;

    // Count how many are set
    uint32_t count = bit1 + bit2 + bit3;

    return count > 1;
}

constexpr uint32_t kMaxSubdivisionLevel = 15;

void addSerializeTestsKHR(tcu::TestCaseGroup *group)
{
    uint32_t seed = 718634540u;

    struct
    {
        uint32_t mode;
        std::string name;
    } modes[] = {{2, "2"}, {4, "4"}};

    for (uint32_t modeNdx = 0; modeNdx < DE_LENGTH_OF_ARRAY(modes); ++modeNdx)
    {
        de::MovePtr<tcu::TestCaseGroup> modeGroup(
            new tcu::TestCaseGroup(group->getTestContext(), modes[modeNdx].name.c_str()));

        for (uint32_t level = 0; level <= kMaxSubdivisionLevel; level++)
        {
            TestParams testParams{
                false, false, false, false, true, 0u, level, modes[modeNdx].mode, seed++,
            };

            std::stringstream css;
            css << "level_" << level;
            const auto testName = css.str();

            modeGroup->addChild(new OpacityMicromapCase(group->getTestContext(), testName, testParams));
        }

        group->addChild(modeGroup.release());
    }
}

tcu::TestCaseGroup *createOpacityMicromapTestsKHR(tcu::TestContext &testCtx)
{
    // Test acceleration structures using opacity micromap with ray pipelines
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "khr"));

    uint32_t seed = 1614343620u;

    const struct
    {
        bool useSpecialIndex;
        std::string name;
    } specialIndexUse[] = {
        {false, "map_value"},
        {true, "special_index"},
    };

    for (uint32_t testFlagMask = 0; testFlagMask < TEST_FLAG_BIT_LAST; testFlagMask++)
    {
        if (isIlliegalRayFlagsComb(testFlagMask))
        {
            continue;
        }
        std::string maskName = "";

        for (uint32_t bit = 0; bit < testFlagBitNames.size(); bit++)
        {
            if (testFlagMask & (1 << bit))
            {
                if (maskName != "")
                    maskName += "_";
                maskName += testFlagBitNames[bit];
            }
        }
        if (maskName == "")
            maskName = "no_flags";

        de::MovePtr<tcu::TestCaseGroup> testFlagGroup(
            new tcu::TestCaseGroup(group->getTestContext(), maskName.c_str()));

        for (size_t specialIndexNdx = 0; specialIndexNdx < DE_LENGTH_OF_ARRAY(specialIndexUse); ++specialIndexNdx)
        {
            de::MovePtr<tcu::TestCaseGroup> specialGroup(
                new tcu::TestCaseGroup(testFlagGroup->getTestContext(), specialIndexUse[specialIndexNdx].name.c_str()));

            if (specialIndexUse[specialIndexNdx].useSpecialIndex)
            {
                for (uint32_t specialIndex = 0; specialIndex < 4; specialIndex++)
                {
                    TestParams testParams{
                        specialIndexUse[specialIndexNdx].useSpecialIndex,
                        false,
                        false,
                        false,
                        false,
                        testFlagMask,
                        0,
                        ~specialIndex,
                        seed++,
                    };

                    std::stringstream css;
                    css << specialIndex;
                    const auto testName = css.str();

                    specialGroup->addChild(new OpacityMicromapCase(testCtx, testName, testParams));

                    if (testFlagMask == 0u)
                    {
                        testParams.useNullHandleForSpecialIndex = true;
                        const auto variantName                  = testName + "_null_handle";
                        specialGroup->addChild(new OpacityMicromapCase(testCtx, variantName, testParams));
                    }
                }
                testFlagGroup->addChild(specialGroup.release());
            }
            else
            {
                struct
                {
                    uint32_t mode;
                    std::string name;
                } modes[] = {{2, "2"}, {4, "4"}};

                for (uint32_t modeNdx = 0; modeNdx < DE_LENGTH_OF_ARRAY(modes); ++modeNdx)
                {
                    de::MovePtr<tcu::TestCaseGroup> modeGroup(
                        new tcu::TestCaseGroup(testFlagGroup->getTestContext(), modes[modeNdx].name.c_str()));
                    if (modes[modeNdx].mode == 4)
                    {
                        struct
                        {
                            bool lossy;
                            std::string name;
                        } lossyModes[] = {{true, "lossy"}, {false, "no_lossy"}};
                        for (size_t lossyModeNdx = 0; lossyModeNdx < DE_LENGTH_OF_ARRAY(lossyModes); lossyModeNdx++)
                        {
                            de::MovePtr<tcu::TestCaseGroup> lossyModeGroup(new tcu::TestCaseGroup(
                                modeGroup->getTestContext(), lossyModes[lossyModeNdx].name.c_str()));
                            for (uint32_t level = 0; level <= kMaxSubdivisionLevel; level++)
                            {
                                TestParams testParams{
                                    specialIndexUse[specialIndexNdx].useSpecialIndex,
                                    false,
                                    false,
                                    lossyModes[lossyModeNdx].lossy,
                                    false,
                                    testFlagMask,
                                    level,
                                    modes[modeNdx].mode,
                                    seed++,
                                };

                                std::stringstream css;
                                css << "level_" << level;
                                const auto testName = css.str();

                                lossyModeGroup->addChild(new OpacityMicromapCase(testCtx, testName, testParams));

                                if (testFlagMask == 0u)
                                {
                                    testParams.nonZeroBase = true;
                                    const auto variantName = testName + "_non_zero_base";
                                    lossyModeGroup->addChild(new OpacityMicromapCase(testCtx, variantName, testParams));
                                }
                            }
                            modeGroup->addChild(lossyModeGroup.release());
                        }
                    }
                    else
                    {
                        for (uint32_t level = 0; level <= kMaxSubdivisionLevel; level++)
                        {
                            TestParams testParams{
                                specialIndexUse[specialIndexNdx].useSpecialIndex,
                                false,
                                false,
                                false,
                                false,
                                testFlagMask,
                                level,
                                modes[modeNdx].mode,
                                seed++,
                            };

                            std::stringstream css;
                            css << "level_" << level;
                            const auto testName = css.str();

                            modeGroup->addChild(new OpacityMicromapCase(testCtx, testName, testParams));

                            if (testFlagMask == 0u)
                            {
                                testParams.nonZeroBase = true;
                                const auto variantName = testName + "_non_zero_base";
                                modeGroup->addChild(new OpacityMicromapCase(testCtx, variantName, testParams));
                            }
                        }
                    }
                    specialGroup->addChild(modeGroup.release());
                }
                testFlagGroup->addChild(specialGroup.release());
            }
        }

        group->addChild(testFlagGroup.release());
    }

    addTestGroup(group.get(), "serialize", addSerializeTestsKHR);

    return group.release();
}

} // namespace RayTracing
} // namespace vkt
