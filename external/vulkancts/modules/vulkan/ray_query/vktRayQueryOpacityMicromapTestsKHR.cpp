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
 * \brief Ray Query Opacity Micromap Tests
 *//*--------------------------------------------------------------------*/

#include "vktRayQueryOpacityMicromapTestsKHR.hpp"
#include "vktTestCase.hpp"

#include "vkRayTracingUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "deUniquePtr.hpp"
#include "deRandom.hpp"

#include <sstream>
#include <vector>
#include <iostream>

namespace vkt
{
namespace RayQuery
{

namespace
{

using namespace vk;

enum ShaderSourcePipeline
{
    SSP_GRAPHICS_PIPELINE,
    SSP_COMPUTE_PIPELINE,
    SSP_RAY_TRACING_PIPELINE
};

enum ShaderSourceType
{
    SST_VERTEX_SHADER,
    SST_COMPUTE_SHADER,
    SST_RAY_GENERATION_SHADER,
};

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

enum CopyType
{
    CT_NONE,
    CT_FIRST_ACTIVE,
    CT_CLONE = CT_FIRST_ACTIVE,
    CT_COMPACT,
    CT_SERIALIZE,
    CT_NUM_COPY_TYPES,
};

std::vector<std::string> copyTypeNames{
    "none",
    "clone",
    "compact",
    "serialize",
};

struct TestParams
{
    ShaderSourceType shaderSourceType;
    ShaderSourcePipeline shaderSourcePipeline;
    bool useSpecialIndex; // Must be 1 for useNullHandleForSpecialIndex
    bool useNullHandleForSpecialIndex;
    bool nonZeroBase;
    bool lossy; // Lossy OMM
    bool update;
    uint32_t testFlagMask;
    uint32_t subdivisionLevel; // Must be 0 for useSpecialIndex
    uint32_t mode;             // Special index value if useSpecialIndex, 2 or 4 for number of states otherwise
    uint32_t seed;
    CopyType copyType;
    bool useMaintenance5;
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

static constexpr uint32_t kNumThreadsAtOnce = 1024;
static constexpr uint32_t kWorkGroupCount   = 8;
static constexpr uint32_t kLocalSize        = 128;
static constexpr uint32_t kConstantID       = 1045;
DE_STATIC_ASSERT(kWorkGroupCount *kLocalSize == kNumThreadsAtOnce);

class OpacityMicromapCompatibilityKHRTestCase : public TestCase
{
public:
    OpacityMicromapCompatibilityKHRTestCase(tcu::TestContext &testCtx, const std::string &name,
                                            const TestParams &params);
    virtual ~OpacityMicromapCompatibilityKHRTestCase(void)
    {
    }

    virtual void checkSupport(Context &context) const;
    virtual TestInstance *createInstance(Context &context) const;

protected:
    TestParams m_params;
};

class OpacityMicromapCompatibilityKHRTestInstance : public TestInstance
{
public:
    OpacityMicromapCompatibilityKHRTestInstance(Context &context, const TestParams &params)
        : TestInstance(context)
        , m_params(params)
    {
    }
    virtual ~OpacityMicromapCompatibilityKHRTestInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void);

protected:
    VkAccelerationStructureCompatibilityKHR getDeviceASCompatibilityKHR(const uint8_t *versionInfoData);
    std::string getUUIDsString(const uint8_t *header) const;
    TestParams m_params;
};

OpacityMicromapCompatibilityKHRTestCase::OpacityMicromapCompatibilityKHRTestCase(tcu::TestContext &testCtx,
                                                                                 const std::string &name,
                                                                                 const TestParams &params)
    : TestCase(testCtx, name)
    , m_params(params)
{
}

void OpacityMicromapCompatibilityKHRTestCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
    context.requireDeviceFunctionality("VK_KHR_opacity_micromap");
    context.requireDeviceFunctionality("VK_KHR_device_address_commands");

    const VkPhysicalDeviceAccelerationStructureFeaturesKHR &accelerationStructureFeaturesKHR =
        context.getAccelerationStructureFeatures();
    if (accelerationStructureFeaturesKHR.accelerationStructure == false)
        TCU_THROW(TestError, "Requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");

    const VkPhysicalDeviceOpacityMicromapFeaturesKHR &opacityMicromapFeaturesKHR = context.getOpacityMicromapFeatures();
    if (opacityMicromapFeaturesKHR.micromap == false)
        TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceOpacityMicromapFeaturesKHR.micromap");

    const VkPhysicalDeviceDeviceAddressCommandsFeaturesKHR &deviceAddressCommandsFeaturesKHR =
        context.getDeviceAddressCommandsFeatures();
    if (deviceAddressCommandsFeaturesKHR.deviceAddressCommands == false)
        TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceDeviceAddressCommandsFeaturesKHR.deviceAddressCommands");
}

TestInstance *OpacityMicromapCompatibilityKHRTestCase::createInstance(Context &context) const
{
    return new OpacityMicromapCompatibilityKHRTestInstance(context, m_params);
}

VkAccelerationStructureCompatibilityKHR OpacityMicromapCompatibilityKHRTestInstance::getDeviceASCompatibilityKHR(
    const uint8_t *versionInfoData)
{
    const VkDevice device      = m_context.getDevice();
    const DeviceInterface &vkd = m_context.getDeviceInterface();

    VkAccelerationStructureCompatibilityKHR compatibility = VK_ACCELERATION_STRUCTURE_COMPATIBILITY_MAX_ENUM_KHR;

    const VkAccelerationStructureVersionInfoKHR versionInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_VERSION_INFO_KHR, // sType
        nullptr,                                                   // pNext
        versionInfoData                                            // pVersionData
    };

    vkd.getDeviceAccelerationStructureCompatibilityKHR(device, &versionInfo, &compatibility);

    return compatibility;
}

std::string OpacityMicromapCompatibilityKHRTestInstance::getUUIDsString(const uint8_t *header) const
{
    std::stringstream ss;

    int offset         = 0;
    const int widths[] = {4, 2, 2, 2, 6};

    for (int h = 0; h < 2; ++h)
    {
        if (h)
            ss << ' ';

        for (int w = 0; w < DE_LENGTH_OF_ARRAY(widths); ++w)
        {
            if (w)
                ss << '-';

            for (int i = 0; i < widths[w]; ++i)
                ss << std::hex << std::uppercase << static_cast<int>(header[i + offset]);

            offset += widths[w];
        }
    }

    return ss.str();
}

tcu::TestStatus OpacityMicromapCompatibilityKHRTestInstance::iterate(void)
{

    const auto &vkd   = m_context.getDeviceInterface();
    const auto device = m_context.getDevice();
    auto &alloc       = m_context.getDefaultAllocator();
    const auto qIndex = m_context.getUniversalQueueFamilyIndex();
    const auto queue  = m_context.getUniversalQueue();
    // Command pool and buffer.
    const auto cmdPool      = makeCommandPool(vkd, device, qIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();
    beginCommandBuffer(vkd, cmdBuffer);

    std::vector<VkDeviceSize> micromapCompactSize(1);
    std::vector<VkDeviceSize> micromapSerilizationSize(1);
    std::vector<VkDeviceSize> micromapCompactSerilizationSize(1);
    Move<VkQueryPool> queryPoolCompact;
    Move<VkQueryPool> queryPoolSerilization;

    auto micromapAS            = makeMicromapAccelerationStructure();
    auto micromapASCompactCopy = makeMicromapAccelerationStructure();

    micromapAS->setUseMaintenance5(m_params.useMaintenance5);
    micromapASCompactCopy->setUseMaintenance5(m_params.useMaintenance5);

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

    // Attach the micromap data
    micromapAS->setBuildFlags(VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR);
    micromapAS->addOpacityMicromap(
        opacityMicromapData, triangleCount, m_params.subdivisionLevel,
        (m_params.mode == 2 ? VK_OPACITY_MICROMAP_FORMAT_2_STATE_KHR : VK_OPACITY_MICROMAP_FORMAT_4_STATE_KHR),
        m_params.useSpecialIndex, m_params.mode);
    micromapAS->createAndBuild(vkd, device, cmdBuffer, alloc, 0);

    queryPoolCompact      = makeQueryPool(vkd, device, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, 1);
    queryPoolSerilization = makeQueryPool(vkd, device, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR, 1);

    queryAccelerationStructureSize(vkd, device, cmdBuffer, {*micromapAS->getPtr()},
                                   VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, *queryPoolCompact,
                                   VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, 0u, micromapCompactSize);
    queryAccelerationStructureSize(vkd, device, cmdBuffer, {*micromapAS->getPtr()},
                                   VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, *queryPoolSerilization,
                                   VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR, 0u,
                                   micromapSerilizationSize);

    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    // query result
    VK_CHECK(vkd.getQueryPoolResults(device, *queryPoolCompact, 0u, 1, sizeof(VkDeviceSize), micromapCompactSize.data(),
                                     sizeof(VkDeviceSize), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));

    VK_CHECK(vkd.getQueryPoolResults(device, *queryPoolSerilization, 0u, 1, sizeof(VkDeviceSize),
                                     micromapSerilizationSize.data(), sizeof(VkDeviceSize),
                                     VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));

    vkd.resetCommandPool(device, *cmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);

    beginCommandBuffer(vkd, cmdBuffer);

    // Compact
    micromapASCompactCopy->createAndCopyFrom(vkd, device, cmdBuffer, alloc, micromapAS.get(), micromapCompactSize[0]);

    vkd.cmdResetQueryPool(cmdBuffer, *queryPoolSerilization, 0u, 1);

    queryAccelerationStructureSize(vkd, device, cmdBuffer, {*micromapASCompactCopy->getPtr()},
                                   VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, *queryPoolSerilization,
                                   VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR, 0u,
                                   micromapCompactSerilizationSize);

    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    VK_CHECK(vkd.getQueryPoolResults(device, *queryPoolSerilization, 0u, 1, sizeof(VkDeviceSize),
                                     micromapCompactSerilizationSize.data(), sizeof(VkDeviceSize),
                                     VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
    vkd.resetCommandPool(device, *cmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);

    beginCommandBuffer(vkd, cmdBuffer);

    // serialize
    de::SharedPtr<SerialStorage> compactSerializeStorage(new SerialStorage(
        vkd, device, alloc, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, micromapCompactSerilizationSize[0]));
    micromapASCompactCopy->serialize(vkd, device, cmdBuffer, compactSerializeStorage.get());

    de::SharedPtr<SerialStorage> serializeStorage(new SerialStorage(
        vkd, device, alloc, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, micromapSerilizationSize[0]));
    micromapAS->serialize(vkd, device, cmdBuffer, serializeStorage.get());
    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    bool result             = true;
    const uint8_t *s_header = static_cast<const uint8_t *>(compactSerializeStorage->getHostAddressConst().hostAddress);
    const uint8_t *c_header = static_cast<const uint8_t *>(serializeStorage->getHostAddressConst().hostAddress);
    const auto s_compatibility = getDeviceASCompatibilityKHR(s_header);
    const auto c_compatibility = getDeviceASCompatibilityKHR(c_header);

    result &= ((s_compatibility == c_compatibility) &&
               (s_compatibility == VK_ACCELERATION_STRUCTURE_COMPATIBILITY_COMPATIBLE_KHR));
    if (!result)
    {
        tcu::TestLog &log = m_context.getTestContext().getLog();

        log << tcu::TestLog::Message << getUUIDsString(s_header) << " serialized AS compatibility failed"
            << tcu::TestLog::EndMessage;
        log << tcu::TestLog::Message << getUUIDsString(c_header) << " compact AS compatibility failed"
            << tcu::TestLog::EndMessage;
    }
    if (!result)
        TCU_FAIL("Unexpected values found in output buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

class RayTracingHeaderMicromapAddressTestCase : public TestCase
{
public:
    RayTracingHeaderMicromapAddressTestCase(tcu::TestContext &testCtx, const std::string &name,
                                            const TestParams &params);
    virtual ~RayTracingHeaderMicromapAddressTestCase(void)
    {
    }

    virtual void checkSupport(Context &context) const;
    virtual TestInstance *createInstance(Context &context) const;

protected:
    TestParams m_params;
};

class RayTracingHeaderMicromapAddressInstance : public TestInstance
{
public:
    RayTracingHeaderMicromapAddressInstance(Context &context, const TestParams &params);
    virtual ~RayTracingHeaderMicromapAddressInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void);
    bool areAddressesTheSame(const std::vector<uint64_t> &addresses,
                             const SerialStorage::AccelerationStructureHeader *header);

    bool areAddressesDifferent(const std::vector<uint64_t> &addresses1, const std::vector<uint64_t> &addresses2);

protected:
    TestParams m_params;
};

RayTracingHeaderMicromapAddressTestCase::RayTracingHeaderMicromapAddressTestCase(tcu::TestContext &testCtx,
                                                                                 const std::string &name,
                                                                                 const TestParams &params)
    : TestCase(testCtx, name)
    , m_params(params)
{
}

void RayTracingHeaderMicromapAddressTestCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
    context.requireDeviceFunctionality("VK_KHR_opacity_micromap");
    context.requireDeviceFunctionality("VK_KHR_device_address_commands");

    const VkPhysicalDeviceAccelerationStructureFeaturesKHR &accelerationStructureFeaturesKHR =
        context.getAccelerationStructureFeatures();
    if (accelerationStructureFeaturesKHR.accelerationStructure == false)
        TCU_THROW(TestError, "Requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");

    const VkPhysicalDeviceOpacityMicromapFeaturesKHR &opacityMicromapFeaturesKHR = context.getOpacityMicromapFeatures();
    if (opacityMicromapFeaturesKHR.micromap == false)
        TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceOpacityMicromapFeaturesKHR.micromap");

    const VkPhysicalDeviceDeviceAddressCommandsFeaturesKHR &deviceAddressCommandsFeaturesKHR =
        context.getDeviceAddressCommandsFeatures();
    if (deviceAddressCommandsFeaturesKHR.deviceAddressCommands == false)
        TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceDeviceAddressCommandsFeaturesKHR.deviceAddressCommands");
}

TestInstance *RayTracingHeaderMicromapAddressTestCase::createInstance(Context &context) const
{
    return new RayTracingHeaderMicromapAddressInstance(context, m_params);
}

RayTracingHeaderMicromapAddressInstance::RayTracingHeaderMicromapAddressInstance(Context &context,
                                                                                 const TestParams &params)
    : TestInstance(context)
    , m_params(params)
{
}

tcu::TestStatus RayTracingHeaderMicromapAddressInstance::iterate(void)
{
    const auto &vkd   = m_context.getDeviceInterface();
    const auto device = m_context.getDevice();
    auto &alloc       = m_context.getDefaultAllocator();
    const auto qIndex = m_context.getUniversalQueueFamilyIndex();
    const auto queue  = m_context.getUniversalQueue();

    // Command pool and buffer.
    const auto cmdPool      = makeCommandPool(vkd, device, qIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    beginCommandBuffer(vkd, cmdBuffer, 0u);

    // Build acceleration structures.
    auto bottomLevelAS                                = makeBottomLevelAccelerationStructure();
    auto micromapAS1                                  = makeMicromapAccelerationStructure();
    de::MovePtr<BottomLevelAccelerationStructure> dst = makeBottomLevelAccelerationStructure();

    AccelerationStructBufferProperties bufferProps; // add it for micromap
    bufferProps.props.residency = ResourceResidency::TRADITIONAL;

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
    // Attach the micromap data
    micromapAS1->setUseMaintenance5(m_params.useMaintenance5);
    micromapAS1->addOpacityMicromap(
        opacityMicromapData, triangleCount, m_params.subdivisionLevel,
        (m_params.mode == 2 ? VK_OPACITY_MICROMAP_FORMAT_2_STATE_KHR : VK_OPACITY_MICROMAP_FORMAT_4_STATE_KHR),
        m_params.useSpecialIndex, m_params.mode);
    micromapAS1->createAndBuild(vkd, device, cmdBuffer, alloc, 0);

    // Attach the micromap to the geometry
    VkAccelerationStructureTrianglesOpacityMicromapKHR opacityGeometryMicromap = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_OPACITY_MICROMAP_KHR, //VkStructureType             sType;
        nullptr,                                                                 // void*                      pNext;
        VK_INDEX_TYPE_UINT32,                         // VkIndexType                indexType;
        micromapAS1->getIndexBufferAddr(vkd, device), // VkDeviceAddress            indexBuffer;
        0,                                            // VkDeviceSize               indexStride;
        (m_params.nonZeroBase ? 1u : 0u),             // uint32_t                   baseTriangle;
        *micromapAS1->getPtr(),                       // VkAccelerationStructureKHR micromap;
    };

    const std::vector<tcu::Vec3> triangle = {
        tcu::Vec3(0.0f, 0.0f, 0.0f),
        tcu::Vec3(1.0f, 0.0f, 0.0f),
        tcu::Vec3(0.0f, 1.0f, 0.0f),
    };
    de::SharedPtr<MicromapAccelerationStructure> omm1(micromapAS1.release());
    bottomLevelAS->setBuildType(VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR);
    bottomLevelAS->addGeometry(triangle, true /*is triangles*/, 0, nullptr, nullptr, &opacityGeometryMicromap, omm1);
    bottomLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc, bufferProps);
    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    de::SharedPtr<BottomLevelAccelerationStructure> src(bottomLevelAS.release());
    const std::vector<uint64_t> inAddrs     = src->getSerializingAddresses(vkd, device);
    const std::vector<VkDeviceSize> inSizes = src->getSerializingSizes(vkd, device, queue, qIndex);
    const SerialInfo serialInfo(inAddrs, inSizes);
    SerialStorage deepStorage(vkd, device, alloc, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, serialInfo);

    // make deep serialization
    vkd.resetCommandBuffer(cmdBuffer, 0);
    beginCommandBuffer(vkd, cmdBuffer);
    src->serialize(vkd, device, cmdBuffer, &deepStorage);
    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    // deserialize all from the previous step to a new bottom-level AS
    // micromap structure addresses should be updated when deep data is deserialized
    vkd.resetCommandBuffer(cmdBuffer, 0);
    beginCommandBuffer(vkd, cmdBuffer);
    dst->setBuildType(VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR);
    dst->createAndDeserializeFrom(vkd, device, cmdBuffer, alloc, bufferProps, &deepStorage);
    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    SerialStorage shallowStorage(vkd, device, alloc, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, inSizes[0]);

    // make shallow serialization - only bottom-level AS without micromap structures
    vkd.resetCommandBuffer(cmdBuffer, 0);
    beginCommandBuffer(vkd, cmdBuffer);
    dst->serialize(vkd, device, cmdBuffer, &shallowStorage);
    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    // get data to verification
    const std::vector<uint64_t> outAddrs                     = dst->getSerializingAddresses(vkd, device);
    const SerialStorage::AccelerationStructureHeader *header = shallowStorage.getASHeader();

    return (areAddressesDifferent(inAddrs, outAddrs) && areAddressesTheSame(outAddrs, header)) ?
               tcu::TestStatus::pass("") :
               tcu::TestStatus::fail("");
}

bool RayTracingHeaderMicromapAddressInstance::areAddressesTheSame(
    const std::vector<uint64_t> &addresses, const SerialStorage::AccelerationStructureHeader *header)
{
    const uint32_t cmicromaps = uint32_t(addresses.size() - 1);
    std::set<uint64_t> refAddrs;
    std::set<uint64_t> checkAddrs;

    // header should contain the same number of handles as serialized/deserialized bottom-level AS
    uint64_t micromapsHandleCount = 0;
    uint32_t serializedBlockCount = static_cast<uint32_t>(header->blockCount & 0xFFFFFFFF);

    DE_ASSERT((header->blockCount >> 32) == 0xFFFFFFFF);
    for (uint32_t i = 0; i < serializedBlockCount; i++)
    {
        // multiple blocks can have same type
        if (header->blocks[i].blockType ==
            static_cast<uint32_t>(VK_ACCELERATION_STRUCTURE_SERIALIZED_BLOCK_TYPE_OPACITY_MICROMAP_KHR))
        {
            micromapsHandleCount += header->blocks[i].blockHandleCount;
            for (uint32_t handleIdx = 0; handleIdx < header->blocks[i].blockHandleCount; handleIdx++)
            {
                checkAddrs.insert(header->blocks[i].blockHandles[handleIdx]);
            }
        }
    }
    if (cmicromaps != micromapsHandleCount)
        return false;

    // distinct, squash and sort address list
    for (uint32_t i = 0; i < cmicromaps; ++i)
    {
        refAddrs.insert(addresses[i + 1]);
    }

    return std::equal(refAddrs.begin(), refAddrs.end(), checkAddrs.begin());
}

bool RayTracingHeaderMicromapAddressInstance::areAddressesDifferent(const std::vector<uint64_t> &addresses1,
                                                                    const std::vector<uint64_t> &addresses2)
{
    // the number of addresses must be equal
    if (addresses1.size() != addresses2.size())
        return false;

    // addresses of top-level AS must differ
    if (addresses1[0] == addresses2[0])
        return false;

    std::set<uint64_t> addrs1;
    std::set<uint64_t> addrs2;
    uint32_t matches          = 0;
    const uint32_t cmicromaps = uint32_t(addresses1.size() - 1);

    for (uint32_t i = 0; i < cmicromaps; ++i)
    {
        addrs1.insert(addresses1[i + 1]);
        addrs2.insert(addresses2[i + 1]);
    }

    // the first addresses set must not contain any address from the second addresses set
    for (auto &addr1 : addrs1)
    {
        if (addrs2.end() != addrs2.find(addr1))
            ++matches;
    }

    return (matches == 0);
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
    context.requireDeviceFunctionality("VK_KHR_ray_query");
    context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
    context.requireDeviceFunctionality("VK_KHR_opacity_micromap");
    context.requireDeviceFunctionality("VK_KHR_device_address_commands");

    if (m_params.useMaintenance5)
        context.requireDeviceFunctionality("VK_KHR_maintenance5");

    const VkPhysicalDeviceRayQueryFeaturesKHR &rayQueryFeaturesKHR = context.getRayQueryFeatures();
    if (rayQueryFeaturesKHR.rayQuery == false)
        TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayQueryFeaturesKHR.rayQuery");

    const VkPhysicalDeviceAccelerationStructureFeaturesKHR &accelerationStructureFeaturesKHR =
        context.getAccelerationStructureFeatures();
    if (accelerationStructureFeaturesKHR.accelerationStructure == false)
        TCU_THROW(TestError,
                  "VK_KHR_ray_query requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");

    const VkPhysicalDeviceOpacityMicromapFeaturesKHR &opacityMicromapFeaturesKHR = context.getOpacityMicromapFeatures();
    if (opacityMicromapFeaturesKHR.micromap == false)
        TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceOpacityMicromapFeaturesKHR.micromap");

    const VkPhysicalDeviceDeviceAddressCommandsFeaturesKHR &deviceAddressCommandsFeaturesKHR =
        context.getDeviceAddressCommandsFeatures();
    if (deviceAddressCommandsFeaturesKHR.deviceAddressCommands == false)
        TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceDeviceAddressCommandsFeaturesKHR.deviceAddressCommands");

    if (m_params.shaderSourceType == SST_RAY_GENERATION_SHADER)
    {
        context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

        const VkPhysicalDeviceRayTracingPipelineFeaturesKHR &rayTracingPipelineFeaturesKHR =
            context.getRayTracingPipelineFeatures();

        if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == false)
            TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");
    }

    switch (m_params.shaderSourceType)
    {
    case SST_VERTEX_SHADER:
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS);
        break;
    default:
        break;
    }

    const VkPhysicalDeviceOpacityMicromapPropertiesKHR &opacityMicromapPropertiesKHR =
        context.getOpacityMicromapProperties();

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

    std::ostringstream sharedHeader;
    sharedHeader << "#version 460 core\n"
                 << "#extension GL_EXT_ray_query : require\n"
                 << "#extension GL_EXT_opacity_micromap : require\n"
                 << "#extension GL_EXT_opacity_micromap_ray_query_mode : require\n"
                 << "\n"
                 << "layout(constant_id = " << kConstantID << ") gl_EnableOpacityMicromapEXT;"
                 << "\n"
                 << "layout(set=0, binding=0) uniform accelerationStructureEXT topLevelAS;\n"
                 << "layout(set=0, binding=1, std430) buffer RayOrigins {\n"
                 << "  vec4 values[" << numRays << "];\n"
                 << "} origins;\n"
                 << "layout(set=0, binding=2, std430) buffer OutputModes {\n"
                 << "  uint values[" << numRays << "];\n"
                 << "} modes;\n";

    std::ostringstream mainLoop;
    mainLoop
        << "  while (index < " << numRays << ") {\n"
        << "    const uint  cullMask  = 0xFF;\n"
        << "    const vec3  origin    = origins.values[index].xyz;\n"
        << "    const vec3  direction = vec3(0.0, 0.0, -1.0);\n"
        << "    const float tMin      = 0.0f;\n"
        << "    const float tMax      = 2.0f;\n"
        << "    uint        outputVal = 0;\n" // 0 for miss, 1 for non-opaque, 2 for opaque
        << "    rayQueryEXT rq;\n"
        << "    rayQueryInitializeEXT(rq, topLevelAS, " << flagsString
        << ", cullMask, origin, tMin, direction, tMax);\n"
        << "    while (rayQueryProceedEXT(rq)) {\n"
        << "      if (rayQueryGetIntersectionTypeEXT(rq, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {\n"
        << "        outputVal = 1;\n"
        << "      }\n"
        << "    }\n"
        << "    if (rayQueryGetIntersectionTypeEXT(rq, true) == gl_RayQueryCommittedIntersectionTriangleEXT) {\n"
        << "      outputVal = 2;\n"
        << "    }\n"
        << "    modes.values[index] = outputVal;\n"
        << "    index += " << kNumThreadsAtOnce << ";\n"
        << "  }\n";

    if (m_params.shaderSourceType == SST_VERTEX_SHADER)
    {
        std::ostringstream vert;
        vert << sharedHeader.str() << "void main()\n"
             << "{\n"
             << "  uint index             = gl_VertexIndex.x;\n"
             << mainLoop.str() << "  gl_PointSize = 1.0f;\n"
             << "}\n";

        programCollection.glslSources.add("vert") << glu::VertexSource(vert.str()) << buildOptions;
    }
    else if (m_params.shaderSourceType == SST_RAY_GENERATION_SHADER)
    {
        std::ostringstream rgen;
        rgen << sharedHeader.str() << "#extension GL_EXT_ray_tracing : require\n"
             << "void main()\n"
             << "{\n"
             << "  uint index             = gl_LaunchIDEXT.x;\n"
             << mainLoop.str() << "}\n";

        programCollection.glslSources.add("rgen")
            << glu::RaygenSource(updateRayTracingGLSL(rgen.str())) << buildOptions;
    }
    else
    {
        DE_ASSERT(m_params.shaderSourceType == SST_COMPUTE_SHADER);
        std::ostringstream comp;
        comp << sharedHeader.str() << "layout(local_size_x=" << kLocalSize << ", local_size_y=1, local_size_z=1) in;\n"
             << "\n"
             << "void main()\n"
             << "{\n"
             << "  uint index             = gl_GlobalInvocationID.x;\n"
             << mainLoop.str() << "}\n";

        programCollection.glslSources.add("comp")
            << glu::ComputeSource(updateRayTracingGLSL(comp.str())) << buildOptions;
    }
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

static Move<VkRenderPass> makeEmptyRenderPass(const DeviceInterface &vk, const VkDevice device)
{
    std::vector<VkSubpassDescription> subpassDescriptions;
    std::vector<VkSubpassDependency> subpassDependencies;

    const VkSubpassDescription description = {
        (VkSubpassDescriptionFlags)0,    //  VkSubpassDescriptionFlags flags;
        VK_PIPELINE_BIND_POINT_GRAPHICS, //  VkPipelineBindPoint pipelineBindPoint;
        0u,                              //  uint32_t inputAttachmentCount;
        nullptr,                         //  const VkAttachmentReference* pInputAttachments;
        0u,                              //  uint32_t colorAttachmentCount;
        nullptr,                         //  const VkAttachmentReference* pColorAttachments;
        nullptr,                         //  const VkAttachmentReference* pResolveAttachments;
        nullptr,                         //  const VkAttachmentReference* pDepthStencilAttachment;
        0,                               //  uint32_t preserveAttachmentCount;
        nullptr                          //  const uint32_t* pPreserveAttachments;
    };
    subpassDescriptions.push_back(description);

    const VkSubpassDependency dependency = {
        0u,                                   //  uint32_t srcSubpass;
        0u,                                   //  uint32_t dstSubpass;
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,  //  VkPipelineStageFlags srcStageMask;
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, //  VkPipelineStageFlags dstStageMask;
        VK_ACCESS_SHADER_WRITE_BIT,           //  VkAccessFlags srcAccessMask;
        VK_ACCESS_MEMORY_READ_BIT,            //  VkAccessFlags dstAccessMask;
        0u                                    //  VkDependencyFlags dependencyFlags;
    };
    subpassDependencies.push_back(dependency);

    const VkRenderPassCreateInfo renderPassInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,         //  VkStructureType sType;
        nullptr,                                           //  const void* pNext;
        static_cast<VkRenderPassCreateFlags>(0u),          //  VkRenderPassCreateFlags flags;
        0u,                                                //  uint32_t attachmentCount;
        nullptr,                                           //  const VkAttachmentDescription* pAttachments;
        static_cast<uint32_t>(subpassDescriptions.size()), //  uint32_t subpassCount;
        &subpassDescriptions[0],                           //  const VkSubpassDescription* pSubpasses;
        static_cast<uint32_t>(subpassDependencies.size()), //  uint32_t dependencyCount;
        subpassDependencies.size() > 0 ? &subpassDependencies[0] : nullptr //  const VkSubpassDependency* pDependencies;
    };

    return createRenderPass(vk, device, &renderPassInfo);
}

Move<VkPipeline> makeGraphicsPipeline(const DeviceInterface &vk, const VkDevice device,
                                      const VkPipelineLayout pipelineLayout, const VkRenderPass renderPass,
                                      const VkShaderModule vertexModule, const uint32_t subpass,
                                      const VkSpecializationInfo *vertexShaderSpecializationInfo)
{
    VkExtent2D renderSize{256, 256};
    VkViewport viewport = makeViewport(renderSize);
    VkRect2D scissor    = makeRect2D(renderSize);

    const VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType                             sType
        nullptr,                                               // const void*                                 pNext
        (VkPipelineViewportStateCreateFlags)0,                 // VkPipelineViewportStateCreateFlags          flags
        1u,        // uint32_t                                    viewportCount
        &viewport, // const VkViewport*                           pViewports
        1u,        // uint32_t                                    scissorCount
        &scissor   // const VkRect2D*                             pScissors
    };

    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType                            sType
        nullptr,                                                     // const void*                                pNext
        0u,                                                          // VkPipelineInputAssemblyStateCreateFlags    flags
        VK_PRIMITIVE_TOPOLOGY_POINT_LIST, // VkPrimitiveTopology                        topology
        VK_FALSE                          // VkBool32                                   primitiveRestartEnable
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, //  VkStructureType                                    sType
        nullptr,                                  //  const void*                                        pNext
        (VkPipelineVertexInputStateCreateFlags)0, //  VkPipelineVertexInputStateCreateFlags            flags
        0u,      //  uint32_t                                        vertexBindingDescriptionCount
        nullptr, //  const VkVertexInputBindingDescription*          pVertexBindingDescriptions
        0u,      //  uint32_t                                        vertexAttributeDescriptionCount
        nullptr, //  const VkVertexInputAttributeDescription*        pVertexAttributeDescriptions
    };

    const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, //  VkStructureType                            sType
        nullptr,                                                    //  const void*                                pNext
        0u,                                                         //  VkPipelineRasterizationStateCreateFlags    flags
        VK_FALSE,                        //  VkBool32                                   depthClampEnable
        VK_TRUE,                         //  VkBool32                                   rasterizerDiscardEnable
        VK_POLYGON_MODE_FILL,            //  VkPolygonMode                              polygonMode
        VK_CULL_MODE_NONE,               //  VkCullModeFlags                            cullMode
        VK_FRONT_FACE_COUNTER_CLOCKWISE, //  VkFrontFace                                frontFace
        VK_FALSE,                        //  VkBool32                                   depthBiasEnable
        0.0f,                            //  float                                      depthBiasConstantFactor
        0.0f,                            //  float                                      depthBiasClamp
        0.0f,                            //  float                                      depthBiasSlopeFactor
        1.0f                             //  float                                      lineWidth
    };

    return makeGraphicsPipeline(
        vk,                            // const DeviceInterface&                           vk
        device,                        // const VkDevice                                   device
        pipelineLayout,                // const VkPipelineLayout                           pipelineLayout
        vertexModule,                  // const VkShaderModule                             vertexShaderModule
        VK_NULL_HANDLE,                // const VkShaderModule                             tessellationControlModule
        VK_NULL_HANDLE,                // const VkShaderModule                             tessellationEvalModule
        VK_NULL_HANDLE,                // const VkShaderModule                             geometryShaderModule
        VK_NULL_HANDLE,                // const VkShaderModule                             fragmentShaderModule
        renderPass,                    // const VkRenderPass                               renderPass
        subpass,                       // const uint32_t                                   subpass
        &vertexInputStateCreateInfo,   // const VkPipelineVertexInputStateCreateInfo*      vertexInputStateCreateInfo
        &inputAssemblyStateCreateInfo, // const VkPipelineInputAssemblyStateCreateInfo*    inputAssemblyStateCreateInfo
        nullptr,                       // const VkPipelineTessellationStateCreateInfo*     tessStateCreateInfo
        &viewportStateCreateInfo,      // const VkPipelineViewportStateCreateInfo*         viewportStateCreateInfo
        &rasterizationStateCreateInfo, // const VkPipelineRasterizationStateCreateInfo*    rasterizationStateCreateInfo
        nullptr,                       // const VkPipelineMultisampleStateCreateInfo*      multisampleStateCreateInfo,
        nullptr,                       // const VkPipelineDepthStencilStateCreateInfo*     depthStencilStateCreateInfo,
        nullptr,                       // const VkPipelineColorBlendStateCreateInfo*       colorBlendStateCreateInfo,
        nullptr,                       // const VkPipelineDynamicStateCreateInfo*          dynamicStateCreateInfo
        nullptr,                       // const void*                                      pNext,
        0,                             // const VkPipelineCreateFlags                      pipelineCreateFlags,
        nullptr,                       // const void*                                      stagePNext,
        vertexShaderSpecializationInfo); // const VkSpecializationInfo*                      vertexShaderSpecializationInfo
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
    const auto &vkd   = m_context.getDeviceInterface();
    const auto device = m_context.getDevice();
    auto &alloc       = m_context.getDefaultAllocator();
    const auto qIndex = m_context.getUniversalQueueFamilyIndex();
    const auto queue  = m_context.getUniversalQueue();

    // Command pool and buffer.
    const auto cmdPool      = makeCommandPool(vkd, device, qIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    beginCommandBuffer(vkd, cmdBuffer);

    // Build acceleration structures.
    auto topLevelAS       = makeTopLevelAccelerationStructure();
    auto bottomLevelAS    = makeBottomLevelAccelerationStructure();
    auto micromapAS       = makeMicromapAccelerationStructure();
    auto micromapASCopy   = makeMicromapAccelerationStructure();
    auto micromapASShadow = makeMicromapAccelerationStructure();
    micromapAS->setUseMaintenance5(m_params.useMaintenance5);
    micromapASCopy->setUseMaintenance5(m_params.useMaintenance5);
    std::vector<de::SharedPtr<SerialStorage>> micromapSerialized;

    AccelerationStructBufferProperties bufferProps; // add it for micromap
    bufferProps.props.residency = ResourceResidency::TRADITIONAL;

    VkBufferUsageFlags2CreateInfoKHR bufferUsageFlags2 = initVulkanStructure();

    const auto triangleCount       = (m_params.nonZeroBase ? 2u : 1u);
    uint32_t numSubtriangles       = levelToSubtriangles(m_params.subdivisionLevel);
    uint32_t triangleMicromapBytes = (m_params.mode == 2) ? (numSubtriangles + 7) / 8 : (numSubtriangles + 3) / 4;
    uint32_t opacityMicromapBytes  = triangleMicromapBytes * triangleCount;
    VkBuildAccelerationStructureFlagsKHR micromapBuildFlag = VkBuildAccelerationStructureFlagsKHR(0);
    VkBuildAccelerationStructureFlagsKHR blasBuildFlag     = VkBuildAccelerationStructureFlagsKHR(0);

    // Generate random micromap data
    std::vector<uint8_t> opacityMicromapData;
    opacityMicromapData.reserve(opacityMicromapBytes);

    de::Random rnd(m_params.seed);

    while (opacityMicromapData.size() < opacityMicromapBytes)
    {
        opacityMicromapData.push_back(rnd.getUint8());
    }

    if (m_params.copyType == CT_COMPACT)
    {
        micromapBuildFlag |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
    }

    if (m_params.lossy)
    {
        micromapBuildFlag |= VK_BUILD_ACCELERATION_STRUCTURE_MICROMAP_LOSSY_BIT_KHR;
    }

    micromapAS->setBuildFlags(micromapBuildFlag);
    // Attach the micromap data
    micromapAS->addOpacityMicromap(
        opacityMicromapData, triangleCount, m_params.subdivisionLevel,
        (m_params.mode == 2 ? VK_OPACITY_MICROMAP_FORMAT_2_STATE_KHR : VK_OPACITY_MICROMAP_FORMAT_4_STATE_KHR),
        m_params.useSpecialIndex, m_params.mode);
    micromapAS->createAndBuild(vkd, device, cmdBuffer, alloc, 0);

    if (m_params.copyType != CT_NONE)
    {
        std::vector<VkDeviceSize> micromapCompactSize(1);
        std::vector<VkDeviceSize> micromapSerializationSize(1);
        Move<VkQueryPool> queryPoolCompact;
        Move<VkQueryPool> queryPoolSerialization;
        if (m_params.copyType == CT_COMPACT)
        {
            queryPoolCompact = makeQueryPool(vkd, device, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, 1);

            queryAccelerationStructureSize(
                vkd, device, cmdBuffer, {*micromapAS->getPtr()}, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                *queryPoolCompact, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, 0u, micromapCompactSize);

            endCommandBuffer(vkd, cmdBuffer);
            submitCommandsAndWait(vkd, device, queue, cmdBuffer);

            VK_CHECK(vkd.getQueryPoolResults(device, *queryPoolCompact, 0u, 1, sizeof(VkDeviceSize),
                                             micromapCompactSize.data(), sizeof(VkDeviceSize),
                                             VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));

            vkd.resetCommandPool(device, *cmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
            beginCommandBuffer(vkd, cmdBuffer);
        }
        if (m_params.copyType == CT_SERIALIZE)
        {
            queryPoolSerialization =
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
        }

        if ((m_params.copyType == CT_CLONE) || (m_params.copyType == CT_COMPACT))
        {
            micromapASCopy->createAndCopyFrom(vkd, device, cmdBuffer, alloc, micromapAS.get(), micromapCompactSize[0]);
        }
        else
        {
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
             ((m_params.copyType == CT_NONE) ? *micromapAS->getPtr() :
                                               *micromapASCopy->getPtr())), // VkAccelerationStructureKHR micromap;
    };

    const std::vector<tcu::Vec3> triangle = {
        tcu::Vec3(0.0f, 0.0f, 0.0f),
        tcu::Vec3(1.0f, 0.0f, 0.0f),
        tcu::Vec3(0.0f, 1.0f, 0.0f),
    };

    if (m_params.testFlagMask & TEST_FLAG_BIT_DISABLE_OPACITY_MICROMAP_INSTANCE)
    {
        blasBuildFlag |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DISABLE_OPACITY_MICROMAPS_BIT_KHR;
    }

    if (m_params.update)
    {
        blasBuildFlag |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
        blasBuildFlag |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_OPACITY_MICROMAP_UPDATE_BIT_KHR;
    }

    bottomLevelAS->addGeometry(triangle, true /*is triangles*/, 0, nullptr, nullptr, &opacityGeometryMicromap);
    bottomLevelAS->setBuildFlags(blasBuildFlag);
    bottomLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc, bufferProps);

    if (m_params.update)
    {
        // invers all bits
        for (uint32_t i = 0; i < opacityMicromapData.size(); ++i)
        {
            opacityMicromapData[i] = ~opacityMicromapData[i];
        }

        micromapASShadow->setBuildFlags(micromapBuildFlag);
        // Attach the micromap data
        micromapASShadow->addOpacityMicromap(
            opacityMicromapData, triangleCount, m_params.subdivisionLevel,
            (m_params.mode == 2 ? VK_OPACITY_MICROMAP_FORMAT_2_STATE_KHR : VK_OPACITY_MICROMAP_FORMAT_4_STATE_KHR),
            m_params.useSpecialIndex, m_params.mode);
        micromapASShadow->createAndBuild(vkd, device, cmdBuffer, alloc, 0);

        // Update Bottom Level Structure
        opacityGeometryMicromap.micromap = *micromapASShadow->getPtr();
        bottomLevelAS->setOpacityMicromap(0, &opacityGeometryMicromap);
        bottomLevelAS->build(vkd, device, cmdBuffer, bottomLevelAS.get());
    }

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
    auto originsBufferInfo       = makeBufferCreateInfo(originsBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    if (m_params.useMaintenance5)
    {
        bufferUsageFlags2.usage = (VkBufferUsageFlagBits2KHR)originsBufferInfo.usage;
        originsBufferInfo.pNext = &bufferUsageFlags2;
        originsBufferInfo.usage = 0;
    }
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
    dsLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_ALL);
    dsLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL);
    dsLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL);
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

    Move<VkPipeline> pipeline;
    de::MovePtr<BufferWithMemory> raygenSBT;
    Move<VkRenderPass> renderPass;
    Move<VkFramebuffer> framebuffer;

    VkSpecializationMapEntry mapEntry = {
        kConstantID,     // constantID
        0,               // offset
        sizeof(VkBool32) // size
    };
    VkBool32 enable                               = VK_TRUE;
    const VkSpecializationInfo specializationInfo = {
        1,              // mapEntryCount
        &mapEntry,      // pMapEntries
        sizeof(enable), // dataSize
        &enable,        // pData
    };

    if (m_params.shaderSourceType == SST_VERTEX_SHADER)
    {
        auto vertexModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0);

        renderPass  = makeEmptyRenderPass(vkd, device);
        framebuffer = makeFramebuffer(vkd, device, *renderPass, 0u, nullptr, 32, 32);
        pipeline =
            makeGraphicsPipeline(vkd, device, *pipelineLayout, *renderPass, *vertexModule, 0, &specializationInfo);

        beginRenderPass(vkd, cmdBuffer, *renderPass, *framebuffer, makeRect2D(32u, 32u));
        vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
        vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u,
                                  &descriptorSet.get(), 0u, nullptr);
        vkd.cmdDraw(cmdBuffer, kNumThreadsAtOnce, 1, 0, 0);
        endRenderPass(vkd, cmdBuffer);
    }
    else if (m_params.shaderSourceType == SST_RAY_GENERATION_SHADER)
    {
        const auto &vki    = m_context.getInstanceInterface();
        const auto physDev = m_context.getPhysicalDevice();

        // Shader module.
        auto rgenModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("rgen"), 0);

        // Get some ray tracing properties.
        uint32_t shaderGroupHandleSize    = 0u;
        uint32_t shaderGroupBaseAlignment = 1u;
        {
            const auto rayTracingPropertiesKHR = makeRayTracingProperties(vki, physDev);
            shaderGroupHandleSize              = rayTracingPropertiesKHR->getShaderGroupHandleSize();
            shaderGroupBaseAlignment           = rayTracingPropertiesKHR->getShaderGroupBaseAlignment();
        }

        auto raygenSBTRegion = makeStridedDeviceAddressRegionKHR(0, 0, 0);
        auto unusedSBTRegion = makeStridedDeviceAddressRegionKHR(0, 0, 0);

        {
            const auto rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

            rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgenModule, 0, &specializationInfo);

            pipeline = rayTracingPipeline->createPipeline(vkd, device, pipelineLayout.get());

            raygenSBT = rayTracingPipeline->createShaderBindingTable(
                vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
            raygenSBTRegion = makeStridedDeviceAddressRegionKHR(
                getBufferDeviceAddress(vkd, device, raygenSBT->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
        }

        // Trace rays.
        vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.get());
        vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout.get(), 0u, 1u,
                                  &descriptorSet.get(), 0u, nullptr);
        vkd.cmdTraceRaysKHR(cmdBuffer, &raygenSBTRegion, &unusedSBTRegion, &unusedSBTRegion, &unusedSBTRegion,
                            kNumThreadsAtOnce, 1u, 1u);
    }
    else
    {
        DE_ASSERT(m_params.shaderSourceType == SST_COMPUTE_SHADER);
        // Shader module.
        const auto compModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"), 0);

        // Pipeline.
        const VkPipelineShaderStageCreateInfo shaderInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            0u,                                                  // VkPipelineShaderStageCreateFlags flags;
            VK_SHADER_STAGE_COMPUTE_BIT,                         // VkShaderStageFlagBits stage;
            compModule.get(),                                    // VkShaderModule module;
            "main",                                              // const char* pName;
            &specializationInfo,                                 // const VkSpecializationInfo* pSpecializationInfo;
        };
        const VkComputePipelineCreateInfo pipelineInfo = {
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                        // const void* pNext;
            0u,                                             // VkPipelineCreateFlags flags;
            shaderInfo,                                     // VkPipelineShaderStageCreateInfo stage;
            pipelineLayout.get(),                           // VkPipelineLayout layout;
            VK_NULL_HANDLE,                                 // VkPipeline basePipelineHandle;
            0,                                              // int32_t basePipelineIndex;
        };
        pipeline = createComputePipeline(vkd, device, VK_NULL_HANDLE, &pipelineInfo);

        // Dispatch work with ray queries.
        vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.get());
        vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout.get(), 0u, 1u,
                                  &descriptorSet.get(), 0u, nullptr);
        vkd.cmdDispatch(cmdBuffer, kWorkGroupCount, 1u, 1u);
    }

    // Barrier for the output buffer.
    const auto bufferBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
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

void addBasicTestsKHR(tcu::TestCaseGroup *group)
{
    uint32_t seed = 1614674687u;

    const struct
    {
        ShaderSourceType shaderSourceType;
        ShaderSourcePipeline shaderSourcePipeline;
        std::string name;
    } shaderSourceTypes[] = {
        {SST_VERTEX_SHADER, SSP_GRAPHICS_PIPELINE, "vertex_shader"},
        {
            SST_COMPUTE_SHADER,
            SSP_COMPUTE_PIPELINE,
            "compute_shader",
        },
        {
            SST_RAY_GENERATION_SHADER,
            SSP_RAY_TRACING_PIPELINE,
            "rgen_shader",
        },
    };

    const struct
    {
        bool useSpecialIndex;
        std::string name;
    } specialIndexUse[] = {
        {false, "map_value"},
        {true, "special_index"},
    };

    auto &testCtx = group->getTestContext();

    for (size_t shaderSourceNdx = 0; shaderSourceNdx < DE_LENGTH_OF_ARRAY(shaderSourceTypes); ++shaderSourceNdx)
    {
        de::MovePtr<tcu::TestCaseGroup> sourceTypeGroup(
            new tcu::TestCaseGroup(group->getTestContext(), shaderSourceTypes[shaderSourceNdx].name.c_str()));

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
                new tcu::TestCaseGroup(sourceTypeGroup->getTestContext(), maskName.c_str()));

            for (size_t specialIndexNdx = 0; specialIndexNdx < DE_LENGTH_OF_ARRAY(specialIndexUse); ++specialIndexNdx)
            {
                de::MovePtr<tcu::TestCaseGroup> specialGroup(new tcu::TestCaseGroup(
                    testFlagGroup->getTestContext(), specialIndexUse[specialIndexNdx].name.c_str()));

                if (specialIndexUse[specialIndexNdx].useSpecialIndex)
                {

                    for (uint32_t specialIndex = 0; specialIndex < 4; specialIndex++)
                    {
                        TestParams testParams{
                            shaderSourceTypes[shaderSourceNdx].shaderSourceType,
                            shaderSourceTypes[shaderSourceNdx].shaderSourcePipeline,
                            specialIndexUse[specialIndexNdx].useSpecialIndex,
                            false,
                            false,
                            false,
                            false,
                            testFlagMask,
                            0,
                            ~specialIndex,
                            seed++,
                            CT_NONE,
                            false,
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
                                        shaderSourceTypes[shaderSourceNdx].shaderSourceType,
                                        shaderSourceTypes[shaderSourceNdx].shaderSourcePipeline,
                                        specialIndexUse[specialIndexNdx].useSpecialIndex,
                                        false,
                                        false,
                                        lossyModes[lossyModeNdx].lossy,
                                        false,
                                        testFlagMask,
                                        level,
                                        modes[modeNdx].mode,
                                        seed++,
                                        CT_NONE,
                                        false,
                                    };

                                    std::stringstream css;
                                    css << "level_" << level;
                                    const auto testName = css.str();

                                    lossyModeGroup->addChild(new OpacityMicromapCase(testCtx, testName, testParams));

                                    if (testFlagMask == 0u)
                                    {
                                        testParams.nonZeroBase = true;
                                        const auto variantName = testName + "_non_zero_base";
                                        lossyModeGroup->addChild(
                                            new OpacityMicromapCase(testCtx, variantName, testParams));
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
                                    shaderSourceTypes[shaderSourceNdx].shaderSourceType,
                                    shaderSourceTypes[shaderSourceNdx].shaderSourcePipeline,
                                    specialIndexUse[specialIndexNdx].useSpecialIndex,
                                    false,
                                    false,
                                    false,
                                    false,
                                    testFlagMask,
                                    level,
                                    modes[modeNdx].mode,
                                    seed++,
                                    CT_NONE,
                                    false,
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

            sourceTypeGroup->addChild(testFlagGroup.release());
        }

        group->addChild(sourceTypeGroup.release());
    }
}

void addCopyTestsKHR(tcu::TestCaseGroup *group)
{
    uint32_t seed = 1614674688u;

    auto &testCtx = group->getTestContext();

    for (size_t copyTypeNdx = CT_FIRST_ACTIVE; copyTypeNdx < CT_NUM_COPY_TYPES; ++copyTypeNdx)
    {
        de::MovePtr<tcu::TestCaseGroup> copyTypeGroup(
            new tcu::TestCaseGroup(group->getTestContext(), copyTypeNames[copyTypeNdx].c_str()));

        struct
        {
            uint32_t mode;
            std::string name;
        } modes[] = {{2, "2"}, {4, "4"}};
        for (uint32_t modeNdx = 0; modeNdx < DE_LENGTH_OF_ARRAY(modes); ++modeNdx)
        {
            de::MovePtr<tcu::TestCaseGroup> modeGroup(
                new tcu::TestCaseGroup(copyTypeGroup->getTestContext(), modes[modeNdx].name.c_str()));

            for (uint32_t level = 0; level <= kMaxSubdivisionLevel; level++)
            {
                TestParams testParams{
                    SST_COMPUTE_SHADER,
                    SSP_COMPUTE_PIPELINE,
                    false,
                    false,
                    false,
                    false,
                    false,
                    0,
                    level,
                    modes[modeNdx].mode,
                    seed++,
                    (CopyType)copyTypeNdx,
                    false,
                };

                std::stringstream css;
                css << "level_" << level;

                modeGroup->addChild(new OpacityMicromapCase(testCtx, css.str(), testParams));
            }
            copyTypeGroup->addChild(modeGroup.release());
        }
        group->addChild(copyTypeGroup.release());
    }

    {
        TestParams testParams{
            SST_COMPUTE_SHADER,
            SSP_COMPUTE_PIPELINE,
            false,
            false,
            false,
            false,
            false,
            0,
            0,
            2,
            1,
            CT_FIRST_ACTIVE,
            true,
        };
        de::MovePtr<tcu::TestCaseGroup> miscGroup(new tcu::TestCaseGroup(group->getTestContext(), "misc"));
        miscGroup->addChild(new OpacityMicromapCase(testCtx, "maintenance5", testParams));
        group->addChild(miscGroup.release());
    }
}

void addUpdateTestsKHR(tcu::TestCaseGroup *group)
{
    uint32_t seed = 1614674688u;

    auto &testCtx = group->getTestContext();

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
                SST_COMPUTE_SHADER,
                SSP_COMPUTE_PIPELINE,
                false,
                false,
                false,
                false,
                true,
                0,
                level,
                modes[modeNdx].mode,
                seed++,
                CT_NONE,
                false,
            };

            std::stringstream css;
            css << "level_" << level;

            modeGroup->addChild(new OpacityMicromapCase(testCtx, css.str(), testParams));
        }
        group->addChild(modeGroup.release());
    }
}

void addGetDeviceAccelerationStructureCompatibilityTests(tcu::TestCaseGroup *group)
{
    auto &testCtx = group->getTestContext();
    TestParams testParams{
        SST_COMPUTE_SHADER, SSP_COMPUTE_PIPELINE, false, false, false, false, false, 0, 2, 2, 1, CT_FIRST_ACTIVE, false,
    };
    group->addChild(new OpacityMicromapCompatibilityKHRTestCase(testCtx, "micromap", testParams));
}

void addUpdateHeaderMiromapAddressTests(tcu::TestCaseGroup *group)
{
    auto &testCtx = group->getTestContext();
    TestParams testParams{
        SST_COMPUTE_SHADER, SSP_COMPUTE_PIPELINE, false, false, false, false, false, 0, 2, 2, 1, CT_FIRST_ACTIVE, false,
    };
    group->addChild(new RayTracingHeaderMicromapAddressTestCase(testCtx, "micromap", testParams));
}

tcu::TestCaseGroup *createOpacityMicromapTestsKHR(tcu::TestContext &testCtx)
{
    // Test acceleration structures using opacity micromap with ray query
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "khr"));

    // Test accessing all formats of opacity micromaps
    addTestGroup(group.get(), "render", addBasicTestsKHR);
    // Test copying opacity micromaps
    addTestGroup(group.get(), "copy", addCopyTestsKHR);
    addTestGroup(group.get(), "update", addUpdateTestsKHR);
    addTestGroup(group.get(), "device_compatibility_khr", addGetDeviceAccelerationStructureCompatibilityTests);
    addTestGroup(group.get(), "header_micromap_address", addUpdateHeaderMiromapAddressTests);
    return group.release();
}

} // namespace RayQuery
} // namespace vkt
