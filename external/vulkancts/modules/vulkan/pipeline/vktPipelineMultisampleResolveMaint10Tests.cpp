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
 * \file vktPipelineMultisampleResolveMaint10Tests.hpp
 * \brief Multisample resolve tests for VK_KHR_maintenance10.
 *//*--------------------------------------------------------------------*/

#include "vktPipelineMultisampleResolveMaint10Tests.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include "deRandom.hpp"

#include <sstream>
#include <cstring>
#include <memory>

namespace vkt
{
namespace pipeline
{

namespace
{

using namespace vk;

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

enum class ResolveMethod
{
    CMD = 0,
    RENDER_PASS,
    DYNAMIC_RENDER,
};

enum class ResolveArea
{
    FULL = 0,
    FULL_MULTILAYER,
    REGION,             // 1 subregion in a single layer, only applies to CMD.
    REGIONS_MULTILAYER, // 3 subregions in a couple of layers, only applies to CMD.
};

enum class SRGBFlags
{
    NONE = 0,
    SKIP,
    ENABLE,
};

struct TestParams
{
    PipelineConstructionType constructionType;
    ResolveMethod resolveMethod;
    VkFormat imageFormat;
    VkImageAspectFlags resolveAspects;
    VkResolveModeFlagBits resolveMode;
    ResolveArea resolveArea;
    SRGBFlags sRGBFlags;

    uint32_t getRandomSeed() const
    {
        return ((imageFormat << 24) | (static_cast<uint32_t>(resolveMethod) << 22) | (resolveAspects << 16) |
                (static_cast<uint32_t>(resolveMode) << 12) | (static_cast<uint32_t>(sRGBFlags) << 10));
    }

    // 16x16 with 1 or 2 layers. Note the Z member is the layer count, not the 3rd dimension.
    // See getImageExtent() and getImageLayers() below.
    tcu::IVec3 getExtent() const
    {
        tcu::IVec3 baseExtent(16, 16, 1);
        if (resolveArea == ResolveArea::FULL_MULTILAYER || resolveArea == ResolveArea::REGIONS_MULTILAYER)
            baseExtent.z() = 2;
        return baseExtent;
    }

    tcu::IVec3 getImageExtent() const
    {
        const auto baseExtent = getExtent();
        return tcu::IVec3(baseExtent.x(), baseExtent.y(), 1);
    }

    uint32_t getImageLayers() const
    {
        const auto baseExtent = getExtent().asUint();
        return baseExtent.z();
    }

    VkSampleCountFlagBits getSampleCount() const
    {
        return VK_SAMPLE_COUNT_4_BIT;
    }

    bool isDepthStencil() const
    {
        return ((resolveAspects & VK_IMAGE_ASPECT_DEPTH_BIT) || (resolveAspects & VK_IMAGE_ASPECT_STENCIL_BIT));
    }

    std::string getGLSLFragOutType() const
    {
        if (isDepthStencil())
            return "";
        if (isUintFormat(imageFormat))
            return "uvec4";
        if (isIntFormat(imageFormat))
            return "ivec4";
        return "vec4";
    }

    VkImageUsageFlags getImageUsage() const
    {
        VkImageUsageFlags usageFlags = 0u;

        if (isDepthStencil())
            usageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        else
            usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        if (resolveMethod == ResolveMethod::CMD)
            usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT; // Required by vkCmdResolveImage2.

        return usageFlags;
    }

    VkImageCreateInfo getImageCreateInfo() const
    {
        return VkImageCreateInfo{
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            0u,
            0u, // flags
            VK_IMAGE_TYPE_2D,
            imageFormat,
            makeExtent3D(getImageExtent()),
            1u,
            getImageLayers(),
            getSampleCount(),
            VK_IMAGE_TILING_OPTIMAL,
            getImageUsage(),
            VK_SHARING_MODE_EXCLUSIVE,
            0u,
            nullptr,
            VK_IMAGE_LAYOUT_UNDEFINED,
        };
    }
};

class Maint10ResolveInstance : public vkt::TestInstance
{
public:
    Maint10ResolveInstance(Context &context, const TestParams &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    ~Maint10ResolveInstance(void) override = default;

    tcu::TestStatus iterate(void) override;

protected:
    const TestParams m_params;
};

class Maint10ResolveCase : public vkt::TestCase
{
public:
    Maint10ResolveCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    ~Maint10ResolveCase(void) override = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new Maint10ResolveInstance(context, m_params);
    }

protected:
    const TestParams m_params;
};

void Maint10ResolveCase::checkSupport(Context &context) const
{
    const auto ctx = context.getContextCommonData();

    checkPipelineConstructionRequirements(ctx.vki, ctx.physicalDevice, m_params.constructionType);
    context.requireDeviceFunctionality("VK_KHR_maintenance10");

    if (m_params.resolveMethod == ResolveMethod::CMD)
        context.requireDeviceFunctionality("VK_KHR_copy_commands2");
    else if (m_params.resolveMethod == ResolveMethod::DYNAMIC_RENDER)
        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
    else if (m_params.resolveMethod == ResolveMethod::RENDER_PASS)
        context.requireDeviceFunctionality("VK_KHR_create_renderpass2");
    else
        DE_ASSERT(false);

    const auto multiLayer = (m_params.getImageLayers() > 1u);

    if (multiLayer)
    {
        if (context.getUsedApiVersion() < VK_API_VERSION_1_2)
            TCU_THROW(NotSupportedError, "Vulkan 1.2 required");

        const auto &vk12Features = context.getDeviceVulkan12Features();
        if (!vk12Features.shaderOutputLayer)
            TCU_THROW(NotSupportedError, "shaderOutputLayer not supported");
    }

    const bool resolveDepth   = (m_params.resolveAspects & VK_IMAGE_ASPECT_DEPTH_BIT);
    const bool resolveStencil = (m_params.resolveAspects & VK_IMAGE_ASPECT_STENCIL_BIT);
    const auto tcuFormat      = mapVkFormat(m_params.imageFormat);

    if (resolveDepth || resolveStencil)
    {
        context.requireDeviceFunctionality("VK_KHR_depth_stencil_resolve");
        const auto &dsResolveProps = context.getDepthStencilResolveProperties();

        if (resolveDepth)
        {
            if ((dsResolveProps.supportedDepthResolveModes & m_params.resolveMode) == 0u)
                TCU_THROW(NotSupportedError, "Required depth resolve mode not supported");
        }

        if (resolveStencil)
        {
            if ((dsResolveProps.supportedStencilResolveModes & m_params.resolveMode) == 0u)
                TCU_THROW(NotSupportedError, "Required stencil resolve mode not supported");

            // This is needed because we will store the stencil per-sample value using stencil exports.
            context.requireDeviceFunctionality("VK_EXT_shader_stencil_export");
        }

        if ((tcu::hasDepthComponent(tcuFormat.order) && !resolveDepth) ||
            (tcu::hasStencilComponent(tcuFormat.order) && !resolveStencil))
        {
            if (!dsResolveProps.independentResolveNone)
                TCU_THROW(NotSupportedError, "independentResolveNone not supported");
        }
    }

    if (tcu::isSRGB(tcuFormat))
    {
        if (m_params.sRGBFlags != SRGBFlags::NONE)
        {
#ifndef CTS_USES_VULKANSC
            const auto &m10Properties = context.getMaintenance10Properties();
            if (!m10Properties.resolveSrgbFormatSupportsTransferFunctionControl)
                TCU_THROW(NotSupportedError, "resolveSrgbFormatSupportsTransferFunctionControl not supported");
#endif // CTS_USES_VULKANSC
        }
    }

    // Check image format support.
    {
        VkImageFormatProperties formatProps;

        const auto imageInfo = m_params.getImageCreateInfo();
        const auto result    = ctx.vki.getPhysicalDeviceImageFormatProperties(
            ctx.physicalDevice, imageInfo.format, imageInfo.imageType, imageInfo.tiling, imageInfo.usage,
            imageInfo.flags, &formatProps);

        if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
            TCU_THROW(NotSupportedError, "Format not supported");

        VK_CHECK(result);

        if ((formatProps.sampleCounts & imageInfo.samples) == 0u)
            TCU_THROW(NotSupportedError, "Required sample count not supported");
    }
}

// The shaders will basically fill the multisample image with contents from a buffer.
void Maint10ResolveCase::initPrograms(vk::SourceCollections &programCollection) const
{
    const auto layerCount      = m_params.getImageLayers();
    const bool multiLayer      = (layerCount > 1u);
    const auto glslFragOutType = m_params.getGLSLFragOutType();
    const bool resolveDepth    = (m_params.resolveAspects & VK_IMAGE_ASPECT_DEPTH_BIT);
    const bool resolveStencil  = (m_params.resolveAspects & VK_IMAGE_ASPECT_STENCIL_BIT);
    const ShaderBuildOptions spv14Options(programCollection.usedVulkanVersion, SPIRV_VERSION_1_4, 0u, true);
    const ShaderBuildOptions defaultOptions;

    std::ostringstream vert;
    vert << "#version 460\n"
         << (multiLayer ? "#extension GL_ARB_shader_viewport_layer_array : require\n" : "")
         << "const vec4 vertices[] = vec4[](\n"
         << "    vec4(-1.0, -1.0, 0.0, 1.0),\n"
         << "    vec4(-1.0,  3.0, 0.0, 1.0),\n"
         << "    vec4( 3.0, -1.0, 0.0, 1.0)\n"
         << ");\n"
         << "void main (void) {\n"
         << "    gl_Position = vertices[gl_VertexIndex % 3];\n"
         << (multiLayer ? "    gl_Layer = gl_InstanceIndex;\n" : "") << "}\n";
    programCollection.glslSources.add("vert")
        << glu::VertexSource(vert.str()) << (multiLayer ? spv14Options : defaultOptions);

    std::ostringstream frag;
    frag << "#version 460\n"
         << (resolveStencil ? "#extension GL_ARB_shader_stencil_export : require\n" : "")
         << (glslFragOutType.empty() ? "" : "layout (location=0) out " + glslFragOutType + " outColor;\n")
         << "struct PixelData {\n"
         << "    " << (glslFragOutType.empty() ? "vec4" : glslFragOutType.c_str()) << " colorValue;\n"
         << "    vec4 dsValue; // .x = depth, .y = stencil (as float)\n"
         << "};\n"
         << "layout (set=0, binding=0) readonly buffer PixelValuesBlock {\n"
         << "    PixelData values[];\n"
         << "} pixels;\n"
         << "layout (push_constant, std430) uniform PushConstantBlock {\n"
         << "    float width;\n"
         << "    float height;\n"
         << "} pc;\n"
         << "void main (void) {\n"
         << "    const uint prevPixels = " << (multiLayer ? "uint(pc.width * pc.height) * uint(gl_Layer)" : "0u")
         << ";\n"
         << "    const uint pixelIndex = uint(floor(gl_FragCoord.y) * pc.width + floor(gl_FragCoord.x)) + prevPixels;\n"
         << "    const uint sampleIndex = pixelIndex * " << m_params.getSampleCount() << " + uint(gl_SampleID);\n"
         << (glslFragOutType.empty() ? "" : "    outColor = pixels.values[sampleIndex].colorValue;\n")
         << (resolveDepth ? "    gl_FragDepth = pixels.values[sampleIndex].dsValue.x;\n" : "")
         << (resolveStencil ? "    gl_FragStencilRefARB = int(pixels.values[sampleIndex].dsValue.y);\n" : "") << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

// Resolve regions. This is used in ::iterate().
struct ResolveRegion
{
    VkRect2D rect;
    uint32_t layer;

    ResolveRegion(VkRect2D rect_, uint32_t layer_) : rect(rect_), layer(layer_)
    {
    }
};

bool inResolveRegion(const tcu::UVec3 &coords, const ResolveRegion &region)
{
    if (coords.z() != region.layer)
        return false;

    DE_ASSERT(region.rect.offset.x >= 0);
    DE_ASSERT(region.rect.offset.y >= 0);

    const auto offsetX = static_cast<uint32_t>(region.rect.offset.x);
    const auto offsetY = static_cast<uint32_t>(region.rect.offset.y);

    if (coords.x() < offsetX || coords.x() >= offsetX + region.rect.extent.width)
        return false;

    if (coords.y() < offsetY || coords.y() >= offsetY + region.rect.extent.height)
        return false;

    return true;
}

bool inAnyResolveRegion(int x, int y, int z, const std::vector<ResolveRegion> &regions)
{
    const tcu::IVec3 iCoords(x, y, z);
    const auto coords = iCoords.asUint();

    for (const auto &region : regions)
    {
        if (inResolveRegion(coords, region))
            return true;
    }

    return false;
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

tcu::TestStatus Maint10ResolveInstance::iterate(void)
{
    const auto ctx        = m_context.getContextCommonData();
    const auto tcuFormat  = mapVkFormat(m_params.imageFormat);
    const auto layerCount = m_params.getImageLayers();
    const auto fullExtent = m_params.getExtent();
    const auto multiLayer = (layerCount > 1u);
    const auto viewType   = (multiLayer ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);
    const auto bindPoint  = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto isSRGB     = tcu::isSRGB(tcuFormat);

    // Create the multisample image.
    auto imageCreateInfo = m_params.getImageCreateInfo();
    ImageWithMemory msImage(ctx.vkd, ctx.device, ctx.allocator, imageCreateInfo, MemoryRequirement::Any);
    const auto fullSRR =
        makeImageSubresourceRange(getImageAspectFlags(tcuFormat), 0u, 1u, 0u, imageCreateInfo.arrayLayers);
    const auto msImageView = makeImageView(ctx.vkd, ctx.device, *msImage, viewType, m_params.imageFormat, fullSRR);

    // Create the single sample image, similar to the multi-sample one with a few changes.
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT; // Always needed for verification.
    if (m_params.resolveMethod == ResolveMethod::CMD)
        imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT; // Needed by vkCmdResolveImage2.
    ImageWithMemory ssImage(ctx.vkd, ctx.device, ctx.allocator, imageCreateInfo, MemoryRequirement::Any);
    const auto ssImageView = makeImageView(ctx.vkd, ctx.device, *ssImage, viewType, m_params.imageFormat, fullSRR);

    // This is equivalent to the frag shaders's PixelData structure.
    struct PixelData
    {
        union
        {
            tcu::Vec4 fColor;
            tcu::IVec4 iColor;
            tcu::UVec4 uColor;
        };
        tcu::Vec4 depthStencil; // .x() is depth, .y() is stencil, as float.

        PixelData()
        {
            memset(reinterpret_cast<void *>(this), 0, sizeof(*this));
        }
    };

    std::vector<ResolveRegion> resolveRegions;
    if (m_params.resolveArea == ResolveArea::FULL || m_params.resolveArea == ResolveArea::FULL_MULTILAYER)
        resolveRegions.push_back(
            ResolveRegion(makeRect2D(0, 0, imageCreateInfo.extent.width, imageCreateInfo.extent.height), 0u));
    else
    {
        DE_ASSERT(m_params.resolveMethod == ResolveMethod::CMD);
        const auto extent2D        = m_params.getImageExtent();
        const auto quadrantExtent  = extent2D / tcu::IVec3(2, 2, 1);
        const auto quadrantExtentU = quadrantExtent.asUint();

        if (m_params.resolveArea == ResolveArea::REGION)
        {
            // Resolve the bottom-right quadrant only.
            resolveRegions.push_back(ResolveRegion(
                makeRect2D(quadrantExtent.x(), quadrantExtent.y(), quadrantExtentU.x(), quadrantExtentU.y()), 0u));
        }
        else if (m_params.resolveArea == ResolveArea::REGIONS_MULTILAYER)
        {
            // Resolve bottom-right quadrant in the 1st layer, and the top and bottom-left quadrants in the 2nd one.
            resolveRegions.push_back(ResolveRegion(
                makeRect2D(quadrantExtent.x(), quadrantExtent.y(), quadrantExtentU.x(), quadrantExtentU.y()), 0u));
            resolveRegions.push_back(ResolveRegion(makeRect2D(0, 0, quadrantExtentU.x(), quadrantExtentU.y()), 1u));
            resolveRegions.push_back(
                ResolveRegion(makeRect2D(0, quadrantExtent.y(), quadrantExtentU.x(), quadrantExtentU.y()), 1u));
        }
        else
            DE_ASSERT(false);
    }

    // Pixels buffer content.
    const auto randomSeed = m_params.getRandomSeed();
    de::Random rnd(randomSeed);

    const auto sampleCount      = m_params.getSampleCount();
    const auto perPixelSamples  = static_cast<uint32_t>(sampleCount);
    const auto layerPixelCount  = imageCreateInfo.extent.width * imageCreateInfo.extent.height;
    const auto layerSampleCount = layerPixelCount * perPixelSamples;
    const auto totalSamples     = layerSampleCount * imageCreateInfo.arrayLayers;
    std::vector<PixelData> pixelDataVec(totalSamples, PixelData());

    const bool isInt   = isIntFormat(m_params.imageFormat);
    const bool isUint  = isUintFormat(m_params.imageFormat);
    const bool isFloat = !(isInt || isUint);

    const bool resolveDepth   = (m_params.resolveAspects & VK_IMAGE_ASPECT_DEPTH_BIT);
    const bool resolveStencil = (m_params.resolveAspects & VK_IMAGE_ASPECT_STENCIL_BIT);
    const bool resolveColor   = (m_params.resolveAspects & VK_IMAGE_ASPECT_COLOR_BIT);

    for (const auto &resolveRegion : resolveRegions)
    {
        for (uint32_t y = 0u; y < resolveRegion.rect.extent.height; ++y)
        {
            const uint32_t yCoord = y + static_cast<uint32_t>(resolveRegion.rect.offset.y);
            for (uint32_t x = 0u; x < resolveRegion.rect.extent.width; ++x)
            {
                const uint32_t xCoord = x + static_cast<uint32_t>(resolveRegion.rect.offset.x);
                for (int s = 0; s < sampleCount; ++s)
                {
                    const uint32_t pixelIdx =
                        (yCoord * imageCreateInfo.extent.width + xCoord) + (layerPixelCount * resolveRegion.layer);
                    const uint32_t sampleIdx = pixelIdx * sampleCount + s;

                    auto &pixelData = pixelDataVec.at(sampleIdx);

                    if (resolveDepth)
                        pixelData.depthStencil.x() = rnd.getFloat();

                    if (resolveStencil)
                        pixelData.depthStencil.y() = static_cast<float>(rnd.getInt(0, 255));

                    if (resolveColor)
                    {
                        const auto bitDepth = tcu::getTextureFormatBitDepth(tcuFormat);
                        if (isInt)
                        {
                            for (int i = 0; i < 4; ++i)
                            {
                                if (bitDepth[i] == 0)
                                    ;
                                else if (bitDepth[i] == 8)
                                    pixelData.iColor[i] = rnd.getInt(-127, 127);
                                else if (bitDepth[i] == 16)
                                    pixelData.iColor[i] = rnd.getInt(-32767, 32767);
                                else if (bitDepth[i] == 32)
                                    pixelData.iColor[i] = rnd.getInt(-2147483647, 2147483647);
                                else
                                    DE_ASSERT(false);
                            }
                        }
                        else if (isUint)
                        {
                            for (int i = 0; i < 4; ++i)
                            {
                                if (bitDepth[i] == 0)
                                    ;
                                else if (bitDepth[i] == 8)
                                    pixelData.uColor[i] = rnd.getUint8();
                                else if (bitDepth[i] == 16)
                                    pixelData.uColor[i] = rnd.getUint16();
                                else if (bitDepth[i] == 32)
                                    pixelData.uColor[i] = rnd.getUint32();
                                else
                                    DE_ASSERT(false);
                            }
                        }
                        else if (isFloat)
                        {
                            for (int i = 0; i < 4; ++i)
                            {
                                if (bitDepth[i] == 0)
                                    ;
                                else
                                    pixelData.fColor[i] = rnd.getFloat();
                            }
                        }
                        else
                            DE_ASSERT(false);
                    }
                }
            }
        }
    }

    // Dump the contents into a storage buffer.
    const auto pixelsBufferSize  = static_cast<VkDeviceSize>(de::dataSize(pixelDataVec));
    const auto pixelsBufferUsage = (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const auto pixelsBufferInfo  = makeBufferCreateInfo(pixelsBufferSize, pixelsBufferUsage);
    BufferWithMemory pixelsBuffer(ctx.vkd, ctx.device, ctx.allocator, pixelsBufferInfo, HostIntent::W);
    {
        auto &alloc = pixelsBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(pixelDataVec), de::dataSize(pixelDataVec));
        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    // Verification buffers for color, depth and stencil.
    using BufferWithMemoryPtr = std::unique_ptr<BufferWithMemory>;
    BufferWithMemoryPtr colorVerifBuffer;
    BufferWithMemoryPtr depthVerifBuffer;
    BufferWithMemoryPtr stencilVerifBuffer;

    const auto verifBufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    uint32_t colorLayerBytes    = 0u;
    uint32_t depthLayerBytes    = 0u;
    uint32_t stencilLayerBytes  = 0u;

    tcu::TextureFormat depthCopyFormat;
    tcu::TextureFormat stencilCopyFormat;

    if (resolveColor)
    {
        const auto pixelBytes = tcu::getPixelSize(tcuFormat);
        colorLayerBytes       = layerPixelCount * pixelBytes;
        const auto bufferSize = colorLayerBytes * layerCount;

        const auto bufferInfo = makeBufferCreateInfo(bufferSize, verifBufferUsage);
        colorVerifBuffer.reset(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, bufferInfo, HostIntent::R));
    }

    if (resolveDepth)
    {
        depthCopyFormat       = getDepthCopyFormat(m_params.imageFormat);
        const auto pixelBytes = tcu::getPixelSize(depthCopyFormat);
        depthLayerBytes       = layerPixelCount * pixelBytes;
        const auto bufferSize = depthLayerBytes * layerCount;

        const auto bufferInfo = makeBufferCreateInfo(bufferSize, verifBufferUsage);
        depthVerifBuffer.reset(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, bufferInfo, HostIntent::R));
    }

    if (resolveStencil)
    {
        stencilCopyFormat     = getStencilCopyFormat(m_params.imageFormat);
        const auto pixelBytes = tcu::getPixelSize(stencilCopyFormat);
        stencilLayerBytes     = layerPixelCount * pixelBytes;
        const auto bufferSize = stencilLayerBytes * layerCount;

        const auto bufferInfo = makeBufferCreateInfo(bufferSize, verifBufferUsage);
        stencilVerifBuffer.reset(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, bufferInfo, HostIntent::R));
    }

    // Descriptor set and pipeline layout.
    const auto dataStages = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto descType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(descType, dataStages);
    const auto setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);

    const auto pcSize  = DE_SIZEOF32(tcu::Vec2);
    const auto pcData  = fullExtent.asFloat().swizzle(0, 1);
    const auto pcRange = makePushConstantRange(dataStages, 0u, pcSize);

    PipelineLayoutWrapper pipelineLayout(m_params.constructionType, ctx.vkd, ctx.device, *setLayout, &pcRange);

    // Descriptor pool, set and update.
    DescriptorPoolBuilder descPoolBuilder;
    descPoolBuilder.addType(descType);
    const auto descPool =
        descPoolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descSet = makeDescriptorSet(ctx.vkd, ctx.device, *descPool, *setLayout);

    DescriptorSetUpdateBuilder setUpdateBuilder;
    const auto descBufferInfo = makeDescriptorBufferInfo(*pixelsBuffer, 0ull, VK_WHOLE_SIZE);
    const auto binding        = DescriptorSetUpdateBuilder::Location::binding;
    setUpdateBuilder.writeSingle(*descSet, binding(0u), descType, &descBufferInfo);
    setUpdateBuilder.update(ctx.vkd, ctx.device);

    // Shaders.
    const auto &binaries = m_context.getBinaryCollection();
    ShaderWrapper vertShader(ctx.vkd, ctx.device, binaries.get("vert"));
    ShaderWrapper fragShader(ctx.vkd, ctx.device, binaries.get("frag"));

    const std::vector<VkViewport> viewports(1u, makeViewport(imageCreateInfo.extent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(imageCreateInfo.extent));

    std::vector<VkAttachmentDescription2> attachmentDescriptions;

    const bool isDS   = isDepthStencilFormat(m_params.imageFormat);
    const bool ssInRP = (m_params.resolveMethod != ResolveMethod::CMD); // Include single-sample att in render pass.

    // Multisample attachment: if we resolve it with a cmd we need to store results. Otherwise it's resolved in the
    // render pass itself and we do not need to store stuff to the multisample attachment.
    const auto msLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
    const auto msStoreOp = (ssInRP ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE);
    const auto ssFinalRPLayout =
        (isDS ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    attachmentDescriptions.push_back(VkAttachmentDescription2{
        VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
        nullptr,
        0u,
        m_params.imageFormat,
        sampleCount,
        msLoadOp,
        msStoreOp,
        msLoadOp,
        msStoreOp,
        VK_IMAGE_LAYOUT_UNDEFINED,
        ssFinalRPLayout,
    });

    if (ssInRP)
    {
        // Single sample attachment for render pass use.
        const auto ssLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        const auto ssStoreOp = VK_ATTACHMENT_STORE_OP_STORE;

        VkAttachmentDescriptionFlags attFlags = 0u;

#ifndef CTS_USES_VULKANSC
        // Note the render pass wrapper will translate these attachment description flags to
        if (m_params.sRGBFlags == SRGBFlags::ENABLE)
            attFlags |= VK_ATTACHMENT_DESCRIPTION_RESOLVE_ENABLE_TRANSFER_FUNCTION_BIT_KHR;
        else if (m_params.sRGBFlags == SRGBFlags::SKIP)
            attFlags |= VK_ATTACHMENT_DESCRIPTION_RESOLVE_SKIP_TRANSFER_FUNCTION_BIT_KHR;
#endif // CTS_USES_VULKANSC

        attachmentDescriptions.push_back(VkAttachmentDescription2{
            VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
            nullptr,
            attFlags,
            m_params.imageFormat,
            VK_SAMPLE_COUNT_1_BIT,
            ssLoadOp,
            ssStoreOp,
            ssLoadOp,
            ssStoreOp,
            VK_IMAGE_LAYOUT_UNDEFINED,
            ssFinalRPLayout,
        });
    }

    // Always used.
    const VkAttachmentReference2 msAttRef = {
        VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, nullptr, 0u, ssFinalRPLayout, fullSRR.aspectMask,
    };

    // Only used if ssInRP.
    auto ssAttRef       = msAttRef;
    ssAttRef.attachment = 1u;

    std::vector<VkAttachmentReference2> colorAttRefs;
    std::vector<VkAttachmentReference2> dsAttRefs;

    const auto depthResolveMode   = (resolveDepth ? m_params.resolveMode : VK_RESOLVE_MODE_NONE);
    const auto stencilResolveMode = (resolveStencil ? m_params.resolveMode : VK_RESOLVE_MODE_NONE);

    // Only used if isDS and ssInRP.
    const VkSubpassDescriptionDepthStencilResolve subpassDSResolve = {
        VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE,
        nullptr,
        depthResolveMode,
        stencilResolveMode,
        &ssAttRef,
    };

    // Multisample attachment reference is always used.
    {
        auto &attRefs = (isDS ? dsAttRefs : colorAttRefs);
        attRefs.push_back(msAttRef);
    }

    // Note how resolve attachments are only added here if ssInRP.
    // The DS one when isDS, and the color one otherwise.
    const VkSubpassDescription2 subpassDescription = {
        VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
        ((isDS && ssInRP) ? &subpassDSResolve : nullptr), // DS resolve.
        0u,
        bindPoint,
        0u,
        0u,
        nullptr,
        de::sizeU32(colorAttRefs),
        de::dataOrNull(colorAttRefs),
        ((isDS || !ssInRP) ? nullptr : &ssAttRef), // Color resolve.
        de::dataOrNull(dsAttRefs),
        0u,
        nullptr,
    };

    const VkRenderPassCreateInfo2 renderPassCreateInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
        nullptr,
        0u,
        de::sizeU32(attachmentDescriptions),
        de::dataOrNull(attachmentDescriptions),
        1u,
        &subpassDescription,
        0u,
        nullptr,
        0u,
        nullptr,
    };

    const bool isDynamicRendering = (m_params.resolveMethod == ResolveMethod::DYNAMIC_RENDER ||
                                     isConstructionTypeShaderObject(m_params.constructionType));
    RenderPassWrapper renderPass(ctx.vkd, ctx.device, &renderPassCreateInfo, isDynamicRendering);

    {
        std::vector<VkImage> fbImages;
        std::vector<VkImageView> fbViews;

        fbImages.push_back(*msImage);
        fbViews.push_back(*msImageView);

        if (ssInRP)
        {
            fbImages.push_back(*ssImage);
            fbViews.push_back(*ssImageView);
        }

        renderPass.createFramebuffer(ctx.vkd, ctx.device, de::sizeU32(fbImages), de::dataOrNull(fbImages),
                                     de::dataOrNull(fbViews), imageCreateInfo.extent.width,
                                     imageCreateInfo.extent.height, imageCreateInfo.arrayLayers);
    }

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();

    const auto stencilOpState = makeStencilOpState(VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE,
                                                   VK_COMPARE_OP_ALWAYS, 0xFFu, 0xFFu, 0u);
    const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        nullptr,
        0u,
        makeVkBool(resolveDepth),
        makeVkBool(resolveDepth),
        VK_COMPARE_OP_ALWAYS,
        VK_FALSE,
        makeVkBool(resolveStencil),
        stencilOpState,
        stencilOpState,
        0.0f,
        1.0f,
    };

    const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        0u,
        sampleCount,
        VK_TRUE, // Sample shading enabled. It should be enabled implicitly in any case due to using gl_SampleID.
        1.0f,
        nullptr,
        VK_FALSE,
        VK_FALSE,
    };

    const auto colorWriteMask =
        (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);

    const VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {
        VK_FALSE,        VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO,
        VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO,
        VK_BLEND_OP_ADD, colorWriteMask,
    };

    const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        nullptr,
        0u,
        VK_FALSE,
        VK_LOGIC_OP_CLEAR,
        (isDS ? 0u : 1u),
        &colorBlendAttachmentState,
        {0.0f, 0.0f, 0.0f, 0.0f},
    };

#ifndef CTS_USES_VULKANSC
    VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        nullptr,
        0u,
        (isDS ? 0u : 1u),
        &m_params.imageFormat,
        (resolveDepth ? m_params.imageFormat : VK_FORMAT_UNDEFINED),
        (resolveStencil ? m_params.imageFormat : VK_FORMAT_UNDEFINED),
    };

    PipelineRenderingCreateInfoWrapper rciWrapper(isDynamicRendering ? &pipelineRenderingCreateInfo : nullptr);
#else
    PipelineRenderingCreateInfoWrapper rciWrapper(nullptr);
#endif

    GraphicsPipelineWrapper pipeline(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device, m_context.getDeviceExtensions(),
                                     m_params.constructionType);
    pipeline.setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .setDefaultRasterizationState()
        .setupVertexInputState(&vertexInputStateCreateInfo)
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass.get(), 0u, vertShader,
                                          nullptr, ShaderWrapper(), ShaderWrapper(), ShaderWrapper(), nullptr, nullptr,
                                          rciWrapper)
        .setupFragmentShaderState(pipelineLayout, renderPass.get(), 0u, fragShader, &depthStencilStateCreateInfo,
                                  &multisampleStateCreateInfo)
        .setupFragmentOutputState(renderPass.get(), 0u, &colorBlendStateCreateInfo, &multisampleStateCreateInfo)
        .buildPipeline();

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer   = *cmd.cmdBuffer;
    VkImageLayout ssLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Track the single-sample image layout.

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    renderPass.begin(ctx.vkd, cmdBuffer, scissors.at(0u), tcu::Vec4(0.0f));
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descSet.get(), 0u, nullptr);
    ctx.vkd.cmdPushConstants(cmdBuffer, *pipelineLayout, dataStages, 0u, pcSize, &pcData);
    pipeline.bind(cmdBuffer);
    ctx.vkd.cmdDraw(cmdBuffer, 3u, imageCreateInfo.arrayLayers, 0u, 0u);
    renderPass.end(ctx.vkd, cmdBuffer);

    if (m_params.resolveMethod == ResolveMethod::CMD)
    {
        // Clear single-sample image and move it to the right layout.
        {
            // Move single-sample image to the right layout for clearing.
            {
                const auto barrier = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, ssLayout,
                                                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, *ssImage, fullSRR);
                cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                              VK_PIPELINE_STAGE_TRANSFER_BIT, &barrier);
            }
            ssLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

            if (isDS)
            {
                VkClearDepthStencilValue clearValue;
                memset(&clearValue, 0, sizeof(clearValue));
                ctx.vkd.cmdClearDepthStencilImage(cmdBuffer, *ssImage, ssLayout, &clearValue, 1u, &fullSRR);
            }
            else
            {
                VkClearColorValue clearValue;
                memset(&clearValue, 0, sizeof(clearValue));
                ctx.vkd.cmdClearColorImage(cmdBuffer, *ssImage, ssLayout, &clearValue, 1u, &fullSRR);
            }

            // Sync single-sample clears with the resolve command.
            {
                const auto barrier = makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                                            ssLayout, ssLayout, *ssImage, fullSRR);
                cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                              VK_PIPELINE_STAGE_TRANSFER_BIT, &barrier);
            }
        }

        // Sync attachment writes with transfer reads in the multi-sample image.
        {
            const auto srcAccess =
                (isDS ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
            const auto srcStages =
                (isDS ? (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT) :
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
            const auto barrier = makeImageMemoryBarrier(srcAccess, VK_ACCESS_TRANSFER_READ_BIT, ssFinalRPLayout,
                                                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *msImage, fullSRR);
            cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, srcStages, VK_PIPELINE_STAGE_TRANSFER_BIT, &barrier);
        }

        // Resolve the selected regions.
        {
            // Convert the resolve regions info we created above to the expected structures.
            std::vector<VkImageResolve2> imageResolveRegions;
            imageResolveRegions.reserve(resolveRegions.size());

            for (const auto &region : resolveRegions)
            {
                const VkImageSubresourceLayers layer = {
                    m_params.resolveAspects,
                    0u,
                    region.layer,
                    1u,
                };
                const auto offset = makeOffset3D(region.rect.offset.x, region.rect.offset.y, 0);
                const auto extent = makeExtent3D(region.rect.extent.width, region.rect.extent.height, 1u);

                imageResolveRegions.push_back(VkImageResolve2{
                    VK_STRUCTURE_TYPE_IMAGE_RESOLVE_2,
                    nullptr,
                    layer,
                    offset,
                    layer,
                    offset,
                    extent,
                });
            }

#ifndef CTS_USES_VULKANSC
            VkResolveImageFlagsKHR resolveModeFlags = 0u;

            if (m_params.sRGBFlags == SRGBFlags::ENABLE)
                resolveModeFlags |= VK_RESOLVE_IMAGE_ENABLE_TRANSFER_FUNCTION_BIT_KHR;
            else if (m_params.sRGBFlags == SRGBFlags::SKIP)
                resolveModeFlags |= VK_RESOLVE_IMAGE_SKIP_TRANSFER_FUNCTION_BIT_KHR;

            const VkResolveImageModeInfoKHR resolveModeInfo = {
                VK_STRUCTURE_TYPE_RESOLVE_IMAGE_MODE_INFO_KHR,
                nullptr,
                resolveModeFlags,
                (isDS ? depthResolveMode : m_params.resolveMode),
                (isDS ? stencilResolveMode : VK_RESOLVE_MODE_NONE),
            };
#endif // CTS_USES_VULKANSC

            const VkResolveImageInfo2 resolveImageInfo = {
                VK_STRUCTURE_TYPE_RESOLVE_IMAGE_INFO_2,
#ifndef CTS_USES_VULKANSC
                &resolveModeInfo,
#else
                nullptr,
#endif // CTS_USES_VULKANSC
                *msImage,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                *ssImage,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                de::sizeU32(imageResolveRegions),
                de::dataOrNull(imageResolveRegions),
            };

#ifndef CTS_USES_VULKANSC
            ctx.vkd.cmdResolveImage2(cmdBuffer, &resolveImageInfo);
#else
            ctx.vkd.cmdResolveImage2KHR(cmdBuffer, &resolveImageInfo);
#endif // CTS_USES_VULKANSC
        }
    }
    else
        ssLayout = ssFinalRPLayout;

    // Copy single-sample image to verification buffer(s).
    {
        // Note the color access flags also apply to DS resolves as per the spec.
        const auto srcAccess =
            ((ssLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) ? VK_ACCESS_TRANSFER_WRITE_BIT :
                                                                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
        const auto srcStage =
            ((ssLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) ? VK_PIPELINE_STAGE_TRANSFER_BIT :
                                                                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        const auto dstAccess = VK_ACCESS_TRANSFER_READ_BIT;
        const auto dstStage  = VK_PIPELINE_STAGE_TRANSFER_BIT;

        const auto barrier = makeImageMemoryBarrier(srcAccess, dstAccess, ssLayout,
                                                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *ssImage, fullSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, srcStage, dstStage, &barrier);
        ssLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        if (resolveColor)
        {
            const auto srLayers   = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, layerCount);
            const auto copyRegion = makeBufferImageCopy(imageCreateInfo.extent, srLayers);
            ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, *ssImage, ssLayout, colorVerifBuffer->get(), 1u, &copyRegion);
        }

        if (resolveDepth)
        {
            const auto srLayers   = makeImageSubresourceLayers(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, layerCount);
            const auto copyRegion = makeBufferImageCopy(imageCreateInfo.extent, srLayers);
            ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, *ssImage, ssLayout, depthVerifBuffer->get(), 1u, &copyRegion);
        }

        if (resolveStencil)
        {
            const auto srLayers   = makeImageSubresourceLayers(VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, layerCount);
            const auto copyRegion = makeBufferImageCopy(imageCreateInfo.extent, srLayers);
            ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, *ssImage, ssLayout, stencilVerifBuffer->get(), 1u, &copyRegion);
        }
    }

    // Sync host reads.
    {
        const auto barrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &barrier);
    }

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    using ConstPixelBufferAccessPtr = std::unique_ptr<tcu::ConstPixelBufferAccess>;
    ConstPixelBufferAccessPtr colorResult;
    ConstPixelBufferAccessPtr depthResult;
    ConstPixelBufferAccessPtr stencilResult;

    if (colorVerifBuffer)
    {
        auto &alloc = colorVerifBuffer->getAllocation();
        invalidateAlloc(ctx.vkd, ctx.device, alloc);
        colorResult.reset(new tcu::ConstPixelBufferAccess(tcuFormat, fullExtent, alloc.getHostPtr()));
    }

    if (depthVerifBuffer)
    {
        auto &alloc = depthVerifBuffer->getAllocation();
        invalidateAlloc(ctx.vkd, ctx.device, alloc);
        depthResult.reset(new tcu::ConstPixelBufferAccess(depthCopyFormat, fullExtent, alloc.getHostPtr()));
    }

    if (stencilVerifBuffer)
    {
        auto &alloc = stencilVerifBuffer->getAllocation();
        invalidateAlloc(ctx.vkd, ctx.device, alloc);
        stencilResult.reset(new tcu::ConstPixelBufferAccess(stencilCopyFormat, fullExtent, alloc.getHostPtr()));
    }

    // Prepare expected outputs.
    using TextureLevelPtr = std::unique_ptr<tcu::TextureLevel>;
    TextureLevelPtr colorRefLevel;
    TextureLevelPtr depthRefLevel;
    TextureLevelPtr stencilRefLevel;

    // Get index for pixelDataVec given the x,y,z and the sample id.
    const auto getSampleIndex = [&](int x, int y, int z, uint32_t s)
    {
        const auto pixelInLayer = (y * fullExtent.x() + x);
        const auto pixelIdx     = z * fullExtent.x() * fullExtent.y() + pixelInLayer;
        const auto sampleIdx    = static_cast<uint32_t>(pixelIdx) * perPixelSamples + s;
        return sampleIdx;
    };

    // When dividing to calculate an average, use these.
    const float sampleCountDiv = static_cast<float>(perPixelSamples);
    const tcu::Vec4 vectorDiv(sampleCountDiv);

#ifndef CTS_USES_VULKANSC
    const auto &m10Properties         = m_context.getMaintenance10Properties();
    const bool averageInLinearDefault = m10Properties.resolveSrgbFormatAppliesTransferFunction;
#else
    const bool averageInLinearDefault = true;
#endif // CTS_USES_VULKANSC

    if (resolveColor)
    {
        colorRefLevel.reset(new tcu::TextureLevel(tcuFormat, fullExtent.x(), fullExtent.y(), fullExtent.z()));
        tcu::PixelBufferAccess colorReference = colorRefLevel->getAccess();
        tcu::clear(colorReference, tcu::Vec4(0.0f));

        for (int z = 0; z < fullExtent.z(); ++z)
            for (int y = 0; y < fullExtent.y(); ++y)
                for (int x = 0; x < fullExtent.x(); ++x)
                {
                    if (!inAnyResolveRegion(x, y, z, resolveRegions))
                        continue;

                    // Gather color samples.
                    std::vector<const PixelData *> samples;
                    samples.reserve(perPixelSamples);

                    for (uint32_t s = 0; s < perPixelSamples; ++s)
                        samples.push_back(&pixelDataVec.at(getSampleIndex(x, y, z, s)));

                    if (isInt)
                    {
                        if (m_params.resolveMode == VK_RESOLVE_MODE_SAMPLE_ZERO_BIT)
                            colorReference.setPixel(samples.front()->iColor, x, y, z);
                        else
                            DE_ASSERT(false);
                    }
                    else if (isUint)
                    {
                        if (m_params.resolveMode == VK_RESOLVE_MODE_SAMPLE_ZERO_BIT)
                            colorReference.setPixel(samples.front()->uColor, x, y, z);
                        else
                            DE_ASSERT(false);
                    }
                    else if (isFloat)
                    {
                        if (m_params.resolveMode == VK_RESOLVE_MODE_AVERAGE_BIT)
                        {
                            bool averageInNonLinear = false;
                            if (isSRGB)
                                averageInNonLinear =
                                    (m_params.sRGBFlags == SRGBFlags::SKIP ||
                                     (m_params.sRGBFlags == SRGBFlags::NONE && !averageInLinearDefault));

                            tcu::Vec4 average(0.0f);
                            for (const auto &sample : samples)
                                average += (averageInNonLinear ? tcu::linearToSRGB(sample->fColor) : sample->fColor);
                            average = average / vectorDiv;

                            // setPixel does not transform anything for sRGB formats, so we need to make sure values
                            // are in non-linear space before saving them.
                            if (isSRGB && !averageInNonLinear)
                                average = tcu::linearToSRGB(average);

                            colorReference.setPixel(average, x, y, z);
                        }
                        else
                            DE_ASSERT(false);
                    }
                    else
                        DE_ASSERT(false);
                }
    }

    if (resolveDepth)
    {
        depthRefLevel.reset(new tcu::TextureLevel(depthCopyFormat, fullExtent.x(), fullExtent.y(), fullExtent.z()));
        tcu::PixelBufferAccess depthReference = depthRefLevel->getAccess();
        tcu::clearDepth(depthReference, 0.0f);

        for (int z = 0; z < fullExtent.z(); ++z)
            for (int y = 0; y < fullExtent.y(); ++y)
                for (int x = 0; x < fullExtent.x(); ++x)
                {
                    if (!inAnyResolveRegion(x, y, z, resolveRegions))
                        continue;

                    // Gather color samples.
                    std::vector<const PixelData *> samples;
                    samples.reserve(perPixelSamples);

                    for (uint32_t s = 0; s < perPixelSamples; ++s)
                        samples.push_back(&pixelDataVec.at(getSampleIndex(x, y, z, s)));

                    if (m_params.resolveMode == VK_RESOLVE_MODE_SAMPLE_ZERO_BIT)
                        depthReference.setPixDepth(samples.front()->depthStencil.x(), x, y, z);
                    else if (m_params.resolveMode == VK_RESOLVE_MODE_MIN_BIT)
                    {
                        float minDepth = 1000.0f; // Large value that will be overwritten.
                        for (const auto &sample : samples)
                        {
                            if (sample->depthStencil.x() < minDepth)
                                minDepth = sample->depthStencil.x();
                        }
                        depthReference.setPixDepth(minDepth, x, y, z);
                    }
                    else if (m_params.resolveMode == VK_RESOLVE_MODE_MAX_BIT)
                    {
                        float maxDepth = -1000.0f; // Small value that will be overwritten.
                        for (const auto &sample : samples)
                        {
                            if (sample->depthStencil.x() > maxDepth)
                                maxDepth = sample->depthStencil.x();
                        }
                        depthReference.setPixDepth(maxDepth, x, y, z);
                    }
                    else if (m_params.resolveMode == VK_RESOLVE_MODE_AVERAGE_BIT)
                    {
                        float avg = 0.0f;
                        for (const auto &sample : samples)
                            avg += sample->depthStencil.x();
                        avg /= sampleCountDiv;
                        depthReference.setPixDepth(avg, x, y, z);
                    }
                    else
                        DE_ASSERT(false);
                }
    }

    if (resolveStencil)
    {
        stencilRefLevel.reset(new tcu::TextureLevel(stencilCopyFormat, fullExtent.x(), fullExtent.y(), fullExtent.z()));
        tcu::PixelBufferAccess stencilReference = stencilRefLevel->getAccess();
        tcu::clearStencil(stencilReference, 0);

        for (int z = 0; z < fullExtent.z(); ++z)
            for (int y = 0; y < fullExtent.y(); ++y)
                for (int x = 0; x < fullExtent.x(); ++x)
                {
                    if (!inAnyResolveRegion(x, y, z, resolveRegions))
                        continue;

                    // Gather color samples.
                    std::vector<const PixelData *> samples;
                    samples.reserve(perPixelSamples);

                    for (uint32_t s = 0; s < perPixelSamples; ++s)
                        samples.push_back(&pixelDataVec.at(getSampleIndex(x, y, z, s)));

                    if (m_params.resolveMode == VK_RESOLVE_MODE_SAMPLE_ZERO_BIT)
                        stencilReference.setPixStencil(static_cast<int>(samples.front()->depthStencil.y()), x, y, z);
                    else if (m_params.resolveMode == VK_RESOLVE_MODE_MIN_BIT)
                    {
                        int minStencil = std::numeric_limits<int>::max(); // Large value that will be overwritten.
                        for (const auto &sample : samples)
                        {
                            const int stencilVal = static_cast<int>(sample->depthStencil.y());
                            if (stencilVal < minStencil)
                                minStencil = stencilVal;
                        }
                        stencilReference.setPixStencil(minStencil, x, y, z);
                    }
                    else if (m_params.resolveMode == VK_RESOLVE_MODE_MAX_BIT)
                    {
                        int maxStencil = std::numeric_limits<int>::min(); // Small value that will be overwritten.
                        for (const auto &sample : samples)
                        {
                            const int stencilVal = static_cast<int>(sample->depthStencil.y());
                            if (stencilVal > maxStencil)
                                maxStencil = stencilVal;
                        }
                        stencilReference.setPixStencil(maxStencil, x, y, z);
                    }
                    else
                        DE_ASSERT(false);
                }
    }

    bool pass = true;
    auto &log = m_context.getTestContext().getLog();

    if (resolveColor)
    {
        tcu::ConstPixelBufferAccess colorReference = colorRefLevel->getAccess();
        for (uint32_t i = 0u; i < layerCount; ++i)
        {
            const auto refLayer =
                tcu::getSubregion(colorReference, 0, 0, static_cast<int>(i), fullExtent.x(), fullExtent.y(), 1);
            const auto resLayer =
                tcu::getSubregion(*colorResult, 0, 0, static_cast<int>(i), fullExtent.x(), fullExtent.y(), 1);

            const std::string setName = "Color-Layer" + std::to_string(i);
            if (isInt || isUint)
            {
                const tcu::UVec4 threshold(0u); // Expect exact results for these resolves.
                if (!tcu::intThresholdCompare(log, setName.c_str(), "", refLayer, resLayer, threshold,
                                              tcu::COMPARE_LOG_ON_ERROR))
                    pass = false;
            }
            else
            {
                const auto threshold = getColorFormatThreshold(m_params.imageFormat);
                if (!tcu::floatThresholdCompare(log, setName.c_str(), "", refLayer, resLayer, threshold,
                                                tcu::COMPARE_LOG_ON_ERROR))
                    pass = false;
            }
        }
    }

    if (resolveDepth)
    {
        tcu::ConstPixelBufferAccess depthReference = depthRefLevel->getAccess();
        for (uint32_t i = 0u; i < layerCount; ++i)
        {
            const auto refLayer =
                tcu::getSubregion(depthReference, 0, 0, static_cast<int>(i), fullExtent.x(), fullExtent.y(), 1);
            const auto resLayer =
                tcu::getSubregion(*depthResult, 0, 0, static_cast<int>(i), fullExtent.x(), fullExtent.y(), 1);

            // Choose a threshold according to the format. The threshold will generally be more than 1 unit but less
            // than 2 for UNORM formats. For SFLOAT, which has 24 mantissa bits (23 explicitly stored), we make it
            // similar to D24.
            float depthThreshold = 0.0f;
            switch (m_params.imageFormat)
            {
            case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_D16_UNORM_S8_UINT:
                depthThreshold = 0.000025f;
                break;
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                // In practice, we detected that the original threshold here (0.000000075f) was not enough in some
                // cases. We need to take into account that the reference value is calculated using floats, which have
                // their own precission issues, and the spec does not specify how the implementation calculates the
                // average. If the implementation is storing the sample values first as D24s and sampling them later,
                // it's losing precission already in that step, on top of the average and final store.
                //
                // Increasing the threshold to 2 units for D24 and D32 is still reasonable.
                //
                //depthThreshold = 0.000000075f;
                depthThreshold = 0.000000125f;
                break;
            default:
                DE_ASSERT(false);
                break;
            }

            const std::string setName = "Depth-Layer" + std::to_string(i);
            if (!tcu::dsThresholdCompare(log, setName.c_str(), "", refLayer, resLayer, depthThreshold,
                                         tcu::COMPARE_LOG_ON_ERROR))
                pass = false;
        }
    }

    if (resolveStencil)
    {
        tcu::ConstPixelBufferAccess stencilReference = stencilRefLevel->getAccess();
        for (uint32_t i = 0u; i < layerCount; ++i)
        {
            const auto refLayer =
                tcu::getSubregion(stencilReference, 0, 0, static_cast<int>(i), fullExtent.x(), fullExtent.y(), 1);
            const auto resLayer =
                tcu::getSubregion(*stencilResult, 0, 0, static_cast<int>(i), fullExtent.x(), fullExtent.y(), 1);

            // Expect exact results for these resolves.
            const std::string setName = "Stencil-Layer" + std::to_string(i);
            if (!tcu::dsThresholdCompare(log, setName.c_str(), "", refLayer, resLayer, 0.0f, tcu::COMPARE_LOG_ON_ERROR))
                pass = false;
        }
    }

    if (!pass)
        TCU_FAIL("Unexpected results found in some buffers; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createMultisampleResolveMaint10Tests(tcu::TestContext &testCtx,
                                                         PipelineConstructionType pipelineConstructionType)
{
    const struct
    {
        ResolveMethod resolveMethod;
        const char *name;
    } resolveMethods[] = {
        {ResolveMethod::CMD, "resolve_cmd"},
        {ResolveMethod::RENDER_PASS, "render_pass_resolve"},
        {ResolveMethod::DYNAMIC_RENDER, "dynamic_render_resolve"},
    };

    // Test a mix of formats with varying bit widths, numeric types and total size.
    const VkFormat formatList[] = {
        VK_FORMAT_R8_UNORM,
        VK_FORMAT_R8_UINT,
        VK_FORMAT_R8_SINT,
        VK_FORMAT_R8_SRGB,
        VK_FORMAT_R8G8_UNORM,
        VK_FORMAT_R8G8_UINT,
        VK_FORMAT_R8G8_SINT,
        VK_FORMAT_R8G8_SRGB,
        VK_FORMAT_R8G8B8_UNORM,
        VK_FORMAT_R8G8B8_UINT,
        VK_FORMAT_R8G8B8_SINT,
        VK_FORMAT_R8G8B8_SRGB,
        VK_FORMAT_B8G8R8_UNORM,
        VK_FORMAT_B8G8R8_UINT,
        VK_FORMAT_B8G8R8_SINT,
        VK_FORMAT_B8G8R8_SRGB,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UINT,
        VK_FORMAT_R8G8B8A8_SINT,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_B8G8R8A8_UINT,
        VK_FORMAT_B8G8R8A8_SINT,
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_A8B8G8R8_UNORM_PACK32,
        VK_FORMAT_A8B8G8R8_UINT_PACK32,
        VK_FORMAT_A8B8G8R8_SINT_PACK32,
        VK_FORMAT_A8B8G8R8_SRGB_PACK32,
        //VK_FORMAT_A2R10G10B10_UNORM_PACK32,
        //VK_FORMAT_A2R10G10B10_UINT_PACK32,
        //VK_FORMAT_A2R10G10B10_SINT_PACK32,
        //VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        VK_FORMAT_R16_UNORM,
        VK_FORMAT_R16_UINT,
        VK_FORMAT_R16_SINT,
        VK_FORMAT_R16_SFLOAT,
        VK_FORMAT_R16G16_UNORM,
        VK_FORMAT_R16G16_UINT,
        VK_FORMAT_R16G16_SINT,
        VK_FORMAT_R16G16_SFLOAT,
        VK_FORMAT_R16G16B16_UNORM,
        VK_FORMAT_R16G16B16_UINT,
        VK_FORMAT_R16G16B16_SINT,
        VK_FORMAT_R16G16B16A16_UNORM,
        VK_FORMAT_R16G16B16A16_UINT,
        VK_FORMAT_R16G16B16A16_SINT,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_FORMAT_R32_UINT,
        VK_FORMAT_R32_SINT,
        VK_FORMAT_R32_SFLOAT,
        VK_FORMAT_R32G32B32A32_UINT,
        VK_FORMAT_R32G32B32A32_SINT,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
    };

    const struct
    {
        VkImageAspectFlags resolveAspects;
        const char *name;
    } resolveAspectsCases[] = {
        {VK_IMAGE_ASPECT_COLOR_BIT, "color"},
        {VK_IMAGE_ASPECT_DEPTH_BIT, "depth"},
        {VK_IMAGE_ASPECT_STENCIL_BIT, "stencil"},
        {(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT), "depth_stencil"},
    };

    const struct
    {
        VkResolveModeFlagBits resolveMode;
        const char *name;
    } resolveModeCases[] = {
        {VK_RESOLVE_MODE_AVERAGE_BIT, "average"},
        {VK_RESOLVE_MODE_SAMPLE_ZERO_BIT, "sample_zero"},
        {VK_RESOLVE_MODE_MIN_BIT, "min"},
        {VK_RESOLVE_MODE_MAX_BIT, "max"},
    };

    const struct
    {
        ResolveArea resolveArea;
        const char *name;
    } resolveAreaCases[] = {
        {ResolveArea::FULL, "full"},
        {ResolveArea::FULL_MULTILAYER, "full_multilayer"},
        {ResolveArea::REGION, "region"},
        {ResolveArea::REGIONS_MULTILAYER, "regions_multilayer"},
    };

    const struct
    {
        SRGBFlags sRGBFlags;
        const char *name;
    } srgbFlagsCases[] = {
        {SRGBFlags::NONE, "no_flags"},
        {SRGBFlags::ENABLE, "enable_transfer"},
        {SRGBFlags::SKIP, "skip_transfer"},
    };

    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "m10_resolve"));

    for (const auto &resolveMethodCase : resolveMethods)
    {
        // When using shader objects, we cannot resolve using render passes.
        if (isConstructionTypeShaderObject(pipelineConstructionType) &&
            resolveMethodCase.resolveMethod == ResolveMethod::RENDER_PASS)
            continue;

        GroupPtr methodGrp(new tcu::TestCaseGroup(testCtx, resolveMethodCase.name));

        for (const auto &format : formatList)
        {
            const auto tcuFormat  = mapVkFormat(format);
            const auto formatName = getFormatSimpleName(format);
            const bool isSRGB     = isSrgbFormat(format);

            // For dynamic rendering and render passes we're only interested in testing the new flags, so we will only use sRGB formats.
            if ((resolveMethodCase.resolveMethod == ResolveMethod::DYNAMIC_RENDER ||
                 resolveMethodCase.resolveMethod == ResolveMethod::RENDER_PASS) &&
                !isSRGB)
                continue;

            GroupPtr formatGrp(new tcu::TestCaseGroup(testCtx, formatName.c_str()));

            for (const auto &resolveAspects : resolveAspectsCases)
            {
                const bool resolveColor   = (resolveAspects.resolveAspects & VK_IMAGE_ASPECT_COLOR_BIT);
                const bool resolveDepth   = (resolveAspects.resolveAspects & VK_IMAGE_ASPECT_DEPTH_BIT);
                const bool resolveStencil = (resolveAspects.resolveAspects & VK_IMAGE_ASPECT_STENCIL_BIT);

                // Skip cases that make no sense for the resolve aspect.
                if (isDepthStencilFormat(format))
                {
                    if (resolveColor)
                        continue;

                    if (resolveDepth && !tcu::hasDepthComponent(tcuFormat.order))
                        continue;

                    if (resolveStencil && !tcu::hasStencilComponent(tcuFormat.order))
                        continue;
                }
                else // Color format.
                {
                    if (resolveDepth || resolveStencil)
                        continue;
                }

                GroupPtr aspectGrp(new tcu::TestCaseGroup(testCtx, resolveAspects.name));

                for (const auto &resolveModeCase : resolveModeCases)
                {
                    if (isDepthStencilFormat(format))
                    {
                        // Spec 1.4.317 2025-06-24 for supportedStencilResolveModes:
                        // "VK_RESOLVE_MODE_AVERAGE_BIT must not be included in the set"
                        if (resolveStencil && resolveModeCase.resolveMode == VK_RESOLVE_MODE_AVERAGE_BIT)
                            continue;
                    }
                    else // Color format.
                    {
                        if (isIntFormat(format) || isUintFormat(format))
                        {
                            // VUID-VkRenderingAttachmentInfo-imageView-06130 and others.
                            if (resolveModeCase.resolveMode != VK_RESOLVE_MODE_SAMPLE_ZERO_BIT)
                                continue;
                        }
                        else // Floating point formats
                        {
                            // VUID-VkRenderingAttachmentInfo-imageView-06129 and others.
                            if (resolveModeCase.resolveMode != VK_RESOLVE_MODE_AVERAGE_BIT)
                                continue;
                        }
                    }

                    GroupPtr resolveModeGrp(new tcu::TestCaseGroup(testCtx, resolveModeCase.name));

                    for (const auto &resolveArea : resolveAreaCases)
                    {
                        // Sub-area resolve can only be used with the resolve command.
                        if ((resolveArea.resolveArea == ResolveArea::REGION ||
                             resolveArea.resolveArea == ResolveArea::REGIONS_MULTILAYER) &&
                            resolveMethodCase.resolveMethod != ResolveMethod::CMD)
                            continue;

                        GroupPtr resolveAreaGrp(new tcu::TestCaseGroup(testCtx, resolveArea.name));

                        for (const auto &srgbFlagsCase : srgbFlagsCases)
                        {
                            if (srgbFlagsCase.sRGBFlags != SRGBFlags::NONE &&
                                resolveModeCase.resolveMode != VK_RESOLVE_MODE_AVERAGE_BIT)
                                continue;

                            // We cannot use the flags if it's not an sRGB format.
                            if (srgbFlagsCase.sRGBFlags != SRGBFlags::NONE && !isSRGB)
                                continue;

                            const TestParams params{
                                // clang-format off
                                pipelineConstructionType,
                                resolveMethodCase.resolveMethod,
                                format,
                                resolveAspects.resolveAspects,
                                resolveModeCase.resolveMode,
                                resolveArea.resolveArea,
                                srgbFlagsCase.sRGBFlags,
                                // clang-format on
                            };
                            resolveAreaGrp->addChild(new Maint10ResolveCase(testCtx, srgbFlagsCase.name, params));
                        }

                        resolveModeGrp->addChild(resolveAreaGrp.release());
                    }

                    aspectGrp->addChild(resolveModeGrp.release());
                }

                formatGrp->addChild(aspectGrp.release());
            }

            methodGrp->addChild(formatGrp.release());
        }

        mainGroup->addChild(methodGrp.release());
    }

    return mainGroup.release();
}

} // namespace pipeline
} // namespace vkt
