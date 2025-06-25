/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
 * Copyright (c) 2024 Valve Corporation.
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
 * \brief Device Generated Commands EXT Graphics Misc Tests
 *//*--------------------------------------------------------------------*/

#include "vktDGCGraphicsMiscTestsExt.hpp"
#include "util/vktShaderObjectUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkRayTracingUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vktDGCUtilExt.hpp"
#include "vkTypeUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vktDGCUtilCommon.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include "deRandom.hpp"
#include "vktTestCaseUtil.hpp"

#include <vector>
#include <memory>
#include <sstream>
#include <string>
#include <map>

namespace vkt
{
namespace DGC
{

namespace
{

using namespace vk;
using DGCShaderExtPtr            = std::unique_ptr<DGCShaderExt>;
using ShaderWrapperPtr           = std::unique_ptr<ShaderWrapper>;
using GraphicsPipelineWrapperPtr = std::unique_ptr<GraphicsPipelineWrapper>;

constexpr uint32_t kBindingCount = 4;

enum class BindingType
{
    POSITION = 0,
    RED_COLOR,
    GREEN_COLOR,
    BLUE_COLOR,
    BINDING_COUNT,
};

std::vector<int> getBindingTypeIntValues(void)
{
    std::vector<int> intValues;
    intValues.reserve(static_cast<int>(BindingType::BINDING_COUNT));
    for (BindingType bindingType = BindingType::POSITION; bindingType != BindingType::BINDING_COUNT;
         bindingType             = static_cast<BindingType>(static_cast<int>(bindingType) + 1))
        intValues.push_back(static_cast<int>(bindingType));
    return intValues;
}

constexpr int kMinPaddingItems = 0;
constexpr int kMaxPaddingItems = 3;

uint32_t bool2Uint(bool b)
{
    return (b ? 1u : 0u);
}

class VBOUpdateInstance : public vkt::TestInstance
{
public:
    struct Params
    {
        bool varyBinding[kBindingCount];

        uint32_t getSeed(void) const
        {
            return (1234000u | (bool2Uint(varyBinding[static_cast<int>(BindingType::POSITION)]) << 3) |
                    (bool2Uint(varyBinding[static_cast<int>(BindingType::RED_COLOR)]) << 2) |
                    (bool2Uint(varyBinding[static_cast<int>(BindingType::GREEN_COLOR)]) << 1) |
                    (bool2Uint(varyBinding[static_cast<int>(BindingType::BLUE_COLOR)]) << 0));
        }

        std::string getVariationString(void) const
        {
            const auto bindingIndices = getBindingTypeIntValues();
            std::string variationString;
            for (const auto idx : bindingIndices)
                variationString += std::to_string(static_cast<int>(varyBinding[idx]));
            return variationString;
        }
    };

    VBOUpdateInstance(Context &context, const Params &params) : vkt::TestInstance(context), m_params(params)
    {
        DE_STATIC_ASSERT(kBindingCount == static_cast<uint32_t>(BindingType::BINDING_COUNT));
    }

    virtual ~VBOUpdateInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void);

protected:
    const Params m_params;
};

class VBOUpdateCase : public vkt::TestCase
{
public:
    VBOUpdateCase(tcu::TestContext &testCtx, const std::string &name, const VBOUpdateInstance::Params &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~VBOUpdateCase(void)
    {
    }

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

protected:
    const VBOUpdateInstance::Params m_params;
};

void VBOUpdateCase::checkSupport(Context &context) const
{
    const auto shaderStages = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    checkDGCExtSupport(context, shaderStages);

    // For the dynamic stride.
    context.requireDeviceFunctionality("VK_EXT_extended_dynamic_state");
};

void VBOUpdateCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "\n"
         << "layout(location=0) in vec4 inPos;\n"
         << "layout(location=1) in float inRed;\n"
         << "layout(location=2) in float inGreen;\n"
         << "layout(location=3) in float inBlue;\n"
         << "\n"
         << "layout(location=0) out float outRed;\n"
         << "layout(location=1) out float outGreen;\n"
         << "layout(location=2) out float outBlue;\n"
         << "\n"
         << "void main(void) {\n"
         << "    gl_Position = inPos;\n"
         << "    outRed = inRed;\n"
         << "    outGreen = inGreen;\n"
         << "    outBlue = inBlue;\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "\n"
         << "layout(location=0) in float inRed;\n"
         << "layout(location=1) in float inGreen;\n"
         << "layout(location=2) in float inBlue;\n"
         << "\n"
         << "layout(location=0) out vec4 outColor;\n"
         << "\n"
         << "void main(void) {\n"
         << "    outColor = vec4(inRed, inGreen, inBlue, 1.0);\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

TestInstance *VBOUpdateCase::createInstance(Context &context) const
{
    return new VBOUpdateInstance(context, m_params);
}

tcu::TestStatus VBOUpdateInstance::iterate(void)
{
    const auto ctx = m_context.getContextCommonData();
    const tcu::IVec3 fbExtent(2, 2, 1);
    const auto vkExtent      = makeExtent3D(fbExtent);
    const auto pixelCountU   = vkExtent.width * vkExtent.height * vkExtent.depth;
    const auto bufferUsage   = (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    const auto bufferMemReqs = (MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress);
    const auto shaderStages  = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 0.0f);

    // General triangle strip. Offsets will be used with it so it covers the 4 framebuffer pixels.
    const std::vector<tcu::Vec4> vertices{
        tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
        tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
    };

    // Offsets to move the strip around and cover different pixels.
    const std::vector<tcu::Vec4> stripOffsets{
        tcu::Vec4(-1.0f, -1.0f, 0.0f, 0.0f),
        tcu::Vec4(0.0f, -1.0f, 0.0f, 0.0f),
        tcu::Vec4(-1.0f, 0.0f, 0.0f, 0.0f),
        tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
    };
    DE_ASSERT(de::sizeU32(stripOffsets) == pixelCountU);

    // Color values for each vertex in each sequence (pixel).
    const std::vector<std::vector<float>> colorValues{
        // For each sequence.
        std::vector<float>{0.00f, 0.00f, 0.00f, 0.00f}, // For each vertex.
        std::vector<float>{0.50f, 0.50f, 0.50f, 0.50f},
        std::vector<float>{0.75f, 0.75f, 0.75f, 0.75f},
        std::vector<float>{1.00f, 1.00f, 1.00f, 1.00f},
    };
    DE_ASSERT(de::sizeU32(colorValues) == pixelCountU);
    for (const auto &vtxColor : colorValues)
    {
        DE_ASSERT(vtxColor.size() == vertices.size());
        DE_UNREF(vtxColor); // For release builds.
    }

    using BufferWithMemoryPtr = std::unique_ptr<BufferWithMemory>;
    struct BufferInfo
    {
        BufferWithMemoryPtr bufferPtr;
        uint32_t size;
        uint32_t stride;

        BufferInfo(BufferWithMemory *ptr, uint32_t size_, uint32_t stride_)
            : bufferPtr(ptr)
            , size(size_)
            , stride(stride_)
        {
        }
    };
    std::vector<BufferInfo> positionBuffers;
    std::vector<BufferInfo> redColorBuffers;
    std::vector<BufferInfo> greenColorBuffers;
    std::vector<BufferInfo> blueColorBuffers;

    de::Random rnd(m_params.getSeed());

    {
        positionBuffers.reserve(pixelCountU);
        for (uint32_t i = 0u; i < pixelCountU; ++i)
        {
            // Create the actual vertices for the strip in each pixel by applying an offset to each position.
            std::vector<tcu::Vec4> actualVertices(vertices);
            const auto &posOffset = stripOffsets.at(i);
            for (uint32_t j = 0u; j < de::sizeU32(vertices); ++j)
                actualVertices.at(j) += posOffset;

            const auto paddingItems = rnd.getInt(kMinPaddingItems, kMaxPaddingItems); // Per vertex.
            const auto totalItems   = paddingItems + 1;                               // Per vertex.
            const auto bufferSize   = static_cast<VkDeviceSize>(totalItems * de::dataSize(vertices));
            const uint32_t stride   = static_cast<uint32_t>(bufferSize / vertices.size());
            const auto createInfo   = makeBufferCreateInfo(bufferSize, bufferUsage);

            // Create and zero-out buffer.
            positionBuffers.emplace_back(
                new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, createInfo, bufferMemReqs),
                static_cast<uint32_t>(bufferSize), stride);
            char *dataPtr = reinterpret_cast<char *>(positionBuffers.back().bufferPtr->getAllocation().getHostPtr());
            deMemset(dataPtr, 0, positionBuffers.back().size);

            // Copy position values respecting the stride.
            for (size_t j = 0u; j < actualVertices.size(); ++j)
            {
                auto itemPtr = dataPtr + stride * j;
                deMemcpy(itemPtr, &actualVertices.at(j), sizeof(decltype(actualVertices)::value_type));
            }
        }
    }

    const std::vector<std::vector<BufferInfo> *> colorBufferVectors{
        &redColorBuffers,
        &greenColorBuffers,
        &blueColorBuffers,
    };
    for (auto &colorBufferVecPtr : colorBufferVectors)
    {
        auto &colorBufferVec = *colorBufferVecPtr;

        colorBufferVec.reserve(pixelCountU);
        for (uint32_t i = 0u; i < pixelCountU; ++i)
        {
            const auto vtxValues    = colorValues.at(i);
            const auto paddingItems = rnd.getInt(kMinPaddingItems, kMaxPaddingItems); // Per vertex.
            const auto totalItems   = paddingItems + 1;                               // Per vertex.
            const auto bufferSize   = static_cast<VkDeviceSize>(totalItems * de::dataSize(vtxValues));
            const uint32_t stride   = static_cast<uint32_t>(bufferSize / vtxValues.size());
            const auto createInfo   = makeBufferCreateInfo(bufferSize, bufferUsage);

            colorBufferVec.emplace_back(
                new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, createInfo, bufferMemReqs),
                static_cast<uint32_t>(bufferSize), stride);
            char *dataPtr = reinterpret_cast<char *>(colorBufferVec.back().bufferPtr->getAllocation().getHostPtr());
            deMemset(dataPtr, 0, colorBufferVec.back().size);

            // Copy color values respecting the stride.
            for (size_t j = 0u; j < vtxValues.size(); ++j)
            {
                auto itemPtr = dataPtr + stride * j;
                deMemcpy(itemPtr, &vtxValues.at(j), sizeof(decltype(vtxValues)::value_type));
            }
        }
    }

    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device);

    // Commands layout.
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, shaderStages, *pipelineLayout);
    const auto intBindingTypes = getBindingTypeIntValues();
    for (const auto idx : intBindingTypes)
    {
        if (m_params.varyBinding[idx])
            cmdsLayoutBuilder.addVertexBufferToken(cmdsLayoutBuilder.getStreamRange(), idx);
    }
    cmdsLayoutBuilder.addDrawToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // DGC data.
    std::vector<uint32_t> dgcData;
    dgcData.reserve(cmdsLayoutBuilder.getStreamStride() * pixelCountU);
    const std::vector<std::vector<BufferInfo> *> allBufferVectors{
        &positionBuffers,
        &redColorBuffers,
        &greenColorBuffers,
        &blueColorBuffers,
    };
    for (uint32_t i = 0u; i < pixelCountU; ++i)
    {
        for (const auto idx : intBindingTypes)
        {
            if (m_params.varyBinding[idx])
            {
                const auto &buffer       = allBufferVectors.at(idx)->at(i);
                const auto deviceAddress = getBufferDeviceAddress(ctx.vkd, ctx.device, buffer.bufferPtr->get());
                const VkBindVertexBufferIndirectCommandEXT cmd{deviceAddress, buffer.size, buffer.stride};
                pushBackElement(dgcData, cmd);
            }
        }

        const VkDrawIndirectCommand drawCmd{de::sizeU32(vertices), 1u, 0u, 0u};
        pushBackElement(dgcData, drawCmd);
    }

    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, static_cast<VkDeviceSize>(de::dataSize(dgcData)));
    deMemcpy(dgcBuffer.getAllocation().getHostPtr(), de::dataOrNull(dgcData), de::dataSize(dgcData));

    // Framebuffer.
    const auto colorUsage =
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto colorSRR    = makeDefaultImageSubresourceRange();
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, colorFormat, colorUsage, VK_IMAGE_TYPE_2D,
                                colorSRR);

    const auto renderPass = makeRenderPass(ctx.vkd, ctx.device, colorFormat);
    const auto framebuffer =
        makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer.getImageView(), vkExtent.width, vkExtent.height);

    // Create pipeline.
    const std::vector<VkVertexInputBindingDescription> bindingDesc{
        // Note strides will be dynamic.
        makeVertexInputBindingDescription(0u, DE_SIZEOF32(tcu::Vec4), VK_VERTEX_INPUT_RATE_VERTEX),
        makeVertexInputBindingDescription(1u, DE_SIZEOF32(float), VK_VERTEX_INPUT_RATE_VERTEX),
        makeVertexInputBindingDescription(2u, DE_SIZEOF32(float), VK_VERTEX_INPUT_RATE_VERTEX),
        makeVertexInputBindingDescription(3u, DE_SIZEOF32(float), VK_VERTEX_INPUT_RATE_VERTEX),
    };
    const std::vector<VkVertexInputAttributeDescription> attribDesc{
        makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u),
        makeVertexInputAttributeDescription(1u, 1u, VK_FORMAT_R32_SFLOAT, 0u),
        makeVertexInputAttributeDescription(2u, 2u, VK_FORMAT_R32_SFLOAT, 0u),
        makeVertexInputAttributeDescription(3u, 3u, VK_FORMAT_R32_SFLOAT, 0u),
    };
    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, //  VkStructureType                              sType;
        nullptr,                     //  const void*                                  pNext;
        0u,                          //  VkPipelineVertexInputStateCreateFlags        flags;
        de::sizeU32(bindingDesc),    //  uint32_t                                 vertexBindingDescriptionCount;
        de::dataOrNull(bindingDesc), //  const VkVertexInputBindingDescription*     pVertexBindingDescriptions;
        de::sizeU32(attribDesc),     //  uint32_t                                   vertexAttributeDescriptionCount;
        de::dataOrNull(attribDesc),  //  const VkVertexInputAttributeDescription*   pVertexAttributeDescriptions;
    };

    const std::vector<VkDynamicState> dynamicStates{VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE};

    const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, //   VkStructureType                      sType;
        nullptr,                                              //   const void*                          pNext;
        0u,                                                   //   VkPipelineDynamicStateCreateFlags    flags;
        de::sizeU32(dynamicStates),                           //   uint32_t                         dynamicStateCount;
        de::dataOrNull(dynamicStates),                        //   const VkDynamicState*                pDynamicStates;
    };

    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    const auto &binaries  = m_context.getBinaryCollection();
    const auto vertModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));
    const auto pipeline   = makeGraphicsPipeline(
        ctx.vkd, ctx.device, *pipelineLayout, *vertModule, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, *fragModule,
        *renderPass, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u, 0u, &vertexInputStateCreateInfo,
        nullptr, nullptr, nullptr, nullptr, &dynamicStateCreateInfo);

    // Preprocess buffer.
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, VK_NULL_HANDLE, *cmdsLayout, pixelCountU,
                                         0u, *pipeline);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), clearColor);
    for (const int idx : intBindingTypes)
    {
        if (!m_params.varyBinding[idx])
        {
            const auto &vertexBuffers = *allBufferVectors.at(idx);
            const auto firstBinding   = static_cast<uint32_t>(idx);
            const auto &vtxBuffer     = vertexBuffers.at(0u); // This input doesn't change: we bind the first buffer.
            const auto buffer         = vtxBuffer.bufferPtr->get();

            const VkDeviceSize offset = 0ull;
            const VkDeviceSize size   = vtxBuffer.size;
            const VkDeviceSize stride = vtxBuffer.stride;

            ctx.vkd.cmdBindVertexBuffers2(cmdBuffer, firstBinding, 1u, &buffer, &offset, &size, &stride);
        }
    }
    ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
    {
        const DGCGenCmdsInfo cmdsInfo(shaderStages, VK_NULL_HANDLE, *cmdsLayout, dgcBuffer.getDeviceAddress(),
                                      dgcBuffer.getSize(), preprocessBuffer.getDeviceAddress(),
                                      preprocessBuffer.getSize(), pixelCountU, 0u, 0ull, *pipeline);
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());
    }

    endRenderPass(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1));
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());

    const auto tcuFormat = mapVkFormat(colorFormat);
    tcu::ConstPixelBufferAccess result(tcuFormat, fbExtent, colorBuffer.getBufferAllocation().getHostPtr());

    tcu::TextureLevel referenceLevel(tcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    auto reference = referenceLevel.getAccess();
    tcu::clear(reference, clearColor);

    for (int y = 0; y < fbExtent.y(); ++y)
        for (int x = 0; x < fbExtent.x(); ++x)
        {
            const auto pixelIdx     = y * fbExtent.x() + x;
            const bool varyPosition = m_params.varyBinding[static_cast<int>(BindingType::POSITION)];

            if (pixelIdx > 0 && !varyPosition)
            {
                // We will not draw over this pixel if the triangle strip doesn't move.
                continue;
            }

            // If a component doesn't vary, we always get the first color value for it.
            // If it varies and the triangle strip moves, each pixel gets its own color according to the pixel index.
            // If it varies and the triangle strip doesn't move, the colored pixel gets the last color value used.
            const auto varyingColorIdx = (varyPosition ? pixelIdx : pixelCountU - 1u);

            const bool varyRed   = m_params.varyBinding[static_cast<int>(BindingType::RED_COLOR)];
            const bool varyGreen = m_params.varyBinding[static_cast<int>(BindingType::GREEN_COLOR)];
            const bool varyBlue  = m_params.varyBinding[static_cast<int>(BindingType::BLUE_COLOR)];

            const float red   = (varyRed ? colorValues.at(varyingColorIdx).at(0) : colorValues.at(0).at(0));
            const float green = (varyGreen ? colorValues.at(varyingColorIdx).at(0) : colorValues.at(0).at(0));
            const float blue  = (varyBlue ? colorValues.at(varyingColorIdx).at(0) : colorValues.at(0).at(0));

            reference.setPixel(tcu::Vec4(red, green, blue, 1.0f), x, y);
        }

    const float colorThreshold = 0.005f; // 1/255 < 0.005 < 2/255
    const tcu::Vec4 threshold(colorThreshold, colorThreshold, colorThreshold, 0.0f);

    auto &log = m_context.getTestContext().getLog();
    if (!tcu::floatThresholdCompare(log, "Result", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Unexpected result in color buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

class NormalDGCMixInstance : public vkt::TestInstance
{
public:
    struct Params
    {
        bool preProcess;
        bool useExecutionSet;
        bool useVBOToken;
        bool mesh;
        bool shaderObjects;

        VkShaderStageFlags getShaderStages(void) const
        {
            VkShaderStageFlags stages = VK_SHADER_STAGE_FRAGMENT_BIT;
            if (mesh)
                stages |= VK_SHADER_STAGE_MESH_BIT_EXT;
            else
                stages |= VK_SHADER_STAGE_VERTEX_BIT;
            return stages;
        }
    };

    NormalDGCMixInstance(Context &context, const Params &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~NormalDGCMixInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;

protected:
    const Params m_params;
};

class NormalDGCMixCase : public vkt::TestCase
{
public:
    NormalDGCMixCase(tcu::TestContext &testCtx, const std::string &name, const NormalDGCMixInstance::Params &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~NormalDGCMixCase(void)
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new NormalDGCMixInstance(context, m_params);
    }

    static tcu::Vec4 getGeomColor(void)
    {
        return tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);
    }

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;

protected:
    const NormalDGCMixInstance::Params m_params;
};

void NormalDGCMixCase::checkSupport(Context &context) const
{
    const auto stages                 = m_params.getShaderStages();
    const auto bindStages             = (m_params.useExecutionSet ? stages : 0u);
    const auto bindStagesPipeline     = (m_params.shaderObjects ? 0u : bindStages);
    const auto bindStagesShaderObject = (m_params.shaderObjects ? bindStages : 0u);

    checkDGCExtSupport(context, stages, bindStagesPipeline, bindStagesShaderObject);

    if (m_params.shaderObjects)
        context.requireDeviceFunctionality("VK_EXT_shader_object");

    if (m_params.mesh)
        context.requireDeviceFunctionality("VK_EXT_mesh_shader");
}

void NormalDGCMixCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main (void) {\n"
         << "    outColor = vec4" << getGeomColor() << ";\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());

    if (m_params.mesh)
    {
        const vk::ShaderBuildOptions meshBuildOpt(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

        std::string commonDecl;
        {
            std::ostringstream decl;
            decl
                // Binding 0: normal draws, per-pixel points.
                // Binding 1: DGC draws, triangle stripes.
                << "layout (set=0, binding=0, std430) readonly buffer VertBuffer0 {\n"
                << "    vec4 position[];\n"
                << "} vb0;\n"
                << "layout (set=0, binding=1, std430) readonly buffer VertBuffer1 {\n"
                << "    vec4 position[];\n"
                << "} vb1;\n"
                << "layout (push_constant, std430) uniform PushConstantBlock {\n"
                << "    uint firstVertex;\n"
                << "} pc;\n";
            commonDecl = decl.str();
        }

        // For points in normal draws, launch 1 WG per point.
        std::ostringstream meshPoints;
        meshPoints << "#version 460\n"
                   << "#extension GL_EXT_mesh_shader : enable\n"
                   << "layout (local_size_x=1) in;\n"
                   << "layout (points) out;\n"
                   << "layout (max_vertices=1, max_primitives=1) out;\n"
                   << commonDecl << "uint getWorkGroupIndex (void) {\n"
                   << "    const uint workGroupIndex = gl_NumWorkGroups.x * gl_NumWorkGroups.y * gl_WorkGroupID.z +\n"
                   << "                                gl_NumWorkGroups.x * gl_WorkGroupID.y +\n"
                   << "                                gl_WorkGroupID.x;\n"
                   << "    return workGroupIndex;\n"
                   << "}\n"
                   << "void main(void) {\n"
                   << "    const uint wgIndex = getWorkGroupIndex();\n"
                   << "    const uint vertIdx = wgIndex + pc.firstVertex;\n"
                   << "    SetMeshOutputsEXT(1, 1);\n"
                   << "    gl_MeshVerticesEXT[0].gl_Position = vb0.position[vertIdx];\n"
                   << "    gl_MeshVerticesEXT[0].gl_PointSize = 1.0;\n"
                   << "    gl_PrimitivePointIndicesEXT[0] = 0;\n"
                   << "}\n";
        programCollection.glslSources.add("mesh-points") << glu::MeshSource(meshPoints.str()) << meshBuildOpt;

        // For mesh DGC draws using the "triangle strip", launch 1 WG per quadrant and it will emit the strip.
        std::ostringstream meshStrip;
        meshStrip << "#version 460\n"
                  << "#extension GL_EXT_mesh_shader : enable\n"
                  << "layout (local_size_x=4) in;\n"
                  << "layout (triangles) out;\n"
                  << "layout (max_vertices=4, max_primitives=2) out;\n"
                  << commonDecl << "void main(void) {\n"
                  << "    SetMeshOutputsEXT(4, 2);\n"
                  << "    gl_MeshVerticesEXT[gl_LocalInvocationIndex].gl_Position = vb1.position[pc.firstVertex + "
                     "gl_LocalInvocationIndex];\n"
                  << "    gl_MeshVerticesEXT[gl_LocalInvocationIndex].gl_PointSize = 1.0;\n"
                  << "    if (gl_LocalInvocationIndex == 0u) {\n"
                  << "        gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);\n"
                  << "        gl_PrimitiveTriangleIndicesEXT[1] = uvec3(2, 1, 3);\n"
                  << "    }\n"
                  << "}\n";
        programCollection.glslSources.add("mesh-strip") << glu::MeshSource(meshStrip.str()) << meshBuildOpt;
    }
    else
    {
        std::ostringstream vert;
        vert << "#version 460\n"
             << "layout (location=0) in vec4 inPos;\n"
             << "void main (void) {\n"
             << "    gl_Position = inPos;\n"
             << "    gl_PointSize = 1.0;\n"
             << "}\n";
        programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
    }
}

tcu::TestStatus NormalDGCMixInstance::iterate(void)
{
    const auto ctx         = m_context.getContextCommonData();
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto depthFormat = VK_FORMAT_D16_UNORM;
    const VkImageUsageFlags colorUsage =
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const VkImageUsageFlags depthUsage = (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    const tcu::IVec3 fbExtent(16, 16, 1);
    const auto apiExtent          = makeExtent3D(fbExtent);
    const auto heightAreas        = 2u;
    const auto widthAreas         = 2u;
    const auto totalAreas         = heightAreas * widthAreas;
    const auto dgcPerAreaVertices = 4u; // One for each corner.
    const float normalDepth       = 0.0f;
    const float dgcDepth          = 1.0f;
    const auto vertexSize         = DE_SIZEOF32(tcu::Vec4);
    const auto shaderStages       = m_params.getShaderStages();

    const auto colorSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto depthSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u);

    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, apiExtent, colorFormat, colorUsage,
                                VK_IMAGE_TYPE_2D, colorSRR);
    ImageWithBuffer depthBuffer(ctx.vkd, ctx.device, ctx.allocator, apiExtent, depthFormat, depthUsage,
                                VK_IMAGE_TYPE_2D, depthSRR);

    // Normal draws will draw one point per pixel, while DGC draws will use triangle strips covering each quadrant.
    // We'll cover 4 quadrants from top to bottom and, in each row, from left to right alternating normal draw and DGC.
    std::vector<tcu::Vec4> normalVertices;
    normalVertices.reserve(apiExtent.width * apiExtent.height);

    // Note we must store vertices quad by quad, not exactly row by row.
    const std::vector<tcu::Vec4> areaOffsets{
        tcu::Vec4(-1.0f, -1.0f, 0.0f, 0.0f),
        tcu::Vec4(0.0f, -1.0f, 0.0f, 0.0f),
        tcu::Vec4(-1.0f, 0.0f, 0.0f, 0.0f),
        tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
    };
    const tcu::IVec3 areaExtent(fbExtent.x() / static_cast<int>(widthAreas),
                                fbExtent.y() / static_cast<int>(heightAreas), 1);
    const auto floatExtent = areaExtent.asFloat();
    for (const auto &aOffset : areaOffsets)
    {
        for (int y = 0; y < areaExtent.y(); ++y)
            for (int x = 0; x < areaExtent.x(); ++x)
            {
                const auto xCenter = (static_cast<float>(x) + 0.5f) / floatExtent.x() + aOffset.x();
                const auto yCenter = (static_cast<float>(y) + 0.5f) / floatExtent.y() + aOffset.y();
                normalVertices.push_back(tcu::Vec4(xCenter, yCenter, normalDepth, 1.0f));
            }
    }

    const auto vertexBufferUsage =
        (m_params.mesh ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    const auto normalVertexBufferSize = static_cast<VkDeviceSize>(de::dataSize(normalVertices));
    const auto normalVertexBufferInfo = makeBufferCreateInfo(normalVertexBufferSize, vertexBufferUsage);
    BufferWithMemory normalVertexBuffer(ctx.vkd, ctx.device, ctx.allocator, normalVertexBufferInfo,
                                        MemoryRequirement::HostVisible);
    auto &normalVertexBufferAlloc = normalVertexBuffer.getAllocation();
    void *normarVertexBufferPtr   = normalVertexBufferAlloc.getHostPtr();
    deMemcpy(normarVertexBufferPtr, de::dataOrNull(normalVertices), de::dataSize(normalVertices));

    std::vector<tcu::Vec4> dgcVertices;
    dgcVertices.reserve(totalAreas * dgcPerAreaVertices);

    // Note: clockwise in each quadrant.
    {
        dgcVertices.push_back(tcu::Vec4(-1.0f, -1.0f, dgcDepth, 1.0f));
        dgcVertices.push_back(tcu::Vec4(0.0f, -1.0f, dgcDepth, 1.0f));
        dgcVertices.push_back(tcu::Vec4(-1.0f, 0.0f, dgcDepth, 1.0f));
        dgcVertices.push_back(tcu::Vec4(0.0f, 0.0f, dgcDepth, 1.0f));
    }
    {
        dgcVertices.push_back(tcu::Vec4(0.0f, -1.0f, dgcDepth, 1.0f));
        dgcVertices.push_back(tcu::Vec4(1.0f, -1.0f, dgcDepth, 1.0f));
        dgcVertices.push_back(tcu::Vec4(0.0f, 0.0f, dgcDepth, 1.0f));
        dgcVertices.push_back(tcu::Vec4(1.0f, 0.0f, dgcDepth, 1.0f));
    }
    {
        dgcVertices.push_back(tcu::Vec4(-1.0f, 0.0f, dgcDepth, 1.0f));
        dgcVertices.push_back(tcu::Vec4(0.0f, 0.0f, dgcDepth, 1.0f));
        dgcVertices.push_back(tcu::Vec4(-1.0f, 1.0f, dgcDepth, 1.0f));
        dgcVertices.push_back(tcu::Vec4(0.0f, 1.0f, dgcDepth, 1.0f));
    }
    {
        dgcVertices.push_back(tcu::Vec4(0.0f, 0.0f, dgcDepth, 1.0f));
        dgcVertices.push_back(tcu::Vec4(1.0f, 0.0f, dgcDepth, 1.0f));
        dgcVertices.push_back(tcu::Vec4(0.0f, 1.0f, dgcDepth, 1.0f));
        dgcVertices.push_back(tcu::Vec4(1.0f, 1.0f, dgcDepth, 1.0f));
    }

    // When not using VBO tokens, a normal buffer could be used but, to simplify things, DGC buffers can be used in all cases.
    const auto dgcVertexBufferSize = static_cast<VkDeviceSize>(de::dataSize(dgcVertices));
    DGCBuffer dgcVertexBuffer(ctx.vkd, ctx.device, ctx.allocator, dgcVertexBufferSize, vertexBufferUsage);
    auto &dgcVertexBufferAlloc = dgcVertexBuffer.getAllocation();
    void *dgcVertexBufferPtr   = dgcVertexBufferAlloc.getHostPtr();
    deMemcpy(dgcVertexBufferPtr, de::dataOrNull(dgcVertices), de::dataSize(dgcVertices));

    // The mesh case uses a storage buffer for these vertices.
    Move<VkDescriptorSetLayout> setLayout;
    std::vector<VkDescriptorSetLayout> setLayouts;
    Move<VkDescriptorPool> descriptorPool;
    Move<VkDescriptorSet> descriptorSet;

    if (m_params.mesh)
    {
        const auto descType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

        DescriptorSetLayoutBuilder layoutBuilder;
        layoutBuilder.addSingleBinding(descType, shaderStages);
        layoutBuilder.addSingleBinding(descType, shaderStages);
        setLayout = layoutBuilder.build(ctx.vkd, ctx.device);
        setLayouts.push_back(*setLayout);

        DescriptorPoolBuilder poolBuilder;
        poolBuilder.addType(descType, 2u); // Normal and DGC buffers.
        descriptorPool = poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

        descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

        using Location = DescriptorSetUpdateBuilder::Location;
        DescriptorSetUpdateBuilder setUpdateBuilder;
        const auto normalVertexBufferDescInfo = makeDescriptorBufferInfo(normalVertexBuffer.get(), 0ull, VK_WHOLE_SIZE);
        const auto dgcVertexBufferDescInfo    = makeDescriptorBufferInfo(dgcVertexBuffer.get(), 0ull, VK_WHOLE_SIZE);
        setUpdateBuilder.writeSingle(*descriptorSet, Location::binding(0u), descType, &normalVertexBufferDescInfo);
        setUpdateBuilder.writeSingle(*descriptorSet, Location::binding(1u), descType, &dgcVertexBufferDescInfo);
        setUpdateBuilder.update(ctx.vkd, ctx.device);
    }

    const auto meshPCSize  = DE_SIZEOF32(uint32_t);
    const auto meshPCRange = makePushConstantRange(shaderStages, 0u, meshPCSize);

    std::vector<VkPushConstantRange> pcRanges;
    if (m_params.mesh)
        pcRanges.push_back(meshPCRange);

    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, de::dataOrNull(pcRanges));

    const auto renderPass = makeRenderPass(ctx.vkd, ctx.device, colorFormat, depthFormat);
    const std::vector<VkImageView> fbViews{colorBuffer.getImageView(), depthBuffer.getImageView()};
    const auto framebuffer = makeFramebuffer(ctx.vkd, ctx.device, *renderPass, de::sizeU32(fbViews),
                                             de::dataOrNull(fbViews), apiExtent.width, apiExtent.height);
    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    const auto &binaries = m_context.getBinaryCollection();
    const auto vertModule =
        (m_params.mesh ? Move<VkShaderModule>() : createShaderModule(ctx.vkd, ctx.device, binaries.get("vert")));
    const auto meshPointsModule =
        (m_params.mesh ? createShaderModule(ctx.vkd, ctx.device, binaries.get("mesh-points")) : Move<VkShaderModule>());
    const auto meshStripModule =
        (m_params.mesh ? createShaderModule(ctx.vkd, ctx.device, binaries.get("mesh-strip")) : Move<VkShaderModule>());
    const auto fragModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

    // To simplify we're going to create all of them as DGC shaders, no matter if they're used in an IES or not.
    DGCShaderExtPtr fragShader;
    DGCShaderExtPtr vertShader;
    DGCShaderExtPtr meshPointsShader;
    DGCShaderExtPtr meshStripShader;

    const auto &features   = m_context.getDeviceFeatures();
    const auto tessFeature = (features.tessellationShader == VK_TRUE);
    const auto geomFeature = (features.geometryShader == VK_TRUE);

    if (m_params.shaderObjects)
    {
        fragShader.reset(new DGCShaderExt(ctx.vkd, ctx.device, VK_SHADER_STAGE_FRAGMENT_BIT, 0u, binaries.get("frag"),
                                          setLayouts, pcRanges, tessFeature, geomFeature));
        if (m_params.mesh)
        {
            const auto createFlags = VK_SHADER_CREATE_NO_TASK_SHADER_BIT_EXT;
            meshPointsShader.reset(new DGCShaderExt(ctx.vkd, ctx.device, VK_SHADER_STAGE_MESH_BIT_EXT, createFlags,
                                                    binaries.get("mesh-points"), setLayouts, pcRanges, tessFeature,
                                                    geomFeature));
            meshStripShader.reset(new DGCShaderExt(ctx.vkd, ctx.device, VK_SHADER_STAGE_MESH_BIT_EXT, createFlags,
                                                   binaries.get("mesh-strip"), setLayouts, pcRanges, tessFeature,
                                                   geomFeature));
        }
        else
            vertShader.reset(new DGCShaderExt(ctx.vkd, ctx.device, VK_SHADER_STAGE_VERTEX_BIT, 0u, binaries.get("vert"),
                                              setLayouts, pcRanges, tessFeature, geomFeature));
    }

    const auto normalTopology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    const auto dgcTopology    = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    const auto normalCullMode = VK_CULL_MODE_BACK_BIT;
    const auto dgcCullMode    = VK_CULL_MODE_FRONT_BIT;

    const auto normalDepthTestEnabled = VK_TRUE;
    const auto normalDepthTestCompare = VK_COMPARE_OP_LESS;
    const auto dgcDepthTestEnabled    = VK_FALSE;
    const auto dgcDepthTestCompare    = VK_COMPARE_OP_NEVER;

    std::vector<Move<VkPipeline>> pipelines;
    pipelines.reserve(2u); // One normal, another one for DGC.

    // For non-mesh cases.
    std::vector<VkVertexInputBindingDescription> vertexBindings;
    std::vector<VkVertexInputAttributeDescription> vertexAttribs;
    if (!m_params.mesh)
    {
        vertexBindings.push_back(
            makeVertexInputBindingDescription(0u, DE_SIZEOF32(tcu::Vec4), VK_VERTEX_INPUT_RATE_VERTEX));
        vertexAttribs.push_back(makeVertexInputAttributeDescription(0u, 0u, vk::VK_FORMAT_R32G32B32A32_SFLOAT, 0u));
    }

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, //  VkStructureType                             sType;
        nullptr,                        //  const void*                                 pNext;
        0u,                             //  VkPipelineVertexInputStateCreateFlags       flags;
        de::sizeU32(vertexBindings),    //  uint32_t                                    vertexBindingDescriptionCount;
        de::dataOrNull(vertexBindings), //  const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
        de::sizeU32(vertexAttribs),     //  uint32_t                                    vertexAttributeDescriptionCount;
        de::dataOrNull(vertexAttribs),  //  const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
    };

    const VkPipelineVertexInputStateCreateInfo *vtxInfoPtr = (m_params.mesh ? nullptr : &vertexInputStateCreateInfo);

    const VkPipelineRasterizationStateCreateInfo normalRasterizationInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, //  VkStructureType                         sType;
        nullptr,                                                    //  const void*                             pNext;
        0u,                                                         //  VkPipelineRasterizationStateCreateFlags flags;
        VK_FALSE,                        //  VkBool32                                depthClampEnable;
        VK_FALSE,                        //  VkBool32                                rasterizerDiscardEnable;
        VK_POLYGON_MODE_FILL,            //  VkPolygonMode                           polygonMode;
        normalCullMode,                  //  VkCullModeFlags                         cullMode;
        VK_FRONT_FACE_COUNTER_CLOCKWISE, //  VkFrontFace                             frontFace;
        VK_FALSE,                        //  VkBool32                                depthBiasEnable;
        0.0f,                            //  float                                   depthBiasConstantFactor;
        0.0f,                            //  float                                   depthBiasClamp;
        0.0f,                            //  float                                   depthBiasSlopeFactor;
        1.0f,                            //  float                                   lineWidth;
    };
    auto dgcRasterizationInfo     = normalRasterizationInfo;
    dgcRasterizationInfo.cullMode = dgcCullMode;

    const auto stencilOp = makeStencilOpState(VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
                                              VK_COMPARE_OP_NEVER, 0xFFu, 0xFFu, 0u);

    const VkPipelineDepthStencilStateCreateInfo normalDepthStencilInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, //  VkStructureType                         sType;
        nullptr,                                                    //  const void*                             pNext;
        0u,                                                         //  VkPipelineDepthStencilStateCreateFlags  flags;
        normalDepthTestEnabled, //  VkBool32                                depthTestEnable;
        normalDepthTestEnabled, //  VkBool32                                depthWriteEnable;
        normalDepthTestCompare, //  VkCompareOp                             depthCompareOp;
        VK_FALSE,               //  VkBool32                                depthBoundsTestEnable;
        VK_FALSE,               //  VkBool32                                stencilTestEnable;
        stencilOp,              //  VkStencilOpState                        front;
        stencilOp,              //  VkStencilOpState                        back;
        0.0f,                   //  float                                   minDepthBounds;
        1.0f,                   //  float                                   maxDepthBounds;
    };
    auto dgcDepthStencilInfo             = normalDepthStencilInfo;
    dgcDepthStencilInfo.depthTestEnable  = dgcDepthTestEnabled;
    dgcDepthStencilInfo.depthWriteEnable = dgcDepthTestEnabled;
    dgcDepthStencilInfo.depthCompareOp   = dgcDepthTestCompare;

    if (!m_params.shaderObjects)
    {
        if (m_params.mesh)
        {
            pipelines.push_back(makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, VK_NULL_HANDLE,
                                                     *meshPointsModule, *fragModule, *renderPass, viewports, scissors,
                                                     0u, &normalRasterizationInfo, nullptr, &normalDepthStencilInfo,
                                                     nullptr, nullptr, 0u));
        }
        else
        {
            pipelines.push_back(makeGraphicsPipeline(
                ctx.vkd, ctx.device, *pipelineLayout, *vertModule, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                *fragModule, *renderPass, viewports, scissors, normalTopology, 0u, 0u, vtxInfoPtr,
                &normalRasterizationInfo, nullptr, &normalDepthStencilInfo, nullptr, nullptr, nullptr, 0u));
        }

        const VkPipelineCreateFlags2CreateInfoKHR flags2 = {
            VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR,
            nullptr,
            VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT,
        };

        const auto pNext = (m_params.useExecutionSet ? &flags2 : nullptr);

        std::vector<VkDynamicState> dynamicStates;
        if (m_params.useVBOToken)
            dynamicStates.push_back(VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE);

        const VkPipelineDynamicStateCreateInfo dynamicState = {
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, //  VkStructureType                     sType;
            nullptr,                                              //  const void*                         pNext;
            0u,                                                   //  VkPipelineDynamicStateCreateFlags   flags;
            de::sizeU32(dynamicStates),    //  uint32_t                            dynamicStateCount;
            de::dataOrNull(dynamicStates), //  const VkDynamicState*               pDynamicStates;
        };

        if (m_params.mesh)
        {
            pipelines.push_back(makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, VK_NULL_HANDLE,
                                                     *meshStripModule, *fragModule, *renderPass, viewports, scissors,
                                                     0u, &dgcRasterizationInfo, nullptr, &dgcDepthStencilInfo, nullptr,
                                                     &dynamicState, 0u, pNext));
        }
        else
        {
            pipelines.push_back(makeGraphicsPipeline(
                ctx.vkd, ctx.device, *pipelineLayout, *vertModule, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                *fragModule, *renderPass, viewports, scissors, dgcTopology, 0u, 0u, vtxInfoPtr, &dgcRasterizationInfo,
                nullptr, &dgcDepthStencilInfo, nullptr, &dynamicState, pNext, 0u));
        }
    }

    ExecutionSetManagerPtr executionSetManager;
    VkIndirectExecutionSetEXT ies = VK_NULL_HANDLE;

    if (m_params.useExecutionSet)
    {
        if (m_params.shaderObjects)
        {
            const uint32_t iesShaderCount = 2u; // vert or mesh + frag

            std::vector<IESStageInfo> stageInfos;
            stageInfos.reserve(iesShaderCount);

            stageInfos.emplace_back(fragShader->get(), setLayouts);
            if (m_params.mesh)
                stageInfos.emplace_back(meshStripShader->get(), setLayouts);
            else
                stageInfos.emplace_back(vertShader->get(), setLayouts);

            executionSetManager =
                makeExecutionSetManagerShader(ctx.vkd, ctx.device, stageInfos, pcRanges, iesShaderCount);
        }
        else
            executionSetManager = makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, pipelines.at(1u).get(), 1u);

        ies = executionSetManager->get();
    }

    // Commands layout.
    VkIndirectCommandsLayoutUsageFlagsEXT cmdsLayoutFlags = 0u;
    if (m_params.preProcess)
        cmdsLayoutFlags |= VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_EXT;
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(cmdsLayoutFlags, shaderStages, *pipelineLayout);
    if (m_params.useExecutionSet)
    {
        const auto iesInfoType = (m_params.shaderObjects ? VK_INDIRECT_EXECUTION_SET_INFO_TYPE_SHADER_OBJECTS_EXT :
                                                           VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT);
        cmdsLayoutBuilder.addExecutionSetToken(0u, iesInfoType, shaderStages);
    }
    if (m_params.useVBOToken)
    {
        DE_ASSERT(!m_params.mesh);
        cmdsLayoutBuilder.addVertexBufferToken(cmdsLayoutBuilder.getStreamRange(), 0u);
    }
    if (m_params.mesh)
    {
        cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(), meshPCRange);
        cmdsLayoutBuilder.addDrawMeshTasksToken(cmdsLayoutBuilder.getStreamRange());
    }
    else
        cmdsLayoutBuilder.addDrawToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);
    const auto dgcStride  = cmdsLayoutBuilder.getStreamStride();

    // DGC command buffer.
    const auto dgcDrawCount = totalAreas / 2u;

    std::vector<uint32_t> dgcData;
    dgcData.reserve((dgcDrawCount * dgcStride) / DE_SIZEOF32(uint32_t));
    const auto dgcVertexBufferAreaStride = dgcPerAreaVertices * vertexSize;
    const auto dgcVertexBufferAddress    = dgcVertexBuffer.getDeviceAddress();
    for (uint32_t i = 0u; i < dgcDrawCount; ++i)
    {
        const auto areaIndex = (2u * i + 1u);
        if (m_params.useExecutionSet)
        {
            if (m_params.shaderObjects)
            {
                // See above: we stored the fragment shader in the first position, followed by mesh/vert shaders.
                std::map<VkShaderStageFlagBits, uint32_t> shaderIndices;
                shaderIndices[VK_SHADER_STAGE_FRAGMENT_BIT] = 0u;
                const auto otherStage     = (m_params.mesh ? VK_SHADER_STAGE_MESH_BIT_EXT : VK_SHADER_STAGE_VERTEX_BIT);
                shaderIndices[otherStage] = 1u;

                // However, in the DGC data buffer we have to use stage bit order, as provided by the map.
                for (const auto &stageIndex : shaderIndices)
                    dgcData.push_back(stageIndex.second);
            }
            else
                dgcData.push_back(0u);
        }
        if (m_params.useVBOToken)
        {
            DE_ASSERT(!m_params.mesh);
            const VkBindVertexBufferIndirectCommandEXT bindCmd{
                dgcVertexBufferAddress + areaIndex * dgcVertexBufferAreaStride,
                dgcVertexBufferAreaStride,
                vertexSize,
            };
            pushBackElement(dgcData, bindCmd);
        }
        if (m_params.mesh)
        {
            const uint32_t firstVertex = dgcPerAreaVertices * areaIndex;
            dgcData.push_back(firstVertex);

            const VkDrawMeshTasksIndirectCommandEXT drawCmd{
                1u,
                1u,
                1u,
            };
            pushBackElement(dgcData, drawCmd);
        }
        else
        {
            const VkDrawIndirectCommand drawCmd{
                dgcPerAreaVertices,
                1u,
                0u, // We'll use vertex buffer offsets intead of firstVertex offsets.
                0u,
            };
            pushBackElement(dgcData, drawCmd);
        }
    }

    DGCBuffer cmdsBuffer(ctx.vkd, ctx.device, ctx.allocator, static_cast<VkDeviceSize>(de::dataSize(dgcData)));
    auto &cmdsBufferAlloc = cmdsBuffer.getAllocation();
    void *cmdsBufferPtr   = cmdsBufferAlloc.getHostPtr();
    deMemcpy(cmdsBufferPtr, de::dataOrNull(dgcData), de::dataSize(dgcData));

    using PreprocessBufferExtPtr = std::unique_ptr<PreprocessBufferExt>;

    std::vector<PreprocessBufferExtPtr> preprocessBuffers;
    preprocessBuffers.reserve(dgcDrawCount);

    const auto preprocessBufferPipeline =
        ((ies == VK_NULL_HANDLE && !m_params.shaderObjects) ? pipelines.at(1u).get() : VK_NULL_HANDLE);
    std::vector<VkShaderEXT> preprocessBufferShaders;
    if (m_params.shaderObjects)
    {
        preprocessBufferShaders.reserve(2u);
        preprocessBufferShaders.push_back(fragShader->get());

        if (m_params.mesh)
            preprocessBufferShaders.push_back(meshStripShader->get());
        else
            preprocessBufferShaders.push_back(vertShader->get());
    }
    const auto preprocessBufferShadersPtr = (preprocessBufferShaders.empty() ? nullptr : &preprocessBufferShaders);

    for (uint32_t i = 0u; i < dgcDrawCount; ++i)
        preprocessBuffers.emplace_back(new PreprocessBufferExt(ctx.vkd, ctx.device, ctx.allocator, ies, *cmdsLayout, 1u,
                                                               0u, preprocessBufferPipeline,
                                                               preprocessBufferShadersPtr));

    const CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;
    Move<VkCommandBuffer> preprocessCmdBuffer;

    using DGCGenCmdsInfoPtr = std::unique_ptr<DGCGenCmdsInfo>;
    std::vector<DGCGenCmdsInfoPtr> cmdInfos;

    const std::vector<VkClearValue> clearValues{
        makeClearValueColor(tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f)),
        makeClearValueDepthStencil(1.0f, 0u),
    };

    const auto areaExtentU                  = areaExtent.asUint();
    const auto areaPixels                   = areaExtentU.x() * areaExtentU.y() * areaExtentU.z();
    const auto normalVertexBufferAreaStride = static_cast<VkDeviceSize>(vertexSize * areaExtentU.x() * areaExtentU.y());
    const auto bindPoint                    = VK_PIPELINE_BIND_POINT_GRAPHICS;

    // Prepare DGC command infos.
    for (uint32_t i = 0u; i < dgcDrawCount; ++i)
    {
        const auto dgcAddress        = cmdsBuffer.getDeviceAddress() + (dgcStride * i);
        const auto &preprocessBuffer = *preprocessBuffers.at(i);
        cmdInfos.emplace_back(new DGCGenCmdsInfo(shaderStages, ies, *cmdsLayout, dgcAddress, dgcStride,
                                                 preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(), 1u,
                                                 0ull, 0u, preprocessBufferPipeline, preprocessBufferShadersPtr));
    }

    if (m_params.preProcess)
    {
        preprocessCmdBuffer = allocateCommandBuffer(ctx.vkd, ctx.device, *cmd.cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        beginCommandBuffer(ctx.vkd, *preprocessCmdBuffer);
    }

    beginCommandBuffer(ctx.vkd, cmdBuffer);

    // Descriptor sets: only used in mesh shader cases for the vertex buffers.
    if (m_params.mesh)
        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);

    if (m_params.shaderObjects)
    {
        // Transition image layouts.
        std::vector<VkImageMemoryBarrier> imgBarriers;
        imgBarriers.reserve(2u); // Color and depth.
        VkPipelineStageFlags pipelineStages = 0u;

        {
            const auto imageAccess = (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
            pipelineStages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            imgBarriers.push_back(makeImageMemoryBarrier(0u, imageAccess, VK_IMAGE_LAYOUT_UNDEFINED,
                                                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                         colorBuffer.getImage(), colorSRR));
        }
        {
            const auto imageAccess =
                (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
            pipelineStages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            pipelineStages |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            imgBarriers.push_back(makeImageMemoryBarrier(0u, imageAccess, VK_IMAGE_LAYOUT_UNDEFINED,
                                                         VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                                         depthBuffer.getImage(), depthSRR));
        }

        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pipelineStages,
                                      de::dataOrNull(imgBarriers), de::sizeU32(imgBarriers));

        beginRendering(ctx.vkd, cmdBuffer, colorBuffer.getImageView(), depthBuffer.getImageView(), false,
                       scissors.at(0u), clearValues.at(0u), clearValues.at(1u),
                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                       VK_ATTACHMENT_LOAD_OP_CLEAR);
    }
    else
    {
        beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), de::sizeU32(clearValues),
                        de::dataOrNull(clearValues));
    }

    const bool meshEnabled = m_context.isDeviceFunctionalitySupported("VK_EXT_mesh_shader");
    if (m_params.mesh)
        DE_ASSERT(meshEnabled); // This should have been checked in checkSupport already.

    for (uint32_t i = 0u; i < dgcDrawCount; ++i)
    {
        // First draw normally.
        {
            const auto areaIdx            = 2u * i;
            const auto vertexBuffer       = normalVertexBuffer.get();
            const auto vertexBufferOffset = normalVertexBufferAreaStride * areaIdx;

            if (!m_params.mesh)
                ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer, &vertexBufferOffset);

            if (m_params.shaderObjects)
            {
                vkt::shaderobjutil::bindShaderObjectState(
                    ctx.vkd, m_context.getDeviceExtensions(), cmdBuffer, viewports, scissors, normalTopology, 0u,
                    vtxInfoPtr, &normalRasterizationInfo, nullptr, &normalDepthStencilInfo, nullptr);

                std::map<VkShaderStageFlagBits, VkShaderEXT> boundShaders;
                if (meshEnabled)
                {
                    // When in a non-mesh test case but with mesh shading support enabled, we need to bind these two.
                    // Otherwise, we must not bind them.
                    boundShaders[VK_SHADER_STAGE_TASK_BIT_EXT] = VK_NULL_HANDLE;
                    boundShaders[VK_SHADER_STAGE_MESH_BIT_EXT] =
                        (m_params.mesh ? meshPointsShader->get() : VK_NULL_HANDLE);
                }
                boundShaders[VK_SHADER_STAGE_VERTEX_BIT] = (m_params.mesh ? VK_NULL_HANDLE : vertShader->get());
                boundShaders[VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT]    = VK_NULL_HANDLE;
                boundShaders[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT] = VK_NULL_HANDLE;
                boundShaders[VK_SHADER_STAGE_GEOMETRY_BIT]                = VK_NULL_HANDLE;
                boundShaders[VK_SHADER_STAGE_FRAGMENT_BIT]                = fragShader->get();

                for (const auto &stageShaderPair : boundShaders)
                    ctx.vkd.cmdBindShadersEXT(cmdBuffer, 1u, &stageShaderPair.first, &stageShaderPair.second);
            }
            else
                ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipelines.at(0u));
            if (m_params.mesh)
            {
                const uint32_t firstVertex = areaIdx * areaPixels;
                ctx.vkd.cmdPushConstants(cmdBuffer, *pipelineLayout, shaderStages, 0u, meshPCSize, &firstVertex);
                ctx.vkd.cmdDrawMeshTasksEXT(cmdBuffer, areaPixels, 1u, 1u);
            }
            else
                ctx.vkd.cmdDraw(cmdBuffer, areaPixels, 1u, 0u, 0u);
        }
        // Then draw with DGC.
        {
            // We need to bind the pipeline or shaders no matter if we use DGC execution sets or not.
            if (m_params.shaderObjects)
            {
                vkt::shaderobjutil::bindShaderObjectState(
                    ctx.vkd, m_context.getDeviceExtensions(), cmdBuffer, viewports, scissors, dgcTopology, 0u,
                    vtxInfoPtr, &dgcRasterizationInfo, nullptr, &dgcDepthStencilInfo, nullptr);

                std::map<VkShaderStageFlagBits, VkShaderEXT> boundShaders;
                if (meshEnabled)
                {
                    // When in a non-mesh test case but with mesh shading support enabled, we need to bind these two.
                    // Otherwise, we must not bind them.
                    boundShaders[VK_SHADER_STAGE_TASK_BIT_EXT] = VK_NULL_HANDLE;
                    boundShaders[VK_SHADER_STAGE_MESH_BIT_EXT] =
                        (m_params.mesh ? meshStripShader->get() : VK_NULL_HANDLE);
                }
                boundShaders[VK_SHADER_STAGE_VERTEX_BIT] = (m_params.mesh ? VK_NULL_HANDLE : vertShader->get());
                boundShaders[VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT]    = VK_NULL_HANDLE;
                boundShaders[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT] = VK_NULL_HANDLE;
                boundShaders[VK_SHADER_STAGE_GEOMETRY_BIT]                = VK_NULL_HANDLE;
                boundShaders[VK_SHADER_STAGE_FRAGMENT_BIT]                = fragShader->get();

                for (const auto &stageShaderPair : boundShaders)
                    ctx.vkd.cmdBindShadersEXT(cmdBuffer, 1u, &stageShaderPair.first, &stageShaderPair.second);
            }
            else
                ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipelines.at(1u));

            if (!m_params.useVBOToken && !m_params.mesh)
            {
                const auto vertexBuffer       = dgcVertexBuffer.get();
                const auto vertexBufferOffset = static_cast<VkDeviceSize>(dgcVertexBufferAreaStride * (2u * i + 1u));
                ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer, &vertexBufferOffset);
            }

            const auto &cmdInfo = cmdInfos.at(i)->get();

            if (m_params.preProcess)
            {
                ctx.vkd.cmdPreprocessGeneratedCommandsEXT(*preprocessCmdBuffer, &cmdInfo, cmdBuffer);
                preprocessToExecuteBarrierExt(ctx.vkd, *preprocessCmdBuffer);
            }
            {
                const auto isPreprocessed = makeVkBool(m_params.preProcess);
                ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, isPreprocessed, &cmdInfo);
            }
        }
    }

    if (m_params.shaderObjects)
        endRendering(ctx.vkd, cmdBuffer);
    else
        endRenderPass(ctx.vkd, cmdBuffer);

    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1));
    endCommandBuffer(ctx.vkd, cmdBuffer);

    if (m_params.preProcess)
    {
        preprocessToExecuteBarrierExt(ctx.vkd, *preprocessCmdBuffer);
        endCommandBuffer(ctx.vkd, *preprocessCmdBuffer);
    }

    submitAndWaitWithPreprocess(ctx.vkd, ctx.device, ctx.queue, cmdBuffer, *preprocessCmdBuffer);

    // Reference image.
    const auto tcuFormat = mapVkFormat(colorFormat);
    tcu::TextureLevel refLevel(tcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    const auto reference = refLevel.getAccess();

    const auto geomColor = NormalDGCMixCase::getGeomColor(); // Must match frag shader.
    tcu::clear(reference, geomColor);

    // Result image.
    auto &colorBufferAlloc = colorBuffer.getBufferAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, colorBufferAlloc);
    tcu::ConstPixelBufferAccess result(tcuFormat, fbExtent, colorBufferAlloc.getHostPtr());

    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f); // Exact results because we only use 1 and 0.
    auto &log = m_context.getTestContext().getLog();
    if (!tcu::floatThresholdCompare(log, "Result", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Unexpected results found in color buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

class NullVBOInstance : public vkt::TestInstance
{
public:
    struct Params
    {
        bool useShaderObjects;
        bool preprocess;

        uint32_t getRandomSeed() const
        {
            return 1721133137u + static_cast<uint32_t>(useShaderObjects);
        }
    };

    static VkShaderStageFlags getStages(void)
    {
        return (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    NullVBOInstance(Context &context, const Params &params) : vkt::TestInstance(context), m_params(params)
    {
    }

    virtual ~NullVBOInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const Params m_params;
};

class NullVBOCase : public vkt::TestCase
{
public:
    NullVBOCase(tcu::TestContext &testCtx, const std::string &name, const NullVBOInstance::Params &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual ~NullVBOCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

    static tcu::Vec4 getClearColor()
    {
        return tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    static tcu::Vec4 getGeometryColor()
    {
        return tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);
    }

protected:
    const NullVBOInstance::Params m_params;
};

void NullVBOCase::checkSupport(Context &context) const
{
    const auto stages = NullVBOInstance::getStages();
    checkDGCExtSupport(context, stages);

    if (m_params.useShaderObjects)
        context.requireDeviceFunctionality("VK_EXT_shader_object");

    // Robust buffer access support check.
    const auto ctx = context.getContextCommonData();
    VkPhysicalDeviceFeatures features;
    ctx.vki.getPhysicalDeviceFeatures(ctx.physicalDevice, &features);
    if (!features.robustBufferAccess)
        TCU_THROW(NotSupportedError, "robustBufferAccess not supported");
}

void NullVBOCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec2 inXY;\n"
         << "layout (location=1) in float inZ;\n"
         << "void main(void) {\n"
         << "    gl_Position = vec4(inXY.xy, inZ, 1.0);\n"
         << "    gl_PointSize = 1.0;\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main(void) {\n"
         << "    outColor = vec4" << getGeometryColor() << ";\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

TestInstance *NullVBOCase::createInstance(Context &context) const
{
    return new NullVBOInstance(context, m_params);
}

tcu::TestStatus NullVBOInstance::iterate(void)
{
    // Custom device with robust buffer access.
    const auto &cmdLine       = m_context.getTestContext().getCommandLine();
    const auto validation     = cmdLine.isValidationEnabled();
    const auto customInstance = createCustomInstanceWithExtension(m_context, "VK_KHR_get_physical_device_properties2");
    const auto &vki           = customInstance.getDriver();
    const auto &vkp           = m_context.getPlatformInterface();
    const auto physicalDevice = m_context.getPhysicalDevice();
    const bool esoSupport     = m_context.getShaderObjectFeaturesEXT().shaderObject;
    const auto qfIndex        = m_context.getUniversalQueueFamilyIndex();

    const float queuePriority = 1.0f;

    const VkDeviceQueueCreateInfo queueCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, //  VkStructureType             sType;
        nullptr,                                    //  const void*                 pNext;
        0u,                                         //  VkDeviceQueueCreateFlags    flags;
        qfIndex,                                    //  uint32_t                    queueFamilyIndex;
        1u,                                         //  uint32_t                    queueCount;
        &queuePriority,                             //  const float*                pQueuePriorities;
    };

    std::vector<std::string> deviceExtensions{
        "VK_EXT_device_generated_commands", "VK_KHR_buffer_device_address", "VK_KHR_maintenance5",
        //"VK_KHR_device_group",
        //"VK_KHR_device_group_creation",
        "VK_KHR_dynamic_rendering", "VK_KHR_depth_stencil_resolve", "VK_KHR_create_renderpass2", "VK_KHR_multiview",
        "VK_KHR_maintenance2",
        "VK_EXT_shader_object", // Last place, see below.
    };

    if (!esoSupport)
        deviceExtensions.pop_back();

    VkPhysicalDeviceFeatures2 features2                            = initVulkanStructure();
    VkPhysicalDeviceDeviceGeneratedCommandsFeaturesEXT dgcFeatures = initVulkanStructure();
    VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bdaFeatures     = initVulkanStructure();
    VkPhysicalDeviceMaintenance5FeaturesKHR maint5Features         = initVulkanStructure();
    VkPhysicalDeviceDynamicRenderingFeaturesKHR drFeatures         = initVulkanStructure();
    VkPhysicalDeviceMultiviewFeaturesKHR mvFeatures                = initVulkanStructure();
    VkPhysicalDeviceShaderObjectFeaturesEXT esoFeatures            = initVulkanStructure();

    const auto addFeatures = makeStructChainAdder(&features2);
    addFeatures(&dgcFeatures);
    addFeatures(&bdaFeatures);
    addFeatures(&maint5Features);
    addFeatures(&drFeatures);
    addFeatures(&mvFeatures);
    if (esoSupport)
        addFeatures(&esoFeatures);

    vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);
    // Note we will not disable any bit here, to make sure robust buffer access stays activated.

    std::vector<const char *> rawDeviceExtensions;
    rawDeviceExtensions.reserve(deviceExtensions.size());
    std::transform(begin(deviceExtensions), end(deviceExtensions), std::back_inserter(rawDeviceExtensions),
                   [](const std::string &s) { return s.c_str(); });

    const VkDeviceCreateInfo deviceCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, //  VkStructureType                 sType;
        &features2,                           //  const void*                     pNext;
        0u,                                   //  VkDeviceCreateFlags             flags;
        1u,                                   //  uint32_t                        queueCreateInfoCount;
        &queueCreateInfo,                     //  const VkDeviceQueueCreateInfo*  pQueueCreateInfos;
        0u,                                   //  uint32_t                        enabledLayerCount;
        nullptr,                              //  const char* const*              ppEnabledLayerNames;
        de::sizeU32(rawDeviceExtensions),     //  uint32_t                        enabledExtensionCount;
        de::dataOrNull(rawDeviceExtensions),  //  const char* const*              ppEnabledExtensionNames;
        nullptr,                              //  const VkPhysicalDeviceFeatures* pEnabledFeatures;
    };

    const auto customDevice =
        createCustomDevice(validation, vkp, customInstance, vki, physicalDevice, &deviceCreateInfo);
    const auto &device = customDevice.get();
    const DeviceDriver vkd(vkp, customInstance, device, m_context.getUsedApiVersion(), cmdLine);
    const auto queue = getDeviceQueue(vkd, device, qfIndex, 0u);

    const auto memoryProperties = getPhysicalDeviceMemoryProperties(vki, physicalDevice);
    SimpleAllocator allocator(vkd, device, memoryProperties);

    // Test using that device for some vertex bindings.
    const tcu::IVec3 fbExtent(16, 16, 1);
    const auto fbExtentU   = fbExtent.asUint();
    const auto pixelCount  = fbExtentU.x() * fbExtentU.y() * fbExtentU.z();
    const auto apiExtent   = makeExtent3D(fbExtent);
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage =
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto depthFormat      = VK_FORMAT_D16_UNORM;
    const auto depthUsage       = (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                             VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto colorSRR         = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto depthSRR         = makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u);
    const auto shaderStages     = getStages();
    const auto constructionType = (m_params.useShaderObjects ? PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV :
                                                               PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC);

    // We'll enable the depth test to make sure depth is zero as per the robust buffer read.
    ImageWithBuffer colorBuffer(vkd, device, allocator, apiExtent, colorFormat, colorUsage, VK_IMAGE_TYPE_2D, colorSRR);
    ImageWithBuffer depthBuffer(vkd, device, allocator, apiExtent, depthFormat, depthUsage, VK_IMAGE_TYPE_2D, depthSRR);

    // We'll have one vertex buffer and draw per row.
    std::vector<tcu::Vec2> vertices;
    vertices.reserve(pixelCount);
    const auto floatExtent = fbExtent.asFloat();

    for (int y = 0; y < fbExtent.y(); ++y)
        for (int x = 0; x < fbExtent.x(); ++x)
        {
            const auto xCenter = (static_cast<float>(x) + 0.5f) / floatExtent.x() * 2.0 - 1.0f;
            const auto yCenter = (static_cast<float>(y) + 0.5f) / floatExtent.y() * 2.0 - 1.0f;
            vertices.emplace_back(xCenter, yCenter);
        }

    using BufferWithMemoryPtr = std::unique_ptr<BufferWithMemory>;
    std::vector<BufferWithMemoryPtr> vertexBuffers;
    vertexBuffers.reserve(fbExtentU.y());

    const auto vertexBufferSize       = static_cast<VkDeviceSize>(sizeof(tcu::Vec2) * fbExtentU.x());
    const auto vertexBufferUsage      = (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    const auto vertexBufferCreateInfo = makeBufferCreateInfo(vertexBufferSize, vertexBufferUsage);
    const auto vertexBufferMemReqs    = (MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress);

    for (uint32_t rowIdx = 0u; rowIdx < fbExtentU.y(); ++rowIdx)
    {
        vertexBuffers.emplace_back(
            new BufferWithMemory(vkd, device, allocator, vertexBufferCreateInfo, vertexBufferMemReqs));
        auto &alloc       = vertexBuffers.back()->getAllocation();
        void *data        = alloc.getHostPtr();
        const auto srcIdx = fbExtentU.x() * rowIdx;
        deMemcpy(data, &vertices.at(srcIdx), static_cast<size_t>(vertexBufferSize));
    }

    const PipelineLayoutWrapper pipelineLayout(constructionType, vkd, device);

    // We have two bindings. From one of them we'll extract the XY coordinates, and the second one will contain the Z.
    // Note the stride will be obtained from the DGC buffer.
    std::vector<VkVertexInputBindingDescription> bindings;
    bindings.reserve(2u);
    bindings.push_back(makeVertexInputBindingDescription(0u, 0u, VK_VERTEX_INPUT_RATE_VERTEX));
    bindings.push_back(makeVertexInputBindingDescription(1u, 0u, VK_VERTEX_INPUT_RATE_VERTEX));

    std::vector<VkVertexInputAttributeDescription> attributes;
    attributes.reserve(2u);
    attributes.push_back(makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32_SFLOAT, 0u));
    attributes.push_back(makeVertexInputAttributeDescription(1u, 1u, VK_FORMAT_R32_SFLOAT, 0u));

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, //  VkStructureType                             sType;
        nullptr,                    //  const void*                                 pNext;
        0u,                         //  VkPipelineVertexInputStateCreateFlags       flags;
        de::sizeU32(bindings),      //  uint32_t                                    vertexBindingDescriptionCount;
        de::dataOrNull(bindings),   //  const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
        de::sizeU32(attributes),    //  uint32_t                                    vertexAttributeDescriptionCount;
        de::dataOrNull(attributes), //  const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
    };

    const auto stencilOpState =
        makeStencilOpState(VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_NEVER, 0u, 0u, 0u);

    const VkPipelineDepthStencilStateCreateInfo depthStencilState = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, //  VkStructureType                         sType;
        nullptr,                                                    //  const void*                             pNext;
        0u,                                                         //  VkPipelineDepthStencilStateCreateFlags  flags;
        VK_TRUE,             //  VkBool32                                depthTestEnable;
        VK_TRUE,             //  VkBool32                                depthWriteEnable;
        VK_COMPARE_OP_EQUAL, //  VkCompareOp                             depthCompareOp;
        VK_FALSE,            //  VkBool32                                depthBoundsTestEnable;
        VK_FALSE,            //  VkBool32                                stencilTestEnable;
        stencilOpState,      //  VkStencilOpState                        front;
        stencilOpState,      //  VkStencilOpState                        back;
        0.0f,                //  float                                   minDepthBounds;
        1.0f,                //  float                                   maxDepthBounds;
    };

    const std::vector<VkDynamicState> dynamicStates{VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE};

    const VkPipelineDynamicStateCreateInfo dynamicState = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, //  VkStructureType                     sType;
        nullptr,                                              //  const void*                         pNext;
        0u,                                                   //  VkPipelineDynamicStateCreateFlags   flags;
        de::sizeU32(dynamicStates),                           //  uint32_t                            dynamicStateCount;
        de::dataOrNull(dynamicStates),                        //  const VkDynamicState*               pDynamicStates;
    };

    const auto &binaries = m_context.getBinaryCollection();
    ShaderWrapper vertShader(vkd, device, binaries.get("vert"));
    ShaderWrapper fragShader(vkd, device, binaries.get("frag"));

    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    const std::vector<VkImage> fbImages{colorBuffer.getImage(), depthBuffer.getImage()};
    const std::vector<VkImageView> fbViews{colorBuffer.getImageView(), depthBuffer.getImageView()};
    DE_ASSERT(fbImages.size() == fbViews.size());

    RenderPassWrapper renderPass(constructionType, vkd, device, colorFormat, depthFormat);
    renderPass.createFramebuffer(vkd, device, de::sizeU32(fbImages), de::dataOrNull(fbImages), de::dataOrNull(fbViews),
                                 apiExtent.width, apiExtent.height);

    GraphicsPipelineWrapper pipelineWrapper(vki, vkd, physicalDevice, device, deviceExtensions, constructionType);
    pipelineWrapper.setMonolithicPipelineLayout(pipelineLayout)
        .setDefaultColorBlendState()
        .setDefaultMultisampleState()
        .setDefaultRasterizationState()
        .setDefaultPatchControlPoints(0u)
        .setDynamicState(&dynamicState)
        .setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
        .setupVertexInputState(&vertexInputStateCreateInfo)
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass.get(), 0u, vertShader)
        .setupFragmentShaderState(pipelineLayout, renderPass.get(), 0u, fragShader, &depthStencilState)
        .setupFragmentOutputState(renderPass.get())
        .buildPipeline();

    VkIndirectCommandsLayoutUsageFlagsEXT layoutFlags = 0u;
    if (m_params.preprocess)
        layoutFlags |= VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_EXT;
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(layoutFlags, shaderStages, *pipelineLayout);
    cmdsLayoutBuilder.addVertexBufferToken(cmdsLayoutBuilder.getStreamRange(), 0u);
    cmdsLayoutBuilder.addVertexBufferToken(cmdsLayoutBuilder.getStreamRange(), 1u);
    cmdsLayoutBuilder.addDrawToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(vkd, device);

    // Each row gets its own vertex buffers and draw.
    std::vector<uint32_t> dgcData;
    dgcData.reserve((cmdsLayoutBuilder.getStreamStride() * fbExtent.y()) / DE_SIZEOF32(uint32_t));

    de::Random rnd(m_params.getRandomSeed());
    const auto kMaxPadding = 3;

    for (uint32_t rowIdx = 0u; rowIdx < fbExtentU.y(); ++rowIdx)
    {
        const auto deviceAddress = getBufferDeviceAddress(vkd, device, vertexBuffers.at(rowIdx)->get());
        const VkBindVertexBufferIndirectCommandEXT realBindCmd{
            deviceAddress,
            static_cast<uint32_t>(vertexBufferSize),
            DE_SIZEOF32(tcu::Vec2),
        };
        pushBackElement(dgcData, realBindCmd);

        const auto nullVBOPadding = static_cast<uint32_t>(rnd.getInt(0, kMaxPadding));
        const auto nullVBOStride  = (nullVBOPadding + 1u) * DE_SIZEOF32(float);
        const auto nullVBOSize    = nullVBOStride * fbExtent.x();
        const VkBindVertexBufferIndirectCommandEXT nullVBOCmd{
            0ull,
            nullVBOSize,
            nullVBOStride,
        };
        pushBackElement(dgcData, nullVBOCmd);

        const VkDrawIndirectCommand drawCmd = {
            fbExtentU.x(),
            1u,
            0u,
            0u,
        };
        pushBackElement(dgcData, drawCmd);
    }

    const auto dgcBufferSize = static_cast<VkDeviceSize>(de::dataSize(dgcData));
    DGCBuffer dgcBuffer(vkd, device, allocator, dgcBufferSize);
    {
        auto &dgcBufferAlloc = dgcBuffer.getAllocation();
        void *dgcBufferData  = dgcBufferAlloc.getHostPtr();
        deMemcpy(dgcBufferData, de::dataOrNull(dgcData), de::dataSize(dgcData));
    }

    CommandPoolWithBuffer cmd(vkd, device, qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;
    Move<VkCommandBuffer> preprocessCmdBuffer;

    const std::vector<VkClearValue> clearValues{
        makeClearValueColor(NullVBOCase::getClearColor()),
        makeClearValueDepthStencil(0.0f, 0u), // Depth must be zero so the test passes with the null VBO Z value.
    };

    const VkPipeline pipelineHandle = (m_params.useShaderObjects ? VK_NULL_HANDLE : pipelineWrapper.getPipeline());
    const std::vector<VkShaderEXT> shadersVec{
        pipelineWrapper.getShader(VK_SHADER_STAGE_VERTEX_BIT),
        pipelineWrapper.getShader(VK_SHADER_STAGE_FRAGMENT_BIT),
    };
    const std::vector<VkShaderEXT> *shadersVecPtr = (m_params.useShaderObjects ? &shadersVec : nullptr);
    PreprocessBufferExt preprocessBuffer(vkd, device, allocator, VK_NULL_HANDLE, *cmdsLayout, fbExtentU.y(), 0u,
                                         pipelineHandle, shadersVecPtr);

    if (m_params.preprocess)
    {
        preprocessCmdBuffer = allocateCommandBuffer(vkd, device, *cmd.cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        beginCommandBuffer(vkd, *preprocessCmdBuffer);
    }

    beginCommandBuffer(vkd, cmdBuffer);
    renderPass.begin(vkd, cmdBuffer, scissors.at(0u), de::sizeU32(clearValues), de::dataOrNull(clearValues));
    pipelineWrapper.bind(cmdBuffer);
    {
        const DGCGenCmdsInfo cmdsInfo(shaderStages, VK_NULL_HANDLE, *cmdsLayout, dgcBuffer.getDeviceAddress(),
                                      dgcBuffer.getSize(), preprocessBuffer.getDeviceAddress(),
                                      preprocessBuffer.getSize(), fbExtentU.y(), 0ull, 0u, pipelineHandle,
                                      shadersVecPtr);
        if (m_params.preprocess)
        {
            vkd.cmdPreprocessGeneratedCommandsEXT(*preprocessCmdBuffer, &cmdsInfo.get(), cmdBuffer);
            preprocessToExecuteBarrierExt(vkd, *preprocessCmdBuffer);
        }
        const auto isPreprocessed = makeVkBool(m_params.preprocess);
        vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, isPreprocessed, &cmdsInfo.get());
    }
    renderPass.end(vkd, cmdBuffer);
    copyImageToBuffer(vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1));
    endCommandBuffer(vkd, cmdBuffer);

    if (m_params.preprocess)
    {
        preprocessToExecuteBarrierExt(vkd, *preprocessCmdBuffer);
        endCommandBuffer(vkd, *preprocessCmdBuffer);
    }

    submitAndWaitWithPreprocess(vkd, device, queue, cmdBuffer, *preprocessCmdBuffer);

    const auto tcuFormat = mapVkFormat(colorFormat);
    tcu::TextureLevel referenceLevel(tcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    auto referenceAccess = referenceLevel.getAccess();
    tcu::clear(referenceAccess, NullVBOCase::getGeometryColor());

    const auto resultAlloc = colorBuffer.getBufferAllocation();
    invalidateAlloc(vkd, device, resultAlloc);

    tcu::ConstPixelBufferAccess resultAccess(tcuFormat, fbExtent, resultAlloc.getHostPtr());

    auto &log = m_context.getTestContext().getLog();
    if (!tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
                                    tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Unexpected result in color buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

// As long as the interface matches between shaders we can have a wide variety of interfaces in the same indirect
// execution set. We'll draw to 4 quadrants of the viewport using 4 sets of shaders.
enum class TestType
{
    SINGLE_EXEC = 0, // Prepare IES with multiple pipelines/shaders and interfaces, execute once.
    REPLACE     = 1, // Use single IES entry, replacing between multiple executions.
    ADDITION    = 2, // Multiple IES entries, multiple executions without synchronization.
};

class MultiIfaceCase : public vkt::TestCase
{
public:
    struct Params
    {
        TestType testType;
        bool useShaderObjects;
    };

    // Push constants.
    struct PushConstants
    {
        // Scale will be fixed in this case.
        PushConstants(float offsetX, float offsetY, float offsetZ, float offsetW)
            : scale(1.0f, 1.0f, 1.0f, 1.0f)
            , offset(offsetX, offsetY, offsetZ, offsetW)
        {
        }

        tcu::Vec4 scale;
        tcu::Vec4 offset;

        static std::string getDeclaration(void)
        {
            return "layout (push_constant, std430) uniform PCBlock { vec4 scale; vec4 offset; } pc;\n";
        }
    };

    MultiIfaceCase(tcu::TestContext &testCtx, const std::string &name, const Params &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~MultiIfaceCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

    static VkShaderStageFlags getStages(void);
    static constexpr uint32_t kQuadrants = 4u;

protected:
    const Params m_params;
};

class IfaceMatchingInstance : public vkt::TestInstance
{
public:
    IfaceMatchingInstance(Context &context, const MultiIfaceCase::Params &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }
    virtual ~IfaceMatchingInstance(void) = default;

    static tcu::Vec4 getClearColor(void);

    virtual std::vector<uint32_t> getIESIndices() const;

    virtual void submitWork(const ContextCommonData &ctx, const std::vector<DGCShaderExtPtr> &vertShaders,
                            const std::vector<DGCShaderExtPtr> &fragShaders,
                            const std::vector<Move<VkPipeline>> &pipelines, const VkPushConstantRange &pcRange,
                            VkRenderPass renderPass, VkFramebuffer framebuffer, const VkViewport &viewport,
                            const VkRect2D &scissor, const VkPipelineVertexInputStateCreateInfo &vertexInputState,
                            VkIndirectCommandsLayoutEXT cmdsLayout, const DGCBuffer &dgcBuffer, uint32_t dgcStride,
                            const BufferWithMemory &vertexBuffer, const ImageWithBuffer &colorBuffer);

    tcu::TestStatus iterate(void) override;

protected:
    const MultiIfaceCase::Params m_params;
};

class IESReplaceInstance : public IfaceMatchingInstance
{
public:
    IESReplaceInstance(Context &context, const MultiIfaceCase::Params &params) : IfaceMatchingInstance(context, params)
    {
    }
    virtual ~IESReplaceInstance(void) = default;

    std::vector<uint32_t> getIESIndices() const override;

    void submitWork(const ContextCommonData &ctx, const std::vector<DGCShaderExtPtr> &vertShaders,
                    const std::vector<DGCShaderExtPtr> &fragShaders, const std::vector<Move<VkPipeline>> &pipelines,
                    const VkPushConstantRange &pcRange, VkRenderPass renderPass, VkFramebuffer framebuffer,
                    const VkViewport &viewport, const VkRect2D &scissor,
                    const VkPipelineVertexInputStateCreateInfo &vertexInputState,
                    VkIndirectCommandsLayoutEXT cmdsLayout, const DGCBuffer &dgcBuffer, uint32_t dgcStride,
                    const BufferWithMemory &vertexBuffer, const ImageWithBuffer &colorBuffer) override;
};

class IESAdditionInstance : public IfaceMatchingInstance
{
public:
    IESAdditionInstance(Context &context, const MultiIfaceCase::Params &params) : IfaceMatchingInstance(context, params)
    {
    }
    virtual ~IESAdditionInstance(void) = default;

    void submitWork(const ContextCommonData &ctx, const std::vector<DGCShaderExtPtr> &vertShaders,
                    const std::vector<DGCShaderExtPtr> &fragShaders, const std::vector<Move<VkPipeline>> &pipelines,
                    const VkPushConstantRange &pcRange, VkRenderPass renderPass, VkFramebuffer framebuffer,
                    const VkViewport &viewport, const VkRect2D &scissor,
                    const VkPipelineVertexInputStateCreateInfo &vertexInputState,
                    VkIndirectCommandsLayoutEXT cmdsLayout, const DGCBuffer &dgcBuffer, uint32_t dgcStride,
                    const BufferWithMemory &vertexBuffer, const ImageWithBuffer &colorBuffer) override;
};

VkShaderStageFlags MultiIfaceCase::getStages()
{
    return (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
}

void MultiIfaceCase::checkSupport(Context &context) const
{
    const auto stages                 = getStages();
    const auto bindStages             = stages;
    const auto bindStagesPipeline     = (m_params.useShaderObjects ? 0u : bindStages);
    const auto bindStagesShaderObject = (m_params.useShaderObjects ? bindStages : 0u);

    checkDGCExtSupport(context, stages, bindStagesPipeline, bindStagesShaderObject);

    if (m_params.useShaderObjects)
        context.requireDeviceFunctionality("VK_EXT_shader_object");
}

using NameValue = std::pair<std::string, std::string>;

struct ShaderInterface
{
    // Maps locations to variable names and vertex output values.
    // For the vertex output values we'll use both fixed values or expressions that depend on inPos to create gradients.
    std::map<uint32_t, NameValue> locationNameValue;

    // String to write in the fragment shader output components.
    // They may be fixed values or names of input variables.
    std::string fragRed;
    std::string fragGreen;
    std::string fragBlue;
    std::string fragAlpha;
};

void genIfaceShaders(vk::SourceCollections &programCollection, const ShaderInterface &shaderIface, uint32_t index)
{
    const auto indexStr = std::to_string(index);

    {
        std::ostringstream vert;
        vert << "#version 460\n"
             << "layout (location=0) in vec4 inPos;\n"
             << MultiIfaceCase::PushConstants::getDeclaration();

        for (const auto &location : shaderIface.locationNameValue)
        {
            const auto &idx  = location.first;
            const auto &name = location.second.first;
            vert << "layout (location=" << idx << ") out float " << name << ";\n";
        }

        vert << "void main (void) {\n";

        for (const auto &location : shaderIface.locationNameValue)
        {
            const auto &name  = location.second.first;
            const auto &value = location.second.second;
            vert << "    " << name << " = " << value << ";\n";
        }

        vert << "    gl_Position = inPos * pc.scale + pc.offset;\n"
             << "}\n";

        const auto vertName = std::string("vert") + indexStr;
        programCollection.glslSources.add(vertName) << glu::VertexSource(vert.str());
    }

    {
        std::ostringstream frag;
        frag << "#version 460\n"
             << "layout (location=0) out vec4 outColor;\n";

        for (const auto &location : shaderIface.locationNameValue)
        {
            const auto &idx  = location.first;
            const auto &name = location.second.first;
            frag << "layout (location=" << idx << ") in float " << name << ";\n";
        }
        frag << "void main (void) {\n"
             << "    outColor = vec4(" << shaderIface.fragRed << ", " << shaderIface.fragGreen << ", "
             << shaderIface.fragBlue << ", " << shaderIface.fragAlpha << ");\n"
             << "}\n";

        const auto fragName = std::string("frag") + indexStr;
        programCollection.glslSources.add(fragName) << glu::FragmentSource(frag.str());
    }
}

void MultiIfaceCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::vector<ShaderInterface> quadrantIfaces;
    quadrantIfaces.reserve(kQuadrants);

    {
        quadrantIfaces.emplace_back();
        auto &iface = quadrantIfaces.back();

        iface.locationNameValue[2] = std::make_pair(std::string("red"), std::string("inPos.x"));
        iface.fragRed              = "red";
        iface.fragGreen            = "0.0";
        iface.fragBlue             = "0.0";
        iface.fragAlpha            = "1.0";
    }
    {
        quadrantIfaces.emplace_back();
        auto &iface = quadrantIfaces.back();

        iface.locationNameValue[5]  = std::make_pair(std::string("red"), std::string("1.0"));
        iface.locationNameValue[6]  = std::make_pair(std::string("green"), std::string("inPos.x"));
        iface.locationNameValue[10] = std::make_pair(std::string("blue"), std::string("0.5"));
        iface.locationNameValue[11] = std::make_pair(std::string("alpha"), std::string("inPos.y"));
        iface.fragRed               = "red";
        iface.fragGreen             = "green";
        iface.fragBlue              = "blue";
        iface.fragAlpha             = "alpha";
    }
    {
        quadrantIfaces.emplace_back();
        auto &iface = quadrantIfaces.back();

        iface.locationNameValue[1] = std::make_pair(std::string("red"), std::string("0.5"));
        iface.locationNameValue[4] = std::make_pair(std::string("blue"), std::string("inPos.x"));
        iface.fragRed              = "red";
        iface.fragGreen            = "1.0";
        iface.fragBlue             = "blue";
        iface.fragAlpha            = "1.0";
    }
    {
        quadrantIfaces.emplace_back();
        auto &iface = quadrantIfaces.back();

        iface.locationNameValue[0] = std::make_pair(std::string("red"), std::string("inPos.y"));
        iface.locationNameValue[1] = std::make_pair(std::string("green"), std::string("inPos.x"));
        iface.locationNameValue[8] = std::make_pair(std::string("blue"), std::string("1.0"));
        iface.fragRed              = "red";
        iface.fragGreen            = "green";
        iface.fragBlue             = "blue";
        iface.fragAlpha            = "1.0";
    }

    DE_ASSERT(quadrantIfaces.size() == kQuadrants);
    for (size_t i = 0u; i < quadrantIfaces.size(); ++i)
    {
        const auto &iface = quadrantIfaces.at(i);
        genIfaceShaders(programCollection, iface, static_cast<uint32_t>(i));
    }
}

tcu::IVec3 getFramebufferExtent(const tcu::IVec3 &quadrantExtent)
{
    return tcu::IVec3(quadrantExtent.x() * 2, quadrantExtent.y() * 2, quadrantExtent.z());
}

// Check the result image matches the parameters from initPrograms above.
bool checkResults(tcu::TestLog &log, const tcu::TextureFormat &tcuFormat, const tcu::IVec3 &quadrantExtent,
                  const tcu::ConstPixelBufferAccess &resultAccess)
{
    const auto fbExtent   = getFramebufferExtent(quadrantExtent);
    const auto clearColor = IfaceMatchingInstance::getClearColor();

    tcu::TextureLevel referenceLevel(tcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    auto referenceAccess = referenceLevel.getAccess();
    tcu::clear(referenceAccess, clearColor);

    // Every quadrant is prepared as per the shader interfaces and values from initPrograms().
    const auto quadrantExtentF = quadrantExtent.asFloat();

    {
        const auto topLeft = tcu::getSubregion(referenceAccess, 0, 0, quadrantExtent.x(), quadrantExtent.y());
        for (int y = 0; y < quadrantExtent.y(); ++y)
            for (int x = 0; x < quadrantExtent.x(); ++x)
            {
                const auto red   = (static_cast<float>(x) + 0.5f) / quadrantExtentF.x();
                const auto green = 0.0f;
                const auto blue  = 0.0f;
                const auto alpha = 1.0f;
                const tcu::Vec4 color(red, green, blue, alpha);
                topLeft.setPixel(color, x, y);
            }
    }

    {
        const auto topRight =
            tcu::getSubregion(referenceAccess, quadrantExtent.x(), 0, quadrantExtent.x(), quadrantExtent.y());
        for (int y = 0; y < quadrantExtent.y(); ++y)
            for (int x = 0; x < quadrantExtent.x(); ++x)
            {
                const auto red   = 1.0f;
                const auto green = (static_cast<float>(x) + 0.5f) / quadrantExtentF.x();
                const auto blue  = 0.5f;
                const auto alpha = (static_cast<float>(y) + 0.5f) / quadrantExtentF.y();
                const tcu::Vec4 color(red, green, blue, alpha);
                topRight.setPixel(color, x, y);
            }
    }

    {
        const auto bottomLeft =
            tcu::getSubregion(referenceAccess, 0, quadrantExtent.y(), quadrantExtent.x(), quadrantExtent.y());
        for (int y = 0; y < quadrantExtent.y(); ++y)
            for (int x = 0; x < quadrantExtent.x(); ++x)
            {
                const auto red   = 0.5f;
                const auto green = 1.0f;
                const auto blue  = (static_cast<float>(x) + 0.5f) / quadrantExtentF.x();
                const auto alpha = 1.0f;
                const tcu::Vec4 color(red, green, blue, alpha);
                bottomLeft.setPixel(color, x, y);
            }
    }

    {
        const auto bottomRight = tcu::getSubregion(referenceAccess, quadrantExtent.x(), quadrantExtent.y(),
                                                   quadrantExtent.x(), quadrantExtent.y());
        for (int y = 0; y < quadrantExtent.y(); ++y)
            for (int x = 0; x < quadrantExtent.x(); ++x)
            {
                const auto red   = (static_cast<float>(y) + 0.5f) / quadrantExtentF.y();
                const auto green = (static_cast<float>(x) + 0.5f) / quadrantExtentF.x();
                const auto blue  = 1.0f;
                const auto alpha = 1.0f;
                const tcu::Vec4 color(red, green, blue, alpha);
                bottomRight.setPixel(color, x, y);
            }
    }

    const float threshold = 0.005f; // 1/255 < 0.005 < 2/255
    const tcu::Vec4 thresholdVec(threshold, threshold, threshold, threshold);

    return tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, thresholdVec,
                                      tcu::COMPARE_LOG_ON_ERROR);
}

TestInstance *MultiIfaceCase::createInstance(Context &context) const
{
    if (m_params.testType == TestType::SINGLE_EXEC)
        return new IfaceMatchingInstance(context, m_params);
    else if (m_params.testType == TestType::REPLACE)
        return new IESReplaceInstance(context, m_params);
    else if (m_params.testType == TestType::ADDITION)
        return new IESAdditionInstance(context, m_params);
    else
        DE_ASSERT(false);

    return nullptr;
}

tcu::Vec4 IfaceMatchingInstance::getClearColor(void)
{
    return tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
}

std::vector<uint32_t> IfaceMatchingInstance::getIESIndices() const
{
    // Increasing indices for pipelines, and increasing double indices for shader objects.
    std::vector<uint32_t> iesIndices;
    const auto perQuadrantIndexCount = (m_params.useShaderObjects ? 2u : 1u);
    iesIndices.reserve(perQuadrantIndexCount * MultiIfaceCase::kQuadrants);

    for (uint32_t i = 0u; i < MultiIfaceCase::kQuadrants; ++i)
    {
        if (m_params.useShaderObjects)
        {
            iesIndices.push_back(2u * i + 0u);
            iesIndices.push_back(2u * i + 1u);
        }
        else
            iesIndices.push_back(i);
    }

    return iesIndices;
}

std::vector<uint32_t> IESReplaceInstance::getIESIndices() const
{
    // For the replacement case, we'll always use the same elements.
    std::vector<uint32_t> iesIndices;
    const auto perQuadrantIndexCount = (m_params.useShaderObjects ? 2u : 1u);
    iesIndices.reserve(perQuadrantIndexCount * MultiIfaceCase::kQuadrants);

    for (uint32_t i = 0u; i < MultiIfaceCase::kQuadrants; ++i)
    {
        // Note the fixed indices.
        if (m_params.useShaderObjects)
        {
            iesIndices.push_back(0u);
            iesIndices.push_back(1u);
        }
        else
            iesIndices.push_back(0u);
    }

    return iesIndices;
}

void IfaceMatchingInstance::submitWork(const ContextCommonData &ctx, const std::vector<DGCShaderExtPtr> &vertShaders,
                                       const std::vector<DGCShaderExtPtr> &fragShaders,
                                       const std::vector<Move<VkPipeline>> &pipelines,
                                       const VkPushConstantRange &pcRange, VkRenderPass renderPass,
                                       VkFramebuffer framebuffer, const VkViewport &viewport, const VkRect2D &scissor,
                                       const VkPipelineVertexInputStateCreateInfo &vertexInputState,
                                       VkIndirectCommandsLayoutEXT cmdsLayout, const DGCBuffer &dgcBuffer,
                                       uint32_t dgcStride, const BufferWithMemory &vertexBuffer,
                                       const ImageWithBuffer &colorBuffer)
{
    const std::vector<vk::VkDescriptorSetLayout> noLayouts;
    const std::vector<VkPushConstantRange> pcRanges{pcRange};
    const auto shaderStages               = MultiIfaceCase::getStages();
    const auto colorSRR                   = makeDefaultImageSubresourceRange();
    const VkDeviceSize vertexBufferOffset = 0ull;
    const std::vector<VkViewport> viewports{viewport};
    const std::vector<VkRect2D> scissors{scissor};

    // Indirect execution set.
    ExecutionSetManagerPtr executionSetManager;
    if (m_params.useShaderObjects)
    {
        std::vector<IESStageInfo> stages;
        stages.reserve(2u);
        stages.push_back(IESStageInfo(vertShaders.at(0u)->get(), noLayouts));
        stages.push_back(IESStageInfo(fragShaders.at(0u)->get(), noLayouts));
        executionSetManager = makeExecutionSetManagerShader(ctx.vkd, ctx.device, stages, pcRanges,
                                                            MultiIfaceCase::kQuadrants * 2u /*vert and frag*/);

        for (uint32_t i = 0u; i < MultiIfaceCase::kQuadrants; ++i)
        {
            // Indices must match what we store in dgcData.
            executionSetManager->addShader(2u * i + 0u, vertShaders.at(i)->get());
            executionSetManager->addShader(2u * i + 1u, fragShaders.at(i)->get());
        }
    }
    else
    {
        executionSetManager =
            makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, *pipelines.at(0u), MultiIfaceCase::kQuadrants);

        for (uint32_t i = 0u; i < MultiIfaceCase::kQuadrants; ++i)
            executionSetManager->addPipeline(i, *pipelines.at(i));
    }
    executionSetManager->update();
    const auto ies = executionSetManager->get();

    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, ies, cmdsLayout,
                                         MultiIfaceCase::kQuadrants, 0u);

    const CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const auto clearColor    = getClearColor();
    const auto apiClearColor = makeClearValueColor(clearColor);

    beginCommandBuffer(ctx.vkd, cmdBuffer);

    // Clear and transition image outside the render pass.
    {
        const auto preClearBarrier =
            makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, colorBuffer.getImage(), colorSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &preClearBarrier);
        ctx.vkd.cmdClearColorImage(cmdBuffer, colorBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   &apiClearColor.color, 1u, &colorSRR);
        const auto postClearBarrier = makeImageMemoryBarrier(
            VK_ACCESS_TRANSFER_WRITE_BIT, (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorBuffer.getImage(),
            colorSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, &postClearBarrier);
    }

    // Begin render pass.
    if (m_params.useShaderObjects)
    {
        beginRendering(ctx.vkd, cmdBuffer, colorBuffer.getImageView(), scissor, apiClearColor,
                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }
    else
    {
        beginRenderPass(ctx.vkd, cmdBuffer, renderPass, framebuffer, scissor);
    }

    // Bind initial state.
    const auto &features     = m_context.getDeviceFeatures();
    const auto &meshFeatures = m_context.getMeshShaderFeaturesEXT();

    if (m_params.useShaderObjects)
    {
        std::map<VkShaderStageFlagBits, VkShaderEXT> boundShaders{
            std::make_pair(VK_SHADER_STAGE_VERTEX_BIT, vertShaders.at(0u)->get()),
            std::make_pair(VK_SHADER_STAGE_FRAGMENT_BIT, fragShaders.at(0u)->get()),
        };
        if (features.tessellationShader)
        {
            boundShaders[VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT]    = VK_NULL_HANDLE;
            boundShaders[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT] = VK_NULL_HANDLE;
        }
        if (features.geometryShader)
            boundShaders[VK_SHADER_STAGE_GEOMETRY_BIT] = VK_NULL_HANDLE;
        if (meshFeatures.meshShader)
            boundShaders[VK_SHADER_STAGE_MESH_BIT_EXT] = VK_NULL_HANDLE;
        if (meshFeatures.taskShader)
            boundShaders[VK_SHADER_STAGE_TASK_BIT_EXT] = VK_NULL_HANDLE;

        for (const auto &stageHandle : boundShaders)
            ctx.vkd.cmdBindShadersEXT(cmdBuffer, 1u, &stageHandle.first, &stageHandle.second);

        vkt::shaderobjutil::bindShaderObjectState(ctx.vkd, m_context.getDeviceExtensions(), cmdBuffer, viewports,
                                                  scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u, &vertexInputState,
                                                  nullptr, nullptr, nullptr, nullptr);
    }
    else
        ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelines.at(0u));

    // Vertex buffer.
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);

    DE_UNREF(dgcStride);
    const DGCGenCmdsInfo cmdsInfo(shaderStages, ies, cmdsLayout, dgcBuffer.getDeviceAddress(), dgcBuffer.getSize(),
                                  preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(),
                                  MultiIfaceCase::kQuadrants, 0ull, 0u);

    ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());

    if (m_params.useShaderObjects)
        endRendering(ctx.vkd, cmdBuffer);
    else
        endRenderPass(ctx.vkd, cmdBuffer);

    const tcu::IVec2 copyArea(static_cast<int>(scissor.extent.width), static_cast<int>(scissor.extent.height));
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), copyArea);
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);
}

void IESReplaceInstance::submitWork(const ContextCommonData &ctx, const std::vector<DGCShaderExtPtr> &vertShaders,
                                    const std::vector<DGCShaderExtPtr> &fragShaders,
                                    const std::vector<Move<VkPipeline>> &pipelines, const VkPushConstantRange &pcRange,
                                    VkRenderPass renderPass, VkFramebuffer framebuffer, const VkViewport &viewport,
                                    const VkRect2D &scissor,
                                    const VkPipelineVertexInputStateCreateInfo &vertexInputState,
                                    VkIndirectCommandsLayoutEXT cmdsLayout, const DGCBuffer &dgcBuffer,
                                    uint32_t dgcStride, const BufferWithMemory &vertexBuffer,
                                    const ImageWithBuffer &colorBuffer)
{
    const std::vector<vk::VkDescriptorSetLayout> noLayouts;
    const std::vector<VkPushConstantRange> pcRanges{pcRange};
    const auto shaderStages               = MultiIfaceCase::getStages();
    const auto colorSRR                   = makeDefaultImageSubresourceRange();
    const VkDeviceSize vertexBufferOffset = 0ull;
    const std::vector<VkViewport> viewports{viewport};
    const std::vector<VkRect2D> scissors{scissor};
    const auto &features     = m_context.getDeviceFeatures();
    const auto &meshFeatures = m_context.getMeshShaderFeaturesEXT();

    // Indirect execution set, initial values.
    ExecutionSetManagerPtr executionSetManager;
    if (m_params.useShaderObjects)
    {
        std::vector<IESStageInfo> stages;
        stages.reserve(2u);
        stages.push_back(IESStageInfo(vertShaders.at(0u)->get(), noLayouts));
        stages.push_back(IESStageInfo(fragShaders.at(0u)->get(), noLayouts));
        executionSetManager = makeExecutionSetManagerShader(ctx.vkd, ctx.device, stages, pcRanges,
                                                            MultiIfaceCase::kQuadrants * 2u /*vert and frag*/);
    }
    else
    {
        executionSetManager =
            makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, *pipelines.at(0u), MultiIfaceCase::kQuadrants);
    }
    const auto ies = executionSetManager->get();

    // We'll reuse the preprocess buffer between executions, single sequence each time.
    const auto maxSequences = 1u;
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, ies, cmdsLayout, maxSequences, 0u);

    const auto clearColor    = getClearColor();
    const auto apiClearColor = makeClearValueColor(clearColor);
    const auto cmdPool       = makeCommandPool(ctx.vkd, ctx.device, ctx.qfIndex);

    // Clear and transition image outside the render pass.
    {
        const auto cmdBufferPtr = allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        const auto cmdBuffer    = *cmdBufferPtr;

        beginCommandBuffer(ctx.vkd, cmdBuffer);

        const auto preClearBarrier =
            makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, colorBuffer.getImage(), colorSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &preClearBarrier);
        ctx.vkd.cmdClearColorImage(cmdBuffer, colorBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   &apiClearColor.color, 1u, &colorSRR);
        const auto postClearBarrier = makeImageMemoryBarrier(
            VK_ACCESS_TRANSFER_WRITE_BIT, (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorBuffer.getImage(),
            colorSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, &postClearBarrier);

        endCommandBuffer(ctx.vkd, cmdBuffer);
        submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);
    }

    for (uint32_t i = 0u; i < MultiIfaceCase::kQuadrants; ++i)
    {
        const auto cmdBufferPtr = allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        const auto cmdBuffer    = *cmdBufferPtr;

        // Update IES replacing the first elements of it.
        if (m_params.useShaderObjects)
        {
            executionSetManager->addShader(0u, vertShaders.at(i)->get());
            executionSetManager->addShader(1u, fragShaders.at(i)->get());
        }
        else
            executionSetManager->addPipeline(0u, *pipelines.at(i));
        executionSetManager->update();

        beginCommandBuffer(ctx.vkd, cmdBuffer);

        // Begin render pass.
        if (m_params.useShaderObjects)
        {
            beginRendering(ctx.vkd, cmdBuffer, colorBuffer.getImageView(), scissor, apiClearColor,
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }
        else
        {
            beginRenderPass(ctx.vkd, cmdBuffer, renderPass, framebuffer, scissor);
        }

        // Bind initial state.
        if (m_params.useShaderObjects)
        {
            std::map<VkShaderStageFlagBits, VkShaderEXT> boundShaders{
                std::make_pair(VK_SHADER_STAGE_VERTEX_BIT, vertShaders.at(0u)->get()),
                std::make_pair(VK_SHADER_STAGE_FRAGMENT_BIT, fragShaders.at(0u)->get()),
            };
            if (features.tessellationShader)
            {
                boundShaders[VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT]    = VK_NULL_HANDLE;
                boundShaders[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT] = VK_NULL_HANDLE;
            }
            if (features.geometryShader)
                boundShaders[VK_SHADER_STAGE_GEOMETRY_BIT] = VK_NULL_HANDLE;
            if (meshFeatures.meshShader)
                boundShaders[VK_SHADER_STAGE_MESH_BIT_EXT] = VK_NULL_HANDLE;
            if (meshFeatures.taskShader)
                boundShaders[VK_SHADER_STAGE_TASK_BIT_EXT] = VK_NULL_HANDLE;

            for (const auto &stageHandle : boundShaders)
                ctx.vkd.cmdBindShadersEXT(cmdBuffer, 1u, &stageHandle.first, &stageHandle.second);

            vkt::shaderobjutil::bindShaderObjectState(ctx.vkd, m_context.getDeviceExtensions(), cmdBuffer, viewports,
                                                      scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u,
                                                      &vertexInputState, nullptr, nullptr, nullptr, nullptr);
        }
        else
            ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelines.at(0u));

        // Vertex buffer.
        ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);

        // Run single sequence i.
        const auto dgcAddress = dgcBuffer.getDeviceAddress() + dgcStride * i;
        const auto dgcSize    = dgcStride;

        const DGCGenCmdsInfo cmdsInfo(shaderStages, ies, cmdsLayout, dgcAddress, dgcSize,
                                      preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(), maxSequences,
                                      0ull, 0u);

        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());

        // End render pass, submit and wait.
        if (m_params.useShaderObjects)
            endRendering(ctx.vkd, cmdBuffer);
        else
            endRenderPass(ctx.vkd, cmdBuffer);

        endCommandBuffer(ctx.vkd, cmdBuffer);
        submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);
    }

    // Finally copy image to output buffer.
    {
        const auto cmdBufferPtr = allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        const auto cmdBuffer    = *cmdBufferPtr;

        beginCommandBuffer(ctx.vkd, cmdBuffer);
        const tcu::IVec2 copyArea(static_cast<int>(scissor.extent.width), static_cast<int>(scissor.extent.height));
        copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), copyArea, 0u,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1u, VK_IMAGE_ASPECT_COLOR_BIT,
                          VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
        endCommandBuffer(ctx.vkd, cmdBuffer);
        submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);
    }
}

void IESAdditionInstance::submitWork(const ContextCommonData &ctx, const std::vector<DGCShaderExtPtr> &vertShaders,
                                     const std::vector<DGCShaderExtPtr> &fragShaders,
                                     const std::vector<Move<VkPipeline>> &pipelines, const VkPushConstantRange &pcRange,
                                     VkRenderPass renderPass, VkFramebuffer framebuffer, const VkViewport &viewport,
                                     const VkRect2D &scissor,
                                     const VkPipelineVertexInputStateCreateInfo &vertexInputState,
                                     VkIndirectCommandsLayoutEXT cmdsLayout, const DGCBuffer &dgcBuffer,
                                     uint32_t dgcStride, const BufferWithMemory &vertexBuffer,
                                     const ImageWithBuffer &colorBuffer)
{
    const std::vector<vk::VkDescriptorSetLayout> noLayouts;
    const std::vector<VkPushConstantRange> pcRanges{pcRange};
    const auto shaderStages               = MultiIfaceCase::getStages();
    const auto colorSRR                   = makeDefaultImageSubresourceRange();
    const VkDeviceSize vertexBufferOffset = 0ull;
    const std::vector<VkViewport> viewports{viewport};
    const std::vector<VkRect2D> scissors{scissor};
    const auto &features     = m_context.getDeviceFeatures();
    const auto &meshFeatures = m_context.getMeshShaderFeaturesEXT();

    // Indirect execution set, initial values.
    ExecutionSetManagerPtr executionSetManager;
    if (m_params.useShaderObjects)
    {
        std::vector<IESStageInfo> stages;
        stages.reserve(2u);
        stages.push_back(IESStageInfo(vertShaders.at(0u)->get(), noLayouts));
        stages.push_back(IESStageInfo(fragShaders.at(0u)->get(), noLayouts));
        executionSetManager = makeExecutionSetManagerShader(ctx.vkd, ctx.device, stages, pcRanges,
                                                            MultiIfaceCase::kQuadrants * 2u /*vert and frag*/);
    }
    else
    {
        executionSetManager =
            makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, *pipelines.at(0u), MultiIfaceCase::kQuadrants);
    }
    const auto ies = executionSetManager->get();

    // As we may have multiple parallel executions, we need multiple preprocess buffers.
    const auto maxSequences      = 1u;
    using PreprocessBufferExtPtr = std::unique_ptr<PreprocessBufferExt>;
    std::vector<PreprocessBufferExtPtr> preprocessBuffers;
    preprocessBuffers.reserve(MultiIfaceCase::kQuadrants);
    for (uint32_t i = 0u; i < MultiIfaceCase::kQuadrants; ++i)
        preprocessBuffers.emplace_back(
            new PreprocessBufferExt(ctx.vkd, ctx.device, ctx.allocator, ies, cmdsLayout, maxSequences, 0u));

    const auto clearColor    = getClearColor();
    const auto apiClearColor = makeClearValueColor(clearColor);
    const auto cmdPool       = makeCommandPool(ctx.vkd, ctx.device, ctx.qfIndex);

    // Clear and transition image outside the render pass.
    {
        const auto cmdBufferPtr = allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        const auto cmdBuffer    = *cmdBufferPtr;

        beginCommandBuffer(ctx.vkd, cmdBuffer);

        const auto preClearBarrier =
            makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, colorBuffer.getImage(), colorSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &preClearBarrier);
        ctx.vkd.cmdClearColorImage(cmdBuffer, colorBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   &apiClearColor.color, 1u, &colorSRR);
        const auto postClearBarrier = makeImageMemoryBarrier(
            VK_ACCESS_TRANSFER_WRITE_BIT, (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorBuffer.getImage(),
            colorSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, &postClearBarrier);

        endCommandBuffer(ctx.vkd, cmdBuffer);
        submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);
    }

    std::vector<Move<VkFence>> fences;
    fences.reserve(MultiIfaceCase::kQuadrants);

    // We need to store command buffers outside the loop because we won't wait for them to complete and we cannot
    // destroy them while they're in flight.
    std::vector<Move<VkCommandBuffer>> cmdBuffers;
    cmdBuffers.reserve(MultiIfaceCase::kQuadrants);

    for (uint32_t i = 0u; i < MultiIfaceCase::kQuadrants; ++i)
    {
        cmdBuffers.emplace_back(allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
        const auto cmdBuffer = cmdBuffers.back().get();

        // Update IES by adding elements to it.
        if (m_params.useShaderObjects)
        {
            executionSetManager->addShader(2u * i + 0u, vertShaders.at(i)->get());
            executionSetManager->addShader(2u * i + 1u, fragShaders.at(i)->get());
        }
        else
            executionSetManager->addPipeline(i, *pipelines.at(i));
        executionSetManager->update();

        beginCommandBuffer(ctx.vkd, cmdBuffer);

        // Begin render pass.
        if (m_params.useShaderObjects)
        {
            beginRendering(ctx.vkd, cmdBuffer, colorBuffer.getImageView(), scissor, apiClearColor,
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }
        else
        {
            beginRenderPass(ctx.vkd, cmdBuffer, renderPass, framebuffer, scissor);
        }

        // Bind initial state.
        if (m_params.useShaderObjects)
        {
            std::map<VkShaderStageFlagBits, VkShaderEXT> boundShaders{
                std::make_pair(VK_SHADER_STAGE_VERTEX_BIT, vertShaders.at(0u)->get()),
                std::make_pair(VK_SHADER_STAGE_FRAGMENT_BIT, fragShaders.at(0u)->get()),
            };
            if (features.tessellationShader)
            {
                boundShaders[VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT]    = VK_NULL_HANDLE;
                boundShaders[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT] = VK_NULL_HANDLE;
            }
            if (features.geometryShader)
                boundShaders[VK_SHADER_STAGE_GEOMETRY_BIT] = VK_NULL_HANDLE;
            if (meshFeatures.meshShader)
                boundShaders[VK_SHADER_STAGE_MESH_BIT_EXT] = VK_NULL_HANDLE;
            if (meshFeatures.taskShader)
                boundShaders[VK_SHADER_STAGE_TASK_BIT_EXT] = VK_NULL_HANDLE;

            for (const auto &stageHandle : boundShaders)
                ctx.vkd.cmdBindShadersEXT(cmdBuffer, 1u, &stageHandle.first, &stageHandle.second);

            vkt::shaderobjutil::bindShaderObjectState(ctx.vkd, m_context.getDeviceExtensions(), cmdBuffer, viewports,
                                                      scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u,
                                                      &vertexInputState, nullptr, nullptr, nullptr, nullptr);
        }
        else
            ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelines.at(0u));

        // Vertex buffer.
        ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);

        // Run single sequence i.
        const auto dgcAddress = dgcBuffer.getDeviceAddress() + dgcStride * i;
        const auto dgcSize    = dgcStride;

        const DGCGenCmdsInfo cmdsInfo(shaderStages, ies, cmdsLayout, dgcAddress, dgcSize,
                                      preprocessBuffers.at(i)->getDeviceAddress(), preprocessBuffers.at(i)->getSize(),
                                      maxSequences, 0ull, 0u);

        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());

        // End render pass and submit *without* wait.
        if (m_params.useShaderObjects)
            endRendering(ctx.vkd, cmdBuffer);
        else
            endRenderPass(ctx.vkd, cmdBuffer);

        endCommandBuffer(ctx.vkd, cmdBuffer);
        fences.emplace_back(submitCommands(ctx.vkd, ctx.device, ctx.queue, cmdBuffer));
    }

    // Wait for completion of all fences.
    std::vector<VkFence> fenceHandles;
    fenceHandles.reserve(fences.size());
    std::transform(begin(fences), end(fences), std::back_inserter(fenceHandles),
                   [](const Move<VkFence> &fence) { return fence.get(); });

    const uint64_t infinite = (~0ull);
    ctx.vkd.waitForFences(ctx.device, de::sizeU32(fenceHandles), de::dataOrNull(fenceHandles), VK_TRUE /*waitAll*/,
                          infinite);

    // Finally copy image to output buffer.
    {
        const auto cmdBufferPtr = allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        const auto cmdBuffer    = *cmdBufferPtr;

        beginCommandBuffer(ctx.vkd, cmdBuffer);
        const tcu::IVec2 copyArea(static_cast<int>(scissor.extent.width), static_cast<int>(scissor.extent.height));
        copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), copyArea, 0u,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1u, VK_IMAGE_ASPECT_COLOR_BIT,
                          VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
        endCommandBuffer(ctx.vkd, cmdBuffer);
        submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);
    }
}

tcu::TestStatus IfaceMatchingInstance::iterate(void)
{
    const auto ctx = m_context.getContextCommonData();
    const tcu::IVec3 quadrantExtent(8, 8, 1);
    const auto fbExtent    = getFramebufferExtent(quadrantExtent);
    const auto apiExtent   = makeExtent3D(fbExtent);
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto tcuFormat   = mapVkFormat(colorFormat);
    const auto colorUsage =
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto shaderStages = MultiIfaceCase::getStages();

    // We'll use a quad covering (0,0) to (1,1) and pass offsets in each draw as push constants.
    std::vector<tcu::Vec4> vertices;
    vertices.reserve(4u);
    vertices.emplace_back(0.0f, 0.0f, 0.0f, 1.0f);
    vertices.emplace_back(0.0f, 1.0f, 0.0f, 1.0f);
    vertices.emplace_back(1.0f, 0.0f, 0.0f, 1.0f);
    vertices.emplace_back(1.0f, 1.0f, 0.0f, 1.0f);

    const auto vertexBufferInfo = makeBufferCreateInfo(de::dataSize(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc   = vertexBuffer.getAllocation();
        void *dataPtr = alloc.getHostPtr();
        deMemcpy(dataPtr, de::dataOrNull(vertices), de::dataSize(vertices));
    }

    const VkDrawIndirectCommand drawCmd = {
        de::sizeU32(vertices),
        1u,
        0u,
        0u,
    };

    // Color buffer.
    const auto colorSRR = makeDefaultImageSubresourceRange();
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, apiExtent, colorFormat, colorUsage,
                                VK_IMAGE_TYPE_2D, colorSRR);

    const auto pcSize         = DE_SIZEOF32(MultiIfaceCase::PushConstants);
    const auto pcRange        = makePushConstantRange(shaderStages, 0u, pcSize);
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, VK_NULL_HANDLE, &pcRange);

    std::vector<MultiIfaceCase::PushConstants> pushConstants;
    pushConstants.reserve(MultiIfaceCase::kQuadrants);
    pushConstants.emplace_back(-1.0f, -1.0f, 0.0f, 0.0f);
    pushConstants.emplace_back(0.0f, -1.0f, 0.0f, 0.0f);
    pushConstants.emplace_back(-1.0f, 0.0f, 0.0f, 0.0f);
    pushConstants.emplace_back(0.0f, 0.0f, 0.0f, 0.0f);
    DE_ASSERT(pushConstants.size() == MultiIfaceCase::kQuadrants);

    const auto iesType = (m_params.useShaderObjects ? VK_INDIRECT_EXECUTION_SET_INFO_TYPE_SHADER_OBJECTS_EXT :
                                                      VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT);
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, shaderStages, *pipelineLayout);
    cmdsLayoutBuilder.addExecutionSetToken(cmdsLayoutBuilder.getStreamRange(), iesType, shaderStages);
    cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(), pcRange);
    cmdsLayoutBuilder.addDrawToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    const auto iesIndices = getIESIndices();

    std::vector<uint32_t> dgcData;
    dgcData.reserve(cmdsLayoutBuilder.getStreamStride() * MultiIfaceCase::kQuadrants / DE_SIZEOF32(uint32_t));
    for (uint32_t i = 0u; i < MultiIfaceCase::kQuadrants; ++i)
    {
        // IES (pipeline or shader indices).
        if (m_params.useShaderObjects)
        {
            dgcData.push_back(iesIndices.at(2u * i + 0u));
            dgcData.push_back(iesIndices.at(2u * i + 1u));
        }
        else
            dgcData.push_back(iesIndices.at(i));

        // Push constants.
        pushBackElement(dgcData, pushConstants.at(i));

        // Draw command.
        pushBackElement(dgcData, drawCmd);
    }

    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, de::dataSize(dgcData));
    {
        auto &allocation = dgcBuffer.getAllocation();
        void *dataPtr    = allocation.getHostPtr();
        deMemcpy(dataPtr, de::dataOrNull(dgcData), de::dataSize(dgcData));
    }

    std::vector<Move<VkShaderModule>> vertModules;
    std::vector<Move<VkShaderModule>> fragModules;

    std::vector<DGCShaderExtPtr> vertShaders;
    std::vector<DGCShaderExtPtr> fragShaders;

    const auto &binaries = m_context.getBinaryCollection();

    if (m_params.useShaderObjects)
    {
        vertShaders.reserve(MultiIfaceCase::kQuadrants);
        fragShaders.reserve(MultiIfaceCase::kQuadrants);
    }
    else
    {
        vertModules.reserve(MultiIfaceCase::kQuadrants);
        fragModules.reserve(MultiIfaceCase::kQuadrants);
    }

    const std::vector<vk::VkDescriptorSetLayout> noLayouts;
    const std::vector<VkPushConstantRange> pcRanges{pcRange};

    const auto &features   = m_context.getDeviceFeatures();
    const auto tessFeature = (features.tessellationShader == VK_TRUE);
    const auto geomFeature = (features.geometryShader == VK_TRUE);

    for (uint32_t i = 0u; i < MultiIfaceCase::kQuadrants; ++i)
    {
        const auto idx         = std::to_string(i);
        const auto vertName    = std::string("vert") + idx;
        const auto fragName    = std::string("frag") + idx;
        const auto &vertBinary = binaries.get(vertName);
        const auto &fragBinary = binaries.get(fragName);

        if (m_params.useShaderObjects)
        {
            vertShaders.emplace_back(new DGCShaderExt(ctx.vkd, ctx.device, VK_SHADER_STAGE_VERTEX_BIT, 0u, vertBinary,
                                                      noLayouts, pcRanges, tessFeature, geomFeature));
            fragShaders.emplace_back(new DGCShaderExt(ctx.vkd, ctx.device, VK_SHADER_STAGE_FRAGMENT_BIT, 0u, fragBinary,
                                                      noLayouts, pcRanges, tessFeature, geomFeature));
        }
        else
        {
            vertModules.emplace_back(createShaderModule(ctx.vkd, ctx.device, vertBinary));
            fragModules.emplace_back(createShaderModule(ctx.vkd, ctx.device, fragBinary));
        }
    }

    std::vector<Move<VkPipeline>> pipelines;
    Move<VkRenderPass> renderPass;
    Move<VkFramebuffer> framebuffer;

    if (!m_params.useShaderObjects)
    {
        renderPass  = makeRenderPass(ctx.vkd, ctx.device, colorFormat, VK_FORMAT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_LOAD);
        framebuffer = makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer.getImageView(), apiExtent.width,
                                      apiExtent.height);
    }

    const auto viewport = makeViewport(fbExtent);
    const auto scissor  = makeRect2D(fbExtent);

    const std::vector<VkViewport> viewports{viewport};
    const std::vector<VkRect2D> scissors{scissor};

    const auto vertexBinding =
        makeVertexInputBindingDescription(0u, DE_SIZEOF32(tcu::Vec4), VK_VERTEX_INPUT_RATE_VERTEX);
    const auto vertexAttrib = makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u);

    const VkPipelineVertexInputStateCreateInfo vertexInputState = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, 0u, 1u, &vertexBinding, 1u, &vertexAttrib,
    };

    if (!m_params.useShaderObjects)
    {
        const VkPipelineCreateFlags2CreateInfoKHR pipelineFlags{
            VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR,
            nullptr,
            VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT,
        };

        for (uint32_t i = 0u; i < MultiIfaceCase::kQuadrants; ++i)
        {
            pipelines.emplace_back(makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *vertModules.at(i),
                                                        VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                                        *fragModules.at(i), *renderPass, viewports, scissors,
                                                        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u, 0u, &vertexInputState,
                                                        nullptr, nullptr, nullptr, nullptr, nullptr, &pipelineFlags));
        }
    }

    // Different variants submit work in different ways, but all should produce the same results.
    submitWork(ctx, vertShaders, fragShaders, pipelines, pcRange, *renderPass, *framebuffer, viewport, scissor,
               vertexInputState, *cmdsLayout, dgcBuffer, cmdsLayoutBuilder.getStreamStride(), vertexBuffer,
               colorBuffer);

    invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());
    tcu::ConstPixelBufferAccess resultAccess(tcuFormat, fbExtent, colorBuffer.getBufferAllocation().getHostPtr());

    auto &log = m_context.getTestContext().getLog();

    if (!checkResults(log, tcuFormat, quadrantExtent, resultAccess))
        return tcu::TestStatus::fail("Unexpected result in color buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

void SequenceIndexPrograms(vk::SourceCollections &dst)
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "void main(void) {\n"
         << "    gl_Position = inPos;\n"
         << "    gl_PointSize = 1.0f;\n"
         << "}\n";
    dst.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (push_constant, std430) uniform PCBlock { uint seqIndex; } pc;\n"
         << "layout (location=0) out uvec4 outColor;\n"
         << "void main(void) {\n"
         << "    outColor = uvec4(pc.seqIndex, 0u, 255u, 255u);\n"
         << "}\n";
    dst.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

void SequenceIndexSupport(Context &context)
{
    const auto stages = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    checkDGCExtSupport(context, stages);
}

tcu::TestStatus SequenceIndexRun(Context &context)
{
    const auto ctx = context.getContextCommonData();
    const tcu::IVec3 fbExtent(256, 1, 1);
    const auto fbExtentU   = fbExtent.asUint();
    const auto floatExtent = fbExtent.asFloat();
    const auto pixelCount  = fbExtentU.x() * fbExtentU.y() * fbExtentU.z();
    const auto apiExtent   = makeExtent3D(fbExtent);
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UINT;
    const auto tcuFormat   = mapVkFormat(colorFormat);
    const auto colorUsage =
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto topology      = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    const auto sequenceCount = pixelCount; // One draw per pixel.

    // Color buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, apiExtent, colorFormat, colorUsage,
                                VK_IMAGE_TYPE_2D);

    // One point per pixel, left to right.
    std::vector<tcu::Vec4> vertices;
    vertices.reserve(pixelCount);

    DE_ASSERT(fbExtent.y() == 1 && fbExtent.z() == 1);
    for (int x = 0; x < fbExtent.x(); ++x)
        vertices.emplace_back((static_cast<float>(x) + 0.5f) / floatExtent.x() * 2.0f - 1.0f, 0.0f, 0.0f, 1.0f);

    const auto vertexBufferInfo = makeBufferCreateInfo(de::dataSize(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc   = vertexBuffer.getAllocation();
        void *dataPtr = alloc.getHostPtr();
        deMemcpy(dataPtr, de::dataOrNull(vertices), de::dataSize(vertices));
    }
    const VkDeviceSize vertexBufferOffset = 0ull;

    // Render pass, framebuffer, shaders, pipeline.
    const auto renderPass  = makeRenderPass(ctx.vkd, ctx.device, colorFormat);
    const auto framebuffer = makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer.getImageView(),
                                             apiExtent.width, apiExtent.height);

    const auto &binaries  = context.getBinaryCollection();
    const auto vertModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

    const auto pcSize   = DE_SIZEOF32(uint32_t);
    const auto pcStages = VK_SHADER_STAGE_FRAGMENT_BIT;
    const auto pcRange  = makePushConstantRange(pcStages, 0u, pcSize);

    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, VK_NULL_HANDLE, &pcRange);

    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    const auto pipeline = makeGraphicsPipeline(
        ctx.vkd, ctx.device, *pipelineLayout, *vertModule, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, *fragModule,
        *renderPass, viewports, scissors, topology); // Default values work fine here including vertex inputs.

    // DGC commands layout, sequences and preprocess buffer.
    const auto shaderStages = (VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, shaderStages, *pipelineLayout);
    cmdsLayoutBuilder.addSequenceIndexToken(0u, pcRange);
    cmdsLayoutBuilder.addDrawToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    std::vector<uint32_t> dgcData;
    dgcData.reserve(cmdsLayoutBuilder.getStreamStride() * sequenceCount / DE_SIZEOF32(uint32_t));

    for (uint32_t i = 0u; i < sequenceCount; ++i)
    {
        dgcData.push_back(~0u); // Sequence index placeholder.
        dgcData.push_back(1u);  // vertexCount
        dgcData.push_back(1u);  // indexCount
        dgcData.push_back(i);   // firstVertex
        dgcData.push_back(0u);  // firstInstance
    }

    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, de::dataSize(dgcData));
    {
        auto &alloc   = dgcBuffer.getAllocation();
        void *dataPtr = alloc.getHostPtr();
        deMemcpy(dataPtr, de::dataOrNull(dgcData), de::dataSize(dgcData));
    }

    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, VK_NULL_HANDLE, *cmdsLayout, sequenceCount,
                                         0u, *pipeline);

    const DGCGenCmdsInfo cmdsInfo(shaderStages, VK_NULL_HANDLE, *cmdsLayout, dgcBuffer.getDeviceAddress(),
                                  dgcBuffer.getSize(), preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(),
                                  sequenceCount, 0ull, 0u, *pipeline);

    // Commands.
    const CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const tcu::UVec4 clearColor(0u, 0u, 0u, 0u);

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
    beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), clearColor);
    ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
    ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());
    endRenderPass(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1));
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Result.
    auto &colorBufferAlloc = colorBuffer.getBufferAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, colorBufferAlloc);

    tcu::ConstPixelBufferAccess resultAccess(tcuFormat, fbExtent, colorBufferAlloc.getHostPtr());

    // Reference.
    tcu::TextureLevel referenceLevel(tcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    auto referenceAccess = referenceLevel.getAccess();

    for (int x = 0; x < fbExtent.x(); ++x)
    {
        // Must match fragment shader, using the sequence index for the red component.
        const tcu::UVec4 color(static_cast<uint32_t>(x), 0u, 255u, 255u);
        referenceAccess.setPixel(color, x, 0);
    }

    auto &log = context.getTestContext().getLog();
    if (!tcu::intThresholdCompare(log, "Result", "", referenceAccess, resultAccess, tcu::UVec4(0u, 0u, 0u, 0u),
                                  tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Unexpected results in color buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

class RayQueryTestInstance : public vkt::TestInstance
{
public:
    struct Params
    {
        bool useExecutionSet;

        VkShaderStageFlags getShaderStages() const
        {
            return (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        }

        uint32_t getFragShaderCount() const
        {
            return (useExecutionSet ? 2u : 1u);
        }
    };

    RayQueryTestInstance(Context &context, const Params &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~RayQueryTestInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const Params m_params;
};

class RayQueryTestCase : public vkt::TestCase
{
public:
    RayQueryTestCase(tcu::TestContext &testCtx, const std::string &name, const RayQueryTestInstance::Params &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~RayQueryTestCase(void) = default;

    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;

protected:
    const RayQueryTestInstance::Params m_params;
};

TestInstance *RayQueryTestCase::createInstance(Context &context) const
{
    return new RayQueryTestInstance(context, m_params);
}

void RayQueryTestCase::checkSupport(Context &context) const
{
    const auto stages     = m_params.getShaderStages();
    const auto bindStages = (m_params.useExecutionSet ? stages : 0u);
    checkDGCExtSupport(context, stages, bindStages);

    context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
    context.requireDeviceFunctionality("VK_KHR_ray_query");
}

void RayQueryTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "void main(void) {\n"
         << "    gl_Position = inPos;\n"
         << "    gl_PointSize = 1.0f;\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
    const auto fragShaderCount = m_params.getFragShaderCount();
    DE_ASSERT(fragShaderCount <= 3u); // Each query will determine one output color component.

    for (uint32_t i = 0u; i < fragShaderCount; ++i)
    {
        std::ostringstream frag;
        frag << "#version 460\n"
             << "#extension GL_EXT_ray_query : enable\n"
             << "layout (location=0) out vec4 outColor;\n"
             << "layout (set=0, binding=0) uniform accelerationStructureEXT topLevelAS;\n";

        frag << "void main(void) {\n"
             << "    const float tmin = 1.0;\n"
             << "    const float tmax = 10.0;\n"
             << "    const uint cullMask = 0xFFu;\n"
             << "    const vec3 direction = vec3(0.0, 0.0, 1.0);\n"
             << "    vec4 colorValue = vec4(0.0, 0.0, 0.0, 1.0);\n"
             << "    bool intersectionFound = false;\n";

        for (uint32_t j = 0u; j <= i; ++j)
        {
            const auto queryName = "query" + std::to_string(j);
            frag << "\n"
                 << "    rayQueryEXT " << queryName << ";\n"
                 << "    rayQueryInitializeEXT(" << queryName << ", topLevelAS, gl_RayFlagsNoneEXT, cullMask, vec3("
                 << j << ", 0.0, 0.0), tmin, direction, tmax);\n"
                 << "    intersectionFound = false;\n"
                 << "    while (rayQueryProceedEXT(" << queryName << ")) {\n"
                 << "        const uint candidateType = rayQueryGetIntersectionTypeEXT(" << queryName << ", false);\n"
                 << "        if (candidateType == gl_RayQueryCandidateIntersectionTriangleEXT || candidateType == "
                    "gl_RayQueryCandidateIntersectionAABBEXT) {\n"
                 << "            intersectionFound = true;\n"
                 << "        }\n"
                 << "    }\n"
                 << "    if (intersectionFound) {\n"
                 << "        colorValue[" << j << "] = 1.0;\n"
                 << "    }\n";
        }

        frag << "\n"
             << "    outColor = colorValue;\n"
             << "}\n";

        const auto shaderName = std::string("frag") + std::to_string(i);
        programCollection.glslSources.add(shaderName) << glu::FragmentSource(frag.str()) << buildOptions;
    }
}

tcu::TestStatus RayQueryTestInstance::iterate()
{
    const auto ctx             = m_context.getContextCommonData();
    const auto fragShaderCount = m_params.getFragShaderCount();
    const tcu::IVec3 fbExtent(static_cast<int>(fragShaderCount), 1, 1);
    const auto fbExtentU   = fbExtent.asUint();
    const auto floatExtent = fbExtent.asFloat();
    const auto pixelCount  = fbExtentU.x() * fbExtentU.y() * fbExtentU.z();
    const auto apiExtent   = makeExtent3D(fbExtent);
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto tcuFormat   = mapVkFormat(colorFormat);
    const auto colorUsage =
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto topology      = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    const auto sequenceCount = pixelCount; // One draw per pixel.

    // Color buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, apiExtent, colorFormat, colorUsage,
                                VK_IMAGE_TYPE_2D);

    // One point per pixel, left to right.
    std::vector<tcu::Vec4> vertices;
    vertices.reserve(pixelCount);

    DE_ASSERT(fbExtent.y() == 1 && fbExtent.z() == 1);
    for (int x = 0; x < fbExtent.x(); ++x)
        vertices.emplace_back((static_cast<float>(x) + 0.5f) / floatExtent.x() * 2.0f - 1.0f, 0.0f, 0.0f, 1.0f);

    const auto vertexBufferInfo = makeBufferCreateInfo(de::dataSize(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc   = vertexBuffer.getAllocation();
        void *dataPtr = alloc.getHostPtr();
        deMemcpy(dataPtr, de::dataOrNull(vertices), de::dataSize(vertices));
    }
    const VkDeviceSize vertexBufferOffset = 0ull;

    // Render pass, framebuffer, shaders, pipeline.
    const auto renderPass  = makeRenderPass(ctx.vkd, ctx.device, colorFormat);
    const auto framebuffer = makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer.getImageView(),
                                             apiExtent.width, apiExtent.height);

    const auto &binaries  = m_context.getBinaryCollection();
    const auto vertModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));

    std::vector<Move<VkShaderModule>> fragModules;
    fragModules.reserve(fragShaderCount);
    for (uint32_t i = 0u; i < fragShaderCount; ++i)
    {
        const auto shaderName = "frag" + std::to_string(i);
        fragModules.emplace_back(createShaderModule(ctx.vkd, ctx.device, binaries.get(shaderName)));
    }

    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto setLayout      = setLayoutBuilder.build(ctx.vkd, ctx.device);
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout);

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
    const auto descPool = poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descSet  = makeDescriptorSet(ctx.vkd, ctx.device, *descPool, *setLayout);

    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    std::vector<Move<VkPipeline>> pipelines;
    pipelines.reserve(fragShaderCount);

    const auto vertexBinding =
        makeVertexInputBindingDescription(0u, DE_SIZEOF32(tcu::Vec4), VK_VERTEX_INPUT_RATE_VERTEX);
    const auto vertexAttribute = makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u);

    const VkPipelineVertexInputStateCreateInfo vertexInput = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        nullptr,
        0u,
        1u,
        &vertexBinding,
        1u,
        &vertexAttribute,
    };

    for (uint32_t i = 0u; i < fragShaderCount; ++i)
    {
        const VkPipelineCreateFlags2CreateInfoKHR flags2 = {
            VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR,
            nullptr,
            VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT,
        };

        const void *pNext = (m_params.useExecutionSet ? &flags2 : nullptr);

        pipelines.emplace_back(makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *vertModule, VK_NULL_HANDLE,
                                                    VK_NULL_HANDLE, VK_NULL_HANDLE, *fragModules.at(i), *renderPass,
                                                    viewports, scissors, topology, 0u, 0u, &vertexInput, nullptr,
                                                    nullptr, nullptr, nullptr, nullptr, pNext));
    }

    // IES if needed.
    ExecutionSetManagerPtr iesManager;
    VkIndirectExecutionSetEXT iesHandle = VK_NULL_HANDLE;

    if (m_params.useExecutionSet)
    {
        iesManager = makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, *pipelines.front(), fragShaderCount);
        for (uint32_t i = 0u; i < fragShaderCount; ++i)
            iesManager->addPipeline(i, *pipelines.at(i));
        iesManager->update();
        iesHandle = iesManager->get();
    }

    // DGC commands layout, sequences and preprocess buffer.
    const auto shaderStages = (VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, shaderStages, *pipelineLayout);
    if (m_params.useExecutionSet)
        cmdsLayoutBuilder.addExecutionSetToken(0u, vk::VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT, shaderStages);
    cmdsLayoutBuilder.addDrawToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    std::vector<uint32_t> dgcData;
    dgcData.reserve(cmdsLayoutBuilder.getStreamStride() * sequenceCount / DE_SIZEOF32(uint32_t));

    for (uint32_t i = 0u; i < sequenceCount; ++i)
    {
        if (m_params.useExecutionSet)
            dgcData.push_back(i);

        dgcData.push_back(1u); // vertexCount
        dgcData.push_back(1u); // indexCount
        dgcData.push_back(i);  // firstVertex
        dgcData.push_back(0u); // firstInstance
    }

    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, de::dataSize(dgcData));
    {
        auto &alloc   = dgcBuffer.getAllocation();
        void *dataPtr = alloc.getHostPtr();
        deMemcpy(dataPtr, de::dataOrNull(dgcData), de::dataSize(dgcData));
    }

    const auto preprocessPipeline = (m_params.useExecutionSet ? VK_NULL_HANDLE : *pipelines.front());

    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, iesHandle, *cmdsLayout, sequenceCount, 0u,
                                         preprocessPipeline);

    const DGCGenCmdsInfo cmdsInfo(shaderStages, iesHandle, *cmdsLayout, dgcBuffer.getDeviceAddress(),
                                  dgcBuffer.getSize(), preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(),
                                  sequenceCount, 0ull, 0u, preprocessPipeline);

    // Commands.
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    const CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);

    // Build acceleration structure.
    auto topLevelAS    = makeTopLevelAccelerationStructure();
    auto bottomLevelAS = makeBottomLevelAccelerationStructure();

    AccelerationStructBufferProperties bufferProps;
    bufferProps.props.residency = ResourceResidency::TRADITIONAL;

    const auto hitIndex       = fragShaderCount - 1u; // Rightmost X location.
    const auto triangleCenter = static_cast<float>(hitIndex);

    const float zCoord                    = 2.0f; // Z value 2 is between tmin (1) and tmax (10).
    const std::vector<tcu::Vec3> triangle = {
        tcu::Vec3(triangleCenter - 0.25f, -1.0f, zCoord),
        tcu::Vec3(triangleCenter + 0.25f, -1.0f, zCoord),
        tcu::Vec3(triangleCenter, 1.0f, zCoord),
    };

    bottomLevelAS->addGeometry(triangle, true /*triangles*/);
    bottomLevelAS->createAndBuild(ctx.vkd, ctx.device, cmdBuffer, ctx.allocator, bufferProps);
    de::SharedPtr<BottomLevelAccelerationStructure> blasSharedPtr(bottomLevelAS.release());

    topLevelAS->setInstanceCount(1);
    topLevelAS->addInstance(blasSharedPtr);
    topLevelAS->createAndBuild(ctx.vkd, ctx.device, cmdBuffer, ctx.allocator, bufferProps);

    // Update descriptor set.
    DescriptorSetUpdateBuilder updateBuilder;
    const VkWriteDescriptorSetAccelerationStructureKHR writeAS = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        nullptr,
        1u,
        topLevelAS.get()->getPtr(),
    };
    updateBuilder.writeSingle(*descSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                              VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &writeAS);
    updateBuilder.update(ctx.vkd, ctx.device);

    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &descSet.get(),
                                  0u, nullptr);
    beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), clearColor);
    ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelines.front());
#if 1
    ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());
#else
    for (uint32_t i = 0u; i < pixelCount; ++i)
    {
        const auto pipeline = *pipelines.at(m_params.useExecutionSet ? i : 0u);
        ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        ctx.vkd.cmdDraw(cmdBuffer, 1u, 1u, i, 0u);
    }
#endif
    endRenderPass(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1));
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);
    ctx.vkd.deviceWaitIdle(ctx.device);

    // Result.
    auto &colorBufferAlloc = colorBuffer.getBufferAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, colorBufferAlloc);

    tcu::ConstPixelBufferAccess resultAccess(tcuFormat, fbExtent, colorBufferAlloc.getHostPtr());

    // Reference.
    tcu::TextureLevel referenceLevel(tcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    auto referenceAccess = referenceLevel.getAccess();
    {
        const tcu::Vec4 baseColor(0.0f, 0.0f, 0.0f, 1.0f);
        tcu::clear(referenceAccess, baseColor);

        tcu::Vec4 hitColor = baseColor;
        hitColor[hitIndex] = 1.0f;

        referenceAccess.setPixel(hitColor, static_cast<int>(hitIndex), 0, 0);
    }

    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);
    auto &log = m_context.getTestContext().getLog();
    if (!tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, threshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Unexpected results in color buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

void EarlyFragmentTestsSupport(Context &context, bool)
{
    const auto stages = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    checkDGCExtSupport(context, stages, stages); // We'll use IES.
}

void EarlyFragmentTestsPrograms(vk::SourceCollections &dst, bool)
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "void main(void) {\n"
         << "    gl_Position = inPos;\n"
         << "    gl_PointSize = 1.0f;\n"
         << "}\n";
    dst.glslSources.add("vert") << glu::VertexSource(vert.str());

    // Note the assignments to gl_FragDepth below should be ignored because we're using early fragment tests.

    std::ostringstream frag0;
    frag0 << "#version 460\n"
          << "layout (early_fragment_tests) in;\n"
          << "layout (location=0) out vec4 outColor;\n"
          << "void main(void) {\n"
          << "    gl_FragDepth = 0.25;\n"
          << "    outColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
          << "}\n";
    dst.glslSources.add("frag0") << glu::FragmentSource(frag0.str());

    std::ostringstream frag1;
    frag1 << "#version 460\n"
          << "layout (early_fragment_tests) in;\n"
          << "layout (location=0) out vec4 outColor;\n"
          << "void main(void) {\n"
          << "    gl_FragDepth = 0.125;\n"
          << "    outColor = vec4(1.0, 0.0, 1.0, 1.0);\n"
          << "}\n";
    dst.glslSources.add("frag1") << glu::FragmentSource(frag1.str());
}

tcu::TestStatus EarlyFragmentTestsRun(Context &context, bool preProcess)
{
    const auto ctx         = context.getContextCommonData();
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto depthFormat = VK_FORMAT_D16_UNORM;
    const tcu::IVec3 extent(32, 32, 1); // Small but varied selection of depths.
    const auto topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    const uint32_t seed = 1722500430u;

    // This range is different from what the fragment shaders store as gl_FragDepth, so we can tell apart the
    // fragment-shader depth and the real geometry depth.
    const float minDepth = 0.5f;
    const float maxDepth = 1.0f;

    const auto uintExtent = extent.asUint();
    const auto pixelCount = uintExtent.x() * uintExtent.y() * uintExtent.z();

    std::vector<tcu::Vec4> vertices;
    vertices.reserve(pixelCount);

    // One point per pixel.
    const auto floatExtent = extent.asFloat();
    de::Random rnd(seed);

    for (int y = 0; y < extent.y(); ++y)
        for (int x = 0; x < extent.x(); ++x)
        {
            const float xCenter = (static_cast<float>(x) + 0.5f) / floatExtent.x() * 2.0f - 1.0f;
            const float yCenter = (static_cast<float>(y) + 0.5f) / floatExtent.y() * 2.0f - 1.0f;
            const float depth   = rnd.getFloat(minDepth, maxDepth);
            vertices.emplace_back(xCenter, yCenter, depth, 1.0f);
        }

    // Vertex buffer.
    const auto vertexBufferInfo = makeBufferCreateInfo(de::dataSize(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &allocation = vertexBuffer.getAllocation();
        void *dataPtr    = allocation.getHostPtr();
        deMemcpy(dataPtr, de::dataOrNull(vertices), de::dataSize(vertices));
    }

    // Color and depth buffers.
    const auto apiExtent = makeExtent3D(extent);
    const auto imageType = VK_IMAGE_TYPE_2D;
    const auto colorUsage =
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto depthUsage = (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto colorSRR   = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto depthSRR   = makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u);
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, apiExtent, colorFormat, colorUsage, imageType,
                                colorSRR);
    ImageWithBuffer depthBuffer(ctx.vkd, ctx.device, ctx.allocator, apiExtent, depthFormat, depthUsage, imageType,
                                depthSRR);

    // Render pass and framebuffer.
    const auto renderPass = makeRenderPass(ctx.vkd, ctx.device, colorFormat, depthFormat);
    const std::vector<VkImageView> fbViews{colorBuffer.getImageView(), depthBuffer.getImageView()};
    const auto framebuffer = makeFramebuffer(ctx.vkd, ctx.device, *renderPass, de::sizeU32(fbViews),
                                             de::dataOrNull(fbViews), apiExtent.width, apiExtent.height);

    // Pipelines.
    const auto fragShaderCount = 2u; // Must match initPrograms.
    const auto &binaries       = context.getBinaryCollection();
    const auto vertModule      = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));

    std::vector<Move<VkShaderModule>> fragModules;
    fragModules.reserve(fragShaderCount);
    for (uint32_t i = 0u; i < fragShaderCount; ++i)
    {
        const auto shaderName = "frag" + std::to_string(i);
        fragModules.push_back(createShaderModule(ctx.vkd, ctx.device, binaries.get(shaderName)));
    }

    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device);
    const std::vector<VkViewport> viewports(1u, makeViewport(extent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(extent));

    std::vector<Move<VkPipeline>> pipelines;
    pipelines.reserve(fragShaderCount);

    const auto stencilOpState = makeStencilOpState(VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
                                                   VK_COMPARE_OP_ALWAYS, 0u, 0u, 0u);
    const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        nullptr,
        0u,
        VK_TRUE,
        VK_TRUE,
        VK_COMPARE_OP_GREATER,
        VK_FALSE,
        VK_FALSE,
        stencilOpState,
        stencilOpState,
        0.0f,
        1.0f,
    };

    for (uint32_t i = 0u; i < fragShaderCount; ++i)
    {
        const VkPipelineCreateFlags2CreateInfoKHR flags2 = {
            VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR,
            nullptr,
            VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT,
        };

        pipelines.push_back(makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *vertModule, VK_NULL_HANDLE,
                                                 VK_NULL_HANDLE, VK_NULL_HANDLE, *fragModules.at(i), *renderPass,
                                                 viewports, scissors, topology, 0u, 0u, nullptr, nullptr, nullptr,
                                                 &depthStencilStateCreateInfo, nullptr, nullptr, &flags2));
    }

    // IES.
    ExecutionSetManagerPtr iesManager =
        makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, *pipelines.back(), fragShaderCount);
    for (uint32_t i = 0u; i < fragShaderCount; ++i)
        iesManager->addPipeline(i, *pipelines.at(i));
    iesManager->update();

    // DGC Layout.
    const auto shaderStages = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto layoutFlags  = static_cast<VkIndirectCommandsLayoutUsageFlagsEXT>(
        preProcess ? VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_EXT : 0);
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(layoutFlags, shaderStages, VK_NULL_HANDLE /*no push constants*/);
    cmdsLayoutBuilder.addExecutionSetToken(0u, VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT, shaderStages);
    cmdsLayoutBuilder.addDrawToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // DGC sequences.
    DE_ASSERT(pixelCount % fragShaderCount == 0u);
    const auto pointsPerDraw = pixelCount / fragShaderCount;

    std::vector<uint32_t> dgcData;
    dgcData.reserve(fragShaderCount * (cmdsLayoutBuilder.getStreamStride() / DE_SIZEOF32(uint32_t)));
    for (uint32_t i = 0u; i < fragShaderCount; ++i)
    {
        dgcData.push_back(i); // Pipeline index.

        const auto firstVertex = pointsPerDraw * i;
        dgcData.push_back(pointsPerDraw); // Vertex count.
        dgcData.push_back(1u);            // Instance count.
        dgcData.push_back(firstVertex);   // First vertex.
        dgcData.push_back(0u);            // First instance.
    }

    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, de::dataSize(dgcData));
    {
        auto &allocation = dgcBuffer.getAllocation();
        void *dataPtr    = allocation.getHostPtr();
        deMemcpy(dataPtr, de::dataOrNull(dgcData), de::dataSize(dgcData));
    }

    // Preprocess buffer.
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, iesManager->get(), *cmdsLayout,
                                         fragShaderCount, 0u);

    // Generated commands info.
    DGCGenCmdsInfo cmdsInfo(shaderStages, iesManager->get(), *cmdsLayout, dgcBuffer.getDeviceAddress(),
                            dgcBuffer.getSize(), preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(),
                            fragShaderCount, 0ull, 0u);

    const CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;
    Move<VkCommandBuffer> preprocessCmdBuffer;

    if (preProcess)
    {
        preprocessCmdBuffer = allocateCommandBuffer(ctx.vkd, ctx.device, *cmd.cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        beginCommandBuffer(ctx.vkd, *preprocessCmdBuffer);
    }

    const VkDeviceSize vertexBufferOffset = 0ull;

    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    const float clearDepth = 0.0f;

    const std::vector<VkClearValue> clearValues{
        makeClearValueColor(clearColor),
        makeClearValueDepthStencil(clearDepth, 0u),
    };

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
    beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), de::sizeU32(clearValues),
                    de::dataOrNull(clearValues));
    ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelines.back());
    if (preProcess)
    {
        ctx.vkd.cmdPreprocessGeneratedCommandsEXT(*preprocessCmdBuffer, &cmdsInfo.get(), cmdBuffer);
        preprocessToExecuteBarrierExt(ctx.vkd, *preprocessCmdBuffer);
    }
    {
        const auto isPreprocessed = makeVkBool(preProcess);
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, isPreprocessed, &cmdsInfo.get());
    }
    endRenderPass(ctx.vkd, cmdBuffer);
    const auto copyExtent = extent.swizzle(0, 1);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), copyExtent,
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1u,
                      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    copyImageToBuffer(ctx.vkd, cmdBuffer, depthBuffer.getImage(), depthBuffer.getBuffer(), copyExtent,
                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                      vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1u, VK_IMAGE_ASPECT_DEPTH_BIT,
                      VK_IMAGE_ASPECT_DEPTH_BIT,
                      (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT));
    endCommandBuffer(ctx.vkd, cmdBuffer);
    if (preProcess)
        endCommandBuffer(ctx.vkd, *preprocessCmdBuffer);
    submitAndWaitWithPreprocess(ctx.vkd, ctx.device, ctx.queue, cmdBuffer, *preprocessCmdBuffer);

    const std::vector<tcu::Vec4> geomColors{tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f)};
    DE_ASSERT(fragShaderCount == de::sizeU32(geomColors));

    const auto colorTcuFormat = mapVkFormat(colorFormat);
    const auto depthTcuFormat = mapVkFormat(depthFormat);

    tcu::TextureLevel refColorLevel(colorTcuFormat, extent.x(), extent.y(), extent.z());
    tcu::TextureLevel refDepthLevel(depthTcuFormat, extent.x(), extent.y(), extent.z());

    auto refColorAccess = refColorLevel.getAccess();
    auto refDepthAccess = refDepthLevel.getAccess();

    // We must iterate in the same order we generated the points.
    for (int y = 0; y < extent.y(); ++y)
        for (int x = 0; x < extent.x(); ++x)
        {
            const auto pointIdx = static_cast<uint32_t>(y * extent.x() + x);
            const auto colorIdx = pointIdx / pointsPerDraw;
            const auto &color   = geomColors.at(colorIdx);
            const auto depth    = vertices.at(pointIdx).z();

            refColorAccess.setPixel(color, x, y);
            refDepthAccess.setPixDepth(depth, x, y);
        }

    invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());
    invalidateAlloc(ctx.vkd, ctx.device, depthBuffer.getBufferAllocation());

    const tcu::ConstPixelBufferAccess resColorAccess(colorTcuFormat, extent,
                                                     colorBuffer.getBufferAllocation().getHostPtr());
    const tcu::ConstPixelBufferAccess resDepthAccess(depthTcuFormat, extent,
                                                     depthBuffer.getBufferAllocation().getHostPtr());

    const tcu::Vec4 colorThreshold(0.0f, 0.0f, 0.0f, 0.0f);
    const float depthThreshold = 0.000025f; // 1/65535 < 0.000025 < 2/65535

    auto &log = context.getTestContext().getLog();

    if (!tcu::floatThresholdCompare(log, "ColorResult", "", refColorAccess, resColorAccess, colorThreshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Error in color buffer; check log for details");

    if (!tcu::dsThresholdCompare(log, "DepthResult", "", refDepthAccess, resDepthAccess, depthThreshold,
                                 tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Error in depth buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

class IESInputBindingsInstance : public vkt::TestInstance
{
public:
    static constexpr uint32_t kPipelineCount = 4u;

    struct Params
    {
        PipelineConstructionType constructionType;
        bool indirectVertexBuffers;
        bool reverseColorOrder;

        // Returns the binding numbers for red, green and blue.
        std::vector<uint32_t> getColorBindings(void) const
        {
            std::vector<uint32_t> colorBindings;
            colorBindings.reserve(3u); // red, green and blue.
            if (reverseColorOrder)
            {
                colorBindings.push_back(3u);
                colorBindings.push_back(2u);
                colorBindings.push_back(1u);
            }
            else
            {
                colorBindings.push_back(1u);
                colorBindings.push_back(2u);
                colorBindings.push_back(3u);
            }
            return colorBindings;
        }
    };

    IESInputBindingsInstance(Context &context, const Params &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~IESInputBindingsInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const Params m_params;
};

class IESInputBindingsCase : public vkt::TestCase
{
public:
    IESInputBindingsCase(tcu::TestContext &testCtx, const std::string &name,
                         const IESInputBindingsInstance::Params &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~IESInputBindingsCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

protected:
    const IESInputBindingsInstance::Params m_params;
};

void IESInputBindingsCase::checkSupport(Context &context) const
{
    const bool useShaderObjects = isConstructionTypeShaderObject(m_params.constructionType);
    const auto stages           = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    if (useShaderObjects)
        context.requireDeviceFunctionality("VK_EXT_shader_object");

    if (m_params.indirectVertexBuffers)
        context.requireDeviceFunctionality("VK_EXT_extended_dynamic_state");

    const auto bindStagesPipeline     = (useShaderObjects ? 0u : stages);
    const auto bindStagesShaderObject = (useShaderObjects ? stages : 0u);
    checkDGCExtSupport(context, stages, bindStagesPipeline, bindStagesShaderObject);
}

void IESInputBindingsCase::initPrograms(vk::SourceCollections &programCollection) const
{
    // These color assignments make sure each pipeline will _use_ more bindings than the previous one.
    // Note all vertex shaders declare all bindings in any case.
    // All color bindings will contain the constant value 1.0 for all vertices, modifying the color of each quadrant.
    // We'll draw a full-screen quad and use push constants to move it to each quadrant of the framebuffer.
    const std::vector<std::string> outColorAssignments{
        "    outColor = vec4(0.0, 0.0, 0.0, 1.0);\n",
        "    outColor = vec4(inRed, 0.0, 0.0, 1.0);\n",
        "    outColor = vec4(inRed, inGreen, 0.0, 1.0);\n",
        "    outColor = vec4(inRed, inGreen, inBlue, 1.0);\n",
    };

    constexpr auto kPipelineCount = IESInputBindingsInstance::kPipelineCount;
    DE_ASSERT(de::sizeU32(outColorAssignments) == kPipelineCount);

    for (uint32_t i = 0u; i < kPipelineCount; ++i)
    {
        const uint32_t kComponentCount = 3u; // Red, green and blue.
        const auto colorLocations      = m_params.getColorBindings();
        DE_ASSERT(de::sizeU32(colorLocations) == kComponentCount);
        DE_UNREF(kComponentCount); // For release builds.

        std::ostringstream vert;
        vert << "#version 460\n"
             << "layout (location=0) in vec4 inPos;\n"
             << "layout (location=" << colorLocations.at(0u) << ") in float inRed;\n"
             << "layout (location=" << colorLocations.at(1u) << ") in float inGreen;\n"
             << "layout (location=" << colorLocations.at(2u) << ") in float inBlue;\n"
             << "\n"
             << "layout (location=0) out vec4 outColor;\n"
             << "\n"
             << "layout (push_constant, std430) uniform PCBlock { vec4 offset; } pc;\n"
             << "\n"
             << "void main (void)\n"
             << "{\n"
             << "    gl_Position = inPos + pc.offset;\n"
             << outColorAssignments.at(i) << "}\n";
        const auto shaderName = "vert" + std::to_string(i);
        programCollection.glslSources.add(shaderName) << glu::VertexSource(vert.str());
    }

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) in vec4 inColor;\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "\n"
         << "void main (void)\n"
         << "{\n"
         << "    outColor = inColor;\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

TestInstance *IESInputBindingsCase::createInstance(Context &context) const
{
    return new IESInputBindingsInstance(context, m_params);
}

using BufferWithMemoryPtr = std::unique_ptr<BufferWithMemory>;

template <class T>
BufferWithMemoryPtr makeBufferFromVector(const DeviceInterface &vkd, VkDevice device, Allocator &allocator,
                                         const std::vector<T> &bufferData, VkBufferUsageFlags usage, bool indirect)
{
    const auto bufferSize  = static_cast<VkDeviceSize>(de::dataSize(bufferData));
    const auto bufferUsage = (usage | (indirect ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0));
    const auto createInfo  = makeBufferCreateInfo(bufferSize, bufferUsage);
    BufferWithMemoryPtr bufferPtr(new BufferWithMemory(
        vkd, device, allocator, createInfo,
        MemoryRequirement::HostVisible | (indirect ? MemoryRequirement::DeviceAddress : MemoryRequirement::Any)));
    auto &bufferAlloc = bufferPtr->getAllocation();
    void *dataPtr     = bufferAlloc.getHostPtr();

    deMemcpy(dataPtr, de::dataOrNull(bufferData), de::dataSize(bufferData));
    return bufferPtr;
}

template <class T>
BufferWithMemoryPtr makeVertexBuffer(const DeviceInterface &vkd, VkDevice device, Allocator &allocator,
                                     const std::vector<T> &bufferData, bool indirect)
{
    return makeBufferFromVector(vkd, device, allocator, bufferData, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, indirect);
}

template <class T>
BufferWithMemoryPtr makeIndexBuffer(const DeviceInterface &vkd, VkDevice device, Allocator &allocator,
                                    const std::vector<T> &bufferData, bool indirect)
{
    return makeBufferFromVector(vkd, device, allocator, bufferData, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indirect);
}

tcu::TestStatus IESInputBindingsInstance::iterate(void)
{
    const auto ctx = m_context.getContextCommonData();
    const tcu::IVec3 fbExtent(2, 2, 1);
    const auto apiExtent   = makeExtent3D(fbExtent);
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage =
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto shaderStages = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    // Color buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, apiExtent, colorFormat, colorUsage,
                                VK_IMAGE_TYPE_2D);

    // Render pass and framebuffer.
    RenderPassWrapper renderPass(m_params.constructionType, ctx.vkd, ctx.device, colorFormat);
    renderPass.createFramebuffer(ctx.vkd, ctx.device, colorBuffer.getImage(), colorBuffer.getImageView(),
                                 apiExtent.width, apiExtent.height);

    // Shaders.
    const auto &binaries = m_context.getBinaryCollection();

    ShaderWrapper fragShader(ctx.vkd, ctx.device, binaries.get("frag"));
    std::vector<ShaderWrapperPtr> vertShaders;
    vertShaders.reserve(kPipelineCount);
    for (uint32_t i = 0u; i < kPipelineCount; ++i)
    {
        const auto shaderName = "vert" + std::to_string(i);
        vertShaders.emplace_back(new ShaderWrapper(ctx.vkd, ctx.device, binaries.get(shaderName)));
    }

    // Push constants.
    const auto pcStages = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT);
    const auto pcSize   = DE_SIZEOF32(tcu::Vec4);
    const auto pcRange  = makePushConstantRange(pcStages, 0u, pcSize);

    // Pipeline layout.
    PipelineLayoutWrapper pipelineLayout(m_params.constructionType, ctx.vkd, ctx.device, 0u, nullptr, 1u, &pcRange);

    // Pipelines.
    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    const uint32_t kComponentCount = 3u; // Red, green and blue.
    const auto colorLocations      = m_params.getColorBindings();
    DE_ASSERT(de::sizeU32(colorLocations) == kComponentCount);
    DE_UNREF(kComponentCount); // For release builds.
    const auto &redLocation   = colorLocations.at(0u);
    const auto &greenLocation = colorLocations.at(1u);
    const auto &blueLocation  = colorLocations.at(2u);

    const std::vector<VkVertexInputBindingDescription> inputBindings{
        makeVertexInputBindingDescription(0u, DE_SIZEOF32(tcu::Vec4), VK_VERTEX_INPUT_RATE_VERTEX),      // inPos buffer
        makeVertexInputBindingDescription(redLocation, DE_SIZEOF32(float), VK_VERTEX_INPUT_RATE_VERTEX), // inRed buffer
        makeVertexInputBindingDescription(greenLocation, DE_SIZEOF32(float),
                                          VK_VERTEX_INPUT_RATE_VERTEX), // inGreen buffer
        makeVertexInputBindingDescription(blueLocation, DE_SIZEOF32(float),
                                          VK_VERTEX_INPUT_RATE_VERTEX), // inBlue buffer
    };

    const std::vector<VkVertexInputAttributeDescription> inputAttributes{
        makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u),              // inPos
        makeVertexInputAttributeDescription(redLocation, redLocation, VK_FORMAT_R32_SFLOAT, 0u),     // inRed
        makeVertexInputAttributeDescription(greenLocation, greenLocation, VK_FORMAT_R32_SFLOAT, 0u), // inGreen
        makeVertexInputAttributeDescription(blueLocation, blueLocation, VK_FORMAT_R32_SFLOAT, 0u),   // inBlue
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        nullptr,
        0u,
        de::sizeU32(inputBindings),
        de::dataOrNull(inputBindings),
        de::sizeU32(inputAttributes),
        de::dataOrNull(inputAttributes),
    };

    std::vector<VkDynamicState> dynamicStates;
    if (m_params.indirectVertexBuffers)
        dynamicStates.push_back(VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE);

    const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        nullptr,
        0u,
        de::sizeU32(dynamicStates),
        de::dataOrNull(dynamicStates),
    };

    std::vector<GraphicsPipelineWrapperPtr> pipelines;
    pipelines.reserve(kPipelineCount);
    for (uint32_t i = 0u; i < kPipelineCount; ++i)
    {
        const auto &extensions = m_context.getDeviceExtensions();
        pipelines.emplace_back(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device, extensions,
                                                           m_params.constructionType));
        auto &pipeline = *pipelines.back();
        pipeline.setPipelineCreateFlags2(VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT)
            .setShaderCreateFlags(VK_SHADER_CREATE_INDIRECT_BINDABLE_BIT_EXT)
            .setMonolithicPipelineLayout(pipelineLayout)
            .setDefaultColorBlendState()
            .setDefaultMultisampleState()
            .setDefaultRasterizationState()
            .setDefaultPatchControlPoints(0u)
            .setDefaultDepthStencilState()
            .setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
            .setDynamicState(&dynamicStateCreateInfo)
            .setupVertexInputState(&vertexInputStateCreateInfo)
            .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass.get(), 0u,
                                              *vertShaders.at(i))
            .setupFragmentShaderState(pipelineLayout, renderPass.get(), 0u, fragShader)
            .setupFragmentOutputState(renderPass.get())
            .buildPipeline();
    }

    // Indirect execution set.
    const bool useShaderObjects = isConstructionTypeShaderObject(m_params.constructionType);
    ExecutionSetManagerPtr iesManager;
    if (useShaderObjects)
    {
        // Note we will be using kPipelineCount vertex shaders, 1 fragment shader, no set layouts and the vertex
        // shaders have push constants. In the IES we'll store the fragment shader first, followed by vertex shaders.
        const std::vector<VkDescriptorSetLayout> noSetLayouts;
        const auto maxShaderCount = kPipelineCount + 1u; // vertex shaders + fragment shader.

        std::vector<IESStageInfo> stageInfos;
        IESStageInfo fragStageInfo(pipelines.at(0u)->getShader(VK_SHADER_STAGE_FRAGMENT_BIT), noSetLayouts);
        IESStageInfo vertStageInfo(pipelines.at(0u)->getShader(VK_SHADER_STAGE_VERTEX_BIT), noSetLayouts);
        stageInfos.push_back(fragStageInfo);
        stageInfos.push_back(vertStageInfo);

        const std::vector<VkPushConstantRange> pcRanges{pcRange};

        iesManager = makeExecutionSetManagerShader(ctx.vkd, ctx.device, stageInfos, pcRanges, maxShaderCount);

        // Overwrite vertex shaders only. Leave  the fragment shader alone in position 0.
        for (uint32_t i = 0u; i < kPipelineCount; ++i)
            iesManager->addShader(i + 1u, pipelines.at(i)->getShader(VK_SHADER_STAGE_VERTEX_BIT));
    }
    else
    {
        iesManager =
            makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, pipelines.at(0u)->getPipeline(), kPipelineCount);

        // Overwrite all pipelines in the set.
        for (uint32_t i = 0u; i < kPipelineCount; ++i)
            iesManager->addPipeline(i, pipelines.at(i)->getPipeline());
    }
    iesManager->update();
    const auto indirectExecutionSet = iesManager->get();

    // Vertex data and vertex buffers. A triangle strip from 0..1 will be offset in each quadrant with push constants.
    const std::vector<tcu::Vec4> vtxPositions{
        tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
        tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
    };

    const std::vector<float> vtxReds{1.0f, 1.0f, 1.0f, 1.0f};
    const std::vector<float> vtxGreens{1.0f, 1.0f, 1.0f, 1.0f};
    const std::vector<float> vtxBlues{1.0f, 1.0f, 1.0f, 1.0f};

    // Separate bad values for when the binding is unused.
    const std::vector<float> badValues{0.0f, 0.0f, 0.0f, 0.0f};

    constexpr uint32_t kVertexCount = 4u;
    DE_ASSERT(de::sizeU32(vtxPositions) == kVertexCount);
    DE_ASSERT(de::sizeU32(vtxReds) == kVertexCount);
    DE_ASSERT(de::sizeU32(vtxGreens) == kVertexCount);
    DE_ASSERT(de::sizeU32(vtxBlues) == kVertexCount);
    DE_ASSERT(de::sizeU32(badValues) == kVertexCount);

    constexpr uint32_t kVertexBufferCount = 4u;
    std::vector<BufferWithMemoryPtr> vtxBuffers;
    vtxBuffers.reserve(kVertexBufferCount);
    vtxBuffers.emplace_back(
        makeVertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vtxPositions, m_params.indirectVertexBuffers));
    vtxBuffers.emplace_back(
        makeVertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vtxReds, m_params.indirectVertexBuffers));
    vtxBuffers.emplace_back(
        makeVertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vtxGreens, m_params.indirectVertexBuffers));
    vtxBuffers.emplace_back(
        makeVertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vtxBlues, m_params.indirectVertexBuffers));

    std::vector<VkDeviceAddress> deviceAddresses;
    BufferWithMemoryPtr badVertexBuffer;
    VkDeviceAddress badVertexBufferAddress = 0ull;

    if (m_params.indirectVertexBuffers)
    {
        deviceAddresses.reserve(kVertexBufferCount);
        for (const auto &bufferPtr : vtxBuffers)
            deviceAddresses.push_back(getBufferDeviceAddress(ctx.vkd, ctx.device, bufferPtr->get(), 0ull));

        badVertexBuffer =
            makeVertexBuffer(ctx.vkd, ctx.device, ctx.allocator, badValues, m_params.indirectVertexBuffers);
        badVertexBufferAddress = getBufferDeviceAddress(ctx.vkd, ctx.device, badVertexBuffer->get(), 0ull);
    }

    // Offsets for push constants. This will determine the quadrant order: which quadrant gets which color.
    // Proceed one row at a time from top to bottom, and in each row from left to right.
    const std::vector<tcu::Vec4> offsets{
        tcu::Vec4(-1.0f, -1.0f, 0.0f, 0.0f),
        tcu::Vec4(0.0f, -1.0f, 0.0f, 0.0f),
        tcu::Vec4(-1.0f, 0.0f, 0.0f, 0.0f),
        tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
    };
    DE_ASSERT(de::sizeU32(offsets) == kPipelineCount);

    // Indirect commands layout.
    const auto setType = (useShaderObjects ? VK_INDIRECT_EXECUTION_SET_INFO_TYPE_SHADER_OBJECTS_EXT :
                                             VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT);
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, shaderStages, pipelineLayout.get());
    cmdsLayoutBuilder.addExecutionSetToken(cmdsLayoutBuilder.getStreamRange(), setType, shaderStages);
    if (m_params.indirectVertexBuffers)
    {
        for (uint32_t i = 0u; i < kVertexBufferCount; ++i)
        {
            // We may be reversing the order of the bindings here.
            const auto bindingNumber = (i == 0u ? i : colorLocations.at(i - 1u));
            cmdsLayoutBuilder.addVertexBufferToken(cmdsLayoutBuilder.getStreamRange(), bindingNumber);
        }
    }
    cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(), pcRange);
    cmdsLayoutBuilder.addDrawToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    std::vector<VkBindVertexBufferIndirectCommandEXT> vertexBindCmds;
    if (m_params.indirectVertexBuffers)
    {
        // Prepare base data for this array. The buffer addresses will be overwritten later for each sequence.
        vertexBindCmds.reserve(kVertexBufferCount);

        // The first one will always be used, so we don't have to care about the bad vertex buffer being too small for
        // this entry (size and stride is bigger in this one).
        vertexBindCmds.push_back(VkBindVertexBufferIndirectCommandEXT{badVertexBufferAddress,
                                                                      static_cast<uint32_t>(de::dataSize(vtxPositions)),
                                                                      DE_SIZEOF32(decltype(vtxPositions)::value_type)});

        vertexBindCmds.push_back(VkBindVertexBufferIndirectCommandEXT{badVertexBufferAddress,
                                                                      static_cast<uint32_t>(de::dataSize(vtxReds)),
                                                                      DE_SIZEOF32(decltype(vtxReds)::value_type)});

        vertexBindCmds.push_back(VkBindVertexBufferIndirectCommandEXT{badVertexBufferAddress,
                                                                      static_cast<uint32_t>(de::dataSize(vtxGreens)),
                                                                      DE_SIZEOF32(decltype(vtxGreens)::value_type)});

        vertexBindCmds.push_back(VkBindVertexBufferIndirectCommandEXT{badVertexBufferAddress,
                                                                      static_cast<uint32_t>(de::dataSize(vtxBlues)),
                                                                      DE_SIZEOF32(decltype(vtxBlues)::value_type)});

        DE_ASSERT(de::sizeU32(vertexBindCmds) == kVertexBufferCount);
    }

    // DGC buffer.
    const VkDrawIndirectCommand drawCmd{kVertexCount, 1u, 0u, 0u};
    const uint32_t dgcDataSize = (cmdsLayoutBuilder.getStreamStride() * kPipelineCount) / DE_SIZEOF32(uint32_t);
    std::vector<uint32_t> dgcData;
    dgcData.reserve(dgcDataSize);
    for (uint32_t i = 0u; i < kPipelineCount; ++i)
    {
        if (useShaderObjects)
        {
            dgcData.push_back(i + 1u); // Vertex shader index for sequence i.
            dgcData.push_back(0u);     // Fragment shader index is constant.
        }
        else
        {
            dgcData.push_back(i); // Pipeline index.
        }
        if (m_params.indirectVertexBuffers)
        {
            // Prepare bind commands.
            for (uint32_t j = 0u; j < i + 1u; ++j)
                vertexBindCmds.at(j).bufferAddress = deviceAddresses.at(j);
            for (uint32_t j = i + 1u; j < kVertexBufferCount; ++j)
                vertexBindCmds.at(j).bufferAddress = badVertexBufferAddress;

            // Push them to the buffer.
            for (uint32_t j = 0u; j < kVertexBufferCount; ++j)
                pushBackElement(dgcData, vertexBindCmds.at(j));
        }
        pushBackElement(dgcData, offsets.at(i)); // Push constants.
        pushBackElement(dgcData, drawCmd);       // Draw command.
    }

    const auto dgcBufferSize = static_cast<VkDeviceSize>(de::dataSize(dgcData));
    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, dgcBufferSize);
    {
        auto &alloc   = dgcBuffer.getAllocation();
        void *dataPtr = alloc.getHostPtr();
        deMemcpy(dataPtr, de::dataOrNull(dgcData), de::dataSize(dgcData));
    }

    // Preprocess buffer.
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, indirectExecutionSet, *cmdsLayout,
                                         kPipelineCount, 0u);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;
    beginCommandBuffer(ctx.vkd, cmdBuffer);

    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 0.0f); // Different from other colors due to the alpha value.
    renderPass.begin(ctx.vkd, cmdBuffer, scissors.at(0u), clearColor);

    std::vector<VkBuffer> vtxBufferHandles;
    vtxBufferHandles.reserve(vtxBuffers.size());
    std::transform(begin(vtxBuffers), end(vtxBuffers), std::back_inserter(vtxBufferHandles),
                   [](const BufferWithMemoryPtr &buf) { return buf->get(); });

    const std::vector<VkDeviceSize> vtxBufferOffsets(vtxBuffers.size(), 0ull);

    if (!m_params.indirectVertexBuffers)
    {
        std::vector<VkBuffer> actualHandles;
        actualHandles.reserve(vtxBufferHandles.size());

        for (uint32_t i = 0u; i < kVertexBufferCount; ++i)
        {
            // We may be reversing the order of the bindings here.
            const auto bindingNumber = (i == 0u ? i : colorLocations.at(i - 1u));
            actualHandles.push_back(vtxBufferHandles.at(bindingNumber));
        }

        ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, de::sizeU32(actualHandles), de::dataOrNull(actualHandles),
                                     de::dataOrNull(vtxBufferOffsets));
    }

    // Initial shader state.
    pipelines.at(0u)->bind(cmdBuffer);

    DGCGenCmdsInfo cmdsInfo(shaderStages, indirectExecutionSet, *cmdsLayout, dgcBuffer.getDeviceAddress(),
                            dgcBuffer.getSize(), preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(),
                            kPipelineCount, 0ull, 0u);
    ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());

    renderPass.end(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1));
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Result verification.
    {
        const auto tcuFormat = mapVkFormat(colorFormat);
        tcu::TextureLevel refLevel(tcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
        tcu::PixelBufferAccess refAccess = refLevel.getAccess();
        refAccess.setPixel(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), 0, 0);
        refAccess.setPixel(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f), 1, 0);
        refAccess.setPixel(tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f), 0, 1);
        refAccess.setPixel(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f), 1, 1);

        const auto bufferAlloc = colorBuffer.getBufferAllocation();
        invalidateAlloc(ctx.vkd, ctx.device, bufferAlloc);
        tcu::ConstPixelBufferAccess resAccess(tcuFormat, fbExtent, bufferAlloc.getHostPtr());

        auto &log = m_context.getTestContext().getLog();
        const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);

        if (!tcu::floatThresholdCompare(log, "Result", "", refAccess, resAccess, threshold, tcu::COMPARE_LOG_ON_ERROR))
            return tcu::TestStatus::fail("Unexpected results in color buffer; check log for details");
    }

    return tcu::TestStatus::pass("Pass");
}

// Test push constants (total or partial updates) with tessellation or geometry only. Each quadrant of the image will be
// covered with a triangle quad of a different color. Red will be fixed at 1.0 and will be updated either independently
// before executing the generated commands or as a separate push constant token inside the generated commands. Green and
// blue will vary per cuadrant using values (0,0) (0,1) (1,0) (1,1) and will always be updated with a push constant
// token.
enum class TessGeomType
{
    TESS = 0,
    GEOM
};

VkShaderStageFlags tessGeomTypeToFlags(TessGeomType type)
{
    VkShaderStageFlags stageFlags = 0u;

    if (type == TessGeomType::TESS)
        stageFlags |= (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
    else if (type == TessGeomType::GEOM)
        stageFlags |= VK_SHADER_STAGE_GEOMETRY_BIT;
    else
        DE_ASSERT(false);

    return stageFlags;
}

struct TessGeomPCParams
{
    TessGeomType type;
    bool partial; // Partial means the red value will be pushed outside execution of the indirect commands.

    bool hasTess(void) const
    {
        return (type == TessGeomType::TESS);
    }
    bool hasGeom(void) const
    {
        return (type == TessGeomType::GEOM);
    }

    VkShaderStageFlags usedStages(void) const
    {
        return (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | tessGeomTypeToFlags(type));
    }
};

void tessGeomPushConstantsCheckSupport(Context &context, TessGeomPCParams params)
{
    if (params.type == TessGeomType::TESS)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);
    else if (params.type == TessGeomType::GEOM)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
    else
        DE_ASSERT(false);

    checkDGCExtSupport(context, params.usedStages());
}

void tessGeomPushConstantsInitPrograms(vk::SourceCollections &programCollection, TessGeomPCParams params)
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "out gl_PerVertex {\n"
         << "    vec4 gl_Position;\n"
         << "};\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "void main(void) {\n"
         << "    gl_Position = inPos;\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) in vec4 inColor;\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main(void) {\n"
         << "    outColor = inColor;\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());

    std::ostringstream pcDecl;
    pcDecl << "layout (push_constant, std430) uniform PCBlock { float red; float green; float blue; } pc;\n";

    if (params.type == TessGeomType::TESS)
    {
        // Passthrough tessellation shaders.
        std::ostringstream tesc;
        tesc << "#version 460\n"
             << "#extension GL_EXT_tessellation_shader : require\n"
             << "layout(vertices=3) out;\n"
             << "in gl_PerVertex\n"
             << "{\n"
             << "    vec4 gl_Position;\n"
             << "} gl_in[gl_MaxPatchVertices];\n"
             << "out gl_PerVertex\n"
             << "{\n"
             << "    vec4 gl_Position;\n"
             << "} gl_out[];\n"
             << "void main() {\n"
             << "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
             << "    gl_TessLevelOuter[0] = 1.0;\n"
             << "    gl_TessLevelOuter[1] = 1.0;\n"
             << "    gl_TessLevelOuter[2] = 1.0;\n"
             << "    gl_TessLevelOuter[3] = 1.0;\n"
             << "    gl_TessLevelInner[0] = 1.0;\n"
             << "    gl_TessLevelInner[1] = 1.0;\n"
             << "}\n";
        programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tesc.str());

        std::ostringstream tese;
        tese << "#version 460\n"
             << "#extension GL_EXT_tessellation_shader : require\n"
             << pcDecl.str() << "layout(triangles) in;\n"
             << "in gl_PerVertex {\n"
             << "    vec4 gl_Position;\n"
             << "} gl_in[gl_MaxPatchVertices];\n"
             << "out gl_PerVertex {\n"
             << "    vec4 gl_Position;\n"
             << "};\n"
             << "layout (location=0) out vec4 outColor;\n"
             << "void main() {\n"
             << "    outColor = vec4(pc.red, pc.green, pc.blue, 1.0);\n"
             << "    gl_Position = (gl_in[0].gl_Position * gl_TessCoord.x + \n"
             << "                   gl_in[1].gl_Position * gl_TessCoord.y + \n"
             << "                   gl_in[2].gl_Position * gl_TessCoord.z);\n"
             << "}\n";
        programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(tese.str());
    }
    else if (params.type == TessGeomType::GEOM)
    {
        // Passthrough geometry shader.
        std::ostringstream geom;
        geom << "#version 460\n"
             << pcDecl.str() << "layout (triangles) in;\n"
             << "layout (triangle_strip, max_vertices=3) out;\n"
             << "in gl_PerVertex {\n"
             << "    vec4 gl_Position;\n"
             << "} gl_in[3];\n"
             << "out gl_PerVertex {\n"
             << "    vec4 gl_Position;\n"
             << "};\n"
             << "layout (location=0) out vec4 outColor;\n"
             << "void main() {\n"
             << "    for (uint i = 0; i < 3; ++i) {\n"
             << "        outColor = vec4(pc.red, pc.green, pc.blue, 1.0);\n"
             << "        gl_Position = gl_in[i].gl_Position;\n"
             << "        EmitVertex();\n"
             << "    }\n"
             << "}\n";
        programCollection.glslSources.add("geom") << glu::GeometrySource(geom.str());
    }
    else
        DE_ASSERT(false);
}

// Each quadrant will be covered by a triangle quad with a different color.
tcu::TestStatus tessGeomPushConstantsRun(Context &context, TessGeomPCParams params)
{
    const auto ctx = context.getContextCommonData();
    const tcu::IVec3 fbExtent(2, 2, 1);
    const auto apiExtent   = makeExtent3D(fbExtent);
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage =
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto colorImgType = VK_IMAGE_TYPE_2D;
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f); // Different from all quad colors below because red is zero.

    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, apiExtent, colorFormat, colorUsage, colorImgType);

    // 9 vertices which are the quadrant corners mixing values -1, 0 and 1 for each XY coord.
    const std::vector<tcu::Vec4> vertices{
        tcu::Vec4(-1.0f, -1.0, 0.0f, 1.0f), // 0
        tcu::Vec4(-1.0f, 0.0, 0.0f, 1.0f),  // 1
        tcu::Vec4(-1.0f, 1.0, 0.0f, 1.0f),  // 2
        tcu::Vec4(0.0f, -1.0, 0.0f, 1.0f),  // 3
        tcu::Vec4(0.0f, 0.0, 0.0f, 1.0f),   // 4
        tcu::Vec4(0.0f, 1.0, 0.0f, 1.0f),   // 5
        tcu::Vec4(1.0f, -1.0, 0.0f, 1.0f),  // 6
        tcu::Vec4(1.0f, 0.0, 0.0f, 1.0f),   // 7
        tcu::Vec4(1.0f, 1.0, 0.0f, 1.0f),   // 8
    };

    const std::vector<uint32_t> indices // Quads with 2 triangles.
        {
            0u, 1u, 3u, 4u, 3u, 1u, // NW
            3u, 4u, 6u, 7u, 6u, 4u, // NE
            1u, 2u, 4u, 5u, 4u, 2u, // SW
            4u, 5u, 7u, 8u, 7u, 5u, // SE
        };

    const auto vertexBufferSize  = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vertexBufferUsage = static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    const auto vertexBufferInfo  = makeBufferCreateInfo(vertexBufferSize, vertexBufferUsage);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferInfo, MemoryRequirement::HostVisible);
    deMemcpy(vertexBuffer.getAllocation().getHostPtr(), de::dataOrNull(vertices), de::dataSize(vertices));
    const VkDeviceSize vertexBufferOffset = 0ull;

    const auto indexBufferSize  = static_cast<VkDeviceSize>(de::dataSize(indices));
    const auto indexBufferUsage = static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    const auto indexBufferInfo  = makeBufferCreateInfo(indexBufferSize, indexBufferUsage);
    BufferWithMemory indexBuffer(ctx.vkd, ctx.device, ctx.allocator, indexBufferInfo, MemoryRequirement::HostVisible);
    deMemcpy(indexBuffer.getAllocation().getHostPtr(), de::dataOrNull(indices), de::dataSize(indices));

    VkShaderStageFlags pcStages = tessGeomTypeToFlags(params.type);
    const auto pcSize           = DE_SIZEOF32(float) * 3u; // red, green and blue floats
    const auto pcRange          = makePushConstantRange(pcStages, 0u, pcSize);

    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, VK_NULL_HANDLE, &pcRange);

    const auto vertexBinding =
        makeVertexInputBindingDescription(0u, DE_SIZEOF32(tcu::Vec4), VK_VERTEX_INPUT_RATE_VERTEX);
    const auto vertexAttribute = makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u);

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        nullptr,
        0u,
        1u,
        &vertexBinding,
        1u,
        &vertexAttribute,
    };

    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    const bool hasTess = params.hasTess();
    const bool hasGeom = params.hasGeom();

    const auto &binaries  = context.getBinaryCollection();
    const auto vertModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto tescModule =
        (hasTess ? createShaderModule(ctx.vkd, ctx.device, binaries.get("tesc")) : Move<VkShaderModule>());
    const auto teseModule =
        (hasTess ? createShaderModule(ctx.vkd, ctx.device, binaries.get("tese")) : Move<VkShaderModule>());
    const auto geomModule =
        (hasGeom ? createShaderModule(ctx.vkd, ctx.device, binaries.get("geom")) : Move<VkShaderModule>());
    const auto fragModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

    const auto renderPass  = makeRenderPass(ctx.vkd, ctx.device, colorFormat);
    const auto framebuffer = makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer.getImageView(),
                                             apiExtent.width, apiExtent.height);

    const auto topology           = (hasTess ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    const auto patchControlPoints = (hasTess ? 3u : 0u);

    const auto pipeline = makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *vertModule, *tescModule,
                                               *teseModule, *geomModule, *fragModule, *renderPass, viewports, scissors,
                                               topology, 0u, patchControlPoints, &vertexInputStateCreateInfo);

    // Commands layout.
    const auto kSequenceCount = 4u;
    const auto stageFlags     = params.usedStages();

    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, stageFlags, *pipelineLayout);
    if (!params.partial)
    {
        // Partial update for the red color.
        const auto redRange = makePushConstantRange(pcStages, 0u, DE_SIZEOF32(float) /*R*/);
        cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(), redRange);
    }
    {
        // Partial update for the green and blue colors.
        const auto gbRange = makePushConstantRange(pcStages, DE_SIZEOF32(float), DE_SIZEOF32(float) * 2u /*GB*/);
        cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(), gbRange);
    }
    cmdsLayoutBuilder.addDrawIndexedToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // Indirect commands.
    const auto dgcDataSize = (kSequenceCount * cmdsLayoutBuilder.getStreamStride()) / DE_SIZEOF32(uint32_t);
    std::vector<uint32_t> dgcData;
    dgcData.reserve(dgcDataSize);

    const std::vector<tcu::Vec2> greenBlue{
        tcu::Vec2(0.0f, 0.0f),
        tcu::Vec2(0.0f, 1.0f),
        tcu::Vec2(1.0f, 0.0f),
        tcu::Vec2(1.0f, 1.0f),
    };
    DE_ASSERT(kSequenceCount == de::sizeU32(greenBlue));
    const float red = 1.0f;

    const auto kVerticesPerSequence = 6u; // 2 triangles with 3 vertices each, triangle list.

    for (uint32_t i = 0u; i < kSequenceCount; ++i)
    {
        if (!params.partial)
            pushBackElement(dgcData, red);
        pushBackElement(dgcData, greenBlue.at(i));
        const VkDrawIndexedIndirectCommand drawCmd{
            kVerticesPerSequence, 1u, kVerticesPerSequence * i, 0, 0u,
        };
        pushBackElement(dgcData, drawCmd);
    }

    const auto dgcBufferSize = static_cast<VkDeviceSize>(de::dataSize(dgcData));
    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, dgcBufferSize);
    deMemcpy(dgcBuffer.getAllocation().getHostPtr(), de::dataOrNull(dgcData), de::dataSize(dgcData));

    // Preprocess buffer.
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, VK_NULL_HANDLE, *cmdsLayout,
                                         kSequenceCount, 0u, *pipeline);

    // Commands.
    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), clearColor);
    ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
    ctx.vkd.cmdBindIndexBuffer(cmdBuffer, indexBuffer.get(), 0ull, VK_INDEX_TYPE_UINT32);
    if (params.partial)
        ctx.vkd.cmdPushConstants(cmdBuffer, *pipelineLayout, pcStages, 0u, DE_SIZEOF32(float), &red);
    {
        const DGCGenCmdsInfo cmdsInfo(stageFlags, VK_NULL_HANDLE, *cmdsLayout, dgcBuffer.getDeviceAddress(),
                                      dgcBuffer.getSize(), preprocessBuffer.getDeviceAddress(),
                                      preprocessBuffer.getSize(), kSequenceCount, 0ull, 0u, *pipeline);
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());
    }
    endRenderPass(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1));
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    auto &colorBufferAlloc = colorBuffer.getBufferAllocation();
    void *colorBufferData  = colorBufferAlloc.getHostPtr();
    invalidateAlloc(ctx.vkd, ctx.device, colorBufferAlloc);

    const auto tcuFormat = mapVkFormat(colorFormat);
    tcu::ConstPixelBufferAccess result(tcuFormat, fbExtent, colorBufferData);

    tcu::TextureLevel referenceLevel(tcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    tcu::PixelBufferAccess reference = referenceLevel.getAccess();
    tcu::clear(reference, clearColor);
    {
        reference.setPixel(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f), 0, 0);
        reference.setPixel(tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f), 1, 0);
        reference.setPixel(tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f), 0, 1);
        reference.setPixel(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f), 1, 1);
    }

    auto &log = context.getTestContext().getLog();
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);

    if (!tcu::floatThresholdCompare(log, "Result", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Unexpected result in color buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

struct DrawIndexBaseInstanceParams
{
    bool countTypeToken;
};

void drawIndexBaseInstanceCheckSupport(Context &context, DrawIndexBaseInstanceParams params)
{
    const auto stageFlags = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    checkDGCExtSupport(context, stageFlags);

    if (params.countTypeToken)
    {
        const auto &dgcProperties = context.getDeviceGeneratedCommandsPropertiesEXT();
        if (!dgcProperties.deviceGeneratedCommandsMultiDrawIndirectCount)
            TCU_THROW(NotSupportedError, "deviceGeneratedCommandsMultiDrawIndirectCount not supported");
    }
}

void drawIndexBaseInstanceInitPrograms(vk::SourceCollections &programCollection, DrawIndexBaseInstanceParams)
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main (void) {\n"
         << "    gl_Position = inPos;\n"
         << "    outColor = vec4(1.0, float(gl_DrawID), float(gl_BaseInstance), 1.0);\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) in vec4 inColor;\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main (void) {\n"
         << "    outColor = inColor;\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

tcu::TestStatus drawIndexBaseInstanceRun(Context &context, DrawIndexBaseInstanceParams params)
{
    const auto ctx = context.getContextCommonData();
    const tcu::IVec3 fbExtent(2, 2, 1);
    const auto apiExtent   = makeExtent3D(fbExtent);
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage =
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto colorImgType = VK_IMAGE_TYPE_2D;
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f); // Different from all quad colors below because red is zero.
    const auto stageFlags = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, apiExtent, colorFormat, colorUsage, colorImgType);

    // 9 vertices which are the quadrant corners mixing values -1, 0 and 1 for each XY coord.
    const std::vector<tcu::Vec4> vertices{
        tcu::Vec4(-1.0f, -1.0, 0.0f, 1.0f), // 0
        tcu::Vec4(-1.0f, 0.0, 0.0f, 1.0f),  // 1
        tcu::Vec4(-1.0f, 1.0, 0.0f, 1.0f),  // 2
        tcu::Vec4(0.0f, -1.0, 0.0f, 1.0f),  // 3
        tcu::Vec4(0.0f, 0.0, 0.0f, 1.0f),   // 4
        tcu::Vec4(0.0f, 1.0, 0.0f, 1.0f),   // 5
        tcu::Vec4(1.0f, -1.0, 0.0f, 1.0f),  // 6
        tcu::Vec4(1.0f, 0.0, 0.0f, 1.0f),   // 7
        tcu::Vec4(1.0f, 1.0, 0.0f, 1.0f),   // 8
    };

    const std::vector<uint32_t> indices // Quads with 2 triangles.
        {
            0u, 1u, 3u, 4u, // NW
            3u, 4u, 6u, 7u, // NE
            1u, 2u, 4u, 5u, // SW
            4u, 5u, 7u, 8u, // SE
        };

    const auto vertexBufferSize  = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vertexBufferUsage = static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    const auto vertexBufferInfo  = makeBufferCreateInfo(vertexBufferSize, vertexBufferUsage);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferInfo, MemoryRequirement::HostVisible);
    deMemcpy(vertexBuffer.getAllocation().getHostPtr(), de::dataOrNull(vertices), de::dataSize(vertices));
    const VkDeviceSize vertexBufferOffset = 0ull;

    const auto indexBufferSize  = static_cast<VkDeviceSize>(de::dataSize(indices));
    const auto indexBufferUsage = static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    const auto indexBufferInfo  = makeBufferCreateInfo(indexBufferSize, indexBufferUsage);
    BufferWithMemory indexBuffer(ctx.vkd, ctx.device, ctx.allocator, indexBufferInfo, MemoryRequirement::HostVisible);
    deMemcpy(indexBuffer.getAllocation().getHostPtr(), de::dataOrNull(indices), de::dataSize(indices));

    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device);

    const auto vertexBinding =
        makeVertexInputBindingDescription(0u, DE_SIZEOF32(tcu::Vec4), VK_VERTEX_INPUT_RATE_VERTEX);
    const auto vertexAttribute = makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u);

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        nullptr,
        0u,
        1u,
        &vertexBinding,
        1u,
        &vertexAttribute,
    };

    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    const auto &binaries  = context.getBinaryCollection();
    const auto vertModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

    const auto renderPass  = makeRenderPass(ctx.vkd, ctx.device, colorFormat);
    const auto framebuffer = makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer.getImageView(),
                                             apiExtent.width, apiExtent.height);

    const auto topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    const auto pipeline = makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *vertModule, VK_NULL_HANDLE,
                                               VK_NULL_HANDLE, VK_NULL_HANDLE, *fragModule, *renderPass, viewports,
                                               scissors, topology, 0u, 0u, &vertexInputStateCreateInfo);

    // Commands layout.
    const auto kSequenceCount    = (params.countTypeToken ? 2u : 4u);
    const auto kDrawsPerSequence = (params.countTypeToken ? 2u : 1u);
    const auto kVerticesPerDraw  = 4u;
    const auto kMaxDrawCount     = kDrawsPerSequence;

    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, stageFlags, *pipelineLayout);
    if (params.countTypeToken)
        cmdsLayoutBuilder.addDrawIndexedCountToken(0u);
    else
        cmdsLayoutBuilder.addDrawIndexedToken(0u);
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // Draw commands. Each of these draws 4 vertices and 1 instance, but the base vertex and instance varies per draw.
    const std::vector<VkDrawIndexedIndirectCommand> drawCmds{
        {kVerticesPerDraw, 1u, kVerticesPerDraw * 0u, 0, 0u},
        {kVerticesPerDraw, 1u, kVerticesPerDraw * 1u, 0, 0u},
        {kVerticesPerDraw, 1u, kVerticesPerDraw * 2u, 0, 1u},
        {kVerticesPerDraw, 1u, kVerticesPerDraw * 3u, 0, 1u},
    };

    // Store them in a DGC buffer (indirect + device address).
    const auto drawCmdsBufferSize = static_cast<VkDeviceSize>(de::dataSize(drawCmds));
    DGCBuffer drawCmdsBuffer(ctx.vkd, ctx.device, ctx.allocator, drawCmdsBufferSize);
    deMemcpy(drawCmdsBuffer.getAllocation().getHostPtr(), de::dataOrNull(drawCmds), de::dataSize(drawCmds));

    const auto drawCmdsAddress = drawCmdsBuffer.getDeviceAddress();
    const auto drawCmdSize     = DE_SIZEOF32(VkDrawIndexedIndirectCommand);

    // Indirect draw commands. Each of these dispatches 2 of the draws above.
    const std::vector<VkDrawIndirectCountIndirectCommandEXT> indirectDrawCmds{
        {drawCmdsAddress + drawCmdSize * kDrawsPerSequence * 0u, drawCmdSize, kDrawsPerSequence},
        {drawCmdsAddress + drawCmdSize * kDrawsPerSequence * 1u, drawCmdSize, kDrawsPerSequence},
    };

    const auto dgcBufferSize = static_cast<VkDeviceSize>(de::dataSize(indirectDrawCmds));
    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, dgcBufferSize);
    deMemcpy(dgcBuffer.getAllocation().getHostPtr(), de::dataOrNull(indirectDrawCmds), de::dataSize(indirectDrawCmds));

    // Preprocess buffer.
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, VK_NULL_HANDLE, *cmdsLayout,
                                         kSequenceCount, kMaxDrawCount, *pipeline);

    // Commands.
    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), clearColor);
    ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
    ctx.vkd.cmdBindIndexBuffer(cmdBuffer, indexBuffer.get(), 0ull, VK_INDEX_TYPE_UINT32);
    {
        const auto dgcAddress = (params.countTypeToken ? dgcBuffer.getDeviceAddress() : drawCmdsAddress);
        const auto dgcSize    = (params.countTypeToken ? dgcBuffer.getSize() : drawCmdsBuffer.getSize());

        const DGCGenCmdsInfo cmdsInfo(stageFlags, VK_NULL_HANDLE, *cmdsLayout, dgcAddress, dgcSize,
                                      preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(), kSequenceCount,
                                      0ull, kMaxDrawCount, *pipeline);
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());
    }
    endRenderPass(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1));
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    auto &colorBufferAlloc = colorBuffer.getBufferAllocation();
    void *colorBufferData  = colorBufferAlloc.getHostPtr();
    invalidateAlloc(ctx.vkd, ctx.device, colorBufferAlloc);

    const auto tcuFormat = mapVkFormat(colorFormat);
    tcu::ConstPixelBufferAccess result(tcuFormat, fbExtent, colorBufferData);

    tcu::TextureLevel referenceLevel(tcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    tcu::PixelBufferAccess reference = referenceLevel.getAccess();
    tcu::clear(reference, clearColor);
    {
        const float altGreen = (params.countTypeToken ? 1.0f : 0.0f); // For non-count tokens, draw id is always zero.

        reference.setPixel(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f), 0, 0);
        reference.setPixel(tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f), 0, 1);
        reference.setPixel(tcu::Vec4(1.0f, altGreen, 0.0f, 1.0f), 1, 0);
        reference.setPixel(tcu::Vec4(1.0f, altGreen, 1.0f, 1.0f), 1, 1);
    }

    auto &log = context.getTestContext().getLog();
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);

    if (!tcu::floatThresholdCompare(log, "Result", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Unexpected result in color buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

void sparseVBOCheckSupport(Context &context)
{
    const auto stageFlags = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    checkDGCExtSupport(context, stageFlags);

    context.requireDeviceFunctionality("VK_EXT_extended_dynamic_state");

    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_BINDING);
}

void sparseVBOInitPrograms(vk::SourceCollections &programCollection)
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "void main (void) {\n"
         << "    gl_Position = inPos;\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main (void) {\n"
         << "    outColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

tcu::TestStatus sparseVBORun(Context &context)
{
    const auto ctx = context.getContextCommonData();
    const tcu::IVec3 fbExtent(2, 2, 1);
    const auto apiExtent   = makeExtent3D(fbExtent);
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage =
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto colorImgType = VK_IMAGE_TYPE_2D;
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f); // Different from the frag shader color.
    const auto stageFlags = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, apiExtent, colorFormat, colorUsage, colorImgType);

    // Full-screen quad.
    const std::vector<tcu::Vec4> vertices{
        tcu::Vec4(-1.0f, -1.0, 0.0f, 1.0f),
        tcu::Vec4(-1.0f, 1.0, 0.0f, 1.0f),
        tcu::Vec4(1.0f, -1.0, 0.0f, 1.0f),
        tcu::Vec4(1.0f, 1.0, 0.0f, 1.0f),
    };

    // We will bind memory and transfer vertex data to this buffer later.
    const auto vertexBufferSize = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vertexBufferUsage =
        static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                        VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    auto vertexBufferInfo              = makeBufferCreateInfo(vertexBufferSize, vertexBufferUsage);
    vertexBufferInfo.flags             = VK_BUFFER_CREATE_SPARSE_BINDING_BIT;
    const auto vertexBuffer            = makeBuffer(ctx.vkd, ctx.device, vertexBufferInfo);
    const auto vertexBufferMemReqFlags = MemoryRequirement::DeviceAddress;
    const auto vertexBufferMemReqs     = getBufferMemoryRequirements(ctx.vkd, ctx.device, *vertexBuffer);
    const auto vertexBufferAlloc       = ctx.allocator.allocate(vertexBufferMemReqs, vertexBufferMemReqFlags);

    const auto xferBufferUsage   = static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    const auto xferBufferInfo    = makeBufferCreateInfo(vertexBufferSize, xferBufferUsage);
    const auto xferBufferMemReqs = MemoryRequirement::HostVisible;
    BufferWithMemory xferBuffer(ctx.vkd, ctx.device, ctx.allocator, xferBufferInfo, xferBufferMemReqs);
    deMemcpy(xferBuffer.getAllocation().getHostPtr(), de::dataOrNull(vertices), de::dataSize(vertices));

    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device);

    const auto vertexBinding =
        makeVertexInputBindingDescription(0u, DE_SIZEOF32(tcu::Vec4), VK_VERTEX_INPUT_RATE_VERTEX);
    const auto vertexAttribute = makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u);

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        nullptr,
        0u,
        1u,
        &vertexBinding,
        1u,
        &vertexAttribute,
    };

    const std::vector<VkDynamicState> dynamicStates{VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT};

    const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        nullptr,
        0u,
        de::sizeU32(dynamicStates),
        de::dataOrNull(dynamicStates),
    };

    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    const auto &binaries  = context.getBinaryCollection();
    const auto vertModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

    const auto renderPass  = makeRenderPass(ctx.vkd, ctx.device, colorFormat);
    const auto framebuffer = makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer.getImageView(),
                                             apiExtent.width, apiExtent.height);

    const auto topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    const auto pipeline =
        makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *vertModule, VK_NULL_HANDLE, VK_NULL_HANDLE,
                             VK_NULL_HANDLE, *fragModule, *renderPass, viewports, scissors, topology, 0u, 0u,
                             &vertexInputStateCreateInfo, nullptr, nullptr, nullptr, nullptr, &dynamicStateCreateInfo);

    // Commands layout.
    const auto kSequenceCount = 1u;

    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, stageFlags, *pipelineLayout);
    cmdsLayoutBuilder.addVertexBufferToken(cmdsLayoutBuilder.getStreamRange(), 0u);
    cmdsLayoutBuilder.addDrawToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    std::vector<uint32_t> dgcData;
    const auto dgcDataSize = (kSequenceCount * cmdsLayoutBuilder.getStreamStride()) / DE_SIZEOF32(uint32_t);
    dgcData.reserve(dgcDataSize);
    {
        const auto vboAddress = getBufferDeviceAddress(ctx.vkd, ctx.device, vertexBuffer.get(), 0ull);
        const VkBindVertexBufferIndirectCommandEXT bindCmd{
            vboAddress,
            static_cast<uint32_t>(de::dataSize(vertices)),
            DE_SIZEOF32(tcu::Vec4),
        };
        pushBackElement(dgcData, bindCmd);
    }
    {
        const VkDrawIndirectCommand drawCmd{de::sizeU32(vertices), 1u, 0u, 0u};
        pushBackElement(dgcData, drawCmd);
    }
    const auto dgcBufferSize = static_cast<VkDeviceSize>(de::dataSize(dgcData));
    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, dgcBufferSize);
    deMemcpy(dgcBuffer.getAllocation().getHostPtr(), de::dataOrNull(dgcData), de::dataSize(dgcData));

    // Preprocess buffer.
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, VK_NULL_HANDLE, *cmdsLayout,
                                         kSequenceCount, 0u, *pipeline);

    // Commands.
    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    {
        const VkBufferCopy copyRegion = {0ull, 0ull, vertexBufferSize};
        ctx.vkd.cmdCopyBuffer(cmdBuffer, xferBuffer.get(), vertexBuffer.get(), 1u, &copyRegion);
    }
    beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), clearColor);
    ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
    {
        const DGCGenCmdsInfo cmdsInfo(stageFlags, VK_NULL_HANDLE, *cmdsLayout, dgcBuffer.getDeviceAddress(),
                                      dgcBuffer.getSize(), preprocessBuffer.getDeviceAddress(),
                                      preprocessBuffer.getSize(), kSequenceCount, 0ull, 0u, *pipeline);
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());
    }
    endRenderPass(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1));
    endCommandBuffer(ctx.vkd, cmdBuffer);

    const auto sparseQueue   = context.getSparseQueue();
    const auto bindSemaphore = createSemaphore(ctx.vkd, ctx.device);

    const VkSparseMemoryBind sparseMemoryBind = {
        0ull, vertexBufferMemReqs.size, vertexBufferAlloc->getMemory(), 0ull, 0u,
    };
    const VkSparseBufferMemoryBindInfo bufferBind = {
        vertexBuffer.get(),
        1u,
        &sparseMemoryBind,
    };
    const VkBindSparseInfo bindInfo = {
        VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,
        nullptr,
        0u,
        nullptr,
        1u,
        &bufferBind,
        0u,
        nullptr,
        0u,
        nullptr,
        1u,
        &bindSemaphore.get(),
    };

    // Bind sparse buffer memory.
    ctx.vkd.queueBindSparse(sparseQueue, 1u, &bindInfo, VK_NULL_HANDLE);

    // Start running the command buffer waiting on the transfer operation.
    {
        const auto waitStages = static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_TRANSFER_BIT);
        submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer, false, 1u,
                              1u,                   // Wait on one semaphore
                              &bindSemaphore.get(), // Wait on this one
                              &waitStages,          // Wait on this stage
                              0u, nullptr);
    }

    auto &colorBufferAlloc = colorBuffer.getBufferAllocation();
    void *colorBufferData  = colorBufferAlloc.getHostPtr();
    invalidateAlloc(ctx.vkd, ctx.device, colorBufferAlloc);

    const auto tcuFormat = mapVkFormat(colorFormat);
    tcu::ConstPixelBufferAccess result(tcuFormat, fbExtent, colorBufferData);

    tcu::TextureLevel referenceLevel(tcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    tcu::PixelBufferAccess reference = referenceLevel.getAccess();
    const tcu::Vec4 geomColor(0.0f, 0.0f, 1.0f, 1.0f); // Must match fragment shader.
    tcu::clear(reference, geomColor);

    auto &log = context.getTestContext().getLog();
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);

    if (!tcu::floatThresholdCompare(log, "Result", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Unexpected result in color buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

// Check dynamic vertex input combined with DGC.
// Idea: 2x2 framebuffer, 2 execute indirect commands, with 2 different vertex input states.
// The first execute indirect will draw twice, once for each pixel in the top row.
// The second execute indirect will do the same with the bottom row.
// All pipelines used will read a vec4 for the position and a vec4 for the color from VBO buffers.
// Those would be locations 0 and 1 in the vertex shader.
// When using indirect execution sets, the variation will come from the frag shader, which may reverse component order.
// The key, in any case, is changing state between both execute indirect calls.
// In one of the cases, there will only be a single binding with two attributes like this:
//   ZZZZ POSITION ZZZZ COLOR
// While in the other case, there will be 2 bindings with separate attributes like this:
//   ZZZZ ZZZZ COLOR
//   ZZZZ ZZZZ ZZZZ ZZZZ POSITION
class DynVtxInputInstance : public vkt::TestInstance
{
public:
    struct Params
    {
        PipelineConstructionType constructionType;
        bool useExecutionSet;

        bool useShaderObjects() const
        {
            return isConstructionTypeShaderObject(constructionType);
        }
    };

    static constexpr uint32_t kFragShaderCount = 2u;

    DynVtxInputInstance(Context &context, const Params &params) : vkt::TestInstance(context), m_params(params)
    {
    }

    ~DynVtxInputInstance() = default;

    tcu::TestStatus iterate() override;

protected:
    const Params m_params;
};

class DynVtxInputCase : public vkt::TestCase
{
public:
    DynVtxInputCase(tcu::TestContext &testCtx, const std::string &name, const DynVtxInputInstance::Params &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~DynVtxInputCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

protected:
    const DynVtxInputInstance::Params m_params;
};

void DynVtxInputCase::checkSupport(Context &context) const
{
    const auto stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto bindStageFlags         = (m_params.useExecutionSet ? stageFlags : 0u);
    const bool useShaderObjects       = m_params.useShaderObjects();
    const auto bindStagesPipeline     = (useShaderObjects ? 0u : bindStageFlags);
    const auto bindStagesShaderObject = (useShaderObjects ? bindStageFlags : 0u);

    checkDGCExtSupport(context, stageFlags, bindStagesPipeline, bindStagesShaderObject);

    if (useShaderObjects)
    {
        // With shader objects everything is dynamic.
        context.requireDeviceFunctionality("VK_EXT_shader_object");
    }
    else
        context.requireDeviceFunctionality("VK_EXT_vertex_input_dynamic_state");
}

void DynVtxInputCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "layout (location=1) in vec4 inColor;\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main (void) {\n"
         << "    gl_Position = inPos;\n"
         << "    gl_PointSize = 1.0f;\n"
         << "    outColor = inColor;\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    constexpr auto kFragShaderCount = DynVtxInputInstance::kFragShaderCount;

    for (uint32_t i = 0u; i < kFragShaderCount; ++i)
    {
        const bool reverse           = (i > 0u);
        const std::string finalColor = (reverse ? "inColor.abgr" : "inColor.rgba");

        std::ostringstream frag;
        frag << "#version 460\n"
             << "layout (location=0) in vec4 inColor;\n"
             << "layout (location=0) out vec4 outColor;\n"
             << "void main (void) {\n"
             << "    outColor = " << finalColor << ";\n"
             << "}\n";
        const auto testName = "frag" + std::to_string(i);
        programCollection.glslSources.add(testName) << glu::FragmentSource(frag.str());
    }
}

TestInstance *DynVtxInputCase::createInstance(Context &context) const
{
    return new DynVtxInputInstance(context, m_params);
}

tcu::TestStatus DynVtxInputInstance::iterate()
{
    const auto ctx = m_context.getContextCommonData();
    const tcu::IVec3 fbExtent(2, 2, 1);
    const auto fbExtentU = fbExtent.asUint();
    const auto apiExtent = makeExtent3D(fbExtent);
    const auto shaderStages =
        static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    // Create data for the 4 quadrants.

    // This will be used for the top row.
    struct PositionColor
    {
        tcu::Vec4 padding0;
        tcu::Vec4 position;
        tcu::Vec4 padding1;
        tcu::Vec4 color;

        PositionColor(const tcu::Vec4 &position_, const tcu::Vec4 &color_)
            : padding0(0.0f, 0.0f, 0.0f, 0.0f)
            , position(position_)
            , padding1(0.0f, 0.0f, 0.0f, 0.0f)
            , color(color_)
        {
        }
    };

    // One triangle strip per pixel.
    constexpr uint32_t kVerticesPerPixel = 4u;
    const uint32_t kElementsPerRow       = kVerticesPerPixel * fbExtentU.x();

    // Top row data.
    std::vector<PositionColor> topRowData;
    topRowData.reserve(kElementsPerRow);

    // NW
    topRowData.emplace_back(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
    topRowData.emplace_back(tcu::Vec4(-1.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
    topRowData.emplace_back(tcu::Vec4(0.0f, -1.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
    topRowData.emplace_back(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));

    // NE
    topRowData.emplace_back(tcu::Vec4(0.0f, -1.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f));
    topRowData.emplace_back(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f));
    topRowData.emplace_back(tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f));
    topRowData.emplace_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f));

    // The bottom row will use separate bindings for color and position.
    struct SeparateColor
    {
        tcu::Vec4 padding0;
        tcu::Vec4 padding1;
        tcu::Vec4 color;

        SeparateColor(const tcu::Vec4 &color_)
            : padding0(0.0f, 0.0f, 0.0f, 0.0f)
            , padding1(0.0f, 0.0f, 0.0f, 0.0f)
            , color(color_)
        {
        }
    };

    struct SeparatePosition
    {
        tcu::Vec4 padding0;
        tcu::Vec4 padding1;
        tcu::Vec4 padding2;
        tcu::Vec4 padding3;
        tcu::Vec4 position;

        SeparatePosition(const tcu::Vec4 &position_)
            : padding0(0.0f, 0.0f, 0.0f, 0.0f)
            , padding1(0.0f, 0.0f, 0.0f, 0.0f)
            , padding2(0.0f, 0.0f, 0.0f, 0.0f)
            , padding3(0.0f, 0.0f, 0.0f, 0.0f)
            , position(position_)
        {
        }
    };

    std::vector<SeparateColor> bottomRowColor;
    bottomRowColor.reserve(kElementsPerRow);

    // SW
    bottomRowColor.emplace_back(tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f));
    bottomRowColor.emplace_back(tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f));
    bottomRowColor.emplace_back(tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f));
    bottomRowColor.emplace_back(tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f));

    // SE
    bottomRowColor.emplace_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    bottomRowColor.emplace_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    bottomRowColor.emplace_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    bottomRowColor.emplace_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

    std::vector<SeparatePosition> bottomRowPosition;
    bottomRowPosition.reserve(kElementsPerRow);

    // SW
    bottomRowPosition.emplace_back(tcu::Vec4(-1.0f, 0.0f, 0.0f, 1.0f));
    bottomRowPosition.emplace_back(tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f));
    bottomRowPosition.emplace_back(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    bottomRowPosition.emplace_back(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));

    // SE
    bottomRowPosition.emplace_back(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    bottomRowPosition.emplace_back(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));
    bottomRowPosition.emplace_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
    bottomRowPosition.emplace_back(tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f));

    // Prepare data for those 3 buffers.
    auto topRowBuffer            = makeVertexBuffer(ctx.vkd, ctx.device, ctx.allocator, topRowData, true);
    auto bottomRowColorBuffer    = makeVertexBuffer(ctx.vkd, ctx.device, ctx.allocator, bottomRowColor, true);
    auto bottomRowPositionBuffer = makeVertexBuffer(ctx.vkd, ctx.device, ctx.allocator, bottomRowPosition, true);

    // Color buffer.
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage =
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, apiExtent, colorFormat, colorUsage,
                                VK_IMAGE_TYPE_2D);

    PipelineLayoutWrapper pipelineLayout(m_params.constructionType, ctx.vkd, ctx.device);

    // Indirect commands layouts.
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, shaderStages, pipelineLayout.get());
    if (m_params.useExecutionSet)
    {
        const auto iesType = (m_params.useShaderObjects() ? VK_INDIRECT_EXECUTION_SET_INFO_TYPE_SHADER_OBJECTS_EXT :
                                                            VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT);

        cmdsLayoutBuilder.addExecutionSetToken(0u, iesType, shaderStages);
    }
    cmdsLayoutBuilder.addVertexBufferToken(cmdsLayoutBuilder.getStreamRange(), 0u);
    // Note binding 1 in the top row will be the null address.
    cmdsLayoutBuilder.addVertexBufferToken(cmdsLayoutBuilder.getStreamRange(), 1u);
    cmdsLayoutBuilder.addDrawToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // Pipelines.
    const uint32_t pipelineCount = (m_params.useExecutionSet ? 2u : 1u);
    std::vector<GraphicsPipelineWrapperPtr> pipelines;
    pipelines.reserve(pipelineCount);

    const std::vector<VkDynamicState> dynamicStates{
        VK_DYNAMIC_STATE_VERTEX_INPUT_EXT,
        VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT,
    };

    const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        nullptr,
        0u,
        de::sizeU32(dynamicStates),
        de::dataOrNull(dynamicStates),
    };

    // Placeholder. The state is dynamic.
    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();

    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    RenderPassWrapper renderPass(m_params.constructionType, ctx.vkd, ctx.device, colorFormat);
    renderPass.createFramebuffer(ctx.vkd, ctx.device, colorBuffer.getImage(), colorBuffer.getImageView(),
                                 apiExtent.width, apiExtent.height);

    const auto &binaries = m_context.getBinaryCollection();
    ShaderWrapper vertShader(ctx.vkd, ctx.device, binaries.get("vert"));
    std::vector<ShaderWrapperPtr> fragShaders;
    fragShaders.reserve(pipelineCount);
    DE_ASSERT(pipelineCount <= kFragShaderCount);

    for (uint32_t i = 0u; i < pipelineCount; ++i)
    {
        const auto shaderName = "frag" + std::to_string(i);
        fragShaders.emplace_back(new ShaderWrapper(ctx.vkd, ctx.device, binaries.get(shaderName)));
    }

    for (uint32_t i = 0u; i < pipelineCount; ++i)
    {
        pipelines.emplace_back(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                                           m_context.getDeviceExtensions(), m_params.constructionType));
        auto &pipeline = *pipelines.back();

        if (m_params.useExecutionSet)
        {
            pipeline.setPipelineCreateFlags2(VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT);
            pipeline.setShaderCreateFlags(VK_SHADER_CREATE_INDIRECT_BINDABLE_BIT_EXT);
        }

        pipeline.setMonolithicPipelineLayout(pipelineLayout)
            .setDefaultColorBlendState()
            .setDefaultMultisampleState()
            .setDefaultRasterizationState()
            .setDefaultPatchControlPoints(0u)
            .setDefaultDepthStencilState()
            .setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
            .setDynamicState(&dynamicStateCreateInfo)
            .setupVertexInputState(&vertexInputStateCreateInfo)
            .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass.get(), 0u, vertShader)
            .setupFragmentShaderState(pipelineLayout, renderPass.get(), 0u, *fragShaders.at(i))
            .setupFragmentOutputState(renderPass.get())
            .buildPipeline();
    }

    // Indirect execution set if used.
    VkIndirectExecutionSetEXT iesHandle = VK_NULL_HANDLE;
    ExecutionSetManagerPtr iesManager;

    if (m_params.useExecutionSet)
    {
        if (m_params.useShaderObjects())
        {
            // The vertex shader will be fixed at position 0 and the fragment shaders will follow.
            const std::vector<VkDescriptorSetLayout> noLayouts;
            const std::vector<VkPushConstantRange> noPCRanges;

            const std::vector<IESStageInfo> stageInfos{
                IESStageInfo(pipelines.at(0u)->getShader(VK_SHADER_STAGE_VERTEX_BIT), noLayouts),
                IESStageInfo(pipelines.at(0u)->getShader(VK_SHADER_STAGE_FRAGMENT_BIT), noLayouts),
            };

            iesManager = makeExecutionSetManagerShader(ctx.vkd, ctx.device, stageInfos, noPCRanges,
                                                       pipelineCount /* fragment shaders */ + 1u /* vertex shader */);
            for (uint32_t i = 0u; i < pipelineCount; ++i)
                iesManager->addShader(i + 1u, pipelines.at(i)->getShader(VK_SHADER_STAGE_FRAGMENT_BIT));
        }
        else
        {
            iesManager =
                makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, pipelines.at(0u)->getPipeline(), pipelineCount);
            for (uint32_t i = 0u; i < pipelineCount; ++i)
                iesManager->addPipeline(i, pipelines.at(i)->getPipeline());
        }

        iesManager->update();
        iesHandle = iesManager->get();
    }

    // Two indirect execution buffers with different contents.
    const uint32_t kSequenceCount = apiExtent.width; // One draw per pixel in each row.
    const uint32_t itemCount      = (cmdsLayoutBuilder.getStreamStride() * kSequenceCount) / DE_SIZEOF32(uint32_t);

    const std::vector<VkBindVertexBufferIndirectCommandEXT> topRowBindingCmds{
        VkBindVertexBufferIndirectCommandEXT{
            getBufferDeviceAddress(ctx.vkd, ctx.device, topRowBuffer->get()),
            static_cast<uint32_t>(de::dataSize(topRowData)),
            DE_SIZEOF32(PositionColor),
        },
        VkBindVertexBufferIndirectCommandEXT{
            0ull,
            0u,
            0u,
        },
    };

    const std::vector<VkBindVertexBufferIndirectCommandEXT> bottomRowBindingCmds{
        VkBindVertexBufferIndirectCommandEXT{
            getBufferDeviceAddress(ctx.vkd, ctx.device, bottomRowColorBuffer->get()),
            static_cast<uint32_t>(de::dataSize(bottomRowColor)),
            DE_SIZEOF32(SeparateColor),
        },
        VkBindVertexBufferIndirectCommandEXT{
            getBufferDeviceAddress(ctx.vkd, ctx.device, bottomRowPositionBuffer->get()),
            static_cast<uint32_t>(de::dataSize(bottomRowPosition)),
            DE_SIZEOF32(SeparatePosition),
        },
    };

    std::vector<uint32_t> topRowDGCData;
    std::vector<uint32_t> bottomRowDGCData;

    const std::vector<std::vector<uint32_t> *> dgcDataVectors{&topRowDGCData, &bottomRowDGCData};
    const std::vector<const std::vector<VkBindVertexBufferIndirectCommandEXT> *> dgcBindingCmdsVec{
        &topRowBindingCmds, &bottomRowBindingCmds};

    DE_ASSERT(de::sizeU32(dgcDataVectors) == de::sizeU32(dgcBindingCmdsVec));

    // For each row.
    for (uint32_t rowIdx = 0u; rowIdx < de::sizeU32(dgcDataVectors); ++rowIdx)
    {
        auto &dataPtr        = dgcDataVectors.at(rowIdx);
        auto &bindingCmdsPtr = dgcBindingCmdsVec.at(rowIdx);

        dataPtr->reserve(itemCount);

        // For each pixel in each row (one sequence per pixel).
        for (uint32_t i = 0u; i < kSequenceCount; ++i)
        {
            if (m_params.useExecutionSet)
            {
                // Same execution set items for both rows.
                if (m_params.useShaderObjects())
                {
                    dataPtr->push_back(0u);     // Vert shader index.
                    dataPtr->push_back(i + 1u); // Frag shader index.
                }
                else
                {
                    dataPtr->push_back(i);
                }
            }

            // Same binding cmds for both sequences.
            for (const auto &bindingCmd : *bindingCmdsPtr)
                pushBackElement(*dataPtr, bindingCmd);

            // Same draw commands for both rows.
            const VkDrawIndirectCommand drawCmd{
                kVerticesPerPixel,
                1u,
                kVerticesPerPixel * i,
                0u,
            };
            pushBackElement(*dataPtr, drawCmd);
        }
    }

    DGCBuffer topRowDGCBuffer(ctx.vkd, ctx.device, ctx.allocator,
                              static_cast<VkDeviceSize>(de::dataSize(topRowDGCData)));
    DGCBuffer bottomRowDGCBuffer(ctx.vkd, ctx.device, ctx.allocator,
                                 static_cast<VkDeviceSize>(de::dataSize(bottomRowDGCData)));

    deMemcpy(topRowDGCBuffer.getAllocation().getHostPtr(), de::dataOrNull(topRowDGCData), de::dataSize(topRowDGCData));
    deMemcpy(bottomRowDGCBuffer.getAllocation().getHostPtr(), de::dataOrNull(bottomRowDGCData),
             de::dataSize(bottomRowDGCData));

    const auto preprocessPipeline =
        ((!m_params.useExecutionSet && !m_params.useShaderObjects()) ? pipelines.at(0u)->getPipeline() :
                                                                       VK_NULL_HANDLE);
    const auto preprocessShaders =
        ((!m_params.useExecutionSet && m_params.useShaderObjects()) ?
             std::vector<VkShaderEXT>{pipelines.at(0u)->getShader(VK_SHADER_STAGE_VERTEX_BIT),
                                      pipelines.at(0u)->getShader(VK_SHADER_STAGE_FRAGMENT_BIT)} :
             std::vector<VkShaderEXT>());
    const auto preprocessShaderVec =
        ((m_params.useExecutionSet || !m_params.useShaderObjects()) ? nullptr : &preprocessShaders);

    PreprocessBufferExt topRowPreProBuffer(ctx.vkd, ctx.device, ctx.allocator, iesHandle, *cmdsLayout, kSequenceCount,
                                           0u, preprocessPipeline, preprocessShaderVec);
    PreprocessBufferExt bottomRowPreProBuffer(ctx.vkd, ctx.device, ctx.allocator, iesHandle, *cmdsLayout,
                                              kSequenceCount, 0u, preprocessPipeline, preprocessShaderVec);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);

    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    renderPass.begin(ctx.vkd, cmdBuffer, scissors.at(0u), clearColor);

    // These are common for the top row and bottom row.
    const std::vector<VkVertexInputBindingDescription2EXT> bindingDescriptions{
        VkVertexInputBindingDescription2EXT{
            VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT,
            nullptr,
            0u,
            0u,
            VK_VERTEX_INPUT_RATE_VERTEX,
            0u,
        },
        VkVertexInputBindingDescription2EXT{
            VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT,
            nullptr,
            1u,
            0u,
            VK_VERTEX_INPUT_RATE_VERTEX,
            0u,
        },
    };

    pipelines.front()->bind(cmdBuffer);
    {
        const std::vector<VkVertexInputAttributeDescription2EXT> bindingAttributes{
            VkVertexInputAttributeDescription2EXT{
                VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT,
                nullptr,
                0u,
                0u,
                VK_FORMAT_R32G32B32A32_SFLOAT,
                static_cast<uint32_t>(offsetof(PositionColor, position)),
            },
            VkVertexInputAttributeDescription2EXT{
                VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT,
                nullptr,
                1u,
                0u,
                VK_FORMAT_R32G32B32A32_SFLOAT,
                static_cast<uint32_t>(offsetof(PositionColor, color)),
            },
        };
        ctx.vkd.cmdSetVertexInputEXT(cmdBuffer, de::sizeU32(bindingDescriptions), de::dataOrNull(bindingDescriptions),
                                     de::sizeU32(bindingAttributes), de::dataOrNull(bindingAttributes));

        const DGCGenCmdsInfo cmdsInfo(shaderStages, iesHandle, *cmdsLayout, topRowDGCBuffer.getDeviceAddress(),
                                      topRowDGCBuffer.getSize(), topRowPreProBuffer.getDeviceAddress(),
                                      topRowPreProBuffer.getSize(), kSequenceCount, 0ull, 0u, preprocessPipeline,
                                      preprocessShaderVec);

        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());
    }

    pipelines.back()->bind(cmdBuffer);
    {
        const std::vector<VkVertexInputAttributeDescription2EXT> bindingAttributes{
            VkVertexInputAttributeDescription2EXT{
                VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT,
                nullptr,
                0u,
                1u,
                VK_FORMAT_R32G32B32A32_SFLOAT,
                static_cast<uint32_t>(offsetof(SeparatePosition, position)),
            },
            VkVertexInputAttributeDescription2EXT{
                VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT,
                nullptr,
                1u,
                0u,
                VK_FORMAT_R32G32B32A32_SFLOAT,
                static_cast<uint32_t>(offsetof(SeparateColor, color)),
            },
        };
        ctx.vkd.cmdSetVertexInputEXT(cmdBuffer, de::sizeU32(bindingDescriptions), de::dataOrNull(bindingDescriptions),
                                     de::sizeU32(bindingAttributes), de::dataOrNull(bindingAttributes));

        const DGCGenCmdsInfo cmdsInfo(shaderStages, iesHandle, *cmdsLayout, bottomRowDGCBuffer.getDeviceAddress(),
                                      bottomRowDGCBuffer.getSize(), bottomRowPreProBuffer.getDeviceAddress(),
                                      bottomRowPreProBuffer.getSize(), kSequenceCount, 0ull, 0u, preprocessPipeline,
                                      preprocessShaderVec);

        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());
    }

    renderPass.end(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1));
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    const auto tcuFormat = mapVkFormat(colorFormat);
    invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());
    tcu::ConstPixelBufferAccess result(tcuFormat, fbExtent, colorBuffer.getBufferAllocation().getHostPtr());

    tcu::TextureLevel referenceLevel(tcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    tcu::PixelBufferAccess reference = referenceLevel.getAccess();

    for (int y = 0; y < fbExtent.y(); ++y)
        for (int x = 0; x < fbExtent.x(); ++x)
        {
            tcu::Vec4 color(1.0f, static_cast<float>(x), static_cast<float>(y), 1.0f);
            if (m_params.useExecutionSet && x > 0)
                color = color.swizzle(3, 2, 1, 0); // The frag shader used in col 1 reverses component order (frag1).
            reference.setPixel(color, x, y);
        }

    auto &log = m_context.getTestContext().getLog();
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);

    if (!tcu::floatThresholdCompare(log, "Result", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Unexpected result in color buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

// Reuse the same pipeline for DGC and normal draws. When using IES, reuse one of them.
// We'll use a 2x2 framebuffer.
// When not using IES, we'll draw half the pixels with normal draws and half without.
// When using IES, we'll draw 3 pixels with the IES and 1 without it.
// Pixels will be covered by quads in 2 triangles, offset by push constants.
class NormalDGCDrawReuseInstance : public vkt::TestInstance
{
public:
    enum class Order
    {
        NORMAL_DGC = 0,
        DGC_NORMAL = 1
    };

    struct Params
    {
        PipelineConstructionType constructionType;
        Order order;
        bool useExecutionSet;

        bool useShaderObjects() const
        {
            return isConstructionTypeShaderObject(constructionType);
        }

        // This will also determine the number of frag shaders in use.
        std::vector<tcu::Vec4> getFragColors() const
        {
            std::vector<tcu::Vec4> colors;

            if (useExecutionSet)
            {
                colors.reserve(3u);
                colors.emplace_back(1.0f, 0.0f, 0.0f, 1.0f);
                colors.emplace_back(1.0f, 1.0f, 0.0f, 1.0f);
                colors.emplace_back(1.0f, 1.0f, 1.0f, 1.0f);
            }
            else
                colors.emplace_back(1.0f, 0.0f, 1.0f, 1.0f);

            return colors;
        }
    };

    NormalDGCDrawReuseInstance(Context &context, const Params &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~NormalDGCDrawReuseInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const Params m_params;
};

class NormalDGCDrawReuseCase : public vkt::TestCase
{
public:
    NormalDGCDrawReuseCase(tcu::TestContext &testCtx, const std::string &name,
                           const NormalDGCDrawReuseInstance::Params params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~NormalDGCDrawReuseCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

protected:
    const NormalDGCDrawReuseInstance::Params m_params;
};

void NormalDGCDrawReuseCase::checkSupport(Context &context) const
{
    const bool useShaderObjects = m_params.useShaderObjects();
    const auto stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto bindStages = (m_params.useExecutionSet ? stageFlags : 0u);
    const auto bindStagesPipeline     = (useShaderObjects ? 0u : bindStages);
    const auto bindStagesShaderObject = (useShaderObjects ? bindStages : 0u);

    checkDGCExtSupport(context, stageFlags, bindStagesPipeline, bindStagesShaderObject);

    if (useShaderObjects)
        context.requireDeviceFunctionality("VK_EXT_shader_object");
}

void NormalDGCDrawReuseCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "layout (push_constant, std430) uniform PCBlock { vec4 offset; } pc;\n"
         << "void main (void)\n"
         << "{\n"
         << "    gl_Position = inPos + pc.offset;\n"
         << "}\n";

    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    const auto fragColors = m_params.getFragColors();
    for (uint32_t i = 0u; i < de::sizeU32(fragColors); ++i)
    {
        std::ostringstream frag;
        frag << "#version 460\n"
             << "layout (location=0) out vec4 outColor;\n"
             << "void main (void)\n"
             << "{\n"
             << "    outColor = vec4" << fragColors.at(i) << ";\n"
             << "}\n";
        const auto shaderName = "frag" + std::to_string(i);
        programCollection.glslSources.add(shaderName) << glu::FragmentSource(frag.str());
    }
}

TestInstance *NormalDGCDrawReuseCase::createInstance(Context &context) const
{
    return new NormalDGCDrawReuseInstance(context, m_params);
}

tcu::TestStatus NormalDGCDrawReuseInstance::iterate(void)
{
    const auto ctx = m_context.getContextCommonData();
    const tcu::IVec3 fbExtent(2, 2, 1);
    const auto apiExtent   = makeExtent3D(fbExtent);
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage =
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto fragColors = m_params.getFragColors();
    const auto kPipelineCount = de::sizeU32(fragColors);

    // Color buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, apiExtent, colorFormat, colorUsage,
                                VK_IMAGE_TYPE_2D);

    // Render pass and framebuffer.
    RenderPassWrapper renderPass(m_params.constructionType, ctx.vkd, ctx.device, colorFormat);
    renderPass.createFramebuffer(ctx.vkd, ctx.device, colorBuffer.getImage(), colorBuffer.getImageView(),
                                 apiExtent.width, apiExtent.height);

    // Shaders.
    const auto &binaries = m_context.getBinaryCollection();

    ShaderWrapper vertShader(ctx.vkd, ctx.device, binaries.get("vert"));
    std::vector<ShaderWrapperPtr> fragShaders;
    fragShaders.reserve(kPipelineCount);
    for (uint32_t i = 0u; i < kPipelineCount; ++i)
    {
        const auto shaderName = "frag" + std::to_string(i);
        fragShaders.emplace_back(new ShaderWrapper(ctx.vkd, ctx.device, binaries.get(shaderName)));
    }

    // Push constants.
    const auto pcStages = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT);
    const auto pcSize   = DE_SIZEOF32(tcu::Vec4);
    const auto pcRange  = makePushConstantRange(pcStages, 0u, pcSize);

    // Pipeline layout.
    PipelineLayoutWrapper pipelineLayout(m_params.constructionType, ctx.vkd, ctx.device, 0u, nullptr, 1u, &pcRange);

    // Pipelines.
    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    std::vector<GraphicsPipelineWrapperPtr> pipelines;
    pipelines.reserve(kPipelineCount);

    const auto pipelineCreateFlags2 = static_cast<VkPipelineCreateFlags2KHR>(
        m_params.useExecutionSet ? VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT : 0);
    const auto shaderCreateFlags =
        static_cast<VkShaderCreateFlagsEXT>(m_params.useExecutionSet ? VK_SHADER_CREATE_INDIRECT_BINDABLE_BIT_EXT : 0);

    for (uint32_t i = 0u; i < kPipelineCount; ++i)
    {
        const auto &extensions = m_context.getDeviceExtensions();
        pipelines.emplace_back(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device, extensions,
                                                           m_params.constructionType));
        auto &pipeline = *pipelines.back();
        pipeline.setPipelineCreateFlags2(pipelineCreateFlags2)
            .setShaderCreateFlags(shaderCreateFlags)
            .setMonolithicPipelineLayout(pipelineLayout)
            .setDefaultColorBlendState()
            .setDefaultMultisampleState()
            .setDefaultRasterizationState()
            .setDefaultPatchControlPoints(0u)
            .setDefaultDepthStencilState()
            .setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
            .setupVertexInputState()
            .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass.get(), 0u, vertShader)
            .setupFragmentShaderState(pipelineLayout, renderPass.get(), 0u, *fragShaders.at(i))
            .setupFragmentOutputState(renderPass.get())
            .buildPipeline();
    }

    // Indirect execution set.
    const bool useShaderObjects = m_params.useShaderObjects();
    ExecutionSetManagerPtr iesManager;
    VkIndirectExecutionSetEXT iesHandle = VK_NULL_HANDLE;

    if (m_params.useExecutionSet)
    {
        if (useShaderObjects)
        {
            // Note we will be using kPipelineCount fragment shaders, 1 vertex shader, no set layouts and the vertex
            // shaders have push constants. In the IES we'll store the vertex shader first, followed by fragment shaders.
            const std::vector<VkDescriptorSetLayout> noSetLayouts;
            const auto maxShaderCount = kPipelineCount + 1u; // vertex shader + fragment shaders.

            std::vector<IESStageInfo> stageInfos;
            IESStageInfo fragStageInfo(pipelines.at(0u)->getShader(VK_SHADER_STAGE_VERTEX_BIT), noSetLayouts);
            IESStageInfo vertStageInfo(pipelines.at(0u)->getShader(VK_SHADER_STAGE_FRAGMENT_BIT), noSetLayouts);
            stageInfos.push_back(fragStageInfo);
            stageInfos.push_back(vertStageInfo);

            const std::vector<VkPushConstantRange> pcRanges{pcRange};

            iesManager = makeExecutionSetManagerShader(ctx.vkd, ctx.device, stageInfos, pcRanges, maxShaderCount);

            // Overwrite fragment shaders only. Leave the vertex shader alone in position 0.
            for (uint32_t i = 0u; i < kPipelineCount; ++i)
                iesManager->addShader(i + 1u, pipelines.at(i)->getShader(VK_SHADER_STAGE_FRAGMENT_BIT));
        }
        else
        {
            iesManager =
                makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, pipelines.at(0u)->getPipeline(), kPipelineCount);

            // Overwrite all pipelines in the set.
            for (uint32_t i = 0u; i < kPipelineCount; ++i)
                iesManager->addPipeline(i, pipelines.at(i)->getPipeline());
        }
        iesManager->update();
        iesHandle = iesManager->get();
    }

    // Vertex data and vertex buffers. A triangle strip from 0..1 will be offset in each quadrant with push constants.
    const std::vector<tcu::Vec4> vtxPositions{
        tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
        tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
    };

    const auto vtxBuffer = makeVertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vtxPositions, false);

    // Offsets for push constants. This will determine the quadrant order: which quadrant gets which color.
    // Proceed one row at a time from top to bottom, and in each row from left to right.
    const std::vector<tcu::Vec4> offsets{
        tcu::Vec4(-1.0f, -1.0f, 0.0f, 0.0f),
        tcu::Vec4(0.0f, -1.0f, 0.0f, 0.0f),
        tcu::Vec4(-1.0f, 0.0f, 0.0f, 0.0f),
        tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
    };

    // Indirect commands layout.
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, stageFlags, pipelineLayout.get());
    if (m_params.useExecutionSet)
    {
        const auto setType = (useShaderObjects ? VK_INDIRECT_EXECUTION_SET_INFO_TYPE_SHADER_OBJECTS_EXT :
                                                 VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT);
        cmdsLayoutBuilder.addExecutionSetToken(cmdsLayoutBuilder.getStreamRange(), setType, stageFlags);
    }
    cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(), pcRange);
    cmdsLayoutBuilder.addDrawToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // DGC buffer.
    const auto kVertexCount = de::sizeU32(vtxPositions);
    const VkDrawIndirectCommand drawCmd{kVertexCount, 1u, 0u, 0u};
    const auto kSequenceCount = (m_params.useExecutionSet ? kPipelineCount : kPipelineCount + 1u);
    DE_ASSERT(m_params.useExecutionSet ||
              kSequenceCount == 2u); // It should be 2 because kPipelineCount should be 1 in that case.

    const uint32_t dgcDataSize = (cmdsLayoutBuilder.getStreamStride() * kSequenceCount) / DE_SIZEOF32(uint32_t);
    std::vector<uint32_t> dgcData;
    dgcData.reserve(dgcDataSize);

    // As explained above, we have 4 different cases:
    // - If not using IES:
    //   - kSequenceCount for DGC should be 2 because...
    //   - There will be 2 normal draws and 2 DGC draws.
    //   - Normal draws followed by DGC: 2 normal draws using the first 2 PC offsets, 2 DGC draws using the last 2 PC offsets.
    //   - DGC draws followed by normal draws: vice versa.
    // - If using IES:
    //   - kSequenceCount for DGC should be 3 because...
    //   - There will be 3 DGC draws and 1 normal draw.
    //   - Normal draws followed by DGC: 1 normal draw using the first PC offset, 3 DGC draws using the rest.
    //   - DGC draws followed by normal draws: 3 DGC draws using the first 3 PC offsets, 1 normal draw using the last one.
    const bool dgcFirst               = (m_params.order == Order::DGC_NORMAL);
    const uint32_t firstDGCPCIndex    = (m_params.useExecutionSet ? (dgcFirst ? 0u : 1u) : (dgcFirst ? 0u : 2u));
    const uint32_t firstNormalPCIndex = (m_params.useExecutionSet ? (dgcFirst ? 3u : 0u) : (dgcFirst ? 2u : 0u));
    const uint32_t dgcDrawCount       = (m_params.useExecutionSet ? 3u : 2u);
    const uint32_t normalDrawCount    = (m_params.useExecutionSet ? 1u : 2u);

    DE_ASSERT(dgcDrawCount == kSequenceCount);
    DE_UNREF(dgcDrawCount); // For release builds.

    for (uint32_t i = 0u; i < kSequenceCount; ++i)
    {
        if (m_params.useExecutionSet)
        {
            if (useShaderObjects)
            {
                dgcData.push_back(0u);     // Vertex shader index is constant.
                dgcData.push_back(i + 1u); // Fragment shader index for sequence i.
            }
            else
            {
                dgcData.push_back(i); // Pipeline index.
            }
        }
        {
            const auto pcIndex = firstDGCPCIndex + i;
            pushBackElement(dgcData, offsets.at(pcIndex)); // Push constants.
        }
        pushBackElement(dgcData, drawCmd); // Draw command.
    }

    const auto dgcBufferSize = static_cast<VkDeviceSize>(de::dataSize(dgcData));
    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, dgcBufferSize);
    {
        auto &alloc   = dgcBuffer.getAllocation();
        void *dataPtr = alloc.getHostPtr();
        deMemcpy(dataPtr, de::dataOrNull(dgcData), de::dataSize(dgcData));
    }

    // Preprocess buffer.
    const auto preprocessPipeline =
        ((m_params.useExecutionSet || useShaderObjects) ? VK_NULL_HANDLE : pipelines.at(0u)->getPipeline());
    const bool needPreprocessShaders = (!m_params.useExecutionSet && useShaderObjects);
    const auto preprocessShaders =
        (needPreprocessShaders ? std::vector<VkShaderEXT>{pipelines.at(0u)->getShader(VK_SHADER_STAGE_VERTEX_BIT),
                                                          pipelines.at(0u)->getShader(VK_SHADER_STAGE_FRAGMENT_BIT)} :
                                 std::vector<VkShaderEXT>());
    const auto preprocessShadersPtr = (needPreprocessShaders ? &preprocessShaders : nullptr);
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, iesHandle, *cmdsLayout, kSequenceCount, 0u,
                                         preprocessPipeline, preprocessShadersPtr);

    // Submit commands.
    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;
    beginCommandBuffer(ctx.vkd, cmdBuffer);

    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f); // Different from other colors due to the red value.
    renderPass.begin(ctx.vkd, cmdBuffer, scissors.at(0u), clearColor);

    const VkDeviceSize vertexBufferOffset = 0ull;
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vtxBuffer->get(), &vertexBufferOffset);

    // Initial shader state. Note we bind the last pipeline to make things more interesting.
    pipelines.back()->bind(cmdBuffer);

    if (!dgcFirst)
    {
        for (uint32_t i = 0; i < normalDrawCount; ++i)
        {
            ctx.vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), pcStages, 0u, pcSize,
                                     &offsets.at(firstNormalPCIndex + i));
            ctx.vkd.cmdDraw(cmdBuffer, kVertexCount, 1u, 0u, 0u);
        }
    }

    DGCGenCmdsInfo cmdsInfo(stageFlags, iesHandle, *cmdsLayout, dgcBuffer.getDeviceAddress(), dgcBuffer.getSize(),
                            preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(), kSequenceCount, 0ull, 0u,
                            preprocessPipeline, preprocessShadersPtr);
    ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());

    if (dgcFirst)
    {
        if (m_params.useExecutionSet)
            pipelines.front()->bind(cmdBuffer); // We bind the first pipeline to make things more interesting.

        for (uint32_t i = 0; i < normalDrawCount; ++i)
        {
            ctx.vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), pcStages, 0u, pcSize,
                                     &offsets.at(firstNormalPCIndex + i));
            ctx.vkd.cmdDraw(cmdBuffer, kVertexCount, 1u, 0u, 0u);
        }
    }

    renderPass.end(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1));
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());

    const auto tcuFormat = mapVkFormat(colorFormat);

    tcu::TextureLevel referenceLevel(tcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    tcu::PixelBufferAccess reference = referenceLevel.getAccess();
    if (m_params.useExecutionSet)
    {
        const std::vector<tcu::IVec2> fragCoords{
            // Must match order with offsets in push constant values.
            tcu::IVec2(0, 0),
            tcu::IVec2(1, 0),
            tcu::IVec2(0, 1),
            tcu::IVec2(1, 1),
        };

        size_t itr = 0u;

        if (!dgcFirst)
        {
            const auto &coords = fragCoords.at(itr++);
            reference.setPixel(fragColors.back(), coords.x(), coords.y());
        }
        for (const auto &color : fragColors)
        {
            const auto &coords = fragCoords.at(itr++);
            reference.setPixel(color, coords.x(), coords.y());
        }
        if (dgcFirst)
        {
            const auto &coords = fragCoords.at(itr++);
            reference.setPixel(fragColors.front(), coords.x(), coords.y());
        }
    }
    else
    {
        tcu::clear(reference, fragColors.front());
    }

    tcu::ConstPixelBufferAccess result(tcuFormat, fbExtent, colorBuffer.getBufferAllocation().getHostPtr());

    auto &log = m_context.getTestContext().getLog();
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);

    if (!tcu::floatThresholdCompare(log, "Result", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Unexpected results in color buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

struct NormalDGCNormalParams
{
    bool useExecutionSet;
};

void normalDGCNormalCheckSupport(Context &context, NormalDGCNormalParams params)
{
    const auto stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto bindFlags  = (params.useExecutionSet ? stageFlags : 0u);
    checkDGCExtSupport(context, stageFlags, bindFlags);

    // Required by the vertex buffer token.
    context.requireDeviceFunctionality("VK_EXT_extended_dynamic_state");
}

void normalDGCNormalInitPrograms(vk::SourceCollections &programCollection, NormalDGCNormalParams params)
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "void main (void)\n"
         << "{\n"
         << "    gl_Position = inPos;\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream fragNormal;
    fragNormal << "#version 460\n"
               << "layout (location=0) out vec4 outColor;\n"
               << "layout (push_constant, std430) uniform PCBlock { uint seqIndex; } pc;\n"
               << "void main (void) {\n"
               << "    outColor = vec4(1.0, 1.0, float(pc.seqIndex), 1.0);\n"
               << "}\n";
    programCollection.glslSources.add("fragNormal") << glu::FragmentSource(fragNormal.str());

    const std::vector<uint32_t> redValues{1u, 0u};
    const uint32_t kDGCFragShaderCount = (params.useExecutionSet ? 2u : 1u);
    for (uint32_t i = 0u; i < kDGCFragShaderCount; ++i)
    {
        std::ostringstream fragDGC;
        fragDGC << "#version 460\n"
                << "layout (location=0) out vec4 outColor;\n"
                << "layout (push_constant, std430) uniform PCBlock { uint seqIndex; } pc;\n"
                << "void main (void) {\n"
                << "    outColor = vec4(" << redValues.at(i) << ", 0.0, float(pc.seqIndex), 1.0);\n"
                << "}\n";
        const auto shaderName = "fragDGC" + std::to_string(i);
        programCollection.glslSources.add(shaderName) << glu::FragmentSource(fragDGC.str());
    }
}

tcu::TestStatus normalDGCNormalRun(Context &context, NormalDGCNormalParams params)
{
    const auto ctx = context.getContextCommonData();
    const tcu::IVec3 fbExtent(2, 2, 1);
    const auto apiExtent   = makeExtent3D(fbExtent);
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage =
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto constructionType        = PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC;
    const uint32_t kDGCFragShaderCount = (params.useExecutionSet ? 2u : 1u);

    // Color buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, apiExtent, colorFormat, colorUsage,
                                VK_IMAGE_TYPE_2D);

    // Render pass and framebuffer.
    RenderPassWrapper renderPass(constructionType, ctx.vkd, ctx.device, colorFormat);
    renderPass.createFramebuffer(ctx.vkd, ctx.device, colorBuffer.getImage(), colorBuffer.getImageView(),
                                 apiExtent.width, apiExtent.height);

    // Shaders.
    const auto &binaries = context.getBinaryCollection();

    ShaderWrapper vertShader(ctx.vkd, ctx.device, binaries.get("vert"));
    ShaderWrapper fragNormalShader(ctx.vkd, ctx.device, binaries.get("fragNormal"));
    std::vector<ShaderWrapperPtr> fragDGCShaders;
    fragDGCShaders.reserve(kDGCFragShaderCount);
    for (uint32_t i = 0u; i < kDGCFragShaderCount; ++i)
    {
        const auto shaderName = "fragDGC" + std::to_string(i);
        fragDGCShaders.emplace_back(new ShaderWrapper(ctx.vkd, ctx.device, binaries.get(shaderName)));
    }

    // Push constants.
    const auto pcStages = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto pcSize   = DE_SIZEOF32(uint32_t);
    const auto pcRange  = makePushConstantRange(pcStages, 0u, pcSize);

    // Pipeline layout.
    PipelineLayoutWrapper pipelineLayout(constructionType, ctx.vkd, ctx.device, 0u, nullptr, 1u, &pcRange);

    // Pipelines.
    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    // Normal pipeline.
    const auto &extensions = context.getDeviceExtensions();
    GraphicsPipelineWrapper normalPipeline(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device, extensions,
                                           constructionType);
    normalPipeline.setMonolithicPipelineLayout(pipelineLayout)
        .setDefaultColorBlendState()
        .setDefaultMultisampleState()
        .setDefaultRasterizationState()
        .setDefaultPatchControlPoints(0u)
        .setDefaultDepthStencilState()
        .setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
        .setupVertexInputState()
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass.get(), 0u, vertShader)
        .setupFragmentShaderState(pipelineLayout, renderPass.get(), 0u, fragNormalShader)
        .setupFragmentOutputState(renderPass.get())
        .buildPipeline();

    // Pipelines to be used with DGC, which may be non-indirect pipelines as well, but they can be indirect ones.
    std::vector<GraphicsPipelineWrapperPtr> dgcPipelines;
    dgcPipelines.reserve(kDGCFragShaderCount);

    const auto pipelineCreateFlags2 = static_cast<VkPipelineCreateFlags2KHR>(
        params.useExecutionSet ? VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT : 0);
    const auto shaderCreateFlags =
        static_cast<VkShaderCreateFlagsEXT>(params.useExecutionSet ? VK_SHADER_CREATE_INDIRECT_BINDABLE_BIT_EXT : 0);

    const std::vector<VkDynamicState> dynamicStates{VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT};

    const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        nullptr,
        0u,
        de::sizeU32(dynamicStates),
        de::dataOrNull(dynamicStates),
    };

    for (uint32_t i = 0u; i < kDGCFragShaderCount; ++i)
    {
        dgcPipelines.emplace_back(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                                              extensions, constructionType));
        auto &pipeline = *dgcPipelines.back();
        pipeline.setPipelineCreateFlags2(pipelineCreateFlags2)
            .setShaderCreateFlags(shaderCreateFlags)
            .setMonolithicPipelineLayout(pipelineLayout)
            .setDefaultColorBlendState()
            .setDefaultMultisampleState()
            .setDefaultRasterizationState()
            .setDefaultPatchControlPoints(0u)
            .setDefaultDepthStencilState()
            .setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
            .setDynamicState(&dynamicStateCreateInfo)
            .setupVertexInputState()
            .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass.get(), 0u, vertShader)
            .setupFragmentShaderState(pipelineLayout, renderPass.get(), 0u, *fragDGCShaders.at(i))
            .setupFragmentOutputState(renderPass.get())
            .buildPipeline();
    }

    ExecutionSetManagerPtr iesManager;
    VkIndirectExecutionSetEXT iesHandle = VK_NULL_HANDLE;

    if (params.useExecutionSet)
    {
        iesManager = makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, dgcPipelines.at(0u)->getPipeline(),
                                                     kDGCFragShaderCount);

        // Overwrite all pipelines in the set.
        for (uint32_t i = 0u; i < kDGCFragShaderCount; ++i)
            iesManager->addPipeline(i, dgcPipelines.at(i)->getPipeline());
        iesManager->update();
        iesHandle = iesManager->get();
    }

    // The framebuffer is 2x2.
    // The first normal draw handles the NW quadrant.
    // The DGC draws handle the NE and SW quadrants.
    // The second normal draw handles the SE quadrant.

    // The idea is using 4 vertices per pixel forming a quad. This gives us a total of 16 vertices.
    // Vertex buffers will have capacity for 16 vertices, but both vertex buffers and index buffers will vary from normal to DGC draws.
    // Normal draws: the first 8 vertices will be bad values, the last ones will handle NW and SE respectively.
    // Normal draws: the index buffer will, thus, contain: 8, 9, 10, 11, 12, 13, 14, 15.
    // DGC draws: the first 8 vertices will contain the NE and SW quadrants, respectively. The last 8 will be bad values.
    // DGC draws: the index buffer will, thus, contain: 0, 1, 2, 3, 4, 5, 6, 7.

    const tcu::Vec4 badVertex(10.0f, 10.0f, 0.0f, 1.0f);

    const std::vector<tcu::Vec4> normalVertices{
        badVertex,
        badVertex,
        badVertex,
        badVertex,
        badVertex,
        badVertex,
        badVertex,
        badVertex,
        // NW
        tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
        tcu::Vec4(-1.0f, 0.0f, 0.0f, 1.0f),
        tcu::Vec4(0.0f, -1.0f, 0.0f, 1.0f),
        tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
        // SE
        tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
        tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
    };

    const auto normalVertexBuffer = makeVertexBuffer(ctx.vkd, ctx.device, ctx.allocator, normalVertices, false);

    const std::vector<uint32_t> normalIndices{8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u};

    const auto normalIndexBuffer = makeIndexBuffer(ctx.vkd, ctx.device, ctx.allocator, normalIndices, false);

    const std::vector<tcu::Vec4> dgcVertices{
        // NE
        tcu::Vec4(0.0f, -1.0f, 0.0f, 1.0f),
        tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
        // SW
        tcu::Vec4(-1.0f, 0.0f, 0.0f, 1.0f),
        tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f),
        tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
        tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f),
        // Others.
        badVertex,
        badVertex,
        badVertex,
        badVertex,
        badVertex,
        badVertex,
        badVertex,
        badVertex,
    };

    const auto dgcVertexBuffer = makeVertexBuffer(ctx.vkd, ctx.device, ctx.allocator, dgcVertices, true);

    const std::vector<uint32_t> dgcIndices{0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u};

    const auto dgcIndexBuffer = makeIndexBuffer(ctx.vkd, ctx.device, ctx.allocator, dgcIndices, true);

    // Indirect commands layout.
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, stageFlags, pipelineLayout.get());
    if (params.useExecutionSet)
    {
        const auto setType = VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT;
        cmdsLayoutBuilder.addExecutionSetToken(cmdsLayoutBuilder.getStreamRange(), setType, stageFlags);
    }
    cmdsLayoutBuilder.addSequenceIndexToken(cmdsLayoutBuilder.getStreamRange(), pcRange);
    cmdsLayoutBuilder.addVertexBufferToken(cmdsLayoutBuilder.getStreamRange(), 0u);
    cmdsLayoutBuilder.addIndexBufferToken(cmdsLayoutBuilder.getStreamRange(),
                                          VK_INDIRECT_COMMANDS_INPUT_MODE_VULKAN_INDEX_BUFFER_EXT);
    cmdsLayoutBuilder.addDrawIndexedToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // DGC buffer.
    const uint32_t kSequenceCount = 2u;
    const uint32_t dgcDataSize    = (cmdsLayoutBuilder.getStreamStride() * kSequenceCount) / DE_SIZEOF32(uint32_t);

    std::vector<uint32_t> dgcData;
    dgcData.reserve(dgcDataSize);

    // DGC vertex and index buffers stay constant.
    const VkBindVertexBufferIndirectCommandEXT vertexBufferBindCmd{
        getBufferDeviceAddress(ctx.vkd, ctx.device, dgcVertexBuffer->get()),
        static_cast<uint32_t>(de::dataSize(dgcVertices)),
        DE_SIZEOF32(tcu::Vec4),
    };

    const VkBindIndexBufferIndirectCommandEXT indexBufferBindCmd{
        getBufferDeviceAddress(ctx.vkd, ctx.device, dgcIndexBuffer->get()),
        static_cast<uint32_t>(de::dataSize(dgcIndices)),
        VK_INDEX_TYPE_UINT32,
    };

    const uint32_t dgcVerticesPerSeq = de::sizeU32(dgcIndices) / kSequenceCount;
    DE_ASSERT(de::sizeU32(dgcIndices) % kSequenceCount == 0u);

    for (uint32_t i = 0u; i < kSequenceCount; ++i)
    {
        if (params.useExecutionSet)
            dgcData.push_back(i);                                // Pipeline index.
        dgcData.push_back(std::numeric_limits<uint32_t>::max()); // Placeholder for the sequence item.
        pushBackElement(dgcData, vertexBufferBindCmd);
        pushBackElement(dgcData, indexBufferBindCmd);
        {
            const VkDrawIndexedIndirectCommand drawCmd{
                dgcVerticesPerSeq, 1u, dgcVerticesPerSeq * i, 0, 0u,
            };
            pushBackElement(dgcData, drawCmd);
        }
    }

    const auto dgcBufferSize = static_cast<VkDeviceSize>(de::dataSize(dgcData));
    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, dgcBufferSize);
    {
        auto &alloc   = dgcBuffer.getAllocation();
        void *dataPtr = alloc.getHostPtr();
        deMemcpy(dataPtr, de::dataOrNull(dgcData), de::dataSize(dgcData));
    }

    // Preprocess buffer.
    const auto preprocessPipeline = (params.useExecutionSet ? VK_NULL_HANDLE : dgcPipelines.at(0u)->getPipeline());
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, iesHandle, *cmdsLayout, kSequenceCount, 0u,
                                         preprocessPipeline);

    // Commands.
    const uint32_t kPerQuadrantVertices = 4u;
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    const VkDeviceSize vertexBufferOffset = 0ull;
    const uint32_t normalSeqIndex = (params.useExecutionSet ? 0u : 1u); // Make it different from the last DGC one.

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    renderPass.begin(ctx.vkd, cmdBuffer, scissors.at(0u), clearColor);

    // First normal draw: NW
    {
        ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &normalVertexBuffer->get(), &vertexBufferOffset);
        ctx.vkd.cmdBindIndexBuffer(cmdBuffer, normalIndexBuffer->get(), 0ull, VK_INDEX_TYPE_UINT32);
        ctx.vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), pcStages, 0u, pcSize, &normalSeqIndex);
        normalPipeline.bind(cmdBuffer);
        ctx.vkd.cmdDrawIndexed(cmdBuffer, kPerQuadrantVertices, 1u, 0u, 0, 0u);
    }

    // DGC draw: NE and SW.
    {
        dgcPipelines.at(0u)->bind(cmdBuffer);
        const DGCGenCmdsInfo cmdsInfo(stageFlags, iesHandle, cmdsLayout.get(), dgcBuffer.getDeviceAddress(),
                                      dgcBuffer.getSize(), preprocessBuffer.getDeviceAddress(),
                                      preprocessBuffer.getSize(), kSequenceCount, 0ull, 0u, preprocessPipeline);
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());
    }

    // Last normal draw: SE
    {
        ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &normalVertexBuffer->get(), &vertexBufferOffset);
        ctx.vkd.cmdBindIndexBuffer(cmdBuffer, normalIndexBuffer->get(), 0ull, VK_INDEX_TYPE_UINT32);
        ctx.vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), pcStages, 0u, pcSize, &normalSeqIndex);
        normalPipeline.bind(cmdBuffer);
        ctx.vkd.cmdDrawIndexed(cmdBuffer, kPerQuadrantVertices, 1u, kPerQuadrantVertices, 0, 0u);
    }

    renderPass.end(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1));
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    const auto tcuFormat = mapVkFormat(colorFormat);

    tcu::TextureLevel refLevel(tcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    tcu::PixelBufferAccess reference = refLevel.getAccess();

    tcu::clear(reference, clearColor);
    {
        const tcu::Vec4 normalColor(1.0f, 1.0f, static_cast<float>(normalSeqIndex), 1.0f);

        reference.setPixel(normalColor, 0, 0);
        reference.setPixel(normalColor, 1, 1);
    }
    {
        const auto red    = 1.0f;
        const auto redAlt = (params.useExecutionSet ? 0.0f : 1.0f);

        reference.setPixel(tcu::Vec4(red, 0.0f, 0.0f, 1.0f), 1, 0);
        reference.setPixel(tcu::Vec4(redAlt, 0.0f, 1.0f, 1.0f), 0, 1);
    }

    invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());
    const tcu::ConstPixelBufferAccess result(tcuFormat, fbExtent, colorBuffer.getBufferAllocation().getHostPtr());

    auto &log = context.getTestContext().getLog();
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);

    if (!tcu::floatThresholdCompare(log, "Reference", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Unexpected results in color buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

class SampleIDStateInstance : public vkt::TestInstance
{
public:
    struct Params
    {
        PipelineConstructionType constructionType;
        bool idFirst;
        bool preprocess;

        VkSampleCountFlagBits getSampleCount() const
        {
            return VK_SAMPLE_COUNT_4_BIT;
        }

        tcu::Vec4 getClearColor() const
        {
            return tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
        }

        tcu::Vec4 getGeometryColor() const
        {
            return tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);
        }
    };

    SampleIDStateInstance(Context &context, const Params &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~SampleIDStateInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const Params m_params;
};

class SampleIDStateCase : public vkt::TestCase
{
public:
    SampleIDStateCase(tcu::TestContext &testCtx, const std::string &name, const SampleIDStateInstance::Params &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~SampleIDStateCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new SampleIDStateInstance(context, m_params);
    }

protected:
    const SampleIDStateInstance::Params m_params;
};

void SampleIDStateCase::checkSupport(Context &context) const
{
    const bool useESO                 = isConstructionTypeShaderObject(m_params.constructionType);
    const auto stages                 = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto bindStagesPipeline     = (useESO ? 0u : stages);
    const auto bindStagesShaderObject = (useESO ? stages : 0u);

    checkDGCExtSupport(context, stages, bindStagesPipeline, bindStagesShaderObject);

    const auto ctx = context.getContextCommonData();
    checkPipelineConstructionRequirements(ctx.vki, ctx.physicalDevice, m_params.constructionType);
}

void SampleIDStateCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "void main (void) {\n"
         << "    gl_Position = inPos;\n"
         << "    gl_PointSize = 1.0;\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    for (const bool useSampleID : {false, true})
    {
        const std::string shaderName = std::string("frag") + (useSampleID ? "Y" : "N");
        const auto geometryColor     = m_params.getGeometryColor();

        std::ostringstream frag;
        frag << "#version 460\n"
             << "layout (location=0) out vec4 outColor;\n"
             << "layout (rgba8, set=0, binding=0) uniform image2D img;\n"
             << "void main(){\n"
             << "    const vec4 geomColor = vec4" << geometryColor << ";\n"
             << "    const ivec2 iFragCoord = ivec2(gl_FragCoord.xy);\n"
             << "    const int yCoord = " << (useSampleID ? "gl_SampleID" : "iFragCoord.x") << ";\n"
             << "    imageStore(img, ivec2(iFragCoord.x, yCoord), geomColor);\n"
             << "    outColor = geomColor;\n"
             << "}\n";
        programCollection.glslSources.add(shaderName) << glu::FragmentSource(frag.str());
    }
}

tcu::TestStatus SampleIDStateInstance::iterate(void)
{
    const auto ctx         = m_context.getContextCommonData();
    const auto useESO      = isConstructionTypeShaderObject(m_params.constructionType);
    const auto sampleCount = m_params.getSampleCount();
    const tcu::IVec3 fbExtent(sampleCount, 1, 1);
    const auto fbExtentVk = makeExtent3D(fbExtent);
    const tcu::IVec3 storageExtent(sampleCount, sampleCount, 1);
    const auto storageExtentVk = makeExtent3D(storageExtent);
    const auto format          = VK_FORMAT_R8G8B8A8_UNORM;
    const auto xferUsage       = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto fbUsage         = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | xferUsage);
    const auto storageUsage    = (VK_IMAGE_USAGE_STORAGE_BIT | xferUsage);
    const auto colorSRR        = makeDefaultImageSubresourceRange();
    const auto shaderStages    = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto clearColor      = m_params.getClearColor();
    const auto geometryColor   = m_params.getGeometryColor();

    // Multisample image for the framebuffer.
    const VkImageCreateInfo msImageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0u,
        VK_IMAGE_TYPE_2D,
        format,
        fbExtentVk,
        1u,
        1u,
        sampleCount,
        VK_IMAGE_TILING_OPTIMAL,
        fbUsage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };
    ImageWithMemory msImg(ctx.vkd, ctx.device, ctx.allocator, msImageCreateInfo, MemoryRequirement::Any);
    const auto msView = makeImageView(ctx.vkd, ctx.device, *msImg, VK_IMAGE_VIEW_TYPE_2D, format, colorSRR);

    // Single sample image for the color resolve, with verification buffer.
    ImageWithBuffer ssImg(ctx.vkd, ctx.device, ctx.allocator, fbExtentVk, format, fbUsage, VK_IMAGE_TYPE_2D);

    // Storage image with verification buffer.
    ImageWithBuffer storageImg(ctx.vkd, ctx.device, ctx.allocator, storageExtentVk, format, storageUsage,
                               VK_IMAGE_TYPE_2D);

    // Vertex buffer.
    std::vector<tcu::Vec4> vertices;
    const auto pixelCount   = fbExtent.x() * fbExtent.y() * fbExtent.z();
    const auto perPixelVert = 4u; // Quad as a triangle strip.
    const auto vertexCount  = pixelCount * perPixelVert;
    vertices.reserve(vertexCount);

    DE_ASSERT(fbExtent.y() == 1 && fbExtent.z() == 1);
    const auto floatExtent = fbExtent.asFloat();

    const auto normalize = [](float v, float size) { return (v / size) * 2.0f - 1.0f; };

    for (int x = 0; x < fbExtent.x(); ++x)
    {
        const auto fx      = static_cast<float>(x);
        const auto xLeft   = normalize(fx, floatExtent.x());
        const auto xRight  = normalize(fx + 1.0f, floatExtent.x());
        const auto yTop    = -1.0f;
        const auto yBottom = 1.0f;

        // Quad covering each pixel completely.
        vertices.emplace_back(xLeft, yTop, 0.0f, 1.0f);
        vertices.emplace_back(xLeft, yBottom, 0.0f, 1.0f);
        vertices.emplace_back(xRight, yTop, 0.0f, 1.0f);
        vertices.emplace_back(xRight, yBottom, 0.0f, 1.0f);
    }

    const auto vertexBufferCreateInfo =
        makeBufferCreateInfo(static_cast<VkDeviceSize>(de::dataSize(vertices)), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferCreateInfo,
                                  MemoryRequirement::HostVisible);
    {
        auto &alloc = vertexBuffer.getAllocation();
        deMemcpy(alloc.getHostPtr(), de::dataOrNull(vertices), de::dataSize(vertices));
    }
    const VkDeviceSize vertexBufferOffset = 0ull;

    // Descriptor set.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    const auto descPool = poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto setLayout     = setLayoutBuilder.build(ctx.vkd, ctx.device);
    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descPool, *setLayout);

    using Location = DescriptorSetUpdateBuilder::Location;
    DescriptorSetUpdateBuilder setUpdateBuilder;
    const auto storageImgDescInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, storageImg.getImageView(), VK_IMAGE_LAYOUT_GENERAL);
    setUpdateBuilder.writeSingle(*descriptorSet, Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                 &storageImgDescInfo);
    setUpdateBuilder.update(ctx.vkd, ctx.device);

    // Pipelines.
    const std::vector<VkAttachmentDescription> attDesc{
        makeAttachmentDescription(0u, format, sampleCount, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                  VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                  VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
        makeAttachmentDescription(0u, format, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                  VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                  VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
    };

    const auto msAttRef = makeAttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const auto ssAttRef = makeAttachmentReference(1u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    const auto subpassDesc = makeSubpassDescription(0u, VK_PIPELINE_BIND_POINT_GRAPHICS, 0u, nullptr, 1u, &msAttRef,
                                                    &ssAttRef, nullptr, 0u, nullptr);

    const VkRenderPassCreateInfo renderPassCreateInfo = {
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
    RenderPassWrapper renderPass(m_params.constructionType, ctx.vkd, ctx.device, &renderPassCreateInfo);

    const std::vector<VkImageView> fbViews{*msView, ssImg.getImageView()};
    const std::vector<VkImage> fbImages{*msImg, ssImg.getImage()};
    DE_ASSERT(fbViews.size() == fbImages.size());

    const VkFramebufferCreateInfo fbCreateInfo = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        nullptr,
        0u,
        *renderPass,
        de::sizeU32(fbViews),
        de::dataOrNull(fbViews),
        fbExtentVk.width,
        fbExtentVk.height,
        1u,
    };
    renderPass.createFramebuffer(ctx.vkd, ctx.device, &fbCreateInfo, fbImages);

    const PipelineLayoutWrapper pipelineLayout(m_params.constructionType, ctx.vkd, ctx.device, *setLayout);

    const auto &binaries = m_context.getBinaryCollection();
    const ShaderWrapper vertShader(ctx.vkd, ctx.device, binaries.get("vert"));
    const ShaderWrapper fragShaderY(ctx.vkd, ctx.device, binaries.get("fragY"));
    const ShaderWrapper fragShaderN(ctx.vkd, ctx.device, binaries.get("fragN"));

    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        0u,
        sampleCount,
        VK_FALSE, // This will be enabled by the shader if needed.
        0.0f,
        nullptr,
        VK_FALSE,
        VK_FALSE,
    };

    // Note we add the shaders/pipeline without sample id first in the vector, followed by the ones with it.
    std::vector<GraphicsPipelineWrapperPtr> pipelineWrappers;
    for (const bool useSampleID : {false, true})
    {
        const auto &fragShader = (useSampleID ? fragShaderY : fragShaderN);

        pipelineWrappers.emplace_back(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                                                  m_context.getDeviceExtensions(),
                                                                  m_params.constructionType));
        auto &pipelineWrapper = *pipelineWrappers.back();

        pipelineWrapper.setShaderCreateFlags(VK_SHADER_CREATE_INDIRECT_BINDABLE_BIT_EXT)
            .setPipelineCreateFlags2(VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT)
            .setDefaultRasterizationState()
            .setDefaultDepthStencilState()
            .setDefaultColorBlendState()
            .setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
            .setupVertexInputState()
            .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass.get(), 0u, vertShader)
            .setupFragmentShaderState(pipelineLayout, renderPass.get(), 0u, fragShader, nullptr,
                                      &multisampleStateCreateInfo)
            .setupFragmentOutputState(renderPass.get(), 0u, nullptr, &multisampleStateCreateInfo)
            .buildPipeline();
    };

    // Indirect execution set.
    ExecutionSetManagerPtr iesManager;
    if (useESO)
    {
        const std::vector<VkDescriptorSetLayout> setLayouts{*setLayout};
        const std::vector<vk::VkPushConstantRange> noPCRanges;

        const auto &initialPipeline = *pipelineWrappers.front();

        const std::vector<IESStageInfo> stageInfos{
            IESStageInfo{initialPipeline.getShader(VK_SHADER_STAGE_VERTEX_BIT), setLayouts},
            IESStageInfo{initialPipeline.getShader(VK_SHADER_STAGE_FRAGMENT_BIT), setLayouts},
        };
        iesManager = makeExecutionSetManagerShader(ctx.vkd, ctx.device, stageInfos, noPCRanges,
                                                   de::sizeU32(pipelineWrappers) * de::sizeU32(stageInfos));

        for (size_t i = 0u; i < pipelineWrappers.size(); ++i)
        {
            const auto &pipelineWrapper = pipelineWrappers.at(i);
            const auto baseIndex        = static_cast<uint32_t>(i);

            iesManager->addShader(baseIndex * 2u + 0u, pipelineWrapper->getShader(VK_SHADER_STAGE_VERTEX_BIT));
            iesManager->addShader(baseIndex * 2u + 1u, pipelineWrapper->getShader(VK_SHADER_STAGE_FRAGMENT_BIT));
        }
    }
    else
    {
        iesManager = makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, pipelineWrappers.front()->getPipeline(),
                                                     de::sizeU32(pipelineWrappers));

        for (size_t i = 0u; i < pipelineWrappers.size(); ++i)
            iesManager->addPipeline(static_cast<uint32_t>(i), pipelineWrappers.at(i)->getPipeline());
    }
    iesManager->update();

    // DGC commands layout.
    const auto iesInfoType = (useESO ? VK_INDIRECT_EXECUTION_SET_INFO_TYPE_SHADER_OBJECTS_EXT :
                                       VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT);
    const auto cmdsLayoutUsage =
        (m_params.preprocess ? VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_EXT : 0);
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(cmdsLayoutUsage, shaderStages, pipelineLayout.get());
    cmdsLayoutBuilder.addExecutionSetToken(0u, iesInfoType, shaderStages);
    cmdsLayoutBuilder.addDrawToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // DGC buffer contents.
    const auto maxSequences = static_cast<uint32_t>(pixelCount);

    std::vector<VkDrawIndirectCommand> drawCmds;
    drawCmds.reserve(maxSequences);

    for (uint32_t i = 0u; i < maxSequences; ++i)
    {
        // One triangle (pixel) per sequence.
        const VkDrawIndirectCommand drawCmd{
            perPixelVert,
            1u,
            (i * perPixelVert),
            0u,
        };

        drawCmds.push_back(drawCmd);
    }

    // Boolean vector indicating if sequence i should use gl_SampleID or not.
    // [N, Y, N, Y] or [Y, N, Y, N] depending on m_params.idFirst.
    std::vector<bool> useSampleID;
    useSampleID.reserve(maxSequences);
    for (uint32_t i = 0u; i < maxSequences; ++i)
    {
        const bool odd  = (i % 2u == 0u);
        const bool flag = (odd == m_params.idFirst);
        useSampleID.push_back(flag);
    }

    std::vector<uint32_t> dgcData;
    dgcData.reserve((maxSequences * cmdsLayoutBuilder.getStreamStride()) / DE_SIZEOF32(uint32_t));
    for (uint32_t i = 0u; i < maxSequences; ++i)
    {
        // baseIndex: 0 or 1 for the first pipeline wrapper or the second one in this sequence.
        const auto baseIndex = static_cast<uint32_t>(useSampleID.at(i)) % de::sizeU32(pipelineWrappers);

        if (useESO)
        {
            // Pairs of vertex and fragment shader indices.
            // id first: (2 3) (0 1) (2 3) (0 1)
            // else:     (0 1) (2 3) (0 1) (2 3)
            dgcData.push_back(baseIndex * 2u + 0u);
            dgcData.push_back(baseIndex * 2u + 1u);
        }
        else
            dgcData.push_back(baseIndex);

        pushBackElement(dgcData, drawCmds.at(i));
    }

    // DGC buffer.
    const auto dgcBufferSize = static_cast<VkDeviceSize>(de::dataSize(dgcData));
    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, dgcBufferSize);
    {
        auto &alloc = dgcBuffer.getAllocation();
        deMemcpy(alloc.getHostPtr(), de::dataOrNull(dgcData), de::dataSize(dgcData));
    }

    // Preprocess buffer.
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, iesManager->get(), *cmdsLayout,
                                         maxSequences, 0u);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    Move<VkCommandBuffer> preprocessCmdBuffer;
    if (m_params.preprocess)
        preprocessCmdBuffer = allocateCommandBuffer(ctx.vkd, ctx.device, *cmd.cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    {
        // Transition storage image to the general layout and clear it before the fragment shader stage.
        const auto clearValue       = makeClearValueColorVec4(clearColor);
        const auto clearAccess      = VK_ACCESS_TRANSFER_WRITE_BIT;
        const auto shaderAccess     = (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        const auto preClearBarrier  = makeImageMemoryBarrier(0u, clearAccess, VK_IMAGE_LAYOUT_UNDEFINED,
                                                             VK_IMAGE_LAYOUT_GENERAL, storageImg.getImage(), colorSRR);
        const auto postClearBarrier = makeMemoryBarrier(clearAccess, shaderAccess);

        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &preClearBarrier);
        ctx.vkd.cmdClearColorImage(cmdBuffer, storageImg.getImage(), VK_IMAGE_LAYOUT_GENERAL, &clearValue.color, 1u,
                                   &colorSRR);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, &postClearBarrier);
    }
    renderPass.begin(ctx.vkd, cmdBuffer, scissors.at(0u), clearColor);
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u,
                                  &descriptorSet.get(), 0u, nullptr);
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
    pipelineWrappers.front()->bind(cmdBuffer);
    {
        DGCGenCmdsInfo cmdsInfo(shaderStages, iesManager->get(), *cmdsLayout, dgcBuffer.getDeviceAddress(),
                                dgcBuffer.getSize(), preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(),
                                maxSequences, 0ull, 0u);
        if (m_params.preprocess)
        {
            beginCommandBuffer(ctx.vkd, *preprocessCmdBuffer);
            ctx.vkd.cmdPreprocessGeneratedCommandsEXT(*preprocessCmdBuffer, &cmdsInfo.get(), cmdBuffer);
            preprocessToExecuteBarrierExt(ctx.vkd, *preprocessCmdBuffer);
            endCommandBuffer(ctx.vkd, *preprocessCmdBuffer);
        }
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, makeVkBool(m_params.preprocess), &cmdsInfo.get());
    }
    renderPass.end(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, ssImg.getImage(), ssImg.getBuffer(), fbExtent.swizzle(0, 1));
    copyImageToBuffer(ctx.vkd, cmdBuffer, storageImg.getImage(), storageImg.getBuffer(), storageExtent.swizzle(0, 1),
                      VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitAndWaitWithPreprocess(ctx.vkd, ctx.device, ctx.queue, cmdBuffer, *preprocessCmdBuffer);

    invalidateAlloc(ctx.vkd, ctx.device, ssImg.getBufferAllocation());
    invalidateAlloc(ctx.vkd, ctx.device, storageImg.getBufferAllocation());

    const auto tcuFormat = mapVkFormat(format);
    tcu::ConstPixelBufferAccess resultFB(tcuFormat, fbExtent, ssImg.getBufferAllocation().getHostPtr());
    tcu::ConstPixelBufferAccess resultStorage(tcuFormat, storageExtent, storageImg.getBufferAllocation().getHostPtr());

    tcu::TextureLevel refLevelFB(tcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    tcu::PixelBufferAccess refAccessFB = refLevelFB.getAccess();
    tcu::clear(refAccessFB, geometryColor);

    tcu::TextureLevel refLevelStorage(tcuFormat, storageExtent.x(), storageExtent.y(), storageExtent.z());
    tcu::PixelBufferAccess refAccessStorage = refLevelStorage.getAccess();
    tcu::clear(refAccessStorage, clearColor);
    for (int x = 0; x < storageExtent.x(); ++x)
    {
        if (useSampleID.at(x))
        {
            for (int y = 0; y < storageExtent.y(); ++y)
                refAccessStorage.setPixel(geometryColor, x, y);
        }
        else
            refAccessStorage.setPixel(geometryColor, x, x);
    }

    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);
    auto &log = m_context.getTestContext().getLog();

    if (!tcu::floatThresholdCompare(log, "Framebuffer", "", refAccessFB, resultFB, threshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Framebuffer contains unexpected results; check log for details --");

    if (!tcu::floatThresholdCompare(log, "Storage", "", refAccessStorage, resultStorage, threshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Storage image contains unexpected results; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

#define USE_DGC_PATH 1
//#undef USE_DGC_PATH

class DynamicA2CInstance : public vkt::TestInstance
{
public:
    struct Params
    {
        PipelineConstructionType constructionType;
        bool alphaToCoverage;
        bool useIES;
        bool usePreprocess;
        bool useSampleMask;

        VkShaderStageFlags getShaderStages() const
        {
            return (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        }

        tcu::IVec3 getExtent() const
        {
            return tcu::IVec3(2, 2, 1);
        }

        uint32_t getDrawCount() const
        {
            return 4u;
        }

    protected:
        uint32_t getFragVariationCount() const
        {
            return (useIES ? getDrawCount() : 1u);
        }

    public:
        std::vector<tcu::Vec4> getFragColors() const
        {
            static const std::vector<tcu::Vec4> colorCatalogue{
                tcu::Vec4(0.0f, 1.0f, 1.0f, 1.0f),
                tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f),
                tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
                tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f),
            };

            std::vector<tcu::Vec4> colors;
            const auto fragCount = getFragVariationCount();
            const auto drawCount = getDrawCount();

            DE_ASSERT(fragCount == 1u || fragCount == drawCount);
            DE_ASSERT(de::sizeU32(colorCatalogue) == drawCount);
            DE_UNREF(drawCount); // For release builds.

            colors.reserve(fragCount);
            if (useIES)
                colors = colorCatalogue;
            else
                colors.push_back(colorCatalogue.front());
            return colors;
        }

        tcu::Vec4 getClearColor() const
        {
            return tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
        }

        VkSampleCountFlagBits getSampleCount() const
        {
            return VK_SAMPLE_COUNT_4_BIT;
        }

        tcu::IVec3 getVerifExtent() const
        {
            const auto extent      = getExtent();
            const auto sampleCount = static_cast<int>(getSampleCount());
            return tcu::IVec3(extent.x() * sampleCount, extent.y(), extent.z());
        }

        VkFormat getFormat() const
        {
            return VK_FORMAT_R8G8B8A8_UNORM;
        }

        VkImageType getImageType() const
        {
            return VK_IMAGE_TYPE_2D;
        }

        VkImageTiling getImageTiling() const
        {
            return VK_IMAGE_TILING_OPTIMAL;
        }

        VkImageUsageFlags getImageUsage() const
        {
            return (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        }
    };

    DynamicA2CInstance(Context &context, const Params &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~DynamicA2CInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const Params m_params;
};

class DynamicA2CCase : public vkt::TestCase
{
public:
    DynamicA2CCase(tcu::TestContext &testCtx, const std::string &name, const DynamicA2CInstance::Params &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~DynamicA2CCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new DynamicA2CInstance(context, m_params);
    }

protected:
    const DynamicA2CInstance::Params m_params;
};

void DynamicA2CCase::checkSupport(Context &context) const
{
#ifdef USE_DGC_PATH
    const auto stages     = m_params.getShaderStages();
    const auto bindStages = (m_params.useIES ? stages : 0u);
    DE_ASSERT(!isConstructionTypeShaderObject(m_params.constructionType));

    checkDGCExtSupport(context, stages, bindStages);
#endif

    const auto ctx         = context.getContextCommonData();
    const auto format      = m_params.getFormat();
    const auto imageType   = m_params.getImageType();
    const auto imageTiling = m_params.getImageTiling();
    const auto imageUsage  = m_params.getImageUsage();
    const auto sampleCount = m_params.getSampleCount();

    VkImageFormatProperties formatProperties;
    const auto result = ctx.vki.getPhysicalDeviceImageFormatProperties(ctx.physicalDevice, format, imageType,
                                                                       imageTiling, imageUsage, 0u, &formatProperties);

    if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
    {
        const auto formatName = getFormatSimpleName(format);
        TCU_THROW(NotSupportedError, formatName + " does not support the required usage flags");
    }
    VK_CHECK(result);

    if ((formatProperties.sampleCounts & sampleCount) != sampleCount)
    {
        const auto formatName = getFormatSimpleName(format);
        TCU_THROW(NotSupportedError, formatName + " does not support the required sample count");
    }
    const auto &eds3Features = context.getExtendedDynamicState3FeaturesEXT();
    if (!eds3Features.extendedDynamicState3AlphaToCoverageEnable)
        TCU_THROW(NotSupportedError, "extendedDynamicState3AlphaToCoverageEnable not supported");
}

void DynamicA2CCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "void main (void) {\n"
         << "    gl_Position = inPos;\n"
         << "    gl_PointSize = 1.0;\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    const auto fragColors = m_params.getFragColors();
    const auto colorCount = de::sizeU32(fragColors);

    std::string sampleMaskUsage;
    if (m_params.useSampleMask)
    {
        std::ostringstream sampleMaskUsageStream;
        sampleMaskUsageStream << "    const bool isTopLeftPixel = (gl_FragCoord.x < 1.0 && gl_FragCoord.y < 1.0);\n"
                              << "    gl_SampleMask[0] = (isTopLeftPixel ? 0 : 0xFF);\n";
        sampleMaskUsage = sampleMaskUsageStream.str();
    }

    for (uint32_t i = 0u; i < colorCount; ++i)
    {
        std::ostringstream frag;
        frag << "#version 460\n"
             << "layout (location=0) out vec4 outColor;\n"
             << "void main (void) {\n"
             << sampleMaskUsage << "    outColor = vec4" << fragColors.at(i) << ";\n"
             << "}\n";
        const auto shaderName = "frag" + std::to_string(i);
        programCollection.glslSources.add(shaderName) << glu::FragmentSource(frag.str());
    }

    // The verification shader copies sample colors from the color buffer to an output buffer.
    // Note the output image has sampleCount as many columns as the original image, to store colors for each sample.
    const auto sampleCount = static_cast<int>(m_params.getSampleCount());
    const auto extent      = m_params.getExtent();
    const auto pixelCount  = extent.x() * extent.y() * extent.z();
    const auto flagCount   = pixelCount * sampleCount;

    std::ostringstream comp;
    comp << "#version 460\n"
         << "layout (set=0, binding=0) uniform sampler2DMS resImage;\n"
         << "layout (set=0, binding=0) uniform sampler2DMS refImage;\n"
         << "layout (set=0, binding=2, std430) buffer OutputBlock { uint flags[" << flagCount << "]; } outBuffer;\n"
         << "layout (local_size_x=" << sampleCount << ", local_size_y=1, local_size_z=1) in;\n"
         << "void main (void) {\n"
         << "    const ivec2 inCoords = ivec2(gl_WorkGroupID.xy);\n"
         << "\n"
         << "    const uint sampleCount = gl_WorkGroupSize.x;\n"
         << "    const uint colCount = gl_NumWorkGroups.x;\n"
         << "    const uint rowCount = gl_NumWorkGroups.y;\n"
         << "    const uint col = gl_WorkGroupID.x;\n"
         << "    const uint row = gl_WorkGroupID.y;\n"
         << "    const uint sampleIdx = gl_LocalInvocationIndex;\n"
         << "    const uint outIndex = row * colCount * sampleCount + col * sampleCount + sampleIdx;\n"
         << "\n"
         << "    const vec4 resColor = texelFetch(resImage, inCoords, int(sampleIdx));\n"
         << "    const vec4 refColor = texelFetch(refImage, inCoords, int(sampleIdx));\n"
         << "    const uint outValue = (resColor == refColor ? 1u : 0u);\n"
         << "    outBuffer.flags[outIndex] = outValue;\n"
         << "}\n";
    programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

tcu::TestStatus DynamicA2CInstance::iterate(void)
{
    const auto &ctx            = m_context.getContextCommonData();
    const auto fbExtent        = m_params.getExtent();
    const auto floatExtent     = fbExtent.asFloat();
    const auto vkExtent        = makeExtent3D(fbExtent);
    const auto fbFormat        = m_params.getFormat();
    const auto imageType       = m_params.getImageType();
    const auto imageTiling     = m_params.getImageTiling();
    const auto fbUsage         = m_params.getImageUsage();
    const tcu::Vec4 clearColor = m_params.getClearColor();
    const auto drawCount       = m_params.getDrawCount();
    const auto perDrawVerts    = 4u;
    const auto totalVerts      = perDrawVerts * drawCount;
    const auto sampleCount     = m_params.getSampleCount();
    const auto pixelCount      = fbExtent.x() * fbExtent.y() * fbExtent.z();
    const auto totalFlags      = pixelCount * sampleCount;

    // Vertices, in 4 triangle strips with a 0.25 pixels margin from the edges of the image.
    const auto pixWidth   = 2.0f / floatExtent.x();
    const auto pixHeight  = 2.0f / floatExtent.y();
    const auto horMargin  = pixWidth * 0.25f;
    const auto vertMargin = pixHeight * 0.25f;

    const tcu::Vec4 topLeft(-1.0f + horMargin, -1.0f + vertMargin, 0.0f, 1.0f);
    const tcu::Vec4 topRight(1.0f - horMargin, -1.0f + vertMargin, 0.0f, 1.0f);
    const tcu::Vec4 bottomLeft(-1.0f + horMargin, 1.0f - vertMargin, 0.0f, 1.0f);
    const tcu::Vec4 bottomRight(1.0f - horMargin, 1.0f - vertMargin, 0.0f, 1.0f);
    const tcu::Vec4 center(0.0f, 0.0f, 0.0f, 1.0f);

    const std::vector<tcu::Vec4> positions{
        // Strip covering the top-left quadrant with some margin.
        topLeft,
        tcu::Vec4(topLeft.x(), 0.0f, 0.0f, 1.0f),
        tcu::Vec4(0.0f, topLeft.y(), 0.0f, 1.0f),
        center,

        // Strip covering the top-right quadrant with some margin.
        tcu::Vec4(0.0f, topRight.y(), 0.0f, 1.0),
        center,
        topRight,
        tcu::Vec4(topRight.x(), 0.0f, 0.0f, 1.0f),

        // Strip covering the bottom-left quadrant with some margin.
        tcu::Vec4(bottomLeft.x(), 0.0f, 0.0f, 1.0f),
        bottomLeft,
        center,
        tcu::Vec4(0.0f, bottomLeft.y(), 0.0f, 1.0f),

        // Strip covering the bottom-right quadrant with some margin.
        center,
        tcu::Vec4(0.0f, bottomRight.y(), 0.0f, 1.0f),
        tcu::Vec4(bottomRight.x(), 0.0f, 0.0f, 1.0f),
        bottomRight,
    };
    DE_ASSERT(de::sizeU32(positions) == totalVerts);
    DE_UNREF(totalVerts); // For release builds.

    // Vertex buffer.
    const auto vertexBufferInfo = makeBufferCreateInfo(de::dataSize(positions), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferInfo, MemoryRequirement::HostVisible);
    const VkDeviceSize vbOffset = 0ull;

    // Color buffers for the result and reference images.
    const VkImageCreateInfo colorCreateInfo{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0u,
        imageType,
        fbFormat,
        vkExtent,
        1u,
        1u,
        sampleCount,
        imageTiling,
        fbUsage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    ImageWithMemory colorBufferRes(ctx.vkd, ctx.device, ctx.allocator, colorCreateInfo, MemoryRequirement::Any);
    ImageWithMemory colorBufferRef(ctx.vkd, ctx.device, ctx.allocator, colorCreateInfo, MemoryRequirement::Any);

    const auto colorSRR = makeDefaultImageSubresourceRange();
    const auto colorBufferResView =
        makeImageView(ctx.vkd, ctx.device, *colorBufferRes, VK_IMAGE_VIEW_TYPE_2D, fbFormat, colorSRR);
    const auto colorBufferRefView =
        makeImageView(ctx.vkd, ctx.device, *colorBufferRef, VK_IMAGE_VIEW_TYPE_2D, fbFormat, colorSRR);

    PipelineLayoutWrapper pipelineLayout(m_params.constructionType, ctx.vkd, ctx.device);

    const auto attDesc =
        makeAttachmentDescription(0u, fbFormat, sampleCount, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
                                  VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const auto attRef  = makeAttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const auto subpass = makeSubpassDescription(0u, VK_PIPELINE_BIND_POINT_GRAPHICS, 0u, nullptr, 1u, &attRef, nullptr,
                                                nullptr, 0u, nullptr);

    const VkRenderPassCreateInfo renderPassCreateInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr, 0u, 1u, &attDesc, 1u, &subpass, 0u, nullptr,
    };

    RenderPassWrapper renderPassRes(m_params.constructionType, ctx.vkd, ctx.device, &renderPassCreateInfo);
    RenderPassWrapper renderPassRef = renderPassRes.clone();
    renderPassRes.createFramebuffer(ctx.vkd, ctx.device, *colorBufferRes, *colorBufferResView, vkExtent.width,
                                    vkExtent.height);
    renderPassRef.createFramebuffer(ctx.vkd, ctx.device, *colorBufferRef, *colorBufferRefView, vkExtent.width,
                                    vkExtent.height);

    // Modules.
    using ShaderPtr      = std::unique_ptr<ShaderWrapper>;
    const auto &binaries = m_context.getBinaryCollection();
    std::vector<ShaderPtr> fragShaders;
    ShaderWrapper vertShader(ctx.vkd, ctx.device, binaries.get("vert"));

    const auto fragColors = m_params.getFragColors();
    fragShaders.reserve(fragColors.size());

    for (uint32_t i = 0u; i < de::sizeU32(fragColors); ++i)
    {
        const auto suffix     = std::to_string(i);
        const auto shaderName = "frag" + suffix;

        fragShaders.emplace_back(new ShaderWrapper(ctx.vkd, ctx.device, binaries.get(shaderName)));
    }

    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(vkExtent));

    using PipelineWrapperPtr = std::unique_ptr<GraphicsPipelineWrapper>;

    const auto goodA2C = m_params.alphaToCoverage;
    const auto badA2C  = !goodA2C;

    const auto cmdPool       = makeCommandPool(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto resCmdBuffer  = allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto refCmdBuffer  = allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto compCmdBuffer = allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    // Result pipelines, using dynamic state.
    const std::vector<VkDynamicState> dynamicStates{VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT};

    const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        nullptr,
        0u,
        de::sizeU32(dynamicStates),
        de::dataOrNull(dynamicStates),
    };

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        0u,
        sampleCount,
        VK_FALSE,
        0.0f,
        nullptr,
        badA2C,
        VK_FALSE,
    };

#ifdef USE_DGC_PATH
    const VkPipelineCreateFlags2KHR pipelineFlags2 =
        (m_params.useIES ? VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT : 0);
    const VkShaderCreateFlagsEXT shaderFlags = (m_params.useIES ? VK_SHADER_CREATE_INDIRECT_BINDABLE_BIT_EXT : 0);
#else
    const VkPipelineCreateFlags2KHR pipelineFlags2 = 0u;
    const VkShaderCreateFlagsEXT shaderFlags       = 0u;
#endif

    std::vector<PipelineWrapperPtr> resPipelines;
    resPipelines.reserve(fragColors.size());
    for (uint32_t i = 0u; i < de::sizeU32(fragColors); ++i)
    {
        resPipelines.emplace_back(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                                              m_context.getDeviceExtensions(),
                                                              m_params.constructionType));
        auto &pipeline = *resPipelines.back();
        pipeline.setDefaultColorBlendState()
            .setDefaultDepthStencilState()
            .setDefaultMultisampleState()
            .setDefaultRasterizationState()
            .setDefaultPatchControlPoints(0u)
            .setPipelineCreateFlags2(pipelineFlags2)
            .setShaderCreateFlags(shaderFlags)
            .setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
            .setDynamicState(&dynamicStateCreateInfo)
            .setupVertexInputState()
            .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPassRef.get(), 0u, vertShader)
            .setupFragmentShaderState(pipelineLayout, renderPassRef.get(), 0u, *fragShaders.at(i), nullptr,
                                      &multisampleStateCreateInfo)
            .setupFragmentOutputState(renderPassRef.get(), 0u, nullptr, &multisampleStateCreateInfo)
            .buildPipeline();
    }

#ifdef USE_DGC_PATH
    // Commands layout.
    const auto useESO = isConstructionTypeShaderObject(m_params.constructionType);
    DE_ASSERT(!useESO); // Not handled below.
    DE_UNREF(useESO);   // For release builds.

    const auto shaderStages = m_params.getShaderStages();

    const VkIndirectCommandsLayoutUsageFlagsEXT cmdsLayoutFlags =
        (m_params.usePreprocess ? VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_EXT : 0);
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(cmdsLayoutFlags, shaderStages, pipelineLayout.get());
    if (m_params.useIES)
        cmdsLayoutBuilder.addExecutionSetToken(0u, VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT, shaderStages);
    cmdsLayoutBuilder.addDrawToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    ExecutionSetManagerPtr iesManager;
    VkIndirectExecutionSetEXT iesHandle = VK_NULL_HANDLE;

    if (m_params.useIES)
    {
        iesManager = makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, resPipelines.front()->getPipeline(),
                                                     de::sizeU32(resPipelines));
        for (uint32_t i = 0u; i < de::sizeU32(resPipelines); ++i)
            iesManager->addPipeline(i, resPipelines.at(i)->getPipeline());
        iesManager->update();
        iesHandle = iesManager->get();
    }

    // DGC buffer contents.
    const auto sequenceCount = drawCount;
    std::vector<uint32_t> dgcData;
    dgcData.reserve((sequenceCount * cmdsLayoutBuilder.getStreamStride()) / DE_SIZEOF32(uint32_t));
    for (uint32_t i = 0u; i < sequenceCount; ++i)
    {
        if (m_params.useIES)
            dgcData.push_back(i);
        dgcData.push_back(perDrawVerts); // vertexCount
        dgcData.push_back(1u);           // instanceCount
        dgcData.push_back(0u);           // firstVertex
        dgcData.push_back(0u);           // firstInstance
    }

    // DGC buffer and preprocess buffer.
    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, de::dataSize(dgcData));
    {
        auto &alloc = dgcBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(dgcData), de::dataSize(dgcData));
    }

    const auto preprocessPipeline =
        ((iesHandle != VK_NULL_HANDLE) ? VK_NULL_HANDLE : resPipelines.front()->getPipeline());
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, iesHandle, *cmdsLayout, sequenceCount, 0u,
                                         preprocessPipeline);
#endif
    Move<VkCommandBuffer> preprocessCmdBuffer;
    VkCommandBuffer cmdBuffer = *resCmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    renderPassRes.begin(ctx.vkd, cmdBuffer, scissors.at(0u), clearColor);
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vbOffset);
    ctx.vkd.cmdSetAlphaToCoverageEnableEXT(cmdBuffer, goodA2C);
#ifdef USE_DGC_PATH
    resPipelines.front()->bind(cmdBuffer); // Bind initial state.
    {
        DGCGenCmdsInfo cmdsInfo(shaderStages, iesHandle, *cmdsLayout, dgcBuffer.getDeviceAddress(), dgcBuffer.getSize(),
                                preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(), sequenceCount, 0ull,
                                0u, preprocessPipeline);

        if (m_params.usePreprocess)
        {
            preprocessCmdBuffer = allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
            beginCommandBuffer(ctx.vkd, *preprocessCmdBuffer);
            ctx.vkd.cmdPreprocessGeneratedCommandsEXT(*preprocessCmdBuffer, &cmdsInfo.get(), cmdBuffer);
            preprocessToExecuteBarrierExt(ctx.vkd, *preprocessCmdBuffer);
            endCommandBuffer(ctx.vkd, *preprocessCmdBuffer);
        }
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, makeVkBool(m_params.usePreprocess), &cmdsInfo.get());
    }
#else
    for (uint32_t i = 0u; i < drawCount; ++i)
    {
        const auto pipelineIdx = (i >= de::sizeU32(resPipelines) ? 0u : i);
        resPipelines.at(pipelineIdx)->bind(cmdBuffer);
        ctx.vkd.cmdDraw(cmdBuffer, perDrawVerts, 1u, 0u, 0u);
    }
#endif
    renderPassRes.end(ctx.vkd, cmdBuffer);
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitAndWaitWithPreprocess(ctx.vkd, ctx.device, ctx.queue, cmdBuffer, *preprocessCmdBuffer);

    // Reference pipelines. These use the right A2C value without dynamic state.
    multisampleStateCreateInfo.alphaToCoverageEnable = goodA2C;

    std::vector<PipelineWrapperPtr> refPipelines;
    refPipelines.reserve(fragColors.size());
    for (uint32_t i = 0u; i < de::sizeU32(fragColors); ++i)
    {
        refPipelines.emplace_back(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                                              m_context.getDeviceExtensions(),
                                                              m_params.constructionType));
        auto &pipeline = *refPipelines.back();
        pipeline.setDefaultColorBlendState()
            .setDefaultDepthStencilState()
            .setDefaultMultisampleState()
            .setDefaultRasterizationState()
            .setDefaultPatchControlPoints(0u)
            .setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
            .setupVertexInputState()
            .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPassRef.get(), 0u, vertShader)
            .setupFragmentShaderState(pipelineLayout, renderPassRef.get(), 0u, *fragShaders.at(i), nullptr,
                                      &multisampleStateCreateInfo)
            .setupFragmentOutputState(renderPassRef.get(), 0u, nullptr, &multisampleStateCreateInfo)
            .buildPipeline();
    }

    // Generate reference image.
    cmdBuffer = *refCmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    renderPassRef.begin(ctx.vkd, cmdBuffer, scissors.at(0u), clearColor);
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vbOffset);
    for (uint32_t i = 0u; i < drawCount; ++i)
    {
        const auto pipelineIdx = (i >= de::sizeU32(refPipelines) ? 0u : i);
        refPipelines.at(pipelineIdx)->bind(cmdBuffer);
        ctx.vkd.cmdDraw(cmdBuffer, perDrawVerts, 1u, 0u, 0u);
    }
    renderPassRef.end(ctx.vkd, cmdBuffer);
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Compare both images match using the compute shader.
    std::vector<uint32_t> flagValues(totalFlags, 0u);
    const auto flagsBufferInfo = makeBufferCreateInfo(de::dataSize(flagValues), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory flagsBuffer(ctx.vkd, ctx.device, ctx.allocator, flagsBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc = flagsBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(flagValues), de::dataSize(flagValues));
    }

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
        0.0,
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

    const auto imageDescType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(imageDescType, 2u);                 // Reference and result images.
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); // Flags buffer.
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(imageDescType, VK_SHADER_STAGE_COMPUTE_BIT);
    setLayoutBuilder.addSingleBinding(imageDescType, VK_SHADER_STAGE_COMPUTE_BIT);
    setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    const auto setLayout          = setLayoutBuilder.build(ctx.vkd, ctx.device);
    const auto compPipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout);
    const auto descriptorSet      = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

    {
        DescriptorSetUpdateBuilder updateBuilder;
        using Location          = DescriptorSetUpdateBuilder::Location;
        const auto resImageInfo = makeDescriptorImageInfo(*sampler, *colorBufferResView, VK_IMAGE_LAYOUT_GENERAL);
        const auto refImageInfo = makeDescriptorImageInfo(*sampler, *colorBufferRefView, VK_IMAGE_LAYOUT_GENERAL);
        const auto flagsBufInfo = makeDescriptorBufferInfo(*flagsBuffer, 0ull, VK_WHOLE_SIZE);
        updateBuilder.writeSingle(*descriptorSet, Location::binding(0u), imageDescType, &resImageInfo);
        updateBuilder.writeSingle(*descriptorSet, Location::binding(1u), imageDescType, &refImageInfo);
        updateBuilder.writeSingle(*descriptorSet, Location::binding(2u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                  &flagsBufInfo);
        updateBuilder.update(ctx.vkd, ctx.device);
    }

    const auto compShader   = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));
    const auto compPipeline = makeComputePipeline(ctx.vkd, ctx.device, *compPipelineLayout, *compShader);

    cmdBuffer = *compCmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    {
        const std::vector<VkImageMemoryBarrier> preUsageBarriers = {
            makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   VK_IMAGE_LAYOUT_GENERAL, *colorBufferRes, colorSRR),
            makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   VK_IMAGE_LAYOUT_GENERAL, *colorBufferRef, colorSRR),
        };
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, de::dataOrNull(preUsageBarriers),
                                      preUsageBarriers.size());
    }
    {
        // We dispatch as many groups as pixels in the image. See shader code for details.
        const auto dispatchSize = fbExtent.asUint();
        const auto bindPoint    = VK_PIPELINE_BIND_POINT_COMPUTE;

        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *compPipelineLayout, 0u, 1u, &descriptorSet.get(), 0u,
                                      nullptr);
        ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *compPipeline);
        ctx.vkd.cmdDispatch(cmdBuffer, dispatchSize.x(), dispatchSize.y(), dispatchSize.z());
    }
    {
        const auto preCopyBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &preCopyBarrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Extract updated flags from the buffer.
    {
        auto &alloc = flagsBuffer.getAllocation();
        invalidateAlloc(ctx.vkd, ctx.device, alloc);
        memcpy(de::dataOrNull(flagValues), alloc.getHostPtr(), de::dataSize(flagValues));
    }

    // Verify flags.
    bool failed = false;
    auto &log   = m_context.getTestContext().getLog();

    const auto rowSamples = fbExtent.x() * sampleCount;
    for (int s = 0; s < sampleCount; ++s)
        for (int y = 0; y < fbExtent.y(); ++y)
            for (int x = 0; x < fbExtent.x(); ++x)
            {
                const auto idx = y * rowSamples + x * sampleCount + s;
                if (flagValues.at(idx) != 1u)
                {
                    failed = true;
                    log << tcu::TestLog::Message << "Wrong value at (" << x << ", " << y << ") sample " << s
                        << tcu::TestLog::EndMessage;
                }
            }

    if (failed)
        TCU_FAIL("Multisample color buffer verification failed; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

class DynamicFSRInstance : public vkt::TestInstance
{
public:
    struct Params
    {
        PipelineConstructionType constructionType;
        bool multiSample;
        bool sampleShadingFirst;
        bool useIES;
        bool preprocess;
        bool dynamicSampleCount;

        VkShaderStageFlags getShaderStages() const
        {
            return (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        }

        VkSampleCountFlagBits getSampleCount() const
        {
            return (multiSample ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT);
        }

        VkSampleCountFlagBits getBadSampleCount() const
        {
            return (multiSample ? VK_SAMPLE_COUNT_1_BIT : VK_SAMPLE_COUNT_4_BIT);
        }

        tcu::IVec3 getExtent() const
        {
            return tcu::IVec3(16, 16, 1);
        }

        tcu::Vec4 getClearColor() const
        {
            return tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
        }

        uint32_t getDrawCount() const
        {
            return 4u;
        }

        VkFormat getFormat() const
        {
            return VK_FORMAT_R8G8B8A8_UNORM;
        }

        VkImageType getImageType() const
        {
            return VK_IMAGE_TYPE_2D;
        }

        VkImageTiling getImageTiling() const
        {
            return VK_IMAGE_TILING_OPTIMAL;
        }

        VkImageUsageFlags getFramebufferUsage() const
        {
            return (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        }

        VkImageUsageFlags getVerificationUsage() const
        {
            return (VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        }
    };

    DynamicFSRInstance(Context &context, const Params &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~DynamicFSRInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const Params m_params;
};

class DynamicFSRCase : public vkt::TestCase
{
public:
    DynamicFSRCase(tcu::TestContext &testCtx, const std::string &name, const DynamicFSRInstance::Params &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
        DE_ASSERT(!isConstructionTypeShaderObject(params.constructionType));
    }
    virtual ~DynamicFSRCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;

    TestInstance *createInstance(Context &context) const override
    {
        return new DynamicFSRInstance(context, m_params);
    }

protected:
    const DynamicFSRInstance::Params m_params;
};

void DynamicFSRCase::checkSupport(Context &context) const
{
#ifdef USE_DGC_PATH
    const auto shaderStages = m_params.getShaderStages();
    const auto bindStages   = (m_params.useIES ? shaderStages : 0u);
    checkDGCExtSupport(context, shaderStages, bindStages);
#endif
    context.requireDeviceFunctionality("VK_KHR_fragment_shading_rate");

    const auto ctx         = context.getContextCommonData();
    const auto format      = m_params.getFormat();
    const auto imageType   = m_params.getImageType();
    const auto imageTiling = m_params.getImageTiling();
    const auto imageUsage  = m_params.getFramebufferUsage();
    const auto sampleCount = m_params.getSampleCount();

    VkImageFormatProperties formatProperties;
    const auto result = ctx.vki.getPhysicalDeviceImageFormatProperties(ctx.physicalDevice, format, imageType,
                                                                       imageTiling, imageUsage, 0u, &formatProperties);

    if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
    {
        const auto formatName = getFormatSimpleName(format);
        TCU_THROW(NotSupportedError, formatName + " does not support the required usage flags");
    }
    VK_CHECK(result);

    if ((formatProperties.sampleCounts & sampleCount) != sampleCount)
    {
        const auto formatName = getFormatSimpleName(format);
        TCU_THROW(NotSupportedError, formatName + " does not support the required sample count");
    }

    if (m_params.dynamicSampleCount)
        context.requireDeviceFunctionality("VK_EXT_extended_dynamic_state3");
}

void DynamicFSRCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "void main (void) {\n"
         << "    gl_Position = inPos;\n"
         << "    gl_PointSize = 1.0;\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    // The fragment shader may have sample shading enabled (fragY) or not forced (fragN).
    const bool multiSample    = m_params.multiSample;
    const auto sampleCount    = static_cast<int>(m_params.getSampleCount());
    const auto sampleCountStr = std::to_string(sampleCount) + ".0";

    for (const auto forceSampleShading : {false, true})
    {
        // Note the red and green components are never zero, so zero can be chosen as a clear color.
        const auto srsPrefix = (forceSampleShading ? "[sampleShading: Y]" : "[sampleShading: N]");
        std::ostringstream frag;
        frag << "#version 460\n"
             << "#extension GL_EXT_debug_printf : enable\n"
             << "#extension GL_EXT_fragment_shading_rate : enable\n"
             << "layout (location=0) out vec4 outColor;\n"
             << "void main (void) {\n"
             << "    const float red = (((gl_ShadingRateEXT & gl_ShadingRateFlag2VerticalPixelsEXT) != 0) ? 1.0 : "
                "0.5);\n"
             << "    const float green = (((gl_ShadingRateEXT & gl_ShadingRateFlag2HorizontalPixelsEXT) != 0) ? 1.0 : "
                "0.5);\n"
             << "    const float blue = " << (forceSampleShading ? "(gl_SampleID + 1) / " + sampleCountStr : "0.0")
             << ";\n"
             << "    debugPrintfEXT(\"" << srsPrefix
             << " [%f, %f] r=%f g=%f b=%f\\n\", gl_FragCoord.x, gl_FragCoord.y, red, green, blue);\n"
             << "    outColor = vec4(red, green, blue, 1.0);\n"
             << "}\n";

        const auto shaderName = std::string("frag") + (forceSampleShading ? "Y" : "N");
        programCollection.glslSources.add(shaderName) << glu::FragmentSource(frag.str());
    }

    // Compute shader to translate a possibly multisample image into a single sample image, expanding the original image
    // horizontally to store the value of each sample in a different column.
    std::ostringstream comp;

    const auto srcImageType = (multiSample ? "sampler2DMS" : "sampler2D");
    const auto loadExtraArg = (multiSample ? ", int(sampleIdx)" : ", 0");

    comp << "#version 460\n"
         << "layout (set=0, binding=0) uniform " << srcImageType << " srcImage;\n"
         << "layout (set=0, binding=1, rgba8) uniform image2D dstImage;\n"
         << "layout (local_size_x=" << sampleCount << ", local_size_y=1, local_size_z=1) in;\n"
         << "void main (void) {\n"
         << "    const ivec2 srcCoords = ivec2(gl_WorkGroupID.xy);\n"
         << "\n"
         << "    const uint sampleCount = gl_WorkGroupSize.x;\n"
         << "    const uint srcCol = gl_WorkGroupID.x;\n"
         << "    const uint srcRow = gl_WorkGroupID.y;\n"
         << "    const uint sampleIdx = gl_LocalInvocationIndex;\n"
         << "    const uint dstCol = srcCol * sampleCount + sampleIdx;\n"
         << "    const uint dstRow = srcRow;\n"
         << "\n"
         << "    const ivec2 dstCoords = ivec2(dstCol, dstRow);\n"
         << "\n"
         << "    const vec4 srcColor = texelFetch(srcImage, srcCoords" << loadExtraArg << ");\n"
         << "    imageStore(dstImage, dstCoords, srcColor);\n"
         << "}\n";
    programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

tcu::TestStatus DynamicFSRInstance::iterate(void)
{
    const auto &ctx         = m_context.getContextCommonData();
    const auto fbExtent     = m_params.getExtent();
    const auto vkExtent     = makeExtent3D(fbExtent);
    const auto fbFormat     = m_params.getFormat();
    const auto fbUsage      = m_params.getFramebufferUsage();
    const auto clearColor   = m_params.getClearColor();
    const auto drawCount    = m_params.getDrawCount();
    const auto perDrawVerts = 4u;
    const auto totalVerts   = perDrawVerts * drawCount;
    const auto sampleCount  = m_params.getSampleCount();
    const auto imageType    = m_params.getImageType();
    const auto imageTiling  = m_params.getImageTiling();

    // Vertices, in 4 triangle strips covering each quadrant.
    // clang-format off
    const tcu::Vec4 topLeft      (-1.0f, -1.0f, 0.0f, 1.0f);
    const tcu::Vec4 topRight     ( 1.0f, -1.0f, 0.0f, 1.0f);
    const tcu::Vec4 bottomLeft   (-1.0f,  1.0f, 0.0f, 1.0f);
    const tcu::Vec4 bottomRight  ( 1.0f,  1.0f, 0.0f, 1.0f);
    const tcu::Vec4 center       ( 0.0f,  0.0f, 0.0f, 1.0f);
    const tcu::Vec4 centerLeft   (-1.0f,  0.0f, 0.0f, 1.0f);
    const tcu::Vec4 centerRight  ( 1.0f,  0.0f, 0.0f, 1.0f);
    const tcu::Vec4 centerTop    ( 0.0f, -1.0f, 0.0f, 1.0f);
    const tcu::Vec4 centerBottom ( 0.0f,  1.0f, 0.0f, 1.0f);
    // clang-format on

    const std::vector<tcu::Vec4> positions{
        // clang-format off
        topLeft,    centerLeft,   centerTop,   center,       // Strip covering the top-left quadrant.
        centerTop,  center,       topRight,    centerRight,  // Strip covering the top-right quadrant.
        centerLeft, bottomLeft,   center,      centerBottom, // Strip covering the bottom-left quadrant.
        center,     centerBottom, centerRight, bottomRight,  // Strip covering the bottom-right quadrant.
        // clang-format on
    };
    DE_ASSERT(de::sizeU32(positions) == totalVerts);
    DE_UNREF(totalVerts); // For release builds.

    // Vertex buffer.
    const auto vertexBufferInfo = makeBufferCreateInfo(de::dataSize(positions), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferInfo, MemoryRequirement::HostVisible);
    const VkDeviceSize vbOffset = 0ull;
    {
        auto &alloc = vertexBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(positions), de::dataSize(positions));
    }

    // Color buffers for the result and reference images.
    const VkImageCreateInfo colorCreateInfo{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0u,
        imageType,
        fbFormat,
        vkExtent,
        1u,
        1u,
        sampleCount,
        imageTiling,
        fbUsage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };
    ImageWithMemory colorBuffer(ctx.vkd, ctx.device, ctx.allocator, colorCreateInfo, MemoryRequirement::Any);
    const auto colorSRR  = makeDefaultImageSubresourceRange();
    const auto colorView = makeImageView(ctx.vkd, ctx.device, *colorBuffer, VK_IMAGE_VIEW_TYPE_2D, fbFormat, colorSRR);

    // Expanded extent, using multiple pixels horizontally, one for each sample.
    const tcu::IVec3 expandedExtent(fbExtent.x() * sampleCount, fbExtent.y(), fbExtent.z());
    const auto expandedExtentVk = makeExtent3D(expandedExtent);
    const auto expandedUsage    = m_params.getVerificationUsage();
    ImageWithBuffer verifBuffer(ctx.vkd, ctx.device, ctx.allocator, expandedExtentVk, fbFormat, expandedUsage,
                                imageType);

#ifdef USE_DGC_PATH
    const VkPipelineCreateFlags2KHR pipelineFlags2 = VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT;
    const VkShaderCreateFlagsEXT shaderFlags       = VK_SHADER_CREATE_INDIRECT_BINDABLE_BIT_EXT;
#else
    const VkPipelineCreateFlags2KHR pipelineFlags2 = 0u;
    const VkShaderCreateFlagsEXT shaderFlags       = 0u;
#endif

    const auto pipelineCount = (m_params.useIES ? 2u : 1u);
    std::vector<GraphicsPipelineWrapperPtr> pipelines;
    pipelines.reserve(pipelineCount);

    const auto &binaries = m_context.getBinaryCollection();
    ShaderWrapper vertShader(ctx.vkd, ctx.device, binaries.get("vert"));
    ShaderWrapper fragNShader(ctx.vkd, ctx.device, binaries.get("fragN"));
    ShaderWrapper fragYShader(ctx.vkd, ctx.device, binaries.get("fragY"));

    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    PipelineLayoutWrapper graphicsPipelineLayout(m_params.constructionType, ctx.vkd, ctx.device);

    const auto attDesc =
        makeAttachmentDescription(0u, fbFormat, sampleCount, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
                                  VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const auto attRef  = makeAttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const auto subpass = makeSubpassDescription(0u, VK_PIPELINE_BIND_POINT_GRAPHICS, 0u, nullptr, 1u, &attRef, nullptr,
                                                nullptr, 0u, nullptr);

    const VkRenderPassCreateInfo rpCreateInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr, 0u, 1u, &attDesc, 1u, &subpass, 0u, nullptr,
    };

    RenderPassWrapper renderPass(m_params.constructionType, ctx.vkd, ctx.device, &rpCreateInfo);
    renderPass.createFramebuffer(ctx.vkd, ctx.device, *colorBuffer, *colorView, vkExtent.width, vkExtent.height);

    const auto staticFragmentSize                           = makeExtent2D(1u, 1u);
    const auto dynamicFragmentSize                          = makeExtent2D(2u, 2u);
    const VkFragmentShadingRateCombinerOpKHR combinerOps[2] = {
        VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR,
        VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR,
    };

    VkPipelineFragmentShadingRateStateCreateInfoKHR fsrInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR,
        nullptr,
        staticFragmentSize,
        {combinerOps[0], combinerOps[1]},
    };

    VkSampleCountFlagBits staticSampleCount = sampleCount;
    if (m_params.dynamicSampleCount)
        staticSampleCount = m_params.getBadSampleCount();

    const VkPipelineMultisampleStateCreateInfo msInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        0u,
        staticSampleCount,
        VK_FALSE,
        0.0f,
        nullptr,
        VK_FALSE,
        VK_FALSE,
    };

    std::vector<VkDynamicState> dynamicStates{
        VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR,
    };

    if (m_params.dynamicSampleCount)
        dynamicStates.push_back(VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT);

    const VkPipelineDynamicStateCreateInfo dsInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        nullptr,
        0u,
        de::sizeU32(dynamicStates),
        de::dataOrNull(dynamicStates),
    };

    for (uint32_t i = 0u; i < pipelineCount; ++i)
    {
        const bool sampleShading = (i == (1u - static_cast<uint32_t>(m_params.sampleShadingFirst)));
        const auto &fragShader   = (sampleShading ? fragYShader : fragNShader);

        pipelines.emplace_back(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                                           m_context.getDeviceExtensions(), m_params.constructionType));
        auto &pipeline = *pipelines.back();

        pipeline.setPipelineCreateFlags2(pipelineFlags2)
            .setShaderCreateFlags(shaderFlags)
            .setDefaultRasterizationState()
            .setDefaultDepthStencilState()
            .setDefaultColorBlendState()
            .setDynamicState(&dsInfo)
            .setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
            .setDefaultPatchControlPoints(0u)
            .setupVertexInputState()
            .setupPreRasterizationShaderState(viewports, scissors, graphicsPipelineLayout, *renderPass, 0u, vertShader,
                                              nullptr, ShaderWrapper(), ShaderWrapper(), ShaderWrapper(), nullptr,
                                              &fsrInfo)
            .setupFragmentShaderState(graphicsPipelineLayout, *renderPass, 0u, fragShader, nullptr, &msInfo)
            .setupFragmentOutputState(*renderPass, 0u, nullptr, &msInfo)
            .buildPipeline();
    }

    // Compute pipeline that expands the multisample attachment.
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
        0.0,
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

    Move<VkDescriptorSetLayout> expandedSetLayout;
    Move<VkDescriptorPool> expandedDescPool;
    Move<VkDescriptorSet> expandedDescSet;
    const VkShaderStageFlags verifStages = (VK_SHADER_STAGE_COMPUTE_BIT);

    {
        DescriptorSetLayoutBuilder setLayoutBuilder;
        setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, verifStages);
        setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, verifStages);
        expandedSetLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);
    }
    {
        DescriptorPoolBuilder poolBuilder;
        poolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u);
        poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u);
        expandedDescPool =
            poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    }
    expandedDescSet = makeDescriptorSet(ctx.vkd, ctx.device, *expandedDescPool, *expandedSetLayout);
    {
        using Location = DescriptorSetUpdateBuilder::Location;
        DescriptorSetUpdateBuilder updateBuilder;
        const auto srcImgInfo = makeDescriptorImageInfo(*sampler, *colorView, VK_IMAGE_LAYOUT_GENERAL);
        const auto dstImgInfo =
            makeDescriptorImageInfo(VK_NULL_HANDLE, verifBuffer.getImageView(), VK_IMAGE_LAYOUT_GENERAL);
        updateBuilder.writeSingle(*expandedDescSet, Location::binding(0u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                  &srcImgInfo);
        updateBuilder.writeSingle(*expandedDescSet, Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                  &dstImgInfo);
        updateBuilder.update(ctx.vkd, ctx.device);
    }

    const auto compPipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *expandedSetLayout);
    const auto compModule         = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));
    const auto compPipeline       = makeComputePipeline(ctx.vkd, ctx.device, *compPipelineLayout, *compModule);

#ifdef USE_DGC_PATH
    // Commands layout.
    const auto useESO = isConstructionTypeShaderObject(m_params.constructionType);
    DE_ASSERT(!useESO); // Not handled below.
    DE_UNREF(useESO);   // For release builds.

    const auto shaderStages = m_params.getShaderStages();

    const VkIndirectCommandsLayoutUsageFlagsEXT cmdsLayoutFlags =
        (m_params.preprocess ? VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_EXT : 0);
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(cmdsLayoutFlags, shaderStages, graphicsPipelineLayout.get());
    if (m_params.useIES)
        cmdsLayoutBuilder.addExecutionSetToken(0u, VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT, shaderStages);
    cmdsLayoutBuilder.addDrawToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    ExecutionSetManagerPtr iesManager;
    VkIndirectExecutionSetEXT iesHandle = VK_NULL_HANDLE;

    if (m_params.useIES)
    {
        iesManager = makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, pipelines.front()->getPipeline(),
                                                     de::sizeU32(pipelines));
        for (uint32_t i = 0u; i < de::sizeU32(pipelines); ++i)
            iesManager->addPipeline(i, pipelines.at(i)->getPipeline());
        iesManager->update();
        iesHandle = iesManager->get();
    }

    // DGC buffer contents.
    const auto sequenceCount = drawCount;
    std::vector<uint32_t> dgcData;
    dgcData.reserve((sequenceCount * cmdsLayoutBuilder.getStreamStride()) / DE_SIZEOF32(uint32_t));
    for (uint32_t i = 0u; i < sequenceCount; ++i)
    {
        if (m_params.useIES)
            dgcData.push_back(i % de::sizeU32(pipelines));
        dgcData.push_back(perDrawVerts);     // vertexCount
        dgcData.push_back(1u);               // instanceCount
        dgcData.push_back(i * perDrawVerts); // firstVertex
        dgcData.push_back(0u);               // firstInstance
    }

    // DGC buffer and preprocess buffer.
    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, de::dataSize(dgcData));
    {
        auto &alloc = dgcBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(dgcData), de::dataSize(dgcData));
    }

    const auto preprocessPipeline = ((iesHandle != VK_NULL_HANDLE) ? VK_NULL_HANDLE : pipelines.front()->getPipeline());
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, iesHandle, *cmdsLayout, sequenceCount, 0u,
                                         preprocessPipeline);
#endif

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;
    Move<VkCommandBuffer> preprocessCmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vbOffset);
    ctx.vkd.cmdSetFragmentShadingRateKHR(cmdBuffer, &dynamicFragmentSize, combinerOps);
    if (m_params.dynamicSampleCount)
        ctx.vkd.cmdSetRasterizationSamplesEXT(cmdBuffer, sampleCount);
    renderPass.begin(ctx.vkd, cmdBuffer, scissors.at(0u), clearColor);
#ifdef USE_DGC_PATH
    pipelines.front()->bind(cmdBuffer); // Bind initial state.
    {
        DGCGenCmdsInfo cmdsInfo(shaderStages, iesHandle, *cmdsLayout, dgcBuffer.getDeviceAddress(), dgcBuffer.getSize(),
                                preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(), sequenceCount, 0ull,
                                0u, preprocessPipeline);

        if (m_params.preprocess)
        {
            preprocessCmdBuffer =
                allocateCommandBuffer(ctx.vkd, ctx.device, *cmd.cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
            beginCommandBuffer(ctx.vkd, *preprocessCmdBuffer);
            ctx.vkd.cmdPreprocessGeneratedCommandsEXT(*preprocessCmdBuffer, &cmdsInfo.get(), cmdBuffer);
            preprocessToExecuteBarrierExt(ctx.vkd, *preprocessCmdBuffer);
            endCommandBuffer(ctx.vkd, *preprocessCmdBuffer);
        }
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, makeVkBool(m_params.preprocess), &cmdsInfo.get());
    }
#else
    for (uint32_t i = 0u; i < drawCount; ++i)
    {
        pipelines.at(i % de::sizeU32(pipelines))->bind(cmdBuffer);
        ctx.vkd.cmdDraw(cmdBuffer, perDrawVerts, 1u, i * perDrawVerts, 0u);
    }
#endif
    renderPass.end(ctx.vkd, cmdBuffer);
    {
        // Layout transitions and barrier for the compute pipeline.
        const auto srcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        const auto dstAccess = (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        const std::vector<VkImageMemoryBarrier> preComputeBarriers{
            makeImageMemoryBarrier(srcAccess, dstAccess, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   VK_IMAGE_LAYOUT_GENERAL, *colorBuffer, colorSRR),
            makeImageMemoryBarrier(srcAccess, dstAccess, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                   verifBuffer.getImage(), colorSRR),
        };
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, de::dataOrNull(preComputeBarriers),
                                      preComputeBarriers.size());
    }
    ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *compPipeline);
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *compPipelineLayout, 0u, 1u,
                                  &expandedDescSet.get(), 0u, nullptr);
    {
        const auto uintExtent = fbExtent.asUint();
        ctx.vkd.cmdDispatch(cmdBuffer, uintExtent.x(), uintExtent.y(), uintExtent.z());
    }
    {
        copyImageToBuffer(ctx.vkd, cmdBuffer, verifBuffer.getImage(), verifBuffer.getBuffer(),
                          expandedExtent.swizzle(0, 1), VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitAndWaitWithPreprocess(ctx.vkd, ctx.device, ctx.queue, cmdBuffer, *preprocessCmdBuffer);
    //ctx.vkd.deviceWaitIdle(ctx.device);

    invalidateAlloc(ctx.vkd, ctx.device, verifBuffer.getBufferAllocation());

    const auto tcuFormat = mapVkFormat(fbFormat);
    tcu::ConstPixelBufferAccess resAccess(tcuFormat, expandedExtent, verifBuffer.getBufferAllocation().getHostPtr());
    tcu::TextureLevel refLevel(tcuFormat, expandedExtent.x(), expandedExtent.y(), expandedExtent.z());
    tcu::PixelBufferAccess refAccess = refLevel.getAccess();

    DE_ASSERT(expandedExtent.z() == 1);
    for (int y = 0; y < expandedExtent.y(); ++y)
        for (int x = 0; x < expandedExtent.x(); ++x)
        {
            const bool isLeft      = (x < expandedExtent.x() / 2);
            const bool isTop       = (y < expandedExtent.y() / 2);
            const uint32_t drawIdx = (isTop ? (isLeft ? 0u : 1u) : (isLeft ? 2u : 3u));
            DE_ASSERT(drawIdx < drawCount);
            const bool sampleShading =
                (drawIdx % pipelineCount == (1u - static_cast<uint32_t>(m_params.sampleShadingFirst)));
            const auto sampleId = (x % sampleCount);

            // These have to match the frag shader logic. Note when sample shading is enabled, the shading rate has to
            // be 1x1 according to the spec.
            const float red   = (sampleShading ? 0.5f : 1.0f);
            const float green = (sampleShading ? 0.5f : 1.0f);
            const float blue =
                (sampleShading ? static_cast<float>(sampleId + 1u) / static_cast<float>(sampleCount) : 0.0f);
            const float alpha = 1.0f;

            refAccess.setPixel(tcu::Vec4(red, green, blue, alpha), x, y);
        }

    auto &log                = m_context.getTestContext().getLog();
    const auto compThreshold = 0.005f; // 1/255 < 0.005 < 2/255
    const tcu::Vec4 threshold(compThreshold, compThreshold, compThreshold, 0.0f);

    if (!tcu::floatThresholdCompare(log, "Expanded Result (4 horizontal pixels per original pixel)", "", refAccess,
                                    resAccess, threshold, tcu::COMPARE_LOG_EVERYTHING))
        TCU_FAIL("Unexpected results in expanded color buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createDGCGraphicsMiscTestsExt(tcu::TestContext &testCtx)
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "misc"));

    const auto bindingIndices = getBindingTypeIntValues();
    const VBOUpdateInstance::Params defaultParams{{false, false, false, false}};
    for (const auto idx : bindingIndices)
    {
        VBOUpdateInstance::Params params = defaultParams;
        params.varyBinding[idx]          = true;

        const std::string testName = "vbo_update_" + params.getVariationString();
        mainGroup->addChild(new VBOUpdateCase(testCtx, testName, params));
    }
    {
        const VBOUpdateInstance::Params params{{true, true, true, true}};
        const std::string testName = "vbo_update_" + params.getVariationString();
        mainGroup->addChild(new VBOUpdateCase(testCtx, testName, params));
    }

    for (const bool shaderObjects : {false, true})
        for (const bool mesh : {false, true})
            for (const bool preProcess : {false, true})
                for (const bool useExecutionSet : {false, true})
                    for (const bool useVBOToken : {false, true})
                    {
                        if (mesh && useVBOToken)
                            continue;

                        const NormalDGCMixInstance::Params params{
                            preProcess, useExecutionSet, useVBOToken, mesh, shaderObjects,
                        };
                        const std::string testName =
                            std::string("mix_normal_dgc") + (shaderObjects ? "_shader_objects" : "") +
                            (mesh ? "_mesh" : "") + (preProcess ? "_preprocess" : "") +
                            (useExecutionSet ? "_with_ies" : "") + (useVBOToken ? "_with_vbo_token" : "");
                        mainGroup->addChild(new NormalDGCMixCase(testCtx, testName, params));
                    }

    for (const auto useShaderObjects : {false, true})
        for (const auto preprocess : {false, true})
        {
            const NullVBOInstance::Params params{useShaderObjects, preprocess};
            const auto testName = std::string("robust_vbo") + (useShaderObjects ? "_shader_objects" : "") +
                                  (preprocess ? "_preprocess" : "");
            mainGroup->addChild(new NullVBOCase(testCtx, testName, params));
        }

    for (const auto useShaderObjects : {false, true})
    {
        const MultiIfaceCase::Params params{TestType::SINGLE_EXEC, useShaderObjects};
        const auto testName = std::string("interface_matching") + (useShaderObjects ? "_shader_objects" : "");
        mainGroup->addChild(new MultiIfaceCase(testCtx, testName, params));
    }

    for (const auto useShaderObjects : {false, true})
    {
        const MultiIfaceCase::Params params{TestType::REPLACE, useShaderObjects};
        const auto testName = std::string("ies_replace") + (useShaderObjects ? "_shader_objects" : "");
        mainGroup->addChild(new MultiIfaceCase(testCtx, testName, params));
    }

    for (const auto useShaderObjects : {false, true})
    {
        const MultiIfaceCase::Params params{TestType::ADDITION, useShaderObjects};
        const auto testName = std::string("ies_add") + (useShaderObjects ? "_shader_objects" : "");
        mainGroup->addChild(new MultiIfaceCase(testCtx, testName, params));
    }

    addFunctionCaseWithPrograms(mainGroup.get(), "sequence_index_token", SequenceIndexSupport, SequenceIndexPrograms,
                                SequenceIndexRun);

    for (const auto useExecutionSet : {false, true})
    {
        const RayQueryTestInstance::Params params{useExecutionSet};
        const auto testName = std::string("ray_query") + (useExecutionSet ? "_ies" : "");
        mainGroup->addChild(new RayQueryTestCase(testCtx, testName, params));
    }

    for (const bool preProcess : {false, true})
    {
        const auto suffix   = (preProcess ? "_preprocess" : "");
        const auto testName = std::string("early_fragment_tests") + suffix;
        addFunctionCaseWithPrograms(mainGroup.get(), testName, EarlyFragmentTestsSupport, EarlyFragmentTestsPrograms,
                                    EarlyFragmentTestsRun, preProcess);
    }

    const struct
    {
        PipelineConstructionType constructionType;
        const char *suffix;
    } constructionTypes[] = {
        {PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, "monolithic"},
        {PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY, "fast_lib"},
        {PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY, "optimized_lib"},
        {PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV, "unlinked_spirv"},
    };

    {

        for (const auto &cType : constructionTypes)
            for (const auto &indirectVertexBinds : {false, true})
                for (const auto &reverseColorOrder : {false, true})
                {
                    const IESInputBindingsInstance::Params params{cType.constructionType, indirectVertexBinds,
                                                                  reverseColorOrder};
                    const auto testName = std::string("ies_increase_vtx_bindings_") + cType.suffix +
                                          (indirectVertexBinds ? "_indirect_vtx_binds" : "") +
                                          (reverseColorOrder ? "_with_holes" : "");
                    mainGroup->addChild(new IESInputBindingsCase(testCtx, testName, params));
                }
    }

    {
        const struct
        {
            TessGeomType type;
            const char *suffix;
        } tessGeomPCCases[] = {
            {TessGeomType::TESS, "tess"},
            {TessGeomType::GEOM, "geom"},
        };

        for (const auto &tessGeomPCCase : tessGeomPCCases)
            for (bool partial : {false, true})
            {
                const TessGeomPCParams params{tessGeomPCCase.type, partial};
                const auto testName =
                    std::string("tg_push_constants_") + tessGeomPCCase.suffix + (partial ? "_partial" : "");
                addFunctionCaseWithPrograms(mainGroup.get(), testName, tessGeomPushConstantsCheckSupport,
                                            tessGeomPushConstantsInitPrograms, tessGeomPushConstantsRun, params);
            }
    }

    {
        for (bool drawCountToken : {true, false})
        {
            DrawIndexBaseInstanceParams params{drawCountToken};
            const auto testName =
                std::string("indexed_draws_with_draw_index_base_instance") + (drawCountToken ? "_count" : "");
            addFunctionCaseWithPrograms(mainGroup.get(), testName, drawIndexBaseInstanceCheckSupport,
                                        drawIndexBaseInstanceInitPrograms, drawIndexBaseInstanceRun, params);
        }
    }

    {
        addFunctionCaseWithPrograms(mainGroup.get(), "sparse_vbo_token", sparseVBOCheckSupport, sparseVBOInitPrograms,
                                    sparseVBORun);
    }

    {
        for (const auto &cType : constructionTypes)
            for (const bool useExecutionSet : {false, true})
            {
                const DynVtxInputInstance::Params params{cType.constructionType, useExecutionSet};
                const auto testName =
                    std::string("dynamic_vertex_input_") + cType.suffix + (useExecutionSet ? "_execution_set" : "");
                mainGroup->addChild(new DynVtxInputCase(testCtx, testName, params));
            }
    }
    {
        const struct
        {
            NormalDGCDrawReuseInstance::Order order;
            const char *suffix;
        } orderCases[] = {
            {NormalDGCDrawReuseInstance::Order::NORMAL_DGC, "_order_normal_dgc"},
            {NormalDGCDrawReuseInstance::Order::DGC_NORMAL, "_order_dgc_normal"},
        };

        for (const auto &cType : constructionTypes)
            for (const auto &orderCase : orderCases)
                for (const bool useExecutionSet : {false, true})
                {
                    const NormalDGCDrawReuseInstance::Params params{cType.constructionType, orderCase.order,
                                                                    useExecutionSet};
                    const auto testName = std::string("reuse_dgc_for_normal_") + cType.suffix + orderCase.suffix +
                                          (useExecutionSet ? "_execution_set" : "");
                    mainGroup->addChild(new NormalDGCDrawReuseCase(testCtx, testName, params));
                }
    }

    for (const bool useExecutionSet : {false, true})
    {
        const NormalDGCNormalParams params{useExecutionSet};
        const auto testName = std::string("rebind_normal_state") + (useExecutionSet ? "_with_execution_set" : "");
        addFunctionCaseWithPrograms(mainGroup.get(), testName, normalDGCNormalCheckSupport, normalDGCNormalInitPrograms,
                                    normalDGCNormalRun, params);
    }

    {
        for (const auto &cType : constructionTypes)
            for (const bool sampleIdFirst : {false, true})
                for (const bool preprocess : {false, true})
                {
                    const SampleIDStateInstance::Params params{
                        cType.constructionType,
                        sampleIdFirst,
                        preprocess,
                    };
                    const auto testName = std::string("sample_id_state") + "_" + std::to_string(sampleIdFirst) +
                                          (preprocess ? "_preprocess" : "") + "_" + cType.suffix;
                    mainGroup->addChild(new SampleIDStateCase(testCtx, testName, params));
                }
    }

    {
        for (const auto &constructionTypeCase : constructionTypes)
        {
            if (isConstructionTypeShaderObject(constructionTypeCase.constructionType))
                continue; // With shader objects, everything is already dynamic.

            for (const bool useIES : {false, true})
                for (const bool preprocess : {false, true})
                    for (const bool useA2C : {false, true})
                        for (const bool useSampleMask : {false, true})
                        {
                            const DynamicA2CInstance::Params params{
                                constructionTypeCase.constructionType, useA2C, useIES, preprocess, useSampleMask,
                            };
                            const auto testName = constructionTypeCase.suffix + std::string("_dynamic_a2c") +
                                                  (useA2C ? "_enabled" : "_disabled") + (useIES ? "_ies" : "") +
                                                  (preprocess ? "_preprocess" : "") +
                                                  (useSampleMask ? "_sample_mask" : "");

                            mainGroup->addChild(new DynamicA2CCase(testCtx, testName, params));
                        }
        }
    }

    {
        for (const auto &constructionTypeCase : constructionTypes)
        {
            if (isConstructionTypeShaderObject(constructionTypeCase.constructionType))
                continue; // With shader objects, everything is already dynamic.

            for (const bool multiSample : {false, true})
                for (const bool sampleShadingFirst : {false, true})
                    for (const bool useIES : {false, true})
                        for (const bool preprocess : {false, true})
                            for (const bool dynamicSampleCount : {false, true})
                            {
                                const DynamicFSRInstance::Params params{
                                    constructionTypeCase.constructionType,
                                    multiSample,
                                    sampleShadingFirst,
                                    useIES,
                                    preprocess,
                                    dynamicSampleCount,
                                };
                                const auto testName =
                                    constructionTypeCase.suffix + std::string("_dynamic_fsr_sample_shading") +
                                    (sampleShadingFirst ? "_first" : "_second") + (useIES ? "_ies" : "") +
                                    (preprocess ? "_preprocess" : "") + (multiSample ? "_multisample" : "") +
                                    (dynamicSampleCount ? "_dynamic_sample_count" : "");

                                mainGroup->addChild(new DynamicFSRCase(testCtx, testName, params));
                            }
        }
    }

    return mainGroup.release();
}

} // namespace DGC
} // namespace vkt
