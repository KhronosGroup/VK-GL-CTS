/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2018-2025 NVIDIA Corporation
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
 * \brief Test pipeline creation with no queues
 *//*--------------------------------------------------------------------*/

#include "vktPipelineNoQueuesTests.hpp"

#include "util/vktShaderObjectUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkRayTracingUtil.hpp"
#include "vkPipelineBinaryUtil.hpp"

#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"
#include "vkDeviceFeatures.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "deDefs.h"
#include "deFloat16.h"
#include "deMath.h"
#include "deRandom.h"
#include "deSharedPtr.hpp"
#include "deString.h"

#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"

#include <string>
#include <sstream>
#include <set>
#include <algorithm>

namespace vkt
{
namespace no_queues
{
namespace
{
using namespace vk;
using namespace std;
using tcu::TestLog;

typedef enum
{
    TT_PIPELINE_CACHE = 0,
    TT_PIPELINE_BINARY,
    TT_SHADER_BINARY,
} TestType;

typedef enum
{
    STAGE_COMPUTE = 0,
    STAGE_RAYGEN,
    STAGE_INTERSECT,
    STAGE_ANY_HIT,
    STAGE_CLOSEST_HIT,
    STAGE_MISS,
    STAGE_CALLABLE,
    STAGE_VERTEX,
    STAGE_FRAGMENT,
    STAGE_GEOMETRY,
    STAGE_TESS_CTRL,
    STAGE_TESS_EVAL,
    STAGE_TASK,
    STAGE_MESH,
} Stage;

struct CaseDef
{
    Stage stage;
    TestType testType;
    uint32_t threadsPerWorkgroupX;
    uint32_t threadsPerWorkgroupY;
    uint32_t workgroupsX;
    uint32_t workgroupsY;
};

bool isRayTracingStageKHR(const Stage stage)
{
    switch (stage)
    {
    case STAGE_RAYGEN:
    case STAGE_INTERSECT:
    case STAGE_ANY_HIT:
    case STAGE_CLOSEST_HIT:
    case STAGE_MISS:
    case STAGE_CALLABLE:
        return true;

    default:
        return false;
    }
}

bool isMeshStage(Stage stage)
{
    return (stage == STAGE_TASK || stage == STAGE_MESH);
}

bool isTessStage(Stage stage)
{
    return (stage == STAGE_TESS_CTRL || stage == STAGE_TESS_EVAL);
}

bool isGeomStage(Stage stage)
{
    return (stage == STAGE_GEOMETRY);
}

static const VkFlags ALL_RAY_TRACING_STAGES = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                                              VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
                                              VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR;

VkShaderStageFlags getAllShaderStagesFor(Stage stage)
{
    if (isRayTracingStageKHR(stage))
        return ALL_RAY_TRACING_STAGES;

    if (isMeshStage(stage))
        return (VK_SHADER_STAGE_MESH_BIT_EXT | ((stage == STAGE_TASK) ? VK_SHADER_STAGE_TASK_BIT_EXT : 0));

    return (VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_ALL_GRAPHICS);
}

VkShaderStageFlagBits getShaderStageFlag(const Stage stage)
{
    switch (stage)
    {
    case STAGE_RAYGEN:
        return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    case STAGE_ANY_HIT:
        return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    case STAGE_CLOSEST_HIT:
        return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    case STAGE_MISS:
        return VK_SHADER_STAGE_MISS_BIT_KHR;
    case STAGE_INTERSECT:
        return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
    case STAGE_CALLABLE:
        return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
    default:
        TCU_THROW(InternalError, "Unknown stage specified");
    }
}

bool usesAccelerationStructure(const Stage stage)
{
    return (isRayTracingStageKHR(stage) && stage != STAGE_RAYGEN && stage != STAGE_CALLABLE);
}

class NoQueuesTestInstance : public TestInstance
{
public:
    NoQueuesTestInstance(Context &context, const CaseDef &data);
    ~NoQueuesTestInstance(void);
    tcu::TestStatus iterate(void);

private:
    CaseDef m_data;
};

NoQueuesTestInstance::NoQueuesTestInstance(Context &context, const CaseDef &data)
    : vkt::TestInstance(context)
    , m_data(data)
{
}

NoQueuesTestInstance::~NoQueuesTestInstance(void)
{
}

class NoQueuesTestCase : public TestCase
{
public:
    NoQueuesTestCase(tcu::TestContext &context, const char *name, const CaseDef data);
    ~NoQueuesTestCase(void);
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    CaseDef m_data;
};

NoQueuesTestCase::NoQueuesTestCase(tcu::TestContext &context, const char *name, const CaseDef data)
    : vkt::TestCase(context, name)
    , m_data(data)
{
}

NoQueuesTestCase::~NoQueuesTestCase(void)
{
}

void NoQueuesTestCase::checkSupport(Context &context) const
{
    if (!context.contextSupports(vk::ApiVersion(0, 1, 1, 0)))
    {
        TCU_THROW(NotSupportedError, "Vulkan 1.1 not supported");
    }

    if (isRayTracingStageKHR(m_data.stage))
    {
        context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
        context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

        const VkPhysicalDeviceRayTracingPipelineFeaturesKHR &rayTracingPipelineFeaturesKHR =
            context.getRayTracingPipelineFeatures();
        if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == false)
            TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");

        const VkPhysicalDeviceAccelerationStructureFeaturesKHR &accelerationStructureFeaturesKHR =
            context.getAccelerationStructureFeatures();
        if (accelerationStructureFeaturesKHR.accelerationStructure == false)
            TCU_THROW(TestError, "VK_KHR_ray_tracing_pipeline requires "
                                 "VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");
    }

    if (isMeshStage(m_data.stage))
    {
        const auto &meshFeatures = context.getMeshShaderFeaturesEXT();

        if (!meshFeatures.meshShader)
            TCU_THROW(NotSupportedError, "Mesh shaders not supported");

        if (m_data.stage == STAGE_TASK && !meshFeatures.taskShader)
            TCU_THROW(NotSupportedError, "Task shaders not supported");
    }

    const auto &features = context.getDeviceFeatures();

    if (isGeomStage(m_data.stage) && !features.geometryShader)
        TCU_THROW(NotSupportedError, "Geometry shader not supported");

    if (isTessStage(m_data.stage) && !features.tessellationShader)
        TCU_THROW(NotSupportedError, "Tessellation shaders not supported");

    if ((isTessStage(m_data.stage) || m_data.stage == Stage::STAGE_VERTEX) && !features.vertexPipelineStoresAndAtomics)
        TCU_THROW(NotSupportedError, "SSBO writes not supported in vertex pipeline");

    if (m_data.stage == Stage::STAGE_FRAGMENT && !features.fragmentStoresAndAtomics)
        TCU_THROW(NotSupportedError, "SSBO writes not supported in fragment shader");

    if (m_data.testType == TT_PIPELINE_BINARY)
    {
        context.requireDeviceFunctionality("VK_KHR_pipeline_binary");
    }
    if (m_data.testType == TT_SHADER_BINARY)
    {
        context.requireDeviceFunctionality("VK_EXT_shader_object");
    }
    context.requireDeviceFunctionality("VK_KHR_maintenance9");
}

void NoQueuesTestCase::initPrograms(SourceCollections &programCollection) const
{
    std::stringstream css;
    css << "#version 460 core\n";
    css << "#pragma use_vulkan_memory_model\n";
    css << "#extension GL_KHR_shader_subgroup_basic : enable\n"
           "#extension GL_KHR_memory_scope_semantics : enable\n"
           "#extension GL_EXT_nonuniform_qualifier : enable\n"

           "#extension GL_EXT_shader_explicit_arithmetic_types : enable\n"
           "#extension GL_EXT_buffer_reference : enable\n"
           "#extension GL_EXT_ray_tracing : enable\n"
           "#extension GL_EXT_control_flow_attributes : enable\n";

    switch (m_data.stage)
    {
    case STAGE_COMPUTE:
        css << "layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z = 1) in;\n";
        break;
    case STAGE_INTERSECT:
        css << "hitAttributeEXT vec3 hitAttribute;\n";
        break;
    case STAGE_ANY_HIT:
    case STAGE_CLOSEST_HIT:
        css << "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
               "hitAttributeEXT vec3 hitAttribute;\n";
        break;
    case STAGE_MISS:
        css << "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n";
        break;
    case STAGE_CALLABLE:
        css << "layout(location = 0) callableDataInEXT float dummy;\n";
        break;
    case STAGE_MESH:
    case STAGE_TASK:
        css << "#extension GL_EXT_mesh_shader : enable\n";
        css << "layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z = 1) in;\n";
        break;
    case STAGE_GEOMETRY:
        css << "layout (triangles) in;\n"
            << "layout (triangle_strip, max_vertices=3) out;\n"
            << "layout (invocations = " << m_data.threadsPerWorkgroupX << ") in;\n";
        break;
    case STAGE_TESS_CTRL:
        css << "layout (vertices = " << m_data.threadsPerWorkgroupX << ") out;\n";
        break;
    case STAGE_TESS_EVAL:
        css << "layout (quads, equal_spacing, cw) in;\n";
        break;
    default:
        break;
    }

    css << "const int workgroupsX = " << m_data.workgroupsX << ";\n";

    css << "layout(set=0, binding=0) uniform sampler2D tex;\n";
    css << "layout(set=0, binding=3) coherent buffer Output { float x[]; } outputO;\n";

    css << "layout(constant_id = 2) const uint width = 0;\n";

    switch (m_data.stage)
    {
    case STAGE_MESH:
        css << "layout(triangles) out;\n"
            << "layout(max_vertices=3, max_primitives=1) out;\n";
        // fallthrough
    case STAGE_TASK:
    case STAGE_COMPUTE:
        css << "uint globalInvocationIndex = gl_LocalInvocationIndex + "
               "gl_WorkGroupSize.x*gl_WorkGroupSize.y*(gl_WorkGroupID.x + gl_WorkGroupID.y*gl_NumWorkGroups.x);\n";
        break;
    case STAGE_VERTEX:
        css << "uint globalInvocationIndex = gl_VertexIndex;\n";
        break;
    case STAGE_FRAGMENT:
        css << "uint globalInvocationIndex = width*uint(gl_FragCoord.y) + uint(gl_FragCoord.x);\n";
        break;
    case STAGE_GEOMETRY:
        css << "uint globalInvocationIndex = " << m_data.threadsPerWorkgroupX
            << " * gl_PrimitiveIDIn + gl_InvocationID;\n";
        break;
    case STAGE_TESS_CTRL:
        css << "uint globalInvocationIndex = gl_PatchVerticesIn * gl_PrimitiveID + gl_InvocationID;\n";
        break;
    case STAGE_TESS_EVAL:
        // One 32x1 "workgroup" per tessellated quad. But we skip storing the results for some threads.
        css << "uint globalInvocationIndex = " << m_data.threadsPerWorkgroupX
            << " * gl_PrimitiveID + uint(round(gl_TessCoord.x * " << m_data.threadsPerWorkgroupX << "));\n";
        break;
    case STAGE_RAYGEN:
    case STAGE_INTERSECT:
    case STAGE_ANY_HIT:
    case STAGE_CLOSEST_HIT:
    case STAGE_MISS:
    case STAGE_CALLABLE:
        css << "uint globalInvocationIndex = gl_LaunchIDEXT.x + gl_LaunchIDEXT.y*gl_LaunchSizeEXT.x;\n";
        break;
    default:
        TCU_THROW(InternalError, "Unknown stage");
    }

    css << "void main()\n"
           "{\n";

    if (m_data.stage == STAGE_TESS_EVAL)
    {
        // We tessellate with an outer level of 32. The threads we want "in the workgroup"
        // are those on the edge, with coord.x < 1 (the first 32).
        css << "   bool dontLoadStore = false;\n"
               "   if (gl_TessCoord.y != 0 || gl_TessCoord.x == 1) { dontLoadStore = true; globalInvocationIndex = 0; "
               "}\n"
               "   if (!dontLoadStore) {\n";
    }

    if (m_data.stage == STAGE_TESS_EVAL)
    {
        css << "   }\n";
    }

    if (m_data.stage == STAGE_TESS_EVAL)
    {
        css << "   if (!dontLoadStore) {\n";
    }

    // The texture fetch should return the border color - VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
    css << "   if (texture(tex, vec2(-1,-1)) == vec4(1, 1, 1, 1)) {\n";
    css << "       outputO.x[globalInvocationIndex] = 1.0;\n";
    css << "   }\n";

    if (m_data.stage == STAGE_TESS_EVAL)
    {
        css << "   }\n";
    }

    switch (m_data.stage)
    {
    case STAGE_INTERSECT:
        css << "  hitAttribute = vec3(0.0f, 0.0f, 0.0f);\n"
               "  reportIntersectionEXT(1.0f, 0);\n";
        break;
    case STAGE_VERTEX:
        css << "  gl_PointSize = 1.0f;\n";
        break;
    case STAGE_TASK:
        css << "  EmitMeshTasksEXT(0, 0, 0);\n";
        break;
    default:
        break;
    }

    css << "}\n";

    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u);

    switch (m_data.stage)
    {
    case STAGE_COMPUTE:
        programCollection.glslSources.add("test") << glu::ComputeSource(css.str()) << buildOptions;
        break;
    case STAGE_VERTEX:
        programCollection.glslSources.add("test") << glu::VertexSource(css.str()) << buildOptions;
        break;
    case STAGE_FRAGMENT:
    {
        std::stringstream vss;
        vss << "#version 450 core\n"
               "void main()\n"
               "{\n"
               // full-viewport quad
               "  gl_Position = vec4( 2.0*float(gl_VertexIndex&2) - 1.0, 4.0*(gl_VertexIndex&1)-1.0, 1.0 - 2.0 * "
               "float(gl_VertexIndex&1), 1);\n"
               "}\n";
        programCollection.glslSources.add("vert") << glu::VertexSource(vss.str());

        programCollection.glslSources.add("test") << glu::FragmentSource(css.str()) << buildOptions;
    }
    break;
    case STAGE_GEOMETRY:
    {
        std::stringstream vss;
        vss << "#version 450 core\n"
               "void main()\n"
               "{\n"
               "  gl_Position = vec4(0,0,0,1);\n"
               "}\n";
        programCollection.glslSources.add("vert") << glu::VertexSource(vss.str());
        programCollection.glslSources.add("test") << glu::GeometrySource(css.str()) << buildOptions;
    }
    break;
    case STAGE_TESS_CTRL:
    {
        std::stringstream vss;
        vss << "#version 450 core\n"
               "void main()\n"
               "{\n"
               "  gl_Position = vec4(0,0,0,1);\n"
               "}\n";
        programCollection.glslSources.add("vert") << glu::VertexSource(vss.str());

        std::stringstream tss;
        tss << "#version 450 core\n"
               "layout (triangles, equal_spacing, cw) in;\n"
               "void main()\n"
               "{\n"
               "}\n";
        programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(tss.str());

        programCollection.glslSources.add("tesc") << glu::TessellationControlSource(css.str()) << buildOptions;
    }
    break;
    case STAGE_TESS_EVAL:
    {
        std::stringstream vss;
        vss << "#version 450 core\n"
               "void main()\n"
               "{\n"
               "  gl_Position = vec4(0,0,0,1);\n"
               "}\n";
        programCollection.glslSources.add("vert") << glu::VertexSource(vss.str());

        std::stringstream tss;
        tss << "#version 450 core\n"
               "layout (vertices = 4) out;\n"
               "void main()\n"
               "{\n"
               "  gl_TessLevelInner[0] = 1.0;\n"
               "  gl_TessLevelInner[1] = 1.0;\n"
               "  gl_TessLevelOuter[0] = 1.0;\n"
               "  gl_TessLevelOuter[1] = "
            << m_data.threadsPerWorkgroupX
            << ";\n"
               "  gl_TessLevelOuter[2] = 1.0;\n"
               "  gl_TessLevelOuter[3] = "
            << m_data.threadsPerWorkgroupX
            << ";\n"
               "}\n";
        programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tss.str());

        programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(css.str()) << buildOptions;
    }
    break;
    case STAGE_TASK:
    {
        programCollection.glslSources.add("test") << glu::TaskSource(css.str()) << buildOptions;

        std::stringstream mesh;
        mesh << "#version 450\n"
             << "#extension GL_EXT_mesh_shader : enable\n"
             << "#extension GL_EXT_nonuniform_qualifier : enable\n"
             << "layout(local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
             << "layout(triangles) out;\n"
             << "layout(max_vertices=3, max_primitives=1) out;\n"
             << "void main()\n"
             << "{\n"
             << "  SetMeshOutputsEXT(0, 0);\n"
             << "}\n";
        programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
    }
    break;
    case STAGE_MESH:
        programCollection.glslSources.add("test") << glu::MeshSource(css.str()) << buildOptions;
        break;
    case STAGE_RAYGEN:
        programCollection.glslSources.add("test") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
        break;
    case STAGE_INTERSECT:
        programCollection.glslSources.add("rgen")
            << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader(0, 5))) << buildOptions;
        programCollection.glslSources.add("test")
            << glu::IntersectionSource(updateRayTracingGLSL(css.str())) << buildOptions;
        break;
    case STAGE_ANY_HIT:
        programCollection.glslSources.add("rgen")
            << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader(0, 5))) << buildOptions;
        programCollection.glslSources.add("test") << glu::AnyHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
        break;
    case STAGE_CLOSEST_HIT:
        programCollection.glslSources.add("rgen")
            << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader(0, 5))) << buildOptions;
        programCollection.glslSources.add("test")
            << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
        break;
    case STAGE_MISS:
        programCollection.glslSources.add("rgen")
            << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader(0, 5))) << buildOptions;
        programCollection.glslSources.add("test") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
        break;
    case STAGE_CALLABLE:
    {
        std::stringstream css2;
        css2 << "#version 460 core\n"
                "#extension GL_EXT_nonuniform_qualifier : enable\n"
                "#extension GL_EXT_ray_tracing : require\n"
                "layout(location = 0) callableDataEXT float dummy;"
                "layout(set = 0, binding = 5) uniform accelerationStructureEXT topLevelAS;\n"
                "\n"
                "void main()\n"
                "{\n"
                "  executeCallableEXT(0, 0);\n"
                "}\n";

        programCollection.glslSources.add("rgen")
            << glu::RaygenSource(updateRayTracingGLSL(css2.str())) << buildOptions;
    }
        programCollection.glslSources.add("test")
            << glu::CallableSource(updateRayTracingGLSL(css.str())) << buildOptions;
        break;
    default:
        TCU_THROW(InternalError, "Unknown stage");
    }
}

TestInstance *NoQueuesTestCase::createInstance(Context &context) const
{
    return new NoQueuesTestInstance(context, m_data);
}

void appendShaderStageCreateInfo(std::vector<VkPipelineShaderStageCreateInfo> &vec, VkShaderModule module,
                                 VkShaderStageFlagBits stage, vk::VkSpecializationInfo const *specInfo)
{
    const VkPipelineShaderStageCreateInfo info = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                             // const void* pNext;
        0u,                                                  // VkPipelineShaderStageCreateFlags flags;
        stage,                                               // VkShaderStageFlagBits stage;
        module,                                              // VkShaderModule module;
        "main",                                              // const char* pName;
        specInfo,                                            // const VkSpecializationInfo* pSpecializationInfo;
    };

    vec.push_back(info);
}

tcu::TestStatus NoQueuesTestInstance::iterate(void)
{
    qpTestResult finalres = QP_TEST_RESULT_PASS;
    tcu::TestLog &log     = m_context.getTestContext().getLog();

    deRandom rnd;
    deRandom_init(&rnd, 1234);

    const vk::InstanceInterface &vki          = m_context.getInstanceInterface();
    const vk::VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    const DeviceInterface &vk                 = m_context.getDeviceInterface();

    const DeviceFeatures deviceFeaturesAll(m_context.getInstanceInterface(), m_context.getUsedApiVersion(),
                                           physicalDevice, m_context.getInstanceExtensions(),
                                           m_context.getDeviceExtensions(), false);
    const VkPhysicalDeviceFeatures2 deviceFeatures2 = deviceFeaturesAll.getCoreFeatures2();

    float priority                                    = 1.0f;
    const vk::VkDeviceQueueCreateInfo queueCreateInfo = {
        vk::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,     nullptr, 0u,

        (uint32_t)m_context.getUniversalQueueFamilyIndex(), 1u,      &priority};

    const auto &extensionPtrs = m_context.getDeviceCreationExtensions();

    size_t cacheDataSize = 0u;
    std::vector<uint8_t> cacheData;

    vector<VkPipelineBinaryKeyKHR> binaryKeys;
    vector<vector<uint8_t>> binaryData;

    vector<size_t> shaderBinarySize;
    vector<vector<uint8_t>> shaderBinaryData;

    // Compile with no queues and populate pipeline cache/binary/etc on iter 0.
    // On iter 1, compile again in device with queues and use the pipeline.
    for (uint32_t iter = 0; iter < 2; ++iter)
    {
        const uint32_t numQueues                      = (iter == 0) ? 0 : 1;
        const vk::VkDeviceCreateInfo deviceCreateInfo = {vk::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                                         &deviceFeatures2,
                                                         0u,

                                                         numQueues,
                                                         &queueCreateInfo,

                                                         0u,
                                                         nullptr,

                                                         (uint32_t)extensionPtrs.size(),
                                                         extensionPtrs.empty() ? nullptr : &extensionPtrs[0],
                                                         0u};

        Move<VkDevice> deviceNoQueues = createCustomDevice(
            m_context.getTestContext().getCommandLine().isValidationEnabled(), m_context.getPlatformInterface(),
            m_context.getInstance(), vki, physicalDevice, &deviceCreateInfo, nullptr);

        const VkDevice device = *deviceNoQueues;

        SimpleAllocator allocator(vk, device, getPhysicalDeviceMemoryProperties(vki, physicalDevice));

        uint32_t shaderGroupHandleSize    = 0;
        uint32_t shaderGroupBaseAlignment = 1;

        const VkPipelineCacheCreateInfo pipelineCacheCreateInfo{
            VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType             sType;
            nullptr,                                      // const void*                 pNext;
            0u,                                           // VkPipelineCacheCreateFlags  flags;
            cacheDataSize,                                // uintptr_t                   initialDataSize;
            cacheDataSize ? cacheData.data() : nullptr,   // const void*                 pInitialData;
        };

        Move<VkPipelineCache> pipelineCache = createPipelineCache(vk, device, &pipelineCacheCreateInfo);
        auto pipelineCacheHandle            = *pipelineCache;
        if (m_data.testType != TT_PIPELINE_CACHE)
        {
            pipelineCacheHandle = VK_NULL_HANDLE;
        }

        if (isRayTracingStageKHR(m_data.stage))
        {
            de::MovePtr<RayTracingProperties> rayTracingPropertiesKHR;

            rayTracingPropertiesKHR =
                makeRayTracingProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice());
            shaderGroupHandleSize    = rayTracingPropertiesKHR->getShaderGroupHandleSize();
            shaderGroupBaseAlignment = rayTracingPropertiesKHR->getShaderGroupBaseAlignment();
        }

        VkPipelineBindPoint bindPoint;

        switch (m_data.stage)
        {
        case STAGE_COMPUTE:
            bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
            break;
        default:
            bindPoint = isRayTracingStageKHR(m_data.stage) ? VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR :
                                                             VK_PIPELINE_BIND_POINT_GRAPHICS;
            break;
        }

        const VkSamplerYcbcrConversionCreateInfo conversionInfo = {
            VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
            nullptr,
            VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT,
            VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY,
            VK_SAMPLER_YCBCR_RANGE_ITU_FULL,
            {
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            VK_CHROMA_LOCATION_MIDPOINT,
            VK_CHROMA_LOCATION_MIDPOINT,
            VK_FILTER_NEAREST,
            VK_FALSE,
        };
        // Note: ycbcrconversion is not currently used, just testing that we can create one
        const Unique<VkSamplerYcbcrConversion> conversion(createSamplerYcbcrConversion(vk, device, &conversionInfo));

        const VkSamplerCreateInfo samplerCreateInfo = {
            VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            nullptr,
            0u,
            VK_FILTER_NEAREST,
            VK_FILTER_NEAREST,
            VK_SAMPLER_MIPMAP_MODE_NEAREST,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            0.0f,
            VK_FALSE,
            0.0f,
            VK_FALSE,
            VK_COMPARE_OP_NEVER,
            0.0f,
            1.0f,
            VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
            VK_FALSE,
        };
        const auto sampler = createSampler(vk, device, &samplerCreateInfo);

        {
            vk::DescriptorSetLayoutBuilder layoutBuilder;

            VkFlags allShaderStages = getAllShaderStagesFor(m_data.stage);

            layoutBuilder.addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u, allShaderStages, &*sampler);
            layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allShaderStages);
            layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allShaderStages);
            layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allShaderStages);
            layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allShaderStages);

            if (usesAccelerationStructure(m_data.stage))
            {
                layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, allShaderStages);
            }

            vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(layoutBuilder.build(vk, device));

            const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
                VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // sType
                nullptr,                                       // pNext
                (VkPipelineLayoutCreateFlags)0,
                1,                          // setLayoutCount
                &descriptorSetLayout.get(), // pSetLayouts
                0u,                         // pushConstantRangeCount
                nullptr,                    // pPushConstantRanges
            };

            Move<VkPipelineLayout> pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo, NULL);

            const uint32_t specData[] = {
                m_data.threadsPerWorkgroupX,
                m_data.threadsPerWorkgroupY,
                m_data.threadsPerWorkgroupX * m_data.workgroupsX,
            };

            const vk::VkSpecializationMapEntry entries[] = {
                {0, (uint32_t)(sizeof(uint32_t) * 0), sizeof(uint32_t)},
                {1, (uint32_t)(sizeof(uint32_t) * 1), sizeof(uint32_t)},
                {2, (uint32_t)(sizeof(uint32_t) * 2), sizeof(uint32_t)},
            };

            const vk::VkSpecializationInfo specInfo = {
                sizeof(specData) / sizeof(specData[0]), // mapEntryCount
                entries,                                // pMapEntries
                sizeof(specData),                       // dataSize
                specData                                // pData
            };

            VkPipelineCreateFlags2CreateInfoKHR createFlags2 = initVulkanStructure();
            createFlags2.flags                               = VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR;

            VkPipelineBinaryInfoKHR binaryInfo = initVulkanStructure();

            PipelineBinaryWrapper binariesWrap(vk, device);

            if (m_data.testType == TT_PIPELINE_BINARY && iter == 1)
            {
                // Create pipeline binaries from what we saved in iter 0.
                vector<VkPipelineBinaryDataKHR> binaryDatas;
                binaryDatas.resize(binaryKeys.size());
                for (uint32_t i = 0; i < binaryKeys.size(); ++i)
                {
                    binaryDatas[i] = {binaryData[i].size(), binaryData[i].data()};
                }

                VkPipelineBinaryKeysAndDataKHR binaryKeysAndData;
                binaryKeysAndData.binaryCount         = (uint32_t)binaryKeys.size();
                binaryKeysAndData.pPipelineBinaryKeys = binaryKeys.data();
                binaryKeysAndData.pPipelineBinaryData = binaryDatas.data();

                VkPipelineBinaryCreateInfoKHR binaryCreateInfo = initVulkanStructure();
                binaryCreateInfo.pKeysAndDataInfo              = &binaryKeysAndData;

                VK_CHECK(binariesWrap.createPipelineBinariesFromCreateInfo(binaryCreateInfo));

                binaryInfo.binaryCount       = binariesWrap.getBinariesCount();
                binaryInfo.pPipelineBinaries = binariesWrap.getPipelineBinaries();
            }

            void *pipelineCreateInfoPnext = nullptr;
            if (m_data.testType == TT_PIPELINE_BINARY)
            {
                pipelineCreateInfoPnext = (iter == 0) ? (void *)&createFlags2 : (void *)&binaryInfo;
            }

            VkShaderCreateInfoEXT shaderCreateInfo = {
                VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT, //    VkStructureType                 sType;
                nullptr,                                  //    const void*                     pNext;
                0,                                        //    VkShaderCreateFlagsEXT          flags;
                VK_SHADER_STAGE_COMPUTE_BIT,              //    VkShaderStageFlagBits           stage;
                0u,                                       //    VkShaderStageFlags              nextStage;
                iter == 0 ? VK_SHADER_CODE_TYPE_SPIRV_EXT :
                            VK_SHADER_CODE_TYPE_BINARY_EXT, //    VkShaderCodeTypeEXT             codeType;
                0,                                          //    size_t                          codeSize;
                nullptr,                                    //    const void*                     pCode;
                "main",                                     //    const char*                     pName;
                1u,                                         //    uint32_t                        setLayoutCount;
                &descriptorSetLayout.get(),                 //    const VkDescriptorSetLayout*    pSetLayouts;
                0u,        //    uint32_t                        pushConstantRangeCount;
                nullptr,   //    const VkPushConstantRange*      pPushConstantRanges;
                &specInfo, //    const VkSpecializationInfo*     pSpecializationInfo;
            };

            Move<VkPipeline> pipeline;
            Move<VkRenderPass> renderPass;
            de::MovePtr<RayTracingPipeline> rayTracingPipeline;
            Move<VkShaderEXT> shaders[3];
            VkShaderStageFlagBits stages[3];
            uint32_t shaderCount          = 0;
            const VkSampleMask sampleMask = 0xFFFFFFFF;

            const auto &binaries = m_context.getBinaryCollection();

            auto const &createShader = [&](VkShaderStageFlagBits stage, const char *shaderName)
            {
                stages[shaderCount]    = stage;
                shaderCreateInfo.stage = stages[shaderCount];
                if (iter == 0)
                {
                    shaderCreateInfo.codeSize = binaries.get(shaderName).getSize();
                    shaderCreateInfo.pCode    = binaries.get(shaderName).getBinary();
                }
                else
                {
                    shaderCreateInfo.codeSize = shaderBinarySize[shaderCount];
                    shaderCreateInfo.pCode    = shaderBinaryData[shaderCount].data();
                }
                shaderCreateInfo.nextStage = 0;
                switch (stage)
                {
                case VK_SHADER_STAGE_TASK_BIT_EXT:
                    shaderCreateInfo.nextStage = VK_SHADER_STAGE_MESH_BIT_EXT;
                    break;
                case VK_SHADER_STAGE_VERTEX_BIT:
                    if (m_data.stage == STAGE_FRAGMENT)
                    {
                        shaderCreateInfo.nextStage = VK_SHADER_STAGE_FRAGMENT_BIT;
                    }
                    else if (m_data.stage == STAGE_GEOMETRY)
                    {
                        shaderCreateInfo.nextStage = VK_SHADER_STAGE_GEOMETRY_BIT;
                    }
                    else if (m_data.stage == STAGE_TESS_CTRL || m_data.stage == STAGE_TESS_EVAL)
                    {
                        shaderCreateInfo.nextStage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
                    }
                    break;
                case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
                    shaderCreateInfo.nextStage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
                    break;
                default:
                    break;
                }
                shaders[shaderCount++] = vk::createShader(vk, device, shaderCreateInfo);
            };

            // graphics pipeline state needs to be saved and sent to bindShaderObjectState
            std::vector<VkViewport> viewports;
            std::vector<VkRect2D> scissors;
            viewports.push_back(makeViewport(m_data.threadsPerWorkgroupX * m_data.workgroupsX,
                                             m_data.threadsPerWorkgroupY * m_data.workgroupsY));
            scissors.push_back(makeRect2D(m_data.threadsPerWorkgroupX * m_data.workgroupsX,
                                          m_data.threadsPerWorkgroupY * m_data.workgroupsY));
            VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo;
            VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo;
            VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo;
            VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo;
            VkPipelineViewportStateCreateInfo viewportStateCreateInfo;
            VkPipelineTessellationStateCreateInfo tessellationStateCreateInfo;
            VkPrimitiveTopology topology = (m_data.stage == STAGE_VERTEX) ? VK_PRIMITIVE_TOPOLOGY_POINT_LIST :
                                           (m_data.stage == STAGE_TESS_CTRL || m_data.stage == STAGE_TESS_EVAL) ?
                                                                            VK_PRIMITIVE_TOPOLOGY_PATCH_LIST :
                                                                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

            if (m_data.stage == STAGE_COMPUTE)
            {
                const Unique<VkShaderModule> shader(createShaderModule(vk, device, binaries.get("test"), 0));

                const VkPipelineShaderStageCreateInfo pipelineStageCreateInfo = {
                    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    nullptr,
                    (VkPipelineShaderStageCreateFlags)0,
                    VK_SHADER_STAGE_COMPUTE_BIT, // stage
                    *shader,                     // shader
                    "main",
                    &specInfo, // pSpecializationInfo
                };

                const VkComputePipelineCreateInfo pipelineCreateInfo = {
                    VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                    pipelineCreateInfoPnext,
                    0u,                      // flags
                    pipelineStageCreateInfo, // cs
                    *pipelineLayout,         // layout
                    VK_NULL_HANDLE,          // basePipelineHandle
                    0u,                      // basePipelineIndex
                };
                if (m_data.testType == TT_SHADER_BINARY)
                {
                    createShader(VK_SHADER_STAGE_COMPUTE_BIT, "test");
                }
                else
                {
                    pipeline = createComputePipeline(vk, device, pipelineCacheHandle, &pipelineCreateInfo, NULL);
                }
            }
            else if (m_data.stage == STAGE_RAYGEN)
            {
                rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

                rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                                              createShaderModule(vk, device, binaries.get("test"), 0), 0, &specInfo);

                pipeline = rayTracingPipeline->createPipeline(vk, device, *pipelineLayout, {}, pipelineCacheHandle,
                                                              pipelineCreateInfoPnext);
            }
            else if (m_data.stage == STAGE_INTERSECT)
            {
                rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

                rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                                              createShaderModule(vk, device, binaries.get("rgen"), 0), 0, &specInfo);
                rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
                                              createShaderModule(vk, device, binaries.get("test"), 0), 1, &specInfo);

                pipeline = rayTracingPipeline->createPipeline(vk, device, *pipelineLayout, {}, pipelineCacheHandle,
                                                              pipelineCreateInfoPnext);
            }
            else if (m_data.stage == STAGE_ANY_HIT)
            {
                rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

                rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                                              createShaderModule(vk, device, binaries.get("rgen"), 0), 0, &specInfo);
                rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
                                              createShaderModule(vk, device, binaries.get("test"), 0), 1, &specInfo);

                pipeline = rayTracingPipeline->createPipeline(vk, device, *pipelineLayout, {}, pipelineCacheHandle,
                                                              pipelineCreateInfoPnext);
            }
            else if (m_data.stage == STAGE_CLOSEST_HIT)
            {
                rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

                rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                                              createShaderModule(vk, device, binaries.get("rgen"), 0), 0, &specInfo);
                rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                                              createShaderModule(vk, device, binaries.get("test"), 0), 1, &specInfo);

                pipeline = rayTracingPipeline->createPipeline(vk, device, *pipelineLayout, {}, pipelineCacheHandle,
                                                              pipelineCreateInfoPnext);
            }
            else if (m_data.stage == STAGE_MISS)
            {
                rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

                rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                                              createShaderModule(vk, device, binaries.get("rgen"), 0), 0, &specInfo);
                rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR,
                                              createShaderModule(vk, device, binaries.get("test"), 0), 1, &specInfo);

                pipeline = rayTracingPipeline->createPipeline(vk, device, *pipelineLayout, {}, pipelineCacheHandle,
                                                              pipelineCreateInfoPnext);
            }
            else if (m_data.stage == STAGE_CALLABLE)
            {
                rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

                rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                                              createShaderModule(vk, device, binaries.get("rgen"), 0), 0, &specInfo);
                rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR,
                                              createShaderModule(vk, device, binaries.get("test"), 0), 1, &specInfo);

                pipeline = rayTracingPipeline->createPipeline(vk, device, *pipelineLayout, {}, pipelineCacheHandle,
                                                              pipelineCreateInfoPnext);
            }
            else
            {

                const VkSubpassDescription subpassDesc = {
                    (VkSubpassDescriptionFlags)0,    // VkSubpassDescriptionFlags    flags
                    VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint            pipelineBindPoint
                    0u,                              // uint32_t                        inputAttachmentCount
                    nullptr,                         // const VkAttachmentReference*    pInputAttachments
                    0u,                              // uint32_t                        colorAttachmentCount
                    nullptr,                         // const VkAttachmentReference*    pColorAttachments
                    nullptr,                         // const VkAttachmentReference*    pResolveAttachments
                    nullptr,                         // const VkAttachmentReference*    pDepthStencilAttachment
                    0u,                              // uint32_t                        preserveAttachmentCount
                    nullptr                          // const uint32_t*                pPreserveAttachments
                };

                const VkRenderPassCreateInfo renderPassParams = {
                    VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureTypei                    sType
                    nullptr,                                   // const void*                        pNext
                    (VkRenderPassCreateFlags)0,                // VkRenderPassCreateFlags            flags
                    0u,                                        // uint32_t                            attachmentCount
                    nullptr,                                   // const VkAttachmentDescription*    pAttachments
                    1u,                                        // uint32_t                            subpassCount
                    &subpassDesc,                              // const VkSubpassDescription*        pSubpasses
                    0u,                                        // uint32_t                            dependencyCount
                    nullptr                                    // const VkSubpassDependency*        pDependencies
                };

                renderPass = createRenderPass(vk, device, &renderPassParams);

                // Note: vertex input state and input assembly state will not be used for mesh pipelines.

                vertexInputStateCreateInfo = {
                    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
                    nullptr,                                                   // const void* pNext;
                    (VkPipelineVertexInputStateCreateFlags)0, // VkPipelineVertexInputStateCreateFlags flags;
                    0u,                                       // uint32_t vertexBindingDescriptionCount;
                    nullptr, // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
                    0u,      // uint32_t vertexAttributeDescriptionCount;
                    nullptr  // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
                };

                inputAssemblyStateCreateInfo = {
                    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType sType;
                    nullptr,                                                     // const void* pNext;
                    (VkPipelineInputAssemblyStateCreateFlags)0, // VkPipelineInputAssemblyStateCreateFlags flags;
                    topology,                                   // VkPrimitiveTopology topology;
                    VK_FALSE                                    // VkBool32 primitiveRestartEnable;
                };

                rasterizationStateCreateInfo = {
                    VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType sType;
                    nullptr,                                                    // const void* pNext;
                    (VkPipelineRasterizationStateCreateFlags)0, // VkPipelineRasterizationStateCreateFlags flags;
                    VK_FALSE,                                   // VkBool32 depthClampEnable;
                    (m_data.stage != STAGE_FRAGMENT) ? VK_TRUE : VK_FALSE, // VkBool32 rasterizerDiscardEnable;
                    VK_POLYGON_MODE_FILL,                                  // VkPolygonMode polygonMode;
                    VK_CULL_MODE_NONE,                                     // VkCullModeFlags cullMode;
                    VK_FRONT_FACE_CLOCKWISE,                               // VkFrontFace frontFace;
                    VK_FALSE,                                              // VkBool32 depthBiasEnable;
                    0.0f,                                                  // float depthBiasConstantFactor;
                    0.0f,                                                  // float depthBiasClamp;
                    0.0f,                                                  // float depthBiasSlopeFactor;
                    1.0f                                                   // float lineWidth;
                };

                multisampleStateCreateInfo = {
                    VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType                            sType
                    nullptr,               // const void*                                pNext
                    0u,                    // VkPipelineMultisampleStateCreateFlags    flags
                    VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits                    rasterizationSamples
                    VK_FALSE,              // VkBool32                                    sampleShadingEnable
                    1.0f,                  // float                                    minSampleShading
                    &sampleMask,           // const VkSampleMask*                        pSampleMask
                    VK_FALSE,              // VkBool32                                    alphaToCoverageEnable
                    VK_FALSE               // VkBool32                                    alphaToOneEnable
                };

                viewportStateCreateInfo = {
                    VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType                            sType
                    nullptr,                               // const void*                                pNext
                    (VkPipelineViewportStateCreateFlags)0, // VkPipelineViewportStateCreateFlags        flags
                    1u,                                    // uint32_t                                    viewportCount
                    viewports.data(),                      // const VkViewport*                        pViewports
                    1u,                                    // uint32_t                                    scissorCount
                    scissors.data()                        // const VkRect2D*                            pScissors
                };

                tessellationStateCreateInfo = {
                    VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO, // VkStructureType sType;
                    nullptr,                                                   // const void* pNext;
                    0u,                          // VkPipelineTessellationStateCreateFlags flags;
                    m_data.threadsPerWorkgroupX, // uint32_t patchControlPoints;
                };

                Move<VkShaderModule> fs;
                Move<VkShaderModule> vs;
                Move<VkShaderModule> tcs;
                Move<VkShaderModule> tes;
                Move<VkShaderModule> gs;
                Move<VkShaderModule> ms;
                Move<VkShaderModule> ts;

                std::vector<VkPipelineShaderStageCreateInfo> stageCreateInfos;

                if (m_data.stage == STAGE_VERTEX)
                {
                    if (m_data.testType == TT_SHADER_BINARY)
                    {
                        createShader(VK_SHADER_STAGE_VERTEX_BIT, "test");
                    }
                    else
                    {
                        vs = createShaderModule(vk, device, binaries.get("test"));
                        appendShaderStageCreateInfo(stageCreateInfos, vs.get(), VK_SHADER_STAGE_VERTEX_BIT, &specInfo);
                    }
                }
                else if (m_data.stage == STAGE_FRAGMENT)
                {
                    if (m_data.testType == TT_SHADER_BINARY)
                    {
                        createShader(VK_SHADER_STAGE_VERTEX_BIT, "vert");
                        createShader(VK_SHADER_STAGE_FRAGMENT_BIT, "test");
                    }
                    else
                    {
                        vs = createShaderModule(vk, device, binaries.get("vert"));
                        fs = createShaderModule(vk, device, binaries.get("test"));
                        appendShaderStageCreateInfo(stageCreateInfos, vs.get(), VK_SHADER_STAGE_VERTEX_BIT, &specInfo);
                        appendShaderStageCreateInfo(stageCreateInfos, fs.get(), VK_SHADER_STAGE_FRAGMENT_BIT,
                                                    &specInfo);
                    }
                }
                else if (m_data.stage == STAGE_GEOMETRY)
                {
                    if (m_data.testType == TT_SHADER_BINARY)
                    {
                        createShader(VK_SHADER_STAGE_VERTEX_BIT, "vert");
                        createShader(VK_SHADER_STAGE_GEOMETRY_BIT, "test");
                    }
                    else
                    {
                        vs = createShaderModule(vk, device, binaries.get("vert"));
                        gs = createShaderModule(vk, device, binaries.get("test"));
                        appendShaderStageCreateInfo(stageCreateInfos, vs.get(), VK_SHADER_STAGE_VERTEX_BIT, &specInfo);
                        appendShaderStageCreateInfo(stageCreateInfos, gs.get(), VK_SHADER_STAGE_GEOMETRY_BIT,
                                                    &specInfo);
                    }
                }
                else if (m_data.stage == STAGE_TESS_CTRL || m_data.stage == STAGE_TESS_EVAL)
                {
                    if (m_data.testType == TT_SHADER_BINARY)
                    {
                        createShader(VK_SHADER_STAGE_VERTEX_BIT, "vert");
                        createShader(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, "tesc");
                        createShader(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "tese");
                    }
                    else
                    {
                        vs  = createShaderModule(vk, device, binaries.get("vert"));
                        tcs = createShaderModule(vk, device, binaries.get("tesc"));
                        tes = createShaderModule(vk, device, binaries.get("tese"));
                        appendShaderStageCreateInfo(stageCreateInfos, vs.get(), VK_SHADER_STAGE_VERTEX_BIT, &specInfo);
                        appendShaderStageCreateInfo(stageCreateInfos, tcs.get(),
                                                    VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, &specInfo);
                        appendShaderStageCreateInfo(stageCreateInfos, tes.get(),
                                                    VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, &specInfo);
                    }
                }
                else if (m_data.stage == STAGE_TASK)
                {
                    if (m_data.testType == TT_SHADER_BINARY)
                    {
                        createShader(VK_SHADER_STAGE_TASK_BIT_EXT, "test");
                        createShader(VK_SHADER_STAGE_MESH_BIT_EXT, "mesh");
                    }
                    else
                    {
                        ts = createShaderModule(vk, device, binaries.get("test"));
                        ms = createShaderModule(vk, device, binaries.get("mesh"));
                        appendShaderStageCreateInfo(stageCreateInfos, ts.get(), vk::VK_SHADER_STAGE_TASK_BIT_EXT,
                                                    &specInfo);
                        appendShaderStageCreateInfo(stageCreateInfos, ms.get(), VK_SHADER_STAGE_MESH_BIT_EXT,
                                                    &specInfo);
                    }
                }
                else if (m_data.stage == STAGE_MESH)
                {
                    if (m_data.testType == TT_SHADER_BINARY)
                    {
                        shaderCreateInfo.flags = VK_SHADER_CREATE_NO_TASK_SHADER_BIT_EXT;
                        createShader(VK_SHADER_STAGE_MESH_BIT_EXT, "test");
                        shaderCreateInfo.flags = 0;
                    }
                    else
                    {
                        ms = createShaderModule(vk, device, binaries.get("test"));
                        appendShaderStageCreateInfo(stageCreateInfos, ms.get(), VK_SHADER_STAGE_MESH_BIT_EXT,
                                                    &specInfo);
                    }
                }

                const VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
                    VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, // VkStructureType sType;
                    pipelineCreateInfoPnext,                         // const void* pNext;
                    (VkPipelineCreateFlags)0,                        // VkPipelineCreateFlags flags;
                    static_cast<uint32_t>(stageCreateInfos.size()),  // uint32_t stageCount;
                    de::dataOrNull(stageCreateInfos),                // const VkPipelineShaderStageCreateInfo* pStages;
                    &vertexInputStateCreateInfo,   // const VkPipelineVertexInputStateCreateInfo* pVertexInputState;
                    &inputAssemblyStateCreateInfo, // const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState;
                    &tessellationStateCreateInfo,  // const VkPipelineTessellationStateCreateInfo* pTessellationState;
                    &viewportStateCreateInfo,      // const VkPipelineViewportStateCreateInfo* pViewportState;
                    &rasterizationStateCreateInfo, // const VkPipelineRasterizationStateCreateInfo* pRasterizationState;
                    &multisampleStateCreateInfo,   // const VkPipelineMultisampleStateCreateInfo* pMultisampleState;
                    nullptr,                       // const VkPipelineDepthStencilStateCreateInfo* pDepthStencilState;
                    nullptr,                       // const VkPipelineColorBlendStateCreateInfo* pColorBlendState;
                    nullptr,                       // const VkPipelineDynamicStateCreateInfo* pDynamicState;
                    pipelineLayout.get(),          // VkPipelineLayout layout;
                    renderPass.get(),              // VkRenderPass renderPass;
                    0u,                            // uint32_t subpass;
                    VK_NULL_HANDLE,                // VkPipeline basePipelineHandle;
                    0                              // int basePipelineIndex;
                };

                if (m_data.testType != TT_SHADER_BINARY)
                {
                    pipeline = createGraphicsPipeline(vk, device, pipelineCacheHandle, &graphicsPipelineCreateInfo);
                }
            }

            if (iter == 0)
            {
                if (m_data.testType == TT_PIPELINE_CACHE)
                {
                    VK_CHECK(
                        vk.getPipelineCacheData(device, pipelineCacheHandle, (uintptr_t *)&cacheDataSize, nullptr));
                    log << TestLog::Message << "cacheDataSize " << cacheDataSize << TestLog::EndMessage;
                    if (cacheDataSize > 0)
                    {
                        cacheData.resize(cacheDataSize);
                        VK_CHECK(vk.getPipelineCacheData(device, pipelineCacheHandle, (uintptr_t *)&cacheDataSize,
                                                         &cacheData[0]));
                    }
                }
                else if (m_data.testType == TT_PIPELINE_BINARY)
                {

                    PipelineBinaryWrapper binariesWrap2(vk, device);

                    VK_CHECK(binariesWrap2.createPipelineBinariesFromPipeline(*pipeline));

                    binaryKeys.resize(binariesWrap2.getBinariesCount(),
                                      {VK_STRUCTURE_TYPE_PIPELINE_BINARY_KEY_KHR, nullptr, 0, {}});
                    binaryData.resize(binariesWrap2.getBinariesCount());

                    // Get each pipeline binary's data
                    for (uint32_t i = 0; i < binariesWrap2.getBinariesCount(); ++i)
                    {
                        VkPipelineBinaryDataInfoKHR binaryDataInfo = initVulkanStructure();
                        binaryDataInfo.pipelineBinary              = binariesWrap2.getPipelineBinaries()[i];

                        size_t binaryDataSize = 0;
                        VK_CHECK(vk.getPipelineBinaryDataKHR(device, &binaryDataInfo, &binaryKeys[i], &binaryDataSize,
                                                             NULL));
                        binaryData[i].resize(binaryDataSize);
                        VK_CHECK(vk.getPipelineBinaryDataKHR(device, &binaryDataInfo, &binaryKeys[i], &binaryDataSize,
                                                             binaryData[i].data()));
                        log << TestLog::Message << "binaryDataSize[" << i << "] = " << binaryDataSize
                            << TestLog::EndMessage;
                    }
                }
                else if (m_data.testType == TT_SHADER_BINARY)
                {
                    shaderBinarySize.resize(shaderCount, 0);
                    shaderBinaryData.resize(shaderCount);
                    for (uint32_t i = 0; i < shaderCount; ++i)
                    {
                        VK_CHECK(vk.getShaderBinaryDataEXT(device, *shaders[i], &shaderBinarySize[i], nullptr));
                        shaderBinaryData[i].resize(shaderBinarySize[i]);
                        VK_CHECK(vk.getShaderBinaryDataEXT(device, *shaders[i], &shaderBinarySize[i],
                                                           shaderBinaryData[i].data()));
                        log << TestLog::Message << "shaderBinarySize[" << i << "] = " << shaderBinarySize[i]
                            << TestLog::EndMessage;
                    }
                }
                continue;
            }

            const VkImageCreateInfo imageCreateInfo = {
                VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
                nullptr,                             // const void* pNext;
                (VkImageCreateFlags)0u,              // VkImageCreateFlags flags;
                VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
                VK_FORMAT_R8G8B8A8_UNORM,            // VkFormat format;
                {
                    1u,                  // uint32_t width;
                    1u,                  // uint32_t height;
                    1u                   // uint32_t depth;
                },                       // VkExtent3D extent;
                1u,                      // uint32_t mipLevels;
                1u,                      // uint32_t arrayLayers;
                VK_SAMPLE_COUNT_1_BIT,   // VkSampleCountFlagBits samples;
                VK_IMAGE_TILING_OPTIMAL, // VkImageTiling tiling;
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
                VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
                0u,                                  // uint32_t queueFamilyIndexCount;
                nullptr,                             // const uint32_t* pQueueFamilyIndices;
                VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout initialLayout;
            };

            VkImageViewCreateInfo imageViewCreateInfo = {
                VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
                nullptr,                                  // const void* pNext;
                (VkImageViewCreateFlags)0u,               // VkImageViewCreateFlags flags;
                VK_NULL_HANDLE,                           // VkImage image;
                VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
                VK_FORMAT_R8G8B8A8_UNORM,                 // VkFormat format;
                {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                 VK_COMPONENT_SWIZZLE_IDENTITY}, // VkComponentMapping  components;
                {
                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    0u,                        // uint32_t baseMipLevel;
                    1u,                        // uint32_t levelCount;
                    0u,                        // uint32_t baseArrayLayer;
                    1u                         // uint32_t layerCount;
                }                              // VkImageSubresourceRange subresourceRange;
            };

            de::MovePtr<ImageWithMemory> image;
            Move<VkImageView> imageView;

            image = de::MovePtr<ImageWithMemory>(
                new ImageWithMemory(vk, device, allocator, imageCreateInfo, MemoryRequirement::Any));
            imageViewCreateInfo.image = **image;
            imageView                 = createImageView(vk, device, &imageViewCreateInfo, NULL);

            const VkQueue queue         = getDeviceQueue(vk, device, m_context.getUniversalQueueFamilyIndex(), 0);
            Move<VkCommandPool> cmdPool = createCommandPool(vk, device, 0, m_context.getUniversalQueueFamilyIndex());
            Move<VkCommandBuffer> cmdBuffer =
                allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

            beginCommandBuffer(vk, *cmdBuffer, 0u);

            VkDeviceSize bufferSizes[5];
            de::MovePtr<BufferWithMemory> buffers[5];
            vk::VkDescriptorBufferInfo bufferDescriptors[5];
            uint32_t totalElements[4] = {1, 1, 1, 1};

            uint32_t totalInvocations =
                m_data.threadsPerWorkgroupX * m_data.threadsPerWorkgroupY * m_data.workgroupsX * m_data.workgroupsY;

            for (uint32_t i = 0; i < 5; ++i)
            {
                if (i < 4)
                {
                    totalElements[i] *= totalInvocations;

                    bufferSizes[i] = totalElements[i] * 4;
                }
                else
                {
                    bufferSizes[4] = sizeof(VkDeviceAddress) * 4;
                }

                try
                {
                    buffers[i] = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
                        vk, device, allocator,
                        makeBufferCreateInfo(bufferSizes[i], VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT),
                        MemoryRequirement::HostVisible | MemoryRequirement::Cached | MemoryRequirement::Coherent |
                            MemoryRequirement::DeviceAddress));
                }
                catch (const tcu::NotSupportedError &)
                {
                    buffers[i] = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
                        vk, device, allocator,
                        makeBufferCreateInfo(bufferSizes[i], VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT),
                        MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress));
                }

                bufferDescriptors[i] = makeDescriptorBufferInfo(**buffers[i], 0, bufferSizes[i]);
            }

            void *ptrs[5];
            for (uint32_t i = 0; i < 5; ++i)
            {
                ptrs[i] = buffers[i]->getAllocation().getHostPtr();
            }

            Move<VkFramebuffer> framebuffer;
            if (m_data.stage != STAGE_COMPUTE && !isRayTracingStageKHR(m_data.stage))
            {
                const vk::VkFramebufferCreateInfo framebufferParams = {
                    vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // sType
                    nullptr,                                       // pNext
                    (vk::VkFramebufferCreateFlags)0,
                    *renderPass,                                      // renderPass
                    0U,                                               // attachmentCount
                    nullptr,                                          // pAttachments
                    m_data.threadsPerWorkgroupX * m_data.workgroupsX, // width
                    m_data.threadsPerWorkgroupY * m_data.workgroupsY, // height
                    1u,                                               // layers
                };

                framebuffer = createFramebuffer(vk, device, &framebufferParams);
            }

            vk::DescriptorPoolBuilder poolBuilder;
            poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5u);
            poolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u);
            if (usesAccelerationStructure(m_data.stage))
            {
                poolBuilder.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1u);
            }

            vk::Unique<vk::VkDescriptorPool> descriptorPool(
                poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
            vk::Unique<vk::VkDescriptorSet> descriptorSet(
                makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

            vk::DescriptorSetUpdateBuilder setUpdateBuilder;
            setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1),
                                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[1]);
            setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(2),
                                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[2]);
            setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(3),
                                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[3]);

            VkImageSubresourceRange range = {
                VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1,
            };
            VkImageMemoryBarrier imageBarrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                nullptr,                                // const void* pNext;
                0,                                      // VkAccessFlags srcAccessMask;
                VK_ACCESS_SHADER_READ_BIT,              // VkAccessFlags dstAccessMask;
                VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout oldLayout;
                VK_IMAGE_LAYOUT_GENERAL,                // VkImageLayout newLayout;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                **image,                                // VkImage image;
                range,                                  // VkImageSubresourceRange subresourceRange;
            };

            vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_HOST_BIT, vk::VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                                  (vk::VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

            VkDescriptorImageInfo imageInfo;
            imageInfo.imageView   = *imageView;
            imageInfo.sampler     = *sampler;
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0),
                                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfo);

            // Create ray tracing structures
            de::MovePtr<vk::BottomLevelAccelerationStructure> bottomLevelAccelerationStructure;
            de::MovePtr<vk::TopLevelAccelerationStructure> topLevelAccelerationStructure;
            VkStridedDeviceAddressRegionKHR raygenShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(0, 0, 0);
            VkStridedDeviceAddressRegionKHR missShaderBindingTableRegion   = makeStridedDeviceAddressRegionKHR(0, 0, 0);
            VkStridedDeviceAddressRegionKHR hitShaderBindingTableRegion    = makeStridedDeviceAddressRegionKHR(0, 0, 0);
            VkStridedDeviceAddressRegionKHR callableShaderBindingTableRegion =
                makeStridedDeviceAddressRegionKHR(0, 0, 0);

            if (usesAccelerationStructure(m_data.stage))
            {
                // Create bottom level acceleration structure
                {
                    bottomLevelAccelerationStructure = makeBottomLevelAccelerationStructure();

                    bottomLevelAccelerationStructure->setDefaultGeometryData(getShaderStageFlag(m_data.stage));
                    AccelerationStructBufferProperties bufferProps;
                    bufferProps.props.residency = ResourceResidency::TRADITIONAL;
                    bottomLevelAccelerationStructure->createAndBuild(vk, device, *cmdBuffer, allocator, bufferProps);
                }

                // Create top level acceleration structure
                {
                    topLevelAccelerationStructure = makeTopLevelAccelerationStructure();

                    topLevelAccelerationStructure->setInstanceCount(1);
                    topLevelAccelerationStructure->addInstance(
                        de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));

                    AccelerationStructBufferProperties bufferProps;
                    bufferProps.props.residency = ResourceResidency::TRADITIONAL;
                    topLevelAccelerationStructure->createAndBuild(vk, device, *cmdBuffer, allocator, bufferProps);
                }

                VkWriteDescriptorSetAccelerationStructureKHR accelerationStructureWriteDescriptorSet = {
                    VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR, //  VkStructureType sType;
                    nullptr,                                                           //  const void* pNext;
                    1, //  uint32_t accelerationStructureCount;
                    topLevelAccelerationStructure
                        ->getPtr(), //  const VkAccelerationStructureKHR* pAccelerationStructures;
                };

                setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(5),
                                             VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                                             &accelerationStructureWriteDescriptorSet);
            }

            setUpdateBuilder.update(vk, device);

            de::MovePtr<BufferWithMemory> raygenShaderBindingTable;
            de::MovePtr<BufferWithMemory> missShaderBindingTable;
            de::MovePtr<BufferWithMemory> hitShaderBindingTable;
            de::MovePtr<BufferWithMemory> callableShaderBindingTable;

            for (uint32_t i = 0; i < 4; ++i)
            {
                for (uint32_t j = 0; j < totalElements[i]; ++j)
                {
                    ((float *)ptrs[i])[j] = ((float)(deRandom_getUint32(&rnd) & 0xff) - 64.0f) / 2.0f;
                }
            }

            flushAlloc(vk, device, buffers[0]->getAllocation());
            flushAlloc(vk, device, buffers[1]->getAllocation());
            flushAlloc(vk, device, buffers[2]->getAllocation());
            flushAlloc(vk, device, buffers[3]->getAllocation());

            vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, *pipelineLayout, 0u, 1, &*descriptorSet, 0u, nullptr);
            if (m_data.testType == TT_SHADER_BINARY)
            {
                vk::VkShaderStageFlagBits allStages[] = {
                    vk::VK_SHADER_STAGE_VERTEX_BIT,
                    vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
                    vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                    vk::VK_SHADER_STAGE_GEOMETRY_BIT,
                    vk::VK_SHADER_STAGE_FRAGMENT_BIT,
                    vk::VK_SHADER_STAGE_TASK_BIT_EXT,
                    vk::VK_SHADER_STAGE_MESH_BIT_EXT,
                };

                vk.cmdBindShadersEXT(*cmdBuffer, 7u, allStages, nullptr);
                for (uint32_t i = 0; i < shaderCount; ++i)
                {
                    vk.cmdBindShadersEXT(*cmdBuffer, 1u, &stages[i], &*shaders[i]);
                }
                if (m_data.stage != STAGE_COMPUTE)
                {
                    vkt::shaderobjutil::bindShaderObjectState(
                        vk, vkt::shaderobjutil::getDeviceCreationExtensions(m_context), *cmdBuffer, viewports, scissors,
                        topology, m_data.threadsPerWorkgroupX, &vertexInputStateCreateInfo,
                        &rasterizationStateCreateInfo, &multisampleStateCreateInfo, nullptr, nullptr);
                }
            }
            else
            {
                vk.cmdBindPipeline(*cmdBuffer, bindPoint, *pipeline);
            }

            if (isRayTracingStageKHR(m_data.stage))
            {
                raygenShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
                    vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
                raygenShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(
                    getBufferDeviceAddress(vk, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize,
                    shaderGroupHandleSize);
            }
            if (m_data.stage == STAGE_INTERSECT || m_data.stage == STAGE_ANY_HIT || m_data.stage == STAGE_CLOSEST_HIT)
            {
                hitShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
                    vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
                hitShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(
                    getBufferDeviceAddress(vk, device, hitShaderBindingTable->get(), 0), shaderGroupHandleSize,
                    shaderGroupHandleSize);
            }
            else if (m_data.stage == STAGE_MISS)
            {
                missShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
                    vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
                missShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(
                    getBufferDeviceAddress(vk, device, missShaderBindingTable->get(), 0), shaderGroupHandleSize,
                    shaderGroupHandleSize);
            }
            else if (m_data.stage == STAGE_CALLABLE)
            {
                callableShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
                    vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
                callableShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(
                    getBufferDeviceAddress(vk, device, callableShaderBindingTable->get(), 0), shaderGroupHandleSize,
                    shaderGroupHandleSize);
            }

            if (m_data.stage == STAGE_COMPUTE)
            {
                vk.cmdDispatch(*cmdBuffer, m_data.workgroupsX, m_data.workgroupsY, 1);
            }
            else if (isRayTracingStageKHR(m_data.stage))
            {
                cmdTraceRays(vk, *cmdBuffer, &raygenShaderBindingTableRegion, &missShaderBindingTableRegion,
                             &hitShaderBindingTableRegion, &callableShaderBindingTableRegion,
                             m_data.workgroupsX * m_data.threadsPerWorkgroupX,
                             m_data.workgroupsY * m_data.threadsPerWorkgroupY, 1);
            }
            else
            {
                if (m_data.testType == TT_SHADER_BINARY)
                {
                    const vk::VkRect2D renderArea = vk::makeRect2D(m_data.threadsPerWorkgroupX * m_data.workgroupsX,
                                                                   m_data.threadsPerWorkgroupY * m_data.workgroupsY);

                    beginRendering(vk, *cmdBuffer, VK_NULL_HANDLE, renderArea, VkClearValue{},
                                   vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_LOAD, 0);
                }
                else
                {
                    beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer,
                                    makeRect2D(m_data.threadsPerWorkgroupX * m_data.workgroupsX,
                                               m_data.threadsPerWorkgroupY * m_data.workgroupsY),
                                    0, nullptr, VK_SUBPASS_CONTENTS_INLINE);
                }
                // Draw a point cloud for vertex shader testing, points forming patches for tessellation testing,
                // and a single quad for fragment shader testing
                if (m_data.stage == STAGE_VERTEX || m_data.stage == STAGE_TESS_CTRL || m_data.stage == STAGE_TESS_EVAL)
                {
                    vk.cmdDraw(*cmdBuffer,
                               m_data.threadsPerWorkgroupX * m_data.workgroupsX * m_data.threadsPerWorkgroupY *
                                   m_data.workgroupsY,
                               1u, 0u, 0u);
                }
                else if (m_data.stage == STAGE_GEOMETRY)
                {
                    // Topology is triangle strips, so launch N+2 vertices to form N triangles.
                    vk.cmdDraw(*cmdBuffer, m_data.workgroupsX * m_data.workgroupsY + 2u, 1u, 0u, 0u);
                }
                else if (m_data.stage == STAGE_FRAGMENT)
                {
                    vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);
                }
                else if (isMeshStage(m_data.stage))
                {
                    vk.cmdDrawMeshTasksEXT(*cmdBuffer, m_data.workgroupsX, m_data.workgroupsY, 1u);
                }
                if (m_data.testType == TT_SHADER_BINARY)
                {
                    endRendering(vk, *cmdBuffer);
                }
                else
                {
                    endRenderPass(vk, *cmdBuffer);
                }
            }

            endCommandBuffer(vk, *cmdBuffer);

            submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

            invalidateAlloc(vk, device, buffers[3]->getAllocation());

            qpTestResult res = QP_TEST_RESULT_PASS;

            uint32_t numInvocations = totalInvocations;
            for (uint32_t i = 0; i < numInvocations; ++i)
            {
                float output = ((float *)ptrs[3])[i];
                if (output != 1.0f)
                    res = QP_TEST_RESULT_FAIL;
            }
            if (res != QP_TEST_RESULT_PASS)
            {
                log << tcu::TestLog::Message << "failed" << tcu::TestLog::EndMessage;
                finalres = res;
            }
        }
    }

    return tcu::TestStatus(finalres, qpGetTestResultName(finalres));
}

} // namespace

template <uint32_t N>
struct TestGroupCaseN
{
    uint32_t value[N];
    const char *name;
    const char *description;
};

tcu::TestCaseGroup *createNoQueuesTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "no_queues", "no_queues tests"));

    typedef struct
    {
        uint32_t value;
        const char *name;
    } TestGroupCase;

    TestGroupCase stageCases[] = {
        {STAGE_COMPUTE, "compute"},   {STAGE_RAYGEN, "raygen"},      {STAGE_INTERSECT, "isect"},
        {STAGE_ANY_HIT, "ahit"},      {STAGE_CLOSEST_HIT, "chit"},   {STAGE_MISS, "miss"},
        {STAGE_CALLABLE, "callable"}, {STAGE_VERTEX, "vertex"},      {STAGE_FRAGMENT, "fragment"},
        {STAGE_GEOMETRY, "geometry"}, {STAGE_TESS_CTRL, "tessctrl"}, {STAGE_TESS_EVAL, "tesseval"},
        {STAGE_TASK, "task"},         {STAGE_MESH, "mesh"},
    };

    TestGroupCase ttCases[] = {
        {TT_PIPELINE_CACHE, "pipeline_cache"},
        {TT_PIPELINE_BINARY, "pipeline_binary"},
        {TT_SHADER_BINARY, "shader_binary"},
    };
    for (int ttNdx = 0; ttNdx < DE_LENGTH_OF_ARRAY(ttCases); ttNdx++)
    {
        de::MovePtr<tcu::TestCaseGroup> ttGroup(new tcu::TestCaseGroup(testCtx, ttCases[ttNdx].name));
        for (int stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stageCases); stageNdx++)
        {
            TestType testType = (TestType)ttCases[ttNdx].value;

            if (testType == TT_SHADER_BINARY && isRayTracingStageKHR((Stage)stageCases[stageNdx].value))
            {
                continue;
            }

            uint32_t threadsPerWorkgroupX = 8u;
            uint32_t threadsPerWorkgroupY = 8u;
            uint32_t workgroupsX          = 2u;
            uint32_t workgroupsY          = 2u;

            if (stageCases[stageNdx].value == STAGE_GEOMETRY || stageCases[stageNdx].value == STAGE_TESS_CTRL ||
                stageCases[stageNdx].value == STAGE_TESS_EVAL || stageCases[stageNdx].value == STAGE_TASK ||
                stageCases[stageNdx].value == STAGE_MESH)
            {
                threadsPerWorkgroupX = 32u;
                threadsPerWorkgroupY = 1u;
            }

            CaseDef c = {
                (Stage)stageCases[stageNdx].value, // Stage stage;
                testType,                          // TestType testtype;
                threadsPerWorkgroupX,              // uint32_t threadsPerWorkgroupX;
                threadsPerWorkgroupY,              // uint32_t threadsPerWorkgroupY;
                workgroupsX,                       // uint32_t workgroupsX;
                workgroupsY,                       // uint32_t workgroupsY;
            };
            ttGroup->addChild(new NoQueuesTestCase(testCtx, stageCases[stageNdx].name, c));
        }
        group->addChild(ttGroup.release());
    }
    return group.release();
}
} // namespace no_queues
} // namespace vkt
