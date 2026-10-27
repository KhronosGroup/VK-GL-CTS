/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 The Khronos Group Inc.
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
 * \brief Ray Tracing Non-Uniform Acceleration Structure Indexing Tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingNonUniformASIndexingTests.hpp"
#include "vktTestCase.hpp"

#include "vkRayTracingUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuStringTemplate.hpp"

#include "deUniquePtr.hpp"
#include "deRandom.hpp"
#include "deSTLUtil.hpp"

#include <map>
#include <string>
#include <vector>

namespace vkt
{
namespace RayTracing
{

namespace
{

using namespace vk;

enum class ShaderType
{
    RAY_TRACING_PIPELINE,
    RAY_QUERY_COMPUTE,
};

enum class Indexing
{
    NON_UNIFORM,
    UNIFORM_LOOP,
};

struct TestParams
{
    ShaderType shaderType;
    Indexing indexing;
    uint32_t seed;

    VkShaderStageFlags getDescriptorStages(const TestParams &params) const
    {
        return (params.shaderType == ShaderType::RAY_TRACING_PIPELINE) ?
                   VkShaderStageFlags(VK_SHADER_STAGE_RAYGEN_BIT_KHR) :
                   VkShaderStageFlags(VK_SHADER_STAGE_COMPUTE_BIT);
    }
};

constexpr uint32_t TlasCount     = 4u;
constexpr uint32_t TlasArraySize = TlasCount - 2u; // TLAS 0 and TLAS TlasCount-1 use single bindings.
constexpr uint32_t BlasPerTlas   = 3u;
constexpr uint32_t GeomsPerBlas  = 3u;
constexpr uint32_t CellsPerTlas  = BlasPerTlas * GeomsPerBlas;
constexpr uint32_t RayCount      = 256u;
constexpr uint32_t LocalSize     = 64u;
constexpr uint32_t HitMarker     = 0xC0FFEEu;
constexpr uint32_t MissMarker    = 0xDEADu;
constexpr float TriangleZ        = 5.0f;

class NonUniformASIndexingCase : public TestCase
{
public:
    NonUniformASIndexingCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~NonUniformASIndexingCase() = default;

    virtual void checkSupport(Context &context) const override;
    virtual void initPrograms(vk::SourceCollections &programCollection) const override;
    virtual TestInstance *createInstance(Context &context) const override;

protected:
    const TestParams m_params;
};

class NonUniformASIndexingInstance : public TestInstance
{
public:
    NonUniformASIndexingInstance(Context &context, const TestParams &params) : TestInstance(context), m_params(params)
    {
    }
    virtual ~NonUniformASIndexingInstance() = default;

    std::vector<tcu::Vec3> makeCellTriangle(uint32_t blasIndex, uint32_t geometryIndex) const;
    virtual tcu::TestStatus iterate() override;

protected:
    const TestParams m_params;
};

void NonUniformASIndexingCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);

    if (!context.getAccelerationStructureFeatures().accelerationStructure)
        TCU_THROW(NotSupportedError,
                  "Requires VkPhysicalDeviceAccelerationStructureFeaturesKHR::accelerationStructure");

    if (m_params.shaderType == ShaderType::RAY_TRACING_PIPELINE)
    {
        context.requireDeviceFunctionality(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);

        if (!context.getRayTracingPipelineFeatures().rayTracingPipeline)
            TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR::rayTracingPipeline");
    }
    else
    {
        context.requireDeviceFunctionality(VK_KHR_RAY_QUERY_EXTENSION_NAME);

        if (!context.getRayQueryFeatures().rayQuery)
            TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayQueryFeaturesKHR::rayQuery");
    }

    if (m_params.indexing == Indexing::NON_UNIFORM)
        context.requireDeviceFunctionality(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
}

void NonUniformASIndexingCase::initPrograms(vk::SourceCollections &programCollection) const
{
    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
    const bool rayTracing = (m_params.shaderType == ShaderType::RAY_TRACING_PIPELINE);

    std::map<std::string, std::string> specs;
    specs["TLAS_COUNT"]    = std::to_string(TlasCount);
    specs["ARRAY_SIZE"]    = std::to_string(TlasArraySize);
    specs["LAST_TLAS"]     = std::to_string(TlasCount - 1u);
    specs["BLAS_PER_TLAS"] = std::to_string(BlasPerTlas);
    specs["HIT_MARKER"]    = std::to_string(HitMarker);
    specs["MISS_MARKER"]   = std::to_string(MissMarker);
    specs["LOCAL_SIZE"]    = std::to_string(LocalSize);

    const tcu::StringTemplate declsTemplate(R"glsl(
layout(set = 0, binding = 0, std430) readonly buffer Selectors { uvec2 sel[]; } selectors;
layout(set = 0, binding = 1, std430) writeonly buffer Results { uvec4 res[]; } results;
layout(set = 0, binding = 2) uniform accelerationStructureEXT tlasSingle0;
layout(set = 0, binding = 3) uniform accelerationStructureEXT tlasArray[${ARRAY_SIZE}];
layout(set = 0, binding = 4) uniform accelerationStructureEXT tlasSingle1;
)glsl");

    const tcu::StringTemplate rtTraceTemplate(
        R"glsl(            traceRayEXT(${AS}, gl_RayFlagsOpaqueEXT, 0xFFu, 0u, 0u, 0u, origin, 0.0, direction, 10.0, 0);
)glsl");

    const tcu::StringTemplate rqTraceTemplate(
        R"glsl(            rayQueryInitializeEXT(rq, ${AS}, gl_RayFlagsOpaqueEXT, 0xFFu, origin, 0.0, direction, 10.0);
)glsl");

    const auto &traceTemplate = rayTracing ? rtTraceTemplate : rqTraceTemplate;
    const auto makeTrace      = [&traceTemplate](const std::string &as) {
        return traceTemplate.specialize(std::map<std::string, std::string>{{"AS", as}});
    };

    const bool nonUniform = (m_params.indexing == Indexing::NON_UNIFORM);

    specs["DECLS"]         = declsTemplate.specialize(specs);
    specs["TLAS_LOOP"]     = nonUniform ?
                                 "    const uint t = sel.x;\n" :
                                 tcu::StringTemplate("    for (uint t = 0u; t < ${TLAS_COUNT}u; ++t)\n").specialize(specs);
    specs["TRACE_SINGLE0"] = makeTrace("tlasSingle0");
    specs["TRACE_SINGLE1"] = makeTrace("tlasSingle1");
    specs["TRACE_ARRAY"]   = makeTrace(nonUniform ? "tlasArray[nonuniformEXT(t - 1u)]" : "tlasArray[t - 1u]");

    const tcu::StringTemplate rtStoreTemplate(R"glsl(        results.res[rayIndex * ${TLAS_COUNT}u + t] = payload;
)glsl");

    const tcu::StringTemplate rqStoreTemplate(R"glsl(        while (rayQueryProceedEXT(rq))
        {
        }
        uvec4 result = uvec4(0u, 0u, 0u, ${MISS_MARKER}u);
        if (rayQueryGetIntersectionTypeEXT(rq, true) == gl_RayQueryCommittedIntersectionTriangleEXT)
        {
            result = uvec4(uint(rayQueryGetIntersectionInstanceCustomIndexEXT(rq, true)),
                           uint(rayQueryGetIntersectionInstanceIdEXT(rq, true)),
                           uint(rayQueryGetIntersectionGeometryIndexEXT(rq, true)),
                           ${HIT_MARKER}u);
        }
        results.res[rayIndex * ${TLAS_COUNT}u + t] = result;
)glsl");

    specs["QUERY_DECL"] = rayTracing ? "" : "        rayQueryEXT rq;\n";
    specs["STORE"]      = (rayTracing ? rtStoreTemplate : rqStoreTemplate).specialize(specs);

    const tcu::StringTemplate bodyTemplate(R"glsl(    const uvec2 sel = selectors.sel[rayIndex];
    const vec3 origin = vec3(float(sel.y % ${BLAS_PER_TLAS}u) + 0.5, float(sel.y / ${BLAS_PER_TLAS}u) + 0.5, 0.0);
    const vec3 direction = vec3(0.0, 0.0, 1.0);
${TLAS_LOOP}    {
${QUERY_DECL}        if (t == 0u)
        {
${TRACE_SINGLE0}        }
        else if (t == ${LAST_TLAS}u)
        {
${TRACE_SINGLE1}        }
        else
        {
${TRACE_ARRAY}        }
${STORE}    }
)glsl");

    specs["BODY"] = bodyTemplate.specialize(specs);

    if (rayTracing)
    {
        const tcu::StringTemplate rgen(R"glsl(#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
${DECLS}
layout(location = 0) rayPayloadEXT uvec4 payload;

void main()
{
    const uint rayIndex = gl_LaunchIDEXT.x;
${BODY}}
)glsl");

        const tcu::StringTemplate chit(R"glsl(#version 460 core
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT uvec4 payload;

void main()
{
    payload = uvec4(uint(gl_InstanceCustomIndexEXT), uint(gl_InstanceID), uint(gl_GeometryIndexEXT), ${HIT_MARKER}u);
}
)glsl");

        const tcu::StringTemplate miss(R"glsl(#version 460 core
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT uvec4 payload;

void main()
{
    payload = uvec4(0u, 0u, 0u, ${MISS_MARKER}u);
}
)glsl");

        programCollection.glslSources.add("rgen")
            << glu::RaygenSource(updateRayTracingGLSL(rgen.specialize(specs))) << buildOptions;
        programCollection.glslSources.add("chit")
            << glu::ClosestHitSource(updateRayTracingGLSL(chit.specialize(specs))) << buildOptions;
        programCollection.glslSources.add("miss")
            << glu::MissSource(updateRayTracingGLSL(miss.specialize(specs))) << buildOptions;
    }
    else
    {
        const tcu::StringTemplate comp(R"glsl(#version 460 core
#extension GL_EXT_ray_query : require
#extension GL_EXT_nonuniform_qualifier : require

layout(local_size_x = ${LOCAL_SIZE}, local_size_y = 1, local_size_z = 1) in;
${DECLS}
void main()
{
    const uint rayIndex = gl_GlobalInvocationID.x;
${BODY}}
)glsl");

        programCollection.glslSources.add("comp") << glu::ComputeSource(comp.specialize(specs)) << buildOptions;
    }
}

TestInstance *NonUniformASIndexingCase::createInstance(Context &context) const
{
    return new NonUniformASIndexingInstance(context, m_params);
}

std::vector<tcu::Vec3> NonUniformASIndexingInstance::makeCellTriangle(uint32_t blasIndex, uint32_t geometryIndex) const
{
    const float cx = static_cast<float>(blasIndex) + 0.5f;
    const float cy = static_cast<float>(geometryIndex) + 0.5f;

    return std::vector<tcu::Vec3>{
        tcu::Vec3(cx - 0.25f, cy - 0.25f, TriangleZ),
        tcu::Vec3(cx + 0.25f, cy - 0.25f, TriangleZ),
        tcu::Vec3(cx, cy + 0.25f, TriangleZ),
    };
}

tcu::TestStatus NonUniformASIndexingInstance::iterate()
{
    const auto &vki       = m_context.getInstanceInterface();
    const auto physDev    = m_context.getPhysicalDevice();
    const auto &vkd       = m_context.getDeviceInterface();
    const auto device     = m_context.getDevice();
    auto &alloc           = m_context.getDefaultAllocator();
    const auto qIndex     = m_context.getUniversalQueueFamilyIndex();
    const auto queue      = m_context.getUniversalQueue();
    const auto stages     = m_params.getDescriptorStages(m_params);
    const bool rayTracing = (m_params.shaderType == ShaderType::RAY_TRACING_PIPELINE);
    const auto bindPoint  = rayTracing ? VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR : VK_PIPELINE_BIND_POINT_COMPUTE;
    const auto shaderStage =
        rayTracing ? VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR : VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    const auto cmdPool      = makeCommandPool(vkd, device, qIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    beginCommandBuffer(vkd, cmdBuffer);

    AccelerationStructBufferProperties bufferProps;
    bufferProps.props.residency = ResourceResidency::TRADITIONAL;

    std::vector<de::SharedPtr<BottomLevelAccelerationStructure>> blasVect;
    std::vector<de::MovePtr<TopLevelAccelerationStructure>> tlasVect(TlasCount);

    for (uint32_t tlasIndex = 0u; tlasIndex < TlasCount; ++tlasIndex)
    {
        auto &tlas = tlasVect[tlasIndex];
        tlas       = makeTopLevelAccelerationStructure();
        tlas->setInstanceCount(BlasPerTlas);

        for (uint32_t blasIndex = 0u; blasIndex < BlasPerTlas; ++blasIndex)
        {
            auto blas = makeBottomLevelAccelerationStructure();
            for (uint32_t geometryIndex = 0u; geometryIndex < GeomsPerBlas; ++geometryIndex)
                blas->addGeometry(makeCellTriangle(blasIndex, geometryIndex), true /*is triangles*/,
                                  VK_GEOMETRY_OPAQUE_BIT_KHR);
            blas->createAndBuild(vkd, device, cmdBuffer, alloc, bufferProps);

            blasVect.emplace_back(blas.release());
            tlas->addInstance(blasVect.back(), identityMatrix3x4, tlasIndex * BlasPerTlas + blasIndex, 0xFFu, 0u,
                              VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR);
        }

        tlas->createAndBuild(vkd, device, cmdBuffer, alloc, bufferProps);
    }

    // Per-ray selector: x = TLAS index, y = cell (blas + geometry * BlasPerTlas).
    std::vector<tcu::UVec2> selectors;
    selectors.reserve(RayCount);
    for (uint32_t i = 0u; i < RayCount; ++i)
        selectors.push_back(tcu::UVec2(i % TlasCount, (i / TlasCount) % CellsPerTlas));

    de::Random rnd(m_params.seed);
    rnd.shuffle(selectors.begin(), selectors.end());

    const auto selectorsBufferSize = static_cast<VkDeviceSize>(de::dataSize(selectors));
    const auto selectorsBufferInfo = makeBufferCreateInfo(selectorsBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory selectorsBuffer(vkd, device, alloc, selectorsBufferInfo, MemoryRequirement::HostVisible);
    auto &selectorsBufferAlloc = selectorsBuffer.getAllocation();
    deMemcpy(selectorsBufferAlloc.getHostPtr(), selectors.data(), static_cast<size_t>(selectorsBufferSize));
    flushAlloc(vkd, device, selectorsBufferAlloc);

    const auto resultsBufferSize = static_cast<VkDeviceSize>(sizeof(tcu::UVec4) * RayCount * TlasCount);
    const auto resultsBufferInfo = makeBufferCreateInfo(resultsBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory resultsBuffer(vkd, device, alloc, resultsBufferInfo, MemoryRequirement::HostVisible);
    auto &resultsBufferAlloc = resultsBuffer.getAllocation();
    void *resultsBufferData  = resultsBufferAlloc.getHostPtr();
    deMemset(resultsBufferData, 0, static_cast<size_t>(resultsBufferSize));
    flushAlloc(vkd, device, resultsBufferAlloc);

    DescriptorSetLayoutBuilder dsLayoutBuilder;
    dsLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stages);
    dsLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stages);
    dsLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, stages);
    dsLayoutBuilder.addArrayBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, TlasArraySize, stages);
    dsLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, stages);
    const auto setLayout = dsLayoutBuilder.build(vkd, device);

    const auto pipelineLayout = makePipelineLayout(vkd, device, setLayout.get());

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, TlasCount);
    const auto descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descriptorSet  = makeDescriptorSet(vkd, device, descriptorPool.get(), setLayout.get());

    {
        std::vector<VkAccelerationStructureKHR> tlasHandles;
        for (const auto &tlas : tlasVect)
            tlasHandles.push_back(*tlas->getPtr());

        VkWriteDescriptorSetAccelerationStructureKHR single0Info = initVulkanStructure();
        single0Info.accelerationStructureCount                   = 1u;
        single0Info.pAccelerationStructures                      = &tlasHandles[0];

        VkWriteDescriptorSetAccelerationStructureKHR arrayInfo = initVulkanStructure();
        arrayInfo.accelerationStructureCount                   = TlasArraySize;
        arrayInfo.pAccelerationStructures                      = &tlasHandles[1];

        VkWriteDescriptorSetAccelerationStructureKHR single1Info = initVulkanStructure();
        single1Info.accelerationStructureCount                   = 1u;
        single1Info.pAccelerationStructures                      = &tlasHandles[TlasCount - 1u];

        const auto selectorsInfo = makeDescriptorBufferInfo(selectorsBuffer.get(), 0ull, VK_WHOLE_SIZE);
        const auto resultsInfo   = makeDescriptorBufferInfo(resultsBuffer.get(), 0ull, VK_WHOLE_SIZE);

        DescriptorSetUpdateBuilder updateBuilder;
        updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                                  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &selectorsInfo);
        updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u),
                                  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultsInfo);
        updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(2u),
                                  VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &single0Info);
        updateBuilder.writeArray(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(3u),
                                 VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, TlasArraySize, &arrayInfo);
        updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(4u),
                                  VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &single1Info);
        updateBuilder.update(vkd, device);
    }

    Move<VkPipeline> pipeline;
    de::MovePtr<BufferWithMemory> raygenSBT;
    de::MovePtr<BufferWithMemory> missSBT;
    de::MovePtr<BufferWithMemory> hitSBT;

    auto raygenSBTRegion   = makeStridedDeviceAddressRegionKHR(0, 0, 0);
    auto missSBTRegion     = makeStridedDeviceAddressRegionKHR(0, 0, 0);
    auto hitSBTRegion      = makeStridedDeviceAddressRegionKHR(0, 0, 0);
    auto callableSBTRegion = makeStridedDeviceAddressRegionKHR(0, 0, 0);

    if (rayTracing)
    {
        const auto rayTracingPropertiesKHR  = makeRayTracingProperties(vki, physDev);
        const auto shaderGroupHandleSize    = rayTracingPropertiesKHR->getShaderGroupHandleSize();
        const auto shaderGroupBaseAlignment = rayTracingPropertiesKHR->getShaderGroupBaseAlignment();

        auto rgenModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("rgen"), 0);
        auto missModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("miss"), 0);
        auto chitModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("chit"), 0);

        const auto rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();
        rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgenModule, 0);
        rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, missModule, 1);
        rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, chitModule, 2);

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
    else
    {
        const auto compModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"), 0);
        pipeline              = makeComputePipeline(vkd, device, pipelineLayout.get(), compModule.get());
    }

    vkd.cmdBindPipeline(cmdBuffer, bindPoint, pipeline.get());
    vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);

    if (rayTracing)
        vkd.cmdTraceRaysKHR(cmdBuffer, &raygenSBTRegion, &missSBTRegion, &hitSBTRegion, &callableSBTRegion, RayCount,
                            1u, 1u);
    else
        vkd.cmdDispatch(cmdBuffer, RayCount / LocalSize, 1u, 1u);

    const auto bufferBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    vkd.cmdPipelineBarrier(cmdBuffer, shaderStage, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &bufferBarrier, 0u, nullptr, 0u,
                           nullptr);

    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    invalidateAlloc(vkd, device, resultsBufferAlloc);
    std::vector<tcu::UVec4> results(RayCount * TlasCount);
    deMemcpy(results.data(), resultsBufferData, static_cast<size_t>(resultsBufferSize));

    auto &log                  = m_context.getTestContext().getLog();
    const uint32_t maxLogged   = 16u;
    uint32_t mismatchCount     = 0u;
    const bool tracesEveryTlas = (m_params.indexing == Indexing::UNIFORM_LOOP);

    for (uint32_t rayIndex = 0u; rayIndex < RayCount; ++rayIndex)
    {
        const auto &selector = selectors[rayIndex];
        const auto blasIndex = selector.y() % BlasPerTlas;
        const auto geomIndex = selector.y() / BlasPerTlas;

        for (uint32_t tlasIndex = 0u; tlasIndex < TlasCount; ++tlasIndex)
        {
            const bool traced   = tracesEveryTlas || (tlasIndex == selector.x());
            const auto expected = traced ?
                                      tcu::UVec4(tlasIndex * BlasPerTlas + blasIndex, blasIndex, geomIndex, HitMarker) :
                                      tcu::UVec4(0u);
            const auto &result  = results[rayIndex * TlasCount + tlasIndex];

            if (result != expected)
            {
                if (mismatchCount < maxLogged)
                    log << tcu::TestLog::Message << "Ray " << rayIndex << " (selected TLAS " << selector.x()
                        << ", cell " << selector.y() << "), TLAS slot " << tlasIndex << ": expected " << expected
                        << " but found " << result << tcu::TestLog::EndMessage;
                ++mismatchCount;
            }
        }
    }

    if (mismatchCount > 0u)
        return tcu::TestStatus::fail("Found " + std::to_string(mismatchCount) + " unexpected results");

    return tcu::TestStatus::pass("Pass");
}

} // unnamed namespace

tcu::TestCaseGroup *createNonUniformASIndexingTests(tcu::TestContext &testCtx)
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    const struct
    {
        ShaderType shaderType;
        const char *name;
    } shaderTypes[] = {
        {ShaderType::RAY_TRACING_PIPELINE, "raygen"},
        {ShaderType::RAY_QUERY_COMPUTE, "ray_query_compute"},
    };

    const struct
    {
        Indexing indexing;
        const char *name;
    } indexings[] = {
        {Indexing::NON_UNIFORM, "nonuniform"},
        {Indexing::UNIFORM_LOOP, "uniform_loop"},
    };

    // Index multiple TLASes bound as single bindings and as an array, without descriptor heaps
    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "non_uniform_as_indexing"));

    uint32_t seed = 6930u;
    for (const auto &shaderType : shaderTypes)
    {
        GroupPtr shaderGroup(new tcu::TestCaseGroup(testCtx, shaderType.name));

        for (const auto &indexing : indexings)
            shaderGroup->addChild(new NonUniformASIndexingCase(
                testCtx, indexing.name, TestParams{shaderType.shaderType, indexing.indexing, seed++}));

        mainGroup->addChild(shaderGroup.release());
    }

    return mainGroup.release();
}

} // namespace RayTracing
} // namespace vkt
