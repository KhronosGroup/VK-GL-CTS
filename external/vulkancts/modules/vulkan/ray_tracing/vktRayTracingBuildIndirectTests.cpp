/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief Ray Tracing Build Large Shader Set tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingBuildIndirectTests.hpp"

#include "vkDefs.hpp"

#include "vktTestCase.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkTypeUtil.hpp"

#include "vkRayTracingUtil.hpp"

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

const uint32_t HIT             = 1;
const uint32_t MISS            = 2;
const uint32_t SQUARE_SIZE     = 5; // in triangles, square's triangles count = squareSize * squareSize
const uint32_t SQUARE_OFFSET_X = 100;
const tcu::Vec3 PADDING_VERTEX = {-9999.9f, -9999.9f, -9999.9f};
const uint32_t VERTEX_STRIDE   = 12; // assumed vertexStride for R32G32B32 vertex format used in vkRayTracingUtil.cpp

bool isMissTriangle(uint32_t primId)
{
    // it is not %==0 to avoid firstVertex false-negatives
    return primId % 7 == 5;
}

enum ShaderGroups
{
    FIRST_GROUP  = 0,
    RAYGEN_GROUP = FIRST_GROUP,
    MISS_GROUP,
    HIT_GROUP,
    GROUP_COUNT
};

struct CaseDef
{
    uint32_t primitiveCount    = SQUARE_SIZE * SQUARE_SIZE;
    int32_t primitiveOffset    = 0;
    uint32_t firstVertex       = 0;
    int32_t transformOffset    = 0;
    uint32_t instancesCount    = 1;
    uint32_t maxInstancesCount = 1;
    int32_t instancesOffset    = 0;
    bool doUpdate              = false;

    static constexpr uint32_t width                = SQUARE_SIZE;
    static constexpr uint32_t height               = SQUARE_SIZE;
    static constexpr uint32_t depth                = 8;
    static constexpr uint32_t geometriesGroupCount = depth;
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

Move<VkPipeline> makePipeline(const DeviceInterface &vkd, const VkDevice device, vk::BinaryCollection &collection,
                              de::MovePtr<RayTracingPipeline> &rayTracingPipeline, VkPipelineLayout pipelineLayout,
                              const std::string &shaderName)
{
    Move<VkShaderModule> raygenShader = createShaderModule(vkd, device, collection.get(shaderName), 0);

    rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, raygenShader, 0);

    Move<VkPipeline> pipeline = rayTracingPipeline->createPipeline(vkd, device, pipelineLayout);

    return pipeline;
}

Move<VkPipeline> makePipeline(const DeviceInterface &vkd, const VkDevice device, vk::BinaryCollection &collection,
                              de::MovePtr<RayTracingPipeline> &rayTracingPipeline, VkPipelineLayout pipelineLayout,
                              const uint32_t raygenGroup, const uint32_t missGroup, const uint32_t hitGroup,
                              const vk::VkGeometryTypeKHR geometryType)
{
    Move<VkShaderModule> raygenShader = createShaderModule(vkd, device, collection.get("rgen"), 0);
    Move<VkShaderModule> hitShader    = createShaderModule(vkd, device, collection.get("chit"), 0);
    Move<VkShaderModule> missShader   = createShaderModule(vkd, device, collection.get("miss"), 0);

    rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, raygenShader, raygenGroup);
    rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, hitShader, hitGroup);
    rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, missShader, missGroup);

    if (geometryType == VK_GEOMETRY_TYPE_AABBS_KHR)
    {
        Move<VkShaderModule> intersectionShader = createShaderModule(vkd, device, collection.get("rint"), 0);
        rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, intersectionShader, hitGroup);
    }

    Move<VkPipeline> pipeline = rayTracingPipeline->createPipeline(vkd, device, pipelineLayout);

    return pipeline;
}

VkImageCreateInfo makeImageCreateInfo(uint32_t width, uint32_t height, uint32_t depth, VkFormat format)
{
    const VkImageUsageFlags usage =
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        (VkImageCreateFlags)0u,              // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_3D,                    // VkImageType imageType;
        format,                              // VkFormat format;
        makeExtent3D(width, height, depth),  // VkExtent3D extent;
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

void initProgramsHelper(SourceCollections &programCollection, const CaseDef &data)
{
    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
    {
        std::stringstream css;
        css << "#version 460 core\n"
               "#extension GL_EXT_ray_tracing : require\n"
               "layout(set = 0, binding = 0, std140) writeonly buffer OutBuf\n"
               "{\n"
               "  uvec4 accelerationStructureBuildOffsetInfoKHR["
            << data.depth
            << "];\n"
               "} b_out;\n"
               "\n"
               "void main()\n"
               "{\n"
               "  for (uint i = 0; i < "
            << data.depth
            << "; i++)\n"
               "  {\n"
               "    uint primitiveCount  = "
            << data.primitiveCount
            << "u;\n"
               "    uint primitiveOffset = "
            << data.primitiveOffset
            << "u;\n"
               "    uint firstVertex     = "
            << data.firstVertex
            << "u;\n"
               "    uint transformOffset = "
            << data.transformOffset
            << "u;\n"
               "\n"
               "    b_out.accelerationStructureBuildOffsetInfoKHR[i] = uvec4(\n"
               "      primitiveCount, primitiveOffset, firstVertex, transformOffset);\n"
               "  }\n"
               "}\n";

        programCollection.glslSources.add("wr-asb")
            << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
    }
    {
        std::stringstream css;
        css << "#version 460 core\n"
               "#extension GL_EXT_ray_tracing : require\n"
               "layout(set = 0, binding = 0, std140) writeonly buffer OutBuf\n"
               "{\n"
               "  uvec4 accelerationStructureBuildOffsetInfoKHR;\n"
               "} b_out;\n"
               "\n"
               "void main()\n"
               "{\n"
               "  uint primitiveCount  = "
            << data.instancesCount
            << "u;\n"
               "  uint primitiveOffset = "
            << data.instancesOffset
            << "u;\n"
               "  uint firstVertex     = "
            << 0
            << "u;\n"
               "  uint transformOffset = "
            << 0
            << "u;\n"
               "\n"
               "  b_out.accelerationStructureBuildOffsetInfoKHR = uvec4(\n"
               "    primitiveCount, primitiveOffset, firstVertex, transformOffset);\n"
               "}\n";

        programCollection.glslSources.add("wr-ast")
            << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
    }
    {
        std::stringstream css;
        css << "#version 460 core\n"
               "#extension GL_EXT_ray_tracing : require\n"
               "layout(location = 0) rayPayloadEXT vec3 hitValue;\n"
               "layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
               "\n"
               "void main()\n"
               "{\n"
               "  uint  rayFlags = 0;\n"
               "  uint  cullMask = 0xFF;\n"
               "  float tmin     = 0.0;\n"
               "  float tmax     = 9.0;\n"
               "  float x        = float(gl_LaunchIDEXT.x);\n"
               "  x              += float(gl_LaunchIDEXT.z) * float("
            << SQUARE_OFFSET_X
            << ") * 2.0f;\n"
               "  float y        = float(gl_LaunchIDEXT.y);\n"
               "  vec3  origin   = vec3(x, y, 0.5);\n"
               "  vec3  direct   = vec3(0.0, 0.0, -1.0);\n"
               "  traceRayEXT(topLevelAS, rayFlags, cullMask, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
               "}\n";

        programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
    }
    {
        std::stringstream css;
        css << "#version 460 core\n"
               "#extension GL_EXT_ray_tracing : require\n"
               "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
               "hitAttributeEXT vec3 attribs;\n"
               "layout(set = 0, binding = 0, r32ui) uniform uimage3D result;\n"
               "void main()\n"
               "{\n"
               "  uvec4 color = uvec4("
            << HIT
            << ",0,0,1);\n"
               "  imageStore(result, ivec3(gl_LaunchIDEXT.xyz), color);\n"
               "}\n";

        programCollection.glslSources.add("chit")
            << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
    }
    {
        std::stringstream css;
        css << "#version 460 core\n"
               "#extension GL_EXT_ray_tracing : require\n"
               "layout(location = 0) rayPayloadInEXT vec3 unusedPayload;\n"
               "layout(set = 0, binding = 0, r32ui) uniform uimage3D result;\n"
               "void main()\n"
               "{\n"
               "  uvec4 color = uvec4("
            << MISS
            << ",0,0,1);\n"
               "  imageStore(result, ivec3(gl_LaunchIDEXT.xyz), color);\n"
               "}\n";

        programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
    }
    {
        std::stringstream css;
        css << "#version 460 core\n"
               "#extension GL_EXT_ray_tracing : require\n"
               "hitAttributeEXT vec3 attribs;\n"
               "void main()\n"
               "{\n"
               "  reportIntersectionEXT(1.5, 0);\n"
               "}\n";

        programCollection.glslSources.add("rint")
            << glu::IntersectionSource(updateRayTracingGLSL(css.str())) << buildOptions;
    }
}

struct IndexedGeometryData
{
    std::vector<tcu::Vec3> vertices;
    std::vector<uint32_t> indices;
};

IndexedGeometryData makeTriangleGeometry(const tcu::Vec3 offset)
{
    IndexedGeometryData geoData;
    const auto kVertexCount = (SQUARE_SIZE + 1) * (SQUARE_SIZE + 1) - 1;
    geoData.vertices.reserve(kVertexCount);
    geoData.indices.reserve(3ull * SQUARE_SIZE * SQUARE_SIZE);

    const float missZ = 1.0f;
    const float hitZ  = 0.0f;

    tcu::Vec3 v;

    uint32_t triId  = 0;
    uint32_t vertId = 0;

    v.y() = -0.2f + offset.y();
    for (uint32_t y = 0; y < SQUARE_SIZE; ++y)
    {
        v.x() = -0.2f + offset.x();
        for (uint32_t x = 0; x < SQUARE_SIZE; ++x)
        {
            v.z() = (isMissTriangle(triId) ? missZ : hitZ) + offset.z();
            geoData.vertices.push_back(v);
            v.x() += 1.0f;

            geoData.indices.push_back(vertId);
            geoData.indices.push_back(vertId + SQUARE_SIZE + 1);
            geoData.indices.push_back(vertId + 1);

            ++triId;
            ++vertId;
        }
        geoData.vertices.push_back(v);
        v.y() += 1.0f;
        ++vertId;
    }
    v.x() = -0.2f + offset.x();
    for (uint32_t x = 0; x < SQUARE_SIZE; ++x)
    {
        geoData.vertices.push_back(v);
        v.x() += 1.0f;
    }

    return geoData;
}

std::vector<tcu::Vec3> makeAABBGeometry(const tcu::Vec3 offset)
{
    std::vector<tcu::Vec3> geoData;
    geoData.reserve(SQUARE_SIZE * SQUARE_SIZE * 2);

    uint32_t aabbId = 0;
    for (uint32_t y = 0; y < SQUARE_SIZE; ++y)
    {
        for (uint32_t x = 0; x < SQUARE_SIZE; ++x)
        {
            tcu::Vec3 min = {static_cast<float>(x) - 0.1f + offset.x(), static_cast<float>(y) - 0.1f + offset.y(),
                             offset.z() - 0.1f};
            tcu::Vec3 max = {static_cast<float>(x) + 0.1f + offset.x(), static_cast<float>(y) + 0.1f + offset.y(),
                             offset.z() + 0.1f};
            if (isMissTriangle(aabbId))
            {
                min.z() += 2.0f;
                max.z() += 2.0f;
            };

            geoData.push_back(min);
            geoData.push_back(max);
            ++aabbId;
        }
    }

    return geoData;
}

class RayTracingBuildIndirectTestInstance : public TestInstance
{
public:
    RayTracingBuildIndirectTestInstance(Context &context, const CaseDef &data);
    ~RayTracingBuildIndirectTestInstance(void);
    tcu::TestStatus iterate(void);

protected:
    void checkSupportInInstance(void) const;
    de::MovePtr<BufferWithMemory> prepareBuffer(VkDeviceSize bufferSizeBytes, const std::string &shaderName);
    de::MovePtr<BufferWithMemory> runTest(const VkBuffer indirectBottomAccelerationStructure,
                                          const VkBuffer indirectTopAccelerationStructure,
                                          const vk::VkGeometryTypeKHR geometryType);

    virtual de::SharedPtr<TopLevelAccelerationStructure> initTopAccelerationStructure(
        VkCommandBuffer cmdBuffer, de::SharedPtr<BottomLevelAccelerationStructure> &bottomLevelAccelerationStructure,
        const VkBuffer indirectBuffer, const VkDeviceSize indirectBufferOffset, const uint32_t indirectBufferStride);

    virtual de::SharedPtr<BottomLevelAccelerationStructure> initBottomAccelerationStructure(
        VkCommandBuffer cmdBuffer, const VkBuffer indirectBuffer, const VkDeviceSize indirectBufferOffset,
        const uint32_t indirectBufferStride);

    VkBuffer initIndirectTopAccelerationStructure(void);
    VkBuffer initIndirectBottomAccelerationStructure(void);
    const CaseDef m_data;

private:
    de::MovePtr<BufferWithMemory> m_indirectAccelerationStructureBottom;
    de::MovePtr<BufferWithMemory> m_indirectAccelerationStructureTop;
};

RayTracingBuildIndirectTestInstance::RayTracingBuildIndirectTestInstance(Context &context, const CaseDef &data)
    : vkt::TestInstance(context)
    , m_data(data)
    , m_indirectAccelerationStructureBottom()
    , m_indirectAccelerationStructureTop()
{
}

RayTracingBuildIndirectTestInstance::~RayTracingBuildIndirectTestInstance(void)
{
}

class RayTracingBuildTrianglesIndexed : public RayTracingBuildIndirectTestInstance
{
public:
    RayTracingBuildTrianglesIndexed(Context &context, const CaseDef &data)
        : RayTracingBuildIndirectTestInstance(context, data){};

protected:
    de::SharedPtr<BottomLevelAccelerationStructure> initBottomAccelerationStructure(
        VkCommandBuffer cmdBuffer, const VkBuffer indirectBuffer, const VkDeviceSize indirectBufferOffset,
        const uint32_t indirectBufferStride) override;
};

class RayTracingBuildAABBs : public RayTracingBuildIndirectTestInstance
{
public:
    RayTracingBuildAABBs(Context &context, const CaseDef &data) : RayTracingBuildIndirectTestInstance(context, data){};
    tcu::TestStatus iterate(void) override;

protected:
    de::SharedPtr<BottomLevelAccelerationStructure> initBottomAccelerationStructure(
        VkCommandBuffer cmdBuffer, const VkBuffer indirectBuffer, const VkDeviceSize indirectBufferOffset,
        const uint32_t indirectBufferStride) override;
};

class RayTracingBuildInstances : public RayTracingBuildIndirectTestInstance
{
public:
    RayTracingBuildInstances(Context &context, const CaseDef &data)
        : RayTracingBuildIndirectTestInstance(context, data){};

protected:
    de::SharedPtr<TopLevelAccelerationStructure> initTopAccelerationStructure(
        VkCommandBuffer cmdBuffer, de::SharedPtr<BottomLevelAccelerationStructure> &bottomLevelAccelerationStructure,
        const VkBuffer indirectBuffer, const VkDeviceSize indirectBufferOffset,
        const uint32_t indirectBufferStride) override;
    de::SharedPtr<BottomLevelAccelerationStructure> initBottomAccelerationStructure(
        VkCommandBuffer cmdBuffer, const VkBuffer indirectBuffer, const VkDeviceSize indirectBufferOffset,
        const uint32_t indirectBufferStride) override;
};

template <class InstanceClass, class InitData>
class RayTracingTestCase : public TestCase
{
public:
    RayTracingTestCase(tcu::TestContext &context, const char *name, const InitData data);
    ~RayTracingTestCase(void)
    {
    }

    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    InitData m_data;
};

template <class InstanceClass, class InitData>
RayTracingTestCase<InstanceClass, InitData>::RayTracingTestCase(tcu::TestContext &context, const char *name,
                                                                const InitData data)
    : vkt::TestCase(context, name)
    , m_data(data)
{
}

template <class InstanceClass, class InitData>
void RayTracingTestCase<InstanceClass, InitData>::initPrograms(SourceCollections &programCollection) const
{
    initProgramsHelper(programCollection, m_data);
}

template <class InstanceClass, class InitData>
TestInstance *RayTracingTestCase<InstanceClass, InitData>::createInstance(Context &context) const
{
    return new InstanceClass(context, m_data);
};

template <class InstanceClass, class InitData>
void RayTracingTestCase<InstanceClass, InitData>::checkSupport(Context &context) const
{
    const VkPhysicalDeviceAccelerationStructureFeaturesKHR &accelerationStructureFeaturesKHR =
        context.getAccelerationStructureFeatures();
    if (accelerationStructureFeaturesKHR.accelerationStructure == false)
        TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");

    const VkPhysicalDeviceRayTracingPipelineFeaturesKHR &rayTracingPipelineFeaturesKHR =
        context.getRayTracingPipelineFeatures();
    if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == false)
        TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");

    if (accelerationStructureFeaturesKHR.accelerationStructureIndirectBuild == false)
        TCU_THROW(NotSupportedError,
                  "Requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructureIndirectBuild");
}

de::SharedPtr<TopLevelAccelerationStructure> RayTracingBuildIndirectTestInstance::initTopAccelerationStructure(
    VkCommandBuffer cmdBuffer, de::SharedPtr<BottomLevelAccelerationStructure> &bottomLevelAccelerationStructure,
    const VkBuffer indirectBuffer, const VkDeviceSize indirectBufferOffset, const uint32_t indirectBufferStride)
{
    const DeviceInterface &vkd                        = m_context.getDeviceInterface();
    const VkDevice device                             = m_context.getDevice();
    Allocator &allocator                              = m_context.getDefaultAllocator();
    de::MovePtr<TopLevelAccelerationStructure> result = makeTopLevelAccelerationStructure();

    AccelerationStructBufferProperties bufferProps;
    bufferProps.props.residency = ResourceResidency::TRADITIONAL;

    result->setInstanceCount(1);
    result->addInstance(bottomLevelAccelerationStructure);
    result->setIndirectBuildParameters(indirectBuffer, indirectBufferOffset, indirectBufferStride);

    result->createAndBuild(vkd, device, cmdBuffer, allocator, bufferProps);

    return de::SharedPtr<TopLevelAccelerationStructure>(result.release());
}

de::SharedPtr<BottomLevelAccelerationStructure> RayTracingBuildIndirectTestInstance::initBottomAccelerationStructure(
    VkCommandBuffer cmdBuffer, const VkBuffer indirectBuffer, const VkDeviceSize indirectBufferOffset,
    const uint32_t indirectBufferStride)
{
    const DeviceInterface &vkd                           = m_context.getDeviceInterface();
    const VkDevice device                                = m_context.getDevice();
    Allocator &allocator                                 = m_context.getDefaultAllocator();
    de::MovePtr<BottomLevelAccelerationStructure> result = makeBottomLevelAccelerationStructure();

    AccelerationStructBufferProperties bufferProps;
    bufferProps.props.residency = ResourceResidency::TRADITIONAL;

    result->setGeometryCount(m_data.geometriesGroupCount);
    result->setIndirectBuildParameters(indirectBuffer, indirectBufferOffset, indirectBufferStride);
    result->setTransformBufferAddressOffset(-m_data.transformOffset);
    const int32_t VertexOffsetInBytes          = m_data.primitiveOffset + VERTEX_STRIDE * m_data.firstVertex;
    const uint32_t TriangleSizeInBytes         = VERTEX_STRIDE * 3;
    const uint32_t CeilVertexOffsetInTriangles = (VertexOffsetInBytes - 1 + TriangleSizeInBytes) / TriangleSizeInBytes;

    for (uint32_t geoId = 0; geoId < m_data.geometriesGroupCount; ++geoId)
    {
        const tcu::Vec3 offset = {static_cast<float>(SQUARE_OFFSET_X * geoId), 0.0f, 0.0f};
        const auto geoData     = makeTriangleGeometry(offset);
        auto rtGeo             = de::SharedPtr<RaytracedGeometryBase>(
            new RaytracedGeometry<tcu::Vec3, EmptyIndex>(vk::VK_GEOMETRY_TYPE_TRIANGLES_KHR));

        if (m_data.doUpdate)
        {
            // add vertices to build invalid geometry 1st time, update will offset vertex buffer to correct vertices
            for (uint32_t i = 0; i < geoData.indices.size() / 3; ++i)
            {
                rtGeo->addVertex({-9999.0f, -9999.0f, -9999.9f - static_cast<float>(i)});
                rtGeo->addVertex({-9999.0f, -9999.9f, -9999.9f - static_cast<float>(i)});
                rtGeo->addVertex({-9999.9f, -9999.0f, -9999.9f - static_cast<float>(i)});
            }
        }

        for (const auto &id : geoData.indices)
        {
            rtGeo->addVertex(geoData.vertices[id]);
        }

        // add padding vertices to prevent running out of maxVertex (buffer range) during build with bigger offsets
        for (uint32_t i = 0; i < CeilVertexOffsetInTriangles * 3; ++i)
        {
            rtGeo->addVertex(PADDING_VERTEX);
        }

        result->addGeometry(rtGeo);

        const VkTransformMatrixKHR transformMatrix = {{
            {1.0f, 0.0f, 0.0f, offset.x()},
            {0.0f, 1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f, 0.0f},
        }};
        result->setGeometryTransform(geoId, transformMatrix);
    }

    if (m_data.doUpdate)
    {
        result->setBuildFlags(VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR);
        result->setVertexBufferAddressOffset(-VertexOffsetInBytes);
        result->createAndBuild(vkd, device, cmdBuffer, allocator, bufferProps);

        const int32_t VertexByteSize = SQUARE_SIZE * SQUARE_SIZE * sizeof(tcu::Vec3) * 3;
        result->setVertexBufferAddressOffset(-VertexOffsetInBytes + VertexByteSize);
        result->build(vkd, device, cmdBuffer, result.get());
    }
    else
    {
        result->setVertexBufferAddressOffset(-VertexOffsetInBytes);
        result->createAndBuild(vkd, device, cmdBuffer, allocator, bufferProps);
    }

    return de::SharedPtr<BottomLevelAccelerationStructure>(result.release());
}

de::SharedPtr<BottomLevelAccelerationStructure> RayTracingBuildTrianglesIndexed::initBottomAccelerationStructure(
    VkCommandBuffer cmdBuffer, const VkBuffer indirectBuffer, const VkDeviceSize indirectBufferOffset,
    const uint32_t indirectBufferStride)
{
    const DeviceInterface &vkd                           = m_context.getDeviceInterface();
    const VkDevice device                                = m_context.getDevice();
    Allocator &allocator                                 = m_context.getDefaultAllocator();
    de::MovePtr<BottomLevelAccelerationStructure> result = makeBottomLevelAccelerationStructure();

    AccelerationStructBufferProperties bufferProps;
    bufferProps.props.residency = ResourceResidency::TRADITIONAL;

    result->setGeometryCount(m_data.geometriesGroupCount);
    result->setIndirectBuildParameters(indirectBuffer, indirectBufferOffset, indirectBufferStride);
    result->setTransformBufferAddressOffset(-m_data.transformOffset);

    for (uint32_t geoId = 0; geoId < m_data.geometriesGroupCount; ++geoId)
    {
        const tcu::Vec3 offset = {static_cast<float>(SQUARE_OFFSET_X * geoId), 0.0f, 0.0f};
        const auto geoData     = makeTriangleGeometry(offset);
        auto rtGeo             = de::SharedPtr<RaytracedGeometryBase>(
            new RaytracedGeometry<tcu::Vec3, uint32_t>(vk::VK_GEOMETRY_TYPE_TRIANGLES_KHR));
        const uint32_t firstVertexReminder = 2 - ((m_data.firstVertex + 2) % 3);
        const uint32_t fakeTriangles       = (m_data.firstVertex + 2) / 3;

        for (uint32_t i = 0; i < fakeTriangles; ++i)
        {
            rtGeo->addVertex({-9999.0f, -9999.0f, -9999.9f - static_cast<float>(i)});
            rtGeo->addVertex({-9999.0f, -9999.9f, -9999.9f - static_cast<float>(i)});
            rtGeo->addVertex({-9999.9f, -9999.0f, -9999.9f - static_cast<float>(i)});
        }

        for (const auto &vert : geoData.vertices)
        {
            rtGeo->addVertex(vert);
        }

        if (m_data.doUpdate)
        {
            // add indices covering only 1st triangle clockwise to build invalid geometry 1st time,
            // update will offset index buffer to correct indices
            for (uint32_t i = 0; i < geoData.indices.size() / 3; ++i)
            {
                rtGeo->addIndex(firstVertexReminder + 0);
                rtGeo->addIndex(firstVertexReminder + 1);
                rtGeo->addIndex(firstVertexReminder + SQUARE_SIZE + 1);
            }
        }

        for (const auto &id : geoData.indices)
        {
            rtGeo->addIndex(id + firstVertexReminder);
        }

        result->addGeometry(rtGeo);

        const VkTransformMatrixKHR transformMatrix = {{
            {1.0f, 0.0f, 0.0f, offset.x()},
            {0.0f, 1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f, 0.0f},
        }};
        result->setGeometryTransform(geoId, transformMatrix);
    }

    if (m_data.doUpdate)
    {
        result->setBuildFlags(VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR);
        result->setIndexBufferAddressOffset(-m_data.primitiveOffset);
        result->createAndBuild(vkd, device, cmdBuffer, allocator, bufferProps);

        const int32_t IndexByteSize = SQUARE_SIZE * SQUARE_SIZE * sizeof(uint32_t) * 3;
        result->setIndexBufferAddressOffset(-m_data.primitiveOffset + IndexByteSize);
        result->build(vkd, device, cmdBuffer, result.get());
    }
    else
    {
        result->setIndexBufferAddressOffset(-m_data.primitiveOffset);
        result->createAndBuild(vkd, device, cmdBuffer, allocator, bufferProps);
    }

    return de::SharedPtr<BottomLevelAccelerationStructure>(result.release());
}

de::SharedPtr<BottomLevelAccelerationStructure> RayTracingBuildAABBs::initBottomAccelerationStructure(
    VkCommandBuffer cmdBuffer, const VkBuffer indirectBuffer, const VkDeviceSize indirectBufferOffset,
    const uint32_t indirectBufferStride)
{
    const DeviceInterface &vkd                           = m_context.getDeviceInterface();
    const VkDevice device                                = m_context.getDevice();
    Allocator &allocator                                 = m_context.getDefaultAllocator();
    de::MovePtr<BottomLevelAccelerationStructure> result = makeBottomLevelAccelerationStructure();

    AccelerationStructBufferProperties bufferProps;
    bufferProps.props.residency = ResourceResidency::TRADITIONAL;

    result->setGeometryCount(m_data.geometriesGroupCount);
    result->setIndirectBuildParameters(indirectBuffer, indirectBufferOffset, indirectBufferStride);
    const uint32_t AABBSizeInBytes         = sizeof(VkAabbPositionsKHR);
    const uint32_t CeilVertexOffsetInAABBs = (m_data.primitiveOffset - 1 + AABBSizeInBytes) / AABBSizeInBytes;

    for (uint32_t geoId = 0; geoId < m_data.geometriesGroupCount; ++geoId)
    {
        const tcu::Vec3 offset = {static_cast<float>(SQUARE_OFFSET_X * geoId * 2), 0.0f, 0.0f};
        const auto geoData     = makeAABBGeometry(offset);
        auto rtGeo             = de::SharedPtr<RaytracedGeometryBase>(
            new RaytracedGeometry<tcu::Vec3, EmptyIndex>(vk::VK_GEOMETRY_TYPE_AABBS_KHR));

        if (m_data.doUpdate)
        {
            // add fake vertices for doUpdate, first build will point to fake vertices,
            // update will offset vertex buffer to correct geometry
            for (uint32_t i = 0; i < geoData.size(); ++i)
            {
                rtGeo->addVertex(PADDING_VERTEX);
            }
        }

        for (const auto &vert : geoData)
        {
            rtGeo->addVertex(vert);
        }
        // add padding vertices to prevent running out of ppMaxPrimitiveCounts during build with bigger offsets

        for (uint32_t i = 0; i < CeilVertexOffsetInAABBs * 6; ++i)
        {
            rtGeo->addVertex(PADDING_VERTEX);
        }

        result->addGeometry(rtGeo);
    }

    if (m_data.doUpdate)
    {
        result->setBuildFlags(VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR);
        result->setVertexBufferAddressOffset(-m_data.primitiveOffset);
        result->createAndBuild(vkd, device, cmdBuffer, allocator, bufferProps);

        const int32_t VertexByteSize = SQUARE_SIZE * SQUARE_SIZE * sizeof(tcu::Vec3) * 2;
        result->setVertexBufferAddressOffset(-m_data.primitiveOffset + VertexByteSize);
        result->build(vkd, device, cmdBuffer, result.get());
    }
    else
    {
        result->setVertexBufferAddressOffset(-m_data.primitiveOffset);
        result->createAndBuild(vkd, device, cmdBuffer, allocator, bufferProps);
    }

    return de::SharedPtr<BottomLevelAccelerationStructure>(result.release());
}

de::SharedPtr<TopLevelAccelerationStructure> RayTracingBuildInstances::initTopAccelerationStructure(
    VkCommandBuffer cmdBuffer, de::SharedPtr<BottomLevelAccelerationStructure> &bottomLevelAccelerationStructure,
    const VkBuffer indirectBuffer, const VkDeviceSize indirectBufferOffset, const uint32_t indirectBufferStride)
{
    const DeviceInterface &vkd                        = m_context.getDeviceInterface();
    const VkDevice device                             = m_context.getDevice();
    Allocator &allocator                              = m_context.getDefaultAllocator();
    de::MovePtr<TopLevelAccelerationStructure> result = makeTopLevelAccelerationStructure();

    AccelerationStructBufferProperties bufferProps;
    bufferProps.props.residency = ResourceResidency::TRADITIONAL;

    result->setInstanceCount(2 * m_data.maxInstancesCount + 1);
    result->setIndirectBuildParameters(indirectBuffer, indirectBufferOffset, indirectBufferStride);

    if (m_data.doUpdate)
    {
        // add fake instances, first build will point to fake blas,
        // update will offset instance buffer to correct blas
        const VkTransformMatrixKHR fakeTransformMatrix = {{
            {1.0f, 0.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f, 999.0f},
        }};
        for (uint32_t instId = 0; instId < m_data.maxInstancesCount; ++instId)
        {

            result->addInstance(bottomLevelAccelerationStructure, fakeTransformMatrix);
        }
    }

    for (uint32_t instId = 0; instId < m_data.maxInstancesCount; ++instId)
    {
        const VkTransformMatrixKHR transformMatrix = {{
            {1.0f, 0.0f, 0.0f, static_cast<float>(SQUARE_OFFSET_X * instId * 2)},
            {0.0f, 1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f, 0.0f},
        }};
        result->addInstance(bottomLevelAccelerationStructure, transformMatrix);
    }

    if (m_data.doUpdate)
    {
        result->setBuildFlags(VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR);
        result->setInstanceBufferAddressOffset(-m_data.instancesOffset);
        result->createAndBuild(vkd, device, cmdBuffer, allocator, bufferProps);

        const int32_t InstanceByteSize =
            m_data.maxInstancesCount * static_cast<int32_t>(sizeof(VkAccelerationStructureInstanceKHR));
        result->setInstanceBufferAddressOffset(-m_data.instancesOffset + InstanceByteSize);
        result->build(vkd, device, cmdBuffer, result.get());
    }
    else
    {
        result->setInstanceBufferAddressOffset(-m_data.instancesOffset);
        result->createAndBuild(vkd, device, cmdBuffer, allocator, bufferProps);
    }

    return de::SharedPtr<TopLevelAccelerationStructure>(result.release());
}

de::SharedPtr<BottomLevelAccelerationStructure> RayTracingBuildInstances::initBottomAccelerationStructure(
    VkCommandBuffer cmdBuffer, const VkBuffer indirectBuffer, const VkDeviceSize indirectBufferOffset,
    const uint32_t indirectBufferStride)
{
    const DeviceInterface &vkd                           = m_context.getDeviceInterface();
    const VkDevice device                                = m_context.getDevice();
    Allocator &allocator                                 = m_context.getDefaultAllocator();
    de::MovePtr<BottomLevelAccelerationStructure> result = makeBottomLevelAccelerationStructure();

    AccelerationStructBufferProperties bufferProps;
    bufferProps.props.residency = ResourceResidency::TRADITIONAL;

    result->setGeometryCount(m_data.geometriesGroupCount);
    result->setIndirectBuildParameters(indirectBuffer, indirectBufferOffset, indirectBufferStride);
    result->setTransformBufferAddressOffset(-m_data.transformOffset);
    const int32_t TotalVertexOffsetInBytes = m_data.primitiveOffset + VERTEX_STRIDE * m_data.firstVertex;
    result->setVertexBufferAddressOffset(-TotalVertexOffsetInBytes);
    const uint32_t TriangleSizeInBytes = VERTEX_STRIDE * 3;
    const uint32_t CeilVertexOffsetInTriangles =
        (TotalVertexOffsetInBytes - 1 + TriangleSizeInBytes) / TriangleSizeInBytes;

    for (uint32_t geoId = 0; geoId < (m_data.geometriesGroupCount / m_data.maxInstancesCount); ++geoId)
    {
        const tcu::Vec3 offset = {static_cast<float>(SQUARE_OFFSET_X * geoId * m_data.maxInstancesCount), 0.0f, 0.0f};
        const auto geoData     = makeTriangleGeometry(offset);
        auto rtGeo             = de::SharedPtr<RaytracedGeometryBase>(
            new RaytracedGeometry<tcu::Vec3, EmptyIndex>(vk::VK_GEOMETRY_TYPE_TRIANGLES_KHR));
        for (const auto &id : geoData.indices)
        {
            rtGeo->addVertex(geoData.vertices[id]);
        }

        // add padding vertices to prevent running out of maxVertex (buffer range) during build with bigger offsets
        for (uint32_t i = 0; i < CeilVertexOffsetInTriangles * 3; ++i)
        {
            rtGeo->addVertex(PADDING_VERTEX);
        }

        result->addGeometry(rtGeo);

        const VkTransformMatrixKHR transformMatrix = {{
            {1.0f, 0.0f, 0.0f, offset.x()},
            {0.0f, 1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f, 0.0f},
        }};
        result->setGeometryTransform(geoId, transformMatrix);
    }

    result->createAndBuild(vkd, device, cmdBuffer, allocator, bufferProps);

    return de::SharedPtr<BottomLevelAccelerationStructure>(result.release());
}

de::MovePtr<BufferWithMemory> RayTracingBuildIndirectTestInstance::prepareBuffer(VkDeviceSize bufferSizeBytes,
                                                                                 const std::string &shaderName)
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

    const VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(
        bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    de::MovePtr<BufferWithMemory> buffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vkd, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress));

    const Move<VkDescriptorSetLayout> descriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ALL_RAY_TRACING_STAGES)
            .build(vkd, device);
    const Move<VkDescriptorPool> descriptorPool =
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const Move<VkDescriptorSet> descriptorSet   = makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout);
    const Move<VkPipelineLayout> pipelineLayout = makePipelineLayout(vkd, device, descriptorSetLayout.get());
    const Move<VkCommandPool> cmdPool           = createCommandPool(vkd, device, 0, queueFamilyIndex);
    const Move<VkCommandBuffer> cmdBuffer =
        allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    const vk::VkDescriptorBufferInfo descriptorBufferInfo = makeDescriptorBufferInfo(**buffer, 0ull, bufferSizeBytes);

    de::MovePtr<RayTracingPipeline> rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();
    const Move<VkPipeline> pipeline =
        makePipeline(vkd, device, m_context.getBinaryCollection(), rayTracingPipeline, *pipelineLayout, shaderName);
    const de::MovePtr<BufferWithMemory> shaderBindingTable = rayTracingPipeline->createShaderBindingTable(
        vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
    const VkStridedDeviceAddressRegionKHR raygenShaderBindingTableRegion =
        makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, shaderBindingTable->get(), 0),
                                          shaderGroupHandleSize, shaderGroupHandleSize);
    const VkStridedDeviceAddressRegionKHR missShaderBindingTableRegion     = makeStridedDeviceAddressRegionKHR(0, 0, 0);
    const VkStridedDeviceAddressRegionKHR hitShaderBindingTableRegion      = makeStridedDeviceAddressRegionKHR(0, 0, 0);
    const VkStridedDeviceAddressRegionKHR callableShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(0, 0, 0);

    beginCommandBuffer(vkd, *cmdBuffer, 0u);
    {
        DescriptorSetUpdateBuilder()
            .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorBufferInfo)
            .update(vkd, device);

        vkd.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipelineLayout, 0, 1,
                                  &descriptorSet.get(), 0, nullptr);

        vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipeline);

        cmdTraceRays(vkd, *cmdBuffer, &raygenShaderBindingTableRegion, &missShaderBindingTableRegion,
                     &hitShaderBindingTableRegion, &callableShaderBindingTableRegion, 1u, 1u, 1u);
    }
    endCommandBuffer(vkd, *cmdBuffer);

    submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

    return buffer;
}

de::MovePtr<BufferWithMemory> RayTracingBuildIndirectTestInstance::runTest(
    const VkBuffer indirectBottomAccelerationStructure, const VkBuffer indirectTopAccelerationStructure,
    const vk::VkGeometryTypeKHR geometryType)
{
    const InstanceInterface &vki            = m_context.getInstanceInterface();
    const DeviceInterface &vkd              = m_context.getDeviceInterface();
    const VkDevice device                   = m_context.getDevice();
    const VkPhysicalDevice physicalDevice   = m_context.getPhysicalDevice();
    const uint32_t queueFamilyIndex         = m_context.getUniversalQueueFamilyIndex();
    const VkQueue queue                     = m_context.getUniversalQueue();
    Allocator &allocator                    = m_context.getDefaultAllocator();
    const VkFormat format                   = VK_FORMAT_R32_UINT;
    const uint32_t pixelCount               = m_data.width * m_data.height * m_data.depth;
    const uint32_t shaderGroupHandleSize    = getShaderGroupSize(vki, physicalDevice);
    const uint32_t shaderGroupBaseAlignment = getShaderGroupBaseAlignment(vki, physicalDevice);

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
    const Move<VkPipeline> pipeline = makePipeline(vkd, device, m_context.getBinaryCollection(), rayTracingPipeline,
                                                   *pipelineLayout, RAYGEN_GROUP, MISS_GROUP, HIT_GROUP, geometryType);
    const de::MovePtr<BufferWithMemory> raygenShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
        vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, RAYGEN_GROUP, 1u);
    const de::MovePtr<BufferWithMemory> missShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
        vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, MISS_GROUP, 1u);
    const de::MovePtr<BufferWithMemory> hitShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
        vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, HIT_GROUP, 1u);
    const VkStridedDeviceAddressRegionKHR raygenShaderBindingTableRegion =
        makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(), 0),
                                          shaderGroupHandleSize, shaderGroupHandleSize);
    const VkStridedDeviceAddressRegionKHR missShaderBindingTableRegion =
        makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missShaderBindingTable->get(), 0),
                                          shaderGroupHandleSize, shaderGroupHandleSize);
    const VkStridedDeviceAddressRegionKHR hitShaderBindingTableRegion =
        makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(), 0),
                                          shaderGroupHandleSize, shaderGroupHandleSize);
    const VkStridedDeviceAddressRegionKHR callableShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(0, 0, 0);

    const VkImageCreateInfo imageCreateInfo = makeImageCreateInfo(m_data.width, m_data.height, m_data.depth, format);
    const VkImageSubresourceRange imageSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0, 1u);
    const de::MovePtr<ImageWithMemory> image = de::MovePtr<ImageWithMemory>(
        new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
    const Move<VkImageView> imageView =
        makeImageView(vkd, device, **image, VK_IMAGE_VIEW_TYPE_3D, format, imageSubresourceRange);

    const VkBufferCreateInfo bufferCreateInfo =
        makeBufferCreateInfo(pixelCount * sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
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
    const VkClearValue clearValue                      = makeClearValueColorU32(5u, 5u, 5u, 255u);
    const uint32_t indirectAccelerationStructureStride = sizeof(VkAccelerationStructureBuildRangeInfoKHR);

    de::SharedPtr<BottomLevelAccelerationStructure> bottomLevelAccelerationStructure;
    de::SharedPtr<TopLevelAccelerationStructure> topLevelAccelerationStructure;

    beginCommandBuffer(vkd, *cmdBuffer, 0u);
    {
        cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &preImageBarrier);
        vkd.cmdClearColorImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color, 1,
                               &imageSubresourceRange);
        cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, &postImageBarrier);

        bottomLevelAccelerationStructure = initBottomAccelerationStructure(
            *cmdBuffer, indirectBottomAccelerationStructure, 0, indirectAccelerationStructureStride);
        topLevelAccelerationStructure =
            initTopAccelerationStructure(*cmdBuffer, bottomLevelAccelerationStructure, indirectTopAccelerationStructure,
                                         0, indirectAccelerationStructureStride);

        const TopLevelAccelerationStructure *topLevelAccelerationStructurePtr = topLevelAccelerationStructure.get();
        VkWriteDescriptorSetAccelerationStructureKHR accelerationStructureWriteDescriptorSet = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR, //  VkStructureType sType;
            nullptr,                                                           //  const void* pNext;
            1u,                                                                //  uint32_t accelerationStructureCount;
            topLevelAccelerationStructurePtr->getPtr(), //  const VkAccelerationStructureKHR *pAccelerationStructures;
        };

        DescriptorSetUpdateBuilder()
            .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorImageInfo)
            .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                         VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelerationStructureWriteDescriptorSet)
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

    invalidateMappedMemoryRange(vkd, device, buffer->getAllocation().getMemory(), buffer->getAllocation().getOffset(),
                                pixelCount * sizeof(uint32_t));

    return buffer;
}

void RayTracingBuildIndirectTestInstance::checkSupportInInstance(void) const
{
    const InstanceInterface &vki                           = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice                  = m_context.getPhysicalDevice();
    de::MovePtr<RayTracingProperties> rayTracingProperties = makeRayTracingProperties(vki, physicalDevice);

    if (rayTracingProperties->getMaxPrimitiveCount() < m_data.primitiveCount)
        TCU_THROW(NotSupportedError, "Triangles required more than supported");

    if (rayTracingProperties->getMaxGeometryCount() < m_data.geometriesGroupCount)
        TCU_THROW(NotSupportedError, "Geometries required more than supported");

    if (rayTracingProperties->getMaxInstanceCount() < m_data.instancesCount)
        TCU_THROW(NotSupportedError, "Instances required more than supported");
}

VkBuffer RayTracingBuildIndirectTestInstance::initIndirectTopAccelerationStructure(void)
{
    VkBuffer result = VK_NULL_HANDLE;

    m_indirectAccelerationStructureTop = prepareBuffer(sizeof(VkAccelerationStructureBuildRangeInfoKHR), "wr-ast");
    result                             = **m_indirectAccelerationStructureTop;

    return result;
}

VkBuffer RayTracingBuildIndirectTestInstance::initIndirectBottomAccelerationStructure(void)
{
    VkBuffer result = VK_NULL_HANDLE;

    m_indirectAccelerationStructureBottom =
        prepareBuffer(sizeof(VkAccelerationStructureBuildRangeInfoKHR) * m_data.geometriesGroupCount, "wr-asb");
    result = **m_indirectAccelerationStructureBottom;

    return result;
}

tcu::TestStatus RayTracingBuildIndirectTestInstance::iterate(void)
{
    checkSupportInInstance();

    const VkBuffer indirectAccelerationStructureBottom = initIndirectBottomAccelerationStructure();
    const VkBuffer indirectAccelerationStructureTop    = initIndirectTopAccelerationStructure();
    const de::MovePtr<BufferWithMemory> buffer         = runTest(
        indirectAccelerationStructureBottom, indirectAccelerationStructureTop, vk::VK_GEOMETRY_TYPE_TRIANGLES_KHR);
    const uint32_t *bufferPtr = (uint32_t *)buffer->getAllocation().getHostPtr();
    uint32_t failures         = 0;

    for (uint32_t z = 0; z < m_data.depth; ++z)
    {
        const bool IsValidInstance     = (z % m_data.maxInstancesCount) < m_data.instancesCount;
        const uint32_t *bufferPtrLevel = &bufferPtr[z * m_data.height * m_data.width];

        for (uint32_t y = 0; y < m_data.height; ++y)
            for (uint32_t x = 0; x < m_data.width; ++x)
            {
                const uint32_t n = m_data.width * y + x;
                const uint32_t expectedValue =
                    (!IsValidInstance || isMissTriangle(n) || n >= m_data.primitiveCount) ? MISS : HIT;
                if (bufferPtrLevel[n] != expectedValue)
                    failures++;
            }
    }

    if (failures == 0)
        return tcu::TestStatus::pass("Pass");
    else
        return tcu::TestStatus::fail("failures=" + de::toString(failures));
}

tcu::TestStatus RayTracingBuildAABBs::iterate(void)
{
    checkSupportInInstance();

    const VkBuffer indirectAccelerationStructureBottom = initIndirectBottomAccelerationStructure();
    const VkBuffer indirectAccelerationStructureTop    = initIndirectTopAccelerationStructure();
    const de::MovePtr<BufferWithMemory> buffer =
        runTest(indirectAccelerationStructureBottom, indirectAccelerationStructureTop, vk::VK_GEOMETRY_TYPE_AABBS_KHR);
    const uint32_t *bufferPtr = (uint32_t *)buffer->getAllocation().getHostPtr();
    uint32_t failures         = 0;

    for (uint32_t z = 0; z < m_data.depth; ++z)
    {
        const bool IsValidInstance     = (z % m_data.maxInstancesCount) < m_data.instancesCount;
        const uint32_t *bufferPtrLevel = &bufferPtr[z * m_data.height * m_data.width];

        // In the case of AABB geometries, implementations may increase their size in an acceleration structure
        // in order to mitigate precision issues. This may result in false positives being reported to the application.
        for (uint32_t y = 0; y < m_data.height; ++y)
            for (uint32_t x = 0; x < m_data.width; ++x)
            {
                const uint32_t n = m_data.width * y + x;
                const uint32_t expectedValue =
                    (!IsValidInstance || isMissTriangle(n) || n >= m_data.primitiveCount) ? MISS : HIT;
                if (bufferPtrLevel[n] != expectedValue && expectedValue == HIT)
                    failures++;
            }
    }

    if (failures == 0)
        return tcu::TestStatus::pass("Pass");
    else
        return tcu::TestStatus::fail("failures=" + de::toString(failures));
}

} // namespace

tcu::TestCaseGroup *createBuildIndirectTests(tcu::TestContext &testCtx)
{
    auto addIndirectTests = [&](bool doUpdate, de::MovePtr<tcu::TestCaseGroup> &group)
    {
        de::MovePtr<tcu::TestCaseGroup> trianglesIndexedGroup(new tcu::TestCaseGroup(testCtx, "triangles_indexed"));
        de::MovePtr<tcu::TestCaseGroup> trianglesNoIndexGroup(new tcu::TestCaseGroup(testCtx, "triangles_no_index"));
        de::MovePtr<tcu::TestCaseGroup> aabbsGroup(new tcu::TestCaseGroup(testCtx, "aabbs"));
        de::MovePtr<tcu::TestCaseGroup> instancesGroup(new tcu::TestCaseGroup(testCtx, "instances"));

        { // BLAS primitive_count
            de::MovePtr<tcu::TestCaseGroup> trianglesIndexedPrimCount(
                new tcu::TestCaseGroup(testCtx, "primitive_count"));
            de::MovePtr<tcu::TestCaseGroup> trianglesNoIndexPrimCount(
                new tcu::TestCaseGroup(testCtx, "primitive_count"));
            de::MovePtr<tcu::TestCaseGroup> aabbPrimCount(new tcu::TestCaseGroup(testCtx, "primitive_count"));

            CaseDef caseDef  = {};
            caseDef.doUpdate = doUpdate;
            for (uint32_t primCount = SQUARE_SIZE * SQUARE_SIZE; primCount >= SQUARE_SIZE; primCount -= SQUARE_SIZE)
            {
                caseDef.primitiveCount = primCount;

                trianglesIndexedPrimCount->addChild(new RayTracingTestCase<RayTracingBuildTrianglesIndexed, CaseDef>(
                    testCtx, std::to_string(primCount).c_str(), caseDef));
                trianglesNoIndexPrimCount->addChild(
                    new RayTracingTestCase<RayTracingBuildIndirectTestInstance, CaseDef>(
                        testCtx, std::to_string(primCount).c_str(), caseDef));
                aabbPrimCount->addChild(new RayTracingTestCase<RayTracingBuildAABBs, CaseDef>(
                    testCtx, std::to_string(primCount).c_str(), caseDef));
            }
            trianglesIndexedGroup->addChild(trianglesIndexedPrimCount.release());
            trianglesNoIndexGroup->addChild(trianglesNoIndexPrimCount.release());
            aabbsGroup->addChild(aabbPrimCount.release());
        }
        { // TLAS primitive_count
            de::MovePtr<tcu::TestCaseGroup> instancePrimCount(new tcu::TestCaseGroup(testCtx, "primitive_count"));

            CaseDef caseDef           = {};
            caseDef.doUpdate          = doUpdate;
            caseDef.maxInstancesCount = 4;
            for (uint32_t instancesCount = 1; instancesCount <= 4; ++instancesCount)
            {
                caseDef.instancesCount = instancesCount;
                instancePrimCount->addChild(new RayTracingTestCase<RayTracingBuildInstances, CaseDef>(
                    testCtx, std::to_string(instancesCount).c_str(), caseDef));
            }
            instancesGroup->addChild(instancePrimCount.release());
        }
        { // BLAS primitive_offset
            de::MovePtr<tcu::TestCaseGroup> trianglesIndexedPrimOffset(
                new tcu::TestCaseGroup(testCtx, "primitive_offset"));
            de::MovePtr<tcu::TestCaseGroup> trianglesNoIndexPrimOffset(
                new tcu::TestCaseGroup(testCtx, "primitive_offset"));
            de::MovePtr<tcu::TestCaseGroup> aabbPrimOffset(new tcu::TestCaseGroup(testCtx, "primitive_offset"));

            CaseDef caseDef  = {};
            caseDef.doUpdate = doUpdate;
            for (uint32_t primOffset = 8; primOffset <= 8 * 6; primOffset += 8)
            {
                caseDef.primitiveOffset = primOffset;
                trianglesIndexedPrimOffset->addChild(new RayTracingTestCase<RayTracingBuildTrianglesIndexed, CaseDef>(
                    testCtx, std::to_string(primOffset).c_str(), caseDef));
                trianglesNoIndexPrimOffset->addChild(
                    new RayTracingTestCase<RayTracingBuildIndirectTestInstance, CaseDef>(
                        testCtx, std::to_string(primOffset).c_str(), caseDef));
                aabbPrimOffset->addChild(new RayTracingTestCase<RayTracingBuildAABBs, CaseDef>(
                    testCtx, std::to_string(primOffset).c_str(), caseDef));
            }
            trianglesIndexedGroup->addChild(trianglesIndexedPrimOffset.release());
            trianglesNoIndexGroup->addChild(trianglesNoIndexPrimOffset.release());
            aabbsGroup->addChild(aabbPrimOffset.release());
        }
        { // TLAS primitive_offset
            de::MovePtr<tcu::TestCaseGroup> instancePrimOffset(new tcu::TestCaseGroup(testCtx, "primitive_offset"));

            CaseDef caseDef           = {};
            caseDef.doUpdate          = doUpdate;
            caseDef.instancesCount    = 4;
            caseDef.maxInstancesCount = 4;
            for (uint32_t primOffset = 16; primOffset <= 16 * 8; primOffset += 16)
            {
                caseDef.instancesOffset = primOffset;
                instancePrimOffset->addChild(new RayTracingTestCase<RayTracingBuildInstances, CaseDef>(
                    testCtx, std::to_string(primOffset).c_str(), caseDef));
            }
            instancesGroup->addChild(instancePrimOffset.release());
        }
        { // Triangles first_vertex
            de::MovePtr<tcu::TestCaseGroup> trianglesIndexedFirstVert(new tcu::TestCaseGroup(testCtx, "first_vertex"));
            de::MovePtr<tcu::TestCaseGroup> trianglesNoIndexFirstVert(new tcu::TestCaseGroup(testCtx, "first_vertex"));

            CaseDef caseDef  = {};
            caseDef.doUpdate = doUpdate;
            for (uint32_t firstVert = 1; firstVert <= 8; ++firstVert)
            {
                caseDef.firstVertex = firstVert;
                trianglesIndexedFirstVert->addChild(new RayTracingTestCase<RayTracingBuildTrianglesIndexed, CaseDef>(
                    testCtx, std::to_string(firstVert).c_str(), caseDef));
                trianglesNoIndexFirstVert->addChild(
                    new RayTracingTestCase<RayTracingBuildIndirectTestInstance, CaseDef>(
                        testCtx, std::to_string(firstVert).c_str(), caseDef));
            }

            trianglesNoIndexGroup->addChild(trianglesNoIndexFirstVert.release());
            trianglesIndexedGroup->addChild(trianglesIndexedFirstVert.release());
        }
        { // Triangles transform_offset
            de::MovePtr<tcu::TestCaseGroup> trianglesIndexedTransformOffset(
                new tcu::TestCaseGroup(testCtx, "transform_offset"));
            de::MovePtr<tcu::TestCaseGroup> trianglesNoIndexTransformOffset(
                new tcu::TestCaseGroup(testCtx, "transform_offset"));

            CaseDef caseDef  = {};
            caseDef.doUpdate = doUpdate;
            for (uint32_t transformOffset = 16; transformOffset <= 16 * 8; transformOffset += 16)
            {
                caseDef.transformOffset = transformOffset;
                trianglesIndexedTransformOffset->addChild(
                    new RayTracingTestCase<RayTracingBuildTrianglesIndexed, CaseDef>(
                        testCtx, std::to_string(transformOffset).c_str(), caseDef));
                trianglesNoIndexTransformOffset->addChild(
                    new RayTracingTestCase<RayTracingBuildIndirectTestInstance, CaseDef>(
                        testCtx, std::to_string(transformOffset).c_str(), caseDef));
            }

            trianglesNoIndexGroup->addChild(trianglesNoIndexTransformOffset.release());
            trianglesIndexedGroup->addChild(trianglesIndexedTransformOffset.release());
        }

        group->addChild(trianglesIndexedGroup.release());
        group->addChild(trianglesNoIndexGroup.release());
        group->addChild(aabbsGroup.release());
        group->addChild(instancesGroup.release());
    };

    de::MovePtr<tcu::TestCaseGroup> accelerationStrucutreGroup(
        new tcu::TestCaseGroup(testCtx, "indirect_acceleration_structure"));

    de::MovePtr<tcu::TestCaseGroup> buildGroup(new tcu::TestCaseGroup(testCtx, "build"));
    addIndirectTests(false, buildGroup);
    accelerationStrucutreGroup->addChild(buildGroup.release());

    de::MovePtr<tcu::TestCaseGroup> updateGroup(new tcu::TestCaseGroup(testCtx, "update"));
    addIndirectTests(true, updateGroup);
    accelerationStrucutreGroup->addChild(updateGroup.release());

    return accelerationStrucutreGroup.release();
}

} // namespace RayTracing
} // namespace vkt
