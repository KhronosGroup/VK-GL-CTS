/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
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
 * \brief Ray Tracing Builtin and specialization constant tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingBuiltinTests.hpp"

#include "vkDefs.hpp"

#include "vktTestCase.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkPipelineConstructionUtil.hpp"

#include "vkRayTracingUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuMatrix.hpp"

#include "deMath.h"

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
                                              VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR;

enum GeomType
{
    GEOM_TYPE_TRIANGLES,
    GEOM_TYPE_AABBS,
};

enum TestId
{
    TEST_ID_LAUNCH_ID_EXT = 0,
    TEST_ID_LAUNCH_SIZE_EXT,
    TEST_ID_PRIMITIVE_ID,
    TEST_ID_INSTANCE_ID,
    TEST_ID_INSTANCE_CUSTOM_INDEX_EXT,
    TEST_ID_GEOMETRY_INDEX_EXT,
    TEST_ID_WORLD_RAY_ORIGIN_EXT,
    TEST_ID_WORLD_RAY_DIRECTION_EXT,
    TEST_ID_OBJECT_RAY_ORIGIN_EXT,
    TEST_ID_OBJECT_RAY_DIRECTION_EXT,
    TEST_ID_RAY_T_MIN_EXT,
    TEST_ID_RAY_T_MAX_EXT,
    TEST_ID_INCOMING_RAY_FLAGS_EXT,
    TEST_ID_HIT_T_EXT,
    TEST_ID_HIT_KIND_EXT,
    TEST_ID_OBJECT_TO_WORLD_EXT,
    TEST_ID_OBJECT_TO_WORLD_3X4_EXT,
    TEST_ID_WORLD_TO_OBJECT_EXT,
    TEST_ID_WORLD_TO_OBJECT_3X4_EXT,
    TEST_ID_INDICES_INDIRECT,
    TEST_ID_TRANSFORMS_INDIRECT,
    TEST_ID_TMINMAX_INDIRECT,
    TEST_ID_INCOMING_RAY_FLAGS_INDIRECT,
    TEST_ID_HIT_KIND_INDIRECT,
    TEST_ID_LAST
};

enum RayFlagBits
{
    RAY_FLAG_BIT_OPAQUE_EXT                      = 0, //  const uint gl_RayFlagsOpaqueEXT = 1U;
    RAY_FLAG_BIT_NO_OPAQUE_EXT                   = 1, //  const uint gl_RayFlagsNoOpaqueEXT = 2U;
    RAY_FLAG_BIT_TERMINATE_ON_FIRST_HIT_EXT      = 2, //  const uint gl_RayFlagsTerminateOnFirstHitEXT = 4U;
    RAY_FLAG_BIT_SKIP_CLOSEST_HIT_SHADER_EXT     = 3, //  const uint gl_RayFlagsSkipClosestHitShaderEXT = 8U;
    RAY_FLAG_BIT_CULL_BACK_FACING_TRIANGLES_EXT  = 4, //  const uint gl_RayFlagsCullBackFacingTrianglesEXT = 16U;
    RAY_FLAG_BIT_CULL_FRONT_FACING_TRIANGLES_EXT = 5, //  const uint gl_RayFlagsCullFrontFacingTrianglesEXT = 32U;
    RAY_FLAG_BIT_CULL_OPAQUE_EXT                 = 6, //  const uint gl_RayFlagsCullOpaqueEXT = 64U;
    RAY_FLAG_BIT_CULL_NO_OPAQUE_EXT              = 7, //  const uint gl_RayFlagsCullNoOpaqueEXT = 128U;
    RAY_FLAG_BIT_LAST_PER_TEST,
    RAY_FLAG_BIT_SKIP_TRIANGLES_EXT = 8, //  const uint gl_RayFlagsSkipTrianglesEXT = 256U;
    RAY_FLAG_BIT_SKIP_AABB_EXT      = 9, //  const uint gl_RayFlagsSkipAABBEXT = 512U;
    RAY_FLAG_BIT_LAST
};

struct CaseDef
{
    TestId id;
    const char *name;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t raysDepth;
    VkFormat format;
    bool fixedPointScalarOutput;
    bool fixedPointVectorOutput;
    bool fixedPointMatrixOutput;
    GeomType geomType;
    uint32_t squaresGroupCount;
    uint32_t geometriesGroupCount;
    uint32_t instancesGroupCount;
    VkShaderStageFlagBits stage;
    bool rayFlagSkipTriangles;
    bool rayFlagSkipAABSs;
    bool opaque;
    bool frontFace;
    VkPipelineCreateFlags pipelineCreateFlags;
    bool useSpecConstants;
    bool skipClosestHit;
    bool useMaintenance5;
};

const uint32_t DEFAULT_UINT_CLEAR_VALUE  = 0x7F000000;
const uint32_t FIXED_POINT_DIVISOR       = 1024 * 1024;
const uint32_t FIXED_POINT_ALLOWED_ERROR = 4;

bool isPlain(const uint32_t width, const uint32_t height, const uint32_t depth)
{
    return (width == 1 || height == 1 || depth == 1);
}

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

VkImageCreateInfo makeImageCreateInfo(uint32_t width, uint32_t height, uint32_t depth, VkFormat format)
{
    const VkImageType imageType = VK_IMAGE_TYPE_3D;
    const VkImageUsageFlags usage =
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                             // const void* pNext;
        (VkImageCreateFlags)0u,              // VkImageCreateFlags flags;
        imageType,                           // VkImageType imageType;
        format,                              // VkFormat format;
        makeExtent3D(width, height, depth),  // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        usage,                               // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        DE_NULL,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout initialLayout;
    };

    return imageCreateInfo;
}

class RayTracingBuiltinLaunchTestInstance : public TestInstance
{
public:
    RayTracingBuiltinLaunchTestInstance(Context &context, const CaseDef &data);
    ~RayTracingBuiltinLaunchTestInstance(void);
    tcu::TestStatus iterate(void);

protected:
    void checkSupportInInstance(void) const;
    Move<VkPipeline> makePipeline(de::MovePtr<RayTracingPipeline> &rayTracingPipeline, VkPipelineLayout pipelineLayout,
                                  const VkSpecializationInfo *specializationInfo);
    std::vector<int32_t> expectedIntValuesBuffer(void);
    std::vector<float> expectedFloatValuesBuffer(void);
    std::vector<float> expectedVectorValuesBuffer(void);
    std::vector<float> expectedMatrixValuesBuffer(void);
    de::MovePtr<BufferWithMemory> runTest(void);
    de::MovePtr<BufferWithMemory> createShaderBindingTable(const InstanceInterface &vki, const DeviceInterface &vkd,
                                                           const VkDevice device, const VkPhysicalDevice physicalDevice,
                                                           const VkPipeline pipeline, Allocator &allocator,
                                                           de::MovePtr<RayTracingPipeline> &rayTracingPipeline,
                                                           const uint32_t group);

    bool validateIntBuffer(de::MovePtr<BufferWithMemory> buffer);
    bool validateFloatBuffer(de::MovePtr<BufferWithMemory> buffer);
    bool validateVectorBuffer(de::MovePtr<BufferWithMemory> buffer);
    bool validateMatrixBuffer(de::MovePtr<BufferWithMemory> buffer);

    de::MovePtr<TopLevelAccelerationStructure> initTopAccelerationStructure(
        VkCommandBuffer cmdBuffer,
        vector<de::SharedPtr<BottomLevelAccelerationStructure>> &bottomLevelAccelerationStructures);
    vector<de::SharedPtr<BottomLevelAccelerationStructure>> initBottomAccelerationStructures(VkCommandBuffer cmdBuffer);
    de::MovePtr<BottomLevelAccelerationStructure> initBottomAccelerationStructure(VkCommandBuffer cmdBuffer,
                                                                                  tcu::UVec2 &startPos);

private:
    CaseDef m_data;
    VkShaderStageFlags m_shaders;
    uint32_t m_raygenShaderGroup;
    uint32_t m_missShaderGroup;
    uint32_t m_hitShaderGroup;
    uint32_t m_callableShaderGroup;
    uint32_t m_shaderGroupCount;
};

RayTracingBuiltinLaunchTestInstance::RayTracingBuiltinLaunchTestInstance(Context &context, const CaseDef &data)
    : vkt::TestInstance(context)
    , m_data(data)
    , m_shaders(0)
    , m_raygenShaderGroup(~0u)
    , m_missShaderGroup(~0u)
    , m_hitShaderGroup(~0u)
    , m_callableShaderGroup(~0u)
    , m_shaderGroupCount(0)
{
    const VkShaderStageFlags hitStages =
        VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
    BinaryCollection &collection = m_context.getBinaryCollection();
    uint32_t group               = 0;
    uint32_t shaderCount         = 0;

    if (collection.contains("rgen"))
        m_shaders |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    if (collection.contains("ahit"))
        m_shaders |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    if (collection.contains("chit"))
        m_shaders |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    if (collection.contains("miss"))
        m_shaders |= VK_SHADER_STAGE_MISS_BIT_KHR;
    if (collection.contains("sect"))
        m_shaders |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
    if (collection.contains("call"))
        m_shaders |= VK_SHADER_STAGE_CALLABLE_BIT_KHR;

    for (BinaryCollection::Iterator it = collection.begin(); it != collection.end(); ++it)
        shaderCount++;

    if (shaderCount != (uint32_t)dePop32(m_shaders))
        TCU_THROW(InternalError, "Unused shaders detected in the collection");

    if (0 != (m_shaders & VK_SHADER_STAGE_RAYGEN_BIT_KHR))
        m_raygenShaderGroup = group++;

    if (0 != (m_shaders & VK_SHADER_STAGE_MISS_BIT_KHR))
        m_missShaderGroup = group++;

    if (0 != (m_shaders & hitStages))
        m_hitShaderGroup = group++;

    if (0 != (m_shaders & VK_SHADER_STAGE_CALLABLE_BIT_KHR))
        m_callableShaderGroup = group++;

    m_shaderGroupCount = group;
}

RayTracingBuiltinLaunchTestInstance::~RayTracingBuiltinLaunchTestInstance(void)
{
}

class RayTracingTestCase : public TestCase
{
public:
    RayTracingTestCase(tcu::TestContext &context, const char *name, const CaseDef data);
    ~RayTracingTestCase(void);

    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    static inline const std::string getIntersectionPassthrough(void);
    static inline const std::string getMissPassthrough(void);
    static inline const std::string getHitPassthrough(void);

    CaseDef m_data;
};

RayTracingTestCase::RayTracingTestCase(tcu::TestContext &context, const char *name, const CaseDef data)
    : vkt::TestCase(context, name)
    , m_data(data)
{
}

RayTracingTestCase::~RayTracingTestCase(void)
{
}

void RayTracingTestCase::checkSupport(Context &context) const
{
    const bool pipelineFlagSkipTriangles =
        ((m_data.pipelineCreateFlags & VK_PIPELINE_CREATE_RAY_TRACING_SKIP_TRIANGLES_BIT_KHR) != 0);
    const bool pipelineFlagSkipAABSs =
        ((m_data.pipelineCreateFlags & VK_PIPELINE_CREATE_RAY_TRACING_SKIP_AABBS_BIT_KHR) != 0);
    const bool cullingFlags =
        m_data.rayFlagSkipTriangles || m_data.rayFlagSkipAABSs || pipelineFlagSkipTriangles || pipelineFlagSkipAABSs;

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

    if (cullingFlags && rayTracingPipelineFeaturesKHR.rayTraversalPrimitiveCulling == false)
        TCU_THROW(NotSupportedError,
                  "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTraversalPrimitiveCulling");

    if (m_data.useMaintenance5)
        context.requireDeviceFunctionality("VK_KHR_maintenance5");
}

const std::string RayTracingTestCase::getIntersectionPassthrough(void)
{
    const std::string intersectionPassthrough = "#version 460 core\n"
                                                "#extension GL_EXT_ray_tracing : require\n"
                                                "hitAttributeEXT vec3 hitAttribute;\n"
                                                "\n"
                                                "void main()\n"
                                                "{\n"
                                                "  reportIntersectionEXT(0.95f, 0x7Eu);\n"
                                                "}\n";

    return intersectionPassthrough;
}

const std::string RayTracingTestCase::getMissPassthrough(void)
{
    const std::string missPassthrough = "#version 460 core\n"
                                        "#extension GL_EXT_ray_tracing : require\n"
                                        "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
                                        "\n"
                                        "void main()\n"
                                        "{\n"
                                        "}\n";

    return missPassthrough;
}

const std::string RayTracingTestCase::getHitPassthrough(void)
{
    const std::string hitPassthrough = "#version 460 core\n"
                                       "#extension GL_EXT_ray_tracing : require\n"
                                       "hitAttributeEXT vec3 attribs;\n"
                                       "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
                                       "\n"
                                       "void main()\n"
                                       "{\n"
                                       "}\n";

    return hitPassthrough;
}

void RayTracingTestCase::initPrograms(SourceCollections &programCollection) const
{
    const bool useSC = m_data.useSpecConstants;
    DE_ASSERT(!useSC || m_data.id == TEST_ID_LAUNCH_ID_EXT);

    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

    if (m_data.id == TEST_ID_LAUNCH_ID_EXT || m_data.id == TEST_ID_LAUNCH_SIZE_EXT)
    {
        const std::string specConstants = "layout (constant_id=0) const highp int factor1 = 1;\n"
                                          "layout (constant_id=1) const highp float factor2 = 2.0;\n";

        const std::string updateImage = "  ivec3 p = ivec3(gl_LaunchIDEXT);\n"
                                        "  ivec3 v = ivec3(gl_" +
                                        std::string(m_data.name) +
                                        ");\n"
                                        "  int   r = v.x + " +
                                        (useSC ? "factor1" : "256") + " * (v.y + " + (useSC ? "int(factor2)" : "256") +
                                        " * v.z) + 1;\n"
                                        "  ivec4 c = ivec4(r,0,0,1);\n"
                                        "  imageStore(result, p, c);\n";

        switch (m_data.stage)
        {
        case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
        {
            std::stringstream css;
            css << "#version 460 core\n"
                   "#extension GL_EXT_ray_tracing : require\n"
                << (useSC ? specConstants : "")
                << "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
                   "\n"
                   "void main()\n"
                   "{\n"
                << updateImage << "}\n";

            programCollection.glslSources.add("rgen")
                << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;

            break;
        }

        case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
        {
            programCollection.glslSources.add("rgen")
                << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

            {
                std::stringstream css;
                css << "#version 460 core\n"
                       "#extension GL_EXT_ray_tracing : require\n"
                    << (useSC ? specConstants : "")
                    << "hitAttributeEXT vec3 attribs;\n"
                       "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
                       "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
                       "\n"
                       "void main()\n"
                       "{\n"
                    << updateImage << "}\n";

                programCollection.glslSources.add("ahit")
                    << glu::AnyHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
            }

            programCollection.glslSources.add("chit")
                << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("miss")
                << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

            break;
        }

        case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
        {
            programCollection.glslSources.add("rgen")
                << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

            {
                std::stringstream css;
                css << "#version 460 core\n"
                       "#extension GL_EXT_ray_tracing : require\n"
                    << (useSC ? specConstants : "")
                    << "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
                       "hitAttributeEXT vec3 attribs;\n"
                       "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
                       "\n"
                       "void main()\n"
                       "{\n"
                    << updateImage << "}\n";

                programCollection.glslSources.add("chit")
                    << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
            }

            programCollection.glslSources.add("ahit")
                << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("miss")
                << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

            break;
        }

        case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
        {
            programCollection.glslSources.add("rgen")
                << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

            {
                std::stringstream css;
                css << "#version 460 core\n"
                       "#extension GL_EXT_ray_tracing : require\n"
                    << (useSC ? specConstants : "")
                    << "hitAttributeEXT vec3 hitAttribute;\n"
                       "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
                       "\n"
                       "void main()\n"
                       "{\n"
                    << updateImage
                    << "  hitAttribute = vec3(0.0f, 0.0f, 0.0f);\n"
                       "  reportIntersectionEXT(1.0f, 0);\n"
                       "}\n";

                programCollection.glslSources.add("sect")
                    << glu::IntersectionSource(updateRayTracingGLSL(css.str())) << buildOptions;
            }

            programCollection.glslSources.add("ahit")
                << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("chit")
                << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("miss")
                << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

            break;
        }

        case VK_SHADER_STAGE_MISS_BIT_KHR:
        {
            programCollection.glslSources.add("rgen")
                << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

            {
                std::stringstream css;
                css << "#version 460 core\n"
                       "#extension GL_EXT_ray_tracing : require\n"
                    << (useSC ? specConstants : "")
                    << "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
                       "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
                       "\n"
                       "void main()\n"
                       "{\n"
                    << updateImage << "}\n";

                programCollection.glslSources.add("miss")
                    << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
            }

            programCollection.glslSources.add("ahit")
                << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("chit")
                << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;

            break;
        }

        case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
        {
            {
                std::stringstream css;
                css << "#version 460 core\n"
                       "#extension GL_EXT_ray_tracing : require\n"
                       "layout(location = 0) callableDataEXT float dummy;"
                       "layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
                       "\n"
                       "void main()\n"
                       "{\n"
                       "  executeCallableEXT(0, 0);\n"
                       "}\n";

                programCollection.glslSources.add("rgen")
                    << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
            }

            {
                std::stringstream css;
                css << "#version 460 core\n"
                       "#extension GL_EXT_ray_tracing : require\n"
                    << (useSC ? specConstants : "")
                    << "layout(location = 0) callableDataInEXT float dummy;"
                       "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
                       "\n"
                       "void main()\n"
                       "{\n"
                    << updateImage << "}\n";

                programCollection.glslSources.add("call")
                    << glu::CallableSource(updateRayTracingGLSL(css.str())) << buildOptions;
            }

            programCollection.glslSources.add("ahit")
                << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("chit")
                << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("miss")
                << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

            break;
        }

        default:
            TCU_THROW(InternalError, "Unknown stage");
        }
    }
    else if (m_data.id == TEST_ID_GEOMETRY_INDEX_EXT || m_data.id == TEST_ID_PRIMITIVE_ID ||
             m_data.id == TEST_ID_INSTANCE_ID || m_data.id == TEST_ID_INSTANCE_CUSTOM_INDEX_EXT ||
             m_data.id == TEST_ID_HIT_KIND_EXT)
    {
        const std::string conditionGeometryIndex = "  int   n = int(gl_LaunchIDEXT.x + gl_LaunchSizeEXT.x * "
                                                   "(gl_LaunchIDEXT.y + gl_LaunchSizeEXT.y * gl_LaunchIDEXT.z));\n"
                                                   "  int   m = (n / " +
                                                   de::toString(m_data.squaresGroupCount) + ") % " +
                                                   de::toString(m_data.geometriesGroupCount) +
                                                   ";\n"
                                                   "  if (r == m)";
        const std::string conditionPrimitiveId = "  int   n = int(gl_LaunchIDEXT.x + gl_LaunchSizeEXT.x * "
                                                 "(gl_LaunchIDEXT.y + gl_LaunchSizeEXT.y * gl_LaunchIDEXT.z));\n"
                                                 "  int   m = n % " +
                                                 de::toString(m_data.squaresGroupCount) +
                                                 ";\n"
                                                 "  if (r == m)";
        const std::string condition =
            (m_data.id == TEST_ID_GEOMETRY_INDEX_EXT) && (m_data.geomType == GEOM_TYPE_AABBS) ? conditionGeometryIndex :
            (m_data.id == TEST_ID_PRIMITIVE_ID) && (m_data.geomType == GEOM_TYPE_AABBS)       ? conditionPrimitiveId :
                                                                                                "";
        const std::string updateImage = "  ivec3 p = ivec3(gl_LaunchIDEXT);\n"
                                        "  int   r = int(gl_" +
                                        std::string(m_data.name) +
                                        ");\n"
                                        "  ivec4 c = ivec4(r,0,0,1);\n" +
                                        condition + "  imageStore(result, p, c);\n";

        switch (m_data.stage)
        {
        case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
        {
            programCollection.glslSources.add("rgen")
                << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

            {
                std::stringstream css;
                css << "#version 460 core\n"
                       "#extension GL_EXT_ray_tracing : require\n"
                       "hitAttributeEXT vec3 attribs;\n"
                       "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
                       "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
                       "\n"
                       "void main()\n"
                       "{\n"
                    << updateImage << "}\n";

                programCollection.glslSources.add("ahit")
                    << glu::AnyHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
            }

            programCollection.glslSources.add("chit")
                << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("miss")
                << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

            if (m_data.geomType == GEOM_TYPE_AABBS)
                programCollection.glslSources.add("sect")
                    << glu::IntersectionSource(updateRayTracingGLSL(getIntersectionPassthrough())) << buildOptions;

            break;
        }

        case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
        {
            programCollection.glslSources.add("rgen")
                << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

            {
                std::stringstream css;
                css << "#version 460 core\n"
                       "#extension GL_EXT_ray_tracing : require\n"
                       "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
                       "hitAttributeEXT vec3 attribs;\n"
                       "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
                       "\n"
                       "void main()\n"
                       "{\n"
                    << updateImage << "}\n";

                programCollection.glslSources.add("chit")
                    << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
            }

            programCollection.glslSources.add("ahit")
                << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("miss")
                << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

            if (m_data.geomType == GEOM_TYPE_AABBS)
            {
                const std::string intersectionShaderSingle = "#version 460 core\n"
                                                             "#extension GL_EXT_ray_tracing : require\n"
                                                             "hitAttributeEXT vec3 hitAttribute;\n"
                                                             "\n"
                                                             "void main()\n"
                                                             "{\n"
                                                             "  int r = int(gl_" +
                                                             std::string(m_data.name) + ");\n" + condition +
                                                             "  reportIntersectionEXT(0.95f, 0x7Eu);\n"
                                                             "}\n";
                const std::string intersectionShader =
                    condition.empty() ? getIntersectionPassthrough() : intersectionShaderSingle;

                programCollection.glslSources.add("sect")
                    << glu::IntersectionSource(updateRayTracingGLSL(intersectionShader)) << buildOptions;
            }

            break;
        }

        case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
        {
            programCollection.glslSources.add("rgen")
                << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

            {
                std::stringstream css;
                css << "#version 460 core\n"
                       "#extension GL_EXT_ray_tracing : require\n"
                       "hitAttributeEXT vec3 hitAttribute;\n"
                       "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
                       "\n"
                       "void main()\n"
                       "{\n"
                    << updateImage
                    << "  reportIntersectionEXT(0.95f, 0);\n"
                       "}\n";

                programCollection.glslSources.add("sect")
                    << glu::IntersectionSource(updateRayTracingGLSL(css.str())) << buildOptions;
            }

            programCollection.glslSources.add("ahit")
                << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("chit")
                << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("miss")
                << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

            break;
        }

        default:
            TCU_THROW(InternalError, "Unknown stage");
        }
    }
    else if (m_data.id == TEST_ID_INCOMING_RAY_FLAGS_EXT)
    {
        const bool cullingFlags            = m_data.rayFlagSkipTriangles || m_data.rayFlagSkipAABSs;
        const std::string cullingFlagsInit = (m_data.rayFlagSkipTriangles && m_data.rayFlagSkipAABSs) ?
                                                 "gl_RayFlagsSkipTrianglesEXT|gl_RayFlagsSkipAABBEXT" :
                                             m_data.rayFlagSkipTriangles ? "gl_RayFlagsSkipTrianglesEXT" :
                                             m_data.rayFlagSkipAABSs     ? "gl_RayFlagsSkipAABBEXT" :
                                                                           "gl_RayFlagsNoneEXT";
        const std::string updateImage      = "  ivec3 p = ivec3(gl_LaunchIDEXT);\n"
                                             "  int   r = int(gl_" +
                                        std::string(m_data.name) +
                                        ");\n"
                                        "  ivec4 c = ivec4(r,0,0,1);\n"
                                        "  imageStore(result, p, c);\n";
        const std::string intersectionShader = "#version 460 core\n"
                                               "#extension GL_EXT_ray_tracing : require\n"
                                               "hitAttributeEXT vec3 hitAttribute;\n"
                                               "\n"
                                               "void main()\n"
                                               "{\n"
                                               "  uint hitKind = " +
                                               std::string(m_data.frontFace ? "0x7Eu" : "0x7Fu") +
                                               ";\n"
                                               "  reportIntersectionEXT(0.95f, hitKind);\n"
                                               "}\n";
        const std::string raygenFlagsFragment = "\n"
                                                "  if      (0 != (n & (1<<" +
                                                de::toString(RAY_FLAG_BIT_OPAQUE_EXT) +
                                                "))) f = f | gl_RayFlagsOpaqueEXT;\n"
                                                "  else if (0 != (n & (1<<" +
                                                de::toString(RAY_FLAG_BIT_NO_OPAQUE_EXT) +
                                                "))) f = f | gl_RayFlagsNoOpaqueEXT;\n"
                                                "  else if (0 != (n & (1<<" +
                                                de::toString(RAY_FLAG_BIT_CULL_OPAQUE_EXT) +
                                                "))) f = f | gl_RayFlagsCullOpaqueEXT;\n"
                                                "  else if (0 != (n & (1<<" +
                                                de::toString(RAY_FLAG_BIT_CULL_NO_OPAQUE_EXT) +
                                                "))) f = f | gl_RayFlagsCullNoOpaqueEXT;\n"
                                                "\n"
                                                "  if      (0 != (n & (1<<" +
                                                de::toString(RAY_FLAG_BIT_CULL_BACK_FACING_TRIANGLES_EXT) +
                                                "))) f = f | gl_RayFlagsCullBackFacingTrianglesEXT;\n"
                                                "  else if (0 != (n & (1<<" +
                                                de::toString(RAY_FLAG_BIT_CULL_FRONT_FACING_TRIANGLES_EXT) +
                                                "))) f = f | gl_RayFlagsCullFrontFacingTrianglesEXT;\n"
                                                "\n"
                                                "  if      (0 != (n & (1<<" +
                                                de::toString(RAY_FLAG_BIT_TERMINATE_ON_FIRST_HIT_EXT) +
                                                "))) f = f | gl_RayFlagsTerminateOnFirstHitEXT;\n"
                                                "  if      (0 != (n & (1<<" +
                                                de::toString(RAY_FLAG_BIT_SKIP_CLOSEST_HIT_SHADER_EXT) +
                                                "))) f = f | gl_RayFlagsSkipClosestHitShaderEXT;\n"
                                                "\n";
        const std::string raygenShader =
            "#version 460 core\n"
            "#extension GL_EXT_ray_tracing : require\n" +
            (cullingFlags ? std::string("#extension GL_EXT_ray_flags_primitive_culling : require\n") : "") +
            "layout(location = 0) rayPayloadEXT vec3 hitValue;\n"
            "layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
            "\n"
            "void main()\n"
            "{\n"
            "  uint  n        = gl_LaunchIDEXT.x + gl_LaunchSizeEXT.x * (gl_LaunchIDEXT.y + gl_LaunchSizeEXT.y * "
            "gl_LaunchIDEXT.z);\n"
            "  uint  f        = " +
            cullingFlagsInit + ";\n" + raygenFlagsFragment +
            "  uint  rayFlags = f;\n"
            "  uint  cullMask = 0xFF;\n"
            "  float tmin     = 0.0;\n"
            "  float tmax     = 9.0;\n"
            "  vec3  origin   = vec3((float(gl_LaunchIDEXT.x) + 0.5f) / float(gl_LaunchSizeEXT.x), "
            "(float(gl_LaunchIDEXT.y) + 0.5f) / float(gl_LaunchSizeEXT.y), 0.0);\n"
            "  vec3  direct   = vec3(0.0, 0.0, -1.0);\n"
            "  traceRayEXT(topLevelAS, rayFlags, cullMask, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
            "}\n";

        switch (m_data.stage)
        {
        case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
        {
            programCollection.glslSources.add("rgen")
                << glu::RaygenSource(updateRayTracingGLSL(raygenShader)) << buildOptions;

            {
                std::stringstream css;
                css << "#version 460 core\n"
                       "#extension GL_EXT_ray_tracing : require\n"
                       "hitAttributeEXT vec3 hitAttribute;\n"
                       "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
                       "\n"
                       "void main()\n"
                       "{\n"
                    << updateImage << "}\n";

                programCollection.glslSources.add("sect")
                    << glu::IntersectionSource(updateRayTracingGLSL(css.str())) << buildOptions;
            }

            programCollection.glslSources.add("ahit")
                << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("chit")
                << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("miss")
                << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

            break;
        }

        case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
        {
            programCollection.glslSources.add("rgen")
                << glu::RaygenSource(updateRayTracingGLSL(raygenShader)) << buildOptions;

            {
                std::stringstream css;
                css << "#version 460 core\n"
                       "#extension GL_EXT_ray_tracing : require\n"
                       "hitAttributeEXT vec3 attribs;\n"
                       "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
                       "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
                       "\n"
                       "void main()\n"
                       "{\n"
                    << updateImage << "}\n";

                programCollection.glslSources.add("ahit")
                    << glu::AnyHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
            }

            programCollection.glslSources.add("chit")
                << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("miss")
                << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

            if (m_data.geomType == GEOM_TYPE_AABBS)
                programCollection.glslSources.add("sect")
                    << glu::IntersectionSource(updateRayTracingGLSL(intersectionShader)) << buildOptions;

            break;
        }

        case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
        {
            programCollection.glslSources.add("rgen")
                << glu::RaygenSource(updateRayTracingGLSL(raygenShader)) << buildOptions;

            {
                std::stringstream css;
                css << "#version 460 core\n"
                       "#extension GL_EXT_ray_tracing : require\n"
                       "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
                       "hitAttributeEXT vec3 attribs;\n"
                       "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
                       "\n"
                       "void main()\n"
                       "{\n"
                    << updateImage << "}\n";

                programCollection.glslSources.add("chit")
                    << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
            }

            programCollection.glslSources.add("ahit")
                << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("miss")
                << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

            if (m_data.geomType == GEOM_TYPE_AABBS)
                programCollection.glslSources.add("sect")
                    << glu::IntersectionSource(updateRayTracingGLSL(intersectionShader)) << buildOptions;

            break;
        }

        case VK_SHADER_STAGE_MISS_BIT_KHR:
        {
            programCollection.glslSources.add("rgen")
                << glu::RaygenSource(updateRayTracingGLSL(raygenShader)) << buildOptions;

            {
                std::stringstream css;
                css << "#version 460 core\n"
                       "#extension GL_EXT_ray_tracing : require\n"
                       "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
                       "\n"
                       "void main()\n"
                       "{\n"
                    << updateImage << "}\n";

                programCollection.glslSources.add("miss")
                    << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
            }

            programCollection.glslSources.add("chit")
                << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("ahit")
                << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;

            if (m_data.geomType == GEOM_TYPE_AABBS)
                programCollection.glslSources.add("sect")
                    << glu::IntersectionSource(updateRayTracingGLSL(intersectionShader)) << buildOptions;

            break;
        }

        default:
            TCU_THROW(InternalError, "Unknown stage");
        }
    }
    else if (m_data.id == TEST_ID_HIT_T_EXT || m_data.id == TEST_ID_RAY_T_MIN_EXT || m_data.id == TEST_ID_RAY_T_MAX_EXT)
    {
        const std::string raygenShader =
            "#version 460 core\n"
            "#extension GL_EXT_ray_tracing : require\n"
            "layout(location = 0) rayPayloadEXT vec3 hitValue;\n"
            "layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
            "\n"
            "void main()\n"
            "{\n"
            "  uint  cullMask = 0xFF;\n"
            "  float a      = float(gl_LaunchIDEXT.x) / gl_LaunchSizeEXT.x;\n"
            "  float b      = 1.0f + float(gl_LaunchIDEXT.y) / gl_LaunchSizeEXT.y;\n"
            "  float c      = 0.25f * a / b;\n"
            "  float tmin   = c;\n"
            "  float tmax   = 0.75f + c;\n"
            "  vec3  origin = vec3((float(gl_LaunchIDEXT.x) + 0.5f) / float(gl_LaunchSizeEXT.x), "
            "(float(gl_LaunchIDEXT.y) + 0.5f) / float(gl_LaunchSizeEXT.y), 0.0);\n"
            "  vec3  direct = vec3(0.0, 0.0, -1.0);\n"
            "  traceRayEXT(topLevelAS, gl_RayFlagsNoneEXT, cullMask, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
            "}\n";
        const std::string intersectionShader = "#version 460 core\n"
                                               "#extension GL_EXT_ray_tracing : require\n"
                                               "hitAttributeEXT vec3 hitAttribute;\n"
                                               "\n"
                                               "void main()\n"
                                               "{\n"
                                               "  float a = float(gl_LaunchIDEXT.x) / gl_LaunchSizeEXT.x;\n"
                                               "  float b = 1.0f + float(gl_LaunchIDEXT.y) / gl_LaunchSizeEXT.y;\n"
                                               "  float c = 0.25f * a / b;\n"
                                               "  reportIntersectionEXT(0.03125f + c, 0);\n"
                                               "}\n";
        const std::string updateImage        = "  ivec3 p = ivec3(gl_LaunchIDEXT);\n"
                                               "  int   r = int(" +
                                        de::toString(FIXED_POINT_DIVISOR) + ".0f * gl_" + std::string(m_data.name) +
                                        ");\n"
                                        "  ivec4 c = ivec4(r,0,0,1);\n"
                                        "  imageStore(result, p, c);\n";

        const std::string updateImageConcurrentlyMin = "  ivec3 p = ivec3(gl_LaunchIDEXT);\n"
                                                       "  int   r = int(" +
                                                       de::toString(FIXED_POINT_DIVISOR) + ".0f * gl_" +
                                                       std::string(m_data.name) +
                                                       ");\n"
                                                       "  imageAtomicMin(result, p, r);\n";

        switch (m_data.stage)
        {
        case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
        {
            programCollection.glslSources.add("rgen")
                << glu::RaygenSource(updateRayTracingGLSL(raygenShader)) << buildOptions;

            // The anyhit shader is launched multiple times for the same launch ID. To
            // avoid write-after-write hazard on result image data and because intesection shader
            // execution is not ordered, we need to use atomic operation to get proper (smallest)
            // TMa result.
            auto anyHitShaderUpdateImage =
                (m_data.id == TEST_ID_RAY_T_MAX_EXT ? updateImageConcurrentlyMin : updateImage);

            {
                std::stringstream css;
                css << "#version 460 core\n"
                       "#extension GL_EXT_ray_tracing : require\n"
                       "hitAttributeEXT vec3 attribs;\n"
                       "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
                       "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
                       "\n"
                       "void main()\n"
                       "{\n"
                    << anyHitShaderUpdateImage << "}\n";

                programCollection.glslSources.add("ahit")
                    << glu::AnyHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
            }

            programCollection.glslSources.add("chit")
                << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("miss")
                << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

            if (m_data.geomType == GEOM_TYPE_AABBS)
                programCollection.glslSources.add("sect")
                    << glu::IntersectionSource(updateRayTracingGLSL(intersectionShader)) << buildOptions;

            break;
        }

        case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
        {
            programCollection.glslSources.add("rgen")
                << glu::RaygenSource(updateRayTracingGLSL(raygenShader)) << buildOptions;

            {
                std::stringstream css;
                css << "#version 460 core\n"
                       "#extension GL_EXT_ray_tracing : require\n"
                       "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
                       "hitAttributeEXT vec3 attribs;\n"
                       "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
                       "\n"
                       "void main()\n"
                       "{\n"
                    << updateImage << "}\n";

                programCollection.glslSources.add("chit")
                    << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
            }

            programCollection.glslSources.add("ahit")
                << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("miss")
                << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

            if (m_data.geomType == GEOM_TYPE_AABBS)
                programCollection.glslSources.add("sect")
                    << glu::IntersectionSource(updateRayTracingGLSL(intersectionShader)) << buildOptions;

            break;
        }

        case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
        {
            programCollection.glslSources.add("rgen")
                << glu::RaygenSource(updateRayTracingGLSL(raygenShader)) << buildOptions;

            {
                // The intersection shader is launched multiple times for the same launch ID. To
                // avoid write-after-write hazard on result image data and because intesection shader
                // execution is not ordered, we need to use atomic operation to get proper (smallest)
                // TMa result.
                auto intersectionShaderUpdateImage =
                    (m_data.id == TEST_ID_RAY_T_MAX_EXT ? updateImageConcurrentlyMin : updateImage);
                std::stringstream css;
                css << "#version 460 core\n"
                       "#extension GL_EXT_ray_tracing : require\n"
                       "hitAttributeEXT vec3 hitAttribute;\n"
                       "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
                       "\n"
                       "void main()\n"
                       "{\n"
                    << intersectionShaderUpdateImage
                    << "\n"
                       "  float a = float(gl_LaunchIDEXT.x) / gl_LaunchSizeEXT.x;\n"
                       "  float b = 1.0f + float(gl_LaunchIDEXT.y) / gl_LaunchSizeEXT.y;\n"
                       "  reportIntersectionEXT(0.4375f + 0.25f * a / b, 0x7Eu);\n"
                       "}\n";

                programCollection.glslSources.add("sect")
                    << glu::IntersectionSource(updateRayTracingGLSL(css.str())) << buildOptions;
            }

            programCollection.glslSources.add("ahit")
                << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("chit")
                << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("miss")
                << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

            break;
        }

        case VK_SHADER_STAGE_MISS_BIT_KHR:
        {
            programCollection.glslSources.add("rgen")
                << glu::RaygenSource(updateRayTracingGLSL(raygenShader)) << buildOptions;

            {
                std::stringstream css;
                css << "#version 460 core\n"
                       "#extension GL_EXT_ray_tracing : require\n"
                       "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
                       "\n"
                       "void main()\n"
                       "{\n"
                    << updateImage << "}\n";

                programCollection.glslSources.add("miss")
                    << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
            }

            programCollection.glslSources.add("chit")
                << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("ahit")
                << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;

            if (m_data.geomType == GEOM_TYPE_AABBS)
                programCollection.glslSources.add("sect")
                    << glu::IntersectionSource(updateRayTracingGLSL(intersectionShader)) << buildOptions;

            break;
        }

        default:
            TCU_THROW(InternalError, "Unknown stage");
        }
    }
    else if (m_data.id == TEST_ID_WORLD_RAY_ORIGIN_EXT || m_data.id == TEST_ID_WORLD_RAY_DIRECTION_EXT ||
             m_data.id == TEST_ID_OBJECT_RAY_ORIGIN_EXT || m_data.id == TEST_ID_OBJECT_RAY_DIRECTION_EXT ||
             m_data.id == TEST_ID_OBJECT_TO_WORLD_EXT || m_data.id == TEST_ID_WORLD_TO_OBJECT_EXT ||
             m_data.id == TEST_ID_OBJECT_TO_WORLD_3X4_EXT || m_data.id == TEST_ID_WORLD_TO_OBJECT_3X4_EXT)
    {
        const bool matrix4x3 = (m_data.id == TEST_ID_OBJECT_TO_WORLD_EXT || m_data.id == TEST_ID_WORLD_TO_OBJECT_EXT);
        const bool matrix3x4 =
            (m_data.id == TEST_ID_OBJECT_TO_WORLD_3X4_EXT || m_data.id == TEST_ID_WORLD_TO_OBJECT_3X4_EXT);
        const bool matrixOutput         = matrix4x3 || matrix3x4;
        const std::string vectorLoop    = "  for (int ndx = 0; ndx < 3; ndx++)\n"
                                          "  {\n";
        const std::string matrixLoop4x3 = "  int ndx = -1;\n"
                                          "  for (int row = 0; row < 3; row++)\n"
                                          "  for (int col = 0; col < 4; col++)\n"
                                          "  {\n"
                                          "    ndx++;\n";
        const std::string matrixLoop3x4 = "  int ndx = -1;\n"
                                          "  for (int col = 0; col < 3; col++)\n"
                                          "  for (int row = 0; row < 4; row++)\n"
                                          "  {\n"
                                          "    ndx++;\n";
        const std::string loop          = matrix4x3 ? matrixLoop4x3 : matrix3x4 ? matrixLoop3x4 : vectorLoop;
        const std::string index         = (matrixOutput ? "[col][row]" : "[ndx]");
        const std::string updateImage   = "  float k = " + de::toString(FIXED_POINT_DIVISOR) + ".0f;\n" + loop +
                                        "    ivec3 p = ivec3(gl_LaunchIDEXT.xy, ndx);\n"
                                        "    float r = k * gl_" +
                                        std::string(m_data.name) + index +
                                        ";\n"
                                        "    ivec4 c = ivec4(int(r),0,0,1);\n"
                                        "    imageStore(result, p, c);\n"
                                        "  }\n";

        switch (m_data.stage)
        {
        case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
        {
            programCollection.glslSources.add("rgen")
                << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

            {
                std::stringstream css;
                css << "#version 460 core\n"
                       "#extension GL_EXT_ray_tracing : require\n"
                       "hitAttributeEXT vec3 attribs;\n"
                       "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
                       "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
                       "\n"
                       "void main()\n"
                       "{\n"
                    << updateImage << "}\n";

                programCollection.glslSources.add("ahit")
                    << glu::AnyHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
            }

            programCollection.glslSources.add("chit")
                << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("miss")
                << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

            if (m_data.geomType == GEOM_TYPE_AABBS)
                programCollection.glslSources.add("sect")
                    << glu::IntersectionSource(updateRayTracingGLSL(getIntersectionPassthrough())) << buildOptions;

            break;
        }

        case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
        {
            programCollection.glslSources.add("rgen")
                << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

            {
                std::stringstream css;
                css << "#version 460 core\n"
                       "#extension GL_EXT_ray_tracing : require\n"
                       "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
                       "hitAttributeEXT vec3 attribs;\n"
                       "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
                       "\n"
                       "void main()\n"
                       "{\n"
                    << updateImage << "}\n";

                programCollection.glslSources.add("chit")
                    << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
            }

            programCollection.glslSources.add("ahit")
                << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("miss")
                << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

            if (m_data.geomType == GEOM_TYPE_AABBS)
                programCollection.glslSources.add("sect")
                    << glu::IntersectionSource(updateRayTracingGLSL(getIntersectionPassthrough())) << buildOptions;

            break;
        }

        case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
        {
            programCollection.glslSources.add("rgen")
                << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

            {
                std::stringstream css;
                css << "#version 460 core\n"
                       "#extension GL_EXT_ray_tracing : require\n"
                       "hitAttributeEXT vec3 hitAttribute;\n"
                       "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
                       "\n"
                       "void main()\n"
                       "{\n"
                    << updateImage
                    << "  reportIntersectionEXT(0.95f, 0);\n"
                       "}\n";

                programCollection.glslSources.add("sect")
                    << glu::IntersectionSource(updateRayTracingGLSL(css.str())) << buildOptions;
            }

            programCollection.glslSources.add("chit")
                << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("ahit")
                << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("miss")
                << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

            break;
        }

        case VK_SHADER_STAGE_MISS_BIT_KHR:
        {
            programCollection.glslSources.add("rgen")
                << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

            {
                std::stringstream css;
                css << "#version 460 core\n"
                       "#extension GL_EXT_ray_tracing : require\n"
                       "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
                       "\n"
                       "void main()\n"
                       "{\n"
                    << updateImage << "}\n";

                programCollection.glslSources.add("miss")
                    << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
            }

            programCollection.glslSources.add("chit")
                << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
            programCollection.glslSources.add("ahit")
                << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;

            if (m_data.geomType == GEOM_TYPE_AABBS)
                programCollection.glslSources.add("sect")
                    << glu::IntersectionSource(updateRayTracingGLSL(getIntersectionPassthrough())) << buildOptions;

            break;
        }

        default:
            TCU_THROW(InternalError, "Unknown stage");
        }
    }
    else
    {
        TCU_THROW(InternalError, "Not implemented");
    }
}

TestInstance *RayTracingTestCase::createInstance(Context &context) const
{
    return new RayTracingBuiltinLaunchTestInstance(context, m_data);
}

de::MovePtr<TopLevelAccelerationStructure> RayTracingBuiltinLaunchTestInstance::initTopAccelerationStructure(
    VkCommandBuffer cmdBuffer,
    vector<de::SharedPtr<BottomLevelAccelerationStructure>> &bottomLevelAccelerationStructures)
{
    const DeviceInterface &vkd                        = m_context.getDeviceInterface();
    const VkDevice device                             = m_context.getDevice();
    Allocator &allocator                              = m_context.getDefaultAllocator();
    de::MovePtr<TopLevelAccelerationStructure> result = makeTopLevelAccelerationStructure();
    const bool transformTest =
        m_data.id == TEST_ID_WORLD_RAY_ORIGIN_EXT || m_data.id == TEST_ID_WORLD_RAY_DIRECTION_EXT ||
        m_data.id == TEST_ID_OBJECT_RAY_ORIGIN_EXT || m_data.id == TEST_ID_OBJECT_RAY_DIRECTION_EXT ||
        m_data.id == TEST_ID_OBJECT_TO_WORLD_EXT || m_data.id == TEST_ID_WORLD_TO_OBJECT_EXT ||
        m_data.id == TEST_ID_OBJECT_TO_WORLD_3X4_EXT || m_data.id == TEST_ID_WORLD_TO_OBJECT_3X4_EXT;

    result->setInstanceCount(bottomLevelAccelerationStructures.size());

    for (size_t structNdx = 0; structNdx < bottomLevelAccelerationStructures.size(); ++structNdx)
    {
        VkTransformMatrixKHR transform = identityMatrix3x4;

        if (transformTest)
        {
            if (structNdx & 1)
                transform.matrix[0][3] = (1.0f / 8.0f) / float(m_data.width);

            if (structNdx & 2)
                transform.matrix[1][3] = (1.0f / 16.0f) / float(m_data.height);
        }

        result->addInstance(bottomLevelAccelerationStructures[structNdx], transform, uint32_t(2 * structNdx));
    }

    result->createAndBuild(vkd, device, cmdBuffer, allocator);

    return result;
}

de::MovePtr<BottomLevelAccelerationStructure> RayTracingBuiltinLaunchTestInstance::initBottomAccelerationStructure(
    VkCommandBuffer cmdBuffer, tcu::UVec2 &startPos)
{
    const DeviceInterface &vkd                           = m_context.getDeviceInterface();
    const VkDevice device                                = m_context.getDevice();
    Allocator &allocator                                 = m_context.getDefaultAllocator();
    de::MovePtr<BottomLevelAccelerationStructure> result = makeBottomLevelAccelerationStructure();

    result->setGeometryCount(m_data.geometriesGroupCount);

    if (m_data.id == TEST_ID_LAUNCH_ID_EXT || m_data.id == TEST_ID_LAUNCH_SIZE_EXT)
    {
        result->setDefaultGeometryData(m_data.stage);
    }
    else if (m_data.id == TEST_ID_GEOMETRY_INDEX_EXT || m_data.id == TEST_ID_PRIMITIVE_ID ||
             m_data.id == TEST_ID_INSTANCE_ID || m_data.id == TEST_ID_INSTANCE_CUSTOM_INDEX_EXT)
    {
        const bool triangles  = (m_data.geomType == GEOM_TYPE_TRIANGLES);
        const bool missShader = (m_data.stage == VK_SHADER_STAGE_MISS_BIT_KHR);
        const float z         = !missShader ? -1.0f : -100.0f;

        DE_ASSERT(m_data.squaresGroupCount != 1);

        for (size_t geometryNdx = 0; geometryNdx < m_data.geometriesGroupCount; ++geometryNdx)
        {
            std::vector<tcu::Vec3> geometryData;

            geometryData.reserve(m_data.squaresGroupCount * (triangles ? 3u : 2u));

            for (size_t squareNdx = 0; squareNdx < m_data.squaresGroupCount; ++squareNdx)
            {
                const uint32_t n = m_data.width * startPos.y() + startPos.x();
                const float x0   = float(startPos.x() + 0) / float(m_data.width);
                const float y0   = float(startPos.y() + 0) / float(m_data.height);
                const float x1   = float(startPos.x() + 1) / float(m_data.width);
                const float y1   = float(startPos.y() + 1) / float(m_data.height);
                const uint32_t m = n + 1;

                if (triangles)
                {
                    const float xm = (x0 + x1) / 2.0f;
                    const float ym = (y0 + y1) / 2.0f;

                    geometryData.push_back(tcu::Vec3(x0, y0, z));
                    geometryData.push_back(tcu::Vec3(xm, y1, z));
                    geometryData.push_back(tcu::Vec3(x1, ym, z));
                }
                else
                {
                    geometryData.push_back(tcu::Vec3(x0, y0, z));
                    geometryData.push_back(tcu::Vec3(x1, y1, z * 0.9f));
                }

                startPos.y() = m / m_data.width;
                startPos.x() = m % m_data.width;
            }

            result->addGeometry(geometryData, triangles);
        }
    }
    else if (m_data.id == TEST_ID_HIT_KIND_EXT)
    {
        const bool triangles  = (m_data.geomType == GEOM_TYPE_TRIANGLES);
        const bool missShader = (m_data.stage == VK_SHADER_STAGE_MISS_BIT_KHR);
        const float z         = !missShader ? -1.0f : -100.0f;

        DE_ASSERT(m_data.squaresGroupCount != 1);
        DE_ASSERT(m_data.geometriesGroupCount == 4);

        std::vector<tcu::Vec3> geometryDataOpaque;
        std::vector<tcu::Vec3> geometryDataNonOpaque;

        geometryDataOpaque.reserve(m_data.squaresGroupCount * (triangles ? 3u : 2u));
        geometryDataNonOpaque.reserve(m_data.squaresGroupCount * (triangles ? 3u : 2u));

        for (size_t geometryNdx = 0; geometryNdx < m_data.geometriesGroupCount; ++geometryNdx)
        {
            const bool cw = ((geometryNdx & 1) == 0) ? true : false;
            std::vector<tcu::Vec3> &geometryData =
                ((geometryNdx & 2) == 0) ? geometryDataOpaque : geometryDataNonOpaque;

            for (size_t squareNdx = 0; squareNdx < m_data.squaresGroupCount; ++squareNdx)
            {
                const uint32_t n = m_data.width * startPos.y() + startPos.x();
                const uint32_t m = n + 1;
                const float x0   = float(startPos.x() + 0) / float(m_data.width);
                const float y0   = float(startPos.y() + 0) / float(m_data.height);
                const float x1   = float(startPos.x() + 1) / float(m_data.width);
                const float y1   = float(startPos.y() + 1) / float(m_data.height);

                if (triangles)
                {
                    const float xm = (x0 + x1) / 2.0f;
                    const float ym = (y0 + y1) / 2.0f;

                    if (cw)
                    {
                        geometryData.push_back(tcu::Vec3(x0, y0, z));
                        geometryData.push_back(tcu::Vec3(x1, ym, z));
                        geometryData.push_back(tcu::Vec3(xm, y1, z));
                    }
                    else
                    {
                        geometryData.push_back(tcu::Vec3(x0, y0, z));
                        geometryData.push_back(tcu::Vec3(xm, y1, z));
                        geometryData.push_back(tcu::Vec3(x1, ym, z));
                    }
                }
                else
                {
                    geometryData.push_back(tcu::Vec3(x0, y0, z));
                    geometryData.push_back(tcu::Vec3(x1, y1, z * 0.9f));
                }

                startPos.y() = m / m_data.width;
                startPos.x() = m % m_data.width;
            }
        }

        DE_ASSERT(startPos.y() == m_data.height && startPos.x() == 0);

        result->addGeometry(geometryDataOpaque, triangles, (VkGeometryFlagsKHR)VK_GEOMETRY_OPAQUE_BIT_KHR);
        result->addGeometry(geometryDataNonOpaque, triangles, (VkGeometryFlagsKHR)0);
    }
    else if (m_data.id == TEST_ID_INCOMING_RAY_FLAGS_EXT)
    {
        const bool triangles                   = (m_data.geomType == GEOM_TYPE_TRIANGLES);
        const bool missShader                  = (m_data.stage == VK_SHADER_STAGE_MISS_BIT_KHR);
        const float z                          = !missShader ? -1.0f : -100.0f;
        const VkGeometryFlagsKHR geometryFlags = m_data.opaque ?
                                                     static_cast<VkGeometryFlagsKHR>(VK_GEOMETRY_OPAQUE_BIT_KHR) :
                                                     static_cast<VkGeometryFlagsKHR>(0);
        const bool cw                          = m_data.frontFace;
        std::vector<tcu::Vec3> geometryData;

        DE_ASSERT(m_data.geometriesGroupCount == 1);
        DE_ASSERT(m_data.squaresGroupCount != 1);

        geometryData.reserve(m_data.squaresGroupCount * (triangles ? 3u : 2u));

        for (size_t squareNdx = 0; squareNdx < m_data.squaresGroupCount; ++squareNdx)
        {
            const uint32_t n = m_data.width * startPos.y() + startPos.x();
            const uint32_t m = n + 1;
            const float x0   = float(startPos.x() + 0) / float(m_data.width);
            const float y0   = float(startPos.y() + 0) / float(m_data.height);
            const float x1   = float(startPos.x() + 1) / float(m_data.width);
            const float y1   = float(startPos.y() + 1) / float(m_data.height);

            if (triangles)
            {
                const float xm = (x0 + x1) / 2.0f;
                const float ym = (y0 + y1) / 2.0f;

                if (cw)
                {
                    geometryData.push_back(tcu::Vec3(x0, y0, z));
                    geometryData.push_back(tcu::Vec3(x1, ym, z));
                    geometryData.push_back(tcu::Vec3(xm, y1, z));
                }
                else
                {
                    geometryData.push_back(tcu::Vec3(x0, y0, z));
                    geometryData.push_back(tcu::Vec3(xm, y1, z));
                    geometryData.push_back(tcu::Vec3(x1, ym, z));
                }
            }
            else
            {
                geometryData.push_back(tcu::Vec3(x0, y0, z));
                geometryData.push_back(tcu::Vec3(x1, y1, z * 0.9f));
            }

            startPos.y() = m / m_data.width;
            startPos.x() = m % m_data.width;
        }

        DE_ASSERT(startPos.y() == m_data.height && startPos.x() == 0);

        result->addGeometry(geometryData, triangles, geometryFlags);
    }
    else if (m_data.id == TEST_ID_HIT_T_EXT || m_data.id == TEST_ID_RAY_T_MIN_EXT || m_data.id == TEST_ID_RAY_T_MAX_EXT)
    {
        const bool triangles  = (m_data.geomType == GEOM_TYPE_TRIANGLES);
        const bool missShader = (m_data.stage == VK_SHADER_STAGE_MISS_BIT_KHR);
        const bool sectShader = (m_data.stage == VK_SHADER_STAGE_INTERSECTION_BIT_KHR);
        const bool maxTTest   = (m_data.id == TEST_ID_RAY_T_MAX_EXT);

        DE_ASSERT(m_data.squaresGroupCount != 1);

        for (size_t geometryNdx = 0; geometryNdx < m_data.geometriesGroupCount; ++geometryNdx)
        {
            std::vector<tcu::Vec3> geometryData;

            geometryData.reserve(m_data.squaresGroupCount * (triangles ? 3u : 2u));

            for (size_t squareNdx = 0; squareNdx < m_data.squaresGroupCount; ++squareNdx)
            {
                const uint32_t n = m_data.width * startPos.y() + startPos.x();
                const uint32_t m = n + 1;
                const bool shiftRight =
                    sectShader && maxTTest && (0 == (startPos.y() & 1)) && (0 == (startPos.x() & 1));
                const uint32_t xo = shiftRight ? 1 : 0;
                const float x0    = float(startPos.x() + 0 + xo) / float(m_data.width);
                const float y0    = float(startPos.y() + 0) / float(m_data.height);
                const float x1    = float(startPos.x() + 1 + xo) / float(m_data.width);
                const float y1    = float(startPos.y() + 1) / float(m_data.height);
                const float a     = x0;
                const float b     = 1.0f + y0;
                const float c     = 0.03125f + 0.25f * a / b;
                const float z     = !missShader ? -c : -100.0f;

                if (triangles)
                {
                    const float xm = (x0 + x1) / 2.0f;
                    const float ym = (y0 + y1) / 2.0f;

                    geometryData.push_back(tcu::Vec3(x0, y0, z));
                    geometryData.push_back(tcu::Vec3(xm, y1, z));
                    geometryData.push_back(tcu::Vec3(x1, ym, z));
                }
                else
                {
                    geometryData.push_back(tcu::Vec3(x0, y0, z));
                    geometryData.push_back(tcu::Vec3(x1, y1, z * 0.9f));
                }

                startPos.y() = m / m_data.width;
                startPos.x() = m % m_data.width;
            }

            result->addGeometry(geometryData, triangles);
        }
    }
    else if (m_data.id == TEST_ID_WORLD_RAY_ORIGIN_EXT || m_data.id == TEST_ID_WORLD_RAY_DIRECTION_EXT ||
             m_data.id == TEST_ID_OBJECT_RAY_ORIGIN_EXT || m_data.id == TEST_ID_OBJECT_RAY_DIRECTION_EXT ||
             m_data.id == TEST_ID_OBJECT_TO_WORLD_EXT || m_data.id == TEST_ID_WORLD_TO_OBJECT_EXT ||
             m_data.id == TEST_ID_OBJECT_TO_WORLD_3X4_EXT || m_data.id == TEST_ID_WORLD_TO_OBJECT_3X4_EXT)
    {
        const bool triangles  = m_data.geomType == GEOM_TYPE_TRIANGLES;
        const float y0        = float(startPos.y() + 0) / float(m_data.height);
        const float y1        = float(startPos.y() + 1) / float(m_data.height);
        const bool missShader = (m_data.stage == VK_SHADER_STAGE_MISS_BIT_KHR);
        const float z         = !missShader ? -1.0f : -100.0f;
        std::vector<tcu::Vec3> geometryData;

        if (triangles)
        {
            geometryData.push_back(tcu::Vec3(-1.0f, y1, z));
            geometryData.push_back(tcu::Vec3(-1.0f, y0, z));
            geometryData.push_back(tcu::Vec3(+1.0f, y0, z));
            geometryData.push_back(tcu::Vec3(-1.0f, y1, z));
            geometryData.push_back(tcu::Vec3(+1.0f, y0, z));
            geometryData.push_back(tcu::Vec3(+1.0f, y1, z));
        }
        else
        {
            geometryData.reserve(2);

            geometryData.push_back(tcu::Vec3(-1.0f, y0, z));
            geometryData.push_back(tcu::Vec3(+1.0f, y1, z));
        }

        DE_ASSERT(startPos.y() < m_data.height);

        startPos.y()++;

        result->addGeometry(geometryData, triangles);
    }
    else
    {
        TCU_THROW(InternalError, "Not implemented");
    }

    result->createAndBuild(vkd, device, cmdBuffer, allocator);

    return result;
}

vector<de::SharedPtr<BottomLevelAccelerationStructure>> RayTracingBuiltinLaunchTestInstance::
    initBottomAccelerationStructures(VkCommandBuffer cmdBuffer)
{
    tcu::UVec2 startPos;
    vector<de::SharedPtr<BottomLevelAccelerationStructure>> result;

    for (size_t instanceNdx = 0; instanceNdx < m_data.instancesGroupCount; ++instanceNdx)
    {
        de::MovePtr<BottomLevelAccelerationStructure> bottomLevelAccelerationStructure =
            initBottomAccelerationStructure(cmdBuffer, startPos);

        result.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));
    }

    return result;
}

Move<VkPipeline> RayTracingBuiltinLaunchTestInstance::makePipeline(de::MovePtr<RayTracingPipeline> &rayTracingPipeline,
                                                                   VkPipelineLayout pipelineLayout,
                                                                   const VkSpecializationInfo *specializationInfo)
{
    const DeviceInterface &vkd       = m_context.getDeviceInterface();
    const VkDevice device            = m_context.getDevice();
    vk::BinaryCollection &collection = m_context.getBinaryCollection();

    if (0 != (m_shaders & VK_SHADER_STAGE_RAYGEN_BIT_KHR))
        rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                                      createShaderModule(vkd, device, collection.get("rgen"), 0), m_raygenShaderGroup,
                                      specializationInfo);
    if (0 != (m_shaders & VK_SHADER_STAGE_ANY_HIT_BIT_KHR))
        rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
                                      createShaderModule(vkd, device, collection.get("ahit"), 0), m_hitShaderGroup,
                                      specializationInfo);
    if (0 != (m_shaders & VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR))
        rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                                      createShaderModule(vkd, device, collection.get("chit"), 0), m_hitShaderGroup,
                                      specializationInfo);
    if (0 != (m_shaders & VK_SHADER_STAGE_MISS_BIT_KHR))
        rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR,
                                      createShaderModule(vkd, device, collection.get("miss"), 0), m_missShaderGroup,
                                      specializationInfo);
    if (0 != (m_shaders & VK_SHADER_STAGE_INTERSECTION_BIT_KHR))
        rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
                                      createShaderModule(vkd, device, collection.get("sect"), 0), m_hitShaderGroup,
                                      specializationInfo);
    if (0 != (m_shaders & VK_SHADER_STAGE_CALLABLE_BIT_KHR))
        rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR,
                                      createShaderModule(vkd, device, collection.get("call"), 0), m_callableShaderGroup,
                                      specializationInfo);

    if (m_data.pipelineCreateFlags != 0)
    {
        rayTracingPipeline->setCreateFlags(m_data.pipelineCreateFlags);
        if (m_data.useMaintenance5)
            rayTracingPipeline->setCreateFlags2(translateCreateFlag(m_data.pipelineCreateFlags));
    }

    Move<VkPipeline> pipeline = rayTracingPipeline->createPipeline(vkd, device, pipelineLayout);

    return pipeline;
}

de::MovePtr<BufferWithMemory> RayTracingBuiltinLaunchTestInstance::createShaderBindingTable(
    const InstanceInterface &vki, const DeviceInterface &vkd, const VkDevice device,
    const VkPhysicalDevice physicalDevice, const VkPipeline pipeline, Allocator &allocator,
    de::MovePtr<RayTracingPipeline> &rayTracingPipeline, const uint32_t group)
{
    de::MovePtr<BufferWithMemory> shaderBindingTable;

    if (group < m_shaderGroupCount)
    {
        const uint32_t shaderGroupHandleSize    = getShaderGroupSize(vki, physicalDevice);
        const uint32_t shaderGroupBaseAlignment = getShaderGroupBaseAlignment(vki, physicalDevice);

        shaderBindingTable = rayTracingPipeline->createShaderBindingTable(
            vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, group, 1u);
    }

    return shaderBindingTable;
}

// Provides two spec constants, one integer and one float, both with value 256.
class SpecConstantsHelper
{
public:
    SpecConstantsHelper();
    const VkSpecializationInfo &getSpecializationInfo(void) const;

private:
    std::vector<uint8_t> m_data;
    std::vector<VkSpecializationMapEntry> m_mapEntries;
    VkSpecializationInfo m_specInfo;
};

SpecConstantsHelper::SpecConstantsHelper() : m_data(), m_mapEntries()
{
    // To make things interesting, make both data unaligned and add some padding.
    const int32_t value1 = 256;
    const float value2   = 256.0f;

    const size_t offset1 = 1u;                       // Offset of 1 byte.
    const size_t offset2 = 1u + sizeof(value1) + 2u; // Offset of 3 bytes plus the size of value1.

    m_data.resize(sizeof(value1) + sizeof(value2) + 5u); // Some extra padding at the end too.
    deMemcpy(&m_data[offset1], &value1, sizeof(value1));
    deMemcpy(&m_data[offset2], &value2, sizeof(value2));

    // Map entries.
    m_mapEntries.reserve(2u);
    m_mapEntries.push_back({0u, static_cast<uint32_t>(offset1), static_cast<uintptr_t>(sizeof(value1))});
    m_mapEntries.push_back({1u, static_cast<uint32_t>(offset2), static_cast<uintptr_t>(sizeof(value2))});

    // Specialization info.
    m_specInfo.mapEntryCount = static_cast<uint32_t>(m_mapEntries.size());
    m_specInfo.pMapEntries   = m_mapEntries.data();
    m_specInfo.dataSize      = static_cast<uintptr_t>(m_data.size());
    m_specInfo.pData         = m_data.data();
}

const VkSpecializationInfo &SpecConstantsHelper::getSpecializationInfo(void) const
{
    return m_specInfo;
}

de::MovePtr<BufferWithMemory> RayTracingBuiltinLaunchTestInstance::runTest(void)
{
    const InstanceInterface &vki          = m_context.getInstanceInterface();
    const DeviceInterface &vkd            = m_context.getDeviceInterface();
    const VkDevice device                 = m_context.getDevice();
    const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    const uint32_t queueFamilyIndex       = m_context.getUniversalQueueFamilyIndex();
    const VkQueue queue                   = m_context.getUniversalQueue();
    Allocator &allocator                  = m_context.getDefaultAllocator();
    const uint32_t shaderGroupHandleSize  = getShaderGroupSize(vki, physicalDevice);
    const VkFormat format                 = m_data.format;
    const uint32_t pixelSize              = tcu::getPixelSize(mapVkFormat(format));
    const uint32_t pixelCount             = m_data.width * m_data.height * m_data.depth;

    const Move<VkDescriptorSetLayout> descriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, ALL_RAY_TRACING_STAGES)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, ALL_RAY_TRACING_STAGES)
            .build(vkd, device);
    const Move<VkDescriptorPool> descriptorPool =
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            .addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
            .build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const Move<VkDescriptorSet> descriptorSet   = makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout);
    const Move<VkPipelineLayout> pipelineLayout = makePipelineLayout(vkd, device, descriptorSetLayout.get());
    const Move<VkCommandPool> cmdPool           = createCommandPool(vkd, device, 0, queueFamilyIndex);
    const Move<VkCommandBuffer> cmdBuffer =
        allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    de::MovePtr<RayTracingPipeline> rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();
    const SpecConstantsHelper specConstantHelper;
    const VkSpecializationInfo *specializationInfo =
        (m_data.useSpecConstants ? &specConstantHelper.getSpecializationInfo() : nullptr);
    const Move<VkPipeline> pipeline = makePipeline(rayTracingPipeline, *pipelineLayout, specializationInfo);
    const de::MovePtr<BufferWithMemory> raygenShaderBindingTable = createShaderBindingTable(
        vki, vkd, device, physicalDevice, *pipeline, allocator, rayTracingPipeline, m_raygenShaderGroup);
    const de::MovePtr<BufferWithMemory> missShaderBindingTable = createShaderBindingTable(
        vki, vkd, device, physicalDevice, *pipeline, allocator, rayTracingPipeline, m_missShaderGroup);
    const de::MovePtr<BufferWithMemory> hitShaderBindingTable = createShaderBindingTable(
        vki, vkd, device, physicalDevice, *pipeline, allocator, rayTracingPipeline, m_hitShaderGroup);
    const de::MovePtr<BufferWithMemory> callableShaderBindingTable = createShaderBindingTable(
        vki, vkd, device, physicalDevice, *pipeline, allocator, rayTracingPipeline, m_callableShaderGroup);

    const VkStridedDeviceAddressRegionKHR raygenShaderBindingTableRegion =
        raygenShaderBindingTable.get() != NULL ?
            makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(), 0),
                                              shaderGroupHandleSize, shaderGroupHandleSize) :
            makeStridedDeviceAddressRegionKHR(0, 0, 0);
    const VkStridedDeviceAddressRegionKHR missShaderBindingTableRegion =
        missShaderBindingTable.get() != NULL ?
            makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missShaderBindingTable->get(), 0),
                                              shaderGroupHandleSize, shaderGroupHandleSize) :
            makeStridedDeviceAddressRegionKHR(0, 0, 0);
    const VkStridedDeviceAddressRegionKHR hitShaderBindingTableRegion =
        hitShaderBindingTable.get() != NULL ?
            makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(), 0),
                                              shaderGroupHandleSize, shaderGroupHandleSize) :
            makeStridedDeviceAddressRegionKHR(0, 0, 0);
    const VkStridedDeviceAddressRegionKHR callableShaderBindingTableRegion =
        callableShaderBindingTable.get() != NULL ?
            makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, callableShaderBindingTable->get(), 0),
                                              shaderGroupHandleSize, shaderGroupHandleSize) :
            makeStridedDeviceAddressRegionKHR(0, 0, 0);

    const VkImageCreateInfo imageCreateInfo = makeImageCreateInfo(m_data.width, m_data.height, m_data.depth, format);
    const VkImageSubresourceRange imageSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0, 1u);
    const de::MovePtr<ImageWithMemory> image = de::MovePtr<ImageWithMemory>(
        new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
    const Move<VkImageView> imageView =
        makeImageView(vkd, device, **image, VK_IMAGE_VIEW_TYPE_3D, format, imageSubresourceRange);

    const VkBufferCreateInfo bufferCreateInfo =
        makeBufferCreateInfo(pixelCount * pixelSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const VkImageSubresourceLayers bufferImageSubresourceLayers =
        makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
    const VkBufferImageCopy bufferImageRegion =
        makeBufferImageCopy(makeExtent3D(m_data.width, m_data.height, m_data.depth), bufferImageSubresourceLayers);
    de::MovePtr<BufferWithMemory> buffer = de::MovePtr<BufferWithMemory>(
        new BufferWithMemory(vkd, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));

    const VkDescriptorImageInfo descriptorImageInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, *imageView, VK_IMAGE_LAYOUT_GENERAL);

    const VkImageMemoryBarrier preImageBarrier =
        makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, **image, imageSubresourceRange);
    const VkImageMemoryBarrier postImageBarrier = makeImageMemoryBarrier(
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, **image, imageSubresourceRange);
    const VkMemoryBarrier postTraceMemoryBarrier =
        makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    const VkMemoryBarrier postCopyMemoryBarrier =
        makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    const VkClearValue clearValue =
        makeClearValueColorU32(DEFAULT_UINT_CLEAR_VALUE, DEFAULT_UINT_CLEAR_VALUE, DEFAULT_UINT_CLEAR_VALUE, 255u);

    vector<de::SharedPtr<BottomLevelAccelerationStructure>> bottomLevelAccelerationStructures;
    de::MovePtr<TopLevelAccelerationStructure> topLevelAccelerationStructure;

    beginCommandBuffer(vkd, *cmdBuffer, 0u);
    {
        cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &preImageBarrier);
        vkd.cmdClearColorImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color, 1,
                               &imageSubresourceRange);
        cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, &postImageBarrier);

        bottomLevelAccelerationStructures = initBottomAccelerationStructures(*cmdBuffer);
        topLevelAccelerationStructure     = initTopAccelerationStructure(*cmdBuffer, bottomLevelAccelerationStructures);

        const TopLevelAccelerationStructure *topLevelAccelerationStructurePtr = topLevelAccelerationStructure.get();
        VkWriteDescriptorSetAccelerationStructureKHR accelerationStructureWriteDescriptorSet = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR, //  VkStructureType sType;
            DE_NULL,                                                           //  const void* pNext;
            1u,                                                                //  uint32_t accelerationStructureCount;
            topLevelAccelerationStructurePtr->getPtr(), //  const VkAccelerationStructureKHR* pAccelerationStructures;
        };

        DescriptorSetUpdateBuilder()
            .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorImageInfo)
            .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                         VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelerationStructureWriteDescriptorSet)
            .update(vkd, device);

        vkd.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipelineLayout, 0, 1,
                                  &descriptorSet.get(), 0, DE_NULL);

        vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipeline);

        cmdTraceRays(vkd, *cmdBuffer, &raygenShaderBindingTableRegion, &missShaderBindingTableRegion,
                     &hitShaderBindingTableRegion, &callableShaderBindingTableRegion, m_data.width, m_data.height,
                     m_data.raysDepth);

        cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, &postTraceMemoryBarrier);

        vkd.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **buffer, 1u, &bufferImageRegion);

        cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &postCopyMemoryBarrier);
    }
    endCommandBuffer(vkd, *cmdBuffer);

    submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

    invalidateMappedMemoryRange(vkd, device, buffer->getAllocation().getMemory(), buffer->getAllocation().getOffset(),
                                VK_WHOLE_SIZE);

    return buffer;
}

void checkFormatSupported(Context &context, VkFormat format, VkImageUsageFlags usage, const VkExtent3D &extent)
{
    VkResult result;
    VkImageFormatProperties properties;

    result = context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
        context.getPhysicalDevice(), format, VK_IMAGE_TYPE_3D, VK_IMAGE_TILING_OPTIMAL, usage, 0, &properties);

    if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
    {
        std::ostringstream msg;

        msg << "Format " << format << " not supported for usage flags 0x" << std::hex << usage;

        TCU_THROW(NotSupportedError, msg.str());
    }

    if (properties.maxExtent.width < extent.width || properties.maxExtent.height < extent.height ||
        properties.maxExtent.depth < extent.depth)
        TCU_THROW(NotSupportedError, "Image size is too large for this format");

    VK_CHECK(result);
}

void RayTracingBuiltinLaunchTestInstance::checkSupportInInstance(void) const
{
    const InstanceInterface &vki                     = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice            = m_context.getPhysicalDevice();
    const vk::VkPhysicalDeviceProperties &properties = m_context.getDeviceProperties();
    const uint32_t requiredAllocations =
        8u + TopLevelAccelerationStructure::getRequiredAllocationCount() +
        m_data.instancesGroupCount * BottomLevelAccelerationStructure::getRequiredAllocationCount();
    const de::MovePtr<RayTracingProperties> rayTracingProperties = makeRayTracingProperties(vki, physicalDevice);
    const VkExtent3D extent = makeExtent3D(m_data.width, m_data.height, m_data.depth);

    checkFormatSupported(m_context, m_data.format,
                         VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                         extent);

    if (rayTracingProperties->getMaxPrimitiveCount() < 2 * m_data.squaresGroupCount)
        TCU_THROW(NotSupportedError, "Triangles required more than supported");

    if (rayTracingProperties->getMaxGeometryCount() < m_data.geometriesGroupCount)
        TCU_THROW(NotSupportedError, "Geometries required more than supported");

    if (rayTracingProperties->getMaxInstanceCount() < m_data.instancesGroupCount)
        TCU_THROW(NotSupportedError, "Instances required more than supported");

    if (properties.limits.maxMemoryAllocationCount < requiredAllocations)
        TCU_THROW(NotSupportedError, "Test requires more allocations allowed");
}

std::vector<int32_t> RayTracingBuiltinLaunchTestInstance::expectedIntValuesBuffer(void)
{
    uint32_t pos = 0;
    std::vector<int32_t> result;

    result.reserve(m_data.depth * m_data.height * m_data.width);

    if (m_data.id == TEST_ID_LAUNCH_ID_EXT)
    {
        for (uint32_t z = 0; z < m_data.depth; ++z)
            for (uint32_t y = 0; y < m_data.height; ++y)
                for (uint32_t x = 0; x < m_data.width; ++x)
                    result.push_back(int32_t(x + 256 * (y + 256 * z)) + 1);
    }
    else if (m_data.id == TEST_ID_LAUNCH_SIZE_EXT)
    {
        const uint32_t expectedValue = m_data.width + 256 * (m_data.height + 256 * m_data.depth);
        const std::vector<int32_t> result2(m_data.depth * m_data.height * m_data.width, int32_t(expectedValue) + 1);

        result = result2;
    }
    else if (m_data.id == TEST_ID_GEOMETRY_INDEX_EXT)
    {
        for (uint32_t z = 0; z < m_data.depth; ++z)
            for (uint32_t y = 0; y < m_data.height; ++y)
                for (uint32_t x = 0; x < m_data.width; ++x)
                    result.push_back(int32_t((pos++ / m_data.squaresGroupCount) % m_data.geometriesGroupCount));
    }
    else if (m_data.id == TEST_ID_PRIMITIVE_ID)
    {
        for (uint32_t z = 0; z < m_data.depth; ++z)
            for (uint32_t y = 0; y < m_data.height; ++y)
                for (uint32_t x = 0; x < m_data.width; ++x)
                    result.push_back(int32_t(pos++ % m_data.squaresGroupCount));
    }
    else if (m_data.id == TEST_ID_INSTANCE_ID)
    {
        for (uint32_t z = 0; z < m_data.depth; ++z)
            for (uint32_t y = 0; y < m_data.height; ++y)
                for (uint32_t x = 0; x < m_data.width; ++x)
                    result.push_back(int32_t(pos++ / (m_data.squaresGroupCount * m_data.geometriesGroupCount)));
    }
    else if (m_data.id == TEST_ID_INSTANCE_CUSTOM_INDEX_EXT)
    {
        for (uint32_t z = 0; z < m_data.depth; ++z)
            for (uint32_t y = 0; y < m_data.height; ++y)
                for (uint32_t x = 0; x < m_data.width; ++x)
                    result.push_back(int32_t(2 * (pos++ / (m_data.squaresGroupCount * m_data.geometriesGroupCount))));
    }
    else if (m_data.id == TEST_ID_INCOMING_RAY_FLAGS_EXT)
    {
        DE_ASSERT(m_data.squaresGroupCount == (1 << RAY_FLAG_BIT_LAST_PER_TEST));
        DE_ASSERT(DEFAULT_UINT_CLEAR_VALUE != (1 << RAY_FLAG_BIT_LAST_PER_TEST));

        for (uint32_t z = 0; z < m_data.depth; ++z)
            for (uint32_t y = 0; y < m_data.height; ++y)
                for (uint32_t x = 0; x < m_data.width; ++x)
                {
                    const uint32_t n                   = x + m_data.width * (y + m_data.height * z);
                    const bool rayOpaque               = (0 != (n & (1 << RAY_FLAG_BIT_OPAQUE_EXT)));
                    const bool rayNoOpaque             = (0 != (n & (1 << RAY_FLAG_BIT_NO_OPAQUE_EXT))) && !rayOpaque;
                    const bool rayTerminateOnFirstHit  = (0 != (n & (1 << RAY_FLAG_BIT_TERMINATE_ON_FIRST_HIT_EXT)));
                    const bool raySkipClosestHitShader = (0 != (n & (1 << RAY_FLAG_BIT_SKIP_CLOSEST_HIT_SHADER_EXT)));
                    const bool rayCullBack = (0 != (n & (1 << RAY_FLAG_BIT_CULL_BACK_FACING_TRIANGLES_EXT)));
                    const bool rayCullFront =
                        (0 != (n & (1 << RAY_FLAG_BIT_CULL_FRONT_FACING_TRIANGLES_EXT))) && !rayCullBack;
                    const bool rayCullOpaque =
                        (0 != (n & (1 << RAY_FLAG_BIT_CULL_OPAQUE_EXT))) && !rayOpaque && !rayNoOpaque;
                    const bool rayCullNoOpaque = (0 != (n & (1 << RAY_FLAG_BIT_CULL_NO_OPAQUE_EXT))) && !rayOpaque &&
                                                 !rayNoOpaque && !rayCullOpaque;
                    const bool raySkipTriangles = m_data.rayFlagSkipTriangles;
                    const bool raySkipAABBs     = m_data.rayFlagSkipAABSs;
                    const bool pipelineSkipTriangles =
                        (m_data.pipelineCreateFlags & VK_PIPELINE_CREATE_RAY_TRACING_SKIP_TRIANGLES_BIT_KHR) != 0;
                    const bool pipelineSkipAABBs =
                        (m_data.pipelineCreateFlags & VK_PIPELINE_CREATE_RAY_TRACING_SKIP_AABBS_BIT_KHR) != 0;
                    const bool cullingTest = m_data.rayFlagSkipTriangles || m_data.rayFlagSkipAABSs ||
                                             pipelineSkipTriangles || pipelineSkipAABBs;
                    const bool geometryFrontFace = m_data.frontFace;
                    const bool geometryOpaque    = m_data.opaque;
                    const bool geometryTriangles = (m_data.geomType == GEOM_TYPE_TRIANGLES) ? true : false;
                    const bool geometryAABBs     = (m_data.geomType == GEOM_TYPE_AABBS) ? true : false;
                    uint32_t v                   = 0 | (rayOpaque ? (1 << RAY_FLAG_BIT_OPAQUE_EXT) : 0) |
                                 (rayNoOpaque ? (1 << RAY_FLAG_BIT_NO_OPAQUE_EXT) : 0) |
                                 (rayTerminateOnFirstHit ? (1 << RAY_FLAG_BIT_TERMINATE_ON_FIRST_HIT_EXT) : 0) |
                                 (raySkipClosestHitShader ? (1 << RAY_FLAG_BIT_SKIP_CLOSEST_HIT_SHADER_EXT) : 0) |
                                 (rayCullBack ? (1 << RAY_FLAG_BIT_CULL_BACK_FACING_TRIANGLES_EXT) : 0) |
                                 (rayCullFront ? (1 << RAY_FLAG_BIT_CULL_FRONT_FACING_TRIANGLES_EXT) : 0) |
                                 (rayCullOpaque ? (1 << RAY_FLAG_BIT_CULL_OPAQUE_EXT) : 0) |
                                 (rayCullNoOpaque ? (1 << RAY_FLAG_BIT_CULL_NO_OPAQUE_EXT) : 0) |
                                 (raySkipTriangles ? (1 << RAY_FLAG_BIT_SKIP_TRIANGLES_EXT) : 0) |
                                 (raySkipAABBs ? (1 << RAY_FLAG_BIT_SKIP_AABB_EXT) : 0);

                    if (m_data.stage == VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR && raySkipClosestHitShader)
                        v = DEFAULT_UINT_CLEAR_VALUE;

                    if (m_data.stage == VK_SHADER_STAGE_ANY_HIT_BIT_KHR &&
                        (rayOpaque || (geometryOpaque && !rayOpaque && !rayNoOpaque)))
                        v = DEFAULT_UINT_CLEAR_VALUE;

                    if (geometryOpaque)
                    {
                        if (rayCullOpaque)
                            if (m_data.stage != VK_SHADER_STAGE_MISS_BIT_KHR)
                                v = DEFAULT_UINT_CLEAR_VALUE;
                    }
                    else
                    {
                        if (rayCullNoOpaque)
                            if (m_data.stage != VK_SHADER_STAGE_MISS_BIT_KHR)
                                v = DEFAULT_UINT_CLEAR_VALUE;
                    }

                    if (geometryTriangles)
                    {
                        if (geometryFrontFace)
                        {
                            if (rayCullFront)
                                if (m_data.stage != VK_SHADER_STAGE_MISS_BIT_KHR)
                                    v = DEFAULT_UINT_CLEAR_VALUE;
                        }
                        else
                        {
                            if (rayCullBack)
                                if (m_data.stage != VK_SHADER_STAGE_MISS_BIT_KHR)
                                    v = DEFAULT_UINT_CLEAR_VALUE;
                        }
                    }

                    if (cullingTest)
                    {
                        if (m_data.stage != VK_SHADER_STAGE_MISS_BIT_KHR)
                        {
                            if (geometryTriangles)
                            {
                                if (raySkipTriangles || pipelineSkipTriangles)
                                    v = DEFAULT_UINT_CLEAR_VALUE;
                            }

                            if (geometryAABBs)
                            {
                                if (raySkipAABBs || pipelineSkipAABBs)
                                    v = DEFAULT_UINT_CLEAR_VALUE;
                            }
                        }
                    }

                    result.push_back(int32_t(v));
                }
    }
    else if (m_data.id == TEST_ID_HIT_KIND_EXT)
    {
        for (uint32_t z = 0; z < m_data.depth; ++z)
            for (uint32_t y = 0; y < m_data.height; ++y)
                for (uint32_t x = 0; x < m_data.width; ++x)
                {
                    const uint32_t n           = x + m_data.width * (y + m_data.height * z);
                    const uint32_t geometryNdx = n / m_data.squaresGroupCount;
                    const uint32_t hitKind     = ((geometryNdx & 1) == 0) ? 0xFEu : 0xFFu;
                    const bool geometryOpaque  = ((geometryNdx & 2) == 0) ? true : false;
                    uint32_t v                 = (m_data.geomType == GEOM_TYPE_TRIANGLES) ? hitKind : 0x7Eu;

                    if (m_data.stage == VK_SHADER_STAGE_ANY_HIT_BIT_KHR && geometryOpaque)
                        v = DEFAULT_UINT_CLEAR_VALUE;

                    result.push_back(int32_t(v));
                }
    }
    else
    {
        TCU_THROW(InternalError, "Not implemented");
    }

    return result;
}

std::vector<float> RayTracingBuiltinLaunchTestInstance::expectedFloatValuesBuffer(void)
{
    std::vector<float> result;

    result.reserve(m_data.depth * m_data.height * m_data.width);

    if (m_data.id == TEST_ID_HIT_T_EXT)
    {
        for (uint32_t z = 0; z < m_data.depth; ++z)
            for (uint32_t y = 0; y < m_data.height; ++y)
                for (uint32_t x = 0; x < m_data.width; ++x)
                {
                    const float a = float(x) / float(m_data.width);
                    const float b = 1.0f + float(y) / float(m_data.height);
                    const float f = 0.03125f + 0.25f * a / b;

                    result.push_back(f);
                }
    }
    else if (m_data.id == TEST_ID_RAY_T_MIN_EXT)
    {
        for (uint32_t z = 0; z < m_data.depth; ++z)
            for (uint32_t y = 0; y < m_data.height; ++y)
                for (uint32_t x = 0; x < m_data.width; ++x)
                {
                    const float a = float(x) / float(m_data.width);
                    const float b = 1.0f + float(y) / float(m_data.height);
                    const float f = 0.25f * a / b;

                    result.push_back(f);
                }
    }
    else if (m_data.id == TEST_ID_RAY_T_MAX_EXT)
    {
        for (uint32_t z = 0; z < m_data.depth; ++z)
            for (uint32_t y = 0; y < m_data.height; ++y)
                for (uint32_t x = 0; x < m_data.width; ++x)
                {
                    const float a = float(x) / float(m_data.width);
                    const float b = 1.0f + float(y) / float(m_data.height);
                    const float c = 0.25f * a / b;
                    // In a miss shader, the value is identical to the parameter passed into traceRayEXT().
                    const float m = 0.75f + c;
                    // In the closest-hit shader, the value reflects the closest distance to the intersected primitive.
                    // In the any-hit shader, it reflects the distance to the primitive currently being intersected.
                    // In the intersection shader, it reflects the distance to the closest primitive intersected so far.
                    const float n              = 0.03125f + c;
                    const bool normalRow       = (y & 1) != 0;
                    const bool doublePrimitive = (x & 1) != 0;
                    const float s              = normalRow       ? m :
                                                 doublePrimitive ? 0.4375f + c :
                                                                   float(DEFAULT_UINT_CLEAR_VALUE) / float(FIXED_POINT_DIVISOR);
                    const float f              = (m_data.stage == VK_SHADER_STAGE_INTERSECTION_BIT_KHR) ? s :
                                                 (m_data.stage == VK_SHADER_STAGE_MISS_BIT_KHR)         ? m :
                                                                                                          n;

                    result.push_back(f);
                }
    }
    else
    {
        TCU_THROW(InternalError, "Not implemented");
    }

    return result;
}

std::vector<float> RayTracingBuiltinLaunchTestInstance::expectedVectorValuesBuffer(void)
{
    const uint32_t imageDepth     = 4;
    const uint32_t expectedFloats = imageDepth * m_data.height * m_data.width;
    std::vector<float> result(expectedFloats, float(DEFAULT_UINT_CLEAR_VALUE) / float(FIXED_POINT_DIVISOR));

    if (m_data.id == TEST_ID_WORLD_RAY_ORIGIN_EXT)
    {
        uint32_t pos = 0;

        for (uint32_t y = 0; y < m_data.height; ++y)
            for (uint32_t x = 0; x < m_data.width; ++x)
                result[pos++] = (0.5f + float(x)) / float(m_data.width);

        for (uint32_t y = 0; y < m_data.height; ++y)
            for (uint32_t x = 0; x < m_data.width; ++x)
                result[pos++] = (0.5f + float(y)) / float(m_data.height);

        for (uint32_t y = 0; y < m_data.height; ++y)
            for (uint32_t x = 0; x < m_data.width; ++x)
                result[pos++] = 0.0f;
    }
    else if (m_data.id == TEST_ID_WORLD_RAY_DIRECTION_EXT)
    {
        uint32_t pos = 0;

        for (uint32_t y = 0; y < m_data.height; ++y)
            for (uint32_t x = 0; x < m_data.width; ++x)
                result[pos++] = 0.0f;

        for (uint32_t y = 0; y < m_data.height; ++y)
            for (uint32_t x = 0; x < m_data.width; ++x)
                result[pos++] = 0.0f;

        for (uint32_t y = 0; y < m_data.height; ++y)
            for (uint32_t x = 0; x < m_data.width; ++x)
                result[pos++] = -1.0f;
    }
    else if (m_data.id == TEST_ID_OBJECT_RAY_ORIGIN_EXT)
    {
        uint32_t pos = 0;

        for (uint32_t y = 0; y < m_data.height; ++y)
            for (uint32_t x = 0; x < m_data.width; ++x)
            {
                const uint32_t instanceId = y / (m_data.height / m_data.instancesGroupCount);
                const float offset        = (instanceId & 1) ? 1.0f / 8.0f : 0.0f;

                result[pos++] = (0.5f + float(x) - offset) / float(m_data.width);
            }

        for (uint32_t y = 0; y < m_data.height; ++y)
            for (uint32_t x = 0; x < m_data.width; ++x)
            {
                const uint32_t instanceId = y / (m_data.height / m_data.instancesGroupCount);
                const float offset        = (instanceId & 2) ? 1.0f / 16.0f : 0.0f;

                result[pos++] = (0.5f + float(y) - offset) / float(m_data.height);
            }

        for (uint32_t y = 0; y < m_data.height; ++y)
            for (uint32_t x = 0; x < m_data.width; ++x)
                result[pos++] = 0.0f;
    }
    else if (m_data.id == TEST_ID_OBJECT_RAY_DIRECTION_EXT)
    {
        uint32_t pos = 0;

        for (uint32_t y = 0; y < m_data.height; ++y)
            for (uint32_t x = 0; x < m_data.width; ++x)
                result[pos++] = 0.0f;

        for (uint32_t y = 0; y < m_data.height; ++y)
            for (uint32_t x = 0; x < m_data.width; ++x)
                result[pos++] = 0.0f;

        for (uint32_t y = 0; y < m_data.height; ++y)
            for (uint32_t x = 0; x < m_data.width; ++x)
                result[pos++] = -1.0f;
    }
    else
    {
        TCU_THROW(InternalError, "Not implemented");
    }

    return result;
}

std::vector<float> RayTracingBuiltinLaunchTestInstance::expectedMatrixValuesBuffer(void)
{
    const uint32_t colCount       = 4;
    const uint32_t rowCount       = 3;
    const uint32_t imageDepth     = 4 * 4;
    const uint32_t zStride        = m_data.height * m_data.width;
    const uint32_t expectedFloats = imageDepth * m_data.height * m_data.width;
    std::vector<float> result(expectedFloats, float(DEFAULT_UINT_CLEAR_VALUE) / float(FIXED_POINT_DIVISOR));

    if (m_data.id == TEST_ID_OBJECT_TO_WORLD_EXT || m_data.id == TEST_ID_WORLD_TO_OBJECT_EXT ||
        m_data.id == TEST_ID_OBJECT_TO_WORLD_3X4_EXT || m_data.id == TEST_ID_WORLD_TO_OBJECT_3X4_EXT)
    {
        const int translateColumnNumber = 3;
        const float translateSign =
            (m_data.id == TEST_ID_WORLD_TO_OBJECT_EXT || m_data.id == TEST_ID_WORLD_TO_OBJECT_3X4_EXT) ? -1.0f : +1.0f;
        const float translateX = translateSign * (1.0f / 8.0f) / float(m_data.width);
        const float translateY = translateSign * (1.0f / 16.0f) / float(m_data.height);

        for (uint32_t y = 0; y < m_data.height; ++y)
        {
            const uint32_t instanceId = y / (m_data.height / m_data.instancesGroupCount);

            for (uint32_t x = 0; x < m_data.width; ++x)
            {
                tcu::Matrix<float, rowCount, colCount> m;
                const uint32_t elem0Pos = x + m_data.width * y;

                if (instanceId & 1)
                    m[translateColumnNumber][0] = translateX;

                if (instanceId & 2)
                    m[translateColumnNumber][1] = translateY;

                for (uint32_t rowNdx = 0; rowNdx < rowCount; ++rowNdx)
                    for (uint32_t colNdx = 0; colNdx < colCount; ++colNdx)
                    {
                        const uint32_t z   = rowNdx * colCount + colNdx;
                        const uint32_t pos = elem0Pos + zStride * z;

                        result[pos] = m[colNdx][rowNdx];
                    }
            }
        }
    }
    else
    {
        TCU_THROW(InternalError, "Not implemented");
    }

    return result;
}

bool RayTracingBuiltinLaunchTestInstance::validateIntBuffer(de::MovePtr<BufferWithMemory> buffer)
{
    const int32_t *bufferPtr             = (int32_t *)buffer->getAllocation().getHostPtr();
    const vector<int32_t> expectedValues = expectedIntValuesBuffer();
    tcu::TestLog &log                    = m_context.getTestContext().getLog();
    uint32_t failures                    = 0;
    uint32_t pos                         = 0;

    for (uint32_t z = 0; z < m_data.depth; ++z)
        for (uint32_t y = 0; y < m_data.height; ++y)
            for (uint32_t x = 0; x < m_data.width; ++x)
            {
                if (bufferPtr[pos] != expectedValues[pos])
                    failures++;

                ++pos;
            }

    if (failures != 0)
    {
        const char *names[] = {"Retrieved:", "Expected:"};
        for (uint32_t n = 0; n < 2; ++n)
        {
            const int32_t *loggedData = (n == 0) ? bufferPtr : expectedValues.data();
            std::stringstream css;

            pos = 0;

            for (uint32_t z = 0; z < m_data.depth; ++z)
                for (uint32_t y = 0; y < m_data.height; ++y)
                {
                    for (uint32_t x = 0; x < m_data.width; ++x)
                    {
                        if (bufferPtr[pos] == expectedValues[pos])
                            css << "____,";
                        else
                            css << std::hex << std::setw(4) << loggedData[pos] << ",";

                        pos++;
                    }

                    css << std::endl;
                }

            log << tcu::TestLog::Message << names[n] << tcu::TestLog::EndMessage;
            log << tcu::TestLog::Message << css.str() << tcu::TestLog::EndMessage;
        }
    }

    return failures == 0;
}

bool RayTracingBuiltinLaunchTestInstance::validateFloatBuffer(de::MovePtr<BufferWithMemory> buffer)
{
    const float eps                    = float(FIXED_POINT_ALLOWED_ERROR) / float(FIXED_POINT_DIVISOR);
    const int32_t *bufferPtr           = (int32_t *)buffer->getAllocation().getHostPtr();
    const vector<float> expectedValues = expectedFloatValuesBuffer();
    tcu::TestLog &log                  = m_context.getTestContext().getLog();
    uint32_t failures                  = 0;
    uint32_t pos                       = 0;

    for (uint32_t z = 0; z < m_data.depth; ++z)
        for (uint32_t y = 0; y < m_data.height; ++y)
            for (uint32_t x = 0; x < m_data.width; ++x)
            {
                const float retrievedValue = float(bufferPtr[pos]) / float(FIXED_POINT_DIVISOR);

                if (deFloatAbs(retrievedValue - expectedValues[pos]) > eps)
                    failures++;

                ++pos;
            }

    if (failures != 0)
    {
        const char *names[] = {"Retrieved:", "Expected:"};

        for (uint32_t n = 0; n < 2; ++n)
        {
            std::stringstream css;

            pos = 0;

            for (uint32_t z = 0; z < m_data.depth; ++z)
                for (uint32_t y = 0; y < m_data.height; ++y)
                {
                    for (uint32_t x = 0; x < m_data.width; ++x)
                    {
                        const float retrievedValue = float(bufferPtr[pos]) / float(FIXED_POINT_DIVISOR);
                        const float expectedValue  = expectedValues[pos];

                        if (deFloatAbs(retrievedValue - expectedValue) > eps)
                            css << std::setprecision(8) << std::setw(12) << (n == 0 ? retrievedValue : expectedValue)
                                << ",";
                        else
                            css << "____________,";

                        pos++;
                    }

                    css << std::endl;
                }

            log << tcu::TestLog::Message << names[n] << tcu::TestLog::EndMessage;
            log << tcu::TestLog::Message << css.str() << tcu::TestLog::EndMessage;
        }
    }

    return failures == 0;
}

bool RayTracingBuiltinLaunchTestInstance::validateVectorBuffer(de::MovePtr<BufferWithMemory> buffer)
{
    const float eps                    = float(FIXED_POINT_ALLOWED_ERROR) / float(FIXED_POINT_DIVISOR);
    const int32_t *bufferPtr           = (int32_t *)buffer->getAllocation().getHostPtr();
    const vector<float> expectedValues = expectedVectorValuesBuffer();
    const uint32_t depth               = 3u; // vec3
    tcu::TestLog &log                  = m_context.getTestContext().getLog();
    uint32_t failures                  = 0;
    uint32_t pos                       = 0;

    DE_ASSERT(depth <= m_data.depth);

    for (uint32_t z = 0; z < depth; ++z)
        for (uint32_t y = 0; y < m_data.height; ++y)
            for (uint32_t x = 0; x < m_data.width; ++x)
            {
                const float retrievedValue = float(bufferPtr[pos]) / float(FIXED_POINT_DIVISOR);

                if (deFloatAbs(retrievedValue - expectedValues[pos]) > eps)
                    failures++;

                ++pos;
            }

    if (failures != 0)
    {
        const char *names[] = {"Retrieved", "Expected "};
        std::stringstream css;

        for (uint32_t y = 0; y < m_data.height; ++y)
        {
            for (uint32_t x = 0; x < m_data.width; ++x)
            {
                for (uint32_t n = 0; n < 2; ++n)
                {
                    css << names[n] << " at (" << x << "," << y << ") {";

                    for (uint32_t z = 0; z < depth; ++z)
                    {
                        pos = x + m_data.width * (y + m_data.height * z);

                        const float retrievedValue = float(bufferPtr[pos]) / float(FIXED_POINT_DIVISOR);
                        const float expectedValue  = expectedValues[pos];

                        if (deFloatAbs(retrievedValue - expectedValue) > eps)
                            css << std::setprecision(8) << std::setw(12) << (n == 0 ? retrievedValue : expectedValue)
                                << ",";
                        else
                            css << "____________,";
                    }

                    css << "}" << std::endl;
                }
            }
        }

        log << tcu::TestLog::Message << css.str() << tcu::TestLog::EndMessage;
    }

    return failures == 0;
}

bool RayTracingBuiltinLaunchTestInstance::validateMatrixBuffer(de::MovePtr<BufferWithMemory> buffer)
{
    const float eps                    = float(FIXED_POINT_ALLOWED_ERROR) / float(FIXED_POINT_DIVISOR);
    const int32_t *bufferPtr           = (int32_t *)buffer->getAllocation().getHostPtr();
    const vector<float> expectedValues = expectedMatrixValuesBuffer();
    const uint32_t depth               = 12u; // mat3x4 or mat4x3
    tcu::TestLog &log                  = m_context.getTestContext().getLog();
    uint32_t failures                  = 0;
    uint32_t pos                       = 0;

    DE_ASSERT(depth <= m_data.depth);

    for (uint32_t z = 0; z < depth; ++z)
        for (uint32_t y = 0; y < m_data.height; ++y)
            for (uint32_t x = 0; x < m_data.width; ++x)
            {
                const float retrievedValue = float(bufferPtr[pos]) / float(FIXED_POINT_DIVISOR);

                if (deFloatAbs(retrievedValue - expectedValues[pos]) > eps)
                    failures++;

                ++pos;
            }

    if (failures != 0)
    {
        const char *names[] = {"Retrieved", "Expected"};
        std::stringstream css;

        for (uint32_t y = 0; y < m_data.height; ++y)
        {
            for (uint32_t x = 0; x < m_data.width; ++x)
            {
                css << "At (" << x << "," << y << ")" << std::endl;
                for (uint32_t n = 0; n < 2; ++n)
                {
                    css << names[n] << std::endl << "{" << std::endl;

                    for (uint32_t z = 0; z < depth; ++z)
                    {
                        pos = x + m_data.width * (y + m_data.height * z);

                        const float retrievedValue = float(bufferPtr[pos]) / float(FIXED_POINT_DIVISOR);
                        const float expectedValue  = expectedValues[pos];

                        if (z % 4 == 0)
                            css << "    {";

                        if (deFloatAbs(retrievedValue - expectedValue) > eps)
                            css << std::setprecision(5) << std::setw(9) << (n == 0 ? retrievedValue : expectedValue)
                                << ",";
                        else
                            css << "_________,";

                        if (z % 4 == 3)
                            css << "}" << std::endl;
                    }

                    css << "}" << std::endl;
                }
            }
        }

        log << tcu::TestLog::Message << css.str() << tcu::TestLog::EndMessage;
    }

    return failures == 0;
}

tcu::TestStatus RayTracingBuiltinLaunchTestInstance::iterate(void)
{
    checkSupportInInstance();

    de::MovePtr<BufferWithMemory> buffer = runTest();
    const bool ok                        = m_data.fixedPointMatrixOutput ? validateMatrixBuffer(buffer) :
                                           m_data.fixedPointVectorOutput ? validateVectorBuffer(buffer) :
                                           m_data.fixedPointScalarOutput ? validateFloatBuffer(buffer) :
                                                                           validateIntBuffer(buffer);

    if (ok)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("fail");
}

enum ShaderSourceFlag
{
    DEFINE_RAY             = 0x1,
    DEFINE_RESULT_BUFFER   = 0x2,
    DEFINE_SCENE           = 0x4,
    DEFINE_RAY_BUFFER      = 0x8,
    DEFINE_SIMPLE_BINDINGS = DEFINE_RESULT_BUFFER | DEFINE_SCENE | DEFINE_RAY_BUFFER
};

std::string generateShaderSource(const char *body, const char *resultType = "", uint32_t flags = 0,
                                 const char *prefix = "")
{
    std::ostringstream src;
    src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n";

    src << "#extension GL_EXT_ray_tracing : enable\n";

    src << prefix << "\n";

    if (flags & DEFINE_SIMPLE_BINDINGS)
        flags |= DEFINE_RAY_BUFFER;

    if (flags & DEFINE_RAY_BUFFER)
        flags |= DEFINE_RAY;

    if (flags & DEFINE_RAY)
    {
        src << "struct Ray { vec3 pos; float tmin; vec3 dir; float tmax; };\n";
    }

    if (flags & DEFINE_RESULT_BUFFER)
        src << "layout(std430, set = 0, binding = " << 0 << ") buffer Results { " << resultType << " results[]; };\n";

    if (flags & DEFINE_SCENE)
    {
        src << "layout(set = 0, binding = " << 1 << ") uniform accelerationStructureEXT scene;\n";
    }

    if (flags & DEFINE_RAY_BUFFER)
        src << "layout(std430, set = 0, binding = " << 2 << ") buffer Rays { Ray rays[]; };\n";

    src << "uint launchIndex() { return gl_LaunchIDEXT.z*gl_LaunchSizeEXT.x*gl_LaunchSizeEXT.y + "
           "gl_LaunchIDEXT.y*gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x; }\n";

    src << body;

    return src.str();
}

std::string getShaderIdentifier(const CaseDef &params, VkShaderStageFlagBits stage)
{
    std::string testStage;

    switch (params.stage)
    {
    case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
        testStage = "raygen";
        break;
    case VK_SHADER_STAGE_MISS_BIT_KHR:
        testStage = "-miss";
        break;
    case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
        testStage = "-closest-hit";
        break;
    case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
        testStage = "-any_hit";
        break;
    case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
        testStage = "-closest_hit";
        break;
    case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
        testStage = "-callable";
        break;
    default:
        DE_ASSERT(false);
        return std::string();
    }

    switch (stage)
    {
    case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
        return testStage + "-rgen";
    case VK_SHADER_STAGE_MISS_BIT_KHR:
        return testStage + "-miss";
    case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
        return testStage + "-closest_hit";
    case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
        return testStage + "-any_hit";
    case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
        return testStage + "-isect";
    case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
        return testStage + "-callable";
    default:
        DE_ASSERT(false);
        return std::string();
    }
}

std::string replaceString(std::string text, const std::string &search, const std::string &replace)
{
    size_t found;

    while ((found = text.find(search)) != std::string::npos)
    {
        text = text.replace(found, search.length(), replace);
    }

    return text;
}

template <typename T>
inline void addBuiltInShaderSource(SourceCollections &programCollection, VkShaderStageFlagBits stage,
                                   const std::string &body, const CaseDef &params, std::string builtInType)
{
    std::string identifier = getShaderIdentifier(params, stage);

    uint32_t flags = 0;

    if (stage == VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        flags |= DEFINE_RAY | DEFINE_SIMPLE_BINDINGS;

    std::string text = generateShaderSource(body.c_str(), builtInType.c_str(), flags, "");

    text = replaceString(text, "$builtInType$", builtInType);

    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0, true);
    programCollection.glslSources.add(identifier) << T(text) << buildOptions;
}

class RayTracingIndirectTestInstance : public TestInstance
{
public:
    RayTracingIndirectTestInstance(Context &context, const CaseDef &data);
    ~RayTracingIndirectTestInstance(void);
    tcu::TestStatus iterate(void);

private:
    void checkSupportInInstance(void) const;
    Move<VkPipeline> createPipelineAndShaderBindingTables(de::MovePtr<RayTracingPipeline> &rayTracingPipeline,
                                                          bool aabb, VkPipelineLayout pipelineLayout);

    void createPipelineLayoutAndSet(uint32_t setCount, vk::Move<VkDescriptorPool> &descriptorPool,
                                    vk::Move<VkDescriptorSetLayout> &descriptorSetLayout,
                                    std::vector<vk::Move<VkDescriptorSet>> &descriptorSets,
                                    vk::Move<VkPipelineLayout> &pipelineLayout);

    de::SharedPtr<TopLevelAccelerationStructure> initTopAccelerationStructure(
        std::vector<de::SharedPtr<BottomLevelAccelerationStructure>> &bottomLevelAccelerationStructures);
    vector<de::SharedPtr<BottomLevelAccelerationStructure>> initBottomAccelerationStructures(void);
    void initializeParameters(void);
    bool verifyResults(void);

private:
    CaseDef m_data;
    std::vector<std::vector<tcu::Vec3>> m_geomData;
    de::MovePtr<BufferWithMemory> m_resultBuffer;
    de::MovePtr<BufferWithMemory> m_rayBuffer;
    uint32_t m_numGeoms;
    uint32_t m_primsPerGeometry;
    uint32_t m_geomsPerInstance;

    de::MovePtr<BufferWithMemory> m_raygenShaderBindingTable;
    de::MovePtr<BufferWithMemory> m_missShaderBindingTable;
    de::MovePtr<BufferWithMemory> m_hitShaderBindingTable;

    VkStridedDeviceAddressRegionKHR m_raygenShaderBindingTableRegion;
    VkStridedDeviceAddressRegionKHR m_missShaderBindingTableRegion;
    VkStridedDeviceAddressRegionKHR m_hitShaderBindingTableRegion;
    VkStridedDeviceAddressRegionKHR m_callableShaderBindingTableRegion;
};

RayTracingIndirectTestInstance::RayTracingIndirectTestInstance(Context &context, const CaseDef &data)
    : vkt::TestInstance(context)
    , m_data(data)
    , m_numGeoms(0)
    , m_primsPerGeometry(0)
    , m_geomsPerInstance(0)
{
}

RayTracingIndirectTestInstance::~RayTracingIndirectTestInstance(void)
{
}

class RayTracingIndirectTestCase : public TestCase
{
public:
    RayTracingIndirectTestCase(tcu::TestContext &context, const char *name, const CaseDef data);
    ~RayTracingIndirectTestCase(void);

    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    static inline const std::string getIntersectionPassthrough(void);
    static inline const std::string getMissPassthrough(void);
    static inline const std::string getHitPassthrough(void);

    CaseDef m_data;
};

RayTracingIndirectTestCase::RayTracingIndirectTestCase(tcu::TestContext &context, const char *name, const CaseDef data)
    : vkt::TestCase(context, name)
    , m_data(data)
{
}

RayTracingIndirectTestCase::~RayTracingIndirectTestCase(void)
{
}

void RayTracingIndirectTestCase::checkSupport(Context &context) const
{
    const bool pipelineFlagSkipTriangles =
        ((m_data.pipelineCreateFlags & VK_PIPELINE_CREATE_RAY_TRACING_SKIP_TRIANGLES_BIT_KHR) != 0);
    const bool pipelineFlagSkipAABSs =
        ((m_data.pipelineCreateFlags & VK_PIPELINE_CREATE_RAY_TRACING_SKIP_AABBS_BIT_KHR) != 0);
    const bool cullingFlags =
        m_data.rayFlagSkipTriangles || m_data.rayFlagSkipAABSs || pipelineFlagSkipTriangles || pipelineFlagSkipAABSs;

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

    if (cullingFlags && rayTracingPipelineFeaturesKHR.rayTraversalPrimitiveCulling == false)
        TCU_THROW(NotSupportedError,
                  "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTraversalPrimitiveCulling");
}

void RayTracingIndirectTestCase::initPrograms(SourceCollections &programCollection) const
{
    if (m_data.id == TEST_ID_INDICES_INDIRECT)
    {
        std::ostringstream raygenSrc;
        raygenSrc << "struct Payload { uvec4 data; };\n"
                  << "layout(location = 0) rayPayloadEXT Payload payload;\n"
                  << "void main() {\n"
                  << "    uint index = launchIndex();\n"
                  << "    payload.data = uvec4(0, 0, 0, 0);\n"
                  << "    Ray ray = rays[index];\n"
                  << "    traceRayEXT(scene, 0, 0xff, 0, 0, 0, ray.pos, ray.tmin, ray.dir, ray.tmax, 0);\n"
                  << "    results[index] = payload.data;\n"
                  << "}";
        const std::string raygen = raygenSrc.str();
        addBuiltInShaderSource<glu::RaygenSource>(programCollection, VK_SHADER_STAGE_RAYGEN_BIT_KHR, raygen, m_data,
                                                  "uvec4");

        std::ostringstream missSrc;
        missSrc << "struct Payload { uvec4 data; };\n"
                << "layout(location = 0) rayPayloadInEXT Payload payload;\n"
                << "void main() {\n"
                << "    payload.data = uvec4(111, 222, 333, 444);\n"
                << "}";
        const std::string miss = missSrc.str();
        addBuiltInShaderSource<glu::MissSource>(programCollection, VK_SHADER_STAGE_MISS_BIT_KHR, miss, m_data, "uvec4");

        std::ostringstream closestHitSrc;
        closestHitSrc << "struct Payload { uvec4 data; };\n"
                      << "layout(location = 0) rayPayloadInEXT Payload payload;\n"
                      << "hitAttributeEXT vec2 attribs;\n"
                      << "void main() {\n"
                      << "    payload.data = uvec4(gl_PrimitiveID, 0, gl_InstanceID, gl_InstanceCustomIndexEXT);\n"
                      << "}";
        const std::string closestHit = closestHitSrc.str();
        addBuiltInShaderSource<glu::ClosestHitSource>(programCollection, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                                                      closestHit, m_data, "uvec4");

        std::ostringstream anyHitSrc;
        anyHitSrc << "struct Payload { uvec4 data; };\n"
                  << "layout(location = 0) rayPayloadInEXT Payload payload;\n"
                  << "hitAttributeEXT vec2 attribs;\n"
                  << "void main() {\n"
                  << "    payload.data = uvec4(gl_PrimitiveID, 0, gl_InstanceID, gl_InstanceCustomIndexEXT);\n"
                  << "}";
        const std::string anyHit = anyHitSrc.str();
        addBuiltInShaderSource<glu::AnyHitSource>(programCollection, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, anyHit, m_data,
                                                  "uvec4");

        std::ostringstream isectSrc;
        isectSrc << "hitAttributeEXT vec2 dummy;\n"
                 << "void main() {\n"
                 << "    reportIntersectionEXT(0.0, 0u);\n"
                 << "}";
        const std::string isect = isectSrc.str();
        addBuiltInShaderSource<glu::IntersectionSource>(programCollection, VK_SHADER_STAGE_INTERSECTION_BIT_KHR, isect,
                                                        m_data, "uvec4");
    }
    else if (m_data.id == TEST_ID_TRANSFORMS_INDIRECT)
    {
        const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0, true);

        std::ostringstream preambleSrc;
        preambleSrc << "struct ResultData { Ray ray; vec4 worldRayOrig; vec4 worldRayDir; vec4 objectRayOrig; vec4 "
                       "objectRayDir;\n"
                    << "vec4 objectToWorld[4]; vec4 worldToObject[4];\n"
                    << "uint missResult; uint closestHitResult; uint anyHitResult; uint isectResult; };\n"
                    << "layout(std430, set = 0, binding = 0) buffer Results { ResultData results; };\n"
                    << "\n"
                    << "#ifdef CHECKS\n"
                    << "bool fuzzy_check(vec3 a, vec3 b) {\n"
                    << "    float eps = 0.00001;\n"
                    << "    return abs(a.x - b.x) <= eps && abs(a.y - b.y) <= eps && abs(a.z - b.z) <= eps;\n"
                    << "}\n"
                    << "bool check_all() {\n"
                    << "    if (fuzzy_check(results.worldRayOrig.xyz, gl_WorldRayOriginEXT) == false)\n"
                    << "        return false;\n"
                    << "    if (fuzzy_check(results.worldRayDir.xyz, gl_WorldRayDirectionEXT) == false)\n"
                    << "        return false;\n"
                    << "#ifndef MISS_SHADER\n"
                    << "    if (fuzzy_check(results.objectRayOrig.xyz, gl_ObjectRayOriginEXT) == false)\n"
                    << "        return false;\n"
                    << "    if (fuzzy_check(results.objectRayDir.xyz, gl_WorldRayDirectionEXT) == false)\n"
                    << "        return false;\n"
                    << "    if (fuzzy_check(results.objectToWorld[0].xyz, gl_ObjectToWorldEXT[0]) == false)\n"
                    << "        return false;\n"
                    << "    if (fuzzy_check(results.objectToWorld[1].xyz, gl_ObjectToWorldEXT[1]) == false)\n"
                    << "        return false;\n"
                    << "    if (fuzzy_check(results.objectToWorld[2].xyz, gl_ObjectToWorldEXT[2]) == false)\n"
                    << "        return false;\n"
                    << "    if (fuzzy_check(results.objectToWorld[3].xyz, gl_ObjectToWorldEXT[3]) == false)\n"
                    << "        return false;\n"
                    << "    if (fuzzy_check(results.worldToObject[0].xyz, gl_WorldToObjectEXT[0]) == false)\n"
                    << "        return false;\n"
                    << "    if (fuzzy_check(results.worldToObject[1].xyz, gl_WorldToObjectEXT[1]) == false)\n"
                    << "        return false;\n"
                    << "    if (fuzzy_check(results.worldToObject[2].xyz, gl_WorldToObjectEXT[2]) == false)\n"
                    << "        return false;\n"
                    << "    if (fuzzy_check(results.worldToObject[3].xyz, gl_WorldToObjectEXT[3]) == false)\n"
                    << "        return false;\n"
                    << "#endif\n"
                    << "    return true;\n"
                    << "};\n"
                    << "#endif\n";
        const std::string preamble = preambleSrc.str();

        std::ostringstream raygenSrc;
        raygenSrc << "$preamble$\n"
                  << "struct Payload { uint x; };\n"
                  << "layout(location = 0) rayPayloadEXT Payload payload;\n"
                  << "void main() {\n"
                  << "    payload.x = 0;\n"
                  << "    results.missResult = 0;\n"
                  << "    results.closestHitResult = 0;\n"
                  << "    results.anyHitResult = 0;\n"
                  << "    results.isectResult = 0;\n"
                  << "    Ray ray = results.ray;\n"
                  << "    traceRayEXT(scene, 0, 0xff, 0, 0, 0, ray.pos, ray.tmin, ray.dir, ray.tmax, 0);\n"
                  << "}";
        std::string raygen = raygenSrc.str();
        raygen             = generateShaderSource(raygen.c_str(), "", DEFINE_RAY | DEFINE_SCENE);
        raygen             = replaceString(raygen, "$preamble$", preamble);
        programCollection.glslSources.add(getShaderIdentifier(m_data, VK_SHADER_STAGE_RAYGEN_BIT_KHR))
            << glu::RaygenSource(raygen) << buildOptions;

        std::ostringstream missSrc;
        missSrc << "#define CHECKS\n"
                << "#define MISS_SHADER\n"
                << "$preamble$\n"
                << "struct Payload { uint x; };\n"
                << "layout(location = 0) rayPayloadInEXT Payload payload;\n"
                << "void main() {\n"
                << "    if (check_all())\n"
                << "        results.missResult = 1;\n"
                << "}";
        std::string miss = missSrc.str();
        miss             = generateShaderSource(miss.c_str(), "", DEFINE_RAY);
        miss             = replaceString(miss, "$preamble$", preamble);
        programCollection.glslSources.add(getShaderIdentifier(m_data, VK_SHADER_STAGE_MISS_BIT_KHR))
            << glu::MissSource(miss) << buildOptions;

        std::ostringstream closestHitSrc;
        closestHitSrc << "#define CHECKS\n"
                      << "$preamble$\n"
                      << "struct Payload { uint x; };\n"
                      << "layout(location = 0) rayPayloadInEXT Payload payload;\n"
                      << "hitAttributeEXT vec2 attribs; "
                      << "void main() {\n"
                      << "    if (check_all())\n"
                      << "        results.closestHitResult = 1;\n"
                      << "}";
        std::string closestHit = closestHitSrc.str();
        closestHit             = generateShaderSource(closestHit.c_str(), "", DEFINE_RAY);
        closestHit             = replaceString(closestHit, "$preamble$", preamble);
        programCollection.glslSources.add(getShaderIdentifier(m_data, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR))
            << glu::ClosestHitSource(closestHit) << buildOptions;

        std::ostringstream anyHitSrc;
        anyHitSrc << "#define CHECKS\n"
                  << "$preamble$\n"
                  << "struct Payload { uint x; };\n"
                  << "layout(location = 0) rayPayloadInEXT Payload payload;\n"
                  << "hitAttributeEXT vec2 attribs; "
                  << "void main() {\n"
                  << "    if (check_all())\n"
                  << "        results.anyHitResult = 1;\n"
                  << "}";
        std::string anyHit = anyHitSrc.str();
        anyHit             = generateShaderSource(anyHit.c_str(), "", DEFINE_RAY);
        anyHit             = replaceString(anyHit, "$preamble$", preamble);
        programCollection.glslSources.add(getShaderIdentifier(m_data, VK_SHADER_STAGE_ANY_HIT_BIT_KHR))
            << glu::AnyHitSource(anyHit) << buildOptions;

        std::ostringstream isectSrc;
        isectSrc << "#define CHECKS\n"
                 << "$preamble$\n"
                 << "hitAttributeEXT vec2 dummy;\n"
                 << "void main() {\n"
                 << "    if (check_all())\n"
                 << "        results.isectResult = 1;\n"
                 << "    reportIntersectionEXT(0.0, 0u);\n"
                 << "}";
        std::string isect = isectSrc.str();
        isect             = generateShaderSource(isect.c_str(), "", DEFINE_RAY);
        isect             = replaceString(isect, "$preamble$", preamble);
        programCollection.glslSources.add(getShaderIdentifier(m_data, VK_SHADER_STAGE_INTERSECTION_BIT_KHR))
            << glu::IntersectionSource(isect) << buildOptions;
    }
    else if (m_data.id == TEST_ID_TMINMAX_INDIRECT)
    {
        const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

        std::ostringstream preambleSrc;
        preambleSrc
            << "struct ResultData { Ray ray; float miss_maxt; float chit_maxt; float ahit_maxt; float isect_maxt;\n"
            << "uint missResult; uint closestHitResult; uint anyHitResult; uint isectResult; float debug; };\n"
            << "layout(std430, set = 0, binding = 0) buffer Results { ResultData results; };\n"
            << "\n"
            << "#ifdef CHECKS\n"
            << "bool fuzzy_check(float a, float b) {\n"
            << "    float eps = 0.0001;\n"
            << "    return abs(a - b) <= eps;\n"
            << "}\n"
            << "bool check_all() {\n"
            << "    if (fuzzy_check(results.ray.tmin, gl_RayTminEXT) == false)\n"
            << "        return false;\n"
            << "#ifdef MISS_SHADER\n"
            << "    if (fuzzy_check(results.miss_maxt, gl_RayTmaxEXT) == false)\n"
            << "        return false;\n"
            << "#endif\n"
            << "#ifdef CHIT_SHADER\n"
            << "    if (fuzzy_check(results.chit_maxt, gl_RayTmaxEXT) == false)\n"
            << "        return false;\n"
            << "#endif\n"
            << "#ifdef AHIT_SHADER\n"
            << "    if (fuzzy_check(results.ahit_maxt, gl_RayTmaxEXT) == false)\n"
            << "        return false;\n"
            << "#endif\n"
            << "#ifdef ISECT_SHADER\n"
            << "    if (fuzzy_check(results.isect_maxt, gl_RayTmaxEXT) == false)\n"
            << "        return false;\n"
            << "#endif\n"
            << "    return true;\n"
            << "};\n"
            << "#endif\n";
        const std::string preamble = preambleSrc.str();

        std::ostringstream raygenSrc;
        raygenSrc << "$preamble$\n"
                  << "struct Payload { uint x; };\n"
                  << "layout(location = 0) rayPayloadEXT Payload payload;\n"
                  << "void main() {\n"
                  << "    payload.x = 0;\n"
                  << "    results.missResult = 0;\n"
                  << "    results.closestHitResult = 0;\n"
                  << "    results.anyHitResult = 0;\n"
                  << "    results.isectResult = 0;\n"
                  << "    Ray ray = results.ray;\n"
                  << "    traceRayEXT(scene, 0, 0xff, 0, 0, 0, ray.pos, ray.tmin, ray.dir, ray.tmax, 0);\n"
                  << "}";
        std::string raygen = raygenSrc.str();
        raygen             = generateShaderSource(raygen.c_str(), "", DEFINE_RAY | DEFINE_SCENE);
        raygen             = replaceString(raygen, "$preamble$", preamble);
        programCollection.glslSources.add(getShaderIdentifier(m_data, VK_SHADER_STAGE_RAYGEN_BIT_KHR))
            << glu::RaygenSource(raygen) << buildOptions;

        std::ostringstream missSrc;
        missSrc << "#define CHECKS\n"
                << "#define MISS_SHADER\n"
                << "$preamble$\n"
                << "struct Payload { uint x; };\n"
                << "layout(location = 0) rayPayloadInEXT Payload payload;\n"
                << "void main() {\n"
                << "    if (check_all())\n"
                << "        results.missResult = 1;\n"
                << "}";
        std::string miss = missSrc.str();
        miss             = generateShaderSource(miss.c_str(), "", DEFINE_RAY);
        miss             = replaceString(miss, "$preamble$", preamble);
        programCollection.glslSources.add(getShaderIdentifier(m_data, VK_SHADER_STAGE_MISS_BIT_KHR))
            << glu::MissSource(miss) << buildOptions;

        std::ostringstream cloesetHitSrc;
        cloesetHitSrc << "#define CHECKS\n"
                      << "#define CHIT_SHADER\n"
                      << "$preamble$\n"
                      << "struct Payload { uint x; };\n"
                      << "layout(location = 0) rayPayloadInEXT Payload payload;\n"
                      << "hitAttributeEXT vec2 attribs; "
                      << "void main() {\n"
                      << "    if (check_all())\n"
                      << "        results.closestHitResult = 1;\n"
                      << "}";
        std::string closestHit = cloesetHitSrc.str();
        closestHit             = generateShaderSource(closestHit.c_str(), "", DEFINE_RAY);
        closestHit             = replaceString(closestHit, "$preamble$", preamble);
        programCollection.glslSources.add(getShaderIdentifier(m_data, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR))
            << glu::ClosestHitSource(closestHit) << buildOptions;

        std::ostringstream anyHitSrc;
        anyHitSrc << "#define CHECKS\n"
                  << "#define AHIT_SHADER\n"
                  << "$preamble$\n"
                  << "struct Payload { uint x; };\n"
                  << "layout(location = 0) rayPayloadInEXT Payload payload;\n"
                  << "hitAttributeEXT vec2 attribs; "
                  << "void main() {\n"
                  << "    if (check_all())\n"
                  << "        results.anyHitResult = 1;\n"
                  << "}";
        std::string anyHit = anyHitSrc.str();
        anyHit             = generateShaderSource(anyHit.c_str(), "", DEFINE_RAY);
        anyHit             = replaceString(anyHit, "$preamble$", preamble);
        programCollection.glslSources.add(getShaderIdentifier(m_data, VK_SHADER_STAGE_ANY_HIT_BIT_KHR))
            << glu::AnyHitSource(anyHit) << buildOptions;

        std::ostringstream isectSrc;
        isectSrc << "#define CHECKS\n"
                 << "#define ISECT_SHADER\n"
                 << "$preamble$\n"
                 << "hitAttributeEXT vec2 dummy;\n"
                 << "void main() {\n"
                 << "    results.debug = gl_RayTmaxEXT;\n"
                 << "    if (check_all())\n"
                 << "        results.isectResult = 1;\n"
                 << "    reportIntersectionEXT(results.chit_maxt, 0u);\n"
                 << "}";
        std::string isect = isectSrc.str();
        isect             = generateShaderSource(isect.c_str(), "", DEFINE_RAY);
        isect             = replaceString(isect, "$preamble$", preamble);
        programCollection.glslSources.add(getShaderIdentifier(m_data, VK_SHADER_STAGE_INTERSECTION_BIT_KHR))
            << glu::IntersectionSource(isect) << buildOptions;
    }
    else if (m_data.id == TEST_ID_INCOMING_RAY_FLAGS_INDIRECT)
    {
        const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0, true);

        std::ostringstream preambleSrc;
        preambleSrc << "struct ResultData { Ray ray[2]; uint flags; uint miss[2]; uint chit[2]; uint ahit[2]; uint "
                       "isect[2]; };\n"
                    << "layout(std430, set = 0, binding = 0) buffer Results { ResultData results; };\n"
                    << "\n"
                    << "#ifdef CHECKS\n"
                    << "bool check_all() {\n"
                    << "    if (gl_IncomingRayFlagsEXT != results.flags)\n"
                    << "        return false;\n"
                    << "    return true;\n"
                    << "}\n"
                    << "#endif\n";
        const std::string preamble = preambleSrc.str();

        std::ostringstream raygenSrc;
        raygenSrc << "$preamble$\n"
                  << "struct Payload { uint x; };\n"
                  << "layout(location = 0) rayPayloadEXT Payload payload;\n"
                  << "void main() {\n"
                  << "    payload.x = 0;\n"
                  << "    results.miss[gl_LaunchIDEXT.x] = 0;\n"
                  << "    results.chit[gl_LaunchIDEXT.x] = 0;\n"
                  << "    results.ahit[gl_LaunchIDEXT.x] = 0;\n"
                  << "    results.isect[gl_LaunchIDEXT.x] = 0;\n"
                  << "    Ray ray = results.ray[gl_LaunchIDEXT.x];\n"
                  << "    traceRayEXT(scene, results.flags, 0xff, 0, 0, 0, ray.pos, ray.tmin, ray.dir, ray.tmax, 0);\n"
                  << "}";
        std::string raygen = raygenSrc.str();
        raygen             = generateShaderSource(raygen.c_str(), "", DEFINE_RAY | DEFINE_SCENE);
        raygen             = replaceString(raygen, "$preamble$", preamble);
        programCollection.glslSources.add(getShaderIdentifier(m_data, VK_SHADER_STAGE_RAYGEN_BIT_KHR))
            << glu::RaygenSource(raygen) << buildOptions;

        std::ostringstream missSrc;
        missSrc << "#define CHECKS\n"
                << "#define MISS_SHADER\n"
                << "$preamble$\n"
                << "struct Payload { uint x; };\n"
                << "layout(location = 0) rayPayloadInEXT Payload payload;\n"
                << "void main() {\n"
                << "    if (check_all())\n"
                << "        results.miss[gl_LaunchIDEXT.x]++;\n"
                << "}";
        std::string miss = missSrc.str();
        miss             = generateShaderSource(miss.c_str(), "", DEFINE_RAY);
        miss             = replaceString(miss, "$preamble$", preamble);
        programCollection.glslSources.add(getShaderIdentifier(m_data, VK_SHADER_STAGE_MISS_BIT_KHR))
            << glu::MissSource(miss) << buildOptions;

        std::ostringstream closestHitSrc;
        closestHitSrc << "#define CHECKS\n"
                      << "#define CHIT_SHADER\n"
                      << "$preamble$\n"
                      << "struct Payload { uint x; };\n"
                      << "layout(location = 0) rayPayloadInEXT Payload payload;\n"
                      << "hitAttributeEXT vec2 attribs; "
                      << "void main() {\n"
                      << "    if (check_all())\n"
                      << "        results.chit[gl_LaunchIDEXT.x]++;\n"
                      << "}";
        std::string closestHit = closestHitSrc.str();
        closestHit             = generateShaderSource(closestHit.c_str(), "", DEFINE_RAY);
        closestHit             = replaceString(closestHit, "$preamble$", preamble);
        programCollection.glslSources.add(getShaderIdentifier(m_data, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR))
            << glu::ClosestHitSource(closestHit) << buildOptions;

        std::ostringstream anyHitSrc;
        anyHitSrc << "#define CHECKS\n"
                  << "#define AHIT_SHADER\n"
                  << "$preamble$\n"
                  << "struct Payload { uint x; };\n"
                  << "layout(location = 0) rayPayloadInEXT Payload payload;\n"
                  << "hitAttributeEXT vec2 attribs; "
                  << "void main() {\n"
                  << "    if (check_all())\n"
                  << "        results.ahit[gl_LaunchIDEXT.x]++;\n"
                  << "}";
        std::string anyHit = anyHitSrc.str();
        anyHit             = generateShaderSource(anyHit.c_str(), "", DEFINE_RAY);
        anyHit             = replaceString(anyHit, "$preamble$", preamble);
        programCollection.glslSources.add(getShaderIdentifier(m_data, VK_SHADER_STAGE_ANY_HIT_BIT_KHR))
            << glu::AnyHitSource(anyHit) << buildOptions;

        std::ostringstream isectSrc;
        isectSrc << "#define CHECKS\n"
                 << "#define ISECT_SHADER\n"
                 << "$preamble$\n"
                 << "hitAttributeEXT vec2 dummy;\n"
                 << "void main() {\n"
                 << "    if (check_all())\n"
                 << "        results.isect[gl_LaunchIDEXT.x]++;\n"
                 << "    reportIntersectionEXT(gl_RayTminEXT, 0u);\n"
                 << "}";
        std::string isect = isectSrc.str();
        isect             = generateShaderSource(isect.c_str(), "", DEFINE_RAY);
        isect             = replaceString(isect, "$preamble$", preamble);
        programCollection.glslSources.add(getShaderIdentifier(m_data, VK_SHADER_STAGE_INTERSECTION_BIT_KHR))
            << glu::IntersectionSource(isect) << buildOptions;
    }
    else if (m_data.id == TEST_ID_HIT_KIND_INDIRECT)
    {
        const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0, true);

        std::ostringstream preambleSrc;
        preambleSrc << "struct ResultData { Ray ray[2]; uint kind[2]; uint chit[2]; uint ahit[2]; uint debug[2]; };\n"
                    << "layout(std430, set = 0, binding = 0) buffer Results { ResultData results; };\n"
                    << "\n"
                    << "#ifdef CHECKS\n"
                    << "bool check_all() {\n"
                    << "#if defined(CHIT_SHADER) || defined(AHIT_SHADER)\n"
                    << "    if (gl_HitKindEXT != results.kind[gl_LaunchIDEXT.x])\n"
                    << "        return false;\n"
                    << "#endif\n"
                    << "    return true;\n"
                    << "}\n"
                    << "#endif\n";
        const std::string preamble = preambleSrc.str();

        std::ostringstream raygenSrc;
        raygenSrc << "$preamble$\n"
                  << "struct Payload { uint x; };\n"
                  << "layout(location = 0) rayPayloadEXT Payload payload;\n"
                  << "void main() {\n"
                  << "    payload.x = 0;\n"
                  << "    uint i = gl_LaunchIDEXT.x;\n"
                  << "    results.chit[i] = 0;\n"
                  << "    results.ahit[i] = 0;\n"
                  << "    Ray ray = results.ray[i];\n"
                  << "    traceRayEXT(scene, 0, 0xff, 0, 0, 0, ray.pos, ray.tmin, ray.dir, ray.tmax, 0);\n"
                  << "}";
        std::string raygen = raygenSrc.str();
        raygen             = generateShaderSource(raygen.c_str(), "", DEFINE_RAY | DEFINE_SCENE);
        raygen             = replaceString(raygen, "$preamble$", preamble);
        programCollection.glslSources.add(getShaderIdentifier(m_data, VK_SHADER_STAGE_RAYGEN_BIT_KHR))
            << glu::RaygenSource(raygen) << buildOptions;

        std::ostringstream missSrc;
        missSrc << "#define CHECKS\n"
                << "#define MISS_SHADER\n"
                << "$preamble$\n"
                << "struct Payload { uint x; };\n"
                << "layout(location = 0) rayPayloadInEXT Payload payload;\n"
                << "void main() {\n"
                << "}";
        std::string miss = missSrc.str();
        miss             = generateShaderSource(miss.c_str(), "", DEFINE_RAY);
        miss             = replaceString(miss, "$preamble$", preamble);
        programCollection.glslSources.add(getShaderIdentifier(m_data, VK_SHADER_STAGE_MISS_BIT_KHR))
            << glu::MissSource(miss) << buildOptions;

        std::ostringstream closestHitSrc;
        closestHitSrc << "#define CHECKS\n"
                      << "#define CHIT_SHADER\n"
                      << "$preamble$\n"
                      << "struct Payload { uint x; };\n"
                      << "layout(location = 0) rayPayloadInEXT Payload payload;\n"
                      << "hitAttributeEXT vec2 attribs; "
                      << "void main() {\n"
                      << "    results.debug[gl_LaunchIDEXT.x] = gl_HitKindEXT;\n"
                      << "    if (check_all())\n"
                      << "        results.chit[gl_LaunchIDEXT.x] = 1;\n"
                      << "}";
        std::string closestHit = closestHitSrc.str();
        closestHit             = generateShaderSource(closestHit.c_str(), "", DEFINE_RAY);
        closestHit             = replaceString(closestHit, "$preamble$", preamble);
        programCollection.glslSources.add(getShaderIdentifier(m_data, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR))
            << glu::ClosestHitSource(closestHit) << buildOptions;

        std::ostringstream anyHitSrc;
        anyHitSrc << "#define CHECKS\n"
                  << "#define AHIT_SHADER\n"
                  << "$preamble$\n"
                  << "struct Payload { uint x; };\n"
                  << "layout(location = 0) rayPayloadInEXT Payload payload;\n"
                  << "hitAttributeEXT vec2 attribs; "
                  << "void main() {\n"
                  << "    if (check_all())\n"
                  << "        results.ahit[gl_LaunchIDEXT.x] = 1;\n"
                  << "}";
        std::string anyHit = anyHitSrc.str();
        anyHit             = generateShaderSource(anyHit.c_str(), "", DEFINE_RAY);
        anyHit             = replaceString(anyHit, "$preamble$", preamble);
        programCollection.glslSources.add(getShaderIdentifier(m_data, VK_SHADER_STAGE_ANY_HIT_BIT_KHR))
            << glu::AnyHitSource(anyHit) << buildOptions;

        std::ostringstream isectSrc;
        isectSrc << "#define CHECKS\n"
                 << "#define ISECT_SHADER\n"
                 << "$preamble$\n"
                 << "hitAttributeEXT vec2 dummy;\n"
                 << "void main() {\n"
                 << "    reportIntersectionEXT(gl_RayTminEXT, results.kind[gl_LaunchIDEXT.x]);\n"
                 << "}";
        std::string isect = isectSrc.str();
        isect             = generateShaderSource(isect.c_str(), "", DEFINE_RAY);
        isect             = replaceString(isect, "$preamble$", preamble);
        programCollection.glslSources.add(getShaderIdentifier(m_data, VK_SHADER_STAGE_INTERSECTION_BIT_KHR))
            << glu::IntersectionSource(isect) << buildOptions;
    }
}

TestInstance *RayTracingIndirectTestCase::createInstance(Context &context) const
{
    return new RayTracingIndirectTestInstance(context, m_data);
}

Move<VkPipeline> RayTracingIndirectTestInstance::createPipelineAndShaderBindingTables(
    de::MovePtr<RayTracingPipeline> &rayTracingPipeline, bool aabb, VkPipelineLayout pipelineLayout)
{
    const InstanceInterface &vki          = m_context.getInstanceInterface();
    const DeviceInterface &vk             = m_context.getDeviceInterface();
    const VkDevice device                 = m_context.getDevice();
    const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    Allocator &allocator                  = m_context.getDefaultAllocator();

    const uint32_t shaderGroupHandleSize    = getShaderGroupSize(vki, physicalDevice);
    const uint32_t shaderGroupBaseAlignment = getShaderGroupBaseAlignment(vki, physicalDevice);

    rayTracingPipeline->addShader(
        VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        createShaderModule(
            vk, device,
            m_context.getBinaryCollection().get(getShaderIdentifier(m_data, VK_SHADER_STAGE_RAYGEN_BIT_KHR)), 0),
        0);
    rayTracingPipeline->addShader(
        VK_SHADER_STAGE_MISS_BIT_KHR,
        createShaderModule(
            vk, device, m_context.getBinaryCollection().get(getShaderIdentifier(m_data, VK_SHADER_STAGE_MISS_BIT_KHR)),
            0),
        1);
    for (uint32_t g = 0; g < m_numGeoms; ++g)
    {
        rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                                      createShaderModule(vk, device,
                                                         m_context.getBinaryCollection().get(getShaderIdentifier(
                                                             m_data, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)),
                                                         0),
                                      2 + g);
        rayTracingPipeline->addShader(
            VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
            createShaderModule(
                vk, device,
                m_context.getBinaryCollection().get(getShaderIdentifier(m_data, VK_SHADER_STAGE_ANY_HIT_BIT_KHR)), 0),
            2 + g);

        if (aabb)
        {
            rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
                                          createShaderModule(vk, device,
                                                             m_context.getBinaryCollection().get(getShaderIdentifier(
                                                                 m_data, VK_SHADER_STAGE_INTERSECTION_BIT_KHR)),
                                                             0),
                                          2 + g);
        }
    }
    Move<VkPipeline> pipeline = rayTracingPipeline->createPipeline(vk, device, pipelineLayout);

    m_raygenShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
        vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
    m_missShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
        vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
    m_hitShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
        vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, m_numGeoms);

    m_raygenShaderBindingTableRegion =
        makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, m_raygenShaderBindingTable->get(), 0),
                                          shaderGroupHandleSize, shaderGroupHandleSize);
    m_missShaderBindingTableRegion =
        makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, m_missShaderBindingTable->get(), 0),
                                          shaderGroupHandleSize, shaderGroupHandleSize);
    m_hitShaderBindingTableRegion =
        makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, m_hitShaderBindingTable->get(), 0),
                                          shaderGroupHandleSize, shaderGroupHandleSize);
    m_callableShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(0, 0, 0);

    return pipeline;
}

void RayTracingIndirectTestInstance::createPipelineLayoutAndSet(uint32_t setCount,
                                                                vk::Move<VkDescriptorPool> &descriptorPool,
                                                                vk::Move<VkDescriptorSetLayout> &descriptorSetLayout,
                                                                std::vector<vk::Move<VkDescriptorSet>> &descriptorSets,
                                                                vk::Move<VkPipelineLayout> &pipelineLayout)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    vk::DescriptorPoolBuilder descriptorPoolBuilder;

    uint32_t storageBufCount = 2 * setCount;

    const VkDescriptorType accelType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    descriptorPoolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, storageBufCount);
    descriptorPoolBuilder.addType(accelType, setCount);

    descriptorPool =
        descriptorPoolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, setCount);

    vk::DescriptorSetLayoutBuilder setLayoutBuilder;

    setLayoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ALL_RAY_TRACING_STAGES);
    setLayoutBuilder.addSingleBinding(accelType, ALL_RAY_TRACING_STAGES);
    setLayoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ALL_RAY_TRACING_STAGES);

    descriptorSetLayout = setLayoutBuilder.build(vk, device);

    const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType sType;
        DE_NULL,                                        // const void* pNext;
        *descriptorPool,                                // VkDescriptorPool descriptorPool;
        1u,                                             // uint32_t setLayoutCount;
        &descriptorSetLayout.get()                      // const VkDescriptorSetLayout* pSetLayouts;
    };

    for (uint32_t i = 0; i < setCount; ++i)
        descriptorSets.push_back(allocateDescriptorSet(vk, device, &descriptorSetAllocateInfo));

    const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                       // const void* pNext;
        (VkPipelineLayoutCreateFlags)0,                // VkPipelineLayoutCreateFlags flags;
        1u,                                            // uint32_t setLayoutCount;
        &descriptorSetLayout.get(),                    // const VkDescriptorSetLayout* pSetLayouts;
        0u,                                            // uint32_t pushConstantRangeCount;
        nullptr,                                       // const VkPushConstantRange* pPushConstantRanges;
    };

    pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutInfo);
}

de::SharedPtr<TopLevelAccelerationStructure> RayTracingIndirectTestInstance::initTopAccelerationStructure(
    std::vector<de::SharedPtr<BottomLevelAccelerationStructure>> &blas)
{
    de::SharedPtr<TopLevelAccelerationStructure> tlas =
        de::SharedPtr<TopLevelAccelerationStructure>(makeTopLevelAccelerationStructure().release());

    VkTransformMatrixKHR transform = identityMatrix3x4;
    if (m_data.id == TEST_ID_TRANSFORMS_INDIRECT)
    {
        tcu::Vec3 instanceOffset = tcu::Vec3(2.0f, 4.0f, 8.0f);
        transform.matrix[0][3]   = instanceOffset[0];
        transform.matrix[1][3]   = instanceOffset[1];
        transform.matrix[2][3]   = instanceOffset[2];
    }

    for (size_t i = 0; i < blas.size(); ++i)
    {
        tlas->addInstance(blas[i], transform, 1000 + static_cast<uint32_t>(i));
    }

    return tlas;
}

std::vector<de::SharedPtr<BottomLevelAccelerationStructure>> RayTracingIndirectTestInstance::
    initBottomAccelerationStructures()
{
    const bool aabb = (m_data.stage == VK_SHADER_STAGE_INTERSECTION_BIT_KHR);

    uint32_t numInstances = m_data.id == TEST_ID_INDICES_INDIRECT ? 5 : 1;

    if (m_data.id == TEST_ID_INDICES_INDIRECT || m_data.id == TEST_ID_TRANSFORMS_INDIRECT ||
        m_data.id == TEST_ID_TMINMAX_INDIRECT)
    {
        m_geomsPerInstance = m_data.id == TEST_ID_INDICES_INDIRECT ? 3 : 1;
        m_primsPerGeometry = m_data.id == TEST_ID_INDICES_INDIRECT ? 4 : 1;

        m_numGeoms = numInstances * m_geomsPerInstance;

        for (uint32_t i = 0; i < m_numGeoms; ++i)
        {
            std::vector<tcu::Vec3> geomPrims;

            for (uint32_t j = 0; j < m_primsPerGeometry; ++j)
            {
                const uint32_t primId = (i * m_primsPerGeometry) + j;
                float dx              = 10.0f * static_cast<float>(primId);
                if (aabb == false)
                {
                    geomPrims.push_back(tcu::Vec3(dx + -1.0f, -1.0f, 1.0f));
                    geomPrims.push_back(tcu::Vec3(dx + 1.0f, -1.0f, 1.0f));
                    geomPrims.push_back(tcu::Vec3(dx + 0.0f, 1.0f, 1.0f));
                }
                else
                {
                    geomPrims.push_back(tcu::Vec3(dx - 1.0f, -1.0f, 1.0f)); // min x/y/z
                    geomPrims.push_back(tcu::Vec3(dx + 1.0f, 1.0f, 2.0f));  // max x/y/z
                }
            }
            m_geomData.push_back(geomPrims);
        }
    }
    else if (m_data.id == TEST_ID_INCOMING_RAY_FLAGS_INDIRECT || m_data.id == TEST_ID_HIT_KIND_INDIRECT)
    {
        m_geomsPerInstance = m_data.id == TEST_ID_INCOMING_RAY_FLAGS_INDIRECT ? 2 : 1;
        m_primsPerGeometry = 1;

        m_numGeoms = numInstances * m_geomsPerInstance;

        for (uint32_t i = 0; i < m_numGeoms; ++i)
        {
            std::vector<tcu::Vec3> geomPrims;

            for (uint32_t j = 0; j < m_primsPerGeometry; ++j)
            {
                const uint32_t primId = (i * m_primsPerGeometry) + j;
                float z               = 1.0f + 10.0f * static_cast<float>(primId);

                bool ccw = (primId % 2) == 0;

                if (aabb == false)
                {
                    if (ccw)
                    {
                        geomPrims.push_back(tcu::Vec3(-1.0f, -1.0f, z));
                        geomPrims.push_back(tcu::Vec3(1.0f, -1.0f, z));
                        geomPrims.push_back(tcu::Vec3(0.0f, 1.0f, z));
                    }
                    else
                    {
                        geomPrims.push_back(tcu::Vec3(1.0f, -1.0f, z));
                        geomPrims.push_back(tcu::Vec3(-1.0f, -1.0f, z));
                        geomPrims.push_back(tcu::Vec3(0.0f, 1.0f, z));
                    }
                }
                else
                {
                    geomPrims.push_back(tcu::Vec3(-1.0f, -1.0f, z));      // min x/y/z
                    geomPrims.push_back(tcu::Vec3(1.0f, 1.0f, z + 1.0f)); // max x/y/z
                }
            }
            m_geomData.push_back(geomPrims);
        }
    }

    std::vector<de::SharedPtr<BottomLevelAccelerationStructure>> blas;

    if (!m_geomData.empty())
    {
        for (uint32_t i = 0; i < numInstances; ++i)
        {
            de::SharedPtr<BottomLevelAccelerationStructure> accel =
                de::SharedPtr<BottomLevelAccelerationStructure>(makeBottomLevelAccelerationStructure().release());
            for (uint32_t j = 0; j < m_geomsPerInstance; ++j)
            {
                accel->addGeometry(m_geomData[(i * m_geomsPerInstance) + j], !aabb, 0U);
            }
            blas.push_back(accel);
        }
    }
    return blas;
}

struct Ray
{
    Ray() : o(0.0f), tmin(0.0f), d(0.0f), tmax(0.0f)
    {
    }
    Ray(const tcu::Vec3 &io, float imin, const tcu::Vec3 &id, float imax) : o(io), tmin(imin), d(id), tmax(imax)
    {
    }
    tcu::Vec3 o;
    float tmin;
    tcu::Vec3 d;
    float tmax;
};

struct TransformResultData
{

    TransformResultData()
        : ray()
        , worldRayOrig(0.f)
        , padding0(0.f)
        , worldRayDir(0.f)
        , padding1(0.f)
        , objectRayOrig(0.f)
        , padding2(0.f)
        , objectRayDir(0.f)
        , padding3(0.f)
        , objectToWorld0(0.f)
        , padding4(0.f)
        , objectToWorld1(0.f)
        , padding5(0.f)
        , objectToWorld2(0.f)
        , padding6(0.f)
        , objectToWorld3(0.f)
        , padding7(0.f)
        , worldToObject0(0.f)
        , padding8(0.f)
        , worldToObject1(0.f)
        , padding9(0.f)
        , worldToObject2(0.f)
        , padding10(0.f)
        , worldToObject3(0.f)
        , padding11(0.f)
        , missResult(0)
        , closestHitResult(0)
        , anyHitResult(0)
        , isectResult(0)
    {
    }

    Ray ray;
    tcu::Vec3 worldRayOrig;
    float padding0;
    tcu::Vec3 worldRayDir;
    float padding1;
    tcu::Vec3 objectRayOrig;
    float padding2;
    tcu::Vec3 objectRayDir;
    float padding3;
    tcu::Vec3 objectToWorld0;
    float padding4;
    tcu::Vec3 objectToWorld1;
    float padding5;
    tcu::Vec3 objectToWorld2;
    float padding6;
    tcu::Vec3 objectToWorld3;
    float padding7;
    tcu::Vec3 worldToObject0;
    float padding8;
    tcu::Vec3 worldToObject1;
    float padding9;
    tcu::Vec3 worldToObject2;
    float padding10;
    tcu::Vec3 worldToObject3;
    float padding11;
    uint32_t missResult;
    uint32_t closestHitResult;
    uint32_t anyHitResult;
    uint32_t isectResult;
};

struct TMinMaxResultData
{
    Ray ray;
    float miss_maxt;
    float chit_maxt;
    float ahit_maxt;
    float isect_maxt;
    uint32_t missResult;
    uint32_t closestHitResult;
    uint32_t anyHitResult;
    uint32_t isectResult;
    float debug;
};

struct IncomingFlagsResultData
{
    Ray ray[2];
    uint32_t flags;
    uint32_t miss[2];
    uint32_t chit[2];
    uint32_t ahit[2];
    uint32_t isect[2];
};

struct HitKindResultData
{
    Ray ray[2];
    uint32_t kind[2];
    uint32_t chit[2];
    uint32_t ahit[2];
    uint32_t debug[2];
};

void RayTracingIndirectTestInstance::initializeParameters()
{
    const VkDevice device     = m_context.getDevice();
    const DeviceInterface &vk = m_context.getDeviceInterface();
    Allocator &allocator      = m_context.getDefaultAllocator();

    const bool aabb = (m_data.stage == VK_SHADER_STAGE_INTERSECTION_BIT_KHR);

    if (m_data.id == TEST_ID_INDICES_INDIRECT)
    {
        std::vector<Ray> rays;
        for (uint32_t g = 0; g < m_numGeoms; ++g)
        {
            for (uint32_t p = 0; p < m_primsPerGeometry; ++p)
            {
                tcu::Vec3 center =
                    aabb ? (m_geomData[g][2 * p + 0] + m_geomData[g][2 * p + 1]) * 0.5f :
                           (m_geomData[g][3 * p + 0] + m_geomData[g][3 * p + 1] + m_geomData[g][3 * p + 2]) *
                               (1.0f / 3.0f);

                Ray r;

                r.o    = tcu::Vec3(center[0], center[1], 0.0f);
                r.d    = tcu::Vec3(0.0f, 0.0f, 1.0f);
                r.tmin = 0.0f;
                r.tmax = 1000.0f;

                rays.push_back(r);
            }
        }
        const VkBufferCreateInfo resultBufferCreateInfo =
            makeBufferCreateInfo(rays.size() * sizeof(uint32_t) * 8, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        m_resultBuffer = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vk, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));

        const VkBufferCreateInfo rayBufferCreateInfo =
            makeBufferCreateInfo(rays.size() * sizeof(Ray), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        m_rayBuffer = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vk, device, allocator, rayBufferCreateInfo, MemoryRequirement::HostVisible));

        memcpy(m_rayBuffer->getAllocation().getHostPtr(), &rays[0], rays.size() * sizeof(Ray));
        flushMappedMemoryRange(vk, device, m_rayBuffer->getAllocation().getMemory(),
                               m_rayBuffer->getAllocation().getOffset(), VK_WHOLE_SIZE);
    }
    else if (m_data.id == TEST_ID_TRANSFORMS_INDIRECT)
    {
        const VkBufferCreateInfo resultBufferCreateInfo =
            makeBufferCreateInfo(sizeof(TransformResultData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        m_resultBuffer = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vk, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));

        TransformResultData *resultData = (TransformResultData *)m_resultBuffer->getAllocation().getHostPtr();
        *resultData                     = {};

        VkTransformMatrixKHR transform = identityMatrix3x4;
        tcu::Vec3 instanceOffset       = tcu::Vec3(2.0f, 4.0f, 8.0f);
        transform.matrix[0][3]         = instanceOffset[0];
        transform.matrix[1][3]         = instanceOffset[1];
        transform.matrix[2][3]         = instanceOffset[2];

        {
            tcu::Vec3 center = aabb ? (m_geomData[0][0] + m_geomData[0][1]) * 0.5f :
                                      (m_geomData[0][0] + m_geomData[0][1] + m_geomData[0][2]) * (1.0f / 3.0f);

            center = center + instanceOffset;

            Ray r;

            r.o    = tcu::Vec3(center[0], center[1], 0.0f);
            r.d    = tcu::Vec3(0.0f, 0.0f, 1.0f);
            r.tmin = 0.0f;
            r.tmax = 1000.0f;

            if (m_data.stage == VK_SHADER_STAGE_MISS_BIT_KHR)
            {
                r.d[2] = -1.0f;
            }

            resultData->ray = r;
        }

        resultData->worldRayOrig  = resultData->ray.o;
        resultData->worldRayDir   = resultData->ray.d;
        resultData->objectRayOrig = resultData->worldRayOrig - instanceOffset;
        resultData->objectRayDir  = resultData->worldRayDir;

        resultData->objectToWorld0 = tcu::Vec3(transform.matrix[0][0], transform.matrix[1][0], transform.matrix[2][0]);
        resultData->objectToWorld1 = tcu::Vec3(transform.matrix[0][1], transform.matrix[1][1], transform.matrix[2][1]);
        resultData->objectToWorld2 = tcu::Vec3(transform.matrix[0][2], transform.matrix[1][2], transform.matrix[2][2]);
        resultData->objectToWorld3 = tcu::Vec3(transform.matrix[0][3], transform.matrix[1][3], transform.matrix[2][3]);

        resultData->worldToObject0 = tcu::Vec3(transform.matrix[0][0], transform.matrix[0][1], transform.matrix[0][2]);
        resultData->worldToObject1 = tcu::Vec3(transform.matrix[1][0], transform.matrix[1][1], transform.matrix[1][2]);
        resultData->worldToObject2 = tcu::Vec3(transform.matrix[2][0], transform.matrix[2][1], transform.matrix[2][2]);
        resultData->worldToObject3 =
            tcu::Vec3(-transform.matrix[0][3], -transform.matrix[1][3], -transform.matrix[2][3]);
    }
    else if (m_data.id == TEST_ID_TMINMAX_INDIRECT)
    {
        const VkBufferCreateInfo resultBufferCreateInfo = makeBufferCreateInfo(
            sizeof(TMinMaxResultData) * m_numGeoms * m_primsPerGeometry, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        m_resultBuffer = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vk, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));

        TMinMaxResultData *resultData = (TMinMaxResultData *)m_resultBuffer->getAllocation().getHostPtr();
        *resultData                   = {};
        {
            tcu::Vec3 center = aabb ? (m_geomData[0][0] + m_geomData[0][1]) * 0.5f :
                                      (m_geomData[0][0] + m_geomData[0][1] + m_geomData[0][2]) * (1.0f / 3.0f);

            Ray r;

            r.o    = tcu::Vec3(center[0], center[1], 0.0f);
            r.d    = tcu::Vec3(0.0f, 0.0f, 1.0f);
            r.tmin = 0.05f;
            r.tmax = 1000.0f;

            if (m_data.stage == VK_SHADER_STAGE_MISS_BIT_KHR)
            {
                r.d[2] = -1.0f;
            }

            resultData->ray = r;
        }

        resultData->miss_maxt  = resultData->ray.tmax;
        resultData->isect_maxt = resultData->ray.tmax;
        resultData->chit_maxt  = aabb ? 1.5f : 1.0f;
        resultData->ahit_maxt  = aabb ? 1.5f : 1.0f;
        resultData->isect_maxt = resultData->ray.tmax;
        resultData->debug      = -2.0f;
    }
    else if (m_data.id == TEST_ID_INCOMING_RAY_FLAGS_INDIRECT)
    {
        const VkBufferCreateInfo resultBufferCreateInfo =
            makeBufferCreateInfo(sizeof(IncomingFlagsResultData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        m_resultBuffer = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vk, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));

        IncomingFlagsResultData *resultData = (IncomingFlagsResultData *)m_resultBuffer->getAllocation().getHostPtr();
        *resultData                         = {};

        for (uint32_t i = 0; i < 2; ++i)
        {
            tcu::Vec3 center = aabb ? (m_geomData[0][0] + m_geomData[0][1]) * 0.5f :
                                      (m_geomData[0][0] + m_geomData[0][1] + m_geomData[0][2]) * (1.0f / 3.0f);

            Ray r;

            r.o    = tcu::Vec3(center[0], center[1], 0.0f);
            r.d    = tcu::Vec3(0.0f, 0.0f, (i == 0) ? -1.0f : 1.0f);
            r.tmin = 0.05f;
            r.tmax = 1000.0f;

            resultData->ray[i] = r;
        }

        if (m_data.opaque)
            resultData->flags |= (1 << RAY_FLAG_BIT_OPAQUE_EXT);
        if (m_data.skipClosestHit)
            resultData->flags |= (1 << RAY_FLAG_BIT_SKIP_CLOSEST_HIT_SHADER_EXT);
    }
    else if (m_data.id == TEST_ID_HIT_KIND_INDIRECT)
    {
        const VkBufferCreateInfo resultBufferCreateInfo =
            makeBufferCreateInfo(sizeof(HitKindResultData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        m_resultBuffer = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vk, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));

        HitKindResultData *resultData = (HitKindResultData *)m_resultBuffer->getAllocation().getHostPtr();
        *resultData                   = {};

        for (uint32_t i = 0; i < 2; ++i)
        {
            tcu::Vec3 center = aabb ? (m_geomData[0][0] + m_geomData[0][1]) * 0.5f :
                                      (m_geomData[0][0] + m_geomData[0][1] + m_geomData[0][2]) * (1.0f / 3.0f);

            Ray r;

            r.o = tcu::Vec3(center[0], center[1], 0.0f);
            r.d = tcu::Vec3(0.0f, 0.0f, (i == 1) ? -1.0f : 1.0f);

            if (i == 1)
                r.o += tcu::Vec3(0, 0, 100.0f);

            r.tmin = 0.05f;
            r.tmax = 1000.0f;

            resultData->ray[i] = r;
        }

        resultData->kind[0] = 255;
        resultData->kind[1] = 254;
    }
}

bool RayTracingIndirectTestInstance::verifyResults()
{
    const bool aabb = (m_data.stage == VK_SHADER_STAGE_INTERSECTION_BIT_KHR);

    if (m_data.id == TEST_ID_INDICES_INDIRECT)
    {
        uint32_t *resultData = (uint32_t *)m_resultBuffer->getAllocation().getHostPtr();
        for (uint32_t r = 0; r < m_numGeoms * m_primsPerGeometry; ++r)
        {
            uint32_t primitiveId         = r % m_primsPerGeometry;
            uint32_t instanceId          = (r / m_primsPerGeometry) / m_geomsPerInstance;
            uint32_t instanceCustomIndex = 1000 + instanceId;

            if (resultData[4 * r + 0] != primitiveId)
                return false;

            if (resultData[4 * r + 2] != instanceId)
                return false;

            if (resultData[4 * r + 3] != instanceCustomIndex)
                return false;
        }
    }
    else if (m_data.id == TEST_ID_TRANSFORMS_INDIRECT)
    {
        TransformResultData *resultData = (TransformResultData *)m_resultBuffer->getAllocation().getHostPtr();
        if (resultData->anyHitResult != 1 || resultData->closestHitResult != 1)
        {
            return false;
        }

        if (aabb && resultData->isectResult != 1)
        {
            return false;
        }
    }
    else if (m_data.id == TEST_ID_TMINMAX_INDIRECT)
    {
        TMinMaxResultData *resultData = (TMinMaxResultData *)m_resultBuffer->getAllocation().getHostPtr();
        if (resultData->anyHitResult != 1 || resultData->closestHitResult != 1)
        {
            return false;
        }

        if (aabb && resultData->isectResult != 1)
        {
            return false;
        }
    }
    else if (m_data.id == TEST_ID_INCOMING_RAY_FLAGS_INDIRECT)
    {
        IncomingFlagsResultData *resultData = (IncomingFlagsResultData *)m_resultBuffer->getAllocation().getHostPtr();

        uint32_t miss[2]     = {1, 0};
        uint32_t chitMin[2]  = {0, 1};
        uint32_t chitMax[2]  = {0, 1};
        uint32_t ahitMin[2]  = {0, 1};
        uint32_t ahitMax[2]  = {0, 2};
        uint32_t isectMin[2] = {0, 0};
        uint32_t isectMax[2] = {0, 0};

        if (aabb)
        {
            isectMin[1] = 1;
            isectMax[1] = 2;
        }

        if (m_data.opaque)
        {
            ahitMin[1] = 0;
            ahitMax[1] = 0;
        }

        if (m_data.skipClosestHit)
        {
            chitMin[1] = 0;
            chitMax[1] = 0;
        }

        for (uint32_t i = 0; i < 2; ++i)
        {
            if (resultData->miss[i] != miss[i])
                return false;
            if (resultData->chit[i] < chitMin[i])
                return false;
            if (resultData->chit[i] > chitMax[i])
                return false;
            if (resultData->ahit[i] < ahitMin[i])
                return false;
            if (resultData->ahit[i] > ahitMax[i])
                return false;
            if (resultData->isect[i] < isectMin[i])
                return false;
            if (resultData->isect[i] > isectMax[i])
                return false;
        }
    }
    else if (m_data.id == TEST_ID_HIT_KIND_INDIRECT)
    {
        HitKindResultData *resultData = (HitKindResultData *)m_resultBuffer->getAllocation().getHostPtr();
        for (uint32_t i = 0; i < 2; ++i)
        {
            if (resultData->chit[i] != 1)
                return false;

            if (resultData->ahit[i] != 1)
                return false;
        }
    }
    else
    {
        return false;
    }
    return true;
}

tcu::TestStatus RayTracingIndirectTestInstance::iterate(void)
{
    const VkDevice device           = m_context.getDevice();
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    const bool aabb = (m_data.stage == VK_SHADER_STAGE_INTERSECTION_BIT_KHR);

    vk::Move<VkDescriptorPool> descriptorPool;
    vk::Move<VkDescriptorSetLayout> descriptorSetLayout;
    std::vector<vk::Move<VkDescriptorSet>> descriptorSet;
    vk::Move<VkPipelineLayout> pipelineLayout;

    createPipelineLayoutAndSet(1u, descriptorPool, descriptorSetLayout, descriptorSet, pipelineLayout);

    std::vector<de::SharedPtr<BottomLevelAccelerationStructure>> blas = initBottomAccelerationStructures();

    de::SharedPtr<TopLevelAccelerationStructure> tlas = initTopAccelerationStructure(blas);

    de::MovePtr<RayTracingPipeline> rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();
    Move<VkPipeline> pipeline = createPipelineAndShaderBindingTables(rayTracingPipeline, aabb, *pipelineLayout);

    initializeParameters();

    const VkDescriptorBufferInfo resultDescriptorInfo =
        makeDescriptorBufferInfo(m_resultBuffer->get(), 0, VK_WHOLE_SIZE);

    const Move<VkCommandPool> cmdPool = createCommandPool(vk, device, 0, queueFamilyIndex);
    const Move<VkCommandBuffer> cmdBuffer =
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vk, *cmdBuffer);

    for (const auto &accel : blas)
    {
        accel->createAndBuild(vk, device, *cmdBuffer, allocator);
    }
    tlas->createAndBuild(vk, device, *cmdBuffer, allocator);

    VkWriteDescriptorSetAccelerationStructureKHR accelerationStructureWriteDescriptorSet = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR, //  VkStructureType sType;
        DE_NULL,                                                           //  const void* pNext;
        1u,                                                                //  uint32_t accelerationStructureCount;
        tlas->getPtr(), //  const VkAccelerationStructureKHR* pAccelerationStructures;
    };

    if (m_rayBuffer)
    {
        const VkDescriptorBufferInfo rayDescriptorInfo = makeDescriptorBufferInfo(m_rayBuffer->get(), 0, VK_WHOLE_SIZE);

        DescriptorSetUpdateBuilder()
            .writeSingle(*descriptorSet[0], DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultDescriptorInfo)
            .writeSingle(*descriptorSet[0], DescriptorSetUpdateBuilder::Location::binding(1u),
                         VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelerationStructureWriteDescriptorSet)
            .writeSingle(*descriptorSet[0], DescriptorSetUpdateBuilder::Location::binding(2u),
                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &rayDescriptorInfo)
            .update(vk, device);
    }
    else
    {
        DescriptorSetUpdateBuilder()
            .writeSingle(*descriptorSet[0], DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultDescriptorInfo)
            .writeSingle(*descriptorSet[0], DescriptorSetUpdateBuilder::Location::binding(1u),
                         VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelerationStructureWriteDescriptorSet)
            .update(vk, device);
    }

    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipelineLayout, 0, 1,
                             &descriptorSet[0].get(), 0, DE_NULL);

    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipeline);

    uint32_t rayCount = m_numGeoms * m_primsPerGeometry;
    if (m_data.id == TEST_ID_HIT_KIND_INDIRECT)
    {
        rayCount *= 2;
    }

    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo = {rayCount, 1, 1, 0};

    VkBufferCreateInfo indirectBufferCreateInfo =
        makeBufferCreateInfo(sizeof(VkAccelerationStructureBuildRangeInfoKHR),
                             VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    de::MovePtr<BufferWithMemory> indirectBuffer = de::MovePtr<BufferWithMemory>(
        new BufferWithMemory(vk, device, allocator, indirectBufferCreateInfo,
                             MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress));

    deMemcpy(indirectBuffer->getAllocation().getHostPtr(), (void *)&buildRangeInfo,
             sizeof(VkAccelerationStructureBuildRangeInfoKHR));
    invalidateMappedMemoryRange(vk, device, indirectBuffer->getAllocation().getMemory(),
                                indirectBuffer->getAllocation().getOffset(), VK_WHOLE_SIZE);

    cmdTraceRaysIndirect(vk, *cmdBuffer, &m_raygenShaderBindingTableRegion, &m_missShaderBindingTableRegion,
                         &m_hitShaderBindingTableRegion, &m_callableShaderBindingTableRegion,
                         getBufferDeviceAddress(vk, device, indirectBuffer->get(), 0));

    endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *cmdBuffer);

    invalidateMappedMemoryRange(vk, device, m_resultBuffer->getAllocation().getMemory(),
                                m_resultBuffer->getAllocation().getOffset(), VK_WHOLE_SIZE);

    return verifyResults() ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail("Invalid results");
}

static const struct Stages
{
    const char *name;
    VkShaderStageFlagBits stage;
} stages[]{
    {"rgen", VK_SHADER_STAGE_RAYGEN_BIT_KHR},      {"ahit", VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
    {"chit", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR}, {"sect", VK_SHADER_STAGE_INTERSECTION_BIT_KHR},
    {"miss", VK_SHADER_STAGE_MISS_BIT_KHR},        {"call", VK_SHADER_STAGE_CALLABLE_BIT_KHR},
};

static const struct GeomTypes
{
    const char *name;
    GeomType geomType;
} geomTypes[] = {
    {"triangles", GEOM_TYPE_TRIANGLES},
    {"aabs", GEOM_TYPE_AABBS},
};

void createLaunchTests(tcu::TestContext &testCtx, tcu::TestCaseGroup *builtinGroup, TestId id, const char *name,
                       const VkShaderStageFlags shaderStageFlags)
{
    const struct
    {
        uint32_t width;
        uint32_t height;
        uint32_t depth;
    } sizes[] = {
        {1, 1, 1},     {16, 16, 16},    {256, 256, 1},   {16384, 1, 1},  {1, 16384, 1},
        {1, 1, 16384}, {128, 128, 128}, {2048, 4096, 1}, {317, 3331, 1}, {1, 1331, 111},
    };

    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, de::toLower(name).c_str()));

    for (size_t stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stages); ++stageNdx)
    {
        if ((shaderStageFlags & stages[stageNdx].stage) == 0)
            continue;

        for (size_t sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(sizes); ++sizeNdx)
        {
            const uint32_t width                = sizes[sizeNdx].width;
            const uint32_t height               = sizes[sizeNdx].height;
            const uint32_t depth                = sizes[sizeNdx].depth;
            const bool plain                    = isPlain(width, height, depth);
            const uint32_t k                    = (plain ? 1 : 6);
            const uint32_t largestGroup         = k * width * height * depth;
            const uint32_t squaresGroupCount    = largestGroup;
            const uint32_t geometriesGroupCount = 1;
            const uint32_t instancesGroupCount  = 1;
            const CaseDef caseDef               = {
                id,                     //  TestId id;
                name,                   //  const char* name;
                width,                  //  uint32_t width;
                height,                 //  uint32_t height;
                depth,                  //  uint32_t depth;
                depth,                  //  uint32_t raysDepth;
                VK_FORMAT_R32_SINT,     //  VkFormat format;
                false,                  //  bool fixedPointScalarOutput;
                false,                  //  bool fixedPointVectorOutput;
                false,                  //  bool fixedPointMatrixOutput;
                GEOM_TYPE_TRIANGLES,    //  GeomType geomType;
                squaresGroupCount,      //  uint32_t squaresGroupCount;
                geometriesGroupCount,   //  uint32_t geometriesGroupCount;
                instancesGroupCount,    //  uint32_t instancesGroupCount;
                stages[stageNdx].stage, //  VkShaderStageFlagBits stage;
                false,                  //  bool skipTriangles;
                false,                  //  bool skipAABSs;
                false,                  //  bool opaque;
                false,                  //  bool frontFace;
                0u,                     //  VkPipelineCreateFlags pipelineCreateFlags;
                false,                  // bool useSpecConstants;
                false,                  // bool skipClosestHit;
                false,                  //  bool useMaintenance5;
            };
            const std::string suffix =
                de::toString(caseDef.width) + '_' + de::toString(caseDef.height) + '_' + de::toString(caseDef.depth);
            const std::string testName = string(stages[stageNdx].name) + '_' + suffix;

            group->addChild(new RayTracingTestCase(testCtx, testName.c_str(), caseDef));
        }
    }

    builtinGroup->addChild(group.release());
}

void createScalarTests(tcu::TestContext &testCtx, tcu::TestCaseGroup *builtinGroup, TestId id, const char *name,
                       const VkShaderStageFlags shaderStageFlags)
{
    const struct
    {
        uint32_t width;
        uint32_t height;
        TestId id;
    } sizes[] = {
        {16, 16, TEST_ID_HIT_KIND_EXT},  {16, 16, TEST_ID_HIT_T_EXT}, {16, 16, TEST_ID_RAY_T_MIN_EXT},
        {16, 16, TEST_ID_RAY_T_MAX_EXT}, {32, 32, TEST_ID_LAST},      {64, 64, TEST_ID_LAST},
        {256, 256, TEST_ID_LAST},
    };
    const bool fourGeometryGroups = id == TEST_ID_HIT_KIND_EXT || id == TEST_ID_HIT_T_EXT ||
                                    id == TEST_ID_RAY_T_MIN_EXT || id == TEST_ID_RAY_T_MAX_EXT;
    const bool fixedPointScalarOutput =
        id == TEST_ID_HIT_T_EXT || id == TEST_ID_RAY_T_MIN_EXT || id == TEST_ID_RAY_T_MAX_EXT;
    const uint32_t imageDepth = 1;
    const uint32_t rayDepth   = 1;

    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, de::toLower(name).c_str()));

    for (size_t geomTypesNdx = 0; geomTypesNdx < DE_LENGTH_OF_ARRAY(geomTypes); ++geomTypesNdx)
        for (size_t stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stages); ++stageNdx)
        {
            const GeomType geomType = geomTypes[geomTypesNdx].geomType;

            if ((shaderStageFlags & stages[stageNdx].stage) == 0)
                continue;

            if (stages[stageNdx].stage == VK_SHADER_STAGE_INTERSECTION_BIT_KHR &&
                geomTypes[geomTypesNdx].geomType == GEOM_TYPE_TRIANGLES)
                continue;

            bool testAdded           = false;
            bool generalTestsStarted = false;

            for (size_t sizesNdx = 0; sizesNdx < DE_LENGTH_OF_ARRAY(sizes); ++sizesNdx)
            {
                const bool specializedTest          = (sizes[sizesNdx].id != TEST_ID_LAST);
                const uint32_t width                = sizes[sizesNdx].width;
                const uint32_t height               = sizes[sizesNdx].height;
                const uint32_t instancesGroupCount  = fourGeometryGroups ? 1 : 4;
                const uint32_t geometriesGroupCount = fourGeometryGroups ? 4 : 8;
                const uint32_t largestGroup         = width * height / geometriesGroupCount / instancesGroupCount;
                const uint32_t squaresGroupCount    = largestGroup;
                const CaseDef caseDef               = {
                    id,                     //  TestId id;
                    name,                   //  const char* name;
                    width,                  //  uint32_t width;
                    height,                 //  uint32_t height;
                    imageDepth,             //  uint32_t depth;
                    rayDepth,               //  uint32_t raysDepth;
                    VK_FORMAT_R32_SINT,     //  VkFormat format;
                    fixedPointScalarOutput, //  bool fixedPointScalarOutput;
                    false,                  //  bool fixedPointVectorOutput;
                    false,                  //  bool fixedPointMatrixOutput;
                    geomType,               //  GeomType geomType;
                    squaresGroupCount,      //  uint32_t squaresGroupCount;
                    geometriesGroupCount,   //  uint32_t geometriesGroupCount;
                    instancesGroupCount,    //  uint32_t instancesGroupCount;
                    stages[stageNdx].stage, //  VkShaderStageFlagBits stage;
                    false,                  //  bool skipTriangles;
                    false,                  //  bool skipAABSs;
                    false,                  //  bool opaque;
                    false,                  //  bool frontFace;
                    0u,                     //  VkPipelineCreateFlags pipelineCreateFlags;
                    false,                  // bool useSpecConstants;
                    false,                  // bool skipClosestHit;
                    false,                  //  bool useMaintenance5;
                };
                const std::string suffix   = '_' + de::toString(caseDef.width) + '_' + de::toString(caseDef.height);
                const std::string testName = string(stages[stageNdx].name) + '_' + geomTypes[geomTypesNdx].name +
                                             (specializedTest ? "" : suffix);

                if (specializedTest)
                {
                    DE_UNREF(generalTestsStarted);
                    DE_ASSERT(!generalTestsStarted);

                    if (sizes[sizesNdx].id != id)
                        continue;
                }
                else
                {
                    generalTestsStarted = true;
                }

                group->addChild(new RayTracingTestCase(testCtx, testName.c_str(), caseDef));
                testAdded = true;

                if (specializedTest)
                    break;
            }

            DE_ASSERT(testAdded);
            DE_UNREF(testAdded);
        }

    builtinGroup->addChild(group.release());
}

void createRayFlagsTests(tcu::TestContext &testCtx, tcu::TestCaseGroup *builtinGroup, TestId id, const char *name,
                         const VkShaderStageFlags shaderStageFlags)
{
    const uint32_t width      = 16;
    const uint32_t height     = 16;
    const uint32_t imageDepth = 1;
    const uint32_t rayDepth   = 1;

    const struct Opaques
    {
        const char *name;
        bool flag;
    } opaques[] = {
        {"opaque", true},
        {"noopaque", false},
    };
    const struct Faces
    {
        const char *name;
        bool flag;
    } faces[] = {
        {"frontface", true},
        {"backface", false},
    };
    const struct SkipRayFlags
    {
        const char *name;
        bool skipTriangles;
        bool skipAABBs;
    } skipRayFlags[] = {
        {"raynoskipflags", false, false},
        {"rayskiptriangles", true, false},
        {"rayskipaabbs", false, true},
    };
    const struct PipelineFlags
    {
        const char *name;
        VkPipelineCreateFlags flag;
    } pipelineFlags[] = {
        {"pipelinenoskipflags", static_cast<VkPipelineCreateFlags>(0)},
        {"pipelineskiptriangles", VK_PIPELINE_CREATE_RAY_TRACING_SKIP_TRIANGLES_BIT_KHR},
        {"pipelineskipaabbs", VK_PIPELINE_CREATE_RAY_TRACING_SKIP_AABBS_BIT_KHR},
    };

    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, de::toLower(name).c_str()));

    for (size_t geomTypesNdx = 0; geomTypesNdx < DE_LENGTH_OF_ARRAY(geomTypes); ++geomTypesNdx)
    {
        const GeomType geomType = geomTypes[geomTypesNdx].geomType;
        de::MovePtr<tcu::TestCaseGroup> geomGroup(new tcu::TestCaseGroup(testCtx, geomTypes[geomTypesNdx].name));

        for (size_t skipRayFlagsNdx = 0; skipRayFlagsNdx < DE_LENGTH_OF_ARRAY(skipRayFlags); ++skipRayFlagsNdx)
        {
            de::MovePtr<tcu::TestCaseGroup> rayFlagsGroup(
                new tcu::TestCaseGroup(testCtx, skipRayFlags[skipRayFlagsNdx].name));

            for (size_t pipelineFlagsNdx = 0; pipelineFlagsNdx < DE_LENGTH_OF_ARRAY(pipelineFlags); ++pipelineFlagsNdx)
            {
                const bool skipTriangles =
                    (skipRayFlags[skipRayFlagsNdx].skipTriangles ||
                     (pipelineFlags[pipelineFlagsNdx].flag & VK_PIPELINE_CREATE_RAY_TRACING_SKIP_TRIANGLES_BIT_KHR));
                const bool skipAABBs =
                    (skipRayFlags[skipRayFlagsNdx].skipAABBs ||
                     (pipelineFlags[pipelineFlagsNdx].flag & VK_PIPELINE_CREATE_RAY_TRACING_SKIP_AABBS_BIT_KHR));

                // Skipping both triangles and AABBs is not legal according to the spec.
                if (skipTriangles && skipAABBs)
                    continue;

                // Skipping - SkipTrianglesKHR is mutually exclusive with CullBackFacingTrianglesKHR and CullFrontFacingTrianglesKHR
                if ((geomTypes[geomTypesNdx].geomType == GEOM_TYPE_TRIANGLES) &&
                    skipRayFlags[skipRayFlagsNdx].skipTriangles &&
                    (pipelineFlags[pipelineFlagsNdx].flag == static_cast<VkPipelineCreateFlags>(0)))
                {
                    continue;
                }

                de::MovePtr<tcu::TestCaseGroup> pipelineFlagsGroup(
                    new tcu::TestCaseGroup(testCtx, pipelineFlags[pipelineFlagsNdx].name));

                for (size_t opaquesNdx = 0; opaquesNdx < DE_LENGTH_OF_ARRAY(opaques); ++opaquesNdx)
                    for (size_t facesNdx = 0; facesNdx < DE_LENGTH_OF_ARRAY(faces); ++facesNdx)
                    {
                        const std::string geomPropertiesGroupName =
                            string(opaques[opaquesNdx].name) + '_' + string(faces[facesNdx].name);
                        de::MovePtr<tcu::TestCaseGroup> geomPropertiesGroup(
                            new tcu::TestCaseGroup(testCtx, geomPropertiesGroupName.c_str()));

                        for (size_t stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stages); ++stageNdx)
                        {
                            if ((shaderStageFlags & stages[stageNdx].stage) == 0)
                                continue;

                            if (stages[stageNdx].stage == VK_SHADER_STAGE_INTERSECTION_BIT_KHR &&
                                geomTypes[geomTypesNdx].geomType == GEOM_TYPE_TRIANGLES)
                                continue;

                            const uint32_t instancesGroupCount  = 1;
                            const uint32_t geometriesGroupCount = 1;
                            const uint32_t squaresGroupCount =
                                width * height / geometriesGroupCount / instancesGroupCount;
                            const CaseDef caseDef = {
                                id,                                          //  TestId id;
                                name,                                        //  const char* name;
                                width,                                       //  uint32_t width;
                                height,                                      //  uint32_t height;
                                imageDepth,                                  //  uint32_t depth;
                                rayDepth,                                    //  uint32_t raysDepth;
                                VK_FORMAT_R32_SINT,                          //  VkFormat format;
                                false,                                       //  bool fixedPointScalarOutput;
                                false,                                       //  bool fixedPointVectorOutput;
                                false,                                       //  bool fixedPointMatrixOutput;
                                geomType,                                    //  GeomType geomType;
                                squaresGroupCount,                           //  uint32_t squaresGroupCount;
                                geometriesGroupCount,                        //  uint32_t geometriesGroupCount;
                                instancesGroupCount,                         //  uint32_t instancesGroupCount;
                                stages[stageNdx].stage,                      //  VkShaderStageFlagBits stage;
                                skipRayFlags[skipRayFlagsNdx].skipTriangles, //  bool skipTriangles;
                                skipRayFlags[skipRayFlagsNdx].skipAABBs,     //  bool skipAABSs;
                                opaques[opaquesNdx].flag,                    //  bool opaque;
                                faces[facesNdx].flag,                        //  bool frontFace;
                                pipelineFlags[pipelineFlagsNdx].flag, //  VkPipelineCreateFlags pipelineCreateFlags;
                                false,                                // bool useSpecConstants;
                                false,                                // bool skipClosestHit;
                                false,                                // bool useMaintenance5;
                            };
                            const std::string testName = string(stages[stageNdx].name);

                            geomPropertiesGroup->addChild(new RayTracingTestCase(testCtx, testName.c_str(), caseDef));
                        }

                        pipelineFlagsGroup->addChild(geomPropertiesGroup.release());
                    }

                rayFlagsGroup->addChild(pipelineFlagsGroup.release());
            }

            geomGroup->addChild(rayFlagsGroup.release());
        }

        group->addChild(geomGroup.release());
    }

    {
        de::MovePtr<tcu::TestCaseGroup> miscGroup(new tcu::TestCaseGroup(testCtx, "misc"));
        CaseDef caseDef{
            TEST_ID_INCOMING_RAY_FLAGS_EXT,                        //  TestId id;
            name,                                                  //  const char* name;
            width,                                                 //  uint32_t width;
            height,                                                //  uint32_t height;
            imageDepth,                                            //  uint32_t depth;
            rayDepth,                                              //  uint32_t raysDepth;
            VK_FORMAT_R32_SINT,                                    //  VkFormat format;
            false,                                                 //  bool fixedPointScalarOutput;
            false,                                                 //  bool fixedPointVectorOutput;
            false,                                                 //  bool fixedPointMatrixOutput;
            GEOM_TYPE_TRIANGLES,                                   //  GeomType geomType;
            width * height,                                        //  uint32_t squaresGroupCount;
            1,                                                     //  uint32_t geometriesGroupCount;
            1,                                                     //  uint32_t instancesGroupCount;
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,                   //  VkShaderStageFlagBits stage;
            false,                                                 //  bool skipTriangles;
            false,                                                 //  bool skipAABSs;
            false,                                                 //  bool opaque;
            true,                                                  //  bool frontFace;
            VK_PIPELINE_CREATE_RAY_TRACING_SKIP_TRIANGLES_BIT_KHR, //  VkPipelineCreateFlags pipelineCreateFlags;
            false,                                                 // bool useSpecConstants;
            false,                                                 // bool skipClosestHit;
            true,                                                  // bool useMaintenance5;
        };

        miscGroup->addChild(new RayTracingTestCase(testCtx, "pipelineskiptriangles_maintenance5", caseDef));
        caseDef.pipelineCreateFlags = VK_PIPELINE_CREATE_RAY_TRACING_SKIP_AABBS_BIT_KHR;
        miscGroup->addChild(new RayTracingTestCase(testCtx, "pipelineskipaabbs_maintenance5", caseDef));

        group->addChild(miscGroup.release());
    }

    builtinGroup->addChild(group.release());
}

void createMultiOutputTests(tcu::TestContext &testCtx, tcu::TestCaseGroup *builtinGroup, TestId id, const char *name,
                            const VkShaderStageFlags shaderStageFlags)
{
    const bool fixedPointVectorOutput = id == TEST_ID_WORLD_RAY_ORIGIN_EXT || id == TEST_ID_WORLD_RAY_DIRECTION_EXT ||
                                        id == TEST_ID_OBJECT_RAY_ORIGIN_EXT || id == TEST_ID_OBJECT_RAY_DIRECTION_EXT;
    const bool fixedPointMatrixOutput = id == TEST_ID_OBJECT_TO_WORLD_EXT || id == TEST_ID_WORLD_TO_OBJECT_EXT ||
                                        id == TEST_ID_OBJECT_TO_WORLD_3X4_EXT || id == TEST_ID_WORLD_TO_OBJECT_3X4_EXT;
    const uint32_t imageDepth = fixedPointMatrixOutput ? 4 * 4 : fixedPointVectorOutput ? 4 : 0;
    const uint32_t rayDepth   = 1;

    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, de::toLower(name).c_str()));

    DE_ASSERT(imageDepth != 0);

    for (size_t geomTypesNdx = 0; geomTypesNdx < DE_LENGTH_OF_ARRAY(geomTypes); ++geomTypesNdx)
        for (size_t stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stages); ++stageNdx)
        {
            const GeomType geomType = geomTypes[geomTypesNdx].geomType;

            if ((shaderStageFlags & stages[stageNdx].stage) == 0)
                continue;

            if (stages[stageNdx].stage == VK_SHADER_STAGE_INTERSECTION_BIT_KHR &&
                geomTypes[geomTypesNdx].geomType == GEOM_TYPE_TRIANGLES)
                continue;

            const uint32_t width                = 4;
            const uint32_t height               = 4;
            const uint32_t instancesGroupCount  = 4;
            const uint32_t geometriesGroupCount = 1;
            const uint32_t largestGroup         = width * height / geometriesGroupCount / instancesGroupCount;
            const uint32_t squaresGroupCount    = largestGroup;
            const CaseDef caseDef               = {
                id,                     //  TestId id;
                name,                   //  const char* name;
                width,                  //  uint32_t width;
                height,                 //  uint32_t height;
                imageDepth,             //  uint32_t depth;
                rayDepth,               //  uint32_t raysDepth;
                VK_FORMAT_R32_SINT,     //  VkFormat format;
                false,                  //  bool fixedPointScalarOutput;
                fixedPointVectorOutput, //  bool fixedPointVectorOutput;
                fixedPointMatrixOutput, //  bool fixedPointMatrixOutput;
                geomType,               //  GeomType geomType;
                squaresGroupCount,      //  uint32_t squaresGroupCount;
                geometriesGroupCount,   //  uint32_t geometriesGroupCount;
                instancesGroupCount,    //  uint32_t instancesGroupCount;
                stages[stageNdx].stage, //  VkShaderStageFlagBits stage;
                false,                  //  bool rayFlagSkipTriangles;
                false,                  //  bool rayFlagSkipAABSs;
                false,                  //  bool opaque;
                false,                  //  bool frontFace;
                0u,                     //  VkPipelineCreateFlags pipelineCreateFlags;
                false,                  // bool useSpecConstants;
                false,                  // bool skipClosestHit;
                false,                  // bool useMaintenance5;
            };
            const std::string testName = string(stages[stageNdx].name) + '_' + geomTypes[geomTypesNdx].name;

            group->addChild(new RayTracingTestCase(testCtx, testName.c_str(), caseDef));
        }

    builtinGroup->addChild(group.release());
}

void createIndirectTestCases(tcu::TestContext &testCtx, tcu::TestCaseGroup *indirectGroup, TestId id, const char *name)
{
    const struct
    {
        VkShaderStageFlagBits stage;
        const char *name;
    } types[] = {
        {VK_SHADER_STAGE_RAYGEN_BIT_KHR, "triangles"},
        {VK_SHADER_STAGE_INTERSECTION_BIT_KHR, "aabbs"},
    };

    const CaseDef caseDef = {
        id,                                 //  TestId id;
        "",                                 //  const char* name;
        0,                                  //  uint32_t width;
        0,                                  //  uint32_t height;
        0,                                  //  uint32_t depth;
        0,                                  //  uint32_t raysDepth;
        VK_FORMAT_R32_SINT,                 //  VkFormat format;
        false,                              //  bool fixedPointScalarOutput;
        false,                              //  bool fixedPointVectorOutput;
        false,                              //  bool fixedPointMatrixOutput;
        GEOM_TYPE_TRIANGLES,                //  GeomType geomType;
        0,                                  //  uint32_t squaresGroupCount;
        0,                                  //  uint32_t geometriesGroupCount;
        0,                                  //  uint32_t instancesGroupCount;
        VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM, //  VkShaderStageFlagBits stage;
        false,                              //  bool rayFlagSkipTriangles;
        false,                              //  bool rayFlagSkipAABSs;
        false,                              //  bool opaque;
        false,                              //  bool frontFace;
        0u,                                 //  VkPipelineCreateFlags pipelineCreateFlags;
        false,                              // bool useSpecConstants;
        false,                              // bool skipClosestHit;
        false,                              // bool useMaintenance5;
    };

    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, name));

    for (size_t typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(types); ++typeNdx)
    {
        CaseDef params = caseDef;
        params.stage   = types[typeNdx].stage;
        group->addChild(new RayTracingIndirectTestCase(testCtx, types[typeNdx].name, params));
    }
    indirectGroup->addChild(group.release());
}

void createIndirectFlagsTestCases(tcu::TestContext &testCtx, tcu::TestCaseGroup *indirectGroup, TestId id,
                                  const char *name)
{
    const struct
    {
        VkShaderStageFlagBits stage;
        const char *name;
    } types[] = {
        {VK_SHADER_STAGE_RAYGEN_BIT_KHR, "triangles"},
        {VK_SHADER_STAGE_INTERSECTION_BIT_KHR, "aabbs"},
    };

    const struct
    {
        bool opaque;
        bool skipClosestHit;
        const char *name;
    } flags[] = {
        {false, false, "none"},
        {true, false, "opaque"},
        {false, true, "skip_closest_hit"},
    };

    const CaseDef caseDef = {
        id,                                 //  TestId id;
        "",                                 //  const char* name;
        0,                                  //  uint32_t width;
        0,                                  //  uint32_t height;
        0,                                  //  uint32_t depth;
        0,                                  //  uint32_t raysDepth;
        VK_FORMAT_R32_SINT,                 //  VkFormat format;
        false,                              //  bool fixedPointScalarOutput;
        false,                              //  bool fixedPointVectorOutput;
        false,                              //  bool fixedPointMatrixOutput;
        GEOM_TYPE_TRIANGLES,                //  GeomType geomType;
        0,                                  //  uint32_t squaresGroupCount;
        0,                                  //  uint32_t geometriesGroupCount;
        0,                                  //  uint32_t instancesGroupCount;
        VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM, //  VkShaderStageFlagBits stage;
        false,                              //  bool rayFlagSkipTriangles;
        false,                              //  bool rayFlagSkipAABSs;
        false,                              //  bool opaque;
        false,                              //  bool frontFace;
        0u,                                 //  VkPipelineCreateFlags pipelineCreateFlags;
        false,                              // bool useSpecConstants;
        false,                              // bool skipClosestHit;
        false,                              // bool useMaintenance5;
    };

    de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, name));

    for (size_t typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(types); ++typeNdx)
    {
        CaseDef params = caseDef;
        params.stage   = types[typeNdx].stage;
        de::MovePtr<tcu::TestCaseGroup> typeGroup(new tcu::TestCaseGroup(testCtx, types[typeNdx].name));

        for (size_t flagsNdx = 0; flagsNdx < DE_LENGTH_OF_ARRAY(flags); ++flagsNdx)
        {
            params.opaque         = flags[flagsNdx].opaque;
            params.skipClosestHit = flags[flagsNdx].skipClosestHit;
            typeGroup->addChild(new RayTracingIndirectTestCase(testCtx, flags[flagsNdx].name, params));
        }
        testGroup->addChild(typeGroup.release());
    }
    indirectGroup->addChild(testGroup.release());
}

void createIndirectTests(tcu::TestContext &testCtx, tcu::TestCaseGroup *builtinGroup)
{
    typedef void CreateIndirectTestsFunc(tcu::TestContext & testCtx, tcu::TestCaseGroup * group, TestId id,
                                         const char *name);

    const struct
    {
        TestId id;
        const char *name;
        CreateIndirectTestsFunc *createTestsFunc;
    } tests[] = {
        {TEST_ID_INDICES_INDIRECT, "indices", createIndirectTestCases},
        {TEST_ID_TRANSFORMS_INDIRECT, "transforms", createIndirectTestCases},
        {TEST_ID_TMINMAX_INDIRECT, "t_min_max", createIndirectTestCases},
        {TEST_ID_INCOMING_RAY_FLAGS_INDIRECT, "incoming_flag", createIndirectFlagsTestCases},
        {TEST_ID_HIT_KIND_INDIRECT, "hit_kind", createIndirectTestCases},
    };

    // Test builtins using indirect trace rays
    de::MovePtr<tcu::TestCaseGroup> indirectGroup(new tcu::TestCaseGroup(testCtx, "indirect"));

    for (size_t testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(tests); ++testNdx)
    {
        tests[testNdx].createTestsFunc(testCtx, indirectGroup.get(), tests[testNdx].id, tests[testNdx].name);
    }
    builtinGroup->addChild(indirectGroup.release());
}
} // namespace

tcu::TestCaseGroup *createBuiltinTests(tcu::TestContext &testCtx)
{
    typedef void CreateBuiltinTestsFunc(tcu::TestContext & testCtx, tcu::TestCaseGroup * group, TestId id,
                                        const char *name, const VkShaderStageFlags);

    const VkShaderStageFlagBits R = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    const VkShaderStageFlagBits A = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    const VkShaderStageFlagBits C = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    const VkShaderStageFlagBits M = VK_SHADER_STAGE_MISS_BIT_KHR;
    const VkShaderStageFlagBits I = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
    const VkShaderStageFlagBits L = VK_SHADER_STAGE_CALLABLE_BIT_KHR;

    const struct
    {
        TestId id;
        const char *name;
        VkShaderStageFlags stages;
        CreateBuiltinTestsFunc *createBuiltinTestsFunc;
    } tests[] = {
        {TEST_ID_LAUNCH_ID_EXT, "LaunchIDEXT", R | A | C | I | M | L, createLaunchTests},
        {TEST_ID_LAUNCH_SIZE_EXT, "LaunchSizeEXT", R | A | C | I | M | L, createLaunchTests},
        {TEST_ID_PRIMITIVE_ID, "PrimitiveID", A | C | I, createScalarTests},
        {TEST_ID_INSTANCE_ID, "InstanceID", A | C | I, createScalarTests},
        {TEST_ID_INSTANCE_CUSTOM_INDEX_EXT, "InstanceCustomIndexEXT", A | C | I, createScalarTests},
        {TEST_ID_GEOMETRY_INDEX_EXT, "GeometryIndexEXT", A | C | I, createScalarTests},
        {TEST_ID_WORLD_RAY_ORIGIN_EXT, "WorldRayOriginEXT", A | C | I | M, createMultiOutputTests},
        {TEST_ID_WORLD_RAY_DIRECTION_EXT, "WorldRayDirectionEXT", A | C | I | M, createMultiOutputTests},
        {TEST_ID_OBJECT_RAY_ORIGIN_EXT, "ObjectRayOriginEXT", A | C | I, createMultiOutputTests},
        {TEST_ID_OBJECT_RAY_DIRECTION_EXT, "ObjectRayDirectionEXT", A | C | I, createMultiOutputTests},
        {TEST_ID_RAY_T_MIN_EXT, "RayTminEXT", A | C | I | M, createScalarTests},
        {TEST_ID_RAY_T_MAX_EXT, "RayTmaxEXT", A | C | I | M, createScalarTests},
        {TEST_ID_INCOMING_RAY_FLAGS_EXT, "IncomingRayFlagsEXT", A | C | I | M, createRayFlagsTests},
        {TEST_ID_HIT_T_EXT, "HitTEXT", A | C, createScalarTests},
        {TEST_ID_HIT_KIND_EXT, "HitKindEXT", A | C, createScalarTests},
        {TEST_ID_OBJECT_TO_WORLD_EXT, "ObjectToWorldEXT", A | C | I, createMultiOutputTests},
        {TEST_ID_WORLD_TO_OBJECT_EXT, "WorldToObjectEXT", A | C | I, createMultiOutputTests},
        {TEST_ID_OBJECT_TO_WORLD_3X4_EXT, "ObjectToWorld3x4EXT", A | C | I, createMultiOutputTests},
        {TEST_ID_WORLD_TO_OBJECT_3X4_EXT, "WorldToObject3x4EXT", A | C | I, createMultiOutputTests},
    };

    // Ray tracing shader builtin tests
    de::MovePtr<tcu::TestCaseGroup> builtinGroup(new tcu::TestCaseGroup(testCtx, "builtin"));

    for (size_t testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(tests); ++testNdx)
    {
        tests[testNdx].createBuiltinTestsFunc(testCtx, builtinGroup.get(), tests[testNdx].id, tests[testNdx].name,
                                              tests[testNdx].stages);
    }

    {
        createIndirectTests(testCtx, builtinGroup.get());
    }

    return builtinGroup.release();
}

tcu::TestCaseGroup *createSpecConstantTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "spec_constants"));

    const VkShaderStageFlags stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                                          VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
                                          VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR;
    const uint32_t width                = 256u;
    const uint32_t height               = 256u;
    const uint32_t depth                = 1u;
    const bool plain                    = isPlain(width, height, depth);
    const uint32_t k                    = (plain ? 1 : 6);
    const uint32_t largestGroup         = k * width * height * depth;
    const uint32_t squaresGroupCount    = largestGroup;
    const uint32_t geometriesGroupCount = 1;
    const uint32_t instancesGroupCount  = 1;

    for (int stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stages); ++stageNdx)
    {
        if ((stageFlags & stages[stageNdx].stage) == 0)
            continue;

        const CaseDef caseDef = {
            TEST_ID_LAUNCH_ID_EXT,  //  TestId id;
            "LaunchIDEXT",          //  const char* name;
            width,                  //  uint32_t width;
            height,                 //  uint32_t height;
            depth,                  //  uint32_t depth;
            depth,                  //  uint32_t raysDepth;
            VK_FORMAT_R32_SINT,     //  VkFormat format;
            false,                  //  bool fixedPointScalarOutput;
            false,                  //  bool fixedPointVectorOutput;
            false,                  //  bool fixedPointMatrixOutput;
            GEOM_TYPE_TRIANGLES,    //  GeomType geomType;
            squaresGroupCount,      //  uint32_t squaresGroupCount;
            geometriesGroupCount,   //  uint32_t geometriesGroupCount;
            instancesGroupCount,    //  uint32_t instancesGroupCount;
            stages[stageNdx].stage, //  VkShaderStageFlagBits stage;
            false,                  //  bool skipTriangles;
            false,                  //  bool skipAABSs;
            false,                  //  bool opaque;
            false,                  //  bool frontFace;
            0u,                     //  VkPipelineCreateFlags pipelineCreateFlags;
            true,                   // bool useSpecConstants;
            false,                  // bool skipClosestHit;
            false,                  // bool useMaintenance5;
        };

        group->addChild(new RayTracingTestCase(testCtx, stages[stageNdx].name, caseDef));
    }

    return group.release();
}

} // namespace RayTracing
} // namespace vkt
