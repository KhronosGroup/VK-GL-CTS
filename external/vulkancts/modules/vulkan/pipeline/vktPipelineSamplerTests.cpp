/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Sampler Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineSamplerTests.hpp"
#include "vktPipelineImageSamplingInstance.hpp"
#ifndef CTS_USES_VULKANSC
#include "vktPipelineSamplerBorderSwizzleTests.hpp"
#endif // CTS_USES_VULKANSC
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktTestCase.hpp"

#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkPrograms.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"

#include "tcuPlatform.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuMaybe.hpp"
#include "tcuImageCompare.hpp"

#include "deStringUtil.hpp"
#include "deMemory.h"

#include <iomanip>
#include <sstream>
#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <algorithm>

namespace vkt
{
namespace pipeline
{

using namespace vk;
using de::MovePtr;

namespace
{

class SamplerViewType
{
public:
    SamplerViewType(vk::VkImageViewType type, bool normalized = true) : m_viewType(type), m_normalized(normalized)
    {
        if (!normalized)
            DE_ASSERT(type == vk::VK_IMAGE_VIEW_TYPE_2D || type == vk::VK_IMAGE_VIEW_TYPE_1D);
    }

    operator vk::VkImageViewType() const
    {
        return m_viewType;
    }

    bool isNormalized() const
    {
        return m_normalized;
    }

private:
    vk::VkImageViewType m_viewType;
    bool m_normalized;
};

class SamplerTest : public vkt::TestCase
{
public:
    SamplerTest(tcu::TestContext &testContext, const char *name, PipelineConstructionType pipelineConstructionType,
                SamplerViewType imageViewType, VkFormat imageFormat, int imageSize, float samplerLod,
                bool separateStencilUsage, bool sampleStencil);
    virtual ~SamplerTest(void)
    {
    }

    virtual ImageSamplingInstanceParams getImageSamplingInstanceParams(SamplerViewType imageViewType,
                                                                       VkFormat imageFormat, int imageSize,
                                                                       float samplerLod, bool separateStencilUsage,
                                                                       bool sampleStencil) const;

    tcu::Vec4 swizzle(tcu::Vec4 inputData, VkComponentMapping componentMapping, float zeroOrOneValue) const;
    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual void checkSupport(Context &context) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual tcu::UVec2 getRenderSize(SamplerViewType viewType) const;
    virtual std::vector<Vertex4Tex4> createVertices(void) const;
    virtual VkSamplerCreateInfo getSamplerCreateInfo(void) const;
    virtual VkComponentMapping getComponentMapping(void) const;

    static std::string getGlslSamplerType(const tcu::TextureFormat &format, SamplerViewType type, bool sampleStencil);
    static tcu::IVec3 getImageSize(SamplerViewType viewType, int size);
    static int getArraySize(SamplerViewType viewType);

protected:
    PipelineConstructionType m_pipelineConstructionType;
    SamplerViewType m_imageViewType;
    VkFormat m_imageFormat;
    int m_imageSize;
    float m_samplerLod;
    bool m_separateStencilUsage;
    bool m_sampleStencil;
};

class SamplerMagFilterTest : public SamplerTest
{
public:
    SamplerMagFilterTest(tcu::TestContext &testContext, const char *name,
                         PipelineConstructionType pipelineConstructionType, SamplerViewType imageViewType,
                         VkFormat imageFormat, VkFilter magFilter, bool separateStencilUsage);
    virtual ~SamplerMagFilterTest(void)
    {
    }
    virtual VkSamplerCreateInfo getSamplerCreateInfo(void) const;

private:
    VkFilter m_magFilter;
};

class SamplerMinFilterTest : public SamplerTest
{
public:
    SamplerMinFilterTest(tcu::TestContext &testContext, const char *name,
                         PipelineConstructionType pipelineConstructionType, SamplerViewType imageViewType,
                         VkFormat imageFormat, VkFilter minFilter, bool separateStencilUsage);
    virtual ~SamplerMinFilterTest(void)
    {
    }
    virtual VkSamplerCreateInfo getSamplerCreateInfo(void) const;

private:
    VkFilter m_minFilter;
};

class SamplerMagReduceFilterTest : public SamplerMagFilterTest
{
public:
    SamplerMagReduceFilterTest(tcu::TestContext &testContext, const char *name,
                               PipelineConstructionType pipelineConstructionType, SamplerViewType imageViewType,
                               VkFormat imageFormat, VkComponentMapping componentMapping,
                               VkSamplerReductionMode reductionMode, bool separateStencilUsage);

    virtual ~SamplerMagReduceFilterTest(void)
    {
    }
    virtual VkSamplerCreateInfo getSamplerCreateInfo(void) const;
    virtual VkComponentMapping getComponentMapping(void) const;

private:
    const VkSamplerReductionModeCreateInfo m_reductionCreaterInfo;
    VkComponentMapping m_componentMapping;
};

class SamplerMinReduceFilterTest : public SamplerMinFilterTest
{
public:
    SamplerMinReduceFilterTest(tcu::TestContext &testContext, const char *name,
                               PipelineConstructionType pipelineConstructionType, SamplerViewType imageViewType,
                               VkFormat imageFormat, VkComponentMapping componentMapping,
                               VkSamplerReductionMode reductionMode, bool separateStencilUsage);

    virtual ~SamplerMinReduceFilterTest(void)
    {
    }
    virtual VkSamplerCreateInfo getSamplerCreateInfo(void) const;
    virtual VkComponentMapping getComponentMapping(void) const;

private:
    const VkSamplerReductionModeCreateInfo m_reductionCreaterInfo;
    VkComponentMapping m_componentMapping;
};

class SamplerLodTest : public SamplerTest
{
public:
    SamplerLodTest(tcu::TestContext &testContext, const char *name, PipelineConstructionType pipelineConstructionType,
                   SamplerViewType imageViewType, VkFormat imageFormat, VkSamplerMipmapMode mipmapMode, float minLod,
                   float maxLod, float mipLodBias, float samplerLod, bool separateStencilUsage);
    virtual ~SamplerLodTest(void)
    {
    }
    virtual VkSamplerCreateInfo getSamplerCreateInfo(void) const;
    virtual void checkSupport(Context &context) const;

private:
    VkSamplerMipmapMode m_mipmapMode;
    float m_minLod;
    float m_maxLod;
    float m_mipLodBias;
};

void SamplerLodTest::checkSupport(Context &context) const
{
    SamplerTest::checkSupport(context);

#ifndef CTS_USES_VULKANSC
    if (m_mipLodBias != 0.0f && context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
        !context.getPortabilitySubsetFeatures().samplerMipLodBias)
    {
        TCU_THROW(NotSupportedError,
                  "VK_KHR_portability_subset: Sampler mipmap LOD bias is not supported by this implementation");
    }
#endif // CTS_USES_VULKANSC
}

class SamplerAddressModesTest : public SamplerTest
{
public:
    SamplerAddressModesTest(tcu::TestContext &testContext, const char *name,
                            PipelineConstructionType pipelineConstructionType, SamplerViewType imageViewType,
                            VkFormat imageFormat, VkSamplerAddressMode addressU, VkSamplerAddressMode addressV,
                            VkSamplerAddressMode addressW, VkBorderColor borderColor,
                            rr::GenericVec4 customBorderColorValue, bool customBorderColorFormatless,
                            bool separateStencilUsage, bool sampleStencil);
    virtual ~SamplerAddressModesTest(void)
    {
    }
    virtual tcu::UVec2 getRenderSize(SamplerViewType viewType) const;
    virtual std::vector<Vertex4Tex4> createVertices(void) const;
    virtual VkSamplerCreateInfo getSamplerCreateInfo(void) const;

    VkSamplerCustomBorderColorCreateInfoEXT getSamplerCustomBorderColorCreateInfo(
        VkFormat format, rr::GenericVec4 customBorderColorValue, bool customBorderColorFormatless) const;

private:
    VkSamplerAddressMode m_addressU;
    VkSamplerAddressMode m_addressV;
    VkSamplerAddressMode m_addressW;
    VkBorderColor m_borderColor;

    const VkSamplerCustomBorderColorCreateInfoEXT m_customBorderColorCreateInfo;
};

// SamplerTest

SamplerTest::SamplerTest(tcu::TestContext &testContext, const char *name,
                         PipelineConstructionType pipelineConstructionType, SamplerViewType imageViewType,
                         VkFormat imageFormat, int imageSize, float samplerLod, bool separateStencilUsage,
                         bool sampleStencil)
    : vkt::TestCase(testContext, name)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_imageViewType(imageViewType)
    , m_imageFormat(imageFormat)
    , m_imageSize(imageSize)
    , m_samplerLod(samplerLod)
    , m_separateStencilUsage(separateStencilUsage)
    , m_sampleStencil(sampleStencil)
{
    // Can't do both at the same time with the current code.
    DE_ASSERT(!separateStencilUsage || !sampleStencil);
}

ImageSamplingInstanceParams SamplerTest::getImageSamplingInstanceParams(SamplerViewType imageViewType,
                                                                        VkFormat imageFormat, int imageSize,
                                                                        float samplerLod, bool separateStencilUsage,
                                                                        bool sampleStencil) const
{
    const tcu::UVec2 renderSize               = getRenderSize(imageViewType);
    const std::vector<Vertex4Tex4> vertices   = createVertices();
    const VkSamplerCreateInfo samplerParams   = getSamplerCreateInfo();
    const VkComponentMapping componentMapping = getComponentMapping();

    const auto isDSFormat  = isDepthStencilFormat(imageFormat);
    const auto tcuFormat   = (isDSFormat ? mapVkFormat(imageFormat) : tcu::TextureFormat());
    const auto hasDepth    = (isDSFormat && tcu::hasDepthComponent(tcuFormat.order));
    const auto hasStencil  = (isDSFormat && tcu::hasStencilComponent(tcuFormat.order));
    const auto imageAspect = (isDSFormat ? (sampleStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT) :
                                           VK_IMAGE_ASPECT_COLOR_BIT);
    const uint32_t mipLevels =
        (imageViewType.isNormalized() ? static_cast<uint32_t>(deLog2Floor32(imageSize)) + 1u : 1u);

    DE_UNREF(hasDepth);   // For debug builds.
    DE_UNREF(hasStencil); // For debug builds.
    DE_ASSERT(imageAspect != VK_IMAGE_ASPECT_DEPTH_BIT || hasDepth);
    DE_ASSERT(imageAspect != VK_IMAGE_ASPECT_STENCIL_BIT || hasStencil);

    const VkImageSubresourceRange subresourceRange = {
        (VkImageAspectFlags)imageAspect,                   // VkImageAspectFlags aspectMask;
        0u,                                                // uint32_t baseMipLevel;
        mipLevels,                                         // uint32_t mipLevels;
        0u,                                                // uint32_t baseArrayLayer;
        (uint32_t)SamplerTest::getArraySize(imageViewType) // uint32_t arraySize;
    };

    return ImageSamplingInstanceParams(m_pipelineConstructionType, renderSize, imageViewType, imageFormat,
                                       getImageSize(imageViewType, imageSize), getArraySize(imageViewType),
                                       componentMapping, subresourceRange, samplerParams, samplerLod, vertices,
                                       separateStencilUsage);
}

void SamplerTest::checkSupport(Context &context) const
{
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_pipelineConstructionType);
#ifndef CTS_USES_VULKANSC
    if (m_imageFormat == VK_FORMAT_A8_UNORM_KHR || m_imageFormat == VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR)
        context.requireDeviceFunctionality("VK_KHR_maintenance5");
#endif // CTS_USES_VULKANSC
    checkSupportImageSamplingInstance(context, getImageSamplingInstanceParams(m_imageViewType, m_imageFormat,
                                                                              m_imageSize, m_samplerLod,
                                                                              m_separateStencilUsage, m_sampleStencil));
}

tcu::Vec4 SamplerTest::swizzle(tcu::Vec4 inputData, VkComponentMapping componentMapping, float zeroOrOneValue) const
{
    // Remove VK_COMPONENT_SWIZZLE_IDENTITY to avoid addressing channelValues[0]
    const vk::VkComponentMapping nonIdentityMapping = {
        componentMapping.r == VK_COMPONENT_SWIZZLE_IDENTITY ? VK_COMPONENT_SWIZZLE_R : componentMapping.r,
        componentMapping.g == VK_COMPONENT_SWIZZLE_IDENTITY ? VK_COMPONENT_SWIZZLE_G : componentMapping.g,
        componentMapping.b == VK_COMPONENT_SWIZZLE_IDENTITY ? VK_COMPONENT_SWIZZLE_B : componentMapping.b,
        componentMapping.a == VK_COMPONENT_SWIZZLE_IDENTITY ? VK_COMPONENT_SWIZZLE_A : componentMapping.a

    };
    // array map with enum VkComponentSwizzle
    const float channelValues[] = {-1.0f,          // impossible
                                   zeroOrOneValue, // SWIZZLE_0
                                   zeroOrOneValue, // SWIZZLE_1
                                   inputData.x(),  inputData.y(), inputData.z(), inputData.w(), -1.0f};

    return tcu::Vec4(channelValues[nonIdentityMapping.r], channelValues[nonIdentityMapping.g],
                     channelValues[nonIdentityMapping.b], channelValues[nonIdentityMapping.a]);
}

void SamplerTest::initPrograms(SourceCollections &sourceCollections) const
{
    std::ostringstream vertexSrc;
    std::ostringstream fragmentSrc;
    const char *texCoordSwizzle = nullptr;
    tcu::TextureFormat format   = (isCompressedFormat(m_imageFormat)) ?
                                      tcu::getUncompressedFormat(mapVkCompressedFormat(m_imageFormat)) :
                                      mapVkFormat(m_imageFormat);
    tcu::Vec4 lookupScale;
    tcu::Vec4 lookupBias;

    getLookupScaleBias(m_imageFormat, lookupScale, lookupBias, m_sampleStencil);

    tcu::Vec4 swizzledScale = swizzle(lookupScale, getComponentMapping(), 1.0f);
    tcu::Vec4 swizzledBias  = swizzle(lookupBias, getComponentMapping(), 0.0f);

    switch (m_imageViewType)
    {
    case VK_IMAGE_VIEW_TYPE_1D:
        texCoordSwizzle = "x";
        break;
    case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
    case VK_IMAGE_VIEW_TYPE_2D:
        texCoordSwizzle = "xy";
        break;
    case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
    case VK_IMAGE_VIEW_TYPE_3D:
    case VK_IMAGE_VIEW_TYPE_CUBE:
        texCoordSwizzle = "xyz";
        break;
    case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
        texCoordSwizzle = "xyzw";
        break;
    default:
        DE_ASSERT(false);
        break;
    }

    vertexSrc << "#version 440\n"
              << "layout(location = 0) in vec4 position;\n"
              << "layout(location = 1) in vec4 texCoords;\n"
              << "layout(location = 0) out highp vec4 vtxTexCoords;\n"
              << "out gl_PerVertex {\n"
              << "    vec4 gl_Position;\n"
              << "};\n"
              << "void main (void)\n"
              << "{\n"
              << "    gl_Position = position;\n"
              << "    vtxTexCoords = texCoords;\n"
              << "}\n";

    fragmentSrc << "#version 440\n"
                << "layout(set = 0, binding = 0) uniform highp "
                << getGlslSamplerType(format, m_imageViewType, m_sampleStencil) << " texSampler;\n"
                << "layout(location = 0) in highp vec4 vtxTexCoords;\n"
                << "layout(location = 0) out highp vec4 fragColor;\n"
                << "void main (void)\n"
                << "{\n"
                << "    fragColor = ";

    if (m_samplerLod > 0.0f || !m_imageViewType.isNormalized())
    {
        DE_ASSERT(m_imageViewType.isNormalized() || (m_samplerLod == 0.0f && !m_imageViewType.isNormalized()));
        fragmentSrc << "textureLod(texSampler, vtxTexCoords." << texCoordSwizzle << ", " << std::fixed << m_samplerLod
                    << ")";
    }
    else
    {
        fragmentSrc << "texture(texSampler, vtxTexCoords." << texCoordSwizzle << ")" << std::fixed;
    }

    fragmentSrc << " * vec4" << std::scientific << swizzledScale << " + vec4" << swizzledBias << ";\n"
                << "}\n";

    sourceCollections.glslSources.add("tex_vert") << glu::VertexSource(vertexSrc.str());
    sourceCollections.glslSources.add("tex_frag") << glu::FragmentSource(fragmentSrc.str());
}

TestInstance *SamplerTest::createInstance(Context &context) const
{
    return new ImageSamplingInstance(context, getImageSamplingInstanceParams(m_imageViewType, m_imageFormat,
                                                                             m_imageSize, m_samplerLod,
                                                                             m_separateStencilUsage, m_sampleStencil));
}

tcu::UVec2 SamplerTest::getRenderSize(SamplerViewType viewType) const
{
    if (viewType == VK_IMAGE_VIEW_TYPE_1D || viewType == VK_IMAGE_VIEW_TYPE_2D)
    {
        return tcu::UVec2(16u, 16u);
    }
    else
    {
        return tcu::UVec2(16u * 3u, 16u * 2u);
    }
}

std::vector<Vertex4Tex4> SamplerTest::createVertices(void) const
{
    std::vector<Vertex4Tex4> vertices = createTestQuadMosaic(m_imageViewType);
    // Adjust texture coordinate to avoid doing NEAREST filtering exactly on texel boundaries.
    // TODO: Would be nice to base this on number of texels and subtexel precision. But this
    // seems to work.
    for (unsigned int i = 0; i < vertices.size(); ++i)
    {
        vertices[i].texCoord += tcu::Vec4(0.002f, 0.002f, 0.002f, 0.0f);
        if (!m_imageViewType.isNormalized())
        {
            const float imageSize = static_cast<float>(m_imageSize);
            for (int j = 0; j < tcu::Vec4::SIZE; ++j)
                vertices[i].texCoord[j] *= imageSize;
        }
    }
    return vertices;
}

VkSamplerCreateInfo SamplerTest::getSamplerCreateInfo(void) const
{
    const VkSamplerCreateInfo defaultSamplerParams = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,           // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        0u,                                              // VkSamplerCreateFlags flags;
        VK_FILTER_NEAREST,                               // VkFilter magFilter;
        VK_FILTER_NEAREST,                               // VkFilter minFilter;
        VK_SAMPLER_MIPMAP_MODE_NEAREST,                  // VkSamplerMipmapMode mipmapMode;
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,           // VkSamplerAddressMode addressModeU;
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,           // VkSamplerAddressMode addressModeV;
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,           // VkSamplerAddressMode addressModeW;
        0.0f,                                            // float mipLodBias;
        VK_FALSE,                                        // VkBool32 anisotropyEnable;
        1.0f,                                            // float maxAnisotropy;
        false,                                           // VkBool32 compareEnable;
        VK_COMPARE_OP_NEVER,                             // VkCompareOp compareOp;
        0.0f,                                            // float minLod;
        (m_imageViewType.isNormalized() ? 0.25f : 0.0f), // float maxLod;
        getFormatBorderColor(BORDER_COLOR_TRANSPARENT_BLACK, m_imageFormat,
                             m_sampleStencil), // VkBorderColor borderColor;
        !m_imageViewType.isNormalized(),       // VkBool32 unnormalizedCoordinates;
    };

    return defaultSamplerParams;
}

VkComponentMapping SamplerTest::getComponentMapping(void) const
{
    const VkComponentMapping componentMapping = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
                                                 VK_COMPONENT_SWIZZLE_A};
    return componentMapping;
}

std::string SamplerTest::getGlslSamplerType(const tcu::TextureFormat &format, SamplerViewType type, bool sampleStencil)
{
    std::ostringstream samplerType;

    if (tcu::getTextureChannelClass(format.type) == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER || sampleStencil)
        samplerType << "u";
    else if (tcu::getTextureChannelClass(format.type) == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER)
        samplerType << "i";

    switch (type)
    {
    case VK_IMAGE_VIEW_TYPE_1D:
        samplerType << "sampler1D";
        break;

    case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
        samplerType << "sampler1DArray";
        break;

    case VK_IMAGE_VIEW_TYPE_2D:
        samplerType << "sampler2D";
        break;

    case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
        samplerType << "sampler2DArray";
        break;

    case VK_IMAGE_VIEW_TYPE_3D:
        samplerType << "sampler3D";
        break;

    case VK_IMAGE_VIEW_TYPE_CUBE:
        samplerType << "samplerCube";
        break;

    case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
        samplerType << "samplerCubeArray";
        break;

    default:
        DE_FATAL("Unknown image view type");
        break;
    }

    return samplerType.str();
}

tcu::IVec3 SamplerTest::getImageSize(SamplerViewType viewType, int size)
{
    switch (viewType)
    {
    case VK_IMAGE_VIEW_TYPE_1D:
    case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
        return tcu::IVec3(size, 1, 1);

    case VK_IMAGE_VIEW_TYPE_3D:
        return tcu::IVec3(size, size, 4);

    default:
        break;
    }

    return tcu::IVec3(size, size, 1);
}

int SamplerTest::getArraySize(SamplerViewType viewType)
{
    switch (viewType)
    {
    case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
    case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
    case VK_IMAGE_VIEW_TYPE_CUBE:
        return 6;

    case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
        return 36;

    default:
        break;
    }

    return 1;
}

// SamplerMagFilterTest

SamplerMagFilterTest::SamplerMagFilterTest(tcu::TestContext &testContext, const char *name,
                                           PipelineConstructionType pipelineConstructionType,
                                           SamplerViewType imageViewType, VkFormat imageFormat, VkFilter magFilter,
                                           bool separateStencilUsage)
    : SamplerTest(testContext, name, pipelineConstructionType, imageViewType, imageFormat, 8, 0.0f,
                  separateStencilUsage, false)
    , m_magFilter(magFilter)
{
}

VkSamplerCreateInfo SamplerMagFilterTest::getSamplerCreateInfo(void) const
{
    VkSamplerCreateInfo samplerParams = SamplerTest::getSamplerCreateInfo();
    samplerParams.magFilter           = m_magFilter;

    return samplerParams;
}

// SamplerMinFilterTest

SamplerMinFilterTest::SamplerMinFilterTest(tcu::TestContext &testContext, const char *name,
                                           PipelineConstructionType pipelineConstructionType,
                                           SamplerViewType imageViewType, VkFormat imageFormat, VkFilter minFilter,
                                           bool separateStencilUsage)
    : SamplerTest(testContext, name, pipelineConstructionType, imageViewType, imageFormat, 32, 0.0f,
                  separateStencilUsage, false)
    , m_minFilter(minFilter)
{
}

VkSamplerCreateInfo SamplerMinFilterTest::getSamplerCreateInfo(void) const
{
    VkSamplerCreateInfo samplerParams = SamplerTest::getSamplerCreateInfo();
    samplerParams.minFilter           = m_minFilter;
    // set minLod to epsilon, to force use of the minFilter
    samplerParams.minLod = 0.01f;

    return samplerParams;
}

namespace
{

VkSamplerReductionModeCreateInfo getSamplerReductionCreateInfo(VkSamplerReductionMode reductionMode)
{
    const VkSamplerReductionModeCreateInfo ret = {
        VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO, // VkStructureType                sType
        nullptr,                                              // const void*                    pNext
        reductionMode                                         // VkSamplerReductionMode        reductionMode
    };
    return ret;
}

} // namespace

// SamplerMagReduceFilterTest

SamplerMagReduceFilterTest::SamplerMagReduceFilterTest(tcu::TestContext &testContext, const char *name,
                                                       PipelineConstructionType pipelineConstructionType,
                                                       SamplerViewType imageViewType, VkFormat imageFormat,
                                                       VkComponentMapping componentMapping,
                                                       VkSamplerReductionMode reductionMode, bool separateStencilUsage)
    : SamplerMagFilterTest(testContext, name, pipelineConstructionType, imageViewType, imageFormat, VK_FILTER_LINEAR,
                           separateStencilUsage)
    , m_reductionCreaterInfo(getSamplerReductionCreateInfo(reductionMode))
    , m_componentMapping(componentMapping)
{
}

VkSamplerCreateInfo SamplerMagReduceFilterTest::getSamplerCreateInfo(void) const
{
    VkSamplerCreateInfo samplerParams = SamplerMagFilterTest::getSamplerCreateInfo();

    samplerParams.pNext = &m_reductionCreaterInfo;

    return samplerParams;
}

VkComponentMapping SamplerMagReduceFilterTest::getComponentMapping(void) const
{
    return m_componentMapping;
}

// SamplerMinReduceFilterTest

SamplerMinReduceFilterTest::SamplerMinReduceFilterTest(tcu::TestContext &testContext, const char *name,
                                                       PipelineConstructionType pipelineConstructionType,
                                                       SamplerViewType imageViewType, VkFormat imageFormat,
                                                       VkComponentMapping componentMapping,
                                                       VkSamplerReductionMode reductionMode, bool separateStencilUsage)
    : SamplerMinFilterTest(testContext, name, pipelineConstructionType, imageViewType, imageFormat, VK_FILTER_LINEAR,
                           separateStencilUsage)
    , m_reductionCreaterInfo(getSamplerReductionCreateInfo(reductionMode))
    , m_componentMapping(componentMapping)
{
}

VkSamplerCreateInfo SamplerMinReduceFilterTest::getSamplerCreateInfo(void) const
{
    VkSamplerCreateInfo samplerParams = SamplerMinFilterTest::getSamplerCreateInfo();

    samplerParams.pNext = &m_reductionCreaterInfo;

    return samplerParams;
}

VkComponentMapping SamplerMinReduceFilterTest::getComponentMapping(void) const
{
    return m_componentMapping;
}

// SamplerLodTest

SamplerLodTest::SamplerLodTest(tcu::TestContext &testContext, const char *name,
                               PipelineConstructionType pipelineConstructionType, SamplerViewType imageViewType,
                               VkFormat imageFormat, VkSamplerMipmapMode mipmapMode, float minLod, float maxLod,
                               float mipLodBias, float samplerLod, bool separateStencilUsage)
    : SamplerTest(testContext, name, pipelineConstructionType, imageViewType, imageFormat, 32, samplerLod,
                  separateStencilUsage, false)
    , m_mipmapMode(mipmapMode)
    , m_minLod(minLod)
    , m_maxLod(maxLod)
    , m_mipLodBias(mipLodBias)
{
}

VkSamplerCreateInfo SamplerLodTest::getSamplerCreateInfo(void) const
{
    VkSamplerCreateInfo samplerParams = SamplerTest::getSamplerCreateInfo();

    samplerParams.mipmapMode = m_mipmapMode;
    samplerParams.minLod     = m_minLod;
    samplerParams.maxLod     = m_maxLod;
    samplerParams.mipLodBias = m_mipLodBias;

    return samplerParams;
}

// SamplerAddressModesTest

SamplerAddressModesTest::SamplerAddressModesTest(
    tcu::TestContext &testContext, const char *name, PipelineConstructionType pipelineConstructionType,
    SamplerViewType imageViewType, VkFormat imageFormat, VkSamplerAddressMode addressU, VkSamplerAddressMode addressV,
    VkSamplerAddressMode addressW, VkBorderColor borderColor, rr::GenericVec4 customBorderColorValue,
    bool customBorderColorFormatless, bool separateStencilUsage, bool sampleStencil)
    : SamplerTest(testContext, name, pipelineConstructionType, imageViewType, imageFormat, 8, 0.0f,
                  separateStencilUsage, sampleStencil)
    , m_addressU(addressU)
    , m_addressV(addressV)
    , m_addressW(addressW)
    , m_borderColor(borderColor)
    , m_customBorderColorCreateInfo(
          getSamplerCustomBorderColorCreateInfo(imageFormat, customBorderColorValue, customBorderColorFormatless))
{
}

tcu::UVec2 SamplerAddressModesTest::getRenderSize(SamplerViewType viewType) const
{
    return 4u * SamplerTest::getRenderSize(viewType);
}

std::vector<Vertex4Tex4> SamplerAddressModesTest::createVertices(void) const
{
    std::vector<Vertex4Tex4> vertices = SamplerTest::createVertices();

    switch (m_imageViewType)
    {
    case VK_IMAGE_VIEW_TYPE_1D:
    case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
        for (size_t vertexNdx = 0; vertexNdx < vertices.size(); vertexNdx++)
            vertices[vertexNdx].texCoord.x() = (vertices[vertexNdx].texCoord.x() - 0.5f) * 4.0f;

        break;

    case VK_IMAGE_VIEW_TYPE_2D:
    case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
        for (size_t vertexNdx = 0; vertexNdx < vertices.size(); vertexNdx++)
            vertices[vertexNdx].texCoord.xy() = (vertices[vertexNdx].texCoord.swizzle(0, 1) - tcu::Vec2(0.5f)) * 4.0f;

        break;

    case VK_IMAGE_VIEW_TYPE_3D:
        for (size_t vertexNdx = 0; vertexNdx < vertices.size(); vertexNdx++)
            vertices[vertexNdx].texCoord.xyz() =
                (vertices[vertexNdx].texCoord.swizzle(0, 1, 2) - tcu::Vec3(0.5f)) * 4.0f;

        break;

    case VK_IMAGE_VIEW_TYPE_CUBE:
    case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
        break;

    default:
        DE_ASSERT(false);
    }

    return vertices;
}

VkSamplerCreateInfo SamplerAddressModesTest::getSamplerCreateInfo(void) const
{
    VkSamplerCreateInfo samplerParams = SamplerTest::getSamplerCreateInfo();
    samplerParams.addressModeU        = m_addressU;
    samplerParams.addressModeV        = m_addressV;
    samplerParams.addressModeW        = m_addressW;
    samplerParams.borderColor         = m_borderColor;

    if (m_borderColor == VK_BORDER_COLOR_FLOAT_CUSTOM_EXT || m_borderColor == VK_BORDER_COLOR_INT_CUSTOM_EXT)
    {
        samplerParams.pNext = &m_customBorderColorCreateInfo;
    }

    return samplerParams;
}

VkSamplerCustomBorderColorCreateInfoEXT SamplerAddressModesTest::getSamplerCustomBorderColorCreateInfo(
    VkFormat format, rr::GenericVec4 customBorderColorValue, bool customBorderColorFormatless) const
{
    const VkSamplerCustomBorderColorCreateInfoEXT defaultSamplerCustomBorderColorParams = {
        VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT, nullptr, mapVkColor(customBorderColorValue),
        customBorderColorFormatless ? VK_FORMAT_UNDEFINED : format};

    return defaultSamplerCustomBorderColorParams;
}

// Utilities to create test nodes

std::string getFormatCaseName(const VkFormat format)
{
    const std::string fullName = getFormatName(format);

    DE_ASSERT(de::beginsWith(fullName, "VK_FORMAT_"));

    return de::toLower(fullName.substr(10));
}

MovePtr<tcu::TestCaseGroup> createSamplerMagFilterTests(tcu::TestContext &testCtx,
                                                        PipelineConstructionType pipelineConstructionType,
                                                        SamplerViewType imageViewType, VkFormat imageFormat,
                                                        bool separateStencilUsage)
{
    // Tests for magnification filter
    MovePtr<tcu::TestCaseGroup> samplerMagFilterTests(new tcu::TestCaseGroup(testCtx, "mag_filter"));

    if (imageViewType.isNormalized() &&
        (isCompressedFormat(imageFormat) || (!isIntFormat(imageFormat) && !isUintFormat(imageFormat))))
        // Magnifies image using VK_FILTER_LINEAR
        samplerMagFilterTests->addChild(new SamplerMagFilterTest(testCtx, "linear", pipelineConstructionType,
                                                                 imageViewType, imageFormat, VK_FILTER_LINEAR,
                                                                 separateStencilUsage));
    // Magnifies image using VK_FILTER_NEAREST
    samplerMagFilterTests->addChild(new SamplerMagFilterTest(testCtx, "nearest", pipelineConstructionType,
                                                             imageViewType, imageFormat, VK_FILTER_NEAREST,
                                                             separateStencilUsage));

    return samplerMagFilterTests;
}

MovePtr<tcu::TestCaseGroup> createSamplerMinFilterTests(tcu::TestContext &testCtx,
                                                        PipelineConstructionType pipelineConstructionType,
                                                        SamplerViewType imageViewType, VkFormat imageFormat,
                                                        bool separateStencilUsage)
{
    // Tests for minification filter
    MovePtr<tcu::TestCaseGroup> samplerMinFilterTests(new tcu::TestCaseGroup(testCtx, "min_filter"));

    if (imageViewType.isNormalized() &&
        (isCompressedFormat(imageFormat) || (!isIntFormat(imageFormat) && !isUintFormat(imageFormat))))
        // Minifies image using VK_FILTER_LINEAR
        samplerMinFilterTests->addChild(new SamplerMinFilterTest(testCtx, "linear", pipelineConstructionType,
                                                                 imageViewType, imageFormat, VK_FILTER_LINEAR,
                                                                 separateStencilUsage));
    // Minifies image using VK_FILTER_NEAREST
    samplerMinFilterTests->addChild(new SamplerMinFilterTest(testCtx, "nearest", pipelineConstructionType,
                                                             imageViewType, imageFormat, VK_FILTER_NEAREST,
                                                             separateStencilUsage));

    return samplerMinFilterTests;
}

const VkComponentMapping reduceFilterComponentMappings[] = {
    // filterMinmaxImageComponentMapping  == false - compatible mappings:
    {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO},
    {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO},

    // other mappings
    {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A},
    {VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_A},
    {VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R},
};

static std::string getShortComponentSwizzleName(VkComponentSwizzle componentSwizzle)
{
    const std::string fullName = getComponentSwizzleName(componentSwizzle);
    const char *prefix         = "VK_COMPONENT_SWIZZLE_";

    DE_ASSERT(de::beginsWith(fullName, prefix));

    return de::toLower(fullName.substr(deStrnlen(prefix, -1)));
}

static std::string getComponentMappingGroupName(const VkComponentMapping &componentMapping)
{
    std::ostringstream name;

    name << "comp_";

    name << getShortComponentSwizzleName(componentMapping.r) << "_" << getShortComponentSwizzleName(componentMapping.g)
         << "_" << getShortComponentSwizzleName(componentMapping.b) << "_"
         << getShortComponentSwizzleName(componentMapping.a);

    return name.str();
}

MovePtr<tcu::TestCaseGroup> createSamplerMagReduceFilterTests(tcu::TestContext &testCtx,
                                                              PipelineConstructionType pipelineConstructionType,
                                                              SamplerViewType imageViewType, VkFormat imageFormat,
                                                              bool separateStencilUsage)
{
    MovePtr<tcu::TestCaseGroup> samplerMagReduceFilterTests(new tcu::TestCaseGroup(testCtx, "mag_reduce"));

    for (size_t i = 0; i < DE_LENGTH_OF_ARRAY(reduceFilterComponentMappings); ++i)
    {
        const VkComponentMapping &mapping = reduceFilterComponentMappings[i];

        MovePtr<tcu::TestCaseGroup> componentGroup(
            new tcu::TestCaseGroup(testCtx, getComponentMappingGroupName(mapping).c_str()));

        if (isCompressedFormat(imageFormat) || (!isIntFormat(imageFormat) && !isUintFormat(imageFormat)))
        {
            // Magnifies image using VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE
            componentGroup->addChild(new SamplerMagReduceFilterTest(
                testCtx, "average", pipelineConstructionType, imageViewType, imageFormat, mapping,
                VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE, separateStencilUsage));
        }
        // Magnifies and reduces image using VK_SAMPLER_REDUCTION_MODE_MIN
        componentGroup->addChild(new SamplerMagReduceFilterTest(testCtx, "min", pipelineConstructionType, imageViewType,
                                                                imageFormat, mapping, VK_SAMPLER_REDUCTION_MODE_MIN,
                                                                separateStencilUsage));
        // Magnifies and reduces image using VK_SAMPLER_REDUCTION_MODE_MAX
        componentGroup->addChild(new SamplerMagReduceFilterTest(testCtx, "max", pipelineConstructionType, imageViewType,
                                                                imageFormat, mapping, VK_SAMPLER_REDUCTION_MODE_MAX,
                                                                separateStencilUsage));
        samplerMagReduceFilterTests->addChild(componentGroup.release());
    }
    return samplerMagReduceFilterTests;
}

MovePtr<tcu::TestCaseGroup> createSamplerMinReduceFilterTests(tcu::TestContext &testCtx,
                                                              PipelineConstructionType pipelineConstructionType,
                                                              SamplerViewType imageViewType, VkFormat imageFormat,
                                                              bool separateStencilUsage)
{
    MovePtr<tcu::TestCaseGroup> samplerMinReduceFilterTests(new tcu::TestCaseGroup(testCtx, "min_reduce"));

    for (size_t i = 0; i < DE_LENGTH_OF_ARRAY(reduceFilterComponentMappings); ++i)
    {
        const VkComponentMapping &mapping = reduceFilterComponentMappings[i];

        MovePtr<tcu::TestCaseGroup> componentGroup(
            new tcu::TestCaseGroup(testCtx, getComponentMappingGroupName(mapping).c_str()));

        if (isCompressedFormat(imageFormat) || (!isIntFormat(imageFormat) && !isUintFormat(imageFormat)))
        {
            // Minifies image using VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE
            componentGroup->addChild(new SamplerMinReduceFilterTest(
                testCtx, "average", pipelineConstructionType, imageViewType, imageFormat, mapping,
                VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE, separateStencilUsage));
        }
        // Minifies and reduces image using VK_SAMPLER_REDUCTION_MODE_MIN
        componentGroup->addChild(new SamplerMinReduceFilterTest(testCtx, "min", pipelineConstructionType, imageViewType,
                                                                imageFormat, mapping, VK_SAMPLER_REDUCTION_MODE_MIN,
                                                                separateStencilUsage));
        // Minifies and reduces image using VK_SAMPLER_REDUCTION_MODE_MAX
        componentGroup->addChild(new SamplerMinReduceFilterTest(testCtx, "max", pipelineConstructionType, imageViewType,
                                                                imageFormat, mapping, VK_SAMPLER_REDUCTION_MODE_MAX,
                                                                separateStencilUsage));
        samplerMinReduceFilterTests->addChild(componentGroup.release());
    }
    return samplerMinReduceFilterTests;
}

MovePtr<tcu::TestCaseGroup> createSamplerLodTests(tcu::TestContext &testCtx,
                                                  PipelineConstructionType pipelineConstructionType,
                                                  SamplerViewType imageViewType, VkFormat imageFormat,
                                                  VkSamplerMipmapMode mipmapMode, bool separateStencilUsage)
{
    struct TestCaseConfig
    {
        const char *name;
        float minLod;
        float maxLod;
        float mipLodBias;
        float lod;
    };

    TestCaseConfig testCaseConfigs[] = {
        // minLod = 3, maxLod = 3, mipLodBias = 0, lod = 0
        {"equal_min_3_max_3", 3.0f, 3.0f, 0.0f, 0.0f},
        // minLod = 1, maxLod = 5, mipLodBias = 0, lod = 0
        {"select_min_1", 1.0f, 5.0f, 0.0f, 0.0f},
        // minLod = 0, maxLod = 4, mipLodBias = 0, lod = 5
        {"select_max_4", 0.0f, 4.0f, 0.0f, 5.0f},
        // minLod = 0, maxLod = 2.1, mipLodBias = 5.0, lod = 0
        {"select_bias_2_1", 0.0f, 2.1f, 5.0f, 0.0f},
        // minLod = 0, maxLod = 5, mipLodBias = 2.5, lod = 0
        {"select_bias_2_5", 0.0f, 5.0f, 2.5f, 0.00001f},
        // minLod = 0, maxLod = 5, mipLodBias = -0.9, lod = 4.0
        {"select_bias_3_1", 0.0f, 5.0f, -0.9f, 4.0f},
        // minLod = 0, maxLod = 5, mipLodBias = 3.0, lod = 0.7
        {"select_bias_3_7", 0.0f, 5.0f, 3.0f, 0.7f},
    };

    MovePtr<tcu::TestCaseGroup> samplerLodTests(new tcu::TestCaseGroup(testCtx, "lod"));

    for (int configNdx = 0; configNdx < DE_LENGTH_OF_ARRAY(testCaseConfigs); configNdx++)
    {
        const TestCaseConfig &config = testCaseConfigs[configNdx];

        samplerLodTests->addChild(new SamplerLodTest(testCtx, config.name, pipelineConstructionType, imageViewType,
                                                     imageFormat, mipmapMode, config.minLod, config.maxLod,
                                                     config.mipLodBias, config.lod, separateStencilUsage));
    }

    return samplerLodTests;
}

MovePtr<tcu::TestCaseGroup> createSamplerMipmapTests(tcu::TestContext &testCtx,
                                                     PipelineConstructionType pipelineConstructionType,
                                                     SamplerViewType imageViewType, VkFormat imageFormat,
                                                     bool separateStencilUsage)
{
    MovePtr<tcu::TestCaseGroup> samplerMipmapTests(new tcu::TestCaseGroup(testCtx, "mipmap"));

    // Mipmap mode: nearest
    MovePtr<tcu::TestCaseGroup> mipmapNearestTests(new tcu::TestCaseGroup(testCtx, "nearest"));
    mipmapNearestTests->addChild(createSamplerLodTests(testCtx, pipelineConstructionType, imageViewType, imageFormat,
                                                       VK_SAMPLER_MIPMAP_MODE_NEAREST, separateStencilUsage)
                                     .release());
    samplerMipmapTests->addChild(mipmapNearestTests.release());

    // Mipmap mode: linear
    if (isCompressedFormat(imageFormat) || (!isIntFormat(imageFormat) && !isUintFormat(imageFormat)))
    {
        MovePtr<tcu::TestCaseGroup> mipmapLinearTests(new tcu::TestCaseGroup(testCtx, "linear"));
        mipmapLinearTests->addChild(createSamplerLodTests(testCtx, pipelineConstructionType, imageViewType, imageFormat,
                                                          VK_SAMPLER_MIPMAP_MODE_LINEAR, separateStencilUsage)
                                        .release());
        samplerMipmapTests->addChild(mipmapLinearTests.release());
    }

    return samplerMipmapTests;
}

std::string getAddressModesCaseName(VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w,
                                    BorderColor border, tcu::IVec4 customIntValue, bool formatless, bool sampleStencil)
{
    static const char *borderColorNames[BORDER_COLOR_COUNT] = {"opaque_black", "opaque_white", "transparent_black",
                                                               "custom"};

    std::ostringstream caseName;

    if (u == v && v == w)
    {
        const std::string fullName = getSamplerAddressModeName(u);
        DE_ASSERT(de::beginsWith(fullName, "VK_SAMPLER_ADDRESS_"));

        caseName << "all_";
        caseName << de::toLower(fullName.substr(19));

        if (u == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
        {
            caseName << "_" << borderColorNames[border];
        }
    }
    else
    {
        const std::string fullNameU = getSamplerAddressModeName(u);
        const std::string fullNameV = getSamplerAddressModeName(v);
        const std::string fullNameW = getSamplerAddressModeName(w);

        DE_ASSERT(de::beginsWith(fullNameU, "VK_SAMPLER_ADDRESS_"));
        DE_ASSERT(de::beginsWith(fullNameV, "VK_SAMPLER_ADDRESS_"));
        DE_ASSERT(de::beginsWith(fullNameW, "VK_SAMPLER_ADDRESS_"));

        caseName << "uvw"
                 << "_" << de::toLower(fullNameU.substr(19)) << "_" << de::toLower(fullNameV.substr(19)) << "_"
                 << de::toLower(fullNameW.substr(19));
    }

    if (border == BORDER_COLOR_CUSTOM)
    {
        caseName << "_";
        for (int i = 0; i < 4; i++)
            caseName << customIntValue[i];

        if (formatless)
            caseName << "_formatless";
    }

    if (sampleStencil)
        caseName << "_stencil";

    return caseName.str();
}

MovePtr<tcu::TestCaseGroup> createSamplerAddressModesTests(tcu::TestContext &testCtx,
                                                           PipelineConstructionType pipelineConstructionType,
                                                           SamplerViewType imageViewType, VkFormat imageFormat,
                                                           bool separateStencilUsage)
{
    struct TestCaseConfig
    {
        TestCaseConfig(VkSamplerAddressMode _u, VkSamplerAddressMode _v, VkSamplerAddressMode _w, BorderColor _border,
                       bool _customColorFormatless = false, tcu::Vec4 _customColorValueFloat = tcu::Vec4(),
                       tcu::IVec4 _customColorValueInt = tcu::IVec4())
            : u(_u)
            , v(_v)
            , w(_w)
            , border(_border)
            , customColorFormatless(_customColorFormatless)
            , customColorValueFloat(_customColorValueFloat)
            , customColorValueInt(_customColorValueInt)
        {
        }

        VkSamplerAddressMode u;
        VkSamplerAddressMode v;
        VkSamplerAddressMode w;
        BorderColor border;
        bool customColorFormatless;
        tcu::Vec4 customColorValueFloat;
        tcu::IVec4 customColorValueInt;
    };

    const TestCaseConfig testCaseConfigs[] = {
        // All address modes equal
        {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, BORDER_COLOR_TRANSPARENT_BLACK},
        {VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT,
         BORDER_COLOR_TRANSPARENT_BLACK},
        {VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
         VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, BORDER_COLOR_TRANSPARENT_BLACK},
        {VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,
         VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE, BORDER_COLOR_TRANSPARENT_BLACK},

        // All address modes equal using border color
        {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, BORDER_COLOR_TRANSPARENT_BLACK},
        {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, BORDER_COLOR_OPAQUE_BLACK},
        {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, BORDER_COLOR_OPAQUE_WHITE},

        // Pairwise combinations of address modes not covered by previous tests
        {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,
         VK_SAMPLER_ADDRESS_MODE_REPEAT, BORDER_COLOR_OPAQUE_WHITE},
        {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
         VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE, BORDER_COLOR_OPAQUE_WHITE},
        {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_REPEAT,
         VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, BORDER_COLOR_OPAQUE_WHITE},
        {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, BORDER_COLOR_OPAQUE_WHITE},
        {VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
         VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, BORDER_COLOR_OPAQUE_WHITE},
        {VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,
         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, BORDER_COLOR_OPAQUE_WHITE},
        {VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
         VK_SAMPLER_ADDRESS_MODE_REPEAT, BORDER_COLOR_OPAQUE_WHITE},
        {VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
         VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE, BORDER_COLOR_OPAQUE_WHITE},
        {VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_REPEAT,
         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, BORDER_COLOR_OPAQUE_WHITE},
        {VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
         VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE, BORDER_COLOR_OPAQUE_WHITE},
        {VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,
         VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE, BORDER_COLOR_OPAQUE_WHITE},
        {VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT,
         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, BORDER_COLOR_OPAQUE_WHITE},
        {VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_REPEAT,
         BORDER_COLOR_OPAQUE_WHITE},
        {VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
         BORDER_COLOR_OPAQUE_WHITE},
        {VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, BORDER_COLOR_OPAQUE_WHITE},
        {VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
         BORDER_COLOR_OPAQUE_WHITE},
        {VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,
         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, BORDER_COLOR_OPAQUE_WHITE},
        {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, BORDER_COLOR_OPAQUE_WHITE},
        {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_REPEAT,
         BORDER_COLOR_OPAQUE_WHITE},
        {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_REPEAT,
         VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE, BORDER_COLOR_OPAQUE_WHITE},
        {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, BORDER_COLOR_OPAQUE_WHITE},
        {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,
         VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, BORDER_COLOR_OPAQUE_WHITE},

        // Custom border color tests
        {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, BORDER_COLOR_CUSTOM, false, tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
         tcu::IVec4(0, 0, 0, 0)},
        {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, BORDER_COLOR_CUSTOM, false, tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f),
         tcu::IVec4(0, 0, 1, 1)},
        {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, BORDER_COLOR_CUSTOM, false, tcu::Vec4(1.0f, 0.0f, 0.0f, 0.0f),
         tcu::IVec4(1, 0, 0, 0)},
        {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, BORDER_COLOR_CUSTOM, false, tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
         tcu::IVec4(1, 0, 0, 1)},
        {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, BORDER_COLOR_CUSTOM, false, tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f),
         tcu::IVec4(1, 0, 1, 1)},
        {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, BORDER_COLOR_CUSTOM, false, tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
         tcu::IVec4(1, 1, 0, 1)},

        // Custom border color formatless
        {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, BORDER_COLOR_CUSTOM, true, tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f),
         tcu::IVec4(1, 0, 1, 1)},
        {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, BORDER_COLOR_CUSTOM, true, tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
         tcu::IVec4(1, 1, 0, 1)},
    };

    MovePtr<tcu::TestCaseGroup> samplerAddressModesTests(new tcu::TestCaseGroup(testCtx, "address_modes"));

    for (int configNdx = 0; configNdx < DE_LENGTH_OF_ARRAY(testCaseConfigs); configNdx++)
    {
        const TestCaseConfig &config = testCaseConfigs[configNdx];

        if (!imageViewType.isNormalized() && ((config.u != VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE &&
                                               config.u != VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER) ||
                                              (config.v != VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE &&
                                               config.v != VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)))
            continue;

        // VK_FORMAT_B4G4R4A4_UNORM_PACK16, VK_FORMAT_B5G6R5_UNORM_PACK16, VK_FORMAT_B5G5R5A1_UNORM_PACK16
        // and VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR are forbidden for non-formatless custom border color.
        if ((imageFormat == VK_FORMAT_B4G4R4A4_UNORM_PACK16 || imageFormat == VK_FORMAT_B5G6R5_UNORM_PACK16 ||
#ifndef CTS_USES_VULKANSC
             imageFormat == VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR ||
#endif
             imageFormat == VK_FORMAT_B5G5R5A1_UNORM_PACK16) &&
            config.border == BORDER_COLOR_CUSTOM && config.customColorFormatless)
            continue;

        for (int i = 0; i < 2; ++i)
        {
            const bool isDSFormat    = isDepthStencilFormat(imageFormat);
            const bool sampleStencil = (i > 0);

            if (separateStencilUsage && sampleStencil)
                continue;

            if (!isDSFormat && sampleStencil)
                continue;

            if (isDSFormat)
            {
                const auto tcuFormat = mapVkFormat(imageFormat);

                if (!sampleStencil && !tcu::hasDepthComponent(tcuFormat.order))
                    continue;
                if (sampleStencil && !tcu::hasStencilComponent(tcuFormat.order))
                    continue;
            }

            samplerAddressModesTests->addChild(new SamplerAddressModesTest(
                testCtx,
                getAddressModesCaseName(config.u, config.v, config.w, config.border, config.customColorValueInt,
                                        config.customColorFormatless, sampleStencil)
                    .c_str(),
                pipelineConstructionType, imageViewType, imageFormat, config.u, config.v, config.w,
                getFormatBorderColor(config.border, imageFormat, sampleStencil),
                getFormatCustomBorderColor(config.customColorValueFloat, config.customColorValueInt, imageFormat,
                                           sampleStencil),
                config.customColorFormatless, separateStencilUsage, sampleStencil));
        }
    }

    return samplerAddressModesTests;
}

// Exact sampling case:
//    1) Create a texture and a framebuffer image of the same size.
//    2) Draw a full screen quad with the texture and VK_FILTER_NEAREST.
//    3) Verify the rendered image matches the texture exactly.
class ExactSamplingCase : public vkt::TestCase
{
public:
    struct Params
    {
        PipelineConstructionType pipelineConstructionType;
        vk::VkFormat format;
        bool unnormalizedCoordinates;
        bool solidColor;
        tcu::Maybe<float> offsetSign; // -1.0 or 1.0
    };

    struct PushConstants
    {
        float texWidth;
        float texHeight;
    };

    struct VertexData
    {
        tcu::Vec2 vtxCoords;
        tcu::Vec2 texCoords;

        static vk::VkVertexInputBindingDescription getBindingDescription(void);
        static std::vector<vk::VkVertexInputAttributeDescription> getAttributeDescriptions(void);
    };

    ExactSamplingCase(tcu::TestContext &testCtx, const std::string &name, const Params &params);
    virtual ~ExactSamplingCase(void)
    {
    }

    virtual void initPrograms(vk::SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    Params m_params;
};

class ExactSamplingInstance : public vkt::TestInstance
{
public:
    using Params = ExactSamplingCase::Params;

    ExactSamplingInstance(Context &context, const Params &params);
    virtual ~ExactSamplingInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void);

    vk::VkExtent3D getTextureExtent(void) const;

private:
    Params m_params;
};

vk::VkVertexInputBindingDescription ExactSamplingCase::VertexData::getBindingDescription(void)
{
    static const vk::VkVertexInputBindingDescription desc = {
        0u,                                                           // uint32_t binding;
        static_cast<uint32_t>(sizeof(ExactSamplingCase::VertexData)), // uint32_t stride;
        vk::VK_VERTEX_INPUT_RATE_VERTEX,                              // VkVertexInputRate inputRate;
    };

    return desc;
}

std::vector<vk::VkVertexInputAttributeDescription> ExactSamplingCase::VertexData::getAttributeDescriptions(void)
{
    static const std::vector<vk::VkVertexInputAttributeDescription> desc = {
        {
            0u,                                                                        // uint32_t location;
            0u,                                                                        // uint32_t binding;
            vk::VK_FORMAT_R32G32_SFLOAT,                                               // VkFormat format;
            static_cast<uint32_t>(offsetof(ExactSamplingCase::VertexData, vtxCoords)), // uint32_t offset;
        },
        {
            1u,                                                                        // uint32_t location;
            0u,                                                                        // uint32_t binding;
            vk::VK_FORMAT_R32G32_SFLOAT,                                               // VkFormat format;
            static_cast<uint32_t>(offsetof(ExactSamplingCase::VertexData, texCoords)), // uint32_t offset;
        },
    };

    return desc;
}

ExactSamplingCase::ExactSamplingCase(tcu::TestContext &testCtx, const std::string &name, const Params &params)
    : vkt::TestCase{testCtx, name}
    , m_params(params)
{
}

void ExactSamplingCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vertexShader;

    std::string texCoordX = "inTexCoord.x";
    std::string texCoordY = "inTexCoord.y";

    if (m_params.unnormalizedCoordinates)
    {
        texCoordX += " * pushc.texWidth";
        texCoordY += " * pushc.texHeight";
    }

    vertexShader << "#version 450\n"
                 << "\n"
                 << "layout(push_constant, std430) uniform PushConstants\n"
                 << "{\n"
                 << "    float texWidth;\n"
                 << "    float texHeight;\n"
                 << "} pushc;\n"
                 << "\n"
                 << "layout(location = 0) in vec2 inPosition;\n"
                 << "layout(location = 1) in vec2 inTexCoord;\n"
                 << "\n"
                 << "layout(location = 0) out vec2 fragTexCoord;\n"
                 << "\n"
                 << "void main() {\n"
                 << "    gl_Position = vec4(inPosition, 0.0, 1.0);\n"
                 << "    fragTexCoord = vec2(" << texCoordX << ", " << texCoordY << ");\n"
                 << "}\n";

    programCollection.glslSources.add("vert") << glu::VertexSource{vertexShader.str()};

    std::ostringstream fragmentShader;

    std::string typePrefix;
    if (vk::isIntFormat(m_params.format))
        typePrefix = "i";
    else if (vk::isUintFormat(m_params.format))
        typePrefix = "u";

    const std::string samplerType = typePrefix + "sampler2D";
    const std::string colorType   = typePrefix + "vec4";

    fragmentShader << "#version 450\n"
                   << "\n"
                   << "layout(set = 0, binding = 0) uniform " << samplerType << " texSampler;\n"
                   << "\n"
                   << "layout(location = 0) in vec2 fragTexCoord;\n"
                   << "\n"
                   << "layout(location = 0) out " << colorType << " outColor;\n"
                   << "\n"
                   << "void main() {\n";

    if (m_params.unnormalizedCoordinates)
    {
        fragmentShader << "    outColor = textureLod(texSampler, fragTexCoord, 0.0f);";
    }
    else
    {
        fragmentShader << "    outColor = texture(texSampler, fragTexCoord);\n";
    }

    fragmentShader << "}\n";

    programCollection.glslSources.add("frag") << glu::FragmentSource{fragmentShader.str()};
}

TestInstance *ExactSamplingCase::createInstance(Context &context) const
{
    return new ExactSamplingInstance{context, m_params};
}

void ExactSamplingCase::checkSupport(Context &context) const
{
#ifndef CTS_USES_VULKANSC
    if (m_params.format == VK_FORMAT_A8_UNORM_KHR)
        context.requireDeviceFunctionality("VK_KHR_maintenance5");
#endif // CTS_USES_VULKANSC

    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();
    const auto props          = vk::getPhysicalDeviceFormatProperties(vki, physicalDevice, m_params.format);
    const vk::VkFormatFeatureFlags requiredFeatures =
        (vk::VK_FORMAT_FEATURE_TRANSFER_DST_BIT | vk::VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
         vk::VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | vk::VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
         (m_params.solidColor ? vk::VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT : 0));

    checkPipelineConstructionRequirements(vki, physicalDevice, m_params.pipelineConstructionType);

    if ((props.optimalTilingFeatures & requiredFeatures) != requiredFeatures)
        TCU_THROW(NotSupportedError, "Selected format does not support the required features");
}

ExactSamplingInstance::ExactSamplingInstance(Context &context, const Params &params)
    : vkt::TestInstance{context}
    , m_params(params)
{
}

vk::VkExtent3D ExactSamplingInstance::getTextureExtent(void) const
{
    return vk::makeExtent3D(256u, 256u, 1u);
}

tcu::TestStatus ExactSamplingInstance::iterate(void)
{
    const auto &vki       = m_context.getInstanceInterface();
    const auto &vkd       = m_context.getDeviceInterface();
    const auto physDevice = m_context.getPhysicalDevice();
    const auto device     = m_context.getDevice();
    auto &allocator       = m_context.getDefaultAllocator();
    const auto queue      = m_context.getUniversalQueue();
    const auto queueIndex = m_context.getUniversalQueueFamilyIndex();

    const auto tcuFormat  = vk::mapVkFormat(m_params.format);
    const auto formatInfo = tcu::getTextureFormatInfo(tcuFormat);
    const auto texExtent  = getTextureExtent();
    const auto texUsage   = (vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT | vk::VK_IMAGE_USAGE_SAMPLED_BIT);
    const auto fbUsage    = (vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    const auto descType   = vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    const auto texLayout  = vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    const bool &unnorm    = m_params.unnormalizedCoordinates;

    // Some code below depends on this.
    DE_ASSERT(texExtent.depth == 1u);

    const vk::VkImageCreateInfo texImgCreateInfo = {
        vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        0u,                                      // VkImageCreateFlags flags;
        vk::VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        m_params.format,                         // VkFormat format;
        texExtent,                               // VkExtent3D extent;
        1u,                                      // uint32_t mipLevels;
        1u,                                      // uint32_t arrayLayers;
        vk::VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        vk::VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        texUsage,                                // VkImageUsageFlags usage;
        vk::VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        1u,                                      // uint32_t queueFamilyIndexCount;
        &queueIndex,                             // const uint32_t* pQueueFamilyIndices;
        vk::VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };

    const vk::VkImageCreateInfo fbImgCreateInfo = {
        vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        0u,                                      // VkImageCreateFlags flags;
        vk::VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        m_params.format,                         // VkFormat format;
        texExtent,                               // VkExtent3D extent;
        1u,                                      // uint32_t mipLevels;
        1u,                                      // uint32_t arrayLayers;
        vk::VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        vk::VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        fbUsage,                                 // VkImageUsageFlags usage;
        vk::VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        1u,                                      // uint32_t queueFamilyIndexCount;
        &queueIndex,                             // const uint32_t* pQueueFamilyIndices;
        vk::VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };

    // Create main and framebuffer images.
    const vk::ImageWithMemory texImage{vkd, device, allocator, texImgCreateInfo, vk::MemoryRequirement::Any};
    const vk::ImageWithMemory fbImage{vkd, device, allocator, fbImgCreateInfo, vk::MemoryRequirement::Any};

    // Corresponding image views.
    const auto colorSubresourceRange = vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto texView = vk::makeImageView(vkd, device, texImage.get(), vk::VK_IMAGE_VIEW_TYPE_2D, m_params.format,
                                           colorSubresourceRange);
    const auto fbView  = vk::makeImageView(vkd, device, fbImage.get(), vk::VK_IMAGE_VIEW_TYPE_2D, m_params.format,
                                           colorSubresourceRange);

    // Buffers to create the texture and verify results.
    const vk::VkDeviceSize texBufferSize = static_cast<vk::VkDeviceSize>(
        static_cast<uint32_t>(tcu::getPixelSize(tcuFormat)) * texExtent.width * texExtent.height * texExtent.depth);
    const auto texBufferInfo     = vk::makeBufferCreateInfo(texBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    const auto resultsBufferInfo = vk::makeBufferCreateInfo(texBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const vk::BufferWithMemory texBuffer{vkd, device, allocator, texBufferInfo, vk::MemoryRequirement::HostVisible};
    const vk::BufferWithMemory resultsBuffer{vkd, device, allocator, resultsBufferInfo,
                                             vk::MemoryRequirement::HostVisible};

    // Create texture.
    const tcu::IVec2 iImgSize{static_cast<int>(texExtent.width), static_cast<int>(texExtent.height)};
    auto &texBufferAlloc = texBuffer.getAllocation();
    auto texBufferPtr    = reinterpret_cast<char *>(texBufferAlloc.getHostPtr()) + texBufferAlloc.getOffset();
    const tcu::PixelBufferAccess texPixels{tcuFormat, iImgSize[0], iImgSize[1], 1, texBufferPtr};

    const int W = texPixels.getWidth();
    const int H = texPixels.getHeight();
    const int D = texPixels.getDepth();

    const float divX = static_cast<float>(W - 1);
    const float divY = static_cast<float>(H - 1);

    tcu::Vec4 texColor;

    for (int x = 0; x < W; ++x)
        for (int y = 0; y < H; ++y)
            for (int z = 0; z < D; ++z)
            {
                if (m_params.solidColor)
                {
                    // Texture with solid color for filtered sampling.
                    texColor = tcu::Vec4{0.5f, 0.25f, 0.7529411764705882f, 1.0f};
                }
                else
                {
                    // Use a color gradient otherwise.
                    const float colorX = static_cast<float>(x) / divX;
                    const float colorY = static_cast<float>(y) / divY;
                    const float colorZ = std::min(colorX, colorY);

                    texColor = tcu::Vec4{colorX, colorY, colorZ, 1.0f};
                }
                const tcu::Vec4 finalColor = (texColor - formatInfo.lookupBias) / formatInfo.lookupScale;
                texPixels.setPixel(finalColor, x, y, z);
            }

    vk::flushAlloc(vkd, device, texBufferAlloc);

    float minU = 0.0f;
    float maxU = 1.0f;
    float minV = 0.0f;
    float maxV = 1.0f;

    // When testing the edges, apply a texture offset of almost half a texel, so the sample location is very close to the texel border.
    if (m_params.offsetSign)
    {
        const float sign = m_params.offsetSign.get();
        DE_ASSERT(sign == 1.0f || sign == -1.0f);
        const float offsetWidth  = 0.499f / static_cast<float>(texExtent.width);
        const float offsetHeight = 0.499f / static_cast<float>(texExtent.height);

        minU += sign * offsetWidth;
        maxU += sign * offsetWidth;
        minV += sign * offsetHeight;
        maxV += sign * offsetHeight;
    }

    const std::vector<ExactSamplingCase::VertexData> fullScreenQuad = {
        {
            {1.f, -1.f},
            {maxU, minV},
        },
        {
            {-1.f, -1.f},
            {minU, minV},
        },
        {
            {-1.f, 1.f},
            {minU, maxV},
        },
        {
            {-1.f, 1.f},
            {minU, maxV},
        },
        {
            {1.f, -1.f},
            {maxU, minV},
        },
        {
            {1.f, 1.f},
            {maxU, maxV},
        },
    };

    // Vertex buffer.
    const vk::VkDeviceSize vertexBufferSize =
        static_cast<vk::VkDeviceSize>(fullScreenQuad.size() * sizeof(decltype(fullScreenQuad)::value_type));
    const auto vertexBufferInfo = vk::makeBufferCreateInfo(vertexBufferSize, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    const vk::BufferWithMemory vertexBuffer{vkd, device, allocator, vertexBufferInfo,
                                            vk::MemoryRequirement::HostVisible};

    // Copy data to vertex buffer.
    const auto &vertexAlloc  = vertexBuffer.getAllocation();
    const auto vertexDataPtr = reinterpret_cast<char *>(vertexAlloc.getHostPtr()) + vertexAlloc.getOffset();
    deMemcpy(vertexDataPtr, fullScreenQuad.data(), static_cast<size_t>(vertexBufferSize));
    vk::flushAlloc(vkd, device, vertexAlloc);

    // Descriptor set layout.
    vk::DescriptorSetLayoutBuilder layoutBuilder;
    layoutBuilder.addSingleBinding(descType, vk::VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto descriptorSetLayout = layoutBuilder.build(vkd, device);

    // Descriptor pool.
    vk::DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descType);
    const auto descriptorPool =
        poolBuilder.build(vkd, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    // Descriptor set.
    const auto descriptorSet = vk::makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayout.get());

    // Texture sampler. When using a solid color, test linear filtering. Linear filtering may incur in a small precission loss, but
    // it should be minimal and we should get the same color when converting back to the original format.
    const auto minMagFilter = (m_params.solidColor ? vk::VK_FILTER_LINEAR : vk::VK_FILTER_NEAREST);
    const auto addressMode  = (unnorm ? vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : vk::VK_SAMPLER_ADDRESS_MODE_REPEAT);
    const auto unnormalizedCoordinates = (unnorm ? VK_TRUE : VK_FALSE);

    const vk::VkSamplerCreateInfo samplerCreateInfo = {
        vk::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,   // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        0u,                                          // VkSamplerCreateFlags flags;
        minMagFilter,                                // VkFilter magFilter;
        minMagFilter,                                // VkFilter minFilter;
        vk::VK_SAMPLER_MIPMAP_MODE_NEAREST,          // VkSamplerMipmapMode mipmapMode;
        addressMode,                                 // VkSamplerAddressMode addressModeU;
        addressMode,                                 // VkSamplerAddressMode addressModeV;
        addressMode,                                 // VkSamplerAddressMode addressModeW;
        0.0f,                                        // float mipLodBias;
        VK_FALSE,                                    // VkBool32 anisotropyEnable;
        1.0f,                                        // float maxAnisotropy;
        VK_FALSE,                                    // VkBool32 compareEnable;
        vk::VK_COMPARE_OP_NEVER,                     // VkCompareOp compareOp;
        0.0f,                                        // float minLod;
        0.0f,                                        // float maxLod;
        vk::VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, // VkBorderColor borderColor;
        unnormalizedCoordinates,                     // VkBool32 unnormalizedCoordinates;
    };
    const auto texSampler = vk::createSampler(vkd, device, &samplerCreateInfo);

    // Update descriptor set with the descriptor.
    vk::DescriptorSetUpdateBuilder updateBuilder;
    const auto descriptorImageInfo = vk::makeDescriptorImageInfo(texSampler.get(), texView.get(), texLayout);
    updateBuilder.writeSingle(descriptorSet.get(), vk::DescriptorSetUpdateBuilder::Location::binding(0u), descType,
                              &descriptorImageInfo);
    updateBuilder.update(vkd, device);

    // Shader modules.
    const auto vertexModule = vk::ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
    const auto fragModule   = vk::ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("frag"), 0u);

    // Render pass.
    const vk::VkAttachmentDescription fbAttachment = {
        0u,                                           // VkAttachmentDescriptionFlags flags;
        m_params.format,                              // VkFormat format;
        vk::VK_SAMPLE_COUNT_1_BIT,                    // VkSampleCountFlagBits samples;
        vk::VK_ATTACHMENT_LOAD_OP_CLEAR,              // VkAttachmentLoadOp loadOp;
        vk::VK_ATTACHMENT_STORE_OP_STORE,             // VkAttachmentStoreOp storeOp;
        vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp stencilLoadOp;
        vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,         // VkAttachmentStoreOp stencilStoreOp;
        vk::VK_IMAGE_LAYOUT_UNDEFINED,                // VkImageLayout initialLayout;
        vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout finalLayout;
    };

    const vk::VkAttachmentReference colorRef = {
        0u,                                           // uint32_t attachment;
        vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout layout;
    };

    const vk::VkSubpassDescription subpass = {
        0u,                                  // VkSubpassDescriptionFlags flags;
        vk::VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint pipelineBindPoint;
        0u,                                  // uint32_t inputAttachmentCount;
        nullptr,                             // const VkAttachmentReference* pInputAttachments;
        1u,                                  // uint32_t colorAttachmentCount;
        &colorRef,                           // const VkAttachmentReference* pColorAttachments;
        0u,                                  // const VkAttachmentReference* pResolveAttachments;
        nullptr,                             // const VkAttachmentReference* pDepthStencilAttachment;
        0u,                                  // uint32_t preserveAttachmentCount;
        nullptr,                             // const uint32_t* pPreserveAttachments;
    };

    const vk::VkRenderPassCreateInfo renderPassInfo = {
        vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType sType;
        nullptr,                                       // const void* pNext;
        0u,                                            // VkRenderPassCreateFlags flags;
        1u,                                            // uint32_t attachmentCount;
        &fbAttachment,                                 // const VkAttachmentDescription* pAttachments;
        1u,                                            // uint32_t subpassCount;
        &subpass,                                      // const VkSubpassDescription* pSubpasses;
        0u,                                            // uint32_t dependencyCount;
        nullptr,                                       // const VkSubpassDependency* pDependencies;
    };
    RenderPassWrapper renderPass(m_params.pipelineConstructionType, vkd, device, &renderPassInfo);

    // Framebuffer.
    std::vector<vk::VkImageView> attachments;
    attachments.push_back(fbView.get());
    renderPass.createFramebuffer(vkd, device, 1u, &fbImage.get(), &fbView.get(), texExtent.width, texExtent.height,
                                 texExtent.depth);

    // Push constant range.
    const vk::VkPushConstantRange pcRange = {
        vk::VK_SHADER_STAGE_VERTEX_BIT,                                  // VkShaderStageFlags stageFlags;
        0u,                                                              // uint32_t offset;
        static_cast<uint32_t>(sizeof(ExactSamplingCase::PushConstants)), // uint32_t size;
    };

    // Pipeline layout.
    const vk::VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
        nullptr,                                           // const void* pNext;
        0u,                                                // VkPipelineLayoutCreateFlags flags;
        1u,                                                // uint32_t setLayoutCount;
        &descriptorSetLayout.get(),                        // const VkDescriptorSetLayout* pSetLayouts;
        1u,                                                // uint32_t pushConstantRangeCount;
        &pcRange,                                          // const VkPushConstantRange* pPushConstantRanges;
    };
    const vk::PipelineLayoutWrapper pipelineLayout(m_params.pipelineConstructionType, vkd, device, &pipelineLayoutInfo);

    // Graphics pipeline.
    const std::vector<vk::VkViewport> viewports{vk::makeViewport(texExtent)};
    const vk::VkRect2D renderArea = vk::makeRect2D(texExtent);
    const std::vector<vk::VkRect2D> scissors{renderArea};

    const auto vtxBindingDescription    = ExactSamplingCase::VertexData::getBindingDescription();
    const auto vtxAttributeDescriptions = ExactSamplingCase::VertexData::getAttributeDescriptions();

    const vk::VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                             sType
        nullptr,                // const void*                                 pNext
        0u,                     // VkPipelineVertexInputStateCreateFlags       flags
        1u,                     // uint32_t                                    vertexBindingDescriptionCount
        &vtxBindingDescription, // const VkVertexInputBindingDescription*      pVertexBindingDescriptions
        static_cast<uint32_t>(
            vtxAttributeDescriptions
                .size()),                // uint32_t                                    vertexAttributeDescriptionCount
        vtxAttributeDescriptions.data(), // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
    };

    GraphicsPipelineWrapper pipeline(vki, vkd, physDevice, device, m_context.getDeviceExtensions(),
                                     m_params.pipelineConstructionType);
    pipeline.setDefaultDepthStencilState()
        .setDefaultRasterizationState()
        .setDefaultMultisampleState()
        .setDefaultColorBlendState()
        .setupVertexInputState(&vertexInputInfo)
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u, vertexModule)
        .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragModule)
        .setupFragmentOutputState(*renderPass)
        .setMonolithicPipelineLayout(pipelineLayout)
        .buildPipeline();

    // Command pool and command buffer.
    const auto cmdPool =
        vk::createCommandPool(vkd, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueIndex);
    const auto cmdBufferPtr =
        vk::allocateCommandBuffer(vkd, device, cmdPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer = cmdBufferPtr.get();

    // Draw quad.
    const ExactSamplingCase::PushConstants pushConstants = {
        static_cast<float>(texExtent.width),
        static_cast<float>(texExtent.height),
    };

    const tcu::Vec4 clearFbColor(0.0f, 0.0f, 0.0f, 1.0f);
    const vk::VkDeviceSize vertexBufferOffset = 0ull;

    const auto vertexBufferBarrier =
        vk::makeBufferMemoryBarrier(vk::VK_ACCESS_HOST_WRITE_BIT, vk::VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                                    vertexBuffer.get(), 0ull, vertexBufferSize);
    const auto preBufferCopyBarrier = vk::makeBufferMemoryBarrier(
        vk::VK_ACCESS_HOST_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT, texBuffer.get(), 0ull, texBufferSize);
    const auto preTexCopyBarrier =
        vk::makeImageMemoryBarrier(0u, vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED,
                                   vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texImage.get(), colorSubresourceRange);
    const auto postTexCopyBarrier = vk::makeImageMemoryBarrier(
        vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_ACCESS_SHADER_READ_BIT, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        texLayout, texImage.get(), colorSubresourceRange);
    const auto texCopyRange = vk::makeImageSubresourceLayers(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
    const auto texImageCopy = vk::makeBufferImageCopy(texExtent, texCopyRange);

    vk::beginCommandBuffer(vkd, cmdBuffer);

    vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_HOST_BIT, vk::VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0u, 0u,
                           nullptr, 1u, &vertexBufferBarrier, 0u, nullptr);
    vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_HOST_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                           nullptr, 1u, &preBufferCopyBarrier, 0u, nullptr);
    vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                           nullptr, 0u, nullptr, 1u, &preTexCopyBarrier);
    vkd.cmdCopyBufferToImage(cmdBuffer, texBuffer.get(), texImage.get(), vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u,
                             &texImageCopy);
    vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u,
                           0u, nullptr, 0u, nullptr, 1u, &postTexCopyBarrier);

    renderPass.begin(vkd, cmdBuffer, renderArea, clearFbColor);
    pipeline.bind(cmdBuffer);
    vkd.cmdBindDescriptorSets(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u,
                              &descriptorSet.get(), 0u, nullptr);
    vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
    vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), vk::VK_SHADER_STAGE_VERTEX_BIT, 0u,
                         static_cast<uint32_t>(sizeof(pushConstants)), &pushConstants);
    vkd.cmdDraw(cmdBuffer, static_cast<uint32_t>(fullScreenQuad.size()), 1u, 0u, 0u);
    renderPass.end(vkd, cmdBuffer);

    vk::copyImageToBuffer(vkd, cmdBuffer, fbImage.get(), resultsBuffer.get(), iImgSize);

    vk::endCommandBuffer(vkd, cmdBuffer);
    vk::submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    // Check results.
    const auto &resultsBufferAlloc = resultsBuffer.getAllocation();
    vk::invalidateAlloc(vkd, device, resultsBufferAlloc);

    const auto resultsBufferPtr =
        reinterpret_cast<const char *>(resultsBufferAlloc.getHostPtr()) + resultsBufferAlloc.getOffset();
    const tcu::ConstPixelBufferAccess resultPixels{tcuFormat, iImgSize[0], iImgSize[1], 1, resultsBufferPtr};

    const tcu::TextureFormat diffFormat{tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8};
    const auto diffBytes = tcu::getPixelSize(diffFormat) * iImgSize[0] * iImgSize[1];
    std::unique_ptr<uint8_t[]> diffData{new uint8_t[diffBytes]};
    const tcu::PixelBufferAccess diffImg{diffFormat, iImgSize[0], iImgSize[1], 1, diffData.get()};

    const tcu::Vec4 colorRed{1.0f, 0.0f, 0.0f, 1.0f};
    const tcu::Vec4 colorGreen{0.0f, 1.0f, 0.0f, 1.0f};

    // Clear diff image.
    deMemset(diffData.get(), 0, static_cast<size_t>(diffBytes));

    bool pass = true;
    for (int x = 0; x < W; ++x)
        for (int y = 0; y < H; ++y)
            for (int z = 0; z < D; ++z)
            {
                const auto inPix  = texPixels.getPixel(x, y, z);
                const auto outPix = resultPixels.getPixel(x, y, z);
                if (inPix == outPix)
                {
                    diffImg.setPixel(colorGreen, x, y, z);
                }
                else
                {
                    pass = false;
                    diffImg.setPixel(colorRed, x, y, z);
                }
            }

    tcu::TestStatus status = tcu::TestStatus::pass("Pass");
    if (!pass)
    {
        auto &log = m_context.getTestContext().getLog();
        log << tcu::TestLog::Image("input", "Input texture", texPixels);
        log << tcu::TestLog::Image("output", "Rendered image", resultPixels);
        log << tcu::TestLog::Image("diff", "Mismatched pixels in red", diffImg);
        status = tcu::TestStatus::fail("Pixel mismatch; please check the rendered image");
    }

    return status;
}

enum class LodBiasCase
{
    SAMPLER_BIAS   = 0,
    SAMPLER_MINLOD = 1,
    SHADER_LOD     = 2,
    SHADER_BIAS    = 3,
    VIEW_MINLOD    = 4,
    CASE_COUNT     = 5,
};

std::string getLodBiasCaseName(LodBiasCase lodBiasCase)
{
    std::string name;

    switch (lodBiasCase)
    {
    case LodBiasCase::SAMPLER_BIAS:
        name = "sampler_bias";
        break;
    case LodBiasCase::SAMPLER_MINLOD:
        name = "sampler_minlod";
        break;
    case LodBiasCase::SHADER_LOD:
        name = "shader_lod";
        break;
    case LodBiasCase::SHADER_BIAS:
        name = "shader_bias";
        break;
    case LodBiasCase::VIEW_MINLOD:
        name = "view_minlod";
        break;
    case LodBiasCase::CASE_COUNT: // fallthrough
    default:
        DE_ASSERT(false);
        break;
    }

    return name;
}

struct MaxSamplerLodBiasParams
{
    const PipelineConstructionType pipelineConstructionType;
    const LodBiasCase lodBiasCase;
};

class MaxSamplerLodBiasCase : public vkt::TestCase
{
public:
    MaxSamplerLodBiasCase(tcu::TestContext &testCtx, const std::string &name, const MaxSamplerLodBiasParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~MaxSamplerLodBiasCase(void)
    {
    }

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

protected:
    const MaxSamplerLodBiasParams m_params;
};

class MaxSamplerLodBiasInstance : public vkt::TestInstance
{
public:
    MaxSamplerLodBiasInstance(Context &context, const MaxSamplerLodBiasParams &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }
    virtual ~MaxSamplerLodBiasInstance(void)
    {
    }

    tcu::TestStatus iterate(void);

protected:
    const MaxSamplerLodBiasParams m_params;
};

void MaxSamplerLodBiasCase::checkSupport(Context &context) const
{
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_params.pipelineConstructionType);

    if (m_params.lodBiasCase == LodBiasCase::VIEW_MINLOD)
        context.requireDeviceFunctionality("VK_EXT_image_view_min_lod");
}

void MaxSamplerLodBiasCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "void main (void) {\n"
         << "    gl_Position = inPos;\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::string sampleStatement;
    const std::string sampleCoords = "vec2(gl_FragCoord.x / pc.fbWidth, gl_FragCoord.y / pc.fbHeight)";

    if (m_params.lodBiasCase == LodBiasCase::SHADER_LOD)
        sampleStatement = "textureLod(texSampler, " + sampleCoords + ", pc.lodLevel)";
    else if (m_params.lodBiasCase == LodBiasCase::SHADER_BIAS)
        sampleStatement = "texture(texSampler, " + sampleCoords + ", pc.lodLevel)";
    else
        sampleStatement = "textureLod(texSampler, " + sampleCoords + ", 0.0)";

    DE_ASSERT(!sampleStatement.empty());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "layout (set=0, binding=0) uniform sampler2D texSampler;\n"
         << "layout (push_constant, std430) uniform PushConstantBlock {\n"
         << "    float lodLevel;\n"
         << "    float fbWidth;\n"
         << "    float fbHeight;\n"
         << "} pc;\n"
         << "void main (void) {\n"
         << "    outColor = " << sampleStatement << ";\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

TestInstance *MaxSamplerLodBiasCase::createInstance(Context &context) const
{
    return new MaxSamplerLodBiasInstance(context, m_params);
}

tcu::TestStatus MaxSamplerLodBiasInstance::iterate(void)
{
    const auto ctx               = m_context.getContextCommonData();
    const auto vkFormat          = VK_FORMAT_R8G8B8A8_UNORM;
    const auto tcuFormat         = mapVkFormat(vkFormat);
    const auto imageType         = VK_IMAGE_TYPE_2D;
    const auto tiling            = VK_IMAGE_TILING_OPTIMAL;
    const auto textureUsage      = (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    const auto fbUsage           = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto sampleCount       = VK_SAMPLE_COUNT_1_BIT;
    const auto descriptorType    = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    const auto maxLodBias        = getPhysicalDeviceProperties(ctx.vki, ctx.physicalDevice).limits.maxSamplerLodBias;
    const auto roundedBias       = deFloatFloor(maxLodBias + 0.5f);
    const auto textureProperties = getPhysicalDeviceImageFormatProperties(ctx.vki, ctx.physicalDevice, vkFormat,
                                                                          imageType, tiling, textureUsage, 0u);
    const auto textureExtent     = makeExtent3D(textureProperties.maxExtent.width, 1u, 1u);
    const auto fbExtent          = tcu::IVec3(1, 1, 1);
    const auto vkExtent          = makeExtent3D(fbExtent);
    const auto baseSeed          = 1687852938u;

    const VkImageCreateInfo textureCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        imageType,                           // VkImageType imageType;
        vkFormat,                            // VkFormat format;
        textureExtent,                       // VkExtent3D extent;
        textureProperties.maxMipLevels,      // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        sampleCount,                         // VkSampleCountFlagBits samples;
        tiling,                              // VkImageTiling tiling;
        textureUsage,                        // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };
    ImageWithMemory texture(ctx.vkd, ctx.device, ctx.allocator, textureCreateInfo, MemoryRequirement::Any);
    const auto textureColorSRR =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, textureCreateInfo.mipLevels, 0u, 1u);
    const auto accessedLevel = std::min(roundedBias, static_cast<float>(textureCreateInfo.mipLevels - 1u));

#ifndef CTS_USES_VULKANSC
    std::unique_ptr<VkImageViewMinLodCreateInfoEXT> viewMinLodPtr;
    if (m_params.lodBiasCase == LodBiasCase::VIEW_MINLOD)
    {
        viewMinLodPtr.reset(new VkImageViewMinLodCreateInfoEXT{
            VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT, nullptr,
            accessedLevel, // VUID-VkImageViewMinLodCreateInfoEXT-minLod-06456 prevents us from using maxLodBias directly.
        });
    }
#endif // CTS_USES_VULKANSC

    const VkImageViewCreateInfo viewCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
#ifndef CTS_USES_VULKANSC
        viewMinLodPtr.get(), // const void* pNext;
#else
        nullptr, // const void* pNext;
#endif                         // CTS_USES_VULKANSC
        0u,                    // VkImageViewCreateFlags flags;
        texture.get(),         // VkImage image;
        VK_IMAGE_VIEW_TYPE_2D, // VkImageViewType viewType;
        vkFormat,              // VkFormat format;
        {
            // VkComponentMapping components;
            VK_COMPONENT_SWIZZLE_R,
            VK_COMPONENT_SWIZZLE_G,
            VK_COMPONENT_SWIZZLE_B,
            VK_COMPONENT_SWIZZLE_A,
        },
        textureColorSRR, // VkImageSubresourceRange subresourceRange;
    };
    const auto textureView = createImageView(ctx.vkd, ctx.device, &viewCreateInfo);

    // Output color buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, vkFormat, fbUsage, imageType);

    // Texture sampler.
    const float samplerMipLodBias = ((m_params.lodBiasCase == LodBiasCase::SAMPLER_BIAS) ? maxLodBias : 0.0f);
    const float samplerMinLod     = ((m_params.lodBiasCase == LodBiasCase::SAMPLER_MINLOD) ? maxLodBias : 0.0f);
    const VkSamplerCreateInfo samplerCreateInfo = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,   // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        0u,                                      // VkSamplerCreateFlags flags;
        VK_FILTER_NEAREST,                       // VkFilter magFilter;
        VK_FILTER_NEAREST,                       // VkFilter minFilter;
        VK_SAMPLER_MIPMAP_MODE_NEAREST,          // VkSamplerMipmapMode mipmapMode;
        VK_SAMPLER_ADDRESS_MODE_REPEAT,          // VkSamplerAddressMode addressModeU;
        VK_SAMPLER_ADDRESS_MODE_REPEAT,          // VkSamplerAddressMode addressModeV;
        VK_SAMPLER_ADDRESS_MODE_REPEAT,          // VkSamplerAddressMode addressModeW;
        samplerMipLodBias,                       // float mipLodBias;
        VK_FALSE,                                // VkBool32 anisotropyEnable;
        0.0f,                                    // float maxAnisotropy;
        VK_FALSE,                                // VkBool32 compareEnable;
        VK_COMPARE_OP_NEVER,                     // VkCompareOp compareOp;
        samplerMinLod,                           // float minLod;
        VK_LOD_CLAMP_NONE,                       // float maxLod;
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, // VkBorderColor borderColor;
        VK_FALSE,                                // VkBool32 unnormalizedCoordinates;
    };
    const auto sampler = createSampler(ctx.vkd, ctx.device, &samplerCreateInfo);

    // Vertex buffer.
    const std::vector<tcu::Vec4> vertices{
        tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
        tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
    };
    const auto vertexBufferSize = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vertexBufferInfo = makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferInfo, MemoryRequirement::HostVisible);
    auto &vertexBufferAlloc       = vertexBuffer.getAllocation();
    void *vertexBufferDataPtr     = vertexBufferAlloc.getHostPtr();
    const auto vertexBufferOffset = static_cast<VkDeviceSize>(0);

    deMemcpy(vertexBufferDataPtr, de::dataOrNull(vertices), de::dataSize(vertices));
    flushAlloc(ctx.vkd, ctx.device, vertexBufferAlloc);

    // Render pass and framebuffer.
    auto renderPass = RenderPassWrapper(m_params.pipelineConstructionType, ctx.vkd, ctx.device, vkFormat);
    renderPass.createFramebuffer(ctx.vkd, ctx.device, colorBuffer.getImage(), colorBuffer.getImageView(),
                                 vkExtent.width, vkExtent.height);

    // Push constants.
    const std::vector<float> pcValues{maxLodBias, static_cast<float>(vkExtent.width),
                                      static_cast<float>(vkExtent.height)};
    const auto pcSize   = static_cast<uint32_t>(de::dataSize(pcValues));
    const auto pcStages = VK_SHADER_STAGE_FRAGMENT_BIT;
    const auto pcRange  = makePushConstantRange(pcStages, 0u, pcSize);

    // Descriptor pool and set.
    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(descriptorType, VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto descriptorSetLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);
    const PipelineLayoutWrapper pipelineLayout(m_params.pipelineConstructionType, ctx.vkd, ctx.device,
                                               *descriptorSetLayout, &pcRange);

    DescriptorPoolBuilder descriptorPoolBuilder;
    descriptorPoolBuilder.addType(descriptorType);
    const auto descriptorPool =
        descriptorPoolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *descriptorSetLayout);

    DescriptorSetUpdateBuilder setUpdateBuilder;
    const auto samplerDescriptorInfo =
        makeDescriptorImageInfo(*sampler, *textureView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    setUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType,
                                 &samplerDescriptorInfo);
    setUpdateBuilder.update(ctx.vkd, ctx.device);

    // Pipeline.
    const auto &binaries = m_context.getBinaryCollection();
    const auto vertMod   = ShaderWrapper(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragMod   = ShaderWrapper(ctx.vkd, ctx.device, binaries.get("frag"));

    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(vkExtent));

    GraphicsPipelineWrapper pipeline(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device, m_context.getDeviceExtensions(),
                                     m_params.pipelineConstructionType);
    pipeline.setDefaultDepthStencilState()
        .setDefaultRasterizationState()
        .setDefaultMultisampleState()
        .setDefaultColorBlendState()
        .setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
        .setupVertexInputState()
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u, vertMod)
        .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragMod)
        .setupFragmentOutputState(*renderPass)
        .setMonolithicPipelineLayout(pipelineLayout)
        .buildPipeline();

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);

    // Transition texture to layout suitable for filling.
    const auto preFillBarrier =
        makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture.get(), textureColorSRR);
    cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  &preFillBarrier);

    // Fill every level with a different random color.
    auto &log = m_context.getTestContext().getLog();
    de::Random rnd(baseSeed + static_cast<uint32_t>(m_params.lodBiasCase));
    std::vector<tcu::Vec4> levelColors;
    levelColors.reserve(textureCreateInfo.mipLevels);

    for (uint32_t level = 0u; level < textureCreateInfo.mipLevels; ++level)
    {
        const auto levelRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, level, 1u, 0u, 1u);
        // If we "embed" these expressions in the constructor below, separated by commas, the order in which colors are obtained would depend on the compiler.
        const auto red   = rnd.getFloat(0.0f, 1.0f);
        const auto green = rnd.getFloat(0.0f, 1.0f);
        const auto blue  = rnd.getFloat(0.0f, 1.0f);
        const tcu::Vec4 clearValue(red, green, blue, 1.0f);
        const auto clearValueColor = makeClearValueColorVec4(clearValue);

        levelColors.push_back(clearValue);
        ctx.vkd.cmdClearColorImage(cmdBuffer, texture.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   &clearValueColor.color, 1u, &levelRange);
        log << tcu::TestLog::Message << "Level " << level << " cleared to " << clearValue << tcu::TestLog::EndMessage;
    }

    // Transition texture to suitable layout for reading it in the shader.
    const auto preReadBarrier = makeImageMemoryBarrier(
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, texture.get(), textureColorSRR);
    cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, &preReadBarrier);

    const tcu::Vec4 fbClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    renderPass.begin(ctx.vkd, cmdBuffer, scissors.at(0u), fbClearColor);
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
    pipeline.bind(cmdBuffer);
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
                                  &descriptorSet.get(), 0u, nullptr);
    ctx.vkd.cmdPushConstants(cmdBuffer, *pipelineLayout, pcStages, 0u, pcSize, de::dataOrNull(pcValues));
    ctx.vkd.cmdDraw(cmdBuffer, de::sizeU32(vertices), 1u, 0u, 0u);
    renderPass.end(ctx.vkd, cmdBuffer);

    // Copy color buffer to verification buffer.
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1),
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1u,
                      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Verify color buffer has the right mip level color.
    const auto &colorBufferAlloc = colorBuffer.getBufferAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, colorBufferAlloc);

    const tcu::ConstPixelBufferAccess resultAccess(tcuFormat, fbExtent, colorBufferAlloc.getHostPtr());
    const auto channelThreshold = 0.005f; // 1/255 < 0.005 < 2/255
    const tcu::Vec4 threshold(channelThreshold, channelThreshold, channelThreshold, 0.0f);

    if (!tcu::floatThresholdCompare(log, "Result", "", levelColors.at(static_cast<size_t>(accessedLevel)), resultAccess,
                                    threshold, tcu::COMPARE_LOG_EVERYTHING))
        return tcu::TestStatus::fail("Unexpected color found in color buffer -- check log for details");

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createAllFormatsSamplerTests(tcu::TestContext &testCtx,
                                                 PipelineConstructionType pipelineConstructionType,
                                                 bool separateStencilUsage = false)
{
    const struct
    {
        SamplerViewType type;
        const char *name;
    } imageViewTypes[] = {{VK_IMAGE_VIEW_TYPE_1D, "1d"},
                          {{VK_IMAGE_VIEW_TYPE_1D, false}, "1d_unnormalized"},
                          {VK_IMAGE_VIEW_TYPE_1D_ARRAY, "1d_array"},
                          {VK_IMAGE_VIEW_TYPE_2D, "2d"},
                          {{VK_IMAGE_VIEW_TYPE_2D, false}, "2d_unnormalized"},
                          {VK_IMAGE_VIEW_TYPE_2D_ARRAY, "2d_array"},
                          {VK_IMAGE_VIEW_TYPE_3D, "3d"},
                          {VK_IMAGE_VIEW_TYPE_CUBE, "cube"},
                          {VK_IMAGE_VIEW_TYPE_CUBE_ARRAY, "cube_array"}};

    const VkFormat formats[] = {
        // Packed formats
        VK_FORMAT_R4G4_UNORM_PACK8,
        VK_FORMAT_R4G4B4A4_UNORM_PACK16,
        VK_FORMAT_R5G6B5_UNORM_PACK16,
        VK_FORMAT_R5G5B5A1_UNORM_PACK16,
        VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        VK_FORMAT_A2R10G10B10_UINT_PACK32,
        VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
        VK_FORMAT_B4G4R4A4_UNORM_PACK16,
        VK_FORMAT_B5G5R5A1_UNORM_PACK16,
        VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT,
        VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT,
#ifndef CTS_USES_VULKANSC
        VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR,
#endif // CTS_USES_VULKANSC

        // Pairwise combinations of 8-bit channel formats, UNORM/SNORM/SINT/UINT/SRGB type x 1-to-4 channels x RGBA/BGRA order
        VK_FORMAT_R8_SRGB,
        VK_FORMAT_R8G8B8_UINT,
        VK_FORMAT_B8G8R8A8_SINT,
        VK_FORMAT_R8G8_UNORM,
        VK_FORMAT_B8G8R8_SNORM,
        VK_FORMAT_R8G8B8A8_SNORM,
        VK_FORMAT_R8G8_UINT,
        VK_FORMAT_R8_SINT,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_B8G8R8_SRGB,
        VK_FORMAT_R8G8_SRGB,
        VK_FORMAT_R8_UINT,
        VK_FORMAT_R8G8B8A8_UINT,
        VK_FORMAT_R8G8_SINT,
        VK_FORMAT_R8_SNORM,
        VK_FORMAT_B8G8R8_SINT,
        VK_FORMAT_R8G8_SNORM,
        VK_FORMAT_B8G8R8_UNORM,
        VK_FORMAT_R8_UNORM,
#ifndef CTS_USES_VULKANSC
        VK_FORMAT_A8_UNORM_KHR,
#endif // CTS_USES_VULKANSC

        // Pairwise combinations of 16/32-bit channel formats x SINT/UINT/SFLOAT type x 1-to-4 channels
        VK_FORMAT_R32G32_SFLOAT,
        VK_FORMAT_R32G32B32_UINT,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_FORMAT_R16G16_UINT,
        VK_FORMAT_R32G32B32A32_SINT,
        VK_FORMAT_R16G16B16_SINT,
        VK_FORMAT_R16_SFLOAT,
        VK_FORMAT_R32_SINT,
        VK_FORMAT_R32_UINT,
        VK_FORMAT_R16G16B16_SFLOAT,
        VK_FORMAT_R16G16_SINT,

        // More 16/32-bit formats required for testing VK_EXT_sampler_filter_minmax
        VK_FORMAT_R16_UNORM,
        VK_FORMAT_R16_SNORM,
        VK_FORMAT_R32_SFLOAT,

        // Scaled formats
        VK_FORMAT_R8G8B8A8_SSCALED,
        VK_FORMAT_A2R10G10B10_USCALED_PACK32,

        // Compressed formats
        VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,
        VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,
        VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
        VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,
        VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
        VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,
        VK_FORMAT_EAC_R11_UNORM_BLOCK,
        VK_FORMAT_EAC_R11_SNORM_BLOCK,
        VK_FORMAT_EAC_R11G11_UNORM_BLOCK,
        VK_FORMAT_EAC_R11G11_SNORM_BLOCK,
        VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
        VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
        VK_FORMAT_ASTC_6x5_UNORM_BLOCK,
        VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
        VK_FORMAT_ASTC_8x6_UNORM_BLOCK,
        VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
        VK_FORMAT_ASTC_10x6_UNORM_BLOCK,
        VK_FORMAT_ASTC_10x8_SRGB_BLOCK,
        VK_FORMAT_ASTC_12x10_UNORM_BLOCK,
        VK_FORMAT_ASTC_12x12_SRGB_BLOCK,

        // Depth formats required for testing VK_EXT_sampler_filter_minmax
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_X8_D24_UNORM_PACK32,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
    };

    de::MovePtr<tcu::TestCaseGroup> viewTypeTests(new tcu::TestCaseGroup(testCtx, "view_type"));

    for (int viewTypeNdx = 0; viewTypeNdx < DE_LENGTH_OF_ARRAY(imageViewTypes); viewTypeNdx++)
    {
        const SamplerViewType viewType = imageViewTypes[viewTypeNdx].type;
        de::MovePtr<tcu::TestCaseGroup> viewTypeGroup(
            new tcu::TestCaseGroup(testCtx, imageViewTypes[viewTypeNdx].name));
        de::MovePtr<tcu::TestCaseGroup> formatTests(new tcu::TestCaseGroup(testCtx, "format"));

        for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
        {
            const VkFormat format     = formats[formatNdx];
            const bool isCompressed   = isCompressedFormat(format);
            const bool isDepthStencil = !isCompressed && tcu::hasDepthComponent(mapVkFormat(format).order) &&
                                        tcu::hasStencilComponent(mapVkFormat(format).order);
            if (isCompressed)
            {
                // Do not use compressed formats with 1D and 1D array textures.
                if (viewType == VK_IMAGE_VIEW_TYPE_1D || viewType == VK_IMAGE_VIEW_TYPE_1D_ARRAY)
                    break;
            }

            if (separateStencilUsage && !isDepthStencil)
                continue;

            de::MovePtr<tcu::TestCaseGroup> formatGroup(
                new tcu::TestCaseGroup(testCtx, getFormatCaseName(format).c_str()));

            if (!isCompressed && viewType.isNormalized())
            {
                // Do not include minFilter tests with compressed formats.
                // Randomly generated compressed textures are too noisy and will derive in false positives.
                de::MovePtr<tcu::TestCaseGroup> minFilterTests = createSamplerMinFilterTests(
                    testCtx, pipelineConstructionType, viewType, format, separateStencilUsage);
                de::MovePtr<tcu::TestCaseGroup> minReduceFilterTests = createSamplerMinReduceFilterTests(
                    testCtx, pipelineConstructionType, viewType, format, separateStencilUsage);
                formatGroup->addChild(minFilterTests.release());
                formatGroup->addChild(minReduceFilterTests.release());
            }

            de::MovePtr<tcu::TestCaseGroup> magFilterTests =
                createSamplerMagFilterTests(testCtx, pipelineConstructionType, viewType, format, separateStencilUsage);
            formatGroup->addChild(magFilterTests.release());

            if (viewType.isNormalized())
            {
                de::MovePtr<tcu::TestCaseGroup> magReduceFilterTests = createSamplerMagReduceFilterTests(
                    testCtx, pipelineConstructionType, viewType, format, separateStencilUsage);
                de::MovePtr<tcu::TestCaseGroup> mipmapTests =
                    createSamplerMipmapTests(testCtx, pipelineConstructionType, viewType, format, separateStencilUsage);

                formatGroup->addChild(magReduceFilterTests.release());
                formatGroup->addChild(mipmapTests.release());
            }

            if (viewType != VK_IMAGE_VIEW_TYPE_CUBE && viewType != VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
            {
                de::MovePtr<tcu::TestCaseGroup> addressModesTests = createSamplerAddressModesTests(
                    testCtx, pipelineConstructionType, viewType, format, separateStencilUsage);
                formatGroup->addChild(addressModesTests.release());
            }

            formatTests->addChild(formatGroup.release());
        }

        viewTypeGroup->addChild(formatTests.release());
        viewTypeTests->addChild(viewTypeGroup.release());
    }

    return viewTypeTests.release();
}

tcu::TestCaseGroup *createExactSamplingTests(tcu::TestContext &testCtx,
                                             PipelineConstructionType pipelineConstructionType)
{
    de::MovePtr<tcu::TestCaseGroup> exactSamplingTests(new tcu::TestCaseGroup(testCtx, "exact_sampling"));

    static const std::vector<vk::VkFormat> formats = {
        vk::VK_FORMAT_R8_SRGB,          vk::VK_FORMAT_R8G8B8_UINT,       vk::VK_FORMAT_B8G8R8A8_SINT,
        vk::VK_FORMAT_R8G8_UNORM,       vk::VK_FORMAT_B8G8R8_SNORM,      vk::VK_FORMAT_R8G8B8A8_SNORM,
        vk::VK_FORMAT_R8G8_UINT,        vk::VK_FORMAT_R8_SINT,           vk::VK_FORMAT_R8G8B8A8_SRGB,
        vk::VK_FORMAT_R8G8B8A8_UNORM,   vk::VK_FORMAT_B8G8R8A8_UNORM,    vk::VK_FORMAT_B8G8R8_SRGB,
        vk::VK_FORMAT_R8G8_SRGB,        vk::VK_FORMAT_R8_UINT,           vk::VK_FORMAT_R8G8B8A8_UINT,
        vk::VK_FORMAT_R8G8_SINT,        vk::VK_FORMAT_R8_SNORM,          vk::VK_FORMAT_B8G8R8_SINT,
        vk::VK_FORMAT_R8G8_SNORM,       vk::VK_FORMAT_B8G8R8_UNORM,      vk::VK_FORMAT_R8_UNORM,
#ifndef CTS_USES_VULKANSC
        vk::VK_FORMAT_A8_UNORM_KHR,
#endif // CTS_USES_VULKANSC

        vk::VK_FORMAT_R32G32_SFLOAT,    vk::VK_FORMAT_R32G32B32_UINT,    vk::VK_FORMAT_R16G16B16A16_SFLOAT,
        vk::VK_FORMAT_R16G16_UINT,      vk::VK_FORMAT_R32G32B32A32_SINT, vk::VK_FORMAT_R16G16B16_SINT,
        vk::VK_FORMAT_R16_SFLOAT,       vk::VK_FORMAT_R32_SINT,          vk::VK_FORMAT_R32_UINT,
        vk::VK_FORMAT_R16G16B16_SFLOAT, vk::VK_FORMAT_R16G16_SINT,

        vk::VK_FORMAT_R16_SNORM,        vk::VK_FORMAT_R32_SFLOAT,
    };

    static const struct
    {
        const bool unnormalized;
        const std::string name;
    } unnormalizedCoordinates[] = {
        // Normalized coordinates
        {false, "normalized_coords"},
        // Unnormalized coordinates
        {true, "unnormalized_coords"},
    };

    static const struct
    {
        const tcu::Maybe<float> offset;
        const std::string name;
    } testEdges[] = {
        // Sampling points centered in texel
        {tcu::Nothing, "centered"},
        // Sampling points near left edge
        {tcu::just<float>(-1.0f), "edge_left"},
        // Sampling points near right edge
        {tcu::just<float>(+1.0f), "edge_right"},
    };

    static const std::vector<std::pair<bool, std::string>> solidColor = {
        {false, "gradient"},
        {true, "solid_color"},
    };

    for (const auto format : formats)
    {
        const std::string formatName = getFormatCaseName(format);

        de::MovePtr<tcu::TestCaseGroup> formatGroup(new tcu::TestCaseGroup(testCtx, formatName.c_str()));

        for (const auto &solid : solidColor)
        {
            de::MovePtr<tcu::TestCaseGroup> solidColorGroup(new tcu::TestCaseGroup(testCtx, solid.second.c_str()));

            for (int unIdx = 0; unIdx < DE_LENGTH_OF_ARRAY(unnormalizedCoordinates); ++unIdx)
            {
                const auto &unnorm = unnormalizedCoordinates[unIdx];
                de::MovePtr<tcu::TestCaseGroup> coordGroup(new tcu::TestCaseGroup(testCtx, unnorm.name.c_str()));

                for (int edgeIdx = 0; edgeIdx < DE_LENGTH_OF_ARRAY(testEdges); ++edgeIdx)
                {
                    const auto &edges                      = testEdges[edgeIdx];
                    const ExactSamplingCase::Params params = {pipelineConstructionType, format, unnorm.unnormalized,
                                                              solid.first, edges.offset};
                    coordGroup->addChild(new ExactSamplingCase{testCtx, edges.name, params});
                }

                solidColorGroup->addChild(coordGroup.release());
            }

            formatGroup->addChild(solidColorGroup.release());
        }

        exactSamplingTests->addChild(formatGroup.release());
    }

    return exactSamplingTests.release();
}

tcu::TestCaseGroup *createMaxSamplerLodBiasTests(tcu::TestContext &testCtx,
                                                 PipelineConstructionType pipelineConstructionType)
{
    de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "max_sampler_lod_bias"));

    for (int caseIdx = 0; caseIdx < static_cast<int>(LodBiasCase::CASE_COUNT); ++caseIdx)
    {
        const auto caseValue = static_cast<LodBiasCase>(caseIdx);
        const auto testName  = getLodBiasCaseName(caseValue);
        const MaxSamplerLodBiasParams params{pipelineConstructionType, caseValue};

#ifdef CTS_USES_VULKANSC
        if (caseValue == LodBiasCase::VIEW_MINLOD)
            continue;
#endif // CTS_USES_VULKANSC

        testGroup->addChild(new MaxSamplerLodBiasCase(testCtx, testName, params));
    }

    return testGroup.release();
}

tcu::TestCaseGroup *createSamplerTests(tcu::TestContext &testCtx, PipelineConstructionType pipelineConstructionType)
{
    const auto genAllTests = (pipelineConstructionType == vk::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC ||
                              pipelineConstructionType == vk::PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV);

    de::MovePtr<tcu::TestCaseGroup> samplerTests(new tcu::TestCaseGroup(testCtx, "sampler"));
    {
        if (genAllTests)
            samplerTests->addChild(createAllFormatsSamplerTests(testCtx, pipelineConstructionType));
        samplerTests->addChild(createExactSamplingTests(testCtx, pipelineConstructionType));
    }

    // tests for VK_EXT_separate_stencil_usage
    de::MovePtr<tcu::TestCaseGroup> separateStencilUsageSamplerTests(
        new tcu::TestCaseGroup(testCtx, "separate_stencil_usage"));
    {
        separateStencilUsageSamplerTests->addChild(
            createAllFormatsSamplerTests(testCtx, pipelineConstructionType, true));
        samplerTests->addChild(separateStencilUsageSamplerTests.release());
    }

#ifndef CTS_USES_VULKANSC
    // Border color swizzle tests.
    if (genAllTests)
        samplerTests->addChild(createSamplerBorderSwizzleTests(testCtx, pipelineConstructionType));
#endif // CTS_USES_VULKANSC

    samplerTests->addChild(createMaxSamplerLodBiasTests(testCtx, pipelineConstructionType));

    return samplerTests.release();
}

} // namespace pipeline
} // namespace vkt
