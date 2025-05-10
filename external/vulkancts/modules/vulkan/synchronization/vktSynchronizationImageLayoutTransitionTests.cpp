/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 Google LLC.
 *
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
 * \brief Test no-op image layout transitions in VK_KHR_synchronization2
 *//*--------------------------------------------------------------------*/

#include "deUniquePtr.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vktSynchronizationUtil.hpp"

#include <string>

using namespace vk;

namespace vkt
{
namespace synchronization
{
namespace
{

using de::MovePtr;
using std::vector;
using tcu::TextureLevel;
using tcu::Vec4;

const int WIDTH       = 64;
const int HEIGHT      = 64;
const VkFormat FORMAT = VK_FORMAT_R8G8B8A8_UNORM;

inline VkImageCreateInfo makeImageCreateInfo()
{
    const VkImageUsageFlags usage =
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    const VkImageCreateInfo imageParams = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, //  VkStructureType         sType;
        nullptr,                             //  const void*             pNext;
        0,                                   //  VkImageCreateFlags      flags;
        VK_IMAGE_TYPE_2D,                    //  VkImageType             imageType;
        FORMAT,                              //  VkFormat                format;
        makeExtent3D(WIDTH, HEIGHT, 1u),     //  VkExtent3D              extent;
        1u,                                  //  uint32_t                mipLevels;
        1u,                                  //  uint32_t                arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               //  VkSampleCountFlagBits   samples;
        VK_IMAGE_TILING_OPTIMAL,             //  VkImageTiling           tiling;
        usage,                               //  VkImageUsageFlags       usage;
        VK_SHARING_MODE_EXCLUSIVE,           //  VkSharingMode           sharingMode;
        0u,                                  //  uint32_t                queueFamilyIndexCount;
        nullptr,                             //  const uint32_t*         pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           //  VkImageLayout           initialLayout;
    };

    return imageParams;
}

Move<VkBuffer> makeVertexBuffer(const DeviceInterface &vk, const VkDevice device, const uint32_t queueFamilyIndex)
{
    const VkBufferCreateInfo vertexBufferParams = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType      sType;
        nullptr,                              // const void*          pNext;
        0u,                                   // VkBufferCreateFlags  flags;
        1024u,                                // VkDeviceSize         size;
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,    // VkBufferUsageFlags   usage;
        VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode        sharingMode;
        1u,                                   // uint32_t             queueFamilyIndexCount;
        &queueFamilyIndex                     // const uint32_t*      pQueueFamilyIndices;
    };

    Move<VkBuffer> vertexBuffer = createBuffer(vk, device, &vertexBufferParams);
    ;
    return vertexBuffer;
}

class SynchronizationImageLayoutTransitionTestInstance : public TestInstance
{
public:
    SynchronizationImageLayoutTransitionTestInstance(Context &context);
    tcu::TestStatus iterate(void);
};

SynchronizationImageLayoutTransitionTestInstance::SynchronizationImageLayoutTransitionTestInstance(Context &context)
    : TestInstance(context)
{
}

template <typename T>
inline size_t sizeInBytes(const vector<T> &vec)
{
    return vec.size() * sizeof(vec[0]);
}

// Draw a quad covering the whole framebuffer
vector<Vec4> genFullQuadVertices(void)
{
    vector<Vec4> vertices;
    vertices.push_back(Vec4(-1.0f, -1.0f, 0.0f, 1.0f));
    vertices.push_back(Vec4(1.0f, -1.0f, 0.0f, 1.0f));
    vertices.push_back(Vec4(-1.0f, 1.0f, 0.0f, 1.0f));
    vertices.push_back(Vec4(1.0f, -1.0f, 0.0f, 1.0f));
    vertices.push_back(Vec4(1.0f, 1.0f, 0.0f, 1.0f));
    vertices.push_back(Vec4(-1.0f, 1.0f, 0.0f, 1.0f));

    return vertices;
}

struct Vertex
{
    Vertex(Vec4 vertices_) : vertices(vertices_)
    {
    }
    Vec4 vertices;

    static VkVertexInputBindingDescription getBindingDescription(void);
    static vector<VkVertexInputAttributeDescription> getAttributeDescriptions(void);
};

VkVertexInputBindingDescription Vertex::getBindingDescription(void)
{
    static const VkVertexInputBindingDescription desc = {
        0u,                                    // uint32_t             binding;
        static_cast<uint32_t>(sizeof(Vertex)), // uint32_t             stride;
        VK_VERTEX_INPUT_RATE_VERTEX,           // VkVertexInputRate    inputRate;
    };

    return desc;
}

vector<VkVertexInputAttributeDescription> Vertex::getAttributeDescriptions(void)
{
    static const vector<VkVertexInputAttributeDescription> desc = {
        {
            0u,                                                // uint32_t    location;
            0u,                                                // uint32_t    binding;
            VK_FORMAT_R32G32B32A32_SFLOAT,                     // VkFormat    format;
            static_cast<uint32_t>(offsetof(Vertex, vertices)), // uint32_t    offset;
        },
    };

    return desc;
}

tcu::TestStatus SynchronizationImageLayoutTransitionTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    Allocator &allocator            = m_context.getDefaultAllocator();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    const VkDeviceSize bufferSize   = 16 * 1024;

    const VkExtent2D renderSize = {uint32_t(WIDTH), uint32_t(HEIGHT)};
    const VkRect2D renderArea   = makeRect2D(makeExtent3D(WIDTH, HEIGHT, 1u));
    const vector<VkRect2D> scissors(1u, renderArea);
    const vector<VkViewport> viewports(1u, makeViewport(makeExtent3D(WIDTH, HEIGHT, 1u)));

    const vector<Vec4> vertices = genFullQuadVertices();
    Move<VkBuffer> vertexBuffer = makeVertexBuffer(vk, device, queueFamilyIndex);
    MovePtr<Allocation> vertexBufferAlloc =
        bindBuffer(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible);
    const VkDeviceSize vertexBufferOffset = 0ull;

    deMemcpy(vertexBufferAlloc->getHostPtr(), &vertices[0], sizeInBytes(vertices));
    flushAlloc(vk, device, *vertexBufferAlloc);

    const VkImageCreateInfo targetCreateInfo = makeImageCreateInfo();
    const VkImageSubresourceRange targetSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1, 0, 1);
    const ImageWithMemory targetImage(vk, device, m_context.getDefaultAllocator(), targetCreateInfo,
                                      MemoryRequirement::Any);
    Move<VkImageView> targetImageView =
        makeImageView(vk, device, *targetImage, VK_IMAGE_VIEW_TYPE_2D, FORMAT, targetSubresourceRange);

    const Move<VkCommandPool> cmdPool =
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
    const Move<VkCommandBuffer> cmdBuffer =
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    Move<VkRenderPass> renderPass = makeRenderPass(vk, device, FORMAT, VK_FORMAT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_LOAD);
    Move<VkFramebuffer> framebuffer =
        makeFramebuffer(vk, device, *renderPass, targetImageView.get(), renderSize.width, renderSize.height);

    const Move<VkShaderModule> vertexModule =
        createShaderModule(vk, device, m_context.getBinaryCollection().get("vert1"), 0u);
    const Move<VkShaderModule> fragmentModule =
        createShaderModule(vk, device, m_context.getBinaryCollection().get("frag1"), 0u);

    const Move<VkPipelineLayout> pipelineLayout = makePipelineLayout(vk, device, VK_NULL_HANDLE);

    const VkPipelineColorBlendAttachmentState clrBlendAttachmentState = {
        VK_TRUE,                             // VkBool32                 blendEnable;
        VK_BLEND_FACTOR_SRC_ALPHA,           // VkBlendFactor            srcColorBlendFactor;
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, // VkBlendFactor            dstColorBlendFactor;
        VK_BLEND_OP_ADD,                     // VkBlendOp                colorBlendOp;
        VK_BLEND_FACTOR_ONE,                 // VkBlendFactor            srcAlphaBlendFactor;
        VK_BLEND_FACTOR_ONE,                 // VkBlendFactor            dstAlphaBlendFactor;
        VK_BLEND_OP_MAX,                     // VkBlendOp                alphaBlendOp;
        (VkColorComponentFlags)0xF           // VkColorComponentFlags    colorWriteMask;
    };

    const VkPipelineColorBlendStateCreateInfo clrBlendStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType                               sType;
        nullptr,                                  // const void*                                   pNext;
        (VkPipelineColorBlendStateCreateFlags)0u, // VkPipelineColorBlendStateCreateFlags          flags;
        VK_FALSE,                                 // VkBool32                                      logicOpEnable;
        VK_LOGIC_OP_CLEAR,                        // VkLogicOp                                     logicOp;
        1u,                                       // uint32_t                                      attachmentCount;
        &clrBlendAttachmentState,                 // const VkPipelineColorBlendAttachmentState*    pAttachments;
        {1.0f, 1.0f, 1.0f, 1.0f}                  // float                                         blendConstants[4];
    };

    const VkVertexInputBindingDescription vtxBindingDescription = Vertex::getBindingDescription();
    const auto vtxAttrDescriptions                              = Vertex::getAttributeDescriptions();

    const VkPipelineVertexInputStateCreateInfo vtxInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                             sType;
        nullptr,                                                   // const void*                                 pNext;
        (VkPipelineVertexInputStateCreateFlags)0,                  // VkPipelineVertexInputStateCreateFlags       flags;
        1u,                     // uint32_t                                    vertexBindingDescriptionCount;
        &vtxBindingDescription, // const VkVertexInputBindingDescription*      pVertexBindingDescriptions
        static_cast<uint32_t>(
            vtxAttrDescriptions.size()), // uint32_t                                    vertexAttributeDescriptionCount
        vtxAttrDescriptions.data(),      // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
    };

    const Move<VkPipeline> graphicsPipeline = makeGraphicsPipeline(
        vk, device, pipelineLayout.get(), vertexModule.get(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
        fragmentModule.get(), renderPass.get(), viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, 0u,
        &vtxInputStateCreateInfo, nullptr, nullptr, nullptr, &clrBlendStateCreateInfo);

    const VkBufferCreateInfo resultBufferCreateInfo =
        makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    Move<VkBuffer> resultBuffer = createBuffer(vk, device, &resultBufferCreateInfo);
    MovePtr<Allocation> resultBufferMemory =
        allocator.allocate(getBufferMemoryRequirements(vk, device, *resultBuffer), MemoryRequirement::HostVisible);
    MovePtr<TextureLevel> resultImage(new TextureLevel(mapVkFormat(FORMAT), renderSize.width, renderSize.height, 1));

    VK_CHECK(
        vk.bindBufferMemory(device, *resultBuffer, resultBufferMemory->getMemory(), resultBufferMemory->getOffset()));

    const Vec4 clearColor(0.0f, 0.0f, 0.0f, 0.0f);

    clearColorImage(vk, device, m_context.getUniversalQueue(), m_context.getUniversalQueueFamilyIndex(),
                    targetImage.get(), clearColor, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1);

    beginCommandBuffer(vk, *cmdBuffer);

    vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);

    beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT), 0, nullptr);
    vk.cmdDraw(*cmdBuffer, static_cast<uint32_t>(vertices.size()), 1u, 0u, 0u);
    endRenderPass(vk, *cmdBuffer);

    // Define an execution dependency and skip the layout transition. This is allowed when oldLayout
    // and newLayout are both UNDEFINED. The test will fail if the driver discards the contents of
    // the image.
    const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // VkPipelineStageFlags2KHR    srcStageMask
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // VkAccessFlags2KHR           srcAccessMask
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // VkPipelineStageFlags2KHR    dstStageMask
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,           // VkAccessFlags2KHR           dstAccessMask
        VK_IMAGE_LAYOUT_UNDEFINED,                     // VkImageLayout               oldLayout
        VK_IMAGE_LAYOUT_UNDEFINED,                     // VkImageLayout               newLayout
        targetImage.get(),                             // VkImage                     image
        targetSubresourceRange                         // VkImageSubresourceRange     subresourceRange
    );
    VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(nullptr, nullptr, &imageMemoryBarrier2);
#ifndef CTS_USES_VULKANSC
    vk.cmdPipelineBarrier2(cmdBuffer.get(), &dependencyInfo);
#else
    vk.cmdPipelineBarrier2KHR(cmdBuffer.get(), &dependencyInfo);
#endif // CTS_USES_VULKANSC

    beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT), 0, nullptr);
    vk.cmdDraw(*cmdBuffer, static_cast<uint32_t>(vertices.size()), 1u, 0u, 0u);
    endRenderPass(vk, *cmdBuffer);

    // Read the result buffer data
    copyImageToBuffer(vk, *cmdBuffer, *targetImage, *resultBuffer, tcu::IVec2(WIDTH, HEIGHT),
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    invalidateAlloc(vk, device, *resultBufferMemory);

    tcu::clear(resultImage->getAccess(), tcu::IVec4(0));
    tcu::copy(resultImage->getAccess(),
              tcu::ConstPixelBufferAccess(resultImage.get()->getFormat(), resultImage.get()->getSize(),
                                          resultBufferMemory->getHostPtr()));

    TextureLevel textureLevel(mapVkFormat(FORMAT), WIDTH, HEIGHT, 1);
    const tcu::PixelBufferAccess expectedImage = textureLevel.getAccess();

    const float alpha = 0.4f;
    const float red   = (2.0f - alpha) * alpha;
    const float green = red;
    const float blue  = 0;
    const Vec4 color  = Vec4(red, green, blue, alpha);

    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            expectedImage.setPixel(color, x, y, 0);

    bool ok = tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Image comparison", "", expectedImage,
                                         resultImage->getAccess(), tcu::Vec4(0.01f), tcu::COMPARE_LOG_RESULT);
    return ok ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail("Fail");
}

class SynchronizationImageLayoutTransitionTest : public TestCase
{
public:
    SynchronizationImageLayoutTransitionTest(tcu::TestContext &testCtx, const std::string &name);

    virtual void checkSupport(Context &context) const;
    void initPrograms(SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &context) const;
};

SynchronizationImageLayoutTransitionTest::SynchronizationImageLayoutTransitionTest(tcu::TestContext &testCtx,
                                                                                   const std::string &name)
    : TestCase(testCtx, name)
{
}

void SynchronizationImageLayoutTransitionTest::initPrograms(SourceCollections &programCollection) const
{
    std::ostringstream vertexSrc;
    vertexSrc << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
              << "layout(location = 0) in vec4 a_position;\n"
              << "void main (void) {\n"
              << "    gl_Position = a_position;\n"
              << "}\n";

    std::ostringstream fragmentSrc;
    fragmentSrc << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                << "layout(location = 0) out vec4 outColor;\n"
                << "void main() {\n"
                << "    outColor = vec4(1., 1., 0., .4);\n"
                << "}\n";

    programCollection.glslSources.add("vert1") << glu::VertexSource(vertexSrc.str());
    programCollection.glslSources.add("frag1") << glu::FragmentSource(fragmentSrc.str());
}

void SynchronizationImageLayoutTransitionTest::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_synchronization2");
}

TestInstance *SynchronizationImageLayoutTransitionTest::createInstance(Context &context) const
{
    return new SynchronizationImageLayoutTransitionTestInstance(context);
}

struct ComputeLayoutTransitionParams
{
    bool storageUsage;

    tcu::IVec3 getImageExtent() const
    {
        return tcu::IVec3(8, 8, 1);
    }

    VkSampleCountFlagBits getImageSampleCount() const
    {
        return VK_SAMPLE_COUNT_4_BIT;
    }

    VkFormat getImageFormat() const
    {
        return VK_FORMAT_R8G8B8A8_UNORM;
    }

    VkImageType getImageType() const
    {
        return VK_IMAGE_TYPE_2D;
    }

    VkImageTiling getImageTiling() const
    {
        return VK_IMAGE_TILING_OPTIMAL;
    }

    VkImageUsageFlags getImageUsage() const
    {
        const auto readUsage  = (storageUsage ? VK_IMAGE_USAGE_STORAGE_BIT : VK_IMAGE_USAGE_SAMPLED_BIT);
        const auto imageUsage = static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | readUsage |
                                                               VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        return imageUsage;
    }
};

class ComputeLayoutTransitionInstance : public vkt::TestInstance
{
public:
    ComputeLayoutTransitionInstance(Context &context, const ComputeLayoutTransitionParams &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }
    virtual ~ComputeLayoutTransitionInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const ComputeLayoutTransitionParams m_params;
};

class ComputeLayoutTransitionCase : public vkt::TestCase
{
public:
    ComputeLayoutTransitionCase(tcu::TestContext &testCtx, const std::string &name,
                                const ComputeLayoutTransitionParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~ComputeLayoutTransitionCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new ComputeLayoutTransitionInstance(context, m_params);
    }

protected:
    const ComputeLayoutTransitionParams m_params;
};

void ComputeLayoutTransitionCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_synchronization2");
    context.getComputeQueue(); // Throws if not available.

    if (m_params.storageUsage)
    {
        const auto ctx         = context.getContextCommonData();
        const auto format      = m_params.getImageFormat();
        const auto imageType   = m_params.getImageType();
        const auto imageTiling = m_params.getImageTiling();
        const auto imageUsage  = m_params.getImageUsage();

        VkImageFormatProperties properties;
        const auto result = ctx.vki.getPhysicalDeviceImageFormatProperties(ctx.physicalDevice, format, imageType,
                                                                           imageTiling, imageUsage, 0u, &properties);

        if (result != VK_SUCCESS)
        {
            if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
                TCU_THROW(NotSupportedError, "Format not supported");
            else
                TCU_FAIL("Unexpected result in vkGetPhysicalDeviceImageFormatProperties");
        }

        if ((properties.sampleCounts & m_params.getImageSampleCount()) == 0u)
            TCU_THROW(NotSupportedError, "Sample count not supported");
    }
}

void ComputeLayoutTransitionCase::initPrograms(vk::SourceCollections &dst) const
{
    const auto extent      = m_params.getImageExtent();
    const auto sampleCount = m_params.getImageSampleCount();

    DE_ASSERT(extent.z() == 1);

    std::ostringstream comp;
    comp << "#version 460\n"
         << "layout (local_size_x=" << extent.x() << ", local_size_y=" << extent.y() << ", local_size_z=" << sampleCount
         << ") in;\n"
         << (m_params.storageUsage ? "layout (set=0, binding=0, rgba8) uniform image2DMS inImage;\n" :
                                     "layout (set=0, binding=0) uniform sampler2DMS inImage;\n")
         << "layout (set=0, binding=1, std430) buffer OutBlock { vec4 color[]; } outBuffer;\n"
         << "void main (void) {\n"
         << "    const uint width = gl_WorkGroupSize.x;\n"
         << "    const uint height = gl_WorkGroupSize.y;\n"
         << "    const uint samples = gl_WorkGroupSize.z;\n"
         << "    const uint x = gl_LocalInvocationID.x;\n"
         << "    const uint y = gl_LocalInvocationID.y;\n"
         << "    const uint s = gl_LocalInvocationID.z;\n"
         << "    const uint idx = samples * width * y + samples * x + s;\n"
         << (m_params.storageUsage ? "    const vec4 color = imageLoad(inImage, ivec2(x, y), int(s));\n" :
                                     "    const vec4 color = texelFetch(inImage, ivec2(x, y), int(s));\n")
         << "    outBuffer.color[idx] = color;\n"
         << "}\n";
    dst.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

tcu::TestStatus ComputeLayoutTransitionInstance::iterate()
{
    const auto ctx           = m_context.getContextCommonData();
    const auto imageExtent   = m_params.getImageExtent();
    const auto imageExtentVk = makeExtent3D(imageExtent);
    const auto imageFormat   = m_params.getImageFormat();
    const auto imageUsage    = m_params.getImageUsage();
    const auto imageType     = m_params.getImageType();
    const auto imageTiling   = m_params.getImageTiling();
    const auto imageViewType = VK_IMAGE_VIEW_TYPE_2D;
    const auto imageDescriptorType =
        (m_params.storageUsage ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    const auto imageReadLayout =
        (m_params.storageUsage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    const auto srr = makeDefaultImageSubresourceRange();
    const tcu::Vec4 clearColor(0.0f, 0.0f, 1.0f, 1.0f);
    const auto sampleCount  = m_params.getImageSampleCount();
    const auto bufferFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
    const tcu::IVec3 bufferExtentFactor(sampleCount, 1, 1);
    const auto bufferExtent         = imageExtent * bufferExtentFactor;
    const auto bufferDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0u,
        imageType,
        imageFormat,
        imageExtentVk,
        1u,
        1u,
        sampleCount,
        imageTiling,
        imageUsage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    ImageWithMemory image(ctx.vkd, ctx.device, ctx.allocator, imageCreateInfo, MemoryRequirement::Any);
    const auto imageView = makeImageView(ctx.vkd, ctx.device, *image, imageViewType, imageFormat, srr);

    const VkSamplerCreateInfo samplerCreateInfo = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        nullptr,
        0u,
        VK_FILTER_NEAREST,
        VK_FILTER_NEAREST,
        VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        0.0f,
        VK_FALSE,
        1.0f,
        VK_FALSE,
        VK_COMPARE_OP_NEVER,
        0.0f,
        0.0f,
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        VK_FALSE,
    };
    const auto sampler = createSampler(ctx.vkd, ctx.device, &samplerCreateInfo);

    const auto tcuBufferFormat = mapVkFormat(bufferFormat);
    const auto pixelSize       = tcu::getPixelSize(tcuBufferFormat);
    const auto bufferSize =
        static_cast<VkDeviceSize>(bufferExtent.x() * bufferExtent.y() * bufferExtent.z() * pixelSize);
    const auto bufferUsage      = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    const auto bufferCreateInfo = makeBufferCreateInfo(bufferSize, bufferUsage);

    BufferWithMemory buffer(ctx.vkd, ctx.device, ctx.allocator, bufferCreateInfo, MemoryRequirement::HostVisible);

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(imageDescriptorType);
    poolBuilder.addType(bufferDescriptorType);
    const auto descPool = poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(imageDescriptorType, VK_SHADER_STAGE_COMPUTE_BIT);
    setLayoutBuilder.addSingleBinding(bufferDescriptorType, VK_SHADER_STAGE_COMPUTE_BIT);
    const auto setLayout      = setLayoutBuilder.build(ctx.vkd, ctx.device);
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout);
    const auto descriptorSet  = makeDescriptorSet(ctx.vkd, ctx.device, *descPool, *setLayout);

    const auto binding = DescriptorSetUpdateBuilder::Location::binding;
    DescriptorSetUpdateBuilder updateBuilder;
    const auto descriptorSampler = (m_params.storageUsage ? VK_NULL_HANDLE : *sampler);
    const auto imageDescInfo     = makeDescriptorImageInfo(descriptorSampler, *imageView, imageReadLayout);
    const auto bufferDescInfo    = makeDescriptorBufferInfo(*buffer, 0ull, VK_WHOLE_SIZE);
    updateBuilder.writeSingle(*descriptorSet, binding(0u), imageDescriptorType, &imageDescInfo);
    updateBuilder.writeSingle(*descriptorSet, binding(1u), bufferDescriptorType, &bufferDescInfo);
    updateBuilder.update(ctx.vkd, ctx.device);

    const auto &binaries  = m_context.getBinaryCollection();
    const auto compShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));
    const auto pipeline   = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compShader);

    const auto srcStageMask  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    const auto srcAccessMask = VK_ACCESS_2_NONE_KHR;
    const auto dstStagesMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    const auto dstAccessMask = VK_ACCESS_2_NONE_KHR;

    const auto recordImageBarrier = [&ctx](VkCommandBuffer cmdBuffer, const VkImageMemoryBarrier2 &barrier)
    {
        const VkDependencyInfo depInfo = {
            VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            nullptr,
            VK_DEPENDENCY_BY_REGION_BIT,
            0u,
            nullptr,
            0u,
            nullptr,
            1u,
            &barrier,
        };
#ifndef CTS_USES_VULKANSC
        ctx.vkd.cmdPipelineBarrier2(cmdBuffer, &depInfo);
#else
        ctx.vkd.cmdPipelineBarrier2KHR(cmdBuffer, &depInfo);
#endif // CTS_USES_VULKANSC
    };

    const auto recordMemBarrier = [&ctx](VkCommandBuffer cmdBuffer, const VkMemoryBarrier2 &barrier)
    {
        const VkDependencyInfo depInfo = {
            VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            nullptr,
            VK_DEPENDENCY_BY_REGION_BIT,
            1u,
            &barrier,
            0u,
            nullptr,
            0u,
            nullptr,
        };
#ifndef CTS_USES_VULKANSC
        ctx.vkd.cmdPipelineBarrier2(cmdBuffer, &depInfo);
#else
        ctx.vkd.cmdPipelineBarrier2KHR(cmdBuffer, &depInfo);
#endif // CTS_USES_VULKANSC
    };

    // First let's change the layout to color attachment optimal on the universal queue.
    {
        CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
        const auto cmdBuffer = *cmd.cmdBuffer;
        const auto barrier   = makeImageMemoryBarrier2(srcStageMask, srcAccessMask, dstStagesMask, dstAccessMask,
                                                       VK_IMAGE_LAYOUT_UNDEFINED,
                                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, image.get(), srr);

        beginCommandBuffer(ctx.vkd, cmdBuffer);
        recordImageBarrier(cmdBuffer, barrier);
        endCommandBuffer(ctx.vkd, cmdBuffer);
        submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);
    }

    // Second: let's move it to the transfer dst layout on the compute queue.
    {
        CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, m_context.getComputeQueueFamilyIndex());
        const auto cmdBuffer = *cmd.cmdBuffer;
        const auto barrier   = makeImageMemoryBarrier2(srcStageMask, srcAccessMask, dstStagesMask, dstAccessMask,
                                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image.get(), srr);

        beginCommandBuffer(ctx.vkd, cmdBuffer);
        recordImageBarrier(cmdBuffer, barrier);
        endCommandBuffer(ctx.vkd, cmdBuffer);
        submitCommandsAndWait(ctx.vkd, ctx.device, m_context.getComputeQueue(), cmdBuffer);
    }

    // Finally: clean it on the universal queue again and copy it out.
    {
        CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
        const auto cmdBuffer    = *cmd.cmdBuffer;
        const auto clearColorVk = makeClearValueColorVec4(clearColor);

        beginCommandBuffer(ctx.vkd, cmdBuffer);
        ctx.vkd.cmdClearColorImage(cmdBuffer, image.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColorVk.color,
                                   1u, &srr);
        {
            // After the clear, copy the image out.
            const auto compStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            const auto barrier   = makeImageMemoryBarrier2(
                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, compStage,
                VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageReadLayout, image.get(), srr);
            recordImageBarrier(cmdBuffer, barrier);

            ctx.vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                                          &descriptorSet.get(), 0u, nullptr);
            ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
            ctx.vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);

            const auto barrier2 = makeMemoryBarrier2(compStage, VK_ACCESS_SHADER_WRITE_BIT,
                                                     VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
            recordMemBarrier(cmdBuffer, barrier2);
        }
        endCommandBuffer(ctx.vkd, cmdBuffer);
        submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);
    }

    // Verify it matches the clear color.
    {
        auto &bufferAlloc = buffer.getAllocation();
        invalidateAlloc(ctx.vkd, ctx.device, bufferAlloc);

        auto &log = m_context.getTestContext().getLog();
        const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);

        tcu::TextureLevel refLevel(tcuBufferFormat, bufferExtent.x(), bufferExtent.y(), bufferExtent.z());
        tcu::PixelBufferAccess reference = refLevel.getAccess();
        tcu::clear(reference, clearColor);

        tcu::ConstPixelBufferAccess result(tcuBufferFormat, bufferExtent, bufferAlloc.getHostPtr());
        if (!tcu::floatThresholdCompare(log, "Result", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
            TCU_FAIL("Unexpected results in color buffer; check log for details --");
    }

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createImageLayoutTransitionTests(tcu::TestContext &testCtx)
{
    // No-op image layout transition tests
    de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "layout_transition"));
    testGroup->addChild(new SynchronizationImageLayoutTransitionTest(testCtx, "no_op"));

    for (const bool storageUsage : {false, true})
    {
        const auto testName = std::string("compute_transition") + (storageUsage ? "_storage" : "");
        const ComputeLayoutTransitionParams params{storageUsage};
        testGroup->addChild(new ComputeLayoutTransitionCase(testCtx, testName, params));
    }

    return testGroup.release();
}

} // namespace synchronization
} // namespace vkt
