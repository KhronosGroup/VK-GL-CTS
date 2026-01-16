/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
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
 * \brief Test clearing some attachments but not all
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassClearSomeAttachmentsTests.hpp"
#include "vktRenderPassTestsUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vktTestCase.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTestLog.hpp"

namespace vkt::renderpass
{

using namespace vk;

enum class TestMode
{
    CLEAR_ONLY_COLOR = 0,
    CLEAR_ONLY_DEPTH
};

struct TestParams
{
    SharedGroupParams groupParams;
    TestMode testMode;
};

namespace
{

class AttachmentTestInstance : public vkt::TestInstance
{
public:
    AttachmentTestInstance(Context &context, const TestParams &testParams);
    virtual ~AttachmentTestInstance(void) = default;
    virtual tcu::TestStatus iterate(void);

protected:
    template <typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep,
              typename RenderPassCreateInfo>
    Move<VkRenderPass> createRenderPass(const DeviceInterface &vk, VkDevice vkDevice, VkFormat dsFormat,
                                        const TestParams testParams);

#ifndef CTS_USES_VULKANSC
    void beginSecondaryCmdBuffer(const DeviceInterface &vk, VkCommandBuffer secCmdBuffer);
#endif // CTS_USES_VULKANSC

private:
    const TestParams m_testParams;
};

AttachmentTestInstance::AttachmentTestInstance(Context &context, const TestParams &testParams)
    : vkt::TestInstance(context)
    , m_testParams(testParams)
{
}

tcu::TestStatus AttachmentTestInstance::iterate(void)
{
    const DeviceInterface &vk    = m_context.getDeviceInterface();
    const InstanceInterface &vki = m_context.getInstanceInterface();
    const VkDevice device        = m_context.getDevice();
    const VkPhysicalDevice pd    = m_context.getPhysicalDevice();
    Allocator &allocator         = m_context.getDefaultAllocator();

    const uint32_t size = 8;
    const VkExtent3D imageExtent{size, size, 1};
    const auto renderArea      = makeRect2D(size, size);
    const VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;

    const auto transferUsage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    const auto colorUsage    = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | transferUsage;
    const auto dsUsage       = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | transferUsage;

    // define color and depth stencil clear values for clearing images before renderpass
    const VkClearColorValue colorClearImage{{0.2f, 0.8f, 0.4f, 0.6f}};
    const VkClearDepthStencilValue dsClearImage{0.2f, 0};

    // define clear values for clearing some attachments in renderpass
    const tcu::Vec4 &clearColorAttachment{0.7f, 0.1f, 0.5f, 0.3f};
    const float clearDepthAttachment      = 0.7f;
    const uint32_t clearStencilAttachment = 2;

    const VkImageType imageType = VK_IMAGE_TYPE_2D;
    const auto tiling           = VK_IMAGE_TILING_OPTIMAL;
    const auto dsAspect         = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    const auto csrr             = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto dssrr            = makeImageSubresourceRange(dsAspect, 0u, 1u, 0u, 1u);

    // pick depth stencil format (one of d24s8 and d32s8 has to be supported)
    VkImageFormatProperties ifp;
    VkFormat dsFormat  = VK_FORMAT_D24_UNORM_S8_UINT;
    auto dsFormatCheck = vki.getPhysicalDeviceImageFormatProperties(pd, dsFormat, imageType, tiling, dsUsage, 0, &ifp);
    if (dsFormatCheck != VK_SUCCESS)
        dsFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;

    // create images for color and depth attachments
    ImageWithBuffer colorBuffer(vk, device, allocator, imageExtent, colorFormat, colorUsage, imageType, csrr);
    ImageWithBuffer dsBuffer(vk, device, allocator, imageExtent, dsFormat, dsUsage, imageType, dssrr);

    Move<VkRenderPass> renderPass;
    if (m_testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS_LEGACY)
        renderPass = createRenderPass<AttachmentDescription1, AttachmentReference1, SubpassDescription1,
                                      SubpassDependency1, RenderPassCreateInfo1>(vk, device, dsFormat, m_testParams);
    else if (m_testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS2)
        renderPass = createRenderPass<AttachmentDescription2, AttachmentReference2, SubpassDescription2,
                                      SubpassDependency2, RenderPassCreateInfo2>(vk, device, dsFormat, m_testParams);

    // create framebuffer if renderpass handle is valid
    Move<VkFramebuffer> framebuffer;
    VkImageView imageViews[]{colorBuffer.getImageView(), dsBuffer.getImageView()};
    if (*renderPass != VK_NULL_HANDLE)
        framebuffer = makeFramebuffer(vk, device, *renderPass, 2, imageViews, size, size);

    auto cmdPool   = makeCommandPool(vk, device, m_context.getUniversalQueueFamilyIndex());
    auto cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    Move<VkCommandBuffer> secCmdBuffer;

#ifndef CTS_USES_VULKANSC

    VkRenderingAttachmentInfo colorAttachment = initVulkanStructure();
    colorAttachment.imageView                 = colorBuffer.getImageView();
    colorAttachment.imageLayout               = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp                    = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp                   = VK_ATTACHMENT_STORE_OP_STORE;
    memcpy(colorAttachment.clearValue.color.float32, clearColorAttachment.getPtr(), sizeof(float) * 4);

    VkRenderingAttachmentInfo depthAttachment     = initVulkanStructure();
    depthAttachment.imageView                     = dsBuffer.getImageView();
    depthAttachment.imageLayout                   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp                        = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttachment.storeOp                       = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.clearValue.depthStencil.depth = clearDepthAttachment;

    if (m_testParams.testMode == TestMode::CLEAR_ONLY_DEPTH)
    {
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    }

    VkRenderingInfo renderingInfo      = initVulkanStructure();
    renderingInfo.renderArea           = renderArea;
    renderingInfo.layerCount           = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments    = &colorAttachment;
    renderingInfo.pDepthAttachment     = &depthAttachment;

    if (m_testParams.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
    {
        secCmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

        beginSecondaryCmdBuffer(vk, *secCmdBuffer);
        vk.cmdBeginRendering(*secCmdBuffer, &renderingInfo);
        vk.cmdEndRendering(*secCmdBuffer);
        endCommandBuffer(vk, *secCmdBuffer);
    }

#endif // CTS_USES_VULKANSC

    const auto tWriteAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
    const auto tDstLayout   = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    const VkImageMemoryBarrier initialBarriers[]{
        makeImageMemoryBarrier(VK_ACCESS_NONE, tWriteAccess, VK_IMAGE_LAYOUT_UNDEFINED, tDstLayout,
                               colorBuffer.getImage(), csrr),
        makeImageMemoryBarrier(VK_ACCESS_NONE, tWriteAccess, VK_IMAGE_LAYOUT_UNDEFINED, tDstLayout, dsBuffer.getImage(),
                               dssrr)};

    const VkImageMemoryBarrier preRenderpassBariers[]{
        makeImageMemoryBarrier(tWriteAccess, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, tDstLayout,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorBuffer.getImage(), csrr),
        makeImageMemoryBarrier(tWriteAccess, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, tDstLayout,
                               VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, dsBuffer.getImage(), dssrr)};

    beginCommandBuffer(vk, *cmdBuffer);

    // transition both images to transfer dst layout
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                          0, nullptr, 2, initialBarriers);

    // clear both images to specified values
    vk.cmdClearColorImage(*cmdBuffer, colorBuffer.getImage(), tDstLayout, &colorClearImage, 1, &csrr);
    vk.cmdClearDepthStencilImage(*cmdBuffer, dsBuffer.getImage(), tDstLayout, &dsClearImage, 1, &dssrr);

    // transition both images to attachment optimal layout
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr,
                          0, nullptr, 2, preRenderpassBariers);

    // clear only one attachment in renderpass to new value
    if (*renderPass != VK_NULL_HANDLE)
    {
        beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea, clearColorAttachment,
                        clearDepthAttachment, clearStencilAttachment);
        endRenderPass(vk, *cmdBuffer);
    }
    else
    {
#ifndef CTS_USES_VULKANSC
        if (*secCmdBuffer)
            vk.cmdExecuteCommands(*cmdBuffer, 1u, &*secCmdBuffer);
        else
        {
            vk.cmdBeginRendering(*cmdBuffer, &renderingInfo);
            vk.cmdEndRendering(*cmdBuffer);
        }
#endif // CTS_USES_VULKANSC
    }

    copyImageToBuffer(vk, *cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), tcu::IVec2(size),
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    copyImageToBuffer(vk, *cmdBuffer, dsBuffer.getImage(), dsBuffer.getBuffer(), tcu::IVec2(size),
                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                      1u, dsAspect, VK_IMAGE_ASPECT_DEPTH_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *cmdBuffer);

    auto &cAllocation = colorBuffer.getBufferAllocation();
    invalidateAlloc(vk, device, cAllocation);
    uint8_t *cBufferPtr = static_cast<uint8_t *>(cAllocation.getHostPtr());
    tcu::PixelBufferAccess colorAccess(mapVkFormat(colorFormat), size, size, 1, cBufferPtr);

    auto &dAllocation = dsBuffer.getBufferAllocation();
    invalidateAlloc(vk, device, dAllocation);
    uint8_t *dBufferPtr = static_cast<uint8_t *>(dAllocation.getHostPtr());

    tcu::TextureFormat dTexFormat(tcu::TextureFormat::D, tcu::TextureFormat::UNSIGNED_INT_24_8_REV);
    if (dsFormat == VK_FORMAT_D32_SFLOAT_S8_UINT)
        dTexFormat = tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::FLOAT);
    tcu::PixelBufferAccess depthAccess(dTexFormat, size, size, 1, dBufferPtr);

    // determine expected color and depth
    const float *ptr = clearColorAttachment.getPtr();
    float expectedDepth(dsClearImage.depth);
    if (m_testParams.testMode == TestMode::CLEAR_ONLY_DEPTH)
    {
        ptr           = colorClearImage.float32;
        expectedDepth = clearDepthAttachment;
    }
    tcu::Vec4 expectedColor(ptr[0], ptr[1], ptr[2], ptr[3]);

    // verify just few fragments
    const float epsilon = 0.05f;
    for (uint32_t i = 0; i < 4u; ++i)
    {
        tcu::Vec4 attColor = colorAccess.getPixel(i * 2, i * 2);
        float attDepth     = depthAccess.getPixDepth(i * 2, i * 2);

        if (tcu::boolAny(tcu::greaterThan(tcu::absDiff(expectedColor, attColor), tcu::Vec4(epsilon))) ||
            (deAbs(expectedDepth - attDepth) > epsilon))
        {
            m_context.getTestContext().getLog()
                << tcu::LogImage("color image", "", colorAccess) << tcu::LogImage("depth image", "", depthAccess);
            return tcu::TestStatus::fail("Fail");
        }
    }

    return tcu::TestStatus::pass("Pass");
}

template <typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep,
          typename RenderPassCreateInfo>
Move<VkRenderPass> AttachmentTestInstance::createRenderPass(const DeviceInterface &vk, VkDevice vkDevice,
                                                            VkFormat dsFormat, const TestParams testParams)
{
    auto colorLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    auto depthLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    if (testParams.testMode == TestMode::CLEAR_ONLY_DEPTH)
    {
        colorLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        depthLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    }

    const VkImageAspectFlags aspectMask =
        testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS_LEGACY ? 0 : VK_IMAGE_ASPECT_COLOR_BIT;

    const AttachmentDesc attachmentDescriptions[]{
        // Color attachment
        {
            nullptr,                                  // const void*                     pNext
            (VkAttachmentDescriptionFlags)0,          // VkAttachmentDescriptionFlags    flags
            VK_FORMAT_R8G8B8A8_UNORM,                 // VkFormat                        format
            VK_SAMPLE_COUNT_1_BIT,                    // VkSampleCountFlagBits           samples
            colorLoadOp,                              // VkAttachmentLoadOp              loadOp
            VK_ATTACHMENT_STORE_OP_STORE,             // VkAttachmentStoreOp             storeOp
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp              stencilLoadOp
            VK_ATTACHMENT_STORE_OP_DONT_CARE,         // VkAttachmentStoreOp             stencilStoreOp
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout                   initialLayout
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL  // VkImageLayout                   finalLayout
        },
        // Depth attachment
        {
            nullptr,                                          // const void*                     pNext
            (VkAttachmentDescriptionFlags)0,                  // VkAttachmentDescriptionFlags    flags
            dsFormat,                                         // VkFormat                        format
            VK_SAMPLE_COUNT_1_BIT,                            // VkSampleCountFlagBits           samples
            depthLoadOp,                                      // VkAttachmentLoadOp              loadOp
            VK_ATTACHMENT_STORE_OP_STORE,                     // VkAttachmentStoreOp             storeOp
            depthLoadOp,                                      // VkAttachmentLoadOp              stencilLoadOp
            VK_ATTACHMENT_STORE_OP_STORE,                     // VkAttachmentStoreOp             stencilStoreOp
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // VkImageLayout                   initialLayout
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL  // VkImageLayout                   finalLayout
        }};

    const AttachmentRef attachmentRefs[]{
        // Color attachment
        {
            nullptr,                                  // const void*          pNext
            0u,                                       // uint32_t             attachment
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout        layout
            aspectMask                                // VkImageAspectFlags   aspectMask
        },
        // Depth attachment
        {
            nullptr,                                                // const void*        pNext
            1u,                                                     // uint32_t           attachment
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,       // VkImageLayout      layout
            VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT // VkImageAspectFlags aspectMask
        }};

    const SubpassDesc subpassDescription{
        nullptr,
        (VkSubpassDescriptionFlags)0,    // VkSubpassDescriptionFlags    flags
        VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint          pipelineBindPoint
        0u,                              // uint32_t                     viewMask
        0u,                              // uint32_t                     inputAttachmentCount
        nullptr,                         // const VkAttachmentReference* pInputAttachments
        1u,                              // uint32_t                     colorAttachmentCount
        attachmentRefs,                  // const VkAttachmentReference* pColorAttachments
        nullptr,                         // const VkAttachmentReference* pResolveAttachments
        &attachmentRefs[1],              // const VkAttachmentReference* pDepthStencilAttachment
        0u,                              // uint32_t                     preserveAttachmentCount
        nullptr};                        // const uint32_t*              pPreserveAttachments

    const RenderPassCreateInfo rpi(nullptr,                    // const void*                    pNext
                                   (VkRenderPassCreateFlags)0, // VkRenderPassCreateFlags        flags
                                   2u,                         // uint32_t                       attachmentCount
                                   attachmentDescriptions,     // const VkAttachmentDescription* pAttachments
                                   1u,                         // uint32_t                       subpassCount
                                   &subpassDescription,        // const VkSubpassDescription*    pSubpasses
                                   0u,                         // uint32_t                       dependencyCount
                                   nullptr,                    // const VkSubpassDependency*     pDependencies
                                   0u,                         // uint32_t                       correlatedViewMaskCount
                                   nullptr);                   // const uint32_t*                pCorrelatedViewMasks

    return rpi.createRenderPass(vk, vkDevice);
}

#ifndef CTS_USES_VULKANSC
void AttachmentTestInstance::beginSecondaryCmdBuffer(const DeviceInterface &vk, VkCommandBuffer secCmdBuffer)
{
    VkFormat colorAttachmentFormat(VK_FORMAT_R8G8B8A8_UNORM);
    VkCommandBufferInheritanceRenderingInfoKHR iri = initVulkanStructure();
    iri.colorAttachmentCount                       = 1u;
    iri.pColorAttachmentFormats                    = &colorAttachmentFormat;
    iri.rasterizationSamples                       = VK_SAMPLE_COUNT_1_BIT;

    const VkCommandBufferInheritanceInfo bufferInheritanceInfo = initVulkanStructure(&iri);

    VkCommandBufferBeginInfo commandBufBeginParams = initVulkanStructure();
    commandBufBeginParams.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    commandBufBeginParams.pInheritanceInfo         = &bufferInheritanceInfo;

    VK_CHECK(vk.beginCommandBuffer(secCmdBuffer, &commandBufBeginParams));
}
#endif // CTS_USES_VULKANSC

class AttachmentTest : public TestCase
{
public:
    AttachmentTest(tcu::TestContext &testContext, const std::string &name, const TestParams &testParams);
    virtual ~AttachmentTest(void) = default;

    void checkSupport(Context &context) const;
    virtual TestInstance *createInstance(Context &context) const;

private:
    const TestParams m_testParams;
};

AttachmentTest::AttachmentTest(tcu::TestContext &testContext, const std::string &name, const TestParams &testParams)
    : TestCase(testContext, name)
    , m_testParams(testParams)
{
}

void AttachmentTest::checkSupport(Context &context) const
{
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_testParams.groupParams->pipelineConstructionType);
    if (m_testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS2)
        context.requireDeviceFunctionality("VK_KHR_create_renderpass2");
    else if (m_testParams.groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
}

TestInstance *AttachmentTest::createInstance(Context &context) const
{
    return new AttachmentTestInstance(context, m_testParams);
}

} // namespace

tcu::TestCaseGroup *createRenderPassClearSomeAttachmentsTests(tcu::TestContext &testCtx,
                                                              const SharedGroupParams groupParams)
{
    de::MovePtr<tcu::TestCaseGroup> clearSomeAttTests(new tcu::TestCaseGroup(testCtx, "clear_some_attachments"));

    // clear_only_color:
    // 1. have color attachment with loadOp = CLEAR and storeOp = STORE
    // 2. have depth attachment with loadOp = LOAD and storeOp = STORE
    // 3. use VkRenderPassBeginInfo to clear only the color attachment

    std::pair<std::string, TestMode> cases[]{{"clear_only_color", TestMode::CLEAR_ONLY_COLOR},
                                             {"clear_only_depth", TestMode::CLEAR_ONLY_DEPTH}};

    TestParams params{
        groupParams,
        TestMode::CLEAR_ONLY_COLOR,
    };
    for (const auto &c : cases)
    {
        params.testMode = c.second;
        clearSomeAttTests->addChild(new AttachmentTest(testCtx, c.first, params));
    }

    return clearSomeAttTests.release();
}

} // namespace vkt::renderpass
