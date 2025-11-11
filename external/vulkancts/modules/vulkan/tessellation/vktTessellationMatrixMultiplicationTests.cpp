/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
 * Copyright (c) 2025 Google LLC
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
 * \brief Tessellation Matrix Multiplication Tests
 *//*--------------------------------------------------------------------*/

#include "vktTessellationMatrixMultiplicationTests.hpp"
#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBarrierUtil.hpp"

namespace vkt
{
namespace tessellation
{

namespace
{

using namespace vk;

enum TestType
{
    TEST_TESC_1,
    TEST_TESC_2,
};

class MatrixMultiplicationTestInstance : public vkt::TestInstance
{
public:
    MatrixMultiplicationTestInstance(vkt::Context &context) : vkt::TestInstance(context)
    {
    }

private:
    tcu::TestStatus iterate(void);
};

tcu::TestStatus MatrixMultiplicationTestInstance::iterate(void)
{
    const InstanceInterface &vki              = m_context.getInstanceInterface();
    const DeviceInterface &vk                 = m_context.getDeviceInterface();
    const vk::VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    const vk::VkDevice device                 = m_context.getDevice();
    const auto &deviceExtensions              = m_context.getDeviceExtensions();
    const uint32_t queueFamilyIndex           = m_context.getUniversalQueueFamilyIndex();
    const vk::VkQueue queue                   = m_context.getUniversalQueue();
    auto &alloc                               = m_context.getDefaultAllocator();

    const VkFormat format        = VK_FORMAT_R8G8B8A8_UNORM;
    const VkExtent3D imageSize   = {4u, 4u, 1u};
    const auto subresourceLayers = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
    const auto subresourceRange  = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

    ShaderWrapper vert = ShaderWrapper(vk, device, m_context.getBinaryCollection().get("vert"));
    ShaderWrapper tesc = ShaderWrapper(vk, device, m_context.getBinaryCollection().get("tesc"));
    ShaderWrapper tese = ShaderWrapper(vk, device, m_context.getBinaryCollection().get("tese"));
    ShaderWrapper frag = ShaderWrapper(vk, device, m_context.getBinaryCollection().get("frag"));

    const Move<VkCommandPool> cmdPool(
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
    const Move<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    const auto outputBufferSize                     = imageSize.width * imageSize.height * 4u;
    de::MovePtr<BufferWithMemory> colorOutputBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, device, alloc, makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        MemoryRequirement::HostVisible));

    const vk::VkImageCreateInfo createInfo = {
        vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                               // VkStructureType          sType
        nullptr,                                                               // const void*              pNext
        0u,                                                                    // VkImageCreateFlags       flags
        VK_IMAGE_TYPE_2D,                                                      // VkImageType              imageType
        format,                                                                // VkFormat                 format
        imageSize,                                                             // VkExtent3D               extent
        1u,                                                                    // uint32_t                 mipLevels
        1u,                                                                    // uint32_t                 arrayLayers
        VK_SAMPLE_COUNT_1_BIT,                                                 // VkSampleCountFlagBits    samples
        VK_IMAGE_TILING_OPTIMAL,                                               // VkImageTiling            tiling
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags        usage
        VK_SHARING_MODE_EXCLUSIVE,                                             // VkSharingMode            sharingMode
        0,                        // uint32_t                 queueFamilyIndexCount
        nullptr,                  // const uint32_t*          pQueueFamilyIndices
        VK_IMAGE_LAYOUT_UNDEFINED // VkImageLayout            initialLayout
    };

    ImageWithMemory outputImage(vk, device, alloc, createInfo, MemoryRequirement::Any);

    VkImageViewCreateInfo imageViewCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        (VkImageViewCreateFlags)0u,               // VkImageViewCreateFlags flags;
        *outputImage,                             // VkImage image;
        VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
        format,                                   // VkFormat format;
        makeComponentMappingRGBA(),               // VkComponentMapping components;
        subresourceRange                          // VkImageSubresourceRange subresourceRange;
    };
    const Move<VkImageView> outputImageView = createImageView(vk, device, &imageViewCreateInfo, nullptr);

    const Move<VkRenderPass> renderPass = makeRenderPass(vk, device, format);
    const Move<VkFramebuffer> framebuffer =
        makeFramebuffer(vk, device, *renderPass, *outputImageView, imageSize.width, imageSize.height);

    const vk::VkPipelineVertexInputStateCreateInfo vertexInput = initVulkanStructure();

    const std::vector<vk::VkViewport> viewports{makeViewport(imageSize.width, imageSize.height)};
    const std::vector<vk::VkRect2D> scissors{makeRect2D(imageSize.width, imageSize.height)};

    const vk::PipelineLayoutWrapper pipelineLayout(PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, vk, device);

    GraphicsPipelineWrapper pipeline(vki, vk, physicalDevice, device, deviceExtensions,
                                     PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC);

    pipeline.setDefaultTopology(vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
        .setDefaultRasterizationState()
        .setDefaultMultisampleState()
        .setDefaultDepthStencilState()
        .setDefaultColorBlendState()
        .setupVertexInputState(&vertexInput)
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u, vert, nullptr, tesc,
                                          tese)
        .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, frag)
        .setupFragmentOutputState(*renderPass)
        .setMonolithicPipelineLayout(pipelineLayout)
        .buildPipeline();

    vk::beginCommandBuffer(vk, *cmdBuffer);
    const vk::VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 0.0f}}};
    beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(imageSize), clearColor);
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipeline());
    vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);
    endRenderPass(vk, *cmdBuffer);

    auto imageMemoryBarrier = makeImageMemoryBarrier(
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *outputImage, subresourceRange);
    vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          vk::VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1, &imageMemoryBarrier);

    const VkBufferImageCopy copyRegion = makeBufferImageCopy(imageSize, subresourceLayers);
    vk.cmdCopyImageToBuffer(*cmdBuffer, *outputImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **colorOutputBuffer, 1u,
                            &copyRegion);
    vk::endCommandBuffer(vk, *cmdBuffer);
    vk::submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    tcu::ConstPixelBufferAccess resultBuffer =
        tcu::ConstPixelBufferAccess(vk::mapVkFormat(format), imageSize.width, imageSize.height, 1,
                                    (const void *)colorOutputBuffer->getAllocation().getHostPtr());

    for (uint32_t j = 0; j < imageSize.height; ++j)
    {
        for (uint32_t i = 0; i < imageSize.width; ++i)
        {
            const tcu::Vec4 color = resultBuffer.getPixel(i, j).asFloat();
            if (color.x() != 1.0f || color.y() != 1.0f || color.z() != 1.0f || color.w() != 1.0f)
            {
                return tcu::TestStatus::fail("Fail");
            }
        }
    }
    return tcu::TestStatus::pass("Pass");
}

class MatrixMultiplicationTestCase : public vkt::TestCase
{
public:
    MatrixMultiplicationTestCase(tcu::TestContext &context, const char *name, TestType testType)
        : TestCase(context, name)
        , m_testType(testType)
    {
    }

private:
    TestType m_testType;

    void checkSupport(vkt::Context &context) const;
    void initPrograms(vk::SourceCollections &programCollection) const;
    vkt::TestInstance *createInstance(vkt::Context &context) const
    {
        return new MatrixMultiplicationTestInstance(context);
    }
};

void MatrixMultiplicationTestCase::checkSupport(vkt::Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);
}

void MatrixMultiplicationTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vert;
    std::ostringstream tesc;
    std::ostringstream tese;
    std::ostringstream frag;

    vert << "#version 450\n"
         << "void main()\n"
         << "{\n"
         << "    gl_Position = vec4(gl_VertexIndex & 1u, (gl_VertexIndex >> 1u) & 1u, 0.0f, 1.0f);\n"
         << "}\n";

    if (m_testType == TEST_TESC_1)
    {
        tesc << "#version 450\n"
             << "layout(vertices = 1) out;\n"
             << "\n"
             << "layout(location = 0) patch out mat4 x;\n"
             << "\n"
             << "void main()\n"
             << "{\n"
             << "    x = mat4(\n"
             << "        0.53455, 0.47307, 0.34935, 0.28717,\n"
             << "        0.67195, 0.59992, 0.48213, 0.43678,\n"
             << "        0.76376, 0.6772, 0.55361, 0.5165,\n"
             << "        0.77996, 0.68862, 0.56187, 0.52611\n"
             << "    );\n"
             << "\n"
             << "    const mat4 m = mat4(\n"
             << "        vec4( -1.0, 3.0,-3.0, 1.0),\n"
             << "        vec4(  3.0,-6.0, 3.0, 0.0),\n"
             << "        vec4( -3.0, 3.0, 0.0, 0.0),\n"
             << "        vec4(  1.0, 0.0, 0.0, 0.0)\n"
             << "    );\n"
             << "\n"
             << "    x = m * x;\n"
             << "\n"
             << "    gl_TessLevelInner[0u] = 1.;\n"
             << "    gl_TessLevelInner[1u] = 1.;\n"
             << "    gl_TessLevelOuter[0u] = 1.;\n"
             << "    gl_TessLevelOuter[1u] = 1.;\n"
             << "    gl_TessLevelOuter[2u] = 1.;\n"
             << "    gl_TessLevelOuter[3u] = 1.;\n"
             << "}\n";

        tese << "#version 450\n"
             << "layout(quads, cw, fractional_odd_spacing) in;\n"
             << "\n"
             << "layout(location = 0) patch in mat4 x;\n"
             << "layout(location = 0) out mat4 x_fs;\n"
             << "\n"
             << "void main()\n"
             << "{\n"
             << "    x_fs = x;\n"
             << "    gl_Position = vec4(gl_TessCoord.xy * 2. - 1., 0, 1);\n"
             << "}\n";

        frag << "#version 450\n"
             << "\n"
             << "layout(location = 0) in mat4 x_fs;\n"
             << "layout(location = 0) out vec4 color;\n"
             << "\n"
             << "void main()\n"
             << "{\n"
             << "    const mat4 expect = mat4(\n"
             << "        0.12378, -0.18672, -0.18444, 0.53455,\n"
             << "        0.1182, -0.13728, -0.21609, 0.67195,\n"
             << "        0.12351, -0.11109, -0.25968, 0.76376,\n"
             << "        0.1264, -0.10623, -0.27402, 0.77996\n"
             << "    );\n"
             << "\n"
             << "    color = vec4(all(lessThan(abs(x_fs[0] - expect[0]), vec4(0.01))),\n"
             << "                 all(lessThan(abs(x_fs[1] - expect[1]), vec4(0.01))),\n"
             << "                 all(lessThan(abs(x_fs[2] - expect[2]), vec4(0.01))),\n"
             << "                 all(lessThan(abs(x_fs[3] - expect[3]), vec4(0.01))));\n"
             << "}\n";
    }
    else if (TEST_TESC_2)
    {
        tesc << "#version 450\n"
             << "layout(vertices = 1) out;\n"
             << "\n"
             << "layout(location = 0) patch out mat4 x;\n"
             << "layout(location = 5) patch out vec4 col0;\n"
             << "\n"
             << "void main()\n"
             << "{\n"
             << "    // Note: if |x| is not an |out| varying, the test passes.\n"
             << "    x = mat4(\n"
             << "        0.53455, 0.47307, 0.34935, 0.28717,\n"
             << "        0.67195, 0.59992, 0.48213, 0.43678,\n"
             << "        0.76376, 0.6772, 0.55361, 0.5165,\n"
             << "        0.77996, 0.68862, 0.56187, 0.52611\n"
             << "    );\n"
             << "\n"
             << "    const mat4 m = mat4(\n"
             << "        vec4( -1.0, 3.0,-3.0, 1.0),\n"
             << "        vec4(  3.0,-6.0, 3.0, 0.0),\n"
             << "        vec4( -3.0, 3.0, 0.0, 0.0),\n"
             << "        vec4(  1.0, 0.0, 0.0, 0.0)\n"
             << "    );\n"
             << "\n"
             << "    mat4 temp = x;\n"
             << "\n"
             << "    // Note: On the failing driver, commenting this line makes the test pass.\n"
             << "    // However, the output being tested is |temp|, assigned above, not |x|.\n"
             << "    x = m * x;\n"
             << "\n"
             << "    col0 = temp[0];\n"
             << "\n"
             << "    gl_TessLevelInner[0u] = 1.;\n"
             << "    gl_TessLevelInner[1u] = 1.;\n"
             << "    gl_TessLevelOuter[0u] = 1.;\n"
             << "    gl_TessLevelOuter[1u] = 1.;\n"
             << "    gl_TessLevelOuter[2u] = 1.;\n"
             << "    gl_TessLevelOuter[3u] = 1.;\n"
             << "}\n";

        tese << "#version 450\n"
             << "layout(quads, cw, fractional_odd_spacing) in;\n"
             << "\n"
             << "layout(location = 5) patch in vec4 col0;\n"
             << "\n"
             << "layout(location = 0) out vec4 col0_fs;\n"
             << "\n"
             << "void main()\n"
             << "{\n"
             << "    col0_fs = col0;\n"
             << "    gl_Position = vec4(gl_TessCoord.xy * 2. - 1., 0, 1);\n"
             << "}\n";

        frag << "#version 450\n"
             << "layout(location = 0) in vec4 col0_fs;\n"
             << "layout(location = 0) out vec4 color;\n"
             << "\n"
             << "void main()\n"
             << "{\n"
             << "    color = vec4(abs(col0_fs.x - 0.53455) < 0.01,\n"
             << "                abs(col0_fs.y - 0.47307) < 0.01,\n"
             << "                abs(col0_fs.z - 0.34935) < 0.01,\n"
             << "                abs(col0_fs.w - 0.28717) < 0.01);\n"
             << "}\n";
    }

    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
    programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tesc.str());
    programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(tese.str());
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

} // namespace

tcu::TestCaseGroup *createTessellationMatrixMultiplicationTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> matrixMultiplicationGroup(new tcu::TestCaseGroup(testCtx, "matrix_multiplication"));

    matrixMultiplicationGroup->addChild(new MatrixMultiplicationTestCase(testCtx, "tesc_1", TEST_TESC_1));
    matrixMultiplicationGroup->addChild(new MatrixMultiplicationTestCase(testCtx, "tesc_2", TEST_TESC_2));

    return matrixMultiplicationGroup.release();
}

} // namespace tessellation
} // namespace vkt
