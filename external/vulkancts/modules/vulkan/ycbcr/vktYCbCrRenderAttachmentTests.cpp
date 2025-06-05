/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 Calder Young
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
 * \brief YCbCr multi-planar format rendering attachment tests
 *//*--------------------------------------------------------------------*/

#include "tcuVectorUtil.hpp"
#include "vkDefs.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktYCbCrRenderAttachmentTests.hpp"
#include "vktDrawUtil.hpp"
#include "vktYCbCrUtil.hpp"

#include <string>
#include <vector>

using namespace vk;
using namespace vkt::drawutil;

namespace vkt
{
namespace ycbcr
{
namespace
{

class RenderAttachmentTestInstance : public TestInstance
{
public:
    RenderAttachmentTestInstance(Context &context, VkFormat format, VkImageAspectFlags aspect, bool disjoint);
    ~RenderAttachmentTestInstance() = default;

protected:
    tcu::TestStatus iterate(void);
    tcu::TestStatus verify(const MultiPlaneImageData &imageData, uint32_t planeIdx);
    bool verifyPlane(const tcu::ConstPixelBufferAccess &imageData, uint32_t planeIdx, bool isTarget);
    Move<VkImage> createImage(tcu::UVec2 size);
    Move<VkImageView> createImageView(VkImage image, VkFormat aspectFormat);
    Move<VkRenderPass> createRenderPass(VkFormat aspectFormat);
    Move<VkFramebuffer> createFramebuffer(VkRenderPass renderPass, VkImageView imageView, tcu::UVec2 size);
    Move<VkPipelineLayout> createRenderPipelineLayout();
    Move<VkPipeline> createRenderPipeline(VkRenderPass renderPass, VkPipelineLayout layout, tcu::UVec2 size);

private:
    const VkFormat m_format;
    const VkImageAspectFlags m_aspect;
    const bool m_disjoint;
    const DeviceInterface &m_vkd;
    const VkDevice m_device;

};

RenderAttachmentTestInstance::RenderAttachmentTestInstance(Context &context, VkFormat format, VkImageAspectFlags aspect,
                                                           bool disjoint)
    : TestInstance(context)
    , m_format(format)
    , m_aspect(aspect)
    , m_disjoint(disjoint)
    , m_vkd(context.getDeviceInterface())
    , m_device(context.getDevice())
{
}

Move<VkImage> RenderAttachmentTestInstance::createImage(tcu::UVec2 size)
{
    VkImageUsageFlags usageFlags =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VkImageCreateFlags createFlags = VK_IMAGE_CREATE_EXTENDED_USAGE_BIT | VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    if (m_disjoint)
        createFlags |= VK_IMAGE_CREATE_DISJOINT_BIT;
    const VkImageCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        createFlags,
        VK_IMAGE_TYPE_2D,
        m_format,
        makeExtent3D(size.x(), size.y(), 1u),
        1u, // mipLevels
        1u, // arrayLayers
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        usageFlags,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    return ::createImage(m_vkd, m_device, &createInfo);
}

Move<VkImageView> RenderAttachmentTestInstance::createImageView(VkImage image, VkFormat aspectFormat)
{
    const VkImageViewCreateInfo viewInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        nullptr,
        (VkImageViewCreateFlags)0,
        image,
        VK_IMAGE_VIEW_TYPE_2D,
        aspectFormat,
        {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        {m_aspect, 0u, 1u, 0u, 1u},
    };

    return ::createImageView(m_vkd, m_device, &viewInfo);
}

Move<VkRenderPass> RenderAttachmentTestInstance::createRenderPass(VkFormat aspectFormat)
{
    const VkAttachmentReference dstAttachmentRef = {
        0u,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    const VkAttachmentDescription dstAttachment = {
        (VkAttachmentDescriptionFlags)0u,
        aspectFormat,
        VK_SAMPLE_COUNT_1_BIT,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    const VkSubpassDescription subpass = {
        (VkSubpassDescriptionFlags)0,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        0u,
        nullptr,
        1u,
        &dstAttachmentRef,
        nullptr,
        nullptr,
        0u,
        nullptr
    };

    const VkRenderPassCreateInfo renderPassInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        nullptr,
        (VkRenderPassCreateFlags)0u,
        1u,
        &dstAttachment,
        1u,
        &subpass,
        0u,
        nullptr
    };

    return ::createRenderPass(m_vkd, m_device, &renderPassInfo);
}

Move<VkFramebuffer> RenderAttachmentTestInstance::createFramebuffer(VkRenderPass renderPass, VkImageView imageView,
                                                                    tcu::UVec2 size)
{
    const VkFramebufferCreateInfo pCreateInfo = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        nullptr,
        0u,
        renderPass,
        1,
        &imageView,
        size.x(),
        size.y(),
        1
    };

    return ::createFramebuffer(m_vkd, m_device, &pCreateInfo);
}

Move<VkPipelineLayout> RenderAttachmentTestInstance::createRenderPipelineLayout()
{
    const VkPipelineLayoutCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        nullptr,
        (vk::VkPipelineLayoutCreateFlags)0,
        0u,
        nullptr,
        0u,
        nullptr
    };

    return ::createPipelineLayout(m_vkd, m_device, &createInfo);
}

Move<VkPipeline> RenderAttachmentTestInstance::createRenderPipeline(VkRenderPass renderPass, VkPipelineLayout layout,
                                                                    tcu::UVec2 size)
{
    const Unique<VkShaderModule> vertexShaderModule(
        createShaderModule(m_vkd, m_device, m_context.getBinaryCollection().get("vert"), 0u));
    const Unique<VkShaderModule> fragmentShaderModule(
        createShaderModule(m_vkd, m_device, m_context.getBinaryCollection().get("frag"), 0u));

    const VkPipelineVertexInputStateCreateInfo vertexInputState = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        nullptr,
        (VkPipelineVertexInputStateCreateFlags)0u,
        0u,
        nullptr,
        0u,
        nullptr
    };

    const std::vector<VkViewport> viewports(1, makeViewport(size));
    const std::vector<VkRect2D> scissors(1, makeRect2D(size));

    return ::makeGraphicsPipeline(
        m_vkd,
        m_device,
        layout,
        *vertexShaderModule,
        VK_NULL_HANDLE,
        VK_NULL_HANDLE,
        VK_NULL_HANDLE,
        *fragmentShaderModule,
        renderPass,
        viewports,
        scissors,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        0u,
        0u,
        &vertexInputState,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr);
}

tcu::TestStatus RenderAttachmentTestInstance::iterate(void)
{
    const tcu::UVec2 imageSize(256, 256);
    const uint32_t planeIdx = (uint32_t) (de::findLSB(m_aspect) - 4);

    // create the image
    const Unique<VkImage> testImage(createImage(imageSize));
    const std::vector<AllocationSp> allocations(
        allocateAndBindImageMemory(m_vkd, m_device, m_context.getDefaultAllocator(), *testImage, m_format,
                                   m_disjoint ? VK_IMAGE_CREATE_DISJOINT_BIT : (VkImageCreateFlagBits)0u));

    // initialize all planes with zeros
    MultiPlaneImageData imageData(m_format, imageSize);
    fillZero(&imageData);
    uploadImage(m_vkd, m_device, m_context.getUniversalQueueFamilyIndex(), m_context.getDefaultAllocator(), *testImage,
                imageData, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0);

    PlanarFormatDescription::Plane planeDesc = imageData.getDescription().planes[planeIdx];
    const tcu::UVec2 renderSize(256 / planeDesc.widthDivisor, 256 / planeDesc.heightDivisor);

    // create the render pass for the target plane
    const Unique<VkImageView> imageView(createImageView(*testImage, planeDesc.planeCompatibleFormat));
    const Unique<VkRenderPass> renderPass(createRenderPass(planeDesc.planeCompatibleFormat));
    const Unique<VkFramebuffer> framebuffer(createFramebuffer(*renderPass, *imageView, renderSize));

    // create the pipeline to render the quad
    const Unique<VkPipelineLayout> pipelineLayout(createRenderPipelineLayout());
    const Unique<VkPipeline> pipeline(createRenderPipeline(*renderPass, *pipelineLayout, renderSize));

    const Unique<VkCommandPool> commandPool(
        createCommandPool(m_vkd, m_context.getDevice(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                          m_context.getUniversalQueueFamilyIndex()));
    const Unique<VkCommandBuffer> commandBuffer(
        allocateCommandBuffer(m_vkd, m_context.getDevice(), *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(m_vkd, *commandBuffer);

    {
        const VkRenderPassBeginInfo beginInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            nullptr,
            *renderPass,
            *framebuffer,
            { 0, 0, renderSize.x(), renderSize.y() },
            0u,
            nullptr
        };

        m_vkd.cmdBeginRenderPass(*commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    // renders the test quad and pattern
    m_vkd.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
    m_vkd.cmdDraw(*commandBuffer, 6u, 1u, 0u, 0u);

    m_vkd.cmdEndRenderPass(*commandBuffer);

    endCommandBuffer(m_vkd, *commandBuffer);

    submitCommandsAndWait(m_vkd, m_context.getDevice(), m_context.getUniversalQueue(), *commandBuffer, false, 1u);

    // refresh local plane pointers
    downloadImage(m_vkd, m_device, m_context.getUniversalQueueFamilyIndex(), m_context.getDefaultAllocator(), *testImage,
                  &imageData, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0);

    // verify the plane contents
    return verify(imageData, planeIdx);
}

tcu::TestStatus RenderAttachmentTestInstance::verify(const MultiPlaneImageData &imageData, uint32_t planeIdx)
{
    // Verify the contents of each plane, the plane at planeIdx will be checked for
    // the sanity pattern drawn by the shader, the other planes be checked for zeros.
    uint32_t numPlanes = imageData.getDescription().numPlanes;
    for (uint32_t i = 0; i < numPlanes; ++i)
    {
        PlanarFormatDescription::Plane planeDesc = imageData.getDescription().planes[i];
        tcu::IVec3 planeSize(imageData.getSize().x() / planeDesc.widthDivisor,
                             imageData.getSize().y() / planeDesc.heightDivisor, 1);
        tcu::ConstPixelBufferAccess pixelAccess(mapVkFormat(planeDesc.planeCompatibleFormat), planeSize,
                                                imageData.getPlanePtr(i));
        if (!verifyPlane(pixelAccess, i, i == planeIdx))
            return tcu::TestStatus::fail(std::string("Comparison failed: PLANE_") + std::to_string(i));
    }

    return tcu::TestStatus::pass("Pass");
}

bool RenderAttachmentTestInstance::verifyPlane(const tcu::ConstPixelBufferAccess &imageData, uint32_t planeIdx,
                                               bool isTarget)
{
    tcu::TextureFormat::ChannelOrder order = imageData.getFormat().order;
    for (uint32_t x = 0; x < (uint32_t) imageData.getWidth(); ++x)
    {
        for (uint32_t y = 0; y < (uint32_t) imageData.getHeight(); ++y)
        {
            // If this is the target plane, check for the sanity pattern,
            // otherwise the plane should be all zeros
            float expect;
            tcu::Vec4 vec = imageData.getPixel(x, y);
            switch (order)
            {
            case tcu::TextureFormat::RG:
                // Tests the second component (if the plane holds two channels)
                expect = (isTarget && y % 2 == 0) ? 1.0f : 0.0f;
                if (vec.y() != expect)
                {
                    m_context.getTestContext().getLog()
                        << tcu::TestLog::Message << "Comparison failed at: planes[" << planeIdx << "].pixels[" << x
                        << "][" << y << "].values[1]: " << vec.y() << " != " << expect << tcu::TestLog::EndMessage;
                    return false;
                }
                [[fallthrough]];
            case tcu::TextureFormat::R:
                // Tests the first component
                expect = (isTarget && x % 2 == 0) ? 1.0f : 0.0f;
                if (vec.x() != expect)
                {
                    m_context.getTestContext().getLog()
                        << tcu::TestLog::Message << "Comparison failed at: planes[" << planeIdx << "].pixels[" << x
                        << "][" << y << "].values[0]: " << vec.x() << " != " << expect << tcu::TestLog::EndMessage;
                    return false;
                }
                break;
            default:
                DE_FATAL("Unreachable");
            }
        }
    }
    return true;
}

class RenderAttachmentTestCase : public TestCase
{
public:
    RenderAttachmentTestCase(tcu::TestContext &context, const char *name, VkFormat format, VkImageAspectFlags aspect,
                             bool disjoint);

protected:
    void checkSupport(Context &context) const;
    TestInstance *createInstance(Context &context) const;
    void initPrograms(SourceCollections &programCollection) const;

private:
    const VkFormat m_format;
    const VkImageAspectFlags m_aspect;
    const bool m_disjoint;

};

RenderAttachmentTestCase::RenderAttachmentTestCase(tcu::TestContext &context, const char *name, VkFormat format,
                                                   VkImageAspectFlags aspect, bool disjoint)
    : TestCase(context, name)
    , m_format(format)
    , m_aspect(aspect)
    , m_disjoint(disjoint)
{
}

void RenderAttachmentTestCase::checkSupport(Context &context) const
{
    // These multiplanar formats are provided by this extension
    context.requireDeviceFunctionality("VK_KHR_sampler_ycbcr_conversion");

    const auto &instInt = context.getInstanceInterface();
    auto physicalDevice = context.getPhysicalDevice();

    VkImageFormatProperties properties;
    deMemset(&properties, 0, sizeof(properties));

    VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                   VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VkImageCreateFlags createFlags = VK_IMAGE_CREATE_EXTENDED_USAGE_BIT | VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    if (m_disjoint)
        createFlags |= VK_IMAGE_CREATE_DISJOINT_BIT;

    // Check if this combination of format and flags are supported
    if (instInt.getPhysicalDeviceImageFormatProperties(physicalDevice, m_format, VK_IMAGE_TYPE_2D,
                                                       VK_IMAGE_TILING_OPTIMAL, usageFlags, createFlags,
                                                       &properties) != VK_SUCCESS)
        TCU_THROW(NotSupportedError, "Image format is not supported");
}

TestInstance *RenderAttachmentTestCase::createInstance(Context &context) const
{
    return new RenderAttachmentTestInstance(context, m_format, m_aspect, m_disjoint);
}

void RenderAttachmentTestCase::initPrograms(SourceCollections &programCollection) const
{
    // Simple vertex shader to render a quad using two triangles
    static const char *vertShader = "#version 450\n"
                                    "precision highp float;\n"
                                    "precision mediump int;\n"
                                    "out gl_PerVertex { vec4 gl_Position; };\n"
                                    "void main (void)\n"
                                    "{\n"
                                    "    gl_Position = vec4(((gl_VertexIndex + 2) / 3) % 2 == 0 ? -1.0 : 1.0,\n"
                                    "                       ((gl_VertexIndex + 1) / 3) % 2 == 0 ? -1.0 : 1.0, 0.0, 1.0);\n"
                                    "}\n";

    // Renders a basic sanity check pattern to the red and green channels
    // red = (x % 2 == 0 ? 1.0 : 0.0)
    // green = (y % 2 == 0 ? 1.0 : 0.0)
    static const char *fragShader = "#version 450\n"
                                    "precision highp float;\n"
                                    "precision mediump int;\n"
                                    "layout(location = 0) out mediump vec4 dEQP_FragColor;\n"
                                    "void main (void)\n"
                                    "{\n"
                                    "    dEQP_FragColor = vec4(int(gl_FragCoord.x) % 2 == 0 ? 1.0 : 0.0,\n"
                                    "                          int(gl_FragCoord.y) % 2 == 0 ? 1.0 : 0.0, 0.0, 1.0);\n"
                                    "}\n";

    programCollection.glslSources.add("vert") << glu::VertexSource(vertShader);
    programCollection.glslSources.add("frag") << glu::FragmentSource(fragShader);
}

} // namespace

tcu::TestCaseGroup *createRenderAttachmentTests(tcu::TestContext &testCtx)
{
    struct YCbCrFormatData
    {
        const char *const name;
        const VkFormat format;
        const std::vector<VkImageAspectFlags> planes;
    };

    static const std::vector<YCbCrFormatData> ycbcrFormats = {
        {
            "g8_b8_r8_3plane_420_unorm",
            VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
            {
                VK_IMAGE_ASPECT_PLANE_0_BIT,
                VK_IMAGE_ASPECT_PLANE_1_BIT,
                VK_IMAGE_ASPECT_PLANE_2_BIT
            }
        },
        {
            "g8_b8r8_2plane_420_unorm",
            VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
            {
                VK_IMAGE_ASPECT_PLANE_0_BIT,
                VK_IMAGE_ASPECT_PLANE_1_BIT
            }
        },
        {
            "g8_b8_r8_3plane_422_unorm",
            VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
            {
                VK_IMAGE_ASPECT_PLANE_0_BIT,
                VK_IMAGE_ASPECT_PLANE_1_BIT,
                VK_IMAGE_ASPECT_PLANE_2_BIT
            }
        },
        {
            "g8_b8r8_2plane_422_unorm",
            VK_FORMAT_G8_B8R8_2PLANE_422_UNORM,
            {
                VK_IMAGE_ASPECT_PLANE_0_BIT,
                VK_IMAGE_ASPECT_PLANE_1_BIT
            }
        },
        {
            "g8_b8_r8_3plane_444_unorm",
            VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM,
            {
                VK_IMAGE_ASPECT_PLANE_0_BIT,
                VK_IMAGE_ASPECT_PLANE_1_BIT,
                VK_IMAGE_ASPECT_PLANE_2_BIT
            }
        },
    };

    de::MovePtr<tcu::TestCaseGroup> renderAttachmentTests(new tcu::TestCaseGroup(testCtx, "render_attachment"));

    for (const auto &ycbcrFormat : ycbcrFormats)
    {
        de::MovePtr<tcu::TestCaseGroup> jointGroup(new tcu::TestCaseGroup(testCtx, "joint"));
        de::MovePtr<tcu::TestCaseGroup> disjointGroup(new tcu::TestCaseGroup(testCtx, "disjoint"));
        for (const auto &ycbcrPlane : ycbcrFormat.planes)
        {
            const char *plane;
            switch (ycbcrPlane)
            {
            case VK_IMAGE_ASPECT_PLANE_0_BIT:
                plane = "plane0";
                break;
            case VK_IMAGE_ASPECT_PLANE_1_BIT:
                plane = "plane1";
                break;
            case VK_IMAGE_ASPECT_PLANE_2_BIT:
                plane = "plane2";
                break;
            default:
                DE_FATAL("Unreachable");
            }
            jointGroup->addChild(new RenderAttachmentTestCase(renderAttachmentTests->getTestContext(), plane,
                                                              ycbcrFormat.format, ycbcrPlane, false));
            disjointGroup->addChild(new RenderAttachmentTestCase(renderAttachmentTests->getTestContext(), plane,
                                                                 ycbcrFormat.format, ycbcrPlane, true));
        }
        de::MovePtr<tcu::TestCaseGroup> formatGroup(new tcu::TestCaseGroup(testCtx, ycbcrFormat.name));
        formatGroup->addChild(jointGroup.release());
        formatGroup->addChild(disjointGroup.release());
        renderAttachmentTests->addChild(formatGroup.release());
    }

    return renderAttachmentTests.release();
}

} // namespace ycbcr

} // namespace vkt
