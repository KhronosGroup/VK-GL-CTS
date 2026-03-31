/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
 * Copyright (c) 2025 ARM Ltd.
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
 * \brief Ray Tracing Invocation Reorder Activity Test
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingInvocationReorderActivityTests.hpp"
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

struct TestParams
{
    bool use_shader_invocation_reorder;
    int32_t resX, resY;
};

class RTIRActivityCase : public TestCase
{
public:
    RTIRActivityCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params);
    virtual ~RTIRActivityCase(void)
    {
    }

    virtual void checkSupport(Context &context) const;
    virtual void initPrograms(vk::SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;

protected:
    TestParams m_params;
    bool m_support_EXT_extension;
};

class RTIRActivityInstance : public TestInstance
{
public:
    RTIRActivityInstance(Context &context, const TestParams &params);
    virtual ~RTIRActivityInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void);

protected:
    TestParams m_params;
};

RTIRActivityCase::RTIRActivityCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
    : TestCase(testCtx, name)
    , m_params(params)
{
}

void RTIRActivityCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_deferred_host_operations");
    context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
    context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");
    context.requireDeviceFunctionality("VK_KHR_buffer_device_address");

    context.requireDeviceFunctionality("VK_EXT_ray_tracing_invocation_reorder");

    const VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures =
        context.getBufferDeviceAddressFeatures();
    if (bufferDeviceAddressFeatures.bufferDeviceAddress == false)
        TCU_THROW(TestError, "Requires VkPhysicalDeviceBufferDeviceAddressFeatures.bufferDeviceAddress");

    const VkPhysicalDeviceRayTracingPipelineFeaturesKHR &rayTracingPipelineFeaturesKHR =
        context.getRayTracingPipelineFeatures();
    if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == false)
        TCU_THROW(TestError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");

    const VkPhysicalDeviceAccelerationStructureFeaturesKHR &accelerationStructureFeaturesKHR =
        context.getAccelerationStructureFeatures();
    if (accelerationStructureFeaturesKHR.accelerationStructure == false)
        TCU_THROW(TestError,
                  "VK_KHR_ray_query requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");

    // TO TEST
    const VkPhysicalDeviceRayTracingInvocationReorderFeaturesEXT &RTIRFeaturesEXT =
        context.getRayTracingInvocationReorderFeaturesEXT();
    if (RTIRFeaturesEXT.rayTracingInvocationReorder == false)
        TCU_THROW(NotSupportedError,
                  "Requires VkPhysicalDeviceRayTracingInvocationReorderFeaturesEXT.rayTracingInvocationReorder");
}

void RTIRActivityCase::initPrograms(vk::SourceCollections &programCollection) const
{
    // The purpose of this test is to submit a ray tracing workload "heavy" enough to trigger reordering and
    // to check that reordering actually happened (instead of doing nothing during the call to 'reorderThread'.
    //
    // A set of 2x4 quads are rendered both on left and right side of the image. The left side is used as reference
    // and the right side is used as test. Each quad is rendered using a material, i.e. a closest hit shader, complex
    // enough to simulate a heavy ray tracing workload. Different parameters are used for each different quads to
    // simulate different materials.
    // Primary rays are traced in an incoherent manner, so consecutive pixels are rendered with different materials,
    // i.e. invoking different closest hit shaders.
    // The final pixel color is derived from the corresponding subgroup invocation ID. When no reordering is performed
    // (e.g. left side), a regular pattern can be observed on tested implementations. When reordering occurs, this
    // regular pattern is perturbed. The test consists in comparing this pattern between left and right sides. If there
    // is a difference, then actual reordering is deduced, and the test is successful. Otherwise, the test fails.

    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

    const float focusDistance = 1.15f;

    const uint32_t outputSize = (uint32_t)sizeof(uint32_t) * m_params.resX * m_params.resY;

    std::ostringstream rgen_begin;
    rgen_begin << "#version 460\n"
               << "#extension GL_EXT_ray_tracing : require\n"
               << "#extension GL_KHR_shader_subgroup_basic : require\n"
               << "#extension GL_EXT_shader_invocation_reorder : require\n"
               << "\n"
               << "layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;\n"
               << "layout(binding = 1, set = 0, std430) buffer OutputBuffer {\n"
               << "    uint values[" << outputSize << "];\n"
               << "} outBuf;\n"
               << "\n"
               << "layout(location = 0) rayPayloadEXT vec3 hitValue;\n"
               << "\n"
               << "float rand(float seed) {\n"
               << "    uint n = floatBitsToUint(seed);\n"
               << "    n = (n ^ 61u) ^ (n >> 16);\n"
               << "    n *= 9u;\n"
               << "    n = n ^ (n >> 4);\n"
               << "    n *= 0x27d4eb2du;\n"
               << "    n = n ^ (n >> 15);\n"
               << "    return float(n) / float(0xFFFFFFFFu);\n"
               << "}\n"
               << "\n"
               << "vec2 sampleDisk(float seed) {\n"
               << "    float theta = 2.0 * 3.141592653589793 * rand(seed);\n"
               << "    float r = sqrt(rand(seed + 1.0)); \n"
               << "    return vec2(r * cos(theta), r * sin(theta));\n"
               << "}\n"
               << "\n"
               << "void main() \n"
               << "{\n"
               << "    const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);\n"
               << "    const vec2 inUV = pixelCenter/vec2(gl_LaunchSizeEXT.xy);\n"
               << "    vec2 d = inUV * 2.0 - 1.0;\n"
               << "\n"
               << "    vec3 origin = vec3(d, -1.0);\n"
               << "    vec3 direction = vec3(0.0, 0.0, 1.0);\n"
               << "\n"
               << "    float apertureSize = 1.0;\n"
               << "    float focusDistance = " << focusDistance << ";\n"
               << "\n"
               << "    vec2 lensSample = sampleDisk(float(pixelCenter.x + pixelCenter.y * gl_LaunchSizeEXT.x)) * "
                  "apertureSize;\n"
               << "    vec3 defocusedRayOrigin = origin + vec3(lensSample, 0.0);\n"
               << "    vec3 focusPoint = origin + focusDistance * direction;\n"
               << "    vec3 finalRayDirection = normalize(focusPoint - defocusedRayOrigin);\n"
               << "\n"
               << "    float tmin = 0.001;\n"
               << "    float tmax = 10000.0;\n"
               << "\n"
               << "    hitValue = vec3(0.0);\n"
               << "    uvec3 invocationRes = uvec3(0);\n"
               << "\n";
    const auto rgen_beginStr = rgen_begin.str();

    std::ostringstream rgen_TraceNoRTIR;
    rgen_TraceNoRTIR << "    traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, 0xFF, 0, 0, 0, defocusedRayOrigin, tmin, "
                        "finalRayDirection, tmax, 0);\n";
    const auto rgen_TraceNoRTIRStr = rgen_TraceNoRTIR.str();

    std::ostringstream rgen_TraceRTIR;

    rgen_TraceRTIR << "    hitObjectEXT hObj;\n"
                   << "    hitObjectRecordEmptyEXT(hObj);\n"
                   << "\n"
                   << "    hitObjectTraceRayEXT(hObj, topLevelAS, gl_RayFlagsOpaqueEXT, 0xFF, 0, 0, 0, "
                      "defocusedRayOrigin, tmin, finalRayDirection, tmax, 0);\n"
                   << "\n"
                   << "    if (gl_LaunchIDEXT.x > (gl_LaunchSizeEXT.x/2))\n"
                   << "        reorderThreadEXT(hObj);\n"
                   << "\n"
                   << "    hitObjectExecuteShaderEXT(hObj,0);\n"
                   << "\n";

    const auto rgen_TraceRTIRStr = rgen_TraceRTIR.str();

    std::ostringstream rgen_end;
    rgen_end << "    uint col[192] = {0, 0, 0, 0, 0, 85,0, 0, 170,0, 0, 255,0, 85, 0,0, 85, 85,0, 85, 170,\n"
             << "0, 85, 255, 0, 170, 0,0, 170, 85,0, 170, 170,0, 170, 255,0, 255, 0,0, 255, 85,0, 255, 170,\n"
             << "0, 255, 255, 85, 0, 0,85, 0, 85,85, 0, 170,85, 0, 255,85, 85, 0,85, 85, 85,85, 85, 170,85,\n"
             << "85, 255,85, 170, 0,85, 170, 85, 85, 170, 170,85, 170, 255,85, 255, 0,85, 255, 85,85, 255,\n"
             << "170,85, 255, 255,170, 0, 0,170, 0, 85,170, 0, 170,170, 0, 255,170, 85, 0,170, 85, 85,170,\n"
             << "85, 170,170, 85, 255,170, 170, 0,170, 170, 85,170, 170, 170,170, 170, 255,170, 255, 0,170,\n"
             << "255, 85,170, 255, 170,170, 255, 255,255, 0, 0,255, 0, 85,255, 0, 170,255, 0, 255,255, 85,\n"
             << "0,255, 85, 85,255, 85, 170,255, 85, 255,255, 170, 0,255, 170, 85,255, 170, 170,255, 170,\n"
             << "255,255, 255, 0,255, 255, 85,255, 255, 170,255, 255, 255};\n"
             << "    uint subInvID = gl_SubgroupInvocationID*3;\n"
             << "    invocationRes = uvec3(col[subInvID], col[subInvID+1], col[subInvID+2]);\n"
             << "\n"
             << "    uint index = gl_LaunchIDEXT.y * " << m_params.resX << " + gl_LaunchIDEXT.x; \n"
             << "    outBuf.values[index] = uint(invocationRes.x) << 16 | uint(invocationRes.y) << 8 | "
                "uint(invocationRes.z);\n"
             << "}\n";
    const auto rgen_endStr = rgen_end.str();

    std::ostringstream rgen;
    if (m_params.use_shader_invocation_reorder)
    {
        rgen << rgen_beginStr << rgen_TraceRTIRStr << rgen_endStr;
    }
    else
    {
        rgen << rgen_beginStr << rgen_TraceNoRTIRStr << rgen_endStr;
    }

    std::ostringstream miss;
    miss << "#version 460\n"
         << "#extension GL_EXT_ray_tracing : enable\n"
         << "\n"
         << "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
         << "\n"
         << "void main()\n"
         << "{\n"
         << "    hitValue = vec3(0.0, 0.0, 0.0);\n"
         << "}\n";

    std::ostringstream ch_start;
    ch_start << "#version 460\n"
             << "#extension GL_EXT_ray_tracing : enable\n"
             << "\n"
             << "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
             << "hitAttributeEXT vec3 attribs;\n"
             << "\n";
    const auto ch_startStr = ch_start.str();

    const int outer_loop = 100;
    const int inner_loop = 100;

    std::ostringstream ch_end;
    ch_end << "float compute(float x, float scale) {\n"
           << "    float acc = x;\n"
           << "    for (int i = 0; i < " << inner_loop << "; ++i) {\n"
           << "        acc += sin(acc * 0.1 + scale) * cos(acc * 0.05 + scale);\n"
           << "        acc = fract(acc + scale * 0.0001);\n"
           << "    }\n"
           << "    return acc;\n"
           << " }\n"
           << "\n"
           << "void main()\n"
           << "{\n"
           << "    vec2 coord = attribs.xy;\n"
           << "    float seed = coord.x + coord.y;\n"
           << "\n"
           << "    int iterations = int(10.0 + mod(uScale * 100.0, 90.0));\n"
           << "    float result = seed;\n"
           << "\n"
           << "    for (int i = 0; i < " << outer_loop << "; ++i) {\n"
           << "        if (i >= iterations) break;\n"
           << "        result = compute(result, uScale);\n"
           << "    }\n"
           << "    hitValue = vec3(fract(result), fract(result * 1.7), fract(result * 2.3));\n"
           << "}\n";
    const auto ch_endStr = ch_end.str();

    std::ostringstream ch0;
    ch0 << ch_startStr << "float uScale = 10.0;\n" << ch_endStr;

    std::ostringstream ch1;
    ch1 << ch_startStr << "float uScale = 20.0;\n" << ch_endStr;

    std::ostringstream ch2;
    ch2 << ch_startStr << "float uScale = 30.0;\n" << ch_endStr;

    std::ostringstream ch3;
    ch3 << ch_startStr << "float uScale = 40.0;\n" << ch_endStr;

    std::ostringstream ch4;
    ch4 << ch_startStr << "float uScale = 50.0;\n" << ch_endStr;

    std::ostringstream ch5;
    ch5 << ch_startStr << "float uScale = 60.0;\n" << ch_endStr;

    std::ostringstream ch6;
    ch6 << ch_startStr << "float uScale = 70.0;\n" << ch_endStr;

    std::ostringstream ch7;
    ch7 << ch_startStr << "float uScale = 80.0;\n" << ch_endStr;

    programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(rgen.str())) << buildOptions;
    programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(miss.str())) << buildOptions;
    programCollection.glslSources.add("ch0") << glu::ClosestHitSource(updateRayTracingGLSL(ch0.str())) << buildOptions;
    programCollection.glslSources.add("ch1") << glu::ClosestHitSource(updateRayTracingGLSL(ch1.str())) << buildOptions;
    programCollection.glslSources.add("ch2") << glu::ClosestHitSource(updateRayTracingGLSL(ch2.str())) << buildOptions;
    programCollection.glslSources.add("ch3") << glu::ClosestHitSource(updateRayTracingGLSL(ch3.str())) << buildOptions;
    programCollection.glslSources.add("ch4") << glu::ClosestHitSource(updateRayTracingGLSL(ch4.str())) << buildOptions;
    programCollection.glslSources.add("ch5") << glu::ClosestHitSource(updateRayTracingGLSL(ch5.str())) << buildOptions;
    programCollection.glslSources.add("ch6") << glu::ClosestHitSource(updateRayTracingGLSL(ch6.str())) << buildOptions;
    programCollection.glslSources.add("ch7") << glu::ClosestHitSource(updateRayTracingGLSL(ch7.str())) << buildOptions;
}

TestInstance *RTIRActivityCase::createInstance(Context &context) const
{
    return new RTIRActivityInstance(context, m_params);
}

RTIRActivityInstance::RTIRActivityInstance(Context &context, const TestParams &params)
    : TestInstance(context)
    , m_params(params)
{
}

tcu::TestStatus RTIRActivityInstance::iterate(void)
{
    const auto &vki    = m_context.getInstanceInterface();
    const auto physDev = m_context.getPhysicalDevice();
    const auto &vkd    = m_context.getDeviceInterface();
    const auto device  = m_context.getDevice();
    auto &alloc        = m_context.getDefaultAllocator();
    const auto qIndex  = m_context.getUniversalQueueFamilyIndex();
    const auto queue   = m_context.getUniversalQueue();
    const auto stages =
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkPhysicalDeviceSubgroupProperties subgroupProps;
    subgroupProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
    subgroupProps.pNext = nullptr;

    uint32_t subgroupSize = 0;

    // TO TEST
    VkPhysicalDeviceRayTracingInvocationReorderPropertiesEXT invocationReoderProperties;
    invocationReoderProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_PROPERTIES_EXT;
    invocationReoderProperties.pNext = &subgroupProps;

    VkPhysicalDeviceProperties2 deviceProps2;
    deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProps2.pNext = &invocationReoderProperties;

    vki.getPhysicalDeviceProperties2(physDev, &deviceProps2);
    subgroupSize = subgroupProps.subgroupSize;

    if (invocationReoderProperties.rayTracingInvocationReorderReorderingHint ==
        VK_RAY_TRACING_INVOCATION_REORDER_MODE_NONE_EXT)
        return tcu::TestStatus::pass("Pass");

    // Testing if the implementation support Invocation Reorder
    // - if no support, the test pass (no need to check if reordering happens)
    // - if support, the test needs to be performed

    VkPhysicalDeviceRayTracingInvocationReorderFeaturesEXT reorderFeature;
    reorderFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_FEATURES_EXT;
    reorderFeature.pNext = nullptr;

    VkPhysicalDeviceFeatures2 deviceFeatures2;
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &reorderFeature;

    vki.getPhysicalDeviceFeatures2(physDev, &deviceFeatures2);

    if (reorderFeature.rayTracingInvocationReorder == 0)
        return tcu::TestStatus::pass("Pass");

    if (m_params.resX % 2 != 0)
    {
        TCU_FAIL("Resolution not multiple of 2");
    }
    if (m_params.resY % 2 != 0)
    {
        TCU_FAIL("Resolution not multiple of 2");
    }

    if (m_params.resX / 2 % subgroupSize != 0)
    {
        TCU_FAIL("Resolution not multiple of subgroup size");
    }
    if (m_params.resY / 2 % subgroupSize != 0)
    {
        TCU_FAIL("Resolution not multiple of subgroup size");
    }

    // Command pool and buffer.
    const auto cmdPool      = makeCommandPool(vkd, device, qIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    beginCommandBuffer(vkd, cmdBuffer);

    // Build acceleration structures.
    auto topLevelAS    = makeTopLevelAccelerationStructure();
    auto bottomLevelAS = makeBottomLevelAccelerationStructure();

    std::vector<tcu::Vec3> vertices;

    const float offset = 0.02f;
    const float dim    = 0.48f;
    vertices.push_back(tcu::Vec3(offset, -1.f + offset, 0.0f));
    vertices.push_back(tcu::Vec3(offset + dim, -1.f + offset, 0.0f));
    vertices.push_back(tcu::Vec3(offset + dim, -1.f + offset + dim, 0.0f));
    vertices.push_back(tcu::Vec3(offset, -1.f + offset, 0.0f));
    vertices.push_back(tcu::Vec3(offset, -1.f + offset + dim, 0.0f));
    vertices.push_back(tcu::Vec3(offset + dim, -1.f + offset + dim, 0.0f));

    AccelerationStructBufferProperties bufferProps;
    bufferProps.props.residency = ResourceResidency::TRADITIONAL;

    bottomLevelAS->addGeometry(vertices, true /*is triangles*/, VK_GEOMETRY_OPAQUE_BIT_KHR, nullptr);
    bottomLevelAS->setBuildFlags(VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
    bottomLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc, bufferProps);
    de::SharedPtr<BottomLevelAccelerationStructure> blasSharedPtr(bottomLevelAS.release());

    VkGeometryInstanceFlagsKHR instanceFlags = VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;

    VkTransformMatrixKHR transfoMatrix3x4 = {
        {{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}}};

    topLevelAS->setInstanceCount(16);

    for (int n = 0; n < 2; ++n)
        for (int j = 0; j < 4; ++j)
            for (int i = 0; i < 2; ++i)
            {
                transfoMatrix3x4.matrix[0][3] = (float)i * 0.5f - 1.0f * (float)n;
                transfoMatrix3x4.matrix[1][3] = (float)j * 0.5f;

                topLevelAS->addInstance(blasSharedPtr, transfoMatrix3x4, 0, 0x7Fu, 2 * j + i, instanceFlags);
            }
    topLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc, bufferProps);

    // Storage buffer for output modes
    const auto outputModesBufferSize = static_cast<VkDeviceSize>(sizeof(uint32_t) * m_params.resX * m_params.resY);
    const auto outputModesBufferInfo = makeBufferCreateInfo(outputModesBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory outputModesBuffer(vkd, device, alloc, outputModesBufferInfo, MemoryRequirement::HostVisible);
    auto &outputModesBufferAlloc = outputModesBuffer.getAllocation();
    void *outputModesBufferData  = outputModesBufferAlloc.getHostPtr();
    deMemset(outputModesBufferData, 0x0, static_cast<size_t>(outputModesBufferSize));
    flushAlloc(vkd, device, outputModesBufferAlloc);

    // Descriptor set layout.
    DescriptorSetLayoutBuilder dsLayoutBuilder;
    dsLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, stages);
    dsLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stages);
    const auto setLayout = dsLayoutBuilder.build(vkd, device);

    // Pipeline layout.
    const auto pipelineLayout = makePipelineLayout(vkd, device, setLayout.get());

    // Descriptor pool and set.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
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
        const auto storageBufferInfo = makeDescriptorBufferInfo(outputModesBuffer.get(), 0ull, VK_WHOLE_SIZE);

        DescriptorSetUpdateBuilder updateBuilder;
        updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                                  VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelDescInfo);
        updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u),
                                  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &storageBufferInfo);
        updateBuilder.update(vkd, device);
    }

    // Shader modules.
    auto rgenModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("rgen"), 0);
    auto missModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("miss"), 0);
    auto ch0Module  = createShaderModule(vkd, device, m_context.getBinaryCollection().get("ch0"), 0);
    auto ch1Module  = createShaderModule(vkd, device, m_context.getBinaryCollection().get("ch1"), 0);
    auto ch2Module  = createShaderModule(vkd, device, m_context.getBinaryCollection().get("ch2"), 0);
    auto ch3Module  = createShaderModule(vkd, device, m_context.getBinaryCollection().get("ch3"), 0);
    auto ch4Module  = createShaderModule(vkd, device, m_context.getBinaryCollection().get("ch4"), 0);
    auto ch5Module  = createShaderModule(vkd, device, m_context.getBinaryCollection().get("ch5"), 0);
    auto ch6Module  = createShaderModule(vkd, device, m_context.getBinaryCollection().get("ch6"), 0);
    auto ch7Module  = createShaderModule(vkd, device, m_context.getBinaryCollection().get("ch7"), 0);

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
        rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, ch0Module, 2);
        rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, ch1Module, 3);
        rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, ch2Module, 4);
        rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, ch3Module, 5);
        rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, ch4Module, 6);
        rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, ch5Module, 7);
        rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, ch6Module, 8);
        rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, ch7Module, 9);

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
                                                              shaderGroupBaseAlignment, 2, 8);
        hitSBTRegion = makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitSBT->get(), 0),
                                                         shaderGroupHandleSize, shaderGroupHandleSize);
    }

    // Trace rays.
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.get());
    vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout.get(), 0u, 1u,
                              &descriptorSet.get(), 0u, nullptr);
    vkd.cmdTraceRaysKHR(cmdBuffer, &raygenSBTRegion, &missSBTRegion, &hitSBTRegion, &callableSBTRegion, m_params.resX,
                        m_params.resY, 1u);

    // Barrier for the output buffer.
    const auto bufferBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                           &bufferBarrier, 0u, nullptr, 0u, nullptr);

    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    // Verify results.
    std::vector<uint32_t> outputData(m_params.resX * m_params.resY);
    const auto outputModesBufferSizeSz = static_cast<size_t>(outputModesBufferSize);

    invalidateAlloc(vkd, device, outputModesBufferAlloc);
    deMemcpy(outputData.data(), outputModesBufferData, outputModesBufferSizeSz);

    bool matchFlag = true;

    for (int j = 0; j < m_params.resY; ++j)
        for (int i = 0; i < m_params.resX / 2; ++i)
        {
            uint32_t index_ref  = j * m_params.resX + i;
            uint32_t index_test = j * m_params.resX + (i + m_params.resX / 2);

            if (outputData[index_ref] != outputData[index_test])
            {
                matchFlag = false;
            }
        }

    if (m_params.use_shader_invocation_reorder)
    {
        // Using RTIR, the comparison should not match
        if (matchFlag == true)
            TCU_FAIL("Comparison should not match");
        else
            return tcu::TestStatus::pass("Pass");
    }
    else
    {
        // Not using RTIR, the comparison should match
        if (matchFlag == true)
            return tcu::TestStatus::pass("Pass");
        else
            TCU_FAIL("Comparison should match");
    }

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createRTIRActivityTests(tcu::TestContext &testCtx)
{
    // Test shader invocation reorder with ray pipelines
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "rtir_activity"));

    TestParams testParams;

    // Test may not work at lower resolutions
    testParams.use_shader_invocation_reorder = true;
    testParams.resX                          = 512;
    testParams.resY                          = 512;

    group->addChild(new RTIRActivityCase(testCtx, "activity", testParams));

    return group.release();
}

} // namespace RayTracing
} // namespace vkt
