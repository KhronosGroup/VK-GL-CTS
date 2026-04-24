/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 ARM Ltd.
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
 */
/*!
 * \file
 * \brief Tensor in graphics pipeline tests
 */
/*--------------------------------------------------------------------*/

#include "vktTensorTests.hpp"

#include "vktTensorTestsUtil.hpp"
#include "vktTestCase.hpp"

#include "vktTestGroupUtil.hpp"
#include "shaders/vktTensorShaders.hpp"
#include "vkTensorMemoryUtil.hpp"

#include "vkBuilderUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "vkTensorWithMemory.hpp"
#include "vkTensorUtil.hpp"
#include "vkImageUtil.hpp"

#include "deMemory.h"

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "tcuFunctionLibrary.hpp"
#include "tcuPlatform.hpp"
#include "tcuResource.hpp"

#include <cstdint>
#include <iostream>

namespace vkt
{
namespace tensor
{

namespace
{

using namespace vk;

class TensorGraphicsPipelineAccessTestInstance : public TestInstance
{
public:
    TensorGraphicsPipelineAccessTestInstance(Context &testCtx, const VkExtent2D imageShape)
        : TestInstance(testCtx)
        , m_imageShape(imageShape)

    {
    }

    tcu::TestStatus iterate() override;

private:
    const VkExtent2D m_imageShape;
};

class TensorGraphicsPipelineAccessTestCase : public TestCase
{
public:
    TensorGraphicsPipelineAccessTestCase(tcu::TestContext &testCtx, const VkExtent2D imageShape)
        : TestCase(testCtx, std::to_string(imageShape.width) + "x" + std::to_string(imageShape.height))
        , m_imageShape(imageShape)
    {
    }

    TestInstance *createInstance(Context &ctx) const override
    {
        return new TensorGraphicsPipelineAccessTestInstance(ctx, m_imageShape);
    }

    void checkSupport(Context &context) const override
    {
        context.requireDeviceFunctionality("VK_ARM_tensors");

        if (3 > getTensorPhysicalDeviceProperties(context).maxTensorDimensionCount)
        {
            TCU_THROW(NotSupportedError, "Tensor dimension count is higher than what the implementation supports");
        }

        if (!formatSupportTensorFlags(context, VK_FORMAT_R8_UINT, VK_TENSOR_TILING_LINEAR_ARM,
                                      VK_FORMAT_FEATURE_2_TENSOR_SHADER_BIT_ARM))
        {
            TCU_THROW(NotSupportedError, "Format not supported");
        }

        if (!formatSupportTensorFlags(context, VK_FORMAT_R32_SINT, VK_TENSOR_TILING_LINEAR_ARM,
                                      VK_FORMAT_FEATURE_2_TENSOR_SHADER_BIT_ARM))
        {
            TCU_THROW(NotSupportedError, "Format not supported");
        }

        if (!deviceSupportsShaderTensorAccess(context))
        {
            TCU_THROW(NotSupportedError, "Device does not support shader tensor access");
        }

        if (!deviceSupportsShaderStagesTensorAccess(context, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT))
        {
            TCU_THROW(NotSupportedError,
                      "Device does not support shader tensor access in both fragment and vertex shader stages");
        }
    }

    void initPrograms(vk::SourceCollections &programCollection) const override
    {
        programCollection.glslSources.add("vertex") << glu::VertexSource(
            R"(
#version 450
#extension GL_ARM_tensors : require
#extension GL_EXT_shader_explicit_arithmetic_types : require

layout(set=0, binding=1) uniform tensorARM<int32_t, 2> tensor;

layout (constant_id = 0) const uint32_t imageShapeWidth  = 0;
layout (constant_id = 1) const uint32_t imageShapeHeight = 0;

vec2 imageShape = vec2(float(imageShapeWidth), float(imageShapeHeight));
vec2 imageShapeHalvedInv = 2.0 / imageShape;

void main() {
    int32_t pos_x, pos_y;
    tensorReadARM(tensor, uint[](gl_VertexIndex, 0), pos_x);
    tensorReadARM(tensor, uint[](gl_VertexIndex, 1), pos_y);
    const vec2 position = vec2(pos_x, pos_y);
    const vec2 clip_space_pos = position * imageShapeHalvedInv - 1.0;
    gl_Position = vec4(clip_space_pos, 0.0, 1.0);
}
    )");

        programCollection.glslSources.add("fragment") << glu::FragmentSource(
            R"(
#version 450
#extension GL_ARM_tensors : require
#extension GL_EXT_shader_explicit_arithmetic_types : require

layout(location = 0) out vec4 outColor;

layout(set=0, binding=0) uniform tensorARM<uint8_t, 3> tensor;

void main() {
    const uint coord_x = uint(gl_FragCoord.x);
    const uint coord_y = uint(gl_FragCoord.y);

    uint8_t tensorValue = uint8_t(0);
    tensorReadARM(tensor, uint[](coord_y, coord_x, 0), tensorValue);
    outColor = vec4(0.0, tensorValue, 0.0, 255.0);
}
    )");
    }

private:
    const VkExtent2D m_imageShape;
};

tcu::TestStatus TensorGraphicsPipelineAccessTestInstance::iterate()
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    /* Tensor with same shape as image that fragment shader fetches data from */
    static constexpr VkFormat formatFragmentTensor = VK_FORMAT_R8_UINT;
    const TensorDimensions shapeFragmentTensor{m_imageShape.height, m_imageShape.width, 1};
    const VkTensorDescriptionARM fragmentTensorDesc =
        makeTensorDescription(VK_TENSOR_TILING_LINEAR_ARM, formatFragmentTensor, shapeFragmentTensor, {},
                              VK_TENSOR_USAGE_SHADER_BIT_ARM | VK_TENSOR_USAGE_TRANSFER_DST_BIT_ARM);
    const VkTensorCreateInfoARM fragmentTensorCreateInfo = makeTensorCreateInfo(&fragmentTensorDesc);
    const TensorWithMemory fragmentTensor(vk, device, allocator, fragmentTensorCreateInfo, vk::MemoryRequirement::Any);
    const Unique<vk::VkTensorViewARM> fragmentTensorView(
        makeTensorView(vk, device, *fragmentTensor, formatFragmentTensor));

    StridedMemoryUtils<uint8_t> fragmentTensorData(shapeFragmentTensor, {});
    fragmentTensorData.fill();
    uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, fragmentTensor, fragmentTensorData.data(),
                   fragmentTensorData.memorySize());

    const std::vector<VkRect2D> rectangles = {{{50, 40}, {200, 200}}, {{350, 340}, {200, 200}}};

    static constexpr size_t vertexCountInRectangle = 6;
    static constexpr size_t dimensionsInVertex     = 2;

    /* Tensor with triangles forming rectangles that vertex shader fetches data from */
    const TensorDimensions shapeVertexTensor{static_cast<int64_t>(vertexCountInRectangle * rectangles.size()),
                                             static_cast<int64_t>(dimensionsInVertex)};
    const VkTensorDescriptionARM vertexTensorDesc =
        makeTensorDescription(VK_TENSOR_TILING_LINEAR_ARM, VK_FORMAT_R32_SINT, shapeVertexTensor, {},
                              VK_TENSOR_USAGE_SHADER_BIT_ARM | VK_TENSOR_USAGE_TRANSFER_DST_BIT_ARM);
    const VkTensorCreateInfoARM vertexTensorCreateInfo = makeTensorCreateInfo(&vertexTensorDesc);
    const TensorWithMemory vertexTensor(vk, device, allocator, vertexTensorCreateInfo, vk::MemoryRequirement::Any);
    const Unique<vk::VkTensorViewARM> vertexTensorView(makeTensorView(vk, device, *vertexTensor, VK_FORMAT_R32_SINT));
    {
        static constexpr size_t valuesInRectangle = vertexCountInRectangle * dimensionsInVertex;
        std::vector<int32_t> vertexTensorData(rectangles.size() * valuesInRectangle);

        for (size_t i = 0; i < rectangles.size(); ++i)
        {
            const VkRect2D &rectangle    = rectangles[i];
            const VkOffset2D upper_left  = {rectangle.offset.x, rectangle.offset.y};
            const VkOffset2D upper_right = {rectangle.offset.x + static_cast<int32_t>(rectangle.extent.width),
                                            rectangle.offset.y};
            const VkOffset2D lower_left  = {rectangle.offset.x,
                                            rectangle.offset.y + static_cast<int32_t>(rectangle.extent.height)};
            const VkOffset2D lower_right = {rectangle.offset.x + static_cast<int32_t>(rectangle.extent.width),
                                            rectangle.offset.y + static_cast<int32_t>(rectangle.extent.height)};

            vertexTensorData[i * valuesInRectangle + 0]  = upper_left.x;
            vertexTensorData[i * valuesInRectangle + 1]  = upper_left.y;
            vertexTensorData[i * valuesInRectangle + 2]  = upper_right.x;
            vertexTensorData[i * valuesInRectangle + 3]  = upper_right.y;
            vertexTensorData[i * valuesInRectangle + 4]  = lower_right.x;
            vertexTensorData[i * valuesInRectangle + 5]  = lower_right.y;
            vertexTensorData[i * valuesInRectangle + 6]  = upper_left.x;
            vertexTensorData[i * valuesInRectangle + 7]  = upper_left.y;
            vertexTensorData[i * valuesInRectangle + 8]  = lower_right.x;
            vertexTensorData[i * valuesInRectangle + 9]  = lower_right.y;
            vertexTensorData[i * valuesInRectangle + 10] = lower_left.x;
            vertexTensorData[i * valuesInRectangle + 11] = lower_left.y;
        }

        uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, vertexTensor, vertexTensorData.data(),
                       vertexTensorData.size() * sizeof(vertexTensorData[0]));
    }

    // Image to render into
    static constexpr vk::VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UINT;
    ImageWithBuffer image(vk, device, allocator, {m_imageShape.width, m_imageShape.height, 1}, imageFormat,
                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_TYPE_2D);

    // Descriptor set
    vk::DescriptorSetLayoutBuilder descriptorSetLayoutBuilder;
    descriptorSetLayoutBuilder.addSingleIndexedBinding(VK_DESCRIPTOR_TYPE_TENSOR_ARM, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
    descriptorSetLayoutBuilder.addSingleIndexedBinding(VK_DESCRIPTOR_TYPE_TENSOR_ARM, VK_SHADER_STAGE_VERTEX_BIT, 1);

    const vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(descriptorSetLayoutBuilder.build(vk, device));

    vk::DescriptorPoolBuilder descriptorPoolBuilder;
    descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_TENSOR_ARM, 2);
    const vk::Unique<vk::VkDescriptorPool> descriptorPool(
        descriptorPoolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                   *descriptorPool, 1u, &descriptorSetLayout.get()};

    const vk::Unique<vk::VkDescriptorSet> descriptorSet(allocateDescriptorSet(vk, device, &allocInfo));

    {
        const VkTensorViewARM fragmentTensorWriteView = fragmentTensorView.get();

        VkWriteDescriptorSetTensorARM fragmentTensorWrite = initVulkanStructure();
        fragmentTensorWrite.tensorViewCount               = 1;
        fragmentTensorWrite.pTensorViews                  = &fragmentTensorWriteView;

        const VkTensorViewARM vertexTensorWriteView = vertexTensorView.get();

        VkWriteDescriptorSetTensorARM vertexTensorWrite = initVulkanStructure();
        vertexTensorWrite.tensorViewCount               = 1;
        vertexTensorWrite.pTensorViews                  = &vertexTensorWriteView;

        vk::DescriptorSetUpdateBuilder descriptorSetUpdateBuilder;
        descriptorSetUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                                               VK_DESCRIPTOR_TYPE_TENSOR_ARM, &fragmentTensorWrite);
        descriptorSetUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                                               VK_DESCRIPTOR_TYPE_TENSOR_ARM, &vertexTensorWrite);
        descriptorSetUpdateBuilder.update(vk, device);
    }

    // Vertex shader
    const ProgramBinary &programBinaryVertexShader = m_context.getBinaryCollection().get("vertex");
    const Unique<VkShaderModule> vertexShaderModule(createShaderModule(vk, device, programBinaryVertexShader, 0u));

    // Fragment shader
    const ProgramBinary &programBinaryFragmentShader = m_context.getBinaryCollection().get("fragment");
    const Unique<VkShaderModule> fragmentShaderModule(createShaderModule(vk, device, programBinaryFragmentShader, 0u));

    // Graphics pipeline
    VkPipelineLayoutCreateInfo pipelineLayoutParams = initVulkanStructure();
    pipelineLayoutParams.setLayoutCount             = 1;
    pipelineLayoutParams.pSetLayouts                = &*descriptorSetLayout;

    const vk::Unique<vk::VkPipelineLayout> pipelineLayout(createPipelineLayout(vk, device, &pipelineLayoutParams));

    struct SpecializationData
    {
        uint32_t imageWidth;
        uint32_t imageHeight;
    };

    const SpecializationData specializationData = {m_imageShape.width, m_imageShape.height};

    const std::vector<VkSpecializationMapEntry> specializationEntries = {
        {0, offsetof(SpecializationData, imageWidth), sizeof(SpecializationData::imageWidth)},
        {1, offsetof(SpecializationData, imageHeight), sizeof(SpecializationData::imageHeight)},
    };

    const VkSpecializationInfo specializationInfo{static_cast<uint32_t>(specializationEntries.size()),
                                                  specializationEntries.data(), sizeof(specializationData),
                                                  &specializationData};

    const VkPipelineShaderStageCreateInfo vertexStageInfo =
        makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, *vertexShaderModule, &specializationInfo);

    const VkPipelineShaderStageCreateInfo fragmentStageInfo =
        makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, *fragmentShaderModule);

    const std::vector<VkPipelineShaderStageCreateInfo> shaderStages = {vertexStageInfo, fragmentStageInfo};

    const VkPipelineVertexInputStateCreateInfo vertexInput = initVulkanStructure();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = initVulkanStructure();
    inputAssembly.topology                               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    const VkViewport viewport = makeViewport(m_imageShape.width, m_imageShape.height);

    const VkRect2D scissor{{0, 0}, m_imageShape};

    VkPipelineViewportStateCreateInfo viewportState = initVulkanStructure();
    viewportState.viewportCount                     = 1;
    viewportState.pViewports                        = &viewport;
    viewportState.scissorCount                      = 1;
    viewportState.pScissors                         = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = initVulkanStructure();
    rasterizer.polygonMode                            = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth                              = 1.0f;
    rasterizer.cullMode                               = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace                              = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling = initVulkanStructure();
    multisampling.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending = initVulkanStructure();
    colorBlending.attachmentCount                     = 1;
    colorBlending.pAttachments                        = &colorBlendAttachment;

    VkPipelineRenderingCreateInfo pipelineRenderingInfo = initVulkanStructure();
    pipelineRenderingInfo.colorAttachmentCount          = 1;
    pipelineRenderingInfo.pColorAttachmentFormats       = &imageFormat;

    VkGraphicsPipelineCreateInfo pipelineInfo = initVulkanStructure();
    pipelineInfo.stageCount                   = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages                      = shaderStages.data();

    pipelineInfo.pVertexInputState   = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pColorBlendState    = &colorBlending;

    pipelineInfo.layout = *pipelineLayout;

    pipelineInfo.pNext = &pipelineRenderingInfo;

    pipelineInfo.renderPass = VK_NULL_HANDLE;
    pipelineInfo.subpass    = 0;

    const vk::Unique<vk::VkPipeline> pipeline(
        createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &pipelineInfo, nullptr));

    const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);

    // Transition color attachment into the expected layout
    {
        const VkImageSubresourceRange imageRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        const VkImageMemoryBarrier2 initialLayoutBarrier = makeImageMemoryBarrier2(
            VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            image.getImage(), imageRange);

        VkDependencyInfo dependencyInfo        = initVulkanStructure();
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers    = &initialLayoutBarrier;

        vk.cmdPipelineBarrier2(*cmdBuffer, &dependencyInfo);
    }

    VkRenderingAttachmentInfo attachmentInfo = initVulkanStructure();
    attachmentInfo.imageView                 = image.getImageView();
    attachmentInfo.imageLayout               = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentInfo.resolveMode               = VK_RESOLVE_MODE_NONE;
    attachmentInfo.loadOp                    = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentInfo.storeOp                   = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentInfo.clearValue.color          = {{1.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderingInfo renderingInfo      = initVulkanStructure();
    renderingInfo.renderArea.offset    = {0, 0};
    renderingInfo.renderArea.extent    = m_imageShape;
    renderingInfo.layerCount           = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments    = &attachmentInfo;

    // Render
    vk.cmdBeginRendering(*cmdBuffer, &renderingInfo);

    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &descriptorSet.get(),
                             0u, nullptr);
    vk.cmdDraw(*cmdBuffer, static_cast<uint32_t>(rectangles.size() * vertexCountInRectangle), 1, 0, 0);

    vk.cmdEndRendering(*cmdBuffer);

    // Copy from color attachment to output buffer
    copyImageToBuffer(vk, *cmdBuffer, image.getImage(), image.getBuffer(),
                      {static_cast<int>(m_imageShape.width), static_cast<int>(m_imageShape.height)});

    endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    Allocation &bufferAlloc = image.getBufferAllocation();
    invalidateAlloc(vk, device, bufferAlloc);

    const uint8_t *const ptr = static_cast<const uint8_t *>(bufferAlloc.getHostPtr());

    for (size_t y = 0; y < m_imageShape.height; ++y)
    {
        for (size_t x = 0; x < m_imageShape.width; ++x)
        {
            bool pixelShouldBeInRectangle = false;
            for (const VkRect2D &rectangle : rectangles)
            {
                if (x >= static_cast<size_t>(rectangle.offset.x) &&
                    x < static_cast<size_t>(rectangle.offset.x) + rectangle.extent.width &&
                    y >= static_cast<size_t>(rectangle.offset.y) &&
                    y < static_cast<size_t>(rectangle.offset.y) + rectangle.extent.width)
                {
                    pixelShouldBeInRectangle = true;
                    break;
                }
            }

            const uint8_t *const pixel = &ptr[4 * sizeof(uint8_t) * (y * m_imageShape.width + x)];
            if (pixelShouldBeInRectangle)
            {
                const uint8_t expectedValue = fragmentTensorData[y * m_imageShape.width + x];
                if (!(pixel[0] == 0 && pixel[1] == expectedValue && pixel[2] == 0 && pixel[3] == 255))
                {
                    std::ostringstream msg;
                    msg << "Comparison failed inside rectangle at image coordinate ( " << x << ", " << y << "): ";
                    msg << "image = (" << static_cast<size_t>(pixel[0]) << ", " << static_cast<size_t>(pixel[1]) << ", "
                        << static_cast<size_t>(pixel[2]) << ", " << static_cast<size_t>(pixel[3]) << "), ";
                    msg << "expected = (" << static_cast<size_t>(0) << ", " << static_cast<size_t>(expectedValue)
                        << ", " << static_cast<size_t>(0) << ", " << static_cast<size_t>(255) << ")";
                    return tcu::TestStatus::fail(msg.str());
                }
            }
            else
            {
                if (!(pixel[0] == 255 && pixel[1] == 0 && pixel[2] == 0 && pixel[3] == 255))
                {
                    std::ostringstream msg;
                    msg << "Comparison failed outside rectangle at image coordinate ( " << x << ", " << y << "): ";
                    msg << "image = (" << static_cast<size_t>(pixel[0]) << ", " << static_cast<size_t>(pixel[1]) << ", "
                        << static_cast<size_t>(pixel[2]) << ", " << static_cast<size_t>(pixel[3]) << "), ";
                    msg << "expected = (" << static_cast<size_t>(255) << ", " << static_cast<size_t>(0) << ", "
                        << static_cast<size_t>(0) << ", " << static_cast<size_t>(255) << ")";
                    return tcu::TestStatus::fail(msg.str());
                }
            }
        }
    }

    return tcu::TestStatus::pass("Tensor test succeeded");
}

void addGraphicsPipelineAccessTest(tcu::TestCaseGroup &testCaseGroup)
{
    testCaseGroup.addChild(new TensorGraphicsPipelineAccessTestCase(testCaseGroup.getTestContext(), {600, 600}));
    testCaseGroup.addChild(new TensorGraphicsPipelineAccessTestCase(testCaseGroup.getTestContext(), {1280, 720}));
    testCaseGroup.addChild(new TensorGraphicsPipelineAccessTestCase(testCaseGroup.getTestContext(), {567, 891}));
    testCaseGroup.addChild(new TensorGraphicsPipelineAccessTestCase(testCaseGroup.getTestContext(), {891, 567}));
}

} // namespace

tcu::TestCaseGroup *createGraphicsPipelineTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(
        new tcu::TestCaseGroup(testCtx, "graphics_pipeline", "Tensor graphics pipeline tests"));

    addGraphicsPipelineAccessTest(*group);

    return group.release();
}

} // namespace tensor
} // namespace vkt
