/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 Google Inc.
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
 * \brief YCbCr misc tests
 *//*--------------------------------------------------------------------*/

#include "vktYCbCrMiscTests.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktYCbCrUtil.hpp"

#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"

#include <string>
#include <vector>
#include "vkBuilderUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkImageUtil.hpp"

namespace vkt
{
namespace ycbcr
{

using namespace vk;

class RelaxedPrecisionTestInstance : public vkt::TestInstance
{
public:
    RelaxedPrecisionTestInstance(vkt::Context &context) : vkt::TestInstance(context)
    {
    }

private:
    tcu::TestStatus iterate(void);
};

tcu::TestStatus RelaxedPrecisionTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &alloc                = m_context.getDefaultAllocator();

    const tcu::IVec2 renderSize(256, 256);
    const VkFormat format = VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM;

    const VkSamplerYcbcrConversionCreateInfo conversionInfo = {
        VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
        nullptr,
        format,
        VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY,
        VK_SAMPLER_YCBCR_RANGE_ITU_FULL,
        {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        VK_CHROMA_LOCATION_COSITED_EVEN,
        VK_CHROMA_LOCATION_COSITED_EVEN,
        VK_FILTER_NEAREST,
        VK_FALSE,
    };

    Move<VkSamplerYcbcrConversion> ycbcrConversion = createSamplerYcbcrConversion(vk, device, &conversionInfo);

    const VkSamplerYcbcrConversionInfo samplerConversionInfo{VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO, nullptr,
                                                             *ycbcrConversion};

    VkSamplerCreateInfo samplerCreateInfo = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        &samplerConversionInfo,
        0u,
        VK_FILTER_NEAREST,                       // magFilter
        VK_FILTER_NEAREST,                       // minFilter
        VK_SAMPLER_MIPMAP_MODE_NEAREST,          // mipmapMode
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // addressModeU
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // addressModeV
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // addressModeW
        0.0f,                                    // mipLodBias
        VK_FALSE,                                // anisotropyEnable
        1.0f,                                    // maxAnisotropy
        VK_FALSE,                                // compareEnable
        VK_COMPARE_OP_ALWAYS,                    // compareOp
        0.0f,                                    // minLod
        0.0f,                                    // maxLod
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, // borderColor
        VK_FALSE,                                // unnormalizedCoords
    };

    const Unique<VkSampler> sampler(createSampler(vk, device, &samplerCreateInfo));

    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        (VkImageCreateFlags)0u,
        VK_IMAGE_TYPE_2D,
        format,
        makeExtent3D(renderSize.x(), renderSize.y(), 1u),
        1u,
        1u,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    ImageWithMemory image(vk, device, alloc, imageCreateInfo, MemoryRequirement::Any);

    const VkImageViewCreateInfo viewInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        &samplerConversionInfo,
        (VkImageViewCreateFlags)0,
        *image,
        VK_IMAGE_VIEW_TYPE_2D,
        format,
        {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u},
    };

    Move<VkImageView> imageView = createImageView(vk, device, &viewInfo);

    const VkDescriptorPoolSize poolSize       = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3u};
    const VkDescriptorPoolCreateInfo poolInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        nullptr,
        (VkDescriptorPoolCreateFlags)VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        1u,
        1u,
        &poolSize};

    const VkDescriptorSetLayoutBinding binding = makeDescriptorSetLayoutBinding(
        0u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u, VK_SHADER_STAGE_FRAGMENT_BIT, &*sampler);
    const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // VkStructureType						sType;
        nullptr,                                             // const void*							pNext;
        (VkDescriptorSetLayoutCreateFlags)0u,                // VkDescriptorSetLayoutCreateFlags	    flags;
        1u,                                                  // uint32_t							    bindingCount;
        &binding                                             // const VkDescriptorSetLayoutBinding*	pBindings;
    };
    const Move<VkDescriptorSetLayout> descriptorSetLayout =
        createDescriptorSetLayout(vk, device, &descriptorSetLayoutCreateInfo);
    const Unique<VkDescriptorPool> descPool(createDescriptorPool(vk, device, &poolInfo));
    const VkDescriptorSetAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, *descPool, 1u, &*descriptorSetLayout,
    };
    const Unique<VkDescriptorSet> descriptorSet(allocateDescriptorSet(vk, device, &allocInfo));

    const VkDescriptorImageInfo imageInfo      = {*sampler, *imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    const VkWriteDescriptorSet descriptorWrite = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        nullptr,
        *descriptorSet,
        0u, // dstBinding
        0u, // dstArrayElement
        1u, // descriptorCount
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        &imageInfo,
        nullptr,
        nullptr,
    };

    vk.updateDescriptorSets(device, 1u, &descriptorWrite, 0u, nullptr);

    const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));

    const VkImageCreateInfo fbImageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        (VkImageCreateFlags)0u,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R8G8B8A8_UNORM,
        makeExtent3D(renderSize.x(), renderSize.y(), 1u),
        1u,
        1u,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    ImageWithMemory fbImage(vk, device, alloc, fbImageCreateInfo, MemoryRequirement::Any);

    const VkImageViewCreateInfo fbViewInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        nullptr,
        (VkImageViewCreateFlags)0,
        *fbImage,
        VK_IMAGE_VIEW_TYPE_2D,
        VK_FORMAT_R8G8B8A8_UNORM,
        {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u},
    };

    Move<VkImageView> fbImageView = createImageView(vk, device, &fbViewInfo);
    const Move<VkRenderPass> renderPass =
        makeRenderPass(vk, device, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
    const Move<VkFramebuffer> framebuffer =
        makeFramebuffer(vk, device, *renderPass, *fbImageView, renderSize.x(), renderSize.y());

    const Unique<VkShaderModule> vertShaderModule(
        createShaderModule(vk, device, m_context.getBinaryCollection().get("vert")));
    const Unique<VkShaderModule> fragShaderModule(
        createShaderModule(vk, device, m_context.getBinaryCollection().get("frag")));

    const std::vector<VkViewport> viewports(1, makeViewport(renderSize));
    const std::vector<VkRect2D> scissors(1, makeRect2D(renderSize));

    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();
    auto pipeline = makeGraphicsPipeline(vk, device, *pipelineLayout, *vertShaderModule, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                         VK_NULL_HANDLE, *fragShaderModule, *renderPass, viewports, scissors,
                                         VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u, 0u, &vertexInputStateCreateInfo);

    const vk::Unique<vk::VkCommandPool> cmdPool(
        createCommandPool(vk, device, (vk::VkCommandPoolCreateFlags)0, queueFamilyIndex));
    const vk::Unique<vk::VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);
    VkImageMemoryBarrier imageMemoryBarrier =
        makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, *image,
                               makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0u,
                          nullptr, 0u, nullptr, 1u, &imageMemoryBarrier);
    beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(renderSize.x(), renderSize.y()));
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u,
                             nullptr);
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
    vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);
    endRenderPass(vk, *cmdBuffer);
    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    return tcu::TestStatus::pass("Pass");
}

class RelaxedPrecisionTestCase : public vkt::TestCase
{
public:
    RelaxedPrecisionTestCase(tcu::TestContext &context, const char *name) : TestCase(context, name)
    {
    }

private:
    vkt::TestInstance *createInstance(vkt::Context &context) const
    {
        return new RelaxedPrecisionTestInstance(context);
    }
    void checkSupport(vkt::Context &context) const;
    void initPrograms(vk::SourceCollections &programCollection) const;
};

void RelaxedPrecisionTestCase::checkSupport(vkt::Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_sampler_ycbcr_conversion");
}

void RelaxedPrecisionTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vert;
    vert << "#version 450\n"
         << "layout (location=0) out vec2 texCoord;\n"
         << "void main()\n"
         << "{\n"
         << "    texCoord = vec2(gl_VertexIndex & 1u, (gl_VertexIndex >> 1u) & 1u);"
         << "    gl_Position = vec4(texCoord * 2.0f - 1.0f, 0.0f, 1.0f);\n"
         << "}\n";

    std::ostringstream frag;
    frag << "               OpCapability Shader\n"
         << "          %1 = OpExtInstImport \"GLSL.std.450\"\n"
         << "               OpMemoryModel Logical GLSL450\n"
         << "               OpEntryPoint Fragment %main \"main\" %sk_FragColor\n"
         << "               OpExecutionMode %main OriginUpperLeft\n"
         << "               OpName %sk_FragColor \"sk_FragColor\"\n"
         << "               OpName %t \"t\"\n"
         << "               OpName %main \"main\"\n"
         << "               OpName %c \"c\"\n"
         << "               OpDecorate %sk_FragColor RelaxedPrecision\n"
         << "               OpDecorate %sk_FragColor Location 0\n"
         << "               OpDecorate %sk_FragColor Index 0\n"
         << "               OpDecorate %t RelaxedPrecision\n"
         << "               OpDecorate %t Binding 0\n"
         << "               OpDecorate %t DescriptorSet 0\n"
         << "               OpDecorate %c RelaxedPrecision\n"
         << "               OpDecorate %16 RelaxedPrecision\n"
         << "               OpDecorate %17 RelaxedPrecision\n"
         << "               OpDecorate %21 RelaxedPrecision\n"
         << "               OpDecorate %22 RelaxedPrecision\n"
         << "               OpDecorate %26 RelaxedPrecision\n"
         << "      %float = OpTypeFloat 32\n"
         << "    %v4float = OpTypeVector %float 4\n"
         << "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
         << "%sk_FragColor = OpVariable %_ptr_Output_v4float Output\n"
         << "          %8 = OpTypeImage %float 2D 0 0 0 1 Unknown\n"
         << "          %9 = OpTypeSampledImage %8\n"
         << "%_ptr_UniformConstant_9 = OpTypePointer UniformConstant %9\n"
         << "          %t = OpVariable %_ptr_UniformConstant_9 UniformConstant\n"
         << "       %void = OpTypeVoid\n"
         << "         %12 = OpTypeFunction %void\n"
         << "%_ptr_Function_v4float = OpTypePointer Function %v4float\n"
         << "    %float_0 = OpConstant %float 0\n"
         << "    %v2float = OpTypeVector %float 2\n"
         << "         %20 = OpConstantComposite %v2float %float_0 %float_0\n"
         << "    %float_1 = OpConstant %float 1\n"
         << "    %v3float = OpTypeVector %float 3\n"
         << "         %25 = OpConstantComposite %v3float %float_1 %float_1 %float_1\n"
         << "       %main = OpFunction %void None %12\n"
         << "         %13 = OpLabel\n"
         << "          %c = OpVariable %_ptr_Function_v4float Function\n"
         << "         %17 = OpLoad %9 %t\n"
         << "         %16 = OpImageSampleImplicitLod %v4float %17 %20\n"
         << "               OpStore %c %16\n"
         << "         %22 = OpLoad %9 %t\n"
         << "         %21 = OpImageSampleProjImplicitLod %v4float %22 %25\n"
         << "         %26 = OpFMul %v4float %16 %21\n"
         << "               OpStore %sk_FragColor %26\n"
         << "               OpReturn\n"
         << "               OpFunctionEnd\n";

    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
    programCollection.spirvAsmSources.add("frag") << frag.str();
}

tcu::TestCaseGroup *createMiscTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> miscGroup(new tcu::TestCaseGroup(testCtx, "misc"));

    miscGroup->addChild(new RelaxedPrecisionTestCase(testCtx, "relaxed_precision"));

    return miscGroup.release();
}

} // namespace ycbcr
} // namespace vkt
