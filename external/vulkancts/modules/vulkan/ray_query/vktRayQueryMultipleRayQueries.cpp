/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2023 Advanced Micro Devices, Inc.
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
* \brief ray query multiple rayQuery objects for VK_KHR_ray_query utility functions
*//*--------------------------------------------------------------------*/

#include "vktRayQueryMultipleRayQueries.hpp"
#include "vkRayTracingUtil.hpp"
#include "tcuTestCase.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "tcuSurface.hpp"

namespace vkt
{
namespace RayQuery
{

namespace
{

using namespace vk;
constexpr float MAX_T_VALUE = 10000000.0;

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

static std::vector<ResultData> computeExpectedResults()
{
    std::vector<ResultData> expectedResults;
    expectedResults.push_back(ResultData{0, 3, 6, 0});
    expectedResults.push_back(ResultData{1, 4, 7, 0});
    expectedResults.push_back(ResultData{2, 5, 8, 0});
    expectedResults.push_back(ResultData{10, 13, 16, 0});
    expectedResults.push_back(ResultData{11, 14, 17, 0});
    expectedResults.push_back(ResultData{12, 15, 18, 0});
    return expectedResults;
}

class MultipleRayQueriesCase : public TestCase
{
public:
    MultipleRayQueriesCase(tcu::TestContext &testCtx, const std::string &name, const RayQueryTestParams &params);
    virtual ~MultipleRayQueriesCase(void)
    {
    }

    virtual void checkSupport(Context &context) const;
    virtual void initPrograms(vk::SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;

protected:
    RayQueryTestParams m_params;
};

class MultipleRayQueriesInstance : public TestInstance
{
public:
    MultipleRayQueriesInstance(Context &context, const RayQueryTestParams &params);
    virtual ~MultipleRayQueriesInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void);

protected:
    RayQueryTestParams m_params;
};

MultipleRayQueriesCase::MultipleRayQueriesCase(tcu::TestContext &testCtx, const std::string &name,
                                               const RayQueryTestParams &params)
    : TestCase(testCtx, name)
    , m_params(params)
{
}

void MultipleRayQueriesCase::checkSupport(Context &context) const
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

    if ((m_params.shaderSourceType == RayQueryShaderSourceType::TESSELLATION_CONTROL ||
         m_params.shaderSourceType == RayQueryShaderSourceType::TESSELLATION_EVALUATION) &&
        features2.features.tessellationShader == false)
        TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceFeatures2.tessellationShader");

    if (m_params.shaderSourceType == RayQueryShaderSourceType::GEOMETRY && features2.features.geometryShader == false)
        TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceFeatures2.geometryShader");

    switch (m_params.shaderSourceType)
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

    if (m_params.shaderSourceType == RayQueryShaderSourceType::RAY_GENERATION ||
        m_params.shaderSourceType == RayQueryShaderSourceType::INTERSECTION ||
        m_params.shaderSourceType == RayQueryShaderSourceType::ANY_HIT ||
        m_params.shaderSourceType == RayQueryShaderSourceType::CLOSEST_HIT ||
        m_params.shaderSourceType == RayQueryShaderSourceType::MISS ||
        m_params.shaderSourceType == RayQueryShaderSourceType::CALLABLE)
    {
        context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

        const VkPhysicalDeviceRayTracingPipelineFeaturesKHR &rayTracingPipelineFeaturesKHR =
            context.getRayTracingPipelineFeatures();

        if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == false)
            TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");
    }
}

void MultipleRayQueriesCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string rayQueryPart = "";
    std::string ifPart       = "";
    std::string elsePart     = "";
    std::ostringstream src;

    // This is rayQueryEXT array version
    src << "   const int rayQueryCount = 3;\n"
           "   Ray ray[rayQueryCount];\n"
           "   ray[0] = rays[index];\n"
           "   ray[1] = rays[index];\n"
           "   ray[2] = rays[index];\n"
           "   ray[1].pos.x += 3.0;\n"
           "   ray[2].pos.x += 6.0;\n"
           "   float x = 0;\n"
           "   float y = 0;\n"
           "   float z = 0;\n"
           "   float w = 0;\n"
           "   float tempResults[] = {0, 0, 0};\n"
           "   rayQueryEXT rqs[rayQueryCount];\n"
           "   bool prcds[] = {true, true, true};\n"
           "\n"
           "   for (int idx=0;idx<rayQueryCount;++idx)\n"
           "   {\n"
           "         rayQueryInitializeEXT(rqs[idx], scene, "
        << m_params.rayFlags
        << ", 0xFF, ray[idx].pos, ray[idx].tmin, ray[idx].dir, ray[idx].tmax);\n"
           "   }\n"
           "\n"
           "   bool proceed = true;\n"
           "   while (proceed)\n" // traverse all rayQueries in parallel to verify rayQueryCount issues
           "    {\n"
           "       proceed = false;\n"
           "        for (int idx=0;idx<rayQueryCount;++idx)\n"
           "        {\n"
           "           prcds[idx] = prcds[idx] && rayQueryProceedEXT(rqs[idx]);\n"
           "            if (prcds[idx])\n"
           "            {\n"
           "               if (rayQueryGetIntersectionTypeEXT(rqs[idx], true) == "
           "gl_RayQueryCommittedIntersectionGeneratedEXT)\n"
           "               {\n"
           "                    prcds[idx] = false;\n"
           "               }\n"
           "               else if (rayQueryGetIntersectionTypeEXT(rqs[idx], false) == "
           "gl_RayQueryCandidateIntersectionTriangleEXT)\n"
           "                {\n"
           "                    rayQueryConfirmIntersectionEXT(rqs[idx]);\n"
           "                }\n"
           "                else if (rayQueryGetIntersectionTypeEXT(rqs[idx], false) == "
           "gl_RayQueryCandidateIntersectionAABBEXT)\n"
           "                {\n"
           "                    uint primIndex = rayQueryGetIntersectionPrimitiveIndexEXT(rqs[idx], false);\n"
           "                    rayQueryGenerateIntersectionEXT(rqs[idx], 100.f + primIndex * 10.f - (index/3 * "
           "95.f));\n"
           "                }\n"
           "            }\n"
           "           proceed = proceed || prcds[idx];\n"
           "       }\n"
           "   }\n"
           "   for (int idx=0;idx<rayQueryCount;++idx)\n"
           "    {\n"
           "        if ((rayQueryGetIntersectionTypeEXT(rqs[idx], true) == "
           "gl_RayQueryCommittedIntersectionTriangleEXT) ||\n"
           "            (rayQueryGetIntersectionTypeEXT(rqs[idx], true) == "
           "gl_RayQueryCommittedIntersectionGeneratedEXT))\n"
           "        {\n"
           "            uint instIdx = rayQueryGetIntersectionInstanceIdEXT(rqs[idx], true);\n"
           "           uint primIndex = rayQueryGetIntersectionPrimitiveIndexEXT(rqs[idx], true);\n"
           "            tempResults[idx] = float(instIdx) * 10.f  +  float(primIndex);\n"
           "        }\n"
           "        rayQueryTerminateEXT(rqs[idx]);\n"
           "   }\n"
           "\n"
           "   x = tempResults[0];\n"
           "   y = tempResults[1];\n"
           "   z = tempResults[2];\n";

    rayQueryPart = src.str();
    generateRayQueryShaders(programCollection, m_params, rayQueryPart, MAX_T_VALUE);
}

TestInstance *MultipleRayQueriesCase::createInstance(Context &context) const
{
    return new MultipleRayQueriesInstance(context, m_params);
}

MultipleRayQueriesInstance::MultipleRayQueriesInstance(Context &context, const RayQueryTestParams &params)
    : vkt::TestInstance(context)
    , m_params(params)
{
}

tcu::TestStatus MultipleRayQueriesInstance::iterate(void)
{
    std::vector<tcu::Vec3> emptyVerts;

    m_params.rays = {Ray{tcu::Vec3(-2.5f, 0.5f, 0.0f), 0.0f, tcu::Vec3(0.0f, 0.0f, 1.0f), MAX_T_VALUE},
                     Ray{tcu::Vec3(-2.5f, -0.5f, 0.0f), 1.0f, tcu::Vec3(0.0f, 0.0f, 1.0f), MAX_T_VALUE},
                     Ray{tcu::Vec3(-1.5f, 0.5f, 0.0f), 2.0f, tcu::Vec3(0.0f, 0.0f, 1.0f), MAX_T_VALUE},
                     Ray{tcu::Vec3(-2.5f, 0.0f, 95.0f), 3.0f, tcu::Vec3(0.0f, 0.0f, 1.0f), MAX_T_VALUE},
                     Ray{tcu::Vec3(-1.5f, 0.0f, 95.0f), 4.0f, tcu::Vec3(0.0f, 0.0f, 1.0f), MAX_T_VALUE},
                     Ray{tcu::Vec3(-0.5f, 0.0f, 95.0f), 5.0f, tcu::Vec3(0.0f, 0.0f, 1.0f), MAX_T_VALUE}};

    const uint32_t width  = static_cast<uint32_t>(m_params.rays.size());
    const uint32_t height = 1;

    // instance 0
    //(-3,1) (-2,1) (-1,1)  (0,1)  (1,1)  (2,1)  (3,1)   (4,1)   (5,1)
    //   X------X------X      X------X------X       X------X------X
    //   | {A} /| {C} /       | {D} /| {F} /        | {G} /| {I} /
    //   | rq1/ | rq1/        | rq2/ | rq2/         | rq3/ | rq3/
    //   |   /  |   /         |   /  |   /          |   /  |   /
    //   |  /   |  /          |  /   |  /           |  /   |  /
    //   | / {B}| /           | / {E}| /            | / {H}| /
    //   |/ rq1 |/            |/ rq2 |/             |/  rq3|/
    //   X------X             X------X              X------X
    //(-3,-1) (-2,-1)       (0,-1) (1,-1)        (3,-1)  (4,-1)
    //

    // instance 1
    // (-3,1) (-2,1) (-1,1) (0,1)  (1,1)  (2,1)  (3,1)  (4,1)  (5,1)  (6,1)
    //   X------X------X------X------X------X------X------X------X------X
    //   |      |      |      |      |      |      |      |      |      |
    //   |      |      |      |      |      |      |      |      |      |
    //   |      |      |      |      |      |      |      |      |      |
    //   | {J}  |  {K} |  {L} | {M}  |  {N} | {O}  | {P}  | {Q}  | {R}  |
    //   | rq1  |  rq1 |  rq1 | rq2  |  rq2 | rq2  | rq3  | rq3  | rq3  |
    //   |      |      |      |      |      |      |      |      |      |
    //   X------X------X------X------X------X------X------X------X------X
    // (-3,-1)(-2,-1) (-1,-1)(0,-1) (1,-1) (2,-1) (3,-1) (4,-1) (5,-1) (6,-1)
    //

    std::vector<tcu::Vec3> instance1 = {{-3.0f, 1.0f, 10.f},                                             // (A) - prim 0
                                        {-2.0f, 1.0f, 10.f}, {-3.0f, -1.0f, 10.f}, {-3.0f, -1.0f, 20.f}, // (B) - prim 1
                                        {-2.0f, 1.0f, 20.f}, {-2.0f, -1.0f, 20.f}, {-2.0f, 1.0f, 30.f},  // (C) - prim 2
                                        {-1.0f, 1.0f, 30.f}, {-2.0f, -1.0f, 30.f}, {0.0f, 1.0f, 40.f},   // (D) - prim 3
                                        {1.0f, 1.0f, 40.f},  {0.0f, -1.0f, 40.f},  {0.0f, -1.0f, 50.f},  // (E) - prim 4
                                        {1.0f, 1.0f, 50.f},  {1.0f, -1.0f, 50.f},  {1.0f, 1.0f, 60.f},   // (F) - prim 5
                                        {2.0f, 1.0f, 60.f},  {1.0f, -1.0f, 60.f},  {3.0f, 1.0f, 70.f},   // (G) - prim 6
                                        {4.0f, 1.0f, 70.f},  {3.0f, -1.0f, 70.f},  {3.0f, -1.0f, 80.f},  // (H) - prim 7
                                        {4.0f, 1.0f, 80.f},  {4.0f, -1.0f, 80.f},  {4.0f, 1.0f, 90.f},   // (I) - prim 8
                                        {5.0f, 1.0f, 90.f},  {4.0f, -1.0f, 90.f}};

    std::vector<tcu::Vec3> instance2 = {{-3.0f, -1.0f, 100.f},                        // (J) - prim 0
                                        {-2.0f, 1.0f, 100.f},  {-2.0f, -1.0f, 110.f}, // (K) - prim 1
                                        {-1.0f, 1.0f, 110.f},  {-1.0f, -1.0f, 120.f}, // (L) - prim 2
                                        {0.0f, 1.0f, 120.f},   {0.0f, -1.0f, 130.f},  // (M) - prim 3
                                        {1.0f, 1.0f, 130.f},   {1.0f, -1.0f, 140.f},  // (N) - prim 4
                                        {2.0f, 1.0f, 140.f},   {2.0f, -1.0f, 150.f},  // (O) - prim 5
                                        {3.0f, 1.0f, 150.f},   {3.0f, -1.0f, 160.f},  // (P) - prim 6
                                        {4.0f, 1.0f, 160.f},   {4.0f, -1.0f, 170.f},  // (Q) - prim 7
                                        {5.0f, 1.0f, 170.f},   {5.0f, -1.0f, 180.f},  // (R) - prim 8
                                        {6.0f, 1.0f, 180.f}};

    m_params.verts.push_back(instance1);
    m_params.verts.push_back(emptyVerts);
    m_params.aabbs.push_back(emptyVerts);
    m_params.aabbs.push_back(instance2);

    std::vector<ResultData> expectedResults = computeExpectedResults();

    std::vector<ResultData> resultData;

    switch (m_params.pipelineType)
    {
    case RayQueryShaderSourcePipeline::COMPUTE:
    {
        resultData = rayQueryComputeTestSetup<ResultData>(
            m_context.getDeviceInterface(), m_context.getDevice(), m_context.getDefaultAllocator(),
            m_context.getInstanceInterface(), m_context.getPhysicalDevice(), m_context.getBinaryCollection(),
            m_context.getUniversalQueue(), m_context.getUniversalQueueFamilyIndex(), m_params);
        break;
    }
    case RayQueryShaderSourcePipeline::RAYTRACING:
    {
        m_context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");
        const VkPhysicalDeviceRayTracingPipelineFeaturesKHR &rayTracingPipelineFeaturesKHR =
            m_context.getRayTracingPipelineFeatures();

        if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == false)
            TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");

        resultData = rayQueryRayTracingTestSetup<ResultData>(
            m_context.getDeviceInterface(), m_context.getDevice(), m_context.getDefaultAllocator(),
            m_context.getInstanceInterface(), m_context.getPhysicalDevice(), m_context.getBinaryCollection(),
            m_context.getUniversalQueue(), m_context.getUniversalQueueFamilyIndex(), m_params);
        break;
    }
    case RayQueryShaderSourcePipeline::GRAPHICS:
    {
        resultData = rayQueryGraphicsTestSetup<ResultData>(
            m_context.getDeviceInterface(), m_context.getDevice(), m_context.getUniversalQueueFamilyIndex(),
            m_context.getDefaultAllocator(), m_context.getBinaryCollection(), m_context.getUniversalQueue(),
            m_context.getInstanceInterface(), m_context.getPhysicalDevice(), m_params);
        break;
    }
    default:
    {
        TCU_FAIL("Invalid shader type!");
    }
    }

    bool mismatch = false;

    tcu::Surface resultImage(width, height);
    for (uint32_t x = 0; x < static_cast<uint32_t>(resultImage.getWidth()); ++x)
    {
        for (uint32_t y = 0; y < static_cast<uint32_t>(resultImage.getHeight()); ++y)
        {
            uint32_t index = x * resultImage.getHeight() + y;
            if (resultData[index].equal(expectedResults[index]))
            {
                resultImage.setPixel(x, y, tcu::RGBA(255, 0, 0, 255));
            }
            else
            {
                mismatch = true;
                resultImage.setPixel(x, y, tcu::RGBA(0, 0, 0, 255));
            }
        }
    }

    // Write Image
    m_context.getTestContext().getLog() << tcu::TestLog::ImageSet("Result of rendering", "Result of rendering")
                                        << tcu::TestLog::Image("Result", "Result", resultImage)
                                        << tcu::TestLog::EndImageSet;

    if (mismatch)
        TCU_FAIL("Result data did not match expected output");

    return tcu::TestStatus::pass("pass");
}

} // anonymous namespace

tcu::TestCaseGroup *createMultipleRayQueryTests(tcu::TestContext &testCtx)
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

    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "multiple_ray_queries"));

    for (size_t shaderSourceNdx = 0; shaderSourceNdx < DE_LENGTH_OF_ARRAY(shaderSourceTypes); ++shaderSourceNdx)
    {
        RayQueryTestParams testParams{};
        testParams.shaderSourceType = shaderSourceTypes[shaderSourceNdx].shaderSourceType;
        testParams.pipelineType     = shaderSourceTypes[shaderSourceNdx].shaderSourcePipeline;
        testParams.resourceRes      = ResourceResidency::TRADITIONAL;
        group->addChild(
            new MultipleRayQueriesCase(group->getTestContext(), shaderSourceTypes[shaderSourceNdx].name, testParams));
    }

    return group.release();
}

} // namespace RayQuery
} // namespace vkt
