/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2014 The Android Open Source Project
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Tessellation Miscellaneous Draw Tests
 *//*--------------------------------------------------------------------*/

#include "vktTessellationMiscDrawTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTessellationUtil.hpp"
#include "vktAmberTestCase.hpp"

#include "tcuTestLog.hpp"
#include "tcuImageIO.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include "vkDefs.hpp"
#include "vkBarrierUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

#include "rrRenderer.hpp"

#include <string>
#include <vector>
#include <utility>

namespace vkt
{
namespace tessellation
{

using namespace vk;

namespace
{

typedef de::MovePtr<vk::Allocation> AllocationMp;

struct CaseDefinition
{
    TessPrimitiveType primitiveType;
    SpacingMode spacingMode;
    DrawType drawType;
    std::string referenceImagePathPrefix; //!< without case suffix and extension (e.g. "_1.png")
};

inline CaseDefinition makeCaseDefinition(const TessPrimitiveType primitiveType, const SpacingMode spacingMode,
                                         const DrawType drawType, const std::string &referenceImagePathPrefix)
{
    CaseDefinition caseDef;
    caseDef.primitiveType            = primitiveType;
    caseDef.spacingMode              = spacingMode;
    caseDef.drawType                 = drawType;
    caseDef.referenceImagePathPrefix = referenceImagePathPrefix;
    return caseDef;
}

std::vector<TessLevels> genTessLevelCases(const SpacingMode spacingMode)
{
    static const TessLevels tessLevelCases[] = {
        {{9.0f, 9.0f}, {9.0f, 9.0f, 9.0f, 9.0f}},
        {{8.0f, 11.0f}, {13.0f, 15.0f, 18.0f, 21.0f}},
        {{17.0f, 14.0f}, {3.0f, 6.0f, 9.0f, 12.0f}},
    };

    std::vector<TessLevels> resultTessLevels(DE_LENGTH_OF_ARRAY(tessLevelCases));

    for (int tessLevelCaseNdx = 0; tessLevelCaseNdx < DE_LENGTH_OF_ARRAY(tessLevelCases); ++tessLevelCaseNdx)
    {
        TessLevels &tessLevels = resultTessLevels[tessLevelCaseNdx];

        for (int i = 0; i < 2; ++i)
            tessLevels.inner[i] =
                static_cast<float>(getClampedRoundedTessLevel(spacingMode, tessLevelCases[tessLevelCaseNdx].inner[i]));

        for (int i = 0; i < 4; ++i)
            tessLevels.outer[i] =
                static_cast<float>(getClampedRoundedTessLevel(spacingMode, tessLevelCases[tessLevelCaseNdx].outer[i]));
    }

    return resultTessLevels;
}

std::vector<tcu::Vec2> genVertexPositions(const TessPrimitiveType primitiveType)
{
    std::vector<tcu::Vec2> positions;
    positions.reserve(4);

    if (primitiveType == TESSPRIMITIVETYPE_TRIANGLES)
    {
        positions.push_back(tcu::Vec2(0.8f, 0.6f));
        positions.push_back(tcu::Vec2(0.0f, -0.786f));
        positions.push_back(tcu::Vec2(-0.8f, 0.6f));
    }
    else if (primitiveType == TESSPRIMITIVETYPE_QUADS || primitiveType == TESSPRIMITIVETYPE_ISOLINES)
    {
        positions.push_back(tcu::Vec2(-0.8f, -0.8f));
        positions.push_back(tcu::Vec2(0.8f, -0.8f));
        positions.push_back(tcu::Vec2(-0.8f, 0.8f));
        positions.push_back(tcu::Vec2(0.8f, 0.8f));
    }
    else
        DE_ASSERT(false);

    return positions;
}

//! Common test function used by all test cases.
tcu::TestStatus runTest(Context &context, const CaseDefinition caseDef)
{
    requireFeatures(context.getInstanceInterface(), context.getPhysicalDevice(), FEATURE_TESSELLATION_SHADER);

    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkDevice device           = context.getDevice();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = context.getDefaultAllocator();

    const std::vector<TessLevels> tessLevelCases = genTessLevelCases(caseDef.spacingMode);
    const std::vector<tcu::Vec2> vertexData      = genVertexPositions(caseDef.primitiveType);
    const uint32_t inPatchSize                   = (caseDef.primitiveType == TESSPRIMITIVETYPE_TRIANGLES ? 3 : 4);

    // Vertex input: positions

    const VkFormat vertexFormat            = VK_FORMAT_R32G32_SFLOAT;
    const uint32_t vertexStride            = tcu::getPixelSize(mapVkFormat(vertexFormat));
    const VkDeviceSize vertexDataSizeBytes = sizeInBytes(vertexData);

    const BufferWithMemory vertexBuffer(vk, device, allocator,
                                        makeBufferCreateInfo(vertexDataSizeBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
                                        MemoryRequirement::HostVisible);

    DE_ASSERT(inPatchSize == vertexData.size());
    DE_ASSERT(sizeof(vertexData[0]) == vertexStride);

    {
        const Allocation &alloc = vertexBuffer.getAllocation();

        deMemcpy(alloc.getHostPtr(), &vertexData[0], static_cast<std::size_t>(vertexDataSizeBytes));
        flushAlloc(vk, device, alloc);
        // No barrier needed, flushed memory is automatically visible
    }

    // Indirect buffer

    const VkDrawIndirectCommand drawIndirectArgs{
        inPatchSize, // uint32_t vertexCount;
        1u,          // uint32_t instanceCount;
        0u,          // uint32_t firstVertex;
        0u,          // uint32_t firstInstance;
    };

    const BufferWithMemory indirectBuffer(
        vk, device, allocator, makeBufferCreateInfo(sizeof(drawIndirectArgs), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
        MemoryRequirement::HostVisible);

    {
        const Allocation &alloc = indirectBuffer.getAllocation();

        deMemcpy(alloc.getHostPtr(), &drawIndirectArgs, sizeof(drawIndirectArgs));
        flushAlloc(vk, device, alloc);
        // No barrier needed, flushed memory is automatically visible
    }

    // Color attachment

    const tcu::IVec2 renderSize = tcu::IVec2(256, 256);
    const VkFormat colorFormat  = VK_FORMAT_R8G8B8A8_UNORM;
    const VkImageSubresourceRange colorImageSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const ImageWithMemory colorAttachmentImage(
        vk, device, allocator,
        makeImageCreateInfo(renderSize, colorFormat,
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 1u),
        MemoryRequirement::Any);

    // Color output buffer: image will be copied here for verification

    const VkDeviceSize colorBufferSizeBytes =
        renderSize.x() * renderSize.y() * tcu::getPixelSize(mapVkFormat(colorFormat));
    const BufferWithMemory colorBuffer(vk, device, allocator,
                                       makeBufferCreateInfo(colorBufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                                       MemoryRequirement::HostVisible);

    // Input buffer: tessellation levels. Data is filled in later.

    const BufferWithMemory tessLevelsBuffer(
        vk, device, allocator, makeBufferCreateInfo(sizeof(TessLevels), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        MemoryRequirement::HostVisible);

    // Descriptors

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                              VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
            .build(vk, device));

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    const VkDescriptorBufferInfo tessLevelsBufferInfo =
        makeDescriptorBufferInfo(tessLevelsBuffer.get(), 0ull, sizeof(TessLevels));

    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &tessLevelsBufferInfo)
        .update(vk, device);

    // Pipeline

    const Unique<VkImageView> colorAttachmentView(makeImageView(
        vk, device, *colorAttachmentImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorImageSubresourceRange));
    const Unique<VkRenderPass> renderPass(makeRenderPass(vk, device, colorFormat));
    const Unique<VkFramebuffer> framebuffer(
        makeFramebuffer(vk, device, *renderPass, *colorAttachmentView, renderSize.x(), renderSize.y()));
    const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
    const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    const Unique<VkPipeline> pipeline(
        GraphicsPipelineBuilder()
            .setRenderSize(renderSize)
            .setVertexInputSingleAttribute(vertexFormat, vertexStride)
            .setPatchControlPoints(inPatchSize)
            .setShader(vk, device, VK_SHADER_STAGE_VERTEX_BIT, context.getBinaryCollection().get("vert"), nullptr)
            .setShader(vk, device, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, context.getBinaryCollection().get("tesc"),
                       nullptr)
            .setShader(vk, device, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                       context.getBinaryCollection().get("tese"), nullptr)
            .setShader(vk, device, VK_SHADER_STAGE_FRAGMENT_BIT, context.getBinaryCollection().get("frag"), nullptr)
            .build(vk, device, *pipelineLayout, *renderPass));

    // Draw commands

    uint32_t numPassedCases = 0;

    for (uint32_t tessLevelCaseNdx = 0; tessLevelCaseNdx < tessLevelCases.size(); ++tessLevelCaseNdx)
    {
        context.getTestContext().getLog()
            << tcu::TestLog::Message << "Tessellation levels: "
            << getTessellationLevelsString(tessLevelCases[tessLevelCaseNdx], caseDef.primitiveType)
            << tcu::TestLog::EndMessage;

        // Upload tessellation levels data to the input buffer
        {
            const Allocation &alloc            = tessLevelsBuffer.getAllocation();
            TessLevels *const bufferTessLevels = static_cast<TessLevels *>(alloc.getHostPtr());

            *bufferTessLevels = tessLevelCases[tessLevelCaseNdx];
            flushAlloc(vk, device, alloc);
        }

        // Reset the command buffer and begin recording.
        beginCommandBuffer(vk, *cmdBuffer);

        // Change color attachment image layout
        {
            // State is slightly different on the first iteration.
            const VkImageLayout currentLayout =
                (tessLevelCaseNdx == 0 ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            const VkAccessFlags srcFlags =
                (tessLevelCaseNdx == 0 ? (VkAccessFlags)0 : (VkAccessFlags)VK_ACCESS_TRANSFER_READ_BIT);

            const VkImageMemoryBarrier colorAttachmentLayoutBarrier = makeImageMemoryBarrier(
                srcFlags, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, currentLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                *colorAttachmentImage, colorImageSubresourceRange);

            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u,
                                  &colorAttachmentLayoutBarrier);
        }

        // Begin render pass
        {
            const VkRect2D renderArea = makeRect2D(renderSize);
            const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);

            beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea, clearColor);
        }

        vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
        vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
                                 &descriptorSet.get(), 0u, nullptr);
        {
            const VkDeviceSize vertexBufferOffset = 0ull;
            vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
        }

        // Process enough vertices to make a patch.
        if (caseDef.drawType == DRAWTYPE_DRAW)
            vk.cmdDraw(*cmdBuffer, inPatchSize, 1u, 0u, 0u);
        else                                                                  // DRAWTYPE_DRAW_INDIRECT
            vk.cmdDrawIndirect(*cmdBuffer, indirectBuffer.get(), 0u, 1u, 0u); // Stride is ignored
        endRenderPass(vk, *cmdBuffer);

        // Copy render result to a host-visible buffer
        copyImageToBuffer(vk, *cmdBuffer, *colorAttachmentImage, *colorBuffer, renderSize);

        endCommandBuffer(vk, *cmdBuffer);
        submitCommandsAndWait(vk, device, queue, *cmdBuffer);

        {
            const Allocation &colorBufferAlloc = colorBuffer.getAllocation();

            invalidateAlloc(vk, device, colorBufferAlloc);

            // Verify case result
            const tcu::ConstPixelBufferAccess resultImageAccess(mapVkFormat(colorFormat), renderSize.x(),
                                                                renderSize.y(), 1, colorBufferAlloc.getHostPtr());

            // Load reference image
            const std::string referenceImagePath =
                caseDef.referenceImagePathPrefix + "_" + de::toString(tessLevelCaseNdx) + ".png";

            tcu::TextureLevel referenceImage;
            tcu::ImageIO::loadPNG(referenceImage, context.getTestContext().getArchive(), referenceImagePath.c_str());

            if (tcu::fuzzyCompare(context.getTestContext().getLog(), "ImageComparison", "Image Comparison",
                                  referenceImage.getAccess(), resultImageAccess, 0.002f, tcu::COMPARE_LOG_RESULT))
                ++numPassedCases;
        }
    } // tessLevelCaseNdx

    return (numPassedCases == tessLevelCases.size() ? tcu::TestStatus::pass("OK") : tcu::TestStatus::fail("Failure"));
}

inline const char *getTessLevelsSSBODeclaration(void)
{
    return "layout(set = 0, binding = 0, std430) readonly restrict buffer TessLevels {\n"
           "    float inner0;\n"
           "    float inner1;\n"
           "    float outer0;\n"
           "    float outer1;\n"
           "    float outer2;\n"
           "    float outer3;\n"
           "} sb_levels;\n";
}

//! Add vertex, fragment, and tessellation control shaders.
void initCommonPrograms(vk::SourceCollections &programCollection, const CaseDefinition caseDef)
{
    DE_ASSERT(!programCollection.glslSources.contains("vert"));
    DE_ASSERT(!programCollection.glslSources.contains("tesc"));
    DE_ASSERT(!programCollection.glslSources.contains("frag"));

    // Vertex shader
    {
        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
            << "\n"
            << "layout(location = 0) in  highp vec2 in_v_position;\n"
            << "layout(location = 0) out highp vec2 in_tc_position;\n"
            << "\n"
            << "void main (void)\n"
            << "{\n"
            << "    in_tc_position = in_v_position;\n"
            << "}\n";

        programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
    }

    // Tessellation control shader
    {
        const int numVertices = (caseDef.primitiveType == TESSPRIMITIVETYPE_TRIANGLES ? 3 : 4);

        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
            << "#extension GL_EXT_tessellation_shader : require\n"
            << "\n"
            << "layout(vertices = " << numVertices << ") out;\n"
            << "\n"
            << getTessLevelsSSBODeclaration() << "\n"
            << "layout(location = 0) in  highp vec2 in_tc_position[];\n"
            << "layout(location = 0) out highp vec2 in_te_position[];\n"
            << "\n"
            << "void main (void)\n"
            << "{\n"
            << "    in_te_position[gl_InvocationID] = in_tc_position[gl_InvocationID];\n"
            << "\n"
            << "    gl_TessLevelInner[0] = sb_levels.inner0;\n"
            << "    gl_TessLevelInner[1] = sb_levels.inner1;\n"
            << "\n"
            << "    gl_TessLevelOuter[0] = sb_levels.outer0;\n"
            << "    gl_TessLevelOuter[1] = sb_levels.outer1;\n"
            << "    gl_TessLevelOuter[2] = sb_levels.outer2;\n"
            << "    gl_TessLevelOuter[3] = sb_levels.outer3;\n"
            << "}\n";

        programCollection.glslSources.add("tesc") << glu::TessellationControlSource(src.str());
    }

    // Fragment shader
    {
        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
            << "\n"
            << "layout(location = 0) in  highp   vec4 in_f_color;\n"
            << "layout(location = 0) out mediump vec4 o_color;\n"
            << "\n"
            << "void main (void)\n"
            << "{\n"
            << "    o_color = in_f_color;\n"
            << "}\n";

        programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
    }
}

void initProgramsFillCoverCase(vk::SourceCollections &programCollection, const CaseDefinition caseDef)
{
    DE_ASSERT(caseDef.primitiveType == TESSPRIMITIVETYPE_TRIANGLES || caseDef.primitiveType == TESSPRIMITIVETYPE_QUADS);

    initCommonPrograms(programCollection, caseDef);

    // Tessellation evaluation shader
    {
        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
            << "#extension GL_EXT_tessellation_shader : require\n"
            << "\n"
            << "layout(" << getTessPrimitiveTypeShaderName(caseDef.primitiveType) << ", "
            << getSpacingModeShaderName(caseDef.spacingMode) << ") in;\n"
            << "\n"
            << "layout(location = 0) in  highp vec2 in_te_position[];\n"
            << "layout(location = 0) out highp vec4 in_f_color;\n"
            << "\n"
            << "void main (void)\n"
            << "{\n"
            << (caseDef.primitiveType == TESSPRIMITIVETYPE_TRIANGLES ?
                    "    highp float d = 3.0 * min(gl_TessCoord.x, min(gl_TessCoord.y, gl_TessCoord.z));\n"
                    "    highp vec2 corner0 = in_te_position[0];\n"
                    "    highp vec2 corner1 = in_te_position[1];\n"
                    "    highp vec2 corner2 = in_te_position[2];\n"
                    "    highp vec2 pos =  corner0*gl_TessCoord.x + corner1*gl_TessCoord.y + corner2*gl_TessCoord.z;\n"
                    "    highp vec2 fromCenter = pos - (corner0 + corner1 + corner2) / 3.0;\n"
                    "    highp float f = (1.0 - length(fromCenter)) * (1.5 - d);\n"
                    "    pos += 0.75 * f * fromCenter / (length(fromCenter) + 0.3);\n"
                    "    gl_Position = vec4(pos, 0.0, 1.0);\n" :
                caseDef.primitiveType == TESSPRIMITIVETYPE_QUADS ?
                    "    highp vec2 corner0 = in_te_position[0];\n"
                    "    highp vec2 corner1 = in_te_position[1];\n"
                    "    highp vec2 corner2 = in_te_position[2];\n"
                    "    highp vec2 corner3 = in_te_position[3];\n"
                    "    highp vec2 pos = (1.0-gl_TessCoord.x)*(1.0-gl_TessCoord.y)*corner0\n"
                    "                   + (    gl_TessCoord.x)*(1.0-gl_TessCoord.y)*corner1\n"
                    "                   + (1.0-gl_TessCoord.x)*(    gl_TessCoord.y)*corner2\n"
                    "                   + (    gl_TessCoord.x)*(    gl_TessCoord.y)*corner3;\n"
                    "    highp float d = 2.0 * min(abs(gl_TessCoord.x-0.5), abs(gl_TessCoord.y-0.5));\n"
                    "    highp vec2 fromCenter = pos - (corner0 + corner1 + corner2 + corner3) / 4.0;\n"
                    "    highp float f = (1.0 - length(fromCenter)) * sqrt(1.7 - d);\n"
                    "    pos += 0.75 * f * fromCenter / (length(fromCenter) + 0.3);\n"
                    "    gl_Position = vec4(pos, 0.0, 1.0);\n" :
                    "")
            << "    in_f_color = vec4(1.0);\n"
            << "}\n";

        programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(src.str());
    }
}

void initProgramsFillNonOverlapCase(vk::SourceCollections &programCollection, const CaseDefinition caseDef)
{
    DE_ASSERT(caseDef.primitiveType == TESSPRIMITIVETYPE_TRIANGLES || caseDef.primitiveType == TESSPRIMITIVETYPE_QUADS);

    initCommonPrograms(programCollection, caseDef);

    // Tessellation evaluation shader
    {
        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
            << "#extension GL_EXT_tessellation_shader : require\n"
            << "\n"
            << "layout(" << getTessPrimitiveTypeShaderName(caseDef.primitiveType) << ", "
            << getSpacingModeShaderName(caseDef.spacingMode) << ") in;\n"
            << "\n"
            << getTessLevelsSSBODeclaration() << "\n"
            << "layout(location = 0) in  highp vec2 in_te_position[];\n"
            << "layout(location = 0) out highp vec4 in_f_color;\n"
            << "\n"
            << "void main (void)\n"
            << "{\n"
            << (caseDef.primitiveType == TESSPRIMITIVETYPE_TRIANGLES ?
                    "    highp vec2 corner0 = in_te_position[0];\n"
                    "    highp vec2 corner1 = in_te_position[1];\n"
                    "    highp vec2 corner2 = in_te_position[2];\n"
                    "    highp vec2 pos =  corner0*gl_TessCoord.x + corner1*gl_TessCoord.y + corner2*gl_TessCoord.z;\n"
                    "    gl_Position = vec4(pos, 0.0, 1.0);\n"
                    "    highp int numConcentricTriangles = int(round(sb_levels.inner0)) / 2 + 1;\n"
                    "    highp float d = 3.0 * min(gl_TessCoord.x, min(gl_TessCoord.y, gl_TessCoord.z));\n"
                    "    highp int phase = int(d*float(numConcentricTriangles)) % 3;\n"
                    "    in_f_color = phase == 0 ? vec4(1.0, 0.0, 0.0, 1.0)\n"
                    "               : phase == 1 ? vec4(0.0, 1.0, 0.0, 1.0)\n"
                    "               :              vec4(0.0, 0.0, 1.0, 1.0);\n" :
                caseDef.primitiveType == TESSPRIMITIVETYPE_QUADS ?
                    "    highp vec2 corner0 = in_te_position[0];\n"
                    "    highp vec2 corner1 = in_te_position[1];\n"
                    "    highp vec2 corner2 = in_te_position[2];\n"
                    "    highp vec2 corner3 = in_te_position[3];\n"
                    "    highp vec2 pos = (1.0-gl_TessCoord.x)*(1.0-gl_TessCoord.y)*corner0\n"
                    "                   + (    gl_TessCoord.x)*(1.0-gl_TessCoord.y)*corner1\n"
                    "                   + (1.0-gl_TessCoord.x)*(    gl_TessCoord.y)*corner2\n"
                    "                   + (    gl_TessCoord.x)*(    gl_TessCoord.y)*corner3;\n"
                    "    gl_Position = vec4(pos, 0.0, 1.0);\n"
                    "    highp int phaseX = int(round((0.5 - abs(gl_TessCoord.x-0.5)) * sb_levels.inner0));\n"
                    "    highp int phaseY = int(round((0.5 - abs(gl_TessCoord.y-0.5)) * sb_levels.inner1));\n"
                    "    highp int phase = min(phaseX, phaseY) % 3;\n"
                    "    in_f_color = phase == 0 ? vec4(1.0, 0.0, 0.0, 1.0)\n"
                    "               : phase == 1 ? vec4(0.0, 1.0, 0.0, 1.0)\n"
                    "               :              vec4(0.0, 0.0, 1.0, 1.0);\n" :
                    "")
            << "}\n";

        programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(src.str());
    }
}

void initProgramsIsolinesCase(vk::SourceCollections &programCollection, const CaseDefinition caseDef)
{
    DE_ASSERT(caseDef.primitiveType == TESSPRIMITIVETYPE_ISOLINES);

    initCommonPrograms(programCollection, caseDef);

    // Tessellation evaluation shader
    {
        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
            << "#extension GL_EXT_tessellation_shader : require\n"
            << "\n"
            << "layout(" << getTessPrimitiveTypeShaderName(caseDef.primitiveType) << ", "
            << getSpacingModeShaderName(caseDef.spacingMode) << ") in;\n"
            << "\n"
            << getTessLevelsSSBODeclaration() << "\n"
            << "layout(location = 0) in  highp vec2 in_te_position[];\n"
            << "layout(location = 0) out highp vec4 in_f_color;\n"
            << "\n"
            << "void main (void)\n"
            << "{\n"
            << "    highp vec2 corner0 = in_te_position[0];\n"
            << "    highp vec2 corner1 = in_te_position[1];\n"
            << "    highp vec2 corner2 = in_te_position[2];\n"
            << "    highp vec2 corner3 = in_te_position[3];\n"
            << "    highp vec2 pos = (1.0-gl_TessCoord.x)*(1.0-gl_TessCoord.y)*corner0\n"
            << "                   + (    gl_TessCoord.x)*(1.0-gl_TessCoord.y)*corner1\n"
            << "                   + (1.0-gl_TessCoord.x)*(    gl_TessCoord.y)*corner2\n"
            << "                   + (    gl_TessCoord.x)*(    gl_TessCoord.y)*corner3;\n"
            << "    pos.y += 0.15*sin(gl_TessCoord.x*10.0);\n"
            << "    gl_Position = vec4(pos, 0.0, 1.0);\n"
            << "    highp int phaseX = int(round(gl_TessCoord.x*sb_levels.outer1));\n"
            << "    highp int phaseY = int(round(gl_TessCoord.y*sb_levels.outer0));\n"
            << "    highp int phase = (phaseX + phaseY) % 3;\n"
            << "    in_f_color = phase == 0 ? vec4(1.0, 0.0, 0.0, 1.0)\n"
            << "               : phase == 1 ? vec4(0.0, 1.0, 0.0, 1.0)\n"
            << "               :              vec4(0.0, 0.0, 1.0, 1.0);\n"
            << "}\n";

        programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(src.str());
    }
}

inline std::string getReferenceImagePathPrefix(const std::string &caseName)
{
    return "vulkan/data/tessellation/" + caseName + "_ref";
}

struct TessStateSwitchParams
{
    const PipelineConstructionType pipelineConstructionType;
    const std::pair<TessPrimitiveType, TessPrimitiveType> patchTypes;
    const std::pair<SpacingMode, SpacingMode> spacing;
    const std::pair<VkTessellationDomainOrigin, VkTessellationDomainOrigin> domainOrigin;
    const std::pair<uint32_t, uint32_t> outputVertices;
    const bool geometryShader;

    bool nonDefaultDomainOrigin(void) const
    {
        return (domainOrigin.first != VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT ||
                domainOrigin.second != VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT);
    }
};

class TessStateSwitchInstance : public vkt::TestInstance
{
public:
    TessStateSwitchInstance(Context &context, const TessStateSwitchParams &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }

    virtual ~TessStateSwitchInstance(void)
    {
    }

    tcu::TestStatus iterate(void);

protected:
    const TessStateSwitchParams m_params;
};

class TessStateSwitchCase : public vkt::TestCase
{
public:
    TessStateSwitchCase(tcu::TestContext &testCtx, const std::string &name, const TessStateSwitchParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual ~TessStateSwitchCase(void)
    {
    }

    void checkSupport(Context &context) const;
    void initPrograms(vk::SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &context) const
    {
        return new TessStateSwitchInstance(context, m_params);
    }

protected:
    const TessStateSwitchParams m_params;
};

void TessStateSwitchCase::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

    if (m_params.geometryShader)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

    if (m_params.nonDefaultDomainOrigin())
        context.requireDeviceFunctionality("VK_KHR_maintenance2");

    const auto ctx = context.getContextCommonData();
    checkPipelineConstructionRequirements(ctx.vki, ctx.physicalDevice, m_params.pipelineConstructionType);
}

void TessStateSwitchCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "layout (push_constant, std430) uniform PushConstantBlock { vec2 offset; } pc;\n"
         << "out gl_PerVertex\n"
         << "{\n"
         << "  vec4 gl_Position;\n"
         << "};\n"
         << "void main() {\n"
         << "    gl_Position = inPos + vec4(pc.offset, 0.0, 0.0);\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    if (m_params.geometryShader)
    {
        std::ostringstream geom;
        geom << "#version 460\n"
             << "layout (triangles) in;\n"
             << "layout (triangle_strip, max_vertices=3) out;\n"
             << "in gl_PerVertex\n"
             << "{\n"
             << "    vec4 gl_Position;\n"
             << "} gl_in[3];\n"
             << "out gl_PerVertex\n"
             << "{\n"
             << "    vec4 gl_Position;\n"
             << "};\n"
             << "void main() {\n"
             << "    gl_Position    = gl_in[0].gl_Position; EmitVertex();\n"
             << "    gl_Position    = gl_in[1].gl_Position; EmitVertex();\n"
             << "    gl_Position    = gl_in[2].gl_Position; EmitVertex();\n"
             << "    gl_PrimitiveID = gl_PrimitiveIDIn;     EndPrimitive();\n"
             << "}\n";
        programCollection.glslSources.add("geom") << glu::GeometrySource(geom.str());
    }

    const auto even       = (m_params.spacing.second == SPACINGMODE_FRACTIONAL_EVEN);
    const auto extraLevel = (even ? "1.0" : "0.0");

    for (uint32_t i = 0u; i < 2u; ++i)
    {
        const auto outVertices = ((i == 0) ? m_params.outputVertices.first : m_params.outputVertices.second);

        std::ostringstream tesc;
        tesc << "#version 460\n"
             << "layout (vertices=" << outVertices << ") out;\n"
             << "in gl_PerVertex\n"
             << "{\n"
             << "  vec4 gl_Position;\n"
             << "} gl_in[gl_MaxPatchVertices];\n"
             << "out gl_PerVertex\n"
             << "{\n"
             << "  vec4 gl_Position;\n"
             << "} gl_out[];\n"
             << "void main() {\n"
             << "    const float extraLevel = " << extraLevel << ";\n"
             << "    gl_TessLevelInner[0] = 10.0 + extraLevel;\n"
             << "    gl_TessLevelInner[1] = 10.0 + extraLevel;\n"
             << "    gl_TessLevelOuter[0] = 50.0 + extraLevel;\n"
             << "    gl_TessLevelOuter[1] = 40.0 + extraLevel;\n"
             << "    gl_TessLevelOuter[2] = 30.0 + extraLevel;\n"
             << "    gl_TessLevelOuter[3] = 20.0 + extraLevel;\n"
             << "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
             << "}\n";
        programCollection.glslSources.add("tesc" + std::to_string(i)) << glu::TessellationControlSource(tesc.str());
    }

    DE_ASSERT(m_params.patchTypes.first != TESSPRIMITIVETYPE_ISOLINES);
    DE_ASSERT(m_params.patchTypes.second != TESSPRIMITIVETYPE_ISOLINES);

    for (uint32_t i = 0u; i < 2u; ++i)
    {
        const auto &primType   = ((i == 0u) ? m_params.patchTypes.first : m_params.patchTypes.second);
        const auto &spacing    = ((i == 0u) ? m_params.spacing.first : m_params.spacing.second);
        const auto outVertices = ((i == 0) ? m_params.outputVertices.first : m_params.outputVertices.second);

        DE_ASSERT(outVertices == 3u || outVertices == 4u);

        std::ostringstream tese;
        tese << "#version 460\n"
             << "layout (" << getTessPrimitiveTypeShaderName(primType) << ", " << getSpacingModeShaderName(spacing)
             << ", ccw) in;\n"
             << "in gl_PerVertex\n"
             << "{\n"
             << "  vec4 gl_Position;\n"
             << "} gl_in[gl_MaxPatchVertices];\n"
             << "out gl_PerVertex\n"
             << "{\n"
             << "  vec4 gl_Position;\n"
             << "};\n"
             << "\n"
             << "// This assumes 2D, calculates barycentrics for point p inside triangle (a, b, c)\n"
             << "vec3 calcBaryCoords(vec2 p, vec2 a, vec2 b, vec2 c)\n"
             << "{\n"
             << "    const vec2 v0 = b - a;\n"
             << "    const vec2 v1 = c - a;\n"
             << "    const vec2 v2 = p - a;\n"
             << "\n"
             << "    const float den = v0.x * v1.y - v1.x * v0.y;\n"
             << "    const float v   = (v2.x * v1.y - v1.x * v2.y) / den;\n"
             << "    const float w   = (v0.x * v2.y - v2.x * v0.y) / den;\n"
             << "    const float u   = 1.0 - v - w;\n"
             << "\n"
             << "    return vec3(u, v, w);\n"
             << "}\n"
             << "\n"
             << "void main() {\n"
             << "    const vec4 p0 = gl_in[0].gl_Position;\n"
             << "    const vec4 p1 = gl_in[1].gl_Position;\n"
             << "    const vec4 p2 = gl_in[2].gl_Position;\n"
             << ((outVertices == 4u)
                     // Copy the 4th vertex directly.
                     ?
                     "    const vec4 p3 = gl_in[3].gl_Position;\n"
                     // Make up a 4th vertex on the fly.
                     :
                     "    const float delta  = 0.75;\n"
                     "    const float width  = p2.x - p0.x;\n"
                     "    const float height = p1.y - p0.y;\n"
                     "    const vec4 p3 = vec4(p0.x + width * delta, p0.y + height * delta, p0.z, p0.w);\n")
             << ((primType == TESSPRIMITIVETYPE_QUADS)
                     // For quads.
                     ?
                     "    const float u = gl_TessCoord.x;\n"
                     "    const float v = gl_TessCoord.y;\n"
                     "    gl_Position = (1 - u) * (1 - v) * p0 + (1 - u) * v * p1 + u * (1 - v) * p2 + u * v * p3;\n"
                     // For triangles.
                     :
                     "    // We have a patch with 4 corners (v0,v1,v2,v3), but triangle-based tessellation.\n"
                     "    // Lets suppose the triangle covers half the patch (triangle v0,v2,v1).\n"
                     "    // Expand the triangle by virtually grabbing it from the midpoint between v1 and v2 (which "
                     "should fall in the middle of the patch) and stretching that point to the fourth corner (v3).\n"
                     "    const vec4 origpoint = (gl_TessCoord.x * p0) +\n"
                     "                           (gl_TessCoord.y * p2) +\n"
                     "                           (gl_TessCoord.z * p1);\n"
                     "    const vec4 midpoint = 0.5 * p1 + 0.5 * p2;\n"
                     "\n"
                     "    // Find out if it falls on left or right side of the triangle.\n"
                     "    vec4 halfTriangle[3];\n"
                     "    vec4 stretchedHalf[3];\n"
                     "\n"
                     "    if (gl_TessCoord.z >= gl_TessCoord.y)\n"
                     "    {\n"
                     "        halfTriangle[0] = p0;\n"
                     "        halfTriangle[1] = midpoint;\n"
                     "        halfTriangle[2] = p1;\n"
                     "\n"
                     "        stretchedHalf[0] = p0;\n"
                     "        stretchedHalf[1] = p3;\n"
                     "        stretchedHalf[2] = p1;\n"
                     "    }\n"
                     "    else\n"
                     "    {\n"
                     "        halfTriangle[0] = p0;\n"
                     "        halfTriangle[1] = p2;\n"
                     "        halfTriangle[2] = midpoint;\n"
                     "\n"
                     "        stretchedHalf[0] = p0;\n"
                     "        stretchedHalf[1] = p2;\n"
                     "        stretchedHalf[2] = p3;\n"
                     "    }\n"
                     "\n"
                     "    // Calculate the barycentric coordinates for the left or right sides.\n"
                     "    vec3 sideBaryCoord = calcBaryCoords(origpoint.xy, halfTriangle[0].xy, halfTriangle[1].xy, "
                     "halfTriangle[2].xy);\n"
                     "\n"
                     "    // Move the point by stretching the half triangle and dragging the midpoint vertex to v3.\n"
                     "    gl_Position = sideBaryCoord.x * stretchedHalf[0] + sideBaryCoord.y * stretchedHalf[1] + "
                     "sideBaryCoord.z * stretchedHalf[2];\n")
             << "}\n";
        programCollection.glslSources.add("tese" + std::to_string(i)) << glu::TessellationEvaluationSource(tese.str());
    }

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main() {\n"
         << "    outColor = vec4(0.0, 1.0, 1.0, 1.0);\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

tcu::TestStatus TessStateSwitchInstance::iterate(void)
{
    const auto &ctx = m_context.getContextCommonData();
    const tcu::IVec3 fbExtent(128, 128, 1);
    const auto vkExtent    = makeExtent3D(fbExtent);
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto tcuFormat   = mapVkFormat(colorFormat);
    const auto colorUsage  = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto imageType   = VK_IMAGE_TYPE_2D;
    const auto colorSRR    = makeDefaultImageSubresourceRange();
    const auto useESO      = isConstructionTypeShaderObject(m_params.pipelineConstructionType);

    ImageWithBuffer referenceBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, colorFormat, colorUsage, imageType,
                                    colorSRR);
    ImageWithBuffer resultBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, colorFormat, colorUsage, imageType,
                                 colorSRR);

    // Vertex buffer containing a single full-screen patch.
    const std::vector<tcu::Vec4> vertices{
        tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
        tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
    };
    const auto vertexCount        = de::sizeU32(vertices);
    const auto patchControlPoints = vertexCount;

    const auto vertexBufferSize = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vertexBufferInfo = makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferInfo, MemoryRequirement::HostVisible);
    auto &vertexBufferAlloc       = vertexBuffer.getAllocation();
    void *vertexBufferData        = vertexBufferAlloc.getHostPtr();
    const auto vertexBufferOffset = static_cast<VkDeviceSize>(0);

    deMemcpy(vertexBufferData, de::dataOrNull(vertices), de::dataSize(vertices));
    flushAlloc(ctx.vkd, ctx.device, vertexBufferAlloc);

    const auto pcSize   = static_cast<uint32_t>(sizeof(tcu::Vec2));
    const auto pcStages = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT);
    const auto pcRange  = makePushConstantRange(pcStages, 0u, pcSize);

    const PipelineLayoutWrapper pipelineLayout(m_params.pipelineConstructionType, ctx.vkd, ctx.device, VK_NULL_HANDLE,
                                               &pcRange);

    RenderPassWrapper renderPass0(m_params.pipelineConstructionType, ctx.vkd, ctx.device, colorFormat);
    RenderPassWrapper renderPass1 = renderPass0.clone(); // Preserves render pass handle.
    DE_ASSERT(renderPass0.get() == renderPass1.get());

    // Framebuffers.
    renderPass0.createFramebuffer(ctx.vkd, ctx.device, referenceBuffer.getImage(), referenceBuffer.getImageView(),
                                  vkExtent.width, vkExtent.height);
    renderPass1.createFramebuffer(ctx.vkd, ctx.device, resultBuffer.getImage(), resultBuffer.getImageView(),
                                  vkExtent.width, vkExtent.height);

    // Viewport and scissor.
    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    // Shaders.
    const auto &binaries = m_context.getBinaryCollection();
    const ShaderWrapper vertModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const ShaderWrapper tescModule0(ctx.vkd, ctx.device, binaries.get("tesc0"));
    const ShaderWrapper tescModule1(ctx.vkd, ctx.device, binaries.get("tesc1"));
    const ShaderWrapper teseModule0(ctx.vkd, ctx.device, binaries.get("tese0"));
    const ShaderWrapper teseModule1(ctx.vkd, ctx.device, binaries.get("tese1"));
    const ShaderWrapper geomModule =
        (m_params.geometryShader ? ShaderWrapper(ctx.vkd, ctx.device, binaries.get("geom")) : ShaderWrapper());
    const ShaderWrapper fragModule(ctx.vkd, ctx.device, binaries.get("frag"));

    const auto primitiveTopology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;

    // In the rasterization parameters, use wireframe mode to see each triangle if possible.
    // This makes the test harder to pass by mistake.
    // We also cull back faces, which will help test domain origin.
    // The front face changes with the domain origin.
    const auto frontFace =
        ((m_params.domainOrigin.second == VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT) ?
             VK_FRONT_FACE_COUNTER_CLOCKWISE // With the default value it's as specified in the shader.
             :
             VK_FRONT_FACE_CLOCKWISE); // Otherwise the winding order changes.
    const auto polygonMode =
        ((m_context.getDeviceFeatures().fillModeNonSolid) ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL);
    const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                    // const void* pNext;
        0u,                                                         // VkPipelineRasterizationStateCreateFlags flags;
        VK_FALSE,                                                   // VkBool32 depthClampEnable;
        VK_FALSE,                                                   // VkBool32 rasterizerDiscardEnable;
        polygonMode,                                                // VkPolygonMode polygonMode;
        VK_CULL_MODE_BACK_BIT,                                      // VkCullModeFlags cullMode;
        frontFace,                                                  // VkFrontFace frontFace;
        VK_FALSE,                                                   // VkBool32 depthBiasEnable;
        0.0f,                                                       // float depthBiasConstantFactor;
        0.0f,                                                       // float depthBiasClamp;
        0.0f,                                                       // float depthBiasSlopeFactor;
        1.0f,                                                       // float lineWidth;
    };

    // Create two pipelines varying the tessellation evaluation module and domain origin.
    GraphicsPipelineWrapper pipeline0(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device, m_context.getDeviceExtensions(),
                                      m_params.pipelineConstructionType);
    pipeline0.setDefaultVertexInputState(true)
        .setDefaultTopology(primitiveTopology)
        .setDefaultPatchControlPoints(patchControlPoints)
        .setDefaultTessellationDomainOrigin(m_params.domainOrigin.first, m_params.nonDefaultDomainOrigin())
        .setDefaultDepthStencilState()
        .setDefaultMultisampleState()
        .setDefaultColorBlendState()
        .setupVertexInputState()
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass0.get(), 0u, vertModule,
                                          &rasterizationStateCreateInfo, tescModule0, teseModule0, geomModule)
        .setupFragmentShaderState(pipelineLayout, renderPass0.get(), 0u, fragModule)
        .setupFragmentOutputState(renderPass0.get(), 0u)
        .buildPipeline();

    GraphicsPipelineWrapper pipeline1(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device, m_context.getDeviceExtensions(),
                                      m_params.pipelineConstructionType);
    pipeline1.setDefaultVertexInputState(true)
        .setDefaultTopology(primitiveTopology)
        .setDefaultPatchControlPoints(patchControlPoints)
        .setDefaultTessellationDomainOrigin(m_params.domainOrigin.second, m_params.nonDefaultDomainOrigin())
        .setDefaultDepthStencilState()
        .setDefaultMultisampleState()
        .setDefaultColorBlendState()
        .setupVertexInputState()
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass1.get(), 0u, vertModule,
                                          &rasterizationStateCreateInfo, tescModule1, teseModule1, geomModule)
        .setupFragmentShaderState(pipelineLayout, renderPass1.get(), 0u, fragModule)
        .setupFragmentOutputState(renderPass1.get(), 0u)
        .buildPipeline();

    // pipeline2 is like pipeline1 but always monolithic, and it's the one used to create the reference image.
    PipelineRenderingCreateInfoWrapper renderingCreateInfoPtr;
#ifndef CTS_USES_VULKANSC
    VkPipelineRenderingCreateInfo renderingCreateInfo = initVulkanStructure();
    if (useESO)
    {
        // Note the render pass is dynamic in this case, so we need to create the pipeline with dynamic rendering.
        renderingCreateInfo.colorAttachmentCount    = 1u;
        renderingCreateInfo.pColorAttachmentFormats = &colorFormat;
        renderingCreateInfoPtr                      = &renderingCreateInfo;
    }
#endif

    GraphicsPipelineWrapper pipeline2(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device, m_context.getDeviceExtensions(),
                                      PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC);
    pipeline2.setDefaultVertexInputState(true)
        .setDefaultTopology(primitiveTopology)
        .setDefaultPatchControlPoints(patchControlPoints)
        .setDefaultTessellationDomainOrigin(m_params.domainOrigin.second, m_params.nonDefaultDomainOrigin())
        .setDefaultDepthStencilState()
        .setDefaultMultisampleState()
        .setDefaultColorBlendState()
        .setupVertexInputState()
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass1.get(), 0u, vertModule,
                                          &rasterizationStateCreateInfo, tescModule1, teseModule1, geomModule, nullptr,
                                          nullptr, renderingCreateInfoPtr)
        .setupFragmentShaderState(pipelineLayout, renderPass1.get(), 0u, fragModule)
        .setupFragmentOutputState(renderPass1.get(), 0u)
        .buildPipeline();

    const auto cmdPool      = makeCommandPool(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBufferRef = allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBufferRes = allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    const tcu::Vec2 noOffset(0.0f, 0.0f);
    const tcu::Vec2 offscreenOffset(50.0f, 50.0f);
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Reference image.
    beginCommandBuffer(ctx.vkd, *cmdBufferRef);
    renderPass0.begin(ctx.vkd, *cmdBufferRef, scissors.at(0u), clearColor);
    ctx.vkd.cmdBindVertexBuffers(*cmdBufferRef, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
    pipeline2.bind(*cmdBufferRef);
    ctx.vkd.cmdPushConstants(*cmdBufferRef, *pipelineLayout, pcStages, 0u, pcSize, &noOffset);
    ctx.vkd.cmdDraw(*cmdBufferRef, vertexCount, 1u, 0u, 0u);
    renderPass0.end(ctx.vkd, *cmdBufferRef);
    copyImageToBuffer(ctx.vkd, *cmdBufferRef, referenceBuffer.getImage(), referenceBuffer.getBuffer(),
                      fbExtent.swizzle(0, 1), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1u, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    endCommandBuffer(ctx.vkd, *cmdBufferRef);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, *cmdBufferRef);

    // Result image.
    beginCommandBuffer(ctx.vkd, *cmdBufferRes);
    renderPass1.begin(ctx.vkd, *cmdBufferRes, scissors.at(0u), clearColor);
    ctx.vkd.cmdBindVertexBuffers(*cmdBufferRes, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
    // Draw offscreen first to force tessellation state emission.
    pipeline0.bind(*cmdBufferRes);
    ctx.vkd.cmdPushConstants(*cmdBufferRes, *pipelineLayout, pcStages, 0u, pcSize, &offscreenOffset);
    ctx.vkd.cmdDraw(*cmdBufferRes, vertexCount, 1u, 0u, 0u);
    // Draw onscreen second, changing some tessellation state.
    if (useESO && (m_params.domainOrigin.first == m_params.domainOrigin.second))
    {
#ifndef CTS_USES_VULKANSC
        // When using shader objects and the domain origin does not change, we can simply bind the tessellation
        // shaders without rebinding all state. This makes for a more interesting case.
        const std::vector<VkShaderStageFlagBits> stages{
            VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
            VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
        };
        std::vector<VkShaderEXT> shaders;
        shaders.reserve(stages.size());
        for (const auto &stage : stages)
            shaders.push_back(pipeline1.getShader(stage));

        DE_ASSERT(shaders.size() == stages.size());
        ctx.vkd.cmdBindShadersEXT(*cmdBufferRes, de::sizeU32(stages), de::dataOrNull(stages), de::dataOrNull(shaders));
#endif
    }
    else
    {
        // Domain origin is not part of the shader state, so we need a full state rebind to change it.
        pipeline1.bind(*cmdBufferRes);
    }
    ctx.vkd.cmdPushConstants(*cmdBufferRes, *pipelineLayout, pcStages, 0u, pcSize, &noOffset);
    ctx.vkd.cmdDraw(*cmdBufferRes, vertexCount, 1u, 0u, 0u);
    renderPass1.end(ctx.vkd, *cmdBufferRes);
    copyImageToBuffer(ctx.vkd, *cmdBufferRes, resultBuffer.getImage(), resultBuffer.getBuffer(), fbExtent.swizzle(0, 1),
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1u,
                      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    endCommandBuffer(ctx.vkd, *cmdBufferRes);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, *cmdBufferRes);

    invalidateAlloc(ctx.vkd, ctx.device, referenceBuffer.getBufferAllocation());
    invalidateAlloc(ctx.vkd, ctx.device, resultBuffer.getBufferAllocation());

    tcu::ConstPixelBufferAccess referenceAccess(tcuFormat, fbExtent,
                                                referenceBuffer.getBufferAllocation().getHostPtr());
    tcu::ConstPixelBufferAccess resultAccess(tcuFormat, fbExtent, resultBuffer.getBufferAllocation().getHostPtr());

    auto &log             = m_context.getTestContext().getLog();
    const float threshold = 0.005f; // 1/255 < 0.005 < 2/255
    const tcu::Vec4 thresholdVec(threshold, threshold, threshold, 0.0f);

    if (!tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, thresholdVec,
                                    tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Color result does not match reference image -- check log for details");

    return tcu::TestStatus::pass("Pass");
}

std::string getDomainOriginName(VkTessellationDomainOrigin value)
{
    static const size_t prefixLen = strlen("VK_TESSELLATION_DOMAIN_ORIGIN_");
    std::string nameStr           = getTessellationDomainOriginName(value);

    return de::toLower(nameStr.substr(prefixLen));
}

enum TessInstancedType
{
    TESSINSTANCEDTYPE_NO_PATCHES = 0,
    TESSINSTANCEDTYPE_INSTANCED,

    TESSINSTANCEDTYPE_LAST,
};

struct TessInstancedDrawTestParams
{
    TessInstancedType testType;
    TessPrimitiveType primitiveType;
};

static inline const char *getInstancedDrawTestName(const TessInstancedType type)
{
    static std::string primitiveName[] = {
        "no_patches", // TESSINSTANCEDTYPE_NO_PATCHES
        "instances",  // TESSINSTANCEDTYPE_INSTANCED
    };

    if (type >= TESSINSTANCEDTYPE_LAST)
    {
        DE_FATAL("Unexpected test type.");
        return nullptr;
    }

    return primitiveName[type].c_str();
}

class TessInstancedDrawTestInstance : public vkt::TestInstance
{
public:
    TessInstancedDrawTestInstance(Context &context, const TessInstancedDrawTestParams &testParams)
        : vkt::TestInstance(context)
        , m_params(testParams)
    {
    }

    ~TessInstancedDrawTestInstance()
    {
    }

    tcu::TestStatus iterate(void);

protected:
    Move<VkBuffer> createBufferAndBindMemory(uint32_t bufferSize, VkBufferUsageFlags usageFlags,
                                             AllocationMp *outMemory);
    Move<VkImage> createImageAndBindMemory(tcu::IVec3 imgSize, VkFormat format, VkImageUsageFlags usageFlags,
                                           AllocationMp *outMemory);
    Move<VkImageView> createImageView(VkFormat format, VkImage image);
    Move<VkPipelineLayout> createPipelineLayout();
    Move<VkPipeline> createGraphicsPipeline(uint32_t patchCnt, VkPipelineLayout layout, VkRenderPass renderpass);

    std::vector<tcu::Vec4> genPerVertexVertexData();
    std::vector<tcu::Vec4> genPerInstanceVertexData();
    std::vector<uint16_t> genIndexData();

protected:
    const TessInstancedDrawTestParams m_params;
};

class InstancedVertexShader : public rr::VertexShader
{
public:
    InstancedVertexShader(void) : rr::VertexShader(2, 0)
    {
        m_inputs[0].type = rr::GENERICVECTYPE_FLOAT;
        m_inputs[1].type = rr::GENERICVECTYPE_FLOAT;
    }

    virtual ~InstancedVertexShader()
    {
    }

    void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets, const int numPackets) const
    {
        for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
        {
            const tcu::Vec4 position =
                rr::readVertexAttribFloat(inputs[0], packets[packetNdx]->instanceNdx, packets[packetNdx]->vertexNdx);
            const tcu::Vec4 instancePosition =
                rr::readVertexAttribFloat(inputs[1], packets[packetNdx]->instanceNdx, packets[packetNdx]->vertexNdx);

            packets[packetNdx]->position = position + instancePosition;
        }
    }
};

class InstancedFragmentShader : public rr::FragmentShader
{
public:
    InstancedFragmentShader(void) : rr::FragmentShader(0, 1)
    {
        m_outputs[0].type = rr::GENERICVECTYPE_FLOAT;
    }

    virtual ~InstancedFragmentShader()
    {
    }

    void shadeFragments(rr::FragmentPacket *, const int numPackets, const rr::FragmentShadingContext &context) const
    {
        for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
        {
            for (int fragNdx = 0; fragNdx < rr::NUM_FRAGMENTS_PER_PACKET; ++fragNdx)
            {
                const tcu::Vec4 color(1.0f, 0.0f, 1.0f, 1.0f);
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, color);
            }
        }
    }
};

class TessInstancedDrawTestCase : public vkt::TestCase
{
public:
    TessInstancedDrawTestCase(tcu::TestContext &testCtx, const std::string &name,
                              const TessInstancedDrawTestParams &testParams)
        : vkt::TestCase(testCtx, name)
        , m_params(testParams)
    {
    }

    ~TessInstancedDrawTestCase()
    {
    }

    void checkSupport(Context &context) const;
    void initPrograms(vk::SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &context) const
    {
        return new TessInstancedDrawTestInstance(context, m_params);
    }

protected:
    const TessInstancedDrawTestParams m_params;
};

void TessInstancedDrawTestCase::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);
}

void TessInstancedDrawTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "\n"
         << "layout (location = 0) in vec4 inPos;\n"
         << "layout (location = 1) in vec4 instancePos;\n"
         << "\n"
         << "void main()\n"
         << "{\n"
         << "    vec4 pos = inPos + instancePos;\n"
         << "\n"
         << "    gl_Position = pos;\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "\n"
         << "layout (location = 0) out vec4 fragColor;\n"
         << "\n"
         << "void main()\n"
         << "{\n"
         << "    fragColor = vec4(1.0f, 0.0f, 1.0f, 1.0f);\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());

    const int numVertices = m_params.primitiveType == TESSPRIMITIVETYPE_TRIANGLES ? 3 : 4;

    std::ostringstream tessCntrl;
    tessCntrl << "#version 460\n"
              << "\n"
              << "layout (vertices = " << numVertices << ") out;\n"
              << "\n"
              << "void main()\n"
              << "{\n"
              << "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
              << "    \n"
              << "    gl_TessLevelInner[0] = 1.0f;\n"
              << "    gl_TessLevelInner[1] = 1.0f;\n"
              << "    gl_TessLevelOuter[0] = 1.0f;\n"
              << "    gl_TessLevelOuter[1] = 1.0f;\n"
              << "    gl_TessLevelOuter[2] = 1.0f;\n"
              << "    gl_TessLevelOuter[3] = 1.0f;\n"
              << "    \n"
              << "}\n";
    programCollection.glslSources.add("tess_ctrl") << glu::TessellationControlSource(tessCntrl.str());

    std::ostringstream tessEvel;
    tessEvel << "#version 460\n"
             << "\n"
             << "layout ( " << getTessPrimitiveTypeShaderName(m_params.primitiveType) << " ) in;\n"
             << "\n"
             << "void main()\n"
             << "{\n"
             << "    const float u = gl_TessCoord.x;\n"
             << "    const float v = gl_TessCoord.y;\n";
    if (m_params.primitiveType == TESSPRIMITIVETYPE_TRIANGLES)
    {
        tessEvel << "    gl_Position = (1 - u) * (1 - v) * gl_in[0].gl_Position + (1 - u) * v * gl_in[1].gl_Position "
                 << "+ u * (1 - v) * gl_in[2].gl_Position;\n";
    }
    else // m_params.primitiveType == TESSPRIMITIVETYPE_QUADS
    {
        tessEvel << "    gl_Position = (1 - u) * (1 - v) * gl_in[0].gl_Position + u * (1 - v) * gl_in[1].gl_Position "
                 << "+ u * v * gl_in[2].gl_Position + (1 - u) * v * gl_in[3].gl_Position;\n";
    }
    tessEvel << "}\n";
    programCollection.glslSources.add("tess_eval") << glu::TessellationEvaluationSource(tessEvel.str());
}

tcu::TestStatus TessInstancedDrawTestInstance::iterate(void)
{
    const DeviceInterface &devInterface = m_context.getDeviceInterface();
    VkDevice dev                        = m_context.getDevice();
    VkQueue queue                       = m_context.getUniversalQueue();

    // Command buffer
    Move<VkCommandPool> cmdPool(makeCommandPool(devInterface, dev, m_context.getUniversalQueueFamilyIndex()));
    Move<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(devInterface, dev, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Per vertex vertex buffer
    const std::vector<tcu::Vec4> perVertexVertices = genPerVertexVertexData();
    const uint32_t perVertexVBSize = static_cast<uint32_t>(perVertexVertices.size() * sizeof(tcu::Vec4));

    AllocationMp perVertexVBMemory;
    Move<VkBuffer> perVertexVB(
        createBufferAndBindMemory(perVertexVBSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &perVertexVBMemory));

    {
        deMemcpy(perVertexVBMemory->getHostPtr(), perVertexVertices.data(), perVertexVBSize);
        flushAlloc(devInterface, dev, *perVertexVBMemory);
        // No barrier needed, flushed memory is automatically visible
    }

    const uint32_t patchCnt = static_cast<uint32_t>(perVertexVertices.size());

    // Per instance vertex buffer
    const std::vector<tcu::Vec4> perInstanceVertices = genPerInstanceVertexData();
    const uint32_t perInstanceVBSize = static_cast<uint32_t>(perInstanceVertices.size() * sizeof(tcu::Vec4));

    AllocationMp perInstanceVBMemory;
    Move<VkBuffer> perInstanceVB(
        createBufferAndBindMemory(perInstanceVBSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &perInstanceVBMemory));

    {
        deMemcpy(perInstanceVBMemory->getHostPtr(), perInstanceVertices.data(), perInstanceVBSize);
        flushAlloc(devInterface, dev, *perInstanceVBMemory);
        // No barrier needed, flushed memory is automatically visible
    }

    // Render target
    const tcu::IVec3 renderSize(256, 256, 1);
    const vk::VkFormat rtFormat = VK_FORMAT_R8G8B8A8_UNORM;

    AllocationMp rtMemory;
    Move<VkImage> renderTargetImage(createImageAndBindMemory(
        renderSize, rtFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, &rtMemory));

    // Render target view
    Move<VkImageView> rtView(createImageView(rtFormat, *renderTargetImage));

    // Pixel buffer
    const uint32_t pixelBufferSize = renderSize.x() * renderSize.y() * tcu::getPixelSize(mapVkFormat(rtFormat));

    AllocationMp pixelBufferMemory;
    Move<VkBuffer> pixelBuffer(
        createBufferAndBindMemory(pixelBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, &pixelBufferMemory));

    // Pipeline layout
    Move<VkPipelineLayout> pipelineLayout(createPipelineLayout());

    // Renderpass
    Move<VkRenderPass> renderPass(makeRenderPass(devInterface, dev, rtFormat));

    // Framebuffer
    Move<VkFramebuffer> framebuffer(
        makeFramebuffer(devInterface, dev, *renderPass, *rtView, renderSize.x(), renderSize.y()));

    // Pipeline
    Move<VkPipeline> pipeline(createGraphicsPipeline(patchCnt, *pipelineLayout, *renderPass));

    // Rendering
    beginCommandBuffer(devInterface, *cmdBuffer);

    const vk::VkBuffer vertexBuffers[]           = {*perVertexVB, *perInstanceVB};
    const vk::VkDeviceSize vertexBufferOffsets[] = {0, 0};

    devInterface.cmdBindVertexBuffers(*cmdBuffer, 0, DE_LENGTH_OF_ARRAY(vertexBuffers), vertexBuffers,
                                      vertexBufferOffsets);

    devInterface.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    beginRenderPass(devInterface, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(renderSize), clearColor);

    if (m_params.testType == TESSINSTANCEDTYPE_INSTANCED)
        devInterface.cmdDraw(*cmdBuffer, patchCnt, static_cast<uint32_t>(perInstanceVertices.size()), 0u, 0u);
    else // m_params.testType == TESSINSTANCEDTYPE_NO_PATCHES
        devInterface.cmdDraw(*cmdBuffer, 2u, 1u, 0u, 0u);

    endRenderPass(devInterface, *cmdBuffer);

    copyImageToBuffer(devInterface, *cmdBuffer, *renderTargetImage, *pixelBuffer, {renderSize.x(), renderSize.y()});

    endCommandBuffer(devInterface, *cmdBuffer);

    submitCommandsAndWait(devInterface, dev, queue, cmdBuffer.get());

    // Reference rendering
    tcu::TextureFormat tcuFormat = mapVkFormat(rtFormat);
    tcu::TextureLevel refImage(tcuFormat, renderSize.x(), renderSize.y());

    tcu::clear(refImage.getAccess(), clearColor);

    if (m_params.testType == TESSINSTANCEDTYPE_INSTANCED)
    {
        const InstancedVertexShader vertShader;
        const InstancedFragmentShader fragShader;
        const rr::Program program(&vertShader, &fragShader);
        const rr::MultisamplePixelBufferAccess colorBuffer =
            rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(refImage.getAccess());
        const rr::RenderTarget renderTarget(colorBuffer);
        const rr::RenderState renderState((rr::ViewportState(colorBuffer)),
                                          m_context.getDeviceProperties().limits.subPixelPrecisionBits);
        const rr::Renderer renderer;

        const rr::VertexAttrib vertexAttribs[] = {
            rr::VertexAttrib(rr::VERTEXATTRIBTYPE_FLOAT, 4, sizeof(tcu::Vec4), 0,
                             perVertexVertices.data()), // 0 means per vertex attribute
            rr::VertexAttrib(rr::VERTEXATTRIBTYPE_FLOAT, 4, sizeof(tcu::Vec4), 1,
                             perInstanceVertices.data()) // 1 means per instance attribute
        };

        if (m_params.primitiveType == TESSPRIMITIVETYPE_QUADS)
        {
            const std::vector<uint16_t> indices = genIndexData();

            const rr::DrawIndices drawIndices(indices.data());
            const rr::PrimitiveList primitives =
                rr::PrimitiveList(rr::PRIMITIVETYPE_TRIANGLES, static_cast<int>(indices.size()), drawIndices);
            const rr::DrawCommand command(renderState, renderTarget, program, DE_LENGTH_OF_ARRAY(vertexAttribs),
                                          &vertexAttribs[0], primitives);

            renderer.drawInstanced(command, 4);
        }
        else
        {
            const rr::PrimitiveList primitives =
                rr::PrimitiveList(rr::PRIMITIVETYPE_TRIANGLES, static_cast<int>(perVertexVertices.size()), 0);
            const rr::DrawCommand command(renderState, renderTarget, program, DE_LENGTH_OF_ARRAY(vertexAttribs),
                                          &vertexAttribs[0], primitives);

            renderer.drawInstanced(command, 4);
        }
    }

    // Compare result
    tcu::TestLog &log = m_context.getTestContext().getLog();
    qpTestResult res  = QP_TEST_RESULT_FAIL;
    const tcu::ConstPixelBufferAccess resultAccess(tcuFormat, renderSize, pixelBufferMemory->getHostPtr());
    const tcu::ConstPixelBufferAccess refAccess = refImage.getAccess();

    if (tcu::fuzzyCompare(log, "Result", "", refAccess, resultAccess, 0.05f, tcu::COMPARE_LOG_RESULT))
        res = QP_TEST_RESULT_PASS;

    return tcu::TestStatus(res, qpGetTestResultName(res));
}

Move<VkBuffer> TessInstancedDrawTestInstance::createBufferAndBindMemory(uint32_t bufferSize,
                                                                        VkBufferUsageFlags usageFlags,
                                                                        AllocationMp *outMemory)
{
    const VkDevice &device      = m_context.getDevice();
    const DeviceInterface &vkdi = m_context.getDeviceInterface();
    Allocator &allocator        = m_context.getDefaultAllocator();

    const VkBufferCreateInfo bufferCreateInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType        sType
        nullptr,                              // const void*            pNext
        0,                                    // VkBufferCreateFlags    flags
        bufferSize,                           // VkDeviceSize           size
        usageFlags,                           // VkBufferUsageFlags     usage
        VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode          sharingMode
        0,                                    // uint32_t               queueFamilyIndexCount
        nullptr,                              // const uint32_t*        pQueueFamilyIndices
    };

    Move<VkBuffer> buffer(vk::createBuffer(vkdi, device, &bufferCreateInfo));
    const VkMemoryRequirements requirements = getBufferMemoryRequirements(vkdi, device, *buffer);
    AllocationMp bufferMemory               = allocator.allocate(requirements, MemoryRequirement::HostVisible);

    VK_CHECK(vkdi.bindBufferMemory(device, *buffer, bufferMemory->getMemory(), bufferMemory->getOffset()));
    *outMemory = bufferMemory;

    return buffer;
}

Move<VkImage> TessInstancedDrawTestInstance::createImageAndBindMemory(tcu::IVec3 imgSize, VkFormat format,
                                                                      VkImageUsageFlags usageFlags,
                                                                      AllocationMp *outMemory)
{
    const VkDevice &device      = m_context.getDevice();
    const DeviceInterface &vkdi = m_context.getDeviceInterface();
    Allocator &allocator        = m_context.getDefaultAllocator();

    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType          sType
        nullptr,                             // const void*              pNext
        0u,                                  // VkImageCreateFlags       flags
        VK_IMAGE_TYPE_2D,                    // VkImageType              imageType
        format,                              // VkFormat                 format
        makeExtent3D(imgSize),               // VkExtent3D               extent
        1u,                                  // uint32_t                 mipLevels
        1u,                                  // uint32_t                 arrayLayers
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits    samples
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling            tiling
        usageFlags,                          // VkImageUsageFlags        usage
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode            sharingMode
        0u,                                  // uint32_t                 queueFamilyIndexCount
        nullptr,                             // const uint32_t*          pQueueFamilyIndices
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout            initialLayout
    };

    Move<VkImage> image(vk::createImage(vkdi, device, &imageCreateInfo));
    const VkMemoryRequirements requirements = getImageMemoryRequirements(vkdi, device, *image);
    AllocationMp imageMemory                = allocator.allocate(requirements, MemoryRequirement::Any);

    VK_CHECK(vkdi.bindImageMemory(device, *image, imageMemory->getMemory(), imageMemory->getOffset()));
    *outMemory = imageMemory;

    return image;
}

Move<VkImageView> TessInstancedDrawTestInstance::createImageView(VkFormat format, VkImage image)
{
    const VkDevice &device      = m_context.getDevice();
    const DeviceInterface &vkdi = m_context.getDeviceInterface();

    VkImageSubresourceRange range = {
        VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask
        0u,                        // uint32_t           baseMipLevel
        1u,                        // uint32_t           levelCount
        0u,                        // uint32_t           baseArrayLayer
        1u,                        // uint32_t           layerCount
    };

    return makeImageView(vkdi, device, image, VK_IMAGE_VIEW_TYPE_2D, format, range);
}

Move<VkPipelineLayout> TessInstancedDrawTestInstance::createPipelineLayout()
{
    const VkDevice &device      = m_context.getDevice();
    const DeviceInterface &vkdi = m_context.getDeviceInterface();

    const VkPipelineLayoutCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType                 sType
        nullptr,                                       // const void*                     pNext
        (VkPipelineLayoutCreateFlags)0,                // VkPipelineLayoutCreateFlags     flags
        0u,                                            // uint32_t                        setLayoutCount
        nullptr,                                       // const VkDescriptorSetLayout*    pSetLayouts
        0u,                                            // uint32_t                        pushConstantRangeCount
        nullptr,                                       // const VkPushConstantRange*      pPushConstantRanges
    };

    return vk::createPipelineLayout(vkdi, device, &createInfo);
}

Move<VkPipeline> TessInstancedDrawTestInstance::createGraphicsPipeline(uint32_t patchCnt, VkPipelineLayout layout,
                                                                       VkRenderPass renderpass)
{
    const VkDevice &device      = m_context.getDevice();
    const DeviceInterface &vkdi = m_context.getDeviceInterface();

    vk::BinaryCollection &binCollection = m_context.getBinaryCollection();
    Move<VkShaderModule> vertModule(createShaderModule(vkdi, device, binCollection.get("vert")));
    Move<VkShaderModule> tessCtrlModule(createShaderModule(vkdi, device, binCollection.get("tess_ctrl")));
    Move<VkShaderModule> tessEvalModule(createShaderModule(vkdi, device, binCollection.get("tess_eval")));
    Move<VkShaderModule> fragModule(createShaderModule(vkdi, device, binCollection.get("frag")));

    VkPipelineShaderStageCreateInfo stageInfos[4];
    uint32_t stageNdx = 0;

    {
        const VkPipelineShaderStageCreateInfo pipelineShaderStageParam = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            0,                                                   // VkPipelineShaderStageCreateFlags flags;
            VK_SHADER_STAGE_VERTEX_BIT,                          // VkShaderStageFlagBits stage;
            *vertModule,                                         // VkShaderModule module;
            "main",                                              // const char* pName;
            nullptr,                                             // const VkSpecializationInfo* pSpecializationInfo;
        };
        stageInfos[stageNdx++] = pipelineShaderStageParam;
    }

    {
        const VkPipelineShaderStageCreateInfo pipelineShaderStageParam = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            0,                                                   // VkPipelineShaderStageCreateFlags flags;
            VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,            // VkShaderStageFlagBits stage;
            *tessCtrlModule,                                     // VkShaderModule module;
            "main",                                              // const char* pName;
            nullptr,                                             // const VkSpecializationInfo* pSpecializationInfo;
        };
        stageInfos[stageNdx++] = pipelineShaderStageParam;
    }

    {
        const VkPipelineShaderStageCreateInfo pipelineShaderStageParam = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            0,                                                   // VkPipelineShaderStageCreateFlags flags;
            VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,         // VkShaderStageFlagBits stage;
            *tessEvalModule,                                     // VkShaderModule module;
            "main",                                              // const char* pName;
            nullptr,                                             // const VkSpecializationInfo* pSpecializationInfo;
        };
        stageInfos[stageNdx++] = pipelineShaderStageParam;
    }

    {
        const VkPipelineShaderStageCreateInfo pipelineShaderStageParam = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            0,                                                   // VkPipelineShaderStageCreateFlags flags;
            VK_SHADER_STAGE_FRAGMENT_BIT,                        // VkShaderStageFlagBits stage;
            *fragModule,                                         // VkShaderModule module;
            "main",                                              // const char* pName;
            nullptr,                                             // const VkSpecializationInfo* pSpecializationInfo;
        };
        stageInfos[stageNdx++] = pipelineShaderStageParam;
    }

    const VkVertexInputBindingDescription vertexInputBindingDescriptions[] = {
        {
            0u,                         // uint32_t             binding
            sizeof(tcu::Vec4),          // uint32_t             stride
            VK_VERTEX_INPUT_RATE_VERTEX // VkVertexInputRate    inputRate
        },
        {
            1u,                           // uint32_t             binding
            sizeof(tcu::Vec4),            // uint32_t             stride
            VK_VERTEX_INPUT_RATE_INSTANCE // VkVertexInputRate    inputRate
        }};

    const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] = {
        {
            0u,                            // uint32_t    location
            0u,                            // uint32_t    binding
            VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat    format
            0u                             // uint32_t    offset
        },
        {
            1u,                            // uint32_t    location
            1u,                            // uint32_t    binding
            VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat    format
            0u,                            // uint32_t    offset
        }};

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfoDefault = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                             sType
        nullptr,                                                   // const void*                                 pNext
        (VkPipelineVertexInputStateCreateFlags)0,                  // VkPipelineVertexInputStateCreateFlags       flags
        2u,                              // uint32_t                                    vertexBindingDescriptionCount
        vertexInputBindingDescriptions,  // const VkVertexInputBindingDescription*      pVertexBindingDescriptions
        2u,                              // uint32_t                                    vertexAttributeDescriptionCount
        vertexInputAttributeDescriptions // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
    };

    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfoDefault = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType                            sType
        nullptr,                                                     // const void*                                pNext
        0u,                                                          // VkPipelineInputAssemblyStateCreateFlags    flags
        VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, // VkPrimitiveTopology                        topology
        VK_FALSE                          // VkBool32                                   primitiveRestartEnable
    };

    const VkPipelineTessellationStateCreateInfo tessStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO, // VkStructureType                        sType
        nullptr,                                                   // const void*                            pNext
        0u,                                                        // VkPipelineTessellationStateCreateFlags flags
        patchCnt // uint32_t                               patchControlPoints
    };

    const VkViewport viewport = {
        0.0f,   // float x
        0.0f,   // float y
        256.0f, // float width
        256.0f, // float height
        0.0f,   // float minDepth
        1.0f    // float maxDepth
    };

    const VkRect2D scissor = {
        {0, 0},      // VkOffset2D    offset
        {256u, 256u} // VkExtent2D    extent
    };

    const VkPipelineViewportStateCreateInfo viewportStateCreateInfoDefault = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType                             sType
        nullptr,                                               // const void*                                 pNext
        (VkPipelineViewportStateCreateFlags)0,                 // VkPipelineViewportStateCreateFlags          flags
        1u,        // uint32_t                                    viewportCount
        &viewport, // const VkViewport*                           pViewports
        1u,        // uint32_t                                    scissorCount
        &scissor   // const VkRect2D*                             pScissors
    };

    const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfoDefault = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType                            sType
        nullptr,                                                    // const void*                                pNext
        0u,                                                         // VkPipelineRasterizationStateCreateFlags    flags
        VK_FALSE,                        // VkBool32                                   depthClampEnable
        VK_FALSE,                        // VkBool32                                   rasterizerDiscardEnable
        VK_POLYGON_MODE_FILL,            // VkPolygonMode                              polygonMode
        VK_CULL_MODE_NONE,               // VkCullModeFlags                            cullMode
        VK_FRONT_FACE_COUNTER_CLOCKWISE, // VkFrontFace                                frontFace
        VK_FALSE,                        // VkBool32                                   depthBiasEnable
        0.0f,                            // float                                      depthBiasConstantFactor
        0.0f,                            // float                                      depthBiasClamp
        0.0f,                            // float                                      depthBiasSlopeFactor
        1.0f                             // float                                      lineWidth
    };

    const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfoDefault = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType                          sType
        nullptr,                                                  // const void*                              pNext
        0u,                                                       // VkPipelineMultisampleStateCreateFlags    flags
        VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits                    rasterizationSamples
        VK_FALSE,              // VkBool32                                 sampleShadingEnable
        1.0f,                  // float                                    minSampleShading
        nullptr,               // const VkSampleMask*                      pSampleMask
        VK_FALSE,              // VkBool32                                 alphaToCoverageEnable
        VK_FALSE               // VkBool32                                 alphaToOneEnable
    };

    const VkStencilOpState stencilOpState = {
        VK_STENCIL_OP_KEEP,  // VkStencilOp    failOp
        VK_STENCIL_OP_KEEP,  // VkStencilOp    passOp
        VK_STENCIL_OP_KEEP,  // VkStencilOp    depthFailOp
        VK_COMPARE_OP_NEVER, // VkCompareOp    compareOp
        0,                   // uint32_t       compareMask
        0,                   // uint32_t       writeMask
        0                    // uint32_t       reference
    };

    const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfoDefault = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType                          sType
        nullptr,                                                    // const void*                              pNext
        0u,                                                         // VkPipelineDepthStencilStateCreateFlags   flags
        VK_FALSE,                    // VkBool32                                 depthTestEnable
        VK_FALSE,                    // VkBool32                                 depthWriteEnable
        VK_COMPARE_OP_LESS_OR_EQUAL, // VkCompareOp                              depthCompareOp
        VK_FALSE,                    // VkBool32                                 depthBoundsTestEnable
        VK_FALSE,                    // VkBool32                                 stencilTestEnable
        stencilOpState,              // VkStencilOpState                         front
        stencilOpState,              // VkStencilOpState                         back
        0.0f,                        // float                                    minDepthBounds
        1.0f,                        // float                                    maxDepthBounds
    };

    const VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {
        VK_FALSE,                // VkBool32                 blendEnable
        VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            srcColorBlendFactor
        VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            dstColorBlendFactor
        VK_BLEND_OP_ADD,         // VkBlendOp                colorBlendOp
        VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            srcAlphaBlendFactor
        VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            dstAlphaBlendFactor
        VK_BLEND_OP_ADD,         // VkBlendOp                alphaBlendOp
        VK_COLOR_COMPONENT_R_BIT // VkColorComponentFlags    colorWriteMask
            | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

    const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfoDefault = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType                               sType
        nullptr,                                                  // const void*                                   pNext
        0u,                                                       // VkPipelineColorBlendStateCreateFlags          flags
        VK_FALSE,                   // VkBool32                                      logicOpEnable
        VK_LOGIC_OP_CLEAR,          // VkLogicOp                                     logicOp
        1u,                         // uint32_t                                      attachmentCount
        &colorBlendAttachmentState, // const VkPipelineColorBlendAttachmentState*    pAttachments
        {0.0f, 0.0f, 0.0f, 0.0f}    // float                                         blendConstants[4]
    };

    const VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, // VkStructureType                                  sType
        nullptr,                                         // const void*                                      pNext
        0,                                               // VkPipelineCreateFlags                            flags
        stageNdx,                                        // uint32_t                                         stageCount
        stageInfos,                                      // const VkPipelineShaderStageCreateInfo*           pStages
        &vertexInputStateCreateInfoDefault,   // const VkPipelineVertexInputStateCreateInfo*      pVertexInputState
        &inputAssemblyStateCreateInfoDefault, // const VkPipelineInputAssemblyStateCreateInfo*    pInputAssemblyState
        &tessStateCreateInfo,                 // const VkPipelineTessellationStateCreateInfo*     pTessellationState
        &viewportStateCreateInfoDefault,      // const VkPipelineViewportStateCreateInfo*         pViewportState
        &rasterizationStateCreateInfoDefault, // const VkPipelineRasterizationStateCreateInfo*    pRasterizationState
        &multisampleStateCreateInfoDefault,   // const VkPipelineMultisampleStateCreateInfo*      pMultisampleState
        &depthStencilStateCreateInfoDefault,  // const VkPipelineDepthStencilStateCreateInfo*     pDepthStencilState
        &colorBlendStateCreateInfoDefault,    // const VkPipelineColorBlendStateCreateInfo*       pColorBlendState
        nullptr,                              // const VkPipelineDynamicStateCreateInfo*          pDynamicState
        layout,                               // VkPipelineLayout                                 layout
        renderpass,                           // VkRenderPass                                     renderPass
        0,                                    // uint32_t                                         subpass
        VK_NULL_HANDLE,                       // VkPipeline                                       basePipelineHandle
        0                                     // int32_t                                          basePipelineIndex;
    };

    return vk::createGraphicsPipeline(vkdi, device, VK_NULL_HANDLE, &pipelineCreateInfo);
}

std::vector<tcu::Vec4> TessInstancedDrawTestInstance::genPerVertexVertexData()
{
    std::vector<tcu::Vec4> vertices = {
        {-0.1f, -0.1f, 0.0f, 1.0f},
        {0.1f, -0.1f, 0.0f, 1.0f},
        {0.1f, 0.1f, 0.0f, 1.0f},
        {-0.1f, 0.1f, 0.0f, 1.0f},
    };

    return vertices;
}

std::vector<tcu::Vec4> TessInstancedDrawTestInstance::genPerInstanceVertexData()
{
    std::vector<tcu::Vec4> vertices = {
        {-0.5f, -0.5f, 0.0f, 1.0f},
        {0.5f, -0.5f, 0.0f, 1.0f},
        {0.5f, 0.5f, 0.0f, 1.0f},
        {-0.5f, 0.5f, 0.0f, 1.0f},
    };

    return vertices;
}

std::vector<uint16_t> TessInstancedDrawTestInstance::genIndexData()
{
    std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};

    return indices;
}

} // namespace

//! These tests correspond to dEQP-GLES31.functional.tessellation.misc_draw.*
tcu::TestCaseGroup *createMiscDrawTests(tcu::TestContext &testCtx)
{
    // Miscellaneous draw-result-verifying cases
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "misc_draw"));

    static const TessPrimitiveType primitivesNoIsolines[] = {
        TESSPRIMITIVETYPE_TRIANGLES,
        TESSPRIMITIVETYPE_QUADS,
    };

    static const DrawType drawTypes[] = {
        DRAWTYPE_DRAW,
        DRAWTYPE_DRAW_INDIRECT,
    };

    // Triangle fill case
    for (int drawTypeNdx = 0; drawTypeNdx < DE_LENGTH_OF_ARRAY(drawTypes); ++drawTypeNdx)
        for (int primitiveTypeNdx = 0; primitiveTypeNdx < DE_LENGTH_OF_ARRAY(primitivesNoIsolines); ++primitiveTypeNdx)
            for (int spacingModeNdx = 0; spacingModeNdx < SPACINGMODE_LAST; ++spacingModeNdx)
            {
                const TessPrimitiveType primitiveType = primitivesNoIsolines[primitiveTypeNdx];
                const SpacingMode spacingMode         = static_cast<SpacingMode>(spacingModeNdx);
                const DrawType drawType               = static_cast<DrawType>(drawTypeNdx);
                const std::string caseName            = std::string() + "fill_cover_" +
                                             getTessPrimitiveTypeShaderName(primitiveType) + "_" +
                                             getSpacingModeShaderName(spacingMode) + "_" + getDrawName(drawType);
                const std::string refName = std::string() + "fill_cover_" +
                                            getTessPrimitiveTypeShaderName(primitiveType) + "_" +
                                            getSpacingModeShaderName(spacingMode);

                // Check that there are no obvious gaps in the triangle-filled area of a tessellated shape
                addFunctionCaseWithPrograms(
                    group.get(), caseName, initProgramsFillCoverCase, runTest,
                    makeCaseDefinition(primitiveType, spacingMode, drawType, getReferenceImagePathPrefix(refName)));
            }

    // Triangle non-overlap case
    for (int drawTypeNdx = 0; drawTypeNdx < DE_LENGTH_OF_ARRAY(drawTypes); ++drawTypeNdx)
        for (int primitiveTypeNdx = 0; primitiveTypeNdx < DE_LENGTH_OF_ARRAY(primitivesNoIsolines); ++primitiveTypeNdx)
            for (int spacingModeNdx = 0; spacingModeNdx < SPACINGMODE_LAST; ++spacingModeNdx)
            {
                const TessPrimitiveType primitiveType = primitivesNoIsolines[primitiveTypeNdx];
                const SpacingMode spacingMode         = static_cast<SpacingMode>(spacingModeNdx);
                const DrawType drawType               = static_cast<DrawType>(drawTypeNdx);
                const std::string caseName            = std::string() + "fill_overlap_" +
                                             getTessPrimitiveTypeShaderName(primitiveType) + "_" +
                                             getSpacingModeShaderName(spacingMode) + "_" + getDrawName(drawType);
                const std::string refName = std::string() + "fill_overlap_" +
                                            getTessPrimitiveTypeShaderName(primitiveType) + "_" +
                                            getSpacingModeShaderName(spacingMode);

                // Check that there are no obvious triangle overlaps in the triangle-filled area of a tessellated shape
                addFunctionCaseWithPrograms(
                    group.get(), caseName, initProgramsFillNonOverlapCase, runTest,
                    makeCaseDefinition(primitiveType, spacingMode, drawType, getReferenceImagePathPrefix(refName)));
            }

    // Isolines
    for (int drawTypeNdx = 0; drawTypeNdx < DE_LENGTH_OF_ARRAY(drawTypes); ++drawTypeNdx)
        for (int spacingModeNdx = 0; spacingModeNdx < SPACINGMODE_LAST; ++spacingModeNdx)
        {
            const SpacingMode spacingMode = static_cast<SpacingMode>(spacingModeNdx);
            const DrawType drawType       = static_cast<DrawType>(drawTypeNdx);
            const std::string caseName =
                std::string() + "isolines_" + getSpacingModeShaderName(spacingMode) + "_" + getDrawName(drawType);
            const std::string refName = std::string() + "isolines_" + getSpacingModeShaderName(spacingMode);

            // Basic isolines render test
            addFunctionCaseWithPrograms(group.get(), caseName, checkSupportCase, initProgramsIsolinesCase, runTest,
                                        makeCaseDefinition(TESSPRIMITIVETYPE_ISOLINES, spacingMode, drawType,
                                                           getReferenceImagePathPrefix(refName)));
        }

    static const TessInstancedType tessInstancedTypes[] = {
        TESSINSTANCEDTYPE_NO_PATCHES,
        TESSINSTANCEDTYPE_INSTANCED,
    };

    // Instanced
    for (int primitiveTypeNdx = 0; primitiveTypeNdx < DE_LENGTH_OF_ARRAY(primitivesNoIsolines); ++primitiveTypeNdx)
        for (int testTypeNdx = 0; testTypeNdx < DE_LENGTH_OF_ARRAY(tessInstancedTypes); ++testTypeNdx)
        {
            const TessPrimitiveType primitiveType = primitivesNoIsolines[primitiveTypeNdx];
            const TessInstancedType testType      = tessInstancedTypes[testTypeNdx];
            const std::string caseName = std::string() + getTessPrimitiveTypeShaderName(primitiveType) + "_" +
                                         getInstancedDrawTestName(testType);

            const TessInstancedDrawTestParams testParams = {
                testType,
                primitiveType,
            };

            group->addChild(new TessInstancedDrawTestCase(testCtx, caseName.c_str(), testParams));
        }

    // Test switching tessellation parameters on the fly.
    struct
    {
        PipelineConstructionType constructionType;
        const char *suffix;
    } constructionCases[] = {
        {PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, ""},
        {PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY, "_fast_lib"},
        {PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV, "_shader_objects"},
    };

    for (const auto &constructionCase : constructionCases)
        for (const auto &geometryShader : {false, true})
        {
            std::string nameSuffix;

            if (geometryShader)
                nameSuffix += "_with_geom_shader";
            nameSuffix += constructionCase.suffix;

            static const VkTessellationDomainOrigin domainOrigins[] = {
                VK_TESSELLATION_DOMAIN_ORIGIN_LOWER_LEFT,
                VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT,
            };

            for (const auto &firstPrimitiveType : primitivesNoIsolines)
                for (const auto &secondPrimitiveType : primitivesNoIsolines)
                {
                    if (firstPrimitiveType == secondPrimitiveType)
                        continue;

                    const TessStateSwitchParams params{
                        constructionCase.constructionType,
                        std::make_pair(firstPrimitiveType, secondPrimitiveType),
                        std::make_pair(SPACINGMODE_EQUAL, SPACINGMODE_EQUAL),
                        std::make_pair(VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT,
                                       VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT),
                        std::make_pair(4u, 4u),
                        geometryShader,
                    };

                    const auto testName = std::string("switch_primitive_") +
                                          getTessPrimitiveTypeShaderName(params.patchTypes.first) + "_to_" +
                                          getTessPrimitiveTypeShaderName(params.patchTypes.second) + nameSuffix;
                    group->addChild(new TessStateSwitchCase(testCtx, testName, params));
                }

            for (const auto &firstDomainOrigin : domainOrigins)
                for (const auto &secondDomainOrigin : domainOrigins)
                {
                    if (firstDomainOrigin == secondDomainOrigin)
                        continue;

                    const TessStateSwitchParams params{
                        constructionCase.constructionType,
                        std::make_pair(TESSPRIMITIVETYPE_QUADS, TESSPRIMITIVETYPE_QUADS),
                        std::make_pair(SPACINGMODE_EQUAL, SPACINGMODE_EQUAL),
                        std::make_pair(firstDomainOrigin, secondDomainOrigin),
                        std::make_pair(4u, 4u),
                        geometryShader,
                    };

                    const auto testName = std::string("switch_domain_origin_") +
                                          getDomainOriginName(params.domainOrigin.first) + "_to_" +
                                          getDomainOriginName(params.domainOrigin.second) + nameSuffix;
                    group->addChild(new TessStateSwitchCase(testCtx, testName, params));
                }

            for (int firstSpacingModeNdx = 0; firstSpacingModeNdx < SPACINGMODE_LAST; ++firstSpacingModeNdx)
                for (int secondSpacingModeNdx = 0; secondSpacingModeNdx < SPACINGMODE_LAST; ++secondSpacingModeNdx)
                {
                    if (firstSpacingModeNdx == secondSpacingModeNdx)
                        continue;

                    const SpacingMode firstSpacingMode  = static_cast<SpacingMode>(firstSpacingModeNdx);
                    const SpacingMode secondSpacingMode = static_cast<SpacingMode>(secondSpacingModeNdx);

                    const TessStateSwitchParams params{
                        constructionCase.constructionType,
                        std::make_pair(TESSPRIMITIVETYPE_QUADS, TESSPRIMITIVETYPE_QUADS),
                        std::make_pair(firstSpacingMode, secondSpacingMode),
                        std::make_pair(VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT,
                                       VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT),
                        std::make_pair(4u, 4u),
                        geometryShader,
                    };

                    const auto testName = std::string("switch_spacing_mode_") +
                                          getSpacingModeShaderName(params.spacing.first) + "_to_" +
                                          getSpacingModeShaderName(params.spacing.second) + nameSuffix;
                    group->addChild(new TessStateSwitchCase(testCtx, testName, params));
                }

            // Switch vertex counts.
            {
                const std::vector<uint32_t> vertexCounts{3u, 4u};
                for (const auto firstCount : vertexCounts)
                    for (const auto secondCount : vertexCounts)
                    {
                        if (firstCount == secondCount)
                            continue;

                        const TessStateSwitchParams params{
                            constructionCase.constructionType,
                            std::make_pair(TESSPRIMITIVETYPE_QUADS, TESSPRIMITIVETYPE_QUADS),
                            std::make_pair(SPACINGMODE_EQUAL, SPACINGMODE_EQUAL),
                            std::make_pair(VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT,
                                           VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT),
                            std::make_pair(firstCount, secondCount),
                            geometryShader,
                        };

                        const auto testName = "switch_out_vertices_" + std::to_string(firstCount) + "_to_" +
                                              std::to_string(secondCount) + nameSuffix;
                        group->addChild(new TessStateSwitchCase(testCtx, testName, params));
                    }
            }
        }

#ifndef CTS_USES_VULKANSC
    {
        const auto testName = std::string("tess_factor_barrier_bug");
        const auto dataDir  = "tessellation";
        const std::vector<std::string> requirements{"Features.tessellationShader",
                                                    "Features.vertexPipelineStoresAndAtomics"};
        group->addChild(
            cts_amber::createAmberTestCase(testCtx, testName.c_str(), dataDir, testName + ".amber", requirements));
    }
#endif // CTS_USES_VULKANSC

    return group.release();
}

} // namespace tessellation
} // namespace vkt
