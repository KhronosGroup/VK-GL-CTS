/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
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
 * \brief Dual Source Blending Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineDualBlendTests.hpp"
#include "vktPipelineBlendTestsCommon.hpp"
#include "vktShaderObjectCreateUtil.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "deRandom.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <numeric>
#include <random>
#include <sstream>
#include <type_traits>
#include <unordered_set>

namespace vkt
{
namespace pipeline
{
using namespace vk;
using namespace blending_common;
namespace fs = std::filesystem;

namespace
{

template <class X>
constexpr inline std::add_pointer_t<std::add_const_t<X>> fwd_as_ptr(X &&x)
{
    return &static_cast<std::add_lvalue_reference_t<std::add_const_t<X>>>(std::forward<X>(x));
}

enum AlphaFactors
{
    AsIs,
    Exclude,
    Only
};

static std::vector<VkBlendFactor> getBlendFactors(bool dualSource, AlphaFactors afs, de::Random *rnd)
{
    static const std::unordered_set<VkBlendFactor> alphaFactors{
        VK_BLEND_FACTOR_SRC_ALPHA,
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        VK_BLEND_FACTOR_DST_ALPHA,
        VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
        VK_BLEND_FACTOR_CONSTANT_ALPHA,
        VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
        VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,
        VK_BLEND_FACTOR_SRC1_ALPHA,
        VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,
    };

    static const std::pair<VkBlendFactor, VkBlendFactor> genericDualSourceCounterparts[]{
        {VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR},
        {VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA},
        {VK_BLEND_FACTOR_DST_COLOR, VK_BLEND_FACTOR_SRC1_COLOR},
        {VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_FACTOR_SRC1_ALPHA},
    };

    std::vector<VkBlendFactor> factors = (afs == AlphaFactors::Only) ?
                                             std::vector<VkBlendFactor>(alphaFactors.begin(), alphaFactors.end()) :
                                             blending_common::getBlendFactors();

    if (dualSource)
    {
        for (VkBlendFactor &f : factors)
        {
            static_assert(std::is_lvalue_reference_v<decltype(f)>, "???");
            for (const auto &subst : genericDualSourceCounterparts)
            {
                if (f == subst.first)
                    f = subst.second;
            }
        }
        // remove duplicates
        std::sort(factors.begin(), factors.end());
        factors.erase(std::unique(factors.begin(), factors.end()), factors.end());
    }

    if (afs == AlphaFactors::Exclude)
    {
        factors.erase(std::remove_if(factors.begin(), factors.end(),
                                     [](const VkBlendFactor &f) { return alphaFactors.find(f) != alphaFactors.end(); }),
                      factors.end());
    }

    if (rnd)
    {
        rnd->shuffle(factors.begin(), factors.end());
    }
    else
    {
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(factors.begin(), factors.end(), g);
    }

    DE_ASSERT(factors.empty() == false);

    return factors;
}

static std::vector<VkBlendOp> getBlendOps(de::Random *rnd, bool excludeMinMax = false)
{
    static const std::unordered_set<VkBlendOp> opsMinMax{VK_BLEND_OP_MIN, VK_BLEND_OP_MAX};

    auto ops = blending_common::getBlendOps();

    if (excludeMinMax)
    {
        ops.erase(std::remove_if(ops.begin(), ops.end(),
                                 [](const VkBlendOp &o) { return opsMinMax.find(o) != opsMinMax.end(); }),
                  ops.end());
    }

    if (rnd)
    {
        rnd->shuffle(ops.begin(), ops.end());
    }
    else
    {
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(ops.begin(), ops.end(), g);
    }

    return ops;
}

static VkPipelineColorBlendAttachmentState &updateColorWriteMask(VkPipelineColorBlendAttachmentState &state,
                                                                 VkFormat format)
{
    state.colorWriteMask = VkColorComponentFlags(0);

    switch (getNumUsedChannels(mapVkFormat(format).order))
    {
    case 4:
        state.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
        [[fallthrough]];
    case 3:
        state.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
        [[fallthrough]];
    case 2:
        state.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
        [[fallthrough]];
    case 1:
        state.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
    }

    return state;
}

static VkPipelineColorBlendAttachmentState makeGenericBlendAttachmentState(
    const VkPipelineColorBlendAttachmentState &other)
{
    DE_ASSERT(other.blendEnable);

    static const std::pair<VkBlendFactor, VkBlendFactor> map[]{
        {VK_BLEND_FACTOR_SRC1_COLOR, VK_BLEND_FACTOR_SRC_COLOR},
        {VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR},
        {VK_BLEND_FACTOR_SRC1_ALPHA, VK_BLEND_FACTOR_SRC_ALPHA},
        {VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA},
    };

    auto replace = [](VkBlendFactor &f)
    {
        static_assert(std::is_reference_v<decltype(f)>, "???");
        for (const std::pair<VkBlendFactor, VkBlendFactor> &m : map)
        {
            if (m.first == f)
            {
                f = m.second;
                break;
            }
        }
    };

    VkPipelineColorBlendAttachmentState state = other;
    replace(state.srcColorBlendFactor);
    replace(state.dstColorBlendFactor);
    replace(state.srcAlphaBlendFactor);
    replace(state.dstAlphaBlendFactor);

    return state;
}

static std::string makeBlendStateName(const VkPipelineColorBlendAttachmentState &blendState)
{
    static const std::pair<const char *, VkBlendFactor> blendFactorNames[]{
        {"z", VK_BLEND_FACTOR_ZERO},
        {"o", VK_BLEND_FACTOR_ONE},
        {"sc", VK_BLEND_FACTOR_SRC_COLOR},
        {"1msc", VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR},
        {"dc", VK_BLEND_FACTOR_DST_COLOR},
        {"1mdc", VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR},
        {"sa", VK_BLEND_FACTOR_SRC_ALPHA},
        {"1msa", VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA},
        {"da", VK_BLEND_FACTOR_DST_ALPHA},
        {"1mda", VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA},
        {"cc", VK_BLEND_FACTOR_CONSTANT_COLOR},
        {"1mcc", VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR},
        {"ca", VK_BLEND_FACTOR_CONSTANT_ALPHA},
        {"1mca", VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA},
        {"sas", VK_BLEND_FACTOR_SRC_ALPHA_SATURATE},
        {"1ms1c", VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR},
        {"1ms1a", VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA},
        {"s1c", VK_BLEND_FACTOR_SRC1_COLOR},
        {"s1a", VK_BLEND_FACTOR_SRC1_ALPHA},
    };

    static const std::pair<const char *, VkBlendOp> blendOpNames[]{
        {"add", VK_BLEND_OP_ADD}, {"sub", VK_BLEND_OP_SUBTRACT}, {"rsub", VK_BLEND_OP_REVERSE_SUBTRACT},
        {"min", VK_BLEND_OP_MIN}, {"max", VK_BLEND_OP_MAX},
    };

    std::ostringstream shortName;

    auto name = [&](auto field) -> std::string
    {
        if constexpr (std::is_same_v<decltype(field), VkBlendFactor>)
        {
            if (auto k =
                    std::find_if(std::begin(blendFactorNames), std::end(blendFactorNames),
                                 [&](const std::pair<const char *, VkBlendFactor> &f) { return f.second == field; });
                k != std::end(blendFactorNames))
                return k->first;
        }
        else
        {
            if (auto k = std::find_if(std::begin(blendOpNames), std::end(blendOpNames),
                                      [&](const std::pair<const char *, VkBlendOp> &f) { return f.second == field; });
                k != std::end(blendOpNames))
                return k->first;
        }
        return "???";
    };

    shortName << "color_" << name(blendState.srcColorBlendFactor) << "_" << name(blendState.dstColorBlendFactor) << "_"
              << name(blendState.colorBlendOp);
    shortName << "_alpha_" << name(blendState.srcAlphaBlendFactor) << "_" << name(blendState.dstAlphaBlendFactor) << "_"
              << name(blendState.alphaBlendOp);

    return shortName.str();
}

static VkColorBlendEquationEXT makeColorBlendEquationEXT(const VkPipelineColorBlendAttachmentState &state)
{
    VkColorBlendEquationEXT e{};

    e.srcColorBlendFactor = state.srcColorBlendFactor;
    e.dstColorBlendFactor = state.dstColorBlendFactor;
    e.colorBlendOp        = state.colorBlendOp;
    e.srcAlphaBlendFactor = state.srcAlphaBlendFactor;
    e.dstAlphaBlendFactor = state.dstAlphaBlendFactor;
    e.alphaBlendOp        = state.alphaBlendOp;

    return e;
}

static VkVertexInputBindingDescription2EXT makeVertexInputBinding2(const VkVertexInputBindingDescription &src)
{
    VkVertexInputBindingDescription2EXT i{};
    i.sType     = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT;
    i.binding   = src.binding;
    i.stride    = src.stride;
    i.inputRate = src.inputRate;
    i.divisor   = 1u;
    return i;
}

static VkVertexInputAttributeDescription2EXT makeVertexInputAttribute2(const VkVertexInputAttributeDescription &src)
{
    VkVertexInputAttributeDescription2EXT a{};
    a.sType    = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT;
    a.location = src.location;
    a.binding  = src.binding;
    a.format   = src.format;
    a.offset   = src.offset;
    return a;
}

struct DualSourceBlendMAParams
{
    VkFormat format;
    PipelineConstructionType pipelineConstructionType;

    DualSourceBlendMAParams()
        : format(VK_FORMAT_UNDEFINED)
        , pipelineConstructionType(PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
    {
    }
};

class DualSourceBlendMACase : public vkt::TestCase
{
public:
    enum
    {
        ATTACHMENT_COUNT = 4
    };

    DualSourceBlendMACase(tcu::TestContext &testContext, const std::string &name, const DualSourceBlendMAParams &params)
        : vkt::TestCase(testContext, name)
        , m_params(params)
    {
    }
    virtual void checkSupport(Context &context) const;
    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual TestInstance *createInstance(Context &context) const;

private:
    const DualSourceBlendMAParams m_params;
};

class DualSourceBlendMAInstance : public vkt::TestInstance
{
public:
    enum
    {
        ATTACHMENT_COUNT = DualSourceBlendMACase::ATTACHMENT_COUNT
    };

    enum Stages
    {
        None,
        TestGenericPipelineSrc,
        TestGenericPipelineDst,
        TestDualSourcePipeline
    };

    typedef std::array<tcu::Vec4, ATTACHMENT_COUNT> PushConstant;

    DualSourceBlendMAInstance(Context &ctx, const DualSourceBlendMAParams &params)
        : vkt::TestInstance(ctx)
        , m_renderWidth(4)
        , m_renderHeight(4)
        , m_renderArea(makeRect2D(m_renderWidth, m_renderHeight))
        , m_vertexCount(6)
        , m_params(params)
        , m_imageRange(makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u))
        , m_imageRegion(makeBufferImageCopy(makeExtent3D(m_renderWidth, m_renderHeight, 1u),
                                            makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u)))
        , m_usedExtensions()
        , m_device(createDualBlendDevice(ctx))
        , m_driver(std::make_shared<DeviceDriver>(ctx.getPlatformInterface(), ctx.getInstance(), *m_device,
                                                  ctx.getUsedApiVersion(), ctx.getTestContext().getCommandLine()))
        , m_vkd(*m_driver)
        , m_allocator(m_vkd, *m_device,
                      getPhysicalDeviceMemoryProperties(ctx.getInstanceInterface(), ctx.getPhysicalDevice()))
        , m_queue(getDeviceQueue(m_vkd, *m_device, ctx.getUniversalQueueFamilyIndex(), 0u))
        , m_vertexShaderModule(
              isConstructionTypeShaderObject(m_params.pipelineConstructionType) ?
                  ShaderWrapper() :
                  ShaderWrapper(m_vkd, *m_device, m_context.getBinaryCollection().get("common_vert"), 0))
        , m_fragmentModuleGeneric(
              isConstructionTypeShaderObject(m_params.pipelineConstructionType) ?
                  ShaderWrapper() :
                  ShaderWrapper(m_vkd, *m_device, m_context.getBinaryCollection().get("generic_frag"), 0))
        , m_fragmentModuleDualSource(
              isConstructionTypeShaderObject(m_params.pipelineConstructionType) ?
                  ShaderWrapper() :
                  ShaderWrapper(m_vkd, *m_device, m_context.getBinaryCollection().get("dual_frag"), 0))
        , m_vertexShaderObject(
              isConstructionTypeShaderObject(m_params.pipelineConstructionType) ?
                  createShader(m_vkd, *m_device,
                               makeShaderCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
                                                    m_context.getBinaryCollection().get("common_vert"))) :
                  Move<VkShaderEXT>())
        , m_fragmentObjectGeneric(
              isConstructionTypeShaderObject(m_params.pipelineConstructionType) ?
                  createShader(m_vkd, *m_device,
                               makeShaderCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                                    m_context.getBinaryCollection().get("generic_frag"))) :
                  Move<VkShaderEXT>())
        , m_fragmentObjectDualSource(
              isConstructionTypeShaderObject(m_params.pipelineConstructionType) ?
                  createShader(m_vkd, *m_device,
                               makeShaderCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                                    m_context.getBinaryCollection().get("dual_frag"))) :
                  Move<VkShaderEXT>())
        , m_inputBinding({0u, (uint32_t)sizeof(tcu::Vec4), VK_VERTEX_INPUT_RATE_VERTEX})
        , m_inputAttribute({0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u})
        , m_inputBinding2(makeVertexInputBinding2(m_inputBinding))
        , m_inputAttribute2(makeVertexInputAttribute2(m_inputAttribute))
        , m_genericPipeline(std::make_unique<GraphicsPipelineWrapper>(
              ctx.getInstanceInterface(), m_vkd, ctx.getPhysicalDevice(), *m_device, ctx.getDeviceExtensions(),
              m_params.pipelineConstructionType))
        , m_dualSourcePipeline(std::make_unique<GraphicsPipelineWrapper>(
              ctx.getInstanceInterface(), m_vkd, ctx.getPhysicalDevice(), *m_device, ctx.getDeviceExtensions(),
              m_params.pipelineConstructionType))
    {
        const uint32_t queueFamilyIndex = 0u;
        const std::vector<uint32_t> queueFamilyIndices{queueFamilyIndex};
        const auto bufferMemoryRequirements = MemoryRequirement::HostVisible | MemoryRequirement::Cached;

        // vertex buffer
        {
            const tcu::Vec4 vertices[]{{-1.0f, -1.0f, 0.0f, 0.0f}, {-1.0f, +1.0f, 0.0f, 0.0f},
                                       {+1.0f, +1.0f, 0.0f, 0.0f}, {+1.0f, +1.0f, 0.0f, 0.0f},
                                       {+1.0f, -1.0f, 0.0f, 0.0f}, {-1.0f, -1.0f, 0.0f, 0.0f}};
            DE_ASSERT(DE_LENGTH_OF_ARRAY(vertices) == m_vertexCount);
            const VkDeviceSize vertexBufferSize = m_vertexCount * sizeof(vertices[0]);
            const auto vertexBufferInfo =
                makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, queueFamilyIndices);
            m_vertexBuffer = std::make_shared<BufferWithMemory>(m_vkd, *m_device, m_allocator, vertexBufferInfo,
                                                                bufferMemoryRequirements);
            deMemcpy(m_vertexBuffer->getAllocation().getHostPtr(), vertices, (size_t)vertexBufferSize);
            flushAlloc(m_vkd, *m_device, m_vertexBuffer->getAllocation());

            m_commandPool =
                createCommandPool(m_vkd, *m_device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
            m_command = allocateCommandBuffer(m_vkd, *m_device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        }

        // pipeline layout
        {
            const VkPipelineLayoutCreateInfo pipelineLayoutParams{
                VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
                nullptr,                                       // const void* pNext;
                (VkPipelineLayoutCreateFlags)0u,               // VkPipelineLayoutCreateFlags flags;
                0u,                                            // uint32_t setLayoutCount;
                nullptr,                                       // const VkDescriptorSetLayout* pSetLayouts;
                1u,                                            // uint32_t pushConstantRangeCount;
                &m_pcRange                                     // const VkPushConstantRange* pPushConstantRanges;
            };

            m_pipelineLayout =
                PipelineLayoutWrapper(m_params.pipelineConstructionType, m_vkd, *m_device, &pipelineLayoutParams);
        }
    }

    static inline const VkPushConstantRange m_pcRange{
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, // VkShaderStageFlags stageFlags;
        0u,                                                        // uint32_t           offset
        (uint32_t)sizeof(PushConstant)                             // uint32_t           size
    };

    VkShaderCreateInfoEXT makeShaderCreateInfo(VkShaderStageFlagBits stage,
                                               const vk::ProgramBinary &programBinary) const
    {
        const bool vertex = VK_SHADER_STAGE_VERTEX_BIT == stage;
        const VkShaderStageFlags nextStage =
            vertex ? VkShaderStageFlags(VK_SHADER_STAGE_FRAGMENT_BIT) : VkShaderStageFlags(0);
        const VkShaderCreateInfoEXT shaderCreateInfo{
            vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT, // VkStructureType sType;
            nullptr,                                      // const void* pNext;
            0u,                                           // VkShaderCreateFlagsEXT flags;
            stage,                                        // VkShaderStageFlagBits stage;
            nextStage,                                    // VkShaderStageFlags nextStage;
            VK_SHADER_CODE_TYPE_SPIRV_EXT,                // VkShaderCodeTypeEXT codeType;
            programBinary.getSize(),                      // size_t codeSize;
            programBinary.getBinary(),                    // const void* pCode;
            "main",                                       // const char* pName;
            0u,                                           // uint32_t setLayoutCount;
            nullptr,                                      // VkDescriptorSetLayout* pSetLayouts;
            1u,                                           // uint32_t pushConstantRangeCount;
            &m_pcRange,                                   // const VkPushConstantRange* pPushConstantRanges;
            nullptr,                                      // const VkSpecializationInfo* pSpecializationInfo;
        };

        return shaderCreateInfo;
    }

    virtual tcu::TestStatus iterate();
    [[maybe_unused]] tcu::TestStatus iteratePerArgs(const VkFormat format,
                                                    const VkPipelineColorBlendAttachmentState dualSourceState,
                                                    const std::pair<uint32_t, uint32_t> &iteration);

    bool compareBuffers(const BufferWithMemory &received, const BufferWithMemory &expected, bool eq, Stages stage,
                        uint32_t attachment) const;
    bool isBufferZero(const BufferWithMemory &buffer) const;

private:
    void createStorages(const VkFormat (&formats)[ATTACHMENT_COUNT])
    {
        const uint32_t queueFamilyIndex = 0u;
        const std::vector<uint32_t> queueFamilyIndices{queueFamilyIndex};
        const auto bufferMemoryRequirements = MemoryRequirement::HostVisible | MemoryRequirement::Cached;

        // images, views, attachments
        for (uint32_t i = 0; i < ATTACHMENT_COUNT; ++i)
        {
            const VkImageCreateInfo ici{
                VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, //
                (const void *)nullptr,               //
                (VkImageCreateFlags)0u,              //
                VK_IMAGE_TYPE_2D,                    //
                formats[i],                          //
                m_imageRegion.imageExtent,           //
                m_imageRange.levelCount,             // mipLevels
                m_imageRange.layerCount,             // arrayLayers
                VK_SAMPLE_COUNT_1_BIT,               //
                VK_IMAGE_TILING_OPTIMAL,             //
                (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT), // usage
                VK_SHARING_MODE_EXCLUSIVE,         //
                1u,                                // queueFamilyIndexCount
                &queueFamilyIndex,                 //
                VK_IMAGE_LAYOUT_UNDEFINED          //
            };

            m_images[i] = std::make_shared<ImageWithMemory>(m_vkd, *m_device, m_allocator, ici, MemoryRequirement::Any);

            const VkImageViewCreateInfo ivci{
                VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
                nullptr,                                  // const void* pNext;
                VkImageViewCreateFlags(0),                // VkImageViewCreateFlags flags;
                **m_images[i],                            // VkImage image;
                VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
                formats[i],                               // VkFormat format;
                {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                 VK_COMPONENT_SWIZZLE_IDENTITY},
                // VkComponentMapping components;
                m_imageRange, // VkImageSubresourceRange subresourceRange;
            };

            m_views[i] = createImageView(m_vkd, *m_device, &ivci);

            const tcu::TextureFormat attachmentFormat = mapVkFormat(formats[i]);
            const VkDeviceSize attachmentSize = m_renderWidth * m_renderHeight * attachmentFormat.getPixelSize();
            const auto attachmentInfo =
                makeBufferCreateInfo(attachmentSize,
                                     (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                                     queueFamilyIndices);

            m_destAttachments[i]    = std::make_shared<BufferWithMemory>(m_vkd, *m_device, m_allocator, attachmentInfo,
                                                                      bufferMemoryRequirements);
            m_genericAttachments[i] = std::make_shared<BufferWithMemory>(m_vkd, *m_device, m_allocator, attachmentInfo,
                                                                         bufferMemoryRequirements);
            m_dualAttachments[i]    = std::make_shared<BufferWithMemory>(m_vkd, *m_device, m_allocator, attachmentInfo,
                                                                      bufferMemoryRequirements);
            m_sourceAttachments[i]  = std::make_shared<BufferWithMemory>(m_vkd, *m_device, m_allocator, attachmentInfo,
                                                                        bufferMemoryRequirements);
        }
    }

    void resetBuffers()
    {
        for (uint32_t i = 0; i < ATTACHMENT_COUNT; ++i)
        {
            deMemset(m_destAttachments[i]->getAllocation().getHostPtr(), 0,
                     (size_t)m_destAttachments[i]->getBufferSize());
            flushAlloc(m_vkd, *m_device, m_destAttachments[i]->getAllocation());

            deMemset(m_genericAttachments[i]->getAllocation().getHostPtr(), 0,
                     (size_t)m_genericAttachments[i]->getBufferSize());
            flushAlloc(m_vkd, *m_device, m_genericAttachments[i]->getAllocation());

            deMemset(m_dualAttachments[i]->getAllocation().getHostPtr(), 0,
                     (size_t)m_dualAttachments[i]->getBufferSize());
            flushAlloc(m_vkd, *m_device, m_dualAttachments[i]->getAllocation());

            deMemset(m_sourceAttachments[i]->getAllocation().getHostPtr(), 0,
                     (size_t)m_sourceAttachments[i]->getBufferSize());
            flushAlloc(m_vkd, *m_device, m_sourceAttachments[i]->getAllocation());
        }
    }

    // generic and dual renderpasses
    void createRenderPassesAndFramebuffers(const VkFormat (&formats)[ATTACHMENT_COUNT])
    {
        std::vector<VkAttachmentDescription> colorAttachmentDescriptions(
            ATTACHMENT_COUNT,
            {
                (VkAttachmentDescriptionFlags)0,         // VkAttachmentDescriptionFlags    flags
                VK_FORMAT_UNDEFINED,                     // VkFormat                        format
                VK_SAMPLE_COUNT_1_BIT,                   // VkSampleCountFlagBits           samples
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // VkAttachmentLoadOp      ***     loadOp
                VK_ATTACHMENT_STORE_OP_STORE,            // VkAttachmentStoreOp             storeOp
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // VkAttachmentLoadOp              stencilLoadOp
                VK_ATTACHMENT_STORE_OP_DONT_CARE,        // VkAttachmentStoreOp             stencilStoreOp
                VK_IMAGE_LAYOUT_UNDEFINED,               // VkImageLayout                   initialLayout
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout                   finalLayout
            });

        std::vector<VkAttachmentReference> colorAttachmentRefs(
            ATTACHMENT_COUNT,
            {
                0u,                                      // uint32_t         attachment
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout    layout
            });

        for (uint32_t i = 0; i < ATTACHMENT_COUNT; ++i)
        {
            colorAttachmentRefs[i].attachment            = i;
            colorAttachmentDescriptions[i].format        = formats[i];
            colorAttachmentDescriptions[i].loadOp        = VK_ATTACHMENT_LOAD_OP_LOAD;
            colorAttachmentDescriptions[i].initialLayout = VK_IMAGE_LAYOUT_GENERAL;
        }

        VkSubpassDescription subpassDescription{
            (VkSubpassDescriptionFlags)0,    // VkSubpassDescriptionFlags       flags
            VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint             pipelineBindPoint
            0u,                              // uint32_t                        inputAttachmentCount
            nullptr,                         // const VkAttachmentReference*    pInputAttachments
            ATTACHMENT_COUNT,                // uint32_t                        colorAttachmentCount
            colorAttachmentRefs.data(),      // const VkAttachmentReference*    pColorAttachments
            nullptr,                         // const VkAttachmentReference*    pResolveAttachments
            nullptr,                         // const VkAttachmentReference*    pDepthStencilAttachment
            0u,                              // uint32_t                        preserveAttachmentCount
            nullptr                          // const uint32_t*                 pPreserveAttachments
        };

        VkRenderPassCreateInfo renderPassInfo{
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType                   sType
            nullptr,                                   // const void*                       pNext
            (VkRenderPassCreateFlags)0,                // VkRenderPassCreateFlags           flags
            ATTACHMENT_COUNT,                          // uint32_t                          attachmentCount
            colorAttachmentDescriptions.data(),        // const VkAttachmentDescription*    pAttachments
            1u,                                        // uint32_t                          subpassCount
            &subpassDescription,                       // const VkSubpassDescription*       pSubpasses
            0u,                                        // uint32_t                          dependencyCount
            nullptr                                    // const VkSubpassDependency*        pDependencies
        };

        subpassDescription.colorAttachmentCount = 1u;
        m_renderPassDualSource                  = createRenderPass(m_vkd, *m_device, &renderPassInfo, nullptr);

        colorAttachmentRefs[0].attachment       = VK_ATTACHMENT_UNUSED;
        subpassDescription.colorAttachmentCount = ATTACHMENT_COUNT;
        m_renderPassGeneric                     = createRenderPass(m_vkd, *m_device, &renderPassInfo, nullptr);

        // framebuffers
        VkImageView attachmentViews[ATTACHMENT_COUNT];
        for (uint32_t i = 0; i < ATTACHMENT_COUNT; ++i)
        {
            attachmentViews[i] = *m_views[i];
        }

        VkFramebufferCreateInfo framebufferParams{
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                                   // const void* pNext;
            (VkFramebufferCreateFlags)0,               // VkFramebufferCreateFlags flags;
            VK_NULL_HANDLE,                            // VkRenderPass renderPass;
            ATTACHMENT_COUNT,                          // uint32_t attachmentCount;
            attachmentViews,                           // const VkImageView* pAttachments;
            m_renderWidth,                             // uint32_t width;
            m_renderHeight,                            // uint32_t height;
            1u                                         // uint32_t layers;
        };

        framebufferParams.renderPass = *m_renderPassDualSource;
        m_framebufferDualSource      = createFramebuffer(m_vkd, *m_device, &framebufferParams);

        framebufferParams.renderPass = *m_renderPassGeneric;
        m_framebufferGeneric         = createFramebuffer(m_vkd, *m_device, &framebufferParams);
    }

    void recreatePipeline(bool dualSource, const VkPipelineColorBlendAttachmentState dualSourceState,
                          bool check = false)
    {
        const VkPipelineVertexInputStateCreateInfo vertexInputStateParams{
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                   // const void* pNext;
            0u,                                                        // VkPipelineVertexInputStateCreateFlags flags;
            1u,                                                        // uint32_t vertexBindingDescriptionCount;
            &m_inputBinding,  // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
            1u,               // uint32_t vertexAttributeDescriptionCount;
            &m_inputAttribute // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
        };

        const std::vector<VkViewport> viewports{makeViewport(tcu::UVec2(m_renderWidth, m_renderHeight))};
        const std::vector<VkRect2D> scissors{m_renderArea};

        std::array<VkPipelineColorBlendAttachmentState, ATTACHMENT_COUNT> attachments;
        attachments.fill(makeGenericBlendAttachmentState(dualSourceState));

        struct BlendConstGenerator
        {
            float first;
            float step;
            BlendConstGenerator(float first_, float step_) : first(first_), step(step_)
            {
            }
            float operator()()
            {
                float k = first;
                first += step;
                return k;
            }
        };

        VkPipelineColorBlendStateCreateInfo colorBlendStateParams{
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                  // const void* pNext;
            0u,                                                       // VkPipelineColorBlendStateCreateFlags flags;
            VK_FALSE,                                                 // VkBool32 logicOpEnable;
            VK_LOGIC_OP_COPY,                                         // VkLogicOp logicOp;
            ATTACHMENT_COUNT,                                         // uint32_t attachmentCount;
            attachments.data(),   // const VkPipelineColorBlendAttachmentState* pAttachments;
            {/*populated later*/} // float blendConstants[4];
        };
        std::generate(std::begin(colorBlendStateParams.blendConstants), std::end(colorBlendStateParams.blendConstants),
                      BlendConstGenerator(0.333f, 0.111f));

        const VkPipeline lastHandle =
            check ? dualSource ? m_dualSourcePipeline->getPipeline() : m_genericPipeline->getPipeline() :
                    VK_NULL_HANDLE;

        if (dualSource)
        {
            attachments.at(0)                     = dualSourceState;
            colorBlendStateParams.attachmentCount = 1;

            auto p = std::make_unique<GraphicsPipelineWrapper>(m_context.getInstanceInterface(), m_vkd,
                                                               m_context.getPhysicalDevice(), *m_device,
                                                               m_usedExtensions, m_params.pipelineConstructionType);
            p->setDefaultRasterizationState()
                .setDefaultDepthStencilState()
                .setDefaultMultisampleState()
                .setupVertexInputState(&vertexInputStateParams)
                .setupPreRasterizationShaderState(viewports, scissors, m_pipelineLayout, *m_renderPassDualSource, 0u,
                                                  m_vertexShaderModule)
                .setupFragmentShaderState(m_pipelineLayout, *m_renderPassDualSource, 0u, m_fragmentModuleDualSource)
                .setupFragmentOutputState(*m_renderPassDualSource, 0u, &colorBlendStateParams)
                .setMonolithicPipelineLayout(m_pipelineLayout)
                .buildPipeline();

            m_dualSourcePipeline.swap(p);
        }
        else
        {
            colorBlendStateParams.attachmentCount = ATTACHMENT_COUNT;

            auto p = std::make_unique<GraphicsPipelineWrapper>(m_context.getInstanceInterface(), m_vkd,
                                                               m_context.getPhysicalDevice(), *m_device,
                                                               m_usedExtensions, m_params.pipelineConstructionType);
            p->setDefaultRasterizationState()
                .setDefaultDepthStencilState()
                .setDefaultMultisampleState()
                .setupVertexInputState(&vertexInputStateParams)
                .setupPreRasterizationShaderState(viewports, scissors, m_pipelineLayout, *m_renderPassGeneric, 0u,
                                                  m_vertexShaderModule)
                .setupFragmentShaderState(m_pipelineLayout, *m_renderPassGeneric, 0u, m_fragmentModuleGeneric)
                .setupFragmentOutputState(*m_renderPassGeneric, 0u, &colorBlendStateParams)
                .setMonolithicPipelineLayout(m_pipelineLayout)
                .buildPipeline();

            m_genericPipeline.swap(p);
        }

        if (check)
        {
            const VkPipeline newHandle =
                dualSource ? m_dualSourcePipeline->getPipeline() : m_genericPipeline->getPipeline();
            DE_ASSERT(newHandle != lastHandle);
            DE_UNREF(lastHandle);
            DE_UNREF(newHandle);
        }
    }

    void beginRendering(VkCommandBuffer cmd, VkRect2D renderArea, VkFormat format, bool dualSource) const
    {
        DE_UNREF(format);

        const VkRenderingAttachmentInfo colorAttachmentTemplate{
            VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR, // VkStructureType sType;
            nullptr,                                         // const void* pNext;
            VK_NULL_HANDLE,                                  // VkImageView imageView;
            VK_IMAGE_LAYOUT_GENERAL,                         // VkImageLayout imageLayout;
            VK_RESOLVE_MODE_NONE,                            // VkResolveModeFlagBits resolveMode;
            VK_NULL_HANDLE,                                  // VkImageView resolveImageView;
            VK_IMAGE_LAYOUT_UNDEFINED,                       // VkImageLayout resolveImageLayout;
            VK_ATTACHMENT_LOAD_OP_LOAD,                      // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,                    // VkAttachmentStoreOp storeOp;
            {}                                               // VkClearValue clearValue;
        };

        std::vector<VkRenderingAttachmentInfo> colorAttachments(ATTACHMENT_COUNT, colorAttachmentTemplate);
        for (uint32_t i = 0u; i < ATTACHMENT_COUNT; ++i)
        {
            colorAttachments[i].imageView = *m_views[i];
        }

        const uint32_t colorAttachmentCount = dualSource ? 1u : uint32_t(ATTACHMENT_COUNT);

        VkRenderingInfoKHR renderingInfo{
            VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
            nullptr,
            (vk::VkRenderingFlags)0u, // VkRenderingFlagsKHR flags;
            renderArea,               // VkRect2D renderArea;
            1u,                       // uint32_t layerCount;
            0x0,                      // uint32_t viewMask;
            colorAttachmentCount,     // uint32_t colorAttachmentCount;
            colorAttachments.data(),  // const VkRenderingAttachmentInfoKHR* pColorAttachments;
            nullptr,                  // const VkRenderingAttachmentInfoKHR* pDepthAttachment;
            nullptr,                  // const VkRenderingAttachmentInfoKHR* pStencilAttachment;
        };

        m_vkd.cmdBeginRendering(cmd, &renderingInfo);
    }

    Move<VkDevice> createDualBlendDevice(Context &ctx);

private:
    const uint32_t m_renderWidth;
    const uint32_t m_renderHeight;
    const VkRect2D m_renderArea;
    const uint32_t m_vertexCount;
    const DualSourceBlendMAParams m_params;
    const VkImageSubresourceRange m_imageRange;
    const VkBufferImageCopy m_imageRegion;
    std::vector<std::string> m_usedExtensions;
    Move<VkDevice> m_device;
    std::shared_ptr<DeviceDriver> m_driver;
    const DeviceInterface &m_vkd;
    SimpleAllocator m_allocator;
    const VkQueue m_queue;
    ShaderWrapper m_vertexShaderModule;
    ShaderWrapper m_fragmentModuleGeneric;
    ShaderWrapper m_fragmentModuleDualSource;
    Move<VkShaderEXT> m_vertexShaderObject;
    Move<VkShaderEXT> m_fragmentObjectGeneric;
    Move<VkShaderEXT> m_fragmentObjectDualSource;
    const VkVertexInputBindingDescription m_inputBinding;
    const VkVertexInputAttributeDescription m_inputAttribute;
    const VkVertexInputBindingDescription2EXT m_inputBinding2;
    const VkVertexInputAttributeDescription2EXT m_inputAttribute2;
    std::unique_ptr<GraphicsPipelineWrapper> m_genericPipeline;
    std::unique_ptr<GraphicsPipelineWrapper> m_dualSourcePipeline;
    Move<VkRenderPass> m_renderPassGeneric;
    Move<VkRenderPass> m_renderPassDualSource;
    Move<VkFramebuffer> m_framebufferGeneric;
    Move<VkFramebuffer> m_framebufferDualSource;
    Move<VkCommandPool> m_commandPool;
    Move<VkCommandBuffer> m_command;
    PipelineLayoutWrapper m_pipelineLayout;
    std::shared_ptr<BufferWithMemory> m_vertexBuffer;
    Move<VkImageView> m_views[ATTACHMENT_COUNT];
    std::shared_ptr<ImageWithMemory> m_images[ATTACHMENT_COUNT];
    std::shared_ptr<BufferWithMemory> m_dualAttachments[ATTACHMENT_COUNT];
    std::shared_ptr<BufferWithMemory> m_destAttachments[ATTACHMENT_COUNT];
    std::shared_ptr<BufferWithMemory> m_genericAttachments[ATTACHMENT_COUNT];
    std::shared_ptr<BufferWithMemory> m_sourceAttachments[ATTACHMENT_COUNT];
};

void DualSourceBlendMACase::checkSupport(Context &context) const
{
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_params.pipelineConstructionType);

    const vk::VkPhysicalDeviceProperties &properties = context.getDeviceProperties();
    if (ATTACHMENT_COUNT > properties.limits.maxFragmentOutputAttachments)
    {
        throw tcu::NotSupportedError("Used attachment count exceeds maxFragmentOutputAttachments limit");
    }

    const vk::VkPhysicalDeviceFeatures &features = context.getDeviceFeatures();
    if (VK_TRUE != features.dualSrcBlend)
        throw tcu::NotSupportedError("Dual-Source blending not supported");

    if (isConstructionTypeShaderObject(m_params.pipelineConstructionType))
    {
        context.requireDeviceFunctionality("VK_EXT_shader_object");
        context.requireDeviceFunctionality("VK_EXT_color_write_enable");
    }

    for (uint32_t i = 0; i < ATTACHMENT_COUNT; ++i)
    {
        if (!isSupportedBlendFormat(context.getInstanceInterface(), context.getPhysicalDevice(), m_params.format))
            throw tcu::NotSupportedError(std::string("Unsupported color blending format: ") +
                                         getFormatName(m_params.format));
        if (!isSupportedTransferFormat(context.getInstanceInterface(), context.getPhysicalDevice(), m_params.format))
            throw tcu::NotSupportedError(std::string("Unsupported color transfer format: ") +
                                         getFormatName(m_params.format));
    }
}

void DualSourceBlendMACase::initPrograms(SourceCollections &sourceCollections) const
{
    const std::string commonVert(
        R"(#version 450
    layout(location = 0) in vec4 pos;
    void main() {
        gl_Position = vec4(pos.xy, 0, 1);
    })");

    const std::string genericFrag(
        R"(#version 450
    layout(push_constant) uniform PC
    {
        vec4 inColor0, inColor1, inColor2, inColor3;
    };
    layout(location = 0) out vec4 outColor0;
    layout(location = 1) out vec4 outColor1;
    layout(location = 2) out vec4 outColor2;
    layout(location = 3) out vec4 outColor3;
    void main() {
        outColor0 = inColor0;
        outColor1 = inColor1;
        outColor2 = inColor2;
        outColor3 = inColor3;
    })");

    const std::string dualSourceFrag(
        R"(#version 450
    layout(push_constant) uniform PC
    {
        vec4 inColor0, inColor1, inColor2, inColor3;
    };
    layout(location = 0, index = 0) out vec4 outColor0;
    layout(location = 0, index = 1) out vec4 outColor1;
    void main() {
        outColor0 = inColor0;
        outColor1 = inColor1;
    })");

    sourceCollections.glslSources.add("common_vert") << glu::VertexSource(commonVert);
    sourceCollections.glslSources.add("generic_frag") << glu::FragmentSource(genericFrag);
    sourceCollections.glslSources.add("dual_frag") << glu::FragmentSource(dualSourceFrag);
}

TestInstance *DualSourceBlendMACase::createInstance(Context &context) const
{
    return new DualSourceBlendMAInstance(context, m_params);
}

struct BlendAttachmentStateGenerator
{
    enum DualSourceFlags
    {
        srcColorFactor = 0x1,
        dstColorFactor = 0x2,
        srcAlphaFactor = 0x4,
        dstAlphaFactor = 0x8,
        allFactors     = 0xF
    };

    BlendAttachmentStateGenerator(uint32_t dualSourceMask, VkFormat fmt, uint32_t limit, de::Random *rnd = nullptr)
        : blendMask(dualSourceMask)
        , format(fmt)
        , hasAlphaComponent(getNumUsedChannels(mapVkFormat(format).order) == 4)
        , srcColorSet(getBlendFactors(blendMask & srcColorFactor,
                                      hasAlphaComponent ? AlphaFactors::AsIs : AlphaFactors::Exclude, rnd))
        , dstColorSet(getBlendFactors(blendMask & dstColorFactor,
                                      hasAlphaComponent ? AlphaFactors::AsIs : AlphaFactors::Exclude, rnd))
        , srcAlphaSet(getBlendFactors(blendMask & srcAlphaFactor, AlphaFactors::Only, rnd))
        , dstAlphaSet(getBlendFactors(blendMask & dstAlphaFactor, AlphaFactors::Only, rnd))
        , colorOpSet(getBlendOps(rnd))
        , alphaOpSet(getBlendOps(rnd))
        , srcColor(genIndices(uint32_t(srcColorSet.size()), rnd, limit))
        , dstColor(genIndices(uint32_t(dstColorSet.size()), rnd, limit))
        , srcAlpha(genIndices(hasAlphaComponent ? uint32_t(srcAlphaSet.size()) : 1u, rnd, limit))
        , dstAlpha(genIndices(hasAlphaComponent ? uint32_t(dstAlphaSet.size()) : 1u, rnd, limit))
        , colorOp(genIndices(uint32_t(colorOpSet.size()), rnd, (limit / 2)))
        , alphaOp(genIndices(hasAlphaComponent ? uint32_t(alphaOpSet.size()) : 1u, rnd, (limit / 2)))
        , vectors{&srcColor, &dstColor, &srcAlpha, &dstAlpha, &colorOp, &alphaOp}
        , combination(vectors.size(), 0)
        , indices(vectors.size(), 0)
    {
        for (const std::vector<uint32_t> *v : vectors)
        {
            if (v->size() == 0)
                DE_ASSERT(false);
        }
    }

    static std::vector<uint32_t> genIndices(uint32_t size, de::Random *, uint32_t limit)
    {
        const uint32_t count = (size > limit) ? limit : size;
        std::vector<uint32_t> indices(count);
        std::iota(indices.begin(), indices.end(), 0u);
        return indices;
    }

    bool next(VkPipelineColorBlendAttachmentState &state, uint32_t *pAchieved = nullptr, bool add = true)
    {
        if (firstCall)
        {
            firstCall = false;
            getCurrentCombination(state);
            count = 1u;
            if (pAchieved)
            {
                if (add)
                    ++*pAchieved;
                else
                    *pAchieved = 1u;
            }
            return true;
        }

        for (int i = int(indices.size()) - 1; i >= 0; --i)
        {
            if (++indices[i] < vectors[i]->size())
            {
                getCurrentCombination(state);
                ++count;
                if (pAchieved)
                {
                    if (add)
                        ++*pAchieved;
                    else
                        *pAchieved = 1u;
                }
                return true;
            }
            indices[i] = 0;
        }

        return false;
    }

    void reset()
    {
        count = 0u;
        for (size_t i = 0; i < vectors.size(); ++i)
            indices[i] = 0u;
    }

    uint32_t getCount() const
    {
        return count;
    }

    bool yieldsZero(const VkPipelineColorBlendAttachmentState &s) const
    {
        bool zero = false;
        switch (s.colorBlendOp)
        {
        case VK_BLEND_OP_SUBTRACT:
            zero = s.srcColorBlendFactor == VK_BLEND_FACTOR_DST_COLOR &&
                   s.dstColorBlendFactor == VK_BLEND_FACTOR_SRC_COLOR;
            break;
        case VK_BLEND_OP_REVERSE_SUBTRACT:
            zero = s.srcColorBlendFactor == VK_BLEND_FACTOR_SRC_COLOR &&
                   s.dstColorBlendFactor == VK_BLEND_FACTOR_DST_COLOR;
            break;
        default:
            break;
        }
        return zero || (s.srcColorBlendFactor == VK_BLEND_FACTOR_ZERO && s.dstColorBlendFactor == VK_BLEND_FACTOR_ZERO);
    }

    uint64_t getMax(bool excludeYieldingZero = false)
    {
        if (0u == max)
        {
            if (excludeYieldingZero)
            {
                VkPipelineColorBlendAttachmentState s{};
                BlendAttachmentStateGenerator g(*this);
                g.reset();
                while (g.next(s))
                {
                    if (false == yieldsZero(s))
                        max += 1u;
                }
            }
            else
            {
                max = 1u;
                for (size_t i = 0; i < vectors.size(); ++i)
                {
                    const auto size = vectors.at(i)->size();
                    max *= size;
                }
            }
        }
        return max;
    }

private:
    void getCurrentCombination(VkPipelineColorBlendAttachmentState &state)
    {
        for (size_t i = 0; i < vectors.size(); ++i)
        {
            combination[i] = vectors[i]->at(indices[i]);
        }

        const bool a              = hasAlphaComponent;
        state.blendEnable         = VK_TRUE;
        state.srcColorBlendFactor = srcColorSet.at(combination.at(0));
        state.dstColorBlendFactor = dstColorSet.at(combination.at(1));
        if (a)
        {
            state.srcAlphaBlendFactor = srcAlphaSet.at(combination.at(2));
            state.dstAlphaBlendFactor = dstAlphaSet.at(combination.at(3));
        }
        else
        {
            state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        }
        state.colorBlendOp = colorOpSet.at(combination.at(4));
        state.alphaBlendOp = alphaOpSet.at(combination.at(5));
        updateColorWriteMask(state, format);

        if (a)
        {
            DE_ASSERT(isAlphaBlendFactor(state.srcAlphaBlendFactor));
            DE_ASSERT(isAlphaBlendFactor(state.dstAlphaBlendFactor));
        }
        else
        {
            DE_ASSERT(false == isAlphaBlendFactor(state));
        }
    }

    const uint32_t blendMask;
    const VkFormat format;
    const bool hasAlphaComponent;
    const std::vector<VkBlendFactor> srcColorSet;
    const std::vector<VkBlendFactor> dstColorSet;
    const std::vector<VkBlendFactor> srcAlphaSet;
    const std::vector<VkBlendFactor> dstAlphaSet;
    const std::vector<VkBlendOp> colorOpSet;
    const std::vector<VkBlendOp> alphaOpSet;
    const std::vector<uint32_t> srcColor;
    const std::vector<uint32_t> dstColor;
    const std::vector<uint32_t> srcAlpha;
    const std::vector<uint32_t> dstAlpha;
    const std::vector<uint32_t> colorOp;
    const std::vector<uint32_t> alphaOp;
    const std::vector<const std::vector<uint32_t> *> vectors;
    std::vector<uint32_t> combination;
    std::vector<size_t> indices;
    bool firstCall = true;
    uint32_t count = 0;
    uint64_t max   = 0;
};

tcu::TestStatus DualSourceBlendMAInstance::iterate()
{
    tcu::TestLog &log = m_context.getTestContext().getLog();

    const std::string logFile =
        fs::path(m_context.getTestContext().getCommandLine().getLogFileName()).filename().string();
    const bool logOver = logFile.find("dual_blend") != std::string::npos;
    const bool logPass = logOver && (logFile.find("pass") != std::string::npos);
    const bool logWarn = logOver && (logFile.find("warn") != std::string::npos);
    const bool logFail = logOver && (logFile.find("fail") != std::string::npos);

    VkFormat formats[ATTACHMENT_COUNT];
    std::fill_n(std::begin(formats), ATTACHMENT_COUNT, m_params.format);

    createStorages(formats);
    createRenderPassesAndFramebuffers(formats);

    const auto seed = m_context.getTestContext().getCommandLine().getBaseSeed();

    de::Random rnd(seed ? uint32_t(seed) : 13u), *pRnd = &rnd;
    const uint32_t dualFlags =
        BlendAttachmentStateGenerator::dstColorFactor | BlendAttachmentStateGenerator::dstAlphaFactor;
    BlendAttachmentStateGenerator stateGenerator(dualFlags, m_params.format, 5u, pRnd);

    log << tcu::TestLog::Message << stateGenerator.getMax() << " will be processed" << tcu::TestLog::EndMessage;

    uint32_t failCount             = 0;
    const uint32_t maxFailCount    = std::numeric_limits<uint32_t>::max();
    const bool excludeYieldingZero = false;
    std::vector<std::string> passMessageCollector;
    std::vector<std::string> failMessageCollector;
    VkPipelineColorBlendAttachmentState dualSourceState{};
    std::pair<uint32_t, uint32_t> iterationInfo{0u, uint32_t(stateGenerator.getMax(excludeYieldingZero))};
    failMessageCollector.reserve(static_cast<std::vector<std::string>::size_type>(stateGenerator.getMax()));

    while (stateGenerator.next(dualSourceState, &iterationInfo.first))
    {
        if (excludeYieldingZero && stateGenerator.yieldsZero(dualSourceState))
        {
            iterationInfo.first -= 1u;
            continue;
        }

        if (const tcu::TestStatus status = iteratePerArgs(m_params.format, dualSourceState, iterationInfo);
            status.isFail())
        {
            failCount += 1u;
            if (logFail)
                failMessageCollector.push_back(status.getDescription());
        }
        else if (logPass || logWarn)
        {
            passMessageCollector.push_back(status.getDescription());
        }

        if (maxFailCount <= failCount)
        {
            break;
        }
    }

    if (0u == failCount)
    {
        return tcu::TestStatus::pass(std::to_string(iterationInfo.first) + " iteration(s) processed");
    }

    if (logFail)
    {
        for (const std::string &failMessage : failMessageCollector)
        {
            log << tcu::TestLog::Message << failMessage << tcu::TestLog::EndMessage;
        }
    }

    if (logPass || logWarn)
    {
        for (const std::string &passMessage : passMessageCollector)
        {
            log << tcu::TestLog::Message << passMessage << tcu::TestLog::EndMessage;
        }
    }

    const uint32_t failPercentage = uint32_t((double(failCount) * 100) / iterationInfo.first);
    return tcu::TestStatus::fail(std::to_string(failCount) + " iteration(s) from " +
                                 std::to_string(iterationInfo.first) + " failed (" + std::to_string(failPercentage) +
                                 "%)");
}

// #define ENABLE_SPECIAL_LOGS

tcu::TestStatus DualSourceBlendMAInstance::iteratePerArgs(const VkFormat format,
                                                          const VkPipelineColorBlendAttachmentState dualSourceState,
                                                          const std::pair<uint32_t, uint32_t> &iteration)
{
    const uint32_t reusedColor = 2u;
    const PushConstant pcGeneric{
        tcu::Vec4(0.1f, 0, 0.5f, 0.25f), // color0
        tcu::Vec4(0.6f, 0.5f, 0, 0.75f), // color1
        tcu::Vec4(0.2f, 0, 0, 0.25f),    // color2
        tcu::Vec4(0.8f, 0, 0.5f, 0.75f)  // color3
    };
    const PushConstant pcDualSource{pcGeneric[reusedColor], pcGeneric[reusedColor], pcGeneric[reusedColor],
                                    pcGeneric[reusedColor]};
    VkClearValue clearValues[ATTACHMENT_COUNT];
    VkClearValue sourceValues[ATTACHMENT_COUNT];
    VkImageMemoryBarrier imageBarriers[ATTACHMENT_COUNT];
    VkBufferMemoryBarrier bufferBarriers[ATTACHMENT_COUNT];

    std::array<VkBool32, ATTACHMENT_COUNT> blendEnables;
    blendEnables.fill(VK_TRUE);

    std::array<VkBool32, ATTACHMENT_COUNT> colorWritesDualSource;
    colorWritesDualSource.fill(VK_TRUE);
    std::array<VkBool32, ATTACHMENT_COUNT> colorWritesGeneric(colorWritesDualSource);
    colorWritesGeneric[0] = VK_FALSE;

    std::array<VkPipelineColorBlendAttachmentState, ATTACHMENT_COUNT> blendStates;
    blendStates.fill(makeGenericBlendAttachmentState(dualSourceState));
    std::array<VkColorBlendEquationEXT, ATTACHMENT_COUNT> colorBlendEquationsGeneric;
    std::transform(blendStates.begin(), blendStates.end(), colorBlendEquationsGeneric.begin(),
                   makeColorBlendEquationEXT);

    std::array<VkColorComponentFlags, ATTACHMENT_COUNT> colorWriteMasks;
    std::transform(blendStates.begin(), blendStates.end(), colorWriteMasks.begin(),
                   std::mem_fn(&VkPipelineColorBlendAttachmentState::colorWriteMask));

    for (uint32_t i = 0u; i < ATTACHMENT_COUNT; ++i)
    {
        clearValues[i] = defaultClearValue(format);

        sourceValues[i] = makeClearValueColorVec4(pcGeneric[i]);

        imageBarriers[i] = makeImageMemoryBarrier(VK_ACCESS_NONE, VK_ACCESS_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
                                                  VK_IMAGE_LAYOUT_UNDEFINED, **m_images[i], m_imageRange, 0u, 0u);

        bufferBarriers[i] = makeBufferMemoryBarrier(VK_ACCESS_NONE, VK_ACCESS_NONE, **m_destAttachments[i], 0ull,
                                                    VK_WHOLE_SIZE, 0u, 0u);
    }

    auto transformImages = [&](uint32_t firstImage, uint32_t imageCount, VkAccessFlags srcAccess,
                               VkAccessFlags dstAccess, VkImageLayout oldLayout,
                               VkImageLayout newLayout) -> const VkImageMemoryBarrier(&)[ATTACHMENT_COUNT]
    {
        const uint32_t N = ((firstImage + imageCount) > uint32_t(ATTACHMENT_COUNT)) ? uint32_t(ATTACHMENT_COUNT) :
                                                                                      (firstImage + imageCount);
        for (; firstImage < N; ++firstImage)
        {
            imageBarriers[firstImage].srcAccessMask = srcAccess;
            imageBarriers[firstImage].dstAccessMask = dstAccess;
            imageBarriers[firstImage].oldLayout     = oldLayout;
            imageBarriers[firstImage].newLayout     = newLayout;
        }
        return imageBarriers;
    };

    auto transformBuffers = [&](const std::shared_ptr<BufferWithMemory>(&buffers)[ATTACHMENT_COUNT],
                                VkAccessFlags srcAccess,
                                VkAccessFlags dstAccess) -> const VkBufferMemoryBarrier(&)[ATTACHMENT_COUNT]
    {
        for (uint32_t i = 0u; i < ATTACHMENT_COUNT; ++i)
        {
            bufferBarriers[i].buffer        = **buffers[i];
            bufferBarriers[i].srcAccessMask = srcAccess;
            bufferBarriers[i].dstAccessMask = dstAccess;
        }
        return bufferBarriers;
    };

    auto _recordCleanImages = [&](VkCommandBuffer cmd) -> void
    {
        // clang-format off

        auto cleanImages = [&](const VkClearValue (&colors)[ATTACHMENT_COUNT],
                               const std::shared_ptr<BufferWithMemory> (&buffers)[ATTACHMENT_COUNT])
        {
            m_vkd.cmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_DEPENDENCY_BY_REGION_BIT, 0u, nullptr, 0u, nullptr,
                ATTACHMENT_COUNT, transformImages(0, ATTACHMENT_COUNT,
                    VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));

            for (uint32_t i = 0; i < ATTACHMENT_COUNT; ++i)
            {
                m_vkd.cmdClearColorImage(cmd, **m_images[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    &colors[i].color, 1u, &m_imageRange);
            }

            m_vkd.cmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_DEPENDENCY_BY_REGION_BIT, 0u, nullptr,
                ATTACHMENT_COUNT, transformBuffers(buffers, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT),
                ATTACHMENT_COUNT, transformImages(0, ATTACHMENT_COUNT,
                    VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));

            for (uint32_t i = 0; i < ATTACHMENT_COUNT; ++i)
            {
                m_vkd.cmdCopyImageToBuffer(cmd, **m_images[i], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    **buffers[i], 1u, &m_imageRegion);
            }

            m_vkd.cmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                VK_DEPENDENCY_BY_REGION_BIT, 0u, nullptr,
                ATTACHMENT_COUNT, transformBuffers(buffers, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_NONE),
                ATTACHMENT_COUNT, transformImages(0, ATTACHMENT_COUNT,
                    VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_NONE,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL));
        };

        cleanImages(sourceValues, m_sourceAttachments);
        cleanImages(clearValues, m_destAttachments);

        // clang-format off
    };

    const bool isso = isConstructionTypeShaderObject(m_params.pipelineConstructionType);

    auto recordGenericBlending = [&](VkCommandBuffer cmd, bool standalone) -> void
    {
        // clang-format off
        _recordCleanImages(cmd);

        m_vkd.cmdBindVertexBuffers(cmd, 0u, 1u, fwd_as_ptr(**m_vertexBuffer), fwd_as_ptr(VkDeviceSize(0)));
        m_vkd.cmdPushConstants(cmd, *m_pipelineLayout, m_pcRange.stageFlags, 0u, m_pcRange.size, pcGeneric.data());

        // draw to all attachments using generic blending
        if (isso)
        {
            beginRendering(cmd, m_renderArea, format, false);
            bindGraphicsShaders(m_vkd, cmd,
                *m_vertexShaderObject, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, *m_fragmentObjectGeneric, false, false);
            setDefaultShaderObjectDynamicStates(m_vkd, cmd, m_usedExtensions, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
            m_vkd.cmdSetVertexInputEXT(cmd, 1u, &m_inputBinding2, 1u, &m_inputAttribute2);
            m_vkd.cmdBindVertexBuffers(cmd, 0u, 1u, fwd_as_ptr(**m_vertexBuffer), fwd_as_ptr(VkDeviceSize(0)));
            m_vkd.cmdSetViewportWithCount(cmd, 1u, fwd_as_ptr(makeViewport(tcu::IVec2((int)m_renderWidth, (int)m_renderHeight))));
            m_vkd.cmdSetScissorWithCount(cmd, 1u, fwd_as_ptr(makeRect2D(tcu::UVec2(m_renderWidth, m_renderHeight))));
            m_vkd.cmdSetColorWriteEnableEXT(cmd, ATTACHMENT_COUNT, colorWritesGeneric.data());
            m_vkd.cmdSetColorBlendEnableEXT(cmd, 0u, ATTACHMENT_COUNT, blendEnables.data());
            m_vkd.cmdSetColorBlendEquationEXT(cmd, 0u, ATTACHMENT_COUNT, colorBlendEquationsGeneric.data());
            m_vkd.cmdSetColorWriteMaskEXT(cmd, 0u, ATTACHMENT_COUNT, colorWriteMasks.data());
            m_vkd.cmdDraw(cmd, m_vertexCount, 1u, 0u, 0u);
            m_vkd.cmdEndRendering(cmd);
        }
        else
        {
            m_genericPipeline->bind(cmd);
            beginRenderPass(m_vkd, cmd, *m_renderPassGeneric, *m_framebufferGeneric, m_renderArea, 0, nullptr);
            m_vkd.cmdDraw(cmd, m_vertexCount, 1u, 0u, 0u);
            endRenderPass(m_vkd, cmd);
        }

        // preparing attachments to be copied to genericAttachments buffers
        m_vkd.cmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_DEPENDENCY_BY_REGION_BIT, 0u, nullptr,
            ATTACHMENT_COUNT, transformBuffers(m_genericAttachments, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT),
            ATTACHMENT_COUNT, transformImages(0, ATTACHMENT_COUNT,
                                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                isso ? VK_IMAGE_LAYOUT_GENERAL
                                     : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));

        for (uint32_t i = 0; i < ATTACHMENT_COUNT; ++i)
        {
            m_vkd.cmdCopyImageToBuffer(cmd, **m_images[i], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                       **m_genericAttachments[i], 1u, &m_imageRegion);
        }

        m_vkd.cmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            (standalone ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT),
            VK_DEPENDENCY_BY_REGION_BIT, 0u, nullptr,
            ATTACHMENT_COUNT, transformBuffers(m_genericAttachments, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_NONE),
            ATTACHMENT_COUNT, transformImages(0, ATTACHMENT_COUNT,
                                VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_NONE,
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL));
        // clang-format on
    };

    auto recordDualSourceBlending = [&](VkCommandBuffer cmd, const bool standalone) -> void
    {
        // clang-format off
        if (standalone)
        {
            _recordCleanImages(cmd);
        }

        m_vkd.cmdBindVertexBuffers(cmd, 0u, 1u, fwd_as_ptr(**m_vertexBuffer), fwd_as_ptr(VkDeviceSize(0)));
        m_vkd.cmdPushConstants(cmd, *m_pipelineLayout, m_pcRange.stageFlags, 0u, m_pcRange.size, pcDualSource.data());

        // draw to attachment 0 using dual-source blending
        if (isso)
        {
            beginRendering(cmd, m_renderArea, format, true);
            bindGraphicsShaders(m_vkd, cmd,
                *m_vertexShaderObject, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, *m_fragmentObjectDualSource, false, false);
            setDefaultShaderObjectDynamicStates(m_vkd, cmd, m_usedExtensions, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
            m_vkd.cmdSetVertexInputEXT(cmd, 1u, &m_inputBinding2, 1u, &m_inputAttribute2);
            m_vkd.cmdBindVertexBuffers(cmd, 0u, 1u, fwd_as_ptr(**m_vertexBuffer), fwd_as_ptr(VkDeviceSize(0)));
            m_vkd.cmdSetViewportWithCount(cmd, 1u, fwd_as_ptr(makeViewport(tcu::IVec2((int)m_renderWidth, (int)m_renderHeight))));
            m_vkd.cmdSetScissorWithCount(cmd, 1u, fwd_as_ptr(makeRect2D(tcu::UVec2(m_renderWidth, m_renderHeight))));
            m_vkd.cmdSetColorWriteEnableEXT(cmd, ATTACHMENT_COUNT, colorWritesDualSource.data());
            m_vkd.cmdSetColorBlendEnableEXT(cmd, 0u, 1u, blendEnables.data());
            m_vkd.cmdSetColorBlendEquationEXT(cmd, 0u, 1u, fwd_as_ptr(makeColorBlendEquationEXT(dualSourceState)));
            m_vkd.cmdSetColorWriteMaskEXT(cmd, 0u, 1u, &dualSourceState.colorWriteMask);
            m_vkd.cmdDraw(cmd, m_vertexCount, 1u, 0u, 0u);
            m_vkd.cmdEndRendering(cmd);
        }
        else
        {
            m_dualSourcePipeline->bind(cmd);
            beginRenderPass(m_vkd, cmd, *m_renderPassDualSource, *m_framebufferDualSource, m_renderArea, 0u, nullptr);
            m_vkd.cmdDraw(cmd, m_vertexCount, 1u, 0u, 0u);
            endRenderPass(m_vkd, cmd);
        }

        // preparing attachments to be copied to dualAttachments buffers
        m_vkd.cmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_DEPENDENCY_BY_REGION_BIT, 0u, nullptr,
            ATTACHMENT_COUNT, transformBuffers(m_dualAttachments, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT),
            ATTACHMENT_COUNT, transformImages(0, ATTACHMENT_COUNT,
                                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                isso ? VK_IMAGE_LAYOUT_GENERAL
                                     : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));

        for (uint32_t i = 0; i < ATTACHMENT_COUNT; ++i)
        {
            m_vkd.cmdCopyImageToBuffer(cmd, **m_images[i], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                       **m_dualAttachments[i], 1u, &m_imageRegion);
        }

        m_vkd.cmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            VK_DEPENDENCY_BY_REGION_BIT, 0u, nullptr,
            ATTACHMENT_COUNT, transformBuffers(m_dualAttachments, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_NONE),
            ATTACHMENT_COUNT, transformImages(0, ATTACHMENT_COUNT,
                                VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_NONE,
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL));
        // clang-format on
    };

    std::array<uint32_t, ATTACHMENT_COUNT> clearedAttachments;
    std::array<uint32_t, ATTACHMENT_COUNT> renderedAttachments;

    if (false == isConstructionTypeShaderObject(m_params.pipelineConstructionType))
    {
        recreatePipeline(false, dualSourceState, (iteration.first > 1u));
        recreatePipeline(true, dualSourceState, (iteration.first > 1u));
    }

    // run both generic and dual-blending pipelines
    resetBuffers();
    beginCommandBuffer(m_vkd, *m_command);

    recordGenericBlending(*m_command, false);
    recordDualSourceBlending(*m_command, false);

    endCommandBuffer(m_vkd, *m_command);
    submitCommandsAndWait(m_vkd, *m_device, m_queue, *m_command);

    if (isBufferZero(*m_destAttachments[reusedColor]))
    {
        // consciously skip the zero-optimized result
        return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "skip the zero-optimized result");
    }

    std::ostringstream failMessage;
    auto composeFailMessage = [&](const char *sender, const std::array<uint32_t, ATTACHMENT_COUNT> &set)
    {
        const uint32_t c = (uint32_t)std::count_if(
            set.begin(), set.end(), std::bind(std::less<int>(), std::placeholders::_1, ATTACHMENT_COUNT));
        failMessage << "Iteration " << iteration.first << " from " << iteration.second << ", "
                    << "State: " << std::quoted(makeBlendStateName(dualSourceState)) << ", " << sender << ": attachment"
                    << (c == 1u ? " " : "s ");
        for (uint32_t i = 0u, n = 0u; i < ATTACHMENT_COUNT; ++i)
        {
            if (set[i] < ATTACHMENT_COUNT)
            {
                if (n++)
                    failMessage << ',';
                failMessage << i;
            }
        }
    };

    bool verdict = true;
    std::fill(clearedAttachments.begin(), clearedAttachments.end(), ATTACHMENT_COUNT);
    std::fill(renderedAttachments.begin(), renderedAttachments.end(), ATTACHMENT_COUNT);

    // After the first drawing the contents of genericAttachments and destAttachments
    // should be different except for the first attachment
    for (uint32_t i = 0u; i < ATTACHMENT_COUNT; ++i)
    {
        bool ok             = false;
        const bool mustDiff = 0u == i;
        if (ok = compareBuffers(*m_genericAttachments[i], *m_destAttachments[i], mustDiff,
                                Stages::TestGenericPipelineDst, i) ||
                 compareBuffers(*m_genericAttachments[i], *m_sourceAttachments[i], mustDiff,
                                Stages::TestGenericPipelineSrc, i);
            !ok)
        {
            clearedAttachments[i] = i;
        }
        verdict &= ok;
    }

    // After the second draw in which only the first attachment has been drawn,
    // the contents of the dualAttachments and genericAttachments buffers should
    // be the same, except for the first attachment, whose color must match reusedColor.
    if (verdict)
    {
        bool ok = false;
        for (uint32_t i = 1u; i < ATTACHMENT_COUNT; ++i)
        {
            const uint32_t j = (reusedColor == i) ? 0u : i;
            if (ok = compareBuffers(*m_dualAttachments[j], *m_genericAttachments[i], true,
                                    Stages::TestDualSourcePipeline, i);
                !ok)
            {
                renderedAttachments[i] = i;
            }
            verdict &= ok;
        }

        if (false == verdict)
        {
            composeFailMessage("DUAL-SOURCE", renderedAttachments);
        }
    }
    else
    {
        composeFailMessage("GENERIC", clearedAttachments);
    }

#ifdef ENABLE_SPECIAL_LOGS
    if (verdict)
    {
        m_context.getTestContext().getLog()
            << tcu::TestLog::Message << "State: " << std::quoted(makeBlendStateName(dualSourceState)) << " PASS"
            << tcu::TestLog::EndMessage;
    }
    else
    {
        m_context.getTestContext().getLog() << tcu::TestLog::Message << failMessage.str() << tcu::TestLog::EndMessage;
    }
#endif

    return verdict ? tcu::TestStatus::pass(std::string()) : tcu::TestStatus::fail(failMessage.str());
}

bool DualSourceBlendMAInstance::isBufferZero(const BufferWithMemory &buffer) const
{
    Allocation &alloc = buffer.getAllocation();
    invalidateAlloc(m_vkd, *m_device, alloc);
    const char *data = reinterpret_cast<char *>(alloc.getHostPtr());
    return std::all_of(data, (data + buffer.getBufferSize()), [](const char byte) { return byte == '\0'; });
}

bool DualSourceBlendMAInstance::compareBuffers(const BufferWithMemory &received, const BufferWithMemory &expected,
                                               bool eq, Stages stage, uint32_t attachment) const
{
    DE_ASSERT(received.getBufferSize() == expected.getBufferSize());

    invalidateAlloc(m_vkd, *m_device, received.getAllocation());
    invalidateAlloc(m_vkd, *m_device, expected.getAllocation());

    tcu::ConstPixelBufferAccess R(mapVkFormat(m_params.format), (int)m_renderWidth, (int)m_renderHeight, 1,
                                  received.getAllocation().getHostPtr());
    tcu::ConstPixelBufferAccess E(mapVkFormat(m_params.format), (int)m_renderWidth, (int)m_renderHeight, 1,
                                  expected.getAllocation().getHostPtr());

    tcu::Vec4 r, e;
    bool result       = true;
    const float delta = 1.0e-4f;

    for (int y = 0; result && y < int(m_renderHeight); ++y)
        for (int x = 0u; result && x < int(m_renderWidth); ++x)
        {
            r = R.getPixel(x, y);
            e = E.getPixel(x, y);

            const bool cmp = std::fabs(r.x() - e.x()) < delta && std::fabs(r.y() - e.y()) < delta &&
                             std::fabs(r.z() - e.z()) < delta && std::fabs(r.w() - e.w()) < delta;

            result = eq ? cmp : !cmp;
        }

#ifdef ENABLE_SPECIAL_LOGS
#define STR(x) (#x)
    static auto strStage = [&]() -> const char *
    {
        switch (stage)
        {
        case TestGenericPipelineSrc:
            return STR(TestGenericPipelineSrc);
        case TestGenericPipelineDst:
            return STR(TestGenericPipelineDst);
        case TestDualSourcePipeline:
            return STR(TestDualSourcePipeline);
        default:
            return "";
        }
    };

    tcu::TestLog &log = m_context.getTestContext().getLog();
    log << tcu::TestLog::Message << ' ' << strStage() << ": EQ " << std::boolalpha << eq << std::noboolalpha
        << ", attachment " << attachment << ", expected " << e << ", got " << r << ' ' << (result ? "OK" : "FAIL")
        << ' ' << tcu::TestLog::EndMessage;

#else
    DE_UNREF(attachment);
    DE_UNREF(stage);
#endif

    return result;
}

Move<VkDevice> DualSourceBlendMAInstance::createDualBlendDevice(Context &ctx)
{
    VkInstance instance       = ctx.getInstance();
    const auto &vki           = ctx.getInstanceInterface();
    const auto physicalDevice = ctx.getPhysicalDevice();
    const float queuePriority = 1.0f;

    // Create a universal queue that supports graphics and compute
    const VkDeviceQueueCreateInfo queueParams{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                    // const void* pNext;
        0u,                                         // VkDeviceQueueCreateFlags flags;
        ctx.getUniversalQueueFamilyIndex(),         // uint32_t queueFamilyIndex;
        1u,                                         // uint32_t queueCount;
        &queuePriority                              // const float* pQueuePriorities;
    };

    void *pNext      = nullptr;
    auto chainInsert = [&](auto &str) -> void
    {
        static_assert(std::is_lvalue_reference_v<decltype(str)>, "???");
        str.pNext = pNext;
        pNext     = &str;
    };

    VkPhysicalDeviceColorWriteEnableFeaturesEXT cwef        = ctx.getColorWriteEnableFeaturesEXT();
    VkPhysicalDeviceDynamicRenderingFeatures drf            = ctx.getDynamicRenderingFeatures();
    VkPhysicalDeviceShaderObjectFeaturesEXT sof             = ctx.getShaderObjectFeaturesEXT();
    VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT gplf = ctx.getGraphicsPipelineLibraryFeaturesEXT();
    VkPhysicalDeviceSynchronization2Features sync2f         = ctx.getSynchronization2Features();
    VkPhysicalDeviceFeatures2 requiredFeatures              = initVulkanStructure();
    VkPhysicalDeviceFeatures availableFeatures              = {};

    vki.getPhysicalDeviceFeatures(physicalDevice, &availableFeatures);
    requiredFeatures.features.dualSrcBlend     = availableFeatures.dualSrcBlend;
    requiredFeatures.features.independentBlend = availableFeatures.independentBlend;
    requiredFeatures.features.depthBiasClamp   = availableFeatures.depthBiasClamp;

    chainInsert(requiredFeatures);
    chainInsert(sync2f);

    if (isConstructionTypeLibrary(m_params.pipelineConstructionType))
    {
        m_usedExtensions.push_back(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME);
        m_usedExtensions.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
        chainInsert(gplf);
    }
    if (isConstructionTypeShaderObject(m_params.pipelineConstructionType))
    {
        m_usedExtensions.push_back(VK_EXT_SHADER_OBJECT_EXTENSION_NAME);
        chainInsert(sof);
        m_usedExtensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        chainInsert(drf);
        m_usedExtensions.push_back(VK_EXT_COLOR_WRITE_ENABLE_EXTENSION_NAME);
        chainInsert(cwef);
    }

    std::vector<const char *> extensionPtrs(m_usedExtensions.size());
    std::transform(m_usedExtensions.begin(), m_usedExtensions.end(), extensionPtrs.begin(),
                   std::mem_fn(&std::string::c_str));

    const VkDeviceCreateInfo deviceParams = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, // VkStructureType sType;
        pNext,                                // const void* pNext;
        0u,                                   // VkDeviceCreateFlags flags;
        1u,                                   // uint32_t queueCreateInfoCount;
        &queueParams,                         // const VkDeviceQueueCreateInfo* pQueueCreateInfos;
        0u,                                   // uint32_t enabledLayerCount;
        nullptr,                              // const char* const* ppEnabledLayerNames;
        de::sizeU32(extensionPtrs),           // uint32_t enabledExtensionCount;
        de::dataOrNull(extensionPtrs),        // const char* const* ppEnabledExtensionNames;
        nullptr                               // const VkPhysicalDeviceFeatures* pEnabledFeatures;
    };

    return createCustomDevice(ctx.getTestContext().getCommandLine().isValidationEnabled(), ctx.getPlatformInterface(),
                              instance, vki, physicalDevice, &deviceParams);
}

} // unnamed namespace

void addDualBlendMultiAttachmentTests(tcu::TestContext &testCtx, tcu::TestCaseGroup *const dualSourceGroup,
                                      PipelineConstructionType pipelineConstructionType)
{
    de::MovePtr<tcu::TestCaseGroup> multiAttachmentGroup(new tcu::TestCaseGroup(testCtx, "multi_attachments"));

    for (const VkFormat format : blending_common::getBlendFormats())
    {
        DualSourceBlendMAParams p;

        p.format                   = format;
        p.pipelineConstructionType = pipelineConstructionType;

        multiAttachmentGroup->addChild(new DualSourceBlendMACase(testCtx, getFormatCaseName(format), p));
    }

    dualSourceGroup->addChild(multiAttachmentGroup.release());
}

} // namespace pipeline
} // namespace vkt
