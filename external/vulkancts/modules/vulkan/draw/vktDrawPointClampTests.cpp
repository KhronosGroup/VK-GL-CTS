/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
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
 * \brief glPointSize clamp test
 *//*--------------------------------------------------------------------*/

#include "vktDrawPointClampTests.hpp"

#include "vktTestCaseUtil.hpp"

#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include "deUniquePtr.hpp"
#include "deMath.h"

namespace vkt
{
namespace Draw
{

using namespace vk;
using namespace de;

struct Vertex
{
    tcu::Vec4 pos;
    tcu::Vec4 color;
};

void createPointSizeClampProgs(SourceCollections &dst)
{
    std::stringstream vertShader;
    vertShader << "#version 450\n"
               << "layout(location = 0) in vec4 in_position;\n"
               << "layout(location = 1) in vec4 in_color;\n"
               << "layout(push_constant) uniform pointSizeBlk {\n"
               << "    float psize;\n"
               << "} in_pointSize;\n"
               << "layout(location = 0) out vec4 out_color;\n"

               << "out gl_PerVertex {\n"
               << "    vec4  gl_Position;\n"
               << "    float gl_PointSize;\n"
               << "};\n"
               << "void main() {\n"
               << "    gl_PointSize = in_pointSize.psize;\n"
               << "    gl_Position  = in_position;\n"
               << "    out_color    = in_color;\n"
               << "}\n";

    std::stringstream fragShader;
    fragShader << "#version 450\n"
               << "layout(location = 0) flat in vec4 in_color;\n"
               << "layout(location = 0) out vec4 out_color;\n"
               << "void main()\n"
               << "{\n"
               << "    out_color = in_color;\n"
               << "}\n";

    dst.glslSources.add("vert") << glu::VertexSource(vertShader.str());
    dst.glslSources.add("frag") << glu::FragmentSource(fragShader.str());
}

tcu::TestStatus renderPointSizeClampTest(Context &context)
{
    const VkDevice vkDevice          = context.getDevice();
    const DeviceInterface &vk        = context.getDeviceInterface();
    const VkQueue queue              = context.getUniversalQueue();
    const uint32_t queueFamilyIndex  = context.getUniversalQueueFamilyIndex();
    const VkPhysicalDevice phyDevice = context.getPhysicalDevice();
    SimpleAllocator memAlloc(vk, vkDevice,
                             getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), phyDevice));
    const VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const tcu::Vec4 clearColor(0.0f, 1.0f, 0.0f, 1.0f);
    const tcu::Vec4 pointColor(0.0f, 0.0f, 0.0f, 0.0f);

    const VkPhysicalDeviceFeatures features = getPhysicalDeviceFeatures(context.getInstanceInterface(), phyDevice);
    if (!features.largePoints)
        throw tcu::NotSupportedError("Large points not supported");

    VkPhysicalDeviceProperties phyDevProps;
    context.getInstanceInterface().getPhysicalDeviceProperties(phyDevice, &phyDevProps);

    //float minPointSizeRange = phyDevProps.limits.pointSizeRange[0];
    float maxPointSizeRange = phyDevProps.limits.pointSizeRange[1];

    uint32_t fbWidthSize = deCeilFloatToInt32(maxPointSizeRange * 0.5f) + 1;

    const tcu::IVec2 renderSize(fbWidthSize, 1);

    const float testPointSize = deFloatFloor(maxPointSizeRange * 2.0f);

    VkPushConstantRange pcPointSize;
    pcPointSize.offset     = 0;
    pcPointSize.size       = sizeof(float);
    pcPointSize.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    float pxCenter = (float)fbWidthSize - 0.25f;

    float testPointXCoord = ((2.0f * pxCenter) / (float)fbWidthSize) - 1.0f;

    const struct Vertex vertices[] = {{tcu::Vec4(testPointXCoord, 0.0f, 0.0f, 1.0f), pointColor}};

    const VkBufferCreateInfo vertexBufferParams = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // sType
        DE_NULL,                              // pNext
        0u,                                   // flags
        (VkDeviceSize)sizeof(vertices),       // size
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,    // usage
        VK_SHARING_MODE_EXCLUSIVE,            // sharingMode
        1u,                                   // queueFamilyIndexCount
        &queueFamilyIndex,                    // pQueueFamilyIndices
    };
    const Unique<VkBuffer> vertexBuffer(createBuffer(vk, vkDevice, &vertexBufferParams));
    const UniquePtr<Allocation> vertexBufferMemory(
        memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *vertexBuffer), MemoryRequirement::HostVisible));

    VK_CHECK(
        vk.bindBufferMemory(vkDevice, *vertexBuffer, vertexBufferMemory->getMemory(), vertexBufferMemory->getOffset()));

    const VkDeviceSize imageSizeBytes              = (VkDeviceSize)(sizeof(uint32_t) * renderSize.x() * renderSize.y());
    const VkBufferCreateInfo readImageBufferParams = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // sType
        DE_NULL,                              // pNext
        (VkBufferCreateFlags)0u,              // flags
        imageSizeBytes,                       // size
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,     // usage
        VK_SHARING_MODE_EXCLUSIVE,            // sharingMode
        1u,                                   // queueFamilyIndexCount
        &queueFamilyIndex,                    // pQueueFamilyIndices
    };
    const Unique<VkBuffer> readImageBuffer(createBuffer(vk, vkDevice, &readImageBufferParams));
    const UniquePtr<Allocation> readImageBufferMemory(
        memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *readImageBuffer), MemoryRequirement::HostVisible));

    VK_CHECK(vk.bindBufferMemory(vkDevice, *readImageBuffer, readImageBufferMemory->getMemory(),
                                 readImageBufferMemory->getOffset()));

    const VkImageCreateInfo imageParams = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                   // sType
        DE_NULL,                                                               // pNext
        0u,                                                                    // flags
        VK_IMAGE_TYPE_2D,                                                      // imageType
        colorFormat,                                                           // format
        {(uint32_t)renderSize.x(), (uint32_t)renderSize.y(), 1},               // extent
        1u,                                                                    // mipLevels
        1u,                                                                    // arraySize
        VK_SAMPLE_COUNT_1_BIT,                                                 // samples
        VK_IMAGE_TILING_OPTIMAL,                                               // tiling
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // usage
        VK_SHARING_MODE_EXCLUSIVE,                                             // sharingMode
        1u,                                                                    // queueFamilyIndexCount
        &queueFamilyIndex,                                                     // pQueueFamilyIndices
        VK_IMAGE_LAYOUT_UNDEFINED,                                             // initialLayout
    };

    const Unique<VkImage> image(createImage(vk, vkDevice, &imageParams));
    const UniquePtr<Allocation> imageMemory(
        memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *image), MemoryRequirement::Any));

    VK_CHECK(vk.bindImageMemory(vkDevice, *image, imageMemory->getMemory(), imageMemory->getOffset()));

    const Unique<VkRenderPass> renderPass(makeRenderPass(vk, vkDevice, colorFormat));

    const VkImageViewCreateInfo colorAttViewParams = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,                                                         // sType
        DE_NULL,                                                                                          // pNext
        0u,                                                                                               // flags
        *image,                                                                                           // image
        VK_IMAGE_VIEW_TYPE_2D,                                                                            // viewType
        colorFormat,                                                                                      // format
        {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A}, // components
        {
            VK_IMAGE_ASPECT_COLOR_BIT, // aspectMask
            0u,                        // baseMipLevel
            1u,                        // levelCount
            0u,                        // baseArrayLayer
            1u,                        // layerCount
        },                             // subresourceRange
    };
    const Unique<VkImageView> colorAttView(createImageView(vk, vkDevice, &colorAttViewParams));

    // Pipeline layout
    const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // sType
        DE_NULL,                                       // pNext
        (vk::VkPipelineLayoutCreateFlags)0,
        0u,           // setLayoutCount
        DE_NULL,      // pSetLayouts
        1u,           // pushConstantRangeCount
        &pcPointSize, // pPushConstantRanges
    };
    const Unique<VkPipelineLayout> pipelineLayout(createPipelineLayout(vk, vkDevice, &pipelineLayoutParams));

    // Shaders
    const Unique<VkShaderModule> vertShaderModule(
        createShaderModule(vk, vkDevice, context.getBinaryCollection().get("vert"), 0));
    const Unique<VkShaderModule> fragShaderModule(
        createShaderModule(vk, vkDevice, context.getBinaryCollection().get("frag"), 0));

    // Pipeline
    const std::vector<VkViewport> viewports(1, makeViewport(renderSize));
    const std::vector<VkRect2D> scissors(1, makeRect2D(renderSize));

    const VkVertexInputBindingDescription vertexInputBindingDescription = {
        0u,                          // uint32_t             binding
        sizeof(Vertex),              // uint32_t             stride
        VK_VERTEX_INPUT_RATE_VERTEX, // VkVertexInputRate    inputRate
    };

    const VkVertexInputAttributeDescription vertexInputAttributeDescriptionPos = {
        0u,                            // uint32_t    location
        0u,                            // uint32_t    binding
        VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat    format
        offsetof(Vertex, pos)          // uint32_t    offset
    };

    const VkVertexInputAttributeDescription vertexInputAttributeDescriptionColor = {
        1u,                     // uint32_t    location
        0u,                     // uint32_t    binding
        colorFormat,            // VkFormat    format
        offsetof(Vertex, color) // uint32_t    offset
    };

    std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions;
    vertexInputAttributeDescriptions.resize(2);
    vertexInputAttributeDescriptions[0] = vertexInputAttributeDescriptionPos;
    vertexInputAttributeDescriptions[1] = vertexInputAttributeDescriptionColor;

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                             sType
        DE_NULL,                                                   // const void*                                 pNext
        (VkPipelineVertexInputStateCreateFlags)0,                  // VkPipelineVertexInputStateCreateFlags       flags
        1u,                             // uint32_t                                    vertexBindingDescriptionCount
        &vertexInputBindingDescription, // const VkVertexInputBindingDescription*      pVertexBindingDescriptions
        (uint32_t)vertexInputAttributeDescriptions
            .size(), // uint32_t                                    vertexAttributeDescriptionCount
        &vertexInputAttributeDescriptions[0] // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
    };

    const Unique<VkPipeline> pipeline(makeGraphicsPipeline(
        vk,                               // const DeviceInterface&            vk
        vkDevice,                         // const VkDevice                    device
        *pipelineLayout,                  // const VkPipelineLayout            pipelineLayout
        *vertShaderModule,                // const VkShaderModule              vertexShaderModule
        VK_NULL_HANDLE,                   // const VkShaderModule              tessellationControlModule
        VK_NULL_HANDLE,                   // const VkShaderModule              tessellationEvalModule
        VK_NULL_HANDLE,                   // const VkShaderModule              geometryShaderModule
        *fragShaderModule,                // const VkShaderModule              fragmentShaderModule
        *renderPass,                      // const VkRenderPass                renderPass
        viewports,                        // const std::vector<VkViewport>&    viewports
        scissors,                         // const std::vector<VkRect2D>&      scissors
        VK_PRIMITIVE_TOPOLOGY_POINT_LIST, // const VkPrimitiveTopology                    topology
        0u,                               // const uint32_t                                subpass
        0u,                               // const uint32_t                                patchControlPoints
        &vertexInputStateCreateInfo));    // const VkPipelineVertexInputStateCreateInfo*    vertexInputStateCreateInfo

    // Framebuffer
    const VkFramebufferCreateInfo framebufferParams = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // sType
        DE_NULL,                                   // pNext
        0u,                                        // flags
        *renderPass,                               // renderPass
        1u,                                        // attachmentCount
        &*colorAttView,                            // pAttachments
        (uint32_t)renderSize.x(),                  // width
        (uint32_t)renderSize.y(),                  // height
        1u,                                        // layers
    };
    const Unique<VkFramebuffer> framebuffer(createFramebuffer(vk, vkDevice, &framebufferParams));

    const VkCommandPoolCreateInfo cmdPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // sType
        DE_NULL,                                         // pNext
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // flags
        queueFamilyIndex,                                // queueFamilyIndex
    };
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, vkDevice, &cmdPoolParams));

    // Command buffer
    const VkCommandBufferAllocateInfo cmdBufParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // sType
        DE_NULL,                                        // pNext
        *cmdPool,                                       // pool
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // level
        1u,                                             // bufferCount
    };
    const Unique<VkCommandBuffer> cmdBuf(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

    // Record commands
    beginCommandBuffer(vk, *cmdBuf);

    {
        const VkMemoryBarrier vertFlushBarrier = {
            VK_STRUCTURE_TYPE_MEMORY_BARRIER,    // sType
            DE_NULL,                             // pNext
            VK_ACCESS_HOST_WRITE_BIT,            // srcAccessMask
            VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, // dstAccessMask
        };
        const VkImageMemoryBarrier colorAttBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,                                       // sType
            DE_NULL,                                                                      // pNext
            0u,                                                                           // srcAccessMask
            (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT), // dstAccessMask
            VK_IMAGE_LAYOUT_UNDEFINED,                                                    // oldLayout
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,                                     // newLayout
            queueFamilyIndex,                                                             // srcQueueFamilyIndex
            queueFamilyIndex,                                                             // dstQueueFamilyIndex
            *image,                                                                       // image
            {
                VK_IMAGE_ASPECT_COLOR_BIT, // aspectMask
                0u,                        // baseMipLevel
                1u,                        // levelCount
                0u,                        // baseArrayLayer
                1u,                        // layerCount
            }                              // subresourceRange
        };
        vk.cmdPipelineBarrier(*cmdBuf, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                              (VkDependencyFlags)0, 1, &vertFlushBarrier, 0, (const VkBufferMemoryBarrier *)DE_NULL, 1,
                              &colorAttBarrier);
    }

    beginRenderPass(vk, *cmdBuf, *renderPass, *framebuffer, makeRect2D(0, 0, renderSize.x(), renderSize.y()),
                    clearColor);

    vk.cmdBindPipeline(*cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
    {
        const VkDeviceSize bindingOffset = 0;
        vk.cmdBindVertexBuffers(*cmdBuf, 0u, 1u, &vertexBuffer.get(), &bindingOffset);
    }
    vk.cmdPushConstants(*cmdBuf, *pipelineLayout, pcPointSize.stageFlags, pcPointSize.offset, pcPointSize.size,
                        &testPointSize);
    vk.cmdDraw(*cmdBuf, 1u, 1u, 0u, 0u);
    endRenderPass(vk, *cmdBuf);
    copyImageToBuffer(vk, *cmdBuf, *image, *readImageBuffer, renderSize);
    endCommandBuffer(vk, *cmdBuf);

    // Upload vertex data
    deMemcpy(vertexBufferMemory->getHostPtr(), &vertices[0], sizeof(vertices));
    flushAlloc(vk, vkDevice, *vertexBufferMemory);

    // Submit & wait for completion
    submitCommandsAndWait(vk, vkDevice, queue, cmdBuf.get());

    {
        invalidateAlloc(vk, vkDevice, *readImageBufferMemory);
        const tcu::TextureFormat tcuFormat = vk::mapVkFormat(colorFormat);
        const tcu::ConstPixelBufferAccess resultAccess(tcuFormat, renderSize.x(), renderSize.y(), 1,
                                                       readImageBufferMemory->getHostPtr());

        tcu::TextureLevel referenceLevel(tcuFormat, renderSize.x(), renderSize.y());
        auto referenceAccess = referenceLevel.getAccess();
        tcu::clear(referenceAccess, pointColor);
        referenceAccess.setPixel(clearColor, 0, 0);

        auto &log = context.getTestContext().getLog();
        const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);

        if (!tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, threshold,
                                        tcu::COMPARE_LOG_ON_ERROR))
            return tcu::TestStatus::fail("Unexpected color in result buffer; check log for details");
    }
    return tcu::TestStatus::pass("Rendering succeeded");
}

tcu::TestCaseGroup *createDrawPointClampTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> pointClampTests(new tcu::TestCaseGroup(testCtx, "point_size_clamp"));

    addFunctionCaseWithPrograms(pointClampTests.get(), "point_size_clamp_max", createPointSizeClampProgs,
                                renderPointSizeClampTest);

    return pointClampTests.release();
}
} // namespace Draw
} // namespace vkt
