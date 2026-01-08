/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
 * Copyright (c) 2025 Valve Corporation.
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
 * \brief Vulkan Dynamic Rendering Local Read Maintenance10 Tests
 *//*--------------------------------------------------------------------*/

#include "vktDynamicRenderingLocalReadMaint10Tests.hpp"
#include "vktTestCase.hpp"

#include "vkImageUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkTypeUtil.hpp"

#include "tcuImageCompare.hpp"

#include "deUniquePtr.hpp"
#include "deRandom.hpp"

#include <sstream>
#include <string>
#include <memory>
#include <vector>
#include <cstring>

//#define CLASSIC_DRLR_WITHOUT_MAINT10 1
#undef CLASSIC_DRLR_WITHOUT_MAINT10

namespace vkt::renderpass
{
namespace
{

using namespace vk;

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

struct TestParams
{
    VkSampleCountFlagBits samples;
    VkFormat attFormat;         // Input attachment format.
    std::vector<bool> feedback; // How many attachments to have, and which ones will contain a feedback loop.
    tcu::Maybe<int> sampleId;   // If present and multisample: read this sample; else: gl_SampleID or single sample.
    bool generalLayout;         // General layout if true, DRLR layout otherwise.

    bool isMultiSample() const
    {
        return (samples != VK_SAMPLE_COUNT_1_BIT);
    }

    VkImageAspectFlags getTestAspects() const
    {
        VkImageAspectFlags aspects = 0u;

        const auto tcuFormat = mapVkFormat(attFormat);

        if (tcuFormat.order == tcu::TextureFormat::DS)
            aspects |= (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
        else if (tcuFormat.order == tcu::TextureFormat::D)
            aspects |= VK_IMAGE_ASPECT_DEPTH_BIT;
        else if (tcuFormat.order == tcu::TextureFormat::S)
            aspects |= VK_IMAGE_ASPECT_STENCIL_BIT;
        else
            aspects |= VK_IMAGE_ASPECT_COLOR_BIT;

        return aspects;
    }

    bool isColorTest() const
    {
        return (getTestAspects() & VK_IMAGE_ASPECT_COLOR_BIT);
    }

    VkImageUsageFlags getImageUsageFlags() const
    {
        VkImageUsageFlags usage = 0u;

        if (isColorTest())
            usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        else
            usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        // We will copy it out sometimes by reading from it.
        if (isMultiSample())
            usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

        usage |=
            (VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

        return usage;
    }

    VkExtent3D getExtent() const
    {
        return makeExtent3D(8u, 8u, 1u);
    }

    // Used to expand multisample images into single-sampled images.
    VkExtent3D getExpandedExtent() const
    {
        auto extent = getExtent();
        extent.width *= static_cast<uint32_t>(samples);
        return extent;
    }

    VkImageCreateInfo getImageCreateInfo() const
    {
        return VkImageCreateInfo{
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            nullptr,
            0u,
            VK_IMAGE_TYPE_2D,
            attFormat,
            getExtent(),
            1u,
            1u,
            samples,
            VK_IMAGE_TILING_OPTIMAL,
            getImageUsageFlags(),
            VK_SHARING_MODE_EXCLUSIVE,
            0u,
            nullptr,
            VK_IMAGE_LAYOUT_UNDEFINED,
        };
    }

    bool isMultiInput() const
    {
        return (feedback.size() > 1);
    }

    // Simple count of the needed amount of attachments that will be used as input attachments.
    uint32_t getAttCount() const
    {
        const bool multiInput = isMultiInput();
        DE_ASSERT(!(multiInput && (getTestAspects() & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))));
        DE_UNREF(multiInput); // For release builds.
        return de::sizeU32(feedback);
    }

    // If not all uses are concurrent, we will create a second set of attachments and use them as output attachments
    // for those cases that need it. Otherwise, we will create a single set of attachments.
    uint32_t getTotalAttCount() const
    {
        bool allConcurrent = true;
        for (const bool feedbackLoop : feedback)
        {
            if (!feedbackLoop)
            {
                allConcurrent = false;
                break;
            }
        }
        return de::sizeU32(feedback) * (allConcurrent ? 1u : 2u);
    }

    uint32_t getTotalSampleCount() const
    {
        const auto extent = getExpandedExtent();
        return (extent.width * extent.height * extent.depth);
    }

    uint32_t getRandomSeed() const
    {
        auto seed = ((static_cast<uint32_t>(attFormat) << 24) | (static_cast<uint32_t>(samples) << 16));
        for (size_t i = 0; i < feedback.size(); ++i)
            seed |= (static_cast<uint32_t>(feedback.at(i)) << i);
        return seed;
    }

    // When reading depth or stencil as input attachments, we will give them unique input attachment indices that will
    // not match any color attachment.
    uint32_t getDepthStencilInputAttachmentOffset() const
    {
        return 0u;
    }

    // Returns the output attachment index for original attachment attIndex. If the attachment contains a feedback loop,
    // the output attachment is itself. If not, it's one of the extra attachments.
    uint32_t getOutputAttForAtt(uint32_t attIndex) const
    {
        const auto attCount = getAttCount();
        DE_ASSERT(attIndex < attCount);
        return (feedback.at(attIndex) ? attIndex : (attIndex + attCount));
    }

    bool anyLoop() const
    {
        for (const bool feedbackLoop : feedback)
        {
            if (feedbackLoop)
                return true;
        }
        return false;
    }
};

class DRLRFeedbackLoopInstance : public vkt::TestInstance
{
public:
    DRLRFeedbackLoopInstance(Context &context, const TestParams &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~DRLRFeedbackLoopInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const TestParams m_params;
};

class DRLRFeedbackLoopCase : public vkt::TestCase
{
public:
    DRLRFeedbackLoopCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~DRLRFeedbackLoopCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new DRLRFeedbackLoopInstance(context, m_params);
    }

protected:
    const TestParams m_params;
};

void DRLRFeedbackLoopCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_dynamic_rendering_local_read");
#ifndef CLASSIC_DRLR_WITHOUT_MAINT10
    context.requireDeviceFunctionality("VK_KHR_maintenance10");
#endif

    const auto testAspects = m_params.getTestAspects();
    const auto testStencil = static_cast<bool>(testAspects & VK_IMAGE_ASPECT_STENCIL_BIT);
    if (testStencil)
        context.requireDeviceFunctionality("VK_EXT_shader_stencil_export");

    if (m_params.isMultiSample())
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);

    const auto ctx             = context.getContextCommonData();
    const auto imageCreateInfo = m_params.getImageCreateInfo();

    VkImageFormatProperties formatProperties;
    const auto result = ctx.vki.getPhysicalDeviceImageFormatProperties(
        ctx.physicalDevice, imageCreateInfo.format, imageCreateInfo.imageType, imageCreateInfo.tiling,
        imageCreateInfo.usage, imageCreateInfo.flags, &formatProperties);

    if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
        TCU_THROW(NotSupportedError, "Format does not support the required parameters");

    if ((formatProperties.sampleCounts & imageCreateInfo.samples) == 0u)
        TCU_THROW(NotSupportedError, "Format does not support the required sample count");
}

// As used by the shaders.
struct PushConstants
{
    tcu::Vec4 scale;
    tcu::Vec4 offset;
    tcu::IVec4 imageSize; // .xyz is the actual size, and .w is the sample count
};

// As used by the shaders to avoid optimizations, but its usage should result in a no-op.
struct Modifiers
{
    const tcu::Vec4 zeros;
    const tcu::Vec4 ones;

    Modifiers() : zeros(0.0f, 0.0f, 0.0f, 0.0f), ones(1.0f, 1.0f, 1.0f, 1.0f)
    {
    }
};

void DRLRFeedbackLoopCase::initPrograms(vk::SourceCollections &programCollection) const
{
    // Matches the definition above.
    const std::string pcDecl = "layout (push_constant, std430) uniform PushConstantBlock {\n"
                               "    vec4 scale;\n"
                               "    vec4 offset;\n"
                               "    ivec4 imageSize; // .xyz is the actual size, and .w is the sample count\n"
                               "} pc;\n";

    {
        // Quad using a triangle strip. The calculated x and y values are in the 0..1 range, so the scale and offset
        // allow us to place the quad wherever we want.
        std::ostringstream vert;
        vert << "#version 460\n"
             << pcDecl << "void main(void) {\n"
             << "    const float x = (((gl_VertexIndex & 2)>>1));\n"
             << "    const float y = ( (gl_VertexIndex & 1));\n"
             << "    vec4 position = vec4(x, y, 0.0, 1.0) * pc.scale + pc.offset;\n"
             << "    gl_Position = position;\n"
             << "}\n";
        programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
    }

    const auto attCount    = m_params.getAttCount();
    const auto isMS        = m_params.isMultiSample();
    const auto testAspects = m_params.getTestAspects();
    const auto testColor   = static_cast<bool>(testAspects & VK_IMAGE_ASPECT_COLOR_BIT);
    const auto testDepth   = static_cast<bool>(testAspects & VK_IMAGE_ASPECT_DEPTH_BIT);
    const auto testStencil = static_cast<bool>(testAspects & VK_IMAGE_ASPECT_STENCIL_BIT);

    {
        // Fragment shader that fills the output images with data read from buffers.
        std::ostringstream frag;
        frag << "#version 460\n";

        if (testStencil)
            frag << "#extension GL_ARB_shader_stencil_export : enable\n";

        if (testColor)
        {
            for (uint32_t i = 0u; i < attCount; ++i)
                frag << "layout (location=" << i << ") out vec4 outColor" << i << ";\n";
        }

        for (uint32_t i = 0u; i < attCount; ++i)
        {
            frag << "layout (set=0, binding=" << i << ", std430) readonly buffer ColorBlock" << i << " {\n"
                 << "    vec4 values[];\n"
                 << "} inputBuffer" << i << ";\n";
        }

        frag << "// For the push constants below: .x=width, .y=height, .w=samples\n"
             << pcDecl << "void main(void) {\n"
             << "    ivec2 pixelCoord = ivec2(gl_FragCoord.xy);\n"
             << "    int pixelIndex = pixelCoord.y * pc.imageSize.x + pixelCoord.x;\n"
             << "    int bufferIndex = pixelIndex * pc.imageSize.w + " << (isMS ? "gl_SampleID" : "0") << ";\n";

        if (testColor)
        {
            for (uint32_t i = 0u; i < attCount; ++i)
                frag << "    outColor" << i << " = inputBuffer" << i << ".values[bufferIndex];\n";
        }
        if (testDepth)
            frag << "    gl_FragDepth = inputBuffer0.values[bufferIndex].x;\n";
        if (testStencil)
            frag << "    gl_FragStencilRefARB = int(inputBuffer0.values[bufferIndex].y);\n";

        frag << "}\n";

        programCollection.glslSources.add("frag-load") << glu::FragmentSource(frag.str());
    }

    if (isMS)
    {
        // Fragment shader that copies multisample images to single sample ones, expanding each pixel to a horizontal
        // multi-pixel block.
        std::ostringstream frag;
        frag << "#version 460\n";

        if (testStencil)
            frag << "#extension GL_ARB_shader_stencil_export : enable\n";

        // Note in the copy render pass we will only use the expanded views, which are not duplicated, so we do not need
        // to add the result attachment offset to the color locations.
        if (testColor)
        {
            for (uint32_t i = 0u; i < attCount; ++i)
                frag << "layout (location=" << i << ") out vec4 outColor" << i << ";\n";
        }

        if (testColor)
        {
            for (uint32_t i = 0u; i < attCount; ++i)
                frag << "layout (set=0, binding=" << i << ") uniform sampler2DMS srcColor" << i << ";\n";
        }
        else
        {
            uint32_t nextBinding = 0u;
            if (testDepth)
                frag << "layout (set=0, binding=" << nextBinding++ << ") uniform sampler2DMS srcDepth;\n";
            if (testStencil)
                frag << "layout (set=0, binding=" << nextBinding++ << ") uniform usampler2DMS srcStencil;\n";
        }

        frag << "// For the push constants below: .x=width, .y=height, .w=samples\n"
             << pcDecl << "void main(void) {\n"
             << "    ivec2 expandedPixelCoord = ivec2(gl_FragCoord.xy);\n"
             << "    int sampleID = expandedPixelCoord.x % pc.imageSize.w;\n"
             << "    int xCoordMS = expandedPixelCoord.x / pc.imageSize.w;\n"
             << "    int yCoordMS = expandedPixelCoord.y;\n";

        if (testColor)
        {
            for (uint32_t i = 0u; i < attCount; ++i)
                frag << "    outColor" << i << " = texelFetch(srcColor" << i
                     << ", ivec2(xCoordMS, yCoordMS), sampleID);\n";
        }
        else
        {
            if (testDepth)
                frag << "    gl_FragDepth = texelFetch(srcDepth, ivec2(xCoordMS, yCoordMS), sampleID).x;\n";
            if (testStencil)
                frag << "    gl_FragStencilRefARB = int(texelFetch(srcStencil, ivec2(xCoordMS, yCoordMS), "
                        "sampleID).x);\n";
        }

        frag << "}\n";

        programCollection.glslSources.add("frag-copy") << glu::FragmentSource(frag.str());
    }

    {
        const auto sampleIndex =
            (isMS ? (static_cast<bool>(m_params.sampleId) ? std::to_string(m_params.sampleId.get()) : "gl_SampleID") :
                    "0");

        // Fragment shader that reads from input attachments, swizzles components and writes to output colors.
        std::ostringstream frag;
        frag << "#version 460\n";

        if (testStencil)
            frag << "#extension GL_ARB_shader_stencil_export : enable\n";

        if (testColor)
        {
            for (uint32_t i = 0u; i < attCount; ++i)
            {
                const auto resultAttIndex = m_params.getOutputAttForAtt(i);
                frag << "layout (location=" << resultAttIndex << ") out vec4 outColor" << i << ";\n";
            }
        }
        else
        {
            // Since we cannot have extra depth/stencil attachments, it only makes sense to test the concurrent case.
            DE_ASSERT(m_params.feedback.size() == 1 && m_params.feedback.front());
        }

        // Matches the Modifiers struct defined above.
        frag << "layout (set=0, binding=0) readonly buffer BufferBlock { vec4 zeros; vec4 ones; } modifiers;\n";

        if (testColor)
        {
            for (uint32_t i = 0u; i < attCount; ++i)
                frag << "layout (set=0, binding=" << (i + 1u) << ", input_attachment_index=" << i
                     << ") uniform subpassInput" << (isMS ? "MS" : "") << " srcImage" << i << ";\n";
        }
        else
        {
            uint32_t nextBinding  = 1u;
            uint32_t nextAttIndex = m_params.getDepthStencilInputAttachmentOffset();

            if (testDepth)
                frag << "layout (set=0, binding=" << nextBinding++ << ", input_attachment_index=" << nextAttIndex++
                     << ") uniform subpassInput" << (isMS ? "MS" : "") << " srcDepth;\n";
            if (testStencil)
                frag << "layout (set=0, binding=" << nextBinding++ << ", input_attachment_index=" << nextAttIndex++
                     << ") uniform usubpassInput" << (isMS ? "MS" : "") << " srcStencil;\n";
        }

        frag << "void main(void) {\n";

        if (testColor)
        {
            for (uint32_t i = 0u; i < attCount; ++i)
                frag << "    vec4 color" << i << " = subpassLoad(srcImage" << i << (isMS ? (", " + sampleIndex) : "")
                     << ") * modifiers.ones + modifiers.zeros;\n";

            for (uint32_t i = 0u; i < attCount; ++i)
                frag << "    outColor" << i << " = color" << i << ".gbra;\n";
        }
        else
        {
            // For depth and stencil we cannot swizzle components, so we will calculate the complementary value.
            if (testDepth)
            {
                frag << "    vec4 depth = subpassLoad(srcDepth" << (isMS ? (", " + sampleIndex) : "")
                     << ") * modifiers.ones + modifiers.zeros;\n";
                frag << "    gl_FragDepth = 1.0 - depth.x;\n";
            }
            if (testStencil)
            {
                frag << "    vec4 stencil = vec4(subpassLoad(srcStencil" << (isMS ? (", " + sampleIndex) : "")
                     << ")) * modifiers.ones + modifiers.zeros;\n";
                frag << "    gl_FragStencilRefARB = 255 - int(stencil.x);\n";
            }
        }

        frag << "}\n";

        programCollection.glslSources.add("frag-modify") << glu::FragmentSource(frag.str());
    }

    {
        // Fragment shader that overwrites part of the result framebuffer with a gradient.
        std::ostringstream frag;
        frag << "#version 460\n";

        if (testStencil)
            frag << "#extension GL_ARB_shader_stencil_export : enable\n";

        if (testColor)
        {
            for (uint32_t i = 0u; i < attCount; ++i)
            {
                const auto resultAttIndex = m_params.getOutputAttForAtt(i);
                frag << "layout (location=" << resultAttIndex << ") out vec4 outColor" << i << ";\n";
            }
        }

        frag << pcDecl << "void main(void) {\n"
             << "    vec4 imageSizeFloat = vec4(pc.imageSize);\n"
             << "    // All samples in a pixel will share color so as not to depend on standard sample locations\n"
             << "    vec2 normalizedCoords = vec2(ivec2(gl_FragCoord.xy)) / imageSizeFloat.xy;\n"
             << "    float gradientValue = (normalizedCoords.x + normalizedCoords.y) / 2.0;\n";

        if (testColor)
        {
            for (uint32_t i = 0u; i < attCount; ++i)
            {
                DE_ASSERT(i < 3u);
                frag << "    vec4 gradient" << i << " = vec4(0.0, 0.0, 0.0, 1.0);\n"
                     << "    gradient" << i << "[" << i << "] = gradientValue;\n"
                     << "    outColor" << i << " = gradient" << i << ";\n";
            }
        }
        else
        {
            if (testDepth)
                frag << "    gl_FragDepth = gradientValue;\n";
            if (testStencil)
                frag << "    gl_FragStencilRefARB = int(gradientValue * 255.0);\n";
        }

        frag << "}\n";

        programCollection.glslSources.add("frag-grad") << glu::FragmentSource(frag.str());
    }
}

using BufferWithMemoryPtr = std::unique_ptr<BufferWithMemory>;
using ImageWithMemoryPtr  = std::unique_ptr<ImageWithMemory>;
using TextureLevelPtr     = std::unique_ptr<tcu::TextureLevel>;
using LoadBufferDataPtr   = std::unique_ptr<std::vector<tcu::Vec4>>;

// Create a rendering attachment info structure without resolve information.
VkRenderingAttachmentInfo makeRenderingAttachmentInfo(VkImageView view, VkImageLayout layout, VkAttachmentLoadOp loadOp,
                                                      VkAttachmentStoreOp storeOp, const VkClearValue &clearValue)
{
    return VkRenderingAttachmentInfo{
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        nullptr,
        view,
        layout,
        VK_RESOLVE_MODE_NONE,
        VK_NULL_HANDLE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        loadOp,
        storeOp,
        clearValue,
    };
};

// Sync attachment writes with future reads and writes.
void fbWritesBarrier(const DeviceInterface &vkd, VkCommandBuffer cmdBuffer)
{
    const auto srcAccess = (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    const auto dstAccess = (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT);
    const auto srcStage  = (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                           VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
    const auto dstStage  = (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                           VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    const auto barrier = makeMemoryBarrier(srcAccess, dstAccess);
    cmdPipelineMemoryBarrier(vkd, cmdBuffer, srcStage, dstStage, &barrier, 1u, VK_DEPENDENCY_BY_REGION_BIT);
}

VkDescriptorBufferInfo makeDescriptorWholeBufferInfo(VkBuffer buffer)
{
    return makeDescriptorBufferInfo(buffer, 0ull, VK_WHOLE_SIZE);
}

tcu::TestStatus DRLRFeedbackLoopInstance::iterate(void)
{
    const auto ctx           = m_context.getContextCommonData();
    const auto totalSamples  = m_params.getTotalSampleCount();
    const auto attCount      = m_params.getAttCount();
    const auto totalAttCount = m_params.getTotalAttCount();
    const auto testAspects   = m_params.getTestAspects();
    const auto testColor     = static_cast<bool>(testAspects & VK_IMAGE_ASPECT_COLOR_BIT);
    const auto testDepth     = static_cast<bool>(testAspects & VK_IMAGE_ASPECT_DEPTH_BIT);
    const auto testStencil   = static_cast<bool>(testAspects & VK_IMAGE_ASPECT_STENCIL_BIT);
    const auto randomSeed    = m_params.getRandomSeed();
    const auto extent        = m_params.getExtent();
    const auto tcuExtentU    = tcu::UVec3(extent.width, extent.height, extent.depth);
    const auto tcuExtent     = tcuExtentU.asInt();
    const auto tcuExtentF    = tcuExtentU.asFloat();
    const auto tcuExtent4 = tcu::IVec4(tcuExtent.x(), tcuExtent.y(), tcuExtent.z(), static_cast<int>(m_params.samples));
    const auto expandedExtent     = m_params.getExpandedExtent();
    const auto tcuExpandedExtentU = tcu::UVec3(expandedExtent.width, expandedExtent.height, expandedExtent.depth);
    const auto tcuExpandedExtent  = tcuExpandedExtentU.asInt();
    const auto imgCreateInfo      = m_params.getImageCreateInfo();
    const auto isMultiSample      = m_params.isMultiSample();
    const auto attLayout = (m_params.generalLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ);
    const auto sampleLayout =
        (m_params.generalLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    const auto bindPoint         = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto tcuAttFormat      = mapVkFormat(m_params.attFormat);
    const auto depthCopyFormat   = (testDepth ? getDepthCopyFormat(m_params.attFormat) : tcu::TextureFormat());
    const auto stencilCopyFormat = (testStencil ? getStencilCopyFormat(m_params.attFormat) : tcu::TextureFormat());
    const auto binding           = DescriptorSetUpdateBuilder::Location::binding;

    const auto pcSize   = DE_SIZEOF32(PushConstants);
    const auto pcStages = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto pcRange  = makePushConstantRange(pcStages, 0u, pcSize);

    DE_ASSERT(imgCreateInfo.imageType == VK_IMAGE_TYPE_2D && imgCreateInfo.arrayLayers == 1u &&
              imgCreateInfo.mipLevels == 1u);
    const auto colorSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, imgCreateInfo.arrayLayers);
    const auto colorSRL = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, imgCreateInfo.arrayLayers);
    const auto dsSRR    = makeImageSubresourceRange(testAspects, 0u, 1u, 0u, imgCreateInfo.arrayLayers);
    const auto depthSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, imgCreateInfo.arrayLayers);
    const auto depthSRL = makeImageSubresourceLayers(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, imgCreateInfo.arrayLayers);
    const auto stencilSRR =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 1u, 0u, imgCreateInfo.arrayLayers);
    const auto stencilSRL = makeImageSubresourceLayers(VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, imgCreateInfo.arrayLayers);

    de::Random rnd(randomSeed);
    std::vector<ImageWithMemoryPtr> colorImages;
    std::vector<Move<VkImageView>> colorViews;

    ImageWithMemoryPtr dsImage;
    Move<VkImageView> dsView;
    Move<VkImageView> depthView;
    Move<VkImageView> stencilView;

    if (testColor)
    {
        colorImages.reserve(totalAttCount);
        colorViews.reserve(totalAttCount);

        for (uint32_t i = 0u; i < totalAttCount; ++i)
        {
            colorImages.emplace_back(
                new ImageWithMemory(ctx.vkd, ctx.device, ctx.allocator, imgCreateInfo, MemoryRequirement::Any));
            colorViews.emplace_back(makeImageView(ctx.vkd, ctx.device, colorImages.back()->get(), VK_IMAGE_VIEW_TYPE_2D,
                                                  imgCreateInfo.format, colorSRR));
        }
    }
    else
    {
        dsImage.reset(new ImageWithMemory(ctx.vkd, ctx.device, ctx.allocator, imgCreateInfo, MemoryRequirement::Any));
        dsView = makeImageView(ctx.vkd, ctx.device, dsImage->get(), VK_IMAGE_VIEW_TYPE_2D, imgCreateInfo.format, dsSRR);

        if (testDepth)
            depthView = makeImageView(ctx.vkd, ctx.device, dsImage->get(), VK_IMAGE_VIEW_TYPE_2D, imgCreateInfo.format,
                                      depthSRR);

        if (testStencil)
            stencilView = makeImageView(ctx.vkd, ctx.device, dsImage->get(), VK_IMAGE_VIEW_TYPE_2D,
                                        imgCreateInfo.format, stencilSRR);
    }

    // Expanded images, used in the multisample case, to read each multisample image and transform each individual pixel
    // into a 4x1 horizontal block (supposing 4 samples), in order to verify each sample individually. The multisample
    // images will be expanded using the fragment copy shader above.
    std::vector<ImageWithMemoryPtr> expandedColorImages;
    std::vector<Move<VkImageView>> expandedColorViews;

    ImageWithMemoryPtr expandedDSImage;
    Move<VkImageView> expandedDSView;

    if (isMultiSample)
    {
        auto singleSampleCreateInfo    = imgCreateInfo;
        singleSampleCreateInfo.extent  = expandedExtent;
        singleSampleCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        singleSampleCreateInfo.usage &= (~VK_IMAGE_USAGE_SAMPLED_BIT);

        if (testColor)
        {
            for (uint32_t i = 0u; i < attCount; ++i)
            {
                expandedColorImages.emplace_back(new ImageWithMemory(ctx.vkd, ctx.device, ctx.allocator,
                                                                     singleSampleCreateInfo, MemoryRequirement::Any));
                expandedColorViews.emplace_back(makeImageView(ctx.vkd, ctx.device, expandedColorImages.back()->get(),
                                                              VK_IMAGE_VIEW_TYPE_2D, singleSampleCreateInfo.format,
                                                              colorSRR));
            }
        }
        else
        {
            expandedDSImage.reset(new ImageWithMemory(ctx.vkd, ctx.device, ctx.allocator, singleSampleCreateInfo,
                                                      MemoryRequirement::Any));
            expandedDSView = makeImageView(ctx.vkd, ctx.device, expandedDSImage->get(), VK_IMAGE_VIEW_TYPE_2D,
                                           singleSampleCreateInfo.format, dsSRR);
        }
    }

    // When loading values from a buffer, generate the source buffers.
    std::vector<BufferWithMemoryPtr> loadBuffers;
    std::vector<LoadBufferDataPtr> loadBuffersData;

    {
        const auto loadBufferSize       = totalSamples * DE_SIZEOF32(tcu::Vec4);
        const auto loadBufferUsage      = static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        const auto loadBufferCreateInfo = makeBufferCreateInfo(loadBufferSize, loadBufferUsage);

        loadBuffers.reserve(attCount);
        for (uint32_t i = 0u; i < attCount; ++i)
        {
            loadBuffers.emplace_back(
                new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, loadBufferCreateInfo, HostIntent::W));
            auto &alloc = loadBuffers.back()->getAllocation();

            loadBuffersData.emplace_back(new std::vector<tcu::Vec4>());
            auto &bufferValues = *loadBuffersData.back();
            bufferValues.reserve(totalSamples);

            for (uint32_t j = 0u; j < totalSamples; ++j)
            {
                if (testColor)
                    bufferValues.emplace_back(rnd.getFloat(), rnd.getFloat(), rnd.getFloat(), 1.0f);
                else
                {
                    // Depth in the .x component, stencil value in the .y component.
                    bufferValues.emplace_back(rnd.getFloat(), static_cast<float>(rnd.getUint8()), 0.0f, 0.0f);
                }
            }

            memcpy(alloc.getHostPtr(), de::dataOrNull(bufferValues), de::dataSize(bufferValues));
            flushAlloc(ctx.vkd, ctx.device, alloc);
        }
    }

    // Modifiers buffer.
    const Modifiers kModifiers;
    BufferWithMemoryPtr modifiersBuffer;
    {
        const auto modifiersBufferSize  = static_cast<VkDeviceSize>(sizeof(kModifiers));
        const auto modifiersBufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        const auto modifiersBufferInfo  = makeBufferCreateInfo(modifiersBufferSize, modifiersBufferUsage);
        modifiersBuffer.reset(
            new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, modifiersBufferInfo, HostIntent::W));

        auto &alloc = modifiersBuffer->getAllocation();
        memcpy(alloc.getHostPtr(), &kModifiers, sizeof(kModifiers));
        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    // Verification buffers.
    std::vector<BufferWithMemoryPtr> verifBuffers;
    {
        const auto verifBufferUsage = static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_TRANSFER_DST_BIT);

        if (testColor)
        {
            const auto verifBufferSize       = totalSamples * tcu::getPixelSize(tcuAttFormat);
            const auto verifBufferCreateInfo = makeBufferCreateInfo(verifBufferSize, verifBufferUsage);

            verifBuffers.reserve(attCount);
            for (uint32_t i = 0u; i < attCount; ++i)
                verifBuffers.emplace_back(
                    new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, verifBufferCreateInfo, HostIntent::R));
        }
        else
        {
            if (testDepth)
            {
                const auto verifBufferSize       = totalSamples * tcu::getPixelSize(depthCopyFormat);
                const auto verifBufferCreateInfo = makeBufferCreateInfo(verifBufferSize, verifBufferUsage);

                verifBuffers.emplace_back(
                    new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, verifBufferCreateInfo, HostIntent::R));
            }

            if (testStencil)
            {
                const auto verifBufferSize       = totalSamples * tcu::getPixelSize(stencilCopyFormat);
                const auto verifBufferCreateInfo = makeBufferCreateInfo(verifBufferSize, verifBufferUsage);

                verifBuffers.emplace_back(
                    new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, verifBufferCreateInfo, HostIntent::R));
            }
        }
    }

    const auto &binaries      = m_context.getBinaryCollection();
    const auto vertShader     = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragLoadShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag-load"));
    const auto fragCopyShader =
        (isMultiSample ? createShaderModule(ctx.vkd, ctx.device, binaries.get("frag-copy")) : Move<VkShaderModule>());
    const auto fragModifyShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag-modify"));
    const auto fragGradShader   = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag-grad"));

    Move<VkPipeline> loadPipeline;
    Move<VkDescriptorSetLayout> loadSetLayout;
    Move<VkPipelineLayout> loadPipelineLayout;
    Move<VkDescriptorPool> loadDescriptorPool;
    Move<VkDescriptorSet> loadDescriptorSet;

    Move<VkPipeline> modPipeline;
    Move<VkDescriptorSetLayout> modSetLayout;
    Move<VkPipelineLayout> modPipelineLayout;
    Move<VkDescriptorPool> modDescriptorPool;
    Move<VkDescriptorSet> modDescriptorSet;

    Move<VkPipeline> gradPipeline;
    Move<VkPipelineLayout> gradPipelineLayout;

    Move<VkSampler> copySampler;
    Move<VkPipeline> copyPipeline;
    Move<VkDescriptorSetLayout> copySetLayout;
    Move<VkPipelineLayout> copyPipelineLayout;
    Move<VkDescriptorPool> copyDescriptorPool;
    Move<VkDescriptorSet> copyDescriptorSet;

    const std::vector<VkViewport> viewports(1u, makeViewport(extent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(extent));

    const std::vector<VkViewport> expandedViewports(1u, makeViewport(expandedExtent));
    const std::vector<VkRect2D> expandedScissors(1u, makeRect2D(expandedExtent));

    const VkPipelineVertexInputStateCreateInfo emptyVertexInput     = initVulkanStructureConst();
    const VkPipelineMultisampleStateCreateInfo loadMultisampleState = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        0u,
        m_params.samples,
        VK_FALSE, // Sample shading should be enabled automatically due to gl_SampleID.
        0.0f,
        nullptr,
        VK_FALSE,
        VK_FALSE,
    };
    const auto &modMultisampleState  = loadMultisampleState;
    const auto &gradMultisampleState = loadMultisampleState;

    const auto allColors =
        (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
    const std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttStates(
        totalAttCount,
        makePipelineColorBlendAttachmentState(VK_FALSE, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
                                              VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, allColors));

    const VkPipelineColorBlendStateCreateInfo colorBlendState = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        nullptr,
        0u,
        VK_FALSE,
        VK_LOGIC_OP_CLEAR,
        de::sizeU32(colorBlendAttStates),
        de::dataOrNull(colorBlendAttStates),
        {0.0f, 0.0f, 0.0f, 0.0f},
    };

    VkPipelineDepthStencilStateCreateInfo depthStencilState = initVulkanStructure();

    if (testDepth)
    {
        depthStencilState.depthTestEnable  = VK_TRUE;
        depthStencilState.depthWriteEnable = VK_TRUE;
        depthStencilState.depthCompareOp   = VK_COMPARE_OP_ALWAYS;
    }

    if (testStencil)
    {
        depthStencilState.stencilTestEnable = VK_TRUE;
        const auto stencilOpState           = makeStencilOpState(VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE,
                                                                 VK_STENCIL_OP_REPLACE, VK_COMPARE_OP_ALWAYS, 0xFFu, 0xFFu, 0u);
        depthStencilState.front             = stencilOpState;
        depthStencilState.back              = stencilOpState;
    }

    std::vector<VkFormat> colorFormats;
    VkFormat renderingDepthFormat   = VK_FORMAT_UNDEFINED;
    VkFormat renderingStencilFormat = VK_FORMAT_UNDEFINED;

    if (testColor)
        colorFormats.resize(totalAttCount, m_params.attFormat);

    if (testDepth)
        renderingDepthFormat = m_params.attFormat;

    if (testStencil)
        renderingStencilFormat = m_params.attFormat;

    std::unique_ptr<VkRenderingInputAttachmentIndexInfo> pRenderingInputAttachmentIndexInfo;

    uint32_t depthInputAttachmentIndex   = 0u;
    uint32_t stencilInputAttachmentIndex = 0u;

    if (testDepth || testStencil)
    {
        uint32_t nextDSInputAttachmentIndex = m_params.getDepthStencilInputAttachmentOffset();

        if (testDepth)
            depthInputAttachmentIndex = nextDSInputAttachmentIndex++;
        if (testStencil)
            stencilInputAttachmentIndex = nextDSInputAttachmentIndex++;

        pRenderingInputAttachmentIndexInfo.reset(new VkRenderingInputAttachmentIndexInfo{
            VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO,
            nullptr,
            0u,
            nullptr,
            (testDepth ? &depthInputAttachmentIndex : nullptr),
            (testStencil ? &stencilInputAttachmentIndex : nullptr),
        });
    }

    const VkPipelineRenderingCreateInfo renderingCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        pRenderingInputAttachmentIndexInfo.get(),
        0u,
        de::sizeU32(colorFormats),
        de::dataOrNull(colorFormats),
        renderingDepthFormat,
        renderingStencilFormat,
    };

    // Load pipeline and related resources.
    {
        const auto descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

        DescriptorSetLayoutBuilder setLayoutBuilder;
        for (uint32_t i = 0u; i < attCount; ++i)
            setLayoutBuilder.addSingleBinding(descriptorType, VK_SHADER_STAGE_FRAGMENT_BIT);
        loadSetLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);

        loadPipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *loadSetLayout, &pcRange);

        loadPipeline = makeGraphicsPipeline(ctx.vkd, ctx.device, *loadPipelineLayout, *vertShader, VK_NULL_HANDLE,
                                            VK_NULL_HANDLE, VK_NULL_HANDLE, *fragLoadShader, VK_NULL_HANDLE, viewports,
                                            scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u, 0u, &emptyVertexInput,
                                            nullptr, &loadMultisampleState, &depthStencilState, &colorBlendState,
                                            nullptr, &renderingCreateInfo);

        DescriptorPoolBuilder poolBuilder;
        poolBuilder.addType(descriptorType, attCount);
        loadDescriptorPool =
            poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
        loadDescriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *loadDescriptorPool, *loadSetLayout);

        DescriptorSetUpdateBuilder updateBuilder;
        for (uint32_t i = 0u; i < attCount; ++i)
        {
            const auto descInfo = makeDescriptorWholeBufferInfo(loadBuffers.at(i)->get());
            updateBuilder.writeSingle(*loadDescriptorSet, binding(i), descriptorType, &descInfo);
        }
        updateBuilder.update(ctx.vkd, ctx.device);
    }

    // Modification pipeline and related resources.
    {
        DescriptorSetLayoutBuilder setLayoutBuilder;
        setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
        if (testColor)
        {
            for (uint32_t i = 0u; i < attCount; ++i)
                setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
        }
        else
        {
            if (testDepth)
                setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
            if (testStencil)
                setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
        }
        modSetLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);

        modPipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *modSetLayout, &pcRange);

        modPipeline = makeGraphicsPipeline(ctx.vkd, ctx.device, *modPipelineLayout, *vertShader, VK_NULL_HANDLE,
                                           VK_NULL_HANDLE, VK_NULL_HANDLE, *fragModifyShader, VK_NULL_HANDLE, viewports,
                                           scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u, 0u, &emptyVertexInput,
                                           nullptr, &modMultisampleState, &depthStencilState, &colorBlendState, nullptr,
                                           &renderingCreateInfo);

        DescriptorPoolBuilder poolBuilder;
        poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u);
        if (testColor)
            poolBuilder.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, attCount);
        else
            poolBuilder.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, (testDepth ? 1u : 0u) + (testStencil ? 1u : 0u));
        modDescriptorPool =
            poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
        modDescriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *modDescriptorPool, *modSetLayout);

        DescriptorSetUpdateBuilder updateBuilder;
        {
            const auto modBuffersDesc = makeDescriptorWholeBufferInfo(modifiersBuffer->get());
            updateBuilder.writeSingle(*modDescriptorSet, binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                      &modBuffersDesc);
        }
        if (testColor)
        {
            for (uint32_t i = 0u; i < attCount; ++i)
            {
                const auto descInfo = makeDescriptorImageInfo(VK_NULL_HANDLE, colorViews.at(i).get(), attLayout);
                updateBuilder.writeSingle(*modDescriptorSet, binding(i + 1u), VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                                          &descInfo);
            }
        }
        else
        {
            uint32_t nextBinding = 1u;
            if (testDepth)
            {
                const auto descInfo = makeDescriptorImageInfo(VK_NULL_HANDLE, depthView.get(), attLayout);
                updateBuilder.writeSingle(*modDescriptorSet, binding(nextBinding++),
                                          VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &descInfo);
            }
            if (testStencil)
            {
                const auto descInfo = makeDescriptorImageInfo(VK_NULL_HANDLE, stencilView.get(), attLayout);
                updateBuilder.writeSingle(*modDescriptorSet, binding(nextBinding++),
                                          VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &descInfo);
            }
        }
        updateBuilder.update(ctx.vkd, ctx.device);
    }

    // Gradient pipeline.
    {
        gradPipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, VK_NULL_HANDLE, &pcRange);

        gradPipeline = makeGraphicsPipeline(ctx.vkd, ctx.device, *gradPipelineLayout, *vertShader, VK_NULL_HANDLE,
                                            VK_NULL_HANDLE, VK_NULL_HANDLE, *fragGradShader, VK_NULL_HANDLE, viewports,
                                            scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u, 0u, &emptyVertexInput,
                                            nullptr, &gradMultisampleState, &depthStencilState, &colorBlendState,
                                            nullptr, &renderingCreateInfo);
    }

    // Copy pipeline and related resources, used in the multisample case to transform images to single-sample.
    if (isMultiSample)
    {
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
            0.0f,
            VK_FALSE,
            VK_COMPARE_OP_NEVER,
            0.0f,
            0.0f,
            VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
            VK_FALSE,
        };
        copySampler = createSampler(ctx.vkd, ctx.device, &samplerCreateInfo);

        DescriptorSetLayoutBuilder setLayoutBuilder;
        if (testColor)
        {
            for (uint32_t i = 0u; i < attCount; ++i)
                setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                  VK_SHADER_STAGE_FRAGMENT_BIT);
        }
        else
        {
            if (testDepth)
                setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                  VK_SHADER_STAGE_FRAGMENT_BIT);
            if (testStencil)
                setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                  VK_SHADER_STAGE_FRAGMENT_BIT);
        }
        copySetLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);

        copyPipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *copySetLayout, &pcRange);

        {
            // This pipeline will not use input attachments, so we never want the input att indices struct in the chain.
            auto copyRenderingCreateInfo  = renderingCreateInfo;
            copyRenderingCreateInfo.pNext = nullptr;

            copyPipeline = makeGraphicsPipeline(
                ctx.vkd, ctx.device, *copyPipelineLayout, *vertShader, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                *fragCopyShader, VK_NULL_HANDLE, expandedViewports, expandedScissors,
                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u, 0u, &emptyVertexInput, nullptr, nullptr /*default MS state*/,
                &depthStencilState, &colorBlendState, nullptr, &copyRenderingCreateInfo);
        }

        DescriptorPoolBuilder poolBuilder;
        if (testColor)
            poolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, attCount);
        else
            poolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                (testDepth ? 1u : 0u) + (testStencil ? 1u : 0u));
        copyDescriptorPool =
            poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
        copyDescriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *copyDescriptorPool, *copySetLayout);

        DescriptorSetUpdateBuilder updateBuilder;
        if (testColor)
        {
            for (uint32_t i = 0u; i < attCount; ++i)
            {
                const auto resultAttIndex = m_params.getOutputAttForAtt(i);
                const auto descInfo =
                    makeDescriptorImageInfo(*copySampler, colorViews.at(resultAttIndex).get(), sampleLayout);
                updateBuilder.writeSingle(*copyDescriptorSet, binding(i), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                          &descInfo);
            }
        }
        else
        {
            uint32_t nextBinding = 0u;
            if (testDepth)
            {
                // Use specific descriptor for sampling the depth with a depth-only view.
                const auto descInfo = makeDescriptorImageInfo(*copySampler, depthView.get(), sampleLayout);
                updateBuilder.writeSingle(*copyDescriptorSet, binding(nextBinding++),
                                          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descInfo);
            }
            if (testStencil)
            {
                // Use specific descriptor for sampling the stencil with a stencil-only view.
                const auto descInfo = makeDescriptorImageInfo(*copySampler, stencilView.get(), sampleLayout);
                updateBuilder.writeSingle(*copyDescriptorSet, binding(nextBinding++),
                                          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descInfo);
            }
        }
        updateBuilder.update(ctx.vkd, ctx.device);
    }

    VkClearValue clearColor;
    memset(&clearColor, 0, sizeof(clearColor));

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);

    // Move images to the right layout.
    {
        std::vector<VkImageMemoryBarrier> barriers;
        barriers.reserve(totalAttCount);

        const auto srcAccess = 0u;
        const auto dstAccess =
            (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        const auto srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        const auto dstStage = (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

        if (testColor)
        {
            for (uint32_t i = 0u; i < totalAttCount; ++i)
                barriers.push_back(makeImageMemoryBarrier(srcAccess, dstAccess, VK_IMAGE_LAYOUT_UNDEFINED, attLayout,
                                                          colorImages.at(i)->get(), colorSRR));
        }
        else
            barriers.push_back(makeImageMemoryBarrier(srcAccess, dstAccess, VK_IMAGE_LAYOUT_UNDEFINED, attLayout,
                                                      dsImage->get(), dsSRR));

        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, srcStage, dstStage, de::dataOrNull(barriers),
                                      barriers.size());
    }

    std::unique_ptr<VkRenderingAttachmentFlagsInfoKHR> pRenderingAttachmentFlagsInfo;
#ifndef CLASSIC_DRLR_WITHOUT_MAINT10
    if (m_params.anyLoop())
    {
        // In the simultaneous case, the first group of attachments will be used both as an input attachment and as a
        // write attachment concurrently, so they need the flag.
        pRenderingAttachmentFlagsInfo.reset(new VkRenderingAttachmentFlagsInfoKHR{
            VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_FLAGS_INFO_KHR,
            nullptr,
            VK_RENDERING_ATTACHMENT_INPUT_ATTACHMENT_FEEDBACK_BIT_KHR,
        });
    }
#endif

    // Prepare the images by loading data onto them with the load pipeline.
    {
        std::vector<VkRenderingAttachmentInfo> colorAttInfos;
        std::vector<VkRenderingAttachmentInfo> dsAttInfos;

        if (testColor)
        {
            colorAttInfos.reserve(totalAttCount);
            for (uint32_t i = 0u; i < totalAttCount; ++i)
                colorAttInfos.push_back(makeRenderingAttachmentInfo(colorViews.at(i).get(), attLayout,
                                                                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                                    VK_ATTACHMENT_STORE_OP_STORE, clearColor));
        }
        else
        {
            dsAttInfos.push_back(makeRenderingAttachmentInfo(dsView.get(), attLayout, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                             VK_ATTACHMENT_STORE_OP_STORE, clearColor));
        }

#ifndef CLASSIC_DRLR_WITHOUT_MAINT10
        // Note we reuse the same extension structure for all affected attachments as the contents should be identical
        // for all of them.
        if (testColor)
        {
            for (uint32_t i = 0u; i < attCount; ++i)
            {
                if (m_params.feedback.at(i))
                    colorAttInfos.at(i).pNext = pRenderingAttachmentFlagsInfo.get();
            }
        }
        for (auto &dsAttInfo : dsAttInfos)
        {
            DE_ASSERT(m_params.feedback.front());
            dsAttInfo.pNext = pRenderingAttachmentFlagsInfo.get();
        }
#endif

        VkRenderingFlags renderingInfoFlags = 0u;
#ifndef CLASSIC_DRLR_WITHOUT_MAINT10
        // All the scenarios we're testing require the concurrent rendering flag to signal that we're being explicit
        // when passing the attachment flag to indicate feedback loops or their absence.
        renderingInfoFlags |= VK_RENDERING_LOCAL_READ_CONCURRENT_ACCESS_CONTROL_BIT_KHR;
#endif

        const VkRenderingInfo renderingInfo = {
            VK_STRUCTURE_TYPE_RENDERING_INFO,
            nullptr,
            renderingInfoFlags,
            scissors.at(0u),
            imgCreateInfo.arrayLayers,
            0u,
            de::sizeU32(colorAttInfos),
            de::dataOrNull(colorAttInfos),
            (testDepth ? de::dataOrNull(dsAttInfos) : nullptr),
            (testStencil ? de::dataOrNull(dsAttInfos) : nullptr),
        };

        const PushConstants pcValues{
            // Scale and offset so that we cover the whole framebuffer (scale 2.0 and offset -1.0 in X/Y).
            tcu::Vec4(2.0f, 2.0f, 1.0f, 1.0f),
            tcu::Vec4(-1.0f, -1.0f, 0.0f, 0.0f),
            tcuExtent4,
        };

        ctx.vkd.cmdBeginRendering(cmdBuffer, &renderingInfo);
        if (pRenderingInputAttachmentIndexInfo)
            ctx.vkd.cmdSetRenderingInputAttachmentIndices(cmdBuffer, pRenderingInputAttachmentIndexInfo.get());
        ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *loadPipeline);
        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *loadPipelineLayout, 0u, 1u, &loadDescriptorSet.get(), 0u,
                                      nullptr);
        ctx.vkd.cmdPushConstants(cmdBuffer, *loadPipelineLayout, pcStages, 0u, pcSize, &pcValues);
        ctx.vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
    }

    // Make sure all attachment writes are ready before the reads.
    fbWritesBarrier(ctx.vkd, cmdBuffer);

    // Read and modify the attachments using the modification pipeline.
    {
        const PushConstants pcValues{
            // Scale and offset so that we cover the whole framebuffer (scale 2.0 and offset -1.0 in X/Y).
            tcu::Vec4(2.0f, 2.0f, 1.0f, 1.0f),
            tcu::Vec4(-1.0f, -1.0f, 0.0f, 0.0f),
            tcuExtent4,
        };

        ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *modPipeline);
        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *modPipelineLayout, 0u, 1u, &modDescriptorSet.get(), 0u,
                                      nullptr);
        ctx.vkd.cmdPushConstants(cmdBuffer, *modPipelineLayout, pcStages, 0u, pcSize, &pcValues);
        ctx.vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
    }

    // Make sure all attachment writes are ready before subsequent writes.
    fbWritesBarrier(ctx.vkd, cmdBuffer);

    // Overwrite part of the framebuffer with a gradient.
    {
        const PushConstants pcValues{
            // Scale and offset so that we cover only the right side of the framebuffer.
            tcu::Vec4(1.0f, 2.0f, 1.0f, 1.0f),
            tcu::Vec4(0.0f, -1.0f, 0.0f, 0.0f),
            tcuExtent4,
        };

        ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *gradPipeline);
        ctx.vkd.cmdPushConstants(cmdBuffer, *gradPipelineLayout, pcStages, 0u, pcSize, &pcValues);
        ctx.vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
        ctx.vkd.cmdEndRendering(cmdBuffer);
    }

    if (isMultiSample)
    {
        // Expand multisample images to single-sample versions.

        // Prepare attachments to be read.
        {
            std::vector<VkImageMemoryBarrier> barriers;
            barriers.reserve(attCount);

            const auto srcAccess =
                (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
            const auto dstAccess = VK_ACCESS_SHADER_READ_BIT;
            const auto srcStage =
                (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                 VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
            const auto dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

            if (testColor)
            {
                for (uint32_t i = 0u; i < attCount; ++i)
                {
                    const auto resultAttIndex = m_params.getOutputAttForAtt(i);
                    barriers.push_back(makeImageMemoryBarrier(srcAccess, dstAccess, attLayout, sampleLayout,
                                                              colorImages.at(resultAttIndex)->get(), colorSRR));
                }
            }
            else
                barriers.push_back(
                    makeImageMemoryBarrier(srcAccess, dstAccess, attLayout, sampleLayout, dsImage->get(), dsSRR));

            cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, srcStage, dstStage, de::dataOrNull(barriers),
                                          barriers.size());
        }

        // Prepare expanded images to be attachments.
        {
            std::vector<VkImageMemoryBarrier> barriers;
            barriers.reserve(attCount);

            const auto srcAccess = 0u;
            const auto dstAccess =
                (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
            const auto srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            const auto dstStage =
                (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                 VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

            if (testColor)
            {
                for (uint32_t i = 0u; i < attCount; ++i)
                    barriers.push_back(makeImageMemoryBarrier(srcAccess, dstAccess, VK_IMAGE_LAYOUT_UNDEFINED,
                                                              attLayout, expandedColorImages.at(i)->get(), colorSRR));
            }
            else
                barriers.push_back(makeImageMemoryBarrier(srcAccess, dstAccess, VK_IMAGE_LAYOUT_UNDEFINED, attLayout,
                                                          expandedDSImage->get(), dsSRR));

            cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, srcStage, dstStage, de::dataOrNull(barriers),
                                          barriers.size());
        }

        // Render and expand images.
        {
            // Note we only use attCount images for the expanded views. We do not need to use resultAttOffset here.
            // Same reason why the frag copy shader does not use resultAttOffset for the color target locations.
            // Note these are never used as input attachments, so they do not need the new flag.
            std::vector<VkRenderingAttachmentInfo> colorAttInfos;
            std::vector<VkRenderingAttachmentInfo> dsAttInfos;

            if (testColor)
            {
                colorAttInfos.reserve(attCount);
                for (uint32_t i = 0u; i < attCount; ++i)
                    colorAttInfos.push_back(makeRenderingAttachmentInfo(expandedColorViews.at(i).get(), attLayout,
                                                                        VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                                        VK_ATTACHMENT_STORE_OP_STORE, clearColor));
            }
            else
            {
                dsAttInfos.push_back(makeRenderingAttachmentInfo(expandedDSView.get(), attLayout,
                                                                 VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                                 VK_ATTACHMENT_STORE_OP_STORE, clearColor));
            }

            const VkRenderingInfo renderingInfo = {
                VK_STRUCTURE_TYPE_RENDERING_INFO,
                nullptr,
                0u,
                expandedScissors.at(0u),
                imgCreateInfo.arrayLayers,
                0u,
                de::sizeU32(colorAttInfos),
                de::dataOrNull(colorAttInfos),
                (testDepth ? de::dataOrNull(dsAttInfos) : nullptr),
                (testStencil ? de::dataOrNull(dsAttInfos) : nullptr),
            };

            const PushConstants pcValues{
                // Scale and offset so that we cover the whole framebuffer (scale 2.0 and offset -1.0 in X/Y).
                tcu::Vec4(2.0f, 2.0f, 1.0f, 1.0f),
                tcu::Vec4(-1.0f, -1.0f, 0.0f, 0.0f),
                tcuExtent4,
            };

            ctx.vkd.cmdBeginRendering(cmdBuffer, &renderingInfo);
            ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *copyPipeline);
            ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *copyPipelineLayout, 0u, 1u, &copyDescriptorSet.get(),
                                          0u, nullptr);
            ctx.vkd.cmdPushConstants(cmdBuffer, *copyPipelineLayout, pcStages, 0u, pcSize, &pcValues);
            ctx.vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
            ctx.vkd.cmdEndRendering(cmdBuffer);
        }
    }

    // Copy attachments to verification buffers.
    {
        const auto xferLayout =
            (m_params.generalLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        std::vector<VkImage> verifiedImages;

        if (testColor)
        {
            verifiedImages.reserve(attCount);
            for (uint32_t i = 0u; i < attCount; ++i)
            {
                verifiedImages.push_back(isMultiSample ? expandedColorImages.at(i)->get() :
                                                         colorImages.at(m_params.getOutputAttForAtt(i))->get());
            }
        }
        else
        {
            verifiedImages.push_back(isMultiSample ? expandedDSImage->get() : dsImage->get());
        }

        std::vector<VkImageMemoryBarrier> barriers;
        barriers.reserve(attCount);

        const auto srcAccess =
            (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        const auto dstAccess = VK_ACCESS_TRANSFER_READ_BIT;
        const auto srcStage  = (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
        const auto dstStage  = VK_PIPELINE_STAGE_TRANSFER_BIT;

        if (testColor)
        {
            for (uint32_t i = 0u; i < attCount; ++i)
                barriers.push_back(makeImageMemoryBarrier(srcAccess, dstAccess, attLayout, xferLayout,
                                                          verifiedImages.at(i), colorSRR));
        }
        else
            barriers.push_back(
                makeImageMemoryBarrier(srcAccess, dstAccess, attLayout, xferLayout, verifiedImages.at(0u), dsSRR));

        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, srcStage, dstStage, de::dataOrNull(barriers),
                                      barriers.size());

        if (testColor)
        {
            const auto copyRegion = makeBufferImageCopy(expandedExtent, colorSRL);
            for (uint32_t i = 0u; i < attCount; ++i)
                ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, verifiedImages.at(i), xferLayout, verifBuffers.at(i)->get(), 1u,
                                             &copyRegion);
        }
        else
        {
            const auto depthCopyRegion   = makeBufferImageCopy(expandedExtent, depthSRL);
            const auto stencilCopyRegion = makeBufferImageCopy(expandedExtent, stencilSRL);

            uint32_t nextBuffer = 0u;

            if (testDepth)
                ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, verifiedImages.at(0u), xferLayout,
                                             verifBuffers.at(nextBuffer++)->get(), 1u, &depthCopyRegion);

            if (testStencil)
                ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, verifiedImages.at(0u), xferLayout,
                                             verifBuffers.at(nextBuffer++)->get(), 1u, &stencilCopyRegion);
        }

        const auto hostBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &hostBarrier);
    }

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Create reference images.
    std::vector<TextureLevelPtr> referenceLevels;
    referenceLevels.reserve(attCount);

    const tcu::IVec3 halfExtent(tcuExpandedExtent.x() / 2, tcuExpandedExtent.y(), tcuExpandedExtent.z());

    if (testColor)
    {
        for (uint32_t i = 0u; i < attCount; ++i)
        {
            referenceLevels.emplace_back(new tcu::TextureLevel(tcuAttFormat, tcuExpandedExtent.x(),
                                                               tcuExpandedExtent.y(), tcuExpandedExtent.z()));
            tcu::PixelBufferAccess reference = referenceLevels.back()->getAccess();
            const auto &bufferValues         = *loadBuffersData.at(i);

            for (int y = 0; y < tcuExpandedExtent.y(); ++y)
                for (int x = 0; x < tcuExpandedExtent.x(); ++x)
                {
                    if (x < halfExtent.x())
                    {
                        // Left half of the image will be covered by load buffer data, with swapped components.
                        // (See the fragment modification shader).
                        auto sampleIdx = y * tcuExpandedExtent.x() + x;
                        if (!!m_params.sampleId)
                        {
                            // Take one particular sample in the sample block by truncating the sample idx to the first
                            // one in the block and then choosing the target sample from it.
                            sampleIdx = (sampleIdx / m_params.samples) * m_params.samples + m_params.sampleId.get();
                        }
                        const auto sampleColor = bufferValues.at(sampleIdx).swizzle(1, 2, 0, 3); // .gbra
                        reference.setPixel(sampleColor, x, y);
                    }
                    else
                    {
                        // Right half of the image will be covered by the gradient.
                        // This mimics the fragment gradient shader.
                        // Note: all samples in the same pixel have the same color.
                        const float xCoord = static_cast<float>(x / static_cast<int>(imgCreateInfo.samples));
                        const float yCoord = static_cast<float>(y);
                        const tcu::Vec2 normalizedCoords(xCoord / tcuExtentF.x(), yCoord / tcuExtentF.y());
                        const float gradientValue = (normalizedCoords.x() + normalizedCoords.y()) / 2.0f;
                        tcu::Vec4 color(0.0f, 0.0f, 0.0f, 1.0f);
                        DE_ASSERT(i < 3u);
                        color[i] = gradientValue;
                        reference.setPixel(color, x, y);
                    }
                }
        }
    }

    if (testDepth)
    {
        referenceLevels.emplace_back(new tcu::TextureLevel(depthCopyFormat, tcuExpandedExtent.x(),
                                                           tcuExpandedExtent.y(), tcuExpandedExtent.z()));
        tcu::PixelBufferAccess reference = referenceLevels.back()->getAccess();
        const auto &bufferValues         = *loadBuffersData.at(0);

        for (int y = 0; y < tcuExpandedExtent.y(); ++y)
            for (int x = 0; x < tcuExpandedExtent.x(); ++x)
            {
                if (x < halfExtent.x())
                {
                    // Left half of the image will be covered by load buffer data, with complementary depth values.
                    // (See the fragment modification shader).
                    auto sampleIdx = y * tcuExpandedExtent.x() + x;
                    if (!!m_params.sampleId)
                    {
                        // Take one particular sample in the sample block by truncating the sample idx to the first one
                        // in the block and then choosing the target sample from it.
                        sampleIdx = (sampleIdx / m_params.samples) * m_params.samples + m_params.sampleId.get();
                    }
                    const auto sampleDepth = 1.0f - bufferValues.at(sampleIdx).x(); // Complementary, see shader.
                    reference.setPixDepth(sampleDepth, x, y);
                }
                else
                {
                    // Right half of the image will be covered by the gradient.
                    // This mimics the fragment gradient shader.
                    // Note: all samples in the same pixel have the same depth.
                    const float xCoord = static_cast<float>(x / static_cast<int>(imgCreateInfo.samples));
                    const float yCoord = static_cast<float>(y);
                    const tcu::Vec2 normalizedCoords(xCoord / tcuExtentF.x(), yCoord / tcuExtentF.y());
                    const float gradientValue = (normalizedCoords.x() + normalizedCoords.y()) / 2.0f;
                    reference.setPixDepth(gradientValue, x, y);
                }
            }
    }

    if (testStencil)
    {
        referenceLevels.emplace_back(new tcu::TextureLevel(stencilCopyFormat, tcuExpandedExtent.x(),
                                                           tcuExpandedExtent.y(), tcuExpandedExtent.z()));
        tcu::PixelBufferAccess reference = referenceLevels.back()->getAccess();
        const auto &bufferValues         = *loadBuffersData.at(0);

        for (int y = 0; y < tcuExpandedExtent.y(); ++y)
            for (int x = 0; x < tcuExpandedExtent.x(); ++x)
            {
                if (x < halfExtent.x())
                {
                    // Left half of the image will be covered by load buffer data, with complementary stencil values.
                    // (See the fragment modification shader).
                    auto sampleIdx = y * tcuExpandedExtent.x() + x;
                    if (!!m_params.sampleId)
                    {
                        // Take one particular sample in the sample block by truncating the sample idx to the first one
                        // in the block and then choosing the target sample from it.
                        sampleIdx = (sampleIdx / m_params.samples) * m_params.samples + m_params.sampleId.get();
                    }
                    const auto sampleStencil = 255 - int(bufferValues.at(sampleIdx).y()); // Complementary, see shader.
                    reference.setPixStencil(sampleStencil, x, y);
                }
                else
                {
                    // Right half of the image will be covered by the gradient.
                    // This mimics the fragment gradient shader.
                    // Note: all samples in the same pixel have the same stencil.
                    const float xCoord = static_cast<float>(x / static_cast<int>(imgCreateInfo.samples));
                    const float yCoord = static_cast<float>(y);
                    const tcu::Vec2 normalizedCoords(xCoord / tcuExtentF.x(), yCoord / tcuExtentF.y());
                    const float gradientValue = (normalizedCoords.x() + normalizedCoords.y()) / 2.0f;
                    const auto sampleStencil  = int(gradientValue * 255.0f);
                    reference.setPixStencil(sampleStencil, x, y);
                }
            }
    }

    // Check results.
    bool fail           = false;
    auto &log           = m_context.getTestContext().getLog();
    const auto logLevel = tcu::COMPARE_LOG_ON_ERROR;

    if (testColor)
    {
        DE_ASSERT(m_params.attFormat == VK_FORMAT_R8G8B8A8_UNORM);
        const float threshold = 2.0f / 0xff; // Max error for 8-bit unorm subtraction is 2/0xff
        const tcu::Vec4 thresholdVec(threshold, threshold, threshold, 0.0f); // Alpha is always 1.0

        for (uint32_t i = 0u; i < attCount; ++i)
        {
            auto &alloc = verifBuffers.at(i)->getAllocation();
            invalidateAlloc(ctx.vkd, ctx.device, alloc);

            tcu::ConstPixelBufferAccess result(tcuAttFormat, tcuExpandedExtent, alloc.getHostPtr());
            tcu::ConstPixelBufferAccess reference = referenceLevels.at(i)->getAccess();

            const auto setName = "ColorResult" + std::to_string(i);
            if (!tcu::floatThresholdCompare(log, setName.c_str(), "", reference, result, thresholdVec, logLevel))
                fail = true;
        }
    }

    if (testDepth)
    {
        auto &alloc = verifBuffers.front()->getAllocation();
        invalidateAlloc(ctx.vkd, ctx.device, alloc);

        tcu::ConstPixelBufferAccess result(depthCopyFormat, tcuExpandedExtent, alloc.getHostPtr());
        tcu::ConstPixelBufferAccess reference = referenceLevels.front()->getAccess();

        const auto &attFormat = m_params.attFormat;
        float threshold       = 0.0f;
        if (attFormat == VK_FORMAT_D16_UNORM)
            threshold = 2.0f / 0xffff; // Max error for 16-bit unorm subtraction is 2/0xffff
        else if (attFormat == VK_FORMAT_D24_UNORM_S8_UINT || attFormat == VK_FORMAT_D32_SFLOAT_S8_UINT)
            threshold = 2.0f / 0xffffff; // Max error for 24-bit unorm subtraction is 2/0xffffff
        else
            DE_ASSERT(false);

        const auto setName = "DepthResult";
        if (!tcu::dsThresholdCompare(log, setName, "", reference, result, threshold, logLevel))
            fail = true;
    }

    if (testStencil)
    {
        auto &alloc = verifBuffers.back()->getAllocation();
        invalidateAlloc(ctx.vkd, ctx.device, alloc);

        tcu::ConstPixelBufferAccess result(stencilCopyFormat, tcuExpandedExtent, alloc.getHostPtr());
        tcu::ConstPixelBufferAccess reference = referenceLevels.back()->getAccess();

        const float threshold = 0.0f; // This is not used when checking stencil.

        const auto setName = "StencilResult";
        if (!tcu::dsThresholdCompare(log, setName, "", reference, result, threshold, logLevel))
            fail = true;
    }

    if (fail)
        TCU_FAIL("Unexpected results in some color or depth/stencil buffers; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

std::string feedbackCaseName(const std::vector<bool> &feedback)
{
    std::string name = "loop_";
    for (const bool feedbackLoop : feedback)
        name += "NY"[static_cast<int>(feedbackLoop)];
    return name;
}

} // namespace

tcu::TestCaseGroup *createDynamicRenderingLocalReadMaint10Tests(tcu::TestContext &testCtx)
{
    GroupPtr mainGroup(
        new tcu::TestCaseGroup(testCtx, "m10_feedback_loop", "Test feedback loops with DRLR and maintenance10"));

    const std::vector<bool> feedbackLoops[] = {
        // clang-format off
        {false},        // One attachment, no feedback loop.
        {true},         // One attachment, feedback loop.
        {false, false}, // Two attachments, no feedback loops.
        {false, true},  // Two attachments, feedback loop on the second one.
        {true, false},  // Two attachments, feedback loop on the first one.
        {true, true},   // Two attachments, feedback loop on both.
        // clang-format on
    };

    for (const auto format : {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_D16_UNORM, VK_FORMAT_S8_UINT,
                              VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT})
        for (const auto sampleCount : {VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_4_BIT})
            for (const auto &feedbackCase : feedbackLoops)
                for (const auto sampleId : {-1, 0, 1, 2, 3})
                    for (const auto generalLayout : {false, true})
                    {
                        if (sampleId >= sampleCount || (sampleCount == VK_SAMPLE_COUNT_1_BIT && sampleId != -1))
                            continue;

                        // These cases cannot be tested with depth/stencil.
                        if (isDepthStencilFormat(format) && (feedbackCase.size() != 1 || !feedbackCase.front()))
                            continue;

                        const TestParams params{
                            sampleCount,   format, feedbackCase, ((sampleId < 0) ? tcu::Nothing : tcu::just(sampleId)),
                            generalLayout,
                        };
                        const auto testName =
                            getFormatSimpleName(format) + ("_samples_" + std::to_string(sampleCount)) + "_" +
                            feedbackCaseName(feedbackCase) +
                            (!!params.sampleId ? "_sample_" + std::to_string(params.sampleId.get()) : "") +
                            (generalLayout ? "_general_layout" : "");
                        mainGroup->addChild(new DRLRFeedbackLoopCase(testCtx, testName, params));
                    }

    return mainGroup.release();
}

} // namespace vkt::renderpass
