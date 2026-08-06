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
 * \brief Dynamic rendering suspend/resume tests common code
 *//*--------------------------------------------------------------------*/
#include "vktDynamicRenderingSuspendResumeTestsUtil.hpp"

#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkImageWithMemory.hpp"

#include "tcuImageCompare.hpp"

#include "deRandom.hpp"

#include <numeric>

namespace vkt
{
namespace renderpass
{
namespace SuspendResume
{

using namespace vk;

namespace
{

// Returns a vector of N integers chosen pseudorandomly from indices 0 to `total`-1.
std::vector<uint32_t> randomPick(de::Random rng, uint32_t total, uint32_t wanted)
{
    DE_ASSERT(wanted <= total);

    // Algo: start with the full vector, pick one random element, swap it to the first position. Leave that one alone
    // and repeat with the rest of the vector for the second position, etc.
    std::vector<uint32_t> all(total);
    std::iota(all.begin(), all.end(), 0u);

    uint32_t startIndex = 0u;     // Start index of the pick area and position to swap to.
    uint32_t remaining  = wanted; // Number of elements left to pick.

    while (remaining--)
    {
        const auto pickedIndex = rng.getInt(static_cast<int>(startIndex), static_cast<int>(total - 1u));
        std::swap(all[startIndex], all[pickedIndex]);
        ++startIndex;
    }

    return std::vector<uint32_t>(all.begin(), all.begin() + wanted);
}

} // anonymous namespace

tcu::IVec3 Params::getExtent() const
{
    if (smallFramebuffer)
        return tcu::IVec3(8, 8, 1);
    return tcu::IVec3(256, 256, 1);
}

uint32_t Params::getQuadrantCount() const
{
    return 4u;
}

uint32_t Params::getSubpixelAreas() const
{
    return 4u;
}

uint32_t Params::getSubpixelTriangleCount() const
{
    return 2u;
}

uint32_t Params::getQuadrantDraws() const
{
    return 4u;
}

uint32_t Params::getTriangleVertexCount() const
{
    return 3u;
}

VkSampleCountFlagBits Params::getSampleCount() const
{
    const VkSampleCountFlagBits count = VK_SAMPLE_COUNT_4_BIT;
    DE_ASSERT(static_cast<uint32_t>(count) == getSubpixelAreas());
    return count;
}

uint32_t Params::getMaxColorAttachmentCount() const
{
    return 4u;
}

uint32_t Params::getSeed() const
{
    return 1785228727u + (static_cast<uint32_t>(groupParams->useSecondaryCmdBuffer) << 8) +
           (static_cast<uint32_t>(resolveDepth) << 9) + seedOffset;
}

std::vector<Params::DrawInfo> Params::genDrawInfos() const
{
    const auto seed = getSeed();
    de::Random rng(seed);

    const auto quadrantCount  = getQuadrantCount();
    const auto quadrantDraws  = getQuadrantDraws();
    const auto regularDraws   = quadrantCount * quadrantDraws;
    const auto resolveDraws   = (customResolve ? quadrantDraws : 0u);
    const auto extraQuadrants = (customResolve ? 1u : 0u);
    const auto totalDraws     = regularDraws + resolveDraws;
    const auto extent         = getExtent().asUint();
    const auto totalPixels    = extent.x() * extent.y() * extent.z();
    const auto quadrantPixels = totalPixels / quadrantCount;

    std::vector<DrawInfo> drawInfos;
    drawInfos.reserve(totalDraws);
    const auto maxColorAttCount = getMaxColorAttachmentCount();

    for (uint32_t quadrantIdx = 0u; quadrantIdx < quadrantCount + extraQuadrants; ++quadrantIdx)
    {
        const bool isResolve             = (quadrantIdx >= quadrantCount);
        uint32_t remainingQuadrantPixels = (isResolve ? quadrantDraws : quadrantPixels);

        for (uint32_t drawIdx = 0u; drawIdx < quadrantDraws; ++drawIdx)
        {
            drawInfos.emplace_back();
            auto &drawInfo = drawInfos.back();

            drawInfo.remapColorAtt = rng.getBool();
            if (drawInfo.remapColorAtt)
            {
                const auto picks = randomPick(rng, maxColorAttCount, kColorAttCount);
                std::copy(picks.begin(), picks.end(), drawInfo.colorAttIndices);
            }
            if (isResolve)
            {
                drawInfo.remapInputAtt = rng.getBool();
                if (drawInfo.remapInputAtt)
                {
                    const auto picks = randomPick(rng, maxColorAttCount, kColorAttCount + 1u /*depth*/);
                    std::copy(picks.begin(), picks.begin() + kColorAttCount, drawInfo.colorInputAttIndices);
                    drawInfo.depthInputAttIndex = picks.back();
                }
                drawInfo.pixelCount = 1u; // One "large" pseudo-pixel for each quadrant in the resolve draws.
            }
            else
            {
                if (drawIdx == quadrantDraws - 1u)
                    drawInfo.pixelCount = remainingQuadrantPixels;
                else
                {
                    const uint32_t maxPixelCount =
                        remainingQuadrantPixels - (quadrantDraws - drawIdx - 1u); // For this draw.
                    drawInfo.pixelCount = static_cast<uint32_t>(rng.getInt(1, static_cast<int>(maxPixelCount)));
                    DE_ASSERT(drawInfo.pixelCount < remainingQuadrantPixels);
                }
                remainingQuadrantPixels -= drawInfo.pixelCount;
            }
        }
    }

    return drawInfos;
}

Instance::Instance(Context &context, ParamsPtr params) : vkt::TestInstance(context), m_params(params)
{
}

Case::Case(tcu::TestContext &testCtx, const std::string &name, ParamsPtr params)
    : vkt::TestCase(testCtx, name)
    , m_params(params)
{
}

void Case::checkSupport(Context &context) const
{
    if (m_params->customResolve)
        context.requireDeviceFunctionality("VK_EXT_custom_resolve");

    context.requireDeviceFunctionality("VK_KHR_dynamic_rendering_local_read");

    const auto ctx = context.getContextCommonData();
    checkPipelineConstructionRequirements(ctx.vki, ctx.physicalDevice, m_params->groupParams->pipelineConstructionType);

    const auto &limits = context.getDeviceProperties().limits;
    if (!limits.standardSampleLocations)
        TCU_THROW(NotSupportedError, "standardSampleLocations not supported");

    DE_ASSERT(m_params->groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING);
}

namespace
{

template <class T, size_t N>
std::vector<T> toVector(const T (&array)[N])
{
    return std::vector<T>(array, array + N);
}

} // anonymous namespace

void Case::initPrograms(vk::SourceCollections &dst) const
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "layout (location=1) in vec4 inColor0;\n"
         << "layout (location=2) in vec4 inColor1;\n"
         << "layout (location=0) out vec4 outColor0;\n"
         << "layout (location=1) out vec4 outColor1;\n"
         << "void main() {\n"
         << "    gl_Position = inPos;\n"
         << "    outColor0 = inColor0;\n"
         << "    outColor1 = inColor1;\n"
         << "}\n";
    dst.glslSources.add("vert") << glu::VertexSource(vert.str());

    const auto quadrantCount = m_params->getQuadrantCount();
    const auto quadrantDraws = m_params->getQuadrantDraws();
    const auto regularDraws  = static_cast<size_t>(quadrantCount * quadrantDraws);

    const auto drawInfos = m_params->genDrawInfos();
    for (size_t i = 0; i < drawInfos.size(); ++i)
    {
        const auto &drawInfo = drawInfos.at(i);
        if (i < regularDraws)
        {
            // Regular draw maybe with color att remapping.
            auto colorAttIndices = toVector(drawInfo.colorAttIndices);
            std::sort(colorAttIndices.begin(), colorAttIndices.end());

            std::ostringstream frag;
            frag << "#version 460\n";
            for (size_t j = 0; j < colorAttIndices.size(); ++j)
            {
                frag << "layout (location=" << colorAttIndices.at(j) << ") out vec4 outColor" << j << ";\n";
                frag << "layout (location=" << j << ") in vec4 inColor" << j << ";\n";
            }
            frag << "void main() {\n";
            for (size_t j = 0; j < colorAttIndices.size(); ++j)
                frag << "    outColor" << j << " = inColor" << j << ";\n";
            frag << "}\n";

            const auto shaderName = "frag-regular-" + std::to_string(i);
            dst.glslSources.add(shaderName) << glu::FragmentSource(frag.str());
        }
        else
        {
            DE_ASSERT(m_params->customResolve);

            // Resolve draw maybe with color att remapping and input attachment remapping.
            auto colorAttIndices = toVector(drawInfo.colorAttIndices);
            std::sort(colorAttIndices.begin(), colorAttIndices.end());

            auto colorInputAttIndices = toVector(drawInfo.colorInputAttIndices);

            std::ostringstream frag;
            frag << "#version 460\n\n";
            frag << "#extension GL_EXT_optional_input_attachment_index : enable\n";

            for (size_t j = 0; j < colorAttIndices.size(); ++j)
                frag << "layout (location=" << colorAttIndices.at(j) << ") out vec4 outColor" << j << ";\n";
            frag << "\n";

            for (size_t j = 0; j < colorInputAttIndices.size(); ++j)
                frag << "layout (set=0, binding=" << j << ", input_attachment_index=" << colorInputAttIndices.at(j)
                     << ") uniform subpassInputMS ia" << j << ";\n";
            std::string depthIndexQualifier;
            if (drawInfo.remapInputAtt)
                depthIndexQualifier = ", input_attachment_index=" + std::to_string(drawInfo.depthInputAttIndex);
            frag << "layout (set=0, binding=" << colorInputAttIndices.size() << depthIndexQualifier
                 << ") uniform subpassInputMS iaDepth;\n\n";

            frag << "void main() {\n"
                 << "    const float depth = subpassLoad(iaDepth, 0).x;\n";

            for (size_t j = 0; j < colorInputAttIndices.size(); ++j)
                frag << "    outColor" << j << " = vec4(subpassLoad(ia" << j
                     << ", 0).rgb * depth, 1.0);\n"; // Note sample 0.

            if (m_params->resolveDepth)
                frag << "    gl_FragDepth = depth;\n";

            frag << "}\n";

            const auto shaderName = "frag-resolve-" + std::to_string(i);
            dst.glslSources.add(shaderName) << glu::FragmentSource(frag.str());
        }
    }
}

vkt::TestInstance *Case::createInstance(Context &context) const
{
    return new Instance(context, m_params);
}

namespace
{

// Converts a tcu::Vector of any size to tcu::Vec4, making sure the last component is 1.0f if needed.
template <class T, int Size>
tcu::Vec4 toVec4(const tcu::Vector<T, Size> &vec)
{
    tcu::Vec4 result(0.0f, 0.0f, 0.0f, 1.0f);
    const auto orig = vec.asFloat();
    memcpy(result.m_data, orig.m_data, sizeof(float) * std::min(4, Size));
    return result;
}

const Params::DrawInfo *getFirstDrawInfo(const std::vector<Params::DrawInfo> &drawInfos, uint32_t pixelVertexCount,
                                         uint32_t vertIndex)
{
    uint32_t accumVtxCount = 0u;
    for (const auto &drawInfo : drawInfos)
    {
        const auto drawVtxCount = drawInfo.pixelCount * pixelVertexCount;
        if (accumVtxCount + drawVtxCount > vertIndex)
            return &drawInfo;
        accumVtxCount += drawVtxCount;
    }
    return nullptr;
}

void beginSecondaryWithInheritance(const DeviceInterface &vkd, VkCommandBuffer cmdBuffer,
                                   const VkRenderingInfo &renderingInfo,
                                   const VkPipelineRenderingCreateInfo &pipelineRenderingInfo,
                                   VkSampleCountFlagBits sampleCount, VkRenderingAttachmentLocationInfo *locations,
                                   VkRenderingInputAttachmentIndexInfo *indices
#ifndef CTS_USES_VULKANSC
                                   ,
                                   VkCustomResolveCreateInfoEXT *customResolve
#endif
)
{
    VkCommandBufferInheritanceInfo inheritanceInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        nullptr,
        VK_NULL_HANDLE,
        0u,
        VK_NULL_HANDLE,
        VK_FALSE,
        0u,
        0u,
    };

    VkCommandBufferInheritanceRenderingInfo inheritanceRenderingInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO,
        nullptr,
        (renderingInfo.flags & (~(VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT))),
        pipelineRenderingInfo.viewMask,
        pipelineRenderingInfo.colorAttachmentCount,
        pipelineRenderingInfo.pColorAttachmentFormats,
        pipelineRenderingInfo.depthAttachmentFormat,
        pipelineRenderingInfo.stencilAttachmentFormat,
        sampleCount,
    };

    const auto addInheritanceInfo = makeStructChainAdder(&inheritanceInfo);

    addInheritanceInfo(&inheritanceRenderingInfo);

    if (locations)
        addInheritanceInfo(locations);

    if (indices)
        addInheritanceInfo(indices);

#ifndef CTS_USES_VULKANSC
    if (customResolve)
        addInheritanceInfo(customResolve);
#endif

    const auto usageFlags = static_cast<VkCommandBufferUsageFlags>(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);

    const VkCommandBufferBeginInfo beginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        usageFlags,                                  // VkCommandBufferUsageFlags flags;
        &inheritanceInfo,                            // const VkCommandBufferInheritanceInfo* pInheritanceInfo;
    };

    vkd.beginCommandBuffer(cmdBuffer, &beginInfo);
}

const Params::DrawInfo *getResolveDrawInfo(const std::vector<Params::DrawInfo> &drawInfos, ParamsPtr params, int x,
                                           int y)
{
    const auto extent        = params->getExtent();
    const auto quadrantCount = params->getQuadrantCount();

    if (de::sizeU32(drawInfos) < quadrantCount)
        return nullptr;

    // One resolve draw per quadrant.
    const auto firstResolveDrawIndex = drawInfos.size() - quadrantCount;
    const auto quadrantExtent        = extent / tcu::IVec3(2, 2, 1);
    const auto quadX                 = x / quadrantExtent.x();
    const auto quadY                 = y / quadrantExtent.y();
    const auto quadrantIndex         = quadY * 2 + quadX;
    return &drawInfos.at(firstResolveDrawIndex + quadrantIndex);
}

} // anonymous namespace

tcu::TestStatus Instance::iterate()
{
    const auto &context = m_context;
    const auto &params  = m_params;

    const auto ctx            = context.getContextCommonData();
    const auto extent         = params->getExtent();
    const auto extentU        = extent.asUint();
    const auto extentFloat    = extent.asFloat();
    const auto extentFloat4   = tcu::Vec4(extentFloat.x(), extentFloat.y(), extentFloat.z(), 1.0f);
    const auto quadrantCount  = params->getQuadrantCount();
    const auto quadrantExtent = extentU / tcu::UVec3(2u, 2u, 1u);
    const auto quadrantPixels = quadrantExtent.x() * quadrantExtent.y();
    const bool useSecondaries = params->groupParams->useSecondaryCmdBuffer;

    if (useSecondaries)
        DE_ASSERT(!params->groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass);

    const auto regularPixelCount = extentU.x() * extentU.y() * extentU.z();
    DE_ASSERT(regularPixelCount == quadrantPixels * quadrantCount);

    const std::vector<tcu::UVec3> quadrantOffsets{
        // In pixels.
        tcu::UVec3(0u, 0u, 0u),
        tcu::UVec3(quadrantExtent.x(), 0u, 0u),
        tcu::UVec3(0u, quadrantExtent.y(), 0u),
        tcu::UVec3(quadrantExtent.x(), quadrantExtent.y(), 0u),
    };
    DE_ASSERT(de::sizeU32(quadrantOffsets) == quadrantCount);

    // In pixels.
    const float subpixelWidth  = 0.5f;
    const float subpixelHeight = 0.5f;

    const std::vector<tcu::Vec4> subpixelAreaOffsets{
        tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
        tcu::Vec4(subpixelWidth, 0.0f, 0.0f, 0.0f),
        tcu::Vec4(0.0f, subpixelHeight, 0.0f, 0.0f),
        tcu::Vec4(subpixelWidth, subpixelHeight, 0.0f, 0.0f),
    };
    const auto subpixelAreas = params->getSubpixelAreas();
    DE_ASSERT(de::sizeU32(subpixelAreaOffsets) == subpixelAreas);

    struct VertexData
    {
        tcu::Vec4 position;
        tcu::Vec4 colors[Params::kColorAttCount];

        VertexData(const tcu::Vec4 &position_, const tcu::Vec4 (&colors_)[Params::kColorAttCount]) : position(position_)
        {
            memcpy(colors, colors_, sizeof(colors));
        }
    };
    std::vector<VertexData> vertices;
    vertices.reserve(2u * regularPixelCount);

    const tcu::Vec4 normSize(2.0f, 2.0f, 1.0f, 1.0f);
    const tcu::Vec4 normOffset(-1.0f, -1.0f, 0.0f, 0.0f);

    const auto toFramebufferCoords = [&](const tcu::Vec4 &coords)
    { return coords / extentFloat4 * normSize + normOffset; };

    // Vertices for regular draws.
    de::Random rng(params->getSeed());

    for (const auto &quadrantOffset : quadrantOffsets)
        for (uint32_t y = 0u; y < quadrantExtent.y(); ++y)
            for (uint32_t x = 0u; x < quadrantExtent.x(); ++x)
            {
                const auto corner3 = (tcu::UVec3(x, y, 0) + quadrantOffset).asFloat();
                auto corner        = toVec4(corner3);

                for (const auto &offset : subpixelAreaOffsets)
                {
                    // Set depth for the whole subpixel area. Note we will clear depth to 0.5, so this passes or not.
                    const bool near     = rng.getBool();
                    const auto minDepth = (near ? 0.0f : 0.75f);
                    const auto maxDepth = (near ? 0.25f : 1.0f);
                    const auto depth    = rng.getFloat(minDepth, maxDepth);
                    corner.z()          = depth;

                    tcu::Vec4 colors[Params::kColorAttCount];
                    for (uint32_t i = 0u; i < Params::kColorAttCount; ++i)
                    {
                        const float r = rng.getFloat();
                        const float g = rng.getFloat();
                        const float b = rng.getFloat();
                        colors[i]     = tcu::Vec4(r, g, b, 1.0f);
                    }

                    const auto topLeft     = corner + offset;
                    const auto topRight    = topLeft + tcu::Vec4(subpixelWidth, 0.0f, 0.0f, 0.0f);
                    const auto bottomLeft  = topLeft + tcu::Vec4(0.0f, subpixelHeight, 0.0f, 0.0f);
                    const auto bottomRight = topLeft + tcu::Vec4(subpixelWidth, subpixelHeight, 0.0f, 0.0f);

                    // Convert those to a triangle list, normalized.
                    const auto normTopLeft     = toFramebufferCoords(topLeft);
                    const auto normTopRight    = toFramebufferCoords(topRight);
                    const auto normBottomLeft  = toFramebufferCoords(bottomLeft);
                    const auto normBottomRight = toFramebufferCoords(bottomRight);

                    vertices.emplace_back(normTopLeft, colors);
                    vertices.emplace_back(normBottomLeft, colors);
                    vertices.emplace_back(normTopRight, colors);

                    vertices.emplace_back(normBottomLeft, colors);
                    vertices.emplace_back(normBottomRight, colors);
                    vertices.emplace_back(normTopRight, colors);
                }
            }

    const auto subpixelVertexCount = params->getSubpixelTriangleCount() * params->getTriangleVertexCount();
    const auto pixelVertexCount    = subpixelAreas * subpixelVertexCount;
    DE_ASSERT(de::sizeU32(vertices) == regularPixelCount * pixelVertexCount);

    // Resolve draws, using a draw per quadrant.
    for (const auto &quadrantOffset : quadrantOffsets)
    {
        const auto topLeft     = quadrantOffset;
        const auto topRight    = topLeft + quadrantOffsets.at(1u);
        const auto bottomLeft  = topLeft + quadrantOffsets.at(2u);
        const auto bottomRight = topLeft + quadrantOffsets.at(3u);

        const auto topLeftV4     = toVec4(topLeft);
        const auto topRightV4    = toVec4(topRight);
        const auto bottomLeftV4  = toVec4(bottomLeft);
        const auto bottomRightV4 = toVec4(bottomRight);

        const auto normTopLeft     = toFramebufferCoords(topLeftV4);
        const auto normTopRight    = toFramebufferCoords(topRightV4);
        const auto normBottomLeft  = toFramebufferCoords(bottomLeftV4);
        const auto normBottomRight = toFramebufferCoords(bottomRightV4);

        // Colors are irrelevant for the resolve frag shader, but since the vert shader is shared and the vertices
        // vector is common...
        tcu::Vec4 colors[Params::kColorAttCount];

        vertices.emplace_back(normTopLeft, colors);
        vertices.emplace_back(normBottomLeft, colors);
        vertices.emplace_back(normTopRight, colors);

        vertices.emplace_back(normBottomLeft, colors);
        vertices.emplace_back(normBottomRight, colors);
        vertices.emplace_back(normTopRight, colors);
    }

    const auto vertexBufferSize       = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vertexBufferUsage      = static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    const auto vertexBufferCreateInfo = makeBufferCreateInfo(vertexBufferSize, vertexBufferUsage);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferCreateInfo, HostIntent::W);
    {
        auto &alloc = vertexBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(vertices), de::dataSize(vertices));
        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    using ImageWithMemoryPtr = std::unique_ptr<ImageWithMemory>;
    using ImageWithBufferPtr = std::unique_ptr<ImageWithBuffer>;

    struct ImageResources
    {
        ImageWithMemoryPtr msImage;
        Move<VkImageView> msImageView;
        ImageWithBufferPtr ssImage;

        ImageResources(const DeviceInterface &vkd, VkDevice device, Allocator &allocator, ParamsPtr params,
                       VkFormat format, bool createSingleSample)
        {
            const auto imageType     = VK_IMAGE_TYPE_2D;
            const auto imageViewType = VK_IMAGE_VIEW_TYPE_2D;
            const auto extent        = makeExtent3D(params->getExtent());
            const bool isDS          = isDepthStencilFormat(format);
            const auto srr = makeImageSubresourceRange((isDS ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
                                                       0u, 1u, 0u, 1u);
            const auto msImageUsage =
                (static_cast<VkImageUsageFlags>(isDS ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT :
                                                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
                 VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
            const auto ssImageUsage = (msImageUsage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

            const VkImageCreateInfo createInfo{
                VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                nullptr,
                0u,
                imageType,
                format,
                extent,
                1u,
                1u,
                params->getSampleCount(),
                VK_IMAGE_TILING_OPTIMAL,
                msImageUsage,
                VK_SHARING_MODE_EXCLUSIVE,
                0u,
                nullptr,
                VK_IMAGE_LAYOUT_UNDEFINED,
            };
            msImage.reset(new ImageWithMemory(vkd, device, allocator, createInfo, MemoryRequirement::Any));
            msImageView = makeImageView(vkd, device, msImage->get(), imageViewType, format, srr);

            if (createSingleSample)
                ssImage.reset(
                    new ImageWithBuffer(vkd, device, allocator, extent, format, ssImageUsage, imageType, srr));
        }
    };
    using ImageResourcesPtr = std::unique_ptr<ImageResources>;

    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto depthFormat = VK_FORMAT_D16_UNORM;

    std::vector<ImageResourcesPtr> colorBuffers;
    colorBuffers.reserve(Params::kColorAttCount);
    for (uint32_t i = 0u; i < Params::kColorAttCount; ++i)
        colorBuffers.emplace_back(new ImageResources(ctx.vkd, ctx.device, ctx.allocator, params, colorFormat, true));

    ImageResources depthBuffer(ctx.vkd, ctx.device, ctx.allocator, params, depthFormat, params->resolveDepth);

    // Information about the dynamic render pass.
    const std::vector<VkViewport> viewports(1u, makeViewport(extent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(extent));

    const auto totalAttCount = Params::kColorAttCount + 1u /*depth*/;
    std::vector<VkRenderingAttachmentInfo> attachmentInfos;
    attachmentInfos.reserve(totalAttCount);

    const auto attLayout    = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ;
    const auto clearColorV4 = tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    const auto clearColor   = makeClearValueColor(clearColorV4);
    const auto clearDepth   = makeClearValueDepthStencil(0.5f, 0u); // Note we clear to 0.5. See geometry generation.
#ifndef CTS_USES_VULKANSC
    const auto colorResolveMode =
        (params->customResolve ? VK_RESOLVE_MODE_CUSTOM_BIT_EXT : VK_RESOLVE_MODE_AVERAGE_BIT);
#else
    DE_ASSERT(!params->customResolve);
    const auto colorResolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
#endif

    for (uint32_t i = 0u; i < Params::kColorAttCount; ++i)
    {
        const auto &colorBuffer = *colorBuffers.at(i);
        attachmentInfos.push_back(VkRenderingAttachmentInfo{
            VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            nullptr,
            colorBuffer.msImageView.get(),
            attLayout,
            colorResolveMode,
            colorBuffer.ssImage->getImageView(),
            attLayout,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            clearColor,
        });
    }

    // Depth.
    attachmentInfos.push_back(VkRenderingAttachmentInfo{
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        nullptr,
        depthBuffer.msImageView.get(),
        attLayout,
#ifndef CTS_USES_VULKANSC
        (params->resolveDepth ?
             (params->customResolve ? VK_RESOLVE_MODE_CUSTOM_BIT_EXT : VK_RESOLVE_MODE_SAMPLE_ZERO_BIT) :
             VK_RESOLVE_MODE_NONE),
#else
        (params->resolveDepth ? VK_RESOLVE_MODE_SAMPLE_ZERO_BIT : VK_RESOLVE_MODE_NONE),
#endif
        (params->resolveDepth ? depthBuffer.ssImage->getImageView() : VK_NULL_HANDLE),
        attLayout,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        clearDepth,
    });

    VkRenderingFlags baseRenderingFlags = 0u;
#ifndef CTS_USES_VULKANSC
    if (params->customResolve)
        baseRenderingFlags |= VK_RENDERING_CUSTOM_RESOLVE_BIT_EXT;
#endif
    if (useSecondaries)
        baseRenderingFlags |= VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;

    const VkRenderingInfo baseRenderingInfo{
        VK_STRUCTURE_TYPE_RENDERING_INFO,
        nullptr,
        baseRenderingFlags,
        scissors.front(),
        1u,
        0u,
        de::sizeU32(attachmentInfos) - 1u,
        de::dataOrNull(attachmentInfos),
        &attachmentInfos.back(),
        nullptr,
    };

    const std::vector<VkFormat> colorFormats(Params::kColorAttCount, colorFormat);

    VkPipelineRenderingCreateInfo pipelineRenderingInfo{
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        nullptr,
        0u,
        de::sizeU32(colorFormats),
        de::dataOrNull(colorFormats),
        depthFormat,
        VK_FORMAT_UNDEFINED,
    };

    const auto quadrantDraws = params->getQuadrantDraws();
    const auto regularDraws  = static_cast<size_t>(quadrantCount * quadrantDraws);

    using GraphicsPipelineWrapperPtr    = std::unique_ptr<GraphicsPipelineWrapper>;
    using GraphicsPipelineWrapperPtrVec = std::vector<GraphicsPipelineWrapperPtr>;

    const auto drawInfos = params->genDrawInfos();
    GraphicsPipelineWrapperPtrVec pipelines;
    pipelines.reserve(drawInfos.size());

    const VkVertexInputBindingDescription vertexBinding{
        0u,
        DE_SIZEOF32(VertexData),
        VK_VERTEX_INPUT_RATE_VERTEX,
    };

    std::vector<VkVertexInputAttributeDescription> vertexAttributes;
    vertexAttributes.reserve(Params::kColorAttCount + 1u /*position*/);

    vertexAttributes.push_back(makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u));
    for (uint32_t i = 0u; i < Params::kColorAttCount; ++i)
    {
        const auto location = i + 1u;
        vertexAttributes.push_back(makeVertexInputAttributeDescription(location, 0u, VK_FORMAT_R32G32B32A32_SFLOAT,
                                                                       DE_SIZEOF32(tcu::Vec4) * location));
    }

    const VkPipelineVertexInputStateCreateInfo vertexInputState{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        nullptr,
        0u,
        1u,
        &vertexBinding,
        de::sizeU32(vertexAttributes),
        de::dataOrNull(vertexAttributes),
    };

    const auto constructionType = params->groupParams->pipelineConstructionType;
    PipelineLayoutWrapper emptyLayout(constructionType, ctx.vkd, ctx.device);

    const auto &binaries = context.getBinaryCollection();
    ShaderWrapper vertShader(ctx.vkd, ctx.device, binaries.get("vert"));

    const VkPipelineDepthStencilStateCreateInfo regularDepthStencilState{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        nullptr,
        0u,
        VK_TRUE,
        VK_TRUE,
        VK_COMPARE_OP_GREATER,
        VK_FALSE,
        VK_FALSE,
        {},
        {},
        0.0f,
        1.0f,
    };

    const VkPipelineDepthStencilStateCreateInfo resolveDepthStencilState{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        nullptr,
        0u,
        makeVkBool(params->resolveDepth),
        makeVkBool(params->resolveDepth),
        VK_COMPARE_OP_ALWAYS,
        VK_FALSE,
        VK_FALSE,
        {},
        {},
        0.0f,
        1.0f,
    };

    const VkPipelineMultisampleStateCreateInfo regularMultisampleState{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        0u,
        params->getSampleCount(),
        VK_FALSE,
        0.0f,
        nullptr,
        VK_FALSE,
        VK_FALSE,
    };

    const VkPipelineMultisampleStateCreateInfo resolveMultisampleState{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        0u,
        VK_SAMPLE_COUNT_1_BIT,
        VK_FALSE,
        0.0f,
        nullptr,
        VK_FALSE,
        VK_FALSE,
    };

#ifndef CTS_USES_VULKANSC
    VkCustomResolveCreateInfoEXT pipelineCustomResolve{
        VK_STRUCTURE_TYPE_CUSTOM_RESOLVE_CREATE_INFO_EXT,
        nullptr,
        VK_FALSE,
        pipelineRenderingInfo.colorAttachmentCount,
        pipelineRenderingInfo.pColorAttachmentFormats,
        (params->resolveDepth ? pipelineRenderingInfo.depthAttachmentFormat : VK_FORMAT_UNDEFINED),
        pipelineRenderingInfo.stencilAttachmentFormat,
    };
    const auto pPipelineCustomResolve = (params->customResolve ? &pipelineCustomResolve : nullptr);
#else
    const PipelineCustomResolveCreateInfoWrapper pPipelineCustomResolve = nullptr;
#endif

    VkPipelineColorBlendAttachmentState blendAttState;
    memset(&blendAttState, 0, sizeof(blendAttState));
    blendAttState.colorWriteMask = 0xFu;

    const std::vector<VkPipelineColorBlendAttachmentState> blendAttStates(Params::kColorAttCount, blendAttState);

    const VkPipelineColorBlendStateCreateInfo colorBlendState{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        nullptr,
        0u,
        VK_FALSE,
        VK_LOGIC_OP_CLEAR,
        de::sizeU32(blendAttStates),
        de::dataOrNull(blendAttStates),
        {0.0f, 0.0f, 0.0f, 0.0f},
    };

    // Descriptors and layouts for the resolve pipelines.
    const auto descriptorType   = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    const auto descriptorStages = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto descriptorCount  = totalAttCount;

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descriptorType, descriptorCount);
    const auto descPool = poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    DescriptorSetLayoutBuilder setLayoutBuilder;
    for (uint32_t i = 0u; i < descriptorCount; ++i)
        setLayoutBuilder.addSingleBinding(descriptorType, descriptorStages);
    const auto setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);

    PipelineLayoutWrapper resolvePipelineLayout(constructionType, ctx.vkd, ctx.device, *setLayout);
    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descPool, *setLayout);

    DescriptorSetUpdateBuilder setUpdateBuilder;
    const auto binding = DescriptorSetUpdateBuilder::Location::binding;
    for (uint32_t i = 0u; i < Params::kColorAttCount; ++i)
    {
        const auto descInfo = makeDescriptorImageInfo(VK_NULL_HANDLE, colorBuffers.at(i)->msImageView.get(), attLayout);
        setUpdateBuilder.writeSingle(*descriptorSet, binding(i), descriptorType, &descInfo);
    }
    {
        const auto descInfo = makeDescriptorImageInfo(VK_NULL_HANDLE, depthBuffer.msImageView.get(), attLayout);
        setUpdateBuilder.writeSingle(*descriptorSet, binding(Params::kColorAttCount), descriptorType, &descInfo);
    }
    setUpdateBuilder.update(ctx.vkd, ctx.device);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    using CommandBufferPtr = std::unique_ptr<Move<VkCommandBuffer>>;
    std::vector<CommandBufferPtr> secondaries;
    secondaries.reserve(drawInfos.size());

    const auto makeNewSecondary = [&]()
    {
        secondaries.emplace_back(new Move<VkCommandBuffer>(
            allocateCommandBuffer(ctx.vkd, ctx.device, *cmd.cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY)));
        return secondaries.back()->get();
    };

    beginCommandBuffer(ctx.vkd, cmdBuffer);

    {
        // Move all images to the proper layouts.
        std::vector<VkImageMemoryBarrier> barriers;
        barriers.reserve(totalAttCount * 2u /*single sample and multisample images*/);

        const auto allAccess = static_cast<VkAccessFlags>(
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        const auto colorSRR = makeDefaultImageSubresourceRange();

        for (uint32_t i = 0u; i < Params::kColorAttCount; ++i)
        {
            barriers.push_back(makeImageMemoryBarrier(0u, allAccess, VK_IMAGE_LAYOUT_UNDEFINED, attLayout,
                                                      colorBuffers.at(i)->msImage->get(), colorSRR));
            barriers.push_back(makeImageMemoryBarrier(0u, allAccess, VK_IMAGE_LAYOUT_UNDEFINED, attLayout,
                                                      colorBuffers.at(i)->ssImage->getImage(), colorSRR));
        }
        {
            const auto depthSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u);
            barriers.push_back(makeImageMemoryBarrier(0u, allAccess, VK_IMAGE_LAYOUT_UNDEFINED, attLayout,
                                                      depthBuffer.msImage->get(), depthSRR));
            if (params->resolveDepth)
            {
                DE_ASSERT(depthBuffer.ssImage.get() != VK_NULL_HANDLE);
                barriers.push_back(makeImageMemoryBarrier(0u, allAccess, VK_IMAGE_LAYOUT_UNDEFINED, attLayout,
                                                          depthBuffer.ssImage->getImage(), depthSRR));
            }
        }

        const auto dstStages = static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                                                 VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                                                 VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, 0u, dstStages, barriers.data(), barriers.size());
    }

    uint32_t quadrantDrawIndex  = 0u;
    bool defaultColorLocations  = true; // We will track the expected state with this. vkCmdBeginRendering resets it.
    bool defaultInputAttIndices = true; // We will track the expected state with this. vkCmdBeginRendering resets it.
    uint32_t prevPixels         = 0u;

    VkRenderingInfo lastRenderingInfo = initVulkanStructure();

    for (size_t i = 0; i < drawInfos.size(); ++i)
    {
        const auto &drawInfo = drawInfos.at(i);

        if (quadrantDrawIndex == 0u)
        {
            if (i != regularDraws)
            {
                // End the previous render pass, but not the first one.
                if (i > 0u)
                    ctx.vkd.cmdEndRendering(cmdBuffer);

                // Begin a new render pass.
                lastRenderingInfo = baseRenderingInfo;
                if (i > 0u)
                    lastRenderingInfo.flags |= VK_RENDERING_RESUMING_BIT;
                if (i < regularDraws - quadrantDraws)
                    lastRenderingInfo.flags |= VK_RENDERING_SUSPENDING_BIT;

                ctx.vkd.cmdBeginRendering(cmdBuffer, &lastRenderingInfo);
                defaultColorLocations  = true;
                defaultInputAttIndices = true;
            }
            else
            {
                DE_ASSERT(params->customResolve);

                // Just before the resolve draws in the last render pass.
                const auto srcAccess = static_cast<VkAccessFlags>(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                                                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
                const auto dstAccess = static_cast<VkAccessFlags>(VK_ACCESS_INPUT_ATTACHMENT_READ_BIT);
                const auto srcStages = static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                                                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                                                         VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
                const auto dstStages = static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
                const auto barrier   = makeMemoryBarrier(srcAccess, dstAccess);
                cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, srcStages, dstStages, &barrier, 1u,
                                         VK_DEPENDENCY_BY_REGION_BIT);

#ifndef CTS_USES_VULKANSC
                const VkBeginCustomResolveInfoEXT beginCustomResolve = initVulkanStructure();
                ctx.vkd.cmdBeginCustomResolveEXT(cmdBuffer, &beginCustomResolve);
#endif
            }
        }

        if (i < regularDraws)
        {
            pipelineRenderingInfo.pNext = nullptr;
#ifndef CTS_USES_VULKANSC
            pipelineCustomResolve.pNext = nullptr;
#endif

            const auto fragShaderName = "frag-regular-" + std::to_string(i);
            ShaderWrapper fragShader(ctx.vkd, ctx.device, binaries.get(fragShaderName));

            pipelines.emplace_back(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                                               context.getDeviceExtensions(), constructionType));
            auto &wrapper = *pipelines.back();

            VkRenderingAttachmentLocationInfo renderingAttLocations{
                VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_LOCATION_INFO,
                nullptr,
                Params::kColorAttCount,
                drawInfo.colorAttIndices,
            };
            VkRenderingAttachmentLocationInfo *pipelineRenderingAttLocations =
                (drawInfo.remapColorAtt ? &renderingAttLocations : nullptr);

            wrapper.setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                .setDefaultRasterizationState()
                .setupVertexInputState(&vertexInputState)
                .setupPreRasterizationShaderState(viewports, scissors, emptyLayout, VK_NULL_HANDLE, 0u, vertShader,
                                                  nullptr, ShaderWrapper(), ShaderWrapper(), ShaderWrapper(), nullptr,
                                                  nullptr, &pipelineRenderingInfo)
                .setupFragmentShaderState(emptyLayout, VK_NULL_HANDLE, 0u, fragShader, &regularDepthStencilState,
                                          &regularMultisampleState, nullptr, VK_NULL_HANDLE, nullptr, nullptr,
                                          pPipelineCustomResolve)
                .setupFragmentOutputState(VK_NULL_HANDLE, 0u, &colorBlendState, &regularMultisampleState,
                                          VK_NULL_HANDLE, nullptr, pipelineRenderingAttLocations, nullptr,
                                          pPipelineCustomResolve)
                .buildPipeline();

            const bool needAttLocationsCmd = (drawInfo.remapColorAtt || !defaultColorLocations);
            if (needAttLocationsCmd)
            {
                renderingAttLocations.pNext = nullptr;
                ctx.vkd.cmdSetRenderingAttachmentLocations(cmdBuffer, &renderingAttLocations);
            }

            VkCommandBuffer rpCmdBuffer = cmdBuffer;
            if (useSecondaries)
            {
                rpCmdBuffer = makeNewSecondary();
                beginSecondaryWithInheritance(ctx.vkd, rpCmdBuffer, lastRenderingInfo, pipelineRenderingInfo,
                                              regularMultisampleState.rasterizationSamples, &renderingAttLocations,
                                              nullptr
#ifndef CTS_USES_VULKANSC
                                              ,
                                              pPipelineCustomResolve
#endif
                );
            }

            // Record render pass commands.
            wrapper.bind(rpCmdBuffer);

            const VkDeviceSize vertexBufferOffset = 0ull;
            ctx.vkd.cmdBindVertexBuffers(rpCmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
            const auto vertexCount     = drawInfo.pixelCount * pixelVertexCount;
            const auto prevVertexCount = prevPixels * pixelVertexCount;
            ctx.vkd.cmdDraw(rpCmdBuffer, vertexCount, 1u, prevVertexCount, 0u);

            if (useSecondaries)
            {
                endCommandBuffer(ctx.vkd, rpCmdBuffer);
                ctx.vkd.cmdExecuteCommands(cmdBuffer, 1u, &rpCmdBuffer);
            }
        }
        else
        {
            DE_ASSERT(params->customResolve);

            pipelineRenderingInfo.pNext = nullptr;
#ifndef CTS_USES_VULKANSC
            pipelineCustomResolve.pNext         = nullptr;
            pipelineCustomResolve.customResolve = VK_TRUE;
#endif

            const auto fragShaderName = "frag-resolve-" + std::to_string(i);
            ShaderWrapper fragShader(ctx.vkd, ctx.device, binaries.get(fragShaderName));

            pipelines.emplace_back(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                                               context.getDeviceExtensions(), constructionType));
            auto &wrapper = *pipelines.back();

            VkRenderingAttachmentLocationInfo renderingAttLocations{
                VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_LOCATION_INFO,
                nullptr,
                Params::kColorAttCount,
                drawInfo.colorAttIndices,
            };
            VkRenderingAttachmentLocationInfo *pipelineRenderingAttLocations =
                (drawInfo.remapColorAtt ? &renderingAttLocations : nullptr);

            // This one is always mandatory because we always want to at least remap the depth attachment.
            VkRenderingInputAttachmentIndexInfo renderingInputAttIndices{
                VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO,
                nullptr,
                Params::kColorAttCount,
                drawInfo.colorInputAttIndices,
                (drawInfo.remapInputAtt ? &drawInfo.depthInputAttIndex : nullptr),
                nullptr,
            };
            VkRenderingInputAttachmentIndexInfo *pipelineRenderingInputAttIndices =
                (drawInfo.remapInputAtt ? &renderingInputAttIndices : nullptr);

            wrapper.setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                .setDefaultRasterizationState()
                .setupVertexInputState(&vertexInputState)
                .setupPreRasterizationShaderState(viewports, scissors, resolvePipelineLayout, VK_NULL_HANDLE, 0u,
                                                  vertShader, nullptr, ShaderWrapper(), ShaderWrapper(),
                                                  ShaderWrapper(), nullptr, nullptr, &pipelineRenderingInfo)
                .setupFragmentShaderState(resolvePipelineLayout, VK_NULL_HANDLE, 0u, fragShader,
                                          &resolveDepthStencilState, &resolveMultisampleState, nullptr, VK_NULL_HANDLE,
                                          nullptr, pipelineRenderingInputAttIndices, pPipelineCustomResolve)
                .setupFragmentOutputState(VK_NULL_HANDLE, 0u, &colorBlendState, &resolveMultisampleState,
                                          VK_NULL_HANDLE, nullptr, pipelineRenderingAttLocations, nullptr,
                                          pPipelineCustomResolve)
                .buildPipeline();

            const bool needAttLocationsCmd = (drawInfo.remapColorAtt || !defaultColorLocations);
            const bool needAttIndicesCmd   = (drawInfo.remapInputAtt || !defaultInputAttIndices);

            if (needAttLocationsCmd)
            {
                renderingAttLocations.pNext = nullptr;
                ctx.vkd.cmdSetRenderingAttachmentLocations(cmdBuffer, &renderingAttLocations);
            }

            if (needAttIndicesCmd)
            {
                renderingInputAttIndices.pNext = nullptr;
                ctx.vkd.cmdSetRenderingInputAttachmentIndices(cmdBuffer, &renderingInputAttIndices);
            }

            VkCommandBuffer rpCmdBuffer = cmdBuffer;
            if (useSecondaries)
            {
                rpCmdBuffer = makeNewSecondary();
                beginSecondaryWithInheritance(ctx.vkd, rpCmdBuffer, lastRenderingInfo, pipelineRenderingInfo,
                                              resolveMultisampleState.rasterizationSamples, &renderingAttLocations,
                                              &renderingInputAttIndices
#ifndef CTS_USES_VULKANSC
                                              ,
                                              pPipelineCustomResolve
#endif
                );
            }

            // Record render pass commands.
            wrapper.bind(rpCmdBuffer);

            const VkDeviceSize vertexBufferOffset = 0ull;
            ctx.vkd.cmdBindVertexBuffers(rpCmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
            ctx.vkd.cmdBindDescriptorSets(rpCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, resolvePipelineLayout.get(), 0u,
                                          1u, &descriptorSet.get(), 0u, nullptr);
            const auto vertexCount     = drawInfo.pixelCount * pixelVertexCount;
            const auto prevVertexCount = prevPixels * pixelVertexCount;
            ctx.vkd.cmdDraw(rpCmdBuffer, vertexCount, 1u, prevVertexCount, 0u);

            if (useSecondaries)
            {
                endCommandBuffer(ctx.vkd, rpCmdBuffer);
                ctx.vkd.cmdExecuteCommands(cmdBuffer, 1u, &rpCmdBuffer);
            }
        }

        prevPixels += drawInfo.pixelCount;
        defaultColorLocations  = !drawInfo.remapColorAtt;
        defaultInputAttIndices = !drawInfo.remapInputAtt;

        // Will the next draw be the first one after a new vkCmdBeginRendering?
        quadrantDrawIndex = (quadrantDrawIndex + 1u) % quadrantDraws;
    }

    // Note the last render pass is left open.
    ctx.vkd.cmdEndRendering(cmdBuffer);

    for (uint32_t i = 0u; i < Params::kColorAttCount; ++i)
    {
        const auto &colorBuffer = *(colorBuffers.at(i)->ssImage);
        copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), extent.swizzle(0, 1),
                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, attLayout);
    }

    if (params->resolveDepth)
    {
        // Note: depth resolves happen in the color attachment output stage as per the spec.
        const auto &ssDepthImage = *depthBuffer.ssImage;
        copyImageToBuffer(ctx.vkd, cmdBuffer, ssDepthImage.getImage(), ssDepthImage.getBuffer(), extent.swizzle(0, 1),
                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, attLayout, 1u, VK_IMAGE_ASPECT_DEPTH_BIT,
                          VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    const auto tcuColorFormat = mapVkFormat(colorFormat);
    const auto tcuDepthFormat = mapVkFormat(depthFormat);
    auto &log                 = context.getTestContext().getLog();
    bool fail                 = false;

    // When using custom resolves, we resolve with sample 0 and multiply RGB by the depth value. This is equivalent to
    // having 1 color to calculate the average, and having that color have that extra multiplication.
    //
    // When not using custom resolves, we apply a real average, which means we have as many samples as subpixel areas
    // and we do not apply the special factor, or we make the factor be 1 instead of the depth.
    const auto resolveVertexCount = (params->customResolve ? 1u : subpixelAreas);

    tcu::TextureLevel depthLevel(tcuDepthFormat, extent);
    tcu::PixelBufferAccess depthReference = depthLevel.getAccess();

    for (uint32_t i = 0u; i < Params::kColorAttCount; ++i)
    {
        const auto &colorBuffer = *(colorBuffers.at(i)->ssImage);
        auto &alloc             = colorBuffer.getBufferAllocation();
        invalidateAlloc(ctx.vkd, ctx.device, alloc);
        tcu::ConstPixelBufferAccess result(tcuColorFormat, extent, alloc.getHostPtr());

        tcu::TextureLevel refLevel(tcuColorFormat, extent);
        tcu::PixelBufferAccess reference = refLevel.getAccess();

        for (size_t q = 0; q < quadrantOffsets.size(); ++q)
            for (uint32_t y = 0u; y < quadrantExtent.y(); ++y)
                for (uint32_t x = 0u; x < quadrantExtent.x(); ++x)
                {
                    // Absolute pixel buffer coordinates.
                    const auto absX = static_cast<int>(quadrantOffsets.at(q).x() + x);
                    const auto absY = static_cast<int>(quadrantOffsets.at(q).y() + y);

                    // This will take the first vertex of sample 0 of the pixel.
                    const auto pixelIndex    = static_cast<int>(q * quadrantPixels + y * quadrantExtent.x() + x);
                    const auto vertBaseIndex = pixelIndex * static_cast<int>(pixelVertexCount);

                    std::vector<tcu::Vec4> pixelColors; // To calculate an average.
                    pixelColors.reserve(resolveVertexCount);

                    for (uint32_t resolveVertexIdx = 0u; resolveVertexIdx < resolveVertexCount; ++resolveVertexIdx)
                    {
                        const auto &vertex       = vertices.at(vertBaseIndex + resolveVertexIdx * subpixelVertexCount);
                        const bool depthPassed   = (vertex.position.z() > clearDepth.depthStencil.depth);
                        const auto firstDrawInfo = getFirstDrawInfo(drawInfos, pixelVertexCount, vertBaseIndex);
                        const auto resolveDrawInfo =
                            (params->customResolve ? getResolveDrawInfo(drawInfos, params, x, y) : nullptr);

                        tcu::Vec4 colors[Params::kColorAttCount] = {
                            (depthPassed ? vertex.colors[0] : clearColorV4),
                            (depthPassed ? vertex.colors[1] : clearColorV4),
                        };

                        if (resolveDrawInfo && resolveDrawInfo->remapColorAtt &&
                            resolveDrawInfo->colorAttIndices[0] > resolveDrawInfo->colorAttIndices[1])
                            std::swap(colors[0], colors[1]);

                        if (firstDrawInfo->remapColorAtt &&
                            firstDrawInfo->colorAttIndices[0] > firstDrawInfo->colorAttIndices[1])
                            std::swap(colors[0], colors[1]);

                        const auto depth = (depthPassed ? vertex.position.z() : clearDepth.depthStencil.depth);

                        if (i == 0 && resolveVertexIdx == 0u)
                            depthReference.setPixDepth(depth, absX, absY);

                        const auto rgbFactor = (params->customResolve ? depth : 1.0f);
                        const tcu::Vec4 colorFactor(rgbFactor, rgbFactor, rgbFactor, 1.0f);
                        const auto setColor = colors[i] * colorFactor;
                        pixelColors.push_back(setColor);
                    }

                    tcu::Vec4 avgColor(0.0f);
                    for (const auto &pxColor : pixelColors)
                        avgColor += pxColor;
                    avgColor = avgColor / tcu::Vec4(static_cast<float>(pixelColors.size()));

                    reference.setPixel(avgColor, absX, absY);
                }

        const auto setName = "ColorBuffer" + std::to_string(i);

        const float rgbThreshold = 0.005f;
        const tcu::Vec4 threshold(rgbThreshold, rgbThreshold, rgbThreshold, 0.0f);
        if (!tcu::floatThresholdCompare(log, setName.c_str(), "", reference, result, threshold,
                                        tcu::COMPARE_LOG_ON_ERROR))
            fail = true;
    }

    if (params->resolveDepth)
    {
        const auto &ssDepthImage = *depthBuffer.ssImage;
        auto &alloc              = ssDepthImage.getBufferAllocation();
        invalidateAlloc(ctx.vkd, ctx.device, alloc);

        const auto threshold = 0.00002f;
        tcu::ConstPixelBufferAccess depthResult(tcuDepthFormat, extent, alloc.getHostPtr());
        if (!tcu::dsThresholdCompare(log, "DepthBuffer", "'", depthReference, depthResult, threshold,
                                     tcu::COMPARE_LOG_ON_ERROR))
            fail = true;
    }

    if (fail)
        TCU_FAIL("Unexpected values in color or depth attachments; check log for details -- ");

    return tcu::TestStatus::pass("Pass");
}

} // namespace SuspendResume

} // namespace renderpass
} // namespace vkt
