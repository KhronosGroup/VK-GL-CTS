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
 * \brief VK_EXT_primitive_restart_index Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelinePrimitiveRestartIndexTests.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"

#include "tcuImageCompare.hpp"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <map>
#include <memory>
#include <vector>

namespace vkt
{
namespace pipeline
{

namespace
{

using namespace vk;

// TEST MECHANISM:
//
// - Create an 4x8 framebuffer (32 pixels).
// - Divide the framebuffer in 4 horizontal 4x2 blocks.
// - Create vertex data to cover each row of pixels with primitives, pixel by pixel:
//   - For VK_PRIMITIVE_TOPOLOGY_POINT_LIST, one point at each pixel center.
//   - For VK_PRIMITIVE_TOPOLOGY_LINE_LIST, one line crossing each pixel (best if lower-left to upper-right corner).
//   - For VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, one line crossing each pixel (alternate up-down: /\/\/\).
//   - For VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, one triangle surrounding each pixel center.
//   - For VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, one quad in each pixel at pixel corners.
//   - For VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
//     - The first point can be the middle of the pixel row, in the bottom.
//     - The second point can be the lower-left corner of the row.
//     - The third one, the upper-left corner of the row.
//     - Then, the top right-corner for each pixel.
//     - The final point is the bottom-right corner of the row.
//   - For VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY, same as the line list with 2 extra vertices for adjacency.
//   - For VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY, same as line strip with 2 extra vertices in total for adjacency.
//   - For VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY, same as triangle list, but with 3 extra vertices per triangle.
//       - Watch out! (vertices 0, 2 and 4 are the triangle).
//   - For VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
//     - Each vertex is followed by one extra adjacency vertex
//         0 a0 1 a1 2 a2 3 a3 4 a4, etc.
//     - Each new pair of vertices (geom, adjacency) adds a new triangle, except for the first two.
//     - But the first two also have adjacency info.
//     - So it is like triangle strips with one extra vertex for adjacency after each regular vertex.
//   - For VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, the same as a VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST.
// - HOWEVER, despite the geometry covering the full row of pixels, try to leave out the last pixel using indices.
//   - This can be achieved by inserting restart indices in the index buffer so that no actual primitive forms.
//   - If the restart index is read as an actual index, this will result in crashes or extra geometry.
// - The first N vertices in the vertex buffer will always be unused, and this N will be used as vertexOffset.
//   - This allows us to use those first N vertices to store special values.
//   - It also allows us to test that the vertex offset is added only after checking the restart index.
//   - Point list, line list, line strip, line strip with adjacency, triangle list, patch list, triangle fan:
//     - The first N vertices replicate the last point.
//     - The last index is restart: if not correct, crash or unexpected coverage.
//   - Triangle strip:
//     - The first N vertices replicate the penultimate one.
//     - The penultimate index is the restart one.
//       - Good: last pixel is not covered.
//       - Wrong: crash or coverage in the last pixel.
//    - Line list with adjacency, triangle list with adjacency:
//     - The first N vertices replicate the penultimate one.
//     - The penultimate index is the restart one.
//       - Good: last pixel is not covered.
//       - Wrong: crash or coverage in the last pixel.
//     - Similar to triangle strip: the last actual vertex (penultimate value) is converted to a restart.
//    - Triangle strip with adjacency:
//     - Similar to triangle strip, but the restart index is not in the penultimate position, but two positions before.
//     - This is due to the extra adjacency information.
// - The 4 blocks will be drawn each with:
//   - Custom restart index.
//   - Regular restart index.
//   - Custom restart index.
//   - Regular restart index.
// - The regular restart index will always be outside the regular indices.
//   - This is because the primitive that uses more vertices (triangle list with adjacency), needs at most 32x6=192 vertices.
// - The tests will also try to use the regular restart index as the custom one, and one less.
//   - All values tested: 0, 1, max-1, max
// - Note list topologies need support for VK_EXT_primitive_topology_list_restart.
//   - primitiveTopologyListRestart for most.
//   - primitiveTopologyPatchListRestart for patches.
// - Dynamic states:
//   - VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE

// If defined, do not use custom restart indices.
#undef AVOID_CUSTOM_RESTART_INDEX
//#define AVOID_CUSTOM_RESTART_INDEX 1

// Types of custom restart indices that the tests will use.
enum class RestartIndex
{
    ZERO = 0,
    ONE,
    PENULTIMATE,
    MAX,
};

enum class DrawCall
{
    DIRECT = 0,
    INDIRECT,
    INDIRECT_COUNT,
    INDIRECT_2,
    INDIRECT_COUNT_2,
};

struct Params
{
    PipelineConstructionType constructionType;
    VkPrimitiveTopology topology;
    VkIndexType indexType;
    RestartIndex restartIndex;
    DrawCall drawCall;
    bool dynamicPrimitiveRestartEnable;
    bool conditionalRendering;
    bool secondaryCmd;

    tcu::IVec3 getExtent() const
    {
        return tcu::IVec3(4, 8, 1);
    }

    bool needsPrimitiveTopologyListRestart() const
    {
        bool needed = false;
        switch (topology)
        {
        case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
        case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
        case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
        case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
        case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
            needed = true;
            break;
        default:
            break;
        }

        return needed;
    }

    bool needsPrimitiveTopologyPatchListRestart() const
    {
        return (topology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST);
    }

    uint32_t blockCount() const
    {
        return 4u; // We'll draw in 4 blocks, as stated above.
    }

    std::vector<tcu::Vec4> blockColors() const
    {
        std::vector<tcu::Vec4> colors{
            tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f),
            tcu::Vec4(0.0f, 1.0f, 1.0f, 1.0f),
            tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f),
            tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f),
        };
        return colors;
    }

    tcu::Vec4 clearColor() const
    {
        return tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    std::vector<tcu::Vec4> getRowVertices(uint32_t rowIndex) const
    {
        const auto extent = getExtent().asUint();
        DE_ASSERT(rowIndex < extent.y());

        const auto normalize = [](uint32_t v, uint32_t total)
        { return ((static_cast<float>(v) / static_cast<float>(total)) * 2.0f - 1.0f); };

        const float yTop = normalize(rowIndex, extent.y());
        const float yBot = normalize(rowIndex + 1u, extent.y());
        const float yMid = (yTop + yBot) / 2.0f;

        std::vector<tcu::Vec4> vertices;
        vertices.reserve(6u * extent.x()); // This is a maximum.

        if (topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP)
            vertices.emplace_back(-1.0f, yTop, 0.0f, 1.0f);
        else if (topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY)
        {
            vertices.emplace_back(0.0f, 0.0f, 0.0f, 1.0f); // adj
            vertices.emplace_back(-1.0f, yTop, 0.0f, 1.0f);
        }
        else if (topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
        {
            vertices.emplace_back(-1.0f, yTop, 0.0f, 1.0f);
            vertices.emplace_back(-1.0f, yBot, 0.0f, 1.0f);
        }
        else if (topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY)
        {
            vertices.emplace_back(-1.0f, yTop, 0.0f, 1.0f);
            vertices.emplace_back(0.0f, 0.0f, 0.0f, 1.0f); // adj
            vertices.emplace_back(-1.0f, yBot, 0.0f, 1.0f);
            vertices.emplace_back(0.0f, 0.0f, 0.0f, 1.0f); // adj
        }
        else if (topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN)
        {
            vertices.emplace_back(0.0, yBot, 0.0f, 1.0f);
            vertices.emplace_back(-1.0f, yBot, 0.0f, 1.0f);
            vertices.emplace_back(-1.0f, yTop, 0.0f, 1.0f);
        }

        for (uint32_t i = 0u; i < extent.x(); ++i)
        {
            const float xLeft  = normalize(i, extent.x());
            const float xRight = normalize(i + 1u, extent.x());
            const float xMid   = (xLeft + xRight) / 2.0f;

            if (topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
                vertices.emplace_back(xMid, yMid, 0.0f, 1.0f);
            else if (topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
            {
                vertices.emplace_back(xLeft, yBot, 0.0f, 1.0f);
                vertices.emplace_back(xRight, yTop, 0.0f, 1.0f);
            }
            else if (topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY)
            {
                vertices.emplace_back(0.0f, 0.0f, 0.0f, 1.0f); // adj
                vertices.emplace_back(xLeft, yBot, 0.0f, 1.0f);
                vertices.emplace_back(xRight, yTop, 0.0f, 1.0f);
                vertices.emplace_back(0.0f, 0.0f, 0.0f, 1.0f); // adj
            }
            else if (topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP ||
                     topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY)
                vertices.emplace_back(xRight, (i % 2 ? yTop : yBot), 0.0f, 1.0f);
            else if (topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST || topology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
            {
                vertices.emplace_back(xLeft, yBot, 0.0f, 1.0f);
                vertices.emplace_back(xRight, yBot, 0.0f, 1.0f);
                vertices.emplace_back(xMid, yTop, 0.0f, 1.0f);
            }
            else if (topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY)
            {
                vertices.emplace_back(xLeft, yBot, 0.0f, 1.0f);
                vertices.emplace_back(0.0f, 0.0f, 0.0f, 1.0f); // adj
                vertices.emplace_back(xRight, yBot, 0.0f, 1.0f);
                vertices.emplace_back(0.0f, 0.0f, 0.0f, 1.0f); // adj
                vertices.emplace_back(xMid, yTop, 0.0f, 1.0f);
                vertices.emplace_back(0.0f, 0.0f, 0.0f, 1.0f); // adj
            }
            else if (topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
            {
                vertices.emplace_back(xRight, yTop, 0.0f, 1.0f);
                vertices.emplace_back(xRight, yBot, 0.0f, 1.0f);
            }
            else if (topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY)
            {
                vertices.emplace_back(xRight, yTop, 0.0f, 1.0f);
                vertices.emplace_back(0.0f, 0.0f, 0.0f, 1.0f); // adj
                vertices.emplace_back(xRight, yBot, 0.0f, 1.0f);
                vertices.emplace_back(0.0f, 0.0f, 0.0f, 1.0f); // adj
            }
            else if (topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN)
                vertices.emplace_back(xRight, yTop, 0.0f, 1.0f);
            else
                DE_ASSERT(false);
        }

        if (topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN)
            vertices.emplace_back(1.0f, yBot, 0.0f, 1.0f);
        else if (topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY)
            vertices.emplace_back(0.0f, 0.0f, 0.0f, 1.0f); // adj

        return vertices;
    }

    uint32_t getDefaultRestartIndex() const
    {
        if (indexType == VK_INDEX_TYPE_UINT32)
            return 0xFFFFFFFFu;
        if (indexType == VK_INDEX_TYPE_UINT16)
            return 0xFFFFu;
        if (indexType == VK_INDEX_TYPE_UINT8)
            return 0xFFu;

        DE_ASSERT(false);
        return 0u;
    }

    uint32_t getCustomRestartIndex() const
    {
#ifdef AVOID_CUSTOM_RESTART_INDEX
        return getDefaultRestartIndex();
#else
        if (restartIndex == RestartIndex::ZERO)
            return 0u;
        if (restartIndex == RestartIndex::ONE)
            return 1u;
        const auto maxIndex = getDefaultRestartIndex();
        if (restartIndex == RestartIndex::PENULTIMATE)
            return maxIndex - 1u;
        if (restartIndex == RestartIndex::MAX)
            return maxIndex;

        DE_ASSERT(false);
        return 128u; // Invalid value, not expected to get here.
#endif
    }

    // Geometry indices do not depend on the row, contrary to vertex values.
    std::vector<uint32_t> getRowIndices() const
    {
        const auto extent = getExtent().asUint();

        std::vector<uint32_t> indices;
        indices.reserve(7u * extent.x()); // This is a maximum only.

        const auto appendIndex = [&](uint32_t count = 1u)
        {
            for (uint32_t i = 0u; i < count; ++i)
                indices.push_back(de::sizeU32(indices));
        };

        if (topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP)
            appendIndex();
        else if (topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY ||
                 topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
            appendIndex(2u);
        else if (topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY)
            appendIndex(4u);
        else if (topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN)
            appendIndex(3u);

        for (uint32_t i = 0u; i < extent.x(); ++i)
        {
            if (topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST || topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP ||
                topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY ||
                topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN)
                appendIndex();
            else if (topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST || topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
                appendIndex(2u);
            else if (topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY ||
                     topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY)
                appendIndex(4u);
            else if (topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST || topology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
                appendIndex(3u);
            else if (topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY)
                appendIndex(6u);
            else
                DE_ASSERT(false);
        }

        if (topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN ||
            topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY)
            appendIndex();

        return indices;
    }

    uint32_t getRestartIndexForBlock(uint32_t blockIndex) const
    {
        const auto custom     = getCustomRestartIndex();
        const auto def        = getDefaultRestartIndex();
        const auto indexValue = (blockIndex % 2 == 0u ? custom : def);
        return indexValue;
    }

    uint32_t getRestartIndexForRow(uint32_t rowIndex) const
    {
        const auto bc         = blockCount();
        const auto extent     = getExtent().asUint();
        const auto blockRows  = extent.y() / bc;
        const auto blockIndex = rowIndex / blockRows;
        return getRestartIndexForBlock(blockIndex);
    }
};

using ParamsPtr = std::shared_ptr<const Params>;

class RestartIndexInstance : public vkt::TestInstance
{
public:
    RestartIndexInstance(Context &context, ParamsPtr params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~RestartIndexInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const ParamsPtr m_params;
};

class RestartIndexCase : public vkt::TestCase
{
public:
    RestartIndexCase(tcu::TestContext &testCtx, const std::string &name, ParamsPtr params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~RestartIndexCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new RestartIndexInstance(context, m_params);
    }

protected:
    const ParamsPtr m_params;
};

void RestartIndexCase::checkSupport(Context &context) const
{
#ifndef AVOID_CUSTOM_RESTART_INDEX
    context.requireDeviceFunctionality("VK_EXT_primitive_restart_index");
#endif // AVOID_CUSTOM_RESTART_INDEX

    if (m_params->indexType == VK_INDEX_TYPE_UINT8)
    {
        const auto &idx8Features = context.getIndexTypeUint8Features();
        if (!idx8Features.indexTypeUint8)
            TCU_THROW(NotSupportedError, "indexTypeUint8 not supported");
    }

    if (m_params->dynamicPrimitiveRestartEnable)
    {
        const auto &eds2Features = context.getExtendedDynamicState2FeaturesEXT();
        if (!eds2Features.extendedDynamicState2)
            TCU_THROW(NotSupportedError, "extendedDynamicState2 not supported");
    }

    const bool needsListRestart  = m_params->needsPrimitiveTopologyListRestart();
    const bool needsPatchRestart = m_params->needsPrimitiveTopologyPatchListRestart();

    if (needsListRestart || needsPatchRestart)
    {
        const auto &restartFeatures = context.getPrimitiveTopologyListRestartFeaturesEXT();

        if (needsListRestart && !restartFeatures.primitiveTopologyListRestart)
            TCU_THROW(NotSupportedError, "primitiveTopologyListRestart not supported");

        if (needsPatchRestart && !restartFeatures.primitiveTopologyPatchListRestart)
            TCU_THROW(NotSupportedError, "primitiveTopologyPatchListRestart not supported");
    }

    if (m_params->needsPrimitiveTopologyPatchListRestart())
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

    if (m_params->drawCall == DrawCall::INDIRECT_COUNT || m_params->drawCall == DrawCall::INDIRECT_COUNT_2)
        context.requireDeviceFunctionality("VK_KHR_draw_indirect_count");

    if (m_params->drawCall == DrawCall::INDIRECT_2 || m_params->drawCall == DrawCall::INDIRECT_COUNT_2)
        context.requireDeviceFunctionality("VK_KHR_device_address_commands");

    if (m_params->conditionalRendering)
        context.requireDeviceFunctionality("VK_EXT_conditional_rendering");
}

void RestartIndexCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "out gl_PerVertex {\n"
         << "    vec4 gl_Position;\n"
         << "    float gl_PointSize;\n"
         << "};\n"
         << "void main(void) {\n"
         << "    gl_Position = inPos;\n"
         << "    gl_PointSize = 1.0;\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "layout (push_constant) uniform PCBlock { vec4 color; } pc;\n"
         << "void main(void) {\n"
         << "    outColor = pc.color;\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());

    if (m_params->needsPrimitiveTopologyPatchListRestart())
    {
        // Passthrough tessellation shaders, triangle version.
        std::ostringstream tesc;
        tesc << "#version 460\n"
             << "#extension GL_EXT_tessellation_shader : require\n"
             << "layout(vertices=3) out;\n"
             << "in gl_PerVertex {\n"
             << "    vec4 gl_Position;\n"
             << "    float gl_PointSize;\n"
             << "} gl_in[gl_MaxPatchVertices];\n"
             << "out gl_PerVertex {\n"
             << "    vec4 gl_Position;\n"
             << "    float gl_PointSize;\n"
             << "} gl_out[];\n"
             << "void main() {\n"
             << "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
             << "    gl_out[gl_InvocationID].gl_PointSize = gl_in[gl_InvocationID].gl_PointSize;\n"
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
             << "layout(triangles) in;\n"
             << "in gl_PerVertex {\n"
             << "    vec4 gl_Position;\n"
             << "    float gl_PointSize;\n"
             << "} gl_in[gl_MaxPatchVertices];\n"
             << "out gl_PerVertex {\n"
             << "    vec4 gl_Position;\n"
             << "    float gl_PointSize;\n"
             << "};\n"
             << "void main() {\n"
             << "    gl_Position = (gl_in[0].gl_Position * gl_TessCoord.x + \n"
             << "                   gl_in[1].gl_Position * gl_TessCoord.y + \n"
             << "                   gl_in[2].gl_Position * gl_TessCoord.z);\n"
             << "    gl_PointSize = gl_in[0].gl_PointSize;\n"
             << "}\n";
        programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(tese.str());
    }
}

using BufferWithMemoryPtr = std::unique_ptr<BufferWithMemory>;

BufferWithMemoryPtr makeIndexBuffer(const ContextCommonData &ctx, const std::vector<uint32_t> &indices,
                                    VkIndexType indexType)
{
    uint32_t itemSize = 0u;
    if (indexType == VK_INDEX_TYPE_UINT32)
        itemSize = DE_SIZEOF32(uint32_t);
    else if (indexType == VK_INDEX_TYPE_UINT16)
        itemSize = DE_SIZEOF32(uint16_t);
    else if (indexType == VK_INDEX_TYPE_UINT8)
        itemSize = DE_SIZEOF32(uint8_t);
    else
        DE_ASSERT(false);

    const auto bufferSize  = static_cast<VkDeviceSize>(de::sizeU32(indices) * itemSize);
    const auto bufferUsage = static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    const auto createInfo  = makeBufferCreateInfo(bufferSize, bufferUsage);
    BufferWithMemoryPtr buffer(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, createInfo, HostIntent::W));

    auto &alloc            = buffer->getAllocation();
    const auto bufferBytes = reinterpret_cast<char *>(alloc.getHostPtr());

    for (size_t i = 0; i < indices.size(); ++i)
    {
        const auto dst = bufferBytes + i * itemSize;
        if (indexType == VK_INDEX_TYPE_UINT32)
        {
            const uint32_t item = indices.at(i);
            memcpy(dst, &item, itemSize);
        }
        else if (indexType == VK_INDEX_TYPE_UINT16)
        {
            const uint16_t item = static_cast<uint16_t>(indices.at(i));
            memcpy(dst, &item, itemSize);
        }
        else if (indexType == VK_INDEX_TYPE_UINT8)
        {
            const uint8_t item = static_cast<uint8_t>(indices.at(i));
            memcpy(dst, &item, itemSize);
        }
        else
            DE_ASSERT(false);
    }

    flushAlloc(ctx.vkd, ctx.device, alloc);
    return buffer;
}

// Create an indirect command buffer with "total" cmds, where "cmd" sits at position "position" and the rest are no-ops.
BufferWithMemoryPtr makeIndirectBuffer(const ContextCommonData &ctx, const VkDrawIndexedIndirectCommand &cmd,
                                       uint32_t position, uint32_t total, bool deviceAddress)
{
    DE_ASSERT(position < total);

    static const VkDrawIndexedIndirectCommand noOpCmd{0u, 0u, 0u, 0, 0u};
    std::vector<VkDrawIndexedIndirectCommand> cmds(total, noOpCmd);
    cmds.at(position) = cmd;

    const auto bufferSize  = static_cast<VkDeviceSize>(de::dataSize(cmds));
    const auto bufferUsage = static_cast<VkBufferUsageFlags>(
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | (deviceAddress ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0));
    const auto createInfo = makeBufferCreateInfo(bufferSize, bufferUsage);

    VkMemoryAllocateFlags memAllocFlags = 0u;
    if (deviceAddress)
        memAllocFlags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    BufferWithMemoryPtr buffer(
        new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, createInfo, HostIntent::W, true, memAllocFlags));
    auto &alloc = buffer->getAllocation();
    memcpy(alloc.getHostPtr(), de::dataOrNull(cmds), de::dataSize(cmds));
    flushAlloc(ctx.vkd, ctx.device, alloc);

    return buffer;
}

tcu::TestStatus RestartIndexInstance::iterate(void)
{
    const auto ctx         = m_context.getContextCommonData();
    const auto extent      = m_params->getExtent();
    const auto extentVk    = makeExtent3D(extent);
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage =
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto imgType        = VK_IMAGE_TYPE_2D;
    const auto padVertexCount = 2u;
    const auto blockCount     = m_params->blockCount();
    const auto blockColors    = m_params->blockColors();

    // Calculate a value for the vertex offset in the indexed draw call and an offset to apply to the indices manually.
    // This is needed because getRowIndices returns 0-based indices for the geometry, but 0 can be a restart index so
    // we cannot leave that value directly in the index buffer. So when the chosen custom restart index is zero, we can
    // add a manual offset of 1 and make vertexOffset 1, adding up to padVertexCount (2). If the restart index is 1,
    // we can add a manual offset of 2 and make vertexOffset 0, adding up to padVertexCount (2) again. For large custom
    // restart indices, we do not have this issue.
    uint32_t vertexOffset = 2u;
    uint32_t manualOffset = 0u;
    if (m_params->restartIndex == RestartIndex::ZERO)
        vertexOffset = manualOffset = 1u;
    else if (m_params->restartIndex == RestartIndex::ONE)
        std::swap(vertexOffset, manualOffset);
    DE_ASSERT(vertexOffset + manualOffset == padVertexCount);

    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, extentVk, colorFormat, colorUsage, imgType);

    BufferWithMemoryPtr crBuffer;
    if (m_params->conditionalRendering)
    {
        const auto crBufferSize  = sizeof(uint32_t);
        const auto crBufferUsage = static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT);
        const auto crBufferInfo  = makeBufferCreateInfo(crBufferSize, crBufferUsage);
        crBuffer.reset(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, crBufferInfo, HostIntent::W));
        {
            auto &alloc = crBuffer->getAllocation();
            memset(alloc.getHostPtr(), 0, crBufferSize); // Disable rendering.
            flushAlloc(ctx.vkd, ctx.device, alloc);
        }
    }

    uint32_t verticesPerRow = 0u;
    std::vector<tcu::Vec4> allVertices;
    tcu::Vec4 paddingVertex(0.0f);

    std::vector<uint32_t> allIndices;
    size_t replacedIndex      = 0xFFFFFFFFu;
    uint32_t rowIndexCount    = 0u;
    const auto baseRowIndices = m_params->getRowIndices();

    for (uint32_t i = 0u; i < extentVk.height; ++i)
    {
        // Each block of rows will have alternating restart indices.
        const auto restartIndex = m_params->getRestartIndexForRow(i);
        const auto rowVertices  = m_params->getRowVertices(i);
        auto rowIndices         = baseRowIndices;

        if (i == 0u)
        {
            verticesPerRow = de::sizeU32(rowVertices);
            allVertices.reserve(verticesPerRow * extentVk.height);
            allIndices.reserve(extentVk.height * (verticesPerRow + 1u /*final restart index after each row*/));

            // Set the value of the padding vertices, according to the plan above.
            // Also select which index we will replace with a restart index so that the last pixel is not covered.
            if (m_params->topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST ||
                m_params->topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST ||
                m_params->topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP ||
                m_params->topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY ||
                m_params->topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST ||
                m_params->topology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST ||
                m_params->topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN)
            {
                paddingVertex = rowVertices.back();
                replacedIndex = rowIndices.size() - 1;
            }
            else if (m_params->topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP ||
                     m_params->topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY ||
                     m_params->topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY)
            {
                DE_ASSERT(rowVertices.size() > 1);
                replacedIndex = rowVertices.size() - 2;
                paddingVertex = rowVertices.at(replacedIndex);
            }
            else if (m_params->topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY)
            {
                DE_ASSERT(rowVertices.size() > 3);
                replacedIndex = rowVertices.size() - 4;
                paddingVertex = rowVertices.at(replacedIndex);
            }
            else
            {
                DE_ASSERT(false);
            }
        }

        // Add the manual offset to returned indices.
        for (auto &v : rowIndices)
            v += manualOffset;

        // Make sure the last pixel is not covered.
        rowIndices.at(replacedIndex) = restartIndex;

        // Modify the indices for this row to take into account the vertex counts for the previous rows.
        // This is because getRowIndices always gives 0-based indices.
        const auto prevVertexCount = de::sizeU32(allVertices);
        for (auto &v : rowIndices)
        {
            // Skip special indices.
            if (v != restartIndex)
                v += prevVertexCount;
        }

        // Append a primitive restart after each row, in any case.
        rowIndices.push_back(restartIndex);

        if (rowIndexCount == 0u)
            rowIndexCount = de::sizeU32(rowIndices); // Should be baseRowIndices.size() + 1, basically.

        // Append vertices and indices to the full list.
        std::copy(begin(rowVertices), end(rowVertices), std::back_inserter(allVertices));
        std::copy(begin(rowIndices), end(rowIndices), std::back_inserter(allIndices));
    }

    // Vertex buffer, with the extra vertices.
    const auto vertexSize           = DE_SIZEOF32(tcu::Vec4);
    const auto vertexBufferPadBytes = vertexSize * padVertexCount;
    const auto vertexBufferSize     = static_cast<VkDeviceSize>(vertexBufferPadBytes + de::dataSize(allVertices));
    const auto vertexBufferUsage    = static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    const auto vertexBufferInfo     = makeBufferCreateInfo(vertexBufferSize, vertexBufferUsage);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferInfo, HostIntent::W);
    {
        auto &alloc = vertexBuffer.getAllocation();
        std::vector<tcu::Vec4> paddingVertices(padVertexCount, paddingVertex);
        memcpy(alloc.getHostPtr(), de::dataOrNull(paddingVertices), de::dataSize(paddingVertices));
        memcpy(reinterpret_cast<uint8_t *>(alloc.getHostPtr()) + vertexBufferPadBytes, de::dataOrNull(allVertices),
               de::dataSize(allVertices));
        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    // Make an index buffer with allIndices, but with the right bit width.
    const auto indexBuffer = makeIndexBuffer(ctx, allIndices, m_params->indexType);

    const bool needsTess = m_params->needsPrimitiveTopologyPatchListRestart();
    const auto &binaries = m_context.getBinaryCollection();
    ShaderWrapper vertShader(ctx.vkd, ctx.device, binaries.get("vert"));
    ShaderWrapper fragShader(ctx.vkd, ctx.device, binaries.get("frag"));
    ShaderWrapper tescShader = (needsTess ? ShaderWrapper(ctx.vkd, ctx.device, binaries.get("tesc")) : ShaderWrapper());
    ShaderWrapper teseShader = (needsTess ? ShaderWrapper(ctx.vkd, ctx.device, binaries.get("tese")) : ShaderWrapper());

    const auto pcStages = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto pcSize   = DE_SIZEOF32(tcu::Vec4);
    const auto pcRange  = makePushConstantRange(pcStages, 0u, pcSize);
    PipelineLayoutWrapper pipelineLayout(m_params->constructionType, ctx.vkd, ctx.device, VK_NULL_HANDLE, &pcRange);

    RenderPassWrapper renderPass(m_params->constructionType, ctx.vkd, ctx.device, colorFormat);
    renderPass.createFramebuffer(ctx.vkd, ctx.device, colorBuffer.getImage(), colorBuffer.getImageView(),
                                 extentVk.width, extentVk.height);

    const std::vector<VkVertexInputBindingDescription> vertexInputBindings{
        makeVertexInputBindingDescription(0u, DE_SIZEOF32(tcu::Vec4), VK_VERTEX_INPUT_RATE_VERTEX),
    };

    const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes{
        makeVertexInputAttributeDescription(0u, 0u, vk::VK_FORMAT_R32G32B32A32_SFLOAT, 0u),
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputState = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        nullptr,
        0u,
        de::sizeU32(vertexInputBindings),
        de::dataOrNull(vertexInputBindings),
        de::sizeU32(vertexInputAttributes),
        de::dataOrNull(vertexInputAttributes),
    };

    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,    nullptr, 0u, m_params->topology,
        (m_params->dynamicPrimitiveRestartEnable ? VK_FALSE : VK_TRUE),
    };

    std::vector<VkDynamicState> dynamicStates;
    if (m_params->dynamicPrimitiveRestartEnable)
        dynamicStates.push_back(VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE);

    const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        nullptr,
        0u,
        de::sizeU32(dynamicStates),
        de::dataOrNull(dynamicStates),
    };

    const std::vector<VkViewport> viewports(1u, makeViewport(extent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(extent));

    GraphicsPipelineWrapper pipeline(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device, m_context.getDeviceExtensions(),
                                     m_params->constructionType);
    pipeline.setDefaultRasterizationState()
        .setDefaultDepthStencilState()
        .setDefaultMultisampleState()
        .setDefaultColorBlendState()
        .setDynamicState(&dynamicStateCreateInfo)
        .setupVertexInputState(&vertexInputState, &inputAssemblyState)
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass.get(), 0u, vertShader,
                                          nullptr, tescShader, teseShader)
        .setupFragmentShaderState(pipelineLayout, renderPass.get(), 0u, fragShader)
        .setupFragmentOutputState(renderPass.get(), 0u)
        .buildPipeline();

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const VkDeviceSize vertexBufferOffset = 0;
    DE_ASSERT(extentVk.height % blockCount == 0u);
    const auto blockRows           = extentVk.height / blockCount;
    const auto clearColor          = m_params->clearColor();
    const auto indicesPerDraw      = blockRows * rowIndexCount;
    const auto defaultRestartIndex = m_params->getDefaultRestartIndex();

    // We want to test resetting to the default index in two ways: rebinding the index buffer, and explicit reset.
    bool resetByRebind = false;

    std::vector<VkDrawIndexedIndirectCommand> drawCmds;
    drawCmds.reserve(blockCount);
    for (uint32_t i = 0u; i < blockCount; ++i)
    {
        drawCmds.push_back(VkDrawIndexedIndirectCommand{indicesPerDraw, 1u, indicesPerDraw * i,
                                                        static_cast<int32_t>(vertexOffset), 0u});
    }

    // In the direct case (and in general), we only have one draw call per loop iteration, which draws one block, and
    // things that needs to change between draws. Indirect cases are not that interesting in this situation, because the
    // draw count is 1 and special rules would apply in those cases (e.g. to the stride). So, in order to make indirect
    // cases more interesting, each indirect draw buffer will contain more than 1 draw command. For example, a number
    // matching the number of blocks or larger. But for block i, only the i-th command actually draws something and the
    // rest are no-op draw commands.
    std::vector<BufferWithMemoryPtr> indirectBuffers;
    const auto indirectStride = DE_SIZEOF32(VkDrawIndexedIndirectCommand);
    const auto maxCount       = blockCount * 2u; // Makes the count buffer cases more interesting.

    if (m_params->drawCall != DrawCall::DIRECT)
    {
        const bool deviceAddress =
            (m_params->drawCall == DrawCall::INDIRECT_2 || m_params->drawCall == DrawCall::INDIRECT_COUNT_2);
        const bool withCount =
            (m_params->drawCall == DrawCall::INDIRECT_COUNT || m_params->drawCall == DrawCall::INDIRECT_COUNT_2);
        const auto totalItems = (withCount ? maxCount : blockCount);

        for (uint32_t i = 0u; i < blockCount; ++i)
            indirectBuffers.emplace_back(makeIndirectBuffer(ctx, drawCmds.at(i), i, totalItems, deviceAddress));
    }

    BufferWithMemoryPtr countBuffer;
    if (m_params->drawCall == DrawCall::INDIRECT_COUNT || m_params->drawCall == DrawCall::INDIRECT_COUNT_2)
    {
        const bool deviceAddress = (m_params->drawCall == DrawCall::INDIRECT_COUNT_2);

        VkMemoryAllocateFlags memAllocFlags = 0u;
        if (deviceAddress)
            memAllocFlags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

        const auto countBufferSize  = static_cast<VkDeviceSize>(sizeof(uint32_t));
        const auto countBufferUsage = static_cast<VkBufferUsageFlags>(
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | (deviceAddress ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0));
        const auto countBufferInfo = makeBufferCreateInfo(countBufferSize, countBufferUsage);
        countBuffer.reset(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, countBufferInfo, HostIntent::W, true,
                                               memAllocFlags));
        auto &alloc = countBuffer->getAllocation();
        memcpy(alloc.getHostPtr(), &blockCount, sizeof(blockCount));
        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    // We may use a primary or secondary command buffer to record the render pass contents.
    const auto recordRPContents = [&](VkCommandBuffer commandBuffer)
    {
        pipeline.bind(commandBuffer);
        ctx.vkd.cmdBindVertexBuffers(commandBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
        ctx.vkd.cmdBindIndexBuffer(commandBuffer, indexBuffer->get(), 0ull, m_params->indexType);
        if (m_params->dynamicPrimitiveRestartEnable)
            ctx.vkd.cmdSetPrimitiveRestartEnable(commandBuffer, VK_TRUE);
        for (uint32_t i = 0u; i < blockCount; ++i)
        {
            ctx.vkd.cmdPushConstants(commandBuffer, pipelineLayout.get(), pcStages, 0u, pcSize, &blockColors.at(i));

            if (m_params->conditionalRendering)
            {
                // Disable rendering using conditional rendering, which should not affect setting the index.
                VkConditionalRenderingBeginInfoEXT crBeginInfo = initVulkanStructure();
                crBeginInfo.buffer                             = crBuffer->get();
                ctx.vkd.cmdBeginConditionalRenderingEXT(commandBuffer, &crBeginInfo);
            }

#ifndef AVOID_CUSTOM_RESTART_INDEX
            const auto restartIndex = m_params->getRestartIndexForBlock(i);
            if (restartIndex == defaultRestartIndex && !resetByRebind)
            {
                ctx.vkd.cmdBindIndexBuffer(commandBuffer, indexBuffer->get(), 0ull, m_params->indexType);
                resetByRebind = true;
            }
            else
                ctx.vkd.cmdSetPrimitiveRestartIndexEXT(commandBuffer, restartIndex);
#endif // AVOID_CUSTOM_RESTART_INDEX

            // When using conditional rendering, the first block will not be drawn: the conditional rendering section
            // will finish after the draw. In subsequent blocks, the draw will left outside the section.
            if (m_params->conditionalRendering && i > 0u)
                ctx.vkd.cmdEndConditionalRenderingEXT(commandBuffer);

            const auto &drawCmd = drawCmds.at(i);
            if (m_params->drawCall == DrawCall::DIRECT)
                ctx.vkd.cmdDrawIndexed(commandBuffer, drawCmd.indexCount, drawCmd.instanceCount, drawCmd.firstIndex,
                                       drawCmd.vertexOffset, drawCmd.firstInstance);
            else if (m_params->drawCall == DrawCall::INDIRECT)
                ctx.vkd.cmdDrawIndexedIndirect(commandBuffer, indirectBuffers.at(i)->get(), 0ull, blockCount,
                                               indirectStride);
            else if (m_params->drawCall == DrawCall::INDIRECT_COUNT)
                ctx.vkd.cmdDrawIndexedIndirectCount(commandBuffer, indirectBuffers.at(i)->get(), 0ull,
                                                    countBuffer->get(), 0ull, maxCount, indirectStride);
            else if (m_params->drawCall == DrawCall::INDIRECT_2)
            {
                DE_ASSERT(false); // TO-DO: use the address of each buffer here.
            }
            else if (m_params->drawCall == DrawCall::INDIRECT_COUNT_2)
            {
                DE_ASSERT(false); // TO-DO: similar situation here, plus the count buffer.
            }
            else
                DE_ASSERT(false);

            // Finish conditional rendering section for the first block, so that it includes the draw command.
            if (m_params->conditionalRendering && i == 0u)
                ctx.vkd.cmdEndConditionalRenderingEXT(commandBuffer);
        }
    };

    Move<VkCommandBuffer> secondaryCmdBufferPtr;
    if (m_params->secondaryCmd)
    {
        secondaryCmdBufferPtr =
            allocateCommandBuffer(ctx.vkd, ctx.device, *cmd.cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
        const auto secCmd = *secondaryCmdBufferPtr;

        const VkCommandBufferInheritanceRenderingInfo inhRenderInfo{
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO,
            nullptr,
            0u,
            0u,
            1u,
            &colorFormat,
            VK_FORMAT_UNDEFINED,
            VK_FORMAT_UNDEFINED,
            VK_SAMPLE_COUNT_1_BIT,
        };
        const VkCommandBufferInheritanceRenderingInfo *inhPNext =
            (isConstructionTypeShaderObject(m_params->constructionType) ? &inhRenderInfo : nullptr);
        auto cmdUsageFlags = static_cast<VkCommandBufferUsageFlags>(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        if (isConstructionTypeShaderObject(m_params->constructionType))
            cmdUsageFlags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
        beginSecondaryCommandBuffer(ctx.vkd, secCmd, renderPass.get(), renderPass.getFramebuffer(), cmdUsageFlags,
                                    inhPNext);
        recordRPContents(secCmd);
        endCommandBuffer(ctx.vkd, secCmd);
    }

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    const auto subpassContents =
        (m_params->secondaryCmd ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE);
    renderPass.begin(ctx.vkd, cmdBuffer, scissors.front(), clearColor, subpassContents);
    if (m_params->secondaryCmd)
        ctx.vkd.cmdExecuteCommands(cmdBuffer, 1u, &secondaryCmdBufferPtr.get());
    else
        recordRPContents(cmdBuffer);
    renderPass.end(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), extent.swizzle(0, 1));
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());
    const auto tcuFormat = mapVkFormat(colorFormat);
    tcu::ConstPixelBufferAccess result(tcuFormat, extent, colorBuffer.getBufferAllocation().getHostPtr());

    tcu::TextureLevel refLevel(tcuFormat, extent.x(), extent.y(), extent.z());
    tcu::PixelBufferAccess reference = refLevel.getAccess();

    for (uint32_t b = 0u; b < blockCount; ++b)
    {
        const auto color = blockColors.at(b);
        for (uint32_t r = 0u; r < blockRows; ++r)
        {
            const int y = static_cast<int>(b * blockRows + r);
            for (int x = 0; x < extent.x(); ++x)
            {
                // Note when using conditional rendering, draws for the first block are disabled.
                if (x < extent.x() - 1 && (!m_params->conditionalRendering || b > 0u))
                    reference.setPixel(color, x, y);
                else
                    reference.setPixel(clearColor, x, y);
            }
        }
    }

    const tcu::Vec4 threshold(0.0f);
    auto &log = m_context.getTestContext().getLog();

    if (!tcu::floatThresholdCompare(log, "Result", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected results found in color buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

std::string getTopologySimpleName(VkPrimitiveTopology topology)
{
    const auto fullName = getPrimitiveTopologyName(topology);
    return de::toLower(fullName + strlen("VK_PRIMITIVE_TOPOLOGY_"));
}

std::string getIndexTypeSimpleName(VkIndexType indexType)
{
    const auto fullName = getIndexTypeName(indexType);
    return de::toLower(fullName + strlen("VK_INDEX_TYPE_")) + "_index";
}

std::string getRestartIndexName(RestartIndex restartIndex)
{
    if (restartIndex == RestartIndex::ZERO)
        return "zero";
    if (restartIndex == RestartIndex::ONE)
        return "one";
    if (restartIndex == RestartIndex::PENULTIMATE)
        return "max_minus_one";
    if (restartIndex == RestartIndex::MAX)
        return "max";

    DE_ASSERT(false);
    return "";
}

std::string getDrawCallName(DrawCall drawCall)
{
    if (drawCall == DrawCall::DIRECT)
        return "draw_indexed";
    if (drawCall == DrawCall::INDIRECT)
        return "draw_indexed_indirect";
    if (drawCall == DrawCall::INDIRECT_COUNT)
        return "draw_indexed_indirect_count";
    //if (drawCall == DrawCall::INDIRECT_2)
    //    return "draw_indexed_indirect_2";
    //if (drawCall == DrawCall::INDIRECT_COUNT_2)
    //    return "draw_indexed_indirect_count_2";

    DE_ASSERT(false);
    return "";
}

std::vector<VkPrimitiveTopology> getTopologies()
{
    const std::vector<VkPrimitiveTopology> topologies{
        VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
        VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
        VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
        VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,
        VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY,
        VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
    };

    return topologies;
}

std::vector<VkIndexType> getIndexTypes()
{
    const std::vector<VkIndexType> indexTypes{
        VK_INDEX_TYPE_UINT32,
        VK_INDEX_TYPE_UINT16,
        VK_INDEX_TYPE_UINT8,
    };

    return indexTypes;
}

std::vector<RestartIndex> getRestartIndices()
{
    const std::vector<RestartIndex> restartIndices{
        RestartIndex::ZERO,
        RestartIndex::ONE,
        RestartIndex::PENULTIMATE,
        RestartIndex::MAX,
    };

    return restartIndices;
}

std::vector<DrawCall> getDrawCalls()
{
    const std::vector<DrawCall> drawCalls{
        DrawCall::DIRECT, DrawCall::INDIRECT, DrawCall::INDIRECT_COUNT,
        //DrawCall::INDIRECT_2,
        //DrawCall::INDIRECT_COUNT_2,
    };

    return drawCalls;
}

} // anonymous namespace

tcu::TestCaseGroup *createPrimitiveRestartIndexTests(tcu::TestContext &testCtx,
                                                     PipelineConstructionType pipelineConstructionType)
{
    return createTestGroup(
        testCtx, "primitive_restart_index",
        [=](tcu::TestCaseGroup *mainGroup)
        {
            for (const auto topology : getTopologies())
            {
                mainGroup->addChild(createTestGroup(
                    mainGroup->getTestContext(), getTopologySimpleName(topology),
                    [=](tcu::TestCaseGroup *topologyGroup)
                    {
                        for (const auto indexType : getIndexTypes())
                        {
                            topologyGroup->addChild(createTestGroup(
                                topologyGroup->getTestContext(), getIndexTypeSimpleName(indexType),
                                [=](tcu::TestCaseGroup *indexTypeGroup)
                                {
                                    for (const auto restartIndex : getRestartIndices())
                                    {
                                        for (const auto drawCall : getDrawCalls())
                                        {
                                            for (const bool dynamicPrimRestart : {false, true})
                                            {
                                                ParamsPtr params(new Params{
                                                    pipelineConstructionType,
                                                    topology,
                                                    indexType,
                                                    restartIndex,
                                                    drawCall,
                                                    dynamicPrimRestart,
                                                    false,
                                                    false,
                                                });

                                                const auto testName = "custom_index_" +
                                                                      getRestartIndexName(restartIndex) + "_" +
                                                                      getDrawCallName(drawCall) +
                                                                      (dynamicPrimRestart ? "_dyn_prim_restart" : "");

                                                indexTypeGroup->addChild(new RestartIndexCase(
                                                    indexTypeGroup->getTestContext(), testName, params));
                                            }
                                        }
                                    }
                                }));
                        }
                    }));
            }

            mainGroup->addChild(createTestGroup(mainGroup->getTestContext(), "secondary_cmd",
                                                [=](tcu::TestCaseGroup *secondariesGroup)
                                                {
                                                    for (const auto drawCall : getDrawCalls())
                                                    {
                                                        for (const bool dynamicPrimRestart : {false, true})
                                                        {
                                                            ParamsPtr params(new Params{
                                                                pipelineConstructionType,
                                                                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
                                                                VK_INDEX_TYPE_UINT16,
                                                                RestartIndex::ONE,
                                                                drawCall,
                                                                dynamicPrimRestart,
                                                                false,
                                                                true,
                                                            });

                                                            const auto testName =
                                                                getDrawCallName(drawCall) +
                                                                (dynamicPrimRestart ? "_dyn_prim_restart" : "");

                                                            secondariesGroup->addChild(new RestartIndexCase(
                                                                secondariesGroup->getTestContext(), testName, params));
                                                        }
                                                    }
                                                }));

            mainGroup->addChild(createTestGroup(
                mainGroup->getTestContext(), "conditional_rendering",
                [=](tcu::TestCaseGroup *crGroup)
                {
                    for (const auto drawCall : getDrawCalls())
                    {
                        for (const bool dynamicPrimRestart : {false, true})
                        {
                            // We are also testing some secondaries with this.
                            for (const bool useSecondaries : {false, true})
                            {
                                ParamsPtr params(new Params{
                                    pipelineConstructionType,
                                    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
                                    VK_INDEX_TYPE_UINT32,
                                    RestartIndex::ZERO,
                                    drawCall,
                                    dynamicPrimRestart,
                                    true,
                                    useSecondaries,
                                });

                                const auto testName = getDrawCallName(drawCall) +
                                                      (dynamicPrimRestart ? "_dyn_prim_restart" : "") +
                                                      (useSecondaries ? "_secondary_cmd" : "");

                                crGroup->addChild(new RestartIndexCase(crGroup->getTestContext(), testName, params));
                            }
                        }
                    }
                }));
        });
}

} // namespace pipeline
} // namespace vkt
