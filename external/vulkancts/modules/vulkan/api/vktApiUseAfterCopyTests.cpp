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
 * \brief Use-after-copy tests for images
 *//*--------------------------------------------------------------------*/
#include "vktApiCopiesAndBlittingUtil.hpp"
#include "vktApiUseAfterCopyTests.hpp"

namespace vkt
{

namespace api
{

namespace
{

using namespace vk;

// Copy data to an image and then try to use it as an attachment or texture later.
struct AfterUsageParams
{
    VkFormat format;
    tcu::IVec3 extent;
    QueueSelectionOptions queue;
    VkImageLayout transferLayout;      // General or transfer_dst.
    std::vector<VkRect2D> copyRegions; // By layer, if empty the full image will be copied.
    bool indirect;                     // Use indirect commands, or use the classic APIs.
    bool imageIs3D;                    // Make the original image 3D instead of 2D_ARRAY, then change it in the view.
    bool viewIs3D;                     // If imageIs3D, then make the view 3D as well.
    bool colorAttFlag;                 // Only applies to color formats: set color attachment usage bit.
    bool imageToImage;                 // Only when indirect is false.
    bool linear;                       // Use linear tiling for the test image.
    VkSampleCountFlagBits sampleCount; // If samples > 1, imageToImage must be true.

    bool isDepthStencilCase() const
    {
        return isDepthStencilFormat(format);
    }

    VkImageCreateInfo getImageCreateInfo() const
    {
        const bool isDS           = isDepthStencilCase();
        const auto usageAfterCopy = (isDS ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_SAMPLED_BIT);

        VkImageUsageFlags usage = 0u;
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        usage |= usageAfterCopy;
        if (colorAttFlag)
        {
            DE_ASSERT(!isDS);
            DE_ASSERT(!viewIs3D); // The view would be 3D, which is not appropriate for color attachments.

            usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }

        const auto imageType = (imageIs3D ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D);

        VkImageCreateFlags createFlags = 0u;
        if (imageIs3D && !viewIs3D)
        {
            if (extent.z() > 1)
                createFlags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
#ifndef CTS_USES_VULKANSC
            else
                createFlags |= VK_IMAGE_CREATE_2D_VIEW_COMPATIBLE_BIT_EXT;
#endif // CTS_USES_VULKANSC
        }

        if (viewIs3D)
        {
            DE_ASSERT(!isDS);
            DE_ASSERT(imageIs3D);
        }

        const tcu::IVec3 creationExtent(extent.x(), extent.y(), (imageIs3D ? extent.z() : 1));
        const auto creationLayers = (imageIs3D ? 1u : static_cast<uint32_t>(extent.z()));
        const auto tiling         = (linear ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL);

        const VkImageCreateInfo createInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            nullptr,
            createFlags,
            imageType,
            format,
            makeExtent3D(creationExtent),
            1u,
            creationLayers,
            sampleCount,
            tiling,
            usage,
            VK_SHARING_MODE_EXCLUSIVE,
            0u,
            nullptr,
            VK_IMAGE_LAYOUT_UNDEFINED,
        };

        return createInfo;
    }

    VkImageCreateInfo getColorAttCreateInfo(bool singleSample) const
    {
        const tcu::IVec3 creationExtent(extent.x(), extent.y(), 1);
        const auto creationLayers = static_cast<uint32_t>(extent.z());
        const auto usage =
            (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        const auto attSampleCount = (singleSample ? VK_SAMPLE_COUNT_1_BIT : sampleCount);

        const VkImageCreateInfo createInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            nullptr,
            0u,
            VK_IMAGE_TYPE_2D,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            makeExtent3D(creationExtent),
            1u,
            creationLayers,
            attSampleCount,
            VK_IMAGE_TILING_OPTIMAL,
            usage,
            VK_SHARING_MODE_EXCLUSIVE,
            0u,
            nullptr,
            VK_IMAGE_LAYOUT_UNDEFINED,
        };

        return createInfo;
    }

    // This one is used in image-to-image operations and is essentially identical to the regular image.
    VkImageCreateInfo getAuxiliarImageCreateInfo() const
    {
        const bool isMS = multiSample();
        const bool isDS = isDepthStencilCase();

        auto createInfo = getImageCreateInfo();
        // In the multisample case, the auxiliary image will be filled using a fragment shader. In the single-sample
        // case we can use a copyBufferToImage operation.
        const auto fillUsage(
            isMS ? (isDS ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) :
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        createInfo.usage = (fillUsage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        return createInfo;
    }

    tcu::Vec4 getClearColor() const
    {
        return tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    tcu::Vec4 getGeomColor() const
    {
        return tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);
    }

    // For the RNG.
    uint32_t getSeed() const
    {
        return static_cast<uint32_t>(format);
    }

    uint32_t getQueueFamilyIndex(const Context &ctx) const
    {
        if (queue == QueueSelectionOptions::Universal)
            return ctx.getUniversalQueueFamilyIndex();
        if (queue == QueueSelectionOptions::ComputeOnly)
            return ctx.getComputeQueueFamilyIndex();
        if (queue == QueueSelectionOptions::TransferOnly)
            return ctx.getTransferQueueFamilyIndex();

        DE_ASSERT(false);
        return std::numeric_limits<uint32_t>::max();
    }

    VkQueue getQueue(const Context &ctx) const
    {
        if (queue == QueueSelectionOptions::Universal)
            return ctx.getUniversalQueue();
        if (queue == QueueSelectionOptions::ComputeOnly)
            return ctx.getComputeQueue();
        if (queue == QueueSelectionOptions::TransferOnly)
            return ctx.getTransferQueue();

        DE_ASSERT(false);
        return VK_NULL_HANDLE;
    }

    bool multiSlice() const
    {
        return (extent.z() > 1);
    }

    bool multiSample() const
    {
        return (sampleCount > VK_SAMPLE_COUNT_1_BIT);
    }
};

class AfterUsageInstance : public vkt::TestInstance
{
public:
    AfterUsageInstance(Context &context, const AfterUsageParams &params) : TestInstance(context), m_params(params)
    {
    }
    virtual ~AfterUsageInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const AfterUsageParams m_params;
};

class AfterUsageCase : public vkt::TestCase
{
public:
    AfterUsageCase(tcu::TestContext &testCtx, const std::string &name, const AfterUsageParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~AfterUsageCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new AfterUsageInstance(context, m_params);
    }

protected:
    const AfterUsageParams m_params;
};

void AfterUsageCase::checkSupport(Context &context) const
{
    if (m_params.indirect)
    {
        context.requireDeviceFunctionality("VK_KHR_copy_memory_indirect");
#ifndef CTS_USES_VULKANSC
        const auto &cmiFeatures = context.getCopyMemoryIndirectFeatures();
        if (!cmiFeatures.indirectMemoryToImageCopy)
            TCU_THROW(NotSupportedError, "indirectMemoryToImageCopy not supported");
#endif // CTS_USES_VULKANSC
    }

    const auto createInfo = m_params.getImageCreateInfo();
    const auto isDS       = m_params.isDepthStencilCase();

    if (createInfo.imageType == VK_IMAGE_TYPE_3D && !m_params.viewIs3D)
    {
        if (m_params.extent.z() > 1)
            context.requireDeviceFunctionality("VK_KHR_maintenance1");
#ifndef CTS_USES_VULKANSC
        else
        {
            const auto &img2DViewOf3DFeatures = context.getImage2DViewOf3DFeaturesEXT();

            if (!img2DViewOf3DFeatures.image2DViewOf3D)
                TCU_THROW(NotSupportedError, "image2DViewOf3D not supported");

            if (!isDS && !img2DViewOf3DFeatures.sampler2DViewOf3D)
                TCU_THROW(NotSupportedError, "sampler2DViewOf3D not supported");
        }
#endif // CTS_USES_VULKANSC
    }

#ifndef CTS_USES_VULKANSC
    const auto formatProperties3 = context.getFormatProperties(createInfo.format);
#endif // CTS_USES_VULKANSC

    if (!m_params.imageToImage && !m_params.indirect && m_params.queue != QueueSelectionOptions::Universal && isDS)
    {
#ifndef CTS_USES_VULKANSC
        // Classic API needs maintenance10 for buffer-to-image or image-to-buffer on non-graphics queues for DS.
        const auto &m10Features = context.getMaintenance10Features();
        if (!m10Features.maintenance10)
            TCU_THROW(NotSupportedError, "maintenance10 not supported");

        // It also needs the proper feature flags.
        const auto wantedBit = ((m_params.queue == QueueSelectionOptions::ComputeOnly) ?
                                    VK_FORMAT_FEATURE_2_DEPTH_COPY_ON_COMPUTE_QUEUE_BIT_KHR :
                                    VK_FORMAT_FEATURE_2_DEPTH_COPY_ON_TRANSFER_QUEUE_BIT_KHR);
        if ((formatProperties3.optimalTilingFeatures & wantedBit) == 0u)
            TCU_THROW(NotSupportedError, "Depth copies not supported on the target queue");
#else
        context.requireDeviceFunctionality("VK_KHR_maintenance10");
#endif // CTS_USES_VULKANSC
    }

#ifndef CTS_USES_VULKANSC
    if (m_params.indirect)
    {
        // Basic format support check.
        if ((formatProperties3.optimalTilingFeatures & VK_FORMAT_FEATURE_2_COPY_IMAGE_INDIRECT_DST_BIT_KHR) == 0u)
            TCU_THROW(NotSupportedError, "Format does not support VK_FORMAT_FEATURE_2_COPY_IMAGE_INDIRECT_DST_BIT_KHR");
    }
#endif // CTS_USES_VULKANSC

    if (m_params.queue == QueueSelectionOptions::ComputeOnly)
        context.getComputeQueue(); // Will throw if not available.
    else if (m_params.queue == QueueSelectionOptions::TransferOnly)
    {
        for (uint32_t layerIdx = 0u; layerIdx < de::sizeU32(m_params.copyRegions); ++layerIdx)
        {
            const auto &copyRegion = m_params.copyRegions.at(layerIdx);
            const auto srl         = makeImageSubresourceLayers(
#ifndef CTS_USES_VULKANSC
                VK_IMAGE_ASPECT_NONE,
#else
                (VkImageAspectFlags)0u,
#endif
                0u, layerIdx, 1u);
            const auto offset = makeOffset3D(copyRegion.offset.x, copyRegion.offset.y, 0);
            const auto extent = makeExtent3D(copyRegion.extent.width, copyRegion.extent.height, 1u);

            const VkBufferImageCopy region = {0ull, 0u, 0u, srl, offset, extent};

            checkTransferQueueGranularity(context, createInfo, region);
        }
    }

    // Check compatibility with indirect copy commands on the queue.
    const auto ctx = context.getContextCommonData();
#ifndef CTS_USES_VULKANSC
    if (m_params.indirect)
    {
        bool supported  = false;
        int queueFamily = -1;

        if (m_params.queue == QueueSelectionOptions::ComputeOnly)
            queueFamily = context.getComputeQueueFamilyIndex();
        else if (m_params.queue == QueueSelectionOptions::TransferOnly)
            queueFamily = context.getTransferQueueFamilyIndex();
        else if (m_params.queue == QueueSelectionOptions::Universal)
            queueFamily = static_cast<int>(context.getUniversalQueueFamilyIndex());
        else
            DE_ASSERT(false);

        if (queueFamily >= 0)
        {
            const auto qfProperties     = getPhysicalDeviceQueueFamilyProperties(ctx.vki, ctx.physicalDevice);
            const auto chosenQueueFlags = qfProperties.at(queueFamily).queueFlags;
            const auto &cmiProperties   = context.getCopyMemoryIndirectProperties();
            supported                   = ((chosenQueueFlags & cmiProperties.supportedQueues) != 0u);
        }

        if (!supported)
            TCU_THROW(NotSupportedError, "Indirect copy commands not supported on the target queue");
    }
#endif // CTS_USES_VULKANSC

    // More detailed format checks.
    VkImageFormatProperties formatProperties;

    const auto res = ctx.vki.getPhysicalDeviceImageFormatProperties(
        ctx.physicalDevice, createInfo.format, createInfo.imageType, createInfo.tiling, createInfo.usage,
        createInfo.flags, &formatProperties);
    if (res == VK_ERROR_FORMAT_NOT_SUPPORTED)
        TCU_THROW(NotSupportedError, "Format not supported for the target parameters");

    if (formatProperties.maxArrayLayers < createInfo.arrayLayers)
        TCU_THROW(NotSupportedError, "Format does not support the required number of layers");

    if (formatProperties.maxExtent.width < createInfo.extent.width ||
        formatProperties.maxExtent.height < createInfo.extent.height)
        TCU_THROW(NotSupportedError, "Format does not support the required image extent");

    if ((formatProperties.sampleCounts & createInfo.samples) == 0)
        TCU_THROW(NotSupportedError, "Format does not support the required sample count");

    if (m_params.multiSlice())
    {
        context.requireDeviceFunctionality("VK_EXT_shader_viewport_index_layer");

        if (!m_params.isDepthStencilCase())
            context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
    }
}

void AfterUsageCase::initPrograms(vk::SourceCollections &dst) const
{
    // Layers: we'll have different geometry (points) for each layer of the framebuffer, so when drawing points for
    // separate layers, we will use the first vertex offset to choose the right points. In the vertex shader, these
    // points will be redirected to the appropriate layer using gl_Layer and assigning it gl_InstanceID. In the draws
    // for each layer, instanceCount will be 1, but the first instance index will need to match the intended layer.
    const bool multiSlice = m_params.multiSlice();
    std::ostringstream vert;
    vert << "#version 460\n"
         << (multiSlice ? "#extension GL_ARB_shader_viewport_layer_array : enable\n" : "")
         << "layout (location=0) in vec4 inPos;\n"
         << "layout (location=0) out flat int layerIndex;\n"
         << "void main(void) {\n"
         << "    gl_Position = inPos;\n"
         << "    gl_PointSize = 1.0;\n"
         << "    layerIndex = gl_InstanceIndex;\n"
         << (multiSlice ? "    gl_Layer = gl_InstanceIndex;\n" : "") << "}\n";

    // We need two versions of the shader due to gl_Layer.
    const auto src = vert.str();
    const vk::ShaderBuildOptions spv15Opts(dst.usedVulkanVersion, vk::SPIRV_VERSION_1_5, 0u, false);

    dst.glslSources.add("vert-spv10") << glu::VertexSource(src);
    dst.glslSources.add("vert-spv15") << glu::VertexSource(src) << spv15Opts;

    // Vertex shader used to copy values from a buffer to a multisample image. It has to generate a full-screen quad.
    std::ostringstream vertFill;
    vertFill << "#version 460\n"
             << (multiSlice ? "#extension GL_ARB_shader_viewport_layer_array : enable\n" : "")
             << "layout (location=0) out flat uint layerIndex;\n"
             << "void main(void) {\n"
             << "    const float x = float((gl_VertexIndex >> 1) & 1) * 2.0 - 1.0;\n"
             << "    const float y = float((gl_VertexIndex >> 0) & 1) * 2.0 - 1.0;\n"
             << "    gl_Position = vec4(x, y, 0.0, 1.0);\n"
             << "    gl_PointSize = 1.0;\n"
             << "    layerIndex = uint(gl_InstanceIndex);\n"
             << (multiSlice ? "    gl_Layer = gl_InstanceIndex;\n" : "") << "}\n";
    const auto fillSrc = vertFill.str();
    dst.glslSources.add("vert-fill-spv10") << glu::VertexSource(fillSrc);
    dst.glslSources.add("vert-fill-spv15") << glu::VertexSource(fillSrc) << spv15Opts;

    const auto geomColor   = m_params.getGeomColor();
    const bool isColorCase = !m_params.isDepthStencilCase();
    const bool &texIs3D    = m_params.viewIs3D;
    const bool isMS        = m_params.multiSample();

    if (isColorCase && texIs3D)
        DE_ASSERT(!isMS); // There is no 3D MS sampler.

    // For isColorCase
    std::string texSamplerType = "sampler";
    texSamplerType += (texIs3D ? "3D" : "2D");
    texSamplerType += (isMS ? "MS" : "");
    texSamplerType += ((multiSlice && !texIs3D) ? "Array" : "");

    const std::string lodOrSample = (isMS ? "gl_SampleID" : "0");

    std::ostringstream frag;
    frag << "#version 460\n"
         << (isColorCase ? "layout (set=0, binding=0) uniform " + texSamplerType + " tex;\n" : "")
         << "layout (location=0) in flat int layerIndex;\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main(void) {\n";

    if (isColorCase)
    {
        const auto coords = ((multiSlice || texIs3D) ? "ivec3(gl_FragCoord.xy, layerIndex)" : "ivec2(gl_FragCoord.xy)");
        frag << "    outColor = texelFetch(tex, " << coords << ", " << lodOrSample << ");\n";
    }
    else
        frag << "    outColor = vec4" << geomColor << ";\n";

    frag << "}\n";
    dst.glslSources.add("frag") << glu::FragmentSource(frag.str());

    // Fragment shader that fills a multisample color attachment with values from a buffer.
    std::ostringstream fragFill;
    fragFill << "#version 460\n"
             << "layout (location=0) in flat uint layerIndex;\n"
             << (isColorCase ? "layout (location=0) out vec4 outColor;\n" : "")
             << "layout (set=0, binding=0) readonly buffer PixelValuesBlock {\n"
             << "    vec4 values[];\n" // The depth value is in .x for depth/stencil cases.
             << "} pixels;\n"
             << "layout (push_constant, std430) uniform PushConstantBlock {\n"
             << "    float width;\n"
             << "    float height;\n"
             << "} pc;\n"
             << "void main (void) {\n"
             << "    const uint pixelsPerLayer = uint(pc.width * pc.height);\n"
             << "    const uint pixelIndex = pixelsPerLayer * layerIndex + uint(floor(gl_FragCoord.y) * pc.width + "
                "floor(gl_FragCoord.x));\n"
             << "    const uint sampleIndex = pixelIndex * " << static_cast<int>(m_params.sampleCount)
             << " + uint(gl_SampleID);\n"
             << (isColorCase ? "    outColor = pixels.values[sampleIndex];\n" :
                               "    gl_FragDepth = pixels.values[sampleIndex].x;\n")
             << "}\n";
    dst.glslSources.add("frag-fill") << glu::FragmentSource(fragFill.str());
}

// Converts floating point width (total or mantissa) to a threshold.
tcu::Vec4 bitWidthToThreshold(const tcu::IVec4 &bitWidth)
{
    const tcu::Vec4 threshold(bitWidth[0] > 0 ? 1.0f / ((float)(1 << bitWidth[0]) - 1.0f) : 0.0f,
                              bitWidth[1] > 0 ? 1.0f / ((float)(1 << bitWidth[1]) - 1.0f) : 0.0f,
                              bitWidth[2] > 0 ? 1.0f / ((float)(1 << bitWidth[2]) - 1.0f) : 0.0f,
                              bitWidth[3] > 0 ? 1.0f / ((float)(1 << bitWidth[3]) - 1.0f) : 0.0f);
    const tcu::Vec4 factor(1.25f); // Add a small margin to allow for at least 1 LSB difference.
    return threshold * factor;
}

// Only used for UNORM and SFLOAT.
tcu::Vec4 getColorFormatThreshold(VkFormat format)
{
    DE_ASSERT(!isDepthStencilFormat(format));

    const auto tcuFormat    = mapVkFormat(format);
    const auto channelClass = getTextureChannelClass(tcuFormat.type);

    tcu::Vec4 threshold(0.0f);

    if (channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT)
    {
        const tcu::IVec4 bitWidth(getTextureFormatBitDepth(tcuFormat));
        threshold = bitWidthToThreshold(bitWidth);

        if (isSRGB(tcuFormat))
        {
            // Widen thresholds a bit due to possible low-precision sRGB conversions.
            for (int i = 0; i < decltype(threshold)::SIZE; ++i)
                threshold[i] *= 2.0f;
        }
    }
    else if (channelClass == tcu::TEXTURECHANNELCLASS_FLOATING_POINT)
    {
        const tcu::IVec4 bitWidth(getTextureFormatMantissaBitDepth(tcuFormat));
        threshold = bitWidthToThreshold(bitWidth);
    }
    else
        DE_ASSERT(false);

    return threshold;
}

Move<VkRenderPass> makeCustomRenderPass(const DeviceInterface &vkd, VkDevice device, VkFormat colorFormat,
                                        VkFormat depthStencilFormat, VkSampleCountFlagBits sampleCount, bool resolve,
                                        VkAttachmentLoadOp loadOp)
{
    const bool isMS               = (sampleCount > VK_SAMPLE_COUNT_1_BIT);
    const bool hasColor           = (colorFormat != VK_FORMAT_UNDEFINED);
    const bool hasDS              = (depthStencilFormat != VK_FORMAT_UNDEFINED);
    const bool isClear            = (loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR);
    const auto initialLayoutColor = (isClear ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const auto initialLayoutDS =
        (isClear ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    if (resolve)
        DE_ASSERT(isMS);

    std::vector<VkAttachmentDescription> attDescs;
    std::vector<VkAttachmentReference> attRefs;

    const VkAttachmentReference *colorAtt   = nullptr;
    const VkAttachmentReference *resolveAtt = nullptr;
    const VkAttachmentReference *dsAtt      = nullptr;

    // Upper limits, makes sure pointers do not change.
    attDescs.reserve(3u);
    attRefs.reserve(3u);

    // Main color attachment, which may be multisample.
    if (hasColor)
    {
        const auto storeOp = ((isMS && resolve) ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE);

        attDescs.push_back(makeAttachmentDescription(0u, colorFormat, sampleCount, loadOp, storeOp,
                                                     VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                     VK_ATTACHMENT_STORE_OP_DONT_CARE, // Stencil ops.
                                                     initialLayoutColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

        attRefs.push_back(makeAttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

        colorAtt = &attRefs.back();
    }

    // Color resolve attachment.
    if (hasColor && resolve)
    {
        const auto ssLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // Values will be loaded or cleared in the MS attachment.
        const auto storeOp  = VK_ATTACHMENT_STORE_OP_STORE;    // Store the result of the resolve.

        attDescs.push_back(makeAttachmentDescription(0u, colorFormat, VK_SAMPLE_COUNT_1_BIT, ssLoadOp, storeOp,
                                                     VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                     VK_ATTACHMENT_STORE_OP_DONT_CARE, // Stencil ops.
                                                     initialLayoutColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

        const auto attIndex = de::sizeU32(attDescs) - 1u;
        attRefs.push_back(makeAttachmentReference(attIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

        resolveAtt = &attRefs.back();
    }

    // Main depth/stencil attachment, which may be multisample.
    if (hasDS)
    {
        const auto storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Always store in case we want to use the values later.

        attDescs.push_back(
            makeAttachmentDescription(0u, depthStencilFormat, sampleCount, loadOp, storeOp,
                                      VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, // Stencil ops.
                                      initialLayoutDS, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));

        const auto attIndex = de::sizeU32(attDescs) - 1u;
        attRefs.push_back(makeAttachmentReference(attIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));

        dsAtt = &attRefs.back();
    }

    const VkSubpassDescription subpassDescription = {
        0u,      VK_PIPELINE_BIND_POINT_GRAPHICS, 0u, nullptr, (colorAtt ? 1u : 0u), colorAtt, resolveAtt, dsAtt, 0u,
        nullptr,
    };

    const VkRenderPassCreateInfo rpCreateInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        nullptr,
        0u,
        de::sizeU32(attDescs),
        de::dataOrNull(attDescs),
        1u,
        &subpassDescription,
        0u,
        nullptr,
    };

    return createRenderPass(vkd, device, &rpCreateInfo);
}

tcu::TestStatus AfterUsageInstance::iterate(void)
{
    const auto ctx = m_context.getContextCommonData();

    const bool useDeviceAddresses = (m_context.getDeviceVulkan12Features().bufferDeviceAddress == VK_TRUE);

    const auto isDS       = m_params.isDepthStencilCase();
    const auto isMS       = m_params.multiSample();
    const auto isSrgb     = (!isDS && isSrgbFormat(m_params.format));
    const auto geomColor  = m_params.getGeomColor();
    const auto clearColor = m_params.getClearColor();
    const auto bindPoint  = VK_PIPELINE_BIND_POINT_GRAPHICS;

    const auto imgCreateInfo = m_params.getImageCreateInfo();
    ImageWithMemory image(ctx.vkd, ctx.device, ctx.allocator, imgCreateInfo, MemoryRequirement::Any);
    const auto tcuFormat = mapVkFormat(imgCreateInfo.format);
    const auto imgSRR    = makeImageSubresourceRange(getImageAspectFlags(tcuFormat), 0u, imgCreateInfo.mipLevels, 0u,
                                                     imgCreateInfo.arrayLayers);
    const auto imageViewType =
        (m_params.viewIs3D ? VK_IMAGE_VIEW_TYPE_3D :
                             (m_params.multiSlice() ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D));
    const auto view = makeImageView(ctx.vkd, ctx.device, *image, imageViewType, imgCreateInfo.format, imgSRR);

    // This will be the main or resolve attachment.
    const auto attCreateInfo = m_params.getColorAttCreateInfo(true /*do not force single-sample*/);
    const auto attSRR        = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, attCreateInfo.mipLevels, 0u,
                                                         attCreateInfo.arrayLayers);
    ImageWithBuffer attImage(ctx.vkd, ctx.device, ctx.allocator, attCreateInfo.extent, attCreateInfo.format,
                             attCreateInfo.usage, attCreateInfo.imageType, attSRR, attCreateInfo.arrayLayers,
                             attCreateInfo.samples, attCreateInfo.tiling, attCreateInfo.mipLevels);

    // This will be the multisample attachment, if needed.
    using ImageWithMemoryPtr = std::unique_ptr<ImageWithMemory>;
    ImageWithMemoryPtr msAttImage;
    Move<VkImageView> msAttView;
    if (isMS)
    {
        DE_ASSERT(m_params.imageToImage);

        const auto msAttCreateInfo = m_params.getColorAttCreateInfo(false /*do not force single-sample*/);
        msAttImage.reset(
            new ImageWithMemory(ctx.vkd, ctx.device, ctx.allocator, msAttCreateInfo, MemoryRequirement::Any));

        const auto viewType =
            ((msAttCreateInfo.arrayLayers == 1) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY);
        msAttView = makeImageView(ctx.vkd, ctx.device, msAttImage->get(), viewType, msAttCreateInfo.format, attSRR);
    }

    // The image-to-image case is essentially similar to buffer-to-image. However, instead of copying data from the
    // buffer to the image, in whole or in part, the full buffer is copied first to an auxiliary image, declared below.
    // Then, the regions to copy to the final target image are copied using image-to-image operations, with the same
    // region as the source and destination in both images.
    const auto auxImgCreateInfo = m_params.getAuxiliarImageCreateInfo();
    std::unique_ptr<ImageWithMemory> auxImage;
    Move<VkImageView> auxImageView;
    if (m_params.imageToImage)
    {
        auxImage.reset(
            new ImageWithMemory(ctx.vkd, ctx.device, ctx.allocator, auxImgCreateInfo, MemoryRequirement::Any));
        if (isMS)
        {
            const auto viewType =
                ((auxImgCreateInfo.arrayLayers == 1) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY);
            auxImageView =
                makeImageView(ctx.vkd, ctx.device, auxImage->get(), viewType, auxImgCreateInfo.format, imgSRR);
        }
    }

    const auto useStage = static_cast<VkPipelineStageFlags>(
        isDS ? (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT) :
               (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT));
    const auto useAccess = static_cast<VkImageUsageFlags>(
        isDS ? (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT) :
               (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT));
    const auto useLayout =
        (isDS ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    const uint32_t seed = m_params.getSeed();
    de::Random rng(seed);

    // In the depth case, for each pixel in the framebuffer we're going to choose a pseudorandom value that will be
    // copied to the depth buffer. Then, we are going to draw a point with a depth value that's pseudorandom as well, so
    // pseudorandomly passing or failing the depth test. We will later verify results using the color buffer and which
    // pixels were drawn or failed to draw. To avoid precision issues, the depths of both the depth buffer and the
    // points will be chosen as 16-bit UINTS, and later converted to the right format and values for copying.
    //
    // In the color case, we're going to generate pseudorandom color values for the color texture, and those values are
    // going to be read from the fragment shader and stored in the color buffer.
    const auto extentU            = m_params.extent.asUint();
    const auto perPixelSamples    = static_cast<uint32_t>(m_params.sampleCount);
    const auto perPixelSamplesI   = static_cast<int>(m_params.sampleCount);
    const auto pixelCountPerLayer = extentU.x() * extentU.y();
    const auto pixelCount         = pixelCountPerLayer * extentU.z();
    const auto totalSampleCount   = pixelCount * perPixelSamples;

    std::vector<uint16_t> depthBufferValues;
    std::vector<uint16_t> pointDepthValues;

    std::unique_ptr<tcu::TextureLevel> textureLevel;
    std::unique_ptr<tcu::PixelBufferAccess> textureAccess;

    // These will be used as a buffer in the multisample case to fill the auxiliary image.
    std::vector<tcu::Vec4> fragFillValues;
    if (isMS)
        fragFillValues.reserve(totalSampleCount);

    const float maxDepth = static_cast<float>(std::numeric_limits<uint16_t>::max());

    if (isDS)
    {
        depthBufferValues.reserve(totalSampleCount);
        pointDepthValues.reserve(pixelCount);

        for (uint32_t i = 0u; i < pixelCount; ++i)
        {
            const auto depthBufferValue = rng.getUint16();
            const auto normDepth        = static_cast<float>(depthBufferValue) / maxDepth;
            for (uint32_t s = 0u; s < perPixelSamples; ++s)
            {
                fragFillValues.emplace_back(normDepth, 0.0f, 0.0f, 0.0f);
                depthBufferValues.push_back(depthBufferValue);
            }
            pointDepthValues.push_back(rng.getUint16());
        }
    }
    else
    {
        // For the multisample case, group all samples in a single pixel next to each other.
        const int iSampleCount         = static_cast<int>(m_params.sampleCount);
        const int realHorizontalExtent = m_params.extent.x() * iSampleCount;

        textureLevel.reset(
            new tcu::TextureLevel(tcuFormat, realHorizontalExtent, m_params.extent.y(), m_params.extent.z()));
        textureAccess.reset(new tcu::PixelBufferAccess(textureLevel->getAccess()));

        for (int z = 0; z < m_params.extent.z(); ++z)
            for (int y = 0; y < m_params.extent.y(); ++y)
                for (int x = 0; x < m_params.extent.x(); ++x)
                {
                    const auto red   = rng.getFloat();
                    const auto green = rng.getFloat();
                    const auto blue  = rng.getFloat();
                    const tcu::Vec4 color(red, blue, green, 1.0f);

                    for (int s = 0; s < iSampleCount; ++s)
                    {
                        if (isMS)
                            fragFillValues.push_back(color);
                        const auto realX = x * iSampleCount + s;
                        textureAccess->setPixel(color, realX, y, z);
                    }
                }
    }

    // Vertex buffer contents: one point per pixel and layer.
    std::vector<tcu::Vec4> vertices;
    vertices.reserve(pixelCount);

    const auto extentF   = m_params.extent.asFloat();
    const auto normalize = [](uint32_t n, float dim) { return (static_cast<float>(n) + 0.5f) / dim * 2.0f - 1.0f; };

    const auto getPixelID = [](uint32_t x, uint32_t y, uint32_t z, const tcu::UVec3 &extent)
    { return (z * extent.x() * extent.y() + y * extent.x() + x); };

    for (uint32_t z = 0u; z < extentU.z(); ++z)
        for (uint32_t y = 0u; y < extentU.y(); ++y)
            for (uint32_t x = 0u; x < extentU.x(); ++x)
            {
                const auto xCoord  = normalize(x, extentF.x());
                const auto yCoord  = normalize(y, extentF.y());
                const auto pixelID = getPixelID(x, y, z, extentU);
                const auto depth   = (isDS ? (static_cast<float>(pointDepthValues.at(pixelID)) / maxDepth) : 0.0f);
                vertices.emplace_back(xCoord, yCoord, depth, 1.0f);
            }

    const auto vertexBufferSize  = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vertexBufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    const auto vertexBufferInfo  = makeBufferCreateInfo(vertexBufferSize, vertexBufferUsage);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferInfo, HostIntent::W);
    {
        auto &alloc = vertexBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(vertices), de::dataSize(vertices));
        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    // In bytes, in the source memory/buffer.
    uint32_t itemSize = 0u;
    if (isDS)
    {
        switch (m_params.format)
        {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_D16_UNORM_S8_UINT:
            itemSize = 2u;
            break;
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            itemSize = 4u;
            break;
        default:
            DE_ASSERT(false);
            break;
        }
    }
    else
    {
        itemSize = static_cast<uint32_t>(tcu::getPixelSize(tcuFormat));
    }

    std::vector<uint8_t> depthMemoryBytes;

    if (isDS)
    {
        const auto depthMemorySize = itemSize * totalSampleCount;
        depthMemoryBytes.reserve(depthMemorySize);

        // Depth buffer values, in the format expected for copying.
        for (const auto &depthBufferValue : depthBufferValues)
        {
            switch (m_params.format)
            {
            case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_D16_UNORM_S8_UINT:
            {
                // 16-bit value as is.
                const auto *ptr = reinterpret_cast<const uint8_t *>(&depthBufferValue);
                depthMemoryBytes.push_back(*ptr++);
                depthMemoryBytes.push_back(*ptr++);
                break;
            }
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_X8_D24_UNORM_PACK32:
            {
                // Translate to 24-bit, store as 32-bit.
                const float normDepth  = static_cast<float>(depthBufferValue) / maxDepth;
                const uint32_t depth24 = static_cast<uint32_t>(normDepth * static_cast<float>((1u << 24) - 1u) + 0.5f);
                const auto *ptr        = reinterpret_cast<const uint8_t *>(&depth24);
                for (int i = 0; i < 4; ++i)
                    depthMemoryBytes.push_back(*ptr++);
                break;
            }
            case VK_FORMAT_D32_SFLOAT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
            {
                // Translate to normalized float, store as is.
                const float normDepth = static_cast<float>(depthBufferValue) / maxDepth;
                const auto *ptr       = reinterpret_cast<const uint8_t *>(&normDepth);
                for (int i = 0; i < 4; ++i)
                    depthMemoryBytes.push_back(*ptr++);
                break;
            }
            default:
                DE_ASSERT(false);
                break;
            }
        }
    }

    // Depth or texture memory (buffer).
    std::unique_ptr<BufferWithMemory> memoryBuffer;
    const auto memoryBufferUsage = static_cast<VkBufferUsageFlags>(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        (useDeviceAddresses ? static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) : 0u));
    const VkMemoryAllocateFlags memAllocDevAddr =
        useDeviceAddresses ? static_cast<VkMemoryAllocateFlags>(VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT) : 0u;

    if (isDS)
    {
        const auto depthMemoryBufferSize = static_cast<VkDeviceSize>(de::dataSize(depthMemoryBytes));
        const auto depthMemoryBufferInfo = makeBufferCreateInfo(depthMemoryBufferSize, memoryBufferUsage);

        memoryBuffer.reset(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, depthMemoryBufferInfo,
                                                HostIntent::W, true, memAllocDevAddr));
        {
            auto &alloc = memoryBuffer->getAllocation();
            memcpy(alloc.getHostPtr(), de::dataOrNull(depthMemoryBytes), de::dataSize(depthMemoryBytes));
            flushAlloc(ctx.vkd, ctx.device, alloc);
        }
    }
    else
    {
        const auto texMemoryBufferSize = static_cast<VkDeviceSize>(itemSize * totalSampleCount);
        const auto texMemoryBufferInfo = makeBufferCreateInfo(texMemoryBufferSize, memoryBufferUsage);

        memoryBuffer.reset(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, texMemoryBufferInfo, HostIntent::W,
                                                true, memAllocDevAddr));
        {
            auto &alloc = memoryBuffer->getAllocation();
            memcpy(alloc.getHostPtr(), textureAccess->getDataPtr(), static_cast<size_t>(texMemoryBufferSize));
            flushAlloc(ctx.vkd, ctx.device, alloc);
        }
    }
    const VkDeviceAddress baseMemoryAddress =
        useDeviceAddresses ? getBufferDeviceAddress(ctx.vkd, ctx.device, memoryBuffer->get()) : 0u;

    DE_ASSERT(imgCreateInfo.mipLevels == 1u);
    const auto copyCount = m_params.copyRegions.empty() ? 1u : m_params.copyRegions.size();

#ifdef CTS_USES_VULKANSC
    struct VkCopyMemoryToImageIndirectCommandKHR
    {
        VkDeviceAddress srcAddress;
        uint32_t bufferRowLength;
        uint32_t bufferImageHeight;
        VkImageSubresourceLayers imageSubresource;
        VkOffset3D imageOffset;
        VkExtent3D imageExtent;
    };
#endif
    std::vector<VkCopyMemoryToImageIndirectCommandKHR> indirectCmds;
    indirectCmds.reserve(copyCount);

    std::vector<VkImageSubresourceLayers> copySRLs;
    copySRLs.reserve(copyCount);

    const auto copyAspect = (isDS ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT);

    if (m_params.copyRegions.empty())
    {
        // Full-image copy. For 3D images in the case of indirect copies, slices of the image are specified as layers
        // (similar to the case of 2D arrays). For direct copies this change does not apply.
        const auto layerCount =
            ((m_params.imageIs3D && m_params.indirect) ? imgCreateInfo.extent.depth : imgCreateInfo.arrayLayers);
        const auto srl = makeImageSubresourceLayers(copyAspect, 0u, 0u, layerCount);

        indirectCmds.push_back(VkCopyMemoryToImageIndirectCommandKHR{
            baseMemoryAddress,
            0u,
            0u,
            srl,
            makeOffset3D(0, 0, 0),
            imgCreateInfo.extent,
        });

        copySRLs.push_back(srl);
    }
    else
    {
        // Multiple regions and partial copies.
        DE_ASSERT(m_params.copyRegions.size() ==
                  static_cast<size_t>(m_params.imageIs3D ? imgCreateInfo.extent.depth : imgCreateInfo.arrayLayers));

        for (uint32_t layerIdx = 0u; layerIdx < de::sizeU32(m_params.copyRegions); ++layerIdx)
        {
            // Note for 3D images in the case of indirect copies, slices of the image are specified as layers (similar
            // to the case of 2D arrays). For direct copies this change does not apply.
            const auto &copyRegion = m_params.copyRegions.at(layerIdx);
            const auto baseLayer   = ((m_params.imageIs3D && !m_params.indirect) ? 0u : layerIdx);
            const auto offsetZ     = ((m_params.imageIs3D && !m_params.indirect) ? static_cast<int32_t>(layerIdx) : 0);
            const auto srl         = makeImageSubresourceLayers(copyAspect, 0u, baseLayer, 1u);
            const auto offset      = makeOffset3D(copyRegion.offset.x, copyRegion.offset.y, offsetZ);
            const auto extent      = makeExtent3D(copyRegion.extent.width, copyRegion.extent.height, 1u);

            const auto pixelIdx   = (layerIdx * pixelCountPerLayer + offset.y * imgCreateInfo.extent.width + offset.x);
            const auto byteOffset = pixelIdx * itemSize;
            const auto address    = baseMemoryAddress + byteOffset;

            indirectCmds.push_back(VkCopyMemoryToImageIndirectCommandKHR{
                address,
                imgCreateInfo.extent.width,
                imgCreateInfo.extent.height,
                srl,
                offset,
                extent,
            });

            copySRLs.push_back(srl);
        }
    }

    const auto indirectBufferSize  = static_cast<VkDeviceSize>(de::dataSize(indirectCmds));
    const auto indirectBufferUsage = static_cast<VkBufferUsageFlags>(
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
        (useDeviceAddresses ? static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) : 0u));
    const auto indirectBufferInfo = makeBufferCreateInfo(indirectBufferSize, indirectBufferUsage);
    BufferWithMemory indirectBuffer(
        ctx.vkd, ctx.device, ctx.allocator, indirectBufferInfo, HostIntent::W, true,
        useDeviceAddresses ? static_cast<VkMemoryAllocateFlags>(VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT) : 0u);
    {
        auto &alloc = indirectBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(indirectCmds), de::dataSize(indirectCmds));
        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    // Queues, command pools and buffers.
    const auto qfIndex = m_params.getQueueFamilyIndex(m_context);
    const auto queue   = m_params.getQueue(m_context);

    const CommandPoolWithBuffer cmdTransfer(ctx.vkd, ctx.device, qfIndex);
    const auto cmdBufferTransfer = *cmdTransfer.cmdBuffer;

    const CommandPoolWithBuffer cmdGraphics(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBufferGraphics = *cmdGraphics.cmdBuffer;
    const auto cmdBufferClearPtr =
        allocateCommandBuffer(ctx.vkd, ctx.device, *cmdGraphics.cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBufferClear = *cmdBufferClearPtr;

    // Partial copies imply a clear to get predictable results, and tests using the color attachment usage flag are more
    // interesting if we force a clear using a render pass.
    const bool needsClear  = (!m_params.copyRegions.empty() || m_params.colorAttFlag);
    const bool needsFill   = (m_params.imageToImage && isMS);
    const bool needsAux    = (m_params.imageToImage && !isMS);
    const bool queueSwitch = (qfIndex != ctx.qfIndex);

    // Barrier that handles moving the resource image the transfer queue to the use queue.
    const auto switchQueueBarrier =
        makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, useAccess, m_params.transferLayout, useLayout, *image,
                               imgSRR, qfIndex, ctx.qfIndex);

    // Barrier that moves the auxiliary image from the fill/graphics queue to the transfer queue, and changes its layout
    // if needed.
    const auto preCopyAuxBarrier =
        makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, m_params.transferLayout,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, (auxImage ? auxImage->get() : VK_NULL_HANDLE),
                               imgSRR, ctx.qfIndex, qfIndex);

    // If the two queues are really separate, we may need two barriers, one for each queue.
    auto transferQueueBarrier          = switchQueueBarrier;
    transferQueueBarrier.dstAccessMask = 0u;

    auto graphicsQueueBarrier          = switchQueueBarrier;
    graphicsQueueBarrier.srcAccessMask = 0u;

    const auto recordInitialLayoutBarrier = [&](VkCommandBuffer cmdBuffer, VkImage imgHandle)
    {
        const auto barrier = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                    m_params.transferLayout, imgHandle, imgSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &barrier);
    };

    const auto clearToTransferBarrier =
        [&](VkCommandBuffer cmdBuffer, VkAccessFlags srcAccess, VkPipelineStageFlags srcStage, VkImageLayout srcLayout)
    {
        const auto barrier = makeImageMemoryBarrier(srcAccess, VK_ACCESS_TRANSFER_WRITE_BIT, srcLayout,
                                                    m_params.transferLayout, *image, imgSRR, ctx.qfIndex, qfIndex);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, srcStage, VK_PIPELINE_STAGE_TRANSFER_BIT, &barrier);
    };

    const auto fillToTransferBarrier =
        [&](VkCommandBuffer cmdBuffer, VkAccessFlags srcAccess, VkPipelineStageFlags srcStage, VkImageLayout srcLayout)
    {
        const auto barrier =
            makeImageMemoryBarrier(srcAccess, VK_ACCESS_TRANSFER_READ_BIT, srcLayout,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, auxImage->get(), imgSRR, ctx.qfIndex, qfIndex);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, srcStage, VK_PIPELINE_STAGE_TRANSFER_BIT, &barrier);
    };

    // When we need to clear color images and we have enabled color attachment usage, we'll make the test a bit more
    // interesting by clearing the image using a "fake" render pass with a clear op.
    Move<VkRenderPass> clearRenderPass;
    Move<VkFramebuffer> clearFramebuffer;
    const bool clearWithRenderPass = (!isDS && m_params.colorAttFlag);
    const auto layoutAfterClear =
        (clearWithRenderPass ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : m_params.transferLayout);

    // For the fill pipeline, if used.
    Move<VkDescriptorPool> fillPool;
    Move<VkDescriptorSetLayout> fillSetLayout;
    Move<VkPipelineLayout> fillPipelineLayout;
    Move<VkDescriptorSet> fillSet;
    std::unique_ptr<BufferWithMemory> fillBuffer;
    Move<VkRenderPass> fillRP;
    Move<VkFramebuffer> fillFB;
    Move<VkPipeline> fillPipeline;
    VkImageLayout afterFillLayout =
        (isDS ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    const auto dataStages  = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto fillPcSize  = DE_SIZEOF32(tcu::Vec2);
    const auto fillPcRange = makePushConstantRange(dataStages, 0u, fillPcSize);
    const auto binding     = DescriptorSetUpdateBuilder::Location::binding;

    const auto &binaries   = m_context.getBinaryCollection();
    const auto vk12Support = m_context.contextSupports(vk::ApiVersion(0u, 1u, 2u, 0u));

    const std::vector<VkViewport> viewports(1u, makeViewport(m_params.extent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(m_params.extent));

    if (needsClear || needsFill || needsAux)
    {
        // Command buffer with the clear op.
        beginCommandBuffer(ctx.vkd, cmdBufferClear);

        if (needsClear)
        {
            if (!clearWithRenderPass) // Else: the clear render pass will transition layouts.
                recordInitialLayoutBarrier(cmdBufferClear, *image);

            if (isDS)
            {
                // Cleared values would never pass the depth test.
                const auto clearDepthValue = makeClearValueDepthStencil(0.0f, 0u);
                ctx.vkd.cmdClearDepthStencilImage(cmdBufferClear, *image, m_params.transferLayout,
                                                  &clearDepthValue.depthStencil, 1u, &imgSRR);
            }
            else
            {
                if (m_params.colorAttFlag)
                {
                    clearRenderPass = makeCustomRenderPass(ctx.vkd, ctx.device, m_params.format, VK_FORMAT_UNDEFINED,
                                                           m_params.sampleCount, false, VK_ATTACHMENT_LOAD_OP_CLEAR);
                    clearFramebuffer =
                        makeFramebuffer(ctx.vkd, ctx.device, *clearRenderPass, *view, imgCreateInfo.extent.width,
                                        imgCreateInfo.extent.height, imgCreateInfo.arrayLayers);
                    const auto renderArea = makeRect2D(imgCreateInfo.extent.width, imgCreateInfo.extent.height);
                    beginRenderPass(ctx.vkd, cmdBufferClear, *clearRenderPass, *clearFramebuffer, renderArea,
                                    clearColor);
                    endRenderPass(ctx.vkd, cmdBufferClear);
                }
                else
                {
                    const auto clearColorValue = makeClearValueColor(clearColor);
                    ctx.vkd.cmdClearColorImage(cmdBufferClear, *image, m_params.transferLayout, &clearColorValue.color,
                                               1u, &imgSRR);
                }
            }

            const auto srcAccess =
                (clearWithRenderPass ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_TRANSFER_WRITE_BIT);
            const auto srcStage =
                (clearWithRenderPass ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT);
            clearToTransferBarrier(cmdBufferClear, srcAccess, srcStage,
                                   layoutAfterClear); // Sync clear and transfer, and maybe release resource.
        }

        if (needsFill)
        {
            // We need to fill the auxiliary image using a copy pipeline, using fragCopyValues.
            const auto copyBufferSize       = static_cast<VkDeviceSize>(de::dataSize(fragFillValues));
            const auto copyBufferCreateInfo = makeBufferCreateInfo(copyBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            fillBuffer.reset(
                new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, copyBufferCreateInfo, HostIntent::W));
            {
                auto &alloc = fillBuffer->getAllocation();
                memcpy(alloc.getHostPtr(), de::dataOrNull(fragFillValues), de::dataSize(fragFillValues));
                flushAlloc(ctx.vkd, ctx.device, alloc);
            }

            const auto descType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            DescriptorPoolBuilder poolBuilder;
            poolBuilder.addType(descType);
            fillPool = poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

            DescriptorSetLayoutBuilder setLayoutBuilder;
            setLayoutBuilder.addSingleBinding(descType, dataStages);
            fillSetLayout      = setLayoutBuilder.build(ctx.vkd, ctx.device);
            fillPipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *fillSetLayout, &fillPcRange);
            fillSet            = makeDescriptorSet(ctx.vkd, ctx.device, *fillPool, *fillSetLayout);

            DescriptorSetUpdateBuilder updateBuilder;
            const auto bufferInfo = makeDescriptorBufferInfo(fillBuffer->get(), 0u, VK_WHOLE_SIZE);
            updateBuilder.writeSingle(*fillSet, binding(0u), descType, &bufferInfo);
            updateBuilder.update(ctx.vkd, ctx.device);

            const auto rpColorFormat = (isDS ? VK_FORMAT_UNDEFINED : auxImgCreateInfo.format);
            const auto rpDSFormat    = (isDS ? auxImgCreateInfo.format : VK_FORMAT_UNDEFINED);
            fillRP = makeCustomRenderPass(ctx.vkd, ctx.device, rpColorFormat, rpDSFormat, m_params.sampleCount, false,
                                          VK_ATTACHMENT_LOAD_OP_CLEAR);
            fillFB = makeFramebuffer(ctx.vkd, ctx.device, *fillRP, *auxImageView, auxImgCreateInfo.extent.width,
                                     auxImgCreateInfo.extent.height, auxImgCreateInfo.arrayLayers);

            const auto vertFillShader = createShaderModule(
                ctx.vkd, ctx.device, binaries.get(vk12Support ? "vert-fill-spv15" : "vert-fill-spv10"));
            const auto fragFillShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag-fill"));

            const VkPipelineVertexInputStateCreateInfo fillVertexInputState = initVulkanStructureConst();
            const VkPipelineMultisampleStateCreateInfo fillMSState          = {
                VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                nullptr,
                0u,
                m_params.sampleCount,
                VK_FALSE,
                0.0f,
                nullptr,
                VK_FALSE,
                VK_FALSE,
            };
            VkStencilOpState stencilOpState;
            memset(&stencilOpState, 0, sizeof(stencilOpState));
            const VkPipelineDepthStencilStateCreateInfo fillDSState = {
                VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                nullptr,
                0u,
                (isDS ? VK_TRUE : VK_FALSE),
                (isDS ? VK_TRUE : VK_FALSE),
                VK_COMPARE_OP_ALWAYS,
                VK_FALSE,
                VK_FALSE,
                stencilOpState,
                stencilOpState,
                0.0f,
                1.0f,
            };
            std::vector<VkPipelineColorBlendAttachmentState> fillCBAttState;
            if (!isDS)
            {
                VkPipelineColorBlendAttachmentState attState;
                memset(&attState, 0, sizeof(attState));
                attState.colorWriteMask = 0xFu;
                fillCBAttState.push_back(attState);
            }
            const VkPipelineColorBlendStateCreateInfo fillCBState = {
                VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                nullptr,
                0u,
                VK_FALSE,
                vk::VK_LOGIC_OP_AND,
                de::sizeU32(fillCBAttState),
                de::dataOrNull(fillCBAttState),
                {0.0f, 0.0f, 0.0f, 0.0f},
            };
            fillPipeline = makeGraphicsPipeline(
                ctx.vkd, ctx.device, *fillPipelineLayout, *vertFillShader, VK_NULL_HANDLE, VK_NULL_HANDLE,
                VK_NULL_HANDLE, *fragFillShader, *fillRP, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u,
                0u, &fillVertexInputState, nullptr, &fillMSState, &fillDSState, &fillCBState);

            const auto pcValues       = m_params.extent.swizzle(0, 1).asFloat();
            const auto fillClearValue = makeClearValueColor(tcu::Vec4(0.0f));

            beginRenderPass(ctx.vkd, cmdBufferClear, *fillRP, *fillFB, scissors.front(), fillClearValue);
            ctx.vkd.cmdBindDescriptorSets(cmdBufferClear, bindPoint, *fillPipelineLayout, 0u, 1u, &fillSet.get(), 0u,
                                          nullptr);
            ctx.vkd.cmdPushConstants(cmdBufferClear, *fillPipelineLayout, dataStages, 0u, fillPcSize, &pcValues);
            ctx.vkd.cmdBindPipeline(cmdBufferClear, bindPoint, *fillPipeline);
            ctx.vkd.cmdDraw(cmdBufferClear, 4u, auxImgCreateInfo.arrayLayers, 0u, 0u);
            endRenderPass(ctx.vkd, cmdBufferClear);

            const auto srcAccess = static_cast<VkAccessFlags>(isDS ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT :
                                                                     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
            const auto srcStage  = static_cast<VkPipelineStageFlags>(
                isDS ? (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT) :
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
            fillToTransferBarrier(cmdBufferClear, srcAccess, srcStage, afterFillLayout);
        }

        if (needsAux)
        {
            // For the non-MS image-to-image case, we will copy the whole buffer first to the auxiliary image using a full
            // image copy with vkCmdCopyBufferToImage.
            const auto auxImgHandle = auxImage->get();

            // Prepare auxiliary image with the full buffer contents.
            recordInitialLayoutBarrier(cmdBufferClear, auxImgHandle);

            // Copy the full buffer to the auxiliary image.
            const auto fullSRL    = makeImageSubresourceLayers(copyAspect, 0u, 0u, imgCreateInfo.arrayLayers);
            const auto fullRegion = makeBufferImageCopy(auxImgCreateInfo.extent, fullSRL);
            ctx.vkd.cmdCopyBufferToImage(cmdBufferClear, memoryBuffer->get(), auxImgHandle, m_params.transferLayout, 1u,
                                         &fullRegion);

            cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBufferClear, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                          VK_PIPELINE_STAGE_TRANSFER_BIT, &preCopyAuxBarrier);
        }

        endCommandBuffer(ctx.vkd, cmdBufferClear);
    }

    // Command buffer with the transfer op.
    beginCommandBuffer(ctx.vkd, cmdBufferTransfer);

    if (!needsClear)
        recordInitialLayoutBarrier(cmdBufferTransfer, *image);

    if (queueSwitch && needsAux)
    {
        // Acquire auxiliary image if needed.
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBufferTransfer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &preCopyAuxBarrier);
    }

    if (needsClear || needsFill)
    {
        if (queueSwitch)
        {
            // Acquisition barriers.
            if (needsClear)
                clearToTransferBarrier(cmdBufferTransfer, 0u, 0u, layoutAfterClear);
            if (needsFill)
                fillToTransferBarrier(cmdBufferTransfer, 0u, 0u, afterFillLayout);
        }
    }

    if (m_params.indirect)
    {
#ifndef CTS_USES_VULKANSC
        const auto indirectBufferStride  = static_cast<VkDeviceSize>(sizeof(VkCopyMemoryToImageIndirectCommandKHR));
        const auto indirectBufferAddress = getBufferDeviceAddress(ctx.vkd, ctx.device, *indirectBuffer);

        const VkCopyMemoryToImageIndirectInfoKHR copyMemoryToImageIndirectInfo = {
            VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INDIRECT_INFO_KHR,
            nullptr,
            0u,
            de::sizeU32(indirectCmds),
            makeStridedDeviceAddressRangeKHR(indirectBufferAddress, indirectBufferSize, indirectBufferStride),
            *image,
            m_params.transferLayout,
            de::dataOrNull(copySRLs),
        };
        ctx.vkd.cmdCopyMemoryToImageIndirectKHR(cmdBufferTransfer, &copyMemoryToImageIndirectInfo);
#endif // CTS_USES_VULKANSC
    }
    else if (m_params.imageToImage)
    {
        // Translate indirect commands to image-to-image operations.
        std::vector<VkImageCopy> copyRegions;
        copyRegions.reserve(m_params.copyRegions.size());

        for (const auto &indirectCmd : indirectCmds)
        {
            copyRegions.push_back(VkImageCopy{
                indirectCmd.imageSubresource,
                indirectCmd.imageOffset,
                indirectCmd.imageSubresource,
                indirectCmd.imageOffset,
                indirectCmd.imageExtent,
            });
        }

        ctx.vkd.cmdCopyImage(cmdBufferTransfer, auxImage->get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *image,
                             m_params.transferLayout, de::sizeU32(copyRegions), de::dataOrNull(copyRegions));
    }
    else
    {
        // Translate the indirect commands to classic APIs.
        std::vector<VkBufferImageCopy> copyRegions;
        copyRegions.reserve(indirectCmds.size());

        for (const auto &indirectCmd : indirectCmds)
        {
            copyRegions.push_back(VkBufferImageCopy{
                indirectCmd.srcAddress - baseMemoryAddress,
                indirectCmd.bufferRowLength,
                indirectCmd.bufferImageHeight,
                indirectCmd.imageSubresource,
                indirectCmd.imageOffset,
                indirectCmd.imageExtent,
            });
        }
        ctx.vkd.cmdCopyBufferToImage(cmdBufferTransfer, memoryBuffer->get(), *image, m_params.transferLayout,
                                     de::sizeU32(copyRegions), de::dataOrNull(copyRegions));
    }

    {
        const auto &barrierPtr = (queueSwitch ? &transferQueueBarrier : &switchQueueBarrier);
        const auto srcStage    = VK_PIPELINE_STAGE_TRANSFER_BIT;
        const auto dstStage    = (queueSwitch ? 0u : static_cast<VkPipelineStageFlags>(useStage));
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBufferTransfer, srcStage, dstStage, barrierPtr);
    }

    endCommandBuffer(ctx.vkd, cmdBufferTransfer);

    Move<VkSampler> sampler;
    DescriptorSetLayoutBuilder setLayoutBuilder;
    const auto descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    const auto descriptorStages = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_FRAGMENT_BIT);
    Move<VkDescriptorSetLayout> setLayout;

    DescriptorPoolBuilder poolBuilder;
    Move<VkDescriptorPool> descriptorPool;
    Move<VkDescriptorSet> descriptorSet;
    DescriptorSetUpdateBuilder setUpdateBuilder;

    if (!isDS)
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
        sampler = createSampler(ctx.vkd, ctx.device, &samplerCreateInfo);

        setLayoutBuilder.addSingleBinding(descriptorType, descriptorStages);
        setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);

        poolBuilder.addType(descriptorType);
        descriptorPool = poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
        descriptorSet  = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

        const auto descInfo = makeDescriptorImageInfo(*sampler, *view, useLayout);
        setUpdateBuilder.writeSingle(*descriptorSet, binding(0u), descriptorType, &descInfo);
        setUpdateBuilder.update(ctx.vkd, ctx.device);
    }

    const auto pipelineLayout       = makePipelineLayout(ctx.vkd, ctx.device, *setLayout);
    const auto rpDepthStencilFormat = (isDS ? imgCreateInfo.format : VK_FORMAT_UNDEFINED);
    const auto renderPass = makeCustomRenderPass(ctx.vkd, ctx.device, attCreateInfo.format, rpDepthStencilFormat,
                                                 m_params.sampleCount, isMS, VK_ATTACHMENT_LOAD_OP_LOAD);

    std::vector<VkImageView> fbViews;
    if (msAttImage)
        fbViews.push_back(*msAttView);
    fbViews.push_back(attImage.getImageView());
    if (isDS)
        fbViews.push_back(*view);

    const auto framebuffer =
        makeFramebuffer(ctx.vkd, ctx.device, *renderPass, de::sizeU32(fbViews), de::dataOrNull(fbViews),
                        attCreateInfo.extent.width, attCreateInfo.extent.height, attCreateInfo.arrayLayers);

    const auto vertShader =
        createShaderModule(ctx.vkd, ctx.device, binaries.get(vk12Support ? "vert-spv15" : "vert-spv10"));
    const auto fragShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

    const auto depthTestEnable = makeVkBool(isDS);
    const auto stencilOp       = makeStencilOpState(VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
                                                    VK_COMPARE_OP_ALWAYS, 0xFFu, 0xFFu, 0u);
    const VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilState = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        nullptr,
        0u,
        depthTestEnable,
        VK_FALSE,
        VK_COMPARE_OP_LESS,
        VK_FALSE,
        VK_FALSE,
        stencilOp,
        stencilOp,
        0.0f,
        0.0f,
    };

    const VkPipelineMultisampleStateCreateInfo multisampleState = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        0u,
        m_params.sampleCount,
        VK_FALSE,
        0.0f,
        nullptr,
        VK_FALSE,
        VK_FALSE,
    };

    const auto pipeline = makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *vertShader, VK_NULL_HANDLE,
                                               VK_NULL_HANDLE, VK_NULL_HANDLE, *fragShader, *renderPass, viewports,
                                               scissors, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, 0u, 0u, nullptr, nullptr,
                                               &multisampleState, &pipelineDepthStencilState);

    beginCommandBuffer(ctx.vkd, cmdBufferGraphics);

    if (queueSwitch)
    {
        // Transfer image to this queue.
        const auto &barrierPtr = &graphicsQueueBarrier;
        const auto srcStage    = 0u;
        const auto dstStage    = useStage;
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBufferGraphics, srcStage, dstStage, barrierPtr);
    }

    // As attachments will be loaded, we need to move the color att. to the proper layout and clear it before the RP.
    {
        std::vector<VkImage> attImages;
        attImages.push_back(attImage.getImage());
        if (msAttImage)
            attImages.push_back(msAttImage->get());

        std::vector<VkImageMemoryBarrier> preClearBarriers;
        for (const auto img : attImages)
        {
            preClearBarriers.push_back(makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT,
                                                              VK_IMAGE_LAYOUT_UNDEFINED,
                                                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, img, attSRR));
        }
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBufferGraphics, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, preClearBarriers.data(), preClearBarriers.size());

        const auto clearColorVk = makeClearValueColor(clearColor);
        for (const auto img : attImages)
        {
            ctx.vkd.cmdClearColorImage(cmdBufferGraphics, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       &clearColorVk.color, 1u, &attSRR);
        }

        const auto colorAccess = (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
        std::vector<VkImageMemoryBarrier> postClearBarriers;
        for (const auto img : attImages)
        {
            postClearBarriers.push_back(makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, colorAccess,
                                                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, img, attSRR));
        }
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBufferGraphics, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, postClearBarriers.data(),
                                      postClearBarriers.size());
    }

    const auto vertexBufferOffset = static_cast<VkDeviceSize>(0);

    beginRenderPass(ctx.vkd, cmdBufferGraphics, *renderPass, *framebuffer, scissors.front());
    ctx.vkd.cmdBindPipeline(cmdBufferGraphics, bindPoint, *pipeline);
    ctx.vkd.cmdBindVertexBuffers(cmdBufferGraphics, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
    if (!isDS)
        ctx.vkd.cmdBindDescriptorSets(cmdBufferGraphics, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u,
                                      nullptr);
    for (uint32_t i = 0u; i < attCreateInfo.arrayLayers; ++i)
    {
        const auto firstVertex = i * pixelCountPerLayer;
        ctx.vkd.cmdDraw(cmdBufferGraphics, pixelCountPerLayer, 1u, firstVertex, i);
    }
    endRenderPass(ctx.vkd, cmdBufferGraphics);

    // Copy color image to its buffer for verification.
    copyImageToBuffer(ctx.vkd, cmdBufferGraphics, attImage.getImage(), attImage.getBuffer(),
                      m_params.extent.swizzle(0, 1), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, attCreateInfo.arrayLayers);

    endCommandBuffer(ctx.vkd, cmdBufferGraphics);

    // Synchronization from clear to transfer and from transfer to use.
    const auto clearSem    = createSemaphore(ctx.vkd, ctx.device);
    const auto auxSem      = createSemaphore(ctx.vkd, ctx.device);
    const auto transferSem = createSemaphore(ctx.vkd, ctx.device);

    // If not using VK_DEPENDENCY_QUEUE_FAMILY_OWNERSHIP_TRANSFER_USE_ALL_STAGES_BIT_KHR, we must wait on all stages
    // because the acquire and release operations do not happen in a defined stage.
    const auto waitStage = static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    const VkSubmitInfo clearSubmitInfo = {
        // clang-format off
        VK_STRUCTURE_TYPE_SUBMIT_INFO,
        nullptr,
        0u,
        nullptr,
        nullptr,
        1u,
        &cmdBufferClear,
        1u,
        &clearSem.get(),
        // clang-format on
    };

    std::vector<VkSemaphore> waitSemaphores;
    std::vector<VkPipelineStageFlags> waitStages;

    if (needsClear || needsFill || needsAux)
    {
        waitSemaphores.push_back(*clearSem);
        waitStages.push_back(waitStage);
    }

    DE_ASSERT(waitSemaphores.size() == waitStages.size());

    const VkSubmitInfo transferSubmitInfo = {
        // clang-format off
        VK_STRUCTURE_TYPE_SUBMIT_INFO,
        nullptr,
        de::sizeU32(waitSemaphores),
        de::dataOrNull(waitSemaphores),
        de::dataOrNull(waitStages),
        1u,
        &cmdBufferTransfer,
        1u,
        &transferSem.get(),
        // clang-format on
    };

    const VkSubmitInfo graphicsSubmitInfo = {
        // clang-format off
        VK_STRUCTURE_TYPE_SUBMIT_INFO,
        nullptr,
        1u,
        &transferSem.get(),
        &waitStage,
        1u,
        &cmdBufferGraphics,
        0u,
        nullptr,
        // clang-format on
    };

    const auto fence = createFence(ctx.vkd, ctx.device);
    if (needsClear || needsFill || needsAux)
        ctx.vkd.queueSubmit(ctx.queue, 1u, &clearSubmitInfo, VK_NULL_HANDLE);
    ctx.vkd.queueSubmit(queue, 1u, &transferSubmitInfo, VK_NULL_HANDLE);
    ctx.vkd.queueSubmit(ctx.queue, 1u, &graphicsSubmitInfo, *fence);
    waitForFence(ctx.vkd, ctx.device, *fence);

    // Verify color results.
    auto &colorBufferAlloc = attImage.getBufferAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, colorBufferAlloc);

    const auto colorTcuFormat = mapVkFormat(attCreateInfo.format);
    tcu::TextureLevel refLevel(colorTcuFormat, m_params.extent.x(), m_params.extent.y(), m_params.extent.z());
    tcu::PixelBufferAccess fullReference = refLevel.getAccess();

    for (int z = 0u; z < m_params.extent.z(); ++z)
    {
        const VkRect2D *region = (m_params.copyRegions.empty() ? nullptr : &m_params.copyRegions.at(z));

        for (int y = 0u; y < m_params.extent.y(); ++y)
            for (int x = 0u; x < m_params.extent.x(); ++x)
            {
                if (region &&
                    (x < region->offset.x ||
                     static_cast<uint32_t>(x) >= static_cast<uint32_t>(region->offset.x) + region->extent.width ||
                     y < region->offset.y ||
                     static_cast<uint32_t>(y) >= static_cast<uint32_t>(region->offset.y) + region->extent.height))
                {
                    // Outside the copied region, depth did not pass or the texture had the clear value.
                    fullReference.setPixel(clearColor, x, y, z);
                }
                else
                {
                    if (isDS)
                    {
                        const auto pixelId    = getPixelID(static_cast<uint32_t>(x), static_cast<uint32_t>(y),
                                                           static_cast<uint32_t>(z), extentU);
                        const auto sampleId   = pixelId * perPixelSamples;
                        const auto &bufferVal = depthBufferValues.at(sampleId);
                        const auto &geomVal   = pointDepthValues.at(pixelId);
                        const bool depthPass  = (geomVal < bufferVal);
                        const auto &color     = (depthPass ? geomColor : clearColor);
                        fullReference.setPixel(color, x, y, z);
                    }
                    else
                    {
                        const auto realX = x * perPixelSamplesI;
                        const auto color = textureAccess->getPixel(realX, y, z);

                        // We convert values from sRGB to linear in sRGB cases but skip that conversion in MSAA cases.
                        // The reasoning follows:
                        //
                        // * In the non-MSAA cases, values are copied from a buffer to the image using
                        //   vkCmdCopyBufferToImage. When this happens, the values are copied directly into the image
                        //   (memcpy semantics), and are presumed to already be in sRGB for sRGB formats. That image is
                        //   then sampled as a texture in a render pass, and stored in a non-SRGB attachment. Shaders
                        //   are supposed to always work in linear space (sRGB conversions happen when reading from or
                        //   storing data in images), so the sampling operation converts values to linear before storing
                        //   them in the non-sRGB attachment. The result is a conversion from sRGB to linear in the data
                        //   originally stored in the buffer, that we replicate below.
                        //
                        // * In the MSAA case, values are copied from a buffer to an MSAA image using a pipeline and a
                        //   fragment shader. The buffer is a descriptor in the shader, and the image is a color
                        //   attachment in the render pass. This means values in the buffer are considered to be in
                        //   linear space, and converted to sRGB when they are stored in the framebuffer. The image is
                        //   then sampled as a texture, converting the values back to linear, and stored as linear in
                        //   the non-sRGB attachment. The end result is that 2 conversions have taken place, but the
                        //   result values should match the original ones (both linear).
                        fullReference.setPixel(((isSrgb && !isMS) ? tcu::sRGBToLinear(color) : color), x, y, z);
                    }
                }
            }
    }

    tcu::ConstPixelBufferAccess fullResult(colorTcuFormat, m_params.extent, colorBufferAlloc.getHostPtr());

    auto &log = m_context.getTestContext().getLog();
    bool fail = false;

    // Depth/stencil threshold is zero because the gist is in the depth test, and color is exactly blue or black.
    const tcu::Vec4 threshold = (isDS ? tcu::Vec4(0.0f) : getColorFormatThreshold(imgCreateInfo.format));

    for (int z = 0; z < m_params.extent.z(); ++z)
    {
        const auto reference = tcu::getSubregion(fullReference, 0, 0, z, m_params.extent.x(), m_params.extent.y(), 1);
        const auto result    = tcu::getSubregion(fullResult, 0, 0, z, m_params.extent.x(), m_params.extent.y(), 1);

        const auto layerName = "Layer" + std::to_string(z);
        if (!tcu::floatThresholdCompare(log, layerName.c_str(), "", reference, result, threshold,
                                        tcu::COMPARE_LOG_ON_ERROR))
            fail = true;
    }

    if (fail)
        TCU_FAIL("Unexpected output in color buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

} // anonymous namespace

tcu::TestCaseGroup *createUseAfterXferGroup(tcu::TestContext &testCtx, bool indirect)
{
    // Create tests for using the transferred-to-image normally after the transfer.
    de::MovePtr<tcu::TestCaseGroup> useAfterXferGroup(new tcu::TestCaseGroup(testCtx, "use_after_copy"));
    {
        const std::vector<VkFormat> testFormats{
            // clang-format off
            VK_FORMAT_D16_UNORM,
            VK_FORMAT_D16_UNORM_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_X8_D24_UNORM_PACK32,
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_R8G8B8_UNORM,
            VK_FORMAT_R8_UNORM,
            VK_FORMAT_R32G32B32_SFLOAT,
            VK_FORMAT_R4G4B4A4_UNORM_PACK16,
            VK_FORMAT_B4G4R4A4_UNORM_PACK16,
            VK_FORMAT_R5G6B5_UNORM_PACK16,
            VK_FORMAT_B5G6R5_UNORM_PACK16,
            VK_FORMAT_R5G5B5A1_UNORM_PACK16,
            VK_FORMAT_B5G5R5A1_UNORM_PACK16,
            VK_FORMAT_A1R5G5B5_UNORM_PACK16,
            VK_FORMAT_R8_SRGB,
            VK_FORMAT_R8G8_SRGB,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_FORMAT_B8G8R8A8_SRGB,
            VK_FORMAT_A8B8G8R8_SRGB_PACK32,
            VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
            // clang-format on
        };

        // Some implementations support some formats only for linear tiling, which makes them interesting cases.
        const std::vector<VkFormat> linearTilingFormats{
            // clang-format off
            VK_FORMAT_R32G32B32_SFLOAT,
            // clang-format on
        };

        for (const auto testFormat : testFormats)
        {
            const auto formatGroupName = getFormatSimpleName(testFormat);
            de::MovePtr<tcu::TestCaseGroup> formatGroup(new tcu::TestCaseGroup(testCtx, formatGroupName.c_str()));

            const bool isDS = isDepthStencilFormat(testFormat);

            for (const auto xferLayout : {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL})
            {
                const auto layoutGroupName =
                    (xferLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ? "transfer_dst_optimal" : "general");
                de::MovePtr<tcu::TestCaseGroup> layoutGroup(new tcu::TestCaseGroup(testCtx, layoutGroupName));

                for (const int layerCount : {1, 2})
                {
                    for (const auto queueSelection :
                         {QueueSelectionOptions::Universal, QueueSelectionOptions::ComputeOnly,
                          QueueSelectionOptions::TransferOnly})
                    {
                        // We choose a larger baseSize for the transfer queue because that helps us get some regions,
                        // declarede below, meet the transfer queue granularity requirements on some implementations.
                        // Those interesting tests can run, then, on more systems.
                        const auto baseSize    = ((queueSelection == QueueSelectionOptions::TransferOnly) ? 64 : 32);
                        const auto imageExtent = tcu::IVec3(baseSize, baseSize, layerCount);

                        // VUID-VkCopyMemoryToImageIndirectInfoKHR-commandBuffer-07674
                        // Depth and stencil memory copies can only happen in queues with graphics capabilities.
                        if (indirect && isDS && queueSelection != QueueSelectionOptions::Universal)
                            continue;

                        for (const bool fullCopy : {true, false})
                        {
                            std::vector<VkRect2D> copyRegions;
                            if (!fullCopy)
                            {
                                DE_ASSERT(layerCount >= 1 && layerCount <= 2);

                                copyRegions.reserve(static_cast<size_t>(layerCount));

                                const auto quarterExtent  = imageExtent.swizzle(0, 1) / tcu::IVec2(2, 2);
                                const auto quarterExtentU = quarterExtent.asUint();

                                for (int layerIdx = 0; layerIdx < layerCount; ++layerIdx)
                                {
                                    tcu::IVec2 offset(0, 0);
                                    tcu::UVec2 extent = quarterExtentU;

                                    // No matter which layer we're at, make the offsets different and at least one of
                                    // them nonzero, and make the extents different in width and height.
                                    offset[layerIdx] = quarterExtent[layerIdx];
                                    extent[layerIdx] /= 2;

                                    copyRegions.push_back(makeRect2D(offset.x(), offset.y(), extent.x(), extent.y()));
                                }
                            }

                            for (const bool use3DImage : {false, true})
                            {
#ifdef CTS_USES_VULKANSC
                                // VK_EXT_image_2d_view_of_3d not available on VulkanSC.
                                if (use3DImage && imageExtent.z() == 1)
                                    continue;
#endif // CTS_USES_VULKANSC

                                // VUID-VkFramebufferCreateInfo-pAttachments-00891: we cannot use 2D-array or 2D views
                                // of a 3D image for the depth/stencil attachments.
                                if (use3DImage && isDS)
                                    continue;

                                for (const bool viewIs3D : {false, true})
                                {
                                    if (viewIs3D && !use3DImage)
                                        continue; // Would make no sense otherwise.

                                    if (viewIs3D && isDS)
                                        continue; // DS images will be used as DS attachments, which cannot be 3D.

                                    // If
                                    //     !isDS --> color image, used as a texture.
                                    //     use3DImage --> it's a 3D image.
                                    //     extent.z() > 1 && !texIs3D --> the image view will be a 2D_ARRAY
                                    // Then
                                    //     We would hit VUID-VkDescriptorImageInfo-imageView-06712.
                                    if (!isDS && use3DImage && imageExtent.z() > 1 && !viewIs3D)
                                        continue;

                                    for (const bool colorAttFlag : {false, true})
                                    {
                                        if (colorAttFlag)
                                        {
                                            // xferLayout == VK_IMAGE_LAYOUT_GENERAL trims combinations.
                                            if (isDS || xferLayout == VK_IMAGE_LAYOUT_GENERAL)
                                                continue;

                                            if (viewIs3D)
                                                continue; // A color attachment view cannot be 3D.
                                        }

                                        for (const bool imageToImage : {false, true})
                                            for (const bool linear : {false, true})
                                            {
                                                // Limit linear tiling formats to a few of them.
                                                if (linear && !de::contains(linearTilingFormats, testFormat))
                                                    continue;

                                                // Indirect is not available, use3DImage is not supportd in code, and
                                                // layerCount == 1 is for trimming combinations.
                                                if (imageToImage && (indirect || use3DImage || layerCount == 1u))
                                                    continue;

                                                for (const bool multiSample : {false, true})
                                                {
                                                    // Cannot copy from buffer to MS image.
                                                    if (multiSample && !imageToImage)
                                                        continue;

                                                    // clang-format off
                                                    AfterUsageParams params{
                                                        testFormat,
                                                        imageExtent,
                                                        queueSelection,
                                                        xferLayout,
                                                        copyRegions,
                                                        indirect,
                                                        use3DImage,
                                                        viewIs3D,
                                                        colorAttFlag,
                                                        imageToImage,
                                                        linear,
                                                        (multiSample ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT),
                                                    };
                                                    // clang-format on

                                                    if (colorAttFlag)
                                                    {
                                                        // Use larger images in these cases. This makes some drivers
                                                        // enable color compression for these images, which may result
                                                        // in problems after copying.
                                                        const int32_t targetSize   = 1024;
                                                        const int32_t sizeFactor   = targetSize / baseSize;
                                                        const uint32_t sizeFactorU = static_cast<uint32_t>(sizeFactor);

                                                        params.extent =
                                                            params.extent * tcu::IVec3(sizeFactor, sizeFactor, 1);
                                                        for (auto &region : params.copyRegions)
                                                        {
                                                            region.offset.x *= sizeFactor;
                                                            region.offset.y *= sizeFactor;
                                                            region.extent.width *= sizeFactorU;
                                                            region.extent.height *= sizeFactorU;
                                                        }
                                                    }

                                                    auto testName = std::to_string(params.extent.x()) + "x" +
                                                                    std::to_string(params.extent.y()) + "x" +
                                                                    std::to_string(params.extent.z());

                                                    if (queueSelection == QueueSelectionOptions::ComputeOnly)
                                                        testName += "_cq";
                                                    else if (queueSelection == QueueSelectionOptions::TransferOnly)
                                                        testName += "_tq";

                                                    if (!fullCopy)
                                                        testName += "_regions";

                                                    if (use3DImage)
                                                        testName += "_3d_img";

                                                    if (viewIs3D)
                                                        testName += "_3d_view";

                                                    if (colorAttFlag)
                                                        testName += "_color_att_flag";

                                                    if (imageToImage)
                                                        testName += "_img2img";

                                                    if (linear)
                                                        testName += "_linear";

                                                    if (multiSample)
                                                        testName += "_msaa";

                                                    layoutGroup->addChild(
                                                        new AfterUsageCase(testCtx, testName, params));
                                                }
                                            }
                                    }
                                }
                            }
                        }
                    }
                }

                formatGroup->addChild(layoutGroup.release());
            }

            useAfterXferGroup->addChild(formatGroup.release());
        }
    }

    return useAfterXferGroup.release();
}

} // namespace api
} // namespace vkt
