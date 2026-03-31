/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 NVIDIA, Inc.
 * Copyright (c) 2025 The Khronos Group Inc.
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
 * \brief Ray Tracing Execution reordering tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingShaderExecutionReorderTests.hpp"
#include "vkDefs.hpp"
#include "vktTestCase.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkImageUtil.hpp"
#include "vkTypeUtil.hpp"

#include "tcuTextureUtil.hpp"

#include "vkRayTracingUtil.hpp"

#include "deClock.h"

#include <cmath>
#include <limits>
#include <iostream>

namespace vkt
{
namespace RayTracing
{
namespace
{
using namespace vk;
using namespace std;

static const VkFlags ALL_RAY_TRACING_STAGES = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                                              VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
                                              VK_SHADER_STAGE_CALLABLE_BIT_KHR;
static const uint32_t kInstanceCustomIndex = 49;

enum class HitObjectTestType
{
    // Group 1: Hit Object tests
    HIT_OBJECT_GROUP_BEGIN,
    HIT_OBJECT_EMPTY = HIT_OBJECT_GROUP_BEGIN,
    HIT_OBJECT_TRACE_RAY,
    HIT_OBJECT_RECORD_FROM_QUERY_EMPTY,
    HIT_OBJECT_RECORD_FROM_QUERY_PROCEED,
    HIT_OBJECT_RECORD_FROM_QUERY_TRACE_EXECUTE,
    HIT_OBJECT_SET_SBT_RECORD_INDEX,
    HIT_OBJECT_RECORD_MISS,
    HIT_OBJECT_TMIN,
    HIT_OBJECT_TMAX,
    HIT_OBJECT_CUSTOM_INDEX,
    HIT_OBJECT_INSTANCE_ID,
    HIT_OBJECT_GET_TRI_VERTICES,
    HIT_OBJECT_PRIMITIVE_INDEX,
    HIT_OBJECT_GEOMETRY_INDEX,
    HIT_OBJECT_RAY_FLAGS,
    HIT_OBJECT_HIT_KIND,
    HIT_OBJECT_WORLD_RAY_ORIGIN,
    HIT_OBJECT_OBJECT_RAY_ORIGIN,
    HIT_OBJECT_WORLD_RAY_DIRECTION,
    HIT_OBJECT_OBJECT_RAY_DIRECTION,
    HIT_OBJECT_OBJECT_TO_WORLD,
    HIT_OBJECT_WORLD_TO_OBJECT,
    HIT_OBJECT_GET_SBT_RECORD_INDEX,
    HIT_OBJECT_GET_SBT_RECORD_HANDLE,
    HIT_OBJECT_GET_ATTRIBUTE,
    HIT_OBJECT_ARRAY,
    HIT_OBJECT_GET_ATTRIBUTE_FROM_INT_SHADER,
    HIT_OBJECT_GROUP_END = HIT_OBJECT_GET_ATTRIBUTE_FROM_INT_SHADER,

    // Group 2: Reorder tests
    REORDER_GROUP_BEGIN,
    REORDER_HINT = REORDER_GROUP_BEGIN,
    REORDER_HOBJ,
    REORDER_EXECUTE,
    REORDER_EXECUTE_HINT,
    REORDER_HOBJ_HINT,
    REORDER_TRACE_EXECUTE,
    REORDER_TRACE_EXECUTE_HINT,
    TRACE_WITH_AND_WITHOUT_REORDER_HOBJ,
    TRACE_WITH_AND_WITHOUT_REORDER_HINT,
    TRACE_WITH_AND_WITHOUT_REORDER_HOBJ_HINT,
    REORDER_WITH_SUBGROUP,
    REORDER_GROUP_END = REORDER_WITH_SUBGROUP,

    // Group 3: Motion tests
    MOTION_GROUP_BEGIN,
    MOTION_TRACERAY = MOTION_GROUP_BEGIN,
    MOTION_RECORD_MISS,
    MOTION_GET_TIME,
    MOTION_REORDER_EXECUTE,
    MOTION_REORDER_EXECUTE_HINT,
    MOTION_GROUP_END = MOTION_REORDER_EXECUTE_HINT,

    // Group 4: Special large dimension tests
    LARGE_DIM_GROUP_BEGIN,
    LARGE_DIM_X_EXECUTE_HINT = LARGE_DIM_GROUP_BEGIN, // Based on REORDER_EXECUTE_HINT with large X
    LARGE_DIM_Y_HOBJ_HINT,                            // Based on REORDER_HOBJ_HINT with large Y
    LARGE_DIM_Z_HOBJ_HINT,                            // Based on REORDER_HOBJ_HINT with large Z
    LARGE_DIM_GROUP_END = LARGE_DIM_Z_HOBJ_HINT,

    TEST_TYPE_COUNT
};

std::string HitObjectTestNames[] = {
    "empty",
    "trace_ray",
    "record_from_query_empty",
    "record_from_query_proceed",
    "record_from_query_trace_execute",
    "set_sbt_record_index",
    "record_miss",
    "tmin",
    "tmax",
    "custom_index",
    "instance_id",
    "get_tri_vertices",
    "primitive_index",
    "geometry_index",
    "ray_flags",
    "hit_kind",
    "world_ray_origin",
    "object_ray_origin",
    "world_ray_direction",
    "object_ray_direction",
    "object_to_world",
    "world_to_object",
    "get_sbt_record_index",
    "get_sbt_record_handle",
    "get_attribute",
    "array",
    "get_attr_from_intersection",
    "reorder_hint",
    "reorder_hit_object",
    "reorder_execute",
    "reorder_execute_hint",
    "reorder_hit_object_hint",
    "reorder_trace_execute",
    "reorder_trace_execute_hint",
    "trace_with_and_without_reorder_hobj",
    "trace_with_and_without_reorder_hint",
    "trace_with_and_without_reorder_hobj_hint",
    "reorder_subgroup",
    "motion_trace_ray",
    "motion_record_miss",
    "motion_get_time",
    "motion_reorder_execute",
    "motion_reorder_execute_hint",
    "large_dim_X_execute_hint",
    "large_dim_Y_hobj_hint",
    "large_dim_Z_hobj_hint",
};

struct TestParams
{
    HitObjectTestType testType;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
};

uint32_t getShaderGroupSize(const InstanceInterface &vki, const VkPhysicalDevice physicalDevice)
{
    de::MovePtr<RayTracingProperties> rayTracingPropertiesKHR;

    rayTracingPropertiesKHR = makeRayTracingProperties(vki, physicalDevice);
    return rayTracingPropertiesKHR->getShaderGroupHandleSize();
}

uint32_t getShaderGroupBaseAlignment(const InstanceInterface &vki, const VkPhysicalDevice physicalDevice)
{
    de::MovePtr<RayTracingProperties> rayTracingPropertiesKHR;

    rayTracingPropertiesKHR = makeRayTracingProperties(vki, physicalDevice);
    return rayTracingPropertiesKHR->getShaderGroupBaseAlignment();
}

VkImageCreateInfo makeImageCreateInfo(uint32_t width, uint32_t height, VkFormat format)
{
    const VkImageUsageFlags usage =
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        (VkImageCreateFlags)0u,              // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        format,                              // VkFormat format;
        makeExtent3D(width, height, 1u),     // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        usage,                               // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout initialLayout;
    };

    return imageCreateInfo;
}

class RayTracingSERTestInstance : public TestInstance
{
public:
    typedef de::SharedPtr<BottomLevelAccelerationStructure> BlasPtr;
    typedef de::SharedPtr<TopLevelAccelerationStructure> TlasPtr;
    typedef BottomLevelAccelerationStructurePool BlasPool;

    RayTracingSERTestInstance(Context &context, const TestParams &data);
    ~RayTracingSERTestInstance(void);
    tcu::TestStatus iterate(void);

protected:
    void checkSupportInInstance(void) const;
    bool validateBuffer(de::MovePtr<BufferWithMemory> buffer);
    de::MovePtr<BufferWithMemory> runTest();
    TlasPtr initTopAccelerationStructure(const BlasPool &pool);
    void createTopAccelerationStructure(VkCommandBuffer cmdBuffer, TopLevelAccelerationStructure *tlas);
    void initBottomAccelerationStructures(BlasPool &pool) const;
    void initBottomAccelerationStructure(BlasPtr blas) const;
    void initMotionBuffer();

private:
    bool m_isMotion;
    bool m_isAABB;
    de::MovePtr<BufferWithMemory> m_motionBuffer;
    TestParams m_data;
    const VkFormat m_format;
};

RayTracingSERTestInstance::RayTracingSERTestInstance(Context &context, const TestParams &data)
    : vkt::TestInstance(context)
    , m_isMotion(false)
    , m_isAABB(false)
    , m_data(data)
    , m_format(VK_FORMAT_R32_SFLOAT)
{
    if (m_data.testType == HitObjectTestType::MOTION_TRACERAY ||
        m_data.testType == HitObjectTestType::MOTION_RECORD_MISS ||
        m_data.testType == HitObjectTestType::MOTION_GET_TIME ||
        m_data.testType == HitObjectTestType::MOTION_REORDER_EXECUTE ||
        m_data.testType == HitObjectTestType::MOTION_REORDER_EXECUTE_HINT)
    {
        m_isMotion = true;
    }

    if (m_data.testType == HitObjectTestType::HIT_OBJECT_GET_ATTRIBUTE_FROM_INT_SHADER)
    {
        m_isAABB = true;
    }
}

RayTracingSERTestInstance::~RayTracingSERTestInstance(void)
{
}

class RayTracingTestCase : public TestCase
{
public:
    RayTracingTestCase(tcu::TestContext &context, const char *name, const TestParams data);
    ~RayTracingTestCase(void);

    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    TestParams m_data;
};

RayTracingTestCase::RayTracingTestCase(tcu::TestContext &context, const char *name, const TestParams data)
    : vkt::TestCase(context, name)
    , m_data(data)
{
}

RayTracingTestCase::~RayTracingTestCase(void)
{
}

void RayTracingTestCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
    context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");
    context.requireDeviceFunctionality("VK_EXT_ray_tracing_invocation_reorder");

    const VkPhysicalDeviceRayTracingPipelineFeaturesKHR &rayTracingPipelineFeaturesKHR =
        context.getRayTracingPipelineFeatures();
    if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == false)
        TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");

    const VkPhysicalDeviceAccelerationStructureFeaturesKHR &accelerationStructureFeaturesKHR =
        context.getAccelerationStructureFeatures();
    if (accelerationStructureFeaturesKHR.accelerationStructure == false)
        TCU_THROW(TestError, "VK_KHR_ray_tracing_pipeline requires "
                             "VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");

    const VkPhysicalDeviceRayTracingInvocationReorderFeaturesEXT &invocationReorderFeaturesEXT =
        context.getRayTracingInvocationReorderFeaturesEXT();
    if (invocationReorderFeaturesEXT.rayTracingInvocationReorder == false)
        TCU_THROW(TestError, "VK_EXT_ray_tracing_invocation_reorder requires "
                             "VkPhysicalDeviceRayTracingInvocationReorderFeaturesEXT.rayTracingInvocationReorder");

    if (m_data.testType == HitObjectTestType::MOTION_TRACERAY ||
        m_data.testType == HitObjectTestType::MOTION_RECORD_MISS ||
        m_data.testType == HitObjectTestType::MOTION_GET_TIME ||
        m_data.testType == HitObjectTestType::MOTION_REORDER_EXECUTE ||
        m_data.testType == HitObjectTestType::MOTION_REORDER_EXECUTE_HINT)
    {
        context.requireDeviceFunctionality("VK_NV_ray_tracing_motion_blur");
        const VkPhysicalDeviceRayTracingMotionBlurFeaturesNV &motionBlurFeatures =
            context.getRayTracingMotionBlurFeatures();
        if (motionBlurFeatures.rayTracingMotionBlur == false)
            TCU_THROW(NotSupportedError,
                      "Requires VkPhysicalDeviceRayTracingMotionBlurFeaturesNV.rayTracingMotionBlur");
    }

    if (m_data.testType == HitObjectTestType::HIT_OBJECT_GET_TRI_VERTICES)
    {
        context.requireDeviceFunctionality("VK_KHR_ray_tracing_position_fetch");
        const VkPhysicalDeviceRayTracingPositionFetchFeaturesKHR &positionFetchFeatures =
            context.getRayTracingPositionFetchFeatures();
        if (positionFetchFeatures.rayTracingPositionFetch == false)
            TCU_THROW(NotSupportedError,
                      "Requires VkPhysicalDeviceRayTracingPositionFetchFeaturesKHR.rayTracingPositionFetch");
    }

    // For large dimension tests, validate against device limits
    const VkPhysicalDeviceLimits &deviceLimits = context.getDeviceProperties().limits;
    if (HitObjectTestType::LARGE_DIM_GROUP_BEGIN <= m_data.testType &&
        m_data.testType <= HitObjectTestType::LARGE_DIM_GROUP_END)
    {
        const VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps = context.getRayTracingPipelineProperties();

        // Check that the maximum dimension doesn't exceed maxImageDimension2D
        const uint32_t maxDimension = std::max({m_data.width, m_data.height, m_data.depth});
        if (maxDimension > deviceLimits.maxImageDimension2D)
        {
            std::stringstream message;
            message << "Test dimensions (" << m_data.width << "x" << m_data.height << "x" << m_data.depth
                    << ") exceed device max image dimension 2D (" << deviceLimits.maxImageDimension2D << ")";
            TCU_THROW(NotSupportedError, message.str().c_str());
        }

        // Check that the product of width, height and depth doesn't exceed maxRayDispatchInvocationCount
        const uint64_t totalInvocations = static_cast<uint64_t>(m_data.width) * static_cast<uint64_t>(m_data.height) *
                                          static_cast<uint64_t>(m_data.depth);

        if (totalInvocations > rtProps.maxRayDispatchInvocationCount)
        {
            std::stringstream message;
            message << "Test dimensions (" << m_data.width << "x" << m_data.height << "x" << m_data.depth << " = "
                    << totalInvocations << " invocations) exceed device max ray dispatch invocation count ("
                    << rtProps.maxRayDispatchInvocationCount << ")";
            TCU_THROW(NotSupportedError, message.str().c_str());
        }
    }
}

void RayTracingTestCase::initPrograms(SourceCollections &programCollection) const
{
    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
    {
        std::stringstream extensions;
        if (m_data.testType == HitObjectTestType::HIT_OBJECT_RECORD_FROM_QUERY_EMPTY ||
            m_data.testType == HitObjectTestType::HIT_OBJECT_RECORD_FROM_QUERY_PROCEED ||
            m_data.testType == HitObjectTestType::HIT_OBJECT_RECORD_FROM_QUERY_TRACE_EXECUTE ||
            m_data.testType == HitObjectTestType::HIT_OBJECT_SET_SBT_RECORD_INDEX)
        {
            extensions << "#extension GL_EXT_ray_query : require\n";
        }

        if (m_data.testType == HitObjectTestType::HIT_OBJECT_RECORD_FROM_QUERY_EMPTY ||
            m_data.testType == HitObjectTestType::HIT_OBJECT_GET_ATTRIBUTE ||
            m_data.testType == HitObjectTestType::HIT_OBJECT_RECORD_FROM_QUERY_PROCEED ||
            m_data.testType == HitObjectTestType::HIT_OBJECT_RECORD_FROM_QUERY_TRACE_EXECUTE ||
            m_data.testType == HitObjectTestType::HIT_OBJECT_GET_ATTRIBUTE_FROM_INT_SHADER ||
            m_data.testType == HitObjectTestType::HIT_OBJECT_SET_SBT_RECORD_INDEX ||
            m_data.testType == HitObjectTestType::HIT_OBJECT_RECORD_MISS)
        {
            extensions << "layout(location = 0) hitObjectAttributeEXT vec2 attr;\n";
        }

        if (m_data.testType == HitObjectTestType::MOTION_TRACERAY ||
            m_data.testType == HitObjectTestType::MOTION_RECORD_MISS ||
            m_data.testType == HitObjectTestType::MOTION_GET_TIME ||
            m_data.testType == HitObjectTestType::MOTION_REORDER_EXECUTE ||
            m_data.testType == HitObjectTestType::MOTION_REORDER_EXECUTE_HINT)
        {
            extensions << "#extension GL_NV_ray_tracing_motion_blur : require\n";
        }

        if (m_data.testType == HitObjectTestType::HIT_OBJECT_GET_TRI_VERTICES)
        {
            extensions << "#extension GL_EXT_ray_tracing_position_fetch : require\n";
        }

        if (m_data.testType == HitObjectTestType::REORDER_HINT ||
            m_data.testType == HitObjectTestType::REORDER_HOBJ_HINT ||
            m_data.testType == HitObjectTestType::REORDER_EXECUTE_HINT ||
            m_data.testType == HitObjectTestType::LARGE_DIM_Y_HOBJ_HINT ||
            m_data.testType == HitObjectTestType::LARGE_DIM_Z_HOBJ_HINT ||
            m_data.testType == HitObjectTestType::LARGE_DIM_X_EXECUTE_HINT ||
            m_data.testType == HitObjectTestType::TRACE_WITH_AND_WITHOUT_REORDER_HOBJ ||
            m_data.testType == HitObjectTestType::TRACE_WITH_AND_WITHOUT_REORDER_HINT ||
            m_data.testType == HitObjectTestType::TRACE_WITH_AND_WITHOUT_REORDER_HOBJ_HINT)
        {
            extensions << "#extension GL_KHR_shader_subgroup_basic : require\n";
        }

        if (m_data.testType == HitObjectTestType::REORDER_WITH_SUBGROUP)
        {
            extensions << "#extension GL_KHR_shader_subgroup_basic : require\n";
            extensions << "#extension GL_KHR_shader_subgroup_shuffle : require\n";
            extensions << "#extension GL_KHR_shader_subgroup_arithmetic : require\n";
            extensions << "#extension GL_KHR_shader_subgroup_ballot : require\n";
            extensions << "#extension GL_KHR_shader_subgroup_vote : require\n";
        }

        std::stringstream sbtHandle;
        if (m_data.testType == HitObjectTestType::HIT_OBJECT_GET_SBT_RECORD_HANDLE)
        {
            sbtHandle << "#extension GL_EXT_buffer_reference_uvec2 : require\n";
            sbtHandle << "layout(buffer_reference) buffer SBT { uvec4 data; };\n";
        }

        std::stringstream css;
        css << "#version 460 core\n"
               "#extension GL_EXT_ray_tracing : require\n"
               "#extension GL_EXT_shader_invocation_reorder : require\n"
               "layout(r32f, set = 0, binding = 0) uniform image2D result;\n"
               "layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
               "layout(set = 0, binding = 2) buffer StorageBuffer { uint data[]; } storageBuffer;\n"
               "layout(location = 0) rayPayloadEXT vec4 payload;\n";

        css << extensions.str() << sbtHandle.str();

        css << "void main()\n"
               "{\n"
               "  uint  rayFlags = gl_RayFlagsOpaqueEXT;\n"
               "  uint  sbtRecordOffset0 = 0;\n"
               "  uint  sbtRecordOffset1 = 1;\n"
               "  uint  sbtRecordStride = 1;\n"
               "  uint  missIndex0 = 0;\n"
               "  uint  missIndex1 = 1;\n"
               "  uint  cullMask = 0xFF;\n"
               "  float tmin     = 0.5f;\n"
               "  float tmax     = 9.0f;\n"
               "  vec3  origin   = vec3((float(gl_LaunchIDEXT.x) + 0.5f) / float(gl_LaunchSizeEXT.x), "
               "(float(gl_LaunchIDEXT.y) + 0.5f) / float(gl_LaunchSizeEXT.y), 0.0);\n"
               "  vec3  direct   = vec3(0.0, 0.0, -1.0);\n";

        switch (m_data.testType)
        {
        case HitObjectTestType::HIT_OBJECT_EMPTY:
            css << "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  hitObjectRecordEmptyEXT(hObj);\n"
                   "  if (hitObjectIsEmptyEXT(hObj)) color = vec4(2,0,0,1);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_TRACE_RAY:
            css << "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  if (hitObjectIsHitEXT(hObj)) color = vec4(3,0,0,1);\n"
                   "  else if (hitObjectIsMissEXT(hObj)) color = vec4(4,0,0,1);\n"
                   "  else if (hitObjectIsEmptyEXT(hObj)) color = vec4(5,0,0,1); // Must not be empty\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_RECORD_FROM_QUERY_EMPTY:
            css << "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  rayQueryEXT rq;\n"
                   "  rayQueryInitializeEXT(rq, topLevelAS, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
                   "  hitObjectRecordFromQueryEXT(hObj, rq, 0, 0);\n"
                   "  if (hitObjectIsEmptyEXT(hObj)) color = vec4(2,0,0,1);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_RECORD_FROM_QUERY_PROCEED:
            css << "  hitObjectEXT hObj;\n"
                   "  payload = vec4(2,0,0,1);\n"
                   "  rayQueryEXT rq;\n"
                   "  rayQueryInitializeEXT(rq, topLevelAS, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
                   "  rayQueryProceedEXT(rq);\n"
                   "  hitObjectRecordFromQueryEXT(hObj, rq, 0, 0);\n"
                   "  hitObjectExecuteShaderEXT(hObj, 0);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), payload);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_RECORD_FROM_QUERY_TRACE_EXECUTE:
            css << "  hitObjectEXT hObj;\n"
                   "  payload = vec4(1,0,0,1);\n"
                   "  rayQueryEXT rq;\n"
                   "  rayQueryInitializeEXT(rq, topLevelAS, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
                   "  hitObjectRecordFromQueryEXT(hObj, rq, 0, 0);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex1, origin, tmin, direct, tmax, 0);\n"
                   "  hitObjectExecuteShaderEXT(hObj, 0);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), payload);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_SET_SBT_RECORD_INDEX:
            css << "  hitObjectEXT hObj;\n"
                   "  payload = vec4(1,0,0,1);\n"
                   "  rayQueryEXT rq;\n"
                   "  rayQueryInitializeEXT(rq, topLevelAS, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
                   "  hitObjectRecordFromQueryEXT(hObj, rq, 0, 0);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  hitObjectSetShaderBindingTableRecordIndexEXT(hObj, 1); // Switch to index 1\n"
                   "  hitObjectExecuteShaderEXT(hObj, 0);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), payload);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_RECORD_MISS:
            css << "  hitObjectEXT hObj;\n"
                   "  payload = vec4(2,0,0,1);\n"
                   "  hitObjectRecordMissEXT(hObj, rayFlags, missIndex1, origin, tmin, direct, tmax);\n"
                   "  hitObjectExecuteShaderEXT(hObj, 0);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), payload);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_TMIN:
            css << "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  color = vec4(hitObjectGetRayTMinEXT(hObj),0,0,1);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_TMAX:
            css << "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  color = vec4(hitObjectGetRayTMaxEXT(hObj),0,0,1);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_CUSTOM_INDEX:
            css << "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  if (hitObjectIsHitEXT(hObj))\n"
                   "    color = vec4(float(hitObjectGetInstanceCustomIndexEXT(hObj)),0,0,1);\n"
                   "  else\n"
                   "    color = vec4(5,0,0,1);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_INSTANCE_ID:
            css << "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  if (hitObjectIsHitEXT(hObj) && (hitObjectGetInstanceIdEXT(hObj) == 0)) // Only 1 instance\n"
                   "    color = vec4(3,0,0,1);\n"
                   "  else\n"
                   "    color = vec4(4,0,0,1);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_GET_TRI_VERTICES:
            css << "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  if (hitObjectIsHitEXT(hObj) && (hitObjectGetPrimitiveIndexEXT(hObj) == 0))\n"
                   "  {\n"
                   "    vec3 positions[3];\n"
                   "    hitObjectGetIntersectionTriangleVertexPositionsEXT(hObj, positions);\n"
                   "    if (positions[0].x == 0 && positions[0].y == 0 && positions[0].z == -1.0)\n"
                   "      if (positions[1].x == 0.5 && positions[1].y == 0 && positions[1].z == -1.0)\n"
                   "        if (positions[2].x == 0.5 && positions[2].y == 1 && positions[2].z == -1.0)\n"
                   "          color = vec4(3,0,0,1);\n"
                   "  }\n"
                   "  else if (hitObjectIsHitEXT(hObj) && (hitObjectGetPrimitiveIndexEXT(hObj) == 1))\n"
                   "  {\n"
                   "    vec3 positions[3];\n"
                   "    hitObjectGetIntersectionTriangleVertexPositionsEXT(hObj, positions);\n"
                   "    if (positions[0].x == 0 && positions[0].y == 0 && positions[0].z == -1.0)\n"
                   "      if (positions[1].x == 0.5 && positions[1].y == 1 && positions[1].z == -1.0)\n"
                   "        if (positions[2].x == 0 && positions[2].y == 1 && positions[2].z == -1.0)\n"
                   "          color = vec4(3,0,0,1);\n"
                   "  }\n"
                   "  else\n"
                   "    color = vec4(4,0,0,1);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_PRIMITIVE_INDEX:
            css << "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  if (hitObjectIsHitEXT(hObj) && (hitObjectGetPrimitiveIndexEXT(hObj) == 0 || "
                   "hitObjectGetPrimitiveIndexEXT(hObj) == 1)) // Only 2 primitives\n"
                   "    color = vec4(3,0,0,1);\n"
                   "  else\n"
                   "    color = vec4(4,0,0,1);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_GEOMETRY_INDEX:
            css << "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  if (hitObjectIsHitEXT(hObj))\n"
                   "    color = vec4(float(hitObjectGetGeometryIndexEXT(hObj)),0,0,1);\n"
                   "  else\n"
                   "    color = vec4(5,0,0,1);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_RAY_FLAGS:
            css << "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  if (hitObjectGetRayFlagsEXT(hObj) == rayFlags)\n"
                   "    color = vec4(3,0,0,1);\n"
                   "  else\n"
                   "    color = vec4(4,0,0,1);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_HIT_KIND:
            css << "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  if (hitObjectIsHitEXT(hObj))\n"
                   "    color = vec4(float(hitObjectGetHitKindEXT(hObj)),0,0,1);\n"
                   "  else\n"
                   "    color = vec4(5,0,0,1);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_WORLD_RAY_ORIGIN:
            css << "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  vec3 worldOrigin = hitObjectGetWorldRayOriginEXT(hObj);\n"
                   "  if (worldOrigin.x == origin.x && worldOrigin.y == origin.y && worldOrigin.z == origin.z)\n"
                   "    color = vec4(3,0,0,1);\n"
                   "  else\n"
                   "    color = vec4(4,0,0,1);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_OBJECT_RAY_ORIGIN:
            css << "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  vec3 objectOrigin = hitObjectGetObjectRayOriginEXT(hObj);\n"
                   "  if (hitObjectIsHitEXT(hObj) && (objectOrigin.x + 0.5f == origin.x) && (objectOrigin.y == "
                   "origin.y) && (objectOrigin.z == origin.z))\n"
                   "    color = vec4(3,0,0,1);\n"
                   "  else\n"
                   "    color = vec4(4,0,0,1);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_WORLD_RAY_DIRECTION:
            css << "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  vec3 worldDirection = hitObjectGetWorldRayDirectionEXT(hObj);\n"
                   "  if (worldDirection.x == direct.x && worldDirection.y == direct.y && worldDirection.z == "
                   "direct.z)\n"
                   "    color = vec4(3,0,0,1);\n"
                   "  else\n"
                   "    color = vec4(4,0,0,1);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_OBJECT_RAY_DIRECTION:
            css << "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  vec3 objectDirection = hitObjectGetObjectRayDirectionEXT(hObj);\n"
                   "  if (hitObjectIsHitEXT(hObj) && (objectDirection.x == direct.x && objectDirection.y == direct.y "
                   "&& objectDirection.z == direct.z))\n"
                   "    color = vec4(3,0,0,1);\n"
                   "  else\n"
                   "    color = vec4(4,0,0,1);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_OBJECT_TO_WORLD:
            css << "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  mat4x3 otw = hitObjectGetObjectToWorldEXT(hObj);\n"
                   "  bool pass  = (otw[0].x == 1.0) && (otw[0].y == 0.0) && (otw[0].z == 0.0f);\n"
                   "  pass = pass && (otw[1].x == 0.0) && (otw[1].y == 1.0) && (otw[1].z == 0.0f);\n"
                   "  pass = pass && (otw[2].x == 0.0) && (otw[2].y == 0.0) && (otw[2].z == 1.0f);\n"
                   "  pass = pass && (otw[3].x == 0.5) && (otw[3].y == 0.0) && (otw[3].z == 0.0f);\n"
                   "  if (pass && hitObjectIsHitEXT(hObj))\n"
                   "      color = vec4(3,0,0,1);\n"
                   "  if (hitObjectIsMissEXT(hObj))\n"
                   "      color = vec4(4,0,0,1);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_WORLD_TO_OBJECT:
            css << "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  mat4x3 wto = hitObjectGetWorldToObjectEXT(hObj);\n"
                   "  bool pass  = (wto[0].x == 1.0) && (wto[0].y == 0.0) && (wto[0].z == 0.0f);\n"
                   "  pass = pass && (wto[1].x == 0.0) && (wto[1].y == 1.0) && (wto[1].z == 0.0f);\n"
                   "  pass = pass && (wto[2].x == 0.0) && (wto[2].y == 0.0) && (wto[2].z == 1.0f);\n"
                   "  pass = pass && (wto[3].x == 0.0) && (wto[3].y == 0.0) && (wto[3].z == 0.0f);\n"
                   "  if (pass && hitObjectIsHitEXT(hObj))\n"
                   "      color = vec4(3,0,0,1);\n"
                   "  if (hitObjectIsMissEXT(hObj))\n"
                   "      color = vec4(4,0,0,1);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_GET_SBT_RECORD_INDEX:
            css << "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset1, sbtRecordStride, "
                   "missIndex1, origin, tmin, direct, tmax, 0);\n"
                   "  if (hitObjectIsHitEXT(hObj)) { \n"
                   "      if (hitObjectGetShaderBindingTableRecordIndexEXT(hObj) == 1) color = vec4(3,0,0,1);\n" // Uses sbtRecordOffset1
                   "  } else if (hitObjectIsMissEXT(hObj)) { \n"
                   "      if (hitObjectGetShaderBindingTableRecordIndexEXT(hObj) == 1) color = vec4(4,0,0,1);\n" // Uses missIndex1
                   "  }\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_GET_SBT_RECORD_HANDLE:
            css << "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  uvec2 handle = hitObjectGetShaderRecordBufferHandleEXT(hObj);\n"
                   "  uvec4 sbtData = SBT(handle).data;\n"
                   "  if (sbtData.x == 10 && sbtData.y == 20 && sbtData.z == 30 && sbtData.w == 40)\n"
                   "    color = vec4(3,0,0,1);\n"
                   "  else\n"
                   "    color = vec4(4,0,0,1);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_GET_ATTRIBUTE:
            css << "  attr = vec2(10.0);\n"
                   "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  if (hitObjectIsHitEXT(hObj)) { \n"
                   "      hitObjectGetAttributesEXT(hObj, 0);\n"
                   "      if (attr.x >= 0.0 && attr.x <= 1.0 && attr.x >= 0.0 && attr.y <= 1.0) color = "
                   "vec4(3,0,0,1);\n"
                   "  } else { \n"
                   "      color = vec4(4,0,0,1);\n"
                   "  }\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_GET_ATTRIBUTE_FROM_INT_SHADER:
            css << "  attr = vec2(10.0);\n"
                   "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  if (hitObjectIsHitEXT(hObj)) { \n"
                   "      hitObjectGetAttributesEXT(hObj, 0);\n"
                   "  }\n"
                   "  color = vec4(attr.x,0,0,1);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::HIT_OBJECT_ARRAY:
            css << "  hitObjectEXT hObj[5];\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  for (int i = 0; i < 5; i++) {\n"
                   "    hitObjectRecordEmptyEXT(hObj[i]);\n"
                   "  }\n"
                   "  hitObjectTraceRayEXT(hObj[3], topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  if (hitObjectIsHitEXT(hObj[3])) color = vec4(3,0,0,1);\n"
                   "  else if (hitObjectIsMissEXT(hObj[3])) color = vec4(4,0,0,1);\n"
                   "  else if (hitObjectIsEmptyEXT(hObj[3])) color = vec4(5,0,0,1); // Must not be empty\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::REORDER_HINT:
            css << "  hitObjectEXT hObj;\n"
                   "  payload = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset1, sbtRecordStride, "
                   "missIndex1, origin, tmin, direct, tmax, 0);\n"
                   "  reorderThreadEXT(gl_SubgroupInvocationID % 2, 1); \n"
                   "  hitObjectExecuteShaderEXT(hObj, 0);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), payload);\n";
            break;

        case HitObjectTestType::REORDER_HOBJ:
            css << "  hitObjectEXT hObj;\n"
                   "  payload = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  reorderThreadEXT(hObj); \n"
                   "  hitObjectExecuteShaderEXT(hObj, 0);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), payload);\n";
            break;

        case HitObjectTestType::TRACE_WITH_AND_WITHOUT_REORDER_HINT:
            css << "  hitObjectEXT hObj;\n"
                   "  payload = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  hitObjectExecuteShaderEXT(hObj, 0);\n"
                   "  vec4 firstResult = payload;\n"
                   "  payload = vec4(0,0,0,0); // Reset payload between operations\n"
                   "  reorderThreadEXT(gl_SubgroupInvocationID % 2, 1); // Reorder and execute shaders again\n"
                   "  hitObjectExecuteShaderEXT(hObj, 0);\n"
                   "  vec4 secondResult = payload;\n"
                   "  if (firstResult == secondResult) imageStore(result, ivec2(gl_LaunchIDEXT.xy), vec4(3,0,0,1));\n"
                   "  else imageStore(result, ivec2(gl_LaunchIDEXT.xy), vec4(4,0,0,1));\n";
            break;

        case HitObjectTestType::TRACE_WITH_AND_WITHOUT_REORDER_HOBJ:
            css << "  hitObjectEXT hObj;\n"
                   "  payload = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  hitObjectExecuteShaderEXT(hObj, 0);\n"
                   "  vec4 firstResult = payload;\n"
                   "  payload = vec4(0,0,0,0); // Reset payload between operations\n"
                   "  reorderThreadEXT(hObj); // Reorder and execute shaders again\n"
                   "  hitObjectExecuteShaderEXT(hObj, 0);\n"
                   "  vec4 secondResult = payload;\n"
                   "  if (firstResult == secondResult) imageStore(result, ivec2(gl_LaunchIDEXT.xy), vec4(3,0,0,1));\n"
                   "  else imageStore(result, ivec2(gl_LaunchIDEXT.xy), vec4(4,0,0,1));\n";
            break;

        case HitObjectTestType::TRACE_WITH_AND_WITHOUT_REORDER_HOBJ_HINT:
            css << "  hitObjectEXT hObj;\n"
                   "  payload = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  hitObjectExecuteShaderEXT(hObj, 0);\n"
                   "  vec4 firstResult = payload;\n"
                   "  payload = vec4(0,0,0,0); // Reset payload between operations\n"
                   "  reorderThreadEXT(hObj, gl_SubgroupInvocationID % 2, 1); // Reorder and execute shaders again\n"
                   "  hitObjectExecuteShaderEXT(hObj, 0);\n"
                   "  vec4 secondResult = payload;\n"
                   "  if (firstResult == secondResult) imageStore(result, ivec2(gl_LaunchIDEXT.xy), vec4(3,0,0,1));\n"
                   "  else imageStore(result, ivec2(gl_LaunchIDEXT.xy), vec4(4,0,0,1));\n";
            break;

        case HitObjectTestType::REORDER_EXECUTE:
            css << "  hitObjectEXT hObj;\n"
                   "  payload = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  hitObjectReorderExecuteEXT(hObj, 0); \n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), payload);\n";
            break;

        case HitObjectTestType::REORDER_EXECUTE_HINT:
            css << "  hitObjectEXT hObj;\n"
                   "  payload = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, sbtRecordStride, "
                   "missIndex0, origin, tmin, direct, tmax, 0);\n"
                   "  hitObjectReorderExecuteEXT(hObj, int(gl_SubgroupInvocationID % 2), 1, 0); \n" //TODO, remove the int cast after glslang bug is fixed.
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), payload);\n";
            break;

        case HitObjectTestType::REORDER_HOBJ_HINT:
            css << "  hitObjectEXT hObj;\n"
                   "  payload = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset1, sbtRecordStride, "
                   "missIndex1, origin, tmin, direct, tmax, 0);\n"
                   "  reorderThreadEXT(hObj, gl_SubgroupInvocationID % 2, 1); \n"
                   "  hitObjectExecuteShaderEXT(hObj, 0);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), payload);\n";
            break;

        case HitObjectTestType::REORDER_TRACE_EXECUTE:
            css << "  hitObjectEXT hObj;\n"
                   "  payload = vec4(1,0,0,1);\n"
                   "  hitObjectTraceReorderExecuteEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, "
                   "sbtRecordStride, missIndex0, origin, tmin, direct, tmax, 0); \n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), payload);\n";
            break;

        case HitObjectTestType::REORDER_TRACE_EXECUTE_HINT:
            css << "  hitObjectEXT hObj;\n"
                   "  payload = vec4(1,0,0,1);\n"
                   "  hitObjectTraceReorderExecuteEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, "
                   "sbtRecordStride, missIndex0, origin, tmin, direct, tmax, 1, 1, 0); \n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), payload);\n";
            break;

        case HitObjectTestType::REORDER_WITH_SUBGROUP:
            css << "  hitObjectEXT hObj;\n"
                   "  payload = vec4(1,0,0,1);\n"

                   "  uint sid = gl_SubgroupInvocationID;\n"
                   "  uint gid = gl_LaunchIDEXT.x + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.z * "
                   "gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y;\n"

                   "  uint sumBefore = subgroupAdd(gid);\n"
                   "  uint minBefore = subgroupMin(gid);\n"
                   "  uint maxBefore = subgroupMax(gid);\n"
                   "  bool allBefore = subgroupAll(sid < gl_SubgroupSize);\n"

                   "  uvec4 subgroupLtMask1 = gl_SubgroupLtMask;\n"
                   "  uvec4 ballotResult1 = subgroupBallot(sid < gl_SubgroupSize);\n"
                   "  uint precIdBefore = subgroupBallotFindMSB(subgroupBallotBitCount(subgroupLtMask1) == 0 ? "
                   "ballotResult1 : subgroupLtMask1);\n"
                   "  uint shuffleBefore = subgroupShuffle(gid, precIdBefore);\n"
                   "  uint shuffleSumBefore = subgroupAdd(shuffleBefore);\n"
                   "  ivec4 count1 = bitCount(ballotResult1);\n"
                   "  uint ballotBefore = count1.x + count1.y + count1.z + count1.w;\n"

                   // Write subgroup operation's reduced value to first 6 dwords of buffer
                   "  if (subgroupElect()) atomicAdd(storageBuffer.data[0], sumBefore);\n" // Use elect, all threads in subgroup may not be populated
                   "  atomicMin(storageBuffer.data[1], minBefore);\n"
                   "  atomicMax(storageBuffer.data[2], maxBefore);\n"
                   "  atomicAdd(storageBuffer.data[3], uint(allBefore));\n"
                   "  if (subgroupElect()) atomicAdd(storageBuffer.data[4], shuffleSumBefore);\n" // Use elect, all threads in subgroup may not be populated
                   "  if (subgroupElect()) atomicAdd(storageBuffer.data[5], ballotBefore);\n"

                   "  // Reorder threads based on odd/even invocation ID\n"
                   "  reorderThreadEXT(sid % 2, 1);\n"

                   "  uint sumAfter = subgroupAdd(gid);\n"
                   "  uint minAfter = subgroupMin(gid);\n"
                   "  uint maxAfter = subgroupMax(gid);\n"
                   "  bool allAfter = subgroupAll(sid < gl_SubgroupSize);\n"
                   "  uvec4 subgroupLtMask2 = gl_SubgroupLtMask;\n"
                   "  uvec4 ballotResult2 = subgroupBallot(sid < gl_SubgroupSize);\n"
                   "  uint precIdAfter = subgroupBallotFindMSB(subgroupBallotBitCount(subgroupLtMask2) == 0 ? "
                   "ballotResult2 : subgroupLtMask2);\n"
                   "  uint shuffleAfter = subgroupShuffle(gid, precIdAfter);\n"
                   "  uint shuffleSumAfter = subgroupAdd(shuffleAfter);\n"
                   "  ivec4 count2 = bitCount(ballotResult2);\n"
                   "  uint ballotAfter = count2.x + count2.y + count2.z + count2.w;\n"

                   // Write subgroup operation's reduced value after reordering
                   "  if (subgroupElect()) atomicAdd(storageBuffer.data[10], sumAfter);\n"
                   "  atomicMin(storageBuffer.data[11], minAfter);\n"
                   "  atomicMax(storageBuffer.data[12], maxAfter);\n"
                   "  atomicAdd(storageBuffer.data[13], uint(allAfter));\n"
                   "  if (subgroupElect()) atomicAdd(storageBuffer.data[14], shuffleSumAfter);\n"
                   "  if (subgroupElect()) atomicAdd(storageBuffer.data[15], ballotAfter);\n"

                   "  memoryBarrier();\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), payload);\n";
            break;

        case HitObjectTestType::MOTION_TRACERAY:
            css << "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayMotionEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, "
                   "sbtRecordStride, missIndex0, origin, tmin, direct, tmax, 0.25, 0);\n"
                   "  if (hitObjectIsHitEXT(hObj)) color = vec4(3,0,0,1);\n"
                   "  else if (hitObjectIsMissEXT(hObj)) color = vec4(4,0,0,1);\n"
                   "  else if (hitObjectIsEmptyEXT(hObj)) color = vec4(5,0,0,1); // Must not be empty\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::MOTION_RECORD_MISS:
            css << "  hitObjectEXT hObj;\n"
                   "  payload = vec4(2,0,0,1);\n"
                   "  hitObjectRecordMissMotionEXT(hObj, rayFlags, missIndex1, origin, tmin, direct, tmax, 0.25);\n"
                   "  hitObjectExecuteShaderEXT(hObj, 0);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), payload);\n";
            break;

        case HitObjectTestType::MOTION_GET_TIME:
            css << "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayMotionEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, "
                   "sbtRecordStride, missIndex0, origin, tmin, direct, tmax, 0.25, 0);\n"
                   "  float time = hitObjectGetCurrentTimeEXT(hObj);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), vec4(time, 0, 0, 1));\n";
            break;

        case HitObjectTestType::MOTION_REORDER_EXECUTE:
            css << "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  hitObjectTraceMotionReorderExecuteEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, "
                   "sbtRecordStride, missIndex0, origin, tmin, direct, tmax, 0.25, 0);\n"
                   "  if (hitObjectIsHitEXT(hObj)) color = vec4(3,0,0,1);\n"
                   "  else if (hitObjectIsMissEXT(hObj)) color = vec4(4,0,0,1);\n"
                   "  else if (hitObjectIsEmptyEXT(hObj)) color = vec4(5,0,0,1); // Must not be empty\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::MOTION_REORDER_EXECUTE_HINT:
            css << "  hitObjectEXT hObj;\n"
                   "  vec4 color = vec4(1,0,0,1);\n"
                   "  hitObjectTraceMotionReorderExecuteEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset0, "
                   "sbtRecordStride, missIndex0, origin, tmin, direct, tmax, 0.25, 1, 1, 0);\n"
                   "  if (hitObjectIsHitEXT(hObj)) color = vec4(3,0,0,1);\n"
                   "  else if (hitObjectIsMissEXT(hObj)) color = vec4(4,0,0,1);\n"
                   "  else if (hitObjectIsEmptyEXT(hObj)) color = vec4(5,0,0,1); // Must not be empty\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n";
            break;

        case HitObjectTestType::LARGE_DIM_X_EXECUTE_HINT:
            css << "  hitObjectEXT hObj;\n"
                   "  payload = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset1, sbtRecordStride, "
                   "missIndex1, origin, tmin, direct, tmax, 0);\n"
                   "  reorderThreadEXT(gl_SubgroupInvocationID % 2, 1); \n"
                   "  hitObjectExecuteShaderEXT(hObj, 0);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), payload);\n";
            break;

        case HitObjectTestType::LARGE_DIM_Y_HOBJ_HINT:
            css << "  hitObjectEXT hObj;\n"
                   "  payload = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset1, sbtRecordStride, "
                   "missIndex1, origin, tmin, direct, tmax, 0);\n"
                   "  reorderThreadEXT(hObj, gl_SubgroupInvocationID % 2, 1); \n"
                   "  hitObjectExecuteShaderEXT(hObj, 0);\n"
                   "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), payload);\n";
            break;

        case HitObjectTestType::LARGE_DIM_Z_HOBJ_HINT:
            css << "  origin = vec3((float(gl_LaunchIDEXT.x) + 0.5f) / float(gl_LaunchSizeEXT.x), "
                   "(float(gl_LaunchIDEXT.z) + 0.5f) / float(gl_LaunchSizeEXT.z), 0.0);\n"
                   "  hitObjectEXT hObj;\n"
                   "  payload = vec4(1,0,0,1);\n"
                   "  hitObjectTraceRayEXT(hObj, topLevelAS, rayFlags, cullMask, sbtRecordOffset1, sbtRecordStride, "
                   "missIndex1, origin, tmin, direct, tmax, 0);\n"
                   "  reorderThreadEXT(hObj, gl_SubgroupInvocationID % 2, 1); \n"
                   "  hitObjectExecuteShaderEXT(hObj, 0);\n"
                   "  ivec2 imageCoord = ivec2(gl_LaunchIDEXT.x, gl_LaunchIDEXT.z);\n"
                   "  imageStore(result, imageCoord, payload);\n";
            break;

        default:
            DE_ASSERT(0);
        }
        css << "}\n";
        programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
    }

    // Closest hit shader with payload 3
    {
        std::stringstream css;
        css << "#version 460 core\n"
               "#extension GL_EXT_ray_tracing : require\n"
               "layout(location = 0) rayPayloadInEXT vec4 payload;\n"
               "layout(r32f, set = 0, binding = 0) uniform image2D result;\n"
               "void main()\n"
               "{\n"
               "  payload = vec4(3,0,0,1);\n"
               "}\n";
        programCollection.glslSources.add("chit1")
            << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
    }

    // Closest hit shader with payload 7
    {
        std::stringstream css;
        css << "#version 460 core\n"
               "#extension GL_EXT_ray_tracing : require\n"
               "layout(location = 0) rayPayloadInEXT vec4 payload;\n"
               "layout(r32f, set = 0, binding = 0) uniform image2D result;\n"
               "void main()\n"
               "{\n"
               "  payload = vec4(7,0,0,1);\n"
               "}\n";
        programCollection.glslSources.add("chit2")
            << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
    }

    // Miss shader with payload 4
    {
        std::stringstream css;
        css << "#version 460 core\n"
               "#extension GL_EXT_ray_tracing : require\n"
               "layout(location = 0) rayPayloadInEXT vec4 payload;\n"
               "layout(r32f, set = 0, binding = 0) uniform image2D result;\n"
               "void main()\n"
               "{\n"
               "  payload = vec4(4,0,0,1);\n"
               "}\n";
        programCollection.glslSources.add("miss1") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
    }

    // Miss shader with payload 11
    {
        std::stringstream css;
        css << "#version 460 core\n"
               "#extension GL_EXT_ray_tracing : require\n"
               "layout(location = 0) rayPayloadInEXT vec4 payload;\n"
               "layout(r32f, set = 0, binding = 0) uniform image2D result;\n"
               "void main()\n"
               "{\n"
               "  payload = vec4(11,0,0,1);\n"
               "}\n";
        programCollection.glslSources.add("miss2") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
    }

    // Int shader for AABB
    {
        std::stringstream iss;
        iss << "#version 460                                                                   \n"
               "#extension GL_EXT_ray_tracing : require                                        \n"
               "hitAttributeEXT vec2 hitAttr;                                                  \n"
               "                                                                               \n"
               "void main()                                                                    \n"
               "{                                                                              \n"
               "    vec3 origin = gl_WorldRayOriginEXT;                                        \n"
               "    vec3 direction = gl_WorldRayDirectionEXT;                                  \n"
               "                                                                               \n"
               "    vec3 aabbMin = vec3(0, 0, -2.0); // hard-coded for now, must match BLAS    \n"
               "    vec3 aabbMax = vec3(0.5, 1.0, -1.0);  // hard-coded, must match BLAS       \n"
               "                                                                               \n"
               "    const float EPSILON = 1e-8;                                                \n"
               "    float tmin = -1e20;                                                        \n"
               "    float tmax = 1e20;                                                         \n"
               "                                                                               \n"
               "    for (int i = 0; i < 3; ++i)                                                \n"
               "    {                                                                          \n"
               "        float dirComp = direction[i];                                          \n"
               "        float origComp = origin[i];                                            \n"
               "        float minComp = aabbMin[i];                                            \n"
               "        float maxComp = aabbMax[i];                                            \n"
               "                                                                               \n"
               "        if (abs(dirComp) < EPSILON)                                            \n"
               "        {                                                                      \n"
               "            // Ray is parallel to slab; no hit if origin not inside slab       \n"
               "            if (origComp < minComp || origComp > maxComp)                      \n"
               "                return;                                                        \n"
               "        }                                                                      \n"
               "        else                                                                   \n"
               "        {                                                                      \n"
               "            float invD = 1.0 / dirComp;                                        \n"
               "            float t0 = (minComp - origComp) * invD;                            \n"
               "            float t1 = (maxComp - origComp) * invD;                            \n"
               "            if (t0 > t1) { float tmp = t0; t0 = t1; t1 = tmp; }                \n"
               "            tmin = max(tmin, t0);                                              \n"
               "            tmax = min(tmax, t1);                                              \n"
               "            if (tmax < tmin)                                                   \n"
               "                return;                                                        \n"
               "        }                                                                      \n"
               "    }                                                                          \n"
               "                                                                               \n"
               "    if (tmax >= 0.0)                                                           \n"
               "    {                                                                          \n"
               "        hitAttr = vec2(111.0f, 222.0f);                                        \n"
               "        reportIntersectionEXT(tmin, 0);                                        \n"
               "    }                                                                          \n"
               "}                                                                              \n";

        programCollection.glslSources.add("intersection")
            << glu::IntersectionSource(updateRayTracingGLSL(iss.str())) << buildOptions;
    }
}

TestInstance *RayTracingTestCase::createInstance(Context &context) const
{
    return new RayTracingSERTestInstance(context, m_data);
}

auto RayTracingSERTestInstance::initTopAccelerationStructure(const BlasPool &pool) -> TlasPtr
{
    de::MovePtr<TopLevelAccelerationStructure> result = makeTopLevelAccelerationStructure();
    const std::vector<BlasPtr> &blases                = pool.structures();

    result->setInstanceCount(blases.size());
    result->setBuildType(VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR);

    VkTransformMatrixKHR transformMatrix = vk::identityMatrix3x4;

    // Translate in X by 0.5 for some test cases, this shifts the 2 triangles on left to right
    if (m_data.testType == HitObjectTestType::HIT_OBJECT_OBJECT_RAY_ORIGIN ||
        m_data.testType == HitObjectTestType::HIT_OBJECT_OBJECT_TO_WORLD)
    {
        transformMatrix.matrix[0][3] += 0.5f;
    }

    for (size_t instanceNdx = 0; instanceNdx < blases.size(); ++instanceNdx)
    {
        uint32_t instanceShaderBindingTableRecordOffset = 0;
        result->addInstance(blases[instanceNdx], transformMatrix, kInstanceCustomIndex, 0xFF,
                            instanceShaderBindingTableRecordOffset);
    }

    return TlasPtr(result.release());
}

void RayTracingSERTestInstance::createTopAccelerationStructure(VkCommandBuffer cmdBuffer,
                                                               TopLevelAccelerationStructure *tlas)
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    const VkDevice device      = m_context.getDevice();
    Allocator &allocator       = m_context.getDefaultAllocator();

    AccelerationStructBufferProperties bufferProps;
    bufferProps.props.residency = ResourceResidency::TRADITIONAL;

    if (m_isMotion)
    {
        tlas->setCreateFlags(VK_ACCELERATION_STRUCTURE_CREATE_MOTION_BIT_NV);
        tlas->setBuildFlags(VK_BUILD_ACCELERATION_STRUCTURE_MOTION_BIT_NV);
        tlas->setMaxMotionInstances(1u);
    }
    tlas->createAndBuild(vkd, device, cmdBuffer, allocator, bufferProps);
}

void RayTracingSERTestInstance::initMotionBuffer()
{
    if (m_isMotion)
    {
        const DeviceInterface &vkd = m_context.getDeviceInterface();
        const VkDevice device      = m_context.getDevice();
        Allocator &allocator       = m_context.getDefaultAllocator();
        std::vector<tcu::Vec3> motionData;
        motionData.reserve(6u);
        float shift = 2.0f; // data at t=1.0, is shifted by 2
        {
            motionData.push_back(tcu::Vec3(shift + 0.f, 0, -1.0f));
            motionData.push_back(tcu::Vec3(shift + 0.5f, 0, -1.0f));
            motionData.push_back(tcu::Vec3(shift + 0.5f, 1, -1.0f));
        }
        {
            motionData.push_back(tcu::Vec3(shift + 0.f, 0, -1.0f));
            motionData.push_back(tcu::Vec3(shift + 0.5f, 1, -1.0f));
            motionData.push_back(tcu::Vec3(shift + 0.f, 1, -1.0f));
        }
        const VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(
            6u * sizeof(tcu::Vec3), VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

        auto reqs = MemoryRequirement::HostVisible | MemoryRequirement::Coherent | MemoryRequirement::DeviceAddress;
        m_motionBuffer =
            de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, bufferCreateInfo, reqs));

        vk::Allocation &bufAlloc = m_motionBuffer->getAllocation();

        uint8_t *vertexData = (uint8_t *)bufAlloc.getHostPtr();
        deMemcpy(vertexData, motionData.data(), 6u * sizeof(tcu::Vec3));
        flushMappedMemoryRange(vkd, device, bufAlloc.getMemory(), bufAlloc.getOffset(), VK_WHOLE_SIZE);
    }
}

void RayTracingSERTestInstance::initBottomAccelerationStructure(BlasPtr blas) const
{
    blas->setBuildType(VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR);
    blas->setGeometryCount(1);

    std::vector<tcu::Vec3> geometryData;

    geometryData.reserve(6u);

    if (m_isAABB)
    {
        geometryData.push_back(tcu::Vec3(0, 0, -2.0f));     //min, must match value in intersection shader
        geometryData.push_back(tcu::Vec3(0.5, 1.0, -1.0f)); //max, must match value in intersection shader
    }
    else
    {
        {
            geometryData.push_back(tcu::Vec3(0, 0, -1.0f));
            geometryData.push_back(tcu::Vec3(0.5, 0, -1.0f));
            geometryData.push_back(tcu::Vec3(0.5, 1, -1.0f));
        }
        {
            geometryData.push_back(tcu::Vec3(0, 0, -1.0f));
            geometryData.push_back(tcu::Vec3(0.5, 1, -1.0f));
            geometryData.push_back(tcu::Vec3(0, 1, -1.0f));
        }
    }

    if (m_isMotion)
    {
        const DeviceInterface &vkd = m_context.getDeviceInterface();
        const VkDevice device      = m_context.getDevice();

        blas->setBuildFlags(VK_BUILD_ACCELERATION_STRUCTURE_MOTION_BIT_NV);
        VkAccelerationStructureGeometryMotionTrianglesDataNV motionTriangleData = {};
        motionTriangleData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_MOTION_TRIANGLES_DATA_NV;
        motionTriangleData.vertexData.deviceAddress = getBufferDeviceAddress(vkd, device, m_motionBuffer->get(), 0);
        blas->addGeometry(geometryData, true, 0, nullptr, &motionTriangleData);
    }
    else
    {
        blas->addGeometry(geometryData, !m_isAABB);
    }
}

void RayTracingSERTestInstance::initBottomAccelerationStructures(BlasPool &pool) const
{
    const DeviceInterface &vkd     = m_context.getDeviceInterface();
    const VkDevice device          = m_context.getDevice();
    Allocator &allocator           = m_context.getDefaultAllocator();
    const VkDeviceSize maxBuffSize = 3 * (VkDeviceSize(1) << 30); // 3GB

    // Add instance
    pool.add();

    const std::vector<BlasPtr> &blases = pool.structures();

    initBottomAccelerationStructure(blases[0]);

    pool.batchCreateAdjust(vkd, device, allocator, maxBuffSize);
}

de::MovePtr<BufferWithMemory> RayTracingSERTestInstance::runTest()
{
    const InstanceInterface &vki            = m_context.getInstanceInterface();
    const DeviceInterface &vkd              = m_context.getDeviceInterface();
    const VkDevice device                   = m_context.getDevice();
    const VkPhysicalDevice physicalDevice   = m_context.getPhysicalDevice();
    const uint32_t queueFamilyIndex         = m_context.getUniversalQueueFamilyIndex();
    const VkQueue queue                     = m_context.getUniversalQueue();
    Allocator &allocator                    = m_context.getDefaultAllocator();
    const uint32_t shaderGroupHandleSize    = getShaderGroupSize(vki, physicalDevice);
    const uint32_t shaderGroupBaseAlignment = getShaderGroupBaseAlignment(vki, physicalDevice);

    const Move<VkDescriptorSetLayout> descriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, ALL_RAY_TRACING_STAGES)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, ALL_RAY_TRACING_STAGES)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ALL_RAY_TRACING_STAGES)
            .build(vkd, device);
    const Move<VkDescriptorPool> descriptorPool =
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            .addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const Move<VkDescriptorSet> descriptorSet   = makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout);
    const Move<VkPipelineLayout> pipelineLayout = makePipelineLayout(vkd, device, descriptorSetLayout.get());
    const Move<VkCommandPool> cmdPool           = createCommandPool(vkd, device, 0, queueFamilyIndex);
    const Move<VkCommandBuffer> cmdBuffer =
        allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    initMotionBuffer();

    de::MovePtr<RayTracingPipeline> rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();
    Move<VkShaderModule> raygenShader = createShaderModule(vkd, device, m_context.getBinaryCollection().get("rgen"), 0);
    Move<VkShaderModule> chitShader1 = createShaderModule(vkd, device, m_context.getBinaryCollection().get("chit1"), 0);
    Move<VkShaderModule> chitShader2 = createShaderModule(vkd, device, m_context.getBinaryCollection().get("chit2"), 0);
    Move<VkShaderModule> missShader1 = createShaderModule(vkd, device, m_context.getBinaryCollection().get("miss1"), 0);
    Move<VkShaderModule> missShader2 = createShaderModule(vkd, device, m_context.getBinaryCollection().get("miss2"), 0);
    Move<VkShaderModule> intShader =
        createShaderModule(vkd, device, m_context.getBinaryCollection().get("intersection"), 0);

    uint32_t group = 0;
    rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, *raygenShader, group++);

    if (m_isAABB)
    {
        rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, *intShader, group);
    }
    rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, *chitShader1, group);
    group++;

    if (m_isAABB)
    {
        rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, *intShader, group);
    }
    rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, *chitShader2, group);
    group++;

    rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, *missShader1, group++);
    rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, *missShader2, group++);

    if (m_isMotion)
    {
        rayTracingPipeline->setCreateFlags(VK_PIPELINE_CREATE_RAY_TRACING_ALLOW_MOTION_BIT_NV);
    }
    Move<VkPipeline> pipeline = rayTracingPipeline->createPipeline(vkd, device, *pipelineLayout);

    // Add shader record data in the SBT
    std::vector<uint32_t> uintVector = {10, 20, 30, 40};
    std::vector<void *> voidPtrVector;
    for (uint32_t &value : uintVector)
        voidPtrVector.push_back(static_cast<void *>(&value));
    uint32_t shaderRecordSize = m_data.testType == HitObjectTestType::HIT_OBJECT_GET_SBT_RECORD_HANDLE ?
                                    static_cast<uint32_t>(sizeof(uint32_t) * uintVector.size()) :
                                    0;
    const de::MovePtr<BufferWithMemory> raygenShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
        vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0u, 1u, 0u, 0u,
        MemoryRequirement::Any, 0u, 0u, shaderRecordSize, const_cast<const void **>(voidPtrVector.data()));
    const de::MovePtr<BufferWithMemory> hitShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
        vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1u, 2u, 0u, 0u,
        MemoryRequirement::Any, 0u, 0u, shaderRecordSize, const_cast<const void **>(voidPtrVector.data()));
    const de::MovePtr<BufferWithMemory> missShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
        vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 3u, 2u, 0u, 0u,
        MemoryRequirement::Any, 0u, 0u, shaderRecordSize, const_cast<const void **>(voidPtrVector.data()));

    const VkStridedDeviceAddressRegionKHR raygenShaderBindingTableRegion =
        makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(), 0),
                                          shaderGroupHandleSize, 1u * shaderGroupHandleSize);
    const VkStridedDeviceAddressRegionKHR hitShaderBindingTableRegion =
        makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(), 0),
                                          shaderGroupHandleSize, 2u * shaderGroupHandleSize);
    const VkStridedDeviceAddressRegionKHR missShaderBindingTableRegion =
        makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missShaderBindingTable->get(), 0),
                                          shaderGroupHandleSize, 2u * shaderGroupHandleSize);
    const VkStridedDeviceAddressRegionKHR callableShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(0, 0, 0);

    // Determine effective image dimensions - for Z test, use depth as height
    uint32_t effectiveWidth = m_data.width;
    uint32_t effectiveHeight =
        (m_data.testType == HitObjectTestType::LARGE_DIM_Z_HOBJ_HINT) ? m_data.depth : m_data.height;

    const VkImageCreateInfo imageCreateInfo = makeImageCreateInfo(effectiveWidth, effectiveHeight, m_format);
    const VkImageSubresourceRange imageSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0, 1u);
    const de::MovePtr<ImageWithMemory> image = de::MovePtr<ImageWithMemory>(
        new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
    const Move<VkImageView> imageView =
        makeImageView(vkd, device, **image, VK_IMAGE_VIEW_TYPE_2D, m_format, imageSubresourceRange);

    // Adjust buffer size for Z-dimension tests
    const VkDeviceSize bufferSize = effectiveWidth * effectiveHeight * sizeof(float); // Image is VK_FORMAT_R32_SFLOAT

    const VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const VkImageSubresourceLayers bufferImageSubresourceLayers =
        makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);

    // Use the effective dimensions for the copy region
    const VkBufferImageCopy bufferImageRegion =
        makeBufferImageCopy(makeExtent3D(effectiveWidth, effectiveHeight, 1u), bufferImageSubresourceLayers);

    de::MovePtr<BufferWithMemory> buffer = de::MovePtr<BufferWithMemory>(
        new BufferWithMemory(vkd, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));

    // Create storage buffer for binding = 2
    const VkDeviceSize storageBufferSize = 1024;
    const VkBufferCreateInfo storageBufferCreateInfo =
        makeBufferCreateInfo(storageBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    de::MovePtr<BufferWithMemory> storageBuffer = de::MovePtr<BufferWithMemory>(
        new BufferWithMemory(vkd, device, allocator, storageBufferCreateInfo, MemoryRequirement::HostVisible));

    const VkDescriptorImageInfo descriptorImageInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, *imageView, VK_IMAGE_LAYOUT_GENERAL);

    const VkDescriptorBufferInfo storageBufferInfo = makeDescriptorBufferInfo(**storageBuffer, 0, storageBufferSize);
    {
        uint32_t *storageBufferPtr = static_cast<uint32_t *>(storageBuffer->getAllocation().getHostPtr());
        deMemset(storageBufferPtr, 0, storageBufferSize);
        flushMappedMemoryRange(vkd, device, storageBuffer->getAllocation().getMemory(),
                               storageBuffer->getAllocation().getOffset(), storageBufferSize);
    }

    const VkImageMemoryBarrier preImageBarrier =
        makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, **image, imageSubresourceRange);
    const VkImageMemoryBarrier postImageBarrier = makeImageMemoryBarrier(
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, **image, imageSubresourceRange);
    const VkMemoryBarrier postTraceMemoryBarrier =
        makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    const VkMemoryBarrier postCopyMemoryBarrier =
        makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    const VkClearValue clearValue = makeClearValueColorU32(5u, 5u, 5u, 255u);

    qpWatchDog *watchDog = m_context.getTestContext().getWatchDog();
    TlasPtr topLevelAccelerationStructure;
    BottomLevelAccelerationStructurePool blasPool;

    initBottomAccelerationStructures(blasPool);
    blasPool.batchBuild(vkd, device, *cmdPool, queue, watchDog);

    beginCommandBuffer(vkd, *cmdBuffer, 0u);
    {
        cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &preImageBarrier);
        vkd.cmdClearColorImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color, 1,
                               &imageSubresourceRange);
        cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, &postImageBarrier);

        topLevelAccelerationStructure = initTopAccelerationStructure(blasPool);
        createTopAccelerationStructure(*cmdBuffer, topLevelAccelerationStructure.get());

        VkWriteDescriptorSetAccelerationStructureKHR accelerationStructureWriteDescriptorSet = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR, //  VkStructureType sType;
            nullptr,                                                           //  const void* pNext;
            1u,                                                                //  uint32_t accelerationStructureCount;
            topLevelAccelerationStructure->getPtr(), //  const VkAccelerationStructureKHR* pAccelerationStructures;
        };

        DescriptorSetUpdateBuilder()
            .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorImageInfo)
            .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                         VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelerationStructureWriteDescriptorSet)
            .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(2u),
                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &storageBufferInfo)
            .update(vkd, device);

        vkd.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipelineLayout, 0, 1,
                                  &descriptorSet.get(), 0, nullptr);

        vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipeline);

        cmdTraceRays(vkd, *cmdBuffer, &raygenShaderBindingTableRegion, &missShaderBindingTableRegion,
                     &hitShaderBindingTableRegion, &callableShaderBindingTableRegion, m_data.width, m_data.height,
                     m_data.depth);

        cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, &postTraceMemoryBarrier);

        vkd.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **buffer, 1u, &bufferImageRegion);

        cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &postCopyMemoryBarrier);
    }
    endCommandBuffer(vkd, *cmdBuffer);

    submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

    // For subgroups, the results are in storageBuffer
    if (m_data.testType == HitObjectTestType::REORDER_WITH_SUBGROUP)
    {
        invalidateMappedMemoryRange(vkd, device, storageBuffer->getAllocation().getMemory(),
                                    storageBuffer->getAllocation().getOffset(), storageBufferSize);
        return storageBuffer;
    }

    invalidateMappedMemoryRange(vkd, device, buffer->getAllocation().getMemory(), buffer->getAllocation().getOffset(),
                                bufferSize);

    return buffer;
}

void RayTracingSERTestInstance::checkSupportInInstance(void) const
{
    const InstanceInterface &vki                           = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice                  = m_context.getPhysicalDevice();
    de::MovePtr<RayTracingProperties> rayTracingProperties = makeRayTracingProperties(vki, physicalDevice);
}

bool RayTracingSERTestInstance::validateBuffer(de::MovePtr<BufferWithMemory> buffer)
{
    bool passed = true;
    if (m_data.testType == HitObjectTestType::REORDER_WITH_SUBGROUP)
    {
        const uint32_t *bufferPtr    = (uint32_t *)buffer->getAllocation().getHostPtr();
        uint32_t sum                 = (m_data.width * m_data.height) * ((m_data.width * m_data.height) - 1) / 2;
        const uint32_t resultAdd     = sum;
        const uint32_t resultMin     = 0;
        const uint32_t resultMax     = (m_data.width * m_data.height) - 1;
        const uint32_t resultAll     = m_data.width * m_data.height;
        const uint32_t resultShuffle = resultAdd;
        const uint32_t resultBallot  = m_data.width * m_data.height;

        if (bufferPtr[0] != resultAdd || bufferPtr[10] != resultAdd || bufferPtr[1] != resultMin ||
            bufferPtr[11] != resultMin || bufferPtr[2] != resultMax || bufferPtr[12] != resultMax ||
            bufferPtr[3] != resultAll || bufferPtr[13] != resultAll || bufferPtr[4] != resultShuffle ||
            bufferPtr[14] != resultShuffle || bufferPtr[5] != resultBallot || bufferPtr[15] != resultBallot)
        {
            passed = false;
        }
        return passed;
    }

    const float *bufferPtr = (float *)buffer->getAllocation().getHostPtr();
    float anyHitValue      = 0;
    float missValue        = 0;

    switch (m_data.testType)
    {
    case HitObjectTestType::HIT_OBJECT_EMPTY:                   // All cases write 2.0f
    case HitObjectTestType::HIT_OBJECT_RECORD_FROM_QUERY_EMPTY: // All cases write 2.0f
        anyHitValue = 2.0f;
        missValue   = 2.0f;
        break;

    case HitObjectTestType::HIT_OBJECT_RECORD_MISS: // All rays are marked miss with miss shader 1
    case HitObjectTestType::MOTION_RECORD_MISS:     // All rays are marked miss with miss shader 1
        anyHitValue = 11.0f;
        missValue   = 11.0f;
        break;

    case HitObjectTestType::MOTION_GET_TIME: // All rays have the same t value
        anyHitValue = 0.25f;
        missValue   = 0.25f;
        break;
    case HitObjectTestType::HIT_OBJECT_TMAX: // Hit happens at t=1.0, miss at t=tmax
        anyHitValue = 1.0f;
        missValue   = 9.0f;
        break;

    case HitObjectTestType::HIT_OBJECT_TMIN: // All rays should have tmin = 0.5
        anyHitValue = 0.5f;
        missValue   = 0.5f;
        break;

    case HitObjectTestType::HIT_OBJECT_RECORD_FROM_QUERY_PROCEED:
        anyHitValue = 3.0f; //Hit object
        missValue   = 2.0f; //Empty object, miss shader should not be called
        break;

    case HitObjectTestType::HIT_OBJECT_RECORD_FROM_QUERY_TRACE_EXECUTE:
        anyHitValue = 3.0f;  // Hit shader
        missValue   = 11.0f; // Miss shader at index 1
        break;

    case HitObjectTestType::HIT_OBJECT_SET_SBT_RECORD_INDEX:
        anyHitValue = 7.0f;  // Hit shader
        missValue   = 11.0f; // Miss shader at index 1
        break;

    case HitObjectTestType::HIT_OBJECT_CUSTOM_INDEX:
        anyHitValue = float(kInstanceCustomIndex); // Hit rays should have custom ID
        missValue   = 5.0f;
        break;

    case HitObjectTestType::HIT_OBJECT_GEOMETRY_INDEX:
        anyHitValue = 0.0f; // Hit rays should have geometry ID
        missValue   = 5.0f;
        break;

    case HitObjectTestType::HIT_OBJECT_HIT_KIND:
        anyHitValue = float(0xFE); // Hit rays write gl_HitKindFrontFacingTriangleEXT
        missValue   = 5.0f;
        break;

    case HitObjectTestType::HIT_OBJECT_WORLD_RAY_ORIGIN:    // All rays should have same origin
    case HitObjectTestType::HIT_OBJECT_WORLD_RAY_DIRECTION: // All rays should have same direction
    case HitObjectTestType::HIT_OBJECT_RAY_FLAGS:           // All rays should have same flags
    case HitObjectTestType::REORDER_WITH_SUBGROUP:
    case HitObjectTestType::HIT_OBJECT_GET_SBT_RECORD_HANDLE:
    case HitObjectTestType::TRACE_WITH_AND_WITHOUT_REORDER_HOBJ:
    case HitObjectTestType::TRACE_WITH_AND_WITHOUT_REORDER_HINT:
    case HitObjectTestType::TRACE_WITH_AND_WITHOUT_REORDER_HOBJ_HINT:
        anyHitValue = 3.0f;
        missValue   = 3.0f;
        break;

    case HitObjectTestType::HIT_OBJECT_OBJECT_RAY_ORIGIN:
    case HitObjectTestType::HIT_OBJECT_OBJECT_TO_WORLD:
        // Due to the +X shift in the transformation, the hit rays appear in the right half
        // so hit/miss values get reversed for the check below
        anyHitValue = 4.0f;
        missValue   = 3.0f;
        break;

    case HitObjectTestType::HIT_OBJECT_GET_ATTRIBUTE_FROM_INT_SHADER:
        anyHitValue = 111.0f; // written in intersection shader
        missValue   = 10.0f;  //attr is initialized to 10.0
        break;

    case HitObjectTestType::HIT_OBJECT_TRACE_RAY:
    case HitObjectTestType::HIT_OBJECT_ARRAY:
    case HitObjectTestType::HIT_OBJECT_INSTANCE_ID:
    case HitObjectTestType::HIT_OBJECT_PRIMITIVE_INDEX:
    case HitObjectTestType::HIT_OBJECT_OBJECT_RAY_DIRECTION:
    case HitObjectTestType::HIT_OBJECT_WORLD_TO_OBJECT:
    case HitObjectTestType::HIT_OBJECT_GET_SBT_RECORD_INDEX:
    case HitObjectTestType::HIT_OBJECT_GET_ATTRIBUTE:
    case HitObjectTestType::HIT_OBJECT_GET_TRI_VERTICES:
    case HitObjectTestType::REORDER_HOBJ:
    case HitObjectTestType::REORDER_EXECUTE:
    case HitObjectTestType::REORDER_EXECUTE_HINT:
    case HitObjectTestType::REORDER_TRACE_EXECUTE:
    case HitObjectTestType::REORDER_TRACE_EXECUTE_HINT:
        anyHitValue = 3.0f;
        missValue   = 4.0f;
        break;

    case HitObjectTestType::REORDER_HINT:
    case HitObjectTestType::REORDER_HOBJ_HINT:
    case HitObjectTestType::LARGE_DIM_X_EXECUTE_HINT:
    case HitObjectTestType::LARGE_DIM_Y_HOBJ_HINT:
    case HitObjectTestType::LARGE_DIM_Z_HOBJ_HINT:
        anyHitValue = 7.0f;  // The closest hit shader is at sbtRecordOffset1
        missValue   = 11.0f; // The miss shader is at missIndex1
        break;

    case HitObjectTestType::MOTION_TRACERAY:
    case HitObjectTestType::MOTION_REORDER_EXECUTE:
    case HitObjectTestType::MOTION_REORDER_EXECUTE_HINT:
        // Motion variants shift the 2 triangles from left to right, so hit/miss values are reversed for check below
        anyHitValue = 4.0f;
        missValue   = 3.0f;
        break;

    default:
        DE_ASSERT(0);
        break;
    }

    // Calculate the actual dimensions to validate based on test type
    uint32_t validateWidth = m_data.width;
    uint32_t validateHeight =
        m_data.testType == HitObjectTestType::LARGE_DIM_Z_HOBJ_HINT ? m_data.depth : m_data.height;

    const float epsilon = 1e-6f;
    for (uint32_t y = 0; y < validateHeight && passed; ++y)
    {
        for (uint32_t x = 0; x < validateWidth && passed; ++x)
        {
            uint32_t offset    = y * validateWidth + x;
            const float result = bufferPtr[offset];

            if (x < validateWidth / 2)
            {
                if (std::abs(result - anyHitValue) > epsilon)
                {
                    m_context.getTestContext().getLog()
                        << tcu::TestLog::Message << "Got " << result << ", expected " << anyHitValue << " at (" << x
                        << ", " << y << ")" << tcu::TestLog::EndMessage;
                    passed = false;
                    break;
                }
            }
            else
            {
                if (std::abs(result - missValue) > epsilon)
                {
                    m_context.getTestContext().getLog()
                        << tcu::TestLog::Message << "Got " << result << ", expected " << missValue << " at (" << x
                        << ", " << y << ")" << tcu::TestLog::EndMessage;
                    passed = false;
                    break;
                }
            }
        }
    }

    return passed;
}

tcu::TestStatus RayTracingSERTestInstance::iterate(void)
{
    checkSupportInInstance();
    const bool passed = validateBuffer(runTest());
    return (passed) ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail("Failed");
}

} // namespace

tcu::TestCaseGroup *createShaderExecutionReorderTests(tcu::TestContext &testCtx)
{
    // Ray tracing SER tests
    de::MovePtr<tcu::TestCaseGroup> serGroup(new tcu::TestCaseGroup(testCtx, "ser"));

    // Group definitions from addBuiltInTests function
    struct TestGroup
    {
        const char *name;
        HitObjectTestType begin;
        HitObjectTestType end;
    };

    const TestGroup groups[] = {
        {"builtin_var", HitObjectTestType::HIT_OBJECT_GROUP_BEGIN, HitObjectTestType::HIT_OBJECT_GROUP_END},
        {"reorder", HitObjectTestType::REORDER_GROUP_BEGIN, HitObjectTestType::REORDER_GROUP_END},
        {"motion", HitObjectTestType::MOTION_GROUP_BEGIN, HitObjectTestType::MOTION_GROUP_END},
        {"large_dim", HitObjectTestType::LARGE_DIM_GROUP_BEGIN, HitObjectTestType::LARGE_DIM_GROUP_END}};

    for (size_t groupIdx = 0; groupIdx < DE_LENGTH_OF_ARRAY(groups); ++groupIdx)
    {
        // Create a group for each test category
        string groupName = groups[groupIdx].name;
        string groupDesc = "Check " + groupName + " in VK_EXT_ray_tracing_invocation_reorder";
        de::MovePtr<tcu::TestCaseGroup> tcGroup(new tcu::TestCaseGroup(testCtx, groupName.c_str()));

        // Create a special addTests function that only adds tests for this specific group
        auto addTestsToGroup = [&testCtx, &groups, groupIdx](tcu::TestCaseGroup *group)
        {
            // Define our own helper to only add tests for the current group
            struct TestGroupSingle
            {
                const char *name;
                HitObjectTestType begin;
                HitObjectTestType end;
            };

            const TestGroupSingle singleGroup = {groups[groupIdx].name, groups[groupIdx].begin, groups[groupIdx].end};

            // Regular test dimensions
            uint32_t launchSizeX = 160;
            uint32_t launchSizeY = 91;

            // Add tests just for this group's range - directly to the parent group
            for (uint32_t t = static_cast<uint32_t>(singleGroup.begin); t <= static_cast<uint32_t>(singleGroup.end);
                 ++t)
            {
                // Special handling for large_dim group
                if (singleGroup.begin == HitObjectTestType::LARGE_DIM_GROUP_BEGIN)
                {
                    if (t == static_cast<uint32_t>(HitObjectTestType::LARGE_DIM_X_EXECUTE_HINT))
                    {
                        const TestParams testParams = {static_cast<HitObjectTestType>(t),
                                                       15210, // Large X
                                                       23,    // Small Y
                                                       1};
                        std::string testName        = HitObjectTestNames[t] + "_15210x23x1";
                        group->addChild(new RayTracingTestCase(testCtx, testName.c_str(), testParams));
                    }
                    else if (t == static_cast<uint32_t>(HitObjectTestType::LARGE_DIM_Y_HOBJ_HINT))
                    {
                        const TestParams testParams = {static_cast<HitObjectTestType>(t),
                                                       44,    // Small X
                                                       15181, // Large Y
                                                       1};
                        std::string testName        = HitObjectTestNames[t] + "_44x15181x1";
                        group->addChild(new RayTracingTestCase(testCtx, testName.c_str(), testParams));
                    }
                    else if (t == static_cast<uint32_t>(HitObjectTestType::LARGE_DIM_Z_HOBJ_HINT))
                    {
                        const TestParams testParams = {
                            static_cast<HitObjectTestType>(t),
                            44,   // Small X
                            1,    // Y = 1
                            17233 // Large Z
                        };
                        std::string testName = HitObjectTestNames[t] + "_44x1x17233";
                        group->addChild(new RayTracingTestCase(testCtx, testName.c_str(), testParams));
                    }
                }
                else // Use standard dimensions for other groups
                {

                    // Even sizes makes checking subgroup operations results easier
                    if (t == static_cast<uint32_t>(HitObjectTestType::REORDER_WITH_SUBGROUP))
                    {
                        launchSizeX = 256;
                        launchSizeY = 64;
                    }

                    const TestParams testParams = {
                        static_cast<HitObjectTestType>(t),
                        launchSizeX, // width
                        launchSizeY, // height
                        1            // depth (default)
                    };
                    const std::string sizeSuffix = "_" + de::toString(launchSizeX) + "x" + de::toString(launchSizeY);
                    std::string testName         = HitObjectTestNames[t] + sizeSuffix;
                    group->addChild(new RayTracingTestCase(testCtx, testName.c_str(), testParams));
                }
            }
        };

        // Add tests for just this group
        addTestsToGroup(tcGroup.get());

        // Add this group to the main ser group
        serGroup->addChild(tcGroup.release());
    }

    return serGroup.release();
}

} // namespace RayTracing
} // namespace vkt
