/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
 * Copyright (c) 2024 Valve Corporation.
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
 * \brief Device Generated Commands EXT Ray Tracing Tests
 *//*--------------------------------------------------------------------*/

#include "vktDGCRayTracingTestsExt.hpp"
#include "vktTestCase.hpp"
#include "vkRayTracingUtil.hpp"
#include "vktDGCUtilExt.hpp"
#include "vktDGCUtilCommon.hpp"

#include "tcuVectorUtil.hpp"

#include "deUniquePtr.hpp"
#include "deRandom.hpp"

#include <string>
#include <sstream>
#include <vector>
#include <cstdlib>

namespace vkt
{
namespace DGC
{

namespace
{

using namespace vk;

// Place geometry in the XY [0, N] range, with one horizontal and vertical unit per instance.
//
// In the Z coordinate, geometry will be located around +10. Inactive geometries, part of the same bottom level AS, will
// be located in negative Z ranges to make sure rays do not hit them.
//
// Rays will be cast from the middle X+0.5, Y+0.5 points, towards +Z.
//
struct BottomLevelASParams
{
    static constexpr uint32_t kTriangles = 0u;
    static constexpr uint32_t kAABBs     = 1u;

    static constexpr uint32_t kCounterClockwise = 0u;
    static constexpr uint32_t kClockwise        = 1u;

    static constexpr uint32_t kPrimitiveCount = 4u;
    static constexpr uint32_t kGeometryCount  = 2u;
    static constexpr float kBaseZ             = 10.0f;

    uint32_t geometryType;        // 0: triangles, 1: AABBs.
    uint32_t activeGeometryIndex; // Other geometries will be located such that the ray doesn't hit them.
    uint32_t windingDirection;    // 0: counter clockwise, 1: clockwise.
    uint32_t closestPrimitive;    // [0,kPrimitiveCount)

    BottomLevelASParams(de::Random &rnd)
    {
        geometryType        = (rnd.getBool() ? kTriangles : kAABBs);
        activeGeometryIndex = static_cast<uint32_t>(rnd.getInt(0, static_cast<int>(kGeometryCount - 1u)));
        windingDirection    = (rnd.getBool() ? kCounterClockwise : kClockwise);
        closestPrimitive    = static_cast<uint32_t>(rnd.getInt(0, static_cast<int>(kPrimitiveCount - 1u)));
    }
};

constexpr uint32_t kWidth      = 16u;
constexpr uint32_t kHeight     = 16u;
constexpr uint32_t kBLASCount  = 16u;
constexpr uint32_t kSBTCount   = 2u;
constexpr uint32_t kDispHeight = kHeight / kSBTCount; // Each dispatch will handle a number of rows.

// === GLSL_EXT_ray_tracing ===
constexpr uint32_t kRayFlagsNoneEXT = 0u;
//constexpr uint32_t kRayFlagsOpaqueEXT = 1u;
//constexpr uint32_t kRayFlagsNoOpaqueEXT = 2u;
//constexpr uint32_t kRayFlagsTerminateOnFirstHitEXT = 4u;
//constexpr uint32_t kRayFlagsSkipClosestHitShaderEXT = 8u;
constexpr uint32_t kRayFlagsCullBackFacingTrianglesEXT  = 16u;
constexpr uint32_t kRayFlagsCullFrontFacingTrianglesEXT = 32u;
constexpr uint32_t kRayFlagsCullOpaqueEXT               = 64u;
//constexpr uint32_t kRayFlagsCullNoOpaqueEXT = 128u;
constexpr uint32_t kHitKindFrontFacingTriangleEXT = 0xFEu;
constexpr uint32_t kHitKindBackFacingTriangleEXT  = 0xFFu;
// === GLSL_EXT_ray_tracing ===

constexpr float kFloatThreshold = 1.0f / 256.0f;

constexpr VkShaderStageFlags kStageFlags =
    (VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR |
     VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR);

// What to do in each XY 1-unit square where we trace rays.
struct CellParams
{
    tcu::Vec4 origin;
    VkTransformMatrixKHR transformMatrix;
    uint32_t closestPrimitive;    // This is a copy of the bottom level AS param. Needed in the isec shader.
    float zDirection;             // +1 or +2.
    float minT;                   // Appropriate so the ray starts at [4,8]
    float maxT;                   // Appropriate so the ray ends at [20,40]
    uint32_t blasIndex;           // [0, kBLASCount)
    uint32_t instanceCustomIndex; // [100 to 150], peudorandomly and without specific meaning.
    VkBool32 opaque;
    uint32_t rayFlags;  // One of: None, CullBackFacingTri, CullFrontFacingTri, CullOpaque.
    uint32_t missIndex; // 0 or 1.

    uint32_t padding0[3]; // Padding to match std430.

    CellParams(uint32_t x, uint32_t y, de::Random &rnd)
    {
        const auto fx = static_cast<float>(x);
        const auto fy = static_cast<float>(y);

        origin           = tcu::Vec4(fx + 0.5f, fy + 0.5f, 0.0f, 1.0f);
        transformMatrix  = VkTransformMatrixKHR{{
            {1.0f, 0.0f, 0.0f, fx},
            {0.0f, 1.0f, 0.0f, fy},
            {0.0f, 0.0f, 1.0f, 0.0f},
        }};
        closestPrimitive = 0u; // This needs to be copied later, after blasIndex is set in this constructor.

        zDirection          = (rnd.getBool() ? 1.0f : 2.0f);
        minT                = (rnd.getBool() ? 4.0f : 8.0f) / zDirection;
        maxT                = (rnd.getBool() ? 20.0f : 40.0f) / zDirection;
        blasIndex           = static_cast<uint32_t>(rnd.getInt(0, static_cast<int>(kBLASCount - 1u)));
        instanceCustomIndex = static_cast<uint32_t>(rnd.getInt(100, 150)); // Just an ID.
        opaque              = (rnd.getBool() ? VK_TRUE : VK_FALSE);

        static const std::vector<uint32_t> kFlagCatalogue{
            kRayFlagsNoneEXT,
            kRayFlagsCullBackFacingTrianglesEXT,
            kRayFlagsCullFrontFacingTrianglesEXT,
            kRayFlagsCullOpaqueEXT,
        };
        rayFlags  = kFlagCatalogue.at(rnd.getInt(0, static_cast<int>(kFlagCatalogue.size()) - 1));
        missIndex = static_cast<uint32_t>(rnd.getInt(0, 1));
    }
};

// Information to be filled from shaders.
struct CellOutput
{
    // I/O Data.
    tcu::Vec4 rgenInitialPayload;
    tcu::Vec4 rgenFinalPayload;
    tcu::Vec4 chitPayload;
    tcu::Vec4 missPayload;
    tcu::Vec4 chitIncomingPayload;
    tcu::Vec4 missIncomingPayload;
    tcu::Vec4 isecAttribute;
    tcu::Vec4 chitAttribute;
    tcu::Vec4 rgenSRB;
    tcu::Vec4 isecSRB;
    tcu::Vec4 chitSRB;
    tcu::Vec4 missSRB;
    tcu::Vec4 call0SRB;
    tcu::Vec4 call1SRB;

    // Built-ins.
    tcu::UVec4 rgenLaunchIDEXT;
    tcu::UVec4 rgenLaunchSizeEXT;

    tcu::UVec4 chitLaunchIDEXT;
    tcu::UVec4 chitLaunchSizeEXT;

    int32_t chitPrimitiveID;
    int32_t chitInstanceID;
    int32_t chitInstanceCustomIndexEXT;
    int32_t chitGeometryIndexEXT;

    tcu::Vec4 chitWorldRayOriginEXT;
    tcu::Vec4 chitWorldRayDirectionEXT;
    tcu::Vec4 chitObjectRayOriginEXT;
    tcu::Vec4 chitObjectRayDirectionEXT;

    float chitRayTminEXT;
    float chitRayTmaxEXT;
    uint32_t chitIncomingRayFlagsEXT;

    float chitHitTEXT;
    uint32_t chitHitKindEXT;

    uint32_t padding0[3]; // To match the GLSL alignment.

    tcu::Vec4 chitObjectToWorldEXT[3];
    tcu::Vec4 chitObjectToWorld3x4EXT[4];
    tcu::Vec4 chitWorldToObjectEXT[3];
    tcu::Vec4 chitWorldToObject3x4EXT[4];

    tcu::UVec4 isecLaunchIDEXT;
    tcu::UVec4 isecLaunchSizeEXT;

    int32_t isecPrimitiveID;
    int32_t isecInstanceID;
    int32_t isecInstanceCustomIndexEXT;
    int32_t isecGeometryIndexEXT;

    tcu::Vec4 isecWorldRayOriginEXT;
    tcu::Vec4 isecWorldRayDirectionEXT;
    tcu::Vec4 isecObjectRayOriginEXT;
    tcu::Vec4 isecObjectRayDirectionEXT;

    float isecRayTminEXT;
    float isecRayTmaxEXT;
    uint32_t isecIncomingRayFlagsEXT;

    uint32_t padding1[1]; // To match the GLSL alignment.

    tcu::Vec4 isecObjectToWorldEXT[3];
    tcu::Vec4 isecObjectToWorld3x4EXT[4];
    tcu::Vec4 isecWorldToObjectEXT[3];
    tcu::Vec4 isecWorldToObject3x4EXT[4];

    tcu::UVec4 missLaunchIDEXT;
    tcu::UVec4 missLaunchSizeEXT;

    tcu::Vec4 missWorldRayOriginEXT;
    tcu::Vec4 missWorldRayDirectionEXT;

    float missRayTminEXT;
    float missRayTmaxEXT;
    uint32_t missIncomingRayFlagsEXT;

    uint32_t padding2[1]; // To match the GLSL alignment.

    tcu::UVec4 callLaunchIDEXT;
    tcu::UVec4 callLaunchSizeEXT;

    CellOutput(void)
    {
        deMemset(this, 0, sizeof(*this));
    }
};

using BLASPtr = de::SharedPtr<BottomLevelAccelerationStructure>;
using TLASPtr = de::SharedPtr<TopLevelAccelerationStructure>;

BLASPtr makeBottomLevelASWithParams(const BottomLevelASParams &params)
{
    auto blas = makeBottomLevelAccelerationStructure();

    if (params.geometryType == BottomLevelASParams::kTriangles)
    {
        static constexpr uint32_t kTriangleVertices = 3u;
        const bool clockwise                        = (params.windingDirection == BottomLevelASParams::kClockwise);

        for (uint32_t geometryIdx = 0u; geometryIdx < BottomLevelASParams::kGeometryCount; ++geometryIdx)
        {
            std::vector<tcu::Vec3> vertices;
            vertices.reserve(kTriangleVertices * BottomLevelASParams::kPrimitiveCount);

            const float zFactor = (geometryIdx == params.activeGeometryIndex ? 1.0f : -1.0f);

            for (uint32_t primIdx = 0u; primIdx < BottomLevelASParams::kPrimitiveCount; ++primIdx)
            {
                const float zOffset = (primIdx == params.closestPrimitive ? 0.0f : static_cast<float>(primIdx + 1u));
                const float zCoord  = zFactor * BottomLevelASParams::kBaseZ + zOffset;

                const tcu::Vec3 vertA(0.25f, 0.25f, zCoord);
                const tcu::Vec3 vertB(0.75f, 0.25f, zCoord);
                const tcu::Vec3 vertC(0.50f, 0.75f, zCoord);

                vertices.push_back(clockwise ? vertB : vertA);
                vertices.push_back(clockwise ? vertA : vertB);
                vertices.push_back(vertC);
            }

            blas->addGeometry(vertices, true /*triangles*/, 0u);
        }
    }
    else
    {
        static constexpr uint32_t kAABBVertices = 2u;

        for (uint32_t geometryIdx = 0u; geometryIdx < BottomLevelASParams::kGeometryCount; ++geometryIdx)
        {
            std::vector<tcu::Vec3> vertices;
            vertices.reserve(kAABBVertices * BottomLevelASParams::kPrimitiveCount);

            const float zFactor = (geometryIdx == params.activeGeometryIndex ? 1.0f : -1.0f);

            for (uint32_t primIdx = 0u; primIdx < BottomLevelASParams::kPrimitiveCount; ++primIdx)
            {
                const float zOffset = (primIdx == params.closestPrimitive ? 0.0f : static_cast<float>(primIdx + 1u));
                const float zCoord  = zFactor * BottomLevelASParams::kBaseZ + zFactor * zOffset;

                const tcu::Vec3 vertA(0.0f, 0.0f, zCoord);
                const tcu::Vec3 vertB(1.0f, 1.0f, zCoord + 0.5f);

                vertices.push_back(vertA);
                vertices.push_back(vertB);
            }

            blas->addGeometry(vertices, false /*triangles*/, 0u);
        }
    }

    return BLASPtr(blas.release());
}

TLASPtr makeTopLevelASWithParams(const std::vector<BLASPtr> &blas, const std::vector<CellParams> &cellParams)
{
    const auto fixedGeometryFlags = static_cast<VkGeometryInstanceFlagsKHR>(VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR);

    auto topLevelAS = makeTopLevelAccelerationStructure();
    topLevelAS->setInstanceCount(cellParams.size());

    for (const auto &cp : cellParams)
        topLevelAS->addInstance(blas.at(cp.blasIndex), cp.transformMatrix, cp.instanceCustomIndex, 0xFFu, 0u,
                                fixedGeometryFlags);

    return TLASPtr(topLevelAS.release());
}

class RayTracingInstance : public vkt::TestInstance
{
public:
    struct Params
    {
        bool useExecutionSet;
        bool preprocess;
        bool unordered;
        bool computeQueue;

        uint32_t getRandomSeed(void) const
        {
            return 1720182500u;
        }
    };

    RayTracingInstance(Context &context, const Params &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~RayTracingInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;

protected:
    const Params m_params;
};

class RayTracingCase : public vkt::TestCase
{
public:
    RayTracingCase(tcu::TestContext &testCtx, const std::string &name, const RayTracingInstance::Params &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~RayTracingCase(void)
    {
    }

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

protected:
    const RayTracingInstance::Params m_params;
};

void RayTracingCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
    context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");
    context.requireDeviceFunctionality("VK_KHR_ray_tracing_maintenance1");

    const auto bindStages = (m_params.useExecutionSet ? kStageFlags : 0u);
    checkDGCExtSupport(context, kStageFlags, bindStages);

    if (m_params.computeQueue)
        context.getComputeQueue(); // Will throw NotSupportedError if not available.
}

// Offset that the miss index applies to payload values.
uint32_t getMissIndexOffset(uint32_t missIndex)
{
    return (missIndex + 1u) * 1000000u;
}

// Offset that the closest-hit index applies ot payload values.
uint32_t getChitIndexOffset(uint32_t chitIndex)
{
    return (chitIndex + 1u) * 100000u;
}

// Offset that the intersection index sets in the hit attribute.
uint32_t getIsecIndexOffset(uint32_t isecIndex)
{
    return (isecIndex + 1u) * 10000u;
}

// Offset that the callable shader applies to the callable data.
uint32_t getCallIndexOffset(uint32_t callIndex)
{
    return (callIndex + 1u) * 1000u;
}

void RayTracingCase::initPrograms(vk::SourceCollections &programCollection) const
{
    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

    std::string cellParamsDecl;
    {
        // Note this must roughly match the CellParams struct declared above.
        std::ostringstream cellParamsStream;
        cellParamsStream << "struct CellParams\n"
                         << "{\n"
                         << "    vec4  origin;\n"
                         << "    float transformMatrix[12];\n"
                         << "    uint  closestPrimitive;\n"
                         << "    float zDirection;\n"
                         << "    float minT;\n"
                         << "    float maxT;\n"
                         << "    uint  blasIndex;\n"
                         << "    uint  instanceCustomIndex;\n"
                         << "    uint  opaque;\n"
                         << "    uint  rayFlags;\n"
                         << "    uint  missIndex;\n"
                         << "};\n";
        cellParamsDecl = cellParamsStream.str();
    }

    std::string cellOutputDecl;
    {
        std::ostringstream cellOutputStream;
        cellOutputStream << "struct CellOutput\n"
                         << "{\n"
                         << "    vec4 rgenInitialPayload;\n"
                         << "    vec4 rgenFinalPayload;\n"
                         << "    vec4 chitPayload;\n"
                         << "    vec4 missPayload;\n"
                         << "    vec4 chitIncomingPayload;\n"
                         << "    vec4 missIncomingPayload;\n"
                         << "    vec4 isecAttribute;\n"
                         << "    vec4 chitAttribute;\n"
                         << "    vec4 rgenSRB;\n"
                         << "    vec4 isecSRB;\n"
                         << "    vec4 chitSRB;\n"
                         << "    vec4 missSRB;\n"
                         << "    vec4 call0SRB;\n"
                         << "    vec4 call1SRB;\n"
                         << "\n"
                         << "    uvec4 rgenLaunchIDEXT;\n"
                         << "    uvec4 rgenLaunchSizeEXT;\n"
                         << "\n"
                         << "    uvec4 chitLaunchIDEXT;\n"
                         << "    uvec4 chitLaunchSizeEXT;\n"
                         << "\n"
                         << "    int chitPrimitiveID;\n"
                         << "    int chitInstanceID;\n"
                         << "    int chitInstanceCustomIndexEXT;\n"
                         << "    int chitGeometryIndexEXT;\n"
                         << "\n"
                         << "    vec4 chitWorldRayOriginEXT;\n"
                         << "    vec4 chitWorldRayDirectionEXT;\n"
                         << "    vec4 chitObjectRayOriginEXT;\n"
                         << "    vec4 chitObjectRayDirectionEXT;\n"
                         << "\n"
                         << "    float chitRayTminEXT;\n"
                         << "    float chitRayTmaxEXT;\n"
                         << "    uint  chitIncomingRayFlagsEXT;\n"
                         << "\n"
                         << "    float chitHitTEXT;\n"
                         << "    uint  chitHitKindEXT;\n"
                         << "\n"
                         << "    vec4 chitObjectToWorldEXT[3];\n"
                         << "    vec4 chitObjectToWorld3x4EXT[4];\n"
                         << "    vec4 chitWorldToObjectEXT[3];\n"
                         << "    vec4 chitWorldToObject3x4EXT[4];\n"
                         << "\n"
                         << "    uvec4 isecLaunchIDEXT;\n"
                         << "    uvec4 isecLaunchSizeEXT;\n"
                         << "\n"
                         << "    int isecPrimitiveID;\n"
                         << "    int isecInstanceID;\n"
                         << "    int isecInstanceCustomIndexEXT;\n"
                         << "    int isecGeometryIndexEXT;\n"
                         << "\n"
                         << "    vec4 isecWorldRayOriginEXT;\n"
                         << "    vec4 isecWorldRayDirectionEXT;\n"
                         << "    vec4 isecObjectRayOriginEXT;\n"
                         << "    vec4 isecObjectRayDirectionEXT;\n"
                         << "\n"
                         << "    float isecRayTminEXT;\n"
                         << "    float isecRayTmaxEXT;\n"
                         << "    uint  isecIncomingRayFlagsEXT;\n"
                         << "\n"
                         << "    vec4 isecObjectToWorldEXT[3];\n"
                         << "    vec4 isecObjectToWorld3x4EXT[4];\n"
                         << "    vec4 isecWorldToObjectEXT[3];\n"
                         << "    vec4 isecWorldToObject3x4EXT[4];\n"
                         << "\n"
                         << "    uvec4 missLaunchIDEXT;\n"
                         << "    uvec4 missLaunchSizeEXT;\n"
                         << "\n"
                         << "    vec4 missWorldRayOriginEXT;\n"
                         << "    vec4 missWorldRayDirectionEXT;\n"
                         << "\n"
                         << "    float missRayTminEXT;\n"
                         << "    float missRayTmaxEXT;\n"
                         << "    uint  missIncomingRayFlagsEXT;\n"
                         << "\n"
                         << "    uvec4 callLaunchIDEXT;\n"
                         << "    uvec4 callLaunchSizeEXT;\n"
                         << "};\n";
        cellOutputDecl = cellOutputStream.str();
    }

    const uint32_t cellCount = kWidth * kHeight;

    std::string descDecl;
    {
        std::ostringstream descStream;
        descStream << cellParamsDecl << cellOutputDecl
                   << "layout (set=0, binding=0) uniform accelerationStructureEXT topLevelAS;\n"
                   << "layout (set=0, binding=1, std430) readonly buffer InputBlock {\n"
                   << "    CellParams params[" << cellCount << "];\n"
                   << "} ib;\n"
                   << "layout (set=0, binding=2, std430) buffer OutputBlock {\n"
                   << "    CellOutput values[" << cellCount << "];\n"
                   << "} ob;\n"
                   << "layout (push_constant, std430) uniform PCBlock { uint offsetY; } pc;\n";
        descDecl = descStream.str();
    }

    std::string cellIdxFuncDecl;
    {
        std::ostringstream cellIdxFuncStream;
        cellIdxFuncStream
            << "uint getCellIndex(bool print) {\n"
            << "    const uint row = gl_LaunchIDEXT.y + pc.offsetY;\n"
            << "    const uint cellIndex = row * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;\n"
            << "    if (print)"
            << "        debugPrintfEXT(\"pc.offsetY=%u gl_LaunchIDEXT.x=%u gl_LaunchIDEXT.y=%u gl_LaunchSizeEXT.x=%u "
               "gl_LaunchSizeEXT.y=%u row=%u cellIndex=%u\\n\", pc.offsetY, gl_LaunchIDEXT.x, gl_LaunchIDEXT.y, "
               "gl_LaunchSizeEXT.x, gl_LaunchSizeEXT.y, row, cellIndex);\n"
            << "    return cellIndex;\n"
            << "}\n";
        cellIdxFuncDecl = cellIdxFuncStream.str();
    }

    std::string shaderRecordDecl;
    {
        std::ostringstream shaderRecordStream;
        shaderRecordStream << "layout(shaderRecordEXT, std430) buffer SRBBlock {\n"
                           << "    vec4 data;\n"
                           << "} srb;\n";
        shaderRecordDecl = shaderRecordStream.str();
    }

    // 2 ray-gen shaders: one without SRB and one with it.
    for (uint32_t rgenIdx = 0u; rgenIdx < kSBTCount; ++rgenIdx)
    {
        const bool withSRB = (rgenIdx > 0u);
        const auto suffix  = (withSRB ? "-srb" : "");

        std::ostringstream rgen;
        rgen << "#version 460 core\n"
             << "#extension GL_EXT_debug_printf : enable\n"
             << "#extension GL_EXT_ray_tracing : require\n"
             << "layout (location=0) rayPayloadEXT vec4 payload;\n"
             << descDecl << (withSRB ? shaderRecordDecl : "") << cellIdxFuncDecl << "void main()\n"
             << "{\n"
             << "    const uint cellIdx = getCellIndex(false);\n"
             << "\n"
             << "    ob.values[cellIdx].rgenLaunchIDEXT = uvec4(gl_LaunchIDEXT.xyz, 0u);\n"
             << "    ob.values[cellIdx].rgenLaunchSizeEXT = uvec4(gl_LaunchSizeEXT.xyz, 0u);\n"
             << "\n"
             << "    const uint  rayFlags  = ib.params[cellIdx].rayFlags;\n"
             << "    const vec3  origin    = ib.params[cellIdx].origin.xyz;\n"
             << "    const vec3  direction = vec3(0, 0, ib.params[cellIdx].zDirection);\n"
             << "    const float tMin      = ib.params[cellIdx].minT;\n"
             << "    const float tMax      = ib.params[cellIdx].maxT;\n"
             << "    const uint  missIndex = ib.params[cellIdx].missIndex;\n"
             << "    const uint  cullMask  = 0xFF;\n"
             << "    const uint  sbtOffset = 0u;\n"
             << "    const uint  sbtStride = 1u;\n"
             << "\n"
             << "    const vec4 payloadValue = vec4(gl_LaunchIDEXT.xyz, 0.0);\n"
             << "    payload = payloadValue;\n"
             << "    ob.values[cellIdx].rgenInitialPayload = payload;\n"
             << "    traceRayEXT(topLevelAS, rayFlags, cullMask, sbtOffset, sbtStride, missIndex, origin, tMin, "
                "direction, tMax, 0);\n"
             << "    ob.values[cellIdx].rgenFinalPayload = payload;\n"
             << (withSRB ? "    ob.values[cellIdx].rgenSRB = srb.data;\n" : "") << "}\n";
        const auto shaderName = std::string("rgen") + suffix;
        programCollection.glslSources.add(shaderName) << glu::RaygenSource(rgen.str()) << buildOptions;
    }

    // 2 miss shaders, and variants with/without SRB for each.
    for (uint32_t missIdx = 0u; missIdx < 2u; ++missIdx)
        for (uint32_t srbIdx = 0u; srbIdx < kSBTCount; ++srbIdx)
        {
            const bool withSRB = (srbIdx > 0u);
            const auto suffix  = (withSRB ? "-srb" : "");

            std::ostringstream miss;
            miss << "#version 460 core\n"
                 << "#extension GL_EXT_debug_printf : enable\n"
                 << "#extension GL_EXT_ray_tracing : require\n"
                 << "layout (location = 0) rayPayloadInEXT vec4 payload;\n"
                 << descDecl << (withSRB ? shaderRecordDecl : "") << cellIdxFuncDecl << "void main()\n"
                 << "{\n"
                 << "    const uint cellIdx = getCellIndex(false);\n"
                 << "\n"
                 << "    ob.values[cellIdx].missLaunchIDEXT = uvec4(gl_LaunchIDEXT, 0u);\n"
                 << "    ob.values[cellIdx].missLaunchSizeEXT = uvec4(gl_LaunchSizeEXT, 0u);\n"
                 << "    ob.values[cellIdx].missWorldRayOriginEXT = vec4(gl_WorldRayOriginEXT, 1.0);\n"
                 << "    ob.values[cellIdx].missWorldRayDirectionEXT = vec4(gl_WorldRayDirectionEXT, 0.0);\n"
                 << "    ob.values[cellIdx].missRayTminEXT = gl_RayTminEXT;\n"
                 << "    ob.values[cellIdx].missRayTmaxEXT = gl_RayTmaxEXT;\n"
                 << "    ob.values[cellIdx].missIncomingRayFlagsEXT = gl_IncomingRayFlagsEXT;\n"
                 << "\n"
                 << "    ob.values[cellIdx].missIncomingPayload = payload;\n"
                 << "    const float valueOffset = " << getMissIndexOffset(missIdx) << ";\n"
                 << "    const vec4 vecOffset = vec4(valueOffset, valueOffset, valueOffset, valueOffset);\n"
                 << "    payload = payload + vecOffset;\n"
                 << "    ob.values[cellIdx].missPayload = payload;\n"
                 << (withSRB ? "    ob.values[cellIdx].missSRB = srb.data;\n" : "") << "}\n";
            const auto shaderName = std::string("miss") + std::to_string(missIdx) + suffix;
            programCollection.glslSources.add(shaderName) << glu::MissSource(miss.str()) << buildOptions;
        }

    // 2 closest-hit shaders and variants with/without SRB for each.
    for (uint32_t chitIdx = 0u; chitIdx < 2u; ++chitIdx)
        for (uint32_t srbIdx = 0u; srbIdx < kSBTCount; ++srbIdx)
        {
            const bool withSRB = (srbIdx > 0u);
            const auto suffix  = (withSRB ? "-srb" : "");

            std::ostringstream chit;
            chit << "#version 460 core\n"
                 << "#extension GL_EXT_debug_printf : enable\n"
                 << "#extension GL_EXT_ray_tracing : require\n"
                 << "layout (location = 0) rayPayloadInEXT vec4 payload;\n"
                 << "layout (location = 0) callableDataEXT vec4 callData;\n"
                 << "hitAttributeEXT vec2 hitAttrib;\n"
                 << descDecl << (withSRB ? shaderRecordDecl : "") << cellIdxFuncDecl << "void main()\n"
                 << "{\n"
                 << "    const uint cellIdx = getCellIndex(false);\n"
                 << "\n"
                 << "    ob.values[cellIdx].chitLaunchIDEXT = uvec4(gl_LaunchIDEXT, 0u);\n"
                 << "    ob.values[cellIdx].chitLaunchSizeEXT = uvec4(gl_LaunchSizeEXT, 0u);\n"
                 << "    ob.values[cellIdx].chitPrimitiveID = gl_PrimitiveID;\n"
                 << "    ob.values[cellIdx].chitInstanceID = gl_InstanceID;\n"
                 << "    ob.values[cellIdx].chitInstanceCustomIndexEXT = gl_InstanceCustomIndexEXT;\n"
                 << "    ob.values[cellIdx].chitGeometryIndexEXT = gl_GeometryIndexEXT;\n"
                 << "    ob.values[cellIdx].chitWorldRayOriginEXT = vec4(gl_WorldRayOriginEXT, 1.0);\n"
                 << "    ob.values[cellIdx].chitWorldRayDirectionEXT = vec4(gl_WorldRayDirectionEXT, 0.0);\n"
                 << "    ob.values[cellIdx].chitObjectRayOriginEXT = vec4(gl_ObjectRayOriginEXT, 1.0);\n"
                 << "    ob.values[cellIdx].chitObjectRayDirectionEXT = vec4(gl_ObjectRayDirectionEXT, 0.0);\n"
                 << "    ob.values[cellIdx].chitRayTminEXT = gl_RayTminEXT;\n"
                 << "    ob.values[cellIdx].chitRayTmaxEXT = gl_RayTmaxEXT;\n"
                 << "    ob.values[cellIdx].chitIncomingRayFlagsEXT = gl_IncomingRayFlagsEXT;\n"
                 << "    ob.values[cellIdx].chitHitTEXT = gl_HitTEXT;\n"
                 << "    ob.values[cellIdx].chitHitKindEXT = gl_HitKindEXT;\n"
                 << "    ob.values[cellIdx].chitObjectToWorldEXT[0] = vec4(gl_ObjectToWorldEXT[0][0], "
                    "gl_ObjectToWorldEXT[1][0], gl_ObjectToWorldEXT[2][0], gl_ObjectToWorldEXT[3][0]);\n"
                 << "    ob.values[cellIdx].chitObjectToWorldEXT[1] = vec4(gl_ObjectToWorldEXT[0][1], "
                    "gl_ObjectToWorldEXT[1][1], gl_ObjectToWorldEXT[2][1], gl_ObjectToWorldEXT[3][1]);\n"
                 << "    ob.values[cellIdx].chitObjectToWorldEXT[2] = vec4(gl_ObjectToWorldEXT[0][2], "
                    "gl_ObjectToWorldEXT[1][2], gl_ObjectToWorldEXT[2][2], gl_ObjectToWorldEXT[3][2]);\n"
                 << "    ob.values[cellIdx].chitObjectToWorld3x4EXT[0] = vec4(gl_ObjectToWorld3x4EXT[0][0], "
                    "gl_ObjectToWorld3x4EXT[1][0], gl_ObjectToWorld3x4EXT[2][0], 0.0);\n"
                 << "    ob.values[cellIdx].chitObjectToWorld3x4EXT[1] = vec4(gl_ObjectToWorld3x4EXT[0][1], "
                    "gl_ObjectToWorld3x4EXT[1][1], gl_ObjectToWorld3x4EXT[2][1], 0.0);\n"
                 << "    ob.values[cellIdx].chitObjectToWorld3x4EXT[2] = vec4(gl_ObjectToWorld3x4EXT[0][2], "
                    "gl_ObjectToWorld3x4EXT[1][2], gl_ObjectToWorld3x4EXT[2][2], 0.0);\n"
                 << "    ob.values[cellIdx].chitObjectToWorld3x4EXT[3] = vec4(gl_ObjectToWorld3x4EXT[0][3], "
                    "gl_ObjectToWorld3x4EXT[1][3], gl_ObjectToWorld3x4EXT[2][3], 0.0);\n"
                 << "    ob.values[cellIdx].chitWorldToObjectEXT[0] = vec4(gl_WorldToObjectEXT[0][0], "
                    "gl_WorldToObjectEXT[1][0], gl_WorldToObjectEXT[2][0], gl_WorldToObjectEXT[3][0]);\n"
                 << "    ob.values[cellIdx].chitWorldToObjectEXT[1] = vec4(gl_WorldToObjectEXT[0][1], "
                    "gl_WorldToObjectEXT[1][1], gl_WorldToObjectEXT[2][1], gl_WorldToObjectEXT[3][1]);\n"
                 << "    ob.values[cellIdx].chitWorldToObjectEXT[2] = vec4(gl_WorldToObjectEXT[0][2], "
                    "gl_WorldToObjectEXT[1][2], gl_WorldToObjectEXT[2][2], gl_WorldToObjectEXT[3][2]);\n"
                 << "    ob.values[cellIdx].chitWorldToObject3x4EXT[0] = vec4(gl_WorldToObject3x4EXT[0][0], "
                    "gl_WorldToObject3x4EXT[1][0], gl_WorldToObject3x4EXT[2][0], 0.0);\n"
                 << "    ob.values[cellIdx].chitWorldToObject3x4EXT[1] = vec4(gl_WorldToObject3x4EXT[0][1], "
                    "gl_WorldToObject3x4EXT[1][1], gl_WorldToObject3x4EXT[2][1], 0.0);\n"
                 << "    ob.values[cellIdx].chitWorldToObject3x4EXT[2] = vec4(gl_WorldToObject3x4EXT[0][2], "
                    "gl_WorldToObject3x4EXT[1][2], gl_WorldToObject3x4EXT[2][2], 0.0);\n"
                 << "    ob.values[cellIdx].chitWorldToObject3x4EXT[3] = vec4(gl_WorldToObject3x4EXT[0][3], "
                    "gl_WorldToObject3x4EXT[1][3], gl_WorldToObject3x4EXT[2][3], 0.0);\n"
                 << "\n"
                 << "    ob.values[cellIdx].chitIncomingPayload = payload;\n"
                 << "    const float valueOffset = " << getChitIndexOffset(chitIdx) << ";\n"
                 << "    const vec4 vecOffset = vec4(valueOffset, valueOffset, valueOffset, valueOffset);\n"
                 << "    payload = payload + vecOffset;\n"
                 << "    callData = payload;\n"
                 << "    executeCallableEXT(1, 0); // Callable shader 1, callable data 0\n"
                 << "    payload = callData;\n"
                 << "    ob.values[cellIdx].chitPayload = payload;\n"
                 << "    ob.values[cellIdx].chitAttribute = ((gl_HitKindEXT < 0xF0u) ? vec4(hitAttrib.xy, 0, 0) : "
                    "vec4(0, 0, 0, 0));\n"
                 << (withSRB ? "    ob.values[cellIdx].chitSRB = srb.data;\n" : "") << "}\n";
            const auto shaderName = std::string("chit") + std::to_string(chitIdx) + suffix;
            programCollection.glslSources.add(shaderName) << glu::ClosestHitSource(chit.str()) << buildOptions;
        }

    // 2 intersection shaders and variants with/without SRB for each.
    for (uint32_t isecIdx = 0u; isecIdx < 2u; ++isecIdx)
        for (uint32_t srbIdx = 0u; srbIdx < kSBTCount; ++srbIdx)
        {
            const bool withSRB = (srbIdx > 0u);
            const auto suffix  = (withSRB ? "-srb" : "");

            std::ostringstream isec;
            isec << "#version 460 core\n"
                 << "#extension GL_EXT_debug_printf : enable\n"
                 << "#extension GL_EXT_ray_tracing : require\n"
                 << "hitAttributeEXT vec2 hitAttrib;\n"
                 << descDecl << (withSRB ? shaderRecordDecl : "") << cellIdxFuncDecl << "void main()\n"
                 << "{\n"
                 << "    const uint cellIdx = getCellIndex(false);\n"
                 << "\n"
                 << "    if (gl_PrimitiveID == ib.params[cellIdx].closestPrimitive) {\n"
                 << "        ob.values[cellIdx].isecLaunchIDEXT = uvec4(gl_LaunchIDEXT, 0u);\n"
                 << "        ob.values[cellIdx].isecLaunchSizeEXT = uvec4(gl_LaunchSizeEXT, 0u);\n"
                 << "        ob.values[cellIdx].isecPrimitiveID = gl_PrimitiveID;\n"
                 << "        ob.values[cellIdx].isecInstanceID = gl_InstanceID;\n"
                 << "        ob.values[cellIdx].isecInstanceCustomIndexEXT = gl_InstanceCustomIndexEXT;\n"
                 << "        ob.values[cellIdx].isecGeometryIndexEXT = gl_GeometryIndexEXT;\n"
                 << "        ob.values[cellIdx].isecWorldRayOriginEXT = vec4(gl_WorldRayOriginEXT, 1.0);\n"
                 << "        ob.values[cellIdx].isecWorldRayDirectionEXT = vec4(gl_WorldRayDirectionEXT, 0.0);\n"
                 << "        ob.values[cellIdx].isecObjectRayOriginEXT = vec4(gl_ObjectRayOriginEXT, 1.0);\n"
                 << "        ob.values[cellIdx].isecObjectRayDirectionEXT = vec4(gl_ObjectRayDirectionEXT, 0.0);\n"
                 << "        ob.values[cellIdx].isecRayTminEXT = gl_RayTminEXT;\n"
                 << "        ob.values[cellIdx].isecRayTmaxEXT = gl_RayTmaxEXT;\n"
                 << "        ob.values[cellIdx].isecIncomingRayFlagsEXT = gl_IncomingRayFlagsEXT;\n"
                 << "        ob.values[cellIdx].isecObjectToWorldEXT[0] = vec4(gl_ObjectToWorldEXT[0][0], "
                    "gl_ObjectToWorldEXT[1][0], gl_ObjectToWorldEXT[2][0], gl_ObjectToWorldEXT[3][0]);\n"
                 << "        ob.values[cellIdx].isecObjectToWorldEXT[1] = vec4(gl_ObjectToWorldEXT[0][1], "
                    "gl_ObjectToWorldEXT[1][1], gl_ObjectToWorldEXT[2][1], gl_ObjectToWorldEXT[3][1]);\n"
                 << "        ob.values[cellIdx].isecObjectToWorldEXT[2] = vec4(gl_ObjectToWorldEXT[0][2], "
                    "gl_ObjectToWorldEXT[1][2], gl_ObjectToWorldEXT[2][2], gl_ObjectToWorldEXT[3][2]);\n"
                 << "        ob.values[cellIdx].isecObjectToWorld3x4EXT[0] = vec4(gl_ObjectToWorld3x4EXT[0][0], "
                    "gl_ObjectToWorld3x4EXT[1][0], gl_ObjectToWorld3x4EXT[2][0], 0.0);\n"
                 << "        ob.values[cellIdx].isecObjectToWorld3x4EXT[1] = vec4(gl_ObjectToWorld3x4EXT[0][1], "
                    "gl_ObjectToWorld3x4EXT[1][1], gl_ObjectToWorld3x4EXT[2][1], 0.0);\n"
                 << "        ob.values[cellIdx].isecObjectToWorld3x4EXT[2] = vec4(gl_ObjectToWorld3x4EXT[0][2], "
                    "gl_ObjectToWorld3x4EXT[1][2], gl_ObjectToWorld3x4EXT[2][2], 0.0);\n"
                 << "        ob.values[cellIdx].isecObjectToWorld3x4EXT[3] = vec4(gl_ObjectToWorld3x4EXT[0][3], "
                    "gl_ObjectToWorld3x4EXT[1][3], gl_ObjectToWorld3x4EXT[2][3], 0.0);\n"
                 << "        ob.values[cellIdx].isecWorldToObjectEXT[0] = vec4(gl_WorldToObjectEXT[0][0], "
                    "gl_WorldToObjectEXT[1][0], gl_WorldToObjectEXT[2][0], gl_WorldToObjectEXT[3][0]);\n"
                 << "        ob.values[cellIdx].isecWorldToObjectEXT[1] = vec4(gl_WorldToObjectEXT[0][1], "
                    "gl_WorldToObjectEXT[1][1], gl_WorldToObjectEXT[2][1], gl_WorldToObjectEXT[3][1]);\n"
                 << "        ob.values[cellIdx].isecWorldToObjectEXT[2] = vec4(gl_WorldToObjectEXT[0][2], "
                    "gl_WorldToObjectEXT[1][2], gl_WorldToObjectEXT[2][2], gl_WorldToObjectEXT[3][2]);\n"
                 << "        ob.values[cellIdx].isecWorldToObject3x4EXT[0] = vec4(gl_WorldToObject3x4EXT[0][0], "
                    "gl_WorldToObject3x4EXT[1][0], gl_WorldToObject3x4EXT[2][0], 0.0);\n"
                 << "        ob.values[cellIdx].isecWorldToObject3x4EXT[1] = vec4(gl_WorldToObject3x4EXT[0][1], "
                    "gl_WorldToObject3x4EXT[1][1], gl_WorldToObject3x4EXT[2][1], 0.0);\n"
                 << "        ob.values[cellIdx].isecWorldToObject3x4EXT[2] = vec4(gl_WorldToObject3x4EXT[0][2], "
                    "gl_WorldToObject3x4EXT[1][2], gl_WorldToObject3x4EXT[2][2], 0.0);\n"
                 << "        ob.values[cellIdx].isecWorldToObject3x4EXT[3] = vec4(gl_WorldToObject3x4EXT[0][3], "
                    "gl_WorldToObject3x4EXT[1][3], gl_WorldToObject3x4EXT[2][3], 0.0);\n"
                 << "\n"
                 << "        const float valueOffset = " << getIsecIndexOffset(isecIdx) << ";\n"
                 << "        hitAttrib = vec2(valueOffset, valueOffset);\n"
                 << "        ob.values[cellIdx].isecAttribute = vec4(hitAttrib, 0.0, 0.0);\n"
                 << (withSRB ? "        ob.values[cellIdx].isecSRB = srb.data;\n" : "")
                 << "        const float hitT = " << BottomLevelASParams::kBaseZ
                 << " / ib.params[cellIdx].zDirection;\n"
                 << "        reportIntersectionEXT(hitT, 0u);\n"
                 << "    }\n"
                 << "}\n";
            const auto shaderName = std::string("isec") + std::to_string(isecIdx) + suffix;
            programCollection.glslSources.add(shaderName) << glu::IntersectionSource(isec.str()) << buildOptions;
        }

    // Callable shader 0, at the top of the stack and storing the built-ins.
    for (uint32_t srbIdx = 0u; srbIdx < kSBTCount; ++srbIdx)
    {
        const bool withSRB = (srbIdx > 0u);
        const auto suffix  = (withSRB ? "-srb" : "");

        std::ostringstream call;
        call << "#version 460 core\n"
             << "#extension GL_EXT_debug_printf : enable\n"
             << "#extension GL_EXT_ray_tracing : require\n"
             << descDecl << (withSRB ? shaderRecordDecl : "") << cellIdxFuncDecl
             << "layout(location = 1) callableDataInEXT vec4 callData;\n"
             << "void main (void) {\n"
             << "    const uint cellIdx = getCellIndex(false);\n"
             << "\n"
             << "    ob.values[cellIdx].callLaunchIDEXT = uvec4(gl_LaunchIDEXT.xyz, 0u);\n"
             << "    ob.values[cellIdx].callLaunchSizeEXT = uvec4(gl_LaunchSizeEXT.xyz, 0u);\n"
             << "\n"
             << "    const float valueOffset = " << getCallIndexOffset(0u) << ";\n"
             << "    const vec4 vecOffset = vec4(valueOffset, valueOffset, valueOffset, valueOffset);\n"
             << "    callData = callData + vecOffset;\n"
             << (withSRB ? "    ob.values[cellIdx].call0SRB = srb.data;\n" : "") << "}\n";
        const auto shaderName = std::string("call0") + suffix;
        programCollection.glslSources.add(shaderName) << glu::CallableSource(call.str()) << buildOptions;
    }

    // Callable shader 1, intermediary.
    for (uint32_t srbIdx = 0u; srbIdx < kSBTCount; ++srbIdx)
    {
        const bool withSRB = (srbIdx > 0u);
        const auto suffix  = (withSRB ? "-srb" : "");

        std::ostringstream call;
        call << "#version 460 core\n"
             << "#extension GL_EXT_debug_printf : enable\n"
             << "#extension GL_EXT_ray_tracing : require\n"
             << descDecl << (withSRB ? shaderRecordDecl : "") << cellIdxFuncDecl
             << "layout(location = 0) callableDataInEXT vec4 callDataIn;\n"
             << "layout(location = 1) callableDataEXT vec4 callData;\n"
             << "void main (void) {\n"
             << "    const uint cellIdx = getCellIndex(false);\n"
             << "\n"
             << "    const float valueOffset = " << getCallIndexOffset(1u) << ";\n"
             << "    const vec4 vecOffset = vec4(valueOffset, valueOffset, valueOffset, valueOffset);\n"
             << "    callData = callDataIn + vecOffset;\n"
             << "    executeCallableEXT(0, 1); // Callable shader 0, callable data 1\n"
             << "    callDataIn = callData;\n"
             << (withSRB ? "    ob.values[cellIdx].call1SRB = srb.data;\n" : "") << "}\n";
        const auto shaderName = std::string("call1") + suffix;
        programCollection.glslSources.add(shaderName) << glu::CallableSource(call.str()) << buildOptions;
    }
}

TestInstance *RayTracingCase::createInstance(Context &context) const
{
    return new RayTracingInstance(context, m_params);
}

using BufferWithMemoryPtr = de::MovePtr<BufferWithMemory>;

struct SBTSet
{
    uint32_t shaderGroupHandleSize;
    uint32_t srbSize;

    BufferWithMemoryPtr rgenSBT;
    BufferWithMemoryPtr missSBT;
    BufferWithMemoryPtr hitsSBT;
    BufferWithMemoryPtr callSBT;

    void setRgenSRB(const tcu::Vec4 &data) const;
    void setMissSRB(uint32_t index, const tcu::Vec4 &data) const;
    void setCallSRB(uint32_t index, const tcu::Vec4 &data) const;
    void setHitsSRB(uint32_t index, const tcu::Vec4 &data) const;

    const tcu::Vec4 &getRgenSRB() const;
    const tcu::Vec4 &getMissSRB(uint32_t index) const;
    const tcu::Vec4 &getCallSRB(uint32_t index) const;
    const tcu::Vec4 &getHitsSRB(uint32_t index) const;

    uint32_t getStride(void) const;

protected:
    char *getDataPtr(const BufferWithMemory &buffer, uint32_t index) const;
    void storeDataAt(const BufferWithMemory &buffer, uint32_t index, const tcu::Vec4 &data) const;
    const tcu::Vec4 &getDataAt(const BufferWithMemory &buffer, uint32_t index) const;
};

char *SBTSet::getDataPtr(const BufferWithMemory &buffer, uint32_t index) const
{
    DE_ASSERT(srbSize > 0u);

    const uint32_t stride = shaderGroupHandleSize + srbSize;
    const uint32_t offset = index * stride + shaderGroupHandleSize;
    char *bufferData      = reinterpret_cast<char *>(buffer.getAllocation().getHostPtr());
    return bufferData + offset;
}

void SBTSet::storeDataAt(const BufferWithMemory &buffer, uint32_t index, const tcu::Vec4 &data) const
{
    char *bufferData = getDataPtr(buffer, index);
    deMemcpy(bufferData, &data, sizeof(data));
}

const tcu::Vec4 &SBTSet::getDataAt(const BufferWithMemory &buffer, uint32_t index) const
{
    const char *bufferData  = getDataPtr(buffer, index);
    const tcu::Vec4 *retPtr = reinterpret_cast<const tcu::Vec4 *>(bufferData);
    return *retPtr;
}

void SBTSet::setRgenSRB(const tcu::Vec4 &data) const
{
    storeDataAt(*rgenSBT, 0u, data);
}

void SBTSet::setMissSRB(uint32_t index, const tcu::Vec4 &data) const
{
    storeDataAt(*missSBT, index, data);
}

void SBTSet::setCallSRB(uint32_t index, const tcu::Vec4 &data) const
{
    storeDataAt(*callSBT, index, data);
}

void SBTSet::setHitsSRB(uint32_t index, const tcu::Vec4 &data) const
{
    storeDataAt(*hitsSBT, index, data);
}

const tcu::Vec4 &SBTSet::getRgenSRB() const
{
    return getDataAt(*rgenSBT, 0u);
}

const tcu::Vec4 &SBTSet::getMissSRB(uint32_t index) const
{
    return getDataAt(*missSBT, index);
}

const tcu::Vec4 &SBTSet::getCallSRB(uint32_t index) const
{
    return getDataAt(*callSBT, index);
}

const tcu::Vec4 &SBTSet::getHitsSRB(uint32_t index) const
{
    return getDataAt(*hitsSBT, index);
}

uint32_t SBTSet::getStride(void) const
{
    return (shaderGroupHandleSize + srbSize);
}

struct ShaderSet
{
    uint32_t baseGroupIndex;
    VkShaderModule rgen;
    VkShaderModule miss0;
    VkShaderModule miss1;
    VkShaderModule call0;
    VkShaderModule call1;
    VkShaderModule chit0;
    VkShaderModule chit1;
    VkShaderModule isec0;
    VkShaderModule isec1;
};

tcu::Vec4 genSRBData(de::Random &rnd)
{
    static const int minVal = 0;
    static const int maxVal = 9;

    tcu::Vec4 data(static_cast<float>(rnd.getInt(minVal, maxVal)), static_cast<float>(rnd.getInt(minVal, maxVal)),
                   static_cast<float>(rnd.getInt(minVal, maxVal)), static_cast<float>(rnd.getInt(minVal, maxVal)));

    return data;
}

bool floatEqual(const tcu::Vec4 &a, const tcu::Vec4 &b)
{
    static const tcu::Vec4 thresholdVec(kFloatThreshold, kFloatThreshold, kFloatThreshold, kFloatThreshold);

    const auto diffs   = tcu::absDiff(a, b);
    const auto inRange = tcu::lessThan(diffs, thresholdVec);
    return tcu::boolAll(inRange);
}

bool floatEqual(float a, float b)
{
    const float diff = std::abs(a - b);
    return (diff < kFloatThreshold);
}

tcu::TestStatus RayTracingInstance::iterate(void)
{
    const auto ctx     = m_context.getContextCommonData();
    const auto qfIndex = (m_params.computeQueue ? m_context.getComputeQueueFamilyIndex() : ctx.qfIndex);
    const auto queue   = (m_params.computeQueue ? m_context.getComputeQueue() : ctx.queue);

    const CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    de::Random rnd(m_params.getRandomSeed());
    beginCommandBuffer(ctx.vkd, cmdBuffer);

    // Bottom level AS and their parameters.
    std::vector<BottomLevelASParams> blasParams;
    std::vector<BLASPtr> blas;

    blasParams.reserve(kBLASCount);
    blas.reserve(kBLASCount);

    for (uint32_t i = 0u; i < kBLASCount; ++i)
    {
        blasParams.emplace_back(rnd);
        blas.emplace_back(makeBottomLevelASWithParams(blasParams.back()));
        blas.back()->createAndBuild(ctx.vkd, ctx.device, cmdBuffer, ctx.allocator);
    }

    // Top level acceleration structure using instances of the previous BLASes.
    const uint32_t cellCount = kWidth * kHeight;
    std::vector<CellParams> cellParams;
    cellParams.reserve(cellCount);

    for (uint32_t y = 0u; y < kHeight; ++y)
        for (uint32_t x = 0u; x < kWidth; ++x)
        {
            cellParams.emplace_back(x, y, rnd);
            auto &cp            = cellParams.back();
            cp.closestPrimitive = blasParams.at(cp.blasIndex).closestPrimitive;
        }

    auto topLevelAS = makeTopLevelASWithParams(blas, cellParams);
    topLevelAS->createAndBuild(ctx.vkd, ctx.device, cmdBuffer, ctx.allocator);

    // Input and output buffer.
    std::vector<CellOutput> cellOutputs(cellCount);

    const auto inputBufferSize = static_cast<VkDeviceSize>(de::dataSize(cellParams));
    const auto inputBufferInfo = makeBufferCreateInfo(inputBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory inputBuffer(ctx.vkd, ctx.device, ctx.allocator, inputBufferInfo, MemoryRequirement::HostVisible);
    auto &inputBufferAlloc = inputBuffer.getAllocation();
    void *inputBufferPtr   = inputBufferAlloc.getHostPtr();
    deMemcpy(inputBufferPtr, de::dataOrNull(cellParams), de::dataSize(cellParams));

    const auto outputBufferSize = static_cast<VkDeviceSize>(de::dataSize(cellOutputs));
    const auto outputBufferInfo = makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory outputBuffer(ctx.vkd, ctx.device, ctx.allocator, outputBufferInfo, MemoryRequirement::HostVisible);
    auto &outputBufferAlloc = outputBuffer.getAllocation();
    void *outputBufferPtr   = outputBufferAlloc.getHostPtr();
    deMemset(outputBufferPtr, 0, de::dataSize(cellOutputs));

    // Descriptor pool and set.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u /*input and output buffers*/);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, kStageFlags);
    setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, kStageFlags);
    setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, kStageFlags);
    const auto setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);

    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

    const auto pcSize         = DE_SIZEOF32(uint32_t);
    const auto pcRange        = makePushConstantRange(kStageFlags, 0u, pcSize);
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

    DescriptorSetUpdateBuilder setUpdateBuilder;
    {
        using Location                                            = DescriptorSetUpdateBuilder::Location;
        const VkWriteDescriptorSetAccelerationStructureKHR asDesc = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR, // VkStructureType                       sType;
            nullptr,                    // const void*                           pNext;
            1u,                         // uint32_t                          accelerationStructureCount;
            topLevelAS.get()->getPtr(), // const VkAccelerationStructureKHR* pAccelerationStructures;
        };
        const auto inputBufferDescInfo  = makeDescriptorBufferInfo(inputBuffer.get(), 0ull, VK_WHOLE_SIZE);
        const auto outputBufferDescInfo = makeDescriptorBufferInfo(outputBuffer.get(), 0ull, VK_WHOLE_SIZE);

        setUpdateBuilder.writeSingle(*descriptorSet, Location::binding(0u),
                                     VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &asDesc);
        setUpdateBuilder.writeSingle(*descriptorSet, Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                     &inputBufferDescInfo);
        setUpdateBuilder.writeSingle(*descriptorSet, Location::binding(2u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                     &outputBufferDescInfo);
    }
    setUpdateBuilder.update(ctx.vkd, ctx.device);

    // Create indirect commands layout.
    VkIndirectCommandsLayoutUsageFlagsEXT cmdsLayoutFlags = 0u;
    if (m_params.preprocess)
        cmdsLayoutFlags |= VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_EXT;
    if (m_params.unordered)
        cmdsLayoutFlags |= VK_INDIRECT_COMMANDS_LAYOUT_USAGE_UNORDERED_SEQUENCES_BIT_EXT;
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(cmdsLayoutFlags, kStageFlags, *pipelineLayout);
    if (m_params.useExecutionSet)
        cmdsLayoutBuilder.addExecutionSetToken(cmdsLayoutBuilder.getStreamRange(),
                                               VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT, kStageFlags);
    cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(), pcRange);
    cmdsLayoutBuilder.addTraceRays2Token(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // Shaders.
    const auto &binaries = m_context.getBinaryCollection();

    const auto rgenMod    = createShaderModule(ctx.vkd, ctx.device, binaries.get("rgen"));
    const auto rgenSRBMod = createShaderModule(ctx.vkd, ctx.device, binaries.get("rgen-srb"));

    const auto miss0Mod    = createShaderModule(ctx.vkd, ctx.device, binaries.get("miss0"));
    const auto miss1Mod    = createShaderModule(ctx.vkd, ctx.device, binaries.get("miss1"));
    const auto miss0SRBMod = createShaderModule(ctx.vkd, ctx.device, binaries.get("miss0-srb"));
    const auto miss1SRBMod = createShaderModule(ctx.vkd, ctx.device, binaries.get("miss1-srb"));

    const auto chit0Mod    = createShaderModule(ctx.vkd, ctx.device, binaries.get("chit0"));
    const auto chit1Mod    = createShaderModule(ctx.vkd, ctx.device, binaries.get("chit1"));
    const auto chit0SRBMod = createShaderModule(ctx.vkd, ctx.device, binaries.get("chit0-srb"));
    const auto chit1SRBMod = createShaderModule(ctx.vkd, ctx.device, binaries.get("chit1-srb"));

    const auto isec0Mod    = createShaderModule(ctx.vkd, ctx.device, binaries.get("isec0"));
    const auto isec1Mod    = createShaderModule(ctx.vkd, ctx.device, binaries.get("isec1"));
    const auto isec0SRBMod = createShaderModule(ctx.vkd, ctx.device, binaries.get("isec0-srb"));
    const auto isec1SRBMod = createShaderModule(ctx.vkd, ctx.device, binaries.get("isec1-srb"));

    const auto call0Mod    = createShaderModule(ctx.vkd, ctx.device, binaries.get("call0"));
    const auto call1Mod    = createShaderModule(ctx.vkd, ctx.device, binaries.get("call1"));
    const auto call0SRBMod = createShaderModule(ctx.vkd, ctx.device, binaries.get("call0-srb"));
    const auto call1SRBMod = createShaderModule(ctx.vkd, ctx.device, binaries.get("call1-srb"));

    const auto rayTracingPropertiesKHR   = makeRayTracingProperties(ctx.vki, ctx.physicalDevice);
    const auto &shaderGroupHandleSize    = rayTracingPropertiesKHR->getShaderGroupHandleSize();
    const auto &shaderGroupBaseAlignment = rayTracingPropertiesKHR->getShaderGroupBaseAlignment();

    // SBTs. We need 2 because we'll divide shaders by the absence or presence of the SRBs.
    std::vector<SBTSet> sbts(kSBTCount);

    const bool multiplePipelines = (m_params.useExecutionSet);
    const auto pipelineCount     = (multiplePipelines ? 2u : 1u);

    using RTPipelinePtr = de::MovePtr<RayTracingPipeline>;
    std::vector<RTPipelinePtr> rayTracingPipelines;
    std::vector<Move<VkPipeline>> pipelines;

    // These are higher than what will be used.
    const auto recursionDepth = 5u;
    const auto size2Vec4      = DE_SIZEOF32(tcu::Vec4) * 2u;

    rayTracingPipelines.reserve(pipelineCount);
    pipelines.reserve(pipelineCount);

    for (uint32_t i = 0u; i < pipelineCount; ++i)
    {
        rayTracingPipelines.emplace_back(de::newMovePtr<RayTracingPipeline>());
        auto &rtPipeline = rayTracingPipelines.back();
        rtPipeline->setCreateFlags2(VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT);
        rtPipeline->setMaxAttributeSize(size2Vec4);
        rtPipeline->setMaxPayloadSize(size2Vec4);
        rtPipeline->setMaxRecursionDepth(recursionDepth);
    }

    // Base shader group numbers.
    const uint32_t rgenGroup     = 0u; // Just one group.
    const uint32_t missGroupBase = 1u; // 2 groups for the rest.
    const uint32_t callGroupBase = 3u;
    const uint32_t hitsGroupBase = 5u;
    const uint32_t groupCount    = 7u;

    std::vector<ShaderSet> shaderSets;
    shaderSets.reserve(kSBTCount);

    shaderSets.push_back(ShaderSet{
        0u,
        *rgenMod,
        *miss0Mod,
        *miss1Mod,
        *call0Mod,
        *call1Mod,
        *chit0Mod,
        *chit1Mod,
        *isec0Mod,
        *isec1Mod,
    });
    shaderSets.push_back(ShaderSet{
        (multiplePipelines ? 0u : groupCount),
        *rgenSRBMod,
        *miss0SRBMod,
        *miss1SRBMod,
        *call0SRBMod,
        *call1SRBMod,
        *chit0SRBMod,
        *chit1SRBMod,
        *isec0SRBMod,
        *isec1SRBMod,
    });

    for (uint32_t i = 0u; i < kSBTCount; ++i)
    {
        const auto pipelineIdx = (multiplePipelines ? i : 0u);
        auto &rtPipeline       = rayTracingPipelines.at(pipelineIdx);

        const auto &shaderSet = shaderSets.at(i);

        rtPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, shaderSet.rgen, shaderSet.baseGroupIndex + rgenGroup);

        rtPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, shaderSet.miss0,
                              shaderSet.baseGroupIndex + missGroupBase + 0u);
        rtPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, shaderSet.miss1,
                              shaderSet.baseGroupIndex + missGroupBase + 1u);

        rtPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR, shaderSet.call0,
                              shaderSet.baseGroupIndex + callGroupBase + 0u);
        rtPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR, shaderSet.call1,
                              shaderSet.baseGroupIndex + callGroupBase + 1u);

        rtPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, shaderSet.chit0,
                              shaderSet.baseGroupIndex + hitsGroupBase + 0u);
        rtPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, shaderSet.isec0,
                              shaderSet.baseGroupIndex + hitsGroupBase + 0u);

        rtPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, shaderSet.chit1,
                              shaderSet.baseGroupIndex + hitsGroupBase + 1u);
        rtPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, shaderSet.isec1,
                              shaderSet.baseGroupIndex + hitsGroupBase + 1u);
    }

    for (uint32_t i = 0u; i < pipelineCount; ++i)
        pipelines.emplace_back(rayTracingPipelines.at(i)->createPipeline(ctx.vkd, ctx.device, *pipelineLayout));

    // Indirect execution set if used.
    VkIndirectExecutionSetEXT iesHandle = VK_NULL_HANDLE;
    ExecutionSetManagerPtr iesManager;
    if (m_params.useExecutionSet)
    {
        // Note we insert the back pipeline at index 0, but we'll overwrite both entries.
        iesManager = makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, *pipelines.back(), pipelineCount);
        for (uint32_t i = 0u; i < pipelineCount; ++i)
            iesManager->addPipeline(i, *pipelines.at(i));
        iesManager->update();
        iesHandle = iesManager->get();
    }

    for (uint32_t i = 0u; i < kSBTCount; ++i)
    {
        const auto withSRB     = (i > 0u);
        const auto srbSize     = (withSRB ? shaderGroupHandleSize : 0u);
        const auto pipelineIdx = (multiplePipelines ? i : 0u);

        auto &rtPipeline    = rayTracingPipelines.at(pipelineIdx);
        const auto pipeline = pipelines.at(pipelineIdx).get();

        auto &sbt = sbts.at(i);

        sbt.shaderGroupHandleSize = shaderGroupHandleSize;
        sbt.srbSize               = srbSize;

        sbt.rgenSBT = rtPipeline->createShaderBindingTable(
            ctx.vkd, ctx.device, pipeline, ctx.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment,
            shaderSets.at(i).baseGroupIndex + rgenGroup, 1u, 0u, 0u, MemoryRequirement::Any, 0u, 0u, srbSize);

        sbt.missSBT = rtPipeline->createShaderBindingTable(
            ctx.vkd, ctx.device, pipeline, ctx.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment,
            shaderSets.at(i).baseGroupIndex + missGroupBase, 2u, 0u, 0u, MemoryRequirement::Any, 0u, 0u, srbSize);

        sbt.callSBT = rtPipeline->createShaderBindingTable(
            ctx.vkd, ctx.device, pipeline, ctx.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment,
            shaderSets.at(i).baseGroupIndex + callGroupBase, 2u, 0u, 0u, MemoryRequirement::Any, 0u, 0u, srbSize);

        sbt.hitsSBT = rtPipeline->createShaderBindingTable(
            ctx.vkd, ctx.device, pipeline, ctx.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment,
            shaderSets.at(i).baseGroupIndex + hitsGroupBase, 2u, 0u, 0u, MemoryRequirement::Any, 0u, 0u, srbSize);

        if (withSRB)
        {
            sbt.setRgenSRB(genSRBData(rnd));
            sbt.setMissSRB(0u, genSRBData(rnd));
            sbt.setMissSRB(1u, genSRBData(rnd));
            sbt.setCallSRB(0u, genSRBData(rnd));
            sbt.setCallSRB(1u, genSRBData(rnd));
            sbt.setHitsSRB(0u, genSRBData(rnd));
            sbt.setHitsSRB(1u, genSRBData(rnd));
        }
    }

    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipelineLayout, 0u, 1u,
                                  &descriptorSet.get(), 0u, nullptr);

    DE_ASSERT(kHeight % kSBTCount == 0u);

    // DGC buffer with device-generated commands.
    const auto dgcDataSize = kSBTCount * cmdsLayoutBuilder.getStreamStride();
    std::vector<uint32_t> dgcData;
    dgcData.reserve(dgcDataSize / DE_SIZEOF32(uint32_t));

    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, dgcDataSize);
    auto &dgcBufferAlloc      = dgcBuffer.getAllocation();
    void *dgcBufferPtr        = dgcBufferAlloc.getHostPtr();
    const auto dgcBaseAddress = dgcBuffer.getDeviceAddress();

    // Fill DGC data and copy it to the buffer.
    for (uint32_t i = 0u; i < kSBTCount; ++i)
    {
        if (m_params.useExecutionSet)
            dgcData.push_back(i);
        const uint32_t offsetY = i * kDispHeight;
        dgcData.push_back(offsetY);

        const auto pipelineIdx = (multiplePipelines ? i : 0u);
        auto &sbt              = sbts.at(i);

        const auto stride      = sbt.getStride();
        const auto twiceStride = stride * 2u; // Size for those SBTs with 2 entries (miss, call, hits).

//#define USE_NON_DGC_PATH 1
#undef USE_NON_DGC_PATH

#ifdef USE_NON_DGC_PATH
        // Non-DGC version.
        ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipelines.at(pipelineIdx));
#else
        // For DGC we need the initial shader state bound.
        // For the single pipeline case, this will also be the pipeline in use.
        if (i == 0u)
            ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipelines.at(pipelineIdx));
#endif

        const auto rgenAddress = getBufferDeviceAddress(ctx.vkd, ctx.device, sbt.rgenSBT->get(), 0u);
        const auto missAddress = getBufferDeviceAddress(ctx.vkd, ctx.device, sbt.missSBT->get(), 0u);
        const auto callAddress = getBufferDeviceAddress(ctx.vkd, ctx.device, sbt.callSBT->get(), 0u);
        const auto hitsAddress = getBufferDeviceAddress(ctx.vkd, ctx.device, sbt.hitsSBT->get(), 0u);

        const auto rgenRegion = makeStridedDeviceAddressRegionKHR(rgenAddress, stride, stride);
        const auto missRegion = makeStridedDeviceAddressRegionKHR(missAddress, stride, twiceStride);
        const auto callRegion = makeStridedDeviceAddressRegionKHR(callAddress, stride, twiceStride);
        const auto hitsRegion = makeStridedDeviceAddressRegionKHR(hitsAddress, stride, twiceStride);

        const VkTraceRaysIndirectCommand2KHR traceRaysCmd{
            rgenRegion.deviceAddress, //  VkDeviceAddress   raygenShaderRecordAddress;
            rgenRegion.size,          //  VkDeviceSize      raygenShaderRecordSize;
            missRegion.deviceAddress, //  VkDeviceAddress   missShaderBindingTableAddress;
            missRegion.size,          //  VkDeviceSize      missShaderBindingTableSize;
            missRegion.stride,        //  VkDeviceSize      missShaderBindingTableStride;
            hitsRegion.deviceAddress, //  VkDeviceAddress   hitShaderBindingTableAddress;
            hitsRegion.size,          //  VkDeviceSize      hitShaderBindingTableSize;
            hitsRegion.stride,        //  VkDeviceSize      hitShaderBindingTableStride;
            callRegion.deviceAddress, //  VkDeviceAddress   callableShaderBindingTableAddress;
            callRegion.size,          //  VkDeviceSize      callableShaderBindingTableSize;
            callRegion.stride,        //  VkDeviceSize      callableShaderBindingTableStride;
            kWidth,                   //  uint32_t          width;
            kDispHeight,              //  uint32_t          height;
            1u,                       //  uint32_t          depth;
        };

        // This is interesting for the non-DGC path below, so we can have indirect ray trace commands.
        // We pick the command offset before adding it to the dgcData vector.
        const auto cmdOffset = static_cast<uint32_t>(de::dataSize(dgcData));
        DE_UNREF(cmdOffset);

        pushBackElement(dgcData, traceRaysCmd);
#ifdef USE_NON_DGC_PATH
        // Non-DGC version.
        ctx.vkd.cmdPushConstants(cmdBuffer, *pipelineLayout, kStageFlags, 0u, pcSize, &offsetY);
        ctx.vkd.cmdTraceRaysIndirect2KHR(cmdBuffer, dgcBaseAddress + cmdOffset);
        //ctx.vkd.cmdTraceRaysKHR(cmdBuffer, &rgenRegion, &missRegion, &hitsRegion, &callRegion, kWidth, kDispHeight, 1u);
#endif
    }

    DE_ASSERT(dgcDataSize == de::dataSize(dgcData));
    deMemcpy(dgcBufferPtr, de::dataOrNull(dgcData), de::dataSize(dgcData));
    flushAlloc(ctx.vkd, ctx.device, dgcBufferAlloc);

    // Create preprocess buffer and execute commands.
    const auto fixedPipeline = (m_params.useExecutionSet ? VK_NULL_HANDLE : *pipelines.front());
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, iesHandle, *cmdsLayout, kSBTCount, 0u,
                                         fixedPipeline);

#ifndef USE_NON_DGC_PATH
    {
        DGCGenCmdsInfo cmdsInfo(kStageFlags, iesHandle, *cmdsLayout, dgcBaseAddress, dgcBuffer.getSize(),
                                preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(), kSBTCount, 0ull, 0u,
                                fixedPipeline);

        if (m_params.preprocess)
        {
            ctx.vkd.cmdPreprocessGeneratedCommandsEXT(cmdBuffer, &cmdsInfo.get(), cmdBuffer);
            preprocessToExecuteBarrierExt(ctx.vkd, cmdBuffer);
        }
        {
            const auto isPreprocessed = makeVkBool(m_params.preprocess);
            ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, isPreprocessed, &cmdsInfo.get());
        }
    }
#endif

    // Sync shader writes to host reads for the output buffer.
    {
        const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                 VK_PIPELINE_STAGE_HOST_BIT, &barrier);
    }

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, queue, cmdBuffer);
    //ctx.vkd.deviceWaitIdle(ctx.device); // For debugPrintf.

    invalidateAlloc(ctx.vkd, ctx.device, outputBuffer.getAllocation());
    deMemcpy(de::dataOrNull(cellOutputs), outputBuffer.getAllocation().getHostPtr(), de::dataSize(cellOutputs));

    // Verify cell outputs.
    bool fail = false;
    auto &log = m_context.getTestContext().getLog();

    for (uint32_t y = 0u; y < kHeight; ++y)
        for (uint32_t x = 0u; x < kWidth; ++x)
        {
            const auto cellIdx     = y * kWidth + x;
            const auto &cellOut    = cellOutputs.at(cellIdx);
            const auto &cellIn     = cellParams.at(cellIdx);
            const auto &blasInfo   = blasParams.at(cellIn.blasIndex);
            const auto isTriangles = (blasInfo.geometryType == BottomLevelASParams::kTriangles);
            const auto sbtIndex    = y / kDispHeight;
            const auto &sbt        = sbts.at(sbtIndex);
            const auto withSRB     = (sbtIndex > 0u);

            bool miss = false;
            if (cellIn.rayFlags != kRayFlagsNoneEXT)
            {

                if (isTriangles)
                {
                    // Front face is clockwise by default.
                    if ((cellIn.rayFlags & kRayFlagsCullBackFacingTrianglesEXT) != 0u &&
                        blasInfo.windingDirection == BottomLevelASParams::kCounterClockwise)
                        miss = true;
                    else if ((cellIn.rayFlags & kRayFlagsCullFrontFacingTrianglesEXT) != 0u &&
                             blasInfo.windingDirection == BottomLevelASParams::kClockwise)
                        miss = true;
                }
                if ((cellIn.rayFlags & kRayFlagsCullOpaqueEXT) != 0u)
                    miss = true;
            }

            const auto launchID            = tcu::UVec4(x, y % kDispHeight, 0u, 0u);
            const auto launchSize          = tcu::UVec4(kWidth, kDispHeight, 1u, 0u);
            const auto rgenInitialPayload  = launchID.asFloat();
            const auto &origin             = cellIn.origin;
            const auto direction           = tcu::Vec4(0.0f, 0.0f, cellIn.zDirection, 0.0f);
            const auto primitiveID         = static_cast<int32_t>(blasInfo.closestPrimitive);
            const auto instanceID          = static_cast<int32_t>(cellIdx);
            const auto instanceCustomIndex = static_cast<int32_t>(cellIn.instanceCustomIndex);
            const auto geometryIndex       = static_cast<int32_t>(blasInfo.activeGeometryIndex);
            const auto objectRayOrigin     = tcu::Vec4(0.5f, 0.5f, 0.0, 1.0f);

            if (cellOut.rgenLaunchIDEXT != launchID)
            {
                log << tcu::TestLog::Message << "Bad rgenLaunchIDEXT at (" << x << ", " << y << "): expected "
                    << launchID << " found " << cellOut.rgenLaunchIDEXT << tcu::TestLog::EndMessage;
                fail = true;
            }

            if (cellOut.rgenLaunchSizeEXT != launchSize)
            {
                log << tcu::TestLog::Message << "Bad rgenLaunchSizeEXT at (" << x << ", " << y << "): expected "
                    << launchSize << " found " << cellOut.rgenLaunchSizeEXT << tcu::TestLog::EndMessage;
                fail = true;
            }

            if (cellOut.rgenInitialPayload != rgenInitialPayload)
            {
                log << tcu::TestLog::Message << "Bad rgenInitialPayload at (" << x << ", " << y << "): expected "
                    << rgenInitialPayload << " found " << cellOut.rgenInitialPayload << tcu::TestLog::EndMessage;
                fail = true;
            }

            if (withSRB)
            {
                const auto srb = sbt.getRgenSRB();
                if (cellOut.rgenSRB != srb)
                {
                    log << tcu::TestLog::Message << "Bad rgenSRB at (" << x << ", " << y << "): expected " << srb
                        << " found " << cellOut.rgenSRB << tcu::TestLog::EndMessage;
                    fail = true;
                }
            }

            tcu::Vec4 payload = rgenInitialPayload;

            if (miss)
            {
                const auto missOffset = static_cast<float>(getMissIndexOffset(cellIn.missIndex));
                const tcu::Vec4 missVecOffset(missOffset, missOffset, missOffset, missOffset);
                payload += missVecOffset;

                // Miss payload verification.
                if (cellOut.missIncomingPayload != rgenInitialPayload)
                {
                    log << tcu::TestLog::Message << "Bad missIncomingPayload at (" << x << ", " << y << "): expected "
                        << rgenInitialPayload << " found " << cellOut.missIncomingPayload << tcu::TestLog::EndMessage;
                    fail = true;
                }
                if (cellOut.missPayload != payload)
                {
                    log << tcu::TestLog::Message << "Bad missPayload at (" << x << ", " << y << "): expected "
                        << payload << " found " << cellOut.missPayload << tcu::TestLog::EndMessage;
                    fail = true;
                }

                if (cellOut.missLaunchIDEXT != launchID)
                {
                    log << tcu::TestLog::Message << "Bad missLaunchIDEXT at (" << x << ", " << y << "): expected "
                        << launchID << " found " << cellOut.missLaunchIDEXT << tcu::TestLog::EndMessage;
                    fail = true;
                }
                if (cellOut.missLaunchSizeEXT != launchSize)
                {
                    log << tcu::TestLog::Message << "Bad missLaunchSizeEXT at (" << x << ", " << y << "): expected "
                        << launchSize << " found " << cellOut.missLaunchSizeEXT << tcu::TestLog::EndMessage;
                    fail = true;
                }
                if (cellOut.missWorldRayOriginEXT != origin)
                {
                    log << tcu::TestLog::Message << "Bad missWorldRayOriginEXT at (" << x << ", " << y << "): expected "
                        << origin << " found " << cellOut.missWorldRayOriginEXT << tcu::TestLog::EndMessage;
                    fail = true;
                }
                if (cellOut.missWorldRayDirectionEXT != direction)
                {
                    log << tcu::TestLog::Message << "Bad missWorldRayDirectionEXT at (" << x << ", " << y
                        << "): expected " << direction << " found " << cellOut.missWorldRayDirectionEXT
                        << tcu::TestLog::EndMessage;
                    fail = true;
                }
                if (cellOut.missRayTminEXT != cellIn.minT)
                {
                    log << tcu::TestLog::Message << "Bad missRayTminEXT at (" << x << ", " << y << "): expected "
                        << cellIn.minT << " found " << cellOut.missRayTminEXT << tcu::TestLog::EndMessage;
                    fail = true;
                }
                if (cellOut.missRayTmaxEXT != cellIn.maxT)
                {
                    log << tcu::TestLog::Message << "Bad missRayTmaxEXT at (" << x << ", " << y << "): expected "
                        << cellIn.maxT << " found " << cellOut.missRayTmaxEXT << tcu::TestLog::EndMessage;
                    fail = true;
                }
                if (cellOut.missIncomingRayFlagsEXT != cellIn.rayFlags)
                {
                    log << tcu::TestLog::Message << "Bad missIncomingRayFlagsEXT at (" << x << ", " << y
                        << "): expected " << cellIn.rayFlags << " found " << cellOut.missIncomingRayFlagsEXT
                        << tcu::TestLog::EndMessage;
                    fail = true;
                }

                if (withSRB)
                {
                    const auto srb = sbt.getMissSRB(cellIn.missIndex);
                    if (cellOut.missSRB != srb)
                    {
                        log << tcu::TestLog::Message << "Bad missSRB at (" << x << ", " << y << "): expected " << srb
                            << " found " << cellOut.missSRB << tcu::TestLog::EndMessage;
                        fail = true;
                    }
                }
            }
            else
            {
                const auto isecOffset  = static_cast<float>(getIsecIndexOffset(blasInfo.activeGeometryIndex));
                const auto chitOffset  = static_cast<float>(getChitIndexOffset(blasInfo.activeGeometryIndex));
                const auto call0Offset = static_cast<float>(getCallIndexOffset(0u));
                const auto call1Offset = static_cast<float>(getCallIndexOffset(1u));

                const tcu::Vec4 chitVecOffset(chitOffset, chitOffset, chitOffset, chitOffset);
                const tcu::Vec4 call0VecOffset(call0Offset, call0Offset, call0Offset, call0Offset);
                const tcu::Vec4 call1VecOffset(call1Offset, call1Offset, call1Offset, call1Offset);

                const auto chitIncomingPayload = payload;

                payload += call0VecOffset;
                payload += call1VecOffset;
                payload += chitVecOffset;

                const tcu::Vec4 hitAttribute(isecOffset, isecOffset, 0.0f, 0.0f);

                const float tMaxAtIsec = BottomLevelASParams::kBaseZ / cellIn.zDirection;
                uint32_t hitKind       = 0u;

                if (blasInfo.geometryType == BottomLevelASParams::kTriangles)
                {
                    hitKind = ((blasInfo.windingDirection == BottomLevelASParams::kClockwise) ?
                                   kHitKindFrontFacingTriangleEXT :
                                   kHitKindBackFacingTriangleEXT);
                }

                if (blasInfo.geometryType == BottomLevelASParams::kAABBs)
                {
                    // Intersection shader.
                    if (cellOut.isecLaunchIDEXT != launchID)
                    {
                        log << tcu::TestLog::Message << "Bad isecLaunchIDEXT at (" << x << ", " << y << "): expected "
                            << launchID << " found " << cellOut.isecLaunchIDEXT << tcu::TestLog::EndMessage;
                        fail = true;
                    }
                    if (cellOut.isecLaunchSizeEXT != launchSize)
                    {
                        log << tcu::TestLog::Message << "Bad isecLaunchSizeEXT at (" << x << ", " << y << "): expected "
                            << launchSize << " found " << cellOut.isecLaunchSizeEXT << tcu::TestLog::EndMessage;
                        fail = true;
                    }

                    if (cellOut.isecPrimitiveID != primitiveID)
                    {
                        log << tcu::TestLog::Message << "Bad isecPrimitiveID at (" << x << ", " << y << "): expected "
                            << primitiveID << " found " << cellOut.isecPrimitiveID << tcu::TestLog::EndMessage;
                        fail = true;
                    }
                    if (cellOut.isecInstanceID != instanceID)
                    {
                        log << tcu::TestLog::Message << "Bad isecInstanceID at (" << x << ", " << y << "): expected "
                            << instanceID << " found " << cellOut.isecInstanceID << tcu::TestLog::EndMessage;
                        fail = true;
                    }
                    if (cellOut.isecInstanceCustomIndexEXT != instanceCustomIndex)
                    {
                        log << tcu::TestLog::Message << "Bad isecInstanceCustomIndexEXT at (" << x << ", " << y
                            << "): expected " << instanceCustomIndex << " found " << cellOut.isecInstanceCustomIndexEXT
                            << tcu::TestLog::EndMessage;
                        fail = true;
                    }
                    if (cellOut.isecGeometryIndexEXT != geometryIndex)
                    {
                        log << tcu::TestLog::Message << "Bad isecGeometryIndexEXT at (" << x << ", " << y
                            << "): expected " << geometryIndex << " found " << cellOut.isecGeometryIndexEXT
                            << tcu::TestLog::EndMessage;
                        fail = true;
                    }
                    if (cellOut.isecWorldRayOriginEXT != origin)
                    {
                        log << tcu::TestLog::Message << "Bad isecWorldRayOriginEXT at (" << x << ", " << y
                            << "): expected " << origin << " found " << cellOut.isecWorldRayOriginEXT
                            << tcu::TestLog::EndMessage;
                        fail = true;
                    }
                    if (cellOut.isecWorldRayDirectionEXT != direction)
                    {
                        log << tcu::TestLog::Message << "Bad isecWorldRayDirectionEXT at (" << x << ", " << y
                            << "): expected " << direction << " found " << cellOut.isecWorldRayDirectionEXT
                            << tcu::TestLog::EndMessage;
                        fail = true;
                    }
                    if (!floatEqual(cellOut.isecObjectRayOriginEXT, objectRayOrigin))
                    {
                        log << tcu::TestLog::Message << "Bad isecObjectRayOriginEXT at (" << x << ", " << y
                            << "): expected " << objectRayOrigin << " found " << cellOut.isecObjectRayOriginEXT
                            << tcu::TestLog::EndMessage;
                        fail = true;
                    }
                    if (!floatEqual(cellOut.isecObjectRayDirectionEXT, direction))
                    {
                        log << tcu::TestLog::Message << "Bad isecObjectRayDirectionEXT at (" << x << ", " << y
                            << "): expected " << direction << " found " << cellOut.isecObjectRayDirectionEXT
                            << tcu::TestLog::EndMessage;
                        fail = true;
                    }
                    if (cellOut.isecRayTminEXT != cellIn.minT)
                    {
                        log << tcu::TestLog::Message << "Bad isecRayTminEXT at (" << x << ", " << y << "): expected "
                            << cellIn.minT << " found " << cellOut.isecRayTminEXT << tcu::TestLog::EndMessage;
                        fail = true;
                    }
                    if (cellOut.isecRayTmaxEXT != cellIn.maxT)
                    {
                        log << tcu::TestLog::Message << "Bad isecRayTmaxEXT at (" << x << ", " << y << "): expected "
                            << cellIn.maxT << " found " << cellOut.isecRayTmaxEXT << tcu::TestLog::EndMessage;
                        fail = true;
                    }
                    if (cellOut.isecIncomingRayFlagsEXT != cellIn.rayFlags)
                    {
                        log << tcu::TestLog::Message << "Bad isecIncomingRayFlagsEXT at (" << x << ", " << y
                            << "): expected " << cellIn.rayFlags << " found " << cellOut.isecIncomingRayFlagsEXT
                            << tcu::TestLog::EndMessage;
                        fail = true;
                    }
                    for (uint32_t i = 0u; i < de::arrayLength(cellIn.transformMatrix.matrix); ++i)
                    {
                        const tcu::Vec4 row(cellIn.transformMatrix.matrix[i][0], cellIn.transformMatrix.matrix[i][1],
                                            cellIn.transformMatrix.matrix[i][2], cellIn.transformMatrix.matrix[i][3]);
                        if (!floatEqual(row, cellOut.isecObjectToWorldEXT[i]))
                        {
                            log << tcu::TestLog::Message << "Bad isecObjectToWorldEXT[" << i << "] at (" << x << ", "
                                << y << "): expected " << row << " found " << cellOut.isecObjectToWorldEXT[i]
                                << tcu::TestLog::EndMessage;
                            fail = true;
                        }
                    }
                    for (uint32_t i = 0u; i < de::arrayLength(cellIn.transformMatrix.matrix); ++i)
                    {
                        const tcu::Vec4 expected(
                            cellIn.transformMatrix.matrix[i][0], cellIn.transformMatrix.matrix[i][1],
                            cellIn.transformMatrix.matrix[i][2], cellIn.transformMatrix.matrix[i][3]);
                        const tcu::Vec4 result(
                            cellOut.isecObjectToWorld3x4EXT[0][i], cellOut.isecObjectToWorld3x4EXT[1][i],
                            cellOut.isecObjectToWorld3x4EXT[2][i], cellOut.isecObjectToWorld3x4EXT[3][i]);
                        if (!floatEqual(expected, result))
                        {
                            log << tcu::TestLog::Message << "Bad isecObjectToWorld3x4EXT[][" << i << "] at (" << x
                                << ", " << y << "): expected " << expected << " found " << result
                                << tcu::TestLog::EndMessage;
                            fail = true;
                        }
                    }
                    for (uint32_t i = 0u; i < de::arrayLength(cellIn.transformMatrix.matrix); ++i)
                    {
                        // Note W column is negative to undo the translation.
                        const tcu::Vec4 row(cellIn.transformMatrix.matrix[i][0], cellIn.transformMatrix.matrix[i][1],
                                            cellIn.transformMatrix.matrix[i][2], -cellIn.transformMatrix.matrix[i][3]);
                        if (!floatEqual(row, cellOut.isecWorldToObjectEXT[i]))
                        {
                            log << tcu::TestLog::Message << "Bad isecWorldToObjectEXT[" << i << "] at (" << x << ", "
                                << y << "): expected " << row << " found " << cellOut.isecWorldToObjectEXT[i]
                                << tcu::TestLog::EndMessage;
                            fail = true;
                        }
                    }
                    for (uint32_t i = 0u; i < de::arrayLength(cellIn.transformMatrix.matrix); ++i)
                    {
                        // Note W column is negative to undo the translation.
                        const tcu::Vec4 expected(
                            cellIn.transformMatrix.matrix[i][0], cellIn.transformMatrix.matrix[i][1],
                            cellIn.transformMatrix.matrix[i][2], -cellIn.transformMatrix.matrix[i][3]);
                        const tcu::Vec4 result(
                            cellOut.isecWorldToObject3x4EXT[0][i], cellOut.isecWorldToObject3x4EXT[1][i],
                            cellOut.isecWorldToObject3x4EXT[2][i], cellOut.isecWorldToObject3x4EXT[3][i]);
                        if (!floatEqual(expected, result))
                        {
                            log << tcu::TestLog::Message << "Bad isecWorldToObject3x4EXT[][" << i << "] at (" << x
                                << ", " << y << "): expected " << expected << " found " << result
                                << tcu::TestLog::EndMessage;
                            fail = true;
                        }
                    }

                    if (cellOut.isecAttribute != hitAttribute)
                    {
                        log << tcu::TestLog::Message << "Bad isecAttribute at (" << x << ", " << y << "): expected "
                            << hitAttribute << " found " << cellOut.isecAttribute << tcu::TestLog::EndMessage;
                        fail = true;
                    }
                    if (cellOut.chitAttribute != hitAttribute)
                    {
                        log << tcu::TestLog::Message << "Bad chitAttribute at (" << x << ", " << y << "): expected "
                            << hitAttribute << " found " << cellOut.chitAttribute << tcu::TestLog::EndMessage;
                        fail = true;
                    }

                    if (withSRB)
                    {
                        const auto srb = sbt.getHitsSRB(blasInfo.activeGeometryIndex);
                        if (cellOut.isecSRB != srb)
                        {
                            log << tcu::TestLog::Message << "Bad isecSRB at (" << x << ", " << y << "): expected "
                                << srb << " found " << cellOut.isecSRB << tcu::TestLog::EndMessage;
                            fail = true;
                        }
                    }
                }

                // Closest-hit shader.
                if (cellOut.chitLaunchIDEXT != launchID)
                {
                    log << tcu::TestLog::Message << "Bad chitLaunchIDEXT at (" << x << ", " << y << "): expected "
                        << launchID << " found " << cellOut.chitLaunchIDEXT << tcu::TestLog::EndMessage;
                    fail = true;
                }
                if (cellOut.chitLaunchSizeEXT != launchSize)
                {
                    log << tcu::TestLog::Message << "Bad chitLaunchSizeEXT at (" << x << ", " << y << "): expected "
                        << launchSize << " found " << cellOut.chitLaunchSizeEXT << tcu::TestLog::EndMessage;
                    fail = true;
                }

                if (cellOut.chitPrimitiveID != primitiveID)
                {
                    log << tcu::TestLog::Message << "Bad chitPrimitiveID at (" << x << ", " << y << "): expected "
                        << primitiveID << " found " << cellOut.chitPrimitiveID << tcu::TestLog::EndMessage;
                    fail = true;
                }
                if (cellOut.chitInstanceID != instanceID)
                {
                    log << tcu::TestLog::Message << "Bad chitInstanceID at (" << x << ", " << y << "): expected "
                        << instanceID << " found " << cellOut.chitInstanceID << tcu::TestLog::EndMessage;
                    fail = true;
                }
                if (cellOut.chitInstanceCustomIndexEXT != instanceCustomIndex)
                {
                    log << tcu::TestLog::Message << "Bad chitInstanceCustomIndexEXT at (" << x << ", " << y
                        << "): expected " << instanceCustomIndex << " found " << cellOut.chitInstanceCustomIndexEXT
                        << tcu::TestLog::EndMessage;
                    fail = true;
                }
                if (cellOut.chitGeometryIndexEXT != geometryIndex)
                {
                    log << tcu::TestLog::Message << "Bad chitGeometryIndexEXT at (" << x << ", " << y << "): expected "
                        << geometryIndex << " found " << cellOut.chitGeometryIndexEXT << tcu::TestLog::EndMessage;
                    fail = true;
                }
                if (cellOut.chitWorldRayOriginEXT != origin)
                {
                    log << tcu::TestLog::Message << "Bad chitWorldRayOriginEXT at (" << x << ", " << y << "): expected "
                        << origin << " found " << cellOut.chitWorldRayOriginEXT << tcu::TestLog::EndMessage;
                    fail = true;
                }
                if (cellOut.chitWorldRayDirectionEXT != direction)
                {
                    log << tcu::TestLog::Message << "Bad chitWorldRayDirectionEXT at (" << x << ", " << y
                        << "): expected " << direction << " found " << cellOut.chitWorldRayDirectionEXT
                        << tcu::TestLog::EndMessage;
                    fail = true;
                }
                if (!floatEqual(cellOut.chitObjectRayOriginEXT, objectRayOrigin))
                {
                    log << tcu::TestLog::Message << "Bad chitObjectRayOriginEXT at (" << x << ", " << y
                        << "): expected " << objectRayOrigin << " found " << cellOut.chitObjectRayOriginEXT
                        << tcu::TestLog::EndMessage;
                    fail = true;
                }
                if (!floatEqual(cellOut.chitObjectRayDirectionEXT, direction))
                {
                    log << tcu::TestLog::Message << "Bad chitObjectRayDirectionEXT at (" << x << ", " << y
                        << "): expected " << direction << " found " << cellOut.chitObjectRayDirectionEXT
                        << tcu::TestLog::EndMessage;
                    fail = true;
                }
                if (cellOut.chitRayTminEXT != cellIn.minT)
                {
                    log << tcu::TestLog::Message << "Bad chitRayTminEXT at (" << x << ", " << y << "): expected "
                        << cellIn.minT << " found " << cellOut.chitRayTminEXT << tcu::TestLog::EndMessage;
                    fail = true;
                }
                if (!floatEqual(cellOut.chitRayTmaxEXT, tMaxAtIsec))
                {
                    log << tcu::TestLog::Message << "Bad chitRayTmaxEXT at (" << x << ", " << y << "): expected "
                        << tMaxAtIsec << " found " << cellOut.chitRayTmaxEXT << tcu::TestLog::EndMessage;
                    fail = true;
                }
                if (cellOut.chitIncomingRayFlagsEXT != cellIn.rayFlags)
                {
                    log << tcu::TestLog::Message << "Bad chitIncomingRayFlagsEXT at (" << x << ", " << y
                        << "): expected " << cellIn.rayFlags << " found " << cellOut.chitIncomingRayFlagsEXT
                        << tcu::TestLog::EndMessage;
                    fail = true;
                }
                if (!floatEqual(cellOut.chitHitTEXT, tMaxAtIsec))
                {
                    log << tcu::TestLog::Message << "Bad chitHitTEXT at (" << x << ", " << y << "): expected "
                        << tMaxAtIsec << " found " << cellOut.chitHitTEXT << tcu::TestLog::EndMessage;
                    fail = true;
                }
                if (cellOut.chitHitKindEXT != hitKind)
                {
                    log << tcu::TestLog::Message << "Bad chitHitKindEXT at (" << x << ", " << y << "): expected "
                        << hitKind << " found " << cellOut.chitHitKindEXT << tcu::TestLog::EndMessage;
                    fail = true;
                }
                for (uint32_t i = 0u; i < de::arrayLength(cellIn.transformMatrix.matrix); ++i)
                {
                    const tcu::Vec4 row(cellIn.transformMatrix.matrix[i][0], cellIn.transformMatrix.matrix[i][1],
                                        cellIn.transformMatrix.matrix[i][2], cellIn.transformMatrix.matrix[i][3]);
                    if (!floatEqual(row, cellOut.chitObjectToWorldEXT[i]))
                    {
                        log << tcu::TestLog::Message << "Bad chitObjectToWorldEXT[" << i << "] at (" << x << ", " << y
                            << "): expected " << row << " found " << cellOut.chitObjectToWorldEXT[i]
                            << tcu::TestLog::EndMessage;
                        fail = true;
                    }
                }
                for (uint32_t i = 0u; i < de::arrayLength(cellIn.transformMatrix.matrix); ++i)
                {
                    const tcu::Vec4 expected(cellIn.transformMatrix.matrix[i][0], cellIn.transformMatrix.matrix[i][1],
                                             cellIn.transformMatrix.matrix[i][2], cellIn.transformMatrix.matrix[i][3]);
                    const tcu::Vec4 result(cellOut.chitObjectToWorld3x4EXT[0][i], cellOut.chitObjectToWorld3x4EXT[1][i],
                                           cellOut.chitObjectToWorld3x4EXT[2][i],
                                           cellOut.chitObjectToWorld3x4EXT[3][i]);
                    if (!floatEqual(expected, result))
                    {
                        log << tcu::TestLog::Message << "Bad chitObjectToWorld3x4EXT[][" << i << "] at (" << x << ", "
                            << y << "): expected " << expected << " found " << result << tcu::TestLog::EndMessage;
                        fail = true;
                    }
                }
                for (uint32_t i = 0u; i < de::arrayLength(cellIn.transformMatrix.matrix); ++i)
                {
                    // Note W column is negative to undo the translation.
                    const tcu::Vec4 row(cellIn.transformMatrix.matrix[i][0], cellIn.transformMatrix.matrix[i][1],
                                        cellIn.transformMatrix.matrix[i][2], -cellIn.transformMatrix.matrix[i][3]);
                    if (!floatEqual(row, cellOut.chitWorldToObjectEXT[i]))
                    {
                        log << tcu::TestLog::Message << "Bad chitWorldToObjectEXT[" << i << "] at (" << x << ", " << y
                            << "): expected " << row << " found " << cellOut.chitWorldToObjectEXT[i]
                            << tcu::TestLog::EndMessage;
                        fail = true;
                    }
                }
                for (uint32_t i = 0u; i < de::arrayLength(cellIn.transformMatrix.matrix); ++i)
                {
                    // Note W column is negative to undo the translation.
                    const tcu::Vec4 expected(cellIn.transformMatrix.matrix[i][0], cellIn.transformMatrix.matrix[i][1],
                                             cellIn.transformMatrix.matrix[i][2], -cellIn.transformMatrix.matrix[i][3]);
                    const tcu::Vec4 result(cellOut.chitWorldToObject3x4EXT[0][i], cellOut.chitWorldToObject3x4EXT[1][i],
                                           cellOut.chitWorldToObject3x4EXT[2][i],
                                           cellOut.chitWorldToObject3x4EXT[3][i]);
                    if (!floatEqual(expected, result))
                    {
                        log << tcu::TestLog::Message << "Bad chitWorldToObject3x4EXT[][" << i << "] at (" << x << ", "
                            << y << "): expected " << expected << " found " << result << tcu::TestLog::EndMessage;
                        fail = true;
                    }
                }

                if (withSRB)
                {
                    const auto srb = sbt.getHitsSRB(blasInfo.activeGeometryIndex);
                    if (cellOut.chitSRB != srb)
                    {
                        log << tcu::TestLog::Message << "Bad chitSRB at (" << x << ", " << y << "): expected " << srb
                            << " found " << cellOut.chitSRB << tcu::TestLog::EndMessage;
                        fail = true;
                    }
                }

                // Call shaders.
                if (cellOut.callLaunchIDEXT != launchID)
                {
                    log << tcu::TestLog::Message << "Bad callLaunchIDEXT at (" << x << ", " << y << "): expected "
                        << launchID << " found " << cellOut.callLaunchIDEXT << tcu::TestLog::EndMessage;
                    fail = true;
                }
                if (cellOut.callLaunchSizeEXT != launchSize)
                {
                    log << tcu::TestLog::Message << "Bad callLaunchSizeEXT at (" << x << ", " << y << "): expected "
                        << launchSize << " found " << cellOut.callLaunchSizeEXT << tcu::TestLog::EndMessage;
                    fail = true;
                }

                if (cellOut.chitIncomingPayload != chitIncomingPayload)
                {
                    log << tcu::TestLog::Message << "Bad chitIncomingPayload at (" << x << ", " << y << "): expected "
                        << chitIncomingPayload << " found " << cellOut.chitIncomingPayload << tcu::TestLog::EndMessage;
                    fail = true;
                }

                if (cellOut.chitPayload != payload)
                {
                    log << tcu::TestLog::Message << "Bad chitPayload at (" << x << ", " << y << "): expected "
                        << payload << " found " << cellOut.chitPayload << tcu::TestLog::EndMessage;
                    fail = true;
                }

                if (withSRB)
                {
                    const auto srb0 = sbt.getCallSRB(0u);
                    if (cellOut.call0SRB != srb0)
                    {
                        log << tcu::TestLog::Message << "Bad call0SRB at (" << x << ", " << y << "): expected " << srb0
                            << " found " << cellOut.call0SRB << tcu::TestLog::EndMessage;
                        fail = true;
                    }

                    const auto srb1 = sbt.getCallSRB(1u);
                    if (cellOut.call1SRB != srb1)
                    {
                        log << tcu::TestLog::Message << "Bad call1SRB at (" << x << ", " << y << "): expected " << srb1
                            << " found " << cellOut.call1SRB << tcu::TestLog::EndMessage;
                        fail = true;
                    }
                }
            }

            if (cellOut.rgenFinalPayload != payload)
            {
                log << tcu::TestLog::Message << "Bad rgenFinalPayload at (" << x << ", " << y << "): expected "
                    << payload << " found " << cellOut.rgenFinalPayload << tcu::TestLog::EndMessage;
                fail = true;
            }
        }

    if (fail)
        return tcu::TestStatus::fail("Fail; check log for details");
    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createDGCRayTracingTestsExt(tcu::TestContext &testCtx)
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;
    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "ray_tracing", ""));

    for (const bool useExecutionSet : {false, true})
        for (const bool preprocess : {false, true})
            for (const bool unordered : {false, true})
                for (const bool computeQueue : {false, true})
                {
                    const RayTracingInstance::Params params{useExecutionSet, preprocess, unordered, computeQueue};
                    const auto testName = std::string(useExecutionSet ? "with_execution_set" : "no_execution_set") +
                                          (preprocess ? "_preprocess" : "") + (unordered ? "_unordered" : "") +
                                          (computeQueue ? "_cq" : "");
                    mainGroup->addChild(new RayTracingCase(testCtx, testName, params));
                }

    return mainGroup.release();
}

} // namespace DGC
} // namespace vkt
