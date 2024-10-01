/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
 * Copyright (c) 2023 Google Inc.
 * Copyright (c) 2023 LunarG, Inc.
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
 * \brief Android Hardware Buffer External Format Resolve Draw Tests
 *//*--------------------------------------------------------------------*/

#include "vktDrawAhbExternalFormatResolveTests.hpp"

#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktDrawBaseClass.hpp"

#include "../image/vktImageTestsUtil.hpp"
#include "../util/vktExternalMemoryUtil.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuVector.hpp"

#include "deRandom.hpp"

using namespace vkt::ExternalMemoryUtil;
using namespace vk;
using std::vector;

namespace vkt
{
namespace Draw
{
namespace
{

struct TestParams
{
    vk::VkRect2D m_renderArea;
    tcu::UVec2 m_imageSize;
    AndroidHardwareBufferInstance::Format m_format;
    AndroidHardwareBufferInstance::Usage m_usage;
    GroupParams m_groupParams;
    bool m_isClearOnly;
    bool m_partialDraw;
    bool m_isInputAttachment; // Tests input attachment
};

struct DrawResources
{
    Move<VkImage> m_androidExternalImage;
    Move<VkDeviceMemory> m_androidExternalImageMemory;
    Move<VkImageView> m_androidExternalImageView;
    de::MovePtr<ImageWithMemory> m_androidColorAttachmentImage; // Used if nullColorAttachment is false
    Move<VkImageView> m_androidColorAttachmentImageView;
    de::MovePtr<BufferWithMemory> m_vertexBuffer;
    Move<VkShaderModule> m_vertexShader;
    Move<VkShaderModule> m_fragmentShaderBase;
    Move<VkShaderModule> m_fragmentShaderInput;
    Move<VkPipelineLayout> m_basePipelineLayout;
    Move<VkPipeline> m_basePipeline; // Draws to external image
    Move<VkPipelineLayout> m_inputAttachmentPipelineLayout;
    Move<VkPipeline>
        m_inputAttachmentPipeline; // Reads from input attachment (external image) and renders to vulkan image
    Move<VkRenderPass> m_renderPass;
    Move<VkFramebuffer> m_framebuffer;
    Move<VkRenderPass> m_renderPassClear;
    Move<VkFramebuffer> m_framebufferClear;

    // Resources for input attachment testing
    de::MovePtr<ImageWithMemory>
        m_resultAttachmentImage; // Used as render target when reading from external image as input attachment
    Move<VkImageView> m_resultAttachmentImageView;
    de::MovePtr<BufferWithMemory> m_resultBuffer;
    Move<VkDescriptorPool> m_descriptorPool;
    Move<VkDescriptorSetLayout> m_descriptorSetLayout;
    Move<VkDescriptorSet> m_descriptorSet;
};

class AhbExternalFormatResolveTestInstance : public TestInstance
{
public:
    AhbExternalFormatResolveTestInstance(Context &context, const TestParams &params);
    virtual ~AhbExternalFormatResolveTestInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;

protected:
    void renderToExternalFormat(AndroidHardwareBufferInstance &androidBuffer);
    void clearAttachments(const vk::DeviceInterface &vk, vk::VkCommandBuffer commandBuffer);
    void doRenderPass(const vk::DeviceInterface &vk, const vk::VkDevice device, const VkQueue queue,
                      const uint32_t queueFamilyIndex, bool renderInputAttachment);
    void copyImageToBuffer(const vk::DeviceInterface &vk, vk::VkCommandBuffer commandBuffer);
    // Transitions all used attachments to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    void initialAttachmentTransition(const vk::DeviceInterface &vk, vk::VkCommandBuffer commandBuffer);
    // Transition input attachment back to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL from VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    void transitionInputAttachmentToOutput(const vk::DeviceInterface &vk, vk::VkCommandBuffer commandBuffer);
    bool checkExternalFormatTestingRequired(const AndroidHardwareBufferInstance &androidBuffer);
    // ahbFormatVulkanFormatAlphaMismatch is used to know if the original AHB format does not have alpha but the texture level passed does.
    // This is required to correctly build the image for inputAttachment tests. When reading from a format with no alpha, we will get maxValue
    // which is the value we need to write to the reference image we are building.
    void buildReferenceImage(tcu::TextureLevel &reference, bool performDownsample,
                             bool ahbFormatVulkanFormatAlphaMismatch) const;

    Move<VkImageView> createImageView(VkImage image, vk::VkFormat format);
    void createImagesAndViews(AndroidHardwareBufferInstance &androidBuffer);
    void createRenderPass(void);
    void createFramebuffer(void);
    void createDescriptors(void);
    void createPipelineLayouts(void);
    void createPipelines(void);

    // Draw commands
    void beginRender(VkCommandBuffer cmd, const vk::VkRect2D &renderArea, bool clearPass) const;
    void endRender(VkCommandBuffer cmd) const;
    // When drawToInputAttachment is true, first subpass that draws to the external format is skipped (values will be loaded from a previous draw)
    // and the external format will be used as input
    void drawCommands(VkCommandBuffer cmd, bool drawFromInputAttachment) const;

    DrawResources m_resources;
    const vk::VkRect2D m_renderArea;
    tcu::Vec4 m_clearColor;
    uint64_t m_retrievedInternalFormat = 0u;
    const AndroidHardwareBufferInstance::Format m_format;
    const AndroidHardwareBufferInstance::Usage m_usage;
    const uint32_t m_width;
    const uint32_t m_height;
    const uint32_t m_layers                = 1u;
    vk::VkChromaLocation m_xChromaLocation = vk::VK_CHROMA_LOCATION_LAST;
    vk::VkChromaLocation m_yChromaLocation = vk::VK_CHROMA_LOCATION_LAST;
    vk::VkFormat m_colorAttachmentFormat   = vk::VK_FORMAT_UNDEFINED;
    vk::VkBool32 m_nullColorAttachment     = VK_FALSE;
    const GroupParams m_groupParams;
    const bool m_isClearOnly;
    bool m_partialDraw;
    const bool m_isInputAttachment;
};

AhbExternalFormatResolveTestInstance::AhbExternalFormatResolveTestInstance(Context &context, const TestParams &params)
    : TestInstance(context)
    , m_renderArea(params.m_renderArea)
    , m_format(params.m_format)
    , m_usage(params.m_usage)
    , m_width(params.m_imageSize.x())
    , m_height(params.m_imageSize.y())
    , m_groupParams(params.m_groupParams)
    , m_isClearOnly(params.m_isClearOnly)
    , m_partialDraw(params.m_partialDraw)
    , m_isInputAttachment(params.m_isInputAttachment)
{
}

tcu::TestStatus AhbExternalFormatResolveTestInstance::iterate(void)
{
    tcu::TestLog &log = m_context.getTestContext().getLog();

    AndroidHardwareBufferInstance androidBuffer;

    if (!androidBuffer.allocate(m_format, m_width, m_height, m_layers, m_usage))
    {
        const std::string formatName = AndroidHardwareBufferInstance::getFormatName(m_format);
        std::string skipReason = "Unable to allocate renderable AHB with parameters: width(" + std::to_string(m_width) +
                                 "), height(" + std::to_string(m_height) + "), layers(" + std::to_string(m_layers) +
                                 "), usage(" + std::to_string(m_usage) + ")";

        log << tcu::TestLog::Message << "Skipping format " << formatName << ". Reason: " << skipReason
            << tcu::TestLog::EndMessage;

        TCU_THROW(NotSupportedError, "Failed to allocate buffer");
    }

    if (!checkExternalFormatTestingRequired(androidBuffer))
        return tcu::TestStatus::pass("Rendering to format was already supported");

    // Vulkan rendering
    renderToExternalFormat(androidBuffer);

    tcu::TextureLevel cpuTexture;
    tcu::ConstPixelBufferAccess resultAccess;
    if (m_isInputAttachment)
    {
        const vk::DeviceInterface &vk = m_context.getDeviceInterface();
        const vk::VkDevice device     = m_context.getDevice();
        const Allocation &allocColor  = m_resources.m_resultBuffer->getAllocation();
        invalidateAlloc(vk, device, allocColor);
        resultAccess = tcu::ConstPixelBufferAccess(mapVkFormat(m_colorAttachmentFormat), m_width, m_height, 1u,
                                                   allocColor.getHostPtr());
    }
    else
    {
        // Need to destroy Vulkan image that has a reference to the android hardware buffer
        m_resources.~DrawResources();

        if (!androidBuffer.lock(AndroidHardwareBufferInstance::Usage::CPU_READ))
        {
            TCU_THROW(NotSupportedError, "Failed to lock buffer for CPU read");
        }

        // Format must have a valid tcu::TextureFormat which should be enforced by the time we reach this
        cpuTexture = tcu::TextureLevel(AndroidHardwareBufferInstance::formatToTextureFormat(m_format), m_width,
                                       m_height, m_layers);

        // RAW16 can be represented as UINT16, so there's no need to have a compressed path for this format
        if (androidBuffer.isRaw() && m_format != AndroidHardwareBufferInstance::Format::RAW16)
        {
            tcu::CompressedTexFormat compressedFormat = m_format == AndroidHardwareBufferInstance::Format::RAW10 ?
                                                            tcu::COMPRESSEDTEXFORMAT_AHB_RAW10 :
                                                            tcu::COMPRESSEDTEXFORMAT_AHB_RAW12;
            tcu::CompressedTexture compressedTexture(compressedFormat, m_width, m_height, m_layers);
            androidBuffer.copyAndroidBufferToCpuBufferCompressed(compressedTexture);
            compressedTexture.decompress(cpuTexture.getAccess());
        }
        else
            androidBuffer.copyAndroidBufferToCpuBuffer(cpuTexture);

        if (!androidBuffer.unlock())
        {
            TCU_THROW(NotSupportedError, "Failed to unlock buffer from CPU read");
        }

        resultAccess = cpuTexture.getAccess();
    }

    // Validate output
    {
        tcu::TextureFormat textureFormat = m_isInputAttachment ?
                                               mapVkFormat(m_colorAttachmentFormat) :
                                               AndroidHardwareBufferInstance::formatToTextureFormat(m_format);
        tcu::TextureLevel reference(textureFormat, m_width, m_height, m_layers);
        const bool alphaMismatch =
            !AndroidHardwareBufferInstance::hasFormatAlpha(m_format) && hasAlphaChannel(textureFormat.order);
        const bool isYuvFormat = AndroidHardwareBufferInstance::isFormatYuv(m_format);
        buildReferenceImage(reference, isYuvFormat, alphaMismatch);
        const tcu::ConstPixelBufferAccess referenceAccess = reference.getAccess();
        const char *name                                  = "Render validation";
        const char *description = "Validate output image was rendered according to expectation (if YUV and input test, "
                                  "a follow up test is done for no downsample)";
        // Some implementations of format YCbCr_P010 will have reduced range, which requires allowing for some threshold since we are rendering with 1.0f
        const tcu::UVec4 threshold = (m_format == AndroidHardwareBufferInstance::Format::YCbCr_P010) ?
                                         tcu::UVec4(4u) :
                                         tcu::UVec4(1u, 0u, 1u, 0u);

        if (!tcu::intThresholdCompare(log, name, description, referenceAccess, resultAccess, threshold,
                                      tcu::COMPARE_LOG_ON_ERROR))
            return tcu::TestStatus::fail("Result image does not match reference image");
    }

    return tcu::TestStatus::pass("");
}

void AhbExternalFormatResolveTestInstance::renderToExternalFormat(AndroidHardwareBufferInstance &androidBuffer)
{
    const vk::DeviceInterface &vk   = m_context.getDeviceInterface();
    const vk::VkDevice device       = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    // Create required resources for test
    createImagesAndViews(androidBuffer);
    createRenderPass();
    createFramebuffer();
    const std::string shaderType = vkt::image::getGlslAttachmentType(m_colorAttachmentFormat);
    m_resources.m_vertexShader   = vk::createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"));
    m_resources.m_fragmentShaderBase =
        vk::createShaderModule(vk, device, m_context.getBinaryCollection().get("frag_" + shaderType));
    if (m_isInputAttachment)
    {
        std::string swizzleOrder = vkt::image::isComponentSwizzled(m_colorAttachmentFormat) ? "bgr" : "rgb";
        std::string shaderName   = "frag_input_" + shaderType + "_" + swizzleOrder;
        m_resources.m_fragmentShaderInput =
            vk::createShaderModule(vk, device, m_context.getBinaryCollection().get(shaderName));
    }
    createDescriptors();
    createPipelineLayouts();
    createPipelines();
    {
        const float vertices[] = {
            -1.0f, -1.0f, // Bot left
            +1.0f, -1.0f, // Bot right
            -1.0f, +1.0f, // Top left
            +1.0f, +1.0f, // Top right
        };
        m_resources.m_vertexBuffer = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vk, device, m_context.getDefaultAllocator(),
                                 makeBufferCreateInfo(sizeof(vertices), vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
                                 vk::MemoryRequirement::HostVisible));

        // Copy vertices
        vk::Allocation &bufferAlloc = m_resources.m_vertexBuffer->getAllocation();
        void *bufferPtr             = bufferAlloc.getHostPtr();
        deMemcpy(bufferPtr, vertices, sizeof(vertices));
        vk::flushAlloc(vk, device, bufferAlloc);
    }

    {
        Move<VkCommandPool> commandPool = vk::createCommandPool(vk, device, 0u, queueFamilyIndex);
        Move<VkCommandBuffer> commandBuffer =
            vk::allocateCommandBuffer(vk, device, commandPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        vk::beginCommandBuffer(vk, commandBuffer.get());

        initialAttachmentTransition(vk, commandBuffer.get());

        // Clear all images for clear only and partial rendering to ensure expected values outside
        // of render area, since not all external formats may support VK_IMAGE_USAGE_TRANSFER_DST_BIT
        // safest clear method is to clear on attachment load with render size of image and do nothing else
        //if (m_isClearOnly || m_partialDraw)
        clearAttachments(vk, commandBuffer.get());

        vk::endCommandBuffer(vk, commandBuffer.get());
        vk::submitCommandsAndWait(vk, device, queue, commandBuffer.get());
    }

    // Render to external format resolve
    if (!m_isClearOnly)
    {
        // Render to external format
        doRenderPass(vk, device, queue, queueFamilyIndex, false);

        // Need to split rendering into 2 to force chroma downsample
        // If this does not force the chroma downsample, next idea to do would be destroying relevant
        // resource and creating them again

        // Render to m_colorAttachmentFormat texture reading from external format
        if (m_isInputAttachment)
            doRenderPass(vk, device, queue, queueFamilyIndex, true);
    }
}

void AhbExternalFormatResolveTestInstance::clearAttachments(const vk::DeviceInterface &vk,
                                                            vk::VkCommandBuffer commandBuffer)
{
    // Clear images on load without doing anything
    beginRender(commandBuffer, makeRect2D(m_width, m_height), true);
    if (m_isInputAttachment)
    {
        const vk::VkSubpassBeginInfo subpassBeginInfo = {
            vk::VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO, // VkStructureType        sType
            nullptr,                                  // const void*            pNext
            vk::VK_SUBPASS_CONTENTS_INLINE            // VkSubpassContents    contents
        };
        const vk::VkSubpassEndInfo subpassEndInfo = {
            vk::VK_STRUCTURE_TYPE_SUBPASS_END_INFO, // VkStructureType    sType
            nullptr                                 // const void*        pNext
        };
        vk.cmdNextSubpass2(commandBuffer, &subpassBeginInfo, &subpassEndInfo);
    }
    endRender(commandBuffer);

    if (m_isInputAttachment)
        transitionInputAttachmentToOutput(vk, commandBuffer);
}

void AhbExternalFormatResolveTestInstance::doRenderPass(const vk::DeviceInterface &vk, const vk::VkDevice device,
                                                        const VkQueue queue, const uint32_t queueFamilyIndex,
                                                        bool renderInputAttachment)
{
    Move<VkCommandPool> commandPool = vk::createCommandPool(vk, device, 0u, queueFamilyIndex);
    Move<VkCommandBuffer> primaryCommandBuffer =
        vk::allocateCommandBuffer(vk, device, commandPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    Move<VkCommandBuffer> secondaryCommandBuffer =
        vk::allocateCommandBuffer(vk, device, commandPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_SECONDARY);

    if (m_groupParams.useSecondaryCmdBuffer)
    {
        vk::VkExternalFormatANDROID externalFormat = {
            vk::VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID, // VkStructureType    sType
            nullptr,                                       // void*            pNext
            m_retrievedInternalFormat                      // uint64_t            externalFormat
        };

        const vk::VkFormat colorAttachmentFormat =
            m_nullColorAttachment ? vk::VK_FORMAT_UNDEFINED : m_colorAttachmentFormat;
        const vk::VkCommandBufferInheritanceRenderingInfo renderInfo = {
            vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO, // VkStructureType            sType
            &externalFormat,                                                 // const void*                pNext
            0u,                                                              // VkRenderingFlags            flags
            0u,                                                              // uint32_t                    viewMask
            1u,                       // uint32_t                    colorAttachmentCount
            &colorAttachmentFormat,   // const VkFormat*            pColorAttachmentFormats
            vk::VK_FORMAT_UNDEFINED,  // VkFormat                    depthAttachmentFormat
            vk::VK_FORMAT_UNDEFINED,  // VkFormat                    stencilAttachmentFormat
            vk::VK_SAMPLE_COUNT_1_BIT // VkSampleCountFlagBits    rasterizationSamples
        };

        const vk::VkCommandBufferInheritanceInfo inheritanceInfo = {
            vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, // VkStructureType                    sType
            &renderInfo,                                           // const void*                        pNext
            VK_NULL_HANDLE,                                        // VkRenderPass                        renderPass
            0u,                                                    // uint32_t                            subpass
            VK_NULL_HANDLE,                                        // VkFramebuffer                    framebuffer
            VK_FALSE, // VkBool32                            occlusionQueryEnable
            0u,       // VkQueryControlFlags                queryFlags
            0u,       // VkQueryPipelineStatisticFlags    pipelineStatistics
        };

        const vk::VkCommandBufferUsageFlags commandBufferBeginFlags =
            m_groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass ?
                static_cast<vk::VkCommandBufferUsageFlags>(0u) :
                static_cast<vk::VkCommandBufferUsageFlags>(vk::VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
        const vk::VkCommandBufferBeginInfo commandBufBeginParams = {
            vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType                    sType
            DE_NULL,                                         // const void*                        pNext
            commandBufferBeginFlags,                         // VkCommandBufferUsageFlags        flags
            &inheritanceInfo                                 // VkCommandBufferInheritanceInfo    pInheritanceInfo
        };
        VK_CHECK(vk.beginCommandBuffer(secondaryCommandBuffer.get(), &commandBufBeginParams));

        if (m_groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass)
            beginRender(secondaryCommandBuffer.get(), m_renderArea, false);

        drawCommands(secondaryCommandBuffer.get(), renderInputAttachment);

        if (m_groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass)
            endRender(secondaryCommandBuffer.get());

        vk::endCommandBuffer(vk, secondaryCommandBuffer.get());
    }

    vk::beginCommandBuffer(vk, primaryCommandBuffer.get());

    if (!m_groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass)
        beginRender(primaryCommandBuffer.get(), m_renderArea, false);

    if (m_groupParams.useSecondaryCmdBuffer)
        vk.cmdExecuteCommands(primaryCommandBuffer.get(), 1u, &secondaryCommandBuffer.get());
    else
        drawCommands(primaryCommandBuffer.get(), renderInputAttachment);

    if (!m_groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass)
        endRender(primaryCommandBuffer.get());

    if (m_isInputAttachment)
    {
        if (!renderInputAttachment)
            transitionInputAttachmentToOutput(vk, primaryCommandBuffer.get());
        else
            copyImageToBuffer(vk, primaryCommandBuffer.get());
    }

    vk::endCommandBuffer(vk, primaryCommandBuffer.get());
    vk::submitCommandsAndWait(vk, device, queue, primaryCommandBuffer.get());
}

void AhbExternalFormatResolveTestInstance::copyImageToBuffer(const vk::DeviceInterface &vk,
                                                             vk::VkCommandBuffer commandBuffer)
{
    // Copy result image to host visible buffer for validation
    if (m_isInputAttachment)
    {
        const vk::VkImageMemoryBarrier imageBarrier = {
            vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType            sType
            nullptr,                                    // const void*                pNext
            vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,   // VkAccessFlags            srcAccessMask
            vk::VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags            dstAccessMask
            vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,   // VkImageLayout            oldLayout
            vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,   // VkImageLayout            newLayout
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t                    srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t                    destQueueFamilyIndex
            m_resources.m_resultAttachmentImage->get(), // VkImage                    image
            vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageSubresourceRange    subresourceRange
                                          0u, VK_REMAINING_MIP_LEVELS, 0u, VK_REMAINING_ARRAY_LAYERS)};

        vk.cmdPipelineBarrier(commandBuffer, vk::VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                              0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &imageBarrier);

        const vk::VkImageSubresourceLayers subresource = {
            vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags    aspectMask
            0u,                            // uint32_t                mipLevel
            0u,                            // uint32_t                baseArrayLayer
            1u                             // uint32_t                layerCount
        };

        const vk::VkBufferImageCopy region = {
            0ull,                               // VkDeviceSize                    bufferOffset
            0u,                                 // uint32_t                        bufferRowLength
            0u,                                 // uint32_t                        bufferImageHeight
            subresource,                        // VkImageSubresourceLayers        imageSubresource
            makeOffset3D(0, 0, 0),              // VkOffset3D                    imageOffset
            makeExtent3D(m_width, m_height, 1u) // VkExtent3D                    imageExtent
        };

        vk.cmdCopyImageToBuffer(commandBuffer, m_resources.m_resultAttachmentImage->get(),
                                vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_resources.m_resultBuffer->get(), 1u,
                                &region);
    }
}

void AhbExternalFormatResolveTestInstance::initialAttachmentTransition(const vk::DeviceInterface &vk,
                                                                       vk::VkCommandBuffer commandBuffer)
{
    const vk::VkImage resultImage = m_isInputAttachment ? m_resources.m_resultAttachmentImage->get() : VK_NULL_HANDLE;
    const vk::VkImageSubresourceRange subresourceRange = vk::makeImageSubresourceRange(
        vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, VK_REMAINING_MIP_LEVELS, 0u, VK_REMAINING_ARRAY_LAYERS);
    const vk::VkImageMemoryBarrier imageBarriers[] = {
        {
            vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,   // VkStructureType            sType
            nullptr,                                      // const void*                pNext
            vk::VK_ACCESS_MEMORY_READ_BIT,                // VkAccessFlags            srcAccessMask
            vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,     // VkAccessFlags            dstAccessMask
            vk::VK_IMAGE_LAYOUT_UNDEFINED,                // VkImageLayout            oldLayout
            vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout            newLayout
            VK_QUEUE_FAMILY_IGNORED,                      // uint32_t                    srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                      // uint32_t                    destQueueFamilyIndex
            m_resources.m_androidExternalImage.get(),     // VkImage                    image
            subresourceRange                              // VkImageSubresourceRange    subresourceRange
        },
        {
            vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,   // VkStructureType            sType
            nullptr,                                      // const void*                pNext
            vk::VK_ACCESS_MEMORY_READ_BIT,                // VkAccessFlags            srcAccessMask
            vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,     // VkAccessFlags            dstAccessMask
            vk::VK_IMAGE_LAYOUT_UNDEFINED,                // VkImageLayout            oldLayout
            vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout            newLayout
            VK_QUEUE_FAMILY_IGNORED,                      // uint32_t                    srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                      // uint32_t                    destQueueFamilyIndex
            m_nullColorAttachment ? resultImage           // VkImage                    image
                                    :
                                    m_resources.m_androidColorAttachmentImage->get(),
            subresourceRange // VkImageSubresourceRange    subresourceRange
        },
        {
            vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,   // VkStructureType            sType
            nullptr,                                      // const void*                pNext
            vk::VK_ACCESS_MEMORY_READ_BIT,                // VkAccessFlags            srcAccessMask
            vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,     // VkAccessFlags            dstAccessMask
            vk::VK_IMAGE_LAYOUT_UNDEFINED,                // VkImageLayout            oldLayout
            vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout            newLayout
            VK_QUEUE_FAMILY_IGNORED,                      // uint32_t                    srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                      // uint32_t                    destQueueFamilyIndex
            resultImage,                                  // VkImage                    image
            subresourceRange                              // VkImageSubresourceRange    subresourceRange
        },
    };
    uint32_t barrierCount = m_nullColorAttachment ? 1u : 2u;
    barrierCount += m_isInputAttachment ? 1u : 0u;

    vk.cmdPipelineBarrier(commandBuffer, vk::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                          vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, barrierCount,
                          imageBarriers);
}

void AhbExternalFormatResolveTestInstance::transitionInputAttachmentToOutput(const vk::DeviceInterface &vk,
                                                                             vk::VkCommandBuffer commandBuffer)
{
    const vk::VkImageSubresourceRange subresourceRange = vk::makeImageSubresourceRange(
        vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, VK_REMAINING_MIP_LEVELS, 0u, VK_REMAINING_ARRAY_LAYERS);
    const vk::VkImageMemoryBarrier imageBarrier = {
        vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,   // VkStructureType            sType
        nullptr,                                      // const void*                pNext
        vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,     // VkAccessFlags            srcAccessMask
        vk::VK_ACCESS_MEMORY_WRITE_BIT,               // VkAccessFlags            dstAccessMask
        vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,     // VkImageLayout            oldLayout
        vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout            newLayout
        VK_QUEUE_FAMILY_IGNORED,                      // uint32_t                    srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                      // uint32_t                    destQueueFamilyIndex
        m_resources.m_resultAttachmentImage->get(),   // VkImage                    image
        subresourceRange                              // VkImageSubresourceRange    subresourceRange
    };
    vk.cmdPipelineBarrier(commandBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &imageBarrier);
}

bool AhbExternalFormatResolveTestInstance::checkExternalFormatTestingRequired(
    const AndroidHardwareBufferInstance &androidBuffer)
{
    const vk::DeviceInterface &vk = m_context.getDeviceInterface();
    const vk::VkDevice device     = m_context.getDevice();

    vk::VkAndroidHardwareBufferFormatResolvePropertiesANDROID formatResolveProperties = {
        vk::VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_RESOLVE_PROPERTIES_ANDROID, // VkStructureType    sType
        nullptr,                                                                         // void*            pNext
        vk::VK_FORMAT_UNDEFINED // VkFormat            colorAttachmentFormat
    };

    vk::VkAndroidHardwareBufferFormatPropertiesANDROID formatProperties = {
        vk::VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID, // VkStructureType                    sType
        &formatResolveProperties, // void*                            pNext
        vk::VK_FORMAT_UNDEFINED,  // VkFormat                            format
        0u,                       // uint64_t                            externalFormat
        0u,                       // VkFormatFeatureFlags                formatFeatures
        vk::VkComponentMapping(), // VkComponentMapping                samplerYcbcrConversionComponents
        vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY, // VkSamplerYcbcrModelConversion    suggestedYcbcrModel
        vk::VK_SAMPLER_YCBCR_RANGE_ITU_FULL,                // VkSamplerYcbcrRange                suggestedYcbcrRange
        vk::VK_CHROMA_LOCATION_COSITED_EVEN, // VkChromaLocation                    suggestedXChromaOffset
        vk::VK_CHROMA_LOCATION_COSITED_EVEN  // VkChromaLocation                    suggestedYChromaOffset
    };

    vk::VkAndroidHardwareBufferPropertiesANDROID bufferProperties = {
        vk::VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID, // VkStructureType    sType
        &formatProperties,                                                // void*            pNext
        0u,                                                               // VkDeviceSize        allocationSize
        0u                                                                // uint32_t            memoryTypeBits
    };

    VK_CHECK(vk.getAndroidHardwareBufferPropertiesANDROID(device, androidBuffer.getHandle(), &bufferProperties));

    if (formatProperties.format != vk::VK_FORMAT_UNDEFINED)
    {
        vk::VkFormatProperties3 colorAttachmentFormatProperties =
            m_context.getFormatProperties(formatProperties.format);
        vk::VkFormatFeatureFlags requiredFlags =
            vk::VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | vk::VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

        if ((colorAttachmentFormatProperties.optimalTilingFeatures & requiredFlags) ||
            (colorAttachmentFormatProperties.linearTilingFeatures & requiredFlags))
            return false;
    }

    // Ensure there's draw support
    if (formatResolveProperties.colorAttachmentFormat == vk::VK_FORMAT_UNDEFINED)
        TCU_THROW(TestError, "No draw support");

    {
        vk::VkFormatProperties3 colorAttachmentFormatProperties =
            m_context.getFormatProperties(formatResolveProperties.colorAttachmentFormat);

        // External formats require optimal tiling
        if ((colorAttachmentFormatProperties.optimalTilingFeatures & vk::VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0u)
            TCU_THROW(TestError, "No draw support");
    }

    m_retrievedInternalFormat = formatProperties.externalFormat;

    {
        const vk::InstanceInterface &vki = m_context.getInstanceInterface();
        vk::VkPhysicalDeviceExternalFormatResolvePropertiesANDROID externalFormatProperties = {
            vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FORMAT_RESOLVE_PROPERTIES_ANDROID, // VkStructureType    sType
            nullptr,                                                                          // void*            pNext
            VK_FALSE,                        // VkBool32            nullColorAttachmentWithExternalFormatResolve
            vk::VK_CHROMA_LOCATION_MIDPOINT, // VkChromaLocation    externalFormatResolveChromaOffsetX
            vk::VK_CHROMA_LOCATION_MIDPOINT  // VkChromaLocation    externalFormatResolveChromaOffsetY
        };

        vk::VkPhysicalDeviceProperties2 physicalDeviceProperties = {
            vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, // VkStructureType                sType
            &externalFormatProperties,                          // void*                        pNext
            vk::VkPhysicalDeviceProperties{}                    // VkPhysicalDeviceProperties    properties
        };

        vki.getPhysicalDeviceProperties2(m_context.getPhysicalDevice(), &physicalDeviceProperties);

        m_nullColorAttachment   = externalFormatProperties.nullColorAttachmentWithExternalFormatResolve;
        m_colorAttachmentFormat = formatResolveProperties.colorAttachmentFormat;
        m_xChromaLocation       = externalFormatProperties.externalFormatResolveChromaOffsetX;
        m_yChromaLocation       = externalFormatProperties.externalFormatResolveChromaOffsetY;
    }

    if (m_isInputAttachment && (m_nullColorAttachment == VK_FALSE) &&
        (formatProperties.formatFeatures & vk::VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0u)
        TCU_THROW(NotSupportedError, "Format lacks input attachment usage: nullColorAttachment is VK_FALSE and format "
                                     "does not support VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT");

    // Need to fetch correct max clear value since it'll depend on each format
    const tcu::Vec4 formatMaxValue = tcu::getTextureFormatInfo(mapVkFormat(m_colorAttachmentFormat)).valueMax;
    m_clearColor[0]                = formatMaxValue[0] * 0.5f;
    m_clearColor[1]                = formatMaxValue[0];
    m_clearColor[3]                = formatMaxValue[3];

    return true;
}

void AhbExternalFormatResolveTestInstance::buildReferenceImage(tcu::TextureLevel &texture, bool performDownsample,
                                                               bool ahbFormatVulkanFormatAlphaMismatch) const
{
    tcu::PixelBufferAccess access  = texture.getAccess();
    const tcu::Vec4 formatMaxValue = tcu::getTextureFormatInfo(texture.getFormat()).valueMax;
    tcu::Vec4 colors[4]            = {
        // Modify alpha value to match output if original AHB format does not contain alpha
        tcu::Vec4(0.0f, 0.0f, 0.0f, (ahbFormatVulkanFormatAlphaMismatch ? formatMaxValue.w() : 0.0f)), // black
        tcu::Vec4(formatMaxValue.x(), 0.0f, 0.0f, formatMaxValue.w()),                                 // red
        tcu::Vec4(0.0f, formatMaxValue.y(), 0.0f, formatMaxValue.w()),        // green
        tcu::Vec4(0.0f, 0.0f, formatMaxValue.z() * 0.5f, formatMaxValue.w()), // blue
    };

    tcu::IVec2 renderAreaStart(m_renderArea.offset.x, m_renderArea.offset.y);
    tcu::IVec2 renderAreaEnd(renderAreaStart.x() + m_renderArea.extent.width,
                             renderAreaStart.y() + m_renderArea.extent.height);

    uint32_t colorIndex = 0u;
    for (uint32_t y = 0u; y < m_height; ++y, colorIndex ^= 2u)
    {
        for (uint32_t x = 0u; x < m_width; ++x, colorIndex ^= 1u)
        {
            if (m_isClearOnly)
                access.setPixel(m_clearColor, x, y);
            else
            {
                bool isInsideRenderArea =
                    ((renderAreaStart.x() <= static_cast<int32_t>(x)) &&
                     (static_cast<int32_t>(x) < renderAreaEnd.x())) &&
                    ((renderAreaStart.y() <= static_cast<int32_t>(y)) && (static_cast<int32_t>(y) < renderAreaEnd.y()));

                if (isInsideRenderArea)
                    access.setPixel(colors[colorIndex], x, y);
                else
                    access.setPixel(m_clearColor, x, y);
            }
        }
    }

    if (performDownsample)
    {
        // Reduce reference image according to chroma locations
        AndroidHardwareBufferInstance::ChromaLocation xLocation =
            AndroidHardwareBufferInstance::vkChromaLocationToChromaLocation(m_xChromaLocation);
        AndroidHardwareBufferInstance::ChromaLocation yLocation =
            AndroidHardwareBufferInstance::vkChromaLocationToChromaLocation(m_yChromaLocation);
        AndroidHardwareBufferInstance::reduceYuvTexture(texture, m_format, xLocation, yLocation);
    }
}

Move<VkImageView> AhbExternalFormatResolveTestInstance::createImageView(VkImage image, vk::VkFormat format)
{
    const vk::DeviceInterface &vk              = m_context.getDeviceInterface();
    const vk::VkDevice device                  = m_context.getDevice();
    const vk::VkImageViewCreateInfo createInfo = {
        vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,                // VkStructureType            sType
        nullptr,                                                     // const void*                pNext
        0u,                                                          // VkImageViewCreateFlags    flags
        image,                                                       // VkImage                    image
        vk::VK_IMAGE_VIEW_TYPE_2D,                                   // VkImageViewType            viewType
        format,                                                      // VkFormat                    format
        makeComponentMappingIdentity(),                              // VkComponentMapping        components
        vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageSubresourceRange    subresourceRange
                                      0u, VK_REMAINING_MIP_LEVELS, 0u, VK_REMAINING_ARRAY_LAYERS)};

    return vk::createImageView(vk, device, &createInfo);
}

void AhbExternalFormatResolveTestInstance::createImagesAndViews(AndroidHardwareBufferInstance &androidBuffer)
{
    const vk::DeviceInterface &vk   = m_context.getDeviceInterface();
    const vk::VkDevice device       = m_context.getDevice();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    // Import android hardware buffer to Vulkan
    {
        // Create VkImage
        {
            vk::VkExternalFormatANDROID externalFormat = {
                vk::VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID, // VkStructureType    sType
                nullptr,                                       // void*            pNext
                m_retrievedInternalFormat                      // uint64_t            externalFormat
            };

            const vk::VkExternalMemoryImageCreateInfo externalCreateInfo = {
                vk::VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO, // VkStructureType    sType
                &externalFormat,                                         // const void*    pNext
                vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID // VkExternalMemoryHandleTypeFlags    handleTypes
            };

            vk::VkImageUsageFlags usage =
                (m_nullColorAttachment && m_isInputAttachment) ?
                    (vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) :
                    vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            const vk::VkImageCreateInfo createInfo = {
                vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType            sType
                &externalCreateInfo,                     // const void*                pNext
                0u,                                      // VkImageCreateFlags        flags
                vk::VK_IMAGE_TYPE_2D,                    // VkImageType                imageType
                vk::VK_FORMAT_UNDEFINED,                 // VkFormat                    format
                {
                    m_width,
                    m_height,
                    1u,
                },                             // VkExtent3D                extent
                1u,                            // uint32_t                    mipLevels
                m_layers,                      // uint32_t                    arrayLayers
                vk::VK_SAMPLE_COUNT_1_BIT,     // VkSampleCountFlagBits    samples
                vk::VK_IMAGE_TILING_OPTIMAL,   // VkImageTiling            tiling
                usage,                         // VkImageUsageFlags        usage
                vk::VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode            sharingMode
                1,                             // uint32_t                    queueFamilyIndexCount
                &queueFamilyIndex,             // const uint32_t*            pQueueFamilyIndices
                vk::VK_IMAGE_LAYOUT_UNDEFINED  // VkImageLayout            initialLayout
            };

            m_resources.m_androidExternalImage = vk::createImage(vk, device, &createInfo);
        }

        // Allocate VkDeviceMemory
        {
            vk::VkAndroidHardwareBufferPropertiesANDROID ahbProperties = {
                vk::VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID, // VkStructureType    sType
                DE_NULL,                                                          // void*            pNext
                0u,                                                               // VkDeviceSize        allocationSize
                0u                                                                // uint32_t            memoryTypeBits
            };

            vk.getAndroidHardwareBufferPropertiesANDROID(device, androidBuffer.getHandle(), &ahbProperties);

            const vk::VkImportAndroidHardwareBufferInfoANDROID importInfo = {
                vk::VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID, // VkStructureType            sType
                DE_NULL,                                                           // const void*                pNext
                androidBuffer.getHandle()                                          // struct AHardwareBuffer*    buffer
            };

            const vk::VkMemoryDedicatedAllocateInfo dedicatedInfo = {
                vk::VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR, // VkStructureType    sType
                &importInfo,                                              // const void*        pNext
                m_resources.m_androidExternalImage.get(),                 // VkImage            image
                DE_NULL,                                                  // VkBuffer            buffer
            };

            const vk::VkMemoryAllocateInfo allocateInfo = {
                vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,    // VkStructureType    sType
                (const void *)&dedicatedInfo,                  // const void*        pNext
                ahbProperties.allocationSize,                  // VkDeviceSize        allocationSize
                chooseMemoryType(ahbProperties.memoryTypeBits) // uint32_t            memoryTypeIndex
            };

            m_resources.m_androidExternalImageMemory = vk::allocateMemory(vk, device, &allocateInfo);
        }

        // Bind
        VK_CHECK(vk.bindImageMemory(device, m_resources.m_androidExternalImage.get(),
                                    m_resources.m_androidExternalImageMemory.get(), 0u));

        // Create view
        m_resources.m_androidExternalImageView =
            createImageView(m_resources.m_androidExternalImage.get(), vk::VK_FORMAT_UNDEFINED);
    }

    vk::VkImageCreateInfo imageCreateInfo = {
        vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType            sType
        nullptr,                                 // const void*                pNext
        0u,                                      // VkImageCreateFlags        flags
        vk::VK_IMAGE_TYPE_2D,                    // VkImageType                imageType
        m_colorAttachmentFormat,                 // VkFormat                    format
        {
            m_width,
            m_height,
            1u,
        },                                       // VkExtent3D                extent
        1u,                                      // uint32_t                    mipLevels
        m_layers,                                // uint32_t                    arrayLayers
        vk::VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits    samples
        vk::VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling            tiling
        vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // VkImageUsageFlags        usage
        vk::VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode            sharingMode
        1,                                       // uint32_t                    queueFamilyIndexCount
        &queueFamilyIndex,                       // const uint32_t*            pQueueFamilyIndices
        vk::VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout            initialLayout
    };

    if (m_isInputAttachment)
    {
        imageCreateInfo.usage |= vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        m_resources.m_resultAttachmentImage = de::MovePtr<ImageWithMemory>(new ImageWithMemory(
            vk, device, m_context.getDefaultAllocator(), imageCreateInfo, vk::MemoryRequirement::Any));
        m_resources.m_resultAttachmentImageView =
            createImageView(m_resources.m_resultAttachmentImage->get(), m_colorAttachmentFormat);

        VkDeviceSize bufferSize = m_width * m_height * mapVkFormat(m_colorAttachmentFormat).getPixelSize();
        vk::VkBufferCreateInfo bufferCreateInfo = {
            vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType        sType
            nullptr,                                  // const void*            pNext
            0u,                                       // VkBufferCreateFlags    flags
            bufferSize,                               // VkDeviceSize            size
            vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT,     // VkBufferUsageFlags    usage
            vk::VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode        sharingMode
            1u,                                       // uint32_t                queueFamilyIndexCount
            &queueFamilyIndex                         // const uint32_t*        pQueueFamilyIndices
        };
        m_resources.m_resultBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
            vk, device, m_context.getDefaultAllocator(), bufferCreateInfo, vk::MemoryRequirement::HostVisible));
    }

    if (m_nullColorAttachment == VK_FALSE)
    {
        imageCreateInfo.usage |=
            m_isInputAttachment ? (vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) :
                                  vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        m_resources.m_androidColorAttachmentImage = de::MovePtr<ImageWithMemory>(new ImageWithMemory(
            vk, device, m_context.getDefaultAllocator(), imageCreateInfo, vk::MemoryRequirement::Any));
        m_resources.m_androidColorAttachmentImageView =
            createImageView(m_resources.m_androidColorAttachmentImage->get(), m_colorAttachmentFormat);
    }
}

void AhbExternalFormatResolveTestInstance::createRenderPass(void)
{
    if (m_groupParams.useDynamicRendering)
        return;

    const vk::DeviceInterface &vk = m_context.getDeviceInterface();
    const vk::VkDevice device     = m_context.getDevice();

    vk::VkExternalFormatANDROID externalFormat = {
        vk::VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID, // VkStructureType    sType
        nullptr,                                       // void*            pNext
        m_retrievedInternalFormat                      // uint64_t            externalFormat
    };

    vk::VkAttachmentDescription2 attachments[] = {
        // Resolve attachment
        {
            vk::VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2, // VkStructureType                    sType
            &externalFormat,                                // const void*                        pNext
            0u,                                             // VkAttachmentDescriptionFlags        flags
            vk::VK_FORMAT_UNDEFINED,                        // VkFormat                            format
            vk::VK_SAMPLE_COUNT_1_BIT,                      // VkSampleCountFlagBits            samples
            vk::VK_ATTACHMENT_LOAD_OP_CLEAR,                // VkAttachmentLoadOp                loadOp
            vk::VK_ATTACHMENT_STORE_OP_STORE,               // VkAttachmentStoreOp                storeOp
            vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,            // VkAttachmentLoadOp                stencilLoadOp
            vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,           // VkAttachmentStoreOp                stencilStoreOp
            vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,   // VkImageLayout                    initialLayout
            vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL    // VkImageLayout                    finalLayout
        },
        // Color attachment
        {
            vk::VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2, // VkStructureType                    sType
            nullptr,                                        // const void*                        pNext
            0u,                                             // VkAttachmentDescriptionFlags        flags
            m_colorAttachmentFormat,                        // VkFormat                            format
            vk::VK_SAMPLE_COUNT_1_BIT,                      // VkSampleCountFlagBits            samples
            vk::VK_ATTACHMENT_LOAD_OP_CLEAR,                // VkAttachmentLoadOp                loadOp
            vk::VK_ATTACHMENT_STORE_OP_STORE,               // VkAttachmentStoreOp                storeOp
            vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,            // VkAttachmentLoadOp                stencilLoadOp
            vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,           // VkAttachmentStoreOp                stencilStoreOp
            vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,   // VkImageLayout                    initialLayout
            vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL    // VkImageLayout                    finalLayout
        },
        // Final attachment, only present when input attachment testing and nullColorAttachment is false
        {
            vk::VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2, // VkStructureType                    sType
            nullptr,                                        // const void*                        pNext
            0u,                                             // VkAttachmentDescriptionFlags        flags
            m_colorAttachmentFormat,                        // VkFormat                            format
            vk::VK_SAMPLE_COUNT_1_BIT,                      // VkSampleCountFlagBits            samples
            vk::VK_ATTACHMENT_LOAD_OP_CLEAR,                // VkAttachmentLoadOp                loadOp
            vk::VK_ATTACHMENT_STORE_OP_STORE,               // VkAttachmentStoreOp                storeOp
            vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,            // VkAttachmentLoadOp                stencilLoadOp
            vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,           // VkAttachmentStoreOp                stencilStoreOp
            vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,   // VkImageLayout                    initialLayout
            vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL    // VkImageLayout                    finalLayout
        }};

    const vk::VkAttachmentReference2 resolveAttachmentReference = {
        vk::VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, // VkStructureType        sType
        nullptr,                                      // const void*            pNext
        0u,                                           // uint32_t                attachment
        vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout        layout
        vk::VK_IMAGE_ASPECT_COLOR_BIT                 // VkImageAspectFlags    aspectMask
    };

    const vk::VkAttachmentReference2 colorAttachmentReference = {
        vk::VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,      // VkStructureType        sType
        nullptr,                                           // const void*            pNext
        m_nullColorAttachment ? VK_ATTACHMENT_UNUSED : 1u, // uint32_t                attachment
        vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,      // VkImageLayout        layout
        vk::VK_IMAGE_ASPECT_COLOR_BIT                      // VkImageAspectFlags    aspectMask
    };

    const vk::VkAttachmentReference2 finalAttachmentReference = {
        vk::VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, // VkStructureType        sType
        nullptr,                                      // const void*            pNext
        m_nullColorAttachment ? 1u : 2u,              // uint32_t                attachment
        vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout        layout
        vk::VK_IMAGE_ASPECT_COLOR_BIT                 // VkImageAspectFlags    aspectMask
    };

    const vk::VkAttachmentReference2 inputAttachmentReference = {
        vk::VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, // VkStructureType        sType
        nullptr,                                      // const void*            pNext
        m_nullColorAttachment ? 0u : 1u,              // uint32_t                attachment
        vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // VkImageLayout        layout
        vk::VK_IMAGE_ASPECT_COLOR_BIT                 // VkImageAspectFlags    aspectMask
    };

    const vk::VkSubpassDescription2 subpassDescriptions[] = {
        // Subpass 0
        {
            vk::VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2, // VkStructureType                    sType
            nullptr,                                     // const void*                        pNext
            0u,                                          // VkSubpassDescriptionFlags        flags
            vk::VK_PIPELINE_BIND_POINT_GRAPHICS,         // VkPipelineBindPoint                pipelineBindPoint
            0u,                                          // uint32_t                            viewMask
            0u,                                          // uint32_t                            inputAttachmentCount
            nullptr,                                     // const VkAttachmentReference2*    pInputAttachments
            1u,                                          // uint32_t                            colorAttachmentCount
            &colorAttachmentReference,                   // const VkAttachmentReference2*    pColorAttachments
            &resolveAttachmentReference,                 // const VkAttachmentReference2*    pResolveAttachments
            nullptr,                                     // const VkAttachmentReference2*    pDepthStencilAttachment
            0u,                                          // uint32_t                            preserveAttachmentCount
            nullptr                                      // const uint32_t*                    pPreserveAttachments
        },
        // Subpass 1
        {
            vk::VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2, // VkStructureType                    sType
            nullptr,                                     // const void*                        pNext
            0u,                                          // VkSubpassDescriptionFlags        flags
            vk::VK_PIPELINE_BIND_POINT_GRAPHICS,         // VkPipelineBindPoint                pipelineBindPoint
            0u,                                          // uint32_t                            viewMask
            m_isInputAttachment ? 1u : 0u,               // uint32_t                            inputAttachmentCount
            m_isInputAttachment ? &inputAttachmentReference :
                                  nullptr, // const VkAttachmentReference2*    pInputAttachments
            1u,                            // uint32_t                            colorAttachmentCount
            &finalAttachmentReference,     // const VkAttachmentReference2*    pColorAttachments
            nullptr,                       // const VkAttachmentReference2*    pResolveAttachments
            nullptr,                       // const VkAttachmentReference2*    pDepthStencilAttachment
            0u,                            // uint32_t                            preserveAttachmentCount
            nullptr                        // const uint32_t*                    pPreserveAttachments
        }};

    vk::VkSubpassDependency2 subpassDependencies[] = {
        {
            vk::VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,        // VkStructureType        sType
            nullptr,                                           // const void*            pNext
            VK_SUBPASS_EXTERNAL,                               // uint32_t                srcSubpass
            0u,                                                // uint32_t                dstSubpass
            vk::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,          // VkPipelineStageFlags    srcStageMask
            vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // VkPipelineStageFlags    dstStageMask
            vk::VK_ACCESS_MEMORY_READ_BIT,                     // VkAccessFlags        srcAccessMask
            vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // VkAccessFlags        dstAccessMask
            vk::VK_DEPENDENCY_BY_REGION_BIT,                   // VkDependencyFlags    dependencyFlags
            0                                                  // int32_t                viewOffset
        },
        {
            vk::VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,        // VkStructureType        sType
            nullptr,                                           // const void*            pNext
            0u,                                                // uint32_t                srcSubpass
            1u,                                                // uint32_t                dstSubpass
            vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // VkPipelineStageFlags    srcStageMask
            vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,         // VkPipelineStageFlags    dstStageMask
            vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // VkAccessFlags        srcAccessMask
            vk::VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,           // VkAccessFlags        dstAccessMask
            vk::VK_DEPENDENCY_BY_REGION_BIT,                   // VkDependencyFlags    dependencyFlags
            0                                                  // int32_t                viewOffset
        }};

    uint32_t attachmentCount = 1u;
    attachmentCount += m_nullColorAttachment ? 0u : 1u;

    if (m_isInputAttachment)
    {
        attachments[attachmentCount].finalLayout = vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        attachmentCount += 1u;
    }
    const vk::VkRenderPassCreateInfo2 renderPassCreateInfo = {
        vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2, // VkStructureType                    sType
        nullptr,                                         // const void*                        pNext
        0u,                                              // VkRenderPassCreateFlags            flags
        attachmentCount,                                 // uint32_t                            attachmentCount
        attachments,                                     // const VkAttachmentDescription2*    pAttachments
        m_isInputAttachment ? 2u : 1u,                   // uint32_t                            subpassCount
        subpassDescriptions,                             // const VkSubpassDescription2*        pSubpasses
        m_isInputAttachment ? 2u : 1u,                   // uint32_t                            dependencyCount
        subpassDependencies,                             // const VkSubpassDependency2*        pDependencies
        0u,                                              // uint32_t                            correlatedViewMaskCount
        nullptr                                          // const uint32_t*                    pCorrelatedViewMasks
    };

    // Render pass in charge of clearing
    m_resources.m_renderPassClear = vk::createRenderPass2(vk, device, &renderPassCreateInfo);

    // Draw render pass with load operation
    attachments[0].loadOp    = vk::VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[1].loadOp    = vk::VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[2].loadOp    = vk::VK_ATTACHMENT_LOAD_OP_LOAD;
    m_resources.m_renderPass = vk::createRenderPass2(vk, device, &renderPassCreateInfo);
}

void AhbExternalFormatResolveTestInstance::createFramebuffer(void)
{
    if (m_groupParams.useDynamicRendering)
        return;

    const vk::DeviceInterface &vk = m_context.getDeviceInterface();
    const vk::VkDevice device     = m_context.getDevice();

    std::vector<VkImageView> imageViews;
    imageViews.emplace_back(m_resources.m_androidExternalImageView.get());
    if (m_nullColorAttachment == VK_FALSE)
        imageViews.emplace_back(m_resources.m_androidColorAttachmentImageView.get());
    if (m_isInputAttachment)
        imageViews.emplace_back(m_resources.m_resultAttachmentImageView.get());

    vk::VkFramebufferCreateInfo createInfo = {
        vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType            sType
        nullptr,                                       // const void*                pNext
        0u,                                            // VkFramebufferCreateFlags    flags
        m_resources.m_renderPassClear.get(),           // VkRenderPass                renderPass
        static_cast<uint32_t>(imageViews.size()),      // uint32_t                    attachmentCount
        imageViews.data(),                             // const VkImageView*        pAttachments
        m_width,                                       // uint32_t                    width
        m_height,                                      // uint32_t                    height
        m_layers                                       // uint32_t                    layers
    };

    m_resources.m_framebufferClear = vk::createFramebuffer(vk, device, &createInfo);

    createInfo.renderPass     = m_resources.m_renderPass.get();
    m_resources.m_framebuffer = vk::createFramebuffer(vk, device, &createInfo);
}

void AhbExternalFormatResolveTestInstance::createDescriptors(void)
{
    // Only needed when input attachment testing is happening
    if (!m_isInputAttachment)
        return;

    const vk::DeviceInterface &vk = m_context.getDeviceInterface();
    const vk::VkDevice device     = m_context.getDevice();

    const vk::VkDescriptorPoolSize poolSize = {
        vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, // VkDescriptorType    type
        1u                                       // uint32_t            descriptorCount
    };

    // VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT added so we can destroy descriptors with Move<>
    const vk::VkDescriptorPoolCreateInfo poolCreateInfo = {
        vk::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, // VkStructureType                sType
        nullptr,                                           // const void*                    pNext
        VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // VkDescriptorPoolCreateFlags    flags
        1u,                                                // uint32_t                        maxSets
        1u,                                                // uint32_t                        poolSizeCount
        &poolSize                                          // const VkDescriptorPoolSize*    pPoolSizes
    };

    m_resources.m_descriptorPool = vk::createDescriptorPool(vk, device, &poolCreateInfo);

    const vk::VkDescriptorSetLayoutBinding binding = {
        0u,                                      // uint32_t                binding
        vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, // VkDescriptorType        descriptorType
        1u,                                      // uint32_t                descriptorCount
        vk::VK_SHADER_STAGE_FRAGMENT_BIT,        // VkShaderStageFlags    stageFlags
        nullptr,                                 // const VkSampler*        pImmutableSamplers
    };

    const vk::VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo = {
        vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // VkStructureType                        sType
        nullptr,                                                 // const void*                            pNext
        0u,                                                      // VkDescriptorSetLayoutCreateFlags        flags
        1u,                                                      // uint32_t                                bindingCount
        &binding                                                 // const VkDescriptorSetLayoutBinding*    pBindings
    };

    m_resources.m_descriptorSetLayout = vk::createDescriptorSetLayout(vk, device, &setLayoutCreateInfo);

    const vk::VkDescriptorSetAllocateInfo allocateInfo = {
        vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType                sType
        nullptr,                                            // const void*                    pNext
        m_resources.m_descriptorPool.get(),                 // VkDescriptorPool                descriptorPool
        1u,                                                 // uint32_t                        descriptorSetCount
        &m_resources.m_descriptorSetLayout.get()            // const VkDescriptorSetLayout*    pSetLayouts
    };

    m_resources.m_descriptorSet = vk::allocateDescriptorSet(vk, device, &allocateInfo);

    const vk::VkDescriptorImageInfo imageInfo = {
        VK_NULL_HANDLE,                                                      // VkSampler        sampler
        m_nullColorAttachment ? m_resources.m_androidExternalImageView.get() // VkImageView        imageView
                                :
                                m_resources.m_androidColorAttachmentImageView.get(),
        vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL // VkImageLayout    imageLayout
    };

    const vk::VkWriteDescriptorSet descriptorWrite = {
        vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType                    sType
        nullptr,                                    // const void*                        pNext
        m_resources.m_descriptorSet.get(),          // VkDescriptorSet                    dstSet
        0u,                                         // uint32_t                            dstBinding
        0u,                                         // uint32_t                            dstArrayElement
        1u,                                         // uint32_t                            descriptorCount
        vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,    // VkDescriptorType                    descriptorType
        &imageInfo,                                 // const VkDescriptorImageInfo*        pImageInfo
        nullptr,                                    // const VkDescriptorBufferInfo*    pBufferInfo
        nullptr                                     // const VkBufferView*                pTexelBufferView
    };

    vk.updateDescriptorSets(device, 1u, &descriptorWrite, 0u, nullptr);
}

void AhbExternalFormatResolveTestInstance::createPipelineLayouts(void)
{
    const vk::DeviceInterface &vk = m_context.getDeviceInterface();
    const vk::VkDevice device     = m_context.getDevice();

    vk::VkPipelineLayoutCreateInfo createInfo = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType                sType
        nullptr,                                           // const void*                    pNext
        0u,                                                // VkPipelineLayoutCreateFlags    flags
        0u,                                                // uint32_t                        setLayoutCount
        nullptr,                                           // const VkDescriptorSetLayout*    pSetLayouts
        0u,                                                // uint32_t                        pushConstantRangeCount
        nullptr                                            // const VkPushConstantRange*    pPushConstantRanges
    };

    m_resources.m_basePipelineLayout = vk::createPipelineLayout(vk, device, &createInfo);

    if (m_isInputAttachment)
    {
        createInfo.setLayoutCount                   = 1u;
        createInfo.pSetLayouts                      = &m_resources.m_descriptorSetLayout.get();
        m_resources.m_inputAttachmentPipelineLayout = vk::createPipelineLayout(vk, device, &createInfo);
    }
}

void AhbExternalFormatResolveTestInstance::createPipelines(void)
{
    const vk::DeviceInterface &vk = m_context.getDeviceInterface();
    const vk::VkDevice device     = m_context.getDevice();

    vk::VkPipelineShaderStageCreateInfo stages[] = {
        // vertex
        {
            vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType                    sType
            nullptr,                                                 // const void*                        pNext
            0u,                                                      // VkPipelineShaderStageCreateFlags    flags
            vk::VK_SHADER_STAGE_VERTEX_BIT,                          // VkShaderStageFlagBits            stage
            m_resources.m_vertexShader.get(),                        // VkShaderModule                    module
            "main",                                                  // const char*                        pName
            nullptr, // const VkSpecializationInfo*        pSpecializationInfo
        },
        // fragment
        {
            vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType                    sType
            nullptr,                                                 // const void*                        pNext
            0u,                                                      // VkPipelineShaderStageCreateFlags    flags
            vk::VK_SHADER_STAGE_FRAGMENT_BIT,                        // VkShaderStageFlagBits            stage
            m_resources.m_fragmentShaderBase.get(),                  // VkShaderModule                    module
            "main",                                                  // const char*                        pName
            nullptr, // const VkSpecializationInfo*        pSpecializationInfo
        }};

    const vk::VkVertexInputBindingDescription vertexInputBindDesc = {
        0u,                         //    uint32_t            binding
        sizeof(float) * 2,          //    uint32_t            stride
        VK_VERTEX_INPUT_RATE_VERTEX //    VkVertexInputRate    inputRate
    };

    const vk::VkVertexInputAttributeDescription vertexInputAttrDesc = {
        0u,                          // uint32_t    location
        0u,                          // uint32_t    binding
        vk::VK_FORMAT_R32G32_SFLOAT, // VkFormat    format
        0u                           // uint32_t    offset
    };

    const vk::VkPipelineVertexInputStateCreateInfo vertexInputState = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                            sType
        nullptr,              // const void*                                pNext
        0u,                   // VkPipelineVertexInputStateCreateFlags    flags
        1u,                   // uint32_t                                    vertexBindingDescriptionCount
        &vertexInputBindDesc, // const VkVertexInputBindingDescription*    pVertexBindingDescriptions
        1u,                   // uint32_t                                    vertexAttributeDescriptionCount
        &vertexInputAttrDesc  // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
    };

    const vk::VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType                            sType
        nullptr,                                  // const void*                                pNext
        0u,                                       // VkPipelineInputAssemblyStateCreateFlags    flags
        vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // VkPrimitiveTopology                        topology
        VK_FALSE                                  // VkBool32                                    primitiveRestartEnable
    };

    const vk::VkViewport viewport = makeViewport(m_width, m_height);

    const vk::VkPipelineViewportStateCreateInfo viewportState = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType                        sType
        nullptr,                                                   // const void*                            pNext
        0u,                                                        // VkPipelineViewportStateCreateFlags    flags
        1u,           // uint32_t                                viewportCount
        &viewport,    // const VkViewport*                    pViewports
        1u,           // uint32_t                                scissorCount
        &m_renderArea // const VkRect2D*                        pScissors
    };

    const vk::VkPipelineRasterizationStateCreateInfo rasterState = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType                            sType
        nullptr,                             // const void*                                pNext
        0u,                                  // VkPipelineRasterizationStateCreateFlags    flags
        VK_FALSE,                            // VkBool32                                    depthClampEnable
        VK_FALSE,                            // VkBool32                                    rasterizerDiscardEnable
        vk::VK_POLYGON_MODE_FILL,            // VkPolygonMode                            polygonMode
        vk::VK_CULL_MODE_NONE,               // VkCullModeFlags                            cullMode
        vk::VK_FRONT_FACE_COUNTER_CLOCKWISE, // VkFrontFace                                frontFace
        VK_FALSE,                            // VkBool32                                    depthBiasEnable
        0.0f,                                // float                                    depthBiasConstantFactor
        0.0f,                                // float                                    depthBiasClamp
        0.0f,                                // float                                    depthBiasSlopeFactor
        1.0f                                 // float                                    lineWidth
    };

    const vk::VkPipelineMultisampleStateCreateInfo msState = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType                            sType
        nullptr,                   // const void*                                pNext
        0u,                        // VkPipelineMultisampleStateCreateFlags    flags
        vk::VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits                    rasterizationSamples
        VK_FALSE,                  // VkBool32                                    sampleShadingEnable
        0.0f,                      // float                                    minSampleShading
        nullptr,                   // const VkSampleMask*                        pSampleMask
        VK_FALSE,                  // VkBool32                                    alphaToCoverageEnable
        VK_FALSE                   // VkBool32                                    alphaToOneEnable
    };

    const vk::VkColorComponentFlags colorFlags = vk::VK_COLOR_COMPONENT_R_BIT | vk::VK_COLOR_COMPONENT_G_BIT |
                                                 vk::VK_COLOR_COMPONENT_B_BIT | vk::VK_COLOR_COMPONENT_A_BIT;
    const vk::VkPipelineColorBlendAttachmentState attBlend = {
        VK_FALSE,                // VkBool32                    blendEnable
        vk::VK_BLEND_FACTOR_ONE, // VkBlendFactor            srcColorBlendFactor
        vk::VK_BLEND_FACTOR_ONE, // VkBlendFactor            dstColorBlendFactor
        vk::VK_BLEND_OP_ADD,     // VkBlendOp                colorBlendOp
        vk::VK_BLEND_FACTOR_ONE, // VkBlendFactor            srcAlphaBlendFactor
        vk::VK_BLEND_FACTOR_ONE, // VkBlendFactor            dstAlphaBlendFactor
        vk::VK_BLEND_OP_ADD,     // VkBlendOp                alphaBlendOp
        colorFlags               // VkColorComponentFlags    colorWriteMask
    };

    const vk::VkPipelineColorBlendStateCreateInfo blendState = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType                                sType
        nullptr,                 // const void*                                    pNext
        0u,                      // VkPipelineColorBlendStateCreateFlags            flags
        VK_FALSE,                // VkBool32                                        logicOpEnable
        vk::VK_LOGIC_OP_NO_OP,   // VkLogicOp                                    logicOp
        1u,                      // uint32_t                                        attachmentCount
        &attBlend,               // const VkPipelineColorBlendAttachmentState*    pAttachments
        {0.0f, 0.0f, 0.0f, 0.0f} // float                                        blendConstants[4]
    };

    vk::VkExternalFormatANDROID externalFormat = {
        vk::VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID, // VkStructureType    sType
        nullptr,                                       // void*            pNext
        m_retrievedInternalFormat                      // uint64_t            externalFormat
    };

    const vk::VkFormat colorAttachmentFormat =
        m_nullColorAttachment ? vk::VK_FORMAT_UNDEFINED : m_colorAttachmentFormat;
    vk::VkPipelineRenderingCreateInfo pipelineRenderingInfo = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, // VkStructureType    sType
        &externalFormat,                                      // const void*        pNext
        0u,                                                   // uint32_t            viewMask
        1u,                                                   // uint32_t            colorAttachmentCount
        &colorAttachmentFormat,                               // const VkFormat*    pColorAttachmentFormats
        vk::VK_FORMAT_UNDEFINED,                              // VkFormat            depthAttachmentFormat
        vk::VK_FORMAT_UNDEFINED                               // VkFormat            stencilAttachmentFormat
    };

    vk::VkGraphicsPipelineCreateInfo createInfo = {
        vk::VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, // VkStructureType                                    sType
        m_groupParams.useDynamicRendering ? &pipelineRenderingInfo :
                                            nullptr, // const void*                                        pNext
        0u,                                          // VkPipelineCreateFlags                            flags
        2u,                                          // uint32_t                                            stageCount
        stages,                                      // const VkPipelineShaderStageCreateInfo*            pStages
        &vertexInputState,   // const VkPipelineVertexInputStateCreateInfo*        pVertexInputState
        &inputAssemblyState, // const VkPipelineInputAssemblyStateCreateInfo*    pInputAssemblyState
        nullptr,             // const VkPipelineTessellationStateCreateInfo*        pTessellationState
        &viewportState,      // const VkPipelineViewportStateCreateInfo*            pViewportState
        &rasterState,        // const VkPipelineRasterizationStateCreateInfo*    pRasterizationState
        &msState,            // const VkPipelineMultisampleStateCreateInfo*        pMultisampleState
        nullptr,             // const VkPipelineDepthStencilStateCreateInfo*        pDepthStencilState
        &blendState,         // const VkPipelineColorBlendStateCreateInfo*        pColorBlendState
        nullptr,             // const VkPipelineDynamicStateCreateInfo*            pDynamicState
        m_resources.m_basePipelineLayout.get(), // VkPipelineLayout                                    layout
        m_resources.m_renderPass.get(),         // VkRenderPass                                        renderPass
        0u,                                     // uint32_t                                            subpass
        VK_NULL_HANDLE,                         // VkPipeline                                        basePipelineHandle
        0                                       // int32_t                                            basePipelineIndex
    };

    m_resources.m_basePipeline = vk::createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &createInfo);

    if (m_isInputAttachment)
    {
        stages[1].module                      = m_resources.m_fragmentShaderInput.get();
        createInfo.layout                     = m_resources.m_inputAttachmentPipelineLayout.get();
        createInfo.subpass                    = 1u;
        m_resources.m_inputAttachmentPipeline = vk::createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &createInfo);
    }
}

void AhbExternalFormatResolveTestInstance::beginRender(vk::VkCommandBuffer cmd, const vk::VkRect2D &renderArea,
                                                       bool clearPass) const
{
    const vk::DeviceInterface &vk = m_context.getDeviceInterface();

    if (m_groupParams.useDynamicRendering)
    {
        vk::VkRenderingFlags renderingFlags =
            m_groupParams.useSecondaryCmdBuffer &&
                    !m_groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass ?
                static_cast<vk::VkRenderingFlags>(vk::VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR) :
                static_cast<vk::VkRenderingFlags>(0u);
        vk::VkClearValue clearValue =
            makeClearValueColorF32(m_clearColor.x(), m_clearColor.y(), m_clearColor.z(), m_clearColor.w());
        vk::VkRenderingAttachmentInfoKHR colorAttachment = {
            vk::VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR, // VkStructureType                        sType
            nullptr,                                             // const void*                            pNext
            m_nullColorAttachment ?
                VK_NULL_HANDLE :
                m_resources.m_androidColorAttachmentImageView.get(), // VkImageView                            imageView
            vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,            // VkImageLayout                        imageLayout
            VK_RESOLVE_MODE_EXTERNAL_FORMAT_DOWNSAMPLE_ANDROID,      // VkResolveModeFlagBits                resolveMode
            m_resources.m_androidExternalImageView.get(), // VkImageView                            resolveImageView
            vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout                        resolveImageLayout
            vk::VK_ATTACHMENT_LOAD_OP_CLEAR,              // VkAttachmentLoadOp                    loadOp
            VK_ATTACHMENT_STORE_OP_STORE,                 // VkAttachmentStoreOp                    storeOp
            clearValue                                    // VkClearValue                            clearValue
        };

        vk::VkRenderingInfoKHR renderingInfo{
            vk::VK_STRUCTURE_TYPE_RENDERING_INFO_KHR, // VkStructureType                        sType
            DE_NULL,                                  // const void*                            pNext
            renderingFlags,                           // VkRenderingFlagsKHR                    flags
            renderArea,                               // VkRect2D                                renderArea
            m_layers,                                 // uint32_t                                layerCount
            0u,                                       // uint32_t                                viewMask
            1u,                                       // uint32_t                                colorAttachmentCount
            &colorAttachment,                         // const VkRenderingAttachmentInfoKHR*    pColorAttachments
            DE_NULL,                                  // const VkRenderingAttachmentInfoKHR*    pDepthAttachment
            DE_NULL,                                  // const VkRenderingAttachmentInfoKHR*    pStencilAttachment
        };
        vk.cmdBeginRendering(cmd, &renderingInfo);
    }
    else
    {
        vk::VkSubpassContents subpassContents = vk::VK_SUBPASS_CONTENTS_INLINE;
        vector<vk::VkClearValue> clearColors(
            m_nullColorAttachment ? 1u : 2u,
            makeClearValueColorF32(m_clearColor.x(), m_clearColor.y(), m_clearColor.z(), m_clearColor.w()));
        if (m_isInputAttachment)
            clearColors.emplace_back(
                clearColors.front()); // All images have the same maximums, so we can reuse clear colors

        const vk::VkRenderPassBeginInfo renderPassBeginInfo = {
            vk::VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, // VkStructureType        sType
            DE_NULL,                                      // const void*            pNext
            clearPass ? m_resources.m_renderPassClear.get() :
                        m_resources.m_renderPass.get(), // VkRenderPass            renderPass
            clearPass ? m_resources.m_framebufferClear.get() :
                        m_resources.m_framebuffer.get(), // VkFramebuffer        framebuffer
            renderArea,                                  // VkRect2D                renderArea
            (uint32_t)clearColors.size(),                // uint32_t                clearValueCount
            clearColors.data(),                          // const VkClearValue*    pClearValues
        };

        vk.cmdBeginRenderPass(cmd, &renderPassBeginInfo, subpassContents);
    }
}

void AhbExternalFormatResolveTestInstance::endRender(vk::VkCommandBuffer cmd) const
{
    const vk::DeviceInterface &vk = m_context.getDeviceInterface();

    if (m_groupParams.useDynamicRendering)
        vk::endRendering(vk, cmd);
    else
        vk::endRenderPass(vk, cmd);
}

void AhbExternalFormatResolveTestInstance::drawCommands(vk::VkCommandBuffer cmd, bool drawFromInputAttachment) const
{
    const vk::DeviceInterface &vk = m_context.getDeviceInterface();

    if (!m_isClearOnly)
    {
        const VkDeviceSize vertexBufferOffset = 0;
        vk.cmdBindVertexBuffers(cmd, 0, 1, &m_resources.m_vertexBuffer->get(), &vertexBufferOffset);

        if (!drawFromInputAttachment)
        {
            vk.cmdBindPipeline(cmd, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, m_resources.m_basePipeline.get());
            vk.cmdDraw(cmd, 4u, 1u, 0u, 0u);
        }

        // Only true in renderpass tests
        if (m_isInputAttachment)
        {
            const vk::VkSubpassBeginInfo subpassBeginInfo = {
                vk::VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO, // VkStructureType        sType
                nullptr,                                  // const void*            pNext
                vk::VK_SUBPASS_CONTENTS_INLINE            // VkSubpassContents    contents
            };
            const vk::VkSubpassEndInfo subpassEndInfo = {
                vk::VK_STRUCTURE_TYPE_SUBPASS_END_INFO, // VkStructureType    sType
                nullptr                                 // const void*        pNext
            };
            vk.cmdNextSubpass2(cmd, &subpassBeginInfo, &subpassEndInfo);

            if (drawFromInputAttachment)
            {
                vk.cmdBindPipeline(cmd, vk::VK_PIPELINE_BIND_POINT_GRAPHICS,
                                   m_resources.m_inputAttachmentPipeline.get());
                vk.cmdBindDescriptorSets(cmd, vk::VK_PIPELINE_BIND_POINT_GRAPHICS,
                                         m_resources.m_inputAttachmentPipelineLayout.get(), 0u, 1u,
                                         &m_resources.m_descriptorSet.get(), 0u, nullptr);
                vk.cmdDraw(cmd, 4u, 1u, 0u, 0u);
            }
        }
    }
}

class AhbExternalFormatResolveTestCase : public TestCase
{
public:
    AhbExternalFormatResolveTestCase(tcu::TestContext &context, const std::string &name, const TestParams &params);
    virtual ~AhbExternalFormatResolveTestCase(void)
    {
    }

    void initPrograms(SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override;

private:
    const TestParams m_params;
};

AhbExternalFormatResolveTestCase::AhbExternalFormatResolveTestCase(tcu::TestContext &context, const std::string &name,
                                                                   const TestParams &params)
    : TestCase(context, name)
    , m_params(params)
{
}

void AhbExternalFormatResolveTestCase::initPrograms(SourceCollections &programCollection) const
{
    {
        std::stringstream source;
        source << "#version 430\n"
               << "layout(location = 0) in vec2 in_position;\n"

               << "void main() {\n"
               << "    gl_Position  = vec4(in_position, 0.0f, 1.0f);\n"
               << "}\n";

        programCollection.glslSources.add("vert") << glu::VertexSource(source.str());
    }

    std::string intMax                                          = std::to_string(std::numeric_limits<int32_t>::max());
    std::string uintMax                                         = std::to_string(std::numeric_limits<uint32_t>::max());
    const std::pair<const char *, const char *> possibleTypes[] = {
        {"i", intMax.c_str()},  // int
        {"u", uintMax.c_str()}, // uint
        {"", "1.0f"},           // float
    };
    const uint32_t typeCount = sizeof(possibleTypes) / sizeof(possibleTypes[0]);

    for (uint32_t i = 0; i < typeCount; ++i)
    {
        std::string shaderName = "frag_" + std::string(possibleTypes[i].first) + "vec4";
        std::stringstream source;
        source << "#version 430\n"
               << "layout(location = 0) out " << possibleTypes[i].first << "vec4 out_color;\n"

               << "const " << possibleTypes[i].first << "vec4 reference_colors[] =\n"
               << "{\n"
               << "    " << possibleTypes[i].first << "vec4(0.0f, 0.0f, 0.0f, 0.0f),\n"
               << "    " << possibleTypes[i].first << "vec4(" << possibleTypes[i].second << ", 0.0f, 0.0f, "
               << possibleTypes[i].second << "),\n"
               << "    " << possibleTypes[i].first << "vec4(0.0f, " << possibleTypes[i].second << ", 0.0f, "
               << possibleTypes[i].second << "),\n"
               << "    " << possibleTypes[i].first << "vec4(0.0f, 0.0f, " << possibleTypes[i].second << " * 0.5, "
               << possibleTypes[i].second << "),\n"
               << "};\n"
               << "void main()\n"
               << "{\n"
               << "    uvec4 fragmentPosition = uvec4(gl_FragCoord);\n"
               << "    uint color_index = (fragmentPosition.x & 1u) + ((fragmentPosition.y & 1u) << 1u);\n"
               << "    out_color = reference_colors[color_index];\n"
               << "}\n";

        programCollection.glslSources.add(shaderName) << glu::FragmentSource(source.str());
    }

    // No need for the input attachment shaders when no input attachment is used
    if (!m_params.m_isInputAttachment)
        return;

    // Required to allow CrYCb that are mapped to BGR formats to match output
    const uint32_t swizzleOrder[][3] = {
        {0, 1, 2}, // Identity (RGB)
        {2, 1, 0}, // First and last element are swapped (BGR)
    };

    const char *shaderIndex[] = {"r", "g", "b"};

    for (uint32_t i = 0; i < typeCount; ++i)
    {
        uint32_t swizzleCount =
            AndroidHardwareBufferInstance::isFormatYuv(m_params.m_format) ? DE_LENGTH_OF_ARRAY(swizzleOrder) : 1u;
        for (uint32_t swizzleIndex = 0u; swizzleIndex < swizzleCount; ++swizzleIndex)
        {
            const uint32_t *swizzle = swizzleOrder[swizzleIndex];
            std::string shaderName  = "frag_input_" + std::string(possibleTypes[i].first) + "vec4_" +
                                     shaderIndex[swizzle[0]] + shaderIndex[swizzle[1]] + shaderIndex[swizzle[2]];
            std::stringstream source;
            source << "#version 430\n"
                   << "layout(location = 0) out " << possibleTypes[i].first << "vec4 out_color;\n"
                   << "layout(input_attachment_index=0, set=0, binding=0) uniform " << possibleTypes[i].first
                   << "subpassInput input_attachment;"

                   << "void main()\n"
                   << "{\n"
                   << "    " << possibleTypes[i].first << "vec4 input_color = subpassLoad(input_attachment);\n"
                   << "    out_color = " << possibleTypes[i].first << "vec4(input_color." << shaderIndex[swizzle[0]]
                   << ","
                      "input_color."
                   << shaderIndex[swizzle[1]]
                   << ","
                      "input_color."
                   << shaderIndex[swizzle[2]]
                   << ","
                      "input_color.w);\n"
                   << "}\n";

            programCollection.glslSources.add(shaderName) << glu::FragmentSource(source.str());
        }
    }
}

TestInstance *AhbExternalFormatResolveTestCase::createInstance(Context &context) const
{
    return new AhbExternalFormatResolveTestInstance(context, m_params);
}

void AhbExternalFormatResolveTestCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_ANDROID_external_format_resolve");

    if (!AndroidHardwareBufferExternalApi::getInstance())
        TCU_THROW(NotSupportedError, "Android Hardware Buffer not present");

    if (m_params.m_groupParams.useDynamicRendering)
        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
}

void createAhbExternalFormatResolveDrawTests(tcu::TestCaseGroup *testGroup, const SharedGroupParams groupParams)
{
    AndroidHardwareBufferInstance::Usage gpuFramebufferCpuRead = static_cast<AndroidHardwareBufferInstance::Usage>(
        AndroidHardwareBufferInstance::Usage::GPU_FRAMEBUFFER | AndroidHardwareBufferInstance::Usage::CPU_READ);

    AndroidHardwareBufferInstance::Usage gpuFramebufferSampled = static_cast<AndroidHardwareBufferInstance::Usage>(
        AndroidHardwareBufferInstance::Usage::GPU_FRAMEBUFFER | AndroidHardwareBufferInstance::Usage::GPU_SAMPLED);
    uint32_t imageDimension        = 64u;
    vk::VkRect2D defaultRenderArea = makeRect2D(imageDimension, imageDimension);
    TestParams params              = {
        defaultRenderArea, // vk::VkRect2D                                m_renderArea
        tcu::UVec2(imageDimension, imageDimension), // tcu::UVec2                                m_imageSize
        AndroidHardwareBufferInstance::Format::UNASSIGNED, // AndroidHardwareBufferInstance::Format    m_format
        gpuFramebufferCpuRead, // AndroidHardwareBufferInstance::Usage        m_usage
        *groupParams,          // GroupParams                                m_groupParams
        false,                 // bool                                        m_isClearOnly
        false,                 // bool                                        m_partialDraw
        false,                 // bool                                        m_isInputAttachment
    };

    std::vector<vk::VkRect2D> partialRenderAreas(10u);
    de::Random randomGenerator(10u);
    for (size_t i = 0u; i < partialRenderAreas.size(); ++i)
    {
        // Partial render areas need to render in multiple of size 2 texel squares to avoid reduction with undefined values due to subsampling
        uint32_t width        = (randomGenerator.getInt(0, static_cast<int32_t>(imageDimension))) & 0xFFFFFFFE;
        uint32_t height       = (randomGenerator.getInt(0, static_cast<int32_t>(imageDimension))) & 0xFFFFFFFE;
        int32_t xOffset       = (randomGenerator.getInt(0, static_cast<int32_t>(imageDimension - width))) & 0xFFFFFFFE;
        int32_t yOffset       = (randomGenerator.getInt(0, static_cast<int32_t>(imageDimension - height))) & 0xFFFFFFFE;
        partialRenderAreas[i] = makeRect2D(xOffset, yOffset, width, height);
    }

    tcu::TextureFormat invalidTextureFormat =
        AndroidHardwareBufferInstance::formatToTextureFormat(AndroidHardwareBufferInstance::Format::UNASSIGNED);
    // Draw tests
    tcu::TestCaseGroup *drawGroup       = new tcu::TestCaseGroup(testGroup->getTestContext(), "draw");
    tcu::TestCaseGroup *inputAttachment = new tcu::TestCaseGroup(testGroup->getTestContext(), "input_attachment");
    tcu::TestCaseGroup *clearGroup      = new tcu::TestCaseGroup(testGroup->getTestContext(), "clear");
    for (uint32_t i = 0; i < AndroidHardwareBufferInstance::Format::COUNT; ++i)
    {
        params.m_format = static_cast<AndroidHardwareBufferInstance::Format>(i);

        tcu::TextureFormat textureFormat = AndroidHardwareBufferInstance::formatToTextureFormat(params.m_format);
        bool isImplementationDefined = params.m_format == AndroidHardwareBufferInstance::Format::IMPLEMENTATION_DEFINED;
        bool isColorFormat           = AndroidHardwareBufferInstance::isFormatColor(params.m_format);
        bool isRawFormat             = AndroidHardwareBufferInstance::isFormatRaw(params.m_format);
        bool hasValidTextureFormat   = (invalidTextureFormat != textureFormat);

        if (isImplementationDefined || (!isColorFormat && !isRawFormat))
            continue;

        const std::string formatName = AndroidHardwareBufferInstance::getFormatName(params.m_format);

        // CPU side validation requires valid tcu::TextureFormat
        if (hasValidTextureFormat)
        {
            tcu::TestCaseGroup *formatGroup = new tcu::TestCaseGroup(testGroup->getTestContext(), formatName.c_str());

            params.m_renderArea = defaultRenderArea;
            // Draw to full render area of external format
            formatGroup->addChild(
                new AhbExternalFormatResolveTestCase(testGroup->getTestContext(), "full_render_area", params));

            params.m_partialDraw = true;
            for (size_t renderAreaIndex = 0u; renderAreaIndex < partialRenderAreas.size(); ++renderAreaIndex)
            {
                params.m_renderArea = partialRenderAreas[renderAreaIndex];
                formatGroup->addChild(new AhbExternalFormatResolveTestCase(
                    testGroup->getTestContext(), "partial_render_area_" + std::to_string(renderAreaIndex), params));
            }
            params.m_partialDraw = false;

            drawGroup->addChild(formatGroup);
        }

        if (!params.m_groupParams.useDynamicRendering)
        {
            params.m_isInputAttachment = true;
            params.m_usage             = gpuFramebufferSampled;
            params.m_renderArea        = defaultRenderArea;

            tcu::TestCaseGroup *formatGroup = new tcu::TestCaseGroup(testGroup->getTestContext(), formatName.c_str());

            params.m_renderArea = defaultRenderArea;
            // Draw to full render area of external format
            formatGroup->addChild(
                new AhbExternalFormatResolveTestCase(testGroup->getTestContext(), "full_render_area", params));

            params.m_partialDraw = true;
            for (size_t renderAreaIndex = 0u; renderAreaIndex < partialRenderAreas.size(); ++renderAreaIndex)
            {
                params.m_renderArea = partialRenderAreas[renderAreaIndex];
                formatGroup->addChild(new AhbExternalFormatResolveTestCase(
                    testGroup->getTestContext(), "partial_render_area_" + std::to_string(renderAreaIndex), params));
            }
            params.m_partialDraw = false;

            inputAttachment->addChild(formatGroup);
            params.m_usage             = gpuFramebufferCpuRead;
            params.m_isInputAttachment = false;
        }

        if (!params.m_groupParams.useSecondaryCmdBuffer ||
            params.m_groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass)
        {
            // CPU side validation requires valid tcu::TextureFormat
            if (hasValidTextureFormat)
            {
                params.m_isClearOnly = true;
                params.m_renderArea  = defaultRenderArea;
                clearGroup->addChild(
                    new AhbExternalFormatResolveTestCase(testGroup->getTestContext(), formatName, params));
                params.m_isClearOnly = false;
            }
        }
    }

    testGroup->addChild(clearGroup);
    testGroup->addChild(drawGroup);
    testGroup->addChild(inputAttachment);
}

} // namespace

tcu::TestCaseGroup *createAhbExternalFormatResolveTests(tcu::TestContext &testCtx, const SharedGroupParams &groupParams)
{
    // Draw tests using Android Hardware Buffer external formats
    return createTestGroup(testCtx, "ahb_external_format_resolve", createAhbExternalFormatResolveDrawTests,
                           groupParams);
}

} // namespace Draw
} // namespace vkt
