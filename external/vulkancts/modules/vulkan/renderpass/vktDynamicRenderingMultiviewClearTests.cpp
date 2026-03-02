/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 The Khronos Group Inc.
 * Copyright (c) 2026 Valve Corporation.
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
 * \brief Vulkan Dynamic Rendering Multiview Clear Tests
 *//*--------------------------------------------------------------------*/

#include "vktDynamicRenderingMultiviewClearTests.hpp"
#include "tcuImageCompare.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkDefs.hpp"
#include "vkImageUtil.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuVector.hpp"

#include "deUniquePtr.hpp"

#include <algorithm>
#include <iterator>
#include <memory>

namespace vkt
{
namespace renderpass
{

namespace
{

using namespace vk;

struct TestParams
{
    uint32_t viewMask;
    VkFormat format;
    tcu::IVec3 extent;
    std::vector<VkRect2D> clearRects; // If not empty, use vkCmdClearAttachments.

    VkImageCreateInfo getImageCreateInfo() const
    {
        const auto isDS  = isDepthStencilFormat(format);
        const auto usage = static_cast<VkImageUsageFlags>(
            (isDS ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

        const VkImageCreateInfo createInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            nullptr,
            0u,
            VK_IMAGE_TYPE_2D,
            format,
            makeExtent3D(tcu::IVec3(extent.x(), extent.y(), 1)),
            1u,
            static_cast<uint32_t>(extent.z()),
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_TILING_OPTIMAL,
            usage,
            VK_SHARING_MODE_EXCLUSIVE,
            1u,
            nullptr,
            VK_IMAGE_LAYOUT_UNDEFINED,
        };
        return createInfo;
    }

    TestParams(uint32_t viewMask_, VkFormat format_)
        : viewMask(viewMask_)
        , format(format_)
        , extent(1024, 1024, 4) // A large default extent enables color compression in some implementations.
        , clearRects()
    {
    }
};

using TestParamsPtr = std::shared_ptr<const TestParams>;

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

void checkSupport(Context &context, TestParamsPtr params)
{
    context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
    context.requireDeviceFunctionality("VK_KHR_multiview");

    VkImageFormatProperties formatProperties;
    const auto createInfo = params->getImageCreateInfo();
    const auto ctx        = context.getContextCommonData();
    const auto result     = ctx.vki.getPhysicalDeviceImageFormatProperties(
        ctx.physicalDevice, createInfo.format, createInfo.imageType, createInfo.tiling, createInfo.usage,
        createInfo.flags, &formatProperties);

    if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
        TCU_THROW(NotSupportedError, "Format not supported");

    if (result != VK_SUCCESS)
        TCU_FAIL("vkGetPhysicalDeviceImageFormatProperties failed unexpectedly");

    if (formatProperties.maxArrayLayers < createInfo.arrayLayers)
        TCU_THROW(NotSupportedError, "Required number of layers not supported");

    if (formatProperties.maxExtent.width < createInfo.extent.width ||
        formatProperties.maxExtent.height < createInfo.extent.height)
        TCU_THROW(NotSupportedError, "Required extent not supported");
}

VkImageViewType getImageViewType(const VkImageCreateInfo &createInfo)
{
    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    switch (createInfo.imageType)
    {
    case VK_IMAGE_TYPE_1D:
        viewType = ((createInfo.arrayLayers == 1) ? VK_IMAGE_VIEW_TYPE_1D : VK_IMAGE_VIEW_TYPE_1D_ARRAY);
        break;
    case VK_IMAGE_TYPE_2D:
        viewType = ((createInfo.arrayLayers == 1) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY);
        break;
    case VK_IMAGE_TYPE_3D:
        viewType = VK_IMAGE_VIEW_TYPE_3D;
        break;
    default:
        DE_ASSERT(false);
    }
    return viewType;
}

tcu::TestStatus runTest(Context &context, TestParamsPtr params)
{
    const bool useCmd      = !params->clearRects.empty();
    const auto ctx         = context.getContextCommonData();
    const auto createInfo  = params->getImageCreateInfo();
    const auto tcuFormat   = mapVkFormat(createInfo.format);
    const auto aspectFlags = getImageAspectFlags(tcuFormat);
    const bool hasColor    = (aspectFlags & VK_IMAGE_ASPECT_COLOR_BIT);
    const bool hasDepth    = (aspectFlags & VK_IMAGE_ASPECT_DEPTH_BIT);
    const bool hasStencil  = (aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT);
    const bool isDS        = (hasDepth || hasStencil);
    const auto srr = makeImageSubresourceRange(aspectFlags, 0u, createInfo.mipLevels, 0u, createInfo.arrayLayers);
    const auto rpLayout =
        (isDS ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const auto pixelCount = params->extent.x() * params->extent.y() * params->extent.z();

    ImageWithMemory image(ctx.vkd, ctx.device, ctx.allocator, createInfo, MemoryRequirement::Any);
    const auto imageViewType = getImageViewType(createInfo);
    const auto imageView     = makeImageView(ctx.vkd, ctx.device, *image, imageViewType, createInfo.format, srr);

    tcu::TextureFormat colorOrDepthCopyFormat;
    tcu::TextureFormat stencilCopyFormat;

    std::unique_ptr<BufferWithMemory> colorOrDepthBuffer;
    if (hasColor || hasDepth)
    {
        colorOrDepthCopyFormat = (isDS ? getDepthCopyFormat(createInfo.format) : tcuFormat);
        const auto colorOrDepthBufferSize =
            static_cast<VkDeviceSize>(tcu::getPixelSize(colorOrDepthCopyFormat) * pixelCount);
        const auto colorOrDepthBufferInfo =
            makeBufferCreateInfo(colorOrDepthBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        colorOrDepthBuffer.reset(
            new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, colorOrDepthBufferInfo, HostIntent::R));
    }

    std::unique_ptr<BufferWithMemory> stencilBuffer;
    if (hasStencil)
    {
        stencilCopyFormat            = getStencilCopyFormat(createInfo.format);
        const auto stencilBufferSize = static_cast<VkDeviceSize>(tcu::getPixelSize(stencilCopyFormat) * pixelCount);
        const auto stencilBufferInfo = makeBufferCreateInfo(stencilBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        stencilBuffer.reset(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, stencilBufferInfo, HostIntent::R));
    }

    const CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);

    // Clear the image to a known value before starting.
    {
        const auto barrier = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, *image, srr);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &barrier);
    }
    const auto initialColor      = tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    const auto initialDepth      = 0.0f;
    const auto initialStencil    = 0;
    const auto initialDSValue    = makeClearDepthStencilValue(initialDepth, initialStencil);
    const auto initialColorValue = makeClearValueColor(initialColor);
    if (isDS)
        ctx.vkd.cmdClearDepthStencilImage(cmdBuffer, *image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &initialDSValue, 1u,
                                          &srr);
    else
        ctx.vkd.cmdClearColorImage(cmdBuffer, *image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &initialColorValue.color,
                                   1u, &srr);
    const auto colorStages = static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    const auto dsStages    = static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
    {
        const auto dstAccess =
            (isDS ? (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT) :
                    (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT));
        const auto dstStage = (isDS ? dsStages : colorStages);
        const auto barrier  = makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, dstAccess,
                                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, rpLayout, *image, srr);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, dstStage, &barrier);
    }

    // Begin render pass.
    const auto renderArea = makeRect2D(params->extent);
    const auto loadOp     = (useCmd ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR);
    VkClearValue rpClearValue;
    const auto rpClearColor   = tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    const auto rpClearDepth   = 1.0f;
    const auto rpClearStencil = 255u;
    if (isDS)
        rpClearValue.depthStencil = makeClearValueDepthStencil(rpClearDepth, rpClearStencil).depthStencil;
    else
        rpClearValue.color = makeClearValueColor(rpClearColor).color;

    const VkRenderingAttachmentInfo renderingAttachmentInfo = {
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        nullptr,
        *imageView,
        rpLayout,
        VK_RESOLVE_MODE_NONE,
        VK_NULL_HANDLE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        loadOp,
        VK_ATTACHMENT_STORE_OP_STORE,
        rpClearValue,
    };
    const VkRenderingAttachmentInfo *pColorAttachment   = (hasColor ? &renderingAttachmentInfo : nullptr);
    const VkRenderingAttachmentInfo *pDepthAttachment   = (hasDepth ? &renderingAttachmentInfo : nullptr);
    const VkRenderingAttachmentInfo *pStencilAttachment = (hasStencil ? &renderingAttachmentInfo : nullptr);
    const VkRenderingInfo renderingInfo                 = {
        VK_STRUCTURE_TYPE_RENDERING_INFO,
        nullptr,
        0u,
        renderArea,
        1u,
        params->viewMask,
        ((pColorAttachment != nullptr) ? 1u : 0u),
        pColorAttachment,
        pDepthAttachment,
        pStencilAttachment,
    };
    ctx.vkd.cmdBeginRendering(cmdBuffer, &renderingInfo);
    if (useCmd)
    {
        const VkClearAttachment attInfo = {
            aspectFlags,
            0u,
            rpClearValue,
        };
        // Adds layer info to the rectangle (0u and 1u for multiview).
        const auto rectToClearRect = [](const VkRect2D &rect) { return VkClearRect{rect, 0u, 1u}; };
        std::vector<VkClearRect> clearRects;
        clearRects.reserve(params->clearRects.size());
        std::transform(params->clearRects.begin(), params->clearRects.end(), std::back_inserter(clearRects),
                       rectToClearRect);
        ctx.vkd.cmdClearAttachments(cmdBuffer, 1u, &attInfo, de::sizeU32(clearRects), de::dataOrNull(clearRects));
    }
    ctx.vkd.cmdEndRendering(cmdBuffer);
    {
        const auto srcAccess =
            (isDS ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
        const auto srcStage  = (isDS ? dsStages : colorStages);
        const auto dstAccess = VK_ACCESS_TRANSFER_READ_BIT;
        const auto dstStage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
        const auto barrier =
            makeImageMemoryBarrier(srcAccess, dstAccess, rpLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *image, srr);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, srcStage, dstStage, &barrier);
    }
    if (hasColor || hasDepth)
    {
        const auto copyAspect = (hasColor ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT);
        const auto srl        = makeImageSubresourceLayers(copyAspect, 0u, 0u, createInfo.arrayLayers);
        const auto copyRegion = makeBufferImageCopy(createInfo.extent, srl);
        ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, *image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorOrDepthBuffer->get(),
                                     1u, &copyRegion);
    }
    if (hasStencil)
    {
        const auto copyAspect = VK_IMAGE_ASPECT_STENCIL_BIT;
        const auto srl        = makeImageSubresourceLayers(copyAspect, 0u, 0u, createInfo.arrayLayers);
        const auto copyRegion = makeBufferImageCopy(createInfo.extent, srl);
        ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, *image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stencilBuffer->get(), 1u,
                                     &copyRegion);
    }
    {
        const auto barrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &barrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    if (colorOrDepthBuffer)
        invalidateAlloc(ctx.vkd, ctx.device, colorOrDepthBuffer->getAllocation());
    if (stencilBuffer)
        invalidateAlloc(ctx.vkd, ctx.device, stencilBuffer->getAllocation());

    std::unique_ptr<tcu::ConstPixelBufferAccess> colorOrDepthResult;
    std::unique_ptr<tcu::ConstPixelBufferAccess> stencilResult;

    std::unique_ptr<tcu::TextureLevel> colorOrDepthReferenceLevel;
    std::unique_ptr<tcu::TextureLevel> stencilReferenceLevel;
    std::unique_ptr<tcu::PixelBufferAccess> colorOrDepthReference;
    std::unique_ptr<tcu::PixelBufferAccess> stencilReference;

    if (hasColor || hasDepth)
    {
        colorOrDepthResult.reset(new tcu::ConstPixelBufferAccess(colorOrDepthCopyFormat, params->extent,
                                                                 colorOrDepthBuffer->getAllocation().getHostPtr()));
        colorOrDepthReferenceLevel.reset(
            new tcu::TextureLevel(colorOrDepthCopyFormat, params->extent.x(), params->extent.y(), params->extent.z()));
        colorOrDepthReference.reset(new tcu::PixelBufferAccess(colorOrDepthReferenceLevel->getAccess()));
    }

    if (hasStencil)
    {
        stencilResult.reset(new tcu::ConstPixelBufferAccess(stencilCopyFormat, params->extent,
                                                            stencilBuffer->getAllocation().getHostPtr()));
        stencilReferenceLevel.reset(
            new tcu::TextureLevel(stencilCopyFormat, params->extent.x(), params->extent.y(), params->extent.z()));
        stencilReference.reset(new tcu::PixelBufferAccess(stencilReferenceLevel->getAccess()));
    }

    auto &log             = context.getTestContext().getLog();
    bool fail             = false;
    const float threshold = 0.0f;
    const tcu::Vec4 thresholdVec(0.0f);

    for (int z = 0; z < params->extent.z(); ++z)
    {
        const bool inViewMask = (((1 << z) & params->viewMask) != 0);

        if (colorOrDepthBuffer)
        {
            const auto refLayer =
                tcu::getSubregion(*colorOrDepthReference, 0, 0, z, params->extent.x(), params->extent.y(), 1);
            if (useCmd)
            {
                if (hasColor)
                    tcu::clear(refLayer, initialColor);
                else if (hasDepth)
                    tcu::clearDepth(refLayer, initialDepth);
                else
                    DE_ASSERT(false);

                if (inViewMask)
                {
                    for (const auto &rect : params->clearRects)
                    {
                        const auto region = tcu::getSubregion(refLayer, rect.offset.x, rect.offset.y,
                                                              static_cast<int>(rect.extent.width),
                                                              static_cast<int>(rect.extent.height));
                        if (hasColor)
                            tcu::clear(region, rpClearColor);
                        else if (hasDepth)
                            tcu::clearDepth(region, rpClearDepth);
                        else
                            DE_ASSERT(false);
                    }
                }
            }
            else
            {
                if (hasColor)
                    tcu::clear(refLayer, (inViewMask ? rpClearColor : initialColor));
                else if (hasDepth)
                    tcu::clearDepth(refLayer, (inViewMask ? rpClearDepth : initialDepth));
                else
                    DE_ASSERT(false);
            }

            const auto resLayer =
                tcu::getSubregion(*colorOrDepthResult, 0, 0, z, params->extent.x(), params->extent.y(), 1);
            if (hasColor)
            {
                const auto imgName = "Color-Layer" + std::to_string(z);
                if (!tcu::floatThresholdCompare(log, imgName.c_str(), "", refLayer, resLayer, thresholdVec,
                                                tcu::COMPARE_LOG_ON_ERROR))
                    fail = true;
            }
            else if (hasDepth)
            {
                const auto imgName = "Depth-Layer" + std::to_string(z);
                if (!tcu::dsThresholdCompare(log, imgName.c_str(), "", refLayer, resLayer, threshold,
                                             tcu::COMPARE_LOG_ON_ERROR))
                    fail = true;
            }
            else
                DE_ASSERT(false);
        }
        if (stencilBuffer)
        {
            const auto refLayer =
                tcu::getSubregion(*stencilReference, 0, 0, z, params->extent.x(), params->extent.y(), 1);
            if (useCmd)
            {
                tcu::clearStencil(refLayer, initialStencil);

                if (inViewMask)
                {
                    for (const auto &rect : params->clearRects)
                    {
                        const auto region = tcu::getSubregion(refLayer, rect.offset.x, rect.offset.y,
                                                              static_cast<int>(rect.extent.width),
                                                              static_cast<int>(rect.extent.height));
                        tcu::clearStencil(region, rpClearStencil);
                    }
                }
            }
            else
            {
                tcu::clearStencil(refLayer, (inViewMask ? rpClearStencil : initialStencil));
            }

            const auto resLayer = tcu::getSubregion(*stencilResult, 0, 0, z, params->extent.x(), params->extent.y(), 1);
            const auto imgName  = "Stencil-Layer" + std::to_string(z);
            if (!tcu::dsThresholdCompare(log, imgName.c_str(), "", refLayer, resLayer, threshold,
                                         tcu::COMPARE_LOG_ON_ERROR))
                fail = true;
        }
    }

    if (fail)
        TCU_FAIL("Unexpected results found in output buffers; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createDynamicRenderingMultiviewClearTests(tcu::TestContext &testCtx)
{
    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "multiview_clear"));

    const std::vector<VkFormat> formatList{
        VK_FORMAT_R8G8B8A8_UNORM,     VK_FORMAT_D16_UNORM, VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_S8_UINT,
    };

    for (const auto format : formatList)
    {
        const auto groupName = getFormatSimpleName(format);
        GroupPtr formatGroup(new tcu::TestCaseGroup(testCtx, groupName.c_str()));
        for (const auto viewMask : {1u, 2u, 4u, 8u, 15u})
        {
            std::ostringstream testNameStream;
            testNameStream << "view_mask_0x" << std::hex << viewMask;
            const auto testName = testNameStream.str();

            TestParams params(viewMask, format);

            TestParamsPtr rpClearParams(new TestParams(params));
            addFunctionCase(formatGroup.get(), testName + "_render_pass", checkSupport, runTest, rpClearParams);

            const auto quadExtent  = params.extent / tcu::IVec3(2, 2, 1);
            const auto quadExtentU = quadExtent.asUint();
            params.clearRects.push_back(makeRect2D(quadExtent.x(), 0, quadExtentU.x(), quadExtentU.y()));
            params.clearRects.push_back(makeRect2D(0, quadExtent.y(), quadExtentU.x(), quadExtentU.y()));

            TestParamsPtr cmdClearParams(new TestParams(params));
            addFunctionCase(formatGroup.get(), testName + "_clear_regions", checkSupport, runTest, cmdClearParams);

            params.clearRects.clear();
            params.clearRects.push_back(makeRect2D(params.extent));

            TestParamsPtr cmdFullClearParams(new TestParams(params));
            addFunctionCase(formatGroup.get(), testName + "_clear_full", checkSupport, runTest, cmdFullClearParams);
        }
        mainGroup->addChild(formatGroup.release());
    }

    return mainGroup.release();
}

} // namespace renderpass
} // namespace vkt
