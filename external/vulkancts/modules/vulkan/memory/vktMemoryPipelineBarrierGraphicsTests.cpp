/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief Pipeline barrier tests - graphics (render pass) backend
 *//*--------------------------------------------------------------------*/

#include "vktMemoryPipelineBarrierGraphicsTests.hpp"

#include "vktTestCaseUtil.hpp"

namespace vkt
{
namespace memory
{
namespace pipelinebarrier
{

class PrepareRenderPassContext
{
public:
    PrepareRenderPassContext(PrepareContext &context, vk::VkRenderPass renderPass, vk::VkFramebuffer framebuffer,
                             int32_t targetWidth, int32_t targetHeight)
        : m_context(context)
        , m_renderPass(renderPass)
        , m_framebuffer(framebuffer)
        , m_targetWidth(targetWidth)
        , m_targetHeight(targetHeight)
    {
    }

    const Memory &getMemory(void) const
    {
        return m_context.getMemory();
    }
    const Context &getContext(void) const
    {
        return m_context.getContext();
    }
    const vk::BinaryCollection &getBinaryCollection(void) const
    {
        return m_context.getBinaryCollection();
    }

    vk::VkBuffer getBuffer(void) const
    {
        return m_context.getBuffer();
    }
    vk::VkDeviceSize getBufferSize(void) const
    {
        return m_context.getBufferSize();
    }

    vk::VkImage getImage(void) const
    {
        return m_context.getImage();
    }
    int32_t getImageWidth(void) const
    {
        return m_context.getImageWidth();
    }
    int32_t getImageHeight(void) const
    {
        return m_context.getImageHeight();
    }
    vk::VkImageLayout getImageLayout(void) const
    {
        return m_context.getImageLayout();
    }

    int32_t getTargetWidth(void) const
    {
        return m_targetWidth;
    }
    int32_t getTargetHeight(void) const
    {
        return m_targetHeight;
    }

    vk::VkRenderPass getRenderPass(void) const
    {
        return m_renderPass;
    }

private:
    PrepareContext &m_context;
    const vk::VkRenderPass m_renderPass;
    const vk::VkFramebuffer m_framebuffer;
    const int32_t m_targetWidth;
    const int32_t m_targetHeight;
};

class VerifyRenderPassContext
{
public:
    VerifyRenderPassContext(VerifyContext &context, int32_t targetWidth, int32_t targetHeight)
        : m_context(context)
        , m_referenceTarget(TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8), targetWidth, targetHeight)
    {
    }

    const Context &getContext(void) const
    {
        return m_context.getContext();
    }
    TestLog &getLog(void) const
    {
        return m_context.getLog();
    }
    tcu::ResultCollector &getResultCollector(void) const
    {
        return m_context.getResultCollector();
    }

    TextureLevel &getReferenceTarget(void)
    {
        return m_referenceTarget;
    }

    ReferenceMemory &getReference(void)
    {
        return m_context.getReference();
    }
    TextureLevel &getReferenceImage(void)
    {
        return m_context.getReferenceImage();
    }

private:
    VerifyContext &m_context;
    TextureLevel m_referenceTarget;
};

class RenderPassCommand
{
public:
    virtual ~RenderPassCommand(void)
    {
    }
    virtual const char *getName(void) const = 0;

    // Log things that are done during prepare
    virtual void logPrepare(TestLog &, size_t) const
    {
    }
    // Log submitted calls etc.
    virtual void logSubmit(TestLog &, size_t) const
    {
    }

    // Allocate vulkan resources and prepare for submit.
    virtual void prepare(PrepareRenderPassContext &)
    {
    }

    // Submit commands to command buffer.
    virtual void submit(SubmitContext &)
    {
    }

    // Verify results
    virtual void verify(VerifyRenderPassContext &, size_t)
    {
    }
};

class SubmitRenderPass : public CmdCommand
{
public:
    SubmitRenderPass(const vector<RenderPassCommand *> &commands);
    ~SubmitRenderPass(void);
    const char *getName(void) const
    {
        return "SubmitRenderPass";
    }

    void logPrepare(TestLog &, size_t) const;
    void logSubmit(TestLog &, size_t) const;

    void prepare(PrepareContext &);
    void submit(SubmitContext &);

    void verify(VerifyContext &, size_t);

private:
    const int32_t m_targetWidth;
    const int32_t m_targetHeight;
    vk::Move<vk::VkRenderPass> m_renderPass;
    vk::Move<vk::VkDeviceMemory> m_colorTargetMemory;
    de::MovePtr<vk::Allocation> m_colorTargetMemory2;
    vk::Move<vk::VkImage> m_colorTarget;
    vk::Move<vk::VkImageView> m_colorTargetView;
    vk::Move<vk::VkFramebuffer> m_framebuffer;
    vector<RenderPassCommand *> m_commands;
};

SubmitRenderPass::SubmitRenderPass(const vector<RenderPassCommand *> &commands)
    : m_targetWidth(256)
    , m_targetHeight(256)
    , m_commands(commands)
{
}

SubmitRenderPass::~SubmitRenderPass()
{
    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
        delete m_commands[cmdNdx];
}

void SubmitRenderPass::logPrepare(TestLog &log, size_t commandIndex) const
{
    const string sectionName(de::toString(commandIndex) + ":" + getName());
    const tcu::ScopedLogSection section(log, sectionName, sectionName);

    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
    {
        RenderPassCommand &command = *m_commands[cmdNdx];
        command.logPrepare(log, cmdNdx);
    }
}

void SubmitRenderPass::logSubmit(TestLog &log, size_t commandIndex) const
{
    const string sectionName(de::toString(commandIndex) + ":" + getName());
    const tcu::ScopedLogSection section(log, sectionName, sectionName);

    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
    {
        RenderPassCommand &command = *m_commands[cmdNdx];
        command.logSubmit(log, cmdNdx);
    }
}

void SubmitRenderPass::prepare(PrepareContext &context)
{
    const vk::InstanceInterface &vki          = context.getContext().getInstanceInterface();
    const vk::DeviceInterface &vkd            = context.getContext().getDeviceInterface();
    const vk::VkPhysicalDevice physicalDevice = context.getContext().getPhysicalDevice();
    const vk::VkDevice device                 = context.getContext().getDevice();
    const vector<uint32_t> &queueFamilies     = context.getContext().getQueueFamilies();

    {
        const vk::VkImageCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                 // VkStructureType sType;
            nullptr,                                                 // const void* pNext;
            0u,                                                      // VkImageCreateFlags flags;
            vk::VK_IMAGE_TYPE_2D,                                    // VkImageType imageType;
            vk::VK_FORMAT_R8G8B8A8_UNORM,                            // VkFormat format;
            {(uint32_t)m_targetWidth, (uint32_t)m_targetHeight, 1u}, // VkExtent3D extent;
            1u,                                                      // uint32_t mipLevels;
            1u,                                                      // uint32_t arrayLayers;
            vk::VK_SAMPLE_COUNT_1_BIT,                               // VkSampleCountFlagBits samples;
            vk::VK_IMAGE_TILING_OPTIMAL,                             // VkImageTiling tiling;
            vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
            vk::VK_SHARING_MODE_EXCLUSIVE,                                                 // VkSharingMode sharingMode;
            (uint32_t)queueFamilies.size(), // uint32_t queueFamilyIndexCount;
            &queueFamilies[0],              // const uint32_t* pQueueFamilyIndices;
            vk::VK_IMAGE_LAYOUT_UNDEFINED   // VkImageLayout initialLayout;
        };

        m_colorTarget = vk::createImage(vkd, device, &createInfo);
    }

    m_colorTargetMemory = bindImageMemory(vki, vkd, physicalDevice, device, *m_colorTarget, 0);

    {
        const vk::VkImageViewCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
            nullptr,                                      // const void* pNext;
            0u,                                           // VkImageViewCreateFlags flags;
            *m_colorTarget,                               // VkImage image;
            vk::VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
            vk::VK_FORMAT_R8G8B8A8_UNORM,                 // VkFormat format;
            {vk::VK_COMPONENT_SWIZZLE_R, vk::VK_COMPONENT_SWIZZLE_G, vk::VK_COMPONENT_SWIZZLE_B,
             vk::VK_COMPONENT_SWIZZLE_A},                   // VkComponentMapping components;
            {vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
        };

        m_colorTargetView = vk::createImageView(vkd, device, &createInfo);
    }

    m_renderPass = vk::makeRenderPass(vkd, device, vk::VK_FORMAT_R8G8B8A8_UNORM, vk::VK_FORMAT_UNDEFINED,
                                      vk::VK_ATTACHMENT_LOAD_OP_CLEAR, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    {
        const vk::VkImageView imageViews[]           = {*m_colorTargetView};
        const vk::VkFramebufferCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                                       // const void* pNext;
            0u,                                            // VkFramebufferCreateFlags flags;
            *m_renderPass,                                 // VkRenderPass renderPass;
            DE_LENGTH_OF_ARRAY(imageViews),                // uint32_t attachmentCount;
            imageViews,                                    // const VkImageView* pAttachments;
            (uint32_t)m_targetWidth,                       // uint32_t width;
            (uint32_t)m_targetHeight,                      // uint32_t height;
            1u                                             // uint32_t layers;
        };

        m_framebuffer = vk::createFramebuffer(vkd, device, &createInfo);
    }

    {
        PrepareRenderPassContext renderpassContext(context, *m_renderPass, *m_framebuffer, m_targetWidth,
                                                   m_targetHeight);

        for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
        {
            RenderPassCommand &command = *m_commands[cmdNdx];
            command.prepare(renderpassContext);
        }
    }
}

void SubmitRenderPass::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();

    beginRenderPass(vkd, commandBuffer, *m_renderPass, *m_framebuffer,
                    vk::makeRect2D(0, 0, m_targetWidth, m_targetHeight), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
    {
        RenderPassCommand &command = *m_commands[cmdNdx];

        command.submit(context);
    }

    endRenderPass(vkd, commandBuffer);
}

void SubmitRenderPass::verify(VerifyContext &context, size_t commandIndex)
{
    TestLog &log(context.getLog());
    tcu::ResultCollector &resultCollector(context.getResultCollector());
    const string sectionName(de::toString(commandIndex) + ":" + getName());
    const tcu::ScopedLogSection section(log, sectionName, sectionName);
    VerifyRenderPassContext verifyContext(context, m_targetWidth, m_targetHeight);

    tcu::clear(verifyContext.getReferenceTarget().getAccess(), Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
    {
        RenderPassCommand &command = *m_commands[cmdNdx];
        command.verify(verifyContext, cmdNdx);
    }

    {
        const vk::InstanceInterface &vki          = context.getContext().getInstanceInterface();
        const vk::DeviceInterface &vkd            = context.getContext().getDeviceInterface();
        const vk::VkPhysicalDevice physicalDevice = context.getContext().getPhysicalDevice();
        const vk::VkDevice device                 = context.getContext().getDevice();
        const vk::VkQueue queue                   = context.getContext().getQueue();
        const vk::VkCommandPool commandPool       = context.getContext().getCommandPool();
        const vk::Unique<vk::VkCommandBuffer> commandBuffer(
            createBeginCommandBuffer(vkd, device, commandPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));
        const vector<uint32_t> &queueFamilies = context.getContext().getQueueFamilies();
        const vk::Unique<vk::VkBuffer> dstBuffer(createBuffer(vkd, device, 4 * m_targetWidth * m_targetHeight,
                                                              vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                              vk::VK_SHARING_MODE_EXCLUSIVE, queueFamilies));
        const vk::Unique<vk::VkDeviceMemory> memory(
            bindBufferMemory(vki, vkd, physicalDevice, device, *dstBuffer, vk::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
        {
            const vk::VkImageMemoryBarrier imageBarrier = {
                vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                nullptr,                                    // const void* pNext;
                vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,   // VkAccessFlags srcAccessMask;
                vk::VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
                vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,   // VkImageLayout oldLayout;
                vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,   // VkImageLayout newLayout;
                VK_QUEUE_FAMILY_IGNORED,                    // uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                    // uint32_t dstQueueFamilyIndex;
                *m_colorTarget,                             // VkImage image;
                {
                    vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    0,                             // uint32_t baseMipLevel;
                    1,                             // uint32_t levelCount;
                    0,                             // uint32_t baseArrayLayer;
                    1                              // uint32_t layerCount;
                }                                  // VkImageSubresourceRange subresourceRange;
            };
            const vk::VkBufferMemoryBarrier bufferBarrier = {
                vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
                nullptr,                                     // const void* pNext;
                vk::VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
                vk::VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
                VK_QUEUE_FAMILY_IGNORED,                     // uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                     // uint32_t dstQueueFamilyIndex;
                *dstBuffer,                                  // VkBuffer buffer;
                0,                                           // VkDeviceSize offset;
                VK_WHOLE_SIZE                                // VkDeviceSize size;
            };
            const vk::VkBufferImageCopy region = {
                0, // VkDeviceSize bufferOffset;
                0, // uint32_t bufferRowLength;
                0, // uint32_t bufferImageHeight;
                {
                    vk::VK_IMAGE_ASPECT_COLOR_BIT,                      // VkImageAspectFlags aspectMask;
                    0,                                                  // uint32_t mipLevel;
                    0,                                                  // uint32_t baseArrayLayer;
                    1                                                   // uint32_t layerCount;
                },                                                      // VkImageSubresourceLayers imageSubresource;
                {0, 0, 0},                                              // VkOffset3D imageOffset;
                {(uint32_t)m_targetWidth, (uint32_t)m_targetHeight, 1u} // VkExtent3D imageExtent;
            };

            vkd.cmdPipelineBarrier(*commandBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (vk::VkDependencyFlags)0, 0, nullptr, 0, nullptr,
                                   1, &imageBarrier);
            vkd.cmdCopyImageToBuffer(*commandBuffer, *m_colorTarget, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                     *dstBuffer, 1, &region);
            vkd.cmdPipelineBarrier(*commandBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT,
                                   (vk::VkDependencyFlags)0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);
        }

        endCommandBuffer(vkd, *commandBuffer);
        submitCommandsAndWait(vkd, device, queue, *commandBuffer);

        {
            void *const ptr = mapMemory(vkd, device, *memory, 4 * m_targetWidth * m_targetHeight);

            vk::invalidateMappedMemoryRange(vkd, device, *memory, 0, VK_WHOLE_SIZE);

            {
                const uint8_t *const data = (const uint8_t *)ptr;
                const ConstPixelBufferAccess resAccess(TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8),
                                                       m_targetWidth, m_targetHeight, 1, data);
                const ConstPixelBufferAccess &refAccess(verifyContext.getReferenceTarget().getAccess());

                if (!tcu::intThresholdCompare(context.getLog(), (de::toString(commandIndex) + ":" + getName()).c_str(),
                                              (de::toString(commandIndex) + ":" + getName()).c_str(), refAccess,
                                              resAccess, UVec4(0), tcu::COMPARE_LOG_ON_ERROR))
                    resultCollector.fail(de::toString(commandIndex) + ":" + getName() + " Image comparison failed");
            }

            vkd.unmapMemory(device, *memory);
        }
    }
}

struct PipelineResources
{
    vk::Move<vk::VkPipeline> pipeline;
    vk::Move<vk::VkDescriptorSetLayout> descriptorSetLayout;
    vk::Move<vk::VkPipelineLayout> pipelineLayout;
};

void createPipelineWithResources(const vk::DeviceInterface &vkd, const vk::VkDevice device,
                                 const vk::VkRenderPass renderPass, const uint32_t subpass,
                                 const vk::VkShaderModule &vertexShaderModule,
                                 const vk::VkShaderModule &fragmentShaderModule, const uint32_t viewPortWidth,
                                 const uint32_t viewPortHeight,
                                 const vector<vk::VkVertexInputBindingDescription> &vertexBindingDescriptions,
                                 const vector<vk::VkVertexInputAttributeDescription> &vertexAttributeDescriptions,
                                 const vector<vk::VkDescriptorSetLayoutBinding> &bindings,
                                 const vk::VkPrimitiveTopology topology, uint32_t pushConstantRangeCount,
                                 const vk::VkPushConstantRange *pushConstantRanges, PipelineResources &resources)
{
    if (!bindings.empty())
    {
        const vk::VkDescriptorSetLayoutCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                 // const void* pNext;
            0u,                                                      // VkDescriptorSetLayoutCreateFlags flags;
            (uint32_t)bindings.size(),                               // uint32_t bindingCount;
            bindings.empty() ? nullptr : &bindings[0]                // const VkDescriptorSetLayoutBinding* pBindings;
        };

        resources.descriptorSetLayout = vk::createDescriptorSetLayout(vkd, device, &createInfo);
    }

    {
        const vk::VkDescriptorSetLayout descriptorSetLayout_ = *resources.descriptorSetLayout;
        const vk::VkPipelineLayoutCreateInfo createInfo      = {
            vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
            nullptr,                                           // const void* pNext;
            0,                                                 // VkPipelineLayoutCreateFlags flags;
            resources.descriptorSetLayout ? 1u : 0u,           // uint32_t setLayoutCount;
            resources.descriptorSetLayout ? &descriptorSetLayout_ :
                                                 nullptr, // const VkDescriptorSetLayout* pSetLayouts;
            pushConstantRangeCount,                  // uint32_t pushConstantRangeCount;
            pushConstantRanges                       // const VkPushConstantRange* pPushConstantRanges;
        };

        resources.pipelineLayout = vk::createPipelineLayout(vkd, device, &createInfo);
    }

    {
        const std::vector<vk::VkViewport> viewports(
            1, vk::makeViewport(0.0f, 0.0f, (float)viewPortWidth, (float)viewPortHeight, 0.0f, 1.0f));
        const std::vector<vk::VkRect2D> scissors(1, vk::makeRect2D(0, 0, viewPortWidth, viewPortHeight));

        const vk::VkPipelineVertexInputStateCreateInfo vertexInputState = {
            vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                       // const void* pNext;
            0u,                                         // VkPipelineVertexInputStateCreateFlags flags;
            (uint32_t)vertexBindingDescriptions.size(), // uint32_t vertexBindingDescriptionCount;
            vertexBindingDescriptions.empty() ?
                nullptr :
                &vertexBindingDescriptions[0], // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
            (uint32_t)vertexAttributeDescriptions.size(), // uint32_t vertexAttributeDescriptionCount;
            vertexAttributeDescriptions.empty() ?
                nullptr :
                &vertexAttributeDescriptions
                    [0] // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
        };

        resources.pipeline = vk::makeGraphicsPipeline(
            vkd,                       // const DeviceInterface&                        vk
            device,                    // const VkDevice                                device
            *resources.pipelineLayout, // const VkPipelineLayout                        pipelineLayout
            vertexShaderModule,        // const VkShaderModule                          vertexShaderModule
            VK_NULL_HANDLE,            // const VkShaderModule                          tessellationControlModule
            VK_NULL_HANDLE,            // const VkShaderModule                          tessellationEvalModule
            VK_NULL_HANDLE,            // const VkShaderModule                          geometryShaderModule
            fragmentShaderModule,      // const VkShaderModule                          fragmentShaderModule
            renderPass,                // const VkRenderPass                            renderPass
            viewports,                 // const std::vector<VkViewport>&                viewports
            scissors,                  // const std::vector<VkRect2D>&                  scissors
            topology,                  // const VkPrimitiveTopology                     topology
            subpass,                   // const uint32_t                                subpass
            0u,                        // const uint32_t                                patchControlPoints
            &vertexInputState);        // const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
    }
}
class RenderIndexBuffer : public RenderPassCommand
{
public:
    RenderIndexBuffer(void)
    {
    }
    ~RenderIndexBuffer(void)
    {
    }

    const char *getName(void) const
    {
        return "RenderIndexBuffer";
    }
    void logPrepare(TestLog &, size_t) const;
    void logSubmit(TestLog &, size_t) const;
    void prepare(PrepareRenderPassContext &);
    void submit(SubmitContext &context);
    void verify(VerifyRenderPassContext &, size_t);

private:
    PipelineResources m_resources;
    vk::VkDeviceSize m_bufferSize;
};

void RenderIndexBuffer::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Create pipeline for render buffer as index buffer."
        << TestLog::EndMessage;
}

void RenderIndexBuffer::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Render using buffer as index buffer."
        << TestLog::EndMessage;
}

void RenderIndexBuffer::prepare(PrepareRenderPassContext &context)
{
    const vk::DeviceInterface &vkd    = context.getContext().getDeviceInterface();
    const vk::VkDevice device         = context.getContext().getDevice();
    const vk::VkRenderPass renderPass = context.getRenderPass();
    const uint32_t subpass            = 0;
    const vk::Unique<vk::VkShaderModule> vertexShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("index-buffer.vert"), 0));
    const vk::Unique<vk::VkShaderModule> fragmentShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("render-white.frag"), 0));

    createPipelineWithResources(
        vkd, device, renderPass, subpass, *vertexShaderModule, *fragmentShaderModule, context.getTargetWidth(),
        context.getTargetHeight(), vector<vk::VkVertexInputBindingDescription>(),
        vector<vk::VkVertexInputAttributeDescription>(), vector<vk::VkDescriptorSetLayoutBinding>(),
        vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST, 0u, nullptr, m_resources);
    m_bufferSize = context.getBufferSize();
}

void RenderIndexBuffer::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();

    vkd.cmdBindPipeline(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipeline);
    vkd.cmdBindIndexBuffer(commandBuffer, context.getBuffer(), 0, vk::VK_INDEX_TYPE_UINT16);
    vkd.cmdDrawIndexed(commandBuffer, (uint32_t)(context.getBufferSize() / 2), 1, 0, 0, 0);
}

void RenderIndexBuffer::verify(VerifyRenderPassContext &context, size_t)
{
    for (size_t pos = 0; pos < (size_t)m_bufferSize / 2; pos++)
    {
        const uint8_t x = context.getReference().get(pos * 2);
        const uint8_t y = context.getReference().get((pos * 2) + 1);

        context.getReferenceTarget().getAccess().setPixel(Vec4(1.0f, 1.0f, 1.0f, 1.0f), x, y);
    }
}

class RenderVertexBuffer : public RenderPassCommand
{
public:
    RenderVertexBuffer(uint32_t stride)
        : m_stride(stride)
        , m_name("RenderVertexBuffer" + de::toString(stride))
        , m_bufferSize(0)
    {
    }
    ~RenderVertexBuffer(void)
    {
    }

    const char *getName(void) const
    {
        return m_name.c_str();
    }
    void logPrepare(TestLog &, size_t) const;
    void logSubmit(TestLog &, size_t) const;
    void prepare(PrepareRenderPassContext &);
    void submit(SubmitContext &context);
    void verify(VerifyRenderPassContext &, size_t);

private:
    const uint32_t m_stride;
    const std::string m_name;
    PipelineResources m_resources;
    vk::VkDeviceSize m_bufferSize;
};

void RenderVertexBuffer::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName()
        << " Create pipeline for render buffer as vertex buffer." << TestLog::EndMessage;
}

void RenderVertexBuffer::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Render using buffer as vertex buffer."
        << TestLog::EndMessage;
}

void RenderVertexBuffer::prepare(PrepareRenderPassContext &context)
{
    const vk::DeviceInterface &vkd    = context.getContext().getDeviceInterface();
    const vk::VkDevice device         = context.getContext().getDevice();
    const vk::VkRenderPass renderPass = context.getRenderPass();
    const uint32_t subpass            = 0;
    const vk::Unique<vk::VkShaderModule> vertexShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("vertex-buffer.vert"), 0));
    const vk::Unique<vk::VkShaderModule> fragmentShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("render-white.frag"), 0));

    vector<vk::VkVertexInputAttributeDescription> vertexAttributeDescriptions;
    vector<vk::VkVertexInputBindingDescription> vertexBindingDescriptions;

    {
        const vk::VkVertexInputBindingDescription vertexBindingDescription = {
            0,                              // uint32_t binding;
            m_stride,                       // uint32_t stride;
            vk::VK_VERTEX_INPUT_RATE_VERTEX // VkVertexInputRate inputRate;
        };

        vertexBindingDescriptions.push_back(vertexBindingDescription);
    }
    {
        const vk::VkVertexInputAttributeDescription vertexAttributeDescription = {
            0,                        // uint32_t location;
            0,                        // uint32_t binding;
            vk::VK_FORMAT_R8G8_UNORM, // VkFormat format;
            0                         // uint32_t offset;
        };

        vertexAttributeDescriptions.push_back(vertexAttributeDescription);
    }
    createPipelineWithResources(vkd, device, renderPass, subpass, *vertexShaderModule, *fragmentShaderModule,
                                context.getTargetWidth(), context.getTargetHeight(), vertexBindingDescriptions,
                                vertexAttributeDescriptions, vector<vk::VkDescriptorSetLayoutBinding>(),
                                vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST, 0u, nullptr, m_resources);

    m_bufferSize = context.getBufferSize();
}

void RenderVertexBuffer::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();
    const vk::VkDeviceSize offset           = 0;
    const vk::VkBuffer buffer               = context.getBuffer();

    vkd.cmdBindPipeline(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipeline);
    vkd.cmdBindVertexBuffers(commandBuffer, 0, 1, &buffer, &offset);
    vkd.cmdDraw(commandBuffer, (uint32_t)(context.getBufferSize() / m_stride), 1, 0, 0);
}

void RenderVertexBuffer::verify(VerifyRenderPassContext &context, size_t)
{
    for (size_t pos = 0; pos < (size_t)m_bufferSize / m_stride; pos++)
    {
        const uint8_t x = context.getReference().get(pos * m_stride);
        const uint8_t y = context.getReference().get((pos * m_stride) + 1);

        context.getReferenceTarget().getAccess().setPixel(Vec4(1.0f, 1.0f, 1.0f, 1.0f), x, y);
    }
}

class RenderVertexUniformBuffer : public RenderPassCommand
{
public:
    RenderVertexUniformBuffer(void)
    {
    }
    ~RenderVertexUniformBuffer(void);

    const char *getName(void) const
    {
        return "RenderVertexUniformBuffer";
    }
    void logPrepare(TestLog &, size_t) const;
    void logSubmit(TestLog &, size_t) const;
    void prepare(PrepareRenderPassContext &);
    void submit(SubmitContext &context);
    void verify(VerifyRenderPassContext &, size_t);

protected:
    uint32_t calculateBufferPartSize(size_t descriptorSetNdx) const;

private:
    PipelineResources m_resources;
    vk::Move<vk::VkDescriptorPool> m_descriptorPool;
    vector<vk::VkDescriptorSet> m_descriptorSets;

    vk::VkDeviceSize m_bufferSize;
};

RenderVertexUniformBuffer::~RenderVertexUniformBuffer(void)
{
}

void RenderVertexUniformBuffer::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName()
        << " Create pipeline for render buffer as uniform buffer." << TestLog::EndMessage;
}

void RenderVertexUniformBuffer::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Render using buffer as uniform buffer."
        << TestLog::EndMessage;
}

void RenderVertexUniformBuffer::prepare(PrepareRenderPassContext &context)
{
    const vk::DeviceInterface &vkd    = context.getContext().getDeviceInterface();
    const vk::VkDevice device         = context.getContext().getDevice();
    const vk::VkRenderPass renderPass = context.getRenderPass();
    const uint32_t subpass            = 0;
    const vk::Unique<vk::VkShaderModule> vertexShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("uniform-buffer.vert"), 0));
    const vk::Unique<vk::VkShaderModule> fragmentShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("render-white.frag"), 0));
    vector<vk::VkDescriptorSetLayoutBinding> bindings;

    // make sure buffer size is multiple of 16 (in glsl we use uvec4 to store 16 values)
    m_bufferSize = context.getBufferSize();
    m_bufferSize = static_cast<vk::VkDeviceSize>(m_bufferSize / 16u) * 16u;

    {
        const vk::VkDescriptorSetLayoutBinding binding = {0u, vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                                          vk::VK_SHADER_STAGE_VERTEX_BIT, nullptr};

        bindings.push_back(binding);
    }

    createPipelineWithResources(vkd, device, renderPass, subpass, *vertexShaderModule, *fragmentShaderModule,
                                context.getTargetWidth(), context.getTargetHeight(),
                                vector<vk::VkVertexInputBindingDescription>(),
                                vector<vk::VkVertexInputAttributeDescription>(), bindings,
                                vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST, 0u, nullptr, m_resources);

    {
        const uint32_t descriptorCount =
            (uint32_t)(divRoundUp(m_bufferSize, (vk::VkDeviceSize)MAX_UNIFORM_BUFFER_SIZE));
        const vk::VkDescriptorPoolSize poolSizes = {
            vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // VkDescriptorType type;
            descriptorCount                        // uint32_t descriptorCount;
        };
        const vk::VkDescriptorPoolCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,     // VkStructureType sType;
            nullptr,                                               // const void* pNext;
            vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // VkDescriptorPoolCreateFlags flags;
            descriptorCount,                                       // uint32_t maxSets;
            1u,                                                    // uint32_t poolSizeCount;
            &poolSizes,                                            // const VkDescriptorPoolSize* pPoolSizes;
        };

        m_descriptorPool = vk::createDescriptorPool(vkd, device, &createInfo);
        m_descriptorSets.resize(descriptorCount);
    }

    for (size_t descriptorSetNdx = 0; descriptorSetNdx < m_descriptorSets.size(); descriptorSetNdx++)
    {
        const vk::VkDescriptorSetLayout layout             = *m_resources.descriptorSetLayout;
        const vk::VkDescriptorSetAllocateInfo allocateInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType sType;
            nullptr,                                            // const void* pNext;
            *m_descriptorPool,                                  // VkDescriptorPool descriptorPool;
            1,                                                  // uint32_t descriptorSetCount;
            &layout                                             // const VkDescriptorSetLayout* pSetLayouts;
        };

        m_descriptorSets[descriptorSetNdx] = vk::allocateDescriptorSet(vkd, device, &allocateInfo).disown();

        {
            const vk::VkDescriptorBufferInfo bufferInfo = {
                context.getBuffer(),                                                    // VkBuffer buffer;
                (vk::VkDeviceSize)(descriptorSetNdx * (size_t)MAX_UNIFORM_BUFFER_SIZE), // VkDeviceSize offset;
                calculateBufferPartSize(descriptorSetNdx)                               // VkDeviceSize range;
            };
            const vk::VkWriteDescriptorSet write = {
                vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType sType;
                nullptr,                                    // const void* pNext;
                m_descriptorSets[descriptorSetNdx],         // VkDescriptorSet dstSet;
                0u,                                         // uint32_t dstBinding;
                0u,                                         // uint32_t dstArrayElement;
                1u,                                         // uint32_t descriptorCount;
                vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,      // VkDescriptorType descriptorType;
                nullptr,                                    // const VkDescriptorImageInfo* pImageInfo;
                &bufferInfo,                                // const VkDescriptorBufferInfo* pBufferInfo;
                nullptr,                                    // const VkBufferView* pTexelBufferView;
            };

            vkd.updateDescriptorSets(device, 1u, &write, 0u, nullptr);
        }
    }
}

void RenderVertexUniformBuffer::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();

    vkd.cmdBindPipeline(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipeline);

    for (size_t descriptorSetNdx = 0; descriptorSetNdx < m_descriptorSets.size(); descriptorSetNdx++)
    {
        const size_t size    = calculateBufferPartSize(descriptorSetNdx);
        const uint32_t count = (uint32_t)(size / 2);

        vkd.cmdBindDescriptorSets(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipelineLayout, 0u,
                                  1u, &m_descriptorSets[descriptorSetNdx], 0u, nullptr);
        vkd.cmdDraw(commandBuffer, count, 1, 0, 0);
    }
}

void RenderVertexUniformBuffer::verify(VerifyRenderPassContext &context, size_t)
{
    for (size_t descriptorSetNdx = 0; descriptorSetNdx < m_descriptorSets.size(); descriptorSetNdx++)
    {
        const size_t offset = descriptorSetNdx * MAX_UNIFORM_BUFFER_SIZE;
        const size_t size   = calculateBufferPartSize(descriptorSetNdx);
        const size_t count  = size / 2;

        for (size_t pos = 0; pos < count; pos++)
        {
            const uint8_t x = context.getReference().get(offset + pos * 2);
            const uint8_t y = context.getReference().get(offset + (pos * 2) + 1);

            context.getReferenceTarget().getAccess().setPixel(Vec4(1.0f, 1.0f, 1.0f, 1.0f), x, y);
        }
    }
}

uint32_t RenderVertexUniformBuffer::calculateBufferPartSize(size_t descriptorSetNdx) const
{
    uint32_t size =
        static_cast<uint32_t>(m_bufferSize) - static_cast<uint32_t>(descriptorSetNdx) * MAX_UNIFORM_BUFFER_SIZE;
    if (size < MAX_UNIFORM_BUFFER_SIZE)
        return size;
    return MAX_UNIFORM_BUFFER_SIZE;
}

class RenderVertexUniformTexelBuffer : public RenderPassCommand
{
public:
    RenderVertexUniformTexelBuffer(void)
    {
    }
    ~RenderVertexUniformTexelBuffer(void);

    const char *getName(void) const
    {
        return "RenderVertexUniformTexelBuffer";
    }
    void logPrepare(TestLog &, size_t) const;
    void logSubmit(TestLog &, size_t) const;
    void prepare(PrepareRenderPassContext &);
    void submit(SubmitContext &context);
    void verify(VerifyRenderPassContext &, size_t);

private:
    PipelineResources m_resources;
    vk::Move<vk::VkDescriptorPool> m_descriptorPool;
    vector<vk::VkDescriptorSet> m_descriptorSets;
    vector<vk::VkBufferView> m_bufferViews;

    const vk::DeviceInterface *m_vkd;
    vk::VkDevice m_device;
    vk::VkDeviceSize m_bufferSize;
    uint32_t m_maxUniformTexelCount;
};

RenderVertexUniformTexelBuffer::~RenderVertexUniformTexelBuffer(void)
{
    for (size_t bufferViewNdx = 0; bufferViewNdx < m_bufferViews.size(); bufferViewNdx++)
    {
        if (!!m_bufferViews[bufferViewNdx])
        {
            m_vkd->destroyBufferView(m_device, m_bufferViews[bufferViewNdx], nullptr);
            m_bufferViews[bufferViewNdx] = VK_NULL_HANDLE;
        }
    }
}

void RenderVertexUniformTexelBuffer::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName()
        << " Create pipeline for render buffer as uniform buffer." << TestLog::EndMessage;
}

void RenderVertexUniformTexelBuffer::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Render using buffer as uniform buffer."
        << TestLog::EndMessage;
}

void RenderVertexUniformTexelBuffer::prepare(PrepareRenderPassContext &context)
{
    const vk::InstanceInterface &vki          = context.getContext().getInstanceInterface();
    const vk::VkPhysicalDevice physicalDevice = context.getContext().getPhysicalDevice();
    const vk::DeviceInterface &vkd            = context.getContext().getDeviceInterface();
    const vk::VkDevice device                 = context.getContext().getDevice();
    const vk::VkRenderPass renderPass         = context.getRenderPass();
    const uint32_t subpass                    = 0;
    const vk::Unique<vk::VkShaderModule> vertexShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("uniform-texel-buffer.vert"), 0));
    const vk::Unique<vk::VkShaderModule> fragmentShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("render-white.frag"), 0));
    vector<vk::VkDescriptorSetLayoutBinding> bindings;

    m_device               = device;
    m_vkd                  = &vkd;
    m_bufferSize           = context.getBufferSize();
    m_maxUniformTexelCount = vk::getPhysicalDeviceProperties(vki, physicalDevice).limits.maxTexelBufferElements;

    {
        const vk::VkDescriptorSetLayoutBinding binding = {0u, vk::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1,
                                                          vk::VK_SHADER_STAGE_VERTEX_BIT, nullptr};

        bindings.push_back(binding);
    }

    createPipelineWithResources(vkd, device, renderPass, subpass, *vertexShaderModule, *fragmentShaderModule,
                                context.getTargetWidth(), context.getTargetHeight(),
                                vector<vk::VkVertexInputBindingDescription>(),
                                vector<vk::VkVertexInputAttributeDescription>(), bindings,
                                vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST, 0u, nullptr, m_resources);

    {
        const uint32_t descriptorCount =
            (uint32_t)(divRoundUp(m_bufferSize, (vk::VkDeviceSize)m_maxUniformTexelCount * 2));
        const vk::VkDescriptorPoolSize poolSizes = {
            vk::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, // VkDescriptorType type;
            descriptorCount                              // uint32_t descriptorCount;
        };
        const vk::VkDescriptorPoolCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,     // VkStructureType sType;
            nullptr,                                               // const void* pNext;
            vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // VkDescriptorPoolCreateFlags flags;
            descriptorCount,                                       // uint32_t maxSets;
            1u,                                                    // uint32_t poolSizeCount;
            &poolSizes,                                            // const VkDescriptorPoolSize* pPoolSizes;
        };

        m_descriptorPool = vk::createDescriptorPool(vkd, device, &createInfo);
        m_descriptorSets.resize(descriptorCount, VK_NULL_HANDLE);
        m_bufferViews.resize(descriptorCount, VK_NULL_HANDLE);
    }

    for (size_t descriptorSetNdx = 0; descriptorSetNdx < m_descriptorSets.size(); descriptorSetNdx++)
    {
        const uint32_t count = (uint32_t)(m_bufferSize < (descriptorSetNdx + 1) * m_maxUniformTexelCount * 2 ?
                                              m_bufferSize - descriptorSetNdx * m_maxUniformTexelCount * 2 :
                                              m_maxUniformTexelCount * 2) /
                               2;
        const vk::VkDescriptorSetLayout layout             = *m_resources.descriptorSetLayout;
        const vk::VkDescriptorSetAllocateInfo allocateInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType sType;
            nullptr,                                            // const void* pNext;
            *m_descriptorPool,                                  // VkDescriptorPool descriptorPool;
            1,                                                  // uint32_t descriptorSetCount;
            &layout                                             // const VkDescriptorSetLayout* pSetLayouts;
        };

        m_descriptorSets[descriptorSetNdx] = vk::allocateDescriptorSet(vkd, device, &allocateInfo).disown();

        {
            const vk::VkBufferViewCreateInfo createInfo = {
                vk::VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, // VkStructureType sType;
                nullptr,                                       // const void* pNext;
                0u,                                            // VkBufferViewCreateFlags flags;
                context.getBuffer(),                           // VkBuffer buffer;
                vk::VK_FORMAT_R16_UINT,                        // VkFormat format;
                descriptorSetNdx * m_maxUniformTexelCount * 2, // VkDeviceSize offset;
                count * 2                                      // VkDeviceSize range;
            };

            VK_CHECK(vkd.createBufferView(device, &createInfo, nullptr, &m_bufferViews[descriptorSetNdx]));
        }

        {
            const vk::VkWriteDescriptorSet write = {
                vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,  // VkStructureType sType;
                nullptr,                                     // const void* pNext;
                m_descriptorSets[descriptorSetNdx],          // VkDescriptorSet dstSet;
                0u,                                          // uint32_t dstBinding;
                0u,                                          // uint32_t dstArrayElement;
                1u,                                          // uint32_t descriptorCount;
                vk::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, // VkDescriptorType descriptorType;
                nullptr,                                     // const VkDescriptorImageInfo* pImageInfo;
                nullptr,                                     // const VkDescriptorBufferInfo* pBufferInfo;
                &m_bufferViews[descriptorSetNdx]             // const VkBufferView* pTexelBufferView;
            };

            vkd.updateDescriptorSets(device, 1u, &write, 0u, nullptr);
        }
    }
}

void RenderVertexUniformTexelBuffer::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();

    vkd.cmdBindPipeline(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipeline);

    for (size_t descriptorSetNdx = 0; descriptorSetNdx < m_descriptorSets.size(); descriptorSetNdx++)
    {
        const uint32_t count = (uint32_t)(m_bufferSize < (descriptorSetNdx + 1) * m_maxUniformTexelCount * 2 ?
                                              m_bufferSize - descriptorSetNdx * m_maxUniformTexelCount * 2 :
                                              m_maxUniformTexelCount * 2) /
                               2;

        vkd.cmdBindDescriptorSets(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipelineLayout, 0u,
                                  1u, &m_descriptorSets[descriptorSetNdx], 0u, nullptr);
        vkd.cmdDraw(commandBuffer, count, 1, 0, 0);
    }
}

void RenderVertexUniformTexelBuffer::verify(VerifyRenderPassContext &context, size_t)
{
    for (size_t descriptorSetNdx = 0; descriptorSetNdx < m_descriptorSets.size(); descriptorSetNdx++)
    {
        const size_t offset  = descriptorSetNdx * m_maxUniformTexelCount * 2;
        const uint32_t count = (uint32_t)(m_bufferSize < (descriptorSetNdx + 1) * m_maxUniformTexelCount * 2 ?
                                              m_bufferSize - descriptorSetNdx * m_maxUniformTexelCount * 2 :
                                              m_maxUniformTexelCount * 2) /
                               2;

        for (size_t pos = 0; pos < (size_t)count; pos++)
        {
            const uint8_t x = context.getReference().get(offset + pos * 2);
            const uint8_t y = context.getReference().get(offset + (pos * 2) + 1);

            context.getReferenceTarget().getAccess().setPixel(Vec4(1.0f, 1.0f, 1.0f, 1.0f), x, y);
        }
    }
}

class RenderVertexStorageBuffer : public RenderPassCommand
{
public:
    RenderVertexStorageBuffer(void)
    {
    }
    ~RenderVertexStorageBuffer(void);

    const char *getName(void) const
    {
        return "RenderVertexStorageBuffer";
    }
    void logPrepare(TestLog &, size_t) const;
    void logSubmit(TestLog &, size_t) const;
    void prepare(PrepareRenderPassContext &);
    void submit(SubmitContext &context);
    void verify(VerifyRenderPassContext &, size_t);

private:
    PipelineResources m_resources;
    vk::Move<vk::VkDescriptorPool> m_descriptorPool;
    vector<vk::VkDescriptorSet> m_descriptorSets;

    vk::VkDeviceSize m_bufferSize;
};

RenderVertexStorageBuffer::~RenderVertexStorageBuffer(void)
{
}

void RenderVertexStorageBuffer::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName()
        << " Create pipeline for render buffer as storage buffer." << TestLog::EndMessage;
}

void RenderVertexStorageBuffer::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Render using buffer as storage buffer."
        << TestLog::EndMessage;
}

void RenderVertexStorageBuffer::prepare(PrepareRenderPassContext &context)
{
    const vk::DeviceInterface &vkd    = context.getContext().getDeviceInterface();
    const vk::VkDevice device         = context.getContext().getDevice();
    const vk::VkRenderPass renderPass = context.getRenderPass();
    const uint32_t subpass            = 0;
    const vk::Unique<vk::VkShaderModule> vertexShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("storage-buffer.vert"), 0));
    const vk::Unique<vk::VkShaderModule> fragmentShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("render-white.frag"), 0));
    vector<vk::VkDescriptorSetLayoutBinding> bindings;

    m_bufferSize = context.getBufferSize();

    {
        const vk::VkDescriptorSetLayoutBinding binding = {0u, vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                                          vk::VK_SHADER_STAGE_VERTEX_BIT, nullptr};

        bindings.push_back(binding);
    }

    createPipelineWithResources(vkd, device, renderPass, subpass, *vertexShaderModule, *fragmentShaderModule,
                                context.getTargetWidth(), context.getTargetHeight(),
                                vector<vk::VkVertexInputBindingDescription>(),
                                vector<vk::VkVertexInputAttributeDescription>(), bindings,
                                vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST, 0u, nullptr, m_resources);

    {
        const uint32_t descriptorCount =
            (uint32_t)(divRoundUp(m_bufferSize, (vk::VkDeviceSize)MAX_STORAGE_BUFFER_SIZE));
        const vk::VkDescriptorPoolSize poolSizes = {
            vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // VkDescriptorType type;
            descriptorCount                        // uint32_t descriptorCount;
        };
        const vk::VkDescriptorPoolCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,     // VkStructureType sType;
            nullptr,                                               // const void* pNext;
            vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // VkDescriptorPoolCreateFlags flags;
            descriptorCount,                                       // uint32_t maxSets;
            1u,                                                    // uint32_t poolSizeCount;
            &poolSizes,                                            // const VkDescriptorPoolSize* pPoolSizes;
        };

        m_descriptorPool = vk::createDescriptorPool(vkd, device, &createInfo);
        m_descriptorSets.resize(descriptorCount);
    }

    for (size_t descriptorSetNdx = 0; descriptorSetNdx < m_descriptorSets.size(); descriptorSetNdx++)
    {
        const vk::VkDescriptorSetLayout layout             = *m_resources.descriptorSetLayout;
        const vk::VkDescriptorSetAllocateInfo allocateInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType sType;
            nullptr,                                            // const void* pNext;
            *m_descriptorPool,                                  // VkDescriptorPool descriptorPool;
            1,                                                  // uint32_t descriptorSetCount;
            &layout                                             // const VkDescriptorSetLayout* pSetLayouts;
        };

        m_descriptorSets[descriptorSetNdx] = vk::allocateDescriptorSet(vkd, device, &allocateInfo).disown();

        {
            const vk::VkDescriptorBufferInfo bufferInfo = {
                context.getBuffer(),                        // VkBuffer buffer;
                descriptorSetNdx * MAX_STORAGE_BUFFER_SIZE, // VkDeviceSize offset;
                de::min(m_bufferSize - descriptorSetNdx * MAX_STORAGE_BUFFER_SIZE,
                        (vk::VkDeviceSize)MAX_STORAGE_BUFFER_SIZE) // VkDeviceSize range;
            };
            const vk::VkWriteDescriptorSet write = {
                vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType sType;
                nullptr,                                    // const void* pNext;
                m_descriptorSets[descriptorSetNdx],         // VkDescriptorSet dstSet;
                0u,                                         // uint32_t dstBinding;
                0u,                                         // uint32_t dstArrayElement;
                1u,                                         // uint32_t descriptorCount;
                vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,      // VkDescriptorType descriptorType;
                nullptr,                                    // const VkDescriptorImageInfo* pImageInfo;
                &bufferInfo,                                // const VkDescriptorBufferInfo* pBufferInfo;
                nullptr,                                    // const VkBufferView* pTexelBufferView;
            };

            vkd.updateDescriptorSets(device, 1u, &write, 0u, nullptr);
        }
    }
}

void RenderVertexStorageBuffer::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();

    vkd.cmdBindPipeline(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipeline);

    for (size_t descriptorSetNdx = 0; descriptorSetNdx < m_descriptorSets.size(); descriptorSetNdx++)
    {
        const size_t size = m_bufferSize < (descriptorSetNdx + 1) * MAX_STORAGE_BUFFER_SIZE ?
                                (size_t)(m_bufferSize - descriptorSetNdx * MAX_STORAGE_BUFFER_SIZE) :
                                (size_t)(MAX_STORAGE_BUFFER_SIZE);

        vkd.cmdBindDescriptorSets(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipelineLayout, 0u,
                                  1u, &m_descriptorSets[descriptorSetNdx], 0u, nullptr);
        vkd.cmdDraw(commandBuffer, (uint32_t)(size / 2), 1, 0, 0);
    }
}

void RenderVertexStorageBuffer::verify(VerifyRenderPassContext &context, size_t)
{
    for (size_t descriptorSetNdx = 0; descriptorSetNdx < m_descriptorSets.size(); descriptorSetNdx++)
    {
        const size_t offset = descriptorSetNdx * MAX_STORAGE_BUFFER_SIZE;
        const size_t size   = m_bufferSize < (descriptorSetNdx + 1) * MAX_STORAGE_BUFFER_SIZE ?
                                  (size_t)(m_bufferSize - descriptorSetNdx * MAX_STORAGE_BUFFER_SIZE) :
                                  (size_t)(MAX_STORAGE_BUFFER_SIZE);

        for (size_t pos = 0; pos < size / 2; pos++)
        {
            const uint8_t x = context.getReference().get(offset + pos * 2);
            const uint8_t y = context.getReference().get(offset + (pos * 2) + 1);

            context.getReferenceTarget().getAccess().setPixel(Vec4(1.0f, 1.0f, 1.0f, 1.0f), x, y);
        }
    }
}

class RenderVertexStorageTexelBuffer : public RenderPassCommand
{
public:
    RenderVertexStorageTexelBuffer(void)
    {
    }
    ~RenderVertexStorageTexelBuffer(void);

    const char *getName(void) const
    {
        return "RenderVertexStorageTexelBuffer";
    }
    void logPrepare(TestLog &, size_t) const;
    void logSubmit(TestLog &, size_t) const;
    void prepare(PrepareRenderPassContext &);
    void submit(SubmitContext &context);
    void verify(VerifyRenderPassContext &, size_t);

private:
    PipelineResources m_resources;
    vk::Move<vk::VkDescriptorPool> m_descriptorPool;
    vector<vk::VkDescriptorSet> m_descriptorSets;
    vector<vk::VkBufferView> m_bufferViews;

    const vk::DeviceInterface *m_vkd;
    vk::VkDevice m_device;
    vk::VkDeviceSize m_bufferSize;
    uint32_t m_maxStorageTexelCount;
};

RenderVertexStorageTexelBuffer::~RenderVertexStorageTexelBuffer(void)
{
    for (size_t bufferViewNdx = 0; bufferViewNdx < m_bufferViews.size(); bufferViewNdx++)
    {
        if (!!m_bufferViews[bufferViewNdx])
        {
            m_vkd->destroyBufferView(m_device, m_bufferViews[bufferViewNdx], nullptr);
            m_bufferViews[bufferViewNdx] = VK_NULL_HANDLE;
        }
    }
}

void RenderVertexStorageTexelBuffer::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName()
        << " Create pipeline for render buffer as storage buffer." << TestLog::EndMessage;
}

void RenderVertexStorageTexelBuffer::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Render using buffer as storage buffer."
        << TestLog::EndMessage;
}

void RenderVertexStorageTexelBuffer::prepare(PrepareRenderPassContext &context)
{
    const vk::InstanceInterface &vki          = context.getContext().getInstanceInterface();
    const vk::VkPhysicalDevice physicalDevice = context.getContext().getPhysicalDevice();
    const vk::DeviceInterface &vkd            = context.getContext().getDeviceInterface();
    const vk::VkDevice device                 = context.getContext().getDevice();
    const vk::VkRenderPass renderPass         = context.getRenderPass();
    const uint32_t subpass                    = 0;
    const vk::Unique<vk::VkShaderModule> vertexShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("storage-texel-buffer.vert"), 0));
    const vk::Unique<vk::VkShaderModule> fragmentShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("render-white.frag"), 0));
    vector<vk::VkDescriptorSetLayoutBinding> bindings;

    m_device               = device;
    m_vkd                  = &vkd;
    m_bufferSize           = context.getBufferSize();
    m_maxStorageTexelCount = vk::getPhysicalDeviceProperties(vki, physicalDevice).limits.maxTexelBufferElements;

    {
        const vk::VkDescriptorSetLayoutBinding binding = {0u, vk::VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1,
                                                          vk::VK_SHADER_STAGE_VERTEX_BIT, nullptr};

        bindings.push_back(binding);
    }

    createPipelineWithResources(vkd, device, renderPass, subpass, *vertexShaderModule, *fragmentShaderModule,
                                context.getTargetWidth(), context.getTargetHeight(),
                                vector<vk::VkVertexInputBindingDescription>(),
                                vector<vk::VkVertexInputAttributeDescription>(), bindings,
                                vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST, 0u, nullptr, m_resources);

    {
        const uint32_t descriptorCount =
            (uint32_t)(divRoundUp(m_bufferSize, (vk::VkDeviceSize)m_maxStorageTexelCount * (uint64_t)(4)));
        const vk::VkDescriptorPoolSize poolSizes = {
            vk::VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, // VkDescriptorType type;
            descriptorCount                              // uint32_t descriptorCount;
        };
        const vk::VkDescriptorPoolCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,     // VkStructureType sType;
            nullptr,                                               // const void* pNext;
            vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // VkDescriptorPoolCreateFlags flags;
            descriptorCount,                                       // uint32_t maxSets;
            1u,                                                    // uint32_t poolSizeCount;
            &poolSizes,                                            // const VkDescriptorPoolSize* pPoolSizes;
        };

        m_descriptorPool = vk::createDescriptorPool(vkd, device, &createInfo);
        m_descriptorSets.resize(descriptorCount, VK_NULL_HANDLE);
        m_bufferViews.resize(descriptorCount, VK_NULL_HANDLE);
    }

    for (size_t descriptorSetNdx = 0; descriptorSetNdx < m_descriptorSets.size(); descriptorSetNdx++)
    {
        const vk::VkDescriptorSetLayout layout             = *m_resources.descriptorSetLayout;
        const vk::VkDescriptorSetAllocateInfo allocateInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType sType;
            nullptr,                                            // const void* pNext;
            *m_descriptorPool,                                  // VkDescriptorPool descriptorPool;
            1,                                                  // uint32_t descriptorSetCount;
            &layout                                             // const VkDescriptorSetLayout* pSetLayouts;
        };

        m_descriptorSets[descriptorSetNdx] = vk::allocateDescriptorSet(vkd, device, &allocateInfo).disown();

        {
            const vk::VkBufferViewCreateInfo createInfo = {
                vk::VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,             // VkStructureType sType;
                nullptr,                                                   // const void* pNext;
                0u,                                                        // VkBufferViewCreateFlags flags;
                context.getBuffer(),                                       // VkBuffer buffer;
                vk::VK_FORMAT_R32_UINT,                                    // VkFormat format;
                descriptorSetNdx * m_maxStorageTexelCount * (uint64_t)(4), // VkDeviceSize offset;
                (uint32_t)de::min<vk::VkDeviceSize>(m_maxStorageTexelCount * (uint64_t)(4),
                                                    m_bufferSize - descriptorSetNdx * m_maxStorageTexelCount *
                                                                       (uint64_t)(4)) // VkDeviceSize range;
            };

            VK_CHECK(vkd.createBufferView(device, &createInfo, nullptr, &m_bufferViews[descriptorSetNdx]));
        }

        {
            const vk::VkWriteDescriptorSet write = {
                vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,  // VkStructureType sType;
                nullptr,                                     // const void* pNext;
                m_descriptorSets[descriptorSetNdx],          // VkDescriptorSet dstSet;
                0u,                                          // uint32_t dstBinding;
                0u,                                          // uint32_t dstArrayElement;
                1u,                                          // uint32_t descriptorCount;
                vk::VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, // VkDescriptorType descriptorType;
                nullptr,                                     // const VkDescriptorImageInfo* pImageInfo;
                nullptr,                                     // const VkDescriptorBufferInfo* pBufferInfo;
                &m_bufferViews[descriptorSetNdx]             // const VkBufferView* pTexelBufferView;
            };

            vkd.updateDescriptorSets(device, 1u, &write, 0u, nullptr);
        }
    }
}

void RenderVertexStorageTexelBuffer::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();

    vkd.cmdBindPipeline(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipeline);

    for (size_t descriptorSetNdx = 0; descriptorSetNdx < m_descriptorSets.size(); descriptorSetNdx++)
    {
        const uint32_t count =
            (uint32_t)(m_bufferSize < (descriptorSetNdx + 1) * m_maxStorageTexelCount * (uint64_t)(4) ?
                           m_bufferSize - descriptorSetNdx * m_maxStorageTexelCount * (uint64_t)(4) :
                           m_maxStorageTexelCount * (uint64_t)(4)) /
            2;

        vkd.cmdBindDescriptorSets(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipelineLayout, 0u,
                                  1u, &m_descriptorSets[descriptorSetNdx], 0u, nullptr);
        vkd.cmdDraw(commandBuffer, count, 1, 0, 0);
    }
}

void RenderVertexStorageTexelBuffer::verify(VerifyRenderPassContext &context, size_t)
{
    for (size_t descriptorSetNdx = 0; descriptorSetNdx < m_descriptorSets.size(); descriptorSetNdx++)
    {
        const uint64_t offset = descriptorSetNdx * m_maxStorageTexelCount * (uint64_t)(4);
        const uint32_t count =
            (uint32_t)(m_bufferSize < (descriptorSetNdx + 1) * m_maxStorageTexelCount * (uint64_t)(4) ?
                           m_bufferSize - descriptorSetNdx * m_maxStorageTexelCount * (uint64_t)(4) :
                           m_maxStorageTexelCount * (uint64_t)(4)) /
            2;

        DE_ASSERT(context.getReference().getSize() <= (uint64_t)(4) * m_maxStorageTexelCount * m_descriptorSets.size());
        DE_ASSERT(context.getReference().getSize() > offset);
        DE_ASSERT(offset + count * 2 <= context.getReference().getSize());

        for (size_t pos = 0; pos < (size_t)count; pos++)
        {
            const uint8_t x = context.getReference().get(offset + pos * 2);
            const uint8_t y = context.getReference().get(offset + (pos * 2) + 1);

            context.getReferenceTarget().getAccess().setPixel(Vec4(1.0f, 1.0f, 1.0f, 1.0f), x, y);
        }
    }
}

class RenderVertexStorageImage : public RenderPassCommand
{
public:
    RenderVertexStorageImage(void)
    {
    }
    ~RenderVertexStorageImage(void);

    const char *getName(void) const
    {
        return "RenderVertexStorageImage";
    }
    void logPrepare(TestLog &, size_t) const;
    void logSubmit(TestLog &, size_t) const;
    void prepare(PrepareRenderPassContext &);
    void submit(SubmitContext &context);
    void verify(VerifyRenderPassContext &, size_t);

private:
    PipelineResources m_resources;
    vk::Move<vk::VkDescriptorPool> m_descriptorPool;
    vk::Move<vk::VkDescriptorSet> m_descriptorSet;
    vk::Move<vk::VkImageView> m_imageView;
};

RenderVertexStorageImage::~RenderVertexStorageImage(void)
{
}

void RenderVertexStorageImage::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Create pipeline for render storage image."
        << TestLog::EndMessage;
}

void RenderVertexStorageImage::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Render using storage image."
        << TestLog::EndMessage;
}

void RenderVertexStorageImage::prepare(PrepareRenderPassContext &context)
{
    const vk::DeviceInterface &vkd    = context.getContext().getDeviceInterface();
    const vk::VkDevice device         = context.getContext().getDevice();
    const vk::VkRenderPass renderPass = context.getRenderPass();
    const uint32_t subpass            = 0;
    const vk::Unique<vk::VkShaderModule> vertexShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("storage-image.vert"), 0));
    const vk::Unique<vk::VkShaderModule> fragmentShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("render-white.frag"), 0));
    vector<vk::VkDescriptorSetLayoutBinding> bindings;

    {
        const vk::VkDescriptorSetLayoutBinding binding = {0u, vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                                                          vk::VK_SHADER_STAGE_VERTEX_BIT, nullptr};

        bindings.push_back(binding);
    }

    createPipelineWithResources(vkd, device, renderPass, subpass, *vertexShaderModule, *fragmentShaderModule,
                                context.getTargetWidth(), context.getTargetHeight(),
                                vector<vk::VkVertexInputBindingDescription>(),
                                vector<vk::VkVertexInputAttributeDescription>(), bindings,
                                vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST, 0u, nullptr, m_resources);

    {
        const vk::VkDescriptorPoolSize poolSizes = {
            vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // VkDescriptorType type;
            1                                     // uint32_t descriptorCount;
        };
        const vk::VkDescriptorPoolCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,     // VkStructureType sType;
            nullptr,                                               // const void* pNext;
            vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // VkDescriptorPoolCreateFlags flags;
            1u,                                                    // uint32_t maxSets;
            1u,                                                    // uint32_t poolSizeCount;
            &poolSizes,                                            // const VkDescriptorPoolSize* pPoolSizes;
        };

        m_descriptorPool = vk::createDescriptorPool(vkd, device, &createInfo);
    }

    {
        const vk::VkDescriptorSetLayout layout             = *m_resources.descriptorSetLayout;
        const vk::VkDescriptorSetAllocateInfo allocateInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType sType;
            nullptr,                                            // const void* pNext;
            *m_descriptorPool,                                  // VkDescriptorPool descriptorPool;
            1,                                                  // uint32_t descriptorSetCount;
            &layout                                             // const VkDescriptorSetLayout* pSetLayouts;
        };

        m_descriptorSet = vk::allocateDescriptorSet(vkd, device, &allocateInfo);

        {
            const vk::VkImageViewCreateInfo createInfo = {
                vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,   // VkStructureType sType;
                nullptr,                                        // const void* pNext;
                0u,                                             // VkImageViewCreateFlags flags;
                context.getImage(),                             // VkImage image;
                vk::VK_IMAGE_VIEW_TYPE_2D,                      // VkImageViewType viewType;
                vk::VK_FORMAT_R8G8B8A8_UNORM,                   // VkFormat format;
                vk::makeComponentMappingRGBA(),                 // VkComponentMapping components;
                {vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
            };

            m_imageView = vk::createImageView(vkd, device, &createInfo);
        }

        {
            const vk::VkDescriptorImageInfo imageInfo = {
                VK_NULL_HANDLE,          // VkSampler sampler;
                *m_imageView,            // VkImageView imageView;
                context.getImageLayout() // VkImageLayout imageLayout;
            };
            const vk::VkWriteDescriptorSet write = {
                vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType sType;
                nullptr,                                    // const void* pNext;
                *m_descriptorSet,                           // VkDescriptorSet dstSet;
                0u,                                         // uint32_t dstBinding;
                0u,                                         // uint32_t dstArrayElement;
                1u,                                         // uint32_t descriptorCount;
                vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,       // VkDescriptorType descriptorType;
                &imageInfo,                                 // const VkDescriptorImageInfo* pImageInfo;
                nullptr,                                    // const VkDescriptorBufferInfo* pBufferInfo;
                nullptr,                                    // const VkBufferView* pTexelBufferView;
            };

            vkd.updateDescriptorSets(device, 1u, &write, 0u, nullptr);
        }
    }
}

void RenderVertexStorageImage::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();

    vkd.cmdBindPipeline(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipeline);

    vkd.cmdBindDescriptorSets(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipelineLayout, 0u, 1u,
                              &(*m_descriptorSet), 0u, nullptr);
    vkd.cmdDraw(commandBuffer, context.getImageWidth() * context.getImageHeight() * 2, 1, 0, 0);
}

void RenderVertexStorageImage::verify(VerifyRenderPassContext &context, size_t)
{
    for (int pos = 0; pos < (int)(context.getReferenceImage().getWidth() * context.getReferenceImage().getHeight() * 2);
         pos++)
    {
        const tcu::IVec3 size = context.getReferenceImage().getAccess().getSize();
        const tcu::UVec4 pixel =
            context.getReferenceImage().getAccess().getPixelUint((pos / 2) / size.x(), (pos / 2) % size.x());

        if (pos % 2 == 0)
            context.getReferenceTarget().getAccess().setPixel(Vec4(1.0f, 1.0f, 1.0f, 1.0f), pixel.x(), pixel.y());
        else
            context.getReferenceTarget().getAccess().setPixel(Vec4(1.0f, 1.0f, 1.0f, 1.0f), pixel.z(), pixel.w());
    }
}

class RenderVertexSampledImage : public RenderPassCommand
{
public:
    RenderVertexSampledImage(void)
    {
    }
    ~RenderVertexSampledImage(void);

    const char *getName(void) const
    {
        return "RenderVertexSampledImage";
    }
    void logPrepare(TestLog &, size_t) const;
    void logSubmit(TestLog &, size_t) const;
    void prepare(PrepareRenderPassContext &);
    void submit(SubmitContext &context);
    void verify(VerifyRenderPassContext &, size_t);

private:
    PipelineResources m_resources;
    vk::Move<vk::VkDescriptorPool> m_descriptorPool;
    vk::Move<vk::VkDescriptorSet> m_descriptorSet;
    vk::Move<vk::VkImageView> m_imageView;
    vk::Move<vk::VkSampler> m_sampler;
};

RenderVertexSampledImage::~RenderVertexSampledImage(void)
{
}

void RenderVertexSampledImage::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Create pipeline for render sampled image."
        << TestLog::EndMessage;
}

void RenderVertexSampledImage::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Render using sampled image."
        << TestLog::EndMessage;
}

void RenderVertexSampledImage::prepare(PrepareRenderPassContext &context)
{
    const vk::DeviceInterface &vkd    = context.getContext().getDeviceInterface();
    const vk::VkDevice device         = context.getContext().getDevice();
    const vk::VkRenderPass renderPass = context.getRenderPass();
    const uint32_t subpass            = 0;
    const vk::Unique<vk::VkShaderModule> vertexShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("sampled-image.vert"), 0));
    const vk::Unique<vk::VkShaderModule> fragmentShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("render-white.frag"), 0));
    vector<vk::VkDescriptorSetLayoutBinding> bindings;

    {
        const vk::VkDescriptorSetLayoutBinding binding = {0u, vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                                          vk::VK_SHADER_STAGE_VERTEX_BIT, nullptr};

        bindings.push_back(binding);
    }

    createPipelineWithResources(vkd, device, renderPass, subpass, *vertexShaderModule, *fragmentShaderModule,
                                context.getTargetWidth(), context.getTargetHeight(),
                                vector<vk::VkVertexInputBindingDescription>(),
                                vector<vk::VkVertexInputAttributeDescription>(), bindings,
                                vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST, 0u, nullptr, m_resources);

    {
        const vk::VkDescriptorPoolSize poolSizes = {
            vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, // VkDescriptorType type;
            1                                              // uint32_t descriptorCount;
        };
        const vk::VkDescriptorPoolCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,     // VkStructureType sType;
            nullptr,                                               // const void* pNext;
            vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // VkDescriptorPoolCreateFlags flags;
            1u,                                                    // uint32_t maxSets;
            1u,                                                    // uint32_t poolSizeCount;
            &poolSizes,                                            // const VkDescriptorPoolSize* pPoolSizes;
        };

        m_descriptorPool = vk::createDescriptorPool(vkd, device, &createInfo);
    }

    {
        const vk::VkDescriptorSetLayout layout             = *m_resources.descriptorSetLayout;
        const vk::VkDescriptorSetAllocateInfo allocateInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType sType;
            nullptr,                                            // const void* pNext;
            *m_descriptorPool,                                  // VkDescriptorPool descriptorPool;
            1,                                                  // uint32_t descriptorSetCount;
            &layout                                             // const VkDescriptorSetLayout* pSetLayouts;
        };

        m_descriptorSet = vk::allocateDescriptorSet(vkd, device, &allocateInfo);

        {
            const vk::VkImageViewCreateInfo createInfo = {
                vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,   // VkStructureType sType;
                nullptr,                                        // const void* pNext;
                0u,                                             // VkImageViewCreateFlags flags;
                context.getImage(),                             // VkImage image;
                vk::VK_IMAGE_VIEW_TYPE_2D,                      // VkImageViewType viewType;
                vk::VK_FORMAT_R8G8B8A8_UNORM,                   // VkFormat format;
                vk::makeComponentMappingRGBA(),                 // VkComponentMapping components;
                {vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
            };

            m_imageView = vk::createImageView(vkd, device, &createInfo);
        }

        {
            const vk::VkSamplerCreateInfo createInfo = {
                vk::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,   // VkStructureType sType;
                nullptr,                                     // const void* pNext;
                0u,                                          // VkSamplerCreateFlags flags;
                vk::VK_FILTER_NEAREST,                       // VkFilter magFilter;
                vk::VK_FILTER_NEAREST,                       // VkFilter minFilter;
                vk::VK_SAMPLER_MIPMAP_MODE_LINEAR,           // VkSamplerMipmapMode mipmapMode;
                vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeU;
                vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeV;
                vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeW;
                0.0f,                                        // float mipLodBias;
                VK_FALSE,                                    // VkBool32 anisotropyEnable;
                1.0f,                                        // float maxAnisotropy;
                VK_FALSE,                                    // VkBool32 compareEnable;
                vk::VK_COMPARE_OP_ALWAYS,                    // VkCompareOp compareOp;
                0.0f,                                        // float minLod;
                0.0f,                                        // float maxLod;
                vk::VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, // VkBorderColor borderColor;
                VK_FALSE                                     // VkBool32 unnormalizedCoordinates;
            };

            m_sampler = vk::createSampler(vkd, device, &createInfo);
        }

        {
            const vk::VkDescriptorImageInfo imageInfo = {
                *m_sampler,              // VkSampler sampler;
                *m_imageView,            // VkImageView imageView;
                context.getImageLayout() // VkImageLayout imageLayout;
            };
            const vk::VkWriteDescriptorSet write = {
                vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,    // VkStructureType sType;
                nullptr,                                       // const void* pNext;
                *m_descriptorSet,                              // VkDescriptorSet dstSet;
                0u,                                            // uint32_t dstBinding;
                0u,                                            // uint32_t dstArrayElement;
                1u,                                            // uint32_t descriptorCount;
                vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, // VkDescriptorType descriptorType;
                &imageInfo,                                    // const VkDescriptorImageInfo* pImageInfo;
                nullptr,                                       // const VkDescriptorBufferInfo* pBufferInfo;
                nullptr,                                       // const VkBufferView* pTexelBufferView;
            };

            vkd.updateDescriptorSets(device, 1u, &write, 0u, nullptr);
        }
    }
}

void RenderVertexSampledImage::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();

    vkd.cmdBindPipeline(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipeline);

    vkd.cmdBindDescriptorSets(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipelineLayout, 0u, 1u,
                              &(*m_descriptorSet), 0u, nullptr);
    vkd.cmdDraw(commandBuffer, context.getImageWidth() * context.getImageHeight() * 2, 1, 0, 0);
}

void RenderVertexSampledImage::verify(VerifyRenderPassContext &context, size_t)
{
    for (int pos = 0; pos < (int)(context.getReferenceImage().getWidth() * context.getReferenceImage().getHeight() * 2);
         pos++)
    {
        const tcu::IVec3 size = context.getReferenceImage().getAccess().getSize();
        const tcu::UVec4 pixel =
            context.getReferenceImage().getAccess().getPixelUint((pos / 2) / size.x(), (pos / 2) % size.x());

        if (pos % 2 == 0)
            context.getReferenceTarget().getAccess().setPixel(Vec4(1.0f, 1.0f, 1.0f, 1.0f), pixel.x(), pixel.y());
        else
            context.getReferenceTarget().getAccess().setPixel(Vec4(1.0f, 1.0f, 1.0f, 1.0f), pixel.z(), pixel.w());
    }
}

class RenderFragmentUniformBuffer : public RenderPassCommand
{
public:
    RenderFragmentUniformBuffer(void)
    {
    }
    ~RenderFragmentUniformBuffer(void);

    const char *getName(void) const
    {
        return "RenderFragmentUniformBuffer";
    }
    void logPrepare(TestLog &, size_t) const;
    void logSubmit(TestLog &, size_t) const;
    void prepare(PrepareRenderPassContext &);
    void submit(SubmitContext &context);
    void verify(VerifyRenderPassContext &, size_t);

protected:
    uint32_t calculateBufferPartSize(size_t descriptorSetNdx) const;

private:
    PipelineResources m_resources;
    vk::Move<vk::VkDescriptorPool> m_descriptorPool;
    vector<vk::VkDescriptorSet> m_descriptorSets;

    vk::VkDeviceSize m_bufferSize;
    size_t m_targetWidth;
    size_t m_targetHeight;
    uint32_t m_valuesPerPixel;
};

RenderFragmentUniformBuffer::~RenderFragmentUniformBuffer(void)
{
}

void RenderFragmentUniformBuffer::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName()
        << " Create pipeline for render buffer as uniform buffer." << TestLog::EndMessage;
}

void RenderFragmentUniformBuffer::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Render using buffer as uniform buffer."
        << TestLog::EndMessage;
}

void RenderFragmentUniformBuffer::prepare(PrepareRenderPassContext &context)
{
    const vk::DeviceInterface &vkd    = context.getContext().getDeviceInterface();
    const vk::VkDevice device         = context.getContext().getDevice();
    const vk::VkRenderPass renderPass = context.getRenderPass();
    const uint32_t subpass            = 0;
    const vk::Unique<vk::VkShaderModule> vertexShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("render-quad.vert"), 0));
    const vk::Unique<vk::VkShaderModule> fragmentShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("uniform-buffer.frag"), 0));
    vector<vk::VkDescriptorSetLayoutBinding> bindings;

    // make sure buffer is smaller then MAX_SIZE and is multiple of 16 (in glsl we use uvec4 to store 16 values)
    m_bufferSize   = de::min(context.getBufferSize(), (vk::VkDeviceSize)MAX_SIZE);
    m_bufferSize   = static_cast<vk::VkDeviceSize>(m_bufferSize / 16u) * 16u;
    m_targetWidth  = context.getTargetWidth();
    m_targetHeight = context.getTargetHeight();

    {
        const vk::VkDescriptorSetLayoutBinding binding = {0u, vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                                          vk::VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};

        bindings.push_back(binding);
    }
    const vk::VkPushConstantRange pushConstantRange = {vk::VK_SHADER_STAGE_FRAGMENT_BIT, 0u, 12u};

    createPipelineWithResources(vkd, device, renderPass, subpass, *vertexShaderModule, *fragmentShaderModule,
                                context.getTargetWidth(), context.getTargetHeight(),
                                vector<vk::VkVertexInputBindingDescription>(),
                                vector<vk::VkVertexInputAttributeDescription>(), bindings,
                                vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1u, &pushConstantRange, m_resources);

    {
        const uint32_t descriptorCount =
            (uint32_t)(divRoundUp(m_bufferSize, (vk::VkDeviceSize)MAX_UNIFORM_BUFFER_SIZE));
        const vk::VkDescriptorPoolSize poolSizes = {
            vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // VkDescriptorType type;
            descriptorCount                        // uint32_t descriptorCount;
        };
        const vk::VkDescriptorPoolCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,     // VkStructureType sType;
            nullptr,                                               // const void* pNext;
            vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // VkDescriptorPoolCreateFlags flags;
            descriptorCount,                                       // uint32_t maxSets;
            1u,                                                    // uint32_t poolSizeCount;
            &poolSizes,                                            // const VkDescriptorPoolSize* pPoolSizes;
        };

        m_descriptorPool = vk::createDescriptorPool(vkd, device, &createInfo);
        m_descriptorSets.resize(descriptorCount);

        m_valuesPerPixel = (uint32_t)divRoundUp<size_t>(
            descriptorCount * de::min<size_t>((size_t)m_bufferSize / 4, MAX_UNIFORM_BUFFER_SIZE / 4),
            m_targetWidth * m_targetHeight);
    }

    for (size_t descriptorSetNdx = 0; descriptorSetNdx < m_descriptorSets.size(); descriptorSetNdx++)
    {
        const vk::VkDescriptorSetLayout layout             = *m_resources.descriptorSetLayout;
        const vk::VkDescriptorSetAllocateInfo allocateInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType sType;
            nullptr,                                            // const void* pNext;
            *m_descriptorPool,                                  // VkDescriptorPool descriptorPool;
            1,                                                  // uint32_t descriptorSetCount;
            &layout                                             // const VkDescriptorSetLayout* pSetLayouts;
        };

        m_descriptorSets[descriptorSetNdx] = vk::allocateDescriptorSet(vkd, device, &allocateInfo).disown();

        {
            const vk::VkDescriptorBufferInfo bufferInfo = {
                context.getBuffer(),                                                    // VkBuffer buffer;
                (vk::VkDeviceSize)(descriptorSetNdx * (size_t)MAX_UNIFORM_BUFFER_SIZE), // VkDeviceSize offset;
                calculateBufferPartSize(descriptorSetNdx)                               // VkDeviceSize range;
            };
            const vk::VkWriteDescriptorSet write = {
                vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType sType;
                nullptr,                                    // const void* pNext;
                m_descriptorSets[descriptorSetNdx],         // VkDescriptorSet dstSet;
                0u,                                         // uint32_t dstBinding;
                0u,                                         // uint32_t dstArrayElement;
                1u,                                         // uint32_t descriptorCount;
                vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,      // VkDescriptorType descriptorType;
                nullptr,                                    // const VkDescriptorImageInfo* pImageInfo;
                &bufferInfo,                                // const VkDescriptorBufferInfo* pBufferInfo;
                nullptr,                                    // const VkBufferView* pTexelBufferView;
            };

            vkd.updateDescriptorSets(device, 1u, &write, 0u, nullptr);
        }
    }
}

void RenderFragmentUniformBuffer::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();

    vkd.cmdBindPipeline(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipeline);

    for (size_t descriptorSetNdx = 0; descriptorSetNdx < m_descriptorSets.size(); descriptorSetNdx++)
    {
        const struct
        {
            const uint32_t callId;
            const uint32_t valuesPerPixel;
            const uint32_t bufferSize;
        } callParams = {(uint32_t)descriptorSetNdx, m_valuesPerPixel, calculateBufferPartSize(descriptorSetNdx) / 16u};

        vkd.cmdBindDescriptorSets(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipelineLayout, 0u,
                                  1u, &m_descriptorSets[descriptorSetNdx], 0u, nullptr);
        vkd.cmdPushConstants(commandBuffer, *m_resources.pipelineLayout, vk::VK_SHADER_STAGE_FRAGMENT_BIT, 0u,
                             (uint32_t)sizeof(callParams), &callParams);
        vkd.cmdDraw(commandBuffer, 6, 1, 0, 0);
    }
}

void RenderFragmentUniformBuffer::verify(VerifyRenderPassContext &context, size_t)
{
    const size_t arrayIntSize = MAX_UNIFORM_BUFFER_SIZE / sizeof(uint32_t);

    for (int y = 0; y < context.getReferenceTarget().getSize().y(); y++)
        for (int x = 0; x < context.getReferenceTarget().getSize().x(); x++)
        {
            const uint32_t id = (uint32_t)y * 256u + (uint32_t)x;
            const size_t firstDescriptorSetNdx =
                de::min<size_t>(id / (arrayIntSize / m_valuesPerPixel), m_descriptorSets.size() - 1);

            for (size_t descriptorSetNdx = firstDescriptorSetNdx; descriptorSetNdx < m_descriptorSets.size();
                 descriptorSetNdx++)
            {
                const size_t offset   = descriptorSetNdx * MAX_UNIFORM_BUFFER_SIZE;
                const uint32_t callId = (uint32_t)descriptorSetNdx;
                const uint32_t count  = calculateBufferPartSize(descriptorSetNdx) / 16u;

                if (id < callId * (arrayIntSize / m_valuesPerPixel))
                    continue;
                else
                {
                    uint32_t value = id;

                    for (uint32_t i = 0; i < m_valuesPerPixel; i++)
                    {
                        // in shader UBO has up to 64 items of uvec4, each uvec4 contains 16 values
                        size_t index = offset + size_t((value % count) * 16u) + size_t((value % 4u) * 4u);
                        value        = (((uint32_t)context.getReference().get(index + 0))) |
                                (((uint32_t)context.getReference().get(index + 1)) << 8u) |
                                (((uint32_t)context.getReference().get(index + 2)) << 16u) |
                                (((uint32_t)context.getReference().get(index + 3)) << 24u);
                    }
                    const UVec4 vec((value >> 0u) & 0xFFu, (value >> 8u) & 0xFFu, (value >> 16u) & 0xFFu,
                                    (value >> 24u) & 0xFFu);

                    context.getReferenceTarget().getAccess().setPixel(vec.asFloat() / Vec4(255.0f), x, y);
                }
            }
        }
}

uint32_t RenderFragmentUniformBuffer::calculateBufferPartSize(size_t descriptorSetNdx) const
{
    uint32_t size =
        static_cast<uint32_t>(m_bufferSize) - static_cast<uint32_t>(descriptorSetNdx) * MAX_UNIFORM_BUFFER_SIZE;
    if (size < MAX_UNIFORM_BUFFER_SIZE)
        return size;
    return MAX_UNIFORM_BUFFER_SIZE;
}

class RenderFragmentStorageBuffer : public RenderPassCommand
{
public:
    RenderFragmentStorageBuffer(void)
    {
    }
    ~RenderFragmentStorageBuffer(void);

    const char *getName(void) const
    {
        return "RenderFragmentStorageBuffer";
    }
    void logPrepare(TestLog &, size_t) const;
    void logSubmit(TestLog &, size_t) const;
    void prepare(PrepareRenderPassContext &);
    void submit(SubmitContext &context);
    void verify(VerifyRenderPassContext &, size_t);

private:
    PipelineResources m_resources;
    vk::Move<vk::VkDescriptorPool> m_descriptorPool;
    vk::Move<vk::VkDescriptorSet> m_descriptorSet;

    vk::VkDeviceSize m_bufferSize;
    size_t m_targetWidth;
    size_t m_targetHeight;
};

RenderFragmentStorageBuffer::~RenderFragmentStorageBuffer(void)
{
}

void RenderFragmentStorageBuffer::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName()
        << " Create pipeline to render buffer as storage buffer." << TestLog::EndMessage;
}

void RenderFragmentStorageBuffer::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Render using buffer as storage buffer."
        << TestLog::EndMessage;
}

void RenderFragmentStorageBuffer::prepare(PrepareRenderPassContext &context)
{
    const vk::DeviceInterface &vkd    = context.getContext().getDeviceInterface();
    const vk::VkDevice device         = context.getContext().getDevice();
    const vk::VkRenderPass renderPass = context.getRenderPass();
    const uint32_t subpass            = 0;
    const vk::Unique<vk::VkShaderModule> vertexShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("render-quad.vert"), 0));
    const vk::Unique<vk::VkShaderModule> fragmentShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("storage-buffer.frag"), 0));
    vector<vk::VkDescriptorSetLayoutBinding> bindings;

    // make sure buffer size is multiple of 16 (in glsl we use uvec4 to store 16 values)
    m_bufferSize   = context.getBufferSize();
    m_bufferSize   = static_cast<vk::VkDeviceSize>(m_bufferSize / 16u) * 16u;
    m_targetWidth  = context.getTargetWidth();
    m_targetHeight = context.getTargetHeight();

    {
        const vk::VkDescriptorSetLayoutBinding binding = {0u, vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                                          vk::VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};

        bindings.push_back(binding);
    }
    const vk::VkPushConstantRange pushConstantRange = {vk::VK_SHADER_STAGE_FRAGMENT_BIT, 0u, 12u};

    createPipelineWithResources(vkd, device, renderPass, subpass, *vertexShaderModule, *fragmentShaderModule,
                                context.getTargetWidth(), context.getTargetHeight(),
                                vector<vk::VkVertexInputBindingDescription>(),
                                vector<vk::VkVertexInputAttributeDescription>(), bindings,
                                vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1u, &pushConstantRange, m_resources);

    {
        const uint32_t descriptorCount           = 1;
        const vk::VkDescriptorPoolSize poolSizes = {
            vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // VkDescriptorType type;
            descriptorCount                        // uint32_t descriptorCount;
        };
        const vk::VkDescriptorPoolCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,     // VkStructureType sType;
            nullptr,                                               // const void* pNext;
            vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // VkDescriptorPoolCreateFlags flags;
            descriptorCount,                                       // uint32_t maxSets;
            1u,                                                    // uint32_t poolSizeCount;
            &poolSizes,                                            // const VkDescriptorPoolSize* pPoolSizes;
        };

        m_descriptorPool = vk::createDescriptorPool(vkd, device, &createInfo);
    }

    {
        const vk::VkDescriptorSetLayout layout             = *m_resources.descriptorSetLayout;
        const vk::VkDescriptorSetAllocateInfo allocateInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType sType;
            nullptr,                                            // const void* pNext;
            *m_descriptorPool,                                  // VkDescriptorPool descriptorPool;
            1,                                                  // uint32_t descriptorSetCount;
            &layout                                             // const VkDescriptorSetLayout* pSetLayouts;
        };

        m_descriptorSet = vk::allocateDescriptorSet(vkd, device, &allocateInfo);

        {
            const vk::VkDescriptorBufferInfo bufferInfo = {
                context.getBuffer(), // VkBuffer buffer;
                0u,                  // VkDeviceSize offset;
                m_bufferSize         // VkDeviceSize range;
            };
            const vk::VkWriteDescriptorSet write = {
                vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType sType;
                nullptr,                                    // const void* pNext;
                m_descriptorSet.get(),                      // VkDescriptorSet dstSet;
                0u,                                         // uint32_t dstBinding;
                0u,                                         // uint32_t dstArrayElement;
                1u,                                         // uint32_t descriptorCount;
                vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,      // VkDescriptorType descriptorType;
                nullptr,                                    // const VkDescriptorImageInfo* pImageInfo;
                &bufferInfo,                                // const VkDescriptorBufferInfo* pBufferInfo;
                nullptr,                                    // const VkBufferView* pTexelBufferView;
            };

            vkd.updateDescriptorSets(device, 1u, &write, 0u, nullptr);
        }
    }
}

void RenderFragmentStorageBuffer::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();

    vkd.cmdBindPipeline(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipeline);

    const struct
    {
        const uint32_t valuesPerPixel;
        const uint32_t bufferSize;
    } callParams = {(uint32_t)divRoundUp<vk::VkDeviceSize>(m_bufferSize / 4, m_targetWidth * m_targetHeight),
                    (uint32_t)m_bufferSize};

    vkd.cmdBindDescriptorSets(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipelineLayout, 0u, 1u,
                              &m_descriptorSet.get(), 0u, nullptr);
    vkd.cmdPushConstants(commandBuffer, *m_resources.pipelineLayout, vk::VK_SHADER_STAGE_FRAGMENT_BIT, 0u,
                         (uint32_t)sizeof(callParams), &callParams);
    vkd.cmdDraw(commandBuffer, 6, 1, 0, 0);
}

void RenderFragmentStorageBuffer::verify(VerifyRenderPassContext &context, size_t)
{
    const uint32_t valuesPerPixel =
        (uint32_t)divRoundUp<vk::VkDeviceSize>(m_bufferSize / 4, m_targetWidth * m_targetHeight);

    for (int y = 0; y < context.getReferenceTarget().getSize().y(); y++)
        for (int x = 0; x < context.getReferenceTarget().getSize().x(); x++)
        {
            const uint32_t id = (uint32_t)y * 256u + (uint32_t)x;

            uint32_t value = id;

            for (uint32_t i = 0; i < valuesPerPixel; i++)
            {
                value =
                    (((uint32_t)context.getReference().get((size_t)(value % (m_bufferSize / sizeof(uint32_t))) * 4 + 0))
                     << 0u) |
                    (((uint32_t)context.getReference().get((size_t)(value % (m_bufferSize / sizeof(uint32_t))) * 4 + 1))
                     << 8u) |
                    (((uint32_t)context.getReference().get((size_t)(value % (m_bufferSize / sizeof(uint32_t))) * 4 + 2))
                     << 16u) |
                    (((uint32_t)context.getReference().get((size_t)(value % (m_bufferSize / sizeof(uint32_t))) * 4 + 3))
                     << 24u);
            }
            const UVec4 vec((value >> 0u) & 0xFFu, (value >> 8u) & 0xFFu, (value >> 16u) & 0xFFu,
                            (value >> 24u) & 0xFFu);

            context.getReferenceTarget().getAccess().setPixel(vec.asFloat() / Vec4(255.0f), x, y);
        }
}

class RenderFragmentUniformTexelBuffer : public RenderPassCommand
{
public:
    RenderFragmentUniformTexelBuffer(void)
    {
    }
    ~RenderFragmentUniformTexelBuffer(void);

    const char *getName(void) const
    {
        return "RenderFragmentUniformTexelBuffer";
    }
    void logPrepare(TestLog &, size_t) const;
    void logSubmit(TestLog &, size_t) const;
    void prepare(PrepareRenderPassContext &);
    void submit(SubmitContext &context);
    void verify(VerifyRenderPassContext &, size_t);

private:
    PipelineResources m_resources;
    vk::Move<vk::VkDescriptorPool> m_descriptorPool;
    vector<vk::VkDescriptorSet> m_descriptorSets;
    vector<vk::VkBufferView> m_bufferViews;

    const vk::DeviceInterface *m_vkd;
    vk::VkDevice m_device;
    vk::VkDeviceSize m_bufferSize;
    uint32_t m_maxUniformTexelCount;
    size_t m_targetWidth;
    size_t m_targetHeight;
};

RenderFragmentUniformTexelBuffer::~RenderFragmentUniformTexelBuffer(void)
{
    for (size_t bufferViewNdx = 0; bufferViewNdx < m_bufferViews.size(); bufferViewNdx++)
    {
        if (!!m_bufferViews[bufferViewNdx])
        {
            m_vkd->destroyBufferView(m_device, m_bufferViews[bufferViewNdx], nullptr);
            m_bufferViews[bufferViewNdx] = VK_NULL_HANDLE;
        }
    }
}

void RenderFragmentUniformTexelBuffer::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName()
        << " Create pipeline for render buffer as uniform buffer." << TestLog::EndMessage;
}

void RenderFragmentUniformTexelBuffer::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Render using buffer as uniform buffer."
        << TestLog::EndMessage;
}

void RenderFragmentUniformTexelBuffer::prepare(PrepareRenderPassContext &context)
{
    const vk::InstanceInterface &vki          = context.getContext().getInstanceInterface();
    const vk::VkPhysicalDevice physicalDevice = context.getContext().getPhysicalDevice();
    const vk::DeviceInterface &vkd            = context.getContext().getDeviceInterface();
    const vk::VkDevice device                 = context.getContext().getDevice();
    const vk::VkRenderPass renderPass         = context.getRenderPass();
    const uint32_t subpass                    = 0;
    const vk::Unique<vk::VkShaderModule> vertexShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("render-quad.vert"), 0));
    const vk::Unique<vk::VkShaderModule> fragmentShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("uniform-texel-buffer.frag"), 0));
    vector<vk::VkDescriptorSetLayoutBinding> bindings;

    m_device               = device;
    m_vkd                  = &vkd;
    m_bufferSize           = context.getBufferSize();
    m_maxUniformTexelCount = vk::getPhysicalDeviceProperties(vki, physicalDevice).limits.maxTexelBufferElements;
    m_targetWidth          = context.getTargetWidth();
    m_targetHeight         = context.getTargetHeight();

    {
        const vk::VkDescriptorSetLayoutBinding binding = {0u, vk::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1,
                                                          vk::VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};

        bindings.push_back(binding);
    }
    const vk::VkPushConstantRange pushConstantRange = {vk::VK_SHADER_STAGE_FRAGMENT_BIT, 0u, 12u};

    createPipelineWithResources(vkd, device, renderPass, subpass, *vertexShaderModule, *fragmentShaderModule,
                                context.getTargetWidth(), context.getTargetHeight(),
                                vector<vk::VkVertexInputBindingDescription>(),
                                vector<vk::VkVertexInputAttributeDescription>(), bindings,
                                vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1u, &pushConstantRange, m_resources);

    {
        const uint32_t descriptorCount =
            (uint32_t)(divRoundUp(m_bufferSize, (vk::VkDeviceSize)m_maxUniformTexelCount * 4));
        const vk::VkDescriptorPoolSize poolSizes = {
            vk::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, // VkDescriptorType type;
            descriptorCount                              // uint32_t descriptorCount;
        };
        const vk::VkDescriptorPoolCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,     // VkStructureType sType;
            nullptr,                                               // const void* pNext;
            vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // VkDescriptorPoolCreateFlags flags;
            descriptorCount,                                       // uint32_t maxSets;
            1u,                                                    // uint32_t poolSizeCount;
            &poolSizes,                                            // const VkDescriptorPoolSize* pPoolSizes;
        };

        m_descriptorPool = vk::createDescriptorPool(vkd, device, &createInfo);
        m_descriptorSets.resize(descriptorCount, VK_NULL_HANDLE);
        m_bufferViews.resize(descriptorCount, VK_NULL_HANDLE);
    }

    for (size_t descriptorSetNdx = 0; descriptorSetNdx < m_descriptorSets.size(); descriptorSetNdx++)
    {
        const uint32_t count = (uint32_t)(m_bufferSize < (descriptorSetNdx + 1) * m_maxUniformTexelCount * 4 ?
                                              m_bufferSize - descriptorSetNdx * m_maxUniformTexelCount * 4 :
                                              m_maxUniformTexelCount * 4) /
                               4;
        const vk::VkDescriptorSetLayout layout             = *m_resources.descriptorSetLayout;
        const vk::VkDescriptorSetAllocateInfo allocateInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType sType;
            nullptr,                                            // const void* pNext;
            *m_descriptorPool,                                  // VkDescriptorPool descriptorPool;
            1,                                                  // uint32_t descriptorSetCount;
            &layout                                             // const VkDescriptorSetLayout* pSetLayouts;
        };

        m_descriptorSets[descriptorSetNdx] = vk::allocateDescriptorSet(vkd, device, &allocateInfo).disown();

        {
            const vk::VkBufferViewCreateInfo createInfo = {
                vk::VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, // VkStructureType sType;
                nullptr,                                       // const void* pNext;
                0u,                                            // VkBufferViewCreateFlags flags;
                context.getBuffer(),                           // VkBuffer buffer;
                vk::VK_FORMAT_R32_UINT,                        // VkFormat format;
                descriptorSetNdx * m_maxUniformTexelCount * 4, // VkDeviceSize offset;
                count * 4                                      // VkDeviceSize range;
            };

            VK_CHECK(vkd.createBufferView(device, &createInfo, nullptr, &m_bufferViews[descriptorSetNdx]));
        }

        {
            const vk::VkWriteDescriptorSet write = {
                vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,  // VkStructureType sType;
                nullptr,                                     // const void* pNext;
                m_descriptorSets[descriptorSetNdx],          // VkDescriptorSet dstSet;
                0u,                                          // uint32_t dstBinding;
                0u,                                          // uint32_t dstArrayElement;
                1u,                                          // uint32_t descriptorCount;
                vk::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, // VkDescriptorType descriptorType;
                nullptr,                                     // const VkDescriptorImageInfo* pImageInfo;
                nullptr,                                     // const VkDescriptorBufferInfo* pBufferInfo;
                &m_bufferViews[descriptorSetNdx]             // const VkBufferView* pTexelBufferView;
            };

            vkd.updateDescriptorSets(device, 1u, &write, 0u, nullptr);
        }
    }
}

void RenderFragmentUniformTexelBuffer::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();

    vkd.cmdBindPipeline(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipeline);

    for (size_t descriptorSetNdx = 0; descriptorSetNdx < m_descriptorSets.size(); descriptorSetNdx++)
    {
        const struct
        {
            const uint32_t callId;
            const uint32_t valuesPerPixel;
            const uint32_t maxUniformTexelCount;
        } callParams = {(uint32_t)descriptorSetNdx,
                        (uint32_t)divRoundUp<size_t>(
                            m_descriptorSets.size() * de::min<size_t>((size_t)m_bufferSize / 4, m_maxUniformTexelCount),
                            m_targetWidth * m_targetHeight),
                        m_maxUniformTexelCount};

        vkd.cmdBindDescriptorSets(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipelineLayout, 0u,
                                  1u, &m_descriptorSets[descriptorSetNdx], 0u, nullptr);
        vkd.cmdPushConstants(commandBuffer, *m_resources.pipelineLayout, vk::VK_SHADER_STAGE_FRAGMENT_BIT, 0u,
                             (uint32_t)sizeof(callParams), &callParams);
        vkd.cmdDraw(commandBuffer, 6, 1, 0, 0);
    }
}

void RenderFragmentUniformTexelBuffer::verify(VerifyRenderPassContext &context, size_t)
{
    const uint32_t valuesPerPixel = (uint32_t)divRoundUp<size_t>(
        m_descriptorSets.size() * de::min<size_t>((size_t)m_bufferSize / 4, m_maxUniformTexelCount),
        m_targetWidth * m_targetHeight);

    for (int y = 0; y < context.getReferenceTarget().getSize().y(); y++)
        for (int x = 0; x < context.getReferenceTarget().getSize().x(); x++)
        {
            const size_t firstDescriptorSetNdx = de::min<size_t>(
                (y * 256u + x) / (m_maxUniformTexelCount / valuesPerPixel), m_descriptorSets.size() - 1);

            for (size_t descriptorSetNdx = firstDescriptorSetNdx; descriptorSetNdx < m_descriptorSets.size();
                 descriptorSetNdx++)
            {
                const size_t offset   = descriptorSetNdx * m_maxUniformTexelCount * 4;
                const uint32_t callId = (uint32_t)descriptorSetNdx;

                const uint32_t id    = (uint32_t)y * 256u + (uint32_t)x;
                const uint32_t count = (uint32_t)(m_bufferSize < (descriptorSetNdx + 1) * m_maxUniformTexelCount * 4 ?
                                                      m_bufferSize - descriptorSetNdx * m_maxUniformTexelCount * 4 :
                                                      m_maxUniformTexelCount * 4) /
                                       4;

                if (y * 256u + x < callId * (m_maxUniformTexelCount / valuesPerPixel))
                    continue;
                else
                {
                    uint32_t value = id;

                    for (uint32_t i = 0; i < valuesPerPixel; i++)
                    {
                        value = ((uint32_t)context.getReference().get(offset + (value % count) * 4 + 0)) |
                                (((uint32_t)context.getReference().get(offset + (value % count) * 4 + 1)) << 8u) |
                                (((uint32_t)context.getReference().get(offset + (value % count) * 4 + 2)) << 16u) |
                                (((uint32_t)context.getReference().get(offset + (value % count) * 4 + 3)) << 24u);
                    }
                    const UVec4 vec((value >> 0u) & 0xFFu, (value >> 8u) & 0xFFu, (value >> 16u) & 0xFFu,
                                    (value >> 24u) & 0xFFu);

                    context.getReferenceTarget().getAccess().setPixel(vec.asFloat() / Vec4(255.0f), x, y);
                }
            }
        }
}

class RenderFragmentStorageTexelBuffer : public RenderPassCommand
{
public:
    RenderFragmentStorageTexelBuffer(void)
    {
    }
    ~RenderFragmentStorageTexelBuffer(void);

    const char *getName(void) const
    {
        return "RenderFragmentStorageTexelBuffer";
    }
    void logPrepare(TestLog &, size_t) const;
    void logSubmit(TestLog &, size_t) const;
    void prepare(PrepareRenderPassContext &);
    void submit(SubmitContext &context);
    void verify(VerifyRenderPassContext &, size_t);

private:
    PipelineResources m_resources;
    vk::Move<vk::VkDescriptorPool> m_descriptorPool;
    vector<vk::VkDescriptorSet> m_descriptorSets;
    vector<vk::VkBufferView> m_bufferViews;

    const vk::DeviceInterface *m_vkd;
    vk::VkDevice m_device;
    vk::VkDeviceSize m_bufferSize;
    uint32_t m_maxStorageTexelCount;
    size_t m_targetWidth;
    size_t m_targetHeight;
};

RenderFragmentStorageTexelBuffer::~RenderFragmentStorageTexelBuffer(void)
{
    for (size_t bufferViewNdx = 0; bufferViewNdx < m_bufferViews.size(); bufferViewNdx++)
    {
        if (!!m_bufferViews[bufferViewNdx])
        {
            m_vkd->destroyBufferView(m_device, m_bufferViews[bufferViewNdx], nullptr);
            m_bufferViews[bufferViewNdx] = VK_NULL_HANDLE;
        }
    }
}

void RenderFragmentStorageTexelBuffer::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName()
        << " Create pipeline for render buffer as storage buffer." << TestLog::EndMessage;
}

void RenderFragmentStorageTexelBuffer::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Render using buffer as storage buffer."
        << TestLog::EndMessage;
}

void RenderFragmentStorageTexelBuffer::prepare(PrepareRenderPassContext &context)
{
    const vk::InstanceInterface &vki          = context.getContext().getInstanceInterface();
    const vk::VkPhysicalDevice physicalDevice = context.getContext().getPhysicalDevice();
    const vk::DeviceInterface &vkd            = context.getContext().getDeviceInterface();
    const vk::VkDevice device                 = context.getContext().getDevice();
    const vk::VkRenderPass renderPass         = context.getRenderPass();
    const uint32_t subpass                    = 0;
    const vk::Unique<vk::VkShaderModule> vertexShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("render-quad.vert"), 0));
    const vk::Unique<vk::VkShaderModule> fragmentShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("storage-texel-buffer.frag"), 0));
    vector<vk::VkDescriptorSetLayoutBinding> bindings;

    m_device               = device;
    m_vkd                  = &vkd;
    m_bufferSize           = context.getBufferSize();
    m_maxStorageTexelCount = vk::getPhysicalDeviceProperties(vki, physicalDevice).limits.maxTexelBufferElements;
    m_targetWidth          = context.getTargetWidth();
    m_targetHeight         = context.getTargetHeight();

    {
        const vk::VkDescriptorSetLayoutBinding binding = {0u, vk::VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1,
                                                          vk::VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};

        bindings.push_back(binding);
    }
    const vk::VkPushConstantRange pushConstantRange = {vk::VK_SHADER_STAGE_FRAGMENT_BIT, 0u, 16u};

    createPipelineWithResources(vkd, device, renderPass, subpass, *vertexShaderModule, *fragmentShaderModule,
                                context.getTargetWidth(), context.getTargetHeight(),
                                vector<vk::VkVertexInputBindingDescription>(),
                                vector<vk::VkVertexInputAttributeDescription>(), bindings,
                                vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1u, &pushConstantRange, m_resources);

    {
        const uint32_t descriptorCount =
            (uint32_t)(divRoundUp(m_bufferSize, (vk::VkDeviceSize)m_maxStorageTexelCount * (uint64_t)(4)));
        const vk::VkDescriptorPoolSize poolSizes = {
            vk::VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, // VkDescriptorType type;
            descriptorCount                              // uint32_t descriptorCount;
        };
        const vk::VkDescriptorPoolCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,     // VkStructureType sType;
            nullptr,                                               // const void* pNext;
            vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // VkDescriptorPoolCreateFlags flags;
            descriptorCount,                                       // uint32_t maxSets;
            1u,                                                    // uint32_t poolSizeCount;
            &poolSizes,                                            // const VkDescriptorPoolSize* pPoolSizes;
        };

        m_descriptorPool = vk::createDescriptorPool(vkd, device, &createInfo);
        m_descriptorSets.resize(descriptorCount, VK_NULL_HANDLE);
        m_bufferViews.resize(descriptorCount, VK_NULL_HANDLE);
    }

    for (size_t descriptorSetNdx = 0; descriptorSetNdx < m_descriptorSets.size(); descriptorSetNdx++)
    {
        const uint32_t count =
            (uint32_t)(m_bufferSize < (descriptorSetNdx + 1) * m_maxStorageTexelCount * (uint64_t)(4) ?
                           m_bufferSize - descriptorSetNdx * m_maxStorageTexelCount * (uint64_t)(4) :
                           m_maxStorageTexelCount * (uint64_t)(4)) /
            4;
        const vk::VkDescriptorSetLayout layout             = *m_resources.descriptorSetLayout;
        const vk::VkDescriptorSetAllocateInfo allocateInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType sType;
            nullptr,                                            // const void* pNext;
            *m_descriptorPool,                                  // VkDescriptorPool descriptorPool;
            1,                                                  // uint32_t descriptorSetCount;
            &layout                                             // const VkDescriptorSetLayout* pSetLayouts;
        };

        m_descriptorSets[descriptorSetNdx] = vk::allocateDescriptorSet(vkd, device, &allocateInfo).disown();

        {
            const vk::VkBufferViewCreateInfo createInfo = {
                vk::VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,             // VkStructureType sType;
                nullptr,                                                   // const void* pNext;
                0u,                                                        // VkBufferViewCreateFlags flags;
                context.getBuffer(),                                       // VkBuffer buffer;
                vk::VK_FORMAT_R32_UINT,                                    // VkFormat format;
                descriptorSetNdx * m_maxStorageTexelCount * (uint64_t)(4), // VkDeviceSize offset;
                count * 4                                                  // VkDeviceSize range;
            };

            VK_CHECK(vkd.createBufferView(device, &createInfo, nullptr, &m_bufferViews[descriptorSetNdx]));
        }

        {
            const vk::VkWriteDescriptorSet write = {
                vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,  // VkStructureType sType;
                nullptr,                                     // const void* pNext;
                m_descriptorSets[descriptorSetNdx],          // VkDescriptorSet dstSet;
                0u,                                          // uint32_t dstBinding;
                0u,                                          // uint32_t dstArrayElement;
                1u,                                          // uint32_t descriptorCount;
                vk::VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, // VkDescriptorType descriptorType;
                nullptr,                                     // const VkDescriptorImageInfo* pImageInfo;
                nullptr,                                     // const VkDescriptorBufferInfo* pBufferInfo;
                &m_bufferViews[descriptorSetNdx]             // const VkBufferView* pTexelBufferView;
            };

            vkd.updateDescriptorSets(device, 1u, &write, 0u, nullptr);
        }
    }
}

void RenderFragmentStorageTexelBuffer::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();

    vkd.cmdBindPipeline(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipeline);

    for (size_t descriptorSetNdx = 0; descriptorSetNdx < m_descriptorSets.size(); descriptorSetNdx++)
    {
        const struct
        {
            const uint32_t callId;
            const uint32_t valuesPerPixel;
            const uint32_t maxStorageTexelCount;
            const uint32_t width;
        } callParams = {(uint32_t)descriptorSetNdx,
                        (uint32_t)divRoundUp<size_t>(
                            m_descriptorSets.size() * de::min<size_t>(m_maxStorageTexelCount, (size_t)m_bufferSize / 4),
                            m_targetWidth * m_targetHeight),
                        m_maxStorageTexelCount,
                        (uint32_t)(m_bufferSize < (descriptorSetNdx + 1u) * m_maxStorageTexelCount * (uint64_t)(4) ?
                                       m_bufferSize - descriptorSetNdx * m_maxStorageTexelCount * (uint64_t)(4) :
                                       m_maxStorageTexelCount * (uint64_t)(4)) /
                            4u};

        vkd.cmdBindDescriptorSets(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipelineLayout, 0u,
                                  1u, &m_descriptorSets[descriptorSetNdx], 0u, nullptr);
        vkd.cmdPushConstants(commandBuffer, *m_resources.pipelineLayout, vk::VK_SHADER_STAGE_FRAGMENT_BIT, 0u,
                             (uint32_t)sizeof(callParams), &callParams);
        vkd.cmdDraw(commandBuffer, 6, 1, 0, 0);
    }
}

void RenderFragmentStorageTexelBuffer::verify(VerifyRenderPassContext &context, size_t)
{
    const uint32_t valuesPerPixel = (uint32_t)divRoundUp<size_t>(
        m_descriptorSets.size() * de::min<size_t>(m_maxStorageTexelCount, (size_t)m_bufferSize / 4),
        m_targetWidth * m_targetHeight);

    for (int y = 0; y < context.getReferenceTarget().getSize().y(); y++)
        for (int x = 0; x < context.getReferenceTarget().getSize().x(); x++)
        {
            const size_t firstDescriptorSetNdx = de::min<size_t>(
                (y * 256u + x) / (m_maxStorageTexelCount / valuesPerPixel), m_descriptorSets.size() - 1);

            for (size_t descriptorSetNdx = firstDescriptorSetNdx; descriptorSetNdx < m_descriptorSets.size();
                 descriptorSetNdx++)
            {
                const uint64_t offset = descriptorSetNdx * m_maxStorageTexelCount * (uint64_t)(4);
                const uint32_t callId = (uint32_t)descriptorSetNdx;

                const uint32_t id = (uint32_t)y * 256u + (uint32_t)x;
                const uint32_t count =
                    (uint32_t)(m_bufferSize < (descriptorSetNdx + 1) * m_maxStorageTexelCount * (uint64_t)(4) ?
                                   m_bufferSize - descriptorSetNdx * m_maxStorageTexelCount * (uint64_t)(4) :
                                   m_maxStorageTexelCount * (uint64_t)(4)) /
                    4;

                if (y * 256u + x < callId * (m_maxStorageTexelCount / valuesPerPixel))
                    continue;
                else
                {
                    uint32_t value = id;

                    for (uint32_t i = 0; i < valuesPerPixel; i++)
                    {
                        value = ((uint32_t)context.getReference().get(offset + (value % count) * 4 + 0)) |
                                (((uint32_t)context.getReference().get(offset + (value % count) * 4 + 1)) << 8u) |
                                (((uint32_t)context.getReference().get(offset + (value % count) * 4 + 2)) << 16u) |
                                (((uint32_t)context.getReference().get(offset + (value % count) * 4 + 3)) << 24u);
                    }
                    const UVec4 vec((value >> 0u) & 0xFFu, (value >> 8u) & 0xFFu, (value >> 16u) & 0xFFu,
                                    (value >> 24u) & 0xFFu);

                    context.getReferenceTarget().getAccess().setPixel(vec.asFloat() / Vec4(255.0f), x, y);
                }
            }
        }
}

class RenderFragmentStorageImage : public RenderPassCommand
{
public:
    RenderFragmentStorageImage(void)
    {
    }
    ~RenderFragmentStorageImage(void);

    const char *getName(void) const
    {
        return "RenderFragmentStorageImage";
    }
    void logPrepare(TestLog &, size_t) const;
    void logSubmit(TestLog &, size_t) const;
    void prepare(PrepareRenderPassContext &);
    void submit(SubmitContext &context);
    void verify(VerifyRenderPassContext &, size_t);

private:
    PipelineResources m_resources;
    vk::Move<vk::VkDescriptorPool> m_descriptorPool;
    vk::Move<vk::VkDescriptorSet> m_descriptorSet;
    vk::Move<vk::VkImageView> m_imageView;
};

RenderFragmentStorageImage::~RenderFragmentStorageImage(void)
{
}

void RenderFragmentStorageImage::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Create pipeline for render storage image."
        << TestLog::EndMessage;
}

void RenderFragmentStorageImage::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Render using storage image."
        << TestLog::EndMessage;
}

void RenderFragmentStorageImage::prepare(PrepareRenderPassContext &context)
{
    const vk::DeviceInterface &vkd    = context.getContext().getDeviceInterface();
    const vk::VkDevice device         = context.getContext().getDevice();
    const vk::VkRenderPass renderPass = context.getRenderPass();
    const uint32_t subpass            = 0;
    const vk::Unique<vk::VkShaderModule> vertexShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("render-quad.vert"), 0));
    const vk::Unique<vk::VkShaderModule> fragmentShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("storage-image.frag"), 0));
    vector<vk::VkDescriptorSetLayoutBinding> bindings;

    {
        const vk::VkDescriptorSetLayoutBinding binding = {0u, vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                                                          vk::VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};

        bindings.push_back(binding);
    }

    createPipelineWithResources(vkd, device, renderPass, subpass, *vertexShaderModule, *fragmentShaderModule,
                                context.getTargetWidth(), context.getTargetHeight(),
                                vector<vk::VkVertexInputBindingDescription>(),
                                vector<vk::VkVertexInputAttributeDescription>(), bindings,
                                vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, nullptr, m_resources);

    {
        const vk::VkDescriptorPoolSize poolSizes = {
            vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // VkDescriptorType type;
            1                                     // uint32_t descriptorCount;
        };
        const vk::VkDescriptorPoolCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,     // VkStructureType sType;
            nullptr,                                               // const void* pNext;
            vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // VkDescriptorPoolCreateFlags flags;
            1u,                                                    // uint32_t maxSets;
            1u,                                                    // uint32_t poolSizeCount;
            &poolSizes,                                            // const VkDescriptorPoolSize* pPoolSizes;
        };

        m_descriptorPool = vk::createDescriptorPool(vkd, device, &createInfo);
    }

    {
        const vk::VkDescriptorSetLayout layout             = *m_resources.descriptorSetLayout;
        const vk::VkDescriptorSetAllocateInfo allocateInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType sType;
            nullptr,                                            // const void* pNext;
            *m_descriptorPool,                                  // VkDescriptorPool descriptorPool;
            1,                                                  // uint32_t descriptorSetCount;
            &layout                                             // const VkDescriptorSetLayout* pSetLayouts;
        };

        m_descriptorSet = vk::allocateDescriptorSet(vkd, device, &allocateInfo);

        {
            const vk::VkImageViewCreateInfo createInfo = {
                vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,   // VkStructureType sType;
                nullptr,                                        // const void* pNext;
                0u,                                             // VkImageViewCreateFlags flags;
                context.getImage(),                             // VkImage image;
                vk::VK_IMAGE_VIEW_TYPE_2D,                      // VkImageViewType viewType;
                vk::VK_FORMAT_R8G8B8A8_UNORM,                   // VkFormat format;
                vk::makeComponentMappingRGBA(),                 // VkComponentMapping components;
                {vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
            };

            m_imageView = vk::createImageView(vkd, device, &createInfo);
        }

        {
            const vk::VkDescriptorImageInfo imageInfo = {
                VK_NULL_HANDLE,          // VkSampler sampler;
                *m_imageView,            // VkImageView imageView;
                context.getImageLayout() // VkImageLayout imageLayout;
            };
            const vk::VkWriteDescriptorSet write = {
                vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType sType;
                nullptr,                                    // const void* pNext;
                *m_descriptorSet,                           // VkDescriptorSet dstSet;
                0u,                                         // uint32_t dstBinding;
                0u,                                         // uint32_t dstArrayElement;
                1u,                                         // uint32_t descriptorCount;
                vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,       // VkDescriptorType descriptorType;
                &imageInfo,                                 // const VkDescriptorImageInfo* pImageInfo;
                nullptr,                                    // const VkDescriptorBufferInfo* pBufferInfo;
                nullptr,                                    // const VkBufferView* pTexelBufferView;
            };

            vkd.updateDescriptorSets(device, 1u, &write, 0u, nullptr);
        }
    }
}

void RenderFragmentStorageImage::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();

    vkd.cmdBindPipeline(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipeline);

    vkd.cmdBindDescriptorSets(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipelineLayout, 0u, 1u,
                              &(*m_descriptorSet), 0u, nullptr);
    vkd.cmdDraw(commandBuffer, 6, 1, 0, 0);
}

void RenderFragmentStorageImage::verify(VerifyRenderPassContext &context, size_t)
{
    const UVec2 size = UVec2(context.getReferenceImage().getWidth(), context.getReferenceImage().getHeight());
    const uint32_t valuesPerPixel = de::max<uint32_t>(1u, (size.x() * size.y()) / (256u * 256u));

    for (int y = 0; y < context.getReferenceTarget().getSize().y(); y++)
        for (int x = 0; x < context.getReferenceTarget().getSize().x(); x++)
        {
            UVec4 value = UVec4(x, y, 0u, 0u);

            for (uint32_t i = 0; i < valuesPerPixel; i++)
            {
                const UVec2 pos =
                    UVec2(value.z() * 256u + (value.x() ^ value.z()), value.w() * 256u + (value.y() ^ value.w()));
                const Vec4 floatValue =
                    context.getReferenceImage().getAccess().getPixel(pos.x() % size.x(), pos.y() % size.y());

                value = UVec4((uint32_t)round(floatValue.x() * 255.0f), (uint32_t)round(floatValue.y() * 255.0f),
                              (uint32_t)round(floatValue.z() * 255.0f), (uint32_t)round(floatValue.w() * 255.0f));
            }
            context.getReferenceTarget().getAccess().setPixel(value.asFloat() / Vec4(255.0f), x, y);
        }
}

class RenderFragmentSampledImage : public RenderPassCommand
{
public:
    RenderFragmentSampledImage(void)
    {
    }
    ~RenderFragmentSampledImage(void);

    const char *getName(void) const
    {
        return "RenderFragmentSampledImage";
    }
    void logPrepare(TestLog &, size_t) const;
    void logSubmit(TestLog &, size_t) const;
    void prepare(PrepareRenderPassContext &);
    void submit(SubmitContext &context);
    void verify(VerifyRenderPassContext &, size_t);

private:
    PipelineResources m_resources;
    vk::Move<vk::VkDescriptorPool> m_descriptorPool;
    vk::Move<vk::VkDescriptorSet> m_descriptorSet;
    vk::Move<vk::VkImageView> m_imageView;
    vk::Move<vk::VkSampler> m_sampler;
};

RenderFragmentSampledImage::~RenderFragmentSampledImage(void)
{
}

void RenderFragmentSampledImage::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Create pipeline for render storage image."
        << TestLog::EndMessage;
}

void RenderFragmentSampledImage::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Render using storage image."
        << TestLog::EndMessage;
}

void RenderFragmentSampledImage::prepare(PrepareRenderPassContext &context)
{
    const vk::DeviceInterface &vkd    = context.getContext().getDeviceInterface();
    const vk::VkDevice device         = context.getContext().getDevice();
    const vk::VkRenderPass renderPass = context.getRenderPass();
    const uint32_t subpass            = 0;
    const vk::Unique<vk::VkShaderModule> vertexShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("render-quad.vert"), 0));
    const vk::Unique<vk::VkShaderModule> fragmentShaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("sampled-image.frag"), 0));
    vector<vk::VkDescriptorSetLayoutBinding> bindings;

    {
        const vk::VkDescriptorSetLayoutBinding binding = {0u, vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                                          vk::VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};

        bindings.push_back(binding);
    }

    createPipelineWithResources(vkd, device, renderPass, subpass, *vertexShaderModule, *fragmentShaderModule,
                                context.getTargetWidth(), context.getTargetHeight(),
                                vector<vk::VkVertexInputBindingDescription>(),
                                vector<vk::VkVertexInputAttributeDescription>(), bindings,
                                vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, nullptr, m_resources);

    {
        const vk::VkDescriptorPoolSize poolSizes = {
            vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, // VkDescriptorType type;
            1                                              // uint32_t descriptorCount;
        };
        const vk::VkDescriptorPoolCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,     // VkStructureType sType;
            nullptr,                                               // const void* pNext;
            vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // VkDescriptorPoolCreateFlags flags;
            1u,                                                    // uint32_t maxSets;
            1u,                                                    // uint32_t poolSizeCount;
            &poolSizes,                                            // const VkDescriptorPoolSize* pPoolSizes;
        };

        m_descriptorPool = vk::createDescriptorPool(vkd, device, &createInfo);
    }

    {
        const vk::VkDescriptorSetLayout layout             = *m_resources.descriptorSetLayout;
        const vk::VkDescriptorSetAllocateInfo allocateInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType sType;
            nullptr,                                            // const void* pNext;
            *m_descriptorPool,                                  // VkDescriptorPool descriptorPool;
            1,                                                  // uint32_t descriptorSetCount;
            &layout                                             // const VkDescriptorSetLayout* pSetLayouts;
        };

        m_descriptorSet = vk::allocateDescriptorSet(vkd, device, &allocateInfo);

        {
            const vk::VkImageViewCreateInfo createInfo = {
                vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,   // VkStructureType sType;
                nullptr,                                        // const void* pNext;
                0u,                                             // VkImageViewCreateFlags flags;
                context.getImage(),                             // VkImage image;
                vk::VK_IMAGE_VIEW_TYPE_2D,                      // VkImageViewType viewType;
                vk::VK_FORMAT_R8G8B8A8_UNORM,                   // VkFormat format;
                vk::makeComponentMappingRGBA(),                 // VkComponentMapping components;
                {vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
            };

            m_imageView = vk::createImageView(vkd, device, &createInfo);
        }

        {
            const vk::VkSamplerCreateInfo createInfo = {
                vk::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,   // VkStructureType sType;
                nullptr,                                     // const void* pNext;
                0u,                                          // VkSamplerCreateFlags flags;
                vk::VK_FILTER_NEAREST,                       // VkFilter magFilter;
                vk::VK_FILTER_NEAREST,                       // VkFilter minFilter;
                vk::VK_SAMPLER_MIPMAP_MODE_LINEAR,           // VkSamplerMipmapMode mipmapMode;
                vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeU;
                vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeV;
                vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeW;
                0.0f,                                        // float mipLodBias;
                VK_FALSE,                                    // VkBool32 anisotropyEnable;
                1.0f,                                        // float maxAnisotropy;
                VK_FALSE,                                    // VkBool32 compareEnable;
                vk::VK_COMPARE_OP_ALWAYS,                    // VkCompareOp compareOp;
                0.0f,                                        // float minLod;
                0.0f,                                        // float maxLod;
                vk::VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, // VkBorderColor borderColor;
                VK_FALSE                                     // VkBool32 unnormalizedCoordinates;
            };

            m_sampler = vk::createSampler(vkd, device, &createInfo);
        }

        {
            const vk::VkDescriptorImageInfo imageInfo = {
                *m_sampler,              // VkSampler sampler;
                *m_imageView,            // VkImageView imageView;
                context.getImageLayout() // VkImageLayout imageLayout;
            };
            const vk::VkWriteDescriptorSet write = {
                vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,    // VkStructureType sType;
                nullptr,                                       // const void* pNext;
                *m_descriptorSet,                              // VkDescriptorSet dstSet;
                0u,                                            // uint32_t dstBinding;
                0u,                                            // uint32_t dstArrayElement;
                1u,                                            // uint32_t descriptorCount;
                vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, // VkDescriptorType descriptorType;
                &imageInfo,                                    // const VkDescriptorImageInfo* pImageInfo;
                nullptr,                                       // const VkDescriptorBufferInfo* pBufferInfo;
                nullptr,                                       // const VkBufferView* pTexelBufferView;
            };

            vkd.updateDescriptorSets(device, 1u, &write, 0u, nullptr);
        }
    }
}

void RenderFragmentSampledImage::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();

    vkd.cmdBindPipeline(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipeline);

    vkd.cmdBindDescriptorSets(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_resources.pipelineLayout, 0u, 1u,
                              &(*m_descriptorSet), 0u, nullptr);
    vkd.cmdDraw(commandBuffer, 6u, 1u, 0u, 0u);
}

void RenderFragmentSampledImage::verify(VerifyRenderPassContext &context, size_t)
{
    const UVec2 size = UVec2(context.getReferenceImage().getWidth(), context.getReferenceImage().getHeight());
    const uint32_t valuesPerPixel = de::max<uint32_t>(1u, (size.x() * size.y()) / (256u * 256u));

    for (int y = 0; y < context.getReferenceTarget().getSize().y(); y++)
        for (int x = 0; x < context.getReferenceTarget().getSize().x(); x++)
        {
            UVec4 value = UVec4(x, y, 0u, 0u);

            for (uint32_t i = 0; i < valuesPerPixel; i++)
            {
                const UVec2 pos =
                    UVec2(value.z() * 256u + (value.x() ^ value.z()), value.w() * 256u + (value.y() ^ value.w()));
                const Vec4 floatValue =
                    context.getReferenceImage().getAccess().getPixel(pos.x() % size.x(), pos.y() % size.y());

                value = UVec4((uint32_t)round(floatValue.x() * 255.0f), (uint32_t)round(floatValue.y() * 255.0f),
                              (uint32_t)round(floatValue.z() * 255.0f), (uint32_t)round(floatValue.w() * 255.0f));
            }

            context.getReferenceTarget().getAccess().setPixel(value.asFloat() / Vec4(255.0f), x, y);
        }
}
de::MovePtr<RenderPassCommand> createRenderPassCommand(de::Random &, const State &, const TestConfig &testConfig, Op op)
{
    switch (op)
    {
    case OP_RENDER_VERTEX_BUFFER:
        return de::MovePtr<RenderPassCommand>(new RenderVertexBuffer(testConfig.vertexBufferStride));
    case OP_RENDER_INDEX_BUFFER:
        return de::MovePtr<RenderPassCommand>(new RenderIndexBuffer());

    case OP_RENDER_VERTEX_UNIFORM_BUFFER:
        return de::MovePtr<RenderPassCommand>(new RenderVertexUniformBuffer());
    case OP_RENDER_FRAGMENT_UNIFORM_BUFFER:
        return de::MovePtr<RenderPassCommand>(new RenderFragmentUniformBuffer());

    case OP_RENDER_VERTEX_UNIFORM_TEXEL_BUFFER:
        return de::MovePtr<RenderPassCommand>(new RenderVertexUniformTexelBuffer());
    case OP_RENDER_FRAGMENT_UNIFORM_TEXEL_BUFFER:
        return de::MovePtr<RenderPassCommand>(new RenderFragmentUniformTexelBuffer());

    case OP_RENDER_VERTEX_STORAGE_BUFFER:
        return de::MovePtr<RenderPassCommand>(new RenderVertexStorageBuffer());
    case OP_RENDER_FRAGMENT_STORAGE_BUFFER:
        return de::MovePtr<RenderPassCommand>(new RenderFragmentStorageBuffer());

    case OP_RENDER_VERTEX_STORAGE_TEXEL_BUFFER:
        return de::MovePtr<RenderPassCommand>(new RenderVertexStorageTexelBuffer());
    case OP_RENDER_FRAGMENT_STORAGE_TEXEL_BUFFER:
        return de::MovePtr<RenderPassCommand>(new RenderFragmentStorageTexelBuffer());

    case OP_RENDER_VERTEX_STORAGE_IMAGE:
        return de::MovePtr<RenderPassCommand>(new RenderVertexStorageImage());
    case OP_RENDER_FRAGMENT_STORAGE_IMAGE:
        return de::MovePtr<RenderPassCommand>(new RenderFragmentStorageImage());

    case OP_RENDER_VERTEX_SAMPLED_IMAGE:
        return de::MovePtr<RenderPassCommand>(new RenderVertexSampledImage());
    case OP_RENDER_FRAGMENT_SAMPLED_IMAGE:
        return de::MovePtr<RenderPassCommand>(new RenderFragmentSampledImage());

    default:
        DE_FATAL("Unknown op");
        return de::MovePtr<RenderPassCommand>(nullptr);
    }
}

de::MovePtr<CmdCommand> createRenderPassCommands(const Memory &memory, de::Random &nextOpRng, State &state,
                                                 const TestConfig &testConfig, size_t &opNdx, size_t opCount)
{
    vector<RenderPassCommand *> commands;

    try
    {
        for (; opNdx < opCount; opNdx++)
        {
            vector<Op> ops;

            getAvailableOps(state, memory.getSupportBuffers(), memory.getSupportImages(), testConfig.usage,
                            testConfig.backend, ops);

            DE_ASSERT(!ops.empty());

            {
                const Op op = nextOpRng.choose<Op>(ops.begin(), ops.end());

                if (op == OP_RENDERPASS_END)
                {
                    break;
                }
                else
                {
                    de::Random rng(state.rng);

                    commands.push_back(createRenderPassCommand(rng, state, testConfig, op).release());
                    applyOp(state, memory, op, testConfig.usage);

                    DE_ASSERT(state.rng == rng);
                }
            }
        }

        applyOp(state, memory, OP_RENDERPASS_END, testConfig.usage);
        return de::MovePtr<CmdCommand>(new SubmitRenderPass(commands));
    }
    catch (...)
    {
        for (size_t commandNdx = 0; commandNdx < commands.size(); commandNdx++)
            delete commands[commandNdx];

        throw;
    }
}
struct AddPrograms
{
    void init(vk::SourceCollections &sources, TestConfig config) const
    {
        // Vertex buffer rendering
        if (config.usage & USAGE_VERTEX_BUFFER)
        {
            const char *const vertexShader = "#version 310 es\n"
                                             "layout(location = 0) in highp vec2 a_position;\n"
                                             "void main (void) {\n"
                                             "\tgl_PointSize = 1.0;\n"
                                             "\tgl_Position = vec4(1.998 * a_position - vec2(0.999), 0.0, 1.0);\n"
                                             "}\n";

            sources.glslSources.add("vertex-buffer.vert") << glu::VertexSource(vertexShader);
        }

        // Index buffer rendering
        if (config.usage & USAGE_INDEX_BUFFER)
        {
            const char *const vertexShader =
                "#version 310 es\n"
                "precision highp float;\n"
                "void main (void) {\n"
                "\tgl_PointSize = 1.0;\n"
                "\thighp vec2 pos = vec2(gl_VertexIndex % 256, gl_VertexIndex / 256) / vec2(255.0);\n"
                "\tgl_Position = vec4(1.998 * pos - vec2(0.999), 0.0, 1.0);\n"
                "}\n";

            sources.glslSources.add("index-buffer.vert") << glu::VertexSource(vertexShader);
        }

        if (config.usage & USAGE_UNIFORM_BUFFER)
        {
            {
                std::ostringstream vertexShader;

                vertexShader << "#version 310 es\n"
                                "precision highp float;\n"
                                "layout(set=0, binding=0) uniform Block\n"
                                "{\n"
                                "\thighp uvec4 values["
                             << de::toString<size_t>(MAX_UNIFORM_BUFFER_SIZE / (sizeof(uint32_t) * 4))
                             << "];\n"
                                "} block;\n"
                                "void main (void) {\n"
                                "\tgl_PointSize = 1.0;\n"
                                "\thighp uvec4 vecVal = block.values[gl_VertexIndex / 8];\n"
                                "\thighp uint val;\n"
                                "\tif (((gl_VertexIndex / 2) % 4 == 0))\n"
                                "\t\tval = vecVal.x;\n"
                                "\telse if (((gl_VertexIndex / 2) % 4 == 1))\n"
                                "\t\tval = vecVal.y;\n"
                                "\telse if (((gl_VertexIndex / 2) % 4 == 2))\n"
                                "\t\tval = vecVal.z;\n"
                                "\telse if (((gl_VertexIndex / 2) % 4 == 3))\n"
                                "\t\tval = vecVal.w;\n"
                                "\tif ((gl_VertexIndex % 2) == 0)\n"
                                "\t\tval = val & 0xFFFFu;\n"
                                "\telse\n"
                                "\t\tval = val >> 16u;\n"
                                "\thighp vec2 pos = vec2(val & 0xFFu, val >> 8u) / vec2(255.0);\n"
                                "\tgl_Position = vec4(1.998 * pos - vec2(0.999), 0.0, 1.0);\n"
                                "}\n";

                sources.glslSources.add("uniform-buffer.vert") << glu::VertexSource(vertexShader.str());
            }

            {
                const size_t arraySize    = MAX_UNIFORM_BUFFER_SIZE / (sizeof(uint32_t) * 4);
                const size_t arrayIntSize = arraySize * 4;
                std::ostringstream fragmentShader;

                fragmentShader << "#version 310 es\n"
                                  "precision highp float;\n"
                                  "precision highp int;\n"
                                  "layout(location = 0) out highp vec4 o_color;\n"
                                  "layout(set=0, binding=0) uniform Block\n"
                                  "{\n"
                                  "\thighp uvec4 values["
                               << arraySize
                               << "];\n"
                                  "} block;\n"
                                  "layout(push_constant) uniform PushC\n"
                                  "{\n"
                                  "\tuint callId;\n"
                                  "\tuint valuesPerPixel;\n"
                                  "\tuint bufferSize;\n"
                                  "} pushC;\n"
                                  "void main (void) {\n"
                                  "\thighp uint id = uint(gl_FragCoord.y) * 256u + uint(gl_FragCoord.x);\n"
                                  "\tif (uint(gl_FragCoord.y) * 256u + uint(gl_FragCoord.x) < pushC.callId * ("
                               << arrayIntSize
                               << "u / pushC.valuesPerPixel))\n"
                                  "\t\tdiscard;\n"
                                  "\thighp uint value = id;\n"
                                  "\tfor (uint i = 0u; i < pushC.valuesPerPixel; i++)\n"
                                  "\t{\n"
                                  "\t\thighp uvec4 vecVal = block.values[value % pushC.bufferSize];\n"
                                  "\t\tif ((value % 4u) == 0u)\n"
                                  "\t\t\tvalue = vecVal.x;\n"
                                  "\t\telse if ((value % 4u) == 1u)\n"
                                  "\t\t\tvalue = vecVal.y;\n"
                                  "\t\telse if ((value % 4u) == 2u)\n"
                                  "\t\t\tvalue = vecVal.z;\n"
                                  "\t\telse if ((value % 4u) == 3u)\n"
                                  "\t\t\tvalue = vecVal.w;\n"
                                  "\t}\n"
                                  "\tuvec4 valueOut = uvec4(value & 0xFFu, (value >> 8u) & 0xFFu, (value >> 16u) & "
                                  "0xFFu, (value >> 24u) & 0xFFu);\n"
                                  "\to_color = vec4(valueOut) / vec4(255.0);\n"
                                  "}\n";

                sources.glslSources.add("uniform-buffer.frag") << glu::FragmentSource(fragmentShader.str());
            }
        }

        if (config.usage & USAGE_STORAGE_BUFFER)
        {
            {
                // Vertex storage buffer rendering
                const char *const vertexShader = "#version 310 es\n"
                                                 "precision highp float;\n"
                                                 "readonly layout(set=0, binding=0) buffer Block\n"
                                                 "{\n"
                                                 "\thighp uvec4 values[];\n"
                                                 "} block;\n"
                                                 "void main (void) {\n"
                                                 "\tgl_PointSize = 1.0;\n"
                                                 "\thighp uvec4 vecVal = block.values[gl_VertexIndex / 8];\n"
                                                 "\thighp uint val;\n"
                                                 "\tif (((gl_VertexIndex / 2) % 4 == 0))\n"
                                                 "\t\tval = vecVal.x;\n"
                                                 "\telse if (((gl_VertexIndex / 2) % 4 == 1))\n"
                                                 "\t\tval = vecVal.y;\n"
                                                 "\telse if (((gl_VertexIndex / 2) % 4 == 2))\n"
                                                 "\t\tval = vecVal.z;\n"
                                                 "\telse if (((gl_VertexIndex / 2) % 4 == 3))\n"
                                                 "\t\tval = vecVal.w;\n"
                                                 "\tif ((gl_VertexIndex % 2) == 0)\n"
                                                 "\t\tval = val & 0xFFFFu;\n"
                                                 "\telse\n"
                                                 "\t\tval = val >> 16u;\n"
                                                 "\thighp vec2 pos = vec2(val & 0xFFu, val >> 8u) / vec2(255.0);\n"
                                                 "\tgl_Position = vec4(1.998 * pos - vec2(0.999), 0.0, 1.0);\n"
                                                 "}\n";

                sources.glslSources.add("storage-buffer.vert") << glu::VertexSource(vertexShader);
            }

            {
                std::ostringstream fragmentShader;

                fragmentShader << "#version 310 es\n"
                                  "precision highp float;\n"
                                  "precision highp int;\n"
                                  "layout(location = 0) out highp vec4 o_color;\n"
                                  "layout(set=0, binding=0) buffer Block\n"
                                  "{\n"
                                  "\thighp uvec4 values[];\n"
                                  "} block;\n"
                                  "layout(push_constant) uniform PushC\n"
                                  "{\n"
                                  "\tuint valuesPerPixel;\n"
                                  "\tuint bufferSize;\n"
                                  "} pushC;\n"
                                  "void main (void) {\n"
                                  "\thighp uint arrayIntSize = pushC.bufferSize / 4u;\n"
                                  "\thighp uint id = uint(gl_FragCoord.y) * 256u + uint(gl_FragCoord.x);\n"
                                  "\thighp uint value = id;\n"
                                  "\tfor (uint i = 0u; i < pushC.valuesPerPixel; i++)\n"
                                  "\t{\n"
                                  "\t\thighp uvec4 vecVal = block.values[(value / 4u) % (arrayIntSize / 4u)];\n"
                                  "\t\tif ((value % 4u) == 0u)\n"
                                  "\t\t\tvalue = vecVal.x;\n"
                                  "\t\telse if ((value % 4u) == 1u)\n"
                                  "\t\t\tvalue = vecVal.y;\n"
                                  "\t\telse if ((value % 4u) == 2u)\n"
                                  "\t\t\tvalue = vecVal.z;\n"
                                  "\t\telse if ((value % 4u) == 3u)\n"
                                  "\t\t\tvalue = vecVal.w;\n"
                                  "\t}\n"
                                  "\tuvec4 valueOut = uvec4(value & 0xFFu, (value >> 8u) & 0xFFu, (value >> 16u) & "
                                  "0xFFu, (value >> 24u) & 0xFFu);\n"
                                  "\to_color = vec4(valueOut) / vec4(255.0);\n"
                                  "}\n";

                sources.glslSources.add("storage-buffer.frag") << glu::FragmentSource(fragmentShader.str());
            }
        }

        if (config.usage & USAGE_UNIFORM_TEXEL_BUFFER)
        {
            {
                // Vertex uniform texel buffer rendering
                const char *const vertexShader = "#version 310 es\n"
                                                 "#extension GL_EXT_texture_buffer : require\n"
                                                 "precision highp float;\n"
                                                 "layout(set=0, binding=0) uniform highp utextureBuffer u_sampler;\n"
                                                 "void main (void) {\n"
                                                 "\tgl_PointSize = 1.0;\n"
                                                 "\thighp uint val = texelFetch(u_sampler, gl_VertexIndex).x;\n"
                                                 "\thighp vec2 pos = vec2(val & 0xFFu, val >> 8u) / vec2(255.0);\n"
                                                 "\tgl_Position = vec4(1.998 * pos - vec2(0.999), 0.0, 1.0);\n"
                                                 "}\n";

                sources.glslSources.add("uniform-texel-buffer.vert") << glu::VertexSource(vertexShader);
            }

            {
                // Fragment uniform texel buffer rendering
                const char *const fragmentShader =
                    "#version 310 es\n"
                    "#extension GL_EXT_texture_buffer : require\n"
                    "#extension GL_EXT_samplerless_texture_functions : require\n"
                    "precision highp float;\n"
                    "precision highp int;\n"
                    "layout(set=0, binding=0) uniform highp utextureBuffer u_sampler;\n"
                    "layout(location = 0) out highp vec4 o_color;\n"
                    "layout(push_constant) uniform PushC\n"
                    "{\n"
                    "\tuint callId;\n"
                    "\tuint valuesPerPixel;\n"
                    "\tuint maxTexelCount;\n"
                    "} pushC;\n"
                    "void main (void) {\n"
                    "\thighp uint id = uint(gl_FragCoord.y) * 256u + uint(gl_FragCoord.x);\n"
                    "\thighp uint value = id;\n"
                    "\tif (uint(gl_FragCoord.y) * 256u + uint(gl_FragCoord.x) < pushC.callId * (pushC.maxTexelCount / "
                    "pushC.valuesPerPixel))\n"
                    "\t\tdiscard;\n"
                    "\tfor (uint i = 0u; i < pushC.valuesPerPixel; i++)\n"
                    "\t{\n"
                    "\t\tvalue = texelFetch(u_sampler, int(value % uint(textureSize(u_sampler)))).x;\n"
                    "\t}\n"
                    "\tuvec4 valueOut = uvec4(value & 0xFFu, (value >> 8u) & 0xFFu, (value >> 16u) & 0xFFu, (value >> "
                    "24u) & 0xFFu);\n"
                    "\to_color = vec4(valueOut) / vec4(255.0);\n"
                    "}\n";

                sources.glslSources.add("uniform-texel-buffer.frag") << glu::FragmentSource(fragmentShader);
            }
        }

        if (config.usage & USAGE_STORAGE_TEXEL_BUFFER)
        {
            {
                // Vertex storage texel buffer rendering
                const char *const vertexShader =
                    "#version 450\n"
                    "#extension GL_EXT_texture_buffer : require\n"
                    "precision highp float;\n"
                    "layout(set=0, binding=0, r32ui) uniform readonly highp uimageBuffer u_sampler;\n"
                    "out gl_PerVertex {\n"
                    "\tvec4 gl_Position;\n"
                    "\tfloat gl_PointSize;\n"
                    "};\n"
                    "void main (void) {\n"
                    "\tgl_PointSize = 1.0;\n"
                    "\thighp uint val = imageLoad(u_sampler, gl_VertexIndex / 2).x;\n"
                    "\tif (gl_VertexIndex % 2 == 0)\n"
                    "\t\tval = val & 0xFFFFu;\n"
                    "\telse\n"
                    "\t\tval = val >> 16;\n"
                    "\thighp vec2 pos = vec2(val & 0xFFu, val >> 8u) / vec2(255.0);\n"
                    "\tgl_Position = vec4(1.998 * pos - vec2(0.999), 0.0, 1.0);\n"
                    "}\n";

                sources.glslSources.add("storage-texel-buffer.vert") << glu::VertexSource(vertexShader);
            }
            {
                // Fragment storage texel buffer rendering
                const char *const fragmentShader =
                    "#version 310 es\n"
                    "#extension GL_EXT_texture_buffer : require\n"
                    "precision highp float;\n"
                    "precision highp int;\n"
                    "layout(set=0, binding=0, r32ui) uniform readonly highp uimageBuffer u_sampler;\n"
                    "layout(location = 0) out highp vec4 o_color;\n"
                    "layout(push_constant) uniform PushC\n"
                    "{\n"
                    "\tuint callId;\n"
                    "\tuint valuesPerPixel;\n"
                    "\tuint maxTexelCount;\n"
                    "\tuint width;\n"
                    "} pushC;\n"
                    "void main (void) {\n"
                    "\thighp uint id = uint(gl_FragCoord.y) * 256u + uint(gl_FragCoord.x);\n"
                    "\thighp uint value = id;\n"
                    "\tif (uint(gl_FragCoord.y) * 256u + uint(gl_FragCoord.x) < pushC.callId * (pushC.maxTexelCount / "
                    "pushC.valuesPerPixel))\n"
                    "\t\tdiscard;\n"
                    "\tfor (uint i = 0u; i < pushC.valuesPerPixel; i++)\n"
                    "\t{\n"
                    "\t\tvalue = imageLoad(u_sampler, int(value % pushC.width)).x;\n"
                    "\t}\n"
                    "\tuvec4 valueOut = uvec4(value & 0xFFu, (value >> 8u) & 0xFFu, (value >> 16u) & 0xFFu, (value >> "
                    "24u) & 0xFFu);\n"
                    "\to_color = vec4(valueOut) / vec4(255.0);\n"
                    "}\n";

                sources.glslSources.add("storage-texel-buffer.frag") << glu::FragmentSource(fragmentShader);
            }
        }

        if (config.usage & USAGE_STORAGE_IMAGE)
        {
            {
                // Vertex storage image
                const char *const vertexShader =
                    "#version 450\n"
                    "precision highp float;\n"
                    "layout(set=0, binding=0, rgba8) uniform readonly image2D u_image;\n"
                    "out gl_PerVertex {\n"
                    "\tvec4 gl_Position;\n"
                    "\tfloat gl_PointSize;\n"
                    "};\n"
                    "void main (void) {\n"
                    "\tgl_PointSize = 1.0;\n"
                    "\thighp vec4 val = imageLoad(u_image, ivec2((gl_VertexIndex / 2) / imageSize(u_image).x, "
                    "(gl_VertexIndex / 2) % imageSize(u_image).x));\n"
                    "\thighp vec2 pos;\n"
                    "\tif (gl_VertexIndex % 2 == 0)\n"
                    "\t\tpos = val.xy;\n"
                    "\telse\n"
                    "\t\tpos = val.zw;\n"
                    "\tgl_Position = vec4(1.998 * pos - vec2(0.999), 0.0, 1.0);\n"
                    "}\n";

                sources.glslSources.add("storage-image.vert") << glu::VertexSource(vertexShader);
            }
            {
                // Fragment storage image
                const char *const fragmentShader =
                    "#version 450\n"
                    "#extension GL_EXT_texture_buffer : require\n"
                    "precision highp float;\n"
                    "layout(set=0, binding=0, rgba8) uniform readonly image2D u_image;\n"
                    "layout(location = 0) out highp vec4 o_color;\n"
                    "void main (void) {\n"
                    "\thighp uvec2 size = uvec2(imageSize(u_image).x, imageSize(u_image).y);\n"
                    "\thighp uint valuesPerPixel = max(1u, (size.x * size.y) / (256u * 256u));\n"
                    "\thighp uvec4 value = uvec4(uint(gl_FragCoord.x), uint(gl_FragCoord.y), 0u, 0u);\n"
                    "\tfor (uint i = 0u; i < valuesPerPixel; i++)\n"
                    "\t{\n"
                    "\t\thighp vec4 floatValue = imageLoad(u_image, ivec2(int((value.z *  256u + (value.x ^ value.z)) "
                    "% size.x), int((value.w * 256u + (value.y ^ value.w)) % size.y)));\n"
                    "\t\tvalue = uvec4(uint(round(floatValue.x * 255.0)), uint(round(floatValue.y * 255.0)), "
                    "uint(round(floatValue.z * 255.0)), uint(round(floatValue.w * 255.0)));\n"
                    "\t}\n"
                    "\to_color = vec4(value) / vec4(255.0);\n"
                    "}\n";

                sources.glslSources.add("storage-image.frag") << glu::FragmentSource(fragmentShader);
            }
        }

        if (config.usage & USAGE_SAMPLED_IMAGE)
        {
            {
                // Vertex storage image
                const char *const vertexShader =
                    "#version 450\n"
                    "precision highp float;\n"
                    "layout(set=0, binding=0) uniform sampler2D u_sampler;\n"
                    "out gl_PerVertex {\n"
                    "\tvec4 gl_Position;\n"
                    "\tfloat gl_PointSize;\n"
                    "};\n"
                    "void main (void) {\n"
                    "\tgl_PointSize = 1.0;\n"
                    "\thighp vec4 val = texelFetch(u_sampler, ivec2((gl_VertexIndex / 2) / textureSize(u_sampler, "
                    "0).x, (gl_VertexIndex / 2) % textureSize(u_sampler, 0).x), 0);\n"
                    "\thighp vec2 pos;\n"
                    "\tif (gl_VertexIndex % 2 == 0)\n"
                    "\t\tpos = val.xy;\n"
                    "\telse\n"
                    "\t\tpos = val.zw;\n"
                    "\tgl_Position = vec4(1.998 * pos - vec2(0.999), 0.0, 1.0);\n"
                    "}\n";

                sources.glslSources.add("sampled-image.vert") << glu::VertexSource(vertexShader);
            }
            {
                // Fragment storage image
                const char *const fragmentShader =
                    "#version 450\n"
                    "#extension GL_EXT_texture_buffer : require\n"
                    "precision highp float;\n"
                    "layout(set=0, binding=0) uniform sampler2D u_sampler;\n"
                    "layout(location = 0) out highp vec4 o_color;\n"
                    "void main (void) {\n"
                    "\thighp uvec2 size = uvec2(textureSize(u_sampler, 0).x, textureSize(u_sampler, 0).y);\n"
                    "\thighp uint valuesPerPixel = max(1u, (size.x * size.y) / (256u * 256u));\n"
                    "\thighp uvec4 value = uvec4(uint(gl_FragCoord.x), uint(gl_FragCoord.y), 0u, 0u);\n"
                    "\tfor (uint i = 0u; i < valuesPerPixel; i++)\n"
                    "\t{\n"
                    "\t\thighp vec4 floatValue = texelFetch(u_sampler, ivec2(int((value.z *  256u + (value.x ^ "
                    "value.z)) % size.x), int((value.w * 256u + (value.y ^ value.w)) % size.y)), 0);\n"
                    "\t\tvalue = uvec4(uint(round(floatValue.x * 255.0)), uint(round(floatValue.y * 255.0)), "
                    "uint(round(floatValue.z * 255.0)), uint(round(floatValue.w * 255.0)));\n"
                    "\t}\n"
                    "\to_color = vec4(value) / vec4(255.0);\n"
                    "}\n";

                sources.glslSources.add("sampled-image.frag") << glu::FragmentSource(fragmentShader);
            }
        }

        {
            const char *const vertexShader =
                "#version 450\n"
                "out gl_PerVertex {\n"
                "\tvec4 gl_Position;\n"
                "};\n"
                "precision highp float;\n"
                "void main (void) {\n"
                "\tgl_Position = vec4(((gl_VertexIndex + 2) / 3) % 2 == 0 ? -1.0 : 1.0,\n"
                "\t                   ((gl_VertexIndex + 1) / 3) % 2 == 0 ? -1.0 : 1.0, 0.0, 1.0);\n"
                "}\n";

            sources.glslSources.add("render-quad.vert") << glu::VertexSource(vertexShader);
        }

        {
            const char *const fragmentShader = "#version 310 es\n"
                                               "layout(location = 0) out highp vec4 o_color;\n"
                                               "void main (void) {\n"
                                               "\to_color = vec4(1.0);\n"
                                               "}\n";

            sources.glslSources.add("render-white.frag") << glu::FragmentSource(fragmentShader);
        }
    }
};

void checkSupport(vkt::Context &context, TestConfig config)
{
#ifndef CTS_USES_VULKANSC
    if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
        ((config.vertexBufferStride % context.getPortabilitySubsetProperties().minVertexInputBindingStrideAlignment) !=
         0u))
    {
        TCU_THROW(NotSupportedError,
                  "VK_KHR_portability_subset: stride is not multiply of minVertexInputBindingStrideAlignment");
    }
#else
    DE_UNREF(context);
    DE_UNREF(config);
#endif // CTS_USES_VULKANSC
}

tcu::TestCaseGroup *createGraphicsTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "graphics"));
    const vk::VkDeviceSize sizes[] = {
        1024,         // 1K
        8 * 1024,     // 8K
        64 * 1024,    // 64K
        ONE_MEGABYTE, // 1M
    };
    const Usage usages[] = {
        USAGE_HOST_READ,      USAGE_HOST_WRITE,           USAGE_TRANSFER_SRC,   USAGE_TRANSFER_DST,
        USAGE_VERTEX_BUFFER,  USAGE_INDEX_BUFFER,         USAGE_UNIFORM_BUFFER, USAGE_UNIFORM_TEXEL_BUFFER,
        USAGE_STORAGE_BUFFER, USAGE_STORAGE_TEXEL_BUFFER, USAGE_STORAGE_IMAGE,  USAGE_SAMPLED_IMAGE};
    const Usage readUsages[] = {USAGE_HOST_READ,      USAGE_TRANSFER_SRC,         USAGE_VERTEX_BUFFER,
                                USAGE_INDEX_BUFFER,   USAGE_UNIFORM_BUFFER,       USAGE_UNIFORM_TEXEL_BUFFER,
                                USAGE_STORAGE_BUFFER, USAGE_STORAGE_TEXEL_BUFFER, USAGE_STORAGE_IMAGE,
                                USAGE_SAMPLED_IMAGE};

    const Usage writeUsages[] = {USAGE_HOST_WRITE, USAGE_TRANSFER_DST};

    const uint32_t vertexStrides[] = {
        DEFAULT_VERTEX_BUFFER_STRIDE,
        ALTERNATIVE_VERTEX_BUFFER_STRIDE,
    };

    for (size_t writeUsageNdx = 0; writeUsageNdx < DE_LENGTH_OF_ARRAY(writeUsages); writeUsageNdx++)
    {
        const Usage writeUsage = writeUsages[writeUsageNdx];

        for (size_t readUsageNdx = 0; readUsageNdx < DE_LENGTH_OF_ARRAY(readUsages); readUsageNdx++)
        {
            const Usage readUsage = readUsages[readUsageNdx];
            const Usage usage     = writeUsage | readUsage;
            const string usageGroupName(usageToName(usage));
            de::MovePtr<tcu::TestCaseGroup> usageGroup(new tcu::TestCaseGroup(testCtx, usageGroupName.c_str()));

            for (size_t sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(sizes); sizeNdx++)
            {
                const vk::VkDeviceSize size = sizes[sizeNdx];
                TestConfig config           = {usage, DEFAULT_VERTEX_BUFFER_STRIDE, size, vk::VK_SHARING_MODE_EXCLUSIVE,
                                               BACKEND_GRAPHICS};
                const string testName(de::toString((uint64_t)(size)));

                if (readUsage == USAGE_VERTEX_BUFFER)
                {
                    for (size_t strideNdx = 0; strideNdx < DE_LENGTH_OF_ARRAY(vertexStrides); ++strideNdx)
                    {
                        const uint32_t stride      = vertexStrides[strideNdx];
                        const string finalTestName = testName + "_vertex_buffer_stride_" + de::toString(stride);

                        config.vertexBufferStride = stride;
                        usageGroup->addChild(new InstanceFactory1WithSupport<MemoryTestInstance, TestConfig,
                                                                             FunctionSupport1<TestConfig>, AddPrograms>(
                            testCtx, finalTestName, config,
                            typename FunctionSupport1<TestConfig>::Args(checkSupport, config)));
                    }
                }
                else
                {
                    usageGroup->addChild(new InstanceFactory1<MemoryTestInstance, TestConfig, AddPrograms>(
                        testCtx, testName, AddPrograms(), config));
                }
            }

            group->addChild(usageGroup.get());
            usageGroup.release();
        }
    }

    {
        Usage all = (Usage)0;

        for (size_t usageNdx = 0; usageNdx < DE_LENGTH_OF_ARRAY(usages); usageNdx++)
            all = all | usages[usageNdx];

        {
            const string usageGroupName("all");
            de::MovePtr<tcu::TestCaseGroup> usageGroup(new tcu::TestCaseGroup(testCtx, usageGroupName.c_str()));

            for (size_t sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(sizes); sizeNdx++)
            {
                const vk::VkDeviceSize size = sizes[sizeNdx];

                for (size_t strideNdx = 0; strideNdx < DE_LENGTH_OF_ARRAY(vertexStrides); ++strideNdx)
                {
                    const uint32_t stride   = vertexStrides[strideNdx];
                    const string testName   = de::toString(size) + "_vertex_buffer_stride_" + de::toString(stride);
                    const TestConfig config = {all, stride, size, vk::VK_SHARING_MODE_EXCLUSIVE, BACKEND_GRAPHICS};

                    usageGroup->addChild(new InstanceFactory1WithSupport<MemoryTestInstance, TestConfig,
                                                                         FunctionSupport1<TestConfig>, AddPrograms>(
                        testCtx, testName, config, typename FunctionSupport1<TestConfig>::Args(checkSupport, config)));
                }
            }

            group->addChild(usageGroup.get());
            usageGroup.release();
        }

        {
            const string usageGroupName("all_device");
            de::MovePtr<tcu::TestCaseGroup> usageGroup(new tcu::TestCaseGroup(testCtx, usageGroupName.c_str()));

            for (size_t sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(sizes); sizeNdx++)
            {
                const vk::VkDeviceSize size = sizes[sizeNdx];

                for (size_t strideNdx = 0; strideNdx < DE_LENGTH_OF_ARRAY(vertexStrides); ++strideNdx)
                {
                    const uint32_t stride   = vertexStrides[strideNdx];
                    const string testName   = de::toString(size) + "_vertex_buffer_stride_" + de::toString(stride);
                    const TestConfig config = {(Usage)(all & (~(USAGE_HOST_READ | USAGE_HOST_WRITE))), stride, size,
                                               vk::VK_SHARING_MODE_EXCLUSIVE, BACKEND_GRAPHICS};

                    usageGroup->addChild(new InstanceFactory1WithSupport<MemoryTestInstance, TestConfig,
                                                                         FunctionSupport1<TestConfig>, AddPrograms>(
                        testCtx, testName, config, typename FunctionSupport1<TestConfig>::Args(checkSupport, config)));
                }
            }

            group->addChild(usageGroup.get());
            usageGroup.release();
        }
    }

    return group.release();
}

} // namespace pipelinebarrier
} // namespace memory
} // namespace vkt
