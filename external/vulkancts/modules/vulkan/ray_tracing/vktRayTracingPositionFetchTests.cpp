/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
 * Copyright (c) 2022 NVIDIA Corporation.
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
 * \brief Ray Tracing Position Fetch Tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingPositionFetchTests.hpp"
#include "vktTestCase.hpp"

#include "vkRayTracingUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "deUniquePtr.hpp"
#include "deRandom.hpp"

#include "tcuVectorUtil.hpp"

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
    TEST_FLAG_BIT_INSTANCE_TRANSFORM = 1U << 0,
    TEST_FLAG_BIT_LAST               = 1U << 1
};

std::vector<std::string> testFlagBitNames = {
    "instance_transform",
};

struct TestParams
{
    vk::VkAccelerationStructureBuildTypeKHR buildType; // are we making AS on CPU or GPU
    VkFormat vertexFormat;
    uint32_t testFlagMask;

    uint32_t getRandomSeed() const
    {
        return (((buildType & 0xFF) << 24) | ((vertexFormat & 0xFF) << 16) | (testFlagMask & 0xFF));
    }
};

class PositionFetchCase : public TestCase
{
public:
    PositionFetchCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params);
    virtual ~PositionFetchCase(void)
    {
    }

    virtual void checkSupport(Context &context) const;
    virtual void initPrograms(vk::SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;

protected:
    TestParams m_params;
};

class PositionFetchInstance : public TestInstance
{
public:
    PositionFetchInstance(Context &context, const TestParams &params);
    virtual ~PositionFetchInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void);

protected:
    TestParams m_params;
};

PositionFetchCase::PositionFetchCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
    : TestCase(testCtx, name)
    , m_params(params)
{
}

void PositionFetchCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
    context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");
    context.requireDeviceFunctionality("VK_KHR_ray_tracing_position_fetch");

    const VkPhysicalDeviceAccelerationStructureFeaturesKHR &accelerationStructureFeaturesKHR =
        context.getAccelerationStructureFeatures();
    if (accelerationStructureFeaturesKHR.accelerationStructure == false)
        TCU_THROW(TestError,
                  "VK_KHR_ray_query requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");

    if (m_params.buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR &&
        accelerationStructureFeaturesKHR.accelerationStructureHostCommands == false)
        TCU_THROW(NotSupportedError,
                  "Requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructureHostCommands");

    const VkPhysicalDeviceRayTracingPositionFetchFeaturesKHR &rayTracingPositionFetchFeaturesKHR =
        context.getRayTracingPositionFetchFeatures();
    if (rayTracingPositionFetchFeaturesKHR.rayTracingPositionFetch == false)
        TCU_THROW(NotSupportedError, "Requires VkPhysicalDevicePositionFetchFeaturesKHR.rayTracingPositionFetch");

    // Check supported vertex format.
    checkAccelerationStructureVertexBufferFormat(context.getInstanceInterface(), context.getPhysicalDevice(),
                                                 m_params.vertexFormat);
}

void PositionFetchCase::initPrograms(vk::SourceCollections &programCollection) const
{
    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

    uint32_t numRays = 1; // XXX

    std::ostringstream layoutDecls;
    layoutDecls << "layout(set=0, binding=0) uniform accelerationStructureEXT topLevelAS;\n"
                << "layout(set=0, binding=1, std430) buffer RayOrigins {\n"
                << "  vec4 values[" << numRays << "];\n"
                << "} origins;\n"
                << "layout(set=0, binding=2, std430) buffer OutputPositions {\n"
                << "  vec4 values[" << 6 * numRays << "];\n"
                << "} modes;\n";
    const auto layoutDeclsStr = layoutDecls.str();

    std::ostringstream rgen;
    rgen << "#version 460 core\n"
         << "#extension GL_EXT_ray_tracing : require\n"
         << "#extension GL_EXT_ray_tracing_position_fetch : require\n"
         << "\n"
         << "layout(location=0) rayPayloadEXT int value;\n"
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
         << "  traceRayEXT(topLevelAS, gl_RayFlagsNoneEXT, cullMask, 0, 0, 0, origin, tMin, direction, tMax, 0);\n"
         << "}\n";

    std::ostringstream ah;
    ah << "#version 460 core\n"
       << "#extension GL_EXT_ray_tracing : require\n"
       << "#extension GL_EXT_ray_tracing_position_fetch : require\n"
       << "\n"
       << layoutDeclsStr << "\n"
       << "\n"
       << "void main()\n"
       << "{\n"
       << "  for (int i=0; i<3; i++) {\n"
       << "    modes.values[6*gl_LaunchIDEXT.x+2*i] = vec4(gl_HitTriangleVertexPositionsEXT[i], 0.0);\n"
       << "  }\n"
       << "  terminateRayEXT;\n"
       << "}\n";

    std::ostringstream ch;
    ch << "#version 460 core\n"
       << "#extension GL_EXT_ray_tracing : require\n"
       << "#extension GL_EXT_ray_tracing_position_fetch : require\n"
       << "\n"
       << layoutDeclsStr << "\n"
       << "layout(location=0) rayPayloadEXT int value;\n"
       << "\n"
       << "void main()\n"
       << "{\n"
       << "  for (int i=0; i<3; i++) {\n"
       << "    modes.values[6*gl_LaunchIDEXT.x+2*i+1] = vec4(gl_HitTriangleVertexPositionsEXT[i], 0);\n"
       << "  }\n"
       << "}\n";

    // Should never miss to fill in with sentinel values to cause a failure
    std::ostringstream miss;
    miss << "#version 460 core\n"
         << "#extension GL_EXT_ray_tracing : require\n"
         << layoutDeclsStr << "\n"
         << "layout(location=0) rayPayloadEXT int value;\n"
         << "\n"
         << "void main()\n"
         << "{\n"
         << "  for (int i=0; i<6; i++) {\n"
         << "    modes.values[6*gl_LaunchIDEXT.x+i] = vec4(123.0f, 456.0f, 789.0f, 0.0f);\n"
         << "  }\n"
         << "}\n";

    programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(rgen.str())) << buildOptions;
    programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(miss.str())) << buildOptions;
    programCollection.glslSources.add("ah") << glu::AnyHitSource(updateRayTracingGLSL(ah.str())) << buildOptions;
    programCollection.glslSources.add("ch") << glu::ClosestHitSource(updateRayTracingGLSL(ch.str())) << buildOptions;
}

TestInstance *PositionFetchCase::createInstance(Context &context) const
{
    return new PositionFetchInstance(context, m_params);
}

PositionFetchInstance::PositionFetchInstance(Context &context, const TestParams &params)
    : TestInstance(context)
    , m_params(params)
{
}

tcu::TestStatus PositionFetchInstance::iterate(void)
{
    const auto &vki    = m_context.getInstanceInterface();
    const auto physDev = m_context.getPhysicalDevice();
    const auto &vkd    = m_context.getDeviceInterface();
    const auto device  = m_context.getDevice();
    auto &alloc        = m_context.getDefaultAllocator();
    const auto qIndex  = m_context.getUniversalQueueFamilyIndex();
    const auto queue   = m_context.getUniversalQueue();
    const auto stages  = (VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
                         VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);

    // Command pool and buffer.
    const auto cmdPool      = makeCommandPool(vkd, device, qIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    beginCommandBuffer(vkd, cmdBuffer);

    // If we add anything to the command buffer here that the AS builds depend on make sure
    // to submit and wait when in CPU build mode

    // Build acceleration structures.
    auto topLevelAS    = makeTopLevelAccelerationStructure();
    auto bottomLevelAS = makeBottomLevelAccelerationStructure();

    AccelerationStructBufferProperties bufferProps;
    bufferProps.props.residency = ResourceResidency::TRADITIONAL;

    const std::vector<tcu::Vec3> triangle = {
        tcu::Vec3(0.0f, 0.0f, 0.0f),
        tcu::Vec3(1.0f, 0.0f, 0.0f),
        tcu::Vec3(0.0f, 1.0f, 0.0f),
    };

    const VkTransformMatrixKHR notQuiteIdentityMatrix3x4 = {
        {{0.98f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.97f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.99f, 0.0f}}};

    // The origin is at Z=1 and the direction is Z=-1, so the triangle needs to be at Z=0 to create a hit. To make
    // things more interesting, when the vertex format has a Z component we will use 4 geometries and 4 triangles per
    // geometry, but only 1 of them will be at Z=0. The rest will be at Z=10+N, where N is calculated based on the
    // geometry and triangle index. To be able to store those Z values, the vertex format needs to be sfloat.
    const auto vertexTcuFormat = mapVkFormat(m_params.vertexFormat);
    const bool multipleTriangles =
        ((tcu::getNumUsedChannels(vertexTcuFormat.order) >= 3) && isSfloatFormat(m_params.vertexFormat));
    const uint32_t zOffset = 10u;

    const auto seed = m_params.getRandomSeed();
    de::Random rnd(seed);

    uint32_t geometryCount = (multipleTriangles ? 4u : 1u);
    uint32_t triangleCount = (multipleTriangles ? 4u : 1u);
    uint32_t chosenGeom =
        (multipleTriangles ? static_cast<uint32_t>(rnd.getInt(1, static_cast<int>(geometryCount) - 1)) : 0u);
    uint32_t chosenTri =
        (multipleTriangles ? static_cast<uint32_t>(rnd.getInt(1, static_cast<int>(triangleCount) - 1)) : 0u);

    const auto getLargeZ = [&](uint32_t geomIndex, uint32_t triangleIndex) -> float
    { return static_cast<float>((triangleCount * geomIndex + triangleIndex) + zOffset); };

    for (uint32_t g = 0u; g < geometryCount; ++g)
    {
        de::SharedPtr<RaytracedGeometryBase> geometry =
            makeRaytracedGeometry(VK_GEOMETRY_TYPE_TRIANGLES_KHR, m_params.vertexFormat, VK_INDEX_TYPE_NONE_KHR);

        for (uint32_t t = 0u; t < triangleCount; ++t)
        {
            const auto z = ((g == chosenGeom && t == chosenTri) ? 0.0f : getLargeZ(g, t));
            for (const auto &v : triangle)
            {
                const tcu::Vec3 finalVertex(v.x(), v.y(), z);
                geometry->addVertex(finalVertex);
            }
        }

        bottomLevelAS->addGeometry(geometry);
    }

    bottomLevelAS->setBuildFlags(VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_KHR);
    bottomLevelAS->setBuildType(m_params.buildType);
    bottomLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc, bufferProps);
    de::SharedPtr<BottomLevelAccelerationStructure> blasSharedPtr(bottomLevelAS.release());

    topLevelAS->setInstanceCount(1);
    topLevelAS->setBuildType(m_params.buildType);
    topLevelAS->addInstance(blasSharedPtr, (m_params.testFlagMask & TEST_FLAG_BIT_INSTANCE_TRANSFORM) ?
                                               notQuiteIdentityMatrix3x4 :
                                               identityMatrix3x4);
    topLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc, bufferProps);

    // One ray for this test
    // XXX Should it be multiple triangles and one ray per triangle for more coverage?
    // XXX If it's really one ray, the origin buffer is complete overkill
    uint32_t numRays = 1; // XXX

    // SSBO buffer for origins.
    const auto originsBufferSize = static_cast<VkDeviceSize>(sizeof(tcu::Vec4) * numRays);
    const auto originsBufferInfo = makeBufferCreateInfo(originsBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory originsBuffer(vkd, device, alloc, originsBufferInfo, MemoryRequirement::HostVisible);
    auto &originsBufferAlloc = originsBuffer.getAllocation();
    void *originsBufferData  = originsBufferAlloc.getHostPtr();

    std::vector<tcu::Vec4> origins;
    std::vector<tcu::Vec3> expectedOutputPositions;
    origins.reserve(numRays);
    expectedOutputPositions.reserve(6 * numRays);

    // Fill in vector of expected outputs
    for (uint32_t index = 0; index < numRays; index++)
    {
        for (uint32_t vert = 0; vert < 3; vert++)
        {
            tcu::Vec3 pos = triangle[vert];

            // One from CH, one from AH
            expectedOutputPositions.push_back(pos);
            expectedOutputPositions.push_back(pos);
        }
    }

    // XXX Arbitrary location and see above
    for (uint32_t index = 0; index < numRays; index++)
    {
        origins.push_back(tcu::Vec4(0.25, 0.25, 1.0, 0.0));
    }

    const auto originsBufferSizeSz = static_cast<size_t>(originsBufferSize);
    deMemcpy(originsBufferData, origins.data(), originsBufferSizeSz);
    flushAlloc(vkd, device, originsBufferAlloc);

    // Storage buffer for output modes
    const auto outputPositionsBufferSize = static_cast<VkDeviceSize>(6 * 4 * sizeof(float) * numRays);
    const auto outputPositionsBufferInfo =
        makeBufferCreateInfo(outputPositionsBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory outputPositionsBuffer(vkd, device, alloc, outputPositionsBufferInfo,
                                           MemoryRequirement::HostVisible);
    auto &outputPositionsBufferAlloc = outputPositionsBuffer.getAllocation();
    void *outputPositionsBufferData  = outputPositionsBufferAlloc.getHostPtr();
    deMemset(outputPositionsBufferData, 0xFF, static_cast<size_t>(outputPositionsBufferSize));
    flushAlloc(vkd, device, outputPositionsBufferAlloc);

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
        const auto storageBufferInfo   = makeDescriptorBufferInfo(outputPositionsBuffer.get(), 0ull, VK_WHOLE_SIZE);

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
    auto ahModule   = makeVkSharedPtr(createShaderModule(vkd, device, m_context.getBinaryCollection().get("ah"), 0));
    auto chModule   = makeVkSharedPtr(createShaderModule(vkd, device, m_context.getBinaryCollection().get("ch"), 0));

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
        rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgenModule, 0);
        rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, missModule, 1);

        // We need one hit group for each of the geomtries.
        for (uint32_t g = 0u; g < geometryCount; ++g)
        {
            const auto hitGroup = 2u + g;

            rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR, ahModule, hitGroup);
            rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, chModule, hitGroup);
        }

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
                                                              shaderGroupBaseAlignment, 2, geometryCount);
        hitSBTRegion = makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitSBT->get(), 0),
                                                         shaderGroupHandleSize, shaderGroupHandleSize * geometryCount);
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
    std::vector<tcu::Vec4> outputData(expectedOutputPositions.size());
    const auto outputPositionsBufferSizeSz = static_cast<size_t>(outputPositionsBufferSize);

    invalidateAlloc(vkd, device, outputPositionsBufferAlloc);
    DE_ASSERT(de::dataSize(outputData) == outputPositionsBufferSizeSz);
    deMemcpy(outputData.data(), outputPositionsBufferData, outputPositionsBufferSizeSz);

    for (size_t i = 0; i < outputData.size(); ++i)
    {
        /*const */ auto &outVal = outputData[i]; // Should be const but .xyz() isn't
        tcu::Vec3 outVec3       = outVal.xyz();
        const auto &expectedVal = expectedOutputPositions[i];
        const auto &diff        = expectedOutputPositions[i] - outVec3;
        float len               = dot(diff, diff);

        // XXX Find a better epsilon
        if (!(len < 1e-5))
        {
            std::ostringstream msg;
            msg << "Unexpected value found for element " << i << ": expected " << expectedVal << " and found " << outVal
                << ";";
            TCU_FAIL(msg.str());
        }
#if 0
        else
        {
            std::ostringstream msg;
            msg << "Expected value found for element " << i << ": expected " << expectedVal << " and found " << outVal << ";\n";
            std::cout << msg.str();
        }
#endif
    }

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createPositionFetchTests(tcu::TestContext &testCtx)
{
    // Test ray pipeline shaders using position fetch
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "position_fetch"));

    struct
    {
        vk::VkAccelerationStructureBuildTypeKHR buildType;
        const char *name;
    } buildTypes[] = {
        {VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR, "cpu_built"},
        {VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, "gpu_built"},
    };

    const VkFormat vertexFormats[] = {
        // Mandatory formats.
        VK_FORMAT_R32G32_SFLOAT,
        VK_FORMAT_R32G32B32_SFLOAT,
        VK_FORMAT_R16G16_SFLOAT,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_FORMAT_R16G16_SNORM,
        VK_FORMAT_R16G16B16A16_SNORM,

        // Additional formats.
        VK_FORMAT_R8G8_SNORM,
        VK_FORMAT_R8G8B8_SNORM,
        VK_FORMAT_R8G8B8A8_SNORM,
        VK_FORMAT_R16G16B16_SNORM,
        VK_FORMAT_R16G16B16_SFLOAT,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_FORMAT_R64G64_SFLOAT,
        VK_FORMAT_R64G64B64_SFLOAT,
        VK_FORMAT_R64G64B64A64_SFLOAT,
    };

    for (size_t buildTypeNdx = 0; buildTypeNdx < DE_LENGTH_OF_ARRAY(buildTypes); ++buildTypeNdx)
    {
        de::MovePtr<tcu::TestCaseGroup> buildGroup(
            new tcu::TestCaseGroup(group->getTestContext(), buildTypes[buildTypeNdx].name));

        for (size_t vertexFormatNdx = 0; vertexFormatNdx < DE_LENGTH_OF_ARRAY(vertexFormats); ++vertexFormatNdx)
        {
            const auto format     = vertexFormats[vertexFormatNdx];
            const auto formatName = getFormatSimpleName(format);

            de::MovePtr<tcu::TestCaseGroup> vertexFormatGroup(
                new tcu::TestCaseGroup(group->getTestContext(), formatName.c_str()));

            for (uint32_t testFlagMask = 0; testFlagMask < TEST_FLAG_BIT_LAST; testFlagMask++)
            {
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
                    maskName = "NoFlags";

                de::MovePtr<tcu::TestCaseGroup> testFlagGroup(
                    new tcu::TestCaseGroup(group->getTestContext(), maskName.c_str()));

                TestParams testParams{
                    buildTypes[buildTypeNdx].buildType,
                    format,
                    testFlagMask,
                };

                vertexFormatGroup->addChild(new PositionFetchCase(testCtx, maskName, testParams));
            }
            buildGroup->addChild(vertexFormatGroup.release());
        }
        group->addChild(buildGroup.release());
    }

    return group.release();
}

} // namespace RayTracing
} // namespace vkt
