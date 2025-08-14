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
 * \brief Blend Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineBlendTests.hpp"
#include "vktPipelineDualBlendTests.hpp"
#include "vktPipelineBlendTestsCommon.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktPipelineUniqueRandomIterator.hpp"
#include "vktPipelineReferenceRenderer.hpp"
#include "vktTestCase.hpp"
#include "vkImageUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuPlatform.hpp"
#include "tcuTextureUtil.hpp"
#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include <cstring>
#include <set>
#include <sstream>
#include <vector>
#include <algorithm>
#include <iterator>

namespace vkt
{
namespace pipeline
{

using namespace vk;
using namespace blending_common;

namespace
{

// VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, with VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT (e.g. when using shader objects), has
// the following rule (Vulkan spec 1.4.313):
//
// If a shader object is bound to any graphics stage or the bound graphics pipeline was created with
// VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT, and the format of any color attachment is VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
// the corresponding element of the pColorWriteMasks parameter of vkCmdSetColorWriteMaskEXT must either include all of
// VK_COLOR_COMPONENT_R_BIT, VK_COLOR_COMPONENT_G_BIT, and VK_COLOR_COMPONENT_B_BIT, or none of them
bool mustIncludeRGBDynamicMask(VkFormat format)
{
    return (format == VK_FORMAT_E5B9G9R9_UFLOAT_PACK32);
}

class BlendStateUniqueRandomIterator : public UniqueRandomIterator<VkPipelineColorBlendAttachmentState>
{
public:
    BlendStateUniqueRandomIterator(uint32_t numberOfCombinations, int seed);
    virtual ~BlendStateUniqueRandomIterator(void)
    {
    }
    VkPipelineColorBlendAttachmentState getIndexedValue(uint32_t index);

private:
    const static inline std::vector<VkBlendFactor> &m_blendFactors = getBlendFactors();
    const static inline std::vector<VkBlendOp> &m_blendOps         = getBlendOps();

    // Pre-calculated constants
    const static uint32_t m_blendFactorsLength;
    const static uint32_t m_blendFactorsLength2;
    const static uint32_t m_blendFactorsLength3;
    const static uint32_t m_blendFactorsLength4;
    const static uint32_t m_blendOpsLength;

    // Total number of cross-combinations of (srcBlendColor x destBlendColor x blendOpColor x srcBlendAlpha x destBlendAlpha x blendOpAlpha)
    const static uint32_t m_totalBlendStates;
};

class BlendStateUniqueRandomIteratorDualSource : public UniqueRandomIterator<VkPipelineColorBlendAttachmentState>
{
public:
    BlendStateUniqueRandomIteratorDualSource(uint32_t numberOfCombinations, int seed);
    virtual ~BlendStateUniqueRandomIteratorDualSource(void)
    {
    }
    VkPipelineColorBlendAttachmentState getIndexedValue(uint32_t index);

private:
    const static inline std::vector<VkBlendFactor> m_blendFactors = getBlendWithDualSourceFactors();
    const static inline std::vector<VkBlendOp> m_blendOps         = getBlendOps();

    // Pre-calculated constants
    const static uint32_t m_blendFactorsLength;
    const static uint32_t m_blendFactorsLength2;
    const static uint32_t m_blendFactorsLength3;
    const static uint32_t m_blendFactorsLength4;
    const static uint32_t m_blendOpsLength;

    // Total number of cross-combinations of (srcBlendColor x destBlendColor x blendOpColor x srcBlendAlpha x destBlendAlpha x blendOpAlpha)
    const static uint32_t m_totalBlendStates;
};

class BlendTest : public vkt::TestCase
{
public:
    enum
    {
        QUAD_COUNT = 4
    };

    const static VkColorComponentFlags s_colorWriteMasks[QUAD_COUNT];
    const static VkColorComponentFlags s_altColorWriteMasks[QUAD_COUNT];
    const static tcu::Vec4 s_blendConst;

    BlendTest(tcu::TestContext &testContext, const std::string &name, PipelineConstructionType pipelineConstructionType,
              const VkFormat colorFormat, const VkPipelineColorBlendAttachmentState blendStates[QUAD_COUNT]);
    virtual ~BlendTest(void);
    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual void checkSupport(Context &context) const;
    virtual TestInstance *createInstance(Context &context) const;

private:
    const PipelineConstructionType m_pipelineConstructionType;
    const VkFormat m_colorFormat;
    VkPipelineColorBlendAttachmentState m_blendStates[QUAD_COUNT];
};

class DualSourceBlendTest : public vkt::TestCase
{
public:
    enum
    {
        QUAD_COUNT = 4
    };

    const static VkColorComponentFlags s_colorWriteMasks[QUAD_COUNT];
    const static VkColorComponentFlags s_altColorWriteMasks[QUAD_COUNT];
    const static tcu::Vec4 s_blendConst;

    DualSourceBlendTest(tcu::TestContext &testContext, const std::string &name,
                        PipelineConstructionType pipelineConstructionType, const VkFormat colorFormat,
                        const VkPipelineColorBlendAttachmentState blendStates[QUAD_COUNT],
                        const bool shaderOutputInArray);
    virtual ~DualSourceBlendTest(void);
    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual void checkSupport(Context &context) const;
    virtual TestInstance *createInstance(Context &context) const;

private:
    const PipelineConstructionType m_pipelineConstructionType;
    const VkFormat m_colorFormat;
    const bool m_shaderOutputInArray;
    VkPipelineColorBlendAttachmentState m_blendStates[QUAD_COUNT];
};

class BlendTestInstance : public vkt::TestInstance
{
public:
    BlendTestInstance(Context &context, PipelineConstructionType pipelineConstructionType, const VkFormat colorFormat,
                      const VkPipelineColorBlendAttachmentState blendStates[BlendTest::QUAD_COUNT]);
    virtual ~BlendTestInstance(void);
    virtual tcu::TestStatus iterate(void);

private:
    tcu::TestStatus verifyImage(void);

    VkPipelineColorBlendAttachmentState m_blendStates[BlendTest::QUAD_COUNT];

    const tcu::UVec2 m_renderSize;
    const VkFormat m_colorFormat;

    VkImageCreateInfo m_colorImageCreateInfo;
    Move<VkImage> m_colorImage;
    de::MovePtr<Allocation> m_colorImageAlloc;
    Move<VkImageView> m_colorAttachmentView;
    RenderPassWrapper m_renderPass;
    Move<VkFramebuffer> m_framebuffer;

    ShaderWrapper m_vertexShaderModule;
    ShaderWrapper m_fragmentShaderModule;

    Move<VkBuffer> m_vertexBuffer;
    std::vector<Vertex4RGBA> m_vertices;
    de::MovePtr<Allocation> m_vertexBufferAlloc;

    PipelineLayoutWrapper m_pipelineLayout;
    GraphicsPipelineWrapper m_graphicsPipelines[BlendTest::QUAD_COUNT];

    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBuffer;
};

// Blend test dual source blending
class DualSourceBlendTestInstance : public vkt::TestInstance
{
public:
    DualSourceBlendTestInstance(Context &context, const PipelineConstructionType pipelineConstructionType,
                                const VkFormat colorFormat,
                                const VkPipelineColorBlendAttachmentState blendStates[DualSourceBlendTest::QUAD_COUNT]);
    virtual ~DualSourceBlendTestInstance(void);
    virtual tcu::TestStatus iterate(void);

private:
    tcu::TestStatus verifyImage(void);

    VkPipelineColorBlendAttachmentState m_blendStates[DualSourceBlendTest::QUAD_COUNT];

    const tcu::UVec2 m_renderSize;
    const VkFormat m_colorFormat;

    VkImageCreateInfo m_colorImageCreateInfo;
    Move<VkImage> m_colorImage;
    de::MovePtr<Allocation> m_colorImageAlloc;
    Move<VkImageView> m_colorAttachmentView;
    RenderPassWrapper m_renderPass;
    Move<VkFramebuffer> m_framebuffer;

    ShaderWrapper m_vertexShaderModule;
    ShaderWrapper m_fragmentShaderModule;

    Move<VkBuffer> m_vertexBuffer;
    std::vector<Vertex4RGBARGBA> m_vertices;
    de::MovePtr<Allocation> m_vertexBufferAlloc;

    PipelineLayoutWrapper m_pipelineLayout;
    GraphicsPipelineWrapper m_graphicsPipelines[DualSourceBlendTest::QUAD_COUNT];

    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBuffer;
};

// BlendStateUniqueRandomIterator

const uint32_t BlendStateUniqueRandomIterator::m_blendFactorsLength  = uint32_t(m_blendFactors.size());
const uint32_t BlendStateUniqueRandomIterator::m_blendFactorsLength2 = m_blendFactorsLength * m_blendFactorsLength;
const uint32_t BlendStateUniqueRandomIterator::m_blendFactorsLength3 = m_blendFactorsLength2 * m_blendFactorsLength;
const uint32_t BlendStateUniqueRandomIterator::m_blendFactorsLength4 = m_blendFactorsLength3 * m_blendFactorsLength;
const uint32_t BlendStateUniqueRandomIterator::m_blendOpsLength      = uint32_t(m_blendOps.size());
const uint32_t BlendStateUniqueRandomIterator::m_totalBlendStates =
    m_blendFactorsLength4 * m_blendOpsLength * m_blendOpsLength;

BlendStateUniqueRandomIterator::BlendStateUniqueRandomIterator(uint32_t numberOfCombinations, int seed)
    : UniqueRandomIterator<VkPipelineColorBlendAttachmentState>(numberOfCombinations, m_totalBlendStates, seed)
{
}

VkPipelineColorBlendAttachmentState BlendStateUniqueRandomIterator::getIndexedValue(uint32_t index)
{
    const uint32_t blendOpAlphaIndex    = index / (m_blendFactorsLength4 * m_blendOpsLength);
    const uint32_t blendOpAlphaSeqIndex = blendOpAlphaIndex * (m_blendFactorsLength4 * m_blendOpsLength);

    const uint32_t destBlendAlphaIndex    = (index - blendOpAlphaSeqIndex) / (m_blendFactorsLength3 * m_blendOpsLength);
    const uint32_t destBlendAlphaSeqIndex = destBlendAlphaIndex * (m_blendFactorsLength3 * m_blendOpsLength);

    const uint32_t srcBlendAlphaIndex =
        (index - blendOpAlphaSeqIndex - destBlendAlphaSeqIndex) / (m_blendFactorsLength2 * m_blendOpsLength);
    const uint32_t srcBlendAlphaSeqIndex = srcBlendAlphaIndex * (m_blendFactorsLength2 * m_blendOpsLength);

    const uint32_t blendOpColorIndex =
        (index - blendOpAlphaSeqIndex - destBlendAlphaSeqIndex - srcBlendAlphaSeqIndex) / m_blendFactorsLength2;
    const uint32_t blendOpColorSeqIndex = blendOpColorIndex * m_blendFactorsLength2;

    const uint32_t destBlendColorIndex =
        (index - blendOpAlphaSeqIndex - destBlendAlphaSeqIndex - srcBlendAlphaSeqIndex - blendOpColorSeqIndex) /
        m_blendFactorsLength;
    const uint32_t destBlendColorSeqIndex = destBlendColorIndex * m_blendFactorsLength;

    const uint32_t srcBlendColorIndex = index - blendOpAlphaSeqIndex - destBlendAlphaSeqIndex - srcBlendAlphaSeqIndex -
                                        blendOpColorSeqIndex - destBlendColorSeqIndex;

    const VkPipelineColorBlendAttachmentState blendAttachmentState = {
        true,                                                 // VkBool32 blendEnable;
        m_blendFactors[srcBlendColorIndex],                   // VkBlendFactor srcColorBlendFactor;
        m_blendFactors[destBlendColorIndex],                  // VkBlendFactor dstColorBlendFactor;
        m_blendOps[blendOpColorIndex],                        // VkBlendOp colorBlendOp;
        m_blendFactors[srcBlendAlphaIndex],                   // VkBlendFactor srcAlphaBlendFactor;
        m_blendFactors[destBlendAlphaIndex],                  // VkBlendFactor dstAlphaBlendFactor;
        m_blendOps[blendOpAlphaIndex],                        // VkBlendOp alphaBlendOp;
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | // VkColorComponentFlags colorWriteMask;
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

    return blendAttachmentState;
}

// BlendStateUniqueRandomIteratorDualSource

const uint32_t BlendStateUniqueRandomIteratorDualSource::m_blendFactorsLength = uint32_t(m_blendFactors.size());
const uint32_t BlendStateUniqueRandomIteratorDualSource::m_blendFactorsLength2 =
    m_blendFactorsLength * m_blendFactorsLength;
const uint32_t BlendStateUniqueRandomIteratorDualSource::m_blendFactorsLength3 =
    m_blendFactorsLength2 * m_blendFactorsLength;
const uint32_t BlendStateUniqueRandomIteratorDualSource::m_blendFactorsLength4 =
    m_blendFactorsLength3 * m_blendFactorsLength;
const uint32_t BlendStateUniqueRandomIteratorDualSource::m_blendOpsLength = uint32_t(m_blendOps.size());
const uint32_t BlendStateUniqueRandomIteratorDualSource::m_totalBlendStates =
    m_blendFactorsLength4 * m_blendOpsLength * m_blendOpsLength;

BlendStateUniqueRandomIteratorDualSource::BlendStateUniqueRandomIteratorDualSource(uint32_t numberOfCombinations,
                                                                                   int seed)
    : UniqueRandomIterator<VkPipelineColorBlendAttachmentState>(numberOfCombinations, m_totalBlendStates, seed)
{
}

VkPipelineColorBlendAttachmentState BlendStateUniqueRandomIteratorDualSource::getIndexedValue(uint32_t index)
{
    const uint32_t blendOpAlphaIndex    = index / (m_blendFactorsLength4 * m_blendOpsLength);
    const uint32_t blendOpAlphaSeqIndex = blendOpAlphaIndex * (m_blendFactorsLength4 * m_blendOpsLength);

    const uint32_t destBlendAlphaIndex    = (index - blendOpAlphaSeqIndex) / (m_blendFactorsLength3 * m_blendOpsLength);
    const uint32_t destBlendAlphaSeqIndex = destBlendAlphaIndex * (m_blendFactorsLength3 * m_blendOpsLength);

    const uint32_t srcBlendAlphaIndex =
        (index - blendOpAlphaSeqIndex - destBlendAlphaSeqIndex) / (m_blendFactorsLength2 * m_blendOpsLength);
    const uint32_t srcBlendAlphaSeqIndex = srcBlendAlphaIndex * (m_blendFactorsLength2 * m_blendOpsLength);

    const uint32_t blendOpColorIndex =
        (index - blendOpAlphaSeqIndex - destBlendAlphaSeqIndex - srcBlendAlphaSeqIndex) / m_blendFactorsLength2;
    const uint32_t blendOpColorSeqIndex = blendOpColorIndex * m_blendFactorsLength2;

    const uint32_t destBlendColorIndex =
        (index - blendOpAlphaSeqIndex - destBlendAlphaSeqIndex - srcBlendAlphaSeqIndex - blendOpColorSeqIndex) /
        m_blendFactorsLength;
    const uint32_t destBlendColorSeqIndex = destBlendColorIndex * m_blendFactorsLength;

    const uint32_t srcBlendColorIndex = index - blendOpAlphaSeqIndex - destBlendAlphaSeqIndex - srcBlendAlphaSeqIndex -
                                        blendOpColorSeqIndex - destBlendColorSeqIndex;

    const VkPipelineColorBlendAttachmentState blendAttachmentState = {
        true,                                                 // VkBool32 blendEnable;
        m_blendFactors[srcBlendColorIndex],                   // VkBlendFactor srcColorBlendFactor;
        m_blendFactors[destBlendColorIndex],                  // VkBlendFactor dstColorBlendFactor;
        m_blendOps[blendOpColorIndex],                        // VkBlendOp colorBlendOp;
        m_blendFactors[srcBlendAlphaIndex],                   // VkBlendFactor srcAlphaBlendFactor;
        m_blendFactors[destBlendAlphaIndex],                  // VkBlendFactor dstAlphaBlendFactor;
        m_blendOps[blendOpAlphaIndex],                        // VkBlendOp alphaBlendOp;
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | // VkColorComponentFlags colorWriteMask;
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

    return blendAttachmentState;
}

// BlendTest

const VkColorComponentFlags BlendTest::s_colorWriteMasks[BlendTest::QUAD_COUNT] = {
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT, // Pair of channels: R & G
    VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT, // Pair of channels: G & B
    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, // Pair of channels: B & A
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT}; // All channels

// See mustIncludeRGBDynamicMask
const VkColorComponentFlags BlendTest::s_altColorWriteMasks[BlendTest::QUAD_COUNT] = {
    (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT),
    (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT),
    (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT),
    (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT),
};

const tcu::Vec4 BlendTest::s_blendConst = tcu::Vec4(0.1f, 0.2f, 0.3f, 0.4f);

BlendTest::BlendTest(tcu::TestContext &testContext, const std::string &name,
                     PipelineConstructionType pipelineConstructionType, const VkFormat colorFormat,
                     const VkPipelineColorBlendAttachmentState blendStates[QUAD_COUNT])
    : vkt::TestCase(testContext, name)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_colorFormat(colorFormat)
{
    deMemcpy(m_blendStates, blendStates, sizeof(VkPipelineColorBlendAttachmentState) * QUAD_COUNT);
}

BlendTest::~BlendTest(void)
{
}

TestInstance *BlendTest::createInstance(Context &context) const
{
    return new BlendTestInstance(context, m_pipelineConstructionType, m_colorFormat, m_blendStates);
}

void BlendTest::checkSupport(Context &context) const
{
    if (!isSupportedBlendFormat(context.getInstanceInterface(), context.getPhysicalDevice(), m_colorFormat))
        throw tcu::NotSupportedError(std::string("Unsupported color blending format: ") + getFormatName(m_colorFormat));

    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_pipelineConstructionType);
#ifndef CTS_USES_VULKANSC
    if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
        !context.getPortabilitySubsetFeatures().constantAlphaColorBlendFactors)
    {
        int quadNdx = 0;
        for (; quadNdx < BlendTest::QUAD_COUNT; quadNdx++)
        {
            const VkPipelineColorBlendAttachmentState &blendState = m_blendStates[quadNdx];
            if (blendState.srcColorBlendFactor == VK_BLEND_FACTOR_CONSTANT_ALPHA ||
                blendState.dstColorBlendFactor == VK_BLEND_FACTOR_CONSTANT_ALPHA ||
                blendState.srcColorBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA ||
                blendState.dstColorBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA)
            {
                break;
            }
        }
        if (quadNdx < BlendTest::QUAD_COUNT)
            TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Constant alpha color blend factors are not "
                                         "supported by this implementation");
    }
#endif // CTS_USES_VULKANSC
}

void BlendTest::initPrograms(SourceCollections &sourceCollections) const
{
    std::ostringstream fragmentSource;

    sourceCollections.glslSources.add("color_vert")
        << glu::VertexSource("#version 310 es\n"
                             "layout(location = 0) in highp vec4 position;\n"
                             "layout(location = 1) in highp vec4 color;\n"
                             "layout(location = 0) out highp vec4 vtxColor;\n"
                             "void main (void)\n"
                             "{\n"
                             "    gl_Position = position;\n"
                             "    vtxColor = color;\n"
                             "}\n");

    fragmentSource << "#version 310 es\n"
                      "layout(location = 0) in highp vec4 vtxColor;\n"
                      "layout(location = 0) out highp vec4 fragColor;\n"
                      "void main (void)\n"
                      "{\n"
                      "    fragColor = vtxColor;\n"
                      "}\n";

    sourceCollections.glslSources.add("color_frag") << glu::FragmentSource(fragmentSource.str());
}

// DualSourceBlendTest

const VkColorComponentFlags DualSourceBlendTest::s_colorWriteMasks[BlendTest::QUAD_COUNT] = {
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT, // Pair of channels: R & G
    VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT, // Pair of channels: G & B
    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, // Pair of channels: B & A
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT}; // All channels

// See mustIncludeRGBDynamicMask
const VkColorComponentFlags DualSourceBlendTest::s_altColorWriteMasks[BlendTest::QUAD_COUNT] = {
    (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT),
    (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT),
    (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT),
    (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT),
};

const tcu::Vec4 DualSourceBlendTest::s_blendConst = tcu::Vec4(0.1f, 0.2f, 0.3f, 0.4f);

DualSourceBlendTest::DualSourceBlendTest(tcu::TestContext &testContext, const std::string &name,
                                         PipelineConstructionType pipelineConstructionType, const VkFormat colorFormat,
                                         const VkPipelineColorBlendAttachmentState blendStates[QUAD_COUNT],
                                         const bool shaderOutputInArray)
    : vkt::TestCase(testContext, name)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_colorFormat(colorFormat)
    , m_shaderOutputInArray(shaderOutputInArray)
{
    deMemcpy(m_blendStates, blendStates, sizeof(VkPipelineColorBlendAttachmentState) * QUAD_COUNT);
}

DualSourceBlendTest::~DualSourceBlendTest(void)
{
}

TestInstance *DualSourceBlendTest::createInstance(Context &context) const
{
    return new DualSourceBlendTestInstance(context, m_pipelineConstructionType, m_colorFormat, m_blendStates);
}

void DualSourceBlendTest::checkSupport(Context &context) const
{
    const vk::VkPhysicalDeviceFeatures features = context.getDeviceFeatures();

    bool isDualSourceTest = false;
    for (int quadNdx = 0; quadNdx < BlendTest::QUAD_COUNT; quadNdx++)
    {
        isDualSourceTest = isSrc1BlendFactor(this->m_blendStates[quadNdx].srcColorBlendFactor) ||
                           isSrc1BlendFactor(this->m_blendStates[quadNdx].dstColorBlendFactor) ||
                           isSrc1BlendFactor(this->m_blendStates[quadNdx].srcAlphaBlendFactor) ||
                           isSrc1BlendFactor(this->m_blendStates[quadNdx].dstAlphaBlendFactor);
        if (isDualSourceTest)
            break;
    }
    if (isDualSourceTest && !features.dualSrcBlend)
        throw tcu::NotSupportedError("Dual-Source blending not supported");

    if (!isSupportedBlendFormat(context.getInstanceInterface(), context.getPhysicalDevice(), m_colorFormat))
        throw tcu::NotSupportedError(std::string("Unsupported color blending format: ") + getFormatName(m_colorFormat));
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_pipelineConstructionType);
}

void DualSourceBlendTest::initPrograms(SourceCollections &sourceCollections) const
{
    sourceCollections.glslSources.add("color_vert")
        << glu::VertexSource("#version 450\n"
                             "layout(location = 0) in highp vec4 position;\n"
                             "layout(location = 1) in highp vec4 color0;\n"
                             "layout(location = 2) in highp vec4 color1;\n"
                             "layout(location = 0) out highp vec4 vtxColor0;\n"
                             "layout(location = 1) out highp vec4 vtxColor1;\n"
                             "void main (void)\n"
                             "{\n"
                             "    gl_Position = position;\n"
                             "    vtxColor0 = color0;\n"
                             "    vtxColor1 = color1;\n"
                             "}\n");

    const char *fragmentSourceOutputVariable = "#version 450\n"
                                               "layout(location = 0) in highp vec4 vtxColor0;\n"
                                               "layout(location = 1) in highp vec4 vtxColor1;\n"
                                               "layout(location = 0, index = 0) out highp vec4 fragColor0;\n"
                                               "layout(location = 0, index = 1) out highp vec4 fragColor1;\n"
                                               "void main (void)\n"
                                               "{\n"
                                               "    fragColor0 = vtxColor0;\n"
                                               "    fragColor1 = vtxColor1;\n"
                                               "   if (int(gl_FragCoord.x) == 2 || int(gl_FragCoord.y) == 3)\n"
                                               "      discard;\n"
                                               "}\n";

    const char *fragmentSourceOutputArray = "#version 450\n"
                                            "layout(location = 0) in highp vec4 vtxColor0;\n"
                                            "layout(location = 1) in highp vec4 vtxColor1;\n"
                                            "layout(location = 0, index = 0) out highp vec4 fragColor0[1];\n"
                                            "layout(location = 0, index = 1) out highp vec4 fragColor1[1];\n"
                                            "void main (void)\n"
                                            "{\n"
                                            "    fragColor0[0] = vtxColor0;\n"
                                            "    fragColor1[0] = vtxColor1;\n"
                                            "   if (int(gl_FragCoord.x) == 2 || int(gl_FragCoord.y) == 3)\n"
                                            "      discard;\n"
                                            "}\n";

    sourceCollections.glslSources.add("color_frag")
        << glu::FragmentSource(m_shaderOutputInArray ? fragmentSourceOutputArray : fragmentSourceOutputVariable);
}

// BlendTestInstance

BlendTestInstance::BlendTestInstance(Context &context, const PipelineConstructionType pipelineConstructionType,
                                     const VkFormat colorFormat,
                                     const VkPipelineColorBlendAttachmentState blendStates[BlendTest::QUAD_COUNT])
    : vkt::TestInstance(context)
    , m_renderSize(32, 32)
    , m_colorFormat(colorFormat)
    , m_graphicsPipelines{{context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                           context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType},
                          {context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                           context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType},
                          {context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                           context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType},
                          {context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                           context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType}}
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice vkDevice         = m_context.getDevice();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    SimpleAllocator memAlloc(
        vk, vkDevice,
        getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));

    // Copy depth operators
    deMemcpy(m_blendStates, blendStates, sizeof(VkPipelineColorBlendAttachmentState) * BlendTest::QUAD_COUNT);

    // Create color image
    {
        const VkImageCreateInfo colorImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                   // VkStructureType sType;
            nullptr,                                                               // const void* pNext;
            0u,                                                                    // VkImageCreateFlags flags;
            VK_IMAGE_TYPE_2D,                                                      // VkImageType imageType;
            m_colorFormat,                                                         // VkFormat format;
            {m_renderSize.x(), m_renderSize.y(), 1u},                              // VkExtent3D extent;
            1u,                                                                    // uint32_t mipLevels;
            1u,                                                                    // uint32_t arrayLayers;
            VK_SAMPLE_COUNT_1_BIT,                                                 // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,                                               // VkImageTiling tiling;
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                             // VkSharingMode sharingMode;
            1u,                                                                    // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex,        // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED // VkImageLayout initialLayout;
        };

        m_colorImageCreateInfo = colorImageParams;
        m_colorImage           = createImage(vk, vkDevice, &m_colorImageCreateInfo);

        // Allocate and bind color image memory
        m_colorImageAlloc =
            memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImage), MemoryRequirement::Any);
        VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImage, m_colorImageAlloc->getMemory(),
                                    m_colorImageAlloc->getOffset()));
    }

    // Create color attachment view
    {
        const VkImageViewCreateInfo colorAttachmentViewParams = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
            nullptr,                                  // const void* pNext;
            0u,                                       // VkImageViewCreateFlags flags;
            *m_colorImage,                            // VkImage image;
            VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
            m_colorFormat,                            // VkFormat format;
            {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
             VK_COMPONENT_SWIZZLE_IDENTITY},
            {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
        };

        m_colorAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
    }

    // Create render pass
    m_renderPass = RenderPassWrapper(pipelineConstructionType, vk, vkDevice, m_colorFormat);

    // Create framebuffer
    {
        const VkFramebufferCreateInfo framebufferParams = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                                   // const void* pNext;
            0u,                                        // VkFramebufferCreateFlags flags;
            *m_renderPass,                             // VkRenderPass renderPass;
            1u,                                        // uint32_t attachmentCount;
            &m_colorAttachmentView.get(),              // const VkImageView* pAttachments;
            (uint32_t)m_renderSize.x(),                // uint32_t width;
            (uint32_t)m_renderSize.y(),                // uint32_t height;
            1u                                         // uint32_t layers;
        };

        m_renderPass.createFramebuffer(vk, vkDevice, &framebufferParams, *m_colorImage);
    }

    // Create pipeline layout
    {
        const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
            nullptr,                                       // const void* pNext;
            0u,                                            // VkPipelineLayoutCreateFlags flags;
            0u,                                            // uint32_t setLayoutCount;
            nullptr,                                       // const VkDescriptorSetLayout* pSetLayouts;
            0u,                                            // uint32_t pushConstantRangeCount;
            nullptr                                        // const VkPushConstantRange* pPushConstantRanges;
        };

        m_pipelineLayout = PipelineLayoutWrapper(pipelineConstructionType, vk, vkDevice, &pipelineLayoutParams);
    }

    m_vertexShaderModule   = ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_vert"), 0);
    m_fragmentShaderModule = ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_frag"), 0);

    // Create pipeline
    {
        const VkVertexInputBindingDescription vertexInputBindingDescription = {
            0u,                         // uint32_t binding;
            sizeof(Vertex4RGBA),        // uint32_t strideInBytes;
            VK_VERTEX_INPUT_RATE_VERTEX // VkVertexInputStepRate inputRate;
        };

        const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2] = {
            {
                0u,                            // uint32_t location;
                0u,                            // uint32_t binding;
                VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
                0u                             // uint32_t offset;
            },
            {
                1u,                            // uint32_t location;
                0u,                            // uint32_t binding;
                VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
                (uint32_t)(sizeof(float) * 4), // uint32_t offset;
            }};

        const VkPipelineVertexInputStateCreateInfo vertexInputStateParams = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                   // const void* pNext;
            0u,                                                        // VkPipelineVertexInputStateCreateFlags flags;
            1u,                                                        // uint32_t vertexBindingDescriptionCount;
            &vertexInputBindingDescription,  // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
            2u,                              // uint32_t vertexAttributeDescriptionCount;
            vertexInputAttributeDescriptions // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
        };

        const std::vector<VkViewport> viewports{makeViewport(m_renderSize)};
        const std::vector<VkRect2D> scissors{makeRect2D(m_renderSize)};

        // The color blend attachment will be set up before creating the graphics pipeline.
        VkPipelineColorBlendStateCreateInfo colorBlendStateParams = {
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                  // const void* pNext;
            0u,                                                       // VkPipelineColorBlendStateCreateFlags flags;
            false,                                                    // VkBool32 logicOpEnable;
            VK_LOGIC_OP_COPY,                                         // VkLogicOp logicOp;
            0u,                                                       // uint32_t attachmentCount;
            nullptr, // const VkPipelineColorBlendAttachmentState* pAttachments;
            {        // float blendConstants[4];
             BlendTest::s_blendConst.x(), BlendTest::s_blendConst.y(), BlendTest::s_blendConst.z(),
             BlendTest::s_blendConst.w()}};

        for (int quadNdx = 0; quadNdx < BlendTest::QUAD_COUNT; quadNdx++)
        {
            colorBlendStateParams.attachmentCount = 1u;
            colorBlendStateParams.pAttachments    = &m_blendStates[quadNdx];

            m_graphicsPipelines[quadNdx]
                .setDefaultMultisampleState()
                .setDefaultDepthStencilState()
                .setDefaultRasterizationState()
                .setupVertexInputState(&vertexInputStateParams)
                .setupPreRasterizationShaderState(viewports, scissors, m_pipelineLayout, *m_renderPass, 0u,
                                                  m_vertexShaderModule)
                .setupFragmentShaderState(m_pipelineLayout, *m_renderPass, 0u, m_fragmentShaderModule)
                .setupFragmentOutputState(*m_renderPass, 0u, &colorBlendStateParams)
                .setMonolithicPipelineLayout(m_pipelineLayout)
                .buildPipeline();
        }
    }

    // Create vertex buffer
    {
        const VkBufferCreateInfo vertexBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            1024u,                                // VkDeviceSize size;
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,    // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            1u,                                   // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex                     // const uint32_t* pQueueFamilyIndices;
        };

        m_vertices          = createOverlappingQuads();
        m_vertexBuffer      = createBuffer(vk, vkDevice, &vertexBufferParams);
        m_vertexBufferAlloc = memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_vertexBuffer),
                                                MemoryRequirement::HostVisible);

        VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(),
                                     m_vertexBufferAlloc->getOffset()));

        // Adjust vertex colors
        if (!isFloatFormat(m_colorFormat))
        {
            const tcu::TextureFormatInfo formatInfo = tcu::getTextureFormatInfo(mapVkFormat(m_colorFormat));
            for (size_t vertexNdx = 0; vertexNdx < m_vertices.size(); vertexNdx++)
                m_vertices[vertexNdx].color =
                    (m_vertices[vertexNdx].color - formatInfo.lookupBias) / formatInfo.lookupScale;
        }

        // Upload vertex data
        deMemcpy(m_vertexBufferAlloc->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4RGBA));

        flushAlloc(vk, vkDevice, *m_vertexBufferAlloc);
    }

    // Create command pool
    m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

    // Create command buffer
    {
        const VkClearValue attachmentClearValue = defaultClearValue(m_colorFormat);

        // Color image layout transition
        const VkImageMemoryBarrier imageLayoutBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,     // VkStructureType            sType;
            nullptr,                                    // const void*                pNext;
            (VkAccessFlags)0,                           // VkAccessFlags              srcAccessMask;
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,       // VkAccessFlags              dstAccessMask;
            VK_IMAGE_LAYOUT_UNDEFINED,                  // VkImageLayout              oldLayout;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,   // VkImageLayout              newLayout;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t                   srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t                   dstQueueFamilyIndex;
            *m_colorImage,                              // VkImage                    image;
            {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange    subresourceRange;
        };

        m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        beginCommandBuffer(vk, *m_cmdBuffer, 0u);

        vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (VkDependencyFlags)0, 0u, nullptr, 0u,
                              nullptr, 1u, &imageLayoutBarrier);

        m_renderPass.begin(vk, *m_cmdBuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()),
                           attachmentClearValue);

        const VkDeviceSize quadOffset = (m_vertices.size() / BlendTest::QUAD_COUNT) * sizeof(Vertex4RGBA);

        for (int quadNdx = 0; quadNdx < BlendTest::QUAD_COUNT; quadNdx++)
        {
            VkDeviceSize vertexBufferOffset = quadOffset * quadNdx;

            m_graphicsPipelines[quadNdx].bind(*m_cmdBuffer);
            vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
            vk.cmdDraw(*m_cmdBuffer, (uint32_t)(m_vertices.size() / BlendTest::QUAD_COUNT), 1, 0, 0);
        }

        m_renderPass.end(vk, *m_cmdBuffer);
        endCommandBuffer(vk, *m_cmdBuffer);
    }
}

BlendTestInstance::~BlendTestInstance(void)
{
}

tcu::TestStatus BlendTestInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();
    const VkQueue queue       = m_context.getUniversalQueue();

    submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

    return verifyImage();
}

float getNormChannelThreshold(const tcu::TextureFormat &format, int numBits)
{
    switch (tcu::getTextureChannelClass(format.type))
    {
    case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
        return static_cast<float>(BlendTest::QUAD_COUNT) / static_cast<float>((1 << numBits) - 1);
    case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
        return static_cast<float>(BlendTest::QUAD_COUNT) / static_cast<float>((1 << (numBits - 1)) - 1);
    default:
        break;
    }

    DE_ASSERT(false);
    return 0.0f;
}

tcu::Vec4 getFormatThreshold(const tcu::TextureFormat &format)
{
    using tcu::TextureFormat;
    using tcu::Vec4;

    Vec4 threshold(0.01f);

    switch (format.type)
    {
    case TextureFormat::UNORM_BYTE_44:
        threshold = Vec4(getNormChannelThreshold(format, 4), getNormChannelThreshold(format, 4), 1.0f, 1.0f);
        break;

    case TextureFormat::UNORM_SHORT_565:
        threshold = Vec4(getNormChannelThreshold(format, 5), getNormChannelThreshold(format, 6),
                         getNormChannelThreshold(format, 5), 1.0f);
        break;

    case TextureFormat::UNORM_SHORT_555:
        threshold = Vec4(getNormChannelThreshold(format, 5), getNormChannelThreshold(format, 5),
                         getNormChannelThreshold(format, 5), 1.0f);
        break;

    case TextureFormat::UNORM_SHORT_4444:
        threshold = Vec4(getNormChannelThreshold(format, 4));
        break;

    case TextureFormat::UNORM_SHORT_5551:
        threshold = Vec4(getNormChannelThreshold(format, 5), getNormChannelThreshold(format, 5),
                         getNormChannelThreshold(format, 5), 0.1f);
        break;

    case TextureFormat::UNORM_SHORT_10:
        threshold = Vec4(getNormChannelThreshold(format, 10));
        break;

    case TextureFormat::UNORM_INT_1010102_REV:
    case TextureFormat::SNORM_INT_1010102_REV:
        threshold = Vec4(getNormChannelThreshold(format, 10), getNormChannelThreshold(format, 10),
                         getNormChannelThreshold(format, 10), 0.34f);
        break;

    case TextureFormat::UNORM_INT8:
    case TextureFormat::SNORM_INT8:
        threshold = Vec4(getNormChannelThreshold(format, 8));
        break;

    case TextureFormat::UNORM_INT16:
    case TextureFormat::SNORM_INT16:
        threshold = Vec4(getNormChannelThreshold(format, 16));
        break;

    case TextureFormat::UNORM_INT32:
    case TextureFormat::SNORM_INT32:
        threshold = Vec4(getNormChannelThreshold(format, 32));
        break;

    case TextureFormat::HALF_FLOAT:
        threshold = Vec4(0.005f);
        break;

    case TextureFormat::FLOAT:
        threshold = Vec4(0.00001f);
        break;

    case TextureFormat::UNSIGNED_INT_11F_11F_10F_REV:
        threshold = Vec4(0.02f, 0.02f, 0.0625f, 1.0f);
        break;

    case TextureFormat::UNSIGNED_INT_999_E5_REV:
        threshold = Vec4(0.05f, 0.05f, 0.05f, 1.0f);
        break;

    case TextureFormat::UNORM_SHORT_1555:
        threshold = Vec4(0.1f, getNormChannelThreshold(format, 5), getNormChannelThreshold(format, 5),
                         getNormChannelThreshold(format, 5));
        break;

    default:
        DE_ASSERT(false);
    }

    // Return value matching the channel order specified by the format
    if (format.order == tcu::TextureFormat::BGR || format.order == tcu::TextureFormat::BGRA)
        return threshold.swizzle(2, 1, 0, 3);
    else
        return threshold;
}

bool isLegalExpandableFormat(tcu::TextureFormat::ChannelType channeltype)
{
    using tcu::TextureFormat;

    switch (channeltype)
    {
    case TextureFormat::UNORM_INT24:
    case TextureFormat::UNORM_BYTE_44:
    case TextureFormat::UNORM_SHORT_565:
    case TextureFormat::UNORM_SHORT_555:
    case TextureFormat::UNORM_SHORT_4444:
    case TextureFormat::UNORM_SHORT_5551:
    case TextureFormat::UNORM_SHORT_1555:
    case TextureFormat::UNORM_SHORT_10:
    case TextureFormat::UNORM_INT_101010:
    case TextureFormat::SNORM_INT_1010102_REV:
    case TextureFormat::UNORM_INT_1010102_REV:
    case TextureFormat::UNSIGNED_BYTE_44:
    case TextureFormat::UNSIGNED_SHORT_565:
    case TextureFormat::UNSIGNED_SHORT_4444:
    case TextureFormat::UNSIGNED_SHORT_5551:
    case TextureFormat::SIGNED_INT_1010102_REV:
    case TextureFormat::UNSIGNED_INT_1010102_REV:
    case TextureFormat::UNSIGNED_INT_11F_11F_10F_REV:
    case TextureFormat::UNSIGNED_INT_999_E5_REV:
    case TextureFormat::UNSIGNED_INT_24_8:
    case TextureFormat::UNSIGNED_INT_24_8_REV:
    case TextureFormat::UNSIGNED_INT24:
    case TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
        return true;

    case TextureFormat::SNORM_INT8:
    case TextureFormat::SNORM_INT16:
    case TextureFormat::SNORM_INT32:
    case TextureFormat::UNORM_INT8:
    case TextureFormat::UNORM_INT16:
    case TextureFormat::UNORM_INT32:
    case TextureFormat::UNSIGNED_INT_16_8_8:
    case TextureFormat::SIGNED_INT8:
    case TextureFormat::SIGNED_INT16:
    case TextureFormat::SIGNED_INT32:
    case TextureFormat::UNSIGNED_INT8:
    case TextureFormat::UNSIGNED_INT16:
    case TextureFormat::UNSIGNED_INT32:
    case TextureFormat::HALF_FLOAT:
    case TextureFormat::FLOAT:
    case TextureFormat::FLOAT64:
        return false;

    default:
        DE_FATAL("Unknown texture format");
    }
    return false;
}

bool isSmallerThan8BitFormat(tcu::TextureFormat::ChannelType channeltype)
{
    using tcu::TextureFormat;

    // Note: only checks the legal expandable formats
    // (i.e, formats that have channels that fall outside
    // the 8, 16 and 32 bit width)
    switch (channeltype)
    {
    case TextureFormat::UNORM_BYTE_44:
    case TextureFormat::UNORM_SHORT_565:
    case TextureFormat::UNORM_SHORT_555:
    case TextureFormat::UNORM_SHORT_4444:
    case TextureFormat::UNORM_SHORT_5551:
    case TextureFormat::UNORM_SHORT_1555:
    case TextureFormat::UNSIGNED_BYTE_44:
    case TextureFormat::UNSIGNED_SHORT_565:
    case TextureFormat::UNSIGNED_SHORT_4444:
    case TextureFormat::UNSIGNED_SHORT_5551:
        return true;

    case TextureFormat::UNORM_INT24:
    case TextureFormat::UNORM_INT_101010:
    case TextureFormat::SNORM_INT_1010102_REV:
    case TextureFormat::UNORM_INT_1010102_REV:
    case TextureFormat::SIGNED_INT_1010102_REV:
    case TextureFormat::UNSIGNED_INT_1010102_REV:
    case TextureFormat::UNSIGNED_INT_11F_11F_10F_REV:
    case TextureFormat::UNSIGNED_INT_999_E5_REV:
    case TextureFormat::UNSIGNED_INT_24_8:
    case TextureFormat::UNSIGNED_INT_24_8_REV:
    case TextureFormat::UNSIGNED_INT24:
    case TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
    case TextureFormat::UNORM_SHORT_10:
        return false;

    default:
        DE_FATAL("Unknown texture format");
    }

    return false;
}

tcu::TestStatus BlendTestInstance::verifyImage(void)
{
    const tcu::TextureFormat tcuColorFormat   = mapVkFormat(m_colorFormat);
    const tcu::TextureFormat tcuColorFormat64 = tcu::TextureFormat(tcuColorFormat.order, tcu::TextureFormat::FLOAT64);
    const tcu::TextureFormat tcuColorFormat8 = tcu::TextureFormat(tcuColorFormat.order, tcu::TextureFormat::UNORM_INT8);
    const tcu::TextureFormat tcuDepthFormat  = tcu::TextureFormat(); // Undefined depth/stencil format
    const ColorVertexShader vertexShader;
    const ColorFragmentShader fragmentShader(tcuColorFormat, tcuDepthFormat);
    const rr::Program program(&vertexShader, &fragmentShader);
    ReferenceRenderer refRenderer(m_renderSize.x(), m_renderSize.y(), 1, tcuColorFormat, tcuDepthFormat, &program);
    ReferenceRenderer refRenderer64(m_renderSize.x(), m_renderSize.y(), 1, tcuColorFormat64, tcuDepthFormat, &program);
    ReferenceRenderer refRenderer8(m_renderSize.x(), m_renderSize.y(), 1, tcuColorFormat8, tcuDepthFormat, &program);
    bool compareOk = false;

    // Render reference image
    {
        const bool altMasks = mustIncludeRGBDynamicMask(m_colorFormat);
        const VkColorComponentFlags *colorWriteMasks =
            (altMasks ? BlendTest::s_altColorWriteMasks : BlendTest::s_colorWriteMasks);

        for (int quadNdx = 0; quadNdx < BlendTest::QUAD_COUNT; quadNdx++)
        {
            const VkPipelineColorBlendAttachmentState &blendState = m_blendStates[quadNdx];

            // Set blend state
            rr::RenderState renderState(refRenderer.getViewportState(),
                                        m_context.getDeviceProperties().limits.subPixelPrecisionBits);
            renderState.fragOps.blendMode              = rr::BLENDMODE_STANDARD;
            renderState.fragOps.blendRGBState.srcFunc  = mapVkBlendFactor(blendState.srcColorBlendFactor);
            renderState.fragOps.blendRGBState.dstFunc  = mapVkBlendFactor(blendState.dstColorBlendFactor);
            renderState.fragOps.blendRGBState.equation = mapVkBlendOp(blendState.colorBlendOp);
            renderState.fragOps.blendAState.srcFunc    = mapVkBlendFactor(blendState.srcAlphaBlendFactor);
            renderState.fragOps.blendAState.dstFunc    = mapVkBlendFactor(blendState.dstAlphaBlendFactor);
            renderState.fragOps.blendAState.equation   = mapVkBlendOp(blendState.alphaBlendOp);
            renderState.fragOps.blendColor             = BlendTest::s_blendConst;
            renderState.fragOps.colorMask              = mapVkColorComponentFlags(colorWriteMasks[quadNdx]);

            refRenderer.draw(
                renderState, rr::PRIMITIVETYPE_TRIANGLES,
                std::vector<Vertex4RGBA>(m_vertices.begin() + quadNdx * 6, m_vertices.begin() + (quadNdx + 1) * 6));

            if (isLegalExpandableFormat(tcuColorFormat.type))
            {
                refRenderer64.draw(
                    renderState, rr::PRIMITIVETYPE_TRIANGLES,
                    std::vector<Vertex4RGBA>(m_vertices.begin() + quadNdx * 6, m_vertices.begin() + (quadNdx + 1) * 6));

                if (isSmallerThan8BitFormat(tcuColorFormat.type))
                    refRenderer8.draw(renderState, rr::PRIMITIVETYPE_TRIANGLES,
                                      std::vector<Vertex4RGBA>(m_vertices.begin() + quadNdx * 6,
                                                               m_vertices.begin() + (quadNdx + 1) * 6));
            }
        }
    }

    // Compare result with reference image
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice vkDevice         = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        SimpleAllocator allocator(
            vk, vkDevice,
            getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
        de::UniquePtr<tcu::TextureLevel> result(readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator,
                                                                    *m_colorImage, m_colorFormat, m_renderSize)
                                                    .release());
        const tcu::Vec4 threshold(getFormatThreshold(tcuColorFormat));
        tcu::TextureLevel refLevel;

        refLevel.setStorage(tcuColorFormat, m_renderSize.x(), m_renderSize.y(), 1);

        compareOk = tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "FloatImageCompare",
                                               "Image comparison", refRenderer.getAccess(), result->getAccess(),
                                               threshold, tcu::COMPARE_LOG_RESULT);

        if (isLegalExpandableFormat(tcuColorFormat.type))
        {
            if (!compareOk && isSmallerThan8BitFormat(tcuColorFormat.type))
            {
                // Convert to target format
                tcu::copy(refLevel.getAccess(), refRenderer8.getAccess());

                compareOk =
                    tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "FloatImageCompare",
                                               "Image comparison, 8 bit intermediate format", refLevel.getAccess(),
                                               result->getAccess(), threshold, tcu::COMPARE_LOG_RESULT);
            }

            if (!compareOk)
            {
                // Convert to target format
                tcu::copy(refLevel.getAccess(), refRenderer64.getAccess());

                compareOk =
                    tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "FloatImageCompare",
                                               "Image comparison, 64 bit intermediate format", refLevel.getAccess(),
                                               result->getAccess(), threshold, tcu::COMPARE_LOG_RESULT);
            }
        }
    }

    if (compareOk)
        return tcu::TestStatus::pass("Result image matches reference");
    else
        return tcu::TestStatus::fail("Image mismatch");
}

// DualSourceBlendTestInstance

DualSourceBlendTestInstance::DualSourceBlendTestInstance(
    Context &context, const PipelineConstructionType pipelineConstructionType, const VkFormat colorFormat,
    const VkPipelineColorBlendAttachmentState blendStates[DualSourceBlendTest::QUAD_COUNT])
    : vkt::TestInstance(context)
    , m_renderSize(32, 32)
    , m_colorFormat(colorFormat)
    , m_graphicsPipelines{{context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                           context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType},
                          {context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                           context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType},
                          {context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                           context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType},
                          {context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                           context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType}}
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice vkDevice         = m_context.getDevice();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    SimpleAllocator memAlloc(
        vk, vkDevice,
        getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));

    // Copy depth operators
    deMemcpy(m_blendStates, blendStates, sizeof(VkPipelineColorBlendAttachmentState) * DualSourceBlendTest::QUAD_COUNT);

    // Create color image
    {
        const VkImageCreateInfo colorImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                   // VkStructureType sType;
            nullptr,                                                               // const void* pNext;
            0u,                                                                    // VkImageCreateFlags flags;
            VK_IMAGE_TYPE_2D,                                                      // VkImageType imageType;
            m_colorFormat,                                                         // VkFormat format;
            {m_renderSize.x(), m_renderSize.y(), 1u},                              // VkExtent3D extent;
            1u,                                                                    // uint32_t mipLevels;
            1u,                                                                    // uint32_t arrayLayers;
            VK_SAMPLE_COUNT_1_BIT,                                                 // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,                                               // VkImageTiling tiling;
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                             // VkSharingMode sharingMode;
            1u,                                                                    // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex,        // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED // VkImageLayout initialLayout;
        };

        m_colorImageCreateInfo = colorImageParams;
        m_colorImage           = createImage(vk, vkDevice, &m_colorImageCreateInfo);

        // Allocate and bind color image memory
        m_colorImageAlloc =
            memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImage), MemoryRequirement::Any);
        VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImage, m_colorImageAlloc->getMemory(),
                                    m_colorImageAlloc->getOffset()));
    }

    // Create color attachment view
    {
        const VkImageViewCreateInfo colorAttachmentViewParams = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
            nullptr,                                  // const void* pNext;
            0u,                                       // VkImageViewCreateFlags flags;
            *m_colorImage,                            // VkImage image;
            VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
            m_colorFormat,                            // VkFormat format;
            {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
             VK_COMPONENT_SWIZZLE_IDENTITY},
            {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
        };

        m_colorAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
    }

    // Create render pass
    m_renderPass = RenderPassWrapper(pipelineConstructionType, vk, vkDevice, m_colorFormat);

    // Create framebuffer
    {
        const VkFramebufferCreateInfo framebufferParams = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                                   // const void* pNext;
            0u,                                        // VkFramebufferCreateFlags flags;
            *m_renderPass,                             // VkRenderPass renderPass;
            1u,                                        // uint32_t attachmentCount;
            &m_colorAttachmentView.get(),              // const VkImageView* pAttachments;
            (uint32_t)m_renderSize.x(),                // uint32_t width;
            (uint32_t)m_renderSize.y(),                // uint32_t height;
            1u                                         // uint32_t layers;
        };

        m_renderPass.createFramebuffer(vk, vkDevice, &framebufferParams, *m_colorImage);
    }

    // Create pipeline layout
    {
        const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
            nullptr,                                       // const void* pNext;
            0u,                                            // VkPipelineLayoutCreateFlags flags;
            0u,                                            // uint32_t setLayoutCount;
            nullptr,                                       // const VkDescriptorSetLayout* pSetLayouts;
            0u,                                            // uint32_t pushConstantRangeCount;
            nullptr                                        // const VkPushConstantRange* pPushConstantRanges;
        };

        m_pipelineLayout = PipelineLayoutWrapper(pipelineConstructionType, vk, vkDevice, &pipelineLayoutParams);
    }

    m_vertexShaderModule   = ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_vert"), 0);
    m_fragmentShaderModule = ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_frag"), 0);

    // Create pipeline
    {
        const VkVertexInputBindingDescription vertexInputBindingDescription = {
            0u,                         // uint32_t binding;
            sizeof(Vertex4RGBARGBA),    // uint32_t strideInBytes;
            VK_VERTEX_INPUT_RATE_VERTEX // VkVertexInputStepRate inputRate;
        };

        const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[3] = {
            {
                0u,                            // uint32_t location;
                0u,                            // uint32_t binding;
                VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
                0u                             // uint32_t offset;
            },
            {
                1u,                            // uint32_t location;
                0u,                            // uint32_t binding;
                VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
                (uint32_t)(sizeof(float) * 4), // uint32_t offset;
            },
            {
                2u,                            // uint32_t location;
                0u,                            // uint32_t binding;
                VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
                (uint32_t)(sizeof(float) * 8), // uint32_t offset;
            }};

        const VkPipelineVertexInputStateCreateInfo vertexInputStateParams = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                   // const void* pNext;
            0u,                                                        // VkPipelineVertexInputStateCreateFlags flags;
            1u,                                                        // uint32_t vertexBindingDescriptionCount;
            &vertexInputBindingDescription,  // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
            3u,                              // uint32_t vertexAttributeDescriptionCount;
            vertexInputAttributeDescriptions // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
        };

        const std::vector<VkViewport> viewports{makeViewport(m_renderSize)};
        const std::vector<VkRect2D> scissors{makeRect2D(m_renderSize)};

        // The color blend attachment will be set up before creating the graphics pipeline.
        VkPipelineColorBlendStateCreateInfo colorBlendStateParams = {
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                  // const void* pNext;
            0u,                                                       // VkPipelineColorBlendStateCreateFlags flags;
            false,                                                    // VkBool32 logicOpEnable;
            VK_LOGIC_OP_COPY,                                         // VkLogicOp logicOp;
            0u,                                                       // uint32_t attachmentCount;
            nullptr, // const VkPipelineColorBlendAttachmentState* pAttachments;
            {        // float blendConstants[4];
             DualSourceBlendTest::s_blendConst.x(), DualSourceBlendTest::s_blendConst.y(),
             DualSourceBlendTest::s_blendConst.z(), DualSourceBlendTest::s_blendConst.w()}};

        for (int quadNdx = 0; quadNdx < DualSourceBlendTest::QUAD_COUNT; quadNdx++)
        {
            colorBlendStateParams.attachmentCount = 1u;
            colorBlendStateParams.pAttachments    = &m_blendStates[quadNdx];

            m_graphicsPipelines[quadNdx]
                .setDefaultRasterizationState()
                .setDefaultDepthStencilState()
                .setDefaultMultisampleState()
                .setupVertexInputState(&vertexInputStateParams)
                .setupPreRasterizationShaderState(viewports, scissors, m_pipelineLayout, *m_renderPass, 0u,
                                                  m_vertexShaderModule)
                .setupFragmentShaderState(m_pipelineLayout, *m_renderPass, 0u, m_fragmentShaderModule)
                .setupFragmentOutputState(*m_renderPass, 0u, &colorBlendStateParams)
                .setMonolithicPipelineLayout(m_pipelineLayout)
                .buildPipeline();
        }
    }

    // Create vertex buffer
    {
        const VkBufferCreateInfo vertexBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            1152u,                                // VkDeviceSize size;
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,    // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            1u,                                   // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex                     // const uint32_t* pQueueFamilyIndices;
        };

        m_vertices          = createOverlappingQuadsDualSource();
        m_vertexBuffer      = createBuffer(vk, vkDevice, &vertexBufferParams);
        m_vertexBufferAlloc = memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_vertexBuffer),
                                                MemoryRequirement::HostVisible);

        VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(),
                                     m_vertexBufferAlloc->getOffset()));

        // Adjust vertex colors
        if (!isFloatFormat(m_colorFormat))
        {
            const tcu::TextureFormatInfo formatInfo = tcu::getTextureFormatInfo(mapVkFormat(m_colorFormat));
            for (size_t vertexNdx = 0; vertexNdx < m_vertices.size(); vertexNdx++)
            {
                m_vertices[vertexNdx].color0 =
                    (m_vertices[vertexNdx].color0 - formatInfo.lookupBias) / formatInfo.lookupScale;
                m_vertices[vertexNdx].color1 =
                    (m_vertices[vertexNdx].color1 - formatInfo.lookupBias) / formatInfo.lookupScale;
            }
        }

        // Upload vertex data
        deMemcpy(m_vertexBufferAlloc->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4RGBARGBA));

        flushAlloc(vk, vkDevice, *m_vertexBufferAlloc);
    }

    // Create command pool
    m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

    // Create command buffer
    {
        const VkClearValue attachmentClearValue = defaultClearValue(m_colorFormat);

        // Color image layout transition
        const VkImageMemoryBarrier imageLayoutBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,     // VkStructureType            sType;
            nullptr,                                    // const void*                pNext;
            (VkAccessFlags)0,                           // VkAccessFlags              srcAccessMask;
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,       // VkAccessFlags              dstAccessMask;
            VK_IMAGE_LAYOUT_UNDEFINED,                  // VkImageLayout              oldLayout;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,   // VkImageLayout              newLayout;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t                   srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t                   dstQueueFamilyIndex;
            *m_colorImage,                              // VkImage                    image;
            {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange    subresourceRange;
        };

        m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        beginCommandBuffer(vk, *m_cmdBuffer, 0u);

        vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (VkDependencyFlags)0, 0u, nullptr, 0u,
                              nullptr, 1u, &imageLayoutBarrier);

        m_renderPass.begin(vk, *m_cmdBuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()),
                           attachmentClearValue);

        const VkDeviceSize quadOffset = (m_vertices.size() / DualSourceBlendTest::QUAD_COUNT) * sizeof(Vertex4RGBARGBA);

        for (int quadNdx = 0; quadNdx < DualSourceBlendTest::QUAD_COUNT; quadNdx++)
        {
            VkDeviceSize vertexBufferOffset = quadOffset * quadNdx;

            m_graphicsPipelines[quadNdx].bind(*m_cmdBuffer);
            vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
            vk.cmdDraw(*m_cmdBuffer, (uint32_t)(m_vertices.size() / DualSourceBlendTest::QUAD_COUNT), 1, 0, 0);
        }

        m_renderPass.end(vk, *m_cmdBuffer);
        endCommandBuffer(vk, *m_cmdBuffer);
    }
}

DualSourceBlendTestInstance::~DualSourceBlendTestInstance(void)
{
}

tcu::TestStatus DualSourceBlendTestInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();
    const VkQueue queue       = m_context.getUniversalQueue();

    submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

    return verifyImage();
}

tcu::TestStatus DualSourceBlendTestInstance::verifyImage(void)
{
    const tcu::TextureFormat tcuColorFormat   = mapVkFormat(m_colorFormat);
    const tcu::TextureFormat tcuColorFormat64 = tcu::TextureFormat(tcuColorFormat.order, tcu::TextureFormat::FLOAT64);
    const tcu::TextureFormat tcuColorFormat8 = tcu::TextureFormat(tcuColorFormat.order, tcu::TextureFormat::UNORM_INT8);
    const tcu::TextureFormat tcuDepthFormat  = tcu::TextureFormat(); // Undefined depth/stencil format
    const ColorVertexShaderDualSource vertexShader;
    const ColorFragmentShaderDualSource fragmentShader(tcuColorFormat, tcuDepthFormat);
    const rr::Program program(&vertexShader, &fragmentShader);
    ReferenceRenderer refRenderer(m_renderSize.x(), m_renderSize.y(), 1, tcuColorFormat, tcuDepthFormat, &program);
    ReferenceRenderer refRenderer64(m_renderSize.x(), m_renderSize.y(), 1, tcuColorFormat64, tcuDepthFormat, &program);
    ReferenceRenderer refRenderer8(m_renderSize.x(), m_renderSize.y(), 1, tcuColorFormat8, tcuDepthFormat, &program);
    bool compareOk                  = false;
    tcu::PixelBufferAccess access   = refRenderer.getAccess();
    tcu::PixelBufferAccess access8  = refRenderer8.getAccess();
    tcu::PixelBufferAccess access64 = refRenderer64.getAccess();

    // Render reference image
    {
        // read clear color
        tcu::Vec4 discardColor   = access.getPixel(0, 0);
        tcu::Vec4 discardColor8  = access8.getPixel(0, 0);
        tcu::Vec4 discardColor64 = access64.getPixel(0, 0);

        const bool altMasks = mustIncludeRGBDynamicMask(m_colorFormat);
        const VkColorComponentFlags *colorWriteMasks =
            (altMasks ? DualSourceBlendTest::s_altColorWriteMasks : DualSourceBlendTest::s_colorWriteMasks);

        for (int quadNdx = 0; quadNdx < BlendTest::QUAD_COUNT; quadNdx++)
        {
            const VkPipelineColorBlendAttachmentState &blendState = m_blendStates[quadNdx];

            // Set blend state
            rr::RenderState renderState(refRenderer.getViewportState(),
                                        m_context.getDeviceProperties().limits.subPixelPrecisionBits);
            renderState.fragOps.blendMode              = rr::BLENDMODE_STANDARD;
            renderState.fragOps.blendRGBState.srcFunc  = mapVkBlendFactor(blendState.srcColorBlendFactor);
            renderState.fragOps.blendRGBState.dstFunc  = mapVkBlendFactor(blendState.dstColorBlendFactor);
            renderState.fragOps.blendRGBState.equation = mapVkBlendOp(blendState.colorBlendOp);
            renderState.fragOps.blendAState.srcFunc    = mapVkBlendFactor(blendState.srcAlphaBlendFactor);
            renderState.fragOps.blendAState.dstFunc    = mapVkBlendFactor(blendState.dstAlphaBlendFactor);
            renderState.fragOps.blendAState.equation   = mapVkBlendOp(blendState.alphaBlendOp);
            renderState.fragOps.blendColor             = DualSourceBlendTest::s_blendConst;
            renderState.fragOps.colorMask              = mapVkColorComponentFlags(colorWriteMasks[quadNdx]);

            refRenderer.draw(
                renderState, rr::PRIMITIVETYPE_TRIANGLES,
                std::vector<Vertex4RGBARGBA>(m_vertices.begin() + quadNdx * 6, m_vertices.begin() + (quadNdx + 1) * 6));

            if (isLegalExpandableFormat(tcuColorFormat.type))
            {
                refRenderer64.draw(renderState, rr::PRIMITIVETYPE_TRIANGLES,
                                   std::vector<Vertex4RGBARGBA>(m_vertices.begin() + quadNdx * 6,
                                                                m_vertices.begin() + (quadNdx + 1) * 6));

                if (isSmallerThan8BitFormat(tcuColorFormat.type))
                    refRenderer8.draw(renderState, rr::PRIMITIVETYPE_TRIANGLES,
                                      std::vector<Vertex4RGBARGBA>(m_vertices.begin() + quadNdx * 6,
                                                                   m_vertices.begin() + (quadNdx + 1) * 6));
            }
        }

        // re-request the pixel access; copies various formats to accessable ones
        // (if we don't do this, the above draws don't matter)
        access   = refRenderer.getAccess();
        access8  = refRenderer8.getAccess();
        access64 = refRenderer64.getAccess();

        // Paint back the discarded pixels with the clear color. The reference
        // renderer doesn't actually run the shader, and doesn't know about discard,
        // so this is a way to get to the images we wanted.
        for (int i = 0; i < access.getWidth(); i++)
        {
            access.setPixel(discardColor, i, 3);
            if (isLegalExpandableFormat(tcuColorFormat.type))
            {
                access64.setPixel(discardColor64, i, 3);
                if (isSmallerThan8BitFormat(tcuColorFormat.type))
                    access8.setPixel(discardColor8, i, 3);
            }
        }

        for (int i = 0; i < access.getHeight(); i++)
        {
            access.setPixel(discardColor, 2, i);
            if (isLegalExpandableFormat(tcuColorFormat.type))
            {
                access64.setPixel(discardColor64, 2, i);
                if (isSmallerThan8BitFormat(tcuColorFormat.type))
                    access8.setPixel(discardColor8, 2, i);
            }
        }
    }

    // Compare result with reference image
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice vkDevice         = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        SimpleAllocator allocator(
            vk, vkDevice,
            getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
        de::UniquePtr<tcu::TextureLevel> result(readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator,
                                                                    *m_colorImage, m_colorFormat, m_renderSize)
                                                    .release());
        tcu::Vec4 threshold(getFormatThreshold(tcuColorFormat));
        tcu::TextureLevel refLevel;

        // For SRGB formats there is an extra precision loss due to doing
        // the following conversions sRGB -> RGB -> blend -> RGB  -> sRGB with floats.
        // Take that into account in the threshold. For example, VK_FORMAT_R8G8B8A8_SRGB
        // threshold is 4/255f, but we changed it to be 10/255f.
        if (tcu::isSRGB(tcuColorFormat))
            threshold = 2.5f * threshold;

        refLevel.setStorage(tcuColorFormat, m_renderSize.x(), m_renderSize.y(), 1);

        compareOk =
            tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "FloatImageCompare", "Image comparison",
                                       access, result->getAccess(), threshold, tcu::COMPARE_LOG_RESULT);

        if (isLegalExpandableFormat(tcuColorFormat.type))
        {
            if (!compareOk && isSmallerThan8BitFormat(tcuColorFormat.type))
            {
                // Convert to target format
                tcu::copy(refLevel.getAccess(), access8);

                compareOk =
                    tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "FloatImageCompare",
                                               "Image comparison, 8 bit intermediate format", refLevel.getAccess(),
                                               result->getAccess(), threshold, tcu::COMPARE_LOG_RESULT);
            }

            if (!compareOk)
            {
                // Convert to target format
                tcu::copy(refLevel.getAccess(), access64);

                compareOk =
                    tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "FloatImageCompare",
                                               "Image comparison, 64 bit intermediate format", refLevel.getAccess(),
                                               result->getAccess(), threshold, tcu::COMPARE_LOG_RESULT);
            }
        }
    }

    if (compareOk)
        return tcu::TestStatus::pass("Result image matches reference");
    else
        return tcu::TestStatus::fail("Image mismatch");
}

// Clamping tests for colors and constants.

struct ClampTestParams
{
    PipelineConstructionType pipelineConstructionType;
    vk::VkFormat colorFormat;
    tcu::Vec4 quadColor;
    tcu::Vec4 blendConstants;
};

class ClampTest : public vkt::TestCase
{
public:
    ClampTest(tcu::TestContext &testContext, const std::string &name, const ClampTestParams &testParams);
    virtual ~ClampTest(void)
    {
    }
    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual void checkSupport(Context &context) const;
    virtual TestInstance *createInstance(Context &context) const;

private:
    const ClampTestParams m_params;
};

class ClampTestInstance : public vkt::TestInstance
{
public:
    ClampTestInstance(Context &context, const ClampTestParams &testParams)
        : vkt::TestInstance(context)
        , m_params(testParams)
    {
    }
    virtual ~ClampTestInstance(void)
    {
    }
    virtual tcu::TestStatus iterate(void);

private:
    const ClampTestParams m_params;
};

ClampTest::ClampTest(tcu::TestContext &testContext, const std::string &name, const ClampTestParams &testParams)
    : vkt::TestCase(testContext, name)
    , m_params(testParams)
{
    // As per the spec:
    //
    //  If the color attachment is fixed-point, the components of the source and destination values and blend factors are each
    //  clamped to [0,1] or [-1,1] respectively for an unsigned normalized or signed normalized color attachment prior to evaluating
    //  the blend operations. If the color attachment is floating-point, no clamping occurs.
    //
    // We will only test signed and unsigned normalized formats, and avoid precision problems by having all channels have the same
    // bit depth.
    //
    DE_ASSERT(isSnormFormat(m_params.colorFormat) || isUnormFormat(m_params.colorFormat));

    const auto bitDepth = tcu::getTextureFormatBitDepth(mapVkFormat(m_params.colorFormat));
    DE_UNREF(bitDepth); // For release builds.
    DE_ASSERT(bitDepth[0] == bitDepth[1] && bitDepth[0] == bitDepth[2] && bitDepth[0] == bitDepth[3]);
}

void ClampTest::initPrograms(SourceCollections &sourceCollections) const
{
    std::ostringstream fragmentSource;

    sourceCollections.glslSources.add("color_vert")
        << glu::VertexSource("#version 310 es\n"
                             "layout(location = 0) in highp vec4 position;\n"
                             "layout(location = 1) in highp vec4 color;\n"
                             "layout(location = 0) out highp vec4 vtxColor;\n"
                             "void main (void)\n"
                             "{\n"
                             "    gl_Position = position;\n"
                             "    vtxColor = color;\n"
                             "}\n");

    fragmentSource << "#version 310 es\n"
                      "layout(location = 0) in highp vec4 vtxColor;\n"
                      "layout(location = 0) out highp vec4 fragColor;\n"
                      "void main (void)\n"
                      "{\n"
                      "    fragColor = vtxColor;\n"
                      "}\n";

    sourceCollections.glslSources.add("color_frag") << glu::FragmentSource(fragmentSource.str());
}

void ClampTest::checkSupport(Context &context) const
{
    if (!isSupportedBlendFormat(context.getInstanceInterface(), context.getPhysicalDevice(), m_params.colorFormat))
        throw tcu::NotSupportedError(std::string("Unsupported color blending format: ") +
                                     getFormatName(m_params.colorFormat));
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_params.pipelineConstructionType);
}

TestInstance *ClampTest::createInstance(Context &context) const
{
    return new ClampTestInstance(context, m_params);
}

tcu::TestStatus ClampTestInstance::iterate(void)
{
    const vk::InstanceInterface &vki          = m_context.getInstanceInterface();
    const vk::DeviceInterface &vkd            = m_context.getDeviceInterface();
    const vk::VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    const vk::VkDevice device                 = m_context.getDevice();
    vk::Allocator &allocator                  = m_context.getDefaultAllocator();
    const vk::VkQueue queue                   = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex           = m_context.getUniversalQueueFamilyIndex();
    const vk::VkExtent3D renderSize           = {32u, 32u, 1u};

    // Image.
    const vk::VkImageCreateInfo imageCreateInfo = {
        vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                       // VkStructureType sType;
        nullptr,                                                                       // const void* pNext;
        0u,                                                                            // VkImageCreateFlags flags;
        vk::VK_IMAGE_TYPE_2D,                                                          // VkImageType imageType;
        m_params.colorFormat,                                                          // VkFormat format;
        renderSize,                                                                    // VkExtent3D extent;
        1u,                                                                            // uint32_t mipLevels;
        1u,                                                                            // uint32_t arrayLayers;
        vk::VK_SAMPLE_COUNT_1_BIT,                                                     // VkSampleCountFlagBits samples;
        vk::VK_IMAGE_TILING_OPTIMAL,                                                   // VkImageTiling tiling;
        vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
        vk::VK_SHARING_MODE_EXCLUSIVE,                                                 // VkSharingMode sharingMode;
        1u,                            // uint32_t queueFamilyIndexCount;
        &queueFamilyIndex,             // const uint32_t* pQueueFamilyIndices;
        vk::VK_IMAGE_LAYOUT_UNDEFINED, // VkImageLayout initialLayout;
    };

    vk::ImageWithMemory colorImage(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any);

    // Image view.
    const vk::VkImageViewCreateInfo imageViewCreateInfo = {
        vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
        nullptr,                                      // const void* pNext;
        0u,                                           // VkImageViewCreateFlags flags;
        colorImage.get(),                             // VkImage image;
        vk::VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
        m_params.colorFormat,                         // VkFormat format;
        {
            // VkComponentMapping components;
            vk::VK_COMPONENT_SWIZZLE_IDENTITY,
            vk::VK_COMPONENT_SWIZZLE_IDENTITY,
            vk::VK_COMPONENT_SWIZZLE_IDENTITY,
            vk::VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        {vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u}, // VkImageSubresourceRange subresourceRange;
    };

    auto colorImageView = createImageView(vkd, device, &imageViewCreateInfo);

    // Render pass.
    RenderPassWrapper renderPass(m_params.pipelineConstructionType, vkd, device, m_params.colorFormat);

    // Frame buffer.
    const vk::VkFramebufferCreateInfo framebufferParams = {
        vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
        nullptr,                                       // const void* pNext;
        0u,                                            // VkFramebufferCreateFlags flags;
        renderPass.get(),                              // VkRenderPass renderPass;
        1u,                                            // uint32_t attachmentCount;
        &colorImageView.get(),                         // const VkImageView* pAttachments;
        renderSize.width,                              // uint32_t width;
        renderSize.height,                             // uint32_t height;
        1u,                                            // uint32_t layers;
    };

    renderPass.createFramebuffer(vkd, device, &framebufferParams, *colorImage);

    // Pipeline layout.
    const vk::VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
        nullptr,                                           // const void* pNext;
        0u,                                                // VkPipelineLayoutCreateFlags flags;
        0u,                                                // uint32_t setLayoutCount;
        nullptr,                                           // const VkDescriptorSetLayout* pSetLayouts;
        0u,                                                // uint32_t pushConstantRangeCount;
        nullptr,                                           // const VkPushConstantRange* pPushConstantRanges;
    };

    const PipelineLayoutWrapper pipelineLayout(m_params.pipelineConstructionType, vkd, device,
                                               &pipelineLayoutCreateInfo);

    // Shader modules.
    auto vertexShaderModule   = ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("color_vert"), 0);
    auto fragmentShaderModule = ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("color_frag"), 0);

    // Graphics pipeline.
    const vk::VkVertexInputBindingDescription vertexInputBindingDescription = {
        0u,                             // uint32_t binding;
        sizeof(Vertex4RGBA),            // uint32_t strideInBytes;
        vk::VK_VERTEX_INPUT_RATE_VERTEX // VkVertexInputStepRate inputRate;
    };

    const vk::VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2] = {
        {
            0u,                                // uint32_t location;
            0u,                                // uint32_t binding;
            vk::VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
            0u                                 // uint32_t offset;
        },
        {
            1u,                                                  // uint32_t location;
            0u,                                                  // uint32_t binding;
            vk::VK_FORMAT_R32G32B32A32_SFLOAT,                   // VkFormat format;
            static_cast<uint32_t>(offsetof(Vertex4RGBA, color)), // uint32_t offset;
        },
    };

    const vk::VkPipelineVertexInputStateCreateInfo vertexInputStateParams{
        vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                       // const void* pNext;
        0u,                                                            // VkPipelineVertexInputStateCreateFlags flags;
        1u,                                                            // uint32_t vertexBindingDescriptionCount;
        &vertexInputBindingDescription, // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
        static_cast<uint32_t>(
            DE_LENGTH_OF_ARRAY(vertexInputAttributeDescriptions)), // uint32_t vertexAttributeDescriptionCount;
        vertexInputAttributeDescriptions, // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
    };

    const std::vector<vk::VkViewport> viewports{makeViewport(renderSize)};
    const std::vector<vk::VkRect2D> scissors{makeRect2D(renderSize)};

    const vk::VkColorComponentFlags colorComponentFlags =
        (0u | vk::VK_COLOR_COMPONENT_R_BIT | vk::VK_COLOR_COMPONENT_G_BIT | vk::VK_COLOR_COMPONENT_B_BIT |
         vk::VK_COLOR_COMPONENT_A_BIT);

    // Color blend attachment state. Central aspect of the test.
    const vk::VkPipelineColorBlendAttachmentState colorBlendAttachmentState{
        VK_TRUE,                            // VkBool32 blendEnable;
        vk::VK_BLEND_FACTOR_CONSTANT_COLOR, // VkBlendFactor srcColorBlendFactor;
        vk::VK_BLEND_FACTOR_ZERO,           // VkBlendFactor dstColorBlendFactor;
        vk::VK_BLEND_OP_ADD,                // VkBlendOp colorBlendOp;
        vk::VK_BLEND_FACTOR_CONSTANT_ALPHA, // VkBlendFactor srcAlphaBlendFactor;
        vk::VK_BLEND_FACTOR_ZERO,           // VkBlendFactor dstAlphaBlendFactor;
        vk::VK_BLEND_OP_ADD,                // VkBlendOp alphaBlendOp;
        colorComponentFlags,                // VkColorComponentFlags colorWriteMask;
    };

    const vk::VkPipelineColorBlendStateCreateInfo colorBlendStateParams{
        vk::VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                      // const void* pNext;
        0u,                                                           // VkPipelineColorBlendStateCreateFlags flags;
        false,                                                        // VkBool32 logicOpEnable;
        vk::VK_LOGIC_OP_COPY,                                         // VkLogicOp logicOp;
        1u,                                                           // uint32_t attachmentCount;
        &colorBlendAttachmentState, // const VkPipelineColorBlendAttachmentState* pAttachments;
        {
            // float blendConstants[4];
            m_params.blendConstants[0],
            m_params.blendConstants[1],
            m_params.blendConstants[2],
            m_params.blendConstants[3],
        },
    };

    GraphicsPipelineWrapper graphicsPipeline(vki, vkd, physicalDevice, device, m_context.getDeviceExtensions(),
                                             m_params.pipelineConstructionType);
    graphicsPipeline.setDefaultRasterizationState()
        .setDefaultDepthStencilState()
        .setDefaultMultisampleState()
        .setupVertexInputState(&vertexInputStateParams)
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u, vertexShaderModule)
        .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragmentShaderModule)
        .setupFragmentOutputState(*renderPass, 0u, &colorBlendStateParams)
        .setMonolithicPipelineLayout(pipelineLayout)
        .buildPipeline();

    // Vertex buffer
    auto quadTexture = createFullscreenQuad();
    std::vector<Vertex4RGBA> vertices;

    // Keep position but replace texture coordinates with our own color.
    vertices.reserve(quadTexture.size());
    std::transform(begin(quadTexture), end(quadTexture), std::back_inserter(vertices),
                   [this](const decltype(quadTexture)::value_type &v) {
                       return Vertex4RGBA{v.position, this->m_params.quadColor};
                   });

    const vk::VkDeviceSize vtxBufferSize =
        static_cast<vk::VkDeviceSize>(vertices.size() * sizeof(decltype(vertices)::value_type));
    const vk::VkBufferCreateInfo bufferCreateInfo = {
        vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        0u,                                       // VkBufferCreateFlags flags;
        vtxBufferSize,                            // VkDeviceSize size;
        vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,    // VkBufferUsageFlags usage;
        vk::VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
        1u,                                       // uint32_t queueFamilyIndexCount;
        &queueFamilyIndex,                        // const uint32_t* pQueueFamilyIndices;
    };

    vk::BufferWithMemory vertexBuffer(vkd, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible);

    // Upload vertex data
    deMemcpy(vertexBuffer.getAllocation().getHostPtr(), vertices.data(), static_cast<size_t>(vtxBufferSize));
    flushAlloc(vkd, device, vertexBuffer.getAllocation());

    // Create command pool
    auto cmdPool = createCommandPool(vkd, device, vk::VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

    // Create and record command buffer
    auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    auto cmdBuffer    = cmdBufferPtr.get();

    vk::VkClearValue clearValue;
    clearValue.color.float32[0] = 0.0f;
    clearValue.color.float32[1] = 0.0f;
    clearValue.color.float32[2] = 0.0f;
    clearValue.color.float32[3] = 1.0f;

    const vk::VkDeviceSize vertexOffets[] = {0u};

    beginCommandBuffer(vkd, cmdBuffer, 0u);
    renderPass.begin(vkd, cmdBuffer, makeRect2D(renderSize), clearValue);
    graphicsPipeline.bind(cmdBuffer);
    vkd.cmdBindVertexBuffers(cmdBuffer, 0, 1u, &vertexBuffer.get(), vertexOffets);
    vkd.cmdDraw(cmdBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
    renderPass.end(vkd, cmdBuffer);
    endCommandBuffer(vkd, cmdBuffer);

    // Submit commands.
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    // Calculate reference final color.
    const tcu::TextureFormat tcuColorFormat = mapVkFormat(m_params.colorFormat);
    const auto formatInfo                   = tcu::getTextureFormatInfo(tcuColorFormat);

    tcu::Vec4 clampedBlendConstants = m_params.blendConstants;
    tcu::Vec4 clampedQuadColor      = m_params.quadColor;

    for (int i = 0; i < tcu::Vec4::SIZE; ++i)
    {
        clampedBlendConstants[i] = de::clamp(clampedBlendConstants[i], formatInfo.valueMin[i], formatInfo.valueMax[i]);
        clampedQuadColor[i]      = de::clamp(clampedQuadColor[i], formatInfo.valueMin[i], formatInfo.valueMax[i]);
    }

    tcu::Vec4 referenceColor;
    for (int i = 0; i < tcu::Vec4::SIZE; ++i)
        referenceColor[i] = clampedBlendConstants[i] * clampedQuadColor[i];

    // Compare result with reference color
    const tcu::UVec2 renderSizeUV2(renderSize.width, renderSize.height);
    de::UniquePtr<tcu::TextureLevel> result(readColorAttachment(vkd, device, queue, queueFamilyIndex, allocator,
                                                                colorImage.get(), m_params.colorFormat, renderSizeUV2)
                                                .release());
    const tcu::Vec4 threshold(getFormatThreshold(tcuColorFormat));
    const tcu::ConstPixelBufferAccess pixelBufferAccess = result->getAccess();

    const bool compareOk = tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "BlendClampCompare",
                                                      "Blend clamping pixel comparison", referenceColor,
                                                      pixelBufferAccess, threshold, tcu::COMPARE_LOG_ON_ERROR);

    if (compareOk)
        return tcu::TestStatus::pass("Pass");
    else
        return tcu::TestStatus::fail("Pixel mismatch");
}

} // namespace

std::string getBlendStateSetName(const VkPipelineColorBlendAttachmentState blendStates[BlendTest::QUAD_COUNT])
{
    std::ostringstream name;

    for (int quadNdx = 0; quadNdx < BlendTest::QUAD_COUNT; quadNdx++)
    {
        name << getBlendStateName(blendStates[quadNdx]);

        if (quadNdx < BlendTest::QUAD_COUNT - 1)
            name << "-";
    }

    return name.str();
}

tcu::TestCaseGroup *createBlendTests(tcu::TestContext &testCtx, PipelineConstructionType pipelineConstructionType)
{
    const bool isESO = vk::isConstructionTypeShaderObject(pipelineConstructionType);
    const auto genFormatTests =
        (!isESO || pipelineConstructionType == vk::PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV);

    const uint32_t blendStatesPerFormat = 100 * BlendTest::QUAD_COUNT;

    const std::pair<const char *, bool> shaderOutputTypes[]{
        {"output_variable", false},
        {"output_array", true},
    };

    // Blend tests
    de::MovePtr<tcu::TestCaseGroup> blendTests(new tcu::TestCaseGroup(testCtx, "blend"));
    // Uses different blend formats
    de::MovePtr<tcu::TestCaseGroup> formatTests(new tcu::TestCaseGroup(testCtx, "format"));
    de::MovePtr<tcu::TestCaseGroup> clampTests(new tcu::TestCaseGroup(testCtx, "clamp"));
    de::MovePtr<tcu::TestCaseGroup> dualSourceBlendTests(new tcu::TestCaseGroup(testCtx, "dual_source"));
    de::MovePtr<tcu::TestCaseGroup> dualSourceFormatTests(new tcu::TestCaseGroup(testCtx, "format"));

    de::MovePtr<tcu::TestCaseGroup> outputVariableTests(new tcu::TestCaseGroup(testCtx, "output_variable"));
    de::MovePtr<tcu::TestCaseGroup> outputArrayTests(new tcu::TestCaseGroup(testCtx, "output_array"));

    BlendStateUniqueRandomIterator blendStateItr(blendStatesPerFormat, 123);
    BlendStateUniqueRandomIteratorDualSource dualSourceBlendStateItr(blendStatesPerFormat, 123);

    if (genFormatTests)
    {
        for (const VkFormat format : getBlendFormats())
        {
            const bool needsAltMasksDynamic = mustIncludeRGBDynamicMask(format);

            // Blend tests
            {
                de::MovePtr<tcu::TestCaseGroup> formatTest(
                    new tcu::TestCaseGroup(testCtx, getFormatCaseName(format).c_str()));
                de::MovePtr<tcu::TestCaseGroup> blendStateTests;
                {
                    std::ostringstream blendStateDescription;
                    blendStateDescription << "Combines blend factors, operators and channel write masks. The constant "
                                             "color used in all tests is "
                                          << BlendTest::s_blendConst;
                    blendStateTests = de::MovePtr<tcu::TestCaseGroup>(new tcu::TestCaseGroup(testCtx, "states"));
                }

                blendStateItr.reset();

                while (blendStateItr.hasNext())
                {
                    VkPipelineColorBlendAttachmentState quadBlendConfigs[BlendTest::QUAD_COUNT];
                    const VkColorComponentFlags *colorWriteMasks =
                        (needsAltMasksDynamic ? BlendTest::s_altColorWriteMasks : BlendTest::s_colorWriteMasks);

                    for (int quadNdx = 0; quadNdx < BlendTest::QUAD_COUNT; quadNdx++)
                    {
                        quadBlendConfigs[quadNdx]                = blendStateItr.next();
                        quadBlendConfigs[quadNdx].colorWriteMask = colorWriteMasks[quadNdx];
                    }

                    blendStateTests->addChild(new BlendTest(testCtx, getBlendStateSetName(quadBlendConfigs),
                                                            pipelineConstructionType, format, quadBlendConfigs));
                }
                formatTest->addChild(blendStateTests.release());
                formatTests->addChild(formatTest.release());
            }

            // Dual-Source blending tests
            {
                de::MovePtr<tcu::TestCaseGroup> formatTest(
                    new tcu::TestCaseGroup(testCtx, getFormatCaseName(format).c_str()));

                for (const std::pair<const char *, bool> &shaderOutputType : shaderOutputTypes)
                {
                    de::MovePtr<tcu::TestCaseGroup> shaderOutputTypeTests(
                        new tcu::TestCaseGroup(testCtx, shaderOutputType.first));

                    de::MovePtr<tcu::TestCaseGroup> blendStateTests;
                    {
                        std::ostringstream blendStateDescription;
                        blendStateDescription << "Combines blend factors, operators and channel write masks. The "
                                                 "constant color used in all tests is "
                                              << BlendTest::s_blendConst;
                        blendStateTests = de::MovePtr<tcu::TestCaseGroup>(new tcu::TestCaseGroup(testCtx, "states"));
                    }

                    dualSourceBlendStateItr.reset();

                    while (dualSourceBlendStateItr.hasNext())
                    {
                        VkPipelineColorBlendAttachmentState quadBlendConfigs[BlendTest::QUAD_COUNT];
                        bool isDualSourceBlendTest = false;
                        const VkColorComponentFlags *colorWriteMasks =
                            (needsAltMasksDynamic ? BlendTest::s_altColorWriteMasks : BlendTest::s_colorWriteMasks);

                        for (int quadNdx = 0; quadNdx < BlendTest::QUAD_COUNT; quadNdx++)
                        {
                            quadBlendConfigs[quadNdx]                = dualSourceBlendStateItr.next();
                            quadBlendConfigs[quadNdx].colorWriteMask = colorWriteMasks[quadNdx];
                            isDualSourceBlendTest                    = isDualSourceBlendTest ||
                                                    isSrc1BlendFactor(quadBlendConfigs[quadNdx].srcColorBlendFactor) ||
                                                    isSrc1BlendFactor(quadBlendConfigs[quadNdx].dstColorBlendFactor) ||
                                                    isSrc1BlendFactor(quadBlendConfigs[quadNdx].srcAlphaBlendFactor) ||
                                                    isSrc1BlendFactor(quadBlendConfigs[quadNdx].dstAlphaBlendFactor);
                        }

                        // Skip tests that don't have dual-source blend factors as they are already tested.
                        if (!isDualSourceBlendTest)
                            continue;

                        blendStateTests->addChild(new DualSourceBlendTest(
                            testCtx, getBlendStateSetName(quadBlendConfigs), pipelineConstructionType, format,
                            quadBlendConfigs, shaderOutputType.second));
                    }

                    shaderOutputTypeTests->addChild(blendStateTests.release());
                    formatTest->addChild(shaderOutputTypeTests.release());
                }
                dualSourceFormatTests->addChild(formatTest.release());
            }
        }
    }

    // Subselection of formats that are easy to test for clamping.
    const vk::VkFormat clampFormats[] = {
        vk::VK_FORMAT_R8G8B8A8_UNORM, vk::VK_FORMAT_R8G8B8A8_SNORM,     vk::VK_FORMAT_B8G8R8A8_UNORM,
        vk::VK_FORMAT_B8G8R8A8_SNORM, vk::VK_FORMAT_R16G16B16A16_UNORM, vk::VK_FORMAT_R16G16B16A16_SNORM,
    };

    for (int formatIdx = 0; formatIdx < DE_LENGTH_OF_ARRAY(clampFormats); ++formatIdx)
    {
        const auto &format = clampFormats[formatIdx];
        ClampTestParams testParams;

        testParams.pipelineConstructionType = pipelineConstructionType;
        testParams.colorFormat              = format;

        if (isUnormFormat(format))
        {
            testParams.quadColor[0] = 2.0f;
            testParams.quadColor[1] = 0.5f;
            testParams.quadColor[2] = 1.0f;
            testParams.quadColor[3] = -1.0f;

            testParams.blendConstants[0] = 0.5f;
            testParams.blendConstants[1] = 2.0f;
            testParams.blendConstants[2] = -1.0f;
            testParams.blendConstants[3] = 1.0f;
        }
        else
        {
            testParams.quadColor[0] = 2.0f;
            testParams.quadColor[1] = 0.5f;
            testParams.quadColor[2] = 1.0f;
            testParams.quadColor[3] = -2.0f;

            testParams.blendConstants[0] = 0.5f;
            testParams.blendConstants[1] = 2.0f;
            testParams.blendConstants[2] = -2.0f;
            testParams.blendConstants[3] = 1.0f;
        }

        clampTests->addChild(new ClampTest(testCtx, getFormatCaseName(format), testParams));
    }

    if (genFormatTests)
        blendTests->addChild(formatTests.release());
    blendTests->addChild(clampTests.release());

    if (genFormatTests)
    {
        dualSourceBlendTests->addChild(dualSourceFormatTests.release());
#ifndef CTS_USES_VULKANSC
        addDualBlendMultiAttachmentTests(testCtx, dualSourceBlendTests.operator->(), pipelineConstructionType);
#endif
        blendTests->addChild(dualSourceBlendTests.release());
    }

    return blendTests.release();
}

} // namespace pipeline
} // namespace vkt
