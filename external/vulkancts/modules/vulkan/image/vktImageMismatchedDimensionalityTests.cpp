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
 * \brief Test that mismatched SPIR-V 'Dim' and descriptor imageType
          doesn't crash the driver.
 *//*--------------------------------------------------------------------*/

#include "vktImageMismatchedDimensionalityTests.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vktImageTexture.hpp"
#include "vktTestCase.hpp"
#include "tcuStringTemplate.hpp"

namespace vkt::image
{

enum class ImageDim
{
    Dim1D = 0,
    Dim2D,
    Dim3D,
    Cube,
    Rect,       // not supported in Vulkan
    Buffer,     // we test only image views mismatch
    SubpassData // this can't be tested because we need to use VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
};

struct TestParams
{
    const char *name;
    ImageDim view;
    ImageDim shader;
};

namespace
{

using namespace vk;
using namespace vkt;
using namespace tcu;

class MismatchedDimensionalityTestInstance : public TestInstance
{
public:
    MismatchedDimensionalityTestInstance(Context &context, const TestParams &params)
        : TestInstance(context)
        , m_testParams(params)
    {
    }
    ~MismatchedDimensionalityTestInstance() = default;

    tcu::TestStatus iterate(void) override;

protected:
    const TestParams m_testParams;
};

tcu::TestStatus MismatchedDimensionalityTestInstance::iterate(void)
{
    const auto &vk         = m_context.getDeviceInterface();
    const auto device      = m_context.getDevice();
    Allocator &allocator   = m_context.getDefaultAllocator();
    const auto imageSize   = 8u;
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto extent      = makeExtent3D(imageSize, imageSize, 1u);

    const auto clearValue(makeClearValueColor(tcu::Vec4(0.0f)));
    VkClearColorValue clearColorValue{{1.0f, 0.0f, 1.0f, 1.0f}};

    // fill structures that are needed for pipeline creation
    const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo = initVulkanStructure();
    const std::vector<VkViewport> viewport{makeViewport(extent)};
    const std::vector<VkRect2D> scissor{makeRect2D(extent)};

    // create image and view for color attachment
    VkImageViewType viewType          = VK_IMAGE_VIEW_TYPE_2D;
    VkImageCreateInfo imageCreateInfo = initVulkanStructure();
    imageCreateInfo.imageType         = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format            = colorFormat;
    imageCreateInfo.extent            = extent;
    imageCreateInfo.mipLevels         = 1;
    imageCreateInfo.arrayLayers       = 1;
    imageCreateInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.usage             = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    auto imageSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    ImageWithMemory attachmentImageWithMemory(vk, device, allocator, imageCreateInfo, vk::MemoryRequirement::Any);
    Move<VkImageView> attachmentImageView = makeImageView(vk, device, *attachmentImageWithMemory, VK_IMAGE_VIEW_TYPE_2D,
                                                          colorFormat, imageSubresourceRange);

    imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    auto descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    if (m_testParams.view == ImageDim::Dim1D)
    {
        viewType                      = VK_IMAGE_VIEW_TYPE_1D;
        imageCreateInfo.imageType     = VK_IMAGE_TYPE_1D;
        imageCreateInfo.extent.height = 1;
    }
    else if (m_testParams.view == ImageDim::Dim3D)
    {
        viewType                     = VK_IMAGE_VIEW_TYPE_3D;
        imageCreateInfo.imageType    = VK_IMAGE_TYPE_3D;
        imageCreateInfo.extent.depth = imageSize;
    }
    else if (m_testParams.view == ImageDim::Cube)
    {
        viewType                         = VK_IMAGE_VIEW_TYPE_CUBE;
        imageCreateInfo.imageType        = VK_IMAGE_TYPE_2D;
        imageCreateInfo.flags            = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        imageCreateInfo.arrayLayers      = 6;
        imageSubresourceRange.layerCount = 6;
    }

    // create second image that will be read in the shader
    ImageWithMemory secondImageWithMemory(vk, device, allocator, imageCreateInfo, vk::MemoryRequirement::Any);
    auto secondImageView =
        makeImageView(vk, device, *secondImageWithMemory, viewType, colorFormat, imageSubresourceRange);

    VkSamplerCreateInfo samplerCreateInfo = initVulkanStructure();
    Move<VkSampler> sampler               = createSampler(vk, device, &samplerCreateInfo);

    const auto beforeClearBarrier(makeImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, *secondImageWithMemory,
                                                         imageSubresourceRange));
    const auto afterClearBarrier(makeImageMemoryBarrier(
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, *secondImageWithMemory, imageSubresourceRange));

    // create descriptor pool, descriptor set layout and descriptor set
    const auto descriptorPool(DescriptorPoolBuilder()
                                  .addType(descriptorType)
                                  .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
    const auto descriptorSetLayout(DescriptorSetLayoutBuilder()
                                       .addBinding(descriptorType, 1u, VK_SHADER_STAGE_FRAGMENT_BIT, DE_NULL)
                                       .build(vk, device));
    const auto descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    const auto secondImageInfos =
        makeDescriptorImageInfo(*sampler, *secondImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    DescriptorSetUpdateBuilder()
        .write(*descriptorSet, 0, 0, 1, descriptorType, &secondImageInfos, 0, 0)
        .update(vk, device);

    const auto pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
    auto renderPass  = makeRenderPass(vk, device, colorFormat);
    auto framebuffer = makeFramebuffer(vk, device, *renderPass, *attachmentImageView, imageSize, imageSize);

    auto &bc(m_context.getBinaryCollection());
    const auto vertModule(createShaderModule(vk, device, bc.get("vert")));
    const auto fragModule(createShaderModule(vk, device, bc.get("frag")));
    const auto pipeline = makeGraphicsPipeline(vk, device, *pipelineLayout, *vertModule, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                               VK_NULL_HANDLE, *fragModule, *renderPass, viewport, scissor,
                                               VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0, 0, &vertexInputStateInfo);

    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    const auto cmdPool(
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
    const auto cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);

    // clear second image
    vk.cmdPipelineBarrier(*cmdBuffer, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0, 0, 0, 0, 1, &beforeClearBarrier);
    vk.cmdClearColorImage(*cmdBuffer, *secondImageWithMemory, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColorValue, 1,
                          &imageSubresourceRange);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0, 0,
                          0, 0, 1, &afterClearBarrier);

    // render triangle that covers whole color attachment
    const auto bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, scissor[0], 1, &clearValue);
    vk.cmdBindPipeline(*cmdBuffer, bindPoint, *pipeline);
    vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u, 0);
    vk.cmdDraw(*cmdBuffer, 3u, 1u, 0u, 0u);
    endRenderPass(vk, *cmdBuffer);
    endCommandBuffer(vk, *cmdBuffer);

    const VkQueue queue = getDeviceQueue(vk, device, queueFamilyIndex, 0);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    // mismatch between the SPIR-V Dim and the dimension of the underlying image view is
    // valid but returns an undefined value; we test that drivers accept this case and don't crash
    return tcu::TestStatus::pass("Pass");
}

class MismatchedDimensionalityTestCase : public TestCase
{
public:
    MismatchedDimensionalityTestCase(tcu::TestContext &testCtx, const TestParams &params)
        : TestCase(testCtx, params.name)
        , m_testParams(params)
    {
    }
    ~MismatchedDimensionalityTestCase() = default;

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

private:
    const TestParams m_testParams;
};

void MismatchedDimensionalityTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
    programCollection.glslSources.add("vert")
        << glu::VertexSource("#version 450\n"
                             "void main (void)\n"
                             "{\n"
                             "  const float x = -1.0 + 4.0 * ((gl_VertexIndex & 2)>>1);\n"
                             "  const float y = -1.0 + 4.0 * (gl_VertexIndex % 2);\n"
                             "  gl_Position = vec4(x, y, 0.0, 1.0);\n"
                             "}\n");

    struct ShaderConfiguration
    {
        std::map<std::string, std::string> specializationMap;
        ShaderConfiguration(const std::string &samplerType, const std::string &coordsType)
            : specializationMap{{"samplerType", samplerType}, {"coordsType", coordsType}}
        {
        }
    };

    const std::map<ImageDim, ShaderConfiguration> shaderPartsMap{
        {ImageDim::Dim1D, ShaderConfiguration("1D", "")},
        {ImageDim::Dim2D, ShaderConfiguration("2D", "vec2")},
        {ImageDim::Dim3D, ShaderConfiguration("3D", "vec3")},
        {ImageDim::Cube, ShaderConfiguration("Cube", "vec3")},
    };

    std::string fragTemplate = "#version 450\n"
                               "layout(binding = 0) uniform sampler${samplerType} data;\n"
                               "layout(location = 0) out highp vec4 fragColor;\n"
                               "void main (void)\n"
                               "{\n"
                               "  fragColor = texture(data, ${coordsType}(0.5));\n"
                               "}\n";

    auto &specializationMap = shaderPartsMap.at(m_testParams.shader).specializationMap;
    programCollection.glslSources.add("frag")
        << glu::FragmentSource(StringTemplate(fragTemplate).specialize(specializationMap));
}

TestInstance *MismatchedDimensionalityTestCase::createInstance(Context &context) const
{
    return new MismatchedDimensionalityTestInstance(context, m_testParams);
}

} // namespace

tcu::TestCaseGroup *createImageMismatchedDimensionalityTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> mainGroup(new tcu::TestCaseGroup(testCtx, "mismatched_dimensionality"));

    const TestParams paramsList[]{
        {"1d", ImageDim::Dim1D, ImageDim::Dim3D},
        {"2d", ImageDim::Dim2D, ImageDim::Dim1D},
        {"3d", ImageDim::Dim3D, ImageDim::Cube},
        {"cube", ImageDim::Cube, ImageDim::Dim2D},
    };

    for (const auto &parma : paramsList)
        mainGroup->addChild(new MismatchedDimensionalityTestCase(testCtx, parma));

    return mainGroup.release();
}

} // namespace vkt::image
