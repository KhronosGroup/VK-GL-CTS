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
 * \brief Tests with large input attachment indices.
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassInputAttachmentIndicesTests.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"

#include "deDefs.hpp"
#include "deRandom.hpp"

#include <vector>
#include <memory>

//#undef REQUIRE_MAINTENANCE_12
#define REQUIRE_MAINTENANCE_12 1

namespace vkt
{
namespace renderpass
{

namespace
{

using namespace vk;

void initQuadStripVertShader(vk::SourceCollections &dst)
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "void main(void) {\n"
         << "    const vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
         << "    gl_Position = vec4(pos * 2.0f - 1.0f, 0.0f, 1.0f);\n"
         << "}\n";
    dst.glslSources.add("vert") << glu::VertexSource(vert.str());
}

VkPipelineBindPoint getBindPoint()
{
    return VK_PIPELINE_BIND_POINT_GRAPHICS;
}

struct GeneralParams
{
    GeneralParams(uint32_t inputAttachmentCount_, bool readDepth_, bool readStencil_)
        : inputAttachmentCount(inputAttachmentCount_)
        , readDepth(readDepth_)
        , readStencil(readStencil_)
    {
    }

    virtual ~GeneralParams()
    {
    }

    uint32_t inputAttachmentCount; // How many input attachments will be in use in the frag shader.
    bool readDepth;                // Use a DS attachment and read depth from it as an input attachment.
    bool readStencil;              // Use a DS attachment and read stencil from it as an input attachment.

    virtual bool useDepthStencil() const
    {
        return (readDepth || readStencil);
    }

    virtual VkImageUsageFlags getColorBufferUsage() const
    {
        return static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                              VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    }

    virtual VkImageUsageFlags getColorInputAttachmentUsage() const
    {
        return static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                              VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    }

    virtual VkImageUsageFlags getDepthStencilInputAttachmentUsage() const
    {
        return static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                              VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    }

    virtual uint32_t getSeed() const
    {
        return 1784718288u;
    }

    virtual tcu::IVec3 getExtent() const
    {
        return tcu::IVec3(1, 1, 1);
    }

    virtual VkSampleCountFlagBits getSampleCount() const
    {
        return VK_SAMPLE_COUNT_1_BIT;
    }

    virtual VkFormat getColorFormat() const
    {
        return VK_FORMAT_R8G8B8A8_UNORM;
    }

    virtual VkFormat getDepthStencilFormat(const InstanceInterface &vki, VkPhysicalDevice physDevice) const
    {
        VkFormat dsFormat                  = VK_FORMAT_UNDEFINED;
        const VkImageCreateInfo createInfo = getImageCreateInfo(dsFormat, getDepthStencilInputAttachmentUsage());
        VkImageFormatProperties formatProperties;
        for (const auto format : {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT})
        {
            const auto result =
                vki.getPhysicalDeviceImageFormatProperties(physDevice, format, createInfo.imageType, createInfo.tiling,
                                                           createInfo.usage, createInfo.flags, &formatProperties);
            if (result == VK_SUCCESS)
                dsFormat = format;
        }
        return dsFormat;
    }

    virtual VkImageCreateInfo getImageCreateInfo(VkFormat format, VkImageUsageFlags usage) const
    {
        const VkImageCreateInfo info{
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            nullptr,
            0u,
            VK_IMAGE_TYPE_2D,
            format,
            makeExtent3D(getExtent()),
            1u,
            1u,
            getSampleCount(),
            VK_IMAGE_TILING_OPTIMAL,
            usage,
            VK_SHARING_MODE_EXCLUSIVE,
            0u,
            nullptr,
            VK_IMAGE_LAYOUT_UNDEFINED,
        };
        return info;
    }

    virtual VkImageLayout getInputAttachmentLayout() const
    {
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    virtual VkImageLayout getColorAttachmentLayout() const
    {
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    virtual uint32_t getColorInputAttachmentCount() const
    {
        uint32_t count = inputAttachmentCount;
        count -= static_cast<uint32_t>(readDepth);
        count -= static_cast<uint32_t>(readStencil);
        return count;
    }

    // Return the simple input attachment index for the depth input attachment, if used.
    virtual uint32_t getDepthInputAttachmentIndex() const
    {
        // If used, the depth input attachment always comes first after the color attachments.
        return (readDepth ? getColorInputAttachmentCount() : std::numeric_limits<uint32_t>::max());
    }

    // Return the simple input attachment index for the stencil input attachment, if used.
    virtual uint32_t getStencilInputAttachmentIndex() const
    {
        // If used, the stencil input attachment is always the last one.
        return (readStencil ? (inputAttachmentCount - 1u) : std::numeric_limits<uint32_t>::max());
    }

    // We have `inputAttachmentCount` input attachments used in the shader. In most parts of this code, each of them is
    // referred to by a simple internal input attachment index that goes from 0 to `inputAttachmentCount` - 1. However,
    // in the GLSL shader we have to assign an input_attachment_index value to each of them, and that is what we call
    // the "shader input attachment index". This method helps us map both values and get the "shader input attachment
    // index" from the simple input attachment index. You will see this method used when generating shader code and when
    // generating structures that need to match shader code, like the subpass input attachment array, or the list of
    // indices used for VkRenderingInputAttachmentIndexInfo.
    virtual uint32_t getShaderInputAttachmentIndex(uint32_t inputAttachmentIndex) const = 0;

    // Returns true if dynamic rendering should be used for the test.
    virtual bool useDynamicRendering() const = 0;

    // Returns the index of the first input attachment that is actually used in the shader. This is needed because in
    // the dynamic rendering case the first color attachment will be used for output only.
    virtual uint32_t getFirstUsedInputAttachmentIndex() const = 0;
};
using ParamsPtr = std::shared_ptr<const GeneralParams>;

void commonCheckSupport(Context &context, ParamsPtr params)
{
#ifdef REQUIRE_MAINTENANCE_12
    context.requireDeviceFunctionality("VK_KHR_maintenance12");
#endif

    const auto &limits = context.getDeviceProperties().limits;
    if (limits.maxPerStageDescriptorInputAttachments < params->inputAttachmentCount)
        TCU_THROW(NotSupportedError, "maxPerStageDescriptorInputAttachments too low");

    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_FRAGMENT_STORES_AND_ATOMICS);

    if (params->useDepthStencil())
    {
        const auto ctx      = context.getContextCommonData();
        const auto dsFormat = params->getDepthStencilFormat(ctx.vki, ctx.physicalDevice);
        if (dsFormat == VK_FORMAT_UNDEFINED)
            TCU_THROW(NotSupportedError, "No mandatory format supports the intended usage");
    }
}

void commonInitPrograms(vk::SourceCollections &dst, ParamsPtr params)
{
    initQuadStripVertShader(dst);

    const auto depthIndex   = params->getDepthInputAttachmentIndex();
    const auto stencilIndex = params->getStencilInputAttachmentIndex();

    const auto getInputAttName = [&](uint32_t attIndex) -> std::string
    {
        if (attIndex == depthIndex)
            return "iaDepth";
        if (attIndex == stencilIndex)
            return "iaStencil";
        return "iaColor" + std::to_string(attIndex);
    };

    const auto firstUsed = params->getFirstUsedInputAttachmentIndex();

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n";
    for (uint32_t i = firstUsed; i < params->inputAttachmentCount; ++i)
    {
        const auto shaderIndex = params->getShaderInputAttachmentIndex(i);
        const bool isStencil   = (i == stencilIndex);
        frag << "layout (set=0, binding=" << i << ", input_attachment_index=" << shaderIndex << ") uniform "
             << (isStencil ? "u" : "") << "subpassInput " << getInputAttName(i) << ";\n";
    }
    frag << "layout (set=1, binding=0, std430) buffer ReadBlock { vec4 values[]; } ssbo;\n";
    frag << "void main(void) {\n"
         << "    outColor = vec4(0.0, 0.0, 1.0, 1.0);\n";
    for (uint32_t i = 0u; i < firstUsed; ++i)
        frag << "    ssbo.values[0] = vec4(0.0, 0.0, 0.0, 1.0);\n";
    for (uint32_t i = firstUsed; i < params->inputAttachmentCount; ++i)
    {
        const bool isDepth   = (i == depthIndex);
        const bool isStencil = (i == stencilIndex);
        frag << "    ssbo.values[" << i << "] = ";
        if (isDepth)
            frag << "vec4(subpassLoad(" << getInputAttName(i) << ").x, 0.0, 0.0, 1.0)";
        else if (isStencil)
            frag << "vec4(float(subpassLoad(" << getInputAttName(i) << ").x) / 255.0, 0.0, 0.0, 1.0)";
        else
            frag << "subpassLoad(" << getInputAttName(i) << ")";
        frag << ";\n";
    }
    frag << "}\n";
    dst.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

namespace ClassicRenderPass
{

struct Params : public GeneralParams
{
    Params(uint32_t inputAttachmentCount_, uint32_t subpassInputAttachments_, bool readDepth_, bool readStencil_)
        : GeneralParams(inputAttachmentCount_, readDepth_, readStencil_)
        , subpassInputAttachments(subpassInputAttachments_)
    {
        DE_ASSERT(subpassInputAttachments >= inputAttachmentCount);
    }

    virtual ~Params()
    {
    }

    // Input attachment count in the subpass: attachments will be at the end of this. See the
    // getShaderInputAttachmentIndex() implementation below.
    uint32_t subpassInputAttachments;

    uint32_t getShaderInputAttachmentIndex(uint32_t inputAttachmentIndex) const override
    {
        DE_ASSERT(inputAttachmentIndex < inputAttachmentCount);
        return subpassInputAttachments - 1u - inputAttachmentIndex;
    }

    bool useDynamicRendering() const override
    {
        return false;
    }

    uint32_t getFirstUsedInputAttachmentIndex() const override
    {
        return 0u;
    }
};

Move<VkRenderPass> makeClassicRenderPass(const DeviceInterface &vkd, VkDevice device,
                                         const GeneralParams *generalParams, VkFormat dsFormat)
{
    const auto params = dynamic_cast<const Params *>(generalParams);
    DE_ASSERT(params);

    std::vector<VkAttachmentDescription> attDesc;
    attDesc.reserve(params->inputAttachmentCount + 1u /*color buffer*/);

    // Color attachment.
    const auto sampleCount    = params->getSampleCount();
    const auto colorFormat    = params->getColorFormat();
    const auto colorAttLayout = params->getColorAttachmentLayout();
    attDesc.push_back(makeAttachmentDescription(0u, colorFormat, sampleCount, VK_ATTACHMENT_LOAD_OP_LOAD,
                                                VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                VK_ATTACHMENT_STORE_OP_DONT_CARE, colorAttLayout, colorAttLayout));

    // Input attachments.
    const auto depthInputAttIndex   = params->getDepthInputAttachmentIndex();
    const auto stencilInputAttIndex = params->getStencilInputAttachmentIndex();
    const auto inputAttLayout       = params->getInputAttachmentLayout();

    for (uint32_t i = 0u; i < params->inputAttachmentCount; ++i)
    {
        const bool isStencil = (i == stencilInputAttIndex);

        const auto mainLoadOp     = (isStencil ? VK_ATTACHMENT_LOAD_OP_DONT_CARE : VK_ATTACHMENT_LOAD_OP_LOAD);
        const auto mainStoreOp    = (isStencil ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE);
        const auto stencilLoadOp  = (isStencil ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE);
        const auto stencilStoreOp = (isStencil ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE);

        const bool isDepth = (i == depthInputAttIndex);
        const auto format  = (isDepth || isStencil ? dsFormat : colorFormat);

        attDesc.push_back(makeAttachmentDescription(0u, format, sampleCount, mainLoadOp, mainStoreOp, stencilLoadOp,
                                                    stencilStoreOp, inputAttLayout, inputAttLayout));
    }

    // Make a large-ish array of input attachments, as described by the parameters, where many of them could be unused.
    const auto defaultInputAttRef = makeAttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED);
    std::vector<VkAttachmentReference> inputAttRefs(params->subpassInputAttachments, defaultInputAttRef);

    // Make changes for each input attachment.
    for (uint32_t i = 0u; i < params->inputAttachmentCount; ++i)
    {
        const auto shaderIndex = params->getShaderInputAttachmentIndex(i);
        auto &inputAttRef      = inputAttRefs.at(shaderIndex);
        inputAttRef.attachment = i + 1u; // Offset by one due to the color attachment description.
        inputAttRef.layout     = inputAttLayout;
    }

    const auto colorAttRef = makeAttachmentReference(0u, colorAttLayout);

    const VkSubpassDescription subpassDesc{
        0u, getBindPoint(), de::sizeU32(inputAttRefs), de::dataOrNull(inputAttRefs), 1u, &colorAttRef, nullptr, nullptr,
        0u, nullptr,
    };

    const VkRenderPassCreateInfo rpCreateInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        nullptr,
        0u,
        de::sizeU32(attDesc),
        de::dataOrNull(attDesc),
        1u,
        &subpassDesc,
        0u,
        nullptr,
    };

    return createRenderPass(vkd, device, &rpCreateInfo);
}

void checkSupport(Context &context, ParamsPtr paramsPtr)
{
    commonCheckSupport(context, paramsPtr);

    auto params = dynamic_cast<const Params *>(paramsPtr.get());
    DE_ASSERT(params);
    DE_UNREF(params); // For VulkanSC.

#ifdef REQUIRE_MAINTENANCE_12
#ifndef CTS_USES_VULKANSC
    const auto &m12Properties = context.getMaintenance12Properties();
    if (m12Properties.maxInputAttachmentIndex < params->subpassInputAttachments)
        TCU_THROW(NotSupportedError, "maxInputAttachmentIndex too low");
#endif // CTS_USES_VULKANSC
#else
    const auto &limits = context.getDeviceProperties().limits;
    if (params->subpassInputAttachments > limits.maxPerStageDescriptorInputAttachments)
        TCU_THROW(NotSupportedError, "maxPerStageDescriptorInputAttachments too low");
#endif
}

} // namespace ClassicRenderPass

namespace DynamicRendering
{

// In dynamic rendering we work with color attachments, and use mappings to access some of then as input attachments.
// The number of color attachments is way more limited, so the number of input attachments is also limited. However,
// we can assign arbitrary shader input attachment indices to each of them. We will always use color attachment 0 as
// the output color attachment, and we will not read from it.
struct Params : public GeneralParams
{
    Params(uint32_t inputAttachmentCount_, bool readDepth_, bool readStencil_, uint32_t topInputAttachmentIndex_)
        : GeneralParams(inputAttachmentCount_, readDepth_, readStencil_)
        , topInputAttachmentIndex(topInputAttachmentIndex_)
    {
    }

    virtual ~Params()
    {
    }

    // Top input attachment index used in the test.
    uint32_t topInputAttachmentIndex;

    uint32_t getShaderInputAttachmentIndex(uint32_t inputAttachmentIndex) const override
    {
        DE_ASSERT(inputAttachmentIndex < inputAttachmentCount);

        // If there's only one, it's the top index.
        if (inputAttachmentCount <= 1u)
            return topInputAttachmentIndex;

        // Else, we divide the range and calculate the index proportionally.
        const auto divisor = inputAttachmentCount - 1u;
        const auto step    = topInputAttachmentIndex / divisor;
        return ((inputAttachmentIndex >= divisor) ? topInputAttachmentIndex : (inputAttachmentIndex * step));
    }

    bool useDynamicRendering() const override
    {
        return true;
    }

    uint32_t getFirstUsedInputAttachmentIndex() const override
    {
        return 1u;
    }

    // Usage flags change with dynamic rendering, because the attachments will be color or depth stencil attachments and
    // input attachments at the same time.

    VkImageUsageFlags getColorBufferUsage() const override
    {
        return static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                              VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                              VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    }

    VkImageUsageFlags getColorInputAttachmentUsage() const override
    {
        return getColorBufferUsage();
    }

    VkImageUsageFlags getDepthStencilInputAttachmentUsage() const override
    {
        return static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                              VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                              VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    }

    // Unified layout from VK_KHR_dynamic_rendering_local_read.

    VkImageLayout getInputAttachmentLayout() const override
    {
        return VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ;
    }

    VkImageLayout getColorAttachmentLayout() const override
    {
        return VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ;
    }
};

void checkSupport(Context &context, ParamsPtr paramsPtr)
{
    commonCheckSupport(context, paramsPtr);

    const auto params = dynamic_cast<const Params *>(paramsPtr.get());
    DE_ASSERT(params);
    DE_UNREF(params); // For VulkanSC.

    context.requireDeviceFunctionality("VK_KHR_dynamic_rendering_local_read");

    // Check that the implementation supports the given color attachment count.
    const auto colorInputAttCount = params->getColorInputAttachmentCount();
    const auto &limits            = context.getDeviceProperties().limits;
    if (colorInputAttCount > limits.maxColorAttachments)
        TCU_THROW(NotSupportedError, "maxColorAttachments too low");

#ifdef REQUIRE_MAINTENANCE_12
#ifndef CTS_USES_VULKANSC
    const auto &m12Properties = context.getMaintenance12Properties();
    if (params->topInputAttachmentIndex >= m12Properties.maxInputAttachmentIndex)
        TCU_THROW(NotSupportedError, "maxInputAttachmentIndex too low");
#endif // CTS_USES_VULKANSC
#else
    if (params->topInputAttachmentIndex >= limits.maxPerStageDescriptorInputAttachments)
        TCU_THROW(NotSupportedError, "maxPerStageDescriptorInputAttachments too low");
#endif
}

VkRenderingAttachmentInfo makeRenderingAttachmentInfo(VkImageView imageView, VkImageLayout layout)
{
    VkRenderingAttachmentInfo info = initVulkanStructure();
    info.imageView                 = imageView;
    info.imageLayout               = layout;
    info.loadOp                    = VK_ATTACHMENT_LOAD_OP_LOAD;
    info.storeOp                   = VK_ATTACHMENT_STORE_OP_STORE;
    return info;
}

struct FramebufferInfo
{
    std::vector<VkRenderingAttachmentInfo> renderingAttInfos;
    std::vector<VkFormat> renderingFormats;
    std::vector<uint32_t> renderingInputAttIndices;

    VkRenderingInfo renderingInfo                                         = initVulkanStructure();
    VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo             = initVulkanStructure();
    VkRenderingInputAttachmentIndexInfo renderingInputAttachmentIndexInfo = initVulkanStructure();

    void *pNext = nullptr;

    FramebufferInfo(const VkRect2D &renderArea, uint32_t arrayLayers, const GeneralParams *params,
                    VkImageView colorView, VkImageView dsView, VkFormat dsFormat,
                    const std::vector<VkImageView> &inputAttachmentViews)
    {
        renderingInfo.renderArea = renderArea;
        renderingInfo.layerCount = arrayLayers;

        renderingAttInfos.reserve(params->inputAttachmentCount);
        renderingFormats.reserve(params->inputAttachmentCount);
        renderingInputAttIndices.reserve(params->inputAttachmentCount);

        const auto firstUsedInputAttIndex = params->getFirstUsedInputAttachmentIndex();
        const auto colorAttLayout         = params->getColorAttachmentLayout();
        const auto colorFormat            = params->getColorFormat();
        const auto depthInputAttIndex     = params->getDepthInputAttachmentIndex();
        const auto stencilInputAttIndex   = params->getStencilInputAttachmentIndex();
        const auto inputAttLayout         = params->getInputAttachmentLayout();
        const auto colorInputAttCount     = params->getColorInputAttachmentCount();

        for (uint32_t i = 0u; i < firstUsedInputAttIndex; ++i)
        {
            renderingAttInfos.push_back(DynamicRendering::makeRenderingAttachmentInfo(colorView, colorAttLayout));
            renderingFormats.push_back(colorFormat);
            renderingInputAttIndices.push_back(i);
        }

        bool dsIncluded = false;
        for (uint32_t i = firstUsedInputAttIndex; i < params->inputAttachmentCount; ++i)
        {
            // Note in the framebuffer we will not use separate views for depth/stencil. We will use a dual-aspect
            // image and include it only once.
            if (i == depthInputAttIndex || i == stencilInputAttIndex)
            {
                if (!dsIncluded)
                {
                    renderingAttInfos.push_back(DynamicRendering::makeRenderingAttachmentInfo(dsView, inputAttLayout));
                    renderingFormats.push_back(dsFormat);
                    dsIncluded = true;
                }
            }
            else
            {
                renderingAttInfos.push_back(
                    DynamicRendering::makeRenderingAttachmentInfo(inputAttachmentViews.at(i), inputAttLayout));
                renderingFormats.push_back(colorFormat);
            }

            renderingInputAttIndices.push_back(params->getShaderInputAttachmentIndex(i));
        }

        renderingInfo.colorAttachmentCount = colorInputAttCount;
        renderingInfo.pColorAttachments    = de::dataOrNull(renderingAttInfos);
        renderingInfo.pDepthAttachment     = (params->readDepth ? &renderingAttInfos.back() : nullptr);
        renderingInfo.pStencilAttachment   = (params->readStencil ? &renderingAttInfos.back() : nullptr);

        pipelineRenderingCreateInfo.colorAttachmentCount    = colorInputAttCount;
        pipelineRenderingCreateInfo.pColorAttachmentFormats = de::dataOrNull(renderingFormats);
        pipelineRenderingCreateInfo.depthAttachmentFormat =
            (params->readDepth ? renderingFormats.back() : VK_FORMAT_UNDEFINED);
        pipelineRenderingCreateInfo.stencilAttachmentFormat =
            (params->readStencil ? renderingFormats.back() : VK_FORMAT_UNDEFINED);

        renderingInputAttachmentIndexInfo.colorAttachmentCount         = colorInputAttCount;
        renderingInputAttachmentIndexInfo.pColorAttachmentInputIndices = de::dataOrNull(renderingInputAttIndices);
        renderingInputAttachmentIndexInfo.pDepthInputAttachmentIndex =
            (params->readDepth ? &renderingInputAttIndices.at(depthInputAttIndex) : nullptr);
        renderingInputAttachmentIndexInfo.pStencilInputAttachmentIndex =
            (params->readStencil ? &renderingInputAttIndices.at(stencilInputAttIndex) : nullptr);

        // String structures together. Notice we leave renderingInputAttachmentIndexInfo at the end of the chain so its
        // pNext pointer is null and it can be used directly in its corresponding vkCmdSet* command.
        pipelineRenderingCreateInfo.pNext = &renderingInputAttachmentIndexInfo;
        pNext                             = &pipelineRenderingCreateInfo;
    }
};

} // namespace DynamicRendering

tcu::TestStatus runTest(Context &context, ParamsPtr params)
{
    const auto ctx                    = context.getContextCommonData();
    const auto colorFormat            = VK_FORMAT_R8G8B8A8_UNORM;
    const auto extent                 = params->getExtent();
    const auto sampleCount            = params->getSampleCount();
    const auto colorInputAttUse       = params->getColorInputAttachmentUsage();
    const auto dsInputAttUse          = params->getDepthStencilInputAttachmentUsage();
    const bool useDynamicRendering    = params->useDynamicRendering();
    const auto inputAttLayout         = params->getInputAttachmentLayout();
    const auto colorAttLayout         = params->getColorAttachmentLayout();
    const auto firstUsedInputAttIndex = params->getFirstUsedInputAttachmentIndex();

    // Output buffer.
    const auto outputBufferSize  = static_cast<VkDeviceSize>(params->inputAttachmentCount * DE_SIZEOF32(tcu::Vec4));
    const auto outputBufferUsage = static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const auto outputBufferInfo  = makeBufferCreateInfo(outputBufferSize, outputBufferUsage);
    BufferWithMemory outputBuffer(ctx.vkd, ctx.device, ctx.allocator, outputBufferInfo, HostIntent::R);

    // Output color attachment.
    const auto extentVk         = makeExtent3D(extent);
    const auto colorBufferUsage = params->getColorBufferUsage();
    const auto imageType        = VK_IMAGE_TYPE_2D;
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, extentVk, colorFormat, colorBufferUsage, imageType);

    // Select DS format.
    VkFormat dsFormat = params->getDepthStencilFormat(ctx.vki, ctx.physicalDevice);

    // Create input attachments.
    using ImagePtr     = std::unique_ptr<Move<VkImage>>;
    using ImageViewPtr = std::unique_ptr<Move<VkImageView>>;

    std::vector<ImagePtr> inputAttachments;
    std::vector<ImageViewPtr> inputAttachmentViews;
    std::vector<VkFormat> inputAttachmentFormats;
    inputAttachments.reserve(params->inputAttachmentCount);
    inputAttachmentViews.reserve(params->inputAttachmentCount);
    inputAttachmentFormats.reserve(params->inputAttachmentCount);

    const auto colorCreateInfo = params->getImageCreateInfo(colorFormat, colorInputAttUse);
    const auto imageViewType   = VK_IMAGE_VIEW_TYPE_2D;

    Move<VkImageView> depthOnlyView;
    Move<VkImageView> stencilOnlyView;

    const auto colorInputAttCount   = params->getColorInputAttachmentCount();
    const auto depthInputAttIndex   = params->getDepthInputAttachmentIndex();
    const auto stencilInputAttIndex = params->getStencilInputAttachmentIndex();

    const auto dsCreateInfo = params->getImageCreateInfo(dsFormat, dsInputAttUse);
    const auto colorSRR     = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, colorCreateInfo.mipLevels, 0u,
                                                        colorCreateInfo.arrayLayers);
    const auto depthSRR =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, dsCreateInfo.mipLevels, 0u, dsCreateInfo.arrayLayers);
    const auto stencilSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_STENCIL_BIT, 0u, dsCreateInfo.mipLevels, 0u,
                                                      dsCreateInfo.arrayLayers);
    const auto dsSRR      = makeImageSubresourceRange((VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT), 0u,
                                                      dsCreateInfo.mipLevels, 0u, dsCreateInfo.arrayLayers);

    // We may have thousands of color input attachments, so we allocate all their memory at the same time.
    de::MovePtr<Allocation> colorInputAttAlloc;
    VkDeviceSize colorInputAttAllocStride = 0ull; // Size for each image chunk in memory.
    de::MovePtr<Allocation> dsInputAttAlloc;

    // When using dynamic rendering, we need a multiaspect view for the framebuffer.
    Move<VkImageView> dsView;

    for (uint32_t i = 0u; i < params->inputAttachmentCount; ++i)
    {
        if (i == depthInputAttIndex || i == stencilInputAttIndex)
        {
            // For depth/stencil we only have one image.
            if (!dsInputAttAlloc)
            {
                DE_ASSERT(dsFormat != VK_FORMAT_UNDEFINED);
                inputAttachments.emplace_back(new Move<VkImage>(createImage(ctx.vkd, ctx.device, &dsCreateInfo)));
                const auto memReqs = getImageMemoryRequirements(ctx.vkd, ctx.device, inputAttachments.back()->get());
                dsInputAttAlloc    = ctx.allocator.allocate(memReqs, MemoryRequirement::Any);
                VK_CHECK(ctx.vkd.bindImageMemory(ctx.device, inputAttachments.back()->get(),
                                                 dsInputAttAlloc->getMemory(), 0ull));

                if (useDynamicRendering)
                    dsView = makeImageView(ctx.vkd, ctx.device, inputAttachments.back()->get(), imageViewType, dsFormat,
                                           dsSRR);
            }

            // But maybe multiple views.
            const auto srr = (i == depthInputAttIndex ? depthSRR : stencilSRR);
            inputAttachmentViews.emplace_back(new Move<VkImageView>(
                makeImageView(ctx.vkd, ctx.device, inputAttachments.back()->get(), imageViewType, dsFormat, srr)));
            inputAttachmentFormats.push_back(dsFormat);
        }
        else
        {
            inputAttachments.emplace_back(new Move<VkImage>(createImage(ctx.vkd, ctx.device, &colorCreateInfo)));
            if (!colorInputAttAlloc)
            {
                auto memReqs = getImageMemoryRequirements(ctx.vkd, ctx.device, inputAttachments.back()->get());
                colorInputAttAllocStride = de::roundUp(memReqs.size, memReqs.alignment);
                const auto fullSize      = colorInputAttAllocStride * colorInputAttCount; // Size for all images.
                memReqs.size             = fullSize;
                colorInputAttAlloc       = ctx.allocator.allocate(memReqs, MemoryRequirement::Any);
            }
            VK_CHECK(ctx.vkd.bindImageMemory(ctx.device, inputAttachments.back()->get(),
                                             colorInputAttAlloc->getMemory(), i * colorInputAttAllocStride));
            inputAttachmentViews.emplace_back(new Move<VkImageView>(makeImageView(
                ctx.vkd, ctx.device, inputAttachments.back()->get(), imageViewType, colorFormat, colorSRR)));
            inputAttachmentFormats.push_back(colorFormat);
        }
    }

    const std::vector<VkViewport> viewports(1u, makeViewport(extent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(extent));

    // Create render pass and framebuffer if needed.
    Move<VkRenderPass> renderPass;
    Move<VkFramebuffer> framebuffer;
    std::unique_ptr<DynamicRendering::FramebufferInfo> dynamicRenderingFbInfo;

    std::vector<VkImageView> inputAttViewHandles;
    inputAttViewHandles.reserve(inputAttachmentViews.size());
    for (const auto &viewPtr : inputAttachmentViews)
        inputAttViewHandles.push_back(viewPtr->get());

    if (useDynamicRendering)
    {
        dynamicRenderingFbInfo.reset(
            new DynamicRendering::FramebufferInfo(scissors.front(), colorCreateInfo.arrayLayers, params.get(),
                                                  colorBuffer.getImageView(), *dsView, dsFormat, inputAttViewHandles));
    }
    else
    {
        renderPass = ClassicRenderPass::makeClassicRenderPass(ctx.vkd, ctx.device, params.get(), dsFormat);

        std::vector<VkImageView> framebufferViews;
        framebufferViews.reserve(inputAttViewHandles.size() + 1);
        framebufferViews.push_back(colorBuffer.getImageView());
        framebufferViews.insert(framebufferViews.end(), inputAttViewHandles.begin(), inputAttViewHandles.end());

        framebuffer =
            makeFramebuffer(ctx.vkd, ctx.device, *renderPass, de::sizeU32(framebufferViews),
                            de::dataOrNull(framebufferViews), extentVk.width, extentVk.height, extentVk.depth);
    }

    // Descriptor sets.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, params->inputAttachmentCount);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u); // Output buffer.
    const auto descPool = poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 2u);
    const auto descriptorStages = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_FRAGMENT_BIT);

    // Set with input attachments.
    Move<VkDescriptorSetLayout> iaSetLayout;
    {
        DescriptorSetLayoutBuilder setLayoutBuilder;
        for (uint32_t i = 0u; i < params->inputAttachmentCount; ++i)
            setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, descriptorStages);
        iaSetLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);
    }

    // Set with output buffer.
    Move<VkDescriptorSetLayout> outSetLayout;
    {
        DescriptorSetLayoutBuilder setLayoutBuilder;
        setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptorStages);
        outSetLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);
    }

    const auto iaSet  = makeDescriptorSet(ctx.vkd, ctx.device, *descPool, *iaSetLayout);
    const auto outSet = makeDescriptorSet(ctx.vkd, ctx.device, *descPool, *outSetLayout);

    DescriptorSetUpdateBuilder updateBuilder;
    const auto binding = DescriptorSetUpdateBuilder::Location::binding;
    for (uint32_t i = 0u; i < params->inputAttachmentCount; ++i)
    {
        const auto descInfo =
            makeDescriptorImageInfo(VK_NULL_HANDLE, inputAttachmentViews.at(i)->get(), inputAttLayout);
        updateBuilder.writeSingle(*iaSet, binding(i), VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &descInfo);
    }
    {
        const auto descInfo = makeDescriptorBufferInfo(*outputBuffer, 0u, VK_WHOLE_SIZE);
        updateBuilder.writeSingle(*outSet, binding(0), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descInfo);
    }
    updateBuilder.update(ctx.vkd, ctx.device);

    const std::vector<VkDescriptorSetLayout> setLayouts{*iaSetLayout, *outSetLayout};
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, setLayouts);
    const std::vector<VkDescriptorSet> descriptorSets{*iaSet, *outSet};

    const auto &binaries  = context.getBinaryCollection();
    const auto vertShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();
    const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        0u,
        sampleCount,
        VK_FALSE,
        0.0f,
        nullptr,
        VK_FALSE,
        VK_FALSE,
    };
    const auto topology    = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    const auto vertexCount = 4u; // See vertex shader.

    VkPipelineColorBlendAttachmentState blendAttState;
    memset(&blendAttState, 0, sizeof(blendAttState));
    blendAttState.colorWriteMask = 0xFu;

    const uint32_t blendAttCount =
        (useDynamicRendering ? dynamicRenderingFbInfo->pipelineRenderingCreateInfo.colorAttachmentCount : 1u);
    const std::vector<VkPipelineColorBlendAttachmentState> blendAttStates(blendAttCount, blendAttState);

    const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        nullptr,
        0u,
        VK_FALSE,
        VK_LOGIC_OP_CLEAR,
        de::sizeU32(blendAttStates),
        de::dataOrNull(blendAttStates),
        {0.0f, 0.0f, 0.0f, 0.0f},
    };

    const auto pipelinePNext = (useDynamicRendering ? dynamicRenderingFbInfo->pNext : nullptr);
    const auto pipeline      = makeGraphicsPipeline(
        ctx.vkd, ctx.device, *pipelineLayout, *vertShader, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, *fragShader,
        *renderPass, viewports, scissors, topology, 0u, 0u, &vertexInputStateCreateInfo, nullptr,
        &multisampleStateCreateInfo, nullptr, &colorBlendStateCreateInfo, nullptr, pipelinePNext);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    std::vector<tcu::Vec4> expectedValues; // For the output buffer.
    expectedValues.reserve(params->inputAttachmentCount);

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    {
        // Move every image to the right layout and clear it with some colors.
        std::vector<VkImageMemoryBarrier> preClearBarriers;
        std::vector<VkImageMemoryBarrier> postClearBarriers;
        preClearBarriers.reserve(params->inputAttachmentCount + 1u);
        postClearBarriers.reserve(params->inputAttachmentCount + 1u);

        const auto xferAccess = static_cast<VkAccessFlags>(VK_ACCESS_TRANSFER_WRITE_BIT);
        const auto xferStages = static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_TRANSFER_BIT);
        const auto colorAccess =
            static_cast<VkAccessFlags>(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
        const auto dsAccess   = static_cast<VkAccessFlags>(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        const auto iaAccess   = (static_cast<VkAccessFlags>(VK_ACCESS_SHADER_READ_BIT) | colorAccess |
                               dsAccess); // Input attachments are also loaded.
        const auto postStages = static_cast<VkPipelineStageFlags>(
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        const auto clearLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        preClearBarriers.emplace_back(makeImageMemoryBarrier(0u, xferAccess, VK_IMAGE_LAYOUT_UNDEFINED, clearLayout,
                                                             colorBuffer.getImage(), colorSRR));
        postClearBarriers.emplace_back(makeImageMemoryBarrier(xferAccess, colorAccess, clearLayout, colorAttLayout,
                                                              colorBuffer.getImage(), colorSRR));

        for (size_t i = 0; i < inputAttachments.size(); ++i)
        {
            const bool isDS = (i == inputAttachments.size() - 1 && (params->readDepth || params->readStencil));
            const auto srr  = (isDS ? dsSRR : colorSRR);
            preClearBarriers.emplace_back(makeImageMemoryBarrier(0u, xferAccess, VK_IMAGE_LAYOUT_UNDEFINED, clearLayout,
                                                                 inputAttachments.at(i)->get(), srr));
            postClearBarriers.emplace_back(makeImageMemoryBarrier(xferAccess, iaAccess, clearLayout, inputAttLayout,
                                                                  inputAttachments.at(i)->get(), srr));
        }

        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, xferStages,
                                      preClearBarriers.data(), preClearBarriers.size());
        {
            // Color attachment.
            const auto clearColor = makeClearValueColorVec4(tcu::Vec4(0.0f));
            ctx.vkd.cmdClearColorImage(cmdBuffer, colorBuffer.getImage(), clearLayout, &clearColor.color, 1u,
                                       &colorSRR);
        }

        de::Random rng(params->getSeed());
        for (size_t i = 0; i < inputAttachments.size(); ++i)
        {
            const auto imgHandle = inputAttachments.at(i)->get();
            const bool isDS      = (i == inputAttachments.size() - 1 && (params->readDepth || params->readStencil));
            const auto srr       = (isDS ? dsSRR : colorSRR);
            if (isDS)
            {
                const float depth     = rng.getFloat();
                const int stencil     = rng.getInt(0, 255);
                const auto clearColor = makeClearDepthStencilValue(depth, static_cast<uint32_t>(stencil));
                ctx.vkd.cmdClearDepthStencilImage(cmdBuffer, imgHandle, clearLayout, &clearColor, 1u, &srr);
                if (params->readDepth)
                    expectedValues.emplace_back(depth, 0.0f, 0.0f, 1.0f);
                if (params->readStencil)
                    expectedValues.emplace_back(static_cast<float>(stencil) / 255.0f, 0.0f, 0.0f, 1.0f);
            }
            else
            {
                if (i < firstUsedInputAttIndex)
                    expectedValues.emplace_back(0.0, 0.0f, 0.0f, 1.0f); // See fragment shader.
                else
                {
                    const float r = rng.getFloat();
                    const float g = rng.getFloat();
                    const float b = rng.getFloat();
                    expectedValues.emplace_back(r, g, b, 1.0f);
                }

                const auto clearColor = makeClearValueColor(expectedValues.back());
                ctx.vkd.cmdClearColorImage(cmdBuffer, imgHandle, clearLayout, &clearColor.color, 1u, &srr);
            }
        }

        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, xferStages, postStages, postClearBarriers.data(),
                                      postClearBarriers.size());
    }
    const auto bindPoint = getBindPoint();
    if (useDynamicRendering)
    {
        ctx.vkd.cmdBeginRendering(cmdBuffer, &(dynamicRenderingFbInfo->renderingInfo));
        ctx.vkd.cmdSetRenderingInputAttachmentIndices(cmdBuffer,
                                                      &(dynamicRenderingFbInfo->renderingInputAttachmentIndexInfo));
    }
    else
        beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.front());
    ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, de::sizeU32(descriptorSets),
                                  de::dataOrNull(descriptorSets), 0u, nullptr);
    ctx.vkd.cmdDraw(cmdBuffer, vertexCount, 1u, 0u, 0u);
    if (useDynamicRendering)
        ctx.vkd.cmdEndRendering(cmdBuffer);
    else
        endRenderPass(ctx.vkd, cmdBuffer);

    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), extent.swizzle(0, 1),
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, colorAttLayout);

    {
        // Sync output buffer with host reads.
        const auto hostBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &hostBarrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    auto &colorBufferAlloc = colorBuffer.getBufferAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, colorBufferAlloc);
    const auto tcuColorFormat = mapVkFormat(colorFormat);
    tcu::ConstPixelBufferAccess colorResult(tcuColorFormat, extent, colorBufferAlloc.getHostPtr());

    tcu::TextureLevel colorRefLevel(tcuColorFormat, extent.x(), extent.y(), extent.z());
    tcu::PixelBufferAccess colorReference = colorRefLevel.getAccess();
    const tcu::Vec4 expectedColor(0.0f, 0.0f, 1.0f, 1.0f);
    tcu::clear(colorReference, expectedColor);

    auto &log = context.getTestContext().getLog();
    bool fail = false;

    const tcu::Vec4 colorThreshold(0.0f);
    if (!tcu::floatThresholdCompare(log, "ColorBuffer", "", colorReference, colorResult, colorThreshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        fail = true;

    std::vector<tcu::Vec4> bufferValues;
    bufferValues.resize(params->inputAttachmentCount);
    auto &outputBufferAlloc = outputBuffer.getAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, outputBufferAlloc);
    memcpy(bufferValues.data(), outputBufferAlloc.getHostPtr(), de::dataSize(bufferValues));

    const float rgbThreshold = 0.005f; // 1/255 < 0.005 < 2/255
    const tcu::Vec4 bufferThreshold(rgbThreshold, rgbThreshold, rgbThreshold, 0.0f);

    for (size_t i = 0; i < bufferValues.size(); ++i)
    {
        const auto &ref = expectedValues.at(i);
        const auto &res = bufferValues.at(i);

        if (tcu::boolAny(tcu::greaterThan(tcu::abs(ref - res), bufferThreshold)))
        {
            fail = true;
            std::ostringstream msg;
            msg << "Unexpected value in output buffer item " << i << ": expected " << ref << " but found " << res;
            log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
        }
    }

    if (fail)
        TCU_FAIL("Unexpected values in color attachment or output buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

} // anonymous namespace

tcu::TestCaseGroup *createRenderPassInputAttachmentIndicesTests(tcu::TestContext &testCtx)
{
    return createTestGroup2(
        testCtx, "input_attachment_indices",
        [](tcu::TestCaseGroup *mainGroup)
        {
            mainGroup->addChild(createTestGroup2(
                mainGroup->getTestContext(), "render_pass",
                [](tcu::TestCaseGroup *classicGroup)
                {
                    for (const uint32_t inputAttachmentCount : {4u, 8u, 64u, 128u, 256u, 512u})
                    {
                        const auto iaCountName = "input_attachment_count_" + std::to_string(inputAttachmentCount);
                        classicGroup->addChild(createTestGroup2(
                            classicGroup->getTestContext(), iaCountName,
                            [=](tcu::TestCaseGroup *iaCountGroup)
                            {
                                for (uint32_t subpassInputAttachments = inputAttachmentCount;
                                     subpassInputAttachments <= 65536u; subpassInputAttachments *= 4u)
                                    for (const bool readDepth : {false, true})
                                        for (const bool readStencil : {false, true})
                                        {
                                            ParamsPtr params(new ClassicRenderPass::Params{
                                                inputAttachmentCount,
                                                subpassInputAttachments,
                                                readDepth,
                                                readStencil,
                                            });
                                            const auto testName =
                                                "subpass_input_attachments_" + std::to_string(subpassInputAttachments) +
                                                (readDepth ? "_read_depth" : "") + (readStencil ? "_read_stencil" : "");
                                            addFunctionCaseWithPrograms(iaCountGroup, testName,
                                                                        ClassicRenderPass::checkSupport,
                                                                        commonInitPrograms, runTest, params);
                                        }
                            }));
                    }
                }));

            mainGroup->addChild(createTestGroup2(
                mainGroup->getTestContext(), "dynamic_rendering",
                [](tcu::TestCaseGroup *drGroup)
                {
                    for (const uint32_t inputAttachmentCount : {4u, 8u})
                        for (const uint32_t topInputAttachmentIndex : {3u, 4u, 6u, 7u, 8u, 63u, 127u, 255u, 999999u})
                        {
                            if (topInputAttachmentIndex <= inputAttachmentCount)
                                continue;

                            for (const bool readDepth : {false, true})
                                for (const bool readStencil : {false, true})
                                {
                                    ParamsPtr params(new DynamicRendering::Params{
                                        inputAttachmentCount,
                                        readDepth,
                                        readStencil,
                                        topInputAttachmentIndex,
                                    });
                                    const auto testName = "input_attachments_" + std::to_string(inputAttachmentCount) +
                                                          "_top_index_" + std::to_string(topInputAttachmentIndex) +
                                                          (readDepth ? "_read_depth" : "") +
                                                          (readStencil ? "_read_stencil" : "");

                                    addFunctionCaseWithPrograms(drGroup, testName, DynamicRendering::checkSupport,
                                                                commonInitPrograms, runTest, params);
                                }
                        }
                }));
        });
}

} // namespace renderpass
} // namespace vkt
