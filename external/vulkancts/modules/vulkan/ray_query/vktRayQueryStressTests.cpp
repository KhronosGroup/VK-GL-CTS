/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2020 Advanced Micro Devices, Inc.
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
* \brief ray query stress tests for VK_KHR_ray_query utility functions
*//*--------------------------------------------------------------------*/

#include "vkRayTracingUtil.hpp"
#include "tcuTestCase.hpp"
#include "tcuSurface.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktRayQueryStressTests.hpp"
#include <iostream>
#include <cmath>
#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795
#endif

namespace vkt
{
namespace RayQuery
{

namespace
{

using namespace vk;
constexpr float MAX_T_VALUE = 10000000.0;

enum class TestType
{
    TRIANGLES,
    AABBS,
};

struct StressTestParams
{
    TestType testType;
};

struct ResultData
{
    ResultData() : x(0.f), y(0.f), z(0.f), w(0.f)
    {
    }
    ResultData(const float ix) : x(ix), y(ix), z(ix), w(ix)
    {
    }
    ResultData(const float ix, const float iy, const float iz, const float iw) : x(ix), y(iy), z(iz), w(iw)
    {
    }
    bool equal(ResultData other)
    {
        const float epsilon = 0.000001f;
        return ((abs(other.x - x) < epsilon) && (abs(other.y - y) < epsilon) && (abs(other.z - z) < epsilon) &&
                (abs(other.w - w) < epsilon));
    }

    float x;
    float y;
    float z;
    float w;
};

class RayQueryStressCase : public TestCase
{
public:
    RayQueryStressCase(tcu::TestContext &testCtx, const std::string &name, const RayQueryTestParams &rayQueryParams,
                       const StressTestParams &stressParams);
    virtual ~RayQueryStressCase(void)
    {
    }

    virtual void checkSupport(Context &context) const;
    virtual void initPrograms(vk::SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;

protected:
    RayQueryTestParams m_rayQueryParams;
    StressTestParams m_stressParams;
};

class RayQueryStressInstance : public TestInstance
{
public:
    RayQueryStressInstance(Context &context, const RayQueryTestParams &rayQueryParams,
                           const StressTestParams &stressParams);
    virtual ~RayQueryStressInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void);

protected:
    RayQueryTestParams m_rayQueryParams;
    StressTestParams m_stressParams;
};

RayQueryStressCase::RayQueryStressCase(tcu::TestContext &testCtx, const std::string &name,
                                       const RayQueryTestParams &rayQueryParams, const StressTestParams &stressParams)
    : TestCase(testCtx, name)
    , m_rayQueryParams(rayQueryParams)
    , m_stressParams(stressParams)
{
}

void RayQueryStressCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
    context.requireDeviceFunctionality("VK_KHR_ray_query");

    const VkPhysicalDeviceRayQueryFeaturesKHR &rayQueryFeaturesKHR = context.getRayQueryFeatures();
    if (rayQueryFeaturesKHR.rayQuery == false)
        TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayQueryFeaturesKHR.rayQuery");

    const VkPhysicalDeviceAccelerationStructureFeaturesKHR &accelerationStructureFeaturesKHR =
        context.getAccelerationStructureFeatures();
    if (accelerationStructureFeaturesKHR.accelerationStructure == false)
        TCU_THROW(TestError,
                  "VK_KHR_ray_query requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");

    const VkPhysicalDeviceFeatures2 &features2 = context.getDeviceFeatures2();

    if ((m_rayQueryParams.shaderSourceType == RayQueryShaderSourceType::TESSELLATION_CONTROL ||
         m_rayQueryParams.shaderSourceType == RayQueryShaderSourceType::TESSELLATION_EVALUATION) &&
        features2.features.tessellationShader == false)
        TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceFeatures2.tessellationShader");

    if (m_rayQueryParams.shaderSourceType == RayQueryShaderSourceType::GEOMETRY &&
        features2.features.geometryShader == false)
        TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceFeatures2.geometryShader");

    switch (m_rayQueryParams.shaderSourceType)
    {
    case RayQueryShaderSourceType::VERTEX:
    case RayQueryShaderSourceType::TESSELLATION_CONTROL:
    case RayQueryShaderSourceType::TESSELLATION_EVALUATION:
    case RayQueryShaderSourceType::GEOMETRY:
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS);
        break;
    default:
        break;
    }

    if (m_rayQueryParams.shaderSourceType == RayQueryShaderSourceType::RAY_GENERATION ||
        m_rayQueryParams.shaderSourceType == RayQueryShaderSourceType::RAY_GENERATION_RT ||
        m_rayQueryParams.shaderSourceType == RayQueryShaderSourceType::INTERSECTION ||
        m_rayQueryParams.shaderSourceType == RayQueryShaderSourceType::ANY_HIT ||
        m_rayQueryParams.shaderSourceType == RayQueryShaderSourceType::CLOSEST_HIT ||
        m_rayQueryParams.shaderSourceType == RayQueryShaderSourceType::MISS ||
        m_rayQueryParams.shaderSourceType == RayQueryShaderSourceType::CALLABLE)
    {
        context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

        const VkPhysicalDeviceRayTracingPipelineFeaturesKHR &rayTracingPipelineFeaturesKHR =
            context.getRayTracingPipelineFeatures();

        if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == false)
            TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");
    }
}

void RayQueryStressCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string rayQueryPart = "";
    std::string ifPart       = "";
    std::string elsePart     = "";
    std::ostringstream src;

    if (m_rayQueryParams.shaderSourceType != RayQueryShaderSourceType::RAY_GENERATION_RT)
    {
        src << "   Ray ray = rays[index];\n"
               "   float x = "
            << MAX_T_VALUE * 2
            << ";\n"
               "   float y = "
            << MAX_T_VALUE * 2
            << ";\n"
               "   float z = index;\n"
               "   float w = ray.pos.z;\n"
               "   rayQueryEXT rayQuery;\n"
               "   rayQueryInitializeEXT(rayQuery, scene, "
            << m_rayQueryParams.rayFlags
            << ", 0xFF, ray.pos, ray.tmin, ray.dir, ray.tmax);\n"
               "   while (rayQueryProceedEXT(rayQuery))\n"
               "   {\n"
               "       if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == "
               "gl_RayQueryCandidateIntersectionTriangleEXT)\n"
               "       {\n"
               "           rayQueryConfirmIntersectionEXT(rayQuery);\n"
               "       }\n"
               "       else if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == "
               "gl_RayQueryCandidateIntersectionAABBEXT)\n"
               "       {\n"
               "           float t = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, false) - index + 0.5f;\n"
               "           if (t < rayQueryGetIntersectionTEXT(rayQuery, true))"
               "           {\n"
               "                rayQueryGenerateIntersectionEXT(rayQuery, t);\n"
               "           }\n"
               "       }\n"
               "   }\n"
               "\n"
               "   if ((rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT) "
               "||\n"
               "       (rayQueryGetIntersectionTypeEXT(rayQuery, true) == "
               "gl_RayQueryCommittedIntersectionGeneratedEXT))\n"
               "   {\n"
               "       x = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);\n"
               "       y = rayQueryGetIntersectionTEXT(rayQuery, true);\n"
               "   }\n"
               "   rayQueryTerminateEXT(rayQuery);\n";
    }
    else if (m_rayQueryParams.shaderSourceType == RayQueryShaderSourceType::RAY_GENERATION_RT)
    {
        src << "   Ray ray = rays[index];\n"
               "   float x = "
            << MAX_T_VALUE * 2
            << ";\n"
               "   float y = "
            << MAX_T_VALUE * 2
            << ";\n"
               "   float z = 0;\n"
               "   float w = 0;\n"
               "   traceRayEXT(scene, 0, 0xFF, 0, 0, 0, ray.pos, ray.tmin, ray.dir, ray.tmax, 0);\n"
               "   x = payload.x;\n"
               "   y = payload.y;\n"
               "   z = payload.z;\n"
               "   w = payload.w;\n";
    }

    rayQueryPart = src.str();
    generateRayQueryShaders(programCollection, m_rayQueryParams, rayQueryPart, MAX_T_VALUE);
}

TestInstance *RayQueryStressCase::createInstance(Context &context) const
{
    return new RayQueryStressInstance(context, m_rayQueryParams, m_stressParams);
}

RayQueryStressInstance::RayQueryStressInstance(Context &context, const RayQueryTestParams &rayQueryParams,
                                               const StressTestParams &stressParams)
    : vkt::TestInstance(context)
    , m_rayQueryParams(rayQueryParams)
    , m_stressParams(stressParams)
{
}

tcu::TestStatus RayQueryStressInstance::iterate(void)
{
    std::vector<tcu::Vec3> emptyVerts;

    /*    TestType::TRIANGLES
        instance 0
        PrimitiveIds and intersectionTs are used to verify correct intersection

        Triangle composed of triangles at increasing z-values - sort of triangular shaped stairs
        This structure is repeated for 20000 levels with increasing z-values for each triangle.

        *   X-------------------X   *
        *    \ \_    z=2      //    *
        *     \   \_       /  /     *
        *      \     \  /    /      *
        *       \     O     /       *
        *        \ z=0|z=1 /        *
        *         \   |   /         *
        *          \  |  /          *
        *           \ | /           *
        *            \|/            *
        *             X             *

        The rays are shot at the center of gravity of the triangles in the xy plane and originated at z minus epsilon to hit the triangle at z.

        TestType::AABBS
        each level is composed 0f 3 aabbs
        there are 20000 levels at increasing z-values
        60000 rays are shot at the center of each aabb originated at z minus epsilon to hit the aabb at (cx, cy, z)

              X-----------------X
              |                 |
              |       z=2       |
              X--------O--------X
              |        |        |
              |  z=0   |  z=1   |
              |        |        |
              X--------X--------X
    */

    const uint32_t numLevels        = 20000;
    const uint32_t numPrimsPerLevel = 3;
    const float alfa                = 2.0f * static_cast<float>(M_PI) / static_cast<float>(numPrimsPerLevel);
    const uint32_t totalNumPrims    = numPrimsPerLevel * numLevels;
    const float incrZ               = 1.0f;
    const float epsilon             = incrZ / 10.f;
    const float cosAlfa             = cos(alfa);
    const float sinAlfa             = sin(alfa);
    const float tanAlfaOver2        = abs(tan(alfa / 2.0f));
    tcu::Vec2 p1(tanAlfaOver2, 1);
    tcu::Vec2 p2(-tanAlfaOver2, 1);
    float z = 0;

    m_rayQueryParams.rays.resize(totalNumPrims);
    std::vector<ResultData> expectedResults(totalNumPrims);
    std::vector<tcu::Vec3> instance1(m_stressParams.testType == TestType::TRIANGLES ? totalNumPrims * 3 :
                                                                                      totalNumPrims * 2);

    for (uint32_t idx = 0; idx < totalNumPrims; ++idx)
    {
        tcu::Vec2 center;

        float x = p1[0] * cosAlfa - p1[1] * sinAlfa;
        float y = p1[0] * sinAlfa + p1[1] * cosAlfa;
        p1      = tcu::Vec2(x, y);
        x       = p2[0] * cosAlfa - p2[1] * sinAlfa;
        y       = p2[0] * sinAlfa + p2[1] * cosAlfa;
        p2      = tcu::Vec2(x, y);

        if (m_stressParams.testType == TestType::TRIANGLES)
        {
            tcu::Vec3 v0(0, 0, z);
            tcu::Vec3 v1(p1[0], p1[1], z);
            tcu::Vec3 v2(p2[0], p2[1], z);

            center[0] = (p1[0] + p2[0]) / 3.f;
            center[1] = (p1[1] + p2[1]) / 3.f;

            instance1[idx * 3]     = v0;
            instance1[idx * 3 + 1] = v1;
            instance1[idx * 3 + 2] = v2;

            expectedResults[idx] = ResultData(static_cast<float>(idx), epsilon, 0, 0);
        }
        else
        {
            tcu::Vec3 v0(de::min<float>(p1[0], p2[0]), de::min<float>(p1[1], p2[1]), z);
            tcu::Vec3 v1(de::max<float>(p1[0], p2[0]), de::max<float>(p1[1], p2[1]), z);

            if (p1.y() > 0 && p2.y() > 0)
            {
                //top box
                v0 = {v0.x(), de::min<float>(v1.y(), 0), z};
            }
            else
            {
                //bottom boxes
                v1 = {v1.x(), de::min<float>(v1.y(), 0), z};
            }

            center[0] = (v0[0] + v1[0]) / 2.f;
            center[1] = (v0[1] + v1[1]) / 2.f;

            instance1[idx * 2]     = v0;
            instance1[idx * 2 + 1] = v1;

            expectedResults[idx] = ResultData(static_cast<float>(idx), 0.5f, 0, 0);
        }

        m_rayQueryParams.rays[idx] =
            Ray{tcu::Vec3(center[0], center[1], z - epsilon), 0.0f, tcu::Vec3(0.0f, 0.0f, 1.0f), MAX_T_VALUE};

        z += incrZ;
    }

    if (m_stressParams.testType == TestType::TRIANGLES)
    {
        m_rayQueryParams.verts.push_back(instance1);
        m_rayQueryParams.aabbs.push_back(emptyVerts);
    }
    else
    {
        m_rayQueryParams.verts.push_back(emptyVerts);
        m_rayQueryParams.aabbs.push_back(instance1);
    }

    std::vector<ResultData> resultData;

    switch (m_rayQueryParams.pipelineType)
    {
    case RayQueryShaderSourcePipeline::COMPUTE:
    {
        resultData = rayQueryComputeTestSetup<ResultData>(
            m_context.getDeviceInterface(), m_context.getDevice(), m_context.getDefaultAllocator(),
            m_context.getInstanceInterface(), m_context.getPhysicalDevice(), m_context.getBinaryCollection(),
            m_context.getUniversalQueue(), m_context.getUniversalQueueFamilyIndex(), m_rayQueryParams);
        break;
    }
    case RayQueryShaderSourcePipeline::RAYTRACING:
    {
        resultData = rayQueryRayTracingTestSetup<ResultData>(
            m_context.getDeviceInterface(), m_context.getDevice(), m_context.getDefaultAllocator(),
            m_context.getInstanceInterface(), m_context.getPhysicalDevice(), m_context.getBinaryCollection(),
            m_context.getUniversalQueue(), m_context.getUniversalQueueFamilyIndex(), m_rayQueryParams);
        break;
    }
    case RayQueryShaderSourcePipeline::GRAPHICS:
    {
        resultData = rayQueryGraphicsTestSetup<ResultData>(
            m_context.getDeviceInterface(), m_context.getDevice(), m_context.getUniversalQueueFamilyIndex(),
            m_context.getDefaultAllocator(), m_context.getBinaryCollection(), m_context.getUniversalQueue(),
            m_context.getInstanceInterface(), m_context.getPhysicalDevice(), m_rayQueryParams);
        break;
    }
    default:
    {
        TCU_FAIL("Invalid shader type!");
    }
    }

    const uint32_t width  = numPrimsPerLevel;
    const uint32_t height = numLevels;

    uint32_t index      = 0;
    uint32_t mismatched = 0;

    tcu::Surface resultImage(width, height);
    for (uint32_t x = 0; x < static_cast<uint32_t>(resultImage.getWidth()); ++x)
    {
        for (uint32_t y = 0; y < static_cast<uint32_t>(resultImage.getHeight()); ++y)
        {
            if ((resultData[index].x == expectedResults[index].x) &&
                (abs(resultData[index].y - expectedResults[index].y) < 0.2))
            {
                resultImage.setPixel(x, y, tcu::RGBA(255, 0, 0, 255));
            }
            else
            {
                mismatched++;
                resultImage.setPixel(x, y, tcu::RGBA(0, 0, 0, 255));
            }
            index++;
        }
    }

    // Write Image
    m_context.getTestContext().getLog() << tcu::TestLog::ImageSet("Result of rendering", "Result of rendering")
                                        << tcu::TestLog::Image("Result", "Result", resultImage)
                                        << tcu::TestLog::EndImageSet;

    if (mismatched > 0)
        TCU_FAIL("Result data did not match expected output");

    return tcu::TestStatus::pass("pass");
}

} // anonymous namespace

tcu::TestCaseGroup *createRayQueryStressTests(tcu::TestContext &testCtx)
{
    struct ShaderSourceTypeData
    {
        RayQueryShaderSourceType shaderSourceType;
        RayQueryShaderSourcePipeline shaderSourcePipeline;
        const char *name;
    } shaderSourceTypes[] = {
        {RayQueryShaderSourceType::VERTEX, RayQueryShaderSourcePipeline::GRAPHICS, "vertex_shader"},
        {RayQueryShaderSourceType::TESSELLATION_CONTROL, RayQueryShaderSourcePipeline::GRAPHICS, "tess_control_shader"},
        {RayQueryShaderSourceType::TESSELLATION_EVALUATION, RayQueryShaderSourcePipeline::GRAPHICS,
         "tess_evaluation_shader"},
        {
            RayQueryShaderSourceType::GEOMETRY,
            RayQueryShaderSourcePipeline::GRAPHICS,
            "geometry_shader",
        },
        {
            RayQueryShaderSourceType::FRAGMENT,
            RayQueryShaderSourcePipeline::GRAPHICS,
            "fragment_shader",
        },
        {
            RayQueryShaderSourceType::COMPUTE,
            RayQueryShaderSourcePipeline::COMPUTE,
            "compute_shader",
        },
        {
            RayQueryShaderSourceType::RAY_GENERATION,
            RayQueryShaderSourcePipeline::RAYTRACING,
            "rgen_shader",
        },
        {
            RayQueryShaderSourceType::RAY_GENERATION_RT,
            RayQueryShaderSourcePipeline::RAYTRACING,
            "rgen_rt_shader",
        },
        {
            RayQueryShaderSourceType::INTERSECTION,
            RayQueryShaderSourcePipeline::RAYTRACING,
            "isect_shader",
        },
        {
            RayQueryShaderSourceType::ANY_HIT,
            RayQueryShaderSourcePipeline::RAYTRACING,
            "ahit_shader",
        },
        {
            RayQueryShaderSourceType::CLOSEST_HIT,
            RayQueryShaderSourcePipeline::RAYTRACING,
            "chit_shader",
        },
        {
            RayQueryShaderSourceType::MISS,
            RayQueryShaderSourcePipeline::RAYTRACING,
            "miss_shader",
        },
        {
            RayQueryShaderSourceType::CALLABLE,
            RayQueryShaderSourcePipeline::RAYTRACING,
            "call_shader",
        },
    };

    struct
    {
        TestType testType;
        const char *name;
    } bottomTestTypes[] = {
        {TestType::TRIANGLES, "triangles"},
        {TestType::AABBS, "aabbs"},
    };

    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "stress", "Ray query stress tests"));

    for (size_t shaderSourceNdx = 0; shaderSourceNdx < DE_LENGTH_OF_ARRAY(shaderSourceTypes); ++shaderSourceNdx)
    {
        de::MovePtr<tcu::TestCaseGroup> sourceTypeGroup(
            new tcu::TestCaseGroup(group->getTestContext(), shaderSourceTypes[shaderSourceNdx].name, ""));
        for (size_t bottomTestNdx = 0; bottomTestNdx < DE_LENGTH_OF_ARRAY(bottomTestTypes); ++bottomTestNdx)
        {
            RayQueryTestParams rayQueryTestParams{};
            rayQueryTestParams.shaderSourceType = shaderSourceTypes[shaderSourceNdx].shaderSourceType;
            rayQueryTestParams.pipelineType     = shaderSourceTypes[shaderSourceNdx].shaderSourcePipeline;
            StressTestParams testParams{};
            testParams.testType = bottomTestTypes[bottomTestNdx].testType;
            sourceTypeGroup->addChild(new RayQueryStressCase(
                group->getTestContext(), bottomTestTypes[bottomTestNdx].name, rayQueryTestParams, testParams));
        }
        group->addChild(sourceTypeGroup.release());
    }

    return group.release();
}

} // namespace RayQuery
} // namespace vkt
