/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 ARM Ltd.
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Pipeline Cache and Pipeline Binary Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineCacheTests.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkPipelineBinaryUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkBuilderUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuImageCompare.hpp"
#include "deUniquePtr.hpp"
#include "deMemory.h"
#include "tcuTestLog.hpp"
#include "deThread.hpp"

#include <sstream>
#include <vector>
#include <memory>

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{

// helper functions

std::string getShaderFlagStr(const VkShaderStageFlags shader, bool isDescription)
{
    std::ostringstream desc;
    if (shader & VK_SHADER_STAGE_COMPUTE_BIT)
    {
        desc << ((isDescription) ? "compute stage" : "compute_stage");
    }
    else
    {
        desc << ((isDescription) ? "vertex stage" : "vertex_stage");
        if (shader & VK_SHADER_STAGE_GEOMETRY_BIT)
            desc << ((isDescription) ? " geometry stage" : "_geometry_stage");
        if (shader & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
            desc << ((isDescription) ? " tessellation control stage" : "_tessellation_control_stage");
        if (shader & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
            desc << ((isDescription) ? " tessellation evaluation stage" : "_tessellation_evaluation_stage");
        desc << ((isDescription) ? " fragment stage" : "_fragment_stage");
    }

    return desc.str();
}

enum class TestMode
{
    CACHE = 0,
    BINARY
};

// helper classes
class TestParam
{
public:
    TestParam(TestMode mode, PipelineConstructionType pipelineConstructionType, const VkShaderStageFlags shaders,
              bool compileMissShaders = false, VkPipelineCacheCreateFlags pipelineCacheCreateFlags = 0u,
              bool useBinariesFromBinaryData = false);
    virtual ~TestParam(void) = default;
    virtual const std::string generateTestName(void) const;
    TestMode getMode(void) const
    {
        return m_mode;
    }
    PipelineConstructionType getPipelineConstructionType(void) const
    {
        return m_pipelineConstructionType;
    };
    VkShaderStageFlags getShaderFlags(void) const
    {
        return m_shaders;
    }
    VkPipelineCacheCreateFlags getPipelineCacheCreateFlags(void) const
    {
        return m_pipelineCacheCreateFlags;
    }
    bool getCompileMissShaders(void) const
    {
        return m_compileMissShaders;
    }
    bool getUseBinariesFromBinaryData(void) const
    {
        return m_useBinariesFromBinaryData;
    }

protected:
    TestMode m_mode;
    PipelineConstructionType m_pipelineConstructionType;
    VkShaderStageFlags m_shaders;
    bool m_compileMissShaders;
    VkPipelineCacheCreateFlags m_pipelineCacheCreateFlags;
    bool m_useBinariesFromBinaryData;
};

TestParam::TestParam(TestMode mode, PipelineConstructionType pipelineConstructionType, const VkShaderStageFlags shaders,
                     bool compileMissShaders, VkPipelineCacheCreateFlags pipelineCacheCreateFlags,
                     bool useBinariesFromBinaryData)
    : m_mode(mode)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_shaders(shaders)
    , m_compileMissShaders(compileMissShaders)
    , m_pipelineCacheCreateFlags(pipelineCacheCreateFlags)
    , m_useBinariesFromBinaryData(useBinariesFromBinaryData)
{
}

const std::string TestParam::generateTestName(void) const
{
    std::string name = getShaderFlagStr(m_shaders, false);
    if (m_pipelineCacheCreateFlags == VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT)
        name += "_externally_synchronized";
    if (m_useBinariesFromBinaryData)
        name += "_use_binary_data";
    return name;
}

template <class Test>
vkt::TestCase *newTestCase(tcu::TestContext &testContext, const TestParam *testParam)
{
    return new Test(testContext, testParam->generateTestName().c_str(), testParam);
}

Move<VkBuffer> createBufferAndBindMemory(Context &context, VkDeviceSize size, VkBufferUsageFlags usage,
                                         de::MovePtr<Allocation> *pAlloc)
{
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkDevice vkDevice         = context.getDevice();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const VkBufferCreateInfo vertexBufferParams{
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType      sType;
        nullptr,                              // const void*          pNext;
        0u,                                   // VkBufferCreateFlags  flags;
        size,                                 // VkDeviceSize         size;
        usage,                                // VkBufferUsageFlags   usage;
        VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode        sharingMode;
        1u,                                   // uint32_t             queueFamilyCount;
        &queueFamilyIndex                     // const uint32_t*      pQueueFamilyIndices;
    };

    Move<VkBuffer> vertexBuffer = createBuffer(vk, vkDevice, &vertexBufferParams);

    *pAlloc = context.getDefaultAllocator().allocate(getBufferMemoryRequirements(vk, vkDevice, *vertexBuffer),
                                                     MemoryRequirement::HostVisible);
    VK_CHECK(vk.bindBufferMemory(vkDevice, *vertexBuffer, (*pAlloc)->getMemory(), (*pAlloc)->getOffset()));

    return vertexBuffer;
}

Move<VkImage> createImage2DAndBindMemory(Context &context, VkFormat format, uint32_t width, uint32_t height,
                                         VkImageUsageFlags usage, VkSampleCountFlagBits sampleCount,
                                         de::details::MovePtr<Allocation> *pAlloc)
{
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkDevice vkDevice         = context.getDevice();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const VkImageCreateInfo colorImageParams = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType      sType;
        nullptr,                             // const void*          pNext;
        0u,                                  // VkImageCreateFlags   flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType          imageType;
        format,                              // VkFormat             format;
        {width, height, 1u},                 // VkExtent3D           extent;
        1u,                                  // uint32_t             mipLevels;
        1u,                                  // uint32_t             arraySize;
        sampleCount,                         // uint32_t             samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling        tiling;
        usage,                               // VkImageUsageFlags    usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode        sharingMode;
        1u,                                  // uint32_t             queueFamilyCount;
        &queueFamilyIndex,                   // const uint32_t*      pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout        initialLayout;
    };

    Move<VkImage> image = createImage(vk, vkDevice, &colorImageParams);

    *pAlloc = context.getDefaultAllocator().allocate(getImageMemoryRequirements(vk, vkDevice, *image),
                                                     MemoryRequirement::Any);
    VK_CHECK(vk.bindImageMemory(vkDevice, *image, (*pAlloc)->getMemory(), (*pAlloc)->getOffset()));

    return image;
}

// Test Classes
class BaseTestCase : public vkt::TestCase
{
public:
    BaseTestCase(tcu::TestContext &testContext, const std::string &name, const TestParam *param)
        : vkt::TestCase(testContext, name)
        , m_param(*param)
    {
    }
    virtual ~BaseTestCase(void) = default;
    virtual void checkSupport(Context &context) const;

protected:
    const TestParam m_param;
};

class BaseTestInstance : public vkt::TestInstance
{
public:
    enum
    {
        PIPELINE_NDX_NO_BLOBS,
        PIPELINE_NDX_USE_BLOBS,
        PIPELINE_NDX_COUNT,
    };
    BaseTestInstance(Context &context, const TestParam *param);
    virtual ~BaseTestInstance(void) = default;
    virtual tcu::TestStatus iterate(void);

protected:
    virtual tcu::TestStatus verifyTestResult(void) = 0;
    virtual void prepareCommandBuffer(void)        = 0;

protected:
    const TestParam *m_param;
    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBuffer;

    // cache is only used when m_mode is set to TestMode::CACHE
    Move<VkPipelineCache> m_cache;

    // binary related structures are used when m_mode is set to TestMode::BINARY
    PipelineBinaryWrapper m_binaries[4];
};

void BaseTestCase::checkSupport(Context &context) const
{
    if (m_param.getMode() == TestMode::BINARY)
        context.requireDeviceFunctionality("VK_KHR_pipeline_binary");
}

BaseTestInstance::BaseTestInstance(Context &context, const TestParam *param)
    : TestInstance(context)
    , m_param(param)
    , m_binaries{
          {context.getDeviceInterface(), context.getDevice()},
          {context.getDeviceInterface(), context.getDevice()},
          {context.getDeviceInterface(), context.getDevice()},
          {context.getDeviceInterface(), context.getDevice()},
      }
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice vkDevice         = m_context.getDevice();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    // Create command pool
    m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

    // Create command buffer
    m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    // Create the Pipeline Cache
    if (m_param->getMode() == TestMode::CACHE)
    {
        const VkPipelineCacheCreateInfo pipelineCacheCreateInfo{
            VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType             sType;
            nullptr,                                      // const void*                 pNext;
            m_param->getPipelineCacheCreateFlags(),       // VkPipelineCacheCreateFlags  flags;
            0u,                                           // uintptr_t                   initialDataSize;
            nullptr,                                      // const void*                 pInitialData;
        };

        m_cache = createPipelineCache(vk, vkDevice, &pipelineCacheCreateInfo);
    }
}

tcu::TestStatus BaseTestInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();
    const VkQueue queue       = m_context.getUniversalQueue();

    prepareCommandBuffer();

    submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

    return verifyTestResult();
}

class GraphicsTest : public BaseTestCase
{
public:
    GraphicsTest(tcu::TestContext &testContext, const std::string &name, const TestParam *param)
        : BaseTestCase(testContext, name, param)
    {
    }
    virtual ~GraphicsTest(void)
    {
    }
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual void checkSupport(Context &context) const;
    virtual TestInstance *createInstance(Context &context) const;
};

class GraphicsTestInstance : public BaseTestInstance
{
public:
    GraphicsTestInstance(Context &context, const TestParam *param);
    virtual ~GraphicsTestInstance(void) = default;

protected:
    void preparePipelineWrapper(GraphicsPipelineWrapper &gpw, VkPipelineCache cache, bool useMissShaders,
                                bool useShaderModules, VkPipelineBinaryInfoKHR *monolithicBinaryInfo,
                                VkPipelineBinaryInfoKHR *vertexPartBinaryInfo,
                                VkPipelineBinaryInfoKHR *preRasterizationPartBinaryInfo,
                                VkPipelineBinaryInfoKHR *fragmentShaderPartBinaryInfo,
                                VkPipelineBinaryInfoKHR *fragmentOutputPartBinaryInfo);
    virtual void preparePipelines(void);
    void preparePipelinesForBinaries(bool createFromBlobs);
    void prepareRenderPass(const RenderPassWrapper &renderPassFramebuffer, GraphicsPipelineWrapper &pipeline);
    virtual void prepareCommandBuffer(void);
    virtual tcu::TestStatus verifyTestResult(void);

    using GraphicsPipelinePtr = std::unique_ptr<GraphicsPipelineWrapper>;

protected:
    const tcu::UVec2 m_renderSize;
    const VkFormat m_colorFormat;
    const VkFormat m_depthFormat;
    PipelineLayoutWrapper m_pipelineLayout;

    Move<VkImage> m_depthImage;
    de::MovePtr<Allocation> m_depthImageAlloc;
    de::MovePtr<Allocation> m_colorImageAlloc[PIPELINE_NDX_COUNT];
    Move<VkImageView> m_depthAttachmentView;
    VkImageMemoryBarrier m_imageLayoutBarriers[3];

    GraphicsPipelinePtr m_pipeline[PIPELINE_NDX_COUNT];
    Move<VkBuffer> m_vertexBuffer;
    de::MovePtr<Allocation> m_vertexBufferMemory;
    std::vector<Vertex4RGBA> m_vertices;

    Move<VkImage> m_colorImage[PIPELINE_NDX_COUNT];
    Move<VkImageView> m_colorAttachmentView[PIPELINE_NDX_COUNT];
    RenderPassWrapper m_renderPassFramebuffer[PIPELINE_NDX_COUNT];
};

void GraphicsTest::initPrograms(SourceCollections &programCollection) const
{
    enum ShaderCacheOpType
    {
        SHADERS_CACHE_OP_HIT = 0,
        SHADERS_CACHE_OP_MISS,

        SHADERS_CACHE_OP_LAST
    };

    for (uint32_t shaderOpNdx = 0u; shaderOpNdx < SHADERS_CACHE_OP_LAST; shaderOpNdx++)
    {
        const ShaderCacheOpType shaderOp = (ShaderCacheOpType)shaderOpNdx;

        if (shaderOp == SHADERS_CACHE_OP_MISS && !m_param.getCompileMissShaders())
            continue;

        const std::string missHitDiff = (shaderOp == SHADERS_CACHE_OP_HIT ? "" : " + 0.1");
        const std::string missSuffix  = (shaderOp == SHADERS_CACHE_OP_HIT ? "" : "_miss");

        programCollection.glslSources.add("color_vert" + missSuffix)
            << glu::VertexSource("#version 450\n"
                                 "layout(location = 0) in vec4 position;\n"
                                 "layout(location = 1) in vec4 color;\n"
                                 "layout(location = 0) out highp vec4 vtxColor;\n"
                                 "out gl_PerVertex { vec4 gl_Position; };\n"
                                 "void main (void)\n"
                                 "{\n"
                                 "  gl_Position = position;\n"
                                 "  vtxColor = color" +
                                 missHitDiff +
                                 ";\n"
                                 "}\n");

        programCollection.glslSources.add("color_frag" + missSuffix)
            << glu::FragmentSource("#version 310 es\n"
                                   "layout(location = 0) in highp vec4 vtxColor;\n"
                                   "layout(location = 0) out highp vec4 fragColor;\n"
                                   "void main (void)\n"
                                   "{\n"
                                   "  fragColor = vtxColor" +
                                   missHitDiff +
                                   ";\n"
                                   "}\n");

        VkShaderStageFlags shaderFlag = m_param.getShaderFlags();
        if (shaderFlag & VK_SHADER_STAGE_GEOMETRY_BIT)
        {
            programCollection.glslSources.add("unused_geo" + missSuffix)
                << glu::GeometrySource("#version 450 \n"
                                       "layout(triangles) in;\n"
                                       "layout(triangle_strip, max_vertices = 3) out;\n"
                                       "layout(location = 0) in highp vec4 in_vtxColor[];\n"
                                       "layout(location = 0) out highp vec4 vtxColor;\n"
                                       "out gl_PerVertex { vec4 gl_Position; };\n"
                                       "in gl_PerVertex { vec4 gl_Position; } gl_in[];\n"
                                       "void main (void)\n"
                                       "{\n"
                                       "  for(int ndx=0; ndx<3; ndx++)\n"
                                       "  {\n"
                                       "    gl_Position = gl_in[ndx].gl_Position;\n"
                                       "    vtxColor    = in_vtxColor[ndx]" +
                                       missHitDiff +
                                       ";\n"
                                       "    EmitVertex();\n"
                                       "  }\n"
                                       "  EndPrimitive();\n"
                                       "}\n");
        }
        if (shaderFlag & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
        {
            programCollection.glslSources.add("basic_tcs" + missSuffix) << glu::TessellationControlSource(
                "#version 450 \n"
                "layout(vertices = 3) out;\n"
                "layout(location = 0) in highp vec4 color[];\n"
                "layout(location = 0) out highp vec4 vtxColor[];\n"
                "out gl_PerVertex { vec4 gl_Position; } gl_out[3];\n"
                "in gl_PerVertex { vec4 gl_Position; } gl_in[gl_MaxPatchVertices];\n"
                "void main()\n"
                "{\n"
                "  gl_TessLevelOuter[0] = 4.0;\n"
                "  gl_TessLevelOuter[1] = 4.0;\n"
                "  gl_TessLevelOuter[2] = 4.0;\n"
                "  gl_TessLevelInner[0] = 4.0;\n"
                "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
                "  vtxColor[gl_InvocationID] = color[gl_InvocationID]" +
                missHitDiff +
                ";\n"
                "}\n");
        }
        if (shaderFlag & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
        {
            programCollection.glslSources.add("basic_tes" + missSuffix) << glu::TessellationEvaluationSource(
                "#version 450 \n"
                "layout(triangles, fractional_even_spacing, ccw) in;\n"
                "layout(location = 0) in highp vec4 colors[];\n"
                "layout(location = 0) out highp vec4 vtxColor;\n"
                "out gl_PerVertex { vec4 gl_Position; };\n"
                "in gl_PerVertex { vec4 gl_Position; } gl_in[gl_MaxPatchVertices];\n"
                "void main() \n"
                "{\n"
                "  float u = gl_TessCoord.x;\n"
                "  float v = gl_TessCoord.y;\n"
                "  float w = gl_TessCoord.z;\n"
                "  vec4 pos = vec4(0);\n"
                "  vec4 color = vec4(0)" +
                missHitDiff +
                ";\n"
                "  pos.xyz += u * gl_in[0].gl_Position.xyz;\n"
                "  color.xyz += u * colors[0].xyz;\n"
                "  pos.xyz += v * gl_in[1].gl_Position.xyz;\n"
                "  color.xyz += v * colors[1].xyz;\n"
                "  pos.xyz += w * gl_in[2].gl_Position.xyz;\n"
                "  color.xyz += w * colors[2].xyz;\n"
                "  pos.w = 1.0;\n"
                "  color.w = 1.0;\n"
                "  gl_Position = pos;\n"
                "  vtxColor = color;\n"
                "}\n");
        }
    }
}

void GraphicsTest::checkSupport(Context &context) const
{
    if (m_param.getMode() == TestMode::BINARY)
        context.requireDeviceFunctionality("VK_KHR_pipeline_binary");

    if (m_param.getShaderFlags() & VK_SHADER_STAGE_GEOMETRY_BIT)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
    if ((m_param.getShaderFlags() & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) ||
        (m_param.getShaderFlags() & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_param.getPipelineConstructionType());
}

TestInstance *GraphicsTest::createInstance(Context &context) const
{
    return new GraphicsTestInstance(context, &m_param);
}

GraphicsTestInstance::GraphicsTestInstance(Context &context, const TestParam *param)
    : BaseTestInstance(context, param)
    , m_renderSize(32u, 32u)
    , m_colorFormat(VK_FORMAT_R8G8B8A8_UNORM)
    , m_depthFormat(VK_FORMAT_D16_UNORM)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();

    // pipeline reconstructed from binaries should not use RETAIN_LINK_TIME_OPTIMIZATION/LINK_TIME_OPTIMIZATION
    PipelineConstructionType pipelineConstructionTypeForUseBlobs = param->getPipelineConstructionType();
    if ((param->getMode() == TestMode::BINARY) &&
        (pipelineConstructionTypeForUseBlobs == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY))
        pipelineConstructionTypeForUseBlobs = PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY;

    m_pipeline[PIPELINE_NDX_NO_BLOBS]  = GraphicsPipelinePtr(new GraphicsPipelineWrapper(
        context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(), context.getDevice(),
        context.getDeviceExtensions(), param->getPipelineConstructionType()));
    m_pipeline[PIPELINE_NDX_USE_BLOBS] = GraphicsPipelinePtr(new GraphicsPipelineWrapper(
        context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(), context.getDevice(),
        context.getDeviceExtensions(), pipelineConstructionTypeForUseBlobs));

    if (param->getMode() == TestMode::BINARY)
    {
        m_pipeline[PIPELINE_NDX_NO_BLOBS]->setPipelineCreateFlags2(VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR);
        m_pipeline[PIPELINE_NDX_USE_BLOBS]->setPipelineCreateFlags2(VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR);
    }

    // Create vertex buffer
    {
        m_vertexBuffer =
            createBufferAndBindMemory(m_context, 1024u, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &m_vertexBufferMemory);

        m_vertices = createOverlappingQuads();
        // Load vertices into vertex buffer
        deMemcpy(m_vertexBufferMemory->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4RGBA));
        flushAlloc(vk, vkDevice, *m_vertexBufferMemory);
    }

    // Create render pass
    m_renderPassFramebuffer[PIPELINE_NDX_NO_BLOBS] =
        RenderPassWrapper(m_param->getPipelineConstructionType(), vk, vkDevice, m_colorFormat, m_depthFormat);
    m_renderPassFramebuffer[PIPELINE_NDX_USE_BLOBS] =
        RenderPassWrapper(m_param->getPipelineConstructionType(), vk, vkDevice, m_colorFormat, m_depthFormat);

    const VkComponentMapping ComponentMappingRGBA = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                                                     VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    // Create color image
    {
        m_colorImage[PIPELINE_NDX_NO_BLOBS] =
            createImage2DAndBindMemory(m_context, m_colorFormat, m_renderSize.x(), m_renderSize.y(),
                                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                       VK_SAMPLE_COUNT_1_BIT, &m_colorImageAlloc[PIPELINE_NDX_NO_BLOBS]);
        m_colorImage[PIPELINE_NDX_USE_BLOBS] =
            createImage2DAndBindMemory(m_context, m_colorFormat, m_renderSize.x(), m_renderSize.y(),
                                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                       VK_SAMPLE_COUNT_1_BIT, &m_colorImageAlloc[PIPELINE_NDX_USE_BLOBS]);
    }

    // Create depth image
    {
        m_depthImage = createImage2DAndBindMemory(m_context, m_depthFormat, m_renderSize.x(), m_renderSize.y(),
                                                  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_SAMPLE_COUNT_1_BIT,
                                                  &m_depthImageAlloc);
    }

    // Set up image layout transition barriers
    {
        VkImageMemoryBarrier colorImageBarrier{
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,      // VkStructureType sType;
            nullptr,                                     // const void* pNext;
            0u,                                          // VkAccessFlags srcAccessMask;
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,        // VkAccessFlags dstAccessMask;
            VK_IMAGE_LAYOUT_UNDEFINED,                   // VkImageLayout oldLayout;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,    // VkImageLayout newLayout;
            VK_QUEUE_FAMILY_IGNORED,                     // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                     // uint32_t dstQueueFamilyIndex;
            *m_colorImage[PIPELINE_NDX_NO_BLOBS],        // VkImage image;
            {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u}, // VkImageSubresourceRange subresourceRange;
        };

        m_imageLayoutBarriers[0] = colorImageBarrier;

        colorImageBarrier.image  = *m_colorImage[PIPELINE_NDX_USE_BLOBS];
        m_imageLayoutBarriers[1] = colorImageBarrier;

        const VkImageMemoryBarrier depthImageBarrier{
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,           // VkStructureType sType;
            nullptr,                                          // const void* pNext;
            0u,                                               // VkAccessFlags srcAccessMask;
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,     // VkAccessFlags dstAccessMask;
            VK_IMAGE_LAYOUT_UNDEFINED,                        // VkImageLayout oldLayout;
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // VkImageLayout newLayout;
            VK_QUEUE_FAMILY_IGNORED,                          // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                          // uint32_t dstQueueFamilyIndex;
            *m_depthImage,                                    // VkImage image;
            {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u},      // VkImageSubresourceRange subresourceRange;
        };

        m_imageLayoutBarriers[2] = depthImageBarrier;
    }
    // Create color attachment view
    {
        VkImageViewCreateInfo colorAttachmentViewParams{
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,    // VkStructureType          sType;
            nullptr,                                     // const void*              pNext;
            0u,                                          // VkImageViewCreateFlags   flags;
            *m_colorImage[PIPELINE_NDX_NO_BLOBS],        // VkImage                  image;
            VK_IMAGE_VIEW_TYPE_2D,                       // VkImageViewType          viewType;
            m_colorFormat,                               // VkFormat                 format;
            ComponentMappingRGBA,                        // VkComponentMapping       components;
            {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u}, // VkImageSubresourceRange  subresourceRange;
        };

        m_colorAttachmentView[PIPELINE_NDX_NO_BLOBS] = createImageView(vk, vkDevice, &colorAttachmentViewParams);

        colorAttachmentViewParams.image               = *m_colorImage[PIPELINE_NDX_USE_BLOBS];
        m_colorAttachmentView[PIPELINE_NDX_USE_BLOBS] = createImageView(vk, vkDevice, &colorAttachmentViewParams);
    }

    // Create depth attachment view
    {
        const VkImageViewCreateInfo depthAttachmentViewParams{
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,    // VkStructureType          sType;
            nullptr,                                     // const void*              pNext;
            0u,                                          // VkImageViewCreateFlags   flags;
            *m_depthImage,                               // VkImage                  image;
            VK_IMAGE_VIEW_TYPE_2D,                       // VkImageViewType          viewType;
            m_depthFormat,                               // VkFormat                 format;
            ComponentMappingRGBA,                        // VkComponentMapping       components;
            {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u}, // VkImageSubresourceRange  subresourceRange;
        };

        m_depthAttachmentView = createImageView(vk, vkDevice, &depthAttachmentViewParams);
    }

    // Create framebuffer
    {
        std::vector<VkImage> images = {
            *m_colorImage[PIPELINE_NDX_NO_BLOBS],
            *m_depthImage,
        };
        VkImageView attachmentBindInfos[2] = {
            *m_colorAttachmentView[PIPELINE_NDX_NO_BLOBS],
            *m_depthAttachmentView,
        };

        VkFramebufferCreateInfo framebufferParams = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,        // VkStructureType              sType;
            nullptr,                                          // const void*                  pNext;
            0u,                                               // VkFramebufferCreateFlags     flags;
            *m_renderPassFramebuffer[PIPELINE_NDX_USE_BLOBS], // VkRenderPass                 renderPass;
            2u,                                               // uint32_t                     attachmentCount;
            attachmentBindInfos,                              // const VkImageView*           pAttachments;
            (uint32_t)m_renderSize.x(),                       // uint32_t                     width;
            (uint32_t)m_renderSize.y(),                       // uint32_t                     height;
            1u,                                               // uint32_t                     layers;
        };

        m_renderPassFramebuffer[PIPELINE_NDX_NO_BLOBS].createFramebuffer(vk, vkDevice, &framebufferParams, images);

        framebufferParams.renderPass = *m_renderPassFramebuffer[PIPELINE_NDX_USE_BLOBS];
        images[0]                    = *m_colorImage[PIPELINE_NDX_USE_BLOBS];
        attachmentBindInfos[0]       = *m_colorAttachmentView[PIPELINE_NDX_USE_BLOBS];
        m_renderPassFramebuffer[PIPELINE_NDX_USE_BLOBS].createFramebuffer(vk, vkDevice, &framebufferParams, images);
    }

    // Create pipeline layout
    {
        const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
            nullptr,                                       // const void* pNext;
            0u,                                            // VkPipelineLayoutCreateFlags flags;
            0u,                                            // uint32_t setLayoutCount;
            nullptr,                                       // const VkDescriptorSetLayout* pSetLayouts;
            0u,                                            // uint32_t pushConstantRangeCount;
            nullptr                                        // const VkPushConstantRange* pPushConstantRanges;
        };

        m_pipelineLayout =
            PipelineLayoutWrapper(m_param->getPipelineConstructionType(), vk, vkDevice, &pipelineLayoutParams);
    }
}

void GraphicsTestInstance::preparePipelineWrapper(GraphicsPipelineWrapper &gpw, VkPipelineCache cache = VK_NULL_HANDLE,
                                                  bool useMissShaders = false, bool useShaderModules = true,
                                                  VkPipelineBinaryInfoKHR *monolithicBinaryInfo           = nullptr,
                                                  VkPipelineBinaryInfoKHR *vertexPartBinaryInfo           = nullptr,
                                                  VkPipelineBinaryInfoKHR *preRasterizationPartBinaryInfo = nullptr,
                                                  VkPipelineBinaryInfoKHR *fragmentShaderPartBinaryInfo   = nullptr,
                                                  VkPipelineBinaryInfoKHR *fragmentOutputPartBinaryInfo   = nullptr)
{
    VkStencilOpState frontAndBack;
    deMemset(&frontAndBack, 0x00, sizeof(VkStencilOpState));

    static const VkPipelineDepthStencilStateCreateInfo defaultDepthStencilState{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                    // const void* pNext;
        0u,                                                         // VkPipelineDepthStencilStateCreateFlags flags;
        VK_TRUE,                                                    // VkBool32 depthTestEnable;
        VK_TRUE,                                                    // VkBool32 depthWriteEnable;
        VK_COMPARE_OP_LESS_OR_EQUAL,                                // VkCompareOp depthCompareOp;
        VK_FALSE,                                                   // VkBool32 depthBoundsTestEnable;
        VK_FALSE,                                                   // VkBool32 stencilTestEnable;
        frontAndBack,                                               // VkStencilOpState front;
        frontAndBack,                                               // VkStencilOpState back;
        0.0f,                                                       // float minDepthBounds;
        1.0f,                                                       // float maxDepthBounds;
    };

    static const VkVertexInputBindingDescription defaultVertexInputBindingDescription{
        0u,                          // uint32_t binding;
        sizeof(Vertex4RGBA),         // uint32_t strideInBytes;
        VK_VERTEX_INPUT_RATE_VERTEX, // VkVertexInputRate inputRate;
    };

    static const VkVertexInputAttributeDescription defaultVertexInputAttributeDescriptions[]{
        {
            0u,                            // uint32_t location;
            0u,                            // uint32_t binding;
            VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
            0u                             // uint32_t offsetInBytes;
        },
        {
            1u,                            // uint32_t location;
            0u,                            // uint32_t binding;
            VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
            offsetof(Vertex4RGBA, color),  // uint32_t offsetInBytes;
        }};

    static const VkPipelineVertexInputStateCreateInfo defaultVertexInputStateParams{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                   // const void* pNext;
        0u,                                                        // VkPipelineVertexInputStateCreateFlags flags;
        1u,                                                        // uint32_t vertexBindingDescriptionCount;
        &defaultVertexInputBindingDescription,   // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
        2u,                                      // uint32_t vertexAttributeDescriptionCount;
        defaultVertexInputAttributeDescriptions, // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
    };

    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();
    const std::string postfix = useMissShaders ? "_miss" : "";

    auto createModule = [&vk, vkDevice, &postfix](Context &context, std::string shaderName)
    { return ShaderWrapper(vk, vkDevice, context.getBinaryCollection().get(shaderName + postfix), 0); };

    // Bind shader stages
    ShaderWrapper vertShaderModule = createModule(m_context, "color_vert");
    ShaderWrapper fragShaderModule = createModule(m_context, "color_frag");
    ShaderWrapper tescShaderModule;
    ShaderWrapper teseShaderModule;
    ShaderWrapper geomShaderModule;

    if (m_param->getShaderFlags() & VK_SHADER_STAGE_GEOMETRY_BIT)
        geomShaderModule = createModule(m_context, "unused_geo");
    if (m_param->getShaderFlags() & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
        tescShaderModule = createModule(m_context, "basic_tcs");
    if (m_param->getShaderFlags() & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
        teseShaderModule = createModule(m_context, "basic_tes");

    const std::vector<VkViewport> viewport{makeViewport(m_renderSize)};
    const std::vector<VkRect2D> scissor{makeRect2D(m_renderSize)};

    gpw.setDefaultTopology((m_param->getShaderFlags() & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) ?
                               VK_PRIMITIVE_TOPOLOGY_PATCH_LIST :
                               VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .setDefaultRasterizationState()
        .setDefaultColorBlendState()
        .setDefaultMultisampleState()
        .setMonolithicPipelineLayout(m_pipelineLayout)
        .disableShaderModules(!useShaderModules)
        .setupVertexInputState(&defaultVertexInputStateParams, 0, VK_NULL_HANDLE, 0, vertexPartBinaryInfo)
        .setupPreRasterizationShaderState3(viewport, scissor, m_pipelineLayout, *m_renderPassFramebuffer[0], 0u,
                                           vertShaderModule, 0, nullptr, tescShaderModule, 0, teseShaderModule, 0,
                                           geomShaderModule, 0, 0, 0, 0, 0, 0, 0, VK_NULL_HANDLE, 0,
                                           preRasterizationPartBinaryInfo)
        .setupFragmentShaderState2(m_pipelineLayout, *m_renderPassFramebuffer[0], 0u, fragShaderModule, 0,
                                   &defaultDepthStencilState, 0, 0, VK_NULL_HANDLE, 0, {}, fragmentShaderPartBinaryInfo)
        .setupFragmentOutputState(*m_renderPassFramebuffer[0], 0, 0, 0, VK_NULL_HANDLE, 0, {},
                                  fragmentOutputPartBinaryInfo)
        .buildPipeline(cache, VK_NULL_HANDLE, 0, {}, monolithicBinaryInfo);

    // reuse graphics tests to also check if pipeline key is valid when pipeline binaries are tested
    if ((m_param->getMode() == TestMode::BINARY) && useShaderModules)
    {
        if (m_param->getPipelineConstructionType() == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
        {
            auto &pipelineCreateInfo = gpw.getPipelineCreateInfo();
            auto pipelineKey         = m_binaries[0].getPipelineKey(&pipelineCreateInfo);
            if (pipelineKey.keySize == 0)
                TCU_FAIL("vkGetPipelineKeyKHR returned keySize == 0");
        }
        else
        {
            for (uint32_t i = 0; i < 4; ++i)
            {
                auto &pipelineCreateInfo = gpw.getPartialPipelineCreateInfo(i);
                auto pipelineKey         = m_binaries[i].getPipelineKey(&pipelineCreateInfo);
                if (pipelineKey.keySize == 0)
                    TCU_FAIL("vkGetPipelineKeyKHR returned keySize == 0");
            }
        }
    }
}

void GraphicsTestInstance::preparePipelinesForBinaries(bool createFromBlobs = false)
{
    DE_ASSERT(m_param->getMode() == TestMode::BINARY);

    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();

    preparePipelineWrapper(*m_pipeline[PIPELINE_NDX_NO_BLOBS], VK_NULL_HANDLE, false, true);

    if (m_param->getPipelineConstructionType() == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
    {
        VkPipeline pipeline = m_pipeline[PIPELINE_NDX_NO_BLOBS]->getPipeline();
        m_binaries[0].createPipelineBinariesFromPipeline(pipeline);

        if (createFromBlobs)
        {
            // read binaries data out of the device
            std::vector<VkPipelineBinaryDataKHR> pipelineDataInfo;
            std::vector<std::vector<uint8_t>> pipelineDataBlob;
            m_binaries[0].getPipelineBinaryData(pipelineDataInfo, pipelineDataBlob);

            // clear pipeline binaries objects
            m_binaries[0].deletePipelineBinariesKeepKeys();

            // recreate binaries from data blobs
            m_binaries[0].createPipelineBinariesFromBinaryData(pipelineDataInfo);
        }
        else
        {
            VkReleaseCapturedPipelineDataInfoKHR releaseCapturedPipelineDataInfo = initVulkanStructure();
            releaseCapturedPipelineDataInfo.pipeline                             = pipeline;
            vk.releaseCapturedPipelineDataKHR(vkDevice, &releaseCapturedPipelineDataInfo, nullptr);
        }

        VkPipelineBinaryInfoKHR pipelineBinaryInfo = m_binaries[0].preparePipelineBinaryInfo();
        preparePipelineWrapper(*m_pipeline[PIPELINE_NDX_USE_BLOBS], VK_NULL_HANDLE, false, false, &pipelineBinaryInfo);
    }
    else
    {
        for (uint32_t i = 0; i < 4; ++i)
        {
            VkPipeline partialPipeline = m_pipeline[PIPELINE_NDX_NO_BLOBS]->getPartialPipeline(i);
            m_binaries[i].createPipelineBinariesFromPipeline(partialPipeline);

            if (createFromBlobs)
            {
                // read binaries data out of the device
                std::vector<VkPipelineBinaryDataKHR> pipelineDataInfo;
                std::vector<std::vector<uint8_t>> pipelineDataBlob;
                m_binaries[i].getPipelineBinaryData(pipelineDataInfo, pipelineDataBlob);

                // clear pipeline binaries objects
                m_binaries[i].deletePipelineBinariesKeepKeys();

                // recreate binaries from data blobs
                m_binaries[i].createPipelineBinariesFromBinaryData(pipelineDataInfo);
            }
            else
            {
                VkReleaseCapturedPipelineDataInfoKHR releaseCapturedPipelineDataInfo = initVulkanStructure();
                releaseCapturedPipelineDataInfo.pipeline                             = partialPipeline;
                vk.releaseCapturedPipelineDataKHR(vkDevice, &releaseCapturedPipelineDataInfo, nullptr);
            }
        }

        VkPipelineBinaryInfoKHR pipelinePartsBinaryInfo[4];
        VkPipelineBinaryInfoKHR *binaryInfoPtr[4];
        deMemset(binaryInfoPtr, 0, 4 * sizeof(nullptr));

        for (uint32_t i = 0; i < 4; ++i)
        {
            if (m_binaries[i].getBinariesCount() == 0)
                continue;
            pipelinePartsBinaryInfo[i] = m_binaries[i].preparePipelineBinaryInfo();
            binaryInfoPtr[i]           = &pipelinePartsBinaryInfo[i];
        };

        preparePipelineWrapper(*m_pipeline[PIPELINE_NDX_USE_BLOBS], VK_NULL_HANDLE, false, false, DE_NULL,
                               binaryInfoPtr[0], binaryInfoPtr[1], binaryInfoPtr[2], binaryInfoPtr[3]);
    }
}

void GraphicsTestInstance::preparePipelines(void)
{
    if (m_param->getMode() == TestMode::CACHE)
    {
        preparePipelineWrapper(*m_pipeline[PIPELINE_NDX_NO_BLOBS], *m_cache);
        preparePipelineWrapper(*m_pipeline[PIPELINE_NDX_USE_BLOBS], *m_cache);
    }
    else
        preparePipelinesForBinaries();
}

void GraphicsTestInstance::prepareRenderPass(const RenderPassWrapper &renderPassFramebuffer,
                                             GraphicsPipelineWrapper &pipeline)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();

    const VkClearValue attachmentClearValues[2]{
        defaultClearValue(m_colorFormat),
        defaultClearValue(m_depthFormat),
    };

    renderPassFramebuffer.begin(vk, *m_cmdBuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), 2u,
                                attachmentClearValues);

    pipeline.bind(*m_cmdBuffer);
    VkDeviceSize offsets = 0u;
    vk.cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &m_vertexBuffer.get(), &offsets);
    vk.cmdDraw(*m_cmdBuffer, (uint32_t)m_vertices.size(), 1u, 0u, 0u);

    renderPassFramebuffer.end(vk, *m_cmdBuffer);
}

void GraphicsTestInstance::prepareCommandBuffer(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();

    preparePipelines();

    beginCommandBuffer(vk, *m_cmdBuffer, 0u);

    vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                          (VkDependencyFlags)0, 0u, nullptr, 0u, nullptr, DE_LENGTH_OF_ARRAY(m_imageLayoutBarriers),
                          m_imageLayoutBarriers);

    prepareRenderPass(m_renderPassFramebuffer[PIPELINE_NDX_NO_BLOBS], *m_pipeline[PIPELINE_NDX_NO_BLOBS]);

    // After the first render pass, the images are in correct layouts

    prepareRenderPass(m_renderPassFramebuffer[PIPELINE_NDX_USE_BLOBS], *m_pipeline[PIPELINE_NDX_USE_BLOBS]);

    endCommandBuffer(vk, *m_cmdBuffer);
}

tcu::TestStatus GraphicsTestInstance::verifyTestResult(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice vkDevice         = m_context.getDevice();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    const VkQueue queue = m_context.getUniversalQueue();
    de::MovePtr<tcu::TextureLevel> resultNoCache =
        readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, m_context.getDefaultAllocator(),
                            *m_colorImage[PIPELINE_NDX_NO_BLOBS], m_colorFormat, m_renderSize);
    de::MovePtr<tcu::TextureLevel> resultCache =
        readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, m_context.getDefaultAllocator(),
                            *m_colorImage[PIPELINE_NDX_USE_BLOBS], m_colorFormat, m_renderSize);

    bool compareOk = tcu::intThresholdCompare(m_context.getTestContext().getLog(), "IntImageCompare",
                                              "Image comparison", resultNoCache->getAccess(), resultCache->getAccess(),
                                              tcu::UVec4(1, 1, 1, 1), tcu::COMPARE_LOG_RESULT);

    if (compareOk)
        return tcu::TestStatus::pass("Render images w/o cached pipeline match.");
    else
        return tcu::TestStatus::fail("Render Images mismatch.");
}

class ComputeTest : public BaseTestCase
{
public:
    ComputeTest(tcu::TestContext &testContext, const std::string &name, const TestParam *param)
        : BaseTestCase(testContext, name, param)
    {
    }
    virtual ~ComputeTest(void) = default;
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
};

class ComputeTestInstance : public BaseTestInstance
{
public:
    ComputeTestInstance(Context &context, const TestParam *param);
    virtual ~ComputeTestInstance(void) = default;
    virtual void prepareCommandBuffer(void);

protected:
    virtual tcu::TestStatus verifyTestResult(void);
    void buildBuffers(void);
    void buildDescriptorSets(uint32_t ndx);
    void buildShader(void);
    void buildPipeline(uint32_t ndx);

protected:
    Move<VkBuffer> m_inputBuf;
    de::MovePtr<Allocation> m_inputBufferAlloc;
    Move<VkShaderModule> m_computeShaderModule;

    Move<VkBuffer> m_outputBuf[PIPELINE_NDX_COUNT];
    de::MovePtr<Allocation> m_outputBufferAlloc[PIPELINE_NDX_COUNT];

    Move<VkDescriptorPool> m_descriptorPool[PIPELINE_NDX_COUNT];
    Move<VkDescriptorSetLayout> m_descriptorSetLayout[PIPELINE_NDX_COUNT];
    Move<VkDescriptorSet> m_descriptorSet[PIPELINE_NDX_COUNT];

    Move<VkPipelineLayout> m_pipelineLayout[PIPELINE_NDX_COUNT];
    Move<VkPipeline> m_pipeline[PIPELINE_NDX_COUNT];
};

void ComputeTest::initPrograms(SourceCollections &programCollection) const
{
    programCollection.glslSources.add("basic_compute") << glu::ComputeSource(
        "#version 310 es\n"
        "layout(local_size_x = 1) in;\n"
        "layout(std430) buffer;\n"
        "layout(binding = 0) readonly buffer Input0\n"
        "{\n"
        "  vec4 elements[];\n"
        "} input_data0;\n"
        "layout(binding = 1) writeonly buffer Output\n"
        "{\n"
        "  vec4 elements[];\n"
        "} output_data;\n"
        "void main()\n"
        "{\n"
        "  uint ident = gl_GlobalInvocationID.x;\n"
        "  output_data.elements[ident] = input_data0.elements[ident] * input_data0.elements[ident];\n"
        "}");
}

TestInstance *ComputeTest::createInstance(Context &context) const
{
    return new ComputeTestInstance(context, &m_param);
}

void ComputeTestInstance::buildBuffers(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();

    // Create buffer object, allocate storage, and generate input data
    const VkDeviceSize size = sizeof(tcu::Vec4) * 128u;
    m_inputBuf = createBufferAndBindMemory(m_context, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &m_inputBufferAlloc);

    // Initialize input buffer
    tcu::Vec4 *pVec = reinterpret_cast<tcu::Vec4 *>(m_inputBufferAlloc->getHostPtr());
    for (uint32_t ndx = 0u; ndx < 128u; ndx++)
    {
        for (uint32_t component = 0u; component < 4u; component++)
            pVec[ndx][component] = (float)(ndx * (component + 1u));
    }
    flushAlloc(vk, vkDevice, *m_inputBufferAlloc);

    // Clear the output buffer
    for (uint32_t ndx = 0; ndx < PIPELINE_NDX_COUNT; ndx++)
    {
        m_outputBuf[ndx] =
            createBufferAndBindMemory(m_context, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &m_outputBufferAlloc[ndx]);

        pVec = reinterpret_cast<tcu::Vec4 *>(m_outputBufferAlloc[ndx]->getHostPtr());

        for (uint32_t i = 0; i < (size / sizeof(tcu::Vec4)); i++)
            pVec[i] = tcu::Vec4(0.0f);

        flushAlloc(vk, vkDevice, *m_outputBufferAlloc[ndx]);
    }
}

void ComputeTestInstance::buildDescriptorSets(uint32_t ndx)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();

    // Create descriptor set layout
    DescriptorSetLayoutBuilder descLayoutBuilder;

    for (uint32_t bindingNdx = 0u; bindingNdx < 2u; bindingNdx++)
        descLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);

    m_descriptorSetLayout[ndx] = descLayoutBuilder.build(vk, vkDevice);

    std::vector<VkDescriptorBufferInfo> descriptorInfos;
    descriptorInfos.push_back(makeDescriptorBufferInfo(*m_inputBuf, 0u, sizeof(tcu::Vec4) * 128u));
    descriptorInfos.push_back(makeDescriptorBufferInfo(*m_outputBuf[ndx], 0u, sizeof(tcu::Vec4) * 128u));

    // Create descriptor pool
    m_descriptorPool[ndx] = DescriptorPoolBuilder()
                                .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u)
                                .build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    // Create descriptor set
    const VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType                 sType;
        nullptr,                                        // const void*                     pNext;
        *m_descriptorPool[ndx],                         // VkDescriptorPool                descriptorPool;
        1u,                                             // uint32_t                        setLayoutCount;
        &m_descriptorSetLayout[ndx].get(),              // const VkDescriptorSetLayout*    pSetLayouts;
    };
    m_descriptorSet[ndx] = allocateDescriptorSet(vk, vkDevice, &descriptorSetAllocInfo);

    DescriptorSetUpdateBuilder builder;
    for (uint32_t descriptorNdx = 0u; descriptorNdx < 2u; descriptorNdx++)
    {
        builder.writeSingle(*m_descriptorSet[ndx], DescriptorSetUpdateBuilder::Location::binding(descriptorNdx),
                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfos[descriptorNdx]);
    }
    builder.update(vk, vkDevice);
}

void ComputeTestInstance::buildShader(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();

    // Create compute shader
    VkShaderModuleCreateInfo shaderModuleCreateInfo = {
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,                    // VkStructureType             sType;
        nullptr,                                                        // const void*                 pNext;
        0u,                                                             // VkShaderModuleCreateFlags   flags;
        m_context.getBinaryCollection().get("basic_compute").getSize(), // uintptr_t                   codeSize;
        (uint32_t *)m_context.getBinaryCollection()
            .get("basic_compute")
            .getBinary(), // const uint32_t*             pCode;
    };
    m_computeShaderModule = createShaderModule(vk, vkDevice, &shaderModuleCreateInfo);
}

void ComputeTestInstance::buildPipeline(uint32_t ndx)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();

    // Create compute pipeline layout
    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType                 sType;
        nullptr,                                       // const void*                     pNext;
        0u,                                            // VkPipelineLayoutCreateFlags     flags;
        1u,                                            // uint32_t                        setLayoutCount;
        &m_descriptorSetLayout[ndx].get(),             // const VkDescriptorSetLayout*    pSetLayouts;
        0u,                                            // uint32_t                        pushConstantRangeCount;
        nullptr,                                       // const VkPushConstantRange*      pPushConstantRanges;
    };

    m_pipelineLayout[ndx] = createPipelineLayout(vk, vkDevice, &pipelineLayoutCreateInfo);

    const VkPipelineShaderStageCreateInfo stageCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType                     sType;
        nullptr,                                             // const void*                         pNext;
        0u,                                                  // VkPipelineShaderStageCreateFlags    flags;
        VK_SHADER_STAGE_COMPUTE_BIT,                         // VkShaderStageFlagBits               stage;
        *m_computeShaderModule,                              // VkShaderModule                      module;
        "main",                                              // const char*                         pName;
        nullptr,                                             // const VkSpecializationInfo*         pSpecializationInfo;
    };

    VkPipelineCreateFlags2CreateInfoKHR pipelineFlags2CreateInfo = initVulkanStructure();
    pipelineFlags2CreateInfo.flags                               = VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR;
    const void *pNext = (m_param->getMode() == TestMode::BINARY) ? &pipelineFlags2CreateInfo : nullptr;
    VkComputePipelineCreateInfo pipelineCreateInfo{
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType sType;
        pNext,                                          // const void* pNext;
        0,                                              // VkPipelineCreateFlags flags;
        stageCreateInfo,                                // VkPipelineShaderStageCreateInfo stage;
        *m_pipelineLayout[ndx],                         // VkPipelineLayout layout;
        VK_NULL_HANDLE,                                 // VkPipeline basePipelineHandle;
        0u,                                             // int32_t basePipelineIndex;
    };

    if (m_param->getMode() == TestMode::CACHE)
        m_pipeline[ndx] = createComputePipeline(vk, vkDevice, *m_cache, &pipelineCreateInfo);
    else
    {
        if (ndx == PIPELINE_NDX_NO_BLOBS)
        {
            auto pipelineKey = m_binaries[0].getPipelineKey(&pipelineCreateInfo);
            if (pipelineKey.keySize == 0)
                TCU_FAIL("vkGetPipelineKeyKHR returned keySize == 0");

            // create pipeline
            m_pipeline[ndx] = createComputePipeline(vk, vkDevice, VK_NULL_HANDLE, &pipelineCreateInfo);

            // prepare pipeline binaries
            m_binaries[0].createPipelineBinariesFromPipeline(*m_pipeline[ndx]);

            if (m_param->getUseBinariesFromBinaryData())
            {
                // read binaries data out of the device
                std::vector<VkPipelineBinaryDataKHR> pipelineDataInfo;
                std::vector<std::vector<uint8_t>> pipelineDataBlob;
                m_binaries[0].getPipelineBinaryData(pipelineDataInfo, pipelineDataBlob);

                // clear pipeline binaries objects
                m_binaries[0].deletePipelineBinariesKeepKeys();

                // recreate binaries from data blobs
                m_binaries[0].createPipelineBinariesFromBinaryData(pipelineDataInfo);
            }
        }
        else
        {
            // create pipeline using binary data and use pipelineCreateInfo with no shader stage
            VkPipelineBinaryInfoKHR pipelineBinaryInfo = m_binaries[0].preparePipelineBinaryInfo();
            pipelineCreateInfo.pNext                   = &pipelineBinaryInfo;
            pipelineCreateInfo.stage.module            = VK_NULL_HANDLE;
            m_pipeline[ndx] = createComputePipeline(vk, vkDevice, VK_NULL_HANDLE, &pipelineCreateInfo);
        }
    }
}

ComputeTestInstance::ComputeTestInstance(Context &context, const TestParam *param) : BaseTestInstance(context, param)
{
    buildBuffers();

    buildDescriptorSets(PIPELINE_NDX_NO_BLOBS);

    buildDescriptorSets(PIPELINE_NDX_USE_BLOBS);

    buildShader();

    buildPipeline(PIPELINE_NDX_NO_BLOBS);

    buildPipeline(PIPELINE_NDX_USE_BLOBS);
}

void ComputeTestInstance::prepareCommandBuffer(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();

    beginCommandBuffer(vk, *m_cmdBuffer, 0u);

    for (uint32_t ndx = 0; ndx < PIPELINE_NDX_COUNT; ndx++)
    {
        vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipeline[ndx]);
        vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout[ndx], 0u, 1u,
                                 &m_descriptorSet[ndx].get(), 0u, nullptr);
        vk.cmdDispatch(*m_cmdBuffer, 128u, 1u, 1u);
    }

    endCommandBuffer(vk, *m_cmdBuffer);
}

tcu::TestStatus ComputeTestInstance::verifyTestResult(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();

    // Read the content of output buffers
    invalidateAlloc(vk, vkDevice, *m_outputBufferAlloc[PIPELINE_NDX_NO_BLOBS]);

    invalidateAlloc(vk, vkDevice, *m_outputBufferAlloc[PIPELINE_NDX_USE_BLOBS]);
    // Compare the content
    uint8_t *bufNoCache = reinterpret_cast<uint8_t *>(m_outputBufferAlloc[PIPELINE_NDX_NO_BLOBS]->getHostPtr());
    uint8_t *bufCached  = reinterpret_cast<uint8_t *>(m_outputBufferAlloc[PIPELINE_NDX_USE_BLOBS]->getHostPtr());
    for (uint32_t ndx = 0u; ndx < sizeof(tcu::Vec4) * 128u; ndx++)
    {
        if (bufNoCache[ndx] != bufCached[ndx])
        {
            return tcu::TestStatus::fail("Output buffers w/o pipeline blobs mismatch.");
        }
    }

    return tcu::TestStatus::pass("Output buffers w/o pipeline blobs match.");
}

class PipelineFromBlobsTest : public GraphicsTest
{
public:
    PipelineFromBlobsTest(tcu::TestContext &testContext, const std::string &name, const TestParam *param);
    virtual ~PipelineFromBlobsTest(void) = default;
    virtual TestInstance *createInstance(Context &context) const;
};

PipelineFromBlobsTest::PipelineFromBlobsTest(tcu::TestContext &testContext, const std::string &name,
                                             const TestParam *param)
    : GraphicsTest(testContext, name, param)
{
}

class PipelineFromBlobsTestInstance : public GraphicsTestInstance
{
public:
    PipelineFromBlobsTestInstance(Context &context, const TestParam *param);
    virtual ~PipelineFromBlobsTestInstance(void);

protected:
    void preparePipelines(void);

protected:
    Move<VkPipelineCache> m_newCache;
    uint8_t *m_data;
};

TestInstance *PipelineFromBlobsTest::createInstance(Context &context) const
{
    return new PipelineFromBlobsTestInstance(context, &m_param);
}

PipelineFromBlobsTestInstance::PipelineFromBlobsTestInstance(Context &context, const TestParam *param)
    : GraphicsTestInstance(context, param)
    , m_data(DE_NULL)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();

    // Create more pipeline caches
    if (m_param->getMode() == TestMode::CACHE)
    {
        size_t dataSize = 0u;

        VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (uintptr_t *)&dataSize, nullptr));

        m_data = new uint8_t[dataSize];
        DE_ASSERT(m_data);
        VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (uintptr_t *)&dataSize, (void *)m_data));

        const VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType             sType;
            nullptr,                                      // const void*                 pNext;
            0u,                                           // VkPipelineCacheCreateFlags  flags;
            dataSize,                                     // uintptr_t                   initialDataSize;
            m_data,                                       // const void*                 pInitialData;
        };
        m_newCache = createPipelineCache(vk, vkDevice, &pipelineCacheCreateInfo);
    }
}

PipelineFromBlobsTestInstance::~PipelineFromBlobsTestInstance(void)
{
    delete[] m_data;
}

void PipelineFromBlobsTestInstance::preparePipelines(void)
{
    if (m_param->getMode() == TestMode::CACHE)
    {
        preparePipelineWrapper(*m_pipeline[PIPELINE_NDX_NO_BLOBS], *m_cache);
        preparePipelineWrapper(*m_pipeline[PIPELINE_NDX_USE_BLOBS], *m_newCache);
    }
    else
        preparePipelinesForBinaries(true);
}

class PipelineFromIncompleteBlobsTest : public GraphicsTest
{
public:
    PipelineFromIncompleteBlobsTest(tcu::TestContext &testContext, const std::string &name, const TestParam *param);
    virtual ~PipelineFromIncompleteBlobsTest(void) = default;
    virtual TestInstance *createInstance(Context &context) const;
};

PipelineFromIncompleteBlobsTest::PipelineFromIncompleteBlobsTest(tcu::TestContext &testContext, const std::string &name,
                                                                 const TestParam *param)
    : GraphicsTest(testContext, name, param)
{
}

class PipelineFromIncompleteBlobsTestInstance : public GraphicsTestInstance
{
public:
    PipelineFromIncompleteBlobsTestInstance(Context &context, const TestParam *param);
    virtual ~PipelineFromIncompleteBlobsTestInstance(void);

protected:
    void preparePipelines(void);

protected:
    Move<VkPipelineCache> m_newCache;
    uint8_t *m_data;
};

TestInstance *PipelineFromIncompleteBlobsTest::createInstance(Context &context) const
{
    return new PipelineFromIncompleteBlobsTestInstance(context, &m_param);
}

PipelineFromIncompleteBlobsTestInstance::PipelineFromIncompleteBlobsTestInstance(Context &context,
                                                                                 const TestParam *param)
    : GraphicsTestInstance(context, param)
    , m_data(DE_NULL)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();

    // Create more pipeline caches
    size_t dataSize = 0u;
    VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (uintptr_t *)&dataSize, nullptr));

    if (dataSize == 0)
        TCU_THROW(NotSupportedError, "Empty pipeline cache - unable to test");

    dataSize--;

    m_data = new uint8_t[dataSize];
    DE_ASSERT(m_data);
    if (vk.getPipelineCacheData(vkDevice, *m_cache, (uintptr_t *)&dataSize, (void *)m_data) != VK_INCOMPLETE)
        TCU_THROW(TestError, "GetPipelineCacheData should return VK_INCOMPLETE state!");

    const VkPipelineCacheCreateInfo pipelineCacheCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType             sType;
        nullptr,                                      // const void*                 pNext;
        0u,                                           // VkPipelineCacheCreateFlags  flags;
        dataSize,                                     // uintptr_t                   initialDataSize;
        m_data,                                       // const void*                 pInitialData;
    };
    m_newCache = createPipelineCache(vk, vkDevice, &pipelineCacheCreateInfo);
}

PipelineFromIncompleteBlobsTestInstance::~PipelineFromIncompleteBlobsTestInstance(void)
{
    delete[] m_data;
}

void PipelineFromIncompleteBlobsTestInstance::preparePipelines(void)
{
    preparePipelineWrapper(*m_pipeline[PIPELINE_NDX_NO_BLOBS], *m_cache);
    preparePipelineWrapper(*m_pipeline[PIPELINE_NDX_USE_BLOBS], *m_newCache);
}

enum class MergeBlobsType
{
    EMPTY = 0,
    FROM_DATA,
    HIT,
    MISS,
    MISS_AND_HIT,
    MERGED,

    LAST = MERGED
};

std::string getMergeBlobsTypeStr(MergeBlobsType type)
{
    switch (type)
    {
    case MergeBlobsType::EMPTY:
        return "empty";
    case MergeBlobsType::FROM_DATA:
        return "from_data";
    case MergeBlobsType::HIT:
        return "hit";
    case MergeBlobsType::MISS_AND_HIT:
        return "misshit";
    case MergeBlobsType::MISS:
        return "miss";
    case MergeBlobsType::MERGED:
        return "merged";
    }
    TCU_FAIL("unhandled merge cache type");
}

std::string getMergeBlobsTypesStr(const std::vector<MergeBlobsType> &types)
{
    std::string ret;
    for (size_t idx = 0; idx < types.size(); ++idx)
    {
        if (ret.size())
            ret += '_';
        ret += getMergeBlobsTypeStr(types[idx]);
    }
    return ret;
}

class MergeBlobsTestParam
{
public:
    MergeBlobsType destBlobsType;
    std::vector<MergeBlobsType> srcBlobTypes;
};

class MergeBlobsTest : public GraphicsTest
{
public:
    MergeBlobsTest(tcu::TestContext &testContext, const std::string &name, const TestParam *param,
                   const MergeBlobsTestParam *mergeBlobsParam)
        : GraphicsTest(testContext, name, param)
        , m_mergeBlobsParam(*mergeBlobsParam)
    {
    }
    virtual ~MergeBlobsTest(void)
    {
    }
    virtual TestInstance *createInstance(Context &context) const;

private:
    const MergeBlobsTestParam m_mergeBlobsParam;
};

class MergeBlobsTestInstance : public GraphicsTestInstance
{
public:
    MergeBlobsTestInstance(Context &context, const TestParam *param, const MergeBlobsTestParam *mergeBlobsParam);

private:
    Move<VkPipelineCache> createPipelineCache(const InstanceInterface &vki, const DeviceInterface &vk,
                                              VkPhysicalDevice physicalDevice, VkDevice device, MergeBlobsType type);

protected:
    void preparePipelines(void);

protected:
    const MergeBlobsTestParam m_mergeBlobsParam;
    Move<VkPipelineCache> m_cacheMerged;
    de::MovePtr<PipelineBinaryWrapper> m_secondBinaries;
};

TestInstance *MergeBlobsTest::createInstance(Context &context) const
{
    return new MergeBlobsTestInstance(context, &m_param, &m_mergeBlobsParam);
}

MergeBlobsTestInstance::MergeBlobsTestInstance(Context &context, const TestParam *param,
                                               const MergeBlobsTestParam *mergeBlobsParam)
    : GraphicsTestInstance(context, param)
    , m_mergeBlobsParam(*mergeBlobsParam)
{
    const InstanceInterface &vki          = context.getInstanceInterface();
    const DeviceInterface &vk             = m_context.getDeviceInterface();
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const VkDevice vkDevice               = m_context.getDevice();

    // this test can't be executed for pipeline binary due to VUID-VkPipelineBinaryInfoKHR-binaryCount-09603
    DE_ASSERT(m_param->getMode() == TestMode::CACHE);

    // Create a merge destination cache
    m_cacheMerged = createPipelineCache(vki, vk, physicalDevice, vkDevice, mergeBlobsParam->destBlobsType);

    // Create more pipeline caches
    std::vector<VkPipelineCache> sourceCaches(mergeBlobsParam->srcBlobTypes.size());
    typedef de::SharedPtr<Move<VkPipelineCache>> PipelineCachePtr;
    std::vector<PipelineCachePtr> sourceCachePtrs(sourceCaches.size());
    {
        for (size_t sourceIdx = 0; sourceIdx < mergeBlobsParam->srcBlobTypes.size(); sourceIdx++)
        {
            // vk::Move is not copyable, so create it on heap and wrap into de::SharedPtr
            PipelineCachePtr pipelineCachePtr(new Move<VkPipelineCache>());
            *pipelineCachePtr =
                createPipelineCache(vki, vk, physicalDevice, vkDevice, mergeBlobsParam->srcBlobTypes[sourceIdx]);

            sourceCachePtrs[sourceIdx] = pipelineCachePtr;
            sourceCaches[sourceIdx]    = **pipelineCachePtr;
        }
    }

    // Merge the caches
    VK_CHECK(
        vk.mergePipelineCaches(vkDevice, *m_cacheMerged, static_cast<uint32_t>(sourceCaches.size()), &sourceCaches[0]));
}

Move<VkPipelineCache> MergeBlobsTestInstance::createPipelineCache(const InstanceInterface &vki,
                                                                  const DeviceInterface &vk,
                                                                  VkPhysicalDevice physicalDevice, VkDevice device,
                                                                  MergeBlobsType type)
{
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType             sType;
        nullptr,                                      // const void*                 pNext;
        0u,                                           // VkPipelineCacheCreateFlags  flags;
        0u,                                           // uintptr_t                   initialDataSize;
        nullptr,                                      // const void*                 pInitialData;
    };

    GraphicsPipelineWrapper localPipeline(vki, vk, physicalDevice, device, m_context.getDeviceExtensions(),
                                          m_param->getPipelineConstructionType());
    GraphicsPipelineWrapper localMissPipeline(vki, vk, physicalDevice, device, m_context.getDeviceExtensions(),
                                              m_param->getPipelineConstructionType());

    switch (type)
    {
    case MergeBlobsType::EMPTY:
    {
        return vk::createPipelineCache(vk, device, &pipelineCacheCreateInfo);
    }
    case MergeBlobsType::FROM_DATA:
    {
        // Create a cache with init data from m_cache
        size_t dataSize = 0u;
        VK_CHECK(vk.getPipelineCacheData(device, *m_cache, (uintptr_t *)&dataSize, nullptr));

        std::vector<uint8_t> data(dataSize);
        VK_CHECK(vk.getPipelineCacheData(device, *m_cache, (uintptr_t *)&dataSize, &data[0]));

        pipelineCacheCreateInfo.initialDataSize = data.size();
        pipelineCacheCreateInfo.pInitialData    = &data[0];
        return vk::createPipelineCache(vk, device, &pipelineCacheCreateInfo);
    }
    case MergeBlobsType::HIT:
    {
        Move<VkPipelineCache> ret = createPipelineCache(vki, vk, physicalDevice, device, MergeBlobsType::EMPTY);

        preparePipelineWrapper(localPipeline, *ret);

        return ret;
    }
    case MergeBlobsType::MISS:
    {
        Move<VkPipelineCache> ret = createPipelineCache(vki, vk, physicalDevice, device, MergeBlobsType::EMPTY);

        preparePipelineWrapper(localMissPipeline, *ret, true);

        return ret;
    }
    case MergeBlobsType::MISS_AND_HIT:
    {
        Move<VkPipelineCache> ret = createPipelineCache(vki, vk, physicalDevice, device, MergeBlobsType::EMPTY);

        preparePipelineWrapper(localPipeline, *ret);
        preparePipelineWrapper(localMissPipeline, *ret, true);

        return ret;
    }
    case MergeBlobsType::MERGED:
    {
        Move<VkPipelineCache> cache1 = createPipelineCache(vki, vk, physicalDevice, device, MergeBlobsType::FROM_DATA);
        Move<VkPipelineCache> cache2 = createPipelineCache(vki, vk, physicalDevice, device, MergeBlobsType::HIT);
        Move<VkPipelineCache> cache3 = createPipelineCache(vki, vk, physicalDevice, device, MergeBlobsType::MISS);

        const VkPipelineCache sourceCaches[] = {*cache1, *cache2, *cache3};

        Move<VkPipelineCache> ret = createPipelineCache(vki, vk, physicalDevice, device, MergeBlobsType::EMPTY);

        // Merge the caches
        VK_CHECK(vk.mergePipelineCaches(device, *ret, DE_LENGTH_OF_ARRAY(sourceCaches), sourceCaches));

        return ret;
    }
    }
    TCU_FAIL("unhandled merge cache type");
}

void MergeBlobsTestInstance::preparePipelines(void)
{
    preparePipelineWrapper(*m_pipeline[PIPELINE_NDX_NO_BLOBS], *m_cache);

    // Create pipeline from merged cache
    preparePipelineWrapper(*m_pipeline[PIPELINE_NDX_USE_BLOBS], *m_cacheMerged);
}

class CacheHeaderTest : public GraphicsTest
{
public:
    CacheHeaderTest(tcu::TestContext &testContext, const std::string &name, const TestParam *param)
        : GraphicsTest(testContext, name, param)
    {
    }
    virtual ~CacheHeaderTest(void)
    {
    }
    virtual TestInstance *createInstance(Context &context) const;
};

class CacheHeaderTestInstance : public GraphicsTestInstance
{
public:
    CacheHeaderTestInstance(Context &context, const TestParam *param);
    virtual ~CacheHeaderTestInstance(void);

protected:
    uint8_t *m_data;

    struct CacheHeader
    {
        uint32_t HeaderLength;
        uint32_t HeaderVersion;
        uint32_t VendorID;
        uint32_t DeviceID;
        uint8_t PipelineCacheUUID[VK_UUID_SIZE];
    } m_header;
};

TestInstance *CacheHeaderTest::createInstance(Context &context) const
{
    return new CacheHeaderTestInstance(context, &m_param);
}

CacheHeaderTestInstance::CacheHeaderTestInstance(Context &context, const TestParam *param)
    : GraphicsTestInstance(context, param)
    , m_data(nullptr)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();

    // Create more pipeline caches
    {
        // Create a cache with init data from m_cache
        size_t dataSize = 0u;
        VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (uintptr_t *)&dataSize, nullptr));

        if (dataSize < sizeof(m_header))
            TCU_THROW(TestError, "Pipeline cache size is smaller than header size");

        m_data = new uint8_t[dataSize];
        DE_ASSERT(m_data);
        VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (uintptr_t *)&dataSize, (void *)m_data));

        deMemcpy(&m_header, m_data, sizeof(m_header));

        if (m_header.HeaderLength - VK_UUID_SIZE != 16)
            TCU_THROW(TestError, "Invalid header size!");

        if (m_header.HeaderVersion != 1)
            TCU_THROW(TestError, "Invalid header version!");

        if (m_header.VendorID != m_context.getDeviceProperties().vendorID)
            TCU_THROW(TestError, "Invalid header vendor ID!");

        if (m_header.DeviceID != m_context.getDeviceProperties().deviceID)
            TCU_THROW(TestError, "Invalid header device ID!");

        if (deMemCmp(&m_header.PipelineCacheUUID, &m_context.getDeviceProperties().pipelineCacheUUID, VK_UUID_SIZE) !=
            0)
            TCU_THROW(TestError, "Invalid header pipeline cache UUID!");
    }
}

CacheHeaderTestInstance::~CacheHeaderTestInstance(void)
{
    delete[] m_data;
}

class InvalidSizeTest : public GraphicsTest
{
public:
    InvalidSizeTest(tcu::TestContext &testContext, const std::string &name, const TestParam *param);
    virtual ~InvalidSizeTest(void)
    {
    }
    virtual TestInstance *createInstance(Context &context) const;
};

InvalidSizeTest::InvalidSizeTest(tcu::TestContext &testContext, const std::string &name, const TestParam *param)
    : GraphicsTest(testContext, name, param)
{
}

class InvalidSizeTestInstance : public GraphicsTestInstance
{
public:
    InvalidSizeTestInstance(Context &context, const TestParam *param);
    virtual ~InvalidSizeTestInstance(void);

protected:
    uint8_t *m_data;
    uint8_t *m_zeroBlock;
};

TestInstance *InvalidSizeTest::createInstance(Context &context) const
{
    return new InvalidSizeTestInstance(context, &m_param);
}

InvalidSizeTestInstance::InvalidSizeTestInstance(Context &context, const TestParam *param)
    : GraphicsTestInstance(context, param)
    , m_data(nullptr)
    , m_zeroBlock(nullptr)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();

    // Create more pipeline caches
    try
    {
        // Create a cache with init data from m_cache
        size_t dataSize      = 0u;
        size_t savedDataSize = 0u;
        VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (uintptr_t *)&dataSize, nullptr));
        savedDataSize = dataSize;

        // If the value of dataSize is less than the maximum size that can be retrieved by the pipeline cache,
        // at most pDataSize bytes will be written to pData, and vkGetPipelineCacheData will return VK_INCOMPLETE.
        dataSize--;

        m_data = new uint8_t[savedDataSize];
        deMemset(m_data, 0, savedDataSize);
        DE_ASSERT(m_data);
        if (vk.getPipelineCacheData(vkDevice, *m_cache, (uintptr_t *)&dataSize, (void *)m_data) != VK_INCOMPLETE)
            TCU_THROW(TestError, "GetPipelineCacheData should return VK_INCOMPLETE state!");

        delete[] m_data;
        m_data = nullptr;

        // If the value of dataSize is less than what is necessary to store the header,
        // nothing will be written to pData and zero will be written to dataSize.
        dataSize = 16 + VK_UUID_SIZE - 1;

        m_data = new uint8_t[savedDataSize];
        deMemset(m_data, 0, savedDataSize);
        DE_ASSERT(m_data);
        if (vk.getPipelineCacheData(vkDevice, *m_cache, (uintptr_t *)&dataSize, (void *)m_data) != VK_INCOMPLETE)
            TCU_THROW(TestError, "GetPipelineCacheData should return VK_INCOMPLETE state!");

        m_zeroBlock = new uint8_t[savedDataSize];
        deMemset(m_zeroBlock, 0, savedDataSize);
        if (deMemCmp(m_data, m_zeroBlock, savedDataSize) != 0 || dataSize != 0)
            TCU_THROW(TestError, "Data needs to be empty and data size should be 0 when invalid size is passed to "
                                 "GetPipelineCacheData!");
    }
    catch (...)
    {
        delete[] m_data;
        delete[] m_zeroBlock;
        throw;
    }
}

InvalidSizeTestInstance::~InvalidSizeTestInstance(void)
{
    delete[] m_data;
    delete[] m_zeroBlock;
}

class ZeroSizeTest : public GraphicsTest
{
public:
    ZeroSizeTest(tcu::TestContext &testContext, const std::string &name, const TestParam *param);
    virtual ~ZeroSizeTest(void)
    {
    }
    virtual TestInstance *createInstance(Context &context) const;
};

ZeroSizeTest::ZeroSizeTest(tcu::TestContext &testContext, const std::string &name, const TestParam *param)
    : GraphicsTest(testContext, name, param)
{
}

class ZeroSizeTestInstance : public GraphicsTestInstance
{
public:
    ZeroSizeTestInstance(Context &context, const TestParam *param);
    virtual ~ZeroSizeTestInstance(void);

protected:
    uint8_t *m_data;
    uint8_t *m_zeroBlock;
};

TestInstance *ZeroSizeTest::createInstance(Context &context) const
{
    return new ZeroSizeTestInstance(context, &m_param);
}

ZeroSizeTestInstance::ZeroSizeTestInstance(Context &context, const TestParam *param)
    : GraphicsTestInstance(context, param)
    , m_data(nullptr)
    , m_zeroBlock(nullptr)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();

    // Create more pipeline caches
    try
    {
        // Create a cache with init data from m_cache
        size_t dataSize = 0u;

        VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (uintptr_t *)&dataSize, nullptr));

        m_data = new uint8_t[dataSize];
        deMemset(m_data, 0, dataSize);
        DE_ASSERT(m_data);

        VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (uintptr_t *)&dataSize, (void *)m_data));

        {
            // Create a cache with initialDataSize = 0 & pInitialData != NULL
            const VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {
                VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType             sType;
                nullptr,                                      // const void*                 pNext;
                0u,                                           // VkPipelineCacheCreateFlags  flags;
                0u,                                           // uintptr_t                   initialDataSize;
                m_data,                                       // const void*                 pInitialData;
            };

            const Unique<VkPipelineCache> pipelineCache(createPipelineCache(vk, vkDevice, &pipelineCacheCreateInfo));
        }
    }
    catch (...)
    {
        delete[] m_data;
        delete[] m_zeroBlock;
        throw;
    }
}

ZeroSizeTestInstance::~ZeroSizeTestInstance(void)
{
    delete[] m_data;
    delete[] m_zeroBlock;
}

class InvalidBlobTest : public GraphicsTest
{
public:
    InvalidBlobTest(tcu::TestContext &testContext, const std::string &name, const TestParam *param);
    virtual ~InvalidBlobTest(void)
    {
    }
    virtual TestInstance *createInstance(Context &context) const;
};

InvalidBlobTest::InvalidBlobTest(tcu::TestContext &testContext, const std::string &name, const TestParam *param)
    : GraphicsTest(testContext, name, param)
{
}

class InvalidBlobTestInstance : public GraphicsTestInstance
{
public:
    InvalidBlobTestInstance(Context &context, const TestParam *param);
    virtual ~InvalidBlobTestInstance(void);

protected:
    uint8_t *m_data;
    uint8_t *m_zeroBlock;
};

TestInstance *InvalidBlobTest::createInstance(Context &context) const
{
    return new InvalidBlobTestInstance(context, &m_param);
}

InvalidBlobTestInstance::InvalidBlobTestInstance(Context &context, const TestParam *param)
    : GraphicsTestInstance(context, param)
    , m_data(nullptr)
    , m_zeroBlock(nullptr)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();

    // Create more pipeline caches
    try
    {
        // Create a cache with init data from m_cache
        size_t dataSize = 0u;

        VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (uintptr_t *)&dataSize, nullptr));

        m_data = new uint8_t[dataSize];
        deMemset(m_data, 0, dataSize);
        DE_ASSERT(m_data);

        VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (uintptr_t *)&dataSize, (void *)m_data));

        const struct
        {
            uint32_t offset;
            std::string name;
        } headerLayout[] = {
            {4u, "pipeline cache header version"}, {8u, "vendor ID"}, {12u, "device ID"}, {16u, "pipeline cache ID"}};

        for (uint32_t i = 0u; i < DE_LENGTH_OF_ARRAY(headerLayout); i++)
        {
            m_context.getTestContext().getLog()
                << tcu::TestLog::Message << "Creating pipeline cache using previously retrieved data with invalid "
                << headerLayout[i].name << tcu::TestLog::EndMessage;

            m_data[headerLayout[i].offset] =
                (uint8_t)(m_data[headerLayout[i].offset] + 13u); // Add arbitrary number to create an invalid value

            const VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {
                VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType             sType;
                nullptr,                                      // const void*                 pNext;
                0u,                                           // VkPipelineCacheCreateFlags  flags;
                dataSize,                                     // uintptr_t                   initialDataSize;
                m_data,                                       // const void*                 pInitialData;
            };

            const Unique<VkPipelineCache> pipelineCache(createPipelineCache(vk, vkDevice, &pipelineCacheCreateInfo));

            m_data[headerLayout[i].offset] =
                (uint8_t)(m_data[headerLayout[i].offset] - 13u); // Return to original value
        }
    }
    catch (...)
    {
        delete[] m_data;
        delete[] m_zeroBlock;
        throw;
    }
}

InvalidBlobTestInstance::~InvalidBlobTestInstance(void)
{
    delete[] m_data;
    delete[] m_zeroBlock;
}

class InternallySynchronizedInstance : public vkt::TestInstance
{
public:
    InternallySynchronizedInstance(Context &context, bool pipelineCreationFeedback)
        : vkt::TestInstance(context)
        , m_pipelineCreationFeedback(pipelineCreationFeedback)
    {
    }
    virtual ~InternallySynchronizedInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;

private:
    bool m_pipelineCreationFeedback;
};

class CreatePipelineThread : public de::Thread
{
public:
    CreatePipelineThread(const DeviceInterface &vkd, VkDevice device, vk::Move<VkShaderModule> &computeShaderModule,
                         vk::Move<VkPipelineLayout> &pipelineLayout, vk::Move<VkPipelineCache> &pipelineCache)
        : de::Thread()
        , m_vkd(vkd)
        , m_device(device)
        , m_computeShaderModule(computeShaderModule)
        , m_pipelineLayout(pipelineLayout)
        , m_pipelineCache(pipelineCache)
    {
    }
    virtual ~CreatePipelineThread(void)
    {
    }

    virtual void run()
    {
        for (uint32_t iterIdx = 0; iterIdx < 1000; iterIdx++)
        {
            const VkPipelineShaderStageCreateInfo stageCreateInfo = {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType                     sType;
                nullptr,                                             // const void*                         pNext;
                0u,                                                  // VkPipelineShaderStageCreateFlags    flags;
                VK_SHADER_STAGE_COMPUTE_BIT,                         // VkShaderStageFlagBits               stage;
                *m_computeShaderModule,                              // VkShaderModule                      module;
                "main",                                              // const char*                         pName;
                nullptr, // const VkSpecializationInfo*         pSpecializationInfo;
            };

            VkComputePipelineCreateInfo pipelineCreateInfo{
                VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType sType;
                nullptr,                                        // const void* pNext;
                0,                                              // VkPipelineCreateFlags flags;
                stageCreateInfo,                                // VkPipelineShaderStageCreateInfo stage;
                *m_pipelineLayout,                              // VkPipelineLayout layout;
                VK_NULL_HANDLE,                                 // VkPipeline basePipelineHandle;
                0u,                                             // int32_t basePipelineIndex;
            };
            auto pipeline = createComputePipeline(m_vkd, m_device, *m_pipelineCache, &pipelineCreateInfo);
        }
    }

private:
    const DeviceInterface &m_vkd;
    VkDevice m_device;
    vk::Move<VkShaderModule> &m_computeShaderModule;
    vk::Move<VkPipelineLayout> &m_pipelineLayout;
    vk::Move<VkPipelineCache> &m_pipelineCache;
};

class MergePipelineCacheThread : public de::Thread
{
public:
    MergePipelineCacheThread(const DeviceInterface &vkd, VkDevice device, vk::Move<VkShaderModule> &computeShaderModule,
                             vk::Move<VkPipelineLayout> &pipelineLayout, vk::Move<VkPipelineCache> &pipelineCache)
        : de::Thread()
        , m_vkd(vkd)
        , m_device(device)
        , m_computeShaderModule(computeShaderModule)
        , m_pipelineLayout(pipelineLayout)
        , m_pipelineCache(pipelineCache)
    {
    }
    virtual ~MergePipelineCacheThread(void)
    {
    }

    virtual void run()
    {
        for (uint32_t iterIdx = 0; iterIdx < 1000; iterIdx++)
        {
            VkPipelineCacheCreateFlags pipelineCacheCreateFlags = 0u;
            if (iterIdx % 2 == 0)
                pipelineCacheCreateFlags |= VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT;

            const VkPipelineCacheCreateInfo pipelineCacheCreateInfo{
                VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType             sType;
                nullptr,                                      // const void*                 pNext;
                pipelineCacheCreateFlags,                     // VkPipelineCacheCreateFlags  flags;
                0u,                                           // uintptr_t                   initialDataSize;
                nullptr,                                      // const void*                 pInitialData;
            };

            auto localPipelineCache = createPipelineCache(m_vkd, m_device, &pipelineCacheCreateInfo);

            const VkPipelineShaderStageCreateInfo stageCreateInfo = {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType                     sType;
                nullptr,                                             // const void*                         pNext;
                0u,                                                  // VkPipelineShaderStageCreateFlags    flags;
                VK_SHADER_STAGE_COMPUTE_BIT,                         // VkShaderStageFlagBits               stage;
                *m_computeShaderModule,                              // VkShaderModule                      module;
                "main",                                              // const char*                         pName;
                nullptr, // const VkSpecializationInfo*         pSpecializationInfo;
            };

            VkComputePipelineCreateInfo pipelineCreateInfo{
                VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType sType;
                nullptr,                                        // const void* pNext;
                0,                                              // VkPipelineCreateFlags flags;
                stageCreateInfo,                                // VkPipelineShaderStageCreateInfo stage;
                *m_pipelineLayout,                              // VkPipelineLayout layout;
                VK_NULL_HANDLE,                                 // VkPipeline basePipelineHandle;
                0u,                                             // int32_t basePipelineIndex;
            };
            auto pipeline = createComputePipeline(m_vkd, m_device, *localPipelineCache, &pipelineCreateInfo);

            m_vkd.mergePipelineCaches(m_device, *m_pipelineCache, 1u, &*localPipelineCache);
        }
    }

private:
    const DeviceInterface &m_vkd;
    VkDevice m_device;
    vk::Move<VkShaderModule> &m_computeShaderModule;
    vk::Move<VkPipelineLayout> &m_pipelineLayout;
    vk::Move<VkPipelineCache> &m_pipelineCache;
};

tcu::TestStatus InternallySynchronizedInstance::iterate(void)
{
    const vk::VkInstance instance = m_context.getInstance();
    const vk::InstanceDriver instanceDriver(m_context.getPlatformInterface(), instance);
    const vk::DeviceInterface &vk = m_context.getDeviceInterface();
    const vk::VkDevice device     = m_context.getDevice();

    DescriptorSetLayoutBuilder descLayoutBuilder;

    for (uint32_t bindingNdx = 0u; bindingNdx < 2u; bindingNdx++)
        descLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);

    auto descriptorSetLayout = descLayoutBuilder.build(vk, device);

    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType                 sType;
        nullptr,                                       // const void*                     pNext;
        0u,                                            // VkPipelineLayoutCreateFlags     flags;
        1u,                                            // uint32_t                        setLayoutCount;
        &descriptorSetLayout.get(),                    // const VkDescriptorSetLayout*    pSetLayouts;
        0u,                                            // uint32_t                        pushConstantRangeCount;
        nullptr,                                       // const VkPushConstantRange*      pPushConstantRanges;
    };

    auto pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

    uint32_t *shaderCode = (uint32_t *)m_context.getBinaryCollection().get("basic_compute").getBinary();
    VkShaderModuleCreateInfo shaderModuleCreateInfo = {
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,                    // VkStructureType             sType;
        nullptr,                                                        // const void*                 pNext;
        0u,                                                             // VkShaderModuleCreateFlags   flags;
        m_context.getBinaryCollection().get("basic_compute").getSize(), // uintptr_t                   codeSize;
        shaderCode,                                                     // const uint32_t*             pCode;
    };
    auto computeShaderModule = createShaderModule(vk, device, &shaderModuleCreateInfo);

    VkPipelineCacheCreateInfo pipelineCacheCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType             sType;
        nullptr,                                      // const void*                 pNext;
        0,                                            // VkPipelineCacheCreateFlags  flags;
        0u,                                           // uintptr_t                   initialDataSize;
        nullptr,                                      // const void*                 pInitialData;
    };

    vk::Move<VkPipelineCache> globalPipelineCache = createPipelineCache(vk, device, &pipelineCacheCreateInfo);

    CreatePipelineThread createPipelineThread(vk, device, computeShaderModule, pipelineLayout, globalPipelineCache);
    MergePipelineCacheThread mergePipelineCacheThread(vk, device, computeShaderModule, pipelineLayout,
                                                      globalPipelineCache);

    createPipelineThread.start();
    mergePipelineCacheThread.start();

    createPipelineThread.join();
    mergePipelineCacheThread.join();

    size_t cacheDataSize;
    vk.getPipelineCacheData(device, *globalPipelineCache, &cacheDataSize, nullptr);
    std::vector<uint8_t> cacheData(cacheDataSize);
    vk.getPipelineCacheData(device, *globalPipelineCache, &cacheDataSize, (void *)cacheData.data());

    globalPipelineCache = {};

    pipelineCacheCreateInfo.initialDataSize = cacheDataSize;
    pipelineCacheCreateInfo.pInitialData    = (void *)cacheData.data();
    globalPipelineCache                     = createPipelineCache(vk, device, &pipelineCacheCreateInfo);

    {
        size_t cacheDataSize2;
        vk.getPipelineCacheData(device, *globalPipelineCache, &cacheDataSize2, nullptr);
        std::vector<uint8_t> cacheData2(cacheDataSize2);
        vk.getPipelineCacheData(device, *globalPipelineCache, &cacheDataSize2, (void *)cacheData2.data());

        if (cacheDataSize != cacheDataSize2)
            return tcu::TestStatus::fail("Pipeline cache data size does not match");

        if (memcmp(cacheData.data(), cacheData2.data(), cacheDataSize) != 0)
            return tcu::TestStatus::fail("Pipeline cache data does not match");
    }

    const VkPipelineShaderStageCreateInfo stageCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType                     sType;
        nullptr,                                             // const void*                         pNext;
        0u,                                                  // VkPipelineShaderStageCreateFlags    flags;
        VK_SHADER_STAGE_COMPUTE_BIT,                         // VkShaderStageFlagBits               stage;
        *computeShaderModule,                                // VkShaderModule                      module;
        "main",                                              // const char*                         pName;
        nullptr,                                             // const VkSpecializationInfo*         pSpecializationInfo;
    };

    VkPipelineCreationFeedbackEXT feedback = {};

    const VkPipelineCreationFeedbackCreateInfo pipelineCreationFeedbackCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void *pNext;
        &feedback, // VkPipelineCreationFeedback *pPipelineCreationFeedback;
        0u,        // uint32_t pipelineStageCreationFeedbackCount;
        nullptr,   // VkPipelineCreationFeedback *pPipelineStageCreationFeedbacks;
    };

    VkComputePipelineCreateInfo pipelineCreateInfo{
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,                             // VkStructureType sType;
        m_pipelineCreationFeedback ? &pipelineCreationFeedbackCreateInfo : nullptr, // const void* pNext;
        0,                                                                          // VkPipelineCreateFlags flags;
        stageCreateInfo, // VkPipelineShaderStageCreateInfo stage;
        *pipelineLayout, // VkPipelineLayout layout;
        VK_NULL_HANDLE,  // VkPipeline basePipelineHandle;
        0u,              // int32_t basePipelineIndex;
    };

    auto pipeline = createComputePipeline(vk, device, *globalPipelineCache, &pipelineCreateInfo);
    createComputePipeline(vk, device, *globalPipelineCache, &pipelineCreateInfo);

    if (m_pipelineCreationFeedback)
    {
        if ((feedback.flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT) == 0)
        {
            return tcu::TestStatus::fail("Pipeline cache missed");
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class InternallySynchronizedTest : public vkt::TestCase
{
public:
    InternallySynchronizedTest(tcu::TestContext &testCtx, const std::string &name, bool pipelineCreationFeedback)
        : vkt::TestCase(testCtx, name)
        , m_pipelineCreationFeedback(pipelineCreationFeedback)
    {
    }
    virtual ~InternallySynchronizedTest(void)
    {
    }

    void checkSupport(vkt::Context &context) const override;
    virtual void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new InternallySynchronizedInstance(context, m_pipelineCreationFeedback);
    }

private:
    bool m_pipelineCreationFeedback;
};

void InternallySynchronizedTest::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_maintenance8");
    if (m_pipelineCreationFeedback)
    {
        context.requireDeviceFunctionality("VK_EXT_pipeline_creation_feedback");
    }
}

void InternallySynchronizedTest::initPrograms(vk::SourceCollections &programCollection) const
{
    programCollection.glslSources.add("basic_compute") << glu::ComputeSource(
        "#version 450 core\n"
        "layout(local_size_x = 1) in;\n"
        "layout(std430, binding = 0) readonly buffer Input0\n"
        "{\n"
        "  vec4 elements[];\n"
        "} input_data0;\n"
        "layout(std430, binding = 1) writeonly buffer Output\n"
        "{\n"
        "  vec4 elements[];\n"
        "} output_data;\n"
        "void main()\n"
        "{\n"
        "  uint ident = gl_GlobalInvocationID.x;\n"
        "  output_data.elements[ident] = input_data0.elements[ident] * input_data0.elements[ident];\n"
        "}");
}

} // namespace

de::MovePtr<tcu::TestCaseGroup> createPipelineBlobTestsInternal(tcu::TestContext &testCtx, TestMode testMode,
                                                                PipelineConstructionType pipelineConstructionType,
                                                                de::MovePtr<tcu::TestCaseGroup> blobTests)
{
    const VkShaderStageFlags vertFragStages     = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    const VkShaderStageFlags vertGeomFragStages = vertFragStages | VK_SHADER_STAGE_GEOMETRY_BIT;
    const VkShaderStageFlags vertTesFragStages =
        vertFragStages | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

    // Graphics Pipeline Tests
    {
        de::MovePtr<tcu::TestCaseGroup> graphicsTests(new tcu::TestCaseGroup(testCtx, "graphics_tests"));

        const TestParam testParams[]{
            {testMode, pipelineConstructionType, vertFragStages, false},
            {testMode, pipelineConstructionType, vertGeomFragStages, false},
            {testMode, pipelineConstructionType, vertTesFragStages, false},
            {testMode, pipelineConstructionType, vertFragStages, false,
             VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT},
            {testMode, pipelineConstructionType, vertGeomFragStages, false,
             VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT},
            {testMode, pipelineConstructionType, vertTesFragStages, false,
             VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT},
        };

        for (const auto &testParam : testParams)
        {
            // cache create flags are tested only for cache cases
            if ((testMode == TestMode::BINARY) && testParam.getPipelineCacheCreateFlags())
                continue;

            graphicsTests->addChild(newTestCase<GraphicsTest>(testCtx, &testParam));
        }

        blobTests->addChild(graphicsTests.release());
    }

    // Graphics Pipeline Tests
    {
        de::MovePtr<tcu::TestCaseGroup> graphicsTests(new tcu::TestCaseGroup(testCtx, "pipeline_from_get_data"));

        const TestParam testParams[]{
            {testMode, pipelineConstructionType, vertFragStages, false},
            {testMode, pipelineConstructionType, vertGeomFragStages, false},
            {testMode, pipelineConstructionType, vertTesFragStages, false},
        };

        for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(testParams); i++)
            graphicsTests->addChild(newTestCase<PipelineFromBlobsTest>(testCtx, &testParams[i]));

        blobTests->addChild(graphicsTests.release());
    }

    // Graphics Pipeline Tests (for pipeline binary there is dedicated.not_enough_space test)
    if (testMode == TestMode::CACHE)
    {
        de::MovePtr<tcu::TestCaseGroup> graphicsTests(
            new tcu::TestCaseGroup(testCtx, "pipeline_from_incomplete_get_data"));

        const TestParam testParams[]{
            {testMode, pipelineConstructionType, vertFragStages, false},
            {testMode, pipelineConstructionType, vertGeomFragStages, false},
            {testMode, pipelineConstructionType, vertTesFragStages, false},
        };

        for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(testParams); i++)
            graphicsTests->addChild(newTestCase<PipelineFromIncompleteBlobsTest>(testCtx, &testParams[i]));

        blobTests->addChild(graphicsTests.release());
    }

    // Compute Pipeline Tests - don't repeat those tests for graphics pipeline library
    if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
    {
        de::MovePtr<tcu::TestCaseGroup> computeTests(new tcu::TestCaseGroup(testCtx, "compute_tests"));

        const TestParam testParams[]{
            {testMode, pipelineConstructionType, VK_SHADER_STAGE_COMPUTE_BIT, false, 0u, false},
            {testMode, pipelineConstructionType, VK_SHADER_STAGE_COMPUTE_BIT, false, 0u, true},
        };

        computeTests->addChild(newTestCase<ComputeTest>(testCtx, &testParams[0]));
        if (testMode == TestMode::BINARY)
            computeTests->addChild(newTestCase<ComputeTest>(testCtx, &testParams[1]));

        blobTests->addChild(computeTests.release());
    }

    // Merge blobs tests
    if (testMode == TestMode::CACHE)
    {
        de::MovePtr<tcu::TestCaseGroup> mergeTests(new tcu::TestCaseGroup(testCtx, "merge"));

        const TestParam testParams[]{
            {testMode, pipelineConstructionType, vertFragStages, true},
            {testMode, pipelineConstructionType, vertGeomFragStages, true},
            {testMode, pipelineConstructionType, vertTesFragStages, true},
        };

        const uint32_t firstTypeIdx = 0u;
        const uint32_t lastTypeIdx  = static_cast<uint32_t>(MergeBlobsType::LAST);

        for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(testParams); i++)
        {
            de::MovePtr<tcu::TestCaseGroup> mergeStagesTests(
                new tcu::TestCaseGroup(testCtx, testParams[i].generateTestName().c_str()));

            for (uint32_t destTypeIdx = firstTypeIdx; destTypeIdx <= lastTypeIdx; destTypeIdx++)
                for (uint32_t srcType1Idx = firstTypeIdx; srcType1Idx <= lastTypeIdx; srcType1Idx++)
                {
                    MergeBlobsTestParam mergeTestParam;
                    mergeTestParam.destBlobsType = MergeBlobsType(destTypeIdx);
                    mergeTestParam.srcBlobTypes.push_back(MergeBlobsType(srcType1Idx));

                    // merge with one cache / binaries
                    {
                        std::string testName = "src_" + getMergeBlobsTypesStr(mergeTestParam.srcBlobTypes) + "_dst_" +
                                               getMergeBlobsTypeStr(mergeTestParam.destBlobsType);
                        mergeStagesTests->addChild(
                            new MergeBlobsTest(testCtx, testName.c_str(), &testParams[i], &mergeTestParam));
                    }

                    // merge with two caches
                    for (uint32_t srcType2Idx = 0u; srcType2Idx <= static_cast<uint32_t>(MergeBlobsType::LAST);
                         srcType2Idx++)
                    {
                        MergeBlobsTestParam cacheTestParamTwoCaches = mergeTestParam;

                        cacheTestParamTwoCaches.srcBlobTypes.push_back(MergeBlobsType(srcType2Idx));

                        std::string testName = "src_" + getMergeBlobsTypesStr(cacheTestParamTwoCaches.srcBlobTypes) +
                                               "_dst_" + getMergeBlobsTypeStr(cacheTestParamTwoCaches.destBlobsType);
                        mergeStagesTests->addChild(
                            new MergeBlobsTest(testCtx, testName.c_str(), &testParams[i], &cacheTestParamTwoCaches));
                    }
                }
            mergeTests->addChild(mergeStagesTests.release());
        }
        blobTests->addChild(mergeTests.release());
    }

    // Misc Tests
    if (testMode == TestMode::CACHE)
    {
        de::MovePtr<tcu::TestCaseGroup> miscTests(new tcu::TestCaseGroup(testCtx, "misc_tests"));

        const TestParam testParam(testMode, pipelineConstructionType, vertFragStages, false);

        miscTests->addChild(new CacheHeaderTest(testCtx, "cache_header_test", &testParam));

        miscTests->addChild(new InvalidSizeTest(testCtx, "invalid_size_test", &testParam));

        miscTests->addChild(new ZeroSizeTest(testCtx, "zero_size_test", &testParam));

        miscTests->addChild(new InvalidBlobTest(testCtx, "invalid_blob_test", &testParam));

        if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
        {
            miscTests->addChild(new InternallySynchronizedTest(testCtx, "internally_synchronized_test", false));

            miscTests->addChild(
                new InternallySynchronizedTest(testCtx, "internally_synchronized_with_feedback_test", true));
        }

        blobTests->addChild(miscTests.release());
    }

    return blobTests;
}

tcu::TestCaseGroup *createCacheTests(tcu::TestContext &testCtx, PipelineConstructionType pipelineConstructionType)
{
    de::MovePtr<tcu::TestCaseGroup> cacheTests(new tcu::TestCaseGroup(testCtx, "cache"));
    return createPipelineBlobTestsInternal(testCtx, TestMode::CACHE, pipelineConstructionType, cacheTests).release();
}

de::MovePtr<tcu::TestCaseGroup> addPipelineBinaryBasicTests(tcu::TestContext &testCtx,
                                                            PipelineConstructionType pipelineConstructionType,
                                                            de::MovePtr<tcu::TestCaseGroup> binaryTests)
{
    return createPipelineBlobTestsInternal(testCtx, TestMode::BINARY, pipelineConstructionType, binaryTests);
}

} // namespace pipeline

} // namespace vkt
