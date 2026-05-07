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
    "none",
    "clone",
    "compact",
};

struct TestParams
{
    ShaderSourceType shaderSourceType;
    ShaderSourcePipeline shaderSourcePipeline;
    bool useSpecialIndex;
    bool nonZeroBase;
    bool nullMicromapHandle;
    bool mix_omm;
    uint32_t testFlagMask;
    uint32_t subdivisionLevel; // Must be 0 for useSpecialIndex
    uint32_t mode;             // Special index value if useSpecialIndex, 2 or 4 for number of states otherwise
    uint32_t seed;
    CopyType copyType;
    bool useMaintenance5;
    VkIndexType indexType;
    VkBuildAccelerationStructureFlagsKHR updateFlags; // ALLOW_OPACITY_MICROMAP_UPDATE_BIT_EXT or _DATA_UPDATE_BIT_EXT
};

static constexpr uint32_t kNumThreadsAtOnce = 1024;
static constexpr uint32_t kWorkGroupCount   = 8;
static constexpr uint32_t kLocalSize        = 128;
static constexpr uint32_t kConstantID       = 1045;
DE_STATIC_ASSERT(kWorkGroupCount *kLocalSize == kNumThreadsAtOnce);

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

    if (m_params.mix_omm)
    {
        context.requireDeviceFunctionality("VK_KHR_opacity_micromap");
        context.requireDeviceFunctionality("VK_KHR_device_address_commands");
        const VkPhysicalDeviceOpacityMicromapFeaturesKHR &opacityMicromapFeaturesKHR =
            context.getOpacityMicromapFeatures();
        if (opacityMicromapFeaturesKHR.micromap == false)
            TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceOpacityMicromapFeaturesKHR.micromap");

        const VkPhysicalDeviceDeviceAddressCommandsFeaturesKHR &deviceAddressCommandsFeaturesKHR =
            context.getDeviceAddressCommandsFeatures();
        if (deviceAddressCommandsFeaturesKHR.deviceAddressCommands == false)
            TCU_THROW(NotSupportedError,
                      "Requires VkPhysicalDeviceDeviceAddressCommandsFeaturesKHR.deviceAddressCommands");
    }

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

        if (m_params.updateFlags == VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_OPACITY_MICROMAP_UPDATE_BIT_EXT)
        {
            switch (m_params.mode == 2 ? 4u : 2u) // Complementary format used by the structural update
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
}

void OpacityMicromapCase::initPrograms(vk::SourceCollections &programCollection) const
{
    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

    uint32_t numRays = levelToSubtriangles(m_params.subdivisionLevel);

    // second half is for KHR OMM
    if (m_params.mix_omm)
        numRays *= 2u;

    std::string flagsString =
        (m_params.testFlagMask & TEST_FLAG_BIT_FORCE_OPAQUE_RAY_FLAG) ? "gl_RayFlagsOpaqueEXT" : "gl_RayFlagsNoneEXT";

    if (m_params.testFlagMask & TEST_FLAG_BIT_FORCE_2_STATE_RAY_FLAG)
        flagsString += " | gl_RayFlagsForceOpacityMicromap2StateEXT";

    std::string executionString  = "\n";
    std::string constantIdString = std::to_string(kConstantID);

    if (m_params.mix_omm)
    {
        executionString = "\n \
                          #extension GL_EXT_opacity_micromap_ray_query_mode : require\n \
                          \n \
                          layout(constant_id =" +
                          constantIdString + ") gl_EnableOpacityMicromapEXT;\n";
    }

    std::ostringstream sharedHeader;
    sharedHeader << "#version 460 core\n"
                 << "#extension GL_EXT_ray_query : require\n"
                 << "#extension GL_EXT_opacity_micromap : require\n"
                 << executionString << "layout(set=0, binding=0) uniform accelerationStructureEXT topLevelAS;\n"
                 << "layout(set=0, binding=1, std430) buffer RayOrigins {\n"
                 << "  vec4 values[];\n"
                 << "} origins;\n"
                 << "layout(set=0, binding=2, std430) buffer OutputModes {\n"
                 << "  uint values[];\n"
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
        comp << sharedHeader.str() << "layout(local_size_x=" << kLocalSize << ", local_size_y=1, local_size_z=1) in;\n"
             << "\n"
             << "void main()\n"
             << "{\n"
             << "  uint index             = gl_GlobalInvocationID.x;\n"
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
        nullptr,                         //  const VkAttachmentReference* pInputAttachments;
        0u,                              //  uint32_t colorAttachmentCount;
        nullptr,                         //  const VkAttachmentReference* pColorAttachments;
        nullptr,                         //  const VkAttachmentReference* pResolveAttachments;
        nullptr,                         //  const VkAttachmentReference* pDepthStencilAttachment;
        0,                               //  uint32_t preserveAttachmentCount;
        nullptr                          //  const uint32_t* pPreserveAttachments;
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
        nullptr,                                           //  const void* pNext;
        static_cast<VkRenderPassCreateFlags>(0u),          //  VkRenderPassCreateFlags flags;
        0u,                                                //  uint32_t attachmentCount;
        nullptr,                                           //  const VkAttachmentDescription* pAttachments;
        static_cast<uint32_t>(subpassDescriptions.size()), //  uint32_t subpassCount;
        &subpassDescriptions[0],                           //  const VkSubpassDescription* pSubpasses;
        static_cast<uint32_t>(subpassDependencies.size()), //  uint32_t dependencyCount;
        subpassDependencies.size() > 0 ? &subpassDependencies[0] : nullptr //  const VkSubpassDependency* pDependencies;
    };

    return createRenderPass(vk, device, &renderPassInfo);
}

Move<VkPipeline> makeGraphicsPipeline(const DeviceInterface &vk, const VkDevice device,
                                      const VkPipelineLayout pipelineLayout, const VkRenderPass renderPass,
                                      const VkShaderModule vertexModule, const uint32_t subpass,
                                      const VkSpecializationInfo *vertexShaderSpecializationInfo)
{
    VkExtent2D renderSize{256, 256};
    VkViewport viewport = makeViewport(renderSize);
    VkRect2D scissor    = makeRect2D(renderSize);

    const VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType                             sType
        nullptr,                                               // const void*                                 pNext
        (VkPipelineViewportStateCreateFlags)0,                 // VkPipelineViewportStateCreateFlags          flags
        1u,        // uint32_t                                    viewportCount
        &viewport, // const VkViewport*                           pViewports
        1u,        // uint32_t                                    scissorCount
        &scissor   // const VkRect2D*                             pScissors
    };

    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType                            sType
        nullptr,                                                     // const void*                                pNext
        0u,                                                          // VkPipelineInputAssemblyStateCreateFlags    flags
        VK_PRIMITIVE_TOPOLOGY_POINT_LIST, // VkPrimitiveTopology                        topology
        VK_FALSE                          // VkBool32                                   primitiveRestartEnable
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, //  VkStructureType                                    sType
        nullptr,                                  //  const void*                                        pNext
        (VkPipelineVertexInputStateCreateFlags)0, //  VkPipelineVertexInputStateCreateFlags            flags
        0u,      //  uint32_t                                        vertexBindingDescriptionCount
        nullptr, //  const VkVertexInputBindingDescription*            pVertexBindingDescriptions
        0u,      //  uint32_t                                        vertexAttributeDescriptionCount
        nullptr, //  const VkVertexInputAttributeDescription*        pVertexAttributeDescriptions
    };

    const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, //  VkStructureType                            sType
        nullptr,                                                    //  const void*                                pNext
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
        vk,                            // const DeviceInterface&                           vk
        device,                        // const VkDevice                                   device
        pipelineLayout,                // const VkPipelineLayout                           pipelineLayout
        vertexModule,                  // const VkShaderModule                             vertexShaderModule
        VK_NULL_HANDLE,                // const VkShaderModule                             tessellationControlModule
        VK_NULL_HANDLE,                // const VkShaderModule                             tessellationEvalModule
        VK_NULL_HANDLE,                // const VkShaderModule                             geometryShaderModule
        VK_NULL_HANDLE,                // const VkShaderModule                             fragmentShaderModule
        renderPass,                    // const VkRenderPass                               renderPass
        subpass,                       // const uint32_t                                   subpass
        &vertexInputStateCreateInfo,   // const VkPipelineVertexInputStateCreateInfo*      vertexInputStateCreateInfo
        &inputAssemblyStateCreateInfo, // const VkPipelineInputAssemblyStateCreateInfo*    inputAssemblyStateCreateInfo
        nullptr,                       // const VkPipelineTessellationStateCreateInfo*     tessStateCreateInfo
        &viewportStateCreateInfo,      // const VkPipelineViewportStateCreateInfo*         viewportStateCreateInfo
        &rasterizationStateCreateInfo, // const VkPipelineRasterizationStateCreateInfo*    rasterizationStateCreateInfo
        nullptr,                       // const VkPipelineMultisampleStateCreateInfo*      multisampleStateCreateInfo,
        nullptr,                       // const VkPipelineDepthStencilStateCreateInfo*     depthStencilStateCreateInfo,
        nullptr,                       // const VkPipelineColorBlendStateCreateInfo*       colorBlendStateCreateInfo,
        nullptr,                       // const VkPipelineDynamicStateCreateInfo*          dynamicStateCreateInfo
        nullptr,                       // const void*                                      pNext,
        0,                             // const VkPipelineCreateFlags                      pipelineCreateFlags,
        nullptr,                       // const void*                                      stagePNext,
        vertexShaderSpecializationInfo); // const VkSpecializationInfo*                      vertexShaderSpecializationInfo
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
    auto micromapKHR   = makeMicromapAccelerationStructure();

    AccelerationStructBufferProperties bufferProps;
    bufferProps.props.residency = ResourceResidency::TRADITIONAL;

    const auto triangleCount       = (m_params.nonZeroBase ? 2u : 1u);
    uint32_t numSubtriangles       = levelToSubtriangles(m_params.subdivisionLevel);
    uint32_t triangleMicromapBytes = (m_params.mode == 2) ? (numSubtriangles + 7) / 8 : (numSubtriangles + 3) / 4;
    uint32_t opacityMicromapBytes  = triangleMicromapBytes * triangleCount;

    // Generate random micromap data
    std::vector<uint8_t> opacityMicromapData;
    opacityMicromapData.reserve(opacityMicromapBytes);

    de::Random rnd(m_params.seed);

    while (opacityMicromapData.size() < opacityMicromapBytes)
    {
        opacityMicromapData.push_back(rnd.getUint8());
    }

    // Build a micromap (ignore infrastructure for now)
    // Create the buffer with the mask and index data
    // Allocate a fairly conservative bound for now
    VkBufferUsageFlags2CreateInfoKHR bufferUsageFlags2 = initVulkanStructure();
    const auto micromapDataBufferSize                  = static_cast<VkDeviceSize>(1024 + opacityMicromapBytes);
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
    mmUsage.count              = triangleCount;
    mmUsage.subdivisionLevel   = m_params.subdivisionLevel;
    mmUsage.format =
        m_params.mode == 2 ? VK_OPACITY_MICROMAP_FORMAT_2_STATE_EXT : VK_OPACITY_MICROMAP_FORMAT_4_STATE_EXT;

    {
        uint8_t *data = static_cast<uint8_t *>(micromapDataBufferData);

        deMemset(data, 0, size_t(micromapDataBufferCreateInfo.size));

        DE_STATIC_ASSERT(sizeof(VkMicromapTriangleEXT) == 8);

        // Triangle information
        for (uint32_t i = 0u; i < triangleCount; ++i)
        {
            VkMicromapTriangleEXT *tri = (VkMicromapTriangleEXT *)(&data[TriangleOffset]) + i;
            tri->dataOffset            = triangleMicromapBytes * i;
            tri->subdivisionLevel      = uint16_t(mmUsage.subdivisionLevel);
            tri->format                = uint16_t(mmUsage.format);
        }

        // Micromap data
        {
            for (size_t i = 0; i < opacityMicromapData.size(); i++)
            {
                data[DataOffset + i] = opacityMicromapData[i];
            }
        }

        // Index information
        {
            const uint32_t indexValue = m_params.useSpecialIndex ? m_params.mode : 0u;
            switch (m_params.indexType)
            {
            case VK_INDEX_TYPE_UINT16:
                *((uint16_t *)&data[IndexOffset]) = static_cast<uint16_t>(indexValue);
                break;
            case VK_INDEX_TYPE_UINT32:
            default:
                *((uint32_t *)&data[IndexOffset]) = indexValue;
                break;
            }
        }
    }

    de::MovePtr<BufferWithMemory> micromapBackingBufferPtr;
    de::MovePtr<BufferWithMemory> micromapScratchBufferPtr;
    de::MovePtr<BufferWithMemory> copyMicromapBackingBuffer;
    VkMicromapEXT micromap = VK_NULL_HANDLE, origMicromap = VK_NULL_HANDLE;

    // Build info reused for the update below
    VkMicromapBuildInfoEXT mmBuildInfo = {
        VK_STRUCTURE_TYPE_MICROMAP_BUILD_INFO_EXT,                // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        VK_MICROMAP_TYPE_OPACITY_MICROMAP_EXT,                    // VkMicromapTypeEXT type;
        0,                                                        // VkBuildMicromapFlagsEXT flags;
        VK_BUILD_MICROMAP_MODE_BUILD_EXT,                         // VkBuildMicromapModeEXT mode;
        VK_NULL_HANDLE,                                           // VkMicromapEXT dstMicromap;
        1,                                                        // uint32_t usageCountsCount;
        &mmUsage,                                                 // const VkMicromapUsageEXT* pUsageCounts;
        nullptr,                                                  // const VkMicromapUsageEXT* const* ppUsageCounts;
        makeDeviceOrHostAddressConstKHR(nullptr),                 // VkDeviceOrHostAddressConstKHR data;
        makeDeviceOrHostAddressKHR(nullptr),                      // VkDeviceOrHostAddressKHR scratchData;
        makeDeviceOrHostAddressConstKHR(nullptr),                 // VkDeviceOrHostAddressConstKHR triangleArray;
        static_cast<VkDeviceSize>(sizeof(VkMicromapTriangleEXT)), // VkDeviceSize triangleArrayStride;
    };

    VkMicromapBuildSizesInfoEXT sizeInfo = {
        VK_STRUCTURE_TYPE_MICROMAP_BUILD_SIZES_INFO_EXT, // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        0,                                               // VkDeviceSize micromapSize;
        0,                                               // VkDeviceSize buildScratchSize;
        false,                                           // VkBool32 discardable;
    };

    vkd.getMicromapBuildSizesEXT(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &mmBuildInfo, &sizeInfo);

    const auto micromapBackingBufferCreateInfo = makeBufferCreateInfo(
        sizeInfo.micromapSize, VK_BUFFER_USAGE_MICROMAP_STORAGE_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    auto micromapScratchBufferCreateInfo =
        makeBufferCreateInfo(sizeInfo.buildScratchSize,
                             VK_BUFFER_USAGE_MICROMAP_STORAGE_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    if (m_params.useMaintenance5)
    {
        bufferUsageFlags2.usage               = (VkBufferUsageFlagBits2KHR)micromapScratchBufferCreateInfo.usage;
        micromapScratchBufferCreateInfo.pNext = &bufferUsageFlags2;
        micromapScratchBufferCreateInfo.usage = 0;
    }

    // Buffer handle filled in below once storage is allocated
    VkMicromapCreateInfoEXT maCreateInfo = {
        VK_STRUCTURE_TYPE_MICROMAP_CREATE_INFO_EXT, // VkStructureType sType;
        nullptr,                                    // const void* pNext;
        0,                                          // VkMicromapCreateFlagsEXT createFlags;
        VK_NULL_HANDLE,                             // VkBuffer buffer;
        0,                                          // VkDeviceSize offset;
        sizeInfo.micromapSize,                      // VkDeviceSize size;
        VK_MICROMAP_TYPE_OPACITY_MICROMAP_EXT,      // VkMicromapTypeEXT type;
        0ull                                        // VkDeviceAddress deviceAddress;
    };

    if (!m_params.nullMicromapHandle)
    {
        // Create the backing and scratch storage
        micromapBackingBufferPtr = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vkd, device, alloc, micromapBackingBufferCreateInfo,
                                 MemoryRequirement::Local | MemoryRequirement::DeviceAddress));
        micromapScratchBufferPtr = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vkd, device, alloc, micromapScratchBufferCreateInfo,
                                 MemoryRequirement::Local | MemoryRequirement::DeviceAddress));

        // Create the micromap itself
        maCreateInfo.buffer = micromapBackingBufferPtr->get();
        VK_CHECK(vkd.createMicromapEXT(device, &maCreateInfo, nullptr, &micromap));

        // Do the build
        mmBuildInfo.dstMicromap = micromap;
        mmBuildInfo.data        = makeDeviceOrHostAddressConstKHR(vkd, device, micromapDataBuffer.get(), DataOffset);
        mmBuildInfo.triangleArray =
            makeDeviceOrHostAddressConstKHR(vkd, device, micromapDataBuffer.get(), TriangleOffset);
        mmBuildInfo.scratchData = makeDeviceOrHostAddressKHR(vkd, device, micromapScratchBufferPtr->get(), 0);

        vkd.cmdBuildMicromapsEXT(cmdBuffer, 1, &mmBuildInfo);

        {
            const auto memoryBarrier = makeMemoryBarrier2(
                VK_PIPELINE_STAGE_2_MICROMAP_BUILD_BIT_EXT, VK_ACCESS_2_MICROMAP_WRITE_BIT_EXT,
                m_params.copyType != CT_NONE ? VK_PIPELINE_STAGE_2_MICROMAP_BUILD_BIT_EXT :
                                               VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                VK_ACCESS_2_MICROMAP_READ_BIT_EXT);
            VkDependencyInfoKHR dependencyInfo = {
                VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR, // VkStructureType sType;
                nullptr,                               // const void* pNext;
                0u,                                    // VkDependencyFlags dependencyFlags;
                1u,                                    // uint32_t memoryBarrierCount;
                &memoryBarrier,                        // const VkMemoryBarrier2KHR* pMemoryBarriers;
                0u,                                    // uint32_t bufferMemoryBarrierCount;
                nullptr,                               // const VkBufferMemoryBarrier2KHR* pBufferMemoryBarriers;
                0u,                                    // uint32_t imageMemoryBarrierCount;
                nullptr,                               // const VkImageMemoryBarrier2KHR* pImageMemoryBarriers;
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
                nullptr,                                  // const void* pNext;
                origMicromap,                             // VkMicromapEXT src;
                micromap,                                 // VkMicromapEXT dst;
                VK_COPY_MICROMAP_MODE_CLONE_EXT           // VkCopyMicromapModeEXT mode;
            };

            vkd.cmdCopyMicromapEXT(cmdBuffer, &copyMicromapInfo);

            {
                const auto memoryBarrier = makeMemoryBarrier2(
                    VK_PIPELINE_STAGE_2_MICROMAP_BUILD_BIT_EXT, VK_ACCESS_2_MICROMAP_WRITE_BIT_EXT,
                    VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_ACCESS_2_MICROMAP_READ_BIT_EXT);
                VkDependencyInfoKHR dependencyInfo = {
                    VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR, // VkStructureType sType;
                    nullptr,                               // const void* pNext;
                    0u,                                    // VkDependencyFlags dependencyFlags;
                    1u,                                    // uint32_t memoryBarrierCount;
                    &memoryBarrier,                        // const VkMemoryBarrier2KHR* pMemoryBarriers;
                    0u,                                    // uint32_t bufferMemoryBarrierCount;
                    nullptr,                               // const VkBufferMemoryBarrier2KHR* pBufferMemoryBarriers;
                    0u,                                    // uint32_t imageMemoryBarrierCount;
                    nullptr,                               // const VkImageMemoryBarrier2KHR* pImageMemoryBarriers;
                };

                dependencyInfo.memoryBarrierCount = 1;
                dependencyInfo.pMemoryBarriers    = &memoryBarrier;

                vkd.cmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
            }
        }
    }

    // Attach the micromap to the geometry
    VkAccelerationStructureTrianglesOpacityMicromapEXT opacityGeometryMicromap = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_OPACITY_MICROMAP_EXT, //VkStructureType sType;
        nullptr,                                                                 //void* pNext;
        m_params.indexType,                                                      //VkIndexType indexType;
        makeDeviceOrHostAddressConstKHR(vkd, device, micromapDataBuffer.get(),
                                        IndexOffset),     //VkDeviceOrHostAddressConstKHR indexBuffer;
        0u,                                               //VkDeviceSize indexStride;
        (m_params.nonZeroBase ? 1u : 0u),                 //uint32_t baseTriangle;
        m_params.nullMicromapHandle ? 0u : 1u,            //uint32_t usageCountsCount;
        m_params.nullMicromapHandle ? nullptr : &mmUsage, //const VkMicromapUsageEXT* pUsageCounts;
        nullptr,                                          //const VkMicromapUsageEXT* const* ppUsageCounts;
        micromap                                          //VkMicromapEXT micromap;
    };

    const std::vector<tcu::Vec3> triangle = {
        tcu::Vec3(0.0f, 0.0f, 0.0f),
        tcu::Vec3(1.0f, 0.0f, 0.0f),
        tcu::Vec3(0.0f, 1.0f, 0.0f),
    };

    de::SharedPtr<RaytracedGeometryBase> geometry =
        makeRaytracedGeometry(VK_GEOMETRY_TYPE_TRIANGLES_KHR, VK_FORMAT_R32G32B32_SFLOAT, VK_INDEX_TYPE_NONE_KHR);
    for (const auto &v : triangle)
        geometry->addVertex(v);
    geometry->setOpacityMicromap(&opacityGeometryMicromap);
    bottomLevelAS->addGeometry(geometry);

    if (m_params.mix_omm)
    {
        // Attach the micromap data
        micromapKHR->addOpacityMicromap(
            opacityMicromapData, triangleCount, m_params.subdivisionLevel,
            (m_params.mode == 2 ? VK_OPACITY_MICROMAP_FORMAT_2_STATE_KHR : VK_OPACITY_MICROMAP_FORMAT_4_STATE_KHR),
            m_params.useSpecialIndex, m_params.mode);
        micromapKHR->createAndBuild(vkd, device, cmdBuffer, alloc, 0);

        // Attach the KHR micromap to the geometry
        VkAccelerationStructureTrianglesOpacityMicromapKHR opacityGeometryMicromapKHR = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_OPACITY_MICROMAP_KHR, //VkStructureType             sType;
            nullptr,                                      // void*                      pNext;
            VK_INDEX_TYPE_UINT32,                         // VkIndexType                indexType;
            micromapKHR->getIndexBufferAddr(vkd, device), // VkDeviceAddress            indexBuffer;
            0,                                            // VkDeviceSize               indexStride;
            (m_params.nonZeroBase ? 1u : 0u),             // uint32_t                   baseTriangle;
            *micromapKHR->getPtr(),                       // VkAccelerationStructureKHR micromap;
        };
        const std::vector<tcu::Vec3> triangleKHR = {
            tcu::Vec3(0.0f, 0.0f, 0.0f),
            tcu::Vec3(-1.0f, 0.0f, 0.0f),
            tcu::Vec3(0.0f, -1.0f, 0.0f),
        };
        bottomLevelAS->addGeometry(triangleKHR, true /*is triangles*/, 0, nullptr, nullptr,
                                   &opacityGeometryMicromapKHR);
    }

    VkBuildAccelerationStructureFlagsKHR blasBuildFlags = 0;
    if (m_params.testFlagMask & TEST_FLAG_BIT_DISABLE_OPACITY_MICROMAP_INSTANCE)
        blasBuildFlags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DISABLE_OPACITY_MICROMAPS_EXT;
    if (m_params.updateFlags != 0)
        blasBuildFlags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR | m_params.updateFlags;
    if (blasBuildFlags != 0)
        bottomLevelAS->setBuildFlags(blasBuildFlags);
    bottomLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc, bufferProps);
    de::SharedPtr<BottomLevelAccelerationStructure> blasSharedPtr(bottomLevelAS.release());

    de::MovePtr<BufferWithMemory> updateDataBuffer;
    de::MovePtr<BufferWithMemory> updateMicromapBacking;
    de::MovePtr<BufferWithMemory> updateMicromapScratch;
    VkMicromapEXT updatedMicromap        = VK_NULL_HANDLE;
    uint32_t verifyMode                  = m_params.mode;
    uint32_t verifyTriangleMicromapBytes = triangleMicromapBytes;

    // Replace the micromap and update BLAS in place
    if (m_params.updateFlags != 0)
    {
        DE_ASSERT(!m_params.useSpecialIndex);

        const bool structuralUpdate =
            (m_params.updateFlags == VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_OPACITY_MICROMAP_UPDATE_BIT_EXT);
        const uint32_t structuralMode =
            m_params.mode == 2 ? 4u : 2u; // Complementary format used by the structural update
        if (structuralUpdate)
            mmUsage.format =
                structuralMode == 2 ? VK_OPACITY_MICROMAP_FORMAT_2_STATE_EXT : VK_OPACITY_MICROMAP_FORMAT_4_STATE_EXT;

        verifyMode                  = structuralUpdate ? structuralMode : m_params.mode;
        verifyTriangleMicromapBytes = (verifyMode == 2) ? (numSubtriangles + 7) / 8 : (numSubtriangles + 3) / 4;
        const uint32_t updOpacityMicromapBytes = verifyTriangleMicromapBytes * triangleCount;

        de::Random rndUpdate(m_params.seed + 0x9E3779B9u);
        std::vector<uint8_t> newOpacityMicromapData;
        newOpacityMicromapData.reserve(updOpacityMicromapBytes);
        while (newOpacityMicromapData.size() < updOpacityMicromapBytes)
            newOpacityMicromapData.push_back(rndUpdate.getUint8());

        const uint32_t upTriangleOffset = 0u;
        const uint32_t upDataOffset     = 256u;
        const auto upDataBufferSize     = static_cast<VkDeviceSize>(upDataOffset + updOpacityMicromapBytes);
        const auto upDataBufferInfo =
            makeBufferCreateInfo(upDataBufferSize, VK_BUFFER_USAGE_MICROMAP_BUILD_INPUT_READ_ONLY_BIT_EXT |
                                                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        updateDataBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
            vkd, device, alloc, upDataBufferInfo, MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress));
        {
            uint8_t *upDataPtr = static_cast<uint8_t *>(updateDataBuffer->getAllocation().getHostPtr());
            deMemset(upDataPtr, 0, static_cast<size_t>(upDataBufferSize));

            for (uint32_t i = 0u; i < triangleCount; ++i)
            {
                VkMicromapTriangleEXT *tri = (VkMicromapTriangleEXT *)(&upDataPtr[upTriangleOffset]) + i;
                tri->dataOffset            = verifyTriangleMicromapBytes * i;
                tri->subdivisionLevel      = uint16_t(mmUsage.subdivisionLevel);
                tri->format                = uint16_t(mmUsage.format);
            }

            deMemcpy(&upDataPtr[upDataOffset], newOpacityMicromapData.data(), newOpacityMicromapData.size());
            flushAlloc(vkd, device, updateDataBuffer->getAllocation());
        }

        VkMicromapBuildInfoEXT upBuildInfo = mmBuildInfo;
        upBuildInfo.flags                  = 0;
        upBuildInfo.mode                   = VK_BUILD_MICROMAP_MODE_BUILD_EXT;
        upBuildInfo.data = makeDeviceOrHostAddressConstKHR(vkd, device, updateDataBuffer->get(), upDataOffset);
        upBuildInfo.triangleArray =
            makeDeviceOrHostAddressConstKHR(vkd, device, updateDataBuffer->get(), upTriangleOffset);

        VkMicromapBuildSizesInfoEXT upSizeInfo = {
            VK_STRUCTURE_TYPE_MICROMAP_BUILD_SIZES_INFO_EXT, nullptr, 0, 0, false,
        };
        vkd.getMicromapBuildSizesEXT(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &upBuildInfo,
                                     &upSizeInfo);

        const auto upBackingBufferCreateInfo =
            makeBufferCreateInfo(upSizeInfo.micromapSize,
                                 VK_BUFFER_USAGE_MICROMAP_STORAGE_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        const auto upScratchBufferCreateInfo =
            makeBufferCreateInfo(upSizeInfo.buildScratchSize,
                                 VK_BUFFER_USAGE_MICROMAP_STORAGE_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

        updateMicromapBacking = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vkd, device, alloc, upBackingBufferCreateInfo,
                                 MemoryRequirement::Local | MemoryRequirement::DeviceAddress));
        updateMicromapScratch = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vkd, device, alloc, upScratchBufferCreateInfo,
                                 MemoryRequirement::Local | MemoryRequirement::DeviceAddress));

        VkMicromapCreateInfoEXT upCreateInfo = maCreateInfo;
        upCreateInfo.buffer                  = updateMicromapBacking->get();
        upCreateInfo.size                    = upSizeInfo.micromapSize;
        VK_CHECK(vkd.createMicromapEXT(device, &upCreateInfo, nullptr, &updatedMicromap));

        upBuildInfo.dstMicromap = updatedMicromap;
        upBuildInfo.scratchData = makeDeviceOrHostAddressKHR(vkd, device, updateMicromapScratch->get(), 0);

        {
            const auto preBuildBarrier = makeMemoryBarrier2(
                VK_PIPELINE_STAGE_2_MICROMAP_BUILD_BIT_EXT | VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_PIPELINE_STAGE_2_MICROMAP_BUILD_BIT_EXT,
                VK_ACCESS_2_MICROMAP_WRITE_BIT_EXT | VK_ACCESS_2_MICROMAP_READ_BIT_EXT);
            VkDependencyInfoKHR preBuildDep = {
                VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR, nullptr, 0u, 1u, &preBuildBarrier, 0u, nullptr, 0u, nullptr};
            vkd.cmdPipelineBarrier2(cmdBuffer, &preBuildDep);
        }

        vkd.cmdBuildMicromapsEXT(cmdBuffer, 1, &upBuildInfo);

        {
            const auto mmToAsBarrier = makeMemoryBarrier2(
                VK_PIPELINE_STAGE_2_MICROMAP_BUILD_BIT_EXT, VK_ACCESS_2_MICROMAP_WRITE_BIT_EXT,
                VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_ACCESS_2_MICROMAP_READ_BIT_EXT);
            VkDependencyInfoKHR mmToAsDep = {
                VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR, nullptr, 0u, 1u, &mmToAsBarrier, 0u, nullptr, 0u, nullptr};
            vkd.cmdPipelineBarrier2(cmdBuffer, &mmToAsDep);
        }

        {
            const auto asBarrier      = makeMemoryBarrier2(VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                                           VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                                                           VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                                           VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR |
                                                               VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR);
            VkDependencyInfoKHR asDep = {
                VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR, nullptr, 0u, 1u, &asBarrier, 0u, nullptr, 0u, nullptr};
            vkd.cmdPipelineBarrier2(cmdBuffer, &asDep);
        }

        opacityGeometryMicromap.micromap = updatedMicromap;
        geometry->setOpacityMicromap(&opacityGeometryMicromap);

        blasSharedPtr->build(vkd, device, cmdBuffer, blasSharedPtr.get());

        opacityMicromapData = std::move(newOpacityMicromapData);
    }

    VkGeometryInstanceFlagsKHR instanceFlags = 0;

    if (m_params.testFlagMask & TEST_FLAG_BIT_FORCE_2_STATE_INSTANCE)
        instanceFlags |= VK_GEOMETRY_INSTANCE_FORCE_OPACITY_MICROMAP_2_STATE_EXT;
    if (m_params.testFlagMask & TEST_FLAG_BIT_FORCE_OPAQUE_INSTANCE)
        instanceFlags |= VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
    if (m_params.testFlagMask & TEST_FLAG_BIT_DISABLE_OPACITY_MICROMAP_INSTANCE)
        instanceFlags |= VK_GEOMETRY_INSTANCE_DISABLE_OPACITY_MICROMAPS_EXT;

    topLevelAS->setInstanceCount(1);
    topLevelAS->addInstance(blasSharedPtr, identityMatrix3x4, 0, 0xFFu, 0u, instanceFlags);
    topLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc, bufferProps);

    // One ray per subtriangle for this test
    uint32_t numRays = numSubtriangles;

    // second half is for KHR OMM
    if (m_params.mix_omm)
        numRays *= 2u;

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

    const auto micromapDataOffset = (m_params.nonZeroBase ? verifyTriangleMicromapBytes : 0u);

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
                state = verifyMode;
            }
            else
            {
                if (verifyMode == 2)
                {
                    uint8_t byte = opacityMicromapData[(index % numSubtriangles) / 8 + micromapDataOffset];
                    state        = (byte >> ((index % numSubtriangles) % 8)) & 0x1;
                }
                else
                {
                    DE_ASSERT(verifyMode == 4);
                    uint8_t byte = opacityMicromapData[(index % numSubtriangles) / 4 + micromapDataOffset];
                    state        = (byte >> 2 * ((index % numSubtriangles) % 4)) & 0x3;
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
        if (index < numSubtriangles)
        {
            tcu::Vec2 centroid = calcSubtriangleCentroid(index, m_params.subdivisionLevel);
            origins.push_back(tcu::Vec4(centroid.x(), centroid.y(), 1.0, 0.0));
        }
        else
        {
            tcu::Vec2 centroid = calcSubtriangleCentroid((index % numSubtriangles), m_params.subdivisionLevel);
            origins.push_back(tcu::Vec4(centroid.x() * -1.0f, centroid.y() * -1.0f, 1.0, 0.0));
        }
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

    VkSpecializationMapEntry mapEntry = {
        kConstantID,     // constantID
        0,               // offset
        sizeof(VkBool32) // size
    };
    VkBool32 enable                               = VK_TRUE;
    const VkSpecializationInfo specializationInfo = {
        1,              // mapEntryCount
        &mapEntry,      // pMapEntries
        sizeof(enable), // dataSize
        &enable,        // pData
    };

    if (m_params.shaderSourceType == SST_VERTEX_SHADER)
    {
        auto vertexModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0);

        renderPass  = makeEmptyRenderPass(vkd, device);
        framebuffer = makeFramebuffer(vkd, device, *renderPass, 0u, nullptr, 32, 32);
        pipeline    = makeGraphicsPipeline(vkd, device, *pipelineLayout, *renderPass, *vertexModule, 0,
                                        m_params.mix_omm ? &specializationInfo : nullptr);

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

        auto raygenSBTRegion = makeStridedDeviceAddressRegionKHR(0, 0, 0);
        auto unusedSBTRegion = makeStridedDeviceAddressRegionKHR(0, 0, 0);

        {
            const auto rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();
            rayTracingPipeline->setCreateFlags(VK_PIPELINE_CREATE_RAY_TRACING_OPACITY_MICROMAP_BIT_EXT);
            if (m_params.useMaintenance5)
                rayTracingPipeline->setCreateFlags2(VK_PIPELINE_CREATE_2_RAY_TRACING_OPACITY_MICROMAP_BIT_EXT);
            rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgenModule, 0,
                                          m_params.mix_omm ? &specializationInfo : nullptr);

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
            m_params.mix_omm ? &specializationInfo : nullptr,    // const VkSpecializationInfo* pSpecializationInfo;
        };
        const VkComputePipelineCreateInfo pipelineInfo = {
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                        // const void* pNext;
            0u,                                             // VkPipelineCreateFlags flags;
            shaderInfo,                                     // VkPipelineShaderStageCreateInfo stage;
            pipelineLayout.get(),                           // VkPipelineLayout layout;
            VK_NULL_HANDLE,                                 // VkPipeline basePipelineHandle;
            0,                                              // int32_t basePipelineIndex;
        };
        pipeline = createComputePipeline(vkd, device, VK_NULL_HANDLE, &pipelineInfo);

        // Dispatch work with ray queries.
        vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.get());
        vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout.get(), 0u, 1u,
                                  &descriptorSet.get(), 0u, nullptr);
        vkd.cmdDispatch(cmdBuffer, kWorkGroupCount, 1u, 1u);
    }

    // Barrier for the output buffer.
    const auto bufferBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                           &bufferBarrier, 0u, nullptr, 0u, nullptr);

    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    if (micromap != VK_NULL_HANDLE)
        vkd.destroyMicromapEXT(device, micromap, nullptr);
    if (origMicromap != VK_NULL_HANDLE)
        vkd.destroyMicromapEXT(device, origMicromap, nullptr);
    if (updatedMicromap != VK_NULL_HANDLE)
        vkd.destroyMicromapEXT(device, updatedMicromap, nullptr);

    // Verify results.
    std::vector<uint32_t> outputData(expectedOutputModes.size());
    const auto outputModesBufferSizeSz = static_cast<size_t>(outputModesBufferSize);

    invalidateAlloc(vkd, device, outputModesBufferAlloc);
    DE_ASSERT(de::dataSize(outputData) == outputModesBufferSizeSz);
    deMemcpy(outputData.data(), outputModesBufferData, outputModesBufferSizeSz);

    bool fail = false;
    auto &log = m_context.getTestContext().getLog();

    const auto logValues = [&](uint32_t ref, uint32_t res, size_t idx)
    {
        std::ostringstream msg;
        msg << "Ray " << idx << ": expected " << ref << " and found " << res;
        log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
    };

    for (size_t i = 0; i < outputData.size(); ++i)
    {
        const auto &outVal      = outputData[i];
        const auto &expectedVal = expectedOutputModes[i];

        if (outVal != expectedVal)
        {
            logValues(expectedVal, outVal, i);
            fail = true;
        }
    }

    if (fail)
        TCU_FAIL("Unexpected values found in output buffer; check log for details --");
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
                maskName = "no_flags";

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
                            false,
                            false,
                            false,
                            testFlagMask,
                            0,
                            ~specialIndex,
                            seed++,
                            CT_NONE,
                            false,
                            VK_INDEX_TYPE_UINT32,
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
                                false,
                                false,
                                false,
                                testFlagMask,
                                level,
                                modes[modeNdx].mode,
                                seed++,
                                CT_NONE,
                                false,
                                VK_INDEX_TYPE_UINT32,
                                false,
                            };

                            std::stringstream css;
                            css << "level_" << level;
                            const auto testName = css.str();

                            modeGroup->addChild(new OpacityMicromapCase(testCtx, testName, testParams));

                            if (testFlagMask == 0u)
                            {
                                testParams.nonZeroBase = true;
                                const auto variantName = testName + "_non_zero_base";
                                modeGroup->addChild(new OpacityMicromapCase(testCtx, variantName, testParams));

                                testParams.nonZeroBase  = false;
                                testParams.mix_omm      = true;
                                const auto variantNameB = testName + "_mix_omm";
                                modeGroup->addChild(new OpacityMicromapCase(testCtx, variantNameB, testParams));
                            }
                        }
                        specialGroup->addChild(modeGroup.release());
                    }
                    testFlagGroup->addChild(specialGroup.release());
                }
            }

            // VK_NULL_HANDLE micromap handle
            {
                de::MovePtr<tcu::TestCaseGroup> nullHandleGroup(
                    new tcu::TestCaseGroup(testFlagGroup->getTestContext(), "null_handle"));

                for (uint32_t specialIndex = 0; specialIndex < 4; specialIndex++)
                {
                    TestParams testParams{
                        shaderSourceTypes[shaderSourceNdx].shaderSourceType,
                        shaderSourceTypes[shaderSourceNdx].shaderSourcePipeline,
                        true,
                        false,
                        true,
                        false,
                        testFlagMask,
                        0,
                        ~specialIndex,
                        seed++,
                        CT_NONE,
                        false,
                        VK_INDEX_TYPE_UINT32,
                        false,
                    };

                    std::stringstream css;
                    css << specialIndex;
                    const auto testName = css.str();

                    nullHandleGroup->addChild(new OpacityMicromapCase(testCtx, testName, testParams));
                }
                testFlagGroup->addChild(nullHandleGroup.release());
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
                    false,
                    false,
                    false,
                    0,
                    level,
                    modes[modeNdx].mode,
                    seed++,
                    (CopyType)copyTypeNdx,
                    false,
                    VK_INDEX_TYPE_UINT32,
                    false,
                };

                std::stringstream css;
                css << "level_" << level;

                modeGroup->addChild(new OpacityMicromapCase(testCtx, css.str(), testParams));
            }
            copyTypeGroup->addChild(modeGroup.release());
        }
        group->addChild(copyTypeGroup.release());
    }

    {
        TestParams testParams{
            SST_COMPUTE_SHADER,
            SSP_COMPUTE_PIPELINE,
            false,
            false,
            false,
            false,
            0,
            0,
            2,
            1,
            CT_FIRST_ACTIVE,
            true,
            VK_INDEX_TYPE_UINT32,
            false,
        };
        de::MovePtr<tcu::TestCaseGroup> miscGroup(new tcu::TestCaseGroup(group->getTestContext(), "misc"));
        miscGroup->addChild(new OpacityMicromapCase(testCtx, "maintenance5", testParams));
        group->addChild(miscGroup.release());
    }
}

void addIndexTypeTests(tcu::TestCaseGroup *group)
{
    uint32_t seed = 1717172000u;
    auto &testCtx = group->getTestContext();

    const struct
    {
        VkIndexType indexType;
        const char *name;
    } indexTypes[] = {
        {VK_INDEX_TYPE_UINT16, "uint16"},
    };

    const struct
    {
        uint32_t mode;
        std::string name;
    } modes[] = {{2, "2"}, {4, "4"}};

    for (const auto &itype : indexTypes)
    {
        de::MovePtr<tcu::TestCaseGroup> itypeGroup(new tcu::TestCaseGroup(testCtx, itype.name));

        de::MovePtr<tcu::TestCaseGroup> mapValueGroup(new tcu::TestCaseGroup(testCtx, "map_value"));
        for (const auto &mode : modes)
        {
            de::MovePtr<tcu::TestCaseGroup> modeGroup(new tcu::TestCaseGroup(testCtx, mode.name.c_str()));
            for (uint32_t level = 0; level <= kMaxSubdivisionLevel; level++)
            {
                TestParams testParams{
                    SST_COMPUTE_SHADER,
                    SSP_COMPUTE_PIPELINE,
                    false,
                    false,
                    false,
                    false,
                    0u,
                    level,
                    mode.mode,
                    seed++,
                    CT_NONE,
                    false,
                    itype.indexType,
                    false,
                };
                std::ostringstream css;
                css << "level_" << level;
                modeGroup->addChild(new OpacityMicromapCase(testCtx, css.str(), testParams));
            }
            mapValueGroup->addChild(modeGroup.release());
        }
        itypeGroup->addChild(mapValueGroup.release());

        // special_index variant
        de::MovePtr<tcu::TestCaseGroup> specialGroup(new tcu::TestCaseGroup(testCtx, "special_index"));
        for (uint32_t specialIndex = 0; specialIndex < 4; specialIndex++)
        {
            TestParams testParams{
                SST_COMPUTE_SHADER,
                SSP_COMPUTE_PIPELINE,
                true,
                false,
                false,
                false,
                0u,
                0u,
                ~specialIndex,
                seed++,
                CT_NONE,
                false,
                itype.indexType,
                false,
            };
            std::ostringstream css;
            css << specialIndex;
            specialGroup->addChild(new OpacityMicromapCase(testCtx, css.str(), testParams));
        }
        itypeGroup->addChild(specialGroup.release());

        group->addChild(itypeGroup.release());
    }
}

void addUpdateTests(tcu::TestCaseGroup *group)
{
    uint32_t seed = 1717173000u;
    auto &testCtx = group->getTestContext();

    const struct
    {
        uint32_t mode;
        std::string name;
    } modes[] = {{2, "2"}, {4, "4"}};

    const uint32_t levels[] = {0u, 1u, kMaxSubdivisionLevel};

    const struct
    {
        VkBuildAccelerationStructureFlagsKHR updateFlags;
        const char *name;
    } updateKinds[] = {
        {VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_OPACITY_MICROMAP_UPDATE_BIT_EXT, "micromap_update"},
        {VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_OPACITY_MICROMAP_DATA_UPDATE_BIT_EXT, "micromap_data_update"},
    };

    for (const auto &kind : updateKinds)
    {
        de::MovePtr<tcu::TestCaseGroup> umGroup(new tcu::TestCaseGroup(testCtx, kind.name));
        for (const auto &mode : modes)
        {
            de::MovePtr<tcu::TestCaseGroup> modeGroup(new tcu::TestCaseGroup(testCtx, mode.name.c_str()));
            for (const auto level : levels)
            {
                TestParams testParams{
                    SST_COMPUTE_SHADER,
                    SSP_COMPUTE_PIPELINE,
                    false,
                    false,
                    false,
                    false,
                    0u,
                    level,
                    mode.mode,
                    seed++,
                    CT_NONE,
                    false,
                    VK_INDEX_TYPE_UINT32,
                    kind.updateFlags,
                };
                std::ostringstream css;
                css << "level_" << level;
                modeGroup->addChild(new OpacityMicromapCase(testCtx, css.str(), testParams));
            }
            umGroup->addChild(modeGroup.release());
        }
        group->addChild(umGroup.release());
    }
}

tcu::TestCaseGroup *createOpacityMicromapTests(tcu::TestContext &testCtx)
{
    // Test acceleration structures using opacity micromap with ray query
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "ext"));

    // Test accessing all formats of opacity micromaps
    addTestGroup(group.get(), "render", addBasicTests);
    // Test copying opacity micromaps
    addTestGroup(group.get(), "copy", addCopyTests);
    // Exercise UINT16 index type in VkAccelerationStructureTrianglesOpacityMicromapEXT
    addTestGroup(group.get(), "index_type", addIndexTypeTests);
    // Exercise BLAS update flow with VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_OPACITY_MICROMAP_UPDATE_BIT_KHR
    addTestGroup(group.get(), "update", addUpdateTests);

    return group.release();
}

} // namespace RayQuery
} // namespace vkt
