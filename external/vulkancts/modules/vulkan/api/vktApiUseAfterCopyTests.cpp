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
            VK_SAMPLE_COUNT_1_BIT,
            tiling,
            usage,
            VK_SHARING_MODE_EXCLUSIVE,
            0u,
            nullptr,
            VK_IMAGE_LAYOUT_UNDEFINED,
        };

        return createInfo;
    }

    VkImageCreateInfo getColorAttCreateInfo() const
    {
        const tcu::IVec3 creationExtent(extent.x(), extent.y(), 1);
        const auto creationLayers = static_cast<uint32_t>(extent.z());
        const auto usage =
            (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

        const VkImageCreateInfo createInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            nullptr,
            0u,
            VK_IMAGE_TYPE_2D,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            makeExtent3D(creationExtent),
            1u,
            creationLayers,
            VK_SAMPLE_COUNT_1_BIT,
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
        auto createInfo  = getImageCreateInfo();
        createInfo.usage = (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT); // Only transfers.
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
            const auto srl         = makeImageSubresourceLayers(VK_IMAGE_ASPECT_NONE, 0u, layerIdx, 1u);
            const auto offset      = makeOffset3D(copyRegion.offset.x, copyRegion.offset.y, 0);
            const auto extent      = makeExtent3D(copyRegion.extent.width, copyRegion.extent.height, 1u);

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

    const auto geomColor   = m_params.getGeomColor();
    const bool isColorCase = !m_params.isDepthStencilCase();
    const bool &texIs3D    = m_params.viewIs3D;

    std::ostringstream frag;
    frag << "#version 460\n"
         << (isColorCase ?
                 "layout (set=0, binding=0) uniform " +
                     std::string(texIs3D ? "sampler3D" : (multiSlice ? "sampler2DArray" : "sampler2D")) + " tex;\n" :
                 "")
         << "layout (location=0) in flat int layerIndex;\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main(void) {\n";

    if (isColorCase)
    {
        const auto coords = ((multiSlice || texIs3D) ? "ivec3(gl_FragCoord.xy, layerIndex)" : "ivec2(gl_FragCoord.xy)");
        frag << "    outColor = texelFetch(tex, " << coords << ", 0);\n";
    }
    else
        frag << "    outColor = vec4" << geomColor << ";\n";

    frag << "}\n";
    dst.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

VkDeviceAddress getBufferDeviceAddress(const DeviceInterface &vkd, VkDevice device, VkBuffer buffer)
{
    if (buffer == VK_NULL_HANDLE)
        return 0ull;

    const VkBufferDeviceAddressInfo deviceAddressInfo{
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, // VkStructureType    sType
        nullptr,                                      // const void*        pNext
        buffer                                        // VkBuffer           buffer;
    };
    return vkd.getBufferDeviceAddress(device, &deviceAddressInfo);
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

tcu::TestStatus AfterUsageInstance::iterate(void)
{
    const auto ctx = m_context.getContextCommonData();

    const auto isDS       = m_params.isDepthStencilCase();
    const auto isSrgb     = (!isDS && isSrgbFormat(m_params.format));
    const auto geomColor  = m_params.getGeomColor();
    const auto clearColor = m_params.getClearColor();

    const auto imgCreateInfo = m_params.getImageCreateInfo();
    ImageWithMemory image(ctx.vkd, ctx.device, ctx.allocator, imgCreateInfo, MemoryRequirement::Any);
    const auto tcuFormat = mapVkFormat(imgCreateInfo.format);
    const auto imgSRR    = makeImageSubresourceRange(getImageAspectFlags(tcuFormat), 0u, imgCreateInfo.mipLevels, 0u,
                                                     imgCreateInfo.arrayLayers);
    const auto imageViewType =
        (m_params.viewIs3D ? VK_IMAGE_VIEW_TYPE_3D :
                             (m_params.multiSlice() ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D));
    const auto view = makeImageView(ctx.vkd, ctx.device, *image, imageViewType, imgCreateInfo.format, imgSRR);

    const auto attCreateInfo = m_params.getColorAttCreateInfo();
    const auto attSRR        = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, attCreateInfo.mipLevels, 0u,
                                                         attCreateInfo.arrayLayers);
    ImageWithBuffer attImage(ctx.vkd, ctx.device, ctx.allocator, attCreateInfo.extent, attCreateInfo.format,
                             attCreateInfo.usage, attCreateInfo.imageType, attSRR, attCreateInfo.arrayLayers,
                             attCreateInfo.samples, attCreateInfo.tiling, attCreateInfo.mipLevels);

    // The image-to-image case is essentially similar to buffer-to-image. However, instead of copying data from the
    // buffer to the image, in whole or in part, the full buffer is copied first to an auxiliary image, declared below.
    // Then, the regions to copy to the final target image are copied using image-to-image operations, with the same
    // region as the source and destination in both images.
    const auto auxImgCreateInfo = m_params.getAuxiliarImageCreateInfo();
    std::unique_ptr<ImageWithMemory> auxImage;
    if (m_params.imageToImage)
        auxImage.reset(
            new ImageWithMemory(ctx.vkd, ctx.device, ctx.allocator, auxImgCreateInfo, MemoryRequirement::Any));

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
    const auto pixelCountPerLayer = extentU.x() * extentU.y();
    const auto pixelCount         = pixelCountPerLayer * extentU.z();

    std::vector<uint16_t> depthBufferValues;
    std::vector<uint16_t> pointDepthValues;

    std::unique_ptr<tcu::TextureLevel> textureLevel;
    std::unique_ptr<tcu::PixelBufferAccess> textureAccess;

    if (isDS)
    {
        depthBufferValues.reserve(pixelCount);
        pointDepthValues.reserve(pixelCount);

        for (uint32_t i = 0u; i < pixelCount; ++i)
        {
            depthBufferValues.push_back(rng.getUint16());
            pointDepthValues.push_back(rng.getUint16());
        }
    }
    else
    {
        textureLevel.reset(
            new tcu::TextureLevel(tcuFormat, m_params.extent.x(), m_params.extent.y(), m_params.extent.z()));
        textureAccess.reset(new tcu::PixelBufferAccess(textureLevel->getAccess()));

        for (int z = 0; z < m_params.extent.z(); ++z)
            for (int y = 0; y < m_params.extent.y(); ++y)
                for (int x = 0; x < m_params.extent.x(); ++x)
                {
                    const auto red   = rng.getFloat();
                    const auto green = rng.getFloat();
                    const auto blue  = rng.getFloat();
                    const tcu::Vec4 color(red, blue, green, 1.0f);
                    textureAccess->setPixel(color, x, y, z);
                }
    }

    // Vertex buffer contents: one point per pixel and layer.
    std::vector<tcu::Vec4> vertices;
    vertices.reserve(pixelCount);

    const auto extentF   = m_params.extent.asFloat();
    const auto normalize = [](uint32_t n, float dim) { return (static_cast<float>(n) + 0.5f) / dim * 2.0f - 1.0f; };

    const auto getPixelID = [](uint32_t x, uint32_t y, uint32_t z, const tcu::UVec3 &extent)
    { return (z * extent.x() * extent.y() + y * extent.x() + x); };

    const float maxDepth = static_cast<float>(std::numeric_limits<uint16_t>::max());

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
        const auto depthMemorySize = itemSize * pixelCount;
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
    const auto memoryBufferUsage =
        static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    if (isDS)
    {
        const auto depthMemoryBufferSize = static_cast<VkDeviceSize>(de::dataSize(depthMemoryBytes));
        const auto depthMemoryBufferInfo = makeBufferCreateInfo(depthMemoryBufferSize, memoryBufferUsage);

        memoryBuffer.reset(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, depthMemoryBufferInfo,
                                                HostIntent::W, true, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT));
        {
            auto &alloc = memoryBuffer->getAllocation();
            memcpy(alloc.getHostPtr(), de::dataOrNull(depthMemoryBytes), de::dataSize(depthMemoryBytes));
            flushAlloc(ctx.vkd, ctx.device, alloc);
        }
    }
    else
    {
        const auto texMemoryBufferSize = static_cast<VkDeviceSize>(itemSize * pixelCount);
        const auto texMemoryBufferInfo = makeBufferCreateInfo(texMemoryBufferSize, memoryBufferUsage);

        memoryBuffer.reset(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, texMemoryBufferInfo, HostIntent::W,
                                                true, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT));
        {
            auto &alloc = memoryBuffer->getAllocation();
            memcpy(alloc.getHostPtr(), textureAccess->getDataPtr(), static_cast<size_t>(texMemoryBufferSize));
            flushAlloc(ctx.vkd, ctx.device, alloc);
        }
    }
    const auto baseMemoryAddress = getBufferDeviceAddress(ctx.vkd, ctx.device, memoryBuffer->get());

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
    const auto indirectBufferUsage = (VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    const auto indirectBufferInfo  = makeBufferCreateInfo(indirectBufferSize, indirectBufferUsage);
    BufferWithMemory indirectBuffer(ctx.vkd, ctx.device, ctx.allocator, indirectBufferInfo, HostIntent::W, true,
                                    VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);
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
    const auto cmdBufferAuxPtr =
        allocateCommandBuffer(ctx.vkd, ctx.device, *cmdGraphics.cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBufferAux = *cmdBufferAuxPtr;

    // Partial copies imply a clear to get predictable results, and tests using the color attachment usage flag are more
    // interesting if we force a clear using a render pass.
    const bool needsClear  = (!m_params.copyRegions.empty() || m_params.colorAttFlag);
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

    // When we need to clear color images and we have enabled color attachment usage, we'll make the test a bit more
    // interesting by clearing the image using a "fake" render pass with a clear op.
    Move<VkRenderPass> clearRenderPass;
    Move<VkFramebuffer> clearFramebuffer;
    const bool clearWithRenderPass = (!isDS && m_params.colorAttFlag);
    const auto layoutAfterClear =
        (clearWithRenderPass ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : m_params.transferLayout);

    if (needsClear)
    {
        // Command buffer with the clear op.
        beginCommandBuffer(ctx.vkd, cmdBufferClear);

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
                clearRenderPass = makeRenderPass(ctx.vkd, ctx.device, m_params.format);
                clearFramebuffer =
                    makeFramebuffer(ctx.vkd, ctx.device, *clearRenderPass, *view, imgCreateInfo.extent.width,
                                    imgCreateInfo.extent.height, imgCreateInfo.arrayLayers);
                const auto renderArea = makeRect2D(imgCreateInfo.extent.width, imgCreateInfo.extent.height);
                beginRenderPass(ctx.vkd, cmdBufferClear, *clearRenderPass, *clearFramebuffer, renderArea, clearColor);
                endRenderPass(ctx.vkd, cmdBufferClear);
            }
            else
            {
                const auto clearColorValue = makeClearValueColor(clearColor);
                ctx.vkd.cmdClearColorImage(cmdBufferClear, *image, m_params.transferLayout, &clearColorValue.color, 1u,
                                           &imgSRR);
            }
        }

        const auto srcAccess =
            (clearWithRenderPass ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_TRANSFER_WRITE_BIT);
        const auto srcStage =
            (clearWithRenderPass ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT);
        clearToTransferBarrier(cmdBufferClear, srcAccess, srcStage,
                               layoutAfterClear); // Sync clear and transfer, and maybe release resource.

        endCommandBuffer(ctx.vkd, cmdBufferClear);
    }

    if (m_params.imageToImage)
    {
        beginCommandBuffer(ctx.vkd, cmdBufferAux);

        const auto auxImgHandle = auxImage->get();

        // Prepare auxiliary image with the full buffer contents.
        recordInitialLayoutBarrier(cmdBufferAux, auxImgHandle);

        // Copy the full buffer to the auxiliary image.
        const auto fullSRL    = makeImageSubresourceLayers(copyAspect, 0u, 0u, imgCreateInfo.arrayLayers);
        const auto fullRegion = makeBufferImageCopy(auxImgCreateInfo.extent, fullSRL);
        ctx.vkd.cmdCopyBufferToImage(cmdBufferAux, memoryBuffer->get(), auxImgHandle, m_params.transferLayout, 1u,
                                     &fullRegion);

        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBufferAux, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &preCopyAuxBarrier);

        endCommandBuffer(ctx.vkd, cmdBufferAux);
    }

    // Command buffer with the transfer op.
    beginCommandBuffer(ctx.vkd, cmdBufferTransfer);

    if (needsClear)
    {
        if (queueSwitch)
            clearToTransferBarrier(cmdBufferTransfer, 0u, 0u, layoutAfterClear); // Acquire.
    }
    else
        recordInitialLayoutBarrier(cmdBufferTransfer, *image);

    if (queueSwitch && m_params.imageToImage)
    {
        // Acquire auxiliary image if needed.
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBufferTransfer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &preCopyAuxBarrier);
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

        const auto binding  = DescriptorSetUpdateBuilder::Location::binding;
        const auto descInfo = makeDescriptorImageInfo(*sampler, *view, useLayout);
        setUpdateBuilder.writeSingle(*descriptorSet, binding(0u), descriptorType, &descInfo);
        setUpdateBuilder.update(ctx.vkd, ctx.device);
    }

    const auto pipelineLayout       = makePipelineLayout(ctx.vkd, ctx.device, *setLayout);
    const auto rpDepthStencilFormat = (isDS ? imgCreateInfo.format : VK_FORMAT_UNDEFINED);
    const auto renderPass =
        makeRenderPass(ctx.vkd, ctx.device, attCreateInfo.format, rpDepthStencilFormat, VK_ATTACHMENT_LOAD_OP_LOAD);
    std::vector<VkImageView> fbViews(1u, attImage.getImageView());
    if (isDS)
        fbViews.push_back(*view);
    const auto framebuffer =
        makeFramebuffer(ctx.vkd, ctx.device, *renderPass, de::sizeU32(fbViews), de::dataOrNull(fbViews),
                        attCreateInfo.extent.width, attCreateInfo.extent.height, attCreateInfo.arrayLayers);

    const auto &binaries   = m_context.getBinaryCollection();
    const auto vk12Support = m_context.contextSupports(vk::ApiVersion(0u, 1u, 2u, 0u));
    const auto vertShader =
        createShaderModule(ctx.vkd, ctx.device, binaries.get(vk12Support ? "vert-spv15" : "vert-spv10"));
    const auto fragShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

    const std::vector<VkViewport> viewports(1u, makeViewport(m_params.extent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(m_params.extent));

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

    const auto pipeline = makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *vertShader, VK_NULL_HANDLE,
                                               VK_NULL_HANDLE, VK_NULL_HANDLE, *fragShader, *renderPass, viewports,
                                               scissors, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, 0u, 0u, nullptr, nullptr,
                                               nullptr, &pipelineDepthStencilState);

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
        const auto preClearBarrier =
            makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, attImage.getImage(), attSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBufferGraphics, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &preClearBarrier);

        const auto clearColorVk = makeClearValueColor(clearColor);
        ctx.vkd.cmdClearColorImage(cmdBufferGraphics, attImage.getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   &clearColorVk.color, 1u, &attSRR);

        const auto colorAccess = (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
        const auto postClearBarrier =
            makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, colorAccess, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, attImage.getImage(), attSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBufferGraphics, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, &postClearBarrier);
    }

    const auto bindPoint          = VK_PIPELINE_BIND_POINT_GRAPHICS;
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

    const VkSubmitInfo auxSubmitInfo = {
        // clang-format off
        VK_STRUCTURE_TYPE_SUBMIT_INFO,
        nullptr,
        0u,
        nullptr,
        nullptr,
        1u,
        &cmdBufferAux,
        1u,
        &auxSem.get(),
        // clang-format on
    };

    std::vector<VkSemaphore> waitSemaphores;
    std::vector<VkPipelineStageFlags> waitStages;

    if (needsClear)
    {
        waitSemaphores.push_back(*clearSem);
        waitStages.push_back(waitStage);
    }

    if (m_params.imageToImage)
    {
        waitSemaphores.push_back(*auxSem);
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
    if (needsClear)
        ctx.vkd.queueSubmit(ctx.queue, 1u, &clearSubmitInfo, VK_NULL_HANDLE);
    if (m_params.imageToImage)
        ctx.vkd.queueSubmit(ctx.queue, 1u, &auxSubmitInfo, VK_NULL_HANDLE);
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
                        const auto &bufferVal = depthBufferValues.at(pixelId);
                        const auto &geomVal   = pointDepthValues.at(pixelId);
                        const bool depthPass  = (geomVal < bufferVal);
                        const auto &color     = (depthPass ? geomColor : clearColor);
                        fullReference.setPixel(color, x, y, z);
                    }
                    else
                    {
                        const auto color = textureAccess->getPixel(x, y, z);
                        fullReference.setPixel((isSrgb ? tcu::sRGBToLinear(color) : color), x, y, z);
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
                    const auto imageExtent = tcu::IVec3(32, 32, layerCount);

                    for (const auto queueSelection :
                         {QueueSelectionOptions::Universal, QueueSelectionOptions::ComputeOnly,
                          QueueSelectionOptions::TransferOnly})
                    {
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
                                                };
                                                // clang-format on

                                                if (colorAttFlag)
                                                {
                                                    // Use larger images in these cases. This makes some drivers enable color
                                                    // compression for these images, which may result in problems after copying.
                                                    const int32_t sizeFactor   = 32;
                                                    const uint32_t sizeFactorU = uint32_t{sizeFactor};

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

                                                layoutGroup->addChild(new AfterUsageCase(testCtx, testName, params));
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
