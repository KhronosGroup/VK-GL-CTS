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
 * \brief Ray Query Opacity Micromap Tests
 *//*--------------------------------------------------------------------*/

#include "vktRayQueryOpacityMicromapTests.hpp"
#include "vktTestCase.hpp"

#include "vkRayTracingUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "deUniquePtr.hpp"
#include "deRandom.hpp"

#include <sstream>
#include <vector>
#include <iostream>

namespace vkt
{
namespace RayQuery
{

namespace
{

using namespace vk;

enum ShaderSourcePipeline
{
    SSP_GRAPHICS_PIPELINE,
    SSP_COMPUTE_PIPELINE,
    SSP_RAY_TRACING_PIPELINE
};

enum ShaderSourceType
{
    SST_VERTEX_SHADER,
    SST_COMPUTE_SHADER,
    SST_RAY_GENERATION_SHADER,
};

enum TestFlagBits
{
    TEST_FLAG_BIT_FORCE_OPAQUE_INSTANCE             = 1U << 0,
    TEST_FLAG_BIT_FORCE_OPAQUE_RAY_FLAG             = 1U << 1,
    TEST_FLAG_BIT_DISABLE_OPACITY_MICROMAP_INSTANCE = 1U << 2,
    TEST_FLAG_BIT_FORCE_2_STATE_INSTANCE            = 1U << 3,
    TEST_FLAG_BIT_FORCE_2_STATE_RAY_FLAG            = 1U << 4,
    TEST_FLAG_BIT_LAST                              = 1U << 5,
};

std::vector<std::string> testFlagBitNames = {
    "force_opaque_instance",  "force_opaque_ray_flag",  "disable_opacity_micromap_instance",
    "force_2_state_instance", "force_2_state_ray_flag",
};

enum CopyType
{
    CT_NONE,
    CT_FIRST_ACTIVE,
    CT_CLONE = CT_FIRST_ACTIVE,
    CT_COMPACT,
    CT_NUM_COPY_TYPES,
};

std::vector<std::string> copyTypeNames{
    "None",
    "Clone",
    "Compact",
};

struct TestParams
{
    ShaderSourceType shaderSourceType;
    ShaderSourcePipeline shaderSourcePipeline;
    bool useSpecialIndex;
    uint32_t testFlagMask;
    uint32_t subdivisionLevel; // Must be 0 for useSpecialIndex
    uint32_t mode;             // Special index value if useSpecialIndex, 2 or 4 for number of states otherwise
    uint32_t seed;
    CopyType copyType;
    bool useMaintenance5;
};

static constexpr uint32_t kNumThreadsAtOnce = 1024;

class OpacityMicromapCase : public TestCase
{
public:
    OpacityMicromapCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params);
    virtual ~OpacityMicromapCase(void)
    {
    }

    virtual void checkSupport(Context &context) const;
    virtual void initPrograms(vk::SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;

protected:
    TestParams m_params;
};

class OpacityMicromapInstance : public TestInstance
{
public:
    OpacityMicromapInstance(Context &context, const TestParams &params);
    virtual ~OpacityMicromapInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void);

protected:
    TestParams m_params;
};

OpacityMicromapCase::OpacityMicromapCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
    : TestCase(testCtx, name)
    , m_params(params)
{
}

void OpacityMicromapCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_ray_query");
    context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
    context.requireDeviceFunctionality("VK_EXT_opacity_micromap");

    if (m_params.useMaintenance5)
        context.requireDeviceFunctionality("VK_KHR_maintenance5");

    const VkPhysicalDeviceRayQueryFeaturesKHR &rayQueryFeaturesKHR = context.getRayQueryFeatures();
    if (rayQueryFeaturesKHR.rayQuery == false)
        TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayQueryFeaturesKHR.rayQuery");

    const VkPhysicalDeviceAccelerationStructureFeaturesKHR &accelerationStructureFeaturesKHR =
        context.getAccelerationStructureFeatures();
    if (accelerationStructureFeaturesKHR.accelerationStructure == false)
        TCU_THROW(TestError,
                  "VK_KHR_ray_query requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");

    const VkPhysicalDeviceOpacityMicromapFeaturesEXT &opacityMicromapFeaturesEXT =
        context.getOpacityMicromapFeaturesEXT();
    if (opacityMicromapFeaturesEXT.micromap == false)
        TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceOpacityMicromapFeaturesEXT.micromap");

    if (m_params.shaderSourceType == SST_RAY_GENERATION_SHADER)
    {
        context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

        const VkPhysicalDeviceRayTracingPipelineFeaturesKHR &rayTracingPipelineFeaturesKHR =
            context.getRayTracingPipelineFeatures();

        if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == false)
            TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");
    }

    switch (m_params.shaderSourceType)
    {
    case SST_VERTEX_SHADER:
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS);
        break;
    default:
        break;
    }

    const VkPhysicalDeviceOpacityMicromapPropertiesEXT &opacityMicromapPropertiesEXT =
        context.getOpacityMicromapPropertiesEXT();

    if (!m_params.useSpecialIndex)
    {
        switch (m_params.mode)
        {
        case 2:
            if (m_params.subdivisionLevel > opacityMicromapPropertiesEXT.maxOpacity2StateSubdivisionLevel)
                TCU_THROW(NotSupportedError, "Requires a higher supported 2 state subdivision level");
            break;
        case 4:
            if (m_params.subdivisionLevel > opacityMicromapPropertiesEXT.maxOpacity4StateSubdivisionLevel)
                TCU_THROW(NotSupportedError, "Requires a higher supported 4 state subdivision level");
            break;
        default:
            DE_ASSERT(false);
            break;
        }
    }
}

static uint32_t levelToSubtriangles(uint32_t level)
{
    return 1 << (2 * level);
}

void OpacityMicromapCase::initPrograms(vk::SourceCollections &programCollection) const
{
    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

    uint32_t numRays = levelToSubtriangles(m_params.subdivisionLevel);

    std::string flagsString =
        (m_params.testFlagMask & TEST_FLAG_BIT_FORCE_OPAQUE_RAY_FLAG) ? "gl_RayFlagsOpaqueEXT" : "gl_RayFlagsNoneEXT";

    if (m_params.testFlagMask & TEST_FLAG_BIT_FORCE_2_STATE_RAY_FLAG)
        flagsString += " | gl_RayFlagsForceOpacityMicromap2StateEXT";

    std::ostringstream sharedHeader;
    sharedHeader << "#version 460 core\n"
                 << "#extension GL_EXT_ray_query : require\n"
                 << "#extension GL_EXT_opacity_micromap : require\n"
                 << "\n"
                 << "layout(set=0, binding=0) uniform accelerationStructureEXT topLevelAS;\n"
                 << "layout(set=0, binding=1, std430) buffer RayOrigins {\n"
                 << "  vec4 values[" << numRays << "];\n"
                 << "} origins;\n"
                 << "layout(set=0, binding=2, std430) buffer OutputModes {\n"
                 << "  uint values[" << numRays << "];\n"
                 << "} modes;\n";

    std::ostringstream mainLoop;
    mainLoop
        << "  while (index < " << numRays << ") {\n"
        << "    const uint  cullMask  = 0xFF;\n"
        << "    const vec3  origin    = origins.values[index].xyz;\n"
        << "    const vec3  direction = vec3(0.0, 0.0, -1.0);\n"
        << "    const float tMin      = 0.0f;\n"
        << "    const float tMax      = 2.0f;\n"
        << "    uint        outputVal = 0;\n" // 0 for miss, 1 for non-opaque, 2 for opaque
        << "    rayQueryEXT rq;\n"
        << "    rayQueryInitializeEXT(rq, topLevelAS, " << flagsString
        << ", cullMask, origin, tMin, direction, tMax);\n"
        << "    while (rayQueryProceedEXT(rq)) {\n"
        << "      if (rayQueryGetIntersectionTypeEXT(rq, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {\n"
        << "        outputVal = 1;\n"
        << "      }\n"
        << "    }\n"
        << "    if (rayQueryGetIntersectionTypeEXT(rq, true) == gl_RayQueryCommittedIntersectionTriangleEXT) {\n"
        << "      outputVal = 2;\n"
        << "    }\n"
        << "    modes.values[index] = outputVal;\n"
        << "    index += " << kNumThreadsAtOnce << ";\n"
        << "  }\n";

    if (m_params.shaderSourceType == SST_VERTEX_SHADER)
    {
        std::ostringstream vert;
        vert << sharedHeader.str() << "void main()\n"
             << "{\n"
             << "  uint index             = gl_VertexIndex.x;\n"
             << mainLoop.str() << "  gl_PointSize = 1.0f;\n"
             << "}\n";

        programCollection.glslSources.add("vert") << glu::VertexSource(vert.str()) << buildOptions;
    }
    else if (m_params.shaderSourceType == SST_RAY_GENERATION_SHADER)
    {
        std::ostringstream rgen;
        rgen << sharedHeader.str() << "#extension GL_EXT_ray_tracing : require\n"
             << "void main()\n"
             << "{\n"
             << "  uint index             = gl_LaunchIDEXT.x;\n"
             << mainLoop.str() << "}\n";

        programCollection.glslSources.add("rgen")
            << glu::RaygenSource(updateRayTracingGLSL(rgen.str())) << buildOptions;
    }
    else
    {
        DE_ASSERT(m_params.shaderSourceType == SST_COMPUTE_SHADER);
        std::ostringstream comp;
        comp << sharedHeader.str() << "layout(local_size_x=1024, local_size_y=1, local_size_z=1) in;\n"
             << "\n"
             << "void main()\n"
             << "{\n"
             << "  uint index             = gl_LocalInvocationID.x;\n"
             << mainLoop.str() << "}\n";

        programCollection.glslSources.add("comp")
            << glu::ComputeSource(updateRayTracingGLSL(comp.str())) << buildOptions;
    }
}

TestInstance *OpacityMicromapCase::createInstance(Context &context) const
{
    return new OpacityMicromapInstance(context, m_params);
}

OpacityMicromapInstance::OpacityMicromapInstance(Context &context, const TestParams &params)
    : TestInstance(context)
    , m_params(params)
{
}

tcu::Vec2 calcSubtriangleCentroid(const uint32_t index, const uint32_t subdivisionLevel)
{
    if (subdivisionLevel == 0)
    {
        return tcu::Vec2(1.0f / 3.0f, 1.0f / 3.0f);
    }

    uint32_t d = index;

    d = ((d >> 1) & 0x22222222u) | ((d << 1) & 0x44444444u) | (d & 0x99999999u);
    d = ((d >> 2) & 0x0c0c0c0cu) | ((d << 2) & 0x30303030u) | (d & 0xc3c3c3c3u);
    d = ((d >> 4) & 0x00f000f0u) | ((d << 4) & 0x0f000f00u) | (d & 0xf00ff00fu);
    d = ((d >> 8) & 0x0000ff00u) | ((d << 8) & 0x00ff0000u) | (d & 0xff0000ffu);

    uint32_t f = (d & 0xffffu) | ((d << 16) & ~d);

    f ^= (f >> 1) & 0x7fff7fffu;
    f ^= (f >> 2) & 0x3fff3fffu;
    f ^= (f >> 4) & 0x0fff0fffu;
    f ^= (f >> 8) & 0x00ff00ffu;

    uint32_t t = (f ^ d) >> 16;

    uint32_t iu = ((f & ~t) | (d & ~t) | (~d & ~f & t)) & 0xffffu;
    uint32_t iv = ((f >> 16) ^ d) & 0xffffu;
    uint32_t iw = ((~f & ~t) | (d & ~t) | (~d & f & t)) & ((1 << subdivisionLevel) - 1);

    const float scale = 1.0f / float(1 << subdivisionLevel);

    float u = (1.0f / 3.0f) * scale;
    float v = (1.0f / 3.0f) * scale;

    // we need to only look at "subdivisionLevel" bits
    iu = iu & ((1 << subdivisionLevel) - 1);
    iv = iv & ((1 << subdivisionLevel) - 1);
    iw = iw & ((1 << subdivisionLevel) - 1);

    bool upright = (iu & 1) ^ (iv & 1) ^ (iw & 1);
    if (!upright)
    {
        iu = iu + 1;
        iv = iv + 1;
    }

    if (upright)
    {
        return tcu::Vec2(u + (float)iu * scale, v + (float)iv * scale);
    }
    else
    {
        return tcu::Vec2((float)iu * scale - u, (float)iv * scale - v);
    }
}

static Move<VkRenderPass> makeEmptyRenderPass(const DeviceInterface &vk, const VkDevice device)
{
    std::vector<VkSubpassDescription> subpassDescriptions;
    std::vector<VkSubpassDependency> subpassDependencies;

    const VkSubpassDescription description = {
        (VkSubpassDescriptionFlags)0,    //  VkSubpassDescriptionFlags flags;
        VK_PIPELINE_BIND_POINT_GRAPHICS, //  VkPipelineBindPoint pipelineBindPoint;
        0u,                              //  uint32_t inputAttachmentCount;
        DE_NULL,                         //  const VkAttachmentReference* pInputAttachments;
        0u,                              //  uint32_t colorAttachmentCount;
        DE_NULL,                         //  const VkAttachmentReference* pColorAttachments;
        DE_NULL,                         //  const VkAttachmentReference* pResolveAttachments;
        DE_NULL,                         //  const VkAttachmentReference* pDepthStencilAttachment;
        0,                               //  uint32_t preserveAttachmentCount;
        DE_NULL                          //  const uint32_t* pPreserveAttachments;
    };
    subpassDescriptions.push_back(description);

    const VkSubpassDependency dependency = {
        0u,                                   //  uint32_t srcSubpass;
        0u,                                   //  uint32_t dstSubpass;
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,  //  VkPipelineStageFlags srcStageMask;
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, //  VkPipelineStageFlags dstStageMask;
        VK_ACCESS_SHADER_WRITE_BIT,           //  VkAccessFlags srcAccessMask;
        VK_ACCESS_MEMORY_READ_BIT,            //  VkAccessFlags dstAccessMask;
        0u                                    //  VkDependencyFlags dependencyFlags;
    };
    subpassDependencies.push_back(dependency);

    const VkRenderPassCreateInfo renderPassInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,         //  VkStructureType sType;
        DE_NULL,                                           //  const void* pNext;
        static_cast<VkRenderPassCreateFlags>(0u),          //  VkRenderPassCreateFlags flags;
        0u,                                                //  uint32_t attachmentCount;
        DE_NULL,                                           //  const VkAttachmentDescription* pAttachments;
        static_cast<uint32_t>(subpassDescriptions.size()), //  uint32_t subpassCount;
        &subpassDescriptions[0],                           //  const VkSubpassDescription* pSubpasses;
        static_cast<uint32_t>(subpassDependencies.size()), //  uint32_t dependencyCount;
        subpassDependencies.size() > 0 ? &subpassDependencies[0] : DE_NULL //  const VkSubpassDependency* pDependencies;
    };

    return createRenderPass(vk, device, &renderPassInfo);
}

Move<VkPipeline> makeGraphicsPipeline(const DeviceInterface &vk, const VkDevice device,
                                      const VkPipelineLayout pipelineLayout, const VkRenderPass renderPass,
                                      const VkShaderModule vertexModule, const uint32_t subpass)
{
    VkExtent2D renderSize{256, 256};
    VkViewport viewport = makeViewport(renderSize);
    VkRect2D scissor    = makeRect2D(renderSize);

    const VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType                             sType
        DE_NULL,                                               // const void*                                 pNext
        (VkPipelineViewportStateCreateFlags)0,                 // VkPipelineViewportStateCreateFlags          flags
        1u,        // uint32_t                                    viewportCount
        &viewport, // const VkViewport*                           pViewports
        1u,        // uint32_t                                    scissorCount
        &scissor   // const VkRect2D*                             pScissors
    };

    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType                            sType
        DE_NULL,                                                     // const void*                                pNext
        0u,                                                          // VkPipelineInputAssemblyStateCreateFlags    flags
        VK_PRIMITIVE_TOPOLOGY_POINT_LIST, // VkPrimitiveTopology                        topology
        VK_FALSE                          // VkBool32                                   primitiveRestartEnable
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, //  VkStructureType                                    sType
        DE_NULL,                                  //  const void*                                        pNext
        (VkPipelineVertexInputStateCreateFlags)0, //  VkPipelineVertexInputStateCreateFlags            flags
        0u,      //  uint32_t                                        vertexBindingDescriptionCount
        DE_NULL, //  const VkVertexInputBindingDescription*            pVertexBindingDescriptions
        0u,      //  uint32_t                                        vertexAttributeDescriptionCount
        DE_NULL, //  const VkVertexInputAttributeDescription*        pVertexAttributeDescriptions
    };

    const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, //  VkStructureType                            sType
        DE_NULL,                                                    //  const void*                                pNext
        0u,                                                         //  VkPipelineRasterizationStateCreateFlags    flags
        VK_FALSE,                        //  VkBool32                                depthClampEnable
        VK_TRUE,                         //  VkBool32                                rasterizerDiscardEnable
        VK_POLYGON_MODE_FILL,            //  VkPolygonMode                            polygonMode
        VK_CULL_MODE_NONE,               //  VkCullModeFlags                            cullMode
        VK_FRONT_FACE_COUNTER_CLOCKWISE, //  VkFrontFace                                frontFace
        VK_FALSE,                        //  VkBool32                                depthBiasEnable
        0.0f,                            //  float                                    depthBiasConstantFactor
        0.0f,                            //  float                                    depthBiasClamp
        0.0f,                            //  float                                    depthBiasSlopeFactor
        1.0f                             //  float                                    lineWidth
    };

    return makeGraphicsPipeline(
        vk,                             // const DeviceInterface&                            vk
        device,                         // const VkDevice                                    device
        pipelineLayout,                 // const VkPipelineLayout                            pipelineLayout
        vertexModule,                   // const VkShaderModule                                vertexShaderModule
        DE_NULL,                        // const VkShaderModule                                tessellationControlModule
        DE_NULL,                        // const VkShaderModule                                tessellationEvalModule
        DE_NULL,                        // const VkShaderModule                                geometryShaderModule
        DE_NULL,                        // const VkShaderModule                                fragmentShaderModule
        renderPass,                     // const VkRenderPass                                renderPass
        subpass,                        // const uint32_t                                    subpass
        &vertexInputStateCreateInfo,    // const VkPipelineVertexInputStateCreateInfo*        vertexInputStateCreateInfo
        &inputAssemblyStateCreateInfo,  // const VkPipelineInputAssemblyStateCreateInfo*    inputAssemblyStateCreateInfo
        DE_NULL,                        // const VkPipelineTessellationStateCreateInfo*        tessStateCreateInfo
        &viewportStateCreateInfo,       // const VkPipelineViewportStateCreateInfo*            viewportStateCreateInfo
        &rasterizationStateCreateInfo); // const VkPipelineRasterizationStateCreateInfo*    rasterizationStateCreateInfo
}

tcu::TestStatus OpacityMicromapInstance::iterate(void)
{
    const auto &vkd   = m_context.getDeviceInterface();
    const auto device = m_context.getDevice();
    auto &alloc       = m_context.getDefaultAllocator();
    const auto qIndex = m_context.getUniversalQueueFamilyIndex();
    const auto queue  = m_context.getUniversalQueue();

    // Command pool and buffer.
    const auto cmdPool      = makeCommandPool(vkd, device, qIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    beginCommandBuffer(vkd, cmdBuffer);

    // Build acceleration structures.
    auto topLevelAS    = makeTopLevelAccelerationStructure();
    auto bottomLevelAS = makeBottomLevelAccelerationStructure();

    uint32_t numSubtriangles      = levelToSubtriangles(m_params.subdivisionLevel);
    uint32_t opacityMicromapBytes = (m_params.mode == 2) ? (numSubtriangles + 3) / 4 : (numSubtriangles + 1) / 2;

    // Generate random micromap data
    std::vector<uint8_t> opacityMicromapData;

    de::Random rnd(m_params.seed);

    while (opacityMicromapData.size() < opacityMicromapBytes)
    {
        opacityMicromapData.push_back(rnd.getUint8());
    }

    // Build a micromap (ignore infrastructure for now)
    // Create the buffer with the mask and index data
    // Allocate a fairly conservative bound for now
    VkBufferUsageFlags2CreateInfoKHR bufferUsageFlags2 = initVulkanStructure();
    ;
    const auto micromapDataBufferSize = static_cast<VkDeviceSize>(1024 + opacityMicromapBytes);
    auto micromapDataBufferCreateInfo =
        makeBufferCreateInfo(micromapDataBufferSize, VK_BUFFER_USAGE_MICROMAP_BUILD_INPUT_READ_ONLY_BIT_EXT |
                                                         VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    if (m_params.useMaintenance5)
    {
        bufferUsageFlags2.usage            = (VkBufferUsageFlagBits2KHR)micromapDataBufferCreateInfo.usage;
        micromapDataBufferCreateInfo.pNext = &bufferUsageFlags2;
        micromapDataBufferCreateInfo.usage = 0;
    }

    BufferWithMemory micromapDataBuffer(vkd, device, alloc, micromapDataBufferCreateInfo,
                                        MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress);
    auto &micromapDataBufferAlloc = micromapDataBuffer.getAllocation();
    void *micromapDataBufferData  = micromapDataBufferAlloc.getHostPtr();

    const int TriangleOffset = 0;
    const int IndexOffset    = 256;
    const int DataOffset     = 512;

    // Fill out VkMicromapUsageEXT with size information
    VkMicromapUsageEXT mmUsage = {};
    mmUsage.count              = 1;
    mmUsage.subdivisionLevel   = m_params.subdivisionLevel;
    mmUsage.format =
        m_params.mode == 2 ? VK_OPACITY_MICROMAP_FORMAT_2_STATE_EXT : VK_OPACITY_MICROMAP_FORMAT_4_STATE_EXT;

    {
        uint8_t *data = static_cast<uint8_t *>(micromapDataBufferData);

        deMemset(data, 0, size_t(micromapDataBufferCreateInfo.size));

        DE_STATIC_ASSERT(sizeof(VkMicromapTriangleEXT) == 8);

        // Triangle information
        VkMicromapTriangleEXT *tri = (VkMicromapTriangleEXT *)(&data[TriangleOffset]);
        tri->dataOffset            = 0;
        tri->subdivisionLevel      = uint16_t(mmUsage.subdivisionLevel);
        tri->format                = uint16_t(mmUsage.format);

        // Micromap data
        {
            for (size_t i = 0; i < opacityMicromapData.size(); i++)
            {
                data[DataOffset + i] = opacityMicromapData[i];
            }
        }

        // Index information
        *((uint32_t *)&data[IndexOffset]) = m_params.useSpecialIndex ? m_params.mode : 0;
    }

    // Query the size from the build info
    VkMicromapBuildInfoEXT mmBuildInfo = {
        VK_STRUCTURE_TYPE_MICROMAP_BUILD_INFO_EXT, // VkStructureType sType;
        DE_NULL,                                   // const void* pNext;
        VK_MICROMAP_TYPE_OPACITY_MICROMAP_EXT,     // VkMicromapTypeEXT type;
        0,                                         // VkBuildMicromapFlagsEXT flags;
        VK_BUILD_MICROMAP_MODE_BUILD_EXT,          // VkBuildMicromapModeEXT mode;
        DE_NULL,                                   // VkMicromapEXT dstMicromap;
        1,                                         // uint32_t usageCountsCount;
        &mmUsage,                                  // const VkMicromapUsageEXT* pUsageCounts;
        DE_NULL,                                   // const VkMicromapUsageEXT* const* ppUsageCounts;
        makeDeviceOrHostAddressConstKHR(DE_NULL),  // VkDeviceOrHostAddressConstKHR data;
        makeDeviceOrHostAddressKHR(DE_NULL),       // VkDeviceOrHostAddressKHR scratchData;
        makeDeviceOrHostAddressConstKHR(DE_NULL),  // VkDeviceOrHostAddressConstKHR triangleArray;
        0,                                         // VkDeviceSize triangleArrayStride;
    };

    VkMicromapBuildSizesInfoEXT sizeInfo = {
        VK_STRUCTURE_TYPE_MICROMAP_BUILD_SIZES_INFO_EXT, // VkStructureType sType;
        DE_NULL,                                         // const void* pNext;
        0,                                               // VkDeviceSize micromapSize;
        0,                                               // VkDeviceSize buildScratchSize;
        false,                                           // VkBool32 discardable;
    };

    vkd.getMicromapBuildSizesEXT(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &mmBuildInfo, &sizeInfo);

    // Create the backing and scratch storage
    const auto micromapBackingBufferCreateInfo = makeBufferCreateInfo(
        sizeInfo.micromapSize, VK_BUFFER_USAGE_MICROMAP_STORAGE_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    BufferWithMemory micromapBackingBuffer(vkd, device, alloc, micromapBackingBufferCreateInfo,
                                           MemoryRequirement::Local | MemoryRequirement::DeviceAddress);

    auto micromapScratchBufferCreateInfo =
        makeBufferCreateInfo(sizeInfo.buildScratchSize,
                             VK_BUFFER_USAGE_MICROMAP_STORAGE_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    if (m_params.useMaintenance5)
    {
        bufferUsageFlags2.usage               = (VkBufferUsageFlagBits2KHR)micromapScratchBufferCreateInfo.usage;
        micromapScratchBufferCreateInfo.pNext = &bufferUsageFlags2;
        micromapScratchBufferCreateInfo.usage = 0;
    }
    BufferWithMemory micromapScratchBuffer(vkd, device, alloc, micromapScratchBufferCreateInfo,
                                           MemoryRequirement::Local | MemoryRequirement::DeviceAddress);

    de::MovePtr<BufferWithMemory> copyMicromapBackingBuffer;

    // Create the micromap itself
    VkMicromapCreateInfoEXT maCreateInfo = {
        VK_STRUCTURE_TYPE_MICROMAP_CREATE_INFO_EXT, // VkStructureType sType;
        DE_NULL,                                    // const void* pNext;
        0,                                          // VkMicromapCreateFlagsEXT createFlags;
        micromapBackingBuffer.get(),                // VkBuffer buffer;
        0,                                          // VkDeviceSize offset;
        sizeInfo.micromapSize,                      // VkDeviceSize size;
        VK_MICROMAP_TYPE_OPACITY_MICROMAP_EXT,      // VkMicromapTypeEXT type;
        0ull                                        // VkDeviceAddress deviceAddress;
    };

    VkMicromapEXT micromap = VK_NULL_HANDLE, origMicromap = VK_NULL_HANDLE;

    VK_CHECK(vkd.createMicromapEXT(device, &maCreateInfo, nullptr, &micromap));

    // Do the build
    mmBuildInfo.dstMicromap   = micromap;
    mmBuildInfo.data          = makeDeviceOrHostAddressConstKHR(vkd, device, micromapDataBuffer.get(), DataOffset);
    mmBuildInfo.triangleArray = makeDeviceOrHostAddressConstKHR(vkd, device, micromapDataBuffer.get(), TriangleOffset);
    mmBuildInfo.scratchData   = makeDeviceOrHostAddressKHR(vkd, device, micromapScratchBuffer.get(), 0);

    vkd.cmdBuildMicromapsEXT(cmdBuffer, 1, &mmBuildInfo);

    {
        VkMemoryBarrier2 memoryBarrier     = {VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                                              NULL,
                                              VK_PIPELINE_STAGE_2_MICROMAP_BUILD_BIT_EXT,
                                              VK_ACCESS_2_MICROMAP_WRITE_BIT_EXT,
                                              VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                              VK_ACCESS_2_MICROMAP_READ_BIT_EXT};
        VkDependencyInfoKHR dependencyInfo = {
            VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR, // VkStructureType sType;
            DE_NULL,                               // const void* pNext;
            0u,                                    // VkDependencyFlags dependencyFlags;
            1u,                                    // uint32_t memoryBarrierCount;
            &memoryBarrier,                        // const VkMemoryBarrier2KHR* pMemoryBarriers;
            0u,                                    // uint32_t bufferMemoryBarrierCount;
            DE_NULL,                               // const VkBufferMemoryBarrier2KHR* pBufferMemoryBarriers;
            0u,                                    // uint32_t imageMemoryBarrierCount;
            DE_NULL,                               // const VkImageMemoryBarrier2KHR* pImageMemoryBarriers;
        };

        vkd.cmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
    }

    if (m_params.copyType != CT_NONE)
    {
        copyMicromapBackingBuffer = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vkd, device, alloc, micromapBackingBufferCreateInfo,
                                 MemoryRequirement::Local | MemoryRequirement::DeviceAddress));

        origMicromap = micromap;

        maCreateInfo.buffer = copyMicromapBackingBuffer->get();

        VK_CHECK(vkd.createMicromapEXT(device, &maCreateInfo, nullptr, &micromap));

        VkCopyMicromapInfoEXT copyMicromapInfo = {
            VK_STRUCTURE_TYPE_COPY_MICROMAP_INFO_EXT, // VkStructureType sType;
            DE_NULL,                                  // const void* pNext;
            origMicromap,                             // VkMicromapEXT src;
            micromap,                                 // VkMicromapEXT dst;
            VK_COPY_MICROMAP_MODE_CLONE_EXT           // VkCopyMicromapModeEXT mode;
        };

        vkd.cmdCopyMicromapEXT(cmdBuffer, &copyMicromapInfo);

        {
            VkMemoryBarrier2 memoryBarrier     = {VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                                                  NULL,
                                                  VK_PIPELINE_STAGE_2_MICROMAP_BUILD_BIT_EXT,
                                                  VK_ACCESS_2_MICROMAP_WRITE_BIT_EXT,
                                                  VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                                  VK_ACCESS_2_MICROMAP_READ_BIT_EXT};
            VkDependencyInfoKHR dependencyInfo = {
                VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR, // VkStructureType sType;
                DE_NULL,                               // const void* pNext;
                0u,                                    // VkDependencyFlags dependencyFlags;
                1u,                                    // uint32_t memoryBarrierCount;
                &memoryBarrier,                        // const VkMemoryBarrier2KHR* pMemoryBarriers;
                0u,                                    // uint32_t bufferMemoryBarrierCount;
                DE_NULL,                               // const VkBufferMemoryBarrier2KHR* pBufferMemoryBarriers;
                0u,                                    // uint32_t imageMemoryBarrierCount;
                DE_NULL,                               // const VkImageMemoryBarrier2KHR* pImageMemoryBarriers;
            };

            dependencyInfo.memoryBarrierCount = 1;
            dependencyInfo.pMemoryBarriers    = &memoryBarrier;

            vkd.cmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
        }
    }

    // Attach the micromap to the geometry
    VkAccelerationStructureTrianglesOpacityMicromapEXT opacityGeometryMicromap = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_OPACITY_MICROMAP_EXT, //VkStructureType sType;
        DE_NULL,                                                                 //void* pNext;
        VK_INDEX_TYPE_UINT32,                                                    //VkIndexType indexType;
        makeDeviceOrHostAddressConstKHR(vkd, device, micromapDataBuffer.get(),
                                        IndexOffset), //VkDeviceOrHostAddressConstKHR indexBuffer;
        0u,                                           //VkDeviceSize indexStride;
        0u,                                           //uint32_t baseTriangle;
        1u,                                           //uint32_t usageCountsCount;
        &mmUsage,                                     //const VkMicromapUsageEXT* pUsageCounts;
        DE_NULL,                                      //const VkMicromapUsageEXT* const* ppUsageCounts;
        micromap                                      //VkMicromapEXT micromap;
    };

    const std::vector<tcu::Vec3> triangle = {
        tcu::Vec3(0.0f, 0.0f, 0.0f),
        tcu::Vec3(1.0f, 0.0f, 0.0f),
        tcu::Vec3(0.0f, 1.0f, 0.0f),
    };

    bottomLevelAS->addGeometry(triangle, true /*is triangles*/, 0, &opacityGeometryMicromap);
    if (m_params.testFlagMask & TEST_FLAG_BIT_DISABLE_OPACITY_MICROMAP_INSTANCE)
        bottomLevelAS->setBuildFlags(VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DISABLE_OPACITY_MICROMAPS_EXT);
    bottomLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc);
    de::SharedPtr<BottomLevelAccelerationStructure> blasSharedPtr(bottomLevelAS.release());

    VkGeometryInstanceFlagsKHR instanceFlags = 0;

    if (m_params.testFlagMask & TEST_FLAG_BIT_FORCE_2_STATE_INSTANCE)
        instanceFlags |= VK_GEOMETRY_INSTANCE_FORCE_OPACITY_MICROMAP_2_STATE_EXT;
    if (m_params.testFlagMask & TEST_FLAG_BIT_FORCE_OPAQUE_INSTANCE)
        instanceFlags |= VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
    if (m_params.testFlagMask & TEST_FLAG_BIT_DISABLE_OPACITY_MICROMAP_INSTANCE)
        instanceFlags |= VK_GEOMETRY_INSTANCE_DISABLE_OPACITY_MICROMAPS_EXT;

    topLevelAS->setInstanceCount(1);
    topLevelAS->addInstance(blasSharedPtr, identityMatrix3x4, 0, 0xFFu, 0u, instanceFlags);
    topLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc);

    // One ray per subtriangle for this test
    uint32_t numRays = numSubtriangles;

    // SSBO buffer for origins.
    const auto originsBufferSize = static_cast<VkDeviceSize>(sizeof(tcu::Vec4) * numRays);
    auto originsBufferInfo       = makeBufferCreateInfo(originsBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    if (m_params.useMaintenance5)
    {
        bufferUsageFlags2.usage = (VkBufferUsageFlagBits2KHR)originsBufferInfo.usage;
        originsBufferInfo.pNext = &bufferUsageFlags2;
        originsBufferInfo.usage = 0;
    }
    BufferWithMemory originsBuffer(vkd, device, alloc, originsBufferInfo, MemoryRequirement::HostVisible);
    auto &originsBufferAlloc = originsBuffer.getAllocation();
    void *originsBufferData  = originsBufferAlloc.getHostPtr();

    std::vector<tcu::Vec4> origins;
    std::vector<uint32_t> expectedOutputModes;
    origins.reserve(numRays);
    expectedOutputModes.reserve(numRays);

    // Fill in vector of expected outputs
    for (uint32_t index = 0; index < numRays; index++)
    {
        uint32_t state =
            m_params.testFlagMask & (TEST_FLAG_BIT_FORCE_OPAQUE_INSTANCE | TEST_FLAG_BIT_FORCE_OPAQUE_RAY_FLAG) ?
                VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_OPAQUE_EXT :
                VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_UNKNOWN_OPAQUE_EXT;

        if (!(m_params.testFlagMask & TEST_FLAG_BIT_DISABLE_OPACITY_MICROMAP_INSTANCE))
        {
            if (m_params.useSpecialIndex)
            {
                state = m_params.mode;
            }
            else
            {
                if (m_params.mode == 2)
                {
                    uint8_t byte = opacityMicromapData[index / 8];
                    state        = (byte >> (index % 8)) & 0x1;
                }
                else
                {
                    DE_ASSERT(m_params.mode == 4);
                    uint8_t byte = opacityMicromapData[index / 4];
                    state        = (byte >> 2 * (index % 4)) & 0x3;
                }
                // Process in SPECIAL_INDEX number space
                state = ~state;
            }

            if (m_params.testFlagMask & (TEST_FLAG_BIT_FORCE_2_STATE_INSTANCE | TEST_FLAG_BIT_FORCE_2_STATE_RAY_FLAG))
            {
                if (state == uint32_t(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_UNKNOWN_TRANSPARENT_EXT))
                    state = uint32_t(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_TRANSPARENT_EXT);
                if (state == uint32_t(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_UNKNOWN_OPAQUE_EXT))
                    state = uint32_t(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_OPAQUE_EXT);
            }
        }

        if (state != uint32_t(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_TRANSPARENT_EXT))
        {
            if (m_params.testFlagMask & (TEST_FLAG_BIT_FORCE_OPAQUE_INSTANCE | TEST_FLAG_BIT_FORCE_OPAQUE_RAY_FLAG))
            {
                state = uint32_t(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_OPAQUE_EXT);
            }
            else if (state != uint32_t(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_OPAQUE_EXT))
            {
                state = uint32_t(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_UNKNOWN_OPAQUE_EXT);
            }
        }

        if (state == uint32_t(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_TRANSPARENT_EXT))
        {
            expectedOutputModes.push_back(0);
        }
        else if (state == uint32_t(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_UNKNOWN_OPAQUE_EXT))
        {
            expectedOutputModes.push_back(1);
        }
        else if (state == uint32_t(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_OPAQUE_EXT))
        {
            expectedOutputModes.push_back(2);
        }
        else
        {
            DE_ASSERT(false);
        }
    }

    for (uint32_t index = 0; index < numRays; index++)
    {
        tcu::Vec2 centroid = calcSubtriangleCentroid(index, m_params.subdivisionLevel);
        origins.push_back(tcu::Vec4(centroid.x(), centroid.y(), 1.0, 0.0));
    }

    const auto originsBufferSizeSz = static_cast<size_t>(originsBufferSize);
    deMemcpy(originsBufferData, origins.data(), originsBufferSizeSz);
    flushAlloc(vkd, device, originsBufferAlloc);

    // Storage buffer for output modes
    const auto outputModesBufferSize = static_cast<VkDeviceSize>(sizeof(uint32_t) * numRays);
    const auto outputModesBufferInfo = makeBufferCreateInfo(outputModesBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory outputModesBuffer(vkd, device, alloc, outputModesBufferInfo, MemoryRequirement::HostVisible);
    auto &outputModesBufferAlloc = outputModesBuffer.getAllocation();
    void *outputModesBufferData  = outputModesBufferAlloc.getHostPtr();
    deMemset(outputModesBufferData, 0xFF, static_cast<size_t>(outputModesBufferSize));
    flushAlloc(vkd, device, outputModesBufferAlloc);

    // Descriptor set layout.
    DescriptorSetLayoutBuilder dsLayoutBuilder;
    dsLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_ALL);
    dsLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL);
    dsLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL);
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
        const auto storageBufferInfo   = makeDescriptorBufferInfo(outputModesBuffer.get(), 0ull, VK_WHOLE_SIZE);

        DescriptorSetUpdateBuilder updateBuilder;
        updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                                  VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelDescInfo);
        updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u),
                                  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &inStorageBufferInfo);
        updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(2u),
                                  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &storageBufferInfo);
        updateBuilder.update(vkd, device);
    }

    Move<VkPipeline> pipeline;
    de::MovePtr<BufferWithMemory> raygenSBT;
    Move<VkRenderPass> renderPass;
    Move<VkFramebuffer> framebuffer;

    if (m_params.shaderSourceType == SST_VERTEX_SHADER)
    {
        auto vertexModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0);

        renderPass  = makeEmptyRenderPass(vkd, device);
        framebuffer = makeFramebuffer(vkd, device, *renderPass, 0u, DE_NULL, 32, 32);
        pipeline    = makeGraphicsPipeline(vkd, device, *pipelineLayout, *renderPass, *vertexModule, 0);

        beginRenderPass(vkd, cmdBuffer, *renderPass, *framebuffer, makeRect2D(32u, 32u));
        vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
        vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u,
                                  &descriptorSet.get(), 0u, nullptr);
        vkd.cmdDraw(cmdBuffer, kNumThreadsAtOnce, 1, 0, 0);
        endRenderPass(vkd, cmdBuffer);
    }
    else if (m_params.shaderSourceType == SST_RAY_GENERATION_SHADER)
    {
        const auto &vki    = m_context.getInstanceInterface();
        const auto physDev = m_context.getPhysicalDevice();

        // Shader module.
        auto rgenModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("rgen"), 0);

        // Get some ray tracing properties.
        uint32_t shaderGroupHandleSize    = 0u;
        uint32_t shaderGroupBaseAlignment = 1u;
        {
            const auto rayTracingPropertiesKHR = makeRayTracingProperties(vki, physDev);
            shaderGroupHandleSize              = rayTracingPropertiesKHR->getShaderGroupHandleSize();
            shaderGroupBaseAlignment           = rayTracingPropertiesKHR->getShaderGroupBaseAlignment();
        }

        auto raygenSBTRegion = makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
        auto unusedSBTRegion = makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

        {
            const auto rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();
            rayTracingPipeline->setCreateFlags(VK_PIPELINE_CREATE_RAY_TRACING_OPACITY_MICROMAP_BIT_EXT);
            if (m_params.useMaintenance5)
                rayTracingPipeline->setCreateFlags2(VK_PIPELINE_CREATE_2_RAY_TRACING_OPACITY_MICROMAP_BIT_EXT);
            rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgenModule, 0);

            pipeline = rayTracingPipeline->createPipeline(vkd, device, pipelineLayout.get());

            raygenSBT = rayTracingPipeline->createShaderBindingTable(
                vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
            raygenSBTRegion = makeStridedDeviceAddressRegionKHR(
                getBufferDeviceAddress(vkd, device, raygenSBT->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
        }

        // Trace rays.
        vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.get());
        vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout.get(), 0u, 1u,
                                  &descriptorSet.get(), 0u, nullptr);
        vkd.cmdTraceRaysKHR(cmdBuffer, &raygenSBTRegion, &unusedSBTRegion, &unusedSBTRegion, &unusedSBTRegion,
                            kNumThreadsAtOnce, 1u, 1u);
    }
    else
    {
        DE_ASSERT(m_params.shaderSourceType == SST_COMPUTE_SHADER);
        // Shader module.
        const auto compModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"), 0);

        // Pipeline.
        const VkPipelineShaderStageCreateInfo shaderInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            0u,                                                  // VkPipelineShaderStageCreateFlags flags;
            VK_SHADER_STAGE_COMPUTE_BIT,                         // VkShaderStageFlagBits stage;
            compModule.get(),                                    // VkShaderModule module;
            "main",                                              // const char* pName;
            nullptr,                                             // const VkSpecializationInfo* pSpecializationInfo;
        };
        const VkComputePipelineCreateInfo pipelineInfo = {
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                        // const void* pNext;
            0u,                                             // VkPipelineCreateFlags flags;
            shaderInfo,                                     // VkPipelineShaderStageCreateInfo stage;
            pipelineLayout.get(),                           // VkPipelineLayout layout;
            DE_NULL,                                        // VkPipeline basePipelineHandle;
            0,                                              // int32_t basePipelineIndex;
        };
        pipeline = createComputePipeline(vkd, device, DE_NULL, &pipelineInfo);

        // Dispatch work with ray queries.
        vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.get());
        vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout.get(), 0u, 1u,
                                  &descriptorSet.get(), 0u, nullptr);
        vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);
    }

    // Barrier for the output buffer.
    const auto bufferBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                           &bufferBarrier, 0u, nullptr, 0u, nullptr);

    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    if (micromap != VK_NULL_HANDLE)
        vkd.destroyMicromapEXT(device, micromap, DE_NULL);
    if (micromap != VK_NULL_HANDLE)
        vkd.destroyMicromapEXT(device, origMicromap, DE_NULL);

    // Verify results.
    std::vector<uint32_t> outputData(expectedOutputModes.size());
    const auto outputModesBufferSizeSz = static_cast<size_t>(outputModesBufferSize);

    invalidateAlloc(vkd, device, outputModesBufferAlloc);
    DE_ASSERT(de::dataSize(outputData) == outputModesBufferSizeSz);
    deMemcpy(outputData.data(), outputModesBufferData, outputModesBufferSizeSz);

    for (size_t i = 0; i < outputData.size(); ++i)
    {
        const auto &outVal      = outputData[i];
        const auto &expectedVal = expectedOutputModes[i];

        if (outVal != expectedVal)
        {
            std::ostringstream msg;
            msg << "Unexpected value found for ray " << i << ": expected " << expectedVal << " and found " << outVal
                << ";";
            TCU_FAIL(msg.str());
        }
#if 0
        else
        {
            std::ostringstream msg;
            msg << "Expected value found for ray " << i << ": expected " << expectedVal << " and found " << outVal << ";\n"; // XXX Debug remove
            std::cout << msg.str();
        }
#endif
    }

    return tcu::TestStatus::pass("Pass");
}

} // namespace

constexpr uint32_t kMaxSubdivisionLevel = 15;

void addBasicTests(tcu::TestCaseGroup *group)
{
    uint32_t seed = 1614674687u;

    const struct
    {
        ShaderSourceType shaderSourceType;
        ShaderSourcePipeline shaderSourcePipeline;
        std::string name;
    } shaderSourceTypes[] = {
        {SST_VERTEX_SHADER, SSP_GRAPHICS_PIPELINE, "vertex_shader"},
        {
            SST_COMPUTE_SHADER,
            SSP_COMPUTE_PIPELINE,
            "compute_shader",
        },
        {
            SST_RAY_GENERATION_SHADER,
            SSP_RAY_TRACING_PIPELINE,
            "rgen_shader",
        },
    };

    const struct
    {
        bool useSpecialIndex;
        std::string name;
    } specialIndexUse[] = {
        {false, "map_value"},
        {true, "special_index"},
    };

    auto &testCtx = group->getTestContext();

    for (size_t shaderSourceNdx = 0; shaderSourceNdx < DE_LENGTH_OF_ARRAY(shaderSourceTypes); ++shaderSourceNdx)
    {
        de::MovePtr<tcu::TestCaseGroup> sourceTypeGroup(
            new tcu::TestCaseGroup(group->getTestContext(), shaderSourceTypes[shaderSourceNdx].name.c_str()));

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
                new tcu::TestCaseGroup(sourceTypeGroup->getTestContext(), maskName.c_str()));

            for (size_t specialIndexNdx = 0; specialIndexNdx < DE_LENGTH_OF_ARRAY(specialIndexUse); ++specialIndexNdx)
            {
                de::MovePtr<tcu::TestCaseGroup> specialGroup(new tcu::TestCaseGroup(
                    testFlagGroup->getTestContext(), specialIndexUse[specialIndexNdx].name.c_str()));

                if (specialIndexUse[specialIndexNdx].useSpecialIndex)
                {
                    for (uint32_t specialIndex = 0; specialIndex < 4; specialIndex++)
                    {
                        TestParams testParams{
                            shaderSourceTypes[shaderSourceNdx].shaderSourceType,
                            shaderSourceTypes[shaderSourceNdx].shaderSourcePipeline,
                            specialIndexUse[specialIndexNdx].useSpecialIndex,
                            testFlagMask,
                            0,
                            ~specialIndex,
                            seed++,
                            CT_NONE,
                            false,
                        };

                        std::stringstream css;
                        css << specialIndex;

                        specialGroup->addChild(new OpacityMicromapCase(testCtx, css.str().c_str(), testParams));
                    }
                    testFlagGroup->addChild(specialGroup.release());
                }
                else
                {
                    struct
                    {
                        uint32_t mode;
                        std::string name;
                    } modes[] = {{2, "2"}, {4, "4"}};
                    for (uint32_t modeNdx = 0; modeNdx < DE_LENGTH_OF_ARRAY(modes); ++modeNdx)
                    {
                        de::MovePtr<tcu::TestCaseGroup> modeGroup(
                            new tcu::TestCaseGroup(testFlagGroup->getTestContext(), modes[modeNdx].name.c_str()));

                        for (uint32_t level = 0; level <= kMaxSubdivisionLevel; level++)
                        {
                            TestParams testParams{
                                shaderSourceTypes[shaderSourceNdx].shaderSourceType,
                                shaderSourceTypes[shaderSourceNdx].shaderSourcePipeline,
                                specialIndexUse[specialIndexNdx].useSpecialIndex,
                                testFlagMask,
                                level,
                                modes[modeNdx].mode,
                                seed++,
                                CT_NONE,
                                false,
                            };

                            std::stringstream css;
                            css << "level_" << level;

                            modeGroup->addChild(new OpacityMicromapCase(testCtx, css.str().c_str(), testParams));
                        }
                        specialGroup->addChild(modeGroup.release());
                    }
                    testFlagGroup->addChild(specialGroup.release());
                }
            }

            sourceTypeGroup->addChild(testFlagGroup.release());
        }

        group->addChild(sourceTypeGroup.release());
    }
}

void addCopyTests(tcu::TestCaseGroup *group)
{
    uint32_t seed = 1614674688u;

    auto &testCtx = group->getTestContext();

    for (size_t copyTypeNdx = CT_FIRST_ACTIVE; copyTypeNdx < CT_NUM_COPY_TYPES; ++copyTypeNdx)
    {
        de::MovePtr<tcu::TestCaseGroup> copyTypeGroup(
            new tcu::TestCaseGroup(group->getTestContext(), copyTypeNames[copyTypeNdx].c_str()));

        struct
        {
            uint32_t mode;
            std::string name;
        } modes[] = {{2, "2"}, {4, "4"}};
        for (uint32_t modeNdx = 0; modeNdx < DE_LENGTH_OF_ARRAY(modes); ++modeNdx)
        {
            de::MovePtr<tcu::TestCaseGroup> modeGroup(
                new tcu::TestCaseGroup(copyTypeGroup->getTestContext(), modes[modeNdx].name.c_str()));

            for (uint32_t level = 0; level <= kMaxSubdivisionLevel; level++)
            {
                TestParams testParams{
                    SST_COMPUTE_SHADER,
                    SSP_COMPUTE_PIPELINE,
                    false,
                    0,
                    level,
                    modes[modeNdx].mode,
                    seed++,
                    (CopyType)copyTypeNdx,
                    false,
                };

                std::stringstream css;
                css << "level_" << level;

                modeGroup->addChild(new OpacityMicromapCase(testCtx, css.str().c_str(), testParams));
            }
            copyTypeGroup->addChild(modeGroup.release());
        }
        group->addChild(copyTypeGroup.release());
    }

    {
        TestParams testParams{
            SST_COMPUTE_SHADER, SSP_COMPUTE_PIPELINE, false, 0, 0, 2, 1, CT_FIRST_ACTIVE, true,
        };
        de::MovePtr<tcu::TestCaseGroup> miscGroup(new tcu::TestCaseGroup(group->getTestContext(), "misc", ""));
        miscGroup->addChild(new OpacityMicromapCase(testCtx, "maintenance5", testParams));
        group->addChild(miscGroup.release());
    }
}

tcu::TestCaseGroup *createOpacityMicromapTests(tcu::TestContext &testCtx)
{
    // Test acceleration structures using opacity micromap with ray query
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "opacity_micromap"));

    // Test accessing all formats of opacity micromaps
    addTestGroup(group.get(), "render", addBasicTests);
    // Test copying opacity micromaps
    addTestGroup(group.get(), "copy", addCopyTests);

    return group.release();
}

} // namespace RayQuery
} // namespace vkt
