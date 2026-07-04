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
 * \brief Device Generated Commands EXT Graphics Multiview Tests
 *//*--------------------------------------------------------------------*/

#include "vktDGCGraphicsMultiviewTestsExt.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktDGCUtilCommon.hpp"
#include "vktDGCUtilExt.hpp"

#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"

#include <algorithm>
#include <memory>
#include <numeric>
#include <sstream>

namespace vkt
{
namespace DGC
{

namespace
{

// Some notes about the test mechanism:
//
// The tests use a 2x2 framebuffer with 2 layers. The framebuffer is divided
// into 4 quadrants, and each quadrant will be drawn over with a triangle strip
// (4 vertices). Each quadrant will also be handled by a separate draw call,
// which implies 4 DGC sequences.
//
// The color for each quadrant will be chosen in the fragment shader, based on
// gl_FragCoord. The fragment shader has an array of colors, assigns an index to
// each quadrant (see below) and picks the color from the array.
//
// When using an indirect execution set, two different frag shaders will be
// used, and the second one has its list of colors reversed to make a
// difference. Each quadrant will alternate the frag shader used.
//
// Every color in the list has zero in the red component. The frag shader will
// set it to the gl_ViewIndex built-in, making it 1 or 0 depending on the view
// (image layer), so we know the built-in works.
//
// Quadrant indices for the color array (in binary):
//
// - 00 10
// - 01 11
//
// In addition, there will only be a single vertex buffer and a single index
// buffer. The vertex buffer will contain 4 sets of 4 vertices, one strip for
// each quadrant, in the order indicated above. The index buffer will contain an
// ordered list of indices from 0 to 15.
//
// In variants that use index and vertex buffer tokens in the DGC sequence, the
// vertex and index buffer addresses will point to the start of the data for
// each quadrant. This implies some care when dealing with the first
// vertex/index values and index offsets. See the code below. If the DGC
// sequence does not use these tokens, the full buffers will be bound once at
// the start and draw command values are more obvious.
//
// The vertices for each quadrant will also have unique depth values and the
// depth test is configured so that the depth value is always overwritten in
// practice.
//
// The expected output takes into account the view mask, to know which layers
// will have content, and the colors for each quadrant in each layer, which
// depend on the layer index and which frag shader was used. The depth buffer
// expects each quadrant to have depth values matching the vertices used.

using namespace vk;

struct Params
{
    uint32_t viewMask; // Subpass view mask.
    bool useIES;       // Indirect execution sets.
    bool useGPL;       // Graphics Pipeline Libraries.
    bool useIndices;   // Indexed draws.
    bool bufferTokens; // Set the vertex and maybe index buffers from tokens, varying offsets.
    bool preprocess;
    bool dynamicRendering;

    uint32_t getLayerCount() const
    {
        return 2u;
    }

    std::vector<tcu::Vec4> getQuadrantBaseColors() const
    {
        return std::vector<tcu::Vec4>{
            tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
            tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f),
            tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f),
            tcu::Vec4(0.0f, 1.0f, 1.0f, 1.0f),
        };
    }

    std::vector<float> getQuadrantDepths() const
    {
        return std::vector<float>{
            0.25f,
            0.50f,
            0.75f,
            1.00f,
        };
    }

    tcu::Vec4 getClearColor() const
    {
        return tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    float getClearDepth() const
    {
        return 0.0f;
    }

    uint32_t getFragShaderCount() const
    {
        return (useIES ? 2u : 1u);
    }

    VkShaderStageFlags getShaderStages() const
    {
        return static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }
};
using ParamsPtr = std::shared_ptr<const Params>;

void checkSupport(Context &context, ParamsPtr params)
{
    const auto stages                 = params->getShaderStages();
    const auto bindStagesPipeline     = (params->useIES ? stages : 0u);
    const auto bindStagesShaderObject = 0u;
    const auto inputModeFlags         = static_cast<VkIndirectCommandsInputModeFlagsEXT>(
        params->useIndices && params->bufferTokens ? VK_INDIRECT_COMMANDS_INPUT_MODE_VULKAN_INDEX_BUFFER_EXT : 0);
    checkDGCExtSupport(context, stages, bindStagesPipeline, bindStagesShaderObject, inputModeFlags);

    if (params->useGPL)
        context.requireDeviceFunctionality("VK_EXT_graphics_pipeline_library");

    if (params->bufferTokens)
        context.requireDeviceFunctionality("VK_EXT_extended_dynamic_state");

    if (params->dynamicRendering)
        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

    context.requireDeviceFunctionality("VK_KHR_multiview");
}

void initPrograms(vk::SourceCollections &dst, ParamsPtr params)
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "void main(void) {\n"
         << "    gl_Position = inPos;\n"
         << "    gl_PointSize = 1.0;\n"
         << "}\n";
    dst.glslSources.add("vert") << glu::VertexSource(vert.str());

    const auto fragShaderCount = params->getFragShaderCount();
    for (uint32_t i = 0u; i < fragShaderCount; ++i)
    {
        auto colors = params->getQuadrantBaseColors();
        if (i > 0u)
            std::reverse(colors.begin(), colors.end());

        std::ostringstream frag;
        frag << "#version 460\n"
             << "#extension GL_EXT_multiview : require\n"
             << "layout (location=0) out vec4 outColor;\n"
             << "layout (push_constant, std430) uniform PushConstantBlock { vec4 fbSize; } pc;\n"
             << "vec4 quadColors[4] = vec4[4](\n";

        for (size_t j = 0; j < colors.size(); ++j)
            frag << "    vec4" << colors.at(j) << (j < colors.size() - 1 ? "," : "") << "\n";

        frag << ");\n"
             << "void main(void) {\n"
             << "    vec2 normCoord = (gl_FragCoord.xy / pc.fbSize.xy) * 2.0 - 1.0;\n"
             << "    // Quad Index: top-left: 0, bottom-left: 1, top-right: 2, bottom-right: 3\n"
             << "    uint quadIndex = 0u;\n"
             << "    if (normCoord.x > 0.0)\n"
             << "        quadIndex |= 2u;\n"
             << "    if (normCoord.y > 0.0)\n"
             << "        quadIndex |= 1u;\n"
             << "    vec4 chosenColor = quadColors[quadIndex];\n"
             << "    chosenColor.r = float(gl_ViewIndex);\n"
             << "    outColor = chosenColor;\n"
             << "}\n";
        const auto shaderName = "frag" + std::to_string(i);
        dst.glslSources.add(shaderName) << glu::FragmentSource(frag.str());
    }
}

tcu::TestStatus iterate(Context &context, ParamsPtr params)
{
    const auto ctx = context.getContextCommonData();
    const tcu::IVec3 extent(2, 2, static_cast<int>(params->getLayerCount()));
    const auto extent3D    = makeExtent3D(extent);
    const auto extent2D    = makeExtent3D(extent3D.width, extent3D.height, 1u);
    const auto layerCount  = extent3D.depth;
    const auto floatExtent = extent.asFloat();
    const auto pcValue     = tcu::Vec4(floatExtent.x(), floatExtent.y(), floatExtent.z(), 0.0f);

    const auto kQuadrantCount             = 4u;
    const auto kQuadrantVertices          = 4u;
    const auto kQuadrantVertexBufferBytes = kQuadrantVertices * DE_SIZEOF32(tcu::Vec4);
    const auto kQuadrantIndexBufferBytes  = kQuadrantVertices * DE_SIZEOF32(uint32_t);

    // Vertices.
    const std::vector<tcu::Vec4> stdVertices{
        tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
        tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
    };
    DE_ASSERT(stdVertices.size() == static_cast<size_t>(kQuadrantVertices));

    const auto quadrantDepths = params->getQuadrantDepths();
    DE_ASSERT(quadrantDepths.size() == static_cast<size_t>(kQuadrantCount));

    // To apply to the 0-1 standard coordinates.
    const std::vector<tcu::Vec4> quadrantOffsets{
        // clang-format off
        tcu::Vec4(-1.0f, -1.0f, 0.0f, 0.0f),
        tcu::Vec4(-1.0f,  0.0f, 0.0f, 0.0f),
        tcu::Vec4( 0.0f, -1.0f, 0.0f, 0.0f),
        tcu::Vec4( 0.0f,  0.0f, 0.0f, 0.0f),
        // clang-format on
    };
    DE_ASSERT(quadrantOffsets.size() == static_cast<size_t>(kQuadrantCount));

    std::vector<tcu::Vec4> vertices;
    vertices.reserve(kQuadrantVertices * kQuadrantCount);
    for (uint32_t i = 0u; i < kQuadrantCount; ++i)
    {
        const auto coordOffset    = quadrantOffsets.at(i);
        const float quadrantDepth = quadrantDepths.at(i);

        auto quadrantVertices = stdVertices;
        for (size_t j = 0; j < quadrantVertices.size(); ++j)
        {
            auto &vert = quadrantVertices.at(j);
            vert += coordOffset;
            vert.z() = quadrantDepth;
        }

        vertices.insert(vertices.end(), quadrantVertices.begin(), quadrantVertices.end());
    }

    const auto bufferAllocFlags =
        static_cast<VkMemoryAllocateFlags>(params->bufferTokens ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT : 0);
    const auto extraVertIdxBufferUsage =
        static_cast<VkBufferUsageFlags>(params->bufferTokens ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0);
    const auto vertexBufferSize = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vertexBufferUsage =
        (static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) | extraVertIdxBufferUsage);
    const auto vertexBufferInfo = makeBufferCreateInfo(vertexBufferSize, vertexBufferUsage);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferInfo, HostIntent::W, true,
                                  bufferAllocFlags);
    {
        auto &alloc = vertexBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(vertices), de::dataSize(vertices));
        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    // Indices.
    std::unique_ptr<BufferWithMemory> indexBuffer;
    if (params->useIndices)
    {
        std::vector<uint32_t> indices(vertices.size(), 0u);
        std::iota(indices.begin(), indices.end(), 0u);

        const auto indexBufferSize = static_cast<VkDeviceSize>(de::dataSize(indices));
        const auto indexBufferUsage =
            (static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_INDEX_BUFFER_BIT) | extraVertIdxBufferUsage);
        const auto indexBufferInfo = makeBufferCreateInfo(indexBufferSize, indexBufferUsage);
        indexBuffer.reset(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, indexBufferInfo, HostIntent::W, true,
                                               bufferAllocFlags));
        {
            auto &alloc = indexBuffer->getAllocation();
            memcpy(alloc.getHostPtr(), de::dataOrNull(indices), de::dataSize(indices));
            flushAlloc(ctx.vkd, ctx.device, alloc);
        }
    }

    // Color and depth buffers.
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto depthFormat = VK_FORMAT_D16_UNORM;

    const auto commonImgUsage =
        static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto colorUsage = (commonImgUsage | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    const auto depthUsage = (commonImgUsage | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

    const auto colorSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, layerCount);
    const auto depthSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, layerCount);

    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, extent2D, colorFormat, colorUsage, VK_IMAGE_TYPE_2D,
                                colorSRR, layerCount);
    ImageWithBuffer depthBuffer(ctx.vkd, ctx.device, ctx.allocator, extent2D, depthFormat, depthUsage, VK_IMAGE_TYPE_2D,
                                depthSRR, layerCount);

    const auto fragShaderCount = params->getFragShaderCount();
    const auto constructionType =
        (params->useGPL ? PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY : PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC);
    const auto pipelineCreationFlags =
        static_cast<VkPipelineCreateFlagBits2>(params->useIES ? VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT : 0);

    // Render pass.
    const std::vector<VkAttachmentDescription> rpAttDesc{
        makeAttachmentDescription(0u, colorFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_LOAD,
                                  VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                  VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
        makeAttachmentDescription(0u, depthFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_LOAD,
                                  VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                  VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
    };

    const std::vector<VkAttachmentReference> rpAttRefs{
        makeAttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
        makeAttachmentReference(1u, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
    };

    const std::vector<VkSubpassDescription> rpSubpasses{
        makeSubpassDescription(0u, VK_PIPELINE_BIND_POINT_GRAPHICS, 0u, nullptr, 1u, &rpAttRefs.front(), nullptr,
                               &rpAttRefs.back(), 0u, nullptr),
    };

    const VkRenderPassMultiviewCreateInfo rpMultiviewInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO, nullptr, 1u, &params->viewMask, 0u, nullptr, 0u, nullptr,
    };

    const VkRenderPassCreateInfo rpCreateInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        &rpMultiviewInfo,
        0u,
        de::sizeU32(rpAttDesc),
        de::dataOrNull(rpAttDesc),
        de::sizeU32(rpSubpasses),
        de::dataOrNull(rpSubpasses),
        0u,
        nullptr,
    };

    RenderPassWrapper renderPass(ctx.vkd, ctx.device, &rpCreateInfo, params->dynamicRendering);
    const std::vector<VkImage> fbImages{colorBuffer.getImage(), depthBuffer.getImage()};
    const std::vector<VkImageView> fbViews{colorBuffer.getImageView(), depthBuffer.getImageView()};
    DE_ASSERT(fbImages.size() == fbViews.size());
    renderPass.createFramebuffer(ctx.vkd, ctx.device, de::sizeU32(fbImages), de::dataOrNull(fbImages),
                                 de::dataOrNull(fbViews), extent2D.width, extent2D.height);

    // Shaders and pipelines.
    const auto &binaries = context.getBinaryCollection();
    ShaderWrapper vertShader(ctx.vkd, ctx.device, binaries.get("vert"));
    std::vector<ShaderWrapper> fragShaders;
    fragShaders.reserve(fragShaderCount);
    for (uint32_t i = 0u; i < fragShaderCount; ++i)
    {
        const auto shaderName = "frag" + std::to_string(i);
        fragShaders.emplace_back(ctx.vkd, ctx.device, binaries.get(shaderName));
    }

    using GraphicsPipelineWrapperPtr = std::unique_ptr<GraphicsPipelineWrapper>;
    std::vector<GraphicsPipelineWrapperPtr> pipelines;
    pipelines.reserve(fragShaderCount);

    const std::vector<VkViewport> viewports(1u, makeViewport(extent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(extent));

    const auto shaderStages = params->getShaderStages();
    const auto pcStages     = shaderStages;
    const auto pcSize       = DE_SIZEOF32(tcu::Vec4);
    const auto pcRange      = makePushConstantRange(pcStages, 0u, pcSize);

    PipelineLayoutWrapper pipelineLayout(constructionType, ctx.vkd, ctx.device, VK_NULL_HANDLE, &pcRange);

    const VkPipelineDepthStencilStateCreateInfo depthStencilState = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        nullptr,
        0u,
        VK_TRUE,
        VK_TRUE,
        VK_COMPARE_OP_GREATER_OR_EQUAL,
        VK_FALSE,
        VK_FALSE,
        VkStencilOpState{},
        VkStencilOpState{},
        0.0f,
        1.0f,
    };

    std::vector<VkDynamicState> dynamicStates;
    if (params->bufferTokens)
        dynamicStates.push_back(VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT);

    const VkPipelineDynamicStateCreateInfo dynamicStateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        nullptr,
        0u,
        de::sizeU32(dynamicStates),
        de::dataOrNull(dynamicStates),
    };

    // VkPipelineRenderingCreateInfo
    VkPipelineRenderingCreateInfo renderingCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        nullptr,
        params->viewMask,
        1u,
        &colorFormat,
        depthFormat,
        VK_FORMAT_UNDEFINED,
    };
    VkPipelineRenderingCreateInfo *pRenderingCreateInfo = (params->dynamicRendering ? &renderingCreateInfo : nullptr);

    for (uint32_t i = 0u; i < fragShaderCount; ++i)
    {
        pipelines.emplace_back(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                                           context.getDeviceExtensions(), constructionType));
        auto &pipeline            = *pipelines.back();
        renderingCreateInfo.pNext = nullptr;

        pipeline.setPipelineCreateFlags2(pipelineCreationFlags)
            .setDefaultVertexInputState(true)
            .setDefaultRasterizationState()
            .setDefaultMultisampleState()
            .setDefaultColorBlendState()
            .setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
            .setDynamicState(&dynamicStateInfo)
            .setupVertexInputState()
            .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass.get(), 0u, vertShader,
                                              nullptr, ShaderWrapper(), ShaderWrapper(), ShaderWrapper(), nullptr,
                                              nullptr, pRenderingCreateInfo)
            .setupFragmentShaderState(pipelineLayout, renderPass.get(), 0u, fragShaders.at(i), &depthStencilState)
            .setupFragmentOutputState(renderPass.get())
            .buildPipeline();
    }

    // Indirect execution set, if needed.
    ExecutionSetManagerPtr iesManager;
    if (params->useIES)
    {
        iesManager =
            makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, pipelines.front()->getPipeline(), fragShaderCount);

        for (uint32_t i = 0u; i < fragShaderCount; ++i)
            iesManager->addPipeline(i, pipelines.at(i)->getPipeline());

        iesManager->update();
    }

    // Indirect commands layout.
    const auto cmdsLayoutFlags = static_cast<VkIndirectCommandsLayoutUsageFlagsEXT>(
        params->preprocess ? VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_EXT : 0);
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(cmdsLayoutFlags, shaderStages, pipelineLayout.get());
    if (params->useIES)
    {
        const auto setType = VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT;
        cmdsLayoutBuilder.addExecutionSetToken(0u, setType, shaderStages);
    }
    if (params->bufferTokens)
    {
        cmdsLayoutBuilder.addVertexBufferToken(cmdsLayoutBuilder.getStreamRange(), 0u);
        if (params->useIndices)
            cmdsLayoutBuilder.addIndexBufferToken(cmdsLayoutBuilder.getStreamRange(),
                                                  VK_INDIRECT_COMMANDS_INPUT_MODE_VULKAN_INDEX_BUFFER_EXT);
    }
    if (params->useIndices)
        cmdsLayoutBuilder.addDrawIndexedToken(cmdsLayoutBuilder.getStreamRange());
    else
        cmdsLayoutBuilder.addDrawToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // DGC buffer with the command data.
    std::vector<uint8_t> dgcData;
    dgcData.reserve(cmdsLayoutBuilder.getStreamStride() * kQuadrantCount);
    for (uint32_t i = 0u; i < kQuadrantCount; ++i)
    {
        // Alternate pipelines.
        if (params->useIES)
        {
            const uint32_t pipelineIdx = (i % de::sizeU32(pipelines));
            pushBackElement(dgcData, pipelineIdx);
        }

        // Bind vertex/index buffers: bind only the subsection of the buffer that corresponds to the current quadrant.
        if (params->bufferTokens)
        {
            const auto baseVertexBDA     = getBufferDeviceAddress(ctx.vkd, ctx.device, vertexBuffer.get());
            const auto quadrantVertexBDA = baseVertexBDA + kQuadrantVertexBufferBytes * i;
            const VkBindVertexBufferIndirectCommandEXT vbCmd{
                quadrantVertexBDA,
                kQuadrantVertexBufferBytes,
                DE_SIZEOF32(tcu::Vec4),
            };
            pushBackElement(dgcData, vbCmd);

            if (params->useIndices)
            {
                const auto baseIndexBDA     = getBufferDeviceAddress(ctx.vkd, ctx.device, indexBuffer->get());
                const auto quadrantIndexBDA = baseIndexBDA + kQuadrantIndexBufferBytes * i;
                const VkBindIndexBufferIndirectCommandEXT ibCmd{
                    quadrantIndexBDA,
                    kQuadrantIndexBufferBytes,
                    VK_INDEX_TYPE_UINT32,
                };
                pushBackElement(dgcData, ibCmd);
            }
        }

        // Draw command. Some footguns in here.
        if (params->useIndices)
        {
            // The situation: when using buffer tokens, the buffer address is going to be updated with every sequence,
            // for every quadrant, and the addresses are going to point to the parts of each buffer where the quadrant
            // data starts, in terms of vertex coordinates and index buffer values. When not using buffer tokens, there
            // will be a prior vertex and index buffer bind and those are going to stay there for every sequence.
            //
            // Take the draw for quadrant 1, for example, with the interesting vertices in brackets:
            //
            // Vertex buffer: Q0V0 Q0V1 Q0V2 Q0V3 [Q1V0 Q1V1 Q1V2 Q1V3] ...
            // Index buffer:  0    1    2    3    [4    5    6    7   ] ...
            //
            // Without buffer tokens, buffers are bound completely as we see them here, and we want to draw those
            // vertices, so it's not an issue: first index is 4, index offset is 0. At position 4 we'll find vertex
            // index 4 (they match), which will draw Q1V0 from the start of the quadrant, etc.
            //
            // With buffer tokens, the bracketed subsection of both the vertex and the index buffers will be bound
            // instead. So firstIndex needs to be 0 to start with the first index in the bracketed section. Also, what
            // we find there is vertex index 4, so we need a vertex offset of -4 so that the final index is 0 and we
            // choose the first vertex from the bracketed section in the vertex buffer.
            const int32_t absOffset    = static_cast<int32_t>(i * kQuadrantVertices);
            const uint32_t firstIndex  = (params->bufferTokens ? 0u : static_cast<uint32_t>(absOffset));
            const int32_t vertexOffset = (params->bufferTokens ? -absOffset : 0);
            VkDrawIndexedIndirectCommand drawCmd{
                kQuadrantVertices, 1u, firstIndex, vertexOffset, 0u,
            };
            pushBackElement(dgcData, drawCmd);
        }
        else
        {
            // See the indexed draw comment for details.
            const uint32_t absOffset   = i * kQuadrantVertices;
            const uint32_t firstVertex = (params->bufferTokens ? 0u : absOffset);
            VkDrawIndirectCommand drawCmd{
                kQuadrantVertices,
                1u,
                firstVertex,
                0u,
            };
            pushBackElement(dgcData, drawCmd);
        }
    }

    const auto dgcBufferSize = static_cast<VkDeviceSize>(de::dataSize(dgcData));
    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, dgcBufferSize);
    {
        auto &alloc = dgcBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(dgcData), de::dataSize(dgcData));
        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    // Preprocess buffer.
    const auto preprocessIES      = (params->useIES ? iesManager->get() : VK_NULL_HANDLE);
    const auto preprocessPipeline = (params->useIES ? VK_NULL_HANDLE : pipelines.front()->getPipeline());
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, preprocessIES, *cmdsLayout, kQuadrantCount,
                                         0u, preprocessPipeline);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;
    Move<VkCommandBuffer> preprocessCmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    {
        // Clear both images. This is done outside the render pass to make sure the full image is cleared.
        const std::vector<VkImageMemoryBarrier> preClearBarriers{
            makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, colorBuffer.getImage(), colorSRR),
            makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, depthBuffer.getImage(), depthSRR),
        };
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, preClearBarriers.data(), preClearBarriers.size());

        const auto colorClearValue = makeClearValueColor(params->getClearColor());
        const auto depthClearValue = makeClearValueDepthStencil(params->getClearDepth(), 0u);

        ctx.vkd.cmdClearColorImage(cmdBuffer, colorBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   &colorClearValue.color, 1u, &colorSRR);
        ctx.vkd.cmdClearDepthStencilImage(cmdBuffer, depthBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                          &depthClearValue.depthStencil, 1u, &depthSRR);

        const std::vector<VkImageMemoryBarrier> postClearBarriers{
            makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                   (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   colorBuffer.getImage(), colorSRR),
            makeImageMemoryBarrier(
                VK_ACCESS_TRANSFER_WRITE_BIT,
                (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT),
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                depthBuffer.getImage(), depthSRR),
        };

        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                       VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT),
                                      postClearBarriers.data(), postClearBarriers.size());
    }

    renderPass.begin(ctx.vkd, cmdBuffer, scissors.front());

    if (!params->bufferTokens)
    {
        const VkDeviceSize vertexBufferOffset = 0ull;
        ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
        if (params->useIndices)
            ctx.vkd.cmdBindIndexBuffer(cmdBuffer, indexBuffer->get(), 0ull, VK_INDEX_TYPE_UINT32);
    }

    // We always need to bind an initial pipeline state.
    pipelines.front()->bind(cmdBuffer);

    ctx.vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), pcStages, 0u, pcSize, &pcValue);

    DGCGenCmdsInfo genCmdsInfo(shaderStages, (params->useIES ? iesManager->get() : VK_NULL_HANDLE), *cmdsLayout,
                               dgcBuffer.getDeviceAddress(), dgcBuffer.getSize(), preprocessBuffer.getDeviceAddress(),
                               preprocessBuffer.getSize(), kQuadrantCount, 0ull, 0u, preprocessPipeline);
    if (params->preprocess)
    {
        preprocessCmdBuffer = allocateCommandBuffer(ctx.vkd, ctx.device, *cmd.cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        beginCommandBuffer(ctx.vkd, *preprocessCmdBuffer);
        ctx.vkd.cmdPreprocessGeneratedCommandsEXT(*preprocessCmdBuffer, &genCmdsInfo.get(), cmdBuffer);
        preprocessToExecuteBarrierExt(ctx.vkd, *preprocessCmdBuffer);
        endCommandBuffer(ctx.vkd, *preprocessCmdBuffer);
    }
    ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, makeVkBool(params->preprocess), &genCmdsInfo.get());
    renderPass.end(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), extent.swizzle(0, 1),
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, layerCount);
    copyImageToBuffer(ctx.vkd, cmdBuffer, depthBuffer.getImage(), depthBuffer.getBuffer(), extent.swizzle(0, 1),
                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                      layerCount, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitAndWaitWithPreprocess(ctx.vkd, ctx.device, ctx.queue, cmdBuffer, *preprocessCmdBuffer);

    const auto tcuColorFormat = mapVkFormat(colorFormat);
    const auto tcuDepthFormat = mapVkFormat(depthFormat);

    // Result images.
    invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());
    tcu::ConstPixelBufferAccess colorResult(tcuColorFormat, extent, colorBuffer.getBufferAllocation().getHostPtr());

    invalidateAlloc(ctx.vkd, ctx.device, depthBuffer.getBufferAllocation());
    tcu::ConstPixelBufferAccess depthResult(tcuDepthFormat, extent, depthBuffer.getBufferAllocation().getHostPtr());

    // Reference images.
    tcu::TextureLevel colorRefLevel(tcuColorFormat, extent.x(), extent.y(), extent.z());
    tcu::PixelBufferAccess colorReference = colorRefLevel.getAccess();

    tcu::TextureLevel depthRefLevel(tcuDepthFormat, extent.x(), extent.y(), extent.z());
    tcu::PixelBufferAccess depthReference = depthRefLevel.getAccess();

    const auto baseColors = params->getQuadrantBaseColors();
    auto reverseColors    = baseColors;
    std::reverse(reverseColors.begin(), reverseColors.end());

    const int quadrantWidth  = extent.x() / 2;
    const int quadrantHeight = extent.y() / 2;

    const tcu::Vec4 colorThreshold(0.0f);
    const float depthThreshold = 0.00002f; // A bit over 1 unit for VK_FORMAT_D16_UNORM.
    auto &log                  = context.getTestContext().getLog();
    bool fail                  = false;

    for (uint32_t i = 0u; i < layerCount; ++i)
    {
        auto colorRefLayer = tcu::getSubregion(colorReference, 0, 0, static_cast<int>(i), extent.x(), extent.y(), 1);
        auto colorResLayer = tcu::getSubregion(colorResult, 0, 0, static_cast<int>(i), extent.x(), extent.y(), 1);

        auto depthRefLayer = tcu::getSubregion(depthReference, 0, 0, static_cast<int>(i), extent.x(), extent.y(), 1);
        auto depthResLayer = tcu::getSubregion(depthResult, 0, 0, static_cast<int>(i), extent.x(), extent.y(), 1);

        if ((params->viewMask & (1u << i)) == 0u)
        {
            tcu::clear(colorRefLayer, params->getClearColor());
            tcu::clearDepth(depthRefLayer, params->getClearDepth());
        }
        else
        {
            for (uint32_t j = 0u; j < kQuadrantCount; ++j)
            {
                // Find the quadrant coordinates using the quadrant index.
                const int xFactor = static_cast<int>((j >> 1) & 1u);
                const int yFactor = static_cast<int>(j & 1u);

                const int x = quadrantWidth * xFactor;
                const int y = quadrantHeight * yFactor;

                auto colorRefQuadrant = tcu::getSubregion(colorRefLayer, x, y, quadrantWidth, quadrantHeight);

                const auto &quadrantColors =
                    ((!params->useIES || (j % de::sizeU32(pipelines)) == 0u) ? baseColors : reverseColors);
                auto quadrantColor = quadrantColors.at(j);
                quadrantColor.x()  = static_cast<float>(i); // Red reflects the layer (view).

                tcu::clear(colorRefQuadrant, quadrantColor);

                auto depthRefQuadrant = tcu::getSubregion(depthRefLayer, x, y, quadrantWidth, quadrantHeight);
                tcu::clearDepth(depthRefQuadrant, quadrantDepths.at(j));
            }
        }

        // Compare layer by layer (easier to see in the logs).
        const auto colorSetName = "ColorLayer" + std::to_string(i);
        if (!tcu::floatThresholdCompare(log, colorSetName.c_str(), "", colorRefLayer, colorResLayer, colorThreshold,
                                        tcu::COMPARE_LOG_ON_ERROR))
            fail = true;

        const auto depthSetName = "DepthLayer" + std::to_string(i);
        if (!tcu::dsThresholdCompare(log, depthSetName.c_str(), "", depthRefLayer, depthResLayer, depthThreshold,
                                     tcu::COMPARE_LOG_ON_ERROR))
            fail = true;
    }

    if (fail)
        TCU_FAIL("Unexpected results in color or depth buffers; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

struct PipelineGroupParams
{
    uint32_t viewMask;
    bool useIES;
    bool useGPL;
};

void populatePipelineGroup(tcu::TestCaseGroup *pipeGroup, PipelineGroupParams groupParams)
{
    for (const bool useIndices : {false, true})
        for (const bool bufferTokens : {false, true})
            for (const bool dynamicRendering : {false, true})
                for (const bool preprocess : {false, true})
                {
                    const auto testName = std::string(useIndices ? "indexed_draw" : "regular_draw") +
                                          std::string(bufferTokens ? "_buffer_tokens" : "") +
                                          std::string(preprocess ? "_preprocess" : "") +
                                          std::string(dynamicRendering ? "_dynamic_rendering" : "");

                    ParamsPtr params(new Params{
                        groupParams.viewMask,
                        groupParams.useIES,
                        groupParams.useGPL,
                        useIndices,
                        bufferTokens,
                        preprocess,
                        dynamicRendering,
                    });

                    addFunctionCaseWithPrograms(pipeGroup, testName, checkSupport, initPrograms, iterate, params);
                }
}

void populateMaskTypeGroup(tcu::TestCaseGroup *maskTypeGroup, uint32_t viewMask)
{
    for (const bool useIES : {false, true})
    {
        if (useIES)
            continue; // DGC + multiview + IES is banned by the spec.

        for (const bool useGPL : {false, true})
        {
            const auto groupName =
                std::string(useIES ? "with_ies" : "no_ies") + std::string(useGPL ? "_gpl" : "_monolithic");

            const PipelineGroupParams groupParams{viewMask, useIES, useGPL};

            maskTypeGroup->addChild(
                createTestGroup(maskTypeGroup->getTestContext(), groupName, populatePipelineGroup, groupParams));
        }
    }
}

void populateMainGroup(tcu::TestCaseGroup *mainGroup)
{
    for (const uint32_t viewMask : {3u, 1u, 2u})
    {
        const auto groupName = "view_mask_" + std::to_string(viewMask);
        mainGroup->addChild(createTestGroup(mainGroup->getTestContext(), groupName, populateMaskTypeGroup, viewMask));
    }
}

} // anonymous namespace

tcu::TestCaseGroup *createDGCGraphicsMultiviewTestsExt(tcu::TestContext &testCtx)
{
    return createTestGroup(testCtx, "multiview", populateMainGroup);
}

} // namespace DGC
} // namespace vkt
