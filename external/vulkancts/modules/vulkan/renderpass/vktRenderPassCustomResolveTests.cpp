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
 * \brief Tests for VK_EXT_custom_resolve
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassCustomResolveTests.hpp"

#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"

#include "deRandom.hpp"

#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <vector>

namespace vkt
{
namespace renderpass
{

using namespace vk;

namespace
{

// To upload data, we will use one buffer per color attachment.
// The buffer will contain one color per sample. If (x, y, s) specifies the
// column, row and sample coordinates, the buffer will contain the values
// for each row, and inside each row for each column, and in each row and column
// for each sample.
// (0,0,s0) (0,0,s1) ... (0,1,s0) ... (the whole row) ... (1,0,s0) ...

// There will be a quad of vertices, forming a triangle strip, from (0,0) to
// (1,1). The quad will be scaled and offsetted into the target position for
// each upload or resolve.
struct CoveredArea
{
    tcu::Vec2 scale;
    tcu::Vec2 offset;

    CoveredArea() : scale(1.0f, 1.0f), offset(0.0f, 0.0f)
    {
    }

    CoveredArea(tcu::Vec2 scale_, tcu::Vec2 offset_) : scale(scale_), offset(offset_)
    {
    }
};

// References an attachment index and a set of aspects that are affected by an upload or resolve operation.
struct AttachmentIndexAspect
{
    uint32_t index;
    VkImageAspectFlags aspects;

    AttachmentIndexAspect() : index(std::numeric_limits<uint32_t>::max()), aspects(0u)
    {
    }

    AttachmentIndexAspect(uint32_t index_, VkImageAspectFlags aspects_) : index(index_), aspects(aspects_)
    {
    }

    bool operator<(const AttachmentIndexAspect &other) const
    {
        if (index < other.index)
            return true;
        if (index > other.index)
            return false;
        return (aspects < other.aspects);
    }

    bool operator==(const AttachmentIndexAspect &other) const
    {
        return (index == other.index && aspects == other.aspects);
    }
};

// Each upload pass will be a render pass or subpass uploading data to a region of a subset of the attachments. Note all
// attachments in the same upload pass should have the same sample count so it matches the one from the pipeline.
struct UploadPass
{
    CoveredArea area;
    std::vector<AttachmentIndexAspect> attachments; // Attachment indices used.
};

// Each resolve operation will resolve attachments in a covered area and,
// for each attachment, a strategy will be used: fixed value, average or
// specific sample.
enum class ResolveType
{
    AVERAGE = 0, // Does not need parameters.
    FIXED_VALUE,
    SELECTED_SAMPLE,
};

union StrategyParams
{
    tcu::Vec4 fixedValue;
    uint32_t sampleIndex;

    StrategyParams()
    {
        memset((void *)this, 0, sizeof(*this));
    }

    StrategyParams(const tcu::Vec4 &fixedValue_) : fixedValue(fixedValue_)
    {
    }

    StrategyParams(uint32_t sampleIndex_) : fixedValue(0.0f) // Zeroes-out padding bytes.
    {
        sampleIndex = sampleIndex_;
    }
};

struct AttachmentResolve
{
    AttachmentIndexAspect attachment;
    ResolveType resolveType;
    StrategyParams resolveParams;

    AttachmentResolve() : attachment(), resolveType(ResolveType::AVERAGE), resolveParams()
    {
    }

    AttachmentResolve(uint32_t index, VkImageAspectFlags aspects, ResolveType rt, const StrategyParams &rp)
        : attachment(index, aspects)
        , resolveType(rt)
        , resolveParams(rp)
    {
    }
};

struct ResolvePass
{
    CoveredArea area;
    std::vector<AttachmentResolve> attachmentResolves;
};

// Putting it all together. Note we should not attempt to resolve an area and
// attachment that is not covered by an upload pass.

struct AttachmentInfo
{
    VkFormat attachmentFormat;
    VkSampleCountFlagBits sampleCount;
    VkFormat resolveFormat;
    uint32_t resolveLocation;

    AttachmentInfo()
        : attachmentFormat(VK_FORMAT_UNDEFINED)
        , sampleCount(VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM)
        , resolveFormat(VK_FORMAT_UNDEFINED)
        , resolveLocation(std::numeric_limits<uint32_t>::max())
    {
    }

    AttachmentInfo(VkFormat format, VkSampleCountFlagBits samples, VkFormat resolveFmt, uint32_t location)
        : attachmentFormat(format)
        , sampleCount(samples)
        , resolveFormat(resolveFmt)
        , resolveLocation(location)
    {
    }

    std::vector<VkFormat> getFormats() const
    {
        std::set<VkFormat> formatSet;
        formatSet.insert(attachmentFormat);
        formatSet.insert(resolveFormat);
        return std::vector<VkFormat>(formatSet.begin(), formatSet.end());
    }

    bool isMultiSample() const
    {
        return (sampleCount > VK_SAMPLE_COUNT_1_BIT);
    }

    bool isDepthStencil() const
    {
        return isDepthStencilFormat(attachmentFormat);
    }

    VkImageUsageFlags getMultiSampleUsageFlags(bool dynamicRendering) const
    {
        const bool isDS              = isDepthStencil();
        VkImageUsageFlags usageFlags = 0u;

        usageFlags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

        // For the clears before rendering.
        if (dynamicRendering)
            usageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        // This one depends on the format.
        usageFlags |= (isDS ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

        return usageFlags;
    }

    VkImageUsageFlags getSingleSampleUsageFlags(bool dynamicRendering) const
    {
        const bool isDS              = isDepthStencilFormat(attachmentFormat);
        VkImageUsageFlags usageFlags = 0u;

        // Needed to copy the image to a verification buffer.
        usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        // For the clears before rendering.
        if (dynamicRendering)
            usageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        // This one depends on the format.
        usageFlags |= (isDS ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

        return usageFlags;
    }
};

struct TestParams
{
    SharedGroupParams groupParams;
    std::vector<AttachmentInfo> attachmentList;
    std::vector<UploadPass> uploadPasses;
    std::vector<ResolvePass> resolvePasses;

    // This only makes sense for dynamic rendering. If locationRemapping is true, the code will not reorder the
    // rendering attachments so they match the shader. Instead, vkCmdSetRenderingAttachmentLocations will be used to
    // remap the rendering attachments so they have the expected locations in the frag shader for resolve passes.
    bool locationRemapping = false;

    // Disable depth writes for attachments. This will make the resolve depth buffer contents not be updated.
    bool disableDepthWrites = false;

    tcu::IVec3 getExtent() const
    {
        return tcu::IVec3(8, 8, 1);
    }

    VkImageType getImageType() const
    {
        return VK_IMAGE_TYPE_2D;
    }

    VkImageViewType getImageViewType() const
    {
        return VK_IMAGE_VIEW_TYPE_2D;
    }

    VkImageTiling getImageTiling() const
    {
        return VK_IMAGE_TILING_OPTIMAL;
    }

    uint32_t getRandomSeed() const
    {
        // We need to be careful here with not including padding bytes in any calculation.
        // Otherwise, test execution will become nondeterministic.
        const auto a = deMemoryHash(de::dataOrNull(attachmentList), de::dataSize(attachmentList));

        uint32_t b = 0u;
        for (const auto &up : uploadPasses)
        {
            b += deMemoryHash(&(up.area), sizeof(up.area));
            b += deMemoryHash(de::dataOrNull(up.attachments), de::dataSize(up.attachments));
        }

        uint32_t c = 0u;
        for (const auto &rp : resolvePasses)
        {
            c += deMemoryHash(&(rp.area), sizeof(rp.area));
            c += deMemoryHash(de::dataOrNull(rp.attachmentResolves), de::dataSize(rp.attachmentResolves));
        }

        return (a + b + c);
    };

    // Counts how many resolve passes resolve a given attachment.
    uint32_t getResolvePassCount(uint32_t attIndex) const
    {
        uint32_t passCount = 0u;

        for (uint32_t i = 0u; i < de::sizeU32(resolvePasses); ++i)
        {
            const auto &resolvePass = resolvePasses.at(i);
            for (uint32_t j = 0u; j < de::sizeU32(resolvePass.attachmentResolves); ++j)
            {
                if (resolvePass.attachmentResolves.at(j).attachment.index == attIndex)
                {
                    ++passCount;
                    break;
                }
            }
        }

        return passCount;
    }

    // Counts how many upload passes touch a given attachment.
    uint32_t getUploadPassCount(uint32_t attIndex) const
    {
        uint32_t passCount = 0u;

        for (uint32_t i = 0u; i < de::sizeU32(uploadPasses); ++i)
        {
            const auto &uploadPass = uploadPasses.at(i);
            for (uint32_t j = 0u; j < de::sizeU32(uploadPass.attachments); ++j)
            {
                if (uploadPass.attachments.at(j).index == attIndex)
                {
                    ++passCount;
                    break;
                }
            }
        }

        return passCount;
    }

    // Check if the different resolve subpasses are disjoint as they should be. When we have multiple resolve passes,
    // the VUs do not make it possible to serialize them, because a subpass with a resolve bit cannot be a source
    // subpass in a dependency. This makes sense because we want to do the resolve as part of a single operation in the
    // hardware if possible, so the driver should be able to merge the resolve subpasses. However, this means if the
    // resolve subpasses have overlapping sets of attachments, we create two kinds of synchronization hazards:
    //
    //   * Layout transition hazards: typically, multisample attachments are transitioned from color-attachment-optimal
    //     to shader-read-only optimal at the start of the resolve subpasses. If two subpasses resolve the same
    //     attachment and are not serialized, the layout transition from the different resolve subpasses race. Note:
    //     this problem could be solved by using the general layout throughout the whole render pass.
    //
    //   * Attachment write hazards: in addition, both draws and writes at the end of render passes from resolve
    //     subpasses race if they touch the same attachment. This is detected by synchronization validation and is not
    //     fixable. The races may happen even if the draws do not overlap, depending on the hardware, caches, specific
    //     draw areas, etc. In general, races could happen.
    //
    // For these reasons, we *can* have different resolve subpasses but only as long as they operate on different
    // attachments.
    //
    bool disjointResolves() const
    {
        // Counts how many resolve passes touch the given attachment, by attachment index. The result should never be
        // greater than 1.
        std::map<uint32_t, uint32_t> resolvePassCountByAttIndex;
        for (uint32_t i = 0u; i < de::sizeU32(attachmentList); ++i)
            resolvePassCountByAttIndex[i] = 0u;

        for (uint32_t i = 0u; i < de::sizeU32(resolvePasses); ++i)
        {
            const auto &resolvePass = resolvePasses.at(i);
            for (uint32_t j = 0u; j < de::sizeU32(resolvePass.attachmentResolves); ++j)
                ++resolvePassCountByAttIndex[resolvePass.attachmentResolves.at(j).attachment.index];
        }

        bool disjoint = true;
        for (const auto &indexAndCount : resolvePassCountByAttIndex)
        {
            if (indexAndCount.second > 1u)
            {
                disjoint = false;
                break;
            }
        }

        return disjoint;
    }

    RenderingType getRenderingType() const
    {
        return groupParams->renderingType;
    }

    bool useDynamicRendering() const
    {
        return (getRenderingType() == RENDERING_TYPE_DYNAMIC_RENDERING);
    }

    uint32_t getDepthStencilInputAttOffsetDynamicRendering() const
    {
        return 100u;
    }
};

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

class CustomResolveInstance : public vkt::TestInstance
{
public:
    CustomResolveInstance(Context &context, const TestParams &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~CustomResolveInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const TestParams m_params;
};

class CustomResolveCase : public vkt::TestCase
{
public:
    CustomResolveCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~CustomResolveCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;

    TestInstance *createInstance(Context &context) const override
    {
        return new CustomResolveInstance(context, m_params);
    }

protected:
    const TestParams m_params;
};

void CustomResolveCase::checkSupport(Context &context) const
{
    DE_ASSERT(m_params.disjointResolves());

    const auto &crFeatures = context.getCustomResolveFeaturesEXT();
    if (!crFeatures.customResolve)
        TCU_THROW(NotSupportedError, "customResolve not supported");

    const auto ctx = context.getContextCommonData();

    checkPipelineConstructionRequirements(ctx.vki, ctx.physicalDevice, m_params.groupParams->pipelineConstructionType);

    const bool useDynamicRendering = m_params.useDynamicRendering();

    if (useDynamicRendering)
    {
        const auto &drlrFeatures = context.getDynamicRenderingLocalReadFeatures();
        if (!drlrFeatures.dynamicRenderingLocalRead)
            TCU_THROW(NotSupportedError, "dynamicRenderingLocalRead not supported");
    }
    else
    {
        // We're not going to bother with render pass 2 for these tests.
        DE_ASSERT(m_params.getRenderingType() == RENDERING_TYPE_RENDERPASS_LEGACY);
    }

    const auto imageType   = m_params.getImageType();
    const auto imageTiling = m_params.getImageTiling();

    for (const auto &att : m_params.attachmentList)
    {
        const auto formats = att.getFormats();
        const auto usage   = (att.getSingleSampleUsageFlags(useDynamicRendering) |
                            (att.isMultiSample() ? att.getMultiSampleUsageFlags(useDynamicRendering) : 0u));

        for (const auto &fmt : formats)
        {
            const VkPhysicalDeviceImageFormatInfo2 formatInfo = {
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2, nullptr, fmt, imageType, imageTiling, usage, 0u,
            };

            VkImageFormatProperties2 formatProperties = initVulkanStructure();

            const auto result =
                ctx.vki.getPhysicalDeviceImageFormatProperties2(ctx.physicalDevice, &formatInfo, &formatProperties);

            if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
            {
                std::ostringstream msg;
                msg << "Format " << getFormatName(fmt) << " not supported";
                TCU_THROW(NotSupportedError, msg.str());
            }

            if (result != VK_SUCCESS)
            {
                std::ostringstream msg;
                msg << "vkGetPhysicalDeviceImageFormatProperties2 returned " << getResultName(result);
                TCU_FAIL(msg.str());
            }

            if ((formatProperties.imageFormatProperties.sampleCounts & att.sampleCount) == 0)
            {
                std::ostringstream msg;
                msg << att.sampleCount << " samples not supported for " << getFormatName(fmt);
                TCU_THROW(NotSupportedError, msg.str());
            }
        }
    }

    bool stencilExport = false;
    for (const auto &pass : m_params.resolvePasses)
        for (const auto &att : pass.attachmentResolves)
        {
            if (att.attachment.aspects & VK_IMAGE_ASPECT_STENCIL_BIT)
            {
                stencilExport = true;
                goto stencil_export_end;
            }
        }
stencil_export_end:
    if (stencilExport)
        context.requireDeviceFunctionality("VK_EXT_shader_stencil_export");

    // Make sure the depth/stencil attachment comes last and that there's only one. Also verify color attachment count
    // indices. Note: in theory we could check these limits individually for each upload and resolve passes, but the
    // upload fragment shaders (and their corresponding rendering passes) create gaps in the attachments list to use
    // the global attachment index as the output location. In practice, this means we're limited by the global
    // attachment count. That's why, to simplify, we also only allow a single depth/stencil attachment.
    const auto &maxColorAttachments = context.getDeviceProperties().limits.maxColorAttachments;
    uint32_t dsCount                = 0u;

    for (const auto &att : m_params.attachmentList)
    {
        if (att.isDepthStencil())
            ++dsCount;
    }

    DE_ASSERT(dsCount <= 1u);
    if (dsCount == 1u)
        DE_ASSERT(m_params.attachmentList.back().isDepthStencil());

    const auto colorCount = de::sizeU32(m_params.attachmentList) - dsCount;
    if (colorCount > maxColorAttachments)
    {
        std::ostringstream msg;
        msg << "Color attachment count (" << colorCount << ") greater than maxColorAttachments (" << maxColorAttachments
            << ")";
        TCU_THROW(NotSupportedError, msg.str());
    }

    // Make sure that, in each upload and resolve pass, the depth/stencil attachment comes last if ever.
    for (const auto &pass : m_params.uploadPasses)
    {
        uint32_t passDSCount = 0u;
        for (const auto &attUpload : pass.attachments)
        {
            if (m_params.attachmentList.at(attUpload.index).isDepthStencil())
                ++passDSCount;
        }

        DE_ASSERT(passDSCount <= 1u);
        if (passDSCount == 1u)
            DE_ASSERT(m_params.attachmentList.at(pass.attachments.back().index).isDepthStencil());
    }
    for (const auto &pass : m_params.resolvePasses)
    {
        uint32_t passDSCount = 0u;
        for (const auto &attResolve : pass.attachmentResolves)
        {
            if (m_params.attachmentList.at(attResolve.attachment.index).isDepthStencil())
                ++passDSCount;
        }

        DE_ASSERT(passDSCount <= 1u);
        if (passDSCount == 1u)
            DE_ASSERT(m_params.attachmentList.at(pass.attachmentResolves.back().attachment.index).isDepthStencil());
    }

    // Verify that the resolve mode is supported if we resolve depth and/or stencil.
    const auto &dsResolveProperties = context.getDepthStencilResolveProperties();
    const bool depthResolveSupport  = (dsResolveProperties.supportedDepthResolveModes & VK_RESOLVE_MODE_CUSTOM_BIT_EXT);
    const bool stencilResolveSupport =
        (dsResolveProperties.supportedStencilResolveModes & VK_RESOLVE_MODE_CUSTOM_BIT_EXT);

    for (const auto &pass : m_params.resolvePasses)
    {
        for (const auto &attResolve : pass.attachmentResolves)
        {
            if ((attResolve.attachment.aspects & VK_IMAGE_ASPECT_DEPTH_BIT) && !depthResolveSupport)
                TCU_THROW(NotSupportedError, "VK_RESOLVE_MODE_CUSTOM_BIT not in supportedDepthResolveModes");

            if ((attResolve.attachment.aspects & VK_IMAGE_ASPECT_STENCIL_BIT) && !stencilResolveSupport)
                TCU_THROW(NotSupportedError, "VK_RESOLVE_MODE_CUSTOM_BIT not in supportedStencilResolveModes");
        }
    }
}

void CustomResolveCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream pcStream;
    pcStream << "layout (push_constant, std430) uniform CoveredAreaBlock {\n"
             << "    vec2 scale;\n"
             << "    vec2 offset;\n"
             << "} pc;\n";
    const auto pcDecl = pcStream.str();

    // The vertex shader is common.
    std::ostringstream vert;
    vert << "#version 460\n"
         << pcDecl << "void main (void) {\n"
         << "    const float xCoord = float((gl_VertexIndex     ) & 1);\n"
         << "    const float yCoord = float((gl_VertexIndex >> 1) & 1);\n"
         << "    vec2 pos = vec2(xCoord, yCoord) * pc.scale + pc.offset;\n"
         << "    gl_Position = vec4(pos, 0.0, 1.0);\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    // We need to create one frag shader for each upload pass and another one for each resolve pass.
    for (size_t i = 0; i < m_params.uploadPasses.size(); ++i)
    {
        const auto &pass = m_params.uploadPasses.at(i);

        std::ostringstream descriptors; // Each input buffer containing pixel values;
        std::ostringstream attachments;
        std::ostringstream stores;

        // These are common for all attachments.
        stores << "    const int x = int(gl_FragCoord.x);\n"
               << "    const int y = int(gl_FragCoord.y);\n"
               << "    const int s = gl_SampleID;\n";

        bool stencilRef = false;
        for (const auto &attachment : pass.attachments)
        {
            const auto attIndex      = attachment.index;
            const bool uploadColor   = (attachment.aspects & VK_IMAGE_ASPECT_COLOR_BIT);
            const bool uploadDepth   = (attachment.aspects & VK_IMAGE_ASPECT_DEPTH_BIT);
            const bool uploadStencil = (attachment.aspects & VK_IMAGE_ASPECT_STENCIL_BIT);

            descriptors
                << "layout (set=0, binding=" << attIndex << ", std430) readonly buffer PixelsBlk" << attIndex << " {\n"
                << "    ivec4 extent; // .xyz is the size and should be the same for all, .w is the sample count\n"
                << "    vec4 colors[]; // rgba for color formats, .r=depth, .g=stencil for DS formats\n"
                << "} pixels" << attIndex << ";\n";

            if (uploadColor)
                attachments << "layout (location=" << attIndex << ") out vec4 outColor" << attIndex << ";\n";

            stores << "    const int p" << attIndex << " = y * pixels" << attIndex << ".extent.x + x; // Pixel index.\n"
                   << "    const int i" << attIndex << " = p" << attIndex << " * pixels" << attIndex
                   << ".extent.w + s; // Sample Index.\n";

            if (uploadColor)
                stores << "    outColor" << attIndex << " = pixels" << attIndex << ".colors[i" << attIndex << "];\n";

            if (uploadDepth)
                stores << "    gl_FragDepth = pixels" << attIndex << ".colors[i" << attIndex << "].r;\n";

            if (uploadStencil)
            {
                stores << "    gl_FragStencilRefARB = int(pixels" << attIndex << ".colors[i" << attIndex << "].g);\n";
                stencilRef = true;
            }
        }

        std::ostringstream frag;
        frag << "#version 460\n"
             << (stencilRef ? "#extension GL_ARB_shader_stencil_export : enable\n" : "") << attachments.str()
             << descriptors.str() << "void main (void) {\n"
             << stores.str() << "}\n";
        const auto shaderName = "frag_upload_" + std::to_string(i);
        programCollection.glslSources.add(shaderName) << glu::FragmentSource(frag.str());
    }

    for (size_t i = 0; i < m_params.resolvePasses.size(); ++i)
    {
        const auto &pass = m_params.resolvePasses.at(i);

        // These are basically the same descriptors we use for the upload part, but we're only interested in the extent
        // because it contains the sample count for the attachment.
        // In addition, a second set will contain the input attachments for those attachments that need resolving.
        std::ostringstream descriptors;

        std::ostringstream attachments;
        std::ostringstream resolves;

        bool stencilRef = false;

        for (const auto &attResolve : pass.attachmentResolves)
        {
            const auto attIndex       = attResolve.attachment.index;
            const auto &attInfo       = m_params.attachmentList.at(attIndex);
            const bool resolveColor   = (attResolve.attachment.aspects & VK_IMAGE_ASPECT_COLOR_BIT);
            const bool resolveDepth   = (attResolve.attachment.aspects & VK_IMAGE_ASPECT_DEPTH_BIT);
            const bool resolveStencil = (attResolve.attachment.aspects & VK_IMAGE_ASPECT_STENCIL_BIT);
            const auto dsInputAttachmentBase =
                (m_params.useDynamicRendering() ? m_params.getDepthStencilInputAttOffsetDynamicRendering() : attIndex);

            // This attachment may not need resolving.
            if (!attInfo.isMultiSample())
                continue;

            descriptors
                << "layout (set=0, binding=" << attIndex << ", std430) readonly buffer AttInfoBlk" << attIndex << " {\n"
                << "    ivec4 extent; // .xyz is the size and should be the same for all, .w is the sample count\n"
                << "} attInfo" << attIndex << ";\n";

            if (resolveColor)
                descriptors << "layout (set=1, binding=" << attIndex << ", input_attachment_index=" << attIndex
                            << ") uniform subpassInputMS inColor" << attIndex << ";\n";
            if (resolveDepth)
                descriptors << "layout (set=1, binding=" << attIndex
                            << ", input_attachment_index=" << dsInputAttachmentBase
                            << ") uniform subpassInputMS inDepth;\n";
            if (resolveStencil)
            {
                // Note we only have one depth/stencil attachment and it's the last one, so for the stencil aspect we're
                // adding a binding (and input attachment) at the end to access the stencil view.
                // IMPORTANT: if the attachment is stencil-only, it still uses the next attachment index, leaving a gap.
                descriptors << "layout (set=1, binding=" << (attIndex + 1u)
                            << ", input_attachment_index=" << (dsInputAttachmentBase + 1u)
                            << ") uniform usubpassInputMS inStencil;\n";
            }

            // We may be remapping the index for this attachment, so its location will vary.
            const auto attLocation = attInfo.resolveLocation;

            if (resolveColor)
            {
                attachments << "layout (location=" << attLocation << ") out vec4 outColor" << attLocation << ";\n";

                if (attResolve.resolveType == ResolveType::AVERAGE)
                {
                    resolves << "    vec4 avgColor" << attLocation << " = vec4(0.0);\n"
                             << "    for (int i = 0; i < attInfo" << attIndex << ".extent.w; ++i)\n"
                             << "        avgColor" << attLocation << " += subpassLoad(inColor" << attIndex << ", i);\n"
                             << "    avgColor" << attLocation << " /= float(attInfo" << attIndex << ".extent.w);\n"
                             << "    outColor" << attLocation << " = avgColor" << attLocation << ";\n";
                }
                else if (attResolve.resolveType == ResolveType::FIXED_VALUE)
                {
                    resolves << "    outColor" << attLocation << " = vec4" << attResolve.resolveParams.fixedValue
                             << ";\n";
                }
                else if (attResolve.resolveType == ResolveType::SELECTED_SAMPLE)
                {
                    resolves << "    outColor" << attLocation << " = subpassLoad(inColor" << attIndex << ", "
                             << attResolve.resolveParams.sampleIndex << ");\n";
                }
                else
                    DE_ASSERT(false);
            }
            if (resolveDepth)
            {
                if (attResolve.resolveType == ResolveType::AVERAGE)
                {
                    resolves << "    float avgDepth = 0.0;\n"
                             << "    for (int i = 0; i < attInfo" << attIndex << ".extent.w; ++i)\n"
                             << "        avgDepth += subpassLoad(inDepth, i).x;\n"
                             << "    avgDepth /= float(attInfo" << attIndex << ".extent.w);\n"
                             << "    gl_FragDepth = avgDepth;\n";
                }
                else if (attResolve.resolveType == ResolveType::FIXED_VALUE)
                {
                    resolves << "    gl_FragDepth = float(" << attResolve.resolveParams.fixedValue.x() << ");\n";
                }
                else if (attResolve.resolveType == ResolveType::SELECTED_SAMPLE)
                {
                    resolves << "    gl_FragDepth = subpassLoad(inDepth, " << attResolve.resolveParams.sampleIndex
                             << ").x;\n";
                }
                else
                    DE_ASSERT(false);
            }
            if (resolveStencil)
            {
                stencilRef = true;
                if (attResolve.resolveType == ResolveType::AVERAGE)
                {
                    resolves << "    uint avgStencil = 0u;\n"
                             << "    for (int i = 0; i < attInfo" << attIndex << ".extent.w; ++i)\n"
                             << "        avgStencil += subpassLoad(inStencil, i).x;\n"
                             << "    avgStencil /= uint(attInfo" << attIndex
                             << ".extent.w);\n" // Note integer division.
                             << "    gl_FragStencilRefARB = int(avgStencil);\n";
                }
                else if (attResolve.resolveType == ResolveType::FIXED_VALUE)
                {
                    resolves << "    gl_FragStencilRefARB = int(" << attResolve.resolveParams.fixedValue.x() << ");\n";
                }
                else if (attResolve.resolveType == ResolveType::SELECTED_SAMPLE)
                {
                    resolves << "    gl_FragStencilRefARB = int(subpassLoad(inStencil, "
                             << attResolve.resolveParams.sampleIndex << ").x);\n";
                }
                else
                    DE_ASSERT(false);
            }
        }

        std::ostringstream frag;
        frag << "#version 460\n"
             << (stencilRef ? "#extension GL_ARB_shader_stencil_export : enable\n" : "") << descriptors.str()
             << attachments.str() << "void main (void) {\n"
             << resolves.str() << "}\n";
        const auto shaderName = "frag_resolve_" + std::to_string(i);
        programCollection.glslSources.add(shaderName) << glu::FragmentSource(frag.str());
    }
}

using BufferWithMemoryPtr                   = std::unique_ptr<BufferWithMemory>;
using BufferWithMemoryPtrVec                = std::vector<BufferWithMemoryPtr>;
using ImageWithMemoryPtr                    = std::unique_ptr<ImageWithMemory>;
using AttachmentReferenceVec                = std::vector<VkAttachmentReference>;
using AttachmentReferenceVecPtr             = std::unique_ptr<AttachmentReferenceVec>;
using AttachmentReferencePtr                = std::unique_ptr<VkAttachmentReference>;
using U32Vec                                = std::vector<uint32_t>;
using U32VecPtr                             = std::unique_ptr<U32Vec>;
using GraphicsPipelineWrapperPtr            = std::unique_ptr<GraphicsPipelineWrapper>;
using GraphicsPipelineWrapperPtrVec         = std::vector<GraphicsPipelineWrapperPtr>;
using ShaderWrapperPtr                      = std::unique_ptr<ShaderWrapper>;
using ShaderWrapperPtrVec                   = std::vector<ShaderWrapperPtr>;
using PipelineMultisampleStateCreateInfoPtr = std::unique_ptr<VkPipelineMultisampleStateCreateInfo>;
using PipelineColorBlendStateCreateInfoPtr  = std::unique_ptr<VkPipelineColorBlendStateCreateInfo>;
using AreaLimit       = std::pair<tcu::IVec2, tcu::IVec2>; // top-left, bottom-right integer coordinates.
using TextureLevelPtr = std::unique_ptr<tcu::TextureLevel>;
using FormatVec       = std::vector<VkFormat>;
using FormatVecPtr    = std::unique_ptr<FormatVec>;
using RenderingAttachmentLocationInfoPtr = std::unique_ptr<VkRenderingAttachmentLocationInfo>;
using RenderingAttachmentInfoVec         = std::vector<VkRenderingAttachmentInfo>;
using RenderingAttachmentInfoVecPtr      = std::unique_ptr<RenderingAttachmentInfoVec>;
using RenderingInfoVec                   = std::vector<VkRenderingInfo>;

VkAttachmentDescription makeDefaultAttachmentDescription(VkFormat format, VkSampleCountFlagBits sampleCount,
                                                         VkImageLayout finalLayout)
{
    return makeAttachmentDescription(0u, format, sampleCount, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
                                     VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
                                     VK_IMAGE_LAYOUT_UNDEFINED, finalLayout);
}

AreaLimit calcArea(const CoveredArea &normalizedArea, const tcu::IVec3 &extent)
{
    DE_ASSERT(extent.z() == 1);
    const tcu::Vec2 extentFloat = extent.swizzle(0, 1).asFloat();
    const tcu::Vec2 topLeftNorm =
        (tcu::Vec2(0.0f) * normalizedArea.scale + normalizedArea.offset + tcu::Vec2(1.0f)) / tcu::Vec2(2.0f);
    const tcu::Vec2 bottomRightNorm =
        (tcu::Vec2(1.0f) * normalizedArea.scale + normalizedArea.offset + tcu::Vec2(1.0f)) / tcu::Vec2(2.0f);

    const tcu::IVec2 topLeft     = (topLeftNorm * extentFloat + tcu::Vec2(0.5f)).asInt();
    const tcu::IVec2 bottomRight = (bottomRightNorm * extentFloat + tcu::Vec2(0.5f)).asInt();
    return AreaLimit(topLeft, bottomRight);
}

// Memory barrier to synchronize color attachment loads and stores.
void syncAttachmentLoadsStores(const DeviceInterface &vkd, VkCommandBuffer cmdBuffer, bool inRenderPass)
{
    const auto access   = (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    const auto stage    = (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
    const auto barrier  = makeMemoryBarrier(access, access);
    const auto depFlags = (inRenderPass ? static_cast<VkDependencyFlags>(VK_DEPENDENCY_BY_REGION_BIT) : 0u);
    cmdPipelineMemoryBarrier(vkd, cmdBuffer, stage, stage, &barrier, 1u, depFlags);
}

VkImageSubresourceRange makeSimpleImageSubresourceRange(VkImageAspectFlags aspects)
{
    return makeImageSubresourceRange(aspects, 0u, 1u, 0u, 1u);
}

VkImageSubresourceLayers makeSimpleImageSubresourceLayers(VkImageAspectFlags aspects)
{
    return makeImageSubresourceLayers(aspects, 0u, 0u, 1u);
}

template <typename T>
inline de::SharedPtr<Move<T>> makeVkSharedPtr(Move<T> move)
{
    return de::SharedPtr<Move<T>>(new Move<T>(move));
}

tcu::TestStatus CustomResolveInstance::iterate(void)
{
    const auto ctx                                   = m_context.getContextCommonData();
    const bool dynamicRendering                      = m_params.useDynamicRendering();
    const uint32_t dynamicRenderingDepthInputIndex   = m_params.getDepthStencilInputAttOffsetDynamicRendering();
    const uint32_t dynamicRenderingStencilInputIndex = dynamicRenderingDepthInputIndex + 1u;
    const auto extent                                = m_params.getExtent();
    const auto extentVk                              = makeExtent3D(extent);
    const auto pixelCount                            = extent.x() * extent.y() * extent.z();
    const auto colorSRL                              = makeSimpleImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT);
    const auto depthSRR                              = makeSimpleImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT);
    const auto depthSRL                              = makeSimpleImageSubresourceLayers(VK_IMAGE_ASPECT_DEPTH_BIT);
    const auto stencilSRR                            = makeSimpleImageSubresourceRange(VK_IMAGE_ASPECT_STENCIL_BIT);
    const auto stencilSRL                            = makeSimpleImageSubresourceLayers(VK_IMAGE_ASPECT_STENCIL_BIT);
    auto &log                                        = m_context.getTestContext().getLog();

    de::Random rnd(m_params.getRandomSeed());

    // Input buffers for the upload phase.
    std::vector<BufferWithMemoryPtr> pixelBuffers;
    pixelBuffers.reserve(m_params.attachmentList.size());

    for (const auto &att : m_params.attachmentList)
    {
        const auto numSamples  = pixelCount * att.sampleCount;
        const auto bufferSize  = static_cast<VkDeviceSize>(sizeof(tcu::IVec4) + sizeof(tcu::Vec4) * numSamples);
        const auto bufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        const auto bufferInfo  = makeBufferCreateInfo(bufferSize, bufferUsage);

        pixelBuffers.emplace_back(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, bufferInfo, HostIntent::W));

        auto &alloc   = pixelBuffers.back()->getAllocation();
        uint8_t *data = reinterpret_cast<uint8_t *>(alloc.getHostPtr());
        tcu::IVec4 attExtent(extent.x(), extent.y(), extent.z(), att.sampleCount);
        memcpy(data, &attExtent, sizeof(attExtent));

        for (int i = 0; i < numSamples; ++i)
        {
            const auto red   = rnd.getFloat();
            const auto green = (att.isDepthStencil() ? static_cast<float>(rnd.getInt(0, 255)) : rnd.getFloat());
            const auto blue  = rnd.getFloat();
            const auto alpha = 1.0f;

            const tcu::Vec4 pixel(red, green, blue, alpha);
            memcpy(data + sizeof(attExtent) + sizeof(pixel) * i, &pixel, sizeof(pixel));
        }

        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    // Attachment images and views.
    std::vector<ImageWithMemoryPtr> attImages;
    std::vector<Move<VkImageView>> attViews;

    // Indexed by attachment index. These will be used for input attachments.
    std::map<uint32_t, de::SharedPtr<Move<VkImageView>>> depthOnlyViews;
    std::map<uint32_t, de::SharedPtr<Move<VkImageView>>> stencilOnlyViews;

    attImages.reserve(m_params.attachmentList.size());
    attViews.reserve(m_params.attachmentList.size());

    for (uint32_t i = 0; i < de::sizeU32(m_params.attachmentList); ++i)
    {
        const auto &att = m_params.attachmentList.at(i);
        const bool isMS = att.isMultiSample();

        const VkImageCreateInfo createInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            nullptr,
            0u,
            m_params.getImageType(),
            att.attachmentFormat,
            extentVk,
            1u,
            1u,
            att.sampleCount,
            m_params.getImageTiling(),
            (isMS ? att.getMultiSampleUsageFlags(dynamicRendering) : att.getSingleSampleUsageFlags(dynamicRendering)),
            VK_SHARING_MODE_EXCLUSIVE,
            0u,
            nullptr,
            VK_IMAGE_LAYOUT_UNDEFINED,
        };

        attImages.emplace_back(
            new ImageWithMemory(ctx.vkd, ctx.device, ctx.allocator, createInfo, MemoryRequirement::Any));
        const auto viewAspects = getImageAspectFlags(mapVkFormat(att.attachmentFormat));
        attViews.emplace_back(makeImageView(ctx.vkd, ctx.device, attImages.back()->get(), m_params.getImageViewType(),
                                            att.attachmentFormat, makeSimpleImageSubresourceRange(viewAspects)));

        if (viewAspects & VK_IMAGE_ASPECT_DEPTH_BIT)
            depthOnlyViews[i] =
                makeVkSharedPtr(makeImageView(ctx.vkd, ctx.device, attImages.back()->get(), m_params.getImageViewType(),
                                              att.attachmentFormat, depthSRR));

        if (viewAspects & VK_IMAGE_ASPECT_STENCIL_BIT)
            stencilOnlyViews[i] =
                makeVkSharedPtr(makeImageView(ctx.vkd, ctx.device, attImages.back()->get(), m_params.getImageViewType(),
                                              att.attachmentFormat, stencilSRR));
    }

    // Resolve images. These are only needed if the attachment is multisample, so we'll store them by att index.
    std::map<uint32_t, ImageWithMemoryPtr> resolveImages;
    std::map<uint32_t, de::SharedPtr<Move<VkImageView>>> resolveViews;

    for (uint32_t i = 0; i < de::sizeU32(m_params.attachmentList); ++i)
    {
        const auto &att = m_params.attachmentList.at(i);

        if (!att.isMultiSample())
            continue;

        const VkImageCreateInfo createInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            nullptr,
            0u,
            m_params.getImageType(),
            att.resolveFormat,
            extentVk,
            1u,
            1u,
            VK_SAMPLE_COUNT_1_BIT,
            m_params.getImageTiling(),
            att.getSingleSampleUsageFlags(dynamicRendering),
            VK_SHARING_MODE_EXCLUSIVE,
            0u,
            nullptr,
            VK_IMAGE_LAYOUT_UNDEFINED,
        };

        resolveImages[i].reset(
            new ImageWithMemory(ctx.vkd, ctx.device, ctx.allocator, createInfo, MemoryRequirement::Any));
        const auto viewAspects = getImageAspectFlags(mapVkFormat(att.resolveFormat));
        const auto srr         = makeSimpleImageSubresourceRange(viewAspects);
        resolveViews[i]        = makeVkSharedPtr(makeImageView(ctx.vkd, ctx.device, resolveImages[i]->get(),
                                                               m_params.getImageViewType(), att.resolveFormat, srr));
    }

    //
    // Render pass or dynamic rendering.
    //

    // Attachment descriptions, both for the regular and the resolve attachments.
    std::vector<VkAttachmentDescription> attachmentDescriptions;

    // Maps each attachment index to its resolve attachments description index.
    std::map<uint32_t, uint32_t> resolveAttDescriptionIndices;

    // Subpass descriptions.
    std::vector<VkSubpassDescription> subpassDescriptions;

    // The color, input and preserve references for each subpass need to be stored somewhere for the pointers to make
    // sense.
    std::vector<AttachmentReferenceVecPtr> subpassColorReferences;
    std::vector<AttachmentReferencePtr> subpassDepthStencilReferences;
    std::vector<AttachmentReferenceVecPtr> subpassInputAttReferences;
    std::vector<U32VecPtr> subpassPreserveReferences;

    // We need different color blend states depending on the color attachment count.
    std::set<uint32_t> colorAttCounts;

    // Subpass dependencies.
    std::vector<VkSubpassDependency> subpassDependencies;

    // Render pass and framebuffer.
    Move<VkRenderPass> renderPass;
    Move<VkFramebuffer> framebuffer;

    // RenderingCreateInfo for each pipeline in the upload and resolve passes.
    std::vector<FormatVecPtr> uploadColorFormats;
    std::vector<FormatVecPtr> resolveColorFormats;
    std::vector<FormatVecPtr> customResolveColorFormats; // Starting to have naming issues here...
    std::vector<VkPipelineRenderingCreateInfo> uploadAttFormats;
    std::vector<VkPipelineRenderingCreateInfo> resolveAttFormats;
    std::vector<VkCustomResolveCreateInfoEXT> customResolveAttFormats;

    // Rendering attachment location info for the resolve pipelines.
    std::vector<U32VecPtr> resolveColorLocations;
    std::vector<RenderingAttachmentLocationInfoPtr> resolveAttLocations;

    // VkRenderingInfo and helper vectors for each upload and resolve passes.
    std::vector<RenderingAttachmentInfoVecPtr> uploadColorRenderingAttachmentInfos;
    std::vector<RenderingAttachmentInfoVecPtr> uploadDepthRenderingAttachmentInfos;
    std::vector<RenderingAttachmentInfoVecPtr> uploadStencilRenderingAttachmentInfos;

    std::vector<RenderingAttachmentInfoVecPtr> resolveColorRenderingAttachmentInfos;
    std::vector<RenderingAttachmentInfoVecPtr> resolveDepthRenderingAttachmentInfos;
    std::vector<RenderingAttachmentInfoVecPtr> resolveStencilRenderingAttachmentInfos;

    RenderingInfoVec uploadRenderingInfos;
    RenderingInfoVec resolveRenderingInfos;

    if (m_params.getRenderingType() == RENDERING_TYPE_RENDERPASS_LEGACY)
    {
        // First the regular attachments.
        attachmentDescriptions.reserve(m_params.attachmentList.size() * 2);
        for (const auto &att : m_params.attachmentList)
        {
            const auto isMS        = att.isMultiSample();
            const auto isDS        = att.isDepthStencil();
            const auto finalLayout = (isMS ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL :
                                             (isDS ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
                                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
            attachmentDescriptions.push_back(
                makeDefaultAttachmentDescription(att.attachmentFormat, att.sampleCount, finalLayout));
        }

        // Then the resolve attachments, remembering the index for each of them in the attachment descriptions vector.
        for (uint32_t i = 0; i < de::sizeU32(m_params.attachmentList); ++i)
        {
            const auto &att = m_params.attachmentList.at(i);

            if (!att.isMultiSample())
                continue;

            const auto finalLayout = (att.isDepthStencil() ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
                                                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            const auto resolveAttDescriptionIndex = static_cast<uint32_t>(attachmentDescriptions.size());
            resolveAttDescriptionIndices[i]       = resolveAttDescriptionIndex;
            attachmentDescriptions.push_back(
                makeDefaultAttachmentDescription(att.resolveFormat, VK_SAMPLE_COUNT_1_BIT, finalLayout));
        }

        // For each upload subpass, we will only use the attachments that are being referenced in the upload operation.
        const auto totalPasses = m_params.uploadPasses.size() + m_params.resolvePasses.size();

        subpassDescriptions.reserve(totalPasses);
        subpassColorReferences.reserve(totalPasses);
        subpassDepthStencilReferences.reserve(totalPasses);
        subpassPreserveReferences.reserve(totalPasses);
        subpassDependencies.reserve(totalPasses);

        for (uint32_t upIndex = 0; upIndex < de::sizeU32(m_params.uploadPasses); ++upIndex)
        {
            const auto &uploadPass = m_params.uploadPasses.at(upIndex);

            std::vector<uint32_t> uploadAttIndices;
            uploadAttIndices.reserve(uploadPass.attachments.size());
            for (const auto &att : uploadPass.attachments)
                uploadAttIndices.push_back(att.index);

            subpassColorReferences.emplace_back(new AttachmentReferenceVec);
            subpassDepthStencilReferences.emplace_back(nullptr);
            subpassInputAttReferences.emplace_back(new AttachmentReferenceVec);
            subpassPreserveReferences.emplace_back(new U32Vec);

            auto &colorRefs    = *subpassColorReferences.back();
            auto &dsRefPtr     = subpassDepthStencilReferences.back();
            auto &inputAttRefs = *subpassInputAttReferences.back(); // Will be empty for this subpass.
            auto &preserveRefs = *subpassPreserveReferences.back();

            for (uint32_t i = 0; i < de::sizeU32(m_params.attachmentList); ++i)
            {
                const auto &att = m_params.attachmentList.at(i);

                if (de::contains(uploadAttIndices.begin(), uploadAttIndices.end(), i))
                {
                    if (att.isDepthStencil())
                    {
                        DE_ASSERT(!dsRefPtr.get());
                        dsRefPtr.reset(new VkAttachmentReference{i, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL});
                    }
                    else
                        colorRefs.push_back(makeAttachmentReference(i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
                }
                else
                {
                    // The depth/stencil attachment will be the last one on the list, so we can skip adding an unused
                    // color reference for it.
                    if (!att.isDepthStencil())
                        colorRefs.push_back(makeAttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED));
                    preserveRefs.push_back(i);
                }
            }

            const VkSubpassDescription subpassDescription = {
                0u,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                de::sizeU32(inputAttRefs),
                de::dataOrNull(inputAttRefs),
                de::sizeU32(colorRefs),
                de::dataOrNull(colorRefs),
                nullptr,
                dsRefPtr.get(),
                de::sizeU32(preserveRefs),
                de::dataOrNull(preserveRefs),
            };

            colorAttCounts.insert(subpassDescription.colorAttachmentCount);

            subpassDescriptions.push_back(subpassDescription);

            // Subpass dependency with the previous subpass.
            if (upIndex > 0u)
            {
                const auto depStage =
                    (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                     VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
                const auto depAccess =
                    (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                     VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT);
                const auto depFlags = VK_DEPENDENCY_BY_REGION_BIT;

                subpassDependencies.emplace_back(VkSubpassDependency{
                    upIndex - 1u,
                    upIndex,
                    depStage,
                    depStage,
                    depAccess,
                    depAccess,
                    depFlags,
                });
            }
        }

        // Resolve subpasses.
        for (uint32_t rpIndex = 0; rpIndex < de::sizeU32(m_params.resolvePasses); ++rpIndex)
        {
            const auto &resolvePass = m_params.resolvePasses.at(rpIndex);

            // Attachments resolved in this subpass, with their resolve information.
            std::map<uint32_t, const AttachmentResolve *> attIndexResolve;
            for (const auto &resolve : resolvePass.attachmentResolves)
                attIndexResolve[resolve.attachment.index] = &resolve;

            subpassColorReferences.emplace_back(new AttachmentReferenceVec);
            subpassDepthStencilReferences.emplace_back(nullptr);
            subpassInputAttReferences.emplace_back(new AttachmentReferenceVec);
            subpassPreserveReferences.emplace_back(new U32Vec);

            auto &colorRefs    = *subpassColorReferences.back();
            auto &dsRefPtr     = subpassDepthStencilReferences.back();
            auto &inputAttRefs = *subpassInputAttReferences.back();
            auto &preserveRefs = *subpassPreserveReferences.back();

            // We need to preserve all attachments which are originally single-sampled.
            // Multisample attachments which are resolved in this subpass need to be included in the input attachment
            // reference list. However, if they're not resolved or they're not multisampled, we need to insert an unused
            // attachment reference in the list so the input attachment index matches the frag shader, which uses the
            // global attachment index as the input attachment index and descriptor binding number.
            for (uint32_t i = 0; i < de::sizeU32(m_params.attachmentList); ++i)
            {
                const auto &attInfo = m_params.attachmentList.at(i);
                if (!attInfo.isMultiSample())
                {
                    preserveRefs.push_back(i);
                    inputAttRefs.push_back(makeAttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED));
                }
                else
                {
                    const auto itr = attIndexResolve.find(i);
                    if (itr != attIndexResolve.end())
                    {
                        // The depth aspect is always read with input attachment i, and the stencil attachment is
                        // always read with input attachment i+1. See the resolve frag shader.
                        const auto resolvedAspects = itr->second->attachment.aspects;
                        const bool resolveDepth    = (resolvedAspects & VK_IMAGE_ASPECT_DEPTH_BIT);
                        const bool resolveStencil  = (resolvedAspects & VK_IMAGE_ASPECT_STENCIL_BIT);

                        if (attInfo.isDepthStencil())
                        {
                            if (resolveDepth)
                                inputAttRefs.push_back(
                                    makeAttachmentReference(i, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
                            else if (resolveStencil)
                                inputAttRefs.push_back(
                                    makeAttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED));

                            if (resolveStencil)
                                inputAttRefs.push_back(
                                    makeAttachmentReference(i, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
                        }
                        else
                            inputAttRefs.push_back(
                                makeAttachmentReference(i, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
                    }
                    else
                        inputAttRefs.push_back(
                            makeAttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED));
                }
            }

            // The color references vector is a bit trickier. Each attachment has a remap index that is used in resolve
            // subpasses. This index is the one used as the color attachment location in the frag shader, so the color
            // references vector needs to have the right number of elements and valid entries in the remap index
            // positions. Furthermore, those entries need to point to the right attachment description item, which we
            // saved in the resolveAttDescriptionIndices map, for each multisampled attachment index.

            // This map lets us track the output location indices used, and which description index they should point
            // to.
            std::map<uint32_t, uint32_t> locationToDescriptionIndex;

            for (uint32_t i = 0u; i < de::sizeU32(resolvePass.attachmentResolves); ++i)
            {
                const auto &attResolve = resolvePass.attachmentResolves.at(i);
                const auto attIndex    = attResolve.attachment.index;
                const auto &att        = m_params.attachmentList.at(attIndex);

                // The depth/stencil attachment does not have a location.
                if (att.isDepthStencil())
                {
                    if (att.isMultiSample())
                    {
                        // The depth/stencil resolve attachment description should be the last one in the list.
                        DE_ASSERT(!dsRefPtr.get());
                        dsRefPtr.reset(new VkAttachmentReference{de::sizeU32(attachmentDescriptions) - 1u,
                                                                 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL});
                    }
                    continue;
                }

                const auto location                  = att.resolveLocation;
                const auto descriptionIndex          = resolveAttDescriptionIndices.at(attIndex);
                locationToDescriptionIndex[location] = descriptionIndex;
            }

            // Find the highest location, then iterate over the range.
            if (!locationToDescriptionIndex.empty())
            {
                const auto topLocation = locationToDescriptionIndex.rbegin()->first;
                for (uint32_t i = 0u; i <= topLocation; ++i)
                {
                    const auto &itr = locationToDescriptionIndex.find(i);
                    if (itr != locationToDescriptionIndex.end())
                        colorRefs.push_back(
                            makeAttachmentReference(itr->second, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
                    else
                        colorRefs.push_back(makeAttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED));
                }
            }

            // Do not forget the resolve flag for the subpass!
            const VkSubpassDescription subpassDescription = {
                VK_SUBPASS_DESCRIPTION_CUSTOM_RESOLVE_BIT_EXT, // IMPORTANT!
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                de::sizeU32(inputAttRefs),
                de::dataOrNull(inputAttRefs),
                de::sizeU32(colorRefs),
                de::dataOrNull(colorRefs),
                nullptr,
                dsRefPtr.get(),
                de::sizeU32(preserveRefs),
                de::dataOrNull(preserveRefs),
            };

            colorAttCounts.insert(subpassDescription.colorAttachmentCount);

            subpassDescriptions.push_back(subpassDescription);
            const auto subpassIndex = de::sizeU32(subpassDescriptions) - 1u;

            // Insert a dependency from all previous upload passes to this one.
            for (uint32_t i = 0u; i < de::sizeU32(m_params.uploadPasses); ++i)
            {
                // Note the dst color attachment write access synchronizes layout transitions.
                const auto srcStage =
                    (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                     VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
                const auto dstStage =
                    (VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                     VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
                const auto srcAccess =
                    (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                     VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
                const auto dstAccess = (VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
                const auto depFlags  = VK_DEPENDENCY_BY_REGION_BIT;

                subpassDependencies.emplace_back(VkSubpassDependency{
                    i,
                    subpassIndex,
                    srcStage,
                    dstStage,
                    srcAccess,
                    dstAccess,
                    depFlags,
                });
            }

            // We might be tempted to insert a dependency from the previous resolve subpass to this one, but
            // VUID-VkSubpassDescription-flags-03343 states that a subpass that includes the resolve flag must be the
            // last one in a dependency chain.
#if 0
            if (rpIndex > 0u)
            {
                const auto depStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                const auto depAccess = (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
                const auto depFlags = VK_DEPENDENCY_BY_REGION_BIT;

                subpassDependencies.emplace_back(VkSubpassDependency{
                    subpassIndex - 1u,
                    subpassIndex,
                    depStage,
                    depStage,
                    depAccess,
                    depAccess,
                    depFlags,
                });
            }
#endif
        }

        const VkRenderPassCreateInfo renderPassCreateInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            nullptr,
            0u,
            de::sizeU32(attachmentDescriptions),
            de::dataOrNull(attachmentDescriptions),
            de::sizeU32(subpassDescriptions),
            de::dataOrNull(subpassDescriptions),
            de::sizeU32(subpassDependencies),
            de::dataOrNull(subpassDependencies),
        };

        renderPass = createRenderPass(ctx.vkd, ctx.device, &renderPassCreateInfo);

        // Framebuffer.
        std::vector<VkImageView> framebufferViews;
        framebufferViews.reserve(m_params.attachmentList.size() * 2);
        for (const auto &attView : attViews)
            framebufferViews.push_back(*attView);
        // We save the resolve views by attachment index and also keep a map tracking attachment description indices
        // according to the attachment index. We do that because there might be holes in the list (not every attachment
        // may have an associated resolve attachment) and we want to quickly know the resolve attachment description
        // index for a given attachment index, or the resolve view for a given attachment index. However, for the
        // purpose of the framebuffer views, the views are sorted by attachment index, so we can iterate over the map
        // (which is sorted) and get the right sequence.
        for (const auto &resolveView : resolveViews)
            framebufferViews.push_back(resolveView.second->get());

        framebuffer = makeFramebuffer(ctx.vkd, ctx.device, *renderPass, de::sizeU32(framebufferViews),
                                      de::dataOrNull(framebufferViews), extentVk.width, extentVk.height);
    }
    else if (dynamicRendering)
    {
        uploadColorFormats.reserve(m_params.uploadPasses.size());
        uploadAttFormats.reserve(m_params.uploadPasses.size());

        for (uint32_t i = 0u; i < de::sizeU32(m_params.uploadPasses); ++i)
        {
            const auto &uploadPass = m_params.uploadPasses.at(i);

            std::map<uint32_t, const AttachmentIndexAspect *> usedAttachments;
            for (const auto &attIndexAspect : uploadPass.attachments)
                usedAttachments[attIndexAspect.index] = &attIndexAspect;

            DE_ASSERT(!usedAttachments.empty());
            const auto lastAttIndex = usedAttachments.rbegin()->first;

            uploadColorFormats.emplace_back(new FormatVec);
            auto &uploadFormatVec = *uploadColorFormats.back();
            uploadFormatVec.reserve(lastAttIndex + 1u);

            VkFormat depthAttachmentFormat   = VK_FORMAT_UNDEFINED;
            VkFormat stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

            for (uint32_t j = 0u; j <= lastAttIndex; ++j)
            {
                const auto &att = m_params.attachmentList.at(j);
                if (att.isDepthStencil())
                {
                    const auto itr = usedAttachments.find(j);
                    if (itr != usedAttachments.end())
                    {
                        if (itr->second->aspects & VK_IMAGE_ASPECT_DEPTH_BIT)
                            depthAttachmentFormat = att.attachmentFormat;
                        if (itr->second->aspects & VK_IMAGE_ASPECT_STENCIL_BIT)
                            stencilAttachmentFormat = att.attachmentFormat;
                    }
                }
                else
                {
                    if (usedAttachments.find(j) != usedAttachments.end())
                        uploadFormatVec.push_back(att.attachmentFormat);
                    else
                        uploadFormatVec.push_back(VK_FORMAT_UNDEFINED);
                }
            }

            uploadAttFormats.emplace_back(VkPipelineRenderingCreateInfo{
                VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                nullptr,
                0u,
                de::sizeU32(uploadFormatVec),
                de::dataOrNull(uploadFormatVec),
                depthAttachmentFormat,
                stencilAttachmentFormat,
            });

            uploadDepthRenderingAttachmentInfos.emplace_back(new RenderingAttachmentInfoVec);
            uploadStencilRenderingAttachmentInfos.emplace_back(new RenderingAttachmentInfoVec);

            uploadColorRenderingAttachmentInfos.emplace_back(new RenderingAttachmentInfoVec);
            auto &colorRenderingAttachmentInfos = *uploadColorRenderingAttachmentInfos.back();
            colorRenderingAttachmentInfos.reserve(lastAttIndex + 1u);

            for (uint32_t j = 0u; j <= lastAttIndex; ++j)
            {
                const auto &attInfo = m_params.attachmentList.at(j);

                if (attInfo.isDepthStencil())
                {
                    const auto itr    = usedAttachments.find(j);
                    const bool isUsed = (itr != usedAttachments.end());

                    if (!isUsed)
                        continue;

                    const bool isMS = (isUsed && attInfo.isMultiSample());

                    const VkRenderingAttachmentInfo dsRenderingAttachmentInfo = {
                        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, nullptr, *attViews.at(j),
                        (isMS ? VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
                        // Note the upload pass contains no resolve information.
                        VK_RESOLVE_MODE_NONE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_LOAD,
                        VK_ATTACHMENT_STORE_OP_STORE, makeClearValueDepthStencil(0.0f, 0u), // Not used.
                    };

                    if (itr->second->aspects & VK_IMAGE_ASPECT_DEPTH_BIT)
                        uploadDepthRenderingAttachmentInfos.back()->push_back(dsRenderingAttachmentInfo);

                    if (itr->second->aspects & VK_IMAGE_ASPECT_STENCIL_BIT)
                        uploadStencilRenderingAttachmentInfos.back()->push_back(dsRenderingAttachmentInfo);
                }
                else
                {
                    const bool isUsed = (usedAttachments.find(j) != usedAttachments.end());
                    const bool isMS   = (isUsed && attInfo.isMultiSample());

                    colorRenderingAttachmentInfos.push_back(VkRenderingAttachmentInfo{
                        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                        nullptr,
                        (isUsed ? *attViews.at(j) : VK_NULL_HANDLE),
                        (isUsed ?
                             (isMS ? VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) :
                             VK_IMAGE_LAYOUT_UNDEFINED),
                        // Note the upload pass contains no resolve information.
                        VK_RESOLVE_MODE_NONE,
                        VK_NULL_HANDLE,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_ATTACHMENT_LOAD_OP_LOAD,
                        VK_ATTACHMENT_STORE_OP_STORE,
                        makeClearValueColor(tcu::Vec4(0.0f)),
                    });
                }
            }

            uploadRenderingInfos.push_back(VkRenderingInfo{
                VK_STRUCTURE_TYPE_RENDERING_INFO,
                nullptr,
                0u, // Note the upload passes don't have any special flags.
                makeRect2D(extent),
                1u,
                0u,
                de::sizeU32(colorRenderingAttachmentInfos),
                de::dataOrNull(colorRenderingAttachmentInfos),
                de::dataOrNull(*uploadDepthRenderingAttachmentInfos.back()),
                de::dataOrNull(*uploadStencilRenderingAttachmentInfos.back()),
            });

            colorAttCounts.insert(uploadRenderingInfos.back().colorAttachmentCount);
        }

        // Resolve passes are a bit more tricky because frag shader locations use the resolve location for the
        // attachment.
        resolveColorFormats.reserve(m_params.resolvePasses.size());
        resolveAttFormats.reserve(m_params.resolvePasses.size());
        customResolveColorFormats.reserve(m_params.resolvePasses.size());

        for (uint32_t i = 0u; i < de::sizeU32(m_params.resolvePasses); ++i)
        {
            const auto &resolvePass = m_params.resolvePasses.at(i);
            bool remappingNeeded    = false;

            const AttachmentIndexAspect *depthStencilAttIndexAspects = nullptr;

            // Map frag shader locations to attachment indices.
            std::map<uint32_t, uint32_t> locationToAttIndex;
            for (uint32_t j = 0u; j < resolvePass.attachmentResolves.size(); ++j)
            {
                const auto &attResolve = resolvePass.attachmentResolves.at(j);
                const auto &attIndex   = attResolve.attachment.index;
                const auto &attInfo    = m_params.attachmentList.at(attIndex);

                if (attInfo.isDepthStencil())
                {
                    DE_ASSERT(!depthStencilAttIndexAspects);
                    depthStencilAttIndexAspects = &attResolve.attachment;
                    continue;
                }

                const auto &location         = m_params.attachmentList.at(attIndex).resolveLocation;
                locationToAttIndex[location] = attIndex;

                if (attIndex != location && m_params.locationRemapping)
                    remappingNeeded = true;
            }
            const bool hasColorAtt = !locationToAttIndex.empty();
            const auto topLocation = (hasColorAtt ? locationToAttIndex.rbegin()->first : 0u);

            resolveColorFormats.emplace_back(new FormatVec);
            auto &resolveFormatVec = *resolveColorFormats.back();

            customResolveColorFormats.emplace_back(new FormatVec);
            auto &customResolveFormatVec = *customResolveColorFormats.back();

            resolveColorLocations.emplace_back(nullptr);
            resolveAttLocations.emplace_back(nullptr);

            // Create the color format vector based on those locations, indices and the attachment format.
            //
            // IMPORTANT: VkPipelineRenderingCreateInfo needs the original attachment format, not the resolve format,
            // because these attachments are going to be the input attachments as well. Resolve formats go in
            // VkCustomResolveCreateInfoEXT.
            if (remappingNeeded)
            {
                resolveFormatVec.reserve(resolvePass.attachmentResolves.size());
                customResolveFormatVec.reserve(resolvePass.attachmentResolves.size());

                resolveColorLocations.back().reset(new U32Vec);
                auto &locationsVec = *resolveColorLocations.back();
                locationsVec.reserve(resolvePass.attachmentResolves.size());

                for (const auto &attResolve : resolvePass.attachmentResolves)
                {
                    const auto &attInfo = m_params.attachmentList.at(attResolve.attachment.index);
                    resolveFormatVec.push_back(attInfo.attachmentFormat);
                    customResolveFormatVec.push_back(attInfo.resolveFormat);
                    locationsVec.push_back(attInfo.resolveLocation);
                }

                resolveAttLocations.back().reset(new VkRenderingAttachmentLocationInfo{
                    VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_LOCATION_INFO,
                    nullptr,
                    de::sizeU32(locationsVec),
                    de::dataOrNull(locationsVec),
                });
            }
            else
            {
                resolveFormatVec.reserve(topLocation + 1u);
                customResolveFormatVec.reserve(topLocation + 1u);

                for (uint32_t j = 0u; hasColorAtt && j <= topLocation; ++j)
                {
                    const auto itr = locationToAttIndex.find(j);
                    if (itr != locationToAttIndex.end())
                    {
                        resolveFormatVec.push_back(m_params.attachmentList.at(itr->second).attachmentFormat);
                        customResolveFormatVec.push_back(m_params.attachmentList.at(itr->second).resolveFormat);
                    }
                    else
                    {
                        resolveFormatVec.push_back(VK_FORMAT_UNDEFINED);
                        customResolveFormatVec.push_back(VK_FORMAT_UNDEFINED);
                    }
                }
            }

            VkFormat depthRenderingFormat   = VK_FORMAT_UNDEFINED;
            VkFormat stencilRenderingFormat = VK_FORMAT_UNDEFINED;

            VkFormat depthResolveFormat   = VK_FORMAT_UNDEFINED;
            VkFormat stencilResolveFormat = VK_FORMAT_UNDEFINED;

            if (depthStencilAttIndexAspects)
            {
                const auto &attInfo = m_params.attachmentList.at(depthStencilAttIndexAspects->index);

                if (depthStencilAttIndexAspects->aspects & VK_IMAGE_ASPECT_DEPTH_BIT)
                {
                    depthRenderingFormat = attInfo.attachmentFormat;
                    depthResolveFormat   = attInfo.resolveFormat;
                }

                if (depthStencilAttIndexAspects->aspects & VK_IMAGE_ASPECT_STENCIL_BIT)
                {
                    stencilRenderingFormat = attInfo.attachmentFormat;
                    stencilResolveFormat   = attInfo.resolveFormat;
                }
            }

            resolveAttFormats.emplace_back(VkPipelineRenderingCreateInfo{
                VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                nullptr,
                0u,
                de::sizeU32(resolveFormatVec),
                de::dataOrNull(resolveFormatVec),
                depthRenderingFormat,
                stencilRenderingFormat,
            });

            customResolveAttFormats.emplace_back(VkCustomResolveCreateInfoEXT{
                VK_STRUCTURE_TYPE_CUSTOM_RESOLVE_CREATE_INFO_EXT,
                nullptr,
                VK_TRUE,
                de::sizeU32(customResolveFormatVec),
                de::dataOrNull(customResolveFormatVec),
                depthResolveFormat,
                stencilResolveFormat,
            });

            resolveDepthRenderingAttachmentInfos.emplace_back(new RenderingAttachmentInfoVec);
            resolveStencilRenderingAttachmentInfos.emplace_back(new RenderingAttachmentInfoVec);

            resolveColorRenderingAttachmentInfos.emplace_back(new RenderingAttachmentInfoVec);
            auto &colorRenderingAttachmentInfos = *resolveColorRenderingAttachmentInfos.back();

            if (remappingNeeded)
            {
                colorRenderingAttachmentInfos.reserve(resolvePass.attachmentResolves.size());

                for (const auto &attResolve : resolvePass.attachmentResolves)
                {
                    const auto attIndex = attResolve.attachment.index;

                    colorRenderingAttachmentInfos.push_back(VkRenderingAttachmentInfo{
                        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                        nullptr,
                        *attViews.at(attIndex),
                        VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                        VK_RESOLVE_MODE_CUSTOM_BIT_EXT,
                        resolveViews.at(attIndex)->get(),
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_ATTACHMENT_LOAD_OP_LOAD,
                        VK_ATTACHMENT_STORE_OP_STORE,
                        makeClearValueColor(tcu::Vec4(0.0f)),
                    });
                }
            }
            else
            {
                colorRenderingAttachmentInfos.reserve(topLocation + 1u);

                for (uint32_t j = 0u; hasColorAtt && j <= topLocation; ++j)
                {
                    const auto itr      = locationToAttIndex.find(j);
                    const bool isUsed   = (itr != locationToAttIndex.end());
                    const auto attIndex = (isUsed ? itr->second : std::numeric_limits<uint32_t>::max());

                    colorRenderingAttachmentInfos.push_back(VkRenderingAttachmentInfo{
                        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                        nullptr,
                        (isUsed ? *attViews.at(attIndex) : VK_NULL_HANDLE),
                        (isUsed ? VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED),
                        (isUsed ? VK_RESOLVE_MODE_CUSTOM_BIT_EXT : VK_RESOLVE_MODE_NONE),
                        (isUsed ? resolveViews.at(attIndex)->get() : VK_NULL_HANDLE),
                        (isUsed ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED),
                        VK_ATTACHMENT_LOAD_OP_LOAD,
                        VK_ATTACHMENT_STORE_OP_STORE,
                        makeClearValueColor(tcu::Vec4(0.0f)),
                    });
                }
            }

            if (depthStencilAttIndexAspects)
            {
                const VkRenderingAttachmentInfo dsRenderingAttachmentInfo = {
                    VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    nullptr,
                    *attViews.at(depthStencilAttIndexAspects->index),
                    VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                    VK_RESOLVE_MODE_CUSTOM_BIT_EXT,
                    resolveViews.at(depthStencilAttIndexAspects->index)->get(),
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_ATTACHMENT_LOAD_OP_LOAD,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    makeClearValueDepthStencil(0.0f, 0u), // Not used.
                };

                if (depthStencilAttIndexAspects->aspects & VK_IMAGE_ASPECT_DEPTH_BIT)
                    resolveDepthRenderingAttachmentInfos.back()->push_back(dsRenderingAttachmentInfo);

                if (depthStencilAttIndexAspects->aspects & VK_IMAGE_ASPECT_STENCIL_BIT)
                    resolveStencilRenderingAttachmentInfos.back()->push_back(dsRenderingAttachmentInfo);
            }

            resolveRenderingInfos.push_back(VkRenderingInfo{
                VK_STRUCTURE_TYPE_RENDERING_INFO,
                nullptr,
                VK_RENDERING_CUSTOM_RESOLVE_BIT_EXT, // Mark as doing a custom resolve here.
                makeRect2D(extent),
                1u,
                0u,
                de::sizeU32(colorRenderingAttachmentInfos),
                de::dataOrNull(colorRenderingAttachmentInfos),
                de::dataOrNull(*resolveDepthRenderingAttachmentInfos.back()),
                de::dataOrNull(*resolveStencilRenderingAttachmentInfos.back()),
            });

            colorAttCounts.insert(resolveRenderingInfos.back().colorAttachmentCount);
        }
    }
    else
        DE_ASSERT(false);

    // Descriptor sets: we need one for the pixel buffers and a second one for the input attachments.
    Move<VkDescriptorPool> descPool;
    {
        DescriptorPoolBuilder poolBuilder;
        poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, de::sizeU32(m_params.attachmentList));
        // The extra +1 is for the extra depth/stencil input attachment that may be needed.
        poolBuilder.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, de::sizeU32(m_params.attachmentList) + 1u);
        descPool = poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 2u);
    }

    Move<VkDescriptorSetLayout> pixelsSetLayout;
    Move<VkDescriptorSetLayout> inputsSetLayout;

    {
        DescriptorSetLayoutBuilder layoutBuilder;
        for (uint32_t i = 0u; i < de::sizeU32(m_params.attachmentList); ++i)
            layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
        pixelsSetLayout = layoutBuilder.build(ctx.vkd, ctx.device);
    }
    {
        DescriptorSetLayoutBuilder layoutBuilder;
        for (uint32_t i = 0u; i < de::sizeU32(m_params.attachmentList); ++i)
        {
            layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);

            // The last attachment may be a depth/stencil one.
            if (m_params.attachmentList.at(i).isDepthStencil())
                layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
        }
        inputsSetLayout = layoutBuilder.build(ctx.vkd, ctx.device);
    }

    const auto pixelsDescriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descPool, *pixelsSetLayout);
    const auto inputsDescriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descPool, *inputsSetLayout);
    const std::vector<VkDescriptorSet> allDescriptorSets{*pixelsDescriptorSet, *inputsDescriptorSet};
    const auto binding = DescriptorSetUpdateBuilder::Location::binding;

    {
        DescriptorSetUpdateBuilder setUpdateBuilder;
        for (uint32_t i = 0u; i < de::sizeU32(m_params.attachmentList); ++i)
        {
            const auto descBufferInfo = makeDescriptorBufferInfo(pixelBuffers.at(i)->get(), 0ull, VK_WHOLE_SIZE);
            setUpdateBuilder.writeSingle(*pixelsDescriptorSet, binding(i), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                         &descBufferInfo);
        }
        setUpdateBuilder.update(ctx.vkd, ctx.device);
    }
    {
        DescriptorSetUpdateBuilder setUpdateBuilder;
        for (uint32_t i = 0u; i < de::sizeU32(m_params.attachmentList); ++i)
        {
            const auto imgLayout =
                (dynamicRendering ? VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            const auto &attInfo = m_params.attachmentList.at(i);

            if (attInfo.isMultiSample())
            {
                if (attInfo.isDepthStencil())
                {
                    const auto viewAspects = getImageAspectFlags(mapVkFormat(attInfo.attachmentFormat));
                    if (viewAspects & VK_IMAGE_ASPECT_DEPTH_BIT)
                    {
                        const auto descImgInfo =
                            makeDescriptorImageInfo(VK_NULL_HANDLE, depthOnlyViews.at(i)->get(), imgLayout);
                        setUpdateBuilder.writeSingle(*inputsDescriptorSet, binding(i),
                                                     VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &descImgInfo);
                    }
                    if (viewAspects & VK_IMAGE_ASPECT_STENCIL_BIT)
                    {
                        const auto descImgInfo =
                            makeDescriptorImageInfo(VK_NULL_HANDLE, stencilOnlyViews.at(i)->get(), imgLayout);
                        setUpdateBuilder.writeSingle(*inputsDescriptorSet, binding(i + 1u),
                                                     VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &descImgInfo);
                    }
                }
                else
                {
                    const auto descImgInfo = makeDescriptorImageInfo(VK_NULL_HANDLE, *attViews.at(i), imgLayout);
                    setUpdateBuilder.writeSingle(*inputsDescriptorSet, binding(i), VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                                                 &descImgInfo);
                }
            }
            else
            {
                // We should not be using the image from the shader (the code is not prepared for that), so we will skip
                // setting it up as an input attachment. Also, usage flags do not include input attachment usage for
                // single-sample images.
#if 0
                const auto descImgInfo = makeDescriptorImageInfo(VK_NULL_HANDLE, *attViews.at(i), imgLayout);
                setUpdateBuilder.writeSingle(*inputsDescriptorSet, binding(i), VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &descImgInfo);
#endif
            }
        }
        setUpdateBuilder.update(ctx.vkd, ctx.device);
    }

    // Pipelines.
    GraphicsPipelineWrapperPtrVec uploadPipelines;
    GraphicsPipelineWrapperPtrVec resolvePipelines;

    ShaderWrapperPtrVec uploadShaders;
    ShaderWrapperPtrVec resolveShaders;

    const auto &binaries = m_context.getBinaryCollection();
    ShaderWrapper vertShader(ctx.vkd, ctx.device, binaries.get("vert"));

    uploadShaders.reserve(m_params.uploadPasses.size());
    for (uint32_t i = 0u; i < de::sizeU32(m_params.uploadPasses); ++i)
    {
        const auto shaderName = "frag_upload_" + std::to_string(i);
        uploadShaders.emplace_back(new ShaderWrapper(ctx.vkd, ctx.device, binaries.get(shaderName)));
    }

    resolveShaders.reserve(m_params.resolvePasses.size());
    for (uint32_t i = 0u; i < de::sizeU32(m_params.resolvePasses); ++i)
    {
        const auto shaderName = "frag_resolve_" + std::to_string(i);
        resolveShaders.emplace_back(new ShaderWrapper(ctx.vkd, ctx.device, binaries.get(shaderName)));
    }

    const auto pcStages = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT);
    const auto pcSize   = DE_SIZEOF32(CoveredArea);
    const auto pcRange  = makePushConstantRange(pcStages, 0u, pcSize);

    PipelineLayoutWrapper uploadPipelineLayout(m_params.groupParams->pipelineConstructionType, ctx.vkd, ctx.device,
                                               *pixelsSetLayout, &pcRange);
    const std::vector<VkDescriptorSetLayout> allSetLayouts{*pixelsSetLayout, *inputsSetLayout};
    PipelineLayoutWrapper resolvePipelineLayout(m_params.groupParams->pipelineConstructionType, ctx.vkd, ctx.device,
                                                de::sizeU32(allSetLayouts), de::dataOrNull(allSetLayouts), 1u,
                                                &pcRange);

    const std::vector<VkViewport> viewports(1u, makeViewport(extent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(extent));

    // This is common for all pipelines, because we use the same vertex shader.
    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo     = initVulkanStructure();
    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        nullptr,
        0u,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        VK_FALSE,
    };

    // When creating each pipeline, we must adjust the value of the depth and stencil test enablement flag. The rest is
    // prepared to overwrite the depth value with the fragment depth and to replace the stencil value with the reference
    // value, which could be set from the shader.
    const auto stencilOpState = makeStencilOpState(VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE,
                                                   VK_COMPARE_OP_ALWAYS, 0xFFu, 0xFFu, 0u);
    VkPipelineDepthStencilStateCreateInfo dsStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        nullptr,
        0u,
        VK_FALSE,
        VK_TRUE,
        VK_COMPARE_OP_ALWAYS,
        VK_FALSE,
        VK_FALSE,
        stencilOpState,
        stencilOpState,
        0.0f,
        1.0f,
    };

    // We need at least a couple of these, maybe more.
    std::map<VkSampleCountFlagBits, PipelineMultisampleStateCreateInfoPtr> multisampleStateMap;

    multisampleStateMap[VK_SAMPLE_COUNT_1_BIT].reset(new VkPipelineMultisampleStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        0u,
        VK_SAMPLE_COUNT_1_BIT,
        VK_FALSE,
        0.0f,
        nullptr,
        VK_FALSE,
        VK_FALSE,
    });

    for (uint32_t i = 0u; i < de::sizeU32(m_params.attachmentList); ++i)
    {
        const auto &att = m_params.attachmentList.at(i);
        multisampleStateMap[att.sampleCount].reset(new VkPipelineMultisampleStateCreateInfo{
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            nullptr,
            0u,
            att.sampleCount,
            VK_FALSE,
            0.0f,
            nullptr,
            VK_FALSE,
            VK_FALSE,
        });
    }

    // This vector can be used with all color blend states because it's large enough.
    DE_ASSERT(!colorAttCounts.empty());
    const auto maxColorAttachments = *colorAttCounts.rbegin();

    const std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates(
        maxColorAttachments,
        VkPipelineColorBlendAttachmentState{VK_FALSE, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
                                            VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
                                            (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)});

    // Create one pipeline color blend state struct per attachment count.
    std::map<uint32_t, PipelineColorBlendStateCreateInfoPtr> colorBlendStateCreateInfos;
    for (const auto &count : colorAttCounts)
    {
        const auto itr = colorBlendStateCreateInfos.find(count);
        if (itr != colorBlendStateCreateInfos.end())
            continue;

        colorBlendStateCreateInfos[count].reset(new VkPipelineColorBlendStateCreateInfo{
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            nullptr,
            0u,
            VK_FALSE,
            VK_LOGIC_OP_CLEAR,
            count,
            de::dataOrNull(colorBlendAttachmentStates),
            {0.0f, 0.0f, 0.0f, 0.0f},
        });
    }

    // We can merge the last upload and the first resolve passes with dynamic rendering if they use the same attachments
    // and there is no resolve remap.
    bool mergeUploadResolve = false;
    if (dynamicRendering && !m_params.uploadPasses.empty() && !m_params.resolvePasses.empty())
    {
        const auto &uploadAtt = m_params.uploadPasses.back().attachments;
        std::set<AttachmentIndexAspect> lastUploadAttachments(uploadAtt.begin(), uploadAtt.end());
        std::set<AttachmentIndexAspect> firstResolveAttachments;
        const auto &resolves = m_params.resolvePasses.front().attachmentResolves;
        for (const auto &attResolve : resolves)
            firstResolveAttachments.insert(attResolve.attachment);

        if (lastUploadAttachments == firstResolveAttachments)
        {
            bool indexMismatch = false;
            for (const auto resolveAtt : firstResolveAttachments)
            {
                if (resolveAtt.index != m_params.attachmentList.at(resolveAtt.index).resolveLocation)
                {
                    indexMismatch = true;
                    break;
                }
            }
            if (!indexMismatch)
                mergeUploadResolve = true;
        }
    }

    uploadPipelines.reserve(m_params.uploadPasses.size());
    for (uint32_t i = 0u; i < de::sizeU32(m_params.uploadPasses); ++i)
    {
        const bool isLastUploadPass = (i == de::sizeU32(m_params.uploadPasses) - 1u);
        const bool mergeThisPass    = (isLastUploadPass && mergeUploadResolve);

        const auto &uploadPass = m_params.uploadPasses.at(i);

        uploadPipelines.emplace_back(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                                                 m_context.getDeviceExtensions(),
                                                                 m_params.groupParams->pipelineConstructionType));

        DE_ASSERT(!uploadPass.attachments.empty());
        const auto sampleCount = m_params.attachmentList.at(uploadPass.attachments.front().index).sampleCount;

        // All attachments in the same upload pass should have the same sample count.
        for (uint32_t j = 0u; j < de::sizeU32(uploadPass.attachments); ++j)
        {
            const auto &att = m_params.attachmentList.at(uploadPass.attachments.at(j).index);
            DE_ASSERT(att.sampleCount == sampleCount);
            DE_UNREF(att); // For release builds.
            DE_UNREF(j);   // For release builds.
        }

        const auto multisampleStatePtr  = multisampleStateMap.at(sampleCount).get();
        const auto colorAttachmentCount = (dynamicRendering ? uploadRenderingInfos.at(i).colorAttachmentCount :
                                                              subpassDescriptions.at(i).colorAttachmentCount);
        VkPipelineRenderingCreateInfo *pRenderingCreateInfo = (dynamicRendering ? &uploadAttFormats.at(i) : nullptr);

        VkBool32 depthTestEnable   = VK_FALSE;
        VkBool32 stencilTestEnable = VK_FALSE;

        const auto &lastUploadAtt = uploadPass.attachments.back();

        if (lastUploadAtt.aspects & VK_IMAGE_ASPECT_DEPTH_BIT)
            depthTestEnable = VK_TRUE;

        if (lastUploadAtt.aspects & VK_IMAGE_ASPECT_STENCIL_BIT)
            stencilTestEnable = VK_TRUE;

        dsStateCreateInfo.depthTestEnable   = depthTestEnable;
        dsStateCreateInfo.stencilTestEnable = stencilTestEnable;

        auto &wrapper = *uploadPipelines.back();

        // We need to include a VkCustomResolveCreateInfoEXT structure if the upload and resolve passes will be merged.
        // The structure needs to be identical to the one used in the first resolve pass, but with customResolve false.
        VkCustomResolveCreateInfoEXT customResolveCreateInfo   = initVulkanStructure();
        VkCustomResolveCreateInfoEXT *pCustomResolveCreateInfo = nullptr;

        if (mergeThisPass)
        {
            customResolveCreateInfo               = customResolveAttFormats.front();
            customResolveCreateInfo.customResolve = VK_FALSE;
            pCustomResolveCreateInfo              = &customResolveCreateInfo;
        }

        wrapper.setDefaultRasterizationState()
            .setupVertexInputState(&vertexInputStateCreateInfo, &inputAssemblyStateCreateInfo)
            .setupPreRasterizationShaderState(viewports, scissors, uploadPipelineLayout, *renderPass, i, vertShader,
                                              nullptr, ShaderWrapper(), ShaderWrapper(), ShaderWrapper(), nullptr,
                                              nullptr, pRenderingCreateInfo)
            .setupFragmentShaderState(uploadPipelineLayout, *renderPass, i, *uploadShaders.at(i), &dsStateCreateInfo,
                                      multisampleStatePtr)
            .setupFragmentOutputState(*renderPass, i, colorBlendStateCreateInfos.at(colorAttachmentCount).get(),
                                      multisampleStatePtr, VK_NULL_HANDLE, nullptr, nullptr, nullptr,
                                      pCustomResolveCreateInfo)
            .buildPipeline();
    }

    resolvePipelines.reserve(m_params.resolvePasses.size());
    for (uint32_t i = 0u; i < de::sizeU32(m_params.resolvePasses); ++i)
    {
        const auto &resolvePass = m_params.resolvePasses.at(i);

        resolvePipelines.emplace_back(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                                                  m_context.getDeviceExtensions(),
                                                                  m_params.groupParams->pipelineConstructionType));

        const auto multisampleStatePtr = multisampleStateMap.at(VK_SAMPLE_COUNT_1_BIT).get();
        const auto subpassIdx          = de::sizeU32(m_params.uploadPasses) + i;

        auto &wrapper = *resolvePipelines.back();

        VkCustomResolveCreateInfoEXT *pCustomResolveCreateInfo              = nullptr;
        VkRenderingAttachmentLocationInfo *pRenderingAttachmentLocationInfo = nullptr;
        VkPipelineRenderingCreateInfoKHR *pRenderingCreateInfo              = nullptr;
        std::unique_ptr<VkRenderingInputAttachmentIndexInfo> pRenderingInputAttachmentIndex;

        if (dynamicRendering)
        {
            pRenderingCreateInfo             = &resolveAttFormats.at(i);
            pCustomResolveCreateInfo         = &customResolveAttFormats.at(i);
            pRenderingAttachmentLocationInfo = resolveAttLocations.at(i).get();
        }

        const auto colorAttachmentCount = (dynamicRendering ? resolveRenderingInfos.at(i).colorAttachmentCount :
                                                              subpassDescriptions.at(subpassIdx).colorAttachmentCount);

        VkBool32 depthTestEnable   = VK_FALSE;
        VkBool32 stencilTestEnable = VK_FALSE;

        const auto &lastResolveAtt = resolvePass.attachmentResolves.back().attachment;

        if (lastResolveAtt.aspects & VK_IMAGE_ASPECT_DEPTH_BIT)
            depthTestEnable = VK_TRUE;

        if (lastResolveAtt.aspects & VK_IMAGE_ASPECT_STENCIL_BIT)
            stencilTestEnable = VK_TRUE;

        if (m_params.disableDepthWrites)
            dsStateCreateInfo.depthWriteEnable = VK_FALSE;
        dsStateCreateInfo.depthTestEnable   = depthTestEnable;
        dsStateCreateInfo.stencilTestEnable = stencilTestEnable;

        if (dynamicRendering && (depthTestEnable || stencilTestEnable))
        {
            pRenderingInputAttachmentIndex.reset(new VkRenderingInputAttachmentIndexInfo{
                VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO,
                nullptr,
                colorAttachmentCount,
                nullptr,
                (depthTestEnable ? &dynamicRenderingDepthInputIndex : nullptr),
                (stencilTestEnable ? &dynamicRenderingStencilInputIndex : nullptr),
            });
        }

        wrapper.setDefaultRasterizationState()
            .setupVertexInputState(&vertexInputStateCreateInfo, &inputAssemblyStateCreateInfo)
            .setupPreRasterizationShaderState(viewports, scissors, resolvePipelineLayout, *renderPass, subpassIdx,
                                              vertShader, nullptr, ShaderWrapper(), ShaderWrapper(), ShaderWrapper(),
                                              nullptr, nullptr, pRenderingCreateInfo)
            .setupFragmentShaderState(resolvePipelineLayout, *renderPass, subpassIdx, *resolveShaders.at(i),
                                      &dsStateCreateInfo, multisampleStatePtr, nullptr, VK_NULL_HANDLE, nullptr,
                                      pRenderingInputAttachmentIndex.get())
            .setupFragmentOutputState(
                *renderPass, subpassIdx, colorBlendStateCreateInfos.at(colorAttachmentCount).get(), multisampleStatePtr,
                VK_NULL_HANDLE, nullptr, pRenderingAttachmentLocationInfo, nullptr, pCustomResolveCreateInfo)
            .buildPipeline();
    }

    // Verification buffers.
    std::vector<VkFormat> resultFormats;
    std::vector<tcu::TextureFormat> resultTcuFormats;

    resultFormats.reserve(m_params.attachmentList.size());
    resultTcuFormats.reserve(m_params.attachmentList.size());

    for (const auto &att : m_params.attachmentList)
    {
        resultFormats.push_back(att.isMultiSample() ? att.resolveFormat : att.attachmentFormat);
        resultTcuFormats.push_back(mapVkFormat(resultFormats.back()));
    }

    BufferWithMemoryPtrVec verifBuffers;
    verifBuffers.reserve(m_params.attachmentList.size());

    for (uint32_t i = 0u; i < de::sizeU32(m_params.attachmentList); ++i)
    {
        const auto &resultFormat    = resultFormats.at(i);
        const auto &resultTcuFormat = resultTcuFormats.at(i);

        std::vector<tcu::TextureFormat> bufferFormats;

        if (tcu::hasDepthComponent(resultTcuFormat.order))
            bufferFormats.push_back(getDepthCopyFormat(resultFormat));

        if (tcu::hasStencilComponent(resultTcuFormat.order))
            bufferFormats.push_back(getStencilCopyFormat(resultFormat));

        if (bufferFormats.empty())
        {
            // This is a color format.
            bufferFormats.push_back(resultTcuFormat);
        }

        for (const auto &tcuFormat : bufferFormats)
        {
            const auto pixelSize        = tcu::getPixelSize(tcuFormat);
            const auto bufferSize       = static_cast<VkDeviceSize>(pixelCount * pixelSize);
            const auto bufferUsage      = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            const auto bufferCreateInfo = makeBufferCreateInfo(bufferSize, bufferUsage);
            verifBuffers.emplace_back(
                new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, bufferCreateInfo, HostIntent::R));
        }
    }

    // Run passes.
    const auto bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    VkClearValue defaultClearValue;
    memset(&defaultClearValue, 0, sizeof(defaultClearValue));
    const std::vector<VkClearValue> clearColors(attachmentDescriptions.size(), defaultClearValue);

    // We need to track this for some barriers. See VUID-vkCmdPipelineBarrier-dependencyFlags-07891.
    bool inRenderPass = false;

    beginCommandBuffer(ctx.vkd, cmdBuffer);

    if (dynamicRendering)
    {
        // Clear all images.
        {
            const auto clearColor = makeClearValueColor(tcu::Vec4(0.0f));
            const auto clearDS    = makeClearValueDepthStencil(0.0f, 0u);

            std::vector<VkImageMemoryBarrier> barriers;
            barriers.reserve(m_params.attachmentList.size() * 2u);

            for (uint32_t i = 0u; i < de::sizeU32(m_params.attachmentList); ++i)
            {
                const auto &attInfo = m_params.attachmentList.at(i);
                const auto aspects  = getImageAspectFlags(mapVkFormat(attInfo.attachmentFormat));
                const auto srr      = makeSimpleImageSubresourceRange(aspects);
                barriers.push_back(makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, attImages.at(i)->get(),
                                                          srr));

                if (attInfo.isMultiSample())
                {
                    const auto resolveAspects = getImageAspectFlags(mapVkFormat(attInfo.resolveFormat));
                    const auto resolveSRR     = makeSimpleImageSubresourceRange(resolveAspects);
                    barriers.push_back(makeImageMemoryBarrier(
                        0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, resolveImages.at(i)->get(), resolveSRR));
                }
            }

            cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                          VK_PIPELINE_STAGE_TRANSFER_BIT, de::dataOrNull(barriers), barriers.size());

            for (uint32_t i = 0u; i < de::sizeU32(m_params.attachmentList); ++i)
            {
                const auto &attInfo = m_params.attachmentList.at(i);
                const auto aspects  = getImageAspectFlags(mapVkFormat(attInfo.attachmentFormat));
                const auto srr      = makeSimpleImageSubresourceRange(aspects);

                if (attInfo.isDepthStencil())
                {
                    ctx.vkd.cmdClearDepthStencilImage(cmdBuffer, attImages.at(i)->get(),
                                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearDS.depthStencil, 1u,
                                                      &srr);
                    if (attInfo.isMultiSample())
                    {
                        const auto resolveAspects = getImageAspectFlags(mapVkFormat(attInfo.resolveFormat));
                        const auto resolveSRR     = makeSimpleImageSubresourceRange(resolveAspects);
                        ctx.vkd.cmdClearDepthStencilImage(cmdBuffer, resolveImages.at(i)->get(),
                                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearDS.depthStencil,
                                                          1u, &resolveSRR);
                    }
                }
                else
                {
                    ctx.vkd.cmdClearColorImage(cmdBuffer, attImages.at(i)->get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                               &clearColor.color, 1u, &srr);
                    if (attInfo.isMultiSample())
                    {
                        ctx.vkd.cmdClearColorImage(cmdBuffer, resolveImages.at(i)->get(),
                                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor.color, 1u, &srr);
                    }
                }
            }
        }

        // Transition all attachments to their corresponding layouts.
        {
            std::vector<VkImageMemoryBarrier> barriers;
            std::map<VkImage, VkImageLayout> imageLayouts;
            std::map<VkImage, VkFormat> imageFormats;

            for (uint32_t i = 0u; i < de::sizeU32(m_params.attachmentList); ++i)
            {
                const auto &attInfo = m_params.attachmentList.at(i);
                const bool isMS     = attInfo.isMultiSample();
                const bool isDS     = attInfo.isDepthStencil();
                const auto ssLayout = (isDS ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
                                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

                if (isMS)
                {
                    const auto msImage    = attImages.at(i)->get();
                    imageLayouts[msImage] = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
                    imageFormats[msImage] = attInfo.attachmentFormat;

                    const auto itr = resolveImages.find(i);
                    if (itr != resolveImages.end())
                    {
                        const auto ssImage    = itr->second->get();
                        imageLayouts[ssImage] = ssLayout;
                        imageFormats[ssImage] = attInfo.resolveFormat;
                    }
                }
                else
                {
                    const auto ssImage    = attImages.at(i)->get();
                    imageLayouts[ssImage] = ssLayout;
                    imageFormats[ssImage] = attInfo.attachmentFormat;
                }
            }

            const auto srcStages = VK_PIPELINE_STAGE_TRANSFER_BIT;
            const auto srcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
            const auto dstStages =
                (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                 VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
            const auto dstAccess = (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT);

            for (const auto &imageLayout : imageLayouts)
            {
                const auto imgHandle = imageLayout.first;
                const auto imgFormat = imageFormats.at(imgHandle);
                const auto aspects   = getImageAspectFlags(mapVkFormat(imgFormat));
                const auto srr       = makeSimpleImageSubresourceRange(aspects);
                barriers.push_back(makeImageMemoryBarrier(srcAccess, dstAccess, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                          imageLayout.second, imageLayout.first, srr));
            }
            cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, srcStages, dstStages, de::dataOrNull(barriers),
                                          barriers.size());
        }
    }
    else
    {
        beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0), de::sizeU32(clearColors),
                        de::dataOrNull(clearColors));
        inRenderPass = true;
    }

    // Upload passes.
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, uploadPipelineLayout.get(), 0u, 1u, &pixelsDescriptorSet.get(),
                                  0u, nullptr);
    for (uint32_t i = 0u; i < de::sizeU32(m_params.uploadPasses); ++i)
    {
        const bool isLastUploadPass = (i == de::sizeU32(m_params.uploadPasses) - 1u);
        const bool mergeThisPass    = (isLastUploadPass && mergeUploadResolve);

        if (dynamicRendering)
        {
            if (i > 0)
                syncAttachmentLoadsStores(ctx.vkd, cmdBuffer, inRenderPass);

            // If we merge this pass, we use the rendering info from the first resolve, which is compatible and contains
            // the custom resolve information that we need.
            const auto &renderingInfoPtr =
                (mergeThisPass ? &resolveRenderingInfos.front() : &uploadRenderingInfos.at(i));
            ctx.vkd.cmdBeginRendering(cmdBuffer, renderingInfoPtr);
            inRenderPass = true;
        }
        else
        {
            if (i > 0)
                ctx.vkd.cmdNextSubpass(cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
        }

        const auto &uploadPass = m_params.uploadPasses.at(i);
        ctx.vkd.cmdPushConstants(cmdBuffer, uploadPipelineLayout.get(), pcStages, 0u, pcSize, &uploadPass.area);
        uploadPipelines.at(i)->bind(cmdBuffer);
        ctx.vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);

        if (dynamicRendering && !mergeThisPass)
        {
            ctx.vkd.cmdEndRendering(cmdBuffer);
            inRenderPass = false;
        }
    }

    // Resolve passes.
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, resolvePipelineLayout.get(), 0u, de::sizeU32(allDescriptorSets),
                                  de::dataOrNull(allDescriptorSets), 0u, nullptr);
    for (uint32_t i = 0u; i < de::sizeU32(m_params.resolvePasses); ++i)
    {
        if (dynamicRendering)
        {
            const bool isFirstResolvePass = (i == 0u);
            const bool mergeThisPass      = (isFirstResolvePass && mergeUploadResolve);

            if (isFirstResolvePass && !m_params.uploadPasses.empty())
                syncAttachmentLoadsStores(ctx.vkd, cmdBuffer, inRenderPass);

            // Begin the resolve pass unless this one is being merged.
            if (!mergeThisPass)
            {
                ctx.vkd.cmdBeginRendering(cmdBuffer, &resolveRenderingInfos.at(i));
                inRenderPass = true;
            }

            ctx.vkd.cmdBeginCustomResolveEXT(cmdBuffer, nullptr);
            if (resolveAttLocations.at(i))
            {
                // The pNext pointer may have been modified while building the pipelines.
                resolveAttLocations.at(i)->pNext = nullptr;
                ctx.vkd.cmdSetRenderingAttachmentLocations(cmdBuffer, resolveAttLocations.at(i).get());
            }
        }
        else
        {
            if (!m_params.uploadPasses.empty())
                ctx.vkd.cmdNextSubpass(cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
        }

        const auto &resolvePass = m_params.resolvePasses.at(i);
        ctx.vkd.cmdPushConstants(cmdBuffer, resolvePipelineLayout.get(), pcStages, 0u, pcSize, &resolvePass.area);
        if (dynamicRendering)
        {
            const auto &lastAttResolve = resolvePass.attachmentResolves.back();
            const auto lastAttIndex    = lastAttResolve.attachment.index;
            const auto &attInfo        = m_params.attachmentList.at(lastAttIndex);

            if (attInfo.isDepthStencil())
            {
                const bool resolveDepth   = (lastAttResolve.attachment.aspects & VK_IMAGE_ASPECT_DEPTH_BIT);
                const bool resolveStencil = (lastAttResolve.attachment.aspects & VK_IMAGE_ASPECT_STENCIL_BIT);

                // We need to recalculate the highest color location to know the color attachment count.
                bool hasColorAttachments = false;
                uint32_t topLocation     = 0u;

                for (const auto &attResolve : resolvePass.attachmentResolves)
                {
                    const auto &resolveAttInfo = m_params.attachmentList.at(attResolve.attachment.index);

                    if (resolveAttInfo.isDepthStencil())
                        continue;

                    hasColorAttachments = true;
                    if (resolveAttInfo.resolveLocation > topLocation)
                        topLocation = resolveAttInfo.resolveLocation;
                }

                const auto colorAttachmentCount = (hasColorAttachments ? topLocation + 1u : 0u);

                const VkRenderingInputAttachmentIndexInfo inputAttIndexInfo = {
                    VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO,
                    nullptr,
                    colorAttachmentCount,
                    nullptr,
                    (resolveDepth ? &dynamicRenderingDepthInputIndex : nullptr),
                    (resolveStencil ? &dynamicRenderingStencilInputIndex : nullptr),
                };

                ctx.vkd.cmdSetRenderingInputAttachmentIndices(cmdBuffer, &inputAttIndexInfo);
            }
        }
        resolvePipelines.at(i)->bind(cmdBuffer);
        ctx.vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);

        if (dynamicRendering)
        {
            ctx.vkd.cmdEndRendering(cmdBuffer);
            inRenderPass = false;
        }
    }

    if (!dynamicRendering)
    {
        endRenderPass(ctx.vkd, cmdBuffer);
        inRenderPass = false;
    }

    // Copy results to verification buffers.
    {
        std::vector<VkImage> images;
        std::vector<VkImageMemoryBarrier> barriers;
        barriers.reserve(m_params.attachmentList.size());
        images.reserve(m_params.attachmentList.size());

        const auto srcAccess = (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        const auto dstAccess = VK_ACCESS_TRANSFER_READ_BIT;
        const auto srcStage  = (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
        const auto dstStage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
        const auto oldLayoutColor = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        const auto oldLayoutDS    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        const auto newLayout      = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        for (uint32_t i = 0u; i < de::sizeU32(m_params.attachmentList); ++i)
        {
            const auto &att  = m_params.attachmentList.at(i);
            const auto isMS  = att.isMultiSample();
            const auto isDS  = att.isDepthStencil();
            const auto image = (isMS ? resolveImages.at(i)->get() : attImages.at(i)->get());

            images.push_back(image);

            const auto resolvePassCount = m_params.getResolvePassCount(i);
            const auto uploadPassCount  = m_params.getUploadPassCount(i);

            if (isMS)
            {
                if (resolvePassCount == 0u)
                    continue;
            }
            else
            {
                if (uploadPassCount == 0u)
                    continue;
            }

            const auto oldLayout        = (isDS ? oldLayoutDS : oldLayoutColor);
            const auto &resultTcuFormat = resultTcuFormats.at(i);
            const auto aspects          = getImageAspectFlags(resultTcuFormat);
            const auto srr              = makeSimpleImageSubresourceRange(aspects);

            barriers.push_back(makeImageMemoryBarrier(srcAccess, dstAccess, oldLayout, newLayout, image, srr));
        }

        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, srcStage, dstStage, de::dataOrNull(barriers),
                                      barriers.size());

        DE_ASSERT(m_params.attachmentList.size() == images.size());
        // We may have more verification buffers due to depth/stencil.
        DE_ASSERT(m_params.attachmentList.size() <= verifBuffers.size());

        for (uint32_t i = 0u; i < de::sizeU32(m_params.attachmentList); ++i)
        {
            // Skip copying images which are not resolved.
            const auto resolvePassCount = m_params.getResolvePassCount(i);
            if (resolvePassCount == 0u)
                continue;

            const auto &resultTcuFormat = resultTcuFormats.at(i);
            const bool hasDepth         = tcu::hasDepthComponent(resultTcuFormat.order);
            const bool hasStencil       = tcu::hasStencilComponent(resultTcuFormat.order);

            if (hasDepth || hasStencil)
            {
                if (hasDepth)
                {
                    const auto copyRegion = makeBufferImageCopy(extentVk, depthSRL);
                    ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, images.at(i), newLayout, verifBuffers.at(i)->get(), 1u,
                                                 &copyRegion);
                }
                if (hasStencil)
                {
                    const auto bufferIndex = (hasDepth ? i + 1u : i);
                    const auto copyRegion  = makeBufferImageCopy(extentVk, stencilSRL);
                    ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, images.at(i), newLayout,
                                                 verifBuffers.at(bufferIndex)->get(), 1u, &copyRegion);
                }
            }
            else
            {
                const auto copyRegion = makeBufferImageCopy(extentVk, colorSRL);
                ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, images.at(i), newLayout, verifBuffers.at(i)->get(), 1u,
                                             &copyRegion);
            }
        }

        // Transfer to host barrier.
        {
            const auto barrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                     &barrier);
        }
    }

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Verify image contents.
    for (const auto &bufferPtr : verifBuffers)
    {
        auto &alloc = bufferPtr->getAllocation();
        invalidateAlloc(ctx.vkd, ctx.device, alloc);
    }

    // Reference and result levels (we use result levels to be able to extract the covered area from the result).
    std::vector<TextureLevelPtr> referenceLevels;
    std::vector<TextureLevelPtr> resultLevels;

    // Prepare both vectors with clear images for each result and reference.
    for (const auto levelsVecPtr : {&referenceLevels, &resultLevels})
    {
        levelsVecPtr->reserve(m_params.attachmentList.size() + 1u);
        for (uint32_t i = 0u; i < de::sizeU32(m_params.attachmentList); ++i)
        {
            std::vector<tcu::TextureFormat> levelFormats;

            const auto &resultTcuFormat = resultTcuFormats.at(i);
            const bool hasDepth         = tcu::hasDepthComponent(resultTcuFormat.order);
            const bool hasStencil       = tcu::hasStencilComponent(resultTcuFormat.order);

            if (hasDepth || hasStencil)
            {
                if (hasDepth)
                    levelFormats.push_back(getDepthCopyFormat(resultFormats.at(i)));
                if (hasStencil)
                    levelFormats.push_back(getStencilCopyFormat(resultFormats.at(i)));
            }
            else
                levelFormats.push_back(resultTcuFormat);

            for (const auto &fmt : levelFormats)
            {
                levelsVecPtr->emplace_back(new tcu::TextureLevel(fmt, extent.x(), extent.y(), extent.z()));
                tcu::PixelBufferAccess access = levelsVecPtr->back()->getAccess();

                if (tcu::hasDepthComponent(fmt.order))
                    tcu::clearDepth(access, 0.0f);
                else if (tcu::hasStencilComponent(fmt.order))
                    tcu::clearStencil(access, 0);
                else
                    tcu::clear(access, tcu::Vec4(0.0f));
            }
        }
    }

    // Check each resolve pass.
    bool fail = false;
    for (uint32_t i = 0u; i < de::sizeU32(m_params.resolvePasses); ++i)
    {
        const auto &pass      = m_params.resolvePasses.at(i);
        const auto areaLimits = calcArea(pass.area, extent);
        const auto areaSize   = areaLimits.second - areaLimits.first;

        // Check each attachment.
        for (uint32_t j = 0u; j < de::sizeU32(pass.attachmentResolves); ++j)
        {
            const auto &attResolve = pass.attachmentResolves.at(j);
            const auto &idx        = attResolve.attachment.index;
            const auto &attInfo    = m_params.attachmentList.at(idx);

            DE_ASSERT(attInfo.isMultiSample());

            // Access the result pixels in the buffer and copy the resolve area into the result level.
            const auto &resultFormat    = resultFormats.at(idx);
            const auto &resultTcuFormat = resultTcuFormats.at(idx);
            const bool hasDepth         = tcu::hasDepthComponent(resultTcuFormat.order);
            const bool hasStencil       = tcu::hasStencilComponent(resultTcuFormat.order);
            const auto origTcuFormat    = mapVkFormat(attInfo.attachmentFormat);

            // We may need to verify one or two buffers in the resolve.
            std::map<tcu::TextureFormat, uint32_t> resultFormatToBufferIndex;

            if (hasDepth || hasStencil)
            {
                // We will only verify the aspects resolved in this resolve pass.
                // However, the buffer index varies according to the aspects present in the format only.

                if (hasDepth && (attResolve.attachment.aspects & VK_IMAGE_ASPECT_DEPTH_BIT))
                    resultFormatToBufferIndex[getDepthCopyFormat(resultFormat)] = idx;

                if (hasStencil && (attResolve.attachment.aspects & VK_IMAGE_ASPECT_STENCIL_BIT))
                {
                    const auto bufferIdx                                          = (hasDepth ? idx + 1u : idx);
                    resultFormatToBufferIndex[getStencilCopyFormat(resultFormat)] = bufferIdx;
                }
            }
            else
                resultFormatToBufferIndex[resultTcuFormat] = idx;

            for (const auto &formatIndex : resultFormatToBufferIndex)
            {
                const auto &tcuFormat  = formatIndex.first;
                const auto bufferIndex = formatIndex.second;

                const bool checkDepth   = tcu::hasDepthComponent(tcuFormat.order);
                const bool checkStencil = tcu::hasStencilComponent(tcuFormat.order);

                tcu::ConstPixelBufferAccess bufferResult(tcuFormat, extent,
                                                         verifBuffers.at(bufferIndex)->getAllocation().getHostPtr());
                tcu::PixelBufferAccess levelResult = resultLevels.at(bufferIndex)->getAccess();
                auto bufferResultRegion = tcu::getSubregion(bufferResult, areaLimits.first.x(), areaLimits.first.y(),
                                                            areaSize.x(), areaSize.y());
                auto levelResultRegion  = tcu::getSubregion(levelResult, areaLimits.first.x(), areaLimits.first.y(),
                                                            areaSize.x(), areaSize.y());
                tcu::copy(levelResultRegion, bufferResultRegion);

                // Calc reference values.
                const int numSamples         = static_cast<int>(attInfo.sampleCount);
                const float sampleCountFloat = static_cast<float>(numSamples);
                const auto &pixelBuffer = *pixelBuffers.at(idx); // Buffer idx contains both depth and stencil info.
                const auto pixelDataPtr =
                    reinterpret_cast<uint8_t *>(pixelBuffer.getAllocation().getHostPtr()) + sizeof(tcu::IVec4);
                const auto pixelValues = reinterpret_cast<const tcu::Vec4 *>(pixelDataPtr);

                auto &refLevel                   = referenceLevels.at(bufferIndex);
                tcu::PixelBufferAccess reference = refLevel->getAccess();

                for (int y = areaLimits.first.y(); y < areaLimits.second.y(); ++y)
                    for (int x = areaLimits.first.x(); x < areaLimits.second.x(); ++x)
                    {
                        const auto pixelIdx   = y * extent.x() + x;
                        const auto baseSample = pixelIdx * numSamples;

                        if (attResolve.resolveType == ResolveType::AVERAGE)
                        {
                            if (checkDepth)
                            {
                                if (m_params.disableDepthWrites)
                                    reference.setPixDepth(0.0f, x, y);
                                else
                                {
                                    float avgDepth = 0.0f;
                                    for (int s = 0; s < numSamples; ++s)
                                        avgDepth += pixelValues[baseSample + s].x();
                                    avgDepth /= sampleCountFloat;
                                    reference.setPixDepth(avgDepth, x, y);
                                }
                            }
                            else if (checkStencil)
                            {
                                int avgStencil = 0;
                                for (int s = 0; s < numSamples; ++s)
                                    avgStencil += static_cast<int>(pixelValues[baseSample + s].y());
                                avgStencil /= numSamples;
                                reference.setPixStencil(avgStencil, x, y);
                            }
                            else
                            {
                                tcu::Vec4 avgColor(0.0f);
                                for (int s = 0; s < numSamples; ++s)
                                    avgColor += pixelValues[baseSample + s];
                                avgColor = avgColor / tcu::Vec4(sampleCountFloat);
                                reference.setPixel(avgColor, x, y);
                            }
                        }
                        else if (attResolve.resolveType == ResolveType::FIXED_VALUE)
                        {
                            if (checkDepth)
                            {
                                if (m_params.disableDepthWrites)
                                    reference.setPixDepth(0.0f, x, y);
                                else
                                    reference.setPixDepth(attResolve.resolveParams.fixedValue.x(), x, y);
                            }
                            else if (checkStencil)
                                reference.setPixStencil(static_cast<int>(attResolve.resolveParams.fixedValue.y()), x,
                                                        y);
                            else
                                reference.setPixel(attResolve.resolveParams.fixedValue, x, y);
                        }
                        else if (attResolve.resolveType == ResolveType::SELECTED_SAMPLE)
                        {
                            if (checkDepth)
                            {
                                if (m_params.disableDepthWrites)
                                    reference.setPixDepth(0.0f, x, y);
                                else
                                    reference.setPixDepth(
                                        pixelValues[baseSample + attResolve.resolveParams.sampleIndex].x(), x, y);
                            }
                            else if (checkStencil)
                                reference.setPixStencil(
                                    static_cast<int>(
                                        pixelValues[baseSample + attResolve.resolveParams.sampleIndex].y()),
                                    x, y);
                            else
                                reference.setPixel(pixelValues[baseSample + attResolve.resolveParams.sampleIndex], x,
                                                   y);
                        }
                        else
                            DE_ASSERT(false);
                    }

                // Compare the result extracted to the level with the reference values.
                if (checkDepth)
                {
                    // Choose a threshold according to the format. The threshold will be more than 1 unit but less than
                    // 2 for UNORM formats. For SFLOAT, which has 24 mantissa bits (23 explicitly stored), we make it
                    // similar to D24.
                    const auto pixelSize = tcu::getPixelSize(tcuFormat);
                    float depthThreshold = 0.0f;

                    switch (pixelSize)
                    {
                    case 2: // D16
                        depthThreshold = 0.000025f;
                        break;
                    case 4: // D32
                        depthThreshold = 0.000000075f;
                        break;
                    default:
                        DE_ASSERT(false);
                        break;
                    }

                    const auto imageSetName =
                        "Resolve" + std::to_string(i) + "_Attachment" + std::to_string(idx) + "_Depth";
                    if (!tcu::dsThresholdCompare(log, imageSetName.c_str(), "", reference, levelResult, depthThreshold,
                                                 tcu::COMPARE_LOG_ON_ERROR))
                        fail = true;
                }
                else if (checkStencil)
                {
                    const float threshold = 0.0f; // Not used for stencil.
                    const auto imageSetName =
                        "Resolve" + std::to_string(i) + "_Attachment" + std::to_string(idx) + "_Stencil";
                    if (!tcu::dsThresholdCompare(log, imageSetName.c_str(), "", reference, levelResult, threshold,
                                                 tcu::COMPARE_LOG_ON_ERROR))
                        fail = true;
                }
                else
                {
                    const auto resultChannelClass = tcu::getTextureChannelClass(resultTcuFormat.type);
                    const auto origChannelClass   = tcu::getTextureChannelClass(origTcuFormat.type);

                    DE_ASSERT((resultChannelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT ||
                               resultChannelClass == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT) &&
                              (origChannelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT ||
                               origChannelClass == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT));

                    DE_UNREF(resultChannelClass); // For release builds.
                    DE_UNREF(origChannelClass);

                    // We'll adapt the threshold to whichever format has the lowest precision.
                    tcu::Vec4 resultThreshold(0.0f);
                    tcu::Vec4 origThreshold(0.0f);

                    const tcu::IVec4 resultBitDepth(tcu::getTextureFormatBitDepth(resultTcuFormat));
                    const tcu::IVec4 origBitDepth(tcu::getTextureFormatBitDepth(origTcuFormat));

                    const int resultModifier =
                        (resultChannelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT) ? 0 : 1;
                    const int origModifier =
                        (origChannelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT) ? 0 : 1;

                    resultThreshold = tcu::Vec4(
                        resultBitDepth[0] > 0 ? 1.0f / ((float)(1 << (resultBitDepth[0] - resultModifier)) - 2.0f) :
                                                0.0f,
                        resultBitDepth[1] > 0 ? 1.0f / ((float)(1 << (resultBitDepth[1] - resultModifier)) - 2.0f) :
                                                0.0f,
                        resultBitDepth[2] > 0 ? 1.0f / ((float)(1 << (resultBitDepth[2] - resultModifier)) - 2.0f) :
                                                0.0f,
                        resultBitDepth[3] > 0 ? 1.0f / ((float)(1 << (resultBitDepth[3] - resultModifier)) - 2.0f) :
                                                0.0f);

                    origThreshold = tcu::Vec4(
                        origBitDepth[0] > 0 ? 1.0f / ((float)(1 << (origBitDepth[0] - origModifier)) - 2.0f) : 0.0f,
                        origBitDepth[1] > 0 ? 1.0f / ((float)(1 << (origBitDepth[1] - origModifier)) - 2.0f) : 0.0f,
                        origBitDepth[2] > 0 ? 1.0f / ((float)(1 << (origBitDepth[2] - origModifier)) - 2.0f) : 0.0f,
                        origBitDepth[3] > 0 ? 1.0f / ((float)(1 << (origBitDepth[3] - origModifier)) - 2.0f) : 0.0f);

                    // Choose the maximum threshold for each of the components.
                    tcu::Vec4 threshold(0.0f);
                    for (int k = 0; k < decltype(threshold)::SIZE; ++k)
                        threshold[k] = std::max(resultThreshold[k], origThreshold[k]);

                    if (tcu::isSRGB(resultTcuFormat) || tcu::isSRGB(origTcuFormat))
                    {
                        // Widen thresholds a bit due to possible low-precision sRGB conversions.
                        for (int k = 0; k < decltype(threshold)::SIZE; ++k)
                            threshold[k] *= 2.0f;
                    }

                    const auto imageSetName = "Resolve" + std::to_string(i) + "_Attachment" + std::to_string(idx);
                    if (!tcu::floatThresholdCompare(log, imageSetName.c_str(), "", reference, levelResult, threshold,
                                                    tcu::COMPARE_LOG_ON_ERROR))
                        fail = true;
                }
            }
        }
    }

    if (fail)
        TCU_FAIL("Unexpected result found for some attachments; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

// Fragment region tests. Attempt to make sure VK_RENDERING_FRAGMENT_REGION_BIT_EXT and
// VK_SUBPASS_DESCRIPTION_FRAGMENT_REGION_BIT_EXT work as advertised.
//
// The close parameter indicates if we want to make reads and writes to the critical region close or far. If close is
// false, writes to that region will happen first, followed by writes to the rest of the image, followed by reads from
// the rest of the image, followed by reads to the critical region. If close is true, writes to the critical region will
// happen last, immediately followed by reads from it.
struct FragmentRegionParams
{
    SharedGroupParams groupParams;
    bool close;
    bool large;

    tcu::IVec3 getExtent() const
    {
        // Make sure width is odd, so the image vertical middle is in the middle of a pixel, making things easier.
        // Also making the framebuffer larger should help in some cases.
        const int dim = (large ? 1024 : 256);
        return tcu::IVec3(dim - 1, dim, 1);
    }

    VkFormat getImageFormat() const
    {
        return VK_FORMAT_R8G8B8A8_UNORM;
    }

    VkSampleCountFlagBits getSampleCount() const
    {
        return VK_SAMPLE_COUNT_4_BIT;
    }

    bool useDynamicRendering() const
    {
        return (groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING);
    }
};

struct FragmentRegionPushConstants
{
    tcu::Vec2 scale;
    tcu::Vec2 offset;
    tcu::Vec2 fbSize;
};

class FragmentRegionInstance : public vkt::TestInstance
{
public:
    FragmentRegionInstance(Context &context, const FragmentRegionParams &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }
    virtual ~FragmentRegionInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const FragmentRegionParams m_params;
};

class FragmentRegionCase : public vkt::TestCase
{
public:
    FragmentRegionCase(tcu::TestContext &testCtx, const std::string &name, const FragmentRegionParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~FragmentRegionCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new FragmentRegionInstance(context, m_params);
    }

protected:
    const FragmentRegionParams m_params;
};

void FragmentRegionCase::checkSupport(Context &context) const
{
    const auto &crFeatures = context.getCustomResolveFeaturesEXT();
    if (!crFeatures.customResolve)
        TCU_THROW(NotSupportedError, "customResolve not supported");

    const bool useDynamicRendering = m_params.useDynamicRendering();
    if (useDynamicRendering)
    {
        const auto &drlrFeatures = context.getDynamicRenderingLocalReadFeatures();
        if (!drlrFeatures.dynamicRenderingLocalRead)
            TCU_THROW(NotSupportedError, "dynamicRenderingLocalRead not supported");
    }
    else
    {
        // We're not going to bother with render pass 2 for these tests.
        DE_ASSERT(m_params.groupParams->renderingType == RENDERING_TYPE_RENDERPASS_LEGACY);
    }

    const auto &deviceProperties = context.getDeviceProperties();
    if (!deviceProperties.limits.standardSampleLocations)
        TCU_THROW(NotSupportedError, "standardSampleLocations not supported");
}

void FragmentRegionCase::initPrograms(vk::SourceCollections &programCollection) const
{
    DE_ASSERT(m_params.getSampleCount() == VK_SAMPLE_COUNT_4_BIT);

    const std::string pcDecl = "layout (push_constant, std430) uniform PCBlock {\n"
                               "    vec2 scale;\n"
                               "    vec2 offset;\n"
                               "    vec2 fbSize;\n"
                               "} pc;\n";

    std::ostringstream vert;
    vert << "#version 460\n"
         << pcDecl << "void main (void) {\n"
         << "    const float xCoord = float((gl_VertexIndex     ) & 1);\n"
         << "    const float yCoord = float((gl_VertexIndex >> 1) & 1);\n"
         << "    vec2 pos = vec2(xCoord, yCoord) * pc.scale + pc.offset;\n"
         << "    gl_Position = vec4(pos, 0.0, 1.0);\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream fragWrite;
    fragWrite
        << "#version 460\n"
        << pcDecl << "layout (location=0) out vec4 outColor;\n"
        << "void main (void) {\n"
        << "    int middleCol = int(pc.fbSize.x) / 2;\n"
        << "    int currentCol = int(gl_FragCoord.x);\n"
        << "    if (currentCol != middleCol) {\n"
        << "        // Fill most of the image with flat blue.\n"
        << "        outColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
        << "    }\n"
        << "    else\n"
        << "    {\n"
        << "        // In the center column, fill samples with a color value that's proportional to the row index.\n"
        << "        // The value will go to red or green, depending on which side of the image we are in.\n"
        << "        float xFrac = gl_FragCoord.x - float(currentCol);\n"
        << "        float colorValue = gl_FragCoord.y / pc.fbSize.y;\n"
        << "        if (xFrac < 0.5)\n"
        << "            outColor = vec4(colorValue, 0.0, 0.0, 1.0);\n"
        << "        else\n"
        << "            outColor = vec4(0.0, colorValue, 0.0, 1.0);\n"
        << "    }\n"
        << "}\n";
    programCollection.glslSources.add("frag-write") << glu::FragmentSource(fragWrite.str());

    std::ostringstream fragRead;
    fragRead << "#version 460\n"
             << "layout (set=0, binding=0, input_attachment_index=0) uniform subpassInputMS inColor;\n"
             << "layout (location=" << (m_params.useDynamicRendering() ? 1u : 0u) << ") out vec4 outColor;\n"
             << "void main (void) {\n"
             << "    // Assume we will run with 4 samples, and standard locations.\n"
             << "    vec2 coordFrac = gl_FragCoord.xy - floor(gl_FragCoord.xy);\n"
             << "    int sampleIndex = -1;\n"
             << "    if (coordFrac.x < 0.5)\n"
             << "    {\n"
             << "        if (coordFrac.y < 0.5)\n"
             << "            sampleIndex = 0;\n"
             << "        else\n"
             << "            sampleIndex = 2;\n"
             << "    }\n"
             << "    else\n"
             << "    {\n"
             << "        if (coordFrac.y < 0.5)\n"
             << "            sampleIndex = 1;\n"
             << "        else\n"
             << "            sampleIndex = 3;\n"
             << "    }\n"
             << "    // Sample from the other side, exchanging colors.\n"
             << "    int assignedSamples[] = int[](1, 0, 3, 2);\n"
             << "    int altIndex = assignedSamples[sampleIndex];\n"
             << "    outColor = subpassLoad(inColor, altIndex);\n"
             << "}\n";
    programCollection.glslSources.add("frag-copy") << glu::FragmentSource(fragRead.str());

    std::ostringstream fragCopy;
    fragCopy << "#version 460\n"
             << pcDecl << "layout (location=0) out vec4 outColor;\n"
             << "layout (set=0, binding=0) uniform sampler2DMS inColor;\n"
             << "void main(void) {\n"
             << "    // Assume we will run with 4 samples\n"
             << "    ivec2 expandedPixelCoord = ivec2(gl_FragCoord.xy);\n"
             << "    int sampleID = expandedPixelCoord.x % 4;\n"
             << "    int xCoordMS = expandedPixelCoord.x / 4;\n"
             << "    int yCoordMS = expandedPixelCoord.y;\n"
             << "    outColor = texelFetch(inColor, ivec2(xCoordMS, yCoordMS), sampleID);\n"
             << "}\n";
    programCollection.glslSources.add("frag-verif") << glu::FragmentSource(fragCopy.str());
}

using PipelineRenderingCreateInfoPtr = std::unique_ptr<VkPipelineRenderingCreateInfo>;

tcu::TestStatus FragmentRegionInstance::iterate(void)
{
    const auto ctx         = m_context.getContextCommonData();
    const auto extent      = m_params.getExtent();
    const auto extentVk    = makeExtent3D(extent);
    const auto imageFormat = m_params.getImageFormat();
    const auto sampleCount = m_params.getSampleCount();
    const auto msImageUsage =
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    const auto ssImageUsage     = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto colorSRR         = makeDefaultImageSubresourceRange();
    const auto msImageCount     = 2u;
    const auto ssExtent         = extent * tcu::IVec3(sampleCount, 1, 1); // Expand each pixel horizontally.
    const auto ssExtentVk       = makeExtent3D(ssExtent);
    const auto bindPoint        = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto constructionType = m_params.groupParams->pipelineConstructionType;

    // Multisample images.
    const VkImageCreateInfo msImageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0u,
        VK_IMAGE_TYPE_2D,
        imageFormat,
        extentVk,
        1u,
        1u,
        sampleCount,
        VK_IMAGE_TILING_OPTIMAL,
        msImageUsage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    std::vector<ImageWithMemoryPtr> msImages;
    std::vector<Move<VkImageView>> msViews;
    msImages.reserve(msImageCount);
    msViews.reserve(msImageCount);

    for (uint32_t i = 0u; i < msImageCount; ++i)
    {
        msImages.emplace_back(
            new ImageWithMemory(ctx.vkd, ctx.device, ctx.allocator, msImageCreateInfo, MemoryRequirement::Any));
        msViews.emplace_back(
            makeImageView(ctx.vkd, ctx.device, msImages.back()->get(), VK_IMAGE_VIEW_TYPE_2D, imageFormat, colorSRR));
    }

    // Single-sample result image.
    ImageWithBuffer ssImage(ctx.vkd, ctx.device, ctx.allocator, ssExtentVk, imageFormat, ssImageUsage,
                            VK_IMAGE_TYPE_2D);

    // Sampler.
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
    const auto sampler = createSampler(ctx.vkd, ctx.device, &samplerCreateInfo);

    // Render pass.
    Move<VkRenderPass> copyRenderPass;
    Move<VkRenderPass> verifRenderPass;
    Move<VkFramebuffer> copyFramebuffer;
    Move<VkFramebuffer> verifFramebuffer;

    // Dynamic-rendering stuff.
    const std::vector<VkFormat> formatVec{
        imageFormat,
        imageFormat,
    };
    PipelineRenderingCreateInfoPtr writeCopyPipelineRendering;
    PipelineRenderingCreateInfoPtr verifPipelineRendering;

    if (m_params.useDynamicRendering())
    {
        writeCopyPipelineRendering.reset(new VkPipelineRenderingCreateInfo{
            VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            nullptr,
            0u,
            2u,
            de::dataOrNull(formatVec),
            VK_FORMAT_UNDEFINED,
            VK_FORMAT_UNDEFINED,
        });
        verifPipelineRendering.reset(new VkPipelineRenderingCreateInfo{
            VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            nullptr,
            0u,
            1u,
            de::dataOrNull(formatVec),
            VK_FORMAT_UNDEFINED,
            VK_FORMAT_UNDEFINED,
        });
    }
    else
    {
        {
            // Copy render pass.
            std::vector<VkAttachmentDescription> attachmentDescriptions;
            std::vector<VkSubpassDescription> subpassDescriptions;
            std::vector<VkSubpassDependency> subpassDependencies;

            // Initial multisample attachment.
            attachmentDescriptions.push_back(makeAttachmentDescription(
                0u, imageFormat, sampleCount, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
                VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));

            // Multisample copy attachment.
            attachmentDescriptions.push_back(attachmentDescriptions.back());
            attachmentDescriptions.back().finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            const auto initialMSImageAsColor = makeAttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            const auto initialMSImageAsInput = makeAttachmentReference(0u, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            const auto copyMSImageAsColor    = makeAttachmentReference(1u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            // The first subpass fills the initial multisample attachment.
            subpassDescriptions.push_back(makeSubpassDescription(0u, bindPoint, 0u, nullptr, 1u, &initialMSImageAsColor,
                                                                 nullptr, nullptr, 0u, nullptr));

            // The second subpass reads from it and fills the second multisample attachment.
            subpassDescriptions.push_back(makeSubpassDescription(
                VK_SUBPASS_DESCRIPTION_FRAGMENT_REGION_BIT_EXT, // THIS IS WHAT WE ARE TRYING TO TEST.
                bindPoint, 1u, &initialMSImageAsInput, 1u, &copyMSImageAsColor, nullptr, nullptr, 0u, nullptr));

            // Subpass dependencies.
            subpassDependencies.push_back(makeSubpassDependency(
                0u, 1u, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                (VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT),
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                (VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
                VK_DEPENDENCY_BY_REGION_BIT));

            const VkRenderPassCreateInfo renderPassCreateInfo = {
                VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                nullptr,
                0u,
                de::sizeU32(attachmentDescriptions),
                de::dataOrNull(attachmentDescriptions),
                de::sizeU32(subpassDescriptions),
                de::dataOrNull(subpassDescriptions),
                de::sizeU32(subpassDependencies),
                de::dataOrNull(subpassDependencies),
            };

            const std::vector<VkImageView> fbViews{msViews.front().get(), msViews.back().get()};
            copyRenderPass  = createRenderPass(ctx.vkd, ctx.device, &renderPassCreateInfo);
            copyFramebuffer = makeFramebuffer(ctx.vkd, ctx.device, *copyRenderPass, de::sizeU32(fbViews),
                                              de::dataOrNull(fbViews), extentVk.width, extentVk.height);
        }
        {
            // Verification render pass with a single color attachment, sampling from the second multisample attachment
            // with a combined image sampler.
            verifRenderPass  = makeRenderPass(ctx.vkd, ctx.device, imageFormat);
            verifFramebuffer = makeFramebuffer(ctx.vkd, ctx.device, *verifRenderPass, ssImage.getImageView(),
                                               ssExtentVk.width, ssExtentVk.height);
        }
    }

    // Shaders and pipelines.
    const auto &binaries = m_context.getBinaryCollection();
    ShaderWrapper vertShader(ctx.vkd, ctx.device, binaries.get("vert"));
    ShaderWrapper fragWriteShader(ctx.vkd, ctx.device, binaries.get("frag-write"));
    ShaderWrapper fragCopyShader(ctx.vkd, ctx.device, binaries.get("frag-copy"));
    ShaderWrapper fragVerifShader(ctx.vkd, ctx.device, binaries.get("frag-verif"));

    const auto pcStages = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto pcSize   = DE_SIZEOF32(FragmentRegionPushConstants);
    const auto pcRange  = makePushConstantRange(pcStages, 0u, pcSize);

    PipelineLayoutWrapper writePipelineLayout(constructionType, ctx.vkd, ctx.device, VK_NULL_HANDLE, &pcRange);

    Move<VkDescriptorSetLayout> fragCopySetLayout;
    {
        DescriptorSetLayoutBuilder setLayoutBuilder;
        setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
        fragCopySetLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);
    }
    PipelineLayoutWrapper copyPipelineLayout(constructionType, ctx.vkd, ctx.device, *fragCopySetLayout, &pcRange);

    Move<VkDescriptorSetLayout> fragVerifSetLayout;
    {
        DescriptorSetLayoutBuilder setLayoutBuilder;
        setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        fragVerifSetLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);
    }
    PipelineLayoutWrapper verifPipelineLayout(constructionType, ctx.vkd, ctx.device, *fragVerifSetLayout, &pcRange);

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();
    const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
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
    VkPipelineMultisampleStateCreateInfo singleSampleStateCreateInfo = multisampleStateCreateInfo;
    singleSampleStateCreateInfo.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;

    const std::vector<VkViewport> writeCopyViewports(1u, makeViewport(extent));
    const std::vector<VkRect2D> writeCopyScissors(1u, makeRect2D(extent));

    const std::vector<VkViewport> verifViewports(1u, makeViewport(ssExtent));
    const std::vector<VkRect2D> verifScissors(1u, makeRect2D(ssExtent));

    const VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {
        VK_FALSE,
        VK_BLEND_FACTOR_ZERO,
        VK_BLEND_FACTOR_ZERO,
        VK_BLEND_OP_ADD,
        VK_BLEND_FACTOR_ZERO,
        VK_BLEND_FACTOR_ZERO,
        VK_BLEND_OP_ADD,
        (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT),
    };
    const std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates(2u, colorBlendAttachmentState);

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        nullptr,
        0u,
        VK_FALSE,
        VK_LOGIC_OP_AND,
        de::sizeU32(colorBlendAttachmentStates),
        de::dataOrNull(colorBlendAttachmentStates),
        {0.0f, 0.0f, 0.0f, 0.0f},
    };

    if (!m_params.useDynamicRendering())
        colorBlendStateCreateInfo.attachmentCount = 1u;

    GraphicsPipelineWrapper writePipeline(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                          m_context.getDeviceExtensions(), constructionType);
    writePipeline.setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
        .setDefaultDepthStencilState()
        .setDefaultRasterizationState()
        .setupVertexInputState(&vertexInputStateCreateInfo)
        .setupPreRasterizationShaderState(writeCopyViewports, writeCopyScissors, writePipelineLayout, *copyRenderPass,
                                          0u, vertShader, nullptr, ShaderWrapper(), ShaderWrapper(), ShaderWrapper(),
                                          nullptr, nullptr, writeCopyPipelineRendering.get())
        .setupFragmentShaderState(writePipelineLayout, *copyRenderPass, 0u, fragWriteShader, nullptr,
                                  &multisampleStateCreateInfo)
        .setupFragmentOutputState(*copyRenderPass, 0u, &colorBlendStateCreateInfo, &multisampleStateCreateInfo)
        .buildPipeline();

    GraphicsPipelineWrapper copyPipeline(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                         m_context.getDeviceExtensions(), constructionType);
    copyPipeline.setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
        .setDefaultDepthStencilState()
        .setDefaultRasterizationState()
        .setupVertexInputState(&vertexInputStateCreateInfo)
        .setupPreRasterizationShaderState(writeCopyViewports, writeCopyScissors, copyPipelineLayout, *copyRenderPass,
                                          1u, vertShader, nullptr, ShaderWrapper(), ShaderWrapper(), ShaderWrapper(),
                                          nullptr, nullptr, writeCopyPipelineRendering.get())
        .setupFragmentShaderState(copyPipelineLayout, *copyRenderPass, 1u, fragCopyShader, nullptr,
                                  &multisampleStateCreateInfo)
        .setupFragmentOutputState(*copyRenderPass, 1u, &colorBlendStateCreateInfo, &multisampleStateCreateInfo)
        .buildPipeline();

    if (m_params.useDynamicRendering())
        colorBlendStateCreateInfo.attachmentCount = 1u; // For the last pipeline.

    GraphicsPipelineWrapper verifPipeline(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                          m_context.getDeviceExtensions(), constructionType);
    verifPipeline.setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
        .setDefaultDepthStencilState()
        .setDefaultRasterizationState()
        .setupVertexInputState(&vertexInputStateCreateInfo)
        .setupPreRasterizationShaderState(verifViewports, verifScissors, verifPipelineLayout, *verifRenderPass, 0u,
                                          vertShader, nullptr, ShaderWrapper(), ShaderWrapper(), ShaderWrapper(),
                                          nullptr, nullptr, verifPipelineRendering.get())
        .setupFragmentShaderState(verifPipelineLayout, *verifRenderPass, 0u, fragVerifShader, nullptr,
                                  &singleSampleStateCreateInfo)
        .setupFragmentOutputState(*verifRenderPass, 0u, &colorBlendStateCreateInfo, &singleSampleStateCreateInfo)
        .buildPipeline();

    // Descriptor sets.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 2u);
    const auto fragCopySet  = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *fragCopySetLayout);
    const auto fragVerifSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *fragVerifSetLayout);
    const auto binding      = DescriptorSetUpdateBuilder::Location::binding;
    {
        DescriptorSetUpdateBuilder updateBuilder;
        const auto imgLayout = (m_params.useDynamicRendering() ? VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL :
                                                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        const auto imgInfo   = makeDescriptorImageInfo(VK_NULL_HANDLE, msViews.front().get(), imgLayout);
        updateBuilder.writeSingle(*fragCopySet, binding(0u), VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &imgInfo);
        updateBuilder.update(ctx.vkd, ctx.device);
    }
    {
        DescriptorSetUpdateBuilder updateBuilder;
        const auto imgInfo =
            makeDescriptorImageInfo(*sampler, msViews.back().get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        updateBuilder.writeSingle(*fragVerifSet, binding(0u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imgInfo);
        updateBuilder.update(ctx.vkd, ctx.device);
    }

    // Lets calculate the scales for the different stages: draw the left side or right side of the image, or draw the
    // central pixel column.
    const auto fbSize       = extent.swizzle(0, 1).asFloat();
    const float pixelWidth  = 1.0f / fbSize.x();                               // normalized width in (0,1) range
    const float sideWidth   = static_cast<float>(extent.x() / 2) * pixelWidth; // normalized width in (0,1) range
    const float sideScale   = sideWidth * 2.0f;                                // (-1,1) range
    const float centerScale = pixelWidth * 2.0f;                               // (-1,1) range

    // Push constants for each draw.
    const FragmentRegionPushConstants leftSidePCs = {
        tcu::Vec2(sideScale, 2.0f),
        tcu::Vec2(-1.0f, -1.0f),
        fbSize,
    };

    const FragmentRegionPushConstants rightSidePCs = {
        tcu::Vec2(sideScale, 2.0f),
        tcu::Vec2(centerScale / 2.0f, -1.0f), // Half a column to the right.
        fbSize,
    };

    const FragmentRegionPushConstants centerColPCs = {
        tcu::Vec2(centerScale, 2.0f),
        tcu::Vec2(-centerScale / 2.0f, -1.0f), // Half a column to the left.
        fbSize,
    };

    const FragmentRegionPushConstants halfCenterColPCs = {
        tcu::Vec2(centerScale / 2.0f, 2.0f),   // Note only half the center column.
        tcu::Vec2(-centerScale / 2.0f, -1.0f), // Half a column to the left.
        fbSize,
    };

    // For the verification shader.
    const FragmentRegionPushConstants fullFramePCs = {
        tcu::Vec2(2.0f, 2.0f),
        tcu::Vec2(-1.0f, -1.0f),
        ssExtent.swizzle(0, 1).asFloat(),
    };

    // Launch work.
    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const std::vector<VkClearValue> clearValues(2u, makeClearValueColor(tcu::Vec4(0.0f)));

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    if (m_params.useDynamicRendering())
    {
        // Move multisample images to the VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL layout.
        const auto dstAccess = (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                VK_ACCESS_INPUT_ATTACHMENT_READ_BIT);
        const auto dstStages = (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        const std::vector<VkImageMemoryBarrier> barriers{
            makeImageMemoryBarrier(0u, dstAccess, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                                   msImages.front()->get(), colorSRR),
            makeImageMemoryBarrier(0u, dstAccess, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                                   msImages.back()->get(), colorSRR),
        };
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dstStages,
                                      de::dataOrNull(barriers), barriers.size());

        const std::vector<VkRenderingAttachmentInfo> renderingAttachments{
            VkRenderingAttachmentInfo{
                VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                nullptr,
                msViews.front().get(),
                VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                VK_RESOLVE_MODE_NONE,
                VK_NULL_HANDLE,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_ATTACHMENT_LOAD_OP_CLEAR,
                VK_ATTACHMENT_STORE_OP_STORE,
                clearValues.at(0u),
            },
            VkRenderingAttachmentInfo{
                VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                nullptr,
                msViews.back().get(),
                VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                VK_RESOLVE_MODE_NONE,
                VK_NULL_HANDLE,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_ATTACHMENT_LOAD_OP_CLEAR,
                VK_ATTACHMENT_STORE_OP_STORE,
                clearValues.at(0u),
            },
        };
        const VkRenderingInfo renderingInfo = {
            VK_STRUCTURE_TYPE_RENDERING_INFO,
            nullptr,
            VK_RENDERING_FRAGMENT_REGION_BIT_EXT, // THIS IS WHAT WE ARE TRYING TO TEST.
            writeCopyScissors.at(0u),
            1u,
            0u,
            de::sizeU32(renderingAttachments),
            de::dataOrNull(renderingAttachments),
            nullptr,
            nullptr,
        };
        ctx.vkd.cmdBeginRendering(cmdBuffer, &renderingInfo);
    }
    else
        beginRenderPass(ctx.vkd, cmdBuffer, *copyRenderPass, *copyFramebuffer, writeCopyScissors.at(0u),
                        de::sizeU32(clearValues), de::dataOrNull(clearValues));
    writePipeline.bind(cmdBuffer);
    if (m_params.close)
    {
        // Draw sides.
        ctx.vkd.cmdPushConstants(cmdBuffer, writePipelineLayout.get(), pcStages, 0u, pcSize, &leftSidePCs);
        ctx.vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
        ctx.vkd.cmdPushConstants(cmdBuffer, writePipelineLayout.get(), pcStages, 0u, pcSize, &rightSidePCs);
        ctx.vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);

        // Draw center column.
        ctx.vkd.cmdPushConstants(cmdBuffer, writePipelineLayout.get(), pcStages, 0u, pcSize, &centerColPCs);
        ctx.vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
    }
    else
    {
        // Draw center column.
        ctx.vkd.cmdPushConstants(cmdBuffer, writePipelineLayout.get(), pcStages, 0u, pcSize, &centerColPCs);
        ctx.vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);

        // Draw sides.
        ctx.vkd.cmdPushConstants(cmdBuffer, writePipelineLayout.get(), pcStages, 0u, pcSize, &leftSidePCs);
        ctx.vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
        ctx.vkd.cmdPushConstants(cmdBuffer, writePipelineLayout.get(), pcStages, 0u, pcSize, &rightSidePCs);
        ctx.vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
    }
    if (m_params.useDynamicRendering())
    {
        const auto srcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        const auto dstAccess = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        const auto srcStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        const auto dstStages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

        const auto barrier = makeMemoryBarrier(srcAccess, dstAccess);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, srcStages, dstStages, &barrier, 1u, VK_DEPENDENCY_BY_REGION_BIT);
    }
    else
        ctx.vkd.cmdNextSubpass(cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
    copyPipeline.bind(cmdBuffer);
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, copyPipelineLayout.get(), 0u, 1u, &fragCopySet.get(), 0u,
                                  nullptr);
    if (m_params.close)
    {
        // Draw half center column.
        ctx.vkd.cmdPushConstants(cmdBuffer, writePipelineLayout.get(), pcStages, 0u, pcSize, &halfCenterColPCs);
        ctx.vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);

        // Draw left side.
        ctx.vkd.cmdPushConstants(cmdBuffer, writePipelineLayout.get(), pcStages, 0u, pcSize, &leftSidePCs);
        ctx.vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
    }
    else
    {
        // Draw left side.
        ctx.vkd.cmdPushConstants(cmdBuffer, writePipelineLayout.get(), pcStages, 0u, pcSize, &leftSidePCs);
        ctx.vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);

        // Draw center column.
        ctx.vkd.cmdPushConstants(cmdBuffer, writePipelineLayout.get(), pcStages, 0u, pcSize, &halfCenterColPCs);
        ctx.vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
    }
    if (m_params.useDynamicRendering())
        ctx.vkd.cmdEndRendering(cmdBuffer);
    else
        endRenderPass(ctx.vkd, cmdBuffer);
    {
        // Synchronize both render passes.
        const auto oldLayout = (m_params.useDynamicRendering() ? VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL :
                                                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        const auto barrier =
            makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, oldLayout,
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, msImages.back()->get(), colorSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, &barrier);
    }
    {
        // Verification render pass.
        if (m_params.useDynamicRendering())
        {
            // Move single sample image to the VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL layout.
            const auto dstAccess = (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
            const auto dstStages = (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

            const auto barrier =
                makeImageMemoryBarrier(0u, dstAccess, VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, ssImage.getImage(), colorSRR);
            cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dstStages, &barrier);

            const std::vector<VkRenderingAttachmentInfo> renderingAttachments{
                VkRenderingAttachmentInfo{
                    VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    nullptr,
                    ssImage.getImageView(),
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_RESOLVE_MODE_NONE,
                    VK_NULL_HANDLE,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    clearValues.at(0u),
                },
            };
            const VkRenderingInfo renderingInfo = {
                VK_STRUCTURE_TYPE_RENDERING_INFO,
                nullptr,
                0u,
                verifScissors.at(0u),
                1u,
                0u,
                de::sizeU32(renderingAttachments),
                de::dataOrNull(renderingAttachments),
                nullptr,
                nullptr,
            };
            ctx.vkd.cmdBeginRendering(cmdBuffer, &renderingInfo);
        }
        else
            beginRenderPass(ctx.vkd, cmdBuffer, *verifRenderPass, *verifFramebuffer, verifScissors.at(0u), 1u,
                            de::dataOrNull(clearValues));
        verifPipeline.bind(cmdBuffer);
        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, verifPipelineLayout.get(), 0u, 1u, &fragVerifSet.get(), 0u,
                                      nullptr);
        ctx.vkd.cmdPushConstants(cmdBuffer, verifPipelineLayout.get(), pcStages, 0u, pcSize, &fullFramePCs);
        ctx.vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
        if (m_params.useDynamicRendering())
            ctx.vkd.cmdEndRendering(cmdBuffer);
        else
            endRenderPass(ctx.vkd, cmdBuffer);
    }
    {
        // Copy image to result buffer.
        copyImageToBuffer(ctx.vkd, cmdBuffer, ssImage.getImage(), ssImage.getBuffer(), ssExtent.swizzle(0, 1));
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);
    ctx.vkd.deviceWaitIdle(ctx.device); // XXX

    auto &log            = m_context.getTestContext().getLog();
    const auto tcuFormat = mapVkFormat(imageFormat);
    tcu::TextureLevel referenceLevel(tcuFormat, ssExtent.x(), ssExtent.y(), ssExtent.z());
    tcu::PixelBufferAccess reference = referenceLevel.getAccess();

    // Clear to transparent black by default.
    tcu::clear(reference, tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f));

    // Expect blue on the left side.
    for (int y = 0; y < extent.y(); ++y)
        for (int x = 0; x < extent.x() / 2; ++x)
            for (int s = 0; s < sampleCount; ++s)
            {
                const int xCoord = x * sampleCount + s;
                reference.setPixel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), xCoord, y);
            }

    // In the even samples of the central column, expect green.
    //
    // Note the write shader set these to red, and the odd ones to green, but the copy shader should have exchanged the
    // samples.
    //
    // Note Y offsets for samples 0,1 and 2,3 are exchanged due to the copy shader, compared to the standard locations.
    const float swappedSampleLocationYOffset[] = {0.375f, 0.125f, 0.875f, 0.625f};
    const float height                         = static_cast<float>(extent.y());

    for (int y = 0; y < extent.y(); ++y)
        for (int s = 0; s < sampleCount; ++s)
        {
            if (s % 2 != 0)
                continue;
            const auto x               = extent.x() / 2;
            const int xCoord           = x * sampleCount + s;
            const float componentValue = (static_cast<float>(y) + swappedSampleLocationYOffset[s]) / height;
            reference.setPixel(tcu::Vec4(0.0f, componentValue, 0.0f, 1.0f), xCoord, y);
        }

    invalidateAlloc(ctx.vkd, ctx.device, ssImage.getBufferAllocation());
    tcu::ConstPixelBufferAccess result(tcuFormat, ssExtent, ssImage.getBufferAllocation().getHostPtr());

    const float thresholdValue = 0.005f; // 1/255 < 0.005 < 2/255.
    const tcu::Vec4 threshold(thresholdValue, thresholdValue, thresholdValue, 0.0f);
    if (!tcu::floatThresholdCompare(log, "Result", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected results in color buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

} // anonymous namespace

tcu::TestCaseGroup *createRenderPassCustomResolveTests(tcu::TestContext &testCtx,
                                                       const SharedGroupParams origGroupParams)
{
    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "custom_resolve", "Tests for VK_EXT_custom_resolve"));

    // The parent groups do not test all the pipeline construction types we're interested in for these tests, so we'll
    // generate all interesting combinations internally here, overwriting the pipeline construction type in the group
    // parameters.
    const struct
    {
        PipelineConstructionType pipelineConstructionType;
        const char *name;
    } constructionTypeCases[] = {
        {PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, "monolithic"},
        {PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY, "fast_lib"},
        {PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV, "shader_objects"},
    };

    const std::map<VkFormat, std::string> dsFormatNames{
        std::make_pair(VK_FORMAT_D16_UNORM, "d16"),
        std::make_pair(VK_FORMAT_X8_D24_UNORM_PACK32, "d24"),
        std::make_pair(VK_FORMAT_D32_SFLOAT, "d32"),
        std::make_pair(VK_FORMAT_S8_UINT, "s8"),
        std::make_pair(VK_FORMAT_D16_UNORM_S8_UINT, "d16s8"),
        std::make_pair(VK_FORMAT_D24_UNORM_S8_UINT, "d24s8"),
        std::make_pair(VK_FORMAT_D32_SFLOAT_S8_UINT, "d32s8"),
    };

    for (const auto &constructionTypeCase : constructionTypeCases)
    {
        if (isConstructionTypeShaderObject(constructionTypeCase.pipelineConstructionType) &&
            origGroupParams->renderingType != RENDERING_TYPE_DYNAMIC_RENDERING)
            continue;

        GroupPtr constructionGroup(new tcu::TestCaseGroup(testCtx, constructionTypeCase.name, ""));
        SharedGroupParams groupParams{new GroupParams(*origGroupParams)};
        groupParams->pipelineConstructionType = constructionTypeCase.pipelineConstructionType;

        {
            // Simple tests: one attachment, no attachment index changes, no format changes.
            TestParams params;
            params.groupParams = groupParams;
            params.attachmentList.emplace_back(VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_4_BIT,
                                               VK_FORMAT_R8G8B8A8_UNORM, 0u);
            params.uploadPasses.push_back(UploadPass{
                CoveredArea(tcu::Vec2(2.0f, 2.0f), tcu::Vec2(-1.0f, -1.0f)),
                {AttachmentIndexAspect(0u, VK_IMAGE_ASPECT_COLOR_BIT)},
            });
            params.resolvePasses.push_back(ResolvePass{
                CoveredArea(tcu::Vec2(2.0f, 2.0f), tcu::Vec2(-1.0f, -1.0f)),
                {
                    AttachmentResolve(0u, VK_IMAGE_ASPECT_COLOR_BIT, ResolveType::AVERAGE, StrategyParams()),
                },
            });

            constructionGroup->addChild(new CustomResolveCase(testCtx, "simple_average", params));

            {
                // Using the "average" resolving strategy may not let us see if the driver is resolving the values
                // itself by mistake, so we add variants with a fixed value and a specific sample.
                auto &resolve = params.resolvePasses.back().attachmentResolves.back();

                resolve.resolveType   = ResolveType::FIXED_VALUE;
                resolve.resolveParams = StrategyParams(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
                constructionGroup->addChild(new CustomResolveCase(testCtx, "simple_fixed", params));

                resolve.resolveType   = ResolveType::SELECTED_SAMPLE;
                resolve.resolveParams = StrategyParams(2u);
                constructionGroup->addChild(new CustomResolveCase(testCtx, "simple_sample_2", params));
            }
        }
        {
            // Depth-only tests.
            const std::vector<VkFormat> depthFormats{
                VK_FORMAT_D16_UNORM,         VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT,   VK_FORMAT_D32_SFLOAT_S8_UINT,
            };

            TestParams params;
            params.groupParams = groupParams;
            params.attachmentList.emplace_back(VK_FORMAT_UNDEFINED, VK_SAMPLE_COUNT_4_BIT, VK_FORMAT_UNDEFINED, 0u);
            params.uploadPasses.push_back(UploadPass{
                CoveredArea(tcu::Vec2(2.0f, 2.0f), tcu::Vec2(-1.0f, -1.0f)),
                {AttachmentIndexAspect(0u, VK_IMAGE_ASPECT_DEPTH_BIT)},
            });
            params.resolvePasses.push_back(ResolvePass{
                CoveredArea(tcu::Vec2(2.0f, 2.0f), tcu::Vec2(-1.0f, -1.0f)),
                {
                    AttachmentResolve(0u, VK_IMAGE_ASPECT_DEPTH_BIT, ResolveType::SELECTED_SAMPLE, StrategyParams(1u)),
                },
            });

            for (const auto &format : depthFormats)
            {
                params.attachmentList.back().attachmentFormat = format;
                params.attachmentList.back().resolveFormat    = format;
                const auto testName                           = "depth_only_" + dsFormatNames.at(format);
                constructionGroup->addChild(new CustomResolveCase(testCtx, testName, params));
            }
        }
        {
            // Stencil-only tests.
            const std::vector<VkFormat> stencilFormats{
                VK_FORMAT_S8_UINT,
                VK_FORMAT_D16_UNORM_S8_UINT,
                VK_FORMAT_D24_UNORM_S8_UINT,
                VK_FORMAT_D32_SFLOAT_S8_UINT,
            };

            TestParams params;
            params.groupParams = groupParams;
            params.attachmentList.emplace_back(VK_FORMAT_UNDEFINED, VK_SAMPLE_COUNT_4_BIT, VK_FORMAT_UNDEFINED, 0u);
            params.uploadPasses.push_back(UploadPass{
                CoveredArea(tcu::Vec2(2.0f, 2.0f), tcu::Vec2(-1.0f, -1.0f)),
                {AttachmentIndexAspect(0u, VK_IMAGE_ASPECT_STENCIL_BIT)},
            });
            params.resolvePasses.push_back(ResolvePass{
                CoveredArea(tcu::Vec2(2.0f, 2.0f), tcu::Vec2(-1.0f, -1.0f)),
                {
                    AttachmentResolve(0u, VK_IMAGE_ASPECT_STENCIL_BIT, ResolveType::AVERAGE, StrategyParams()),
                },
            });

            for (const auto &format : stencilFormats)
            {
                params.attachmentList.back().attachmentFormat = format;
                params.attachmentList.back().resolveFormat    = format;
                const auto testName                           = "stencil_only_" + dsFormatNames.at(format);
                constructionGroup->addChild(new CustomResolveCase(testCtx, testName, params));
            }
        }
        {
            // Combined depth-stencil tests, uploading both aspects at the same time.
            const std::vector<VkFormat> dsFormats{
                VK_FORMAT_D16_UNORM_S8_UINT,
                VK_FORMAT_D24_UNORM_S8_UINT,
                VK_FORMAT_D32_SFLOAT_S8_UINT,
            };

            TestParams params;
            params.groupParams = groupParams;
            params.attachmentList.emplace_back(VK_FORMAT_UNDEFINED, VK_SAMPLE_COUNT_4_BIT, VK_FORMAT_UNDEFINED, 0u);
            params.uploadPasses.push_back(UploadPass{
                CoveredArea(tcu::Vec2(2.0f, 2.0f), tcu::Vec2(-1.0f, -1.0f)),
                {AttachmentIndexAspect(0u, (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))},
            });
            params.resolvePasses.push_back(ResolvePass{
                CoveredArea(tcu::Vec2(2.0f, 2.0f), tcu::Vec2(-1.0f, -1.0f)),
                {
                    AttachmentResolve(0u, VK_IMAGE_ASPECT_DEPTH_BIT, ResolveType::SELECTED_SAMPLE, StrategyParams(3u)),
                },
            });

            for (const auto &format : dsFormats)
            {
                params.attachmentList.back().attachmentFormat = format;
                params.attachmentList.back().resolveFormat    = format;
                const auto testName = "depth_stencil_upload_both_resolve_depth_" + dsFormatNames.at(format);
                constructionGroup->addChild(new CustomResolveCase(testCtx, testName, params));
            }

            params.resolvePasses.back().attachmentResolves.back().attachment.aspects = VK_IMAGE_ASPECT_STENCIL_BIT;

            for (const auto &format : dsFormats)
            {
                params.attachmentList.back().attachmentFormat = format;
                params.attachmentList.back().resolveFormat    = format;
                const auto testName = "depth_stencil_upload_both_resolve_stencil_" + dsFormatNames.at(format);
                constructionGroup->addChild(new CustomResolveCase(testCtx, testName, params));
            }

            params.resolvePasses.back().attachmentResolves.back().attachment.aspects =
                (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

            for (const auto &format : dsFormats)
            {
                params.disableDepthWrites                     = false;
                params.attachmentList.back().attachmentFormat = format;
                params.attachmentList.back().resolveFormat    = format;
                const auto testName = "depth_stencil_upload_both_resolve_both_" + dsFormatNames.at(format);
                constructionGroup->addChild(new CustomResolveCase(testCtx, testName, params));

                params.disableDepthWrites = true;
                const auto testName2 =
                    "depth_stencil_upload_both_resolve_both_disable_depth_writes_" + dsFormatNames.at(format);
                constructionGroup->addChild(new CustomResolveCase(testCtx, testName2, params));
            }
        }
        {
            // Combined depth-stencil tests, uploading one aspect at a time.
            const std::vector<VkFormat> dsFormats{
                VK_FORMAT_D16_UNORM_S8_UINT,
                VK_FORMAT_D24_UNORM_S8_UINT,
                VK_FORMAT_D32_SFLOAT_S8_UINT,
            };

            TestParams params;
            params.groupParams = groupParams;
            params.attachmentList.emplace_back(VK_FORMAT_UNDEFINED, VK_SAMPLE_COUNT_4_BIT, VK_FORMAT_UNDEFINED, 0u);
            params.uploadPasses.push_back(UploadPass{
                CoveredArea(tcu::Vec2(2.0f, 2.0f), tcu::Vec2(-1.0f, -1.0f)),
                {AttachmentIndexAspect(0u, VK_IMAGE_ASPECT_DEPTH_BIT)},
            });
            params.uploadPasses.push_back(UploadPass{
                CoveredArea(tcu::Vec2(2.0f, 2.0f), tcu::Vec2(-1.0f, -1.0f)),
                {AttachmentIndexAspect(0u, VK_IMAGE_ASPECT_STENCIL_BIT)},
            });
            params.resolvePasses.push_back(ResolvePass{
                CoveredArea(tcu::Vec2(2.0f, 2.0f), tcu::Vec2(-1.0f, -1.0f)),
                {
                    AttachmentResolve(0u, VK_IMAGE_ASPECT_DEPTH_BIT, ResolveType::SELECTED_SAMPLE, StrategyParams(2u)),
                },
            });

            for (const auto &format : dsFormats)
            {
                params.attachmentList.back().attachmentFormat = format;
                params.attachmentList.back().resolveFormat    = format;
                const auto testName = "depth_stencil_upload_both_separate_resolve_depth_" + dsFormatNames.at(format);
                constructionGroup->addChild(new CustomResolveCase(testCtx, testName, params));
            }

            params.resolvePasses.back().attachmentResolves.back().attachment.aspects = VK_IMAGE_ASPECT_STENCIL_BIT;

            for (const auto &format : dsFormats)
            {
                params.attachmentList.back().attachmentFormat = format;
                params.attachmentList.back().resolveFormat    = format;
                const auto testName = "depth_stencil_upload_both_separate_resolve_stencil_" + dsFormatNames.at(format);
                constructionGroup->addChild(new CustomResolveCase(testCtx, testName, params));
            }

            params.resolvePasses.back().attachmentResolves.back().attachment.aspects =
                (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

            for (const auto &format : dsFormats)
            {
                params.attachmentList.back().attachmentFormat = format;
                params.attachmentList.back().resolveFormat    = format;
                const auto testName = "depth_stencil_upload_both_separate_resolve_both_" + dsFormatNames.at(format);
                constructionGroup->addChild(new CustomResolveCase(testCtx, testName, params));
            }
        }
        {
            // Attachment index tests: simple test but the resolve pipeline uses a different att index.
            // This will prevent the upload and resolve passes from being merged in dynamic rendering.
            TestParams params;
            params.groupParams = groupParams;
            params.attachmentList.emplace_back(VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_4_BIT,
                                               VK_FORMAT_R8G8B8A8_UNORM, 1u);
            params.uploadPasses.push_back(UploadPass{
                CoveredArea(tcu::Vec2(2.0f, 2.0f), tcu::Vec2(-1.0f, -1.0f)),
                {AttachmentIndexAspect(0u, VK_IMAGE_ASPECT_COLOR_BIT)},
            });
            params.resolvePasses.push_back(ResolvePass{
                CoveredArea(tcu::Vec2(2.0f, 2.0f), tcu::Vec2(-1.0f, -1.0f)),
                {
                    AttachmentResolve(0u, VK_IMAGE_ASPECT_COLOR_BIT, ResolveType::SELECTED_SAMPLE, StrategyParams(3u)),
                },
            });

            constructionGroup->addChild(new CustomResolveCase(testCtx, "att_index_change", params));

            if (groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
            {
                params.locationRemapping = true;
                constructionGroup->addChild(new CustomResolveCase(testCtx, "att_index_change_with_remap", params));
            }
        }
        {
            // Different resolve format: simple test, but the resolve attachment has a different format.
            TestParams params;
            params.groupParams = groupParams;
            params.attachmentList.emplace_back(VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_4_BIT,
                                               VK_FORMAT_R16G16B16A16_UNORM, 0u);
            params.uploadPasses.push_back(UploadPass{
                CoveredArea(tcu::Vec2(2.0f, 2.0f), tcu::Vec2(-1.0f, -1.0f)),
                {AttachmentIndexAspect(0u, VK_IMAGE_ASPECT_COLOR_BIT)},
            });
            params.resolvePasses.push_back(ResolvePass{
                CoveredArea(tcu::Vec2(2.0f, 2.0f), tcu::Vec2(-1.0f, -1.0f)),
                {
                    AttachmentResolve(0u, VK_IMAGE_ASPECT_COLOR_BIT, ResolveType::AVERAGE, StrategyParams()),
                },
            });

            constructionGroup->addChild(new CustomResolveCase(testCtx, "format_change", params));

            std::swap(params.attachmentList.back().attachmentFormat, params.attachmentList.back().resolveFormat);
            constructionGroup->addChild(new CustomResolveCase(testCtx, "format_change_reverse", params));
        }
        {
            // Complex case with multiple attachments, upload passes and resolves, including format and index changes.
            TestParams params;
            params.groupParams = groupParams;
            params.attachmentList.emplace_back(VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_4_BIT,
                                               VK_FORMAT_R16G16B16A16_UNORM, 1u);
            params.attachmentList.emplace_back(VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_4_BIT,
                                               VK_FORMAT_R16G16B16A16_UNORM, 0u);
            params.uploadPasses.push_back(UploadPass{
                // Upload to top half.
                CoveredArea(tcu::Vec2(2.0f, 1.0f), tcu::Vec2(-1.0f, -1.0f)),
                {AttachmentIndexAspect(0u, VK_IMAGE_ASPECT_COLOR_BIT),
                 AttachmentIndexAspect(1u, VK_IMAGE_ASPECT_COLOR_BIT)},
            });
            params.uploadPasses.push_back(UploadPass{
                // Upload to bottom half.
                CoveredArea(tcu::Vec2(2.0f, 1.0f), tcu::Vec2(-1.0f, 0.0f)),
                {AttachmentIndexAspect(0u, VK_IMAGE_ASPECT_COLOR_BIT),
                 AttachmentIndexAspect(1u, VK_IMAGE_ASPECT_COLOR_BIT)},
            });
            params.resolvePasses.push_back(ResolvePass{
                // Resolving first attachment.
                CoveredArea(tcu::Vec2(2.0f, 2.0f), tcu::Vec2(-1.0f, -1.0f)),
                {
                    AttachmentResolve(0u, VK_IMAGE_ASPECT_COLOR_BIT, ResolveType::SELECTED_SAMPLE, StrategyParams(3u)),
                },
            });
            params.resolvePasses.push_back(ResolvePass{
                // Resolving the second attachment, partially.
                CoveredArea(tcu::Vec2(2.0f, 1.0f), tcu::Vec2(-1.0f, -1.0f)),
                {
                    AttachmentResolve(1u, VK_IMAGE_ASPECT_COLOR_BIT, ResolveType::AVERAGE, StrategyParams()),
                },
            });

            constructionGroup->addChild(
                new CustomResolveCase(testCtx, "color_multi_upload_multi_resolve_complex", params));

            // Simplification of the previous case removing the format and index change.
            for (uint32_t i = 0u; i < de::sizeU32(params.attachmentList); ++i)
            {
                auto &attInfo           = params.attachmentList.at(i);
                attInfo.resolveFormat   = VK_FORMAT_R8G8B8A8_UNORM;
                attInfo.resolveLocation = i;
            }

            constructionGroup->addChild(
                new CustomResolveCase(testCtx, "color_multi_upload_multi_resolve_simple", params));
        }
        {
            // More complex case mixing color and depth/stencil attachments, with multiple upload and resolve passes.
            TestParams params;
            params.groupParams = groupParams;
            params.attachmentList.emplace_back(VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_4_BIT,
                                               VK_FORMAT_R16G16B16A16_UNORM, 1u);
            params.attachmentList.emplace_back(VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT,
                                               VK_FORMAT_R8G8B8A8_UNORM, 2u);
            params.attachmentList.emplace_back(VK_FORMAT_R16G16B16A16_UNORM, VK_SAMPLE_COUNT_4_BIT,
                                               VK_FORMAT_R8G8B8A8_UNORM, 0u);

            // The last attachment will be depth/stencil, but the format will be chosen below.
            params.attachmentList.emplace_back(VK_FORMAT_UNDEFINED, VK_SAMPLE_COUNT_4_BIT, VK_FORMAT_UNDEFINED, 0u);

            // Last color attachment.
            params.uploadPasses.push_back(UploadPass{
                CoveredArea(tcu::Vec2(2.0f, 2.0f), tcu::Vec2(-1.0f, -1.0f)),
                {AttachmentIndexAspect(2u, VK_IMAGE_ASPECT_COLOR_BIT)},
            });

            // Middle attachment. This needs to be separate because it's single-sampled.
            params.uploadPasses.push_back(UploadPass{
                CoveredArea(tcu::Vec2(2.0f, 2.0f), tcu::Vec2(-1.0f, -1.0f)),
                {AttachmentIndexAspect(1u, VK_IMAGE_ASPECT_COLOR_BIT)},
            });

            // First attachment together with depth/stencil.
            params.uploadPasses.push_back(UploadPass{
                CoveredArea(tcu::Vec2(2.0f, 2.0f), tcu::Vec2(-1.0f, -1.0f)),
                {
                    AttachmentIndexAspect(0u, VK_IMAGE_ASPECT_COLOR_BIT),
                    AttachmentIndexAspect(3u, (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)),
                },
            });

            // Resolve last attachment first.
            params.resolvePasses.push_back(ResolvePass{
                CoveredArea(tcu::Vec2(2.0f, 1.0f), tcu::Vec2(-1.0f, -1.0f)),
                {
                    AttachmentResolve(2u, VK_IMAGE_ASPECT_COLOR_BIT, ResolveType::SELECTED_SAMPLE, StrategyParams(1u)),
                },
            });

            // Finally, first and depth/stencil.
            params.resolvePasses.push_back(ResolvePass{
                CoveredArea(tcu::Vec2(1.0f, 2.0f), tcu::Vec2(0.0f, -1.0f)),
                {
                    AttachmentResolve(0u, VK_IMAGE_ASPECT_COLOR_BIT, ResolveType::SELECTED_SAMPLE, StrategyParams(1u)),
                    AttachmentResolve(3u, (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT),
                                      ResolveType::AVERAGE, StrategyParams()),
                },
            });

            const std::vector<VkFormat> dsFormats{
                VK_FORMAT_D24_UNORM_S8_UINT,
                VK_FORMAT_D32_SFLOAT_S8_UINT,
            };

            for (const auto &format : dsFormats)
            {
                params.attachmentList.back().attachmentFormat = format;
                params.attachmentList.back().resolveFormat    = format;
                const auto testName = "mix_multi_upload_multi_resolve_" + dsFormatNames.at(format);
                constructionGroup->addChild(new CustomResolveCase(testCtx, testName, params));
            }

            // Now we swap the resolve passes and make the resolve attachment locations the identity, which should
            // enable pass merging with dynamic rendering.
            for (uint32_t i = 0u; i < de::sizeU32(params.attachmentList); ++i)
                params.attachmentList.at(i).resolveLocation = i;
            std::swap(params.resolvePasses.front(), params.resolvePasses.back());

            for (const auto &format : dsFormats)
            {
                params.attachmentList.back().attachmentFormat = format;
                params.attachmentList.back().resolveFormat    = format;
                const auto testName = "mix_multi_upload_multi_resolve_with_merge_" + dsFormatNames.at(format);
                constructionGroup->addChild(new CustomResolveCase(testCtx, testName, params));
            }
        }
        {
            // Upload and resolve multiple color attachments at the same time, with and without remapping.
            TestParams params;
            params.groupParams = groupParams;
            params.attachmentList.emplace_back(VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_4_BIT,
                                               VK_FORMAT_R16G16B16A16_UNORM, 1u);
            params.attachmentList.emplace_back(VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT,
                                               VK_FORMAT_R8G8B8A8_UNORM, 2u);
            params.attachmentList.emplace_back(VK_FORMAT_R16G16B16A16_UNORM, VK_SAMPLE_COUNT_4_BIT,
                                               VK_FORMAT_R8G8B8A8_UNORM, 0u);

            // Middle attachment. This needs to be separate because it's single-sampled.
            params.uploadPasses.push_back(UploadPass{
                CoveredArea(tcu::Vec2(2.0f, 2.0f), tcu::Vec2(-1.0f, -1.0f)),
                {AttachmentIndexAspect(1u, VK_IMAGE_ASPECT_COLOR_BIT)},
            });

            // First and last color attachments.
            params.uploadPasses.push_back(UploadPass{
                CoveredArea(tcu::Vec2(2.0f, 2.0f), tcu::Vec2(-1.0f, -1.0f)),
                {
                    AttachmentIndexAspect(0u, VK_IMAGE_ASPECT_COLOR_BIT),
                    AttachmentIndexAspect(2u, VK_IMAGE_ASPECT_COLOR_BIT),
                },
            });

            // Resolve both multisample attachments.
            params.resolvePasses.push_back(ResolvePass{
                CoveredArea(tcu::Vec2(2.0f, 2.0f), tcu::Vec2(-1.0f, -1.0f)),
                {
                    AttachmentResolve(0u, VK_IMAGE_ASPECT_COLOR_BIT, ResolveType::SELECTED_SAMPLE, StrategyParams(2u)),
                    AttachmentResolve(2u, VK_IMAGE_ASPECT_COLOR_BIT, ResolveType::SELECTED_SAMPLE, StrategyParams(1u)),
                },
            });

            constructionGroup->addChild(
                new CustomResolveCase(testCtx, "color_upload_resolve_multi_attachment", params));

            // Make the resolve attachment locations the identity, which should enable pass merging with dynamic
            // rendering.
            for (uint32_t i = 0u; i < de::sizeU32(params.attachmentList); ++i)
                params.attachmentList.at(i).resolveLocation = i;
            constructionGroup->addChild(
                new CustomResolveCase(testCtx, "color_upload_resolve_multi_attachment_simple", params));
        }

        //if (groupParams->renderingType == RENDERING_TYPE_RENDERPASS_LEGACY)
        {
            for (const bool close : {false, true})
                for (const bool large : {false, true})
                {
                    const FragmentRegionParams params{
                        groupParams,
                        close,
                        large,
                    };
                    const auto testName =
                        std::string("fragment_region") + (close ? "_close" : "_far") + (large ? "_large" : "_small");
                    constructionGroup->addChild(new FragmentRegionCase(testCtx, testName, params));
                }
        }

        mainGroup->addChild(constructionGroup.release());
    }

    return mainGroup.release();
}

} // namespace renderpass
} // namespace vkt
