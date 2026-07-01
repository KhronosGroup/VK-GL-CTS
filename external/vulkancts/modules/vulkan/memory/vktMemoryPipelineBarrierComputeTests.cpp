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
 * \brief Pipeline barrier tests - compute-only queue backend
 *//*--------------------------------------------------------------------*/

#include "vktMemoryPipelineBarrierComputeTests.hpp"

#include "vktTestCaseUtil.hpp"

namespace vkt
{
namespace memory
{
namespace pipelinebarrier
{

class PrepareComputePassContext
{
public:
    PrepareComputePassContext(PrepareContext &context, vk::VkImageView outputImageView, int32_t targetWidth,
                              int32_t targetHeight)
        : m_context(context)
        , m_outputImageView(outputImageView)
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

    vk::VkImageView getOutputImageView(void) const
    {
        return m_outputImageView;
    }

private:
    PrepareContext &m_context;
    const vk::VkImageView m_outputImageView;
    const int32_t m_targetWidth;
    const int32_t m_targetHeight;
};

class VerifyComputePassContext
{
public:
    VerifyComputePassContext(VerifyContext &context, int32_t targetWidth, int32_t targetHeight)
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

class ComputeCommand
{
public:
    virtual ~ComputeCommand(void)
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
    virtual void prepare(PrepareComputePassContext &)
    {
    }

    // Submit commands to command buffer.
    virtual void submit(SubmitContext &)
    {
    }

    // Verify results
    virtual void verify(VerifyComputePassContext &, size_t)
    {
    }
};

// Unlike render pass color-attachment writes (ordered across draws by the spec), storage image
// writes from separate vkCmdDispatch calls have no implicit ordering guarantee. The commands in
// this file rely on a later dispatch's writes overwriting an earlier dispatch's writes to the
// same output-image texels (mirroring how multiple full-screen draws layer in a render pass), so
// an explicit write-after-write barrier is required between every dispatch that touches it.
void insertComputeWriteBarrier(const vk::DeviceInterface &vkd, vk::VkCommandBuffer commandBuffer)
{
    const vk::VkMemoryBarrier barrier = {
        vk::VK_STRUCTURE_TYPE_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                              // const void* pNext;
        vk::VK_ACCESS_SHADER_WRITE_BIT,       // VkAccessFlags srcAccessMask;
        vk::VK_ACCESS_SHADER_WRITE_BIT        // VkAccessFlags dstAccessMask;
    };

    vkd.cmdPipelineBarrier(commandBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (vk::VkDependencyFlags)0, 1, &barrier, 0, nullptr,
                           0, nullptr);
}

class SubmitComputePass : public CmdCommand
{
public:
    SubmitComputePass(const vector<ComputeCommand *> &commands);
    ~SubmitComputePass(void);
    const char *getName(void) const
    {
        return "SubmitComputePass";
    }

    void logPrepare(TestLog &, size_t) const;
    void logSubmit(TestLog &, size_t) const;

    void prepare(PrepareContext &);
    void submit(SubmitContext &);

    void verify(VerifyContext &, size_t);

private:
    const int32_t m_targetWidth;
    const int32_t m_targetHeight;
    vk::Move<vk::VkImage> m_outputImage;
    vk::Move<vk::VkDeviceMemory> m_outputImageMemory;
    vk::Move<vk::VkImageView> m_outputImageView;
    vector<ComputeCommand *> m_commands;
};

SubmitComputePass::SubmitComputePass(const vector<ComputeCommand *> &commands)
    : m_targetWidth(256)
    , m_targetHeight(256)
    , m_commands(commands)
{
}

SubmitComputePass::~SubmitComputePass()
{
    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
        delete m_commands[cmdNdx];
}

void SubmitComputePass::logPrepare(TestLog &log, size_t commandIndex) const
{
    const string sectionName(de::toString(commandIndex) + ":" + getName());
    const tcu::ScopedLogSection section(log, sectionName, sectionName);

    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
    {
        ComputeCommand &command = *m_commands[cmdNdx];
        command.logPrepare(log, cmdNdx);
    }
}

void SubmitComputePass::logSubmit(TestLog &log, size_t commandIndex) const
{
    const string sectionName(de::toString(commandIndex) + ":" + getName());
    const tcu::ScopedLogSection section(log, sectionName, sectionName);

    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
    {
        ComputeCommand &command = *m_commands[cmdNdx];
        command.logSubmit(log, cmdNdx);
    }
}

void SubmitComputePass::prepare(PrepareContext &context)
{
    const vk::InstanceInterface &vki          = context.getContext().getInstanceInterface();
    const vk::DeviceInterface &vkd            = context.getContext().getDeviceInterface();
    const vk::VkPhysicalDevice physicalDevice = context.getContext().getPhysicalDevice();
    const vk::VkDevice device                 = context.getContext().getDevice();
    const vector<uint32_t> &queueFamilies     = context.getContext().getQueueFamilies();

    {
        const vk::VkImageCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                              // VkStructureType sType;
            nullptr,                                                              // const void* pNext;
            0u,                                                                   // VkImageCreateFlags flags;
            vk::VK_IMAGE_TYPE_2D,                                                 // VkImageType imageType;
            vk::VK_FORMAT_R8G8B8A8_UNORM,                                         // VkFormat format;
            {(uint32_t)m_targetWidth, (uint32_t)m_targetHeight, 1u},              // VkExtent3D extent;
            1u,                                                                   // uint32_t mipLevels;
            1u,                                                                   // uint32_t arrayLayers;
            vk::VK_SAMPLE_COUNT_1_BIT,                                            // VkSampleCountFlagBits samples;
            vk::VK_IMAGE_TILING_OPTIMAL,                                          // VkImageTiling tiling;
            vk::VK_IMAGE_USAGE_STORAGE_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
            vk::VK_SHARING_MODE_EXCLUSIVE,                                        // VkSharingMode sharingMode;
            (uint32_t)queueFamilies.size(),                                       // uint32_t queueFamilyIndexCount;
            &queueFamilies[0],            // const uint32_t* pQueueFamilyIndices;
            vk::VK_IMAGE_LAYOUT_UNDEFINED // VkImageLayout initialLayout;
        };

        m_outputImage = vk::createImage(vkd, device, &createInfo);
    }

    m_outputImageMemory = bindImageMemory(vki, vkd, physicalDevice, device, *m_outputImage, 0);

    {
        const vk::VkImageViewCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,   // VkStructureType sType;
            nullptr,                                        // const void* pNext;
            0u,                                             // VkImageViewCreateFlags flags;
            *m_outputImage,                                 // VkImage image;
            vk::VK_IMAGE_VIEW_TYPE_2D,                      // VkImageViewType viewType;
            vk::VK_FORMAT_R8G8B8A8_UNORM,                   // VkFormat format;
            vk::makeComponentMappingRGBA(),                 // VkComponentMapping components;
            {vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
        };

        m_outputImageView = vk::createImageView(vkd, device, &createInfo);
    }

    {
        PrepareComputePassContext computeContext(context, *m_outputImageView, m_targetWidth, m_targetHeight);

        for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
        {
            ComputeCommand &command = *m_commands[cmdNdx];
            command.prepare(computeContext);
        }
    }
}

void SubmitComputePass::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();

    // Output image starts out undefined; storage image writes require the GENERAL layout.
    {
        const vk::VkImageMemoryBarrier barrier = {
            vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            0,                                          // VkAccessFlags srcAccessMask;
            vk::VK_ACCESS_SHADER_WRITE_BIT,             // VkAccessFlags dstAccessMask;
            vk::VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout oldLayout;
            vk::VK_IMAGE_LAYOUT_GENERAL,                // VkImageLayout newLayout;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t dstQueueFamilyIndex;
            *m_outputImage,                             // VkImage image;
            {
                vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0,                             // uint32_t baseMipLevel;
                1,                             // uint32_t levelCount;
                0,                             // uint32_t baseArrayLayer;
                1                              // uint32_t layerCount;
            }                                  // VkImageSubresourceRange subresourceRange;
        };

        vkd.cmdPipelineBarrier(commandBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                               vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (vk::VkDependencyFlags)0, 0, nullptr, 0,
                               nullptr, 1, &barrier);
    }

    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
    {
        ComputeCommand &command = *m_commands[cmdNdx];

        command.submit(context);

        // Order this command's writes to the output image before the next command's.
        insertComputeWriteBarrier(vkd, commandBuffer);
    }
}

void SubmitComputePass::verify(VerifyContext &context, size_t commandIndex)
{
    TestLog &log(context.getLog());
    tcu::ResultCollector &resultCollector(context.getResultCollector());
    const string sectionName(de::toString(commandIndex) + ":" + getName());
    const tcu::ScopedLogSection section(log, sectionName, sectionName);
    VerifyComputePassContext verifyContext(context, m_targetWidth, m_targetHeight);

    tcu::clear(verifyContext.getReferenceTarget().getAccess(), Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
    {
        ComputeCommand &command = *m_commands[cmdNdx];
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
                vk::VK_ACCESS_SHADER_WRITE_BIT,             // VkAccessFlags srcAccessMask;
                vk::VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
                vk::VK_IMAGE_LAYOUT_GENERAL,                // VkImageLayout oldLayout;
                vk::VK_IMAGE_LAYOUT_GENERAL,                // VkImageLayout newLayout;
                VK_QUEUE_FAMILY_IGNORED,                    // uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                    // uint32_t dstQueueFamilyIndex;
                *m_outputImage,                             // VkImage image;
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

            vkd.cmdPipelineBarrier(*commandBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                   vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (vk::VkDependencyFlags)0, 0, nullptr, 0, nullptr,
                                   1, &imageBarrier);
            vkd.cmdCopyImageToBuffer(*commandBuffer, *m_outputImage, vk::VK_IMAGE_LAYOUT_GENERAL, *dstBuffer, 1,
                                     &region);
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

struct ComputePipelineResources
{
    vk::Move<vk::VkPipeline> pipeline;
    vk::Move<vk::VkDescriptorSetLayout> descriptorSetLayout;
    vk::Move<vk::VkPipelineLayout> pipelineLayout;
};

void createComputePipelineWithResources(const vk::DeviceInterface &vkd, const vk::VkDevice device,
                                        const vk::VkShaderModule &shaderModule,
                                        const vector<vk::VkDescriptorSetLayoutBinding> &bindings,
                                        uint32_t pushConstantRangeCount,
                                        const vk::VkPushConstantRange *pushConstantRanges,
                                        ComputePipelineResources &resources)
{
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
            1u,                                                // uint32_t setLayoutCount;
            &descriptorSetLayout_,                             // const VkDescriptorSetLayout* pSetLayouts;
            pushConstantRangeCount,                            // uint32_t pushConstantRangeCount;
            pushConstantRanges                                 // const VkPushConstantRange* pPushConstantRanges;
        };

        resources.pipelineLayout = vk::createPipelineLayout(vkd, device, &createInfo);
    }

    {
        const vk::VkPipelineShaderStageCreateInfo stageCreateInfo = {
            vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                 // const void* pNext;
            0u,                                                      // VkPipelineShaderStageCreateFlags flags;
            vk::VK_SHADER_STAGE_COMPUTE_BIT,                         // VkShaderStageFlagBits stage;
            shaderModule,                                            // VkShaderModule module;
            "main",                                                  // const char* pName;
            nullptr                                                  // const VkSpecializationInfo* pSpecializationInfo;
        };
        const vk::VkComputePipelineCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                            // const void* pNext;
            0u,                                                 // VkPipelineCreateFlags flags;
            stageCreateInfo,                                    // VkPipelineShaderStageCreateInfo stage;
            *resources.pipelineLayout,                          // VkPipelineLayout layout;
            VK_NULL_HANDLE,                                     // VkPipeline basePipelineHandle;
            0                                                   // int32_t basePipelineIndex;
        };

        resources.pipeline = vk::createComputePipeline(vkd, device, VK_NULL_HANDLE, &createInfo);
    }
}

class ComputeUniformBuffer : public ComputeCommand
{
public:
    ComputeUniformBuffer(void)
    {
    }
    ~ComputeUniformBuffer(void)
    {
    }

    const char *getName(void) const
    {
        return "ComputeUniformBuffer";
    }
    void logPrepare(TestLog &, size_t) const;
    void logSubmit(TestLog &, size_t) const;
    void prepare(PrepareComputePassContext &);
    void submit(SubmitContext &context);
    void verify(VerifyComputePassContext &, size_t);

protected:
    uint32_t calculateBufferPartSize(size_t descriptorSetNdx) const;

private:
    ComputePipelineResources m_resources;
    vk::Move<vk::VkDescriptorPool> m_descriptorPool;
    vector<vk::VkDescriptorSet> m_descriptorSets;

    vk::VkDeviceSize m_bufferSize;
    size_t m_targetWidth;
    size_t m_targetHeight;
    uint32_t m_valuesPerPixel;
};

void ComputeUniformBuffer::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName()
        << " Create pipeline to dispatch buffer as uniform buffer." << TestLog::EndMessage;
}

void ComputeUniformBuffer::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Dispatch using buffer as uniform buffer."
        << TestLog::EndMessage;
}

void ComputeUniformBuffer::prepare(PrepareComputePassContext &context)
{
    const vk::DeviceInterface &vkd = context.getContext().getDeviceInterface();
    const vk::VkDevice device      = context.getContext().getDevice();
    const vk::Unique<vk::VkShaderModule> shaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("uniform-buffer.comp"), 0));
    vector<vk::VkDescriptorSetLayoutBinding> bindings;

    // make sure buffer is smaller then MAX_SIZE and is multiple of 16 (in glsl we use uvec4 to store 16 values)
    m_bufferSize   = de::min(context.getBufferSize(), (vk::VkDeviceSize)MAX_SIZE);
    m_bufferSize   = static_cast<vk::VkDeviceSize>(m_bufferSize / 16u) * 16u;
    m_targetWidth  = context.getTargetWidth();
    m_targetHeight = context.getTargetHeight();

    {
        const vk::VkDescriptorSetLayoutBinding outputBinding = {
            0u,                                   // uint32_t binding;
            vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // VkDescriptorType descriptorType;
            1,                                    // uint32_t descriptorCount;
            vk::VK_SHADER_STAGE_COMPUTE_BIT,      // VkShaderStageFlags stageFlags;
            nullptr                               // const VkSampler* pImmutableSamplers;
        };
        const vk::VkDescriptorSetLayoutBinding bufferBinding = {
            1u,                                    // uint32_t binding;
            vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // VkDescriptorType descriptorType;
            1,                                     // uint32_t descriptorCount;
            vk::VK_SHADER_STAGE_COMPUTE_BIT,       // VkShaderStageFlags stageFlags;
            nullptr                                // const VkSampler* pImmutableSamplers;
        };

        bindings.push_back(outputBinding);
        bindings.push_back(bufferBinding);
    }
    const vk::VkPushConstantRange pushConstantRange = {
        vk::VK_SHADER_STAGE_COMPUTE_BIT, // VkShaderStageFlags stageFlags;
        0u,                              // uint32_t offset;
        12u                              // uint32_t size;
    };

    createComputePipelineWithResources(vkd, device, *shaderModule, bindings, 1u, &pushConstantRange, m_resources);

    {
        const uint32_t descriptorCount =
            (uint32_t)(divRoundUp(m_bufferSize, (vk::VkDeviceSize)MAX_UNIFORM_BUFFER_SIZE));
        const vk::VkDescriptorPoolSize poolSizes[] = {
            {vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, descriptorCount}, // VkDescriptorType type; uint32_t descriptorCount;
            {vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
             descriptorCount}, // VkDescriptorType type; uint32_t descriptorCount;
        };
        const vk::VkDescriptorPoolCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,     // VkStructureType sType;
            nullptr,                                               // const void* pNext;
            vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // VkDescriptorPoolCreateFlags flags;
            descriptorCount,                                       // uint32_t maxSets;
            DE_LENGTH_OF_ARRAY(poolSizes),                         // uint32_t poolSizeCount;
            poolSizes,                                             // const VkDescriptorPoolSize* pPoolSizes;
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
            const vk::VkDescriptorImageInfo outputInfo = {
                VK_NULL_HANDLE,               // VkSampler sampler;
                context.getOutputImageView(), // VkImageView imageView;
                vk::VK_IMAGE_LAYOUT_GENERAL   // VkImageLayout imageLayout;
            };
            const vk::VkDescriptorBufferInfo bufferInfo = {
                context.getBuffer(),                                                    // VkBuffer buffer;
                (vk::VkDeviceSize)(descriptorSetNdx * (size_t)MAX_UNIFORM_BUFFER_SIZE), // VkDeviceSize offset;
                calculateBufferPartSize(descriptorSetNdx)                               // VkDeviceSize range;
            };
            const vk::VkWriteDescriptorSet writes[] = {
                {
                    vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType sType;
                    nullptr,                                    // const void* pNext;
                    m_descriptorSets[descriptorSetNdx],         // VkDescriptorSet dstSet;
                    0u,                                         // uint32_t dstBinding;
                    0u,                                         // uint32_t dstArrayElement;
                    1u,                                         // uint32_t descriptorCount;
                    vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,       // VkDescriptorType descriptorType;
                    &outputInfo,                                // const VkDescriptorImageInfo* pImageInfo;
                    nullptr,                                    // const VkDescriptorBufferInfo* pBufferInfo;
                    nullptr                                     // const VkBufferView* pTexelBufferView;
                },
                {
                    vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType sType;
                    nullptr,                                    // const void* pNext;
                    m_descriptorSets[descriptorSetNdx],         // VkDescriptorSet dstSet;
                    1u,                                         // uint32_t dstBinding;
                    0u,                                         // uint32_t dstArrayElement;
                    1u,                                         // uint32_t descriptorCount;
                    vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,      // VkDescriptorType descriptorType;
                    nullptr,                                    // const VkDescriptorImageInfo* pImageInfo;
                    &bufferInfo,                                // const VkDescriptorBufferInfo* pBufferInfo;
                    nullptr                                     // const VkBufferView* pTexelBufferView;
                },
            };

            vkd.updateDescriptorSets(device, DE_LENGTH_OF_ARRAY(writes), writes, 0u, nullptr);
        }
    }
}

void ComputeUniformBuffer::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();

    vkd.cmdBindPipeline(commandBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *m_resources.pipeline);

    for (size_t descriptorSetNdx = 0; descriptorSetNdx < m_descriptorSets.size(); descriptorSetNdx++)
    {
        const struct
        {
            const uint32_t callId;
            const uint32_t valuesPerPixel;
            const uint32_t bufferSize;
        } callParams = {(uint32_t)descriptorSetNdx, m_valuesPerPixel, calculateBufferPartSize(descriptorSetNdx) / 16u};

        vkd.cmdBindDescriptorSets(commandBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *m_resources.pipelineLayout, 0u,
                                  1u, &m_descriptorSets[descriptorSetNdx], 0u, nullptr);
        vkd.cmdPushConstants(commandBuffer, *m_resources.pipelineLayout, vk::VK_SHADER_STAGE_COMPUTE_BIT, 0u,
                             (uint32_t)sizeof(callParams), &callParams);
        vkd.cmdDispatch(commandBuffer, (uint32_t)m_targetWidth / 16u, (uint32_t)m_targetHeight / 16u, 1u);

        // The next chunk's dispatch must overwrite this one's output-image writes in order.
        insertComputeWriteBarrier(vkd, commandBuffer);
    }
}

void ComputeUniformBuffer::verify(VerifyComputePassContext &context, size_t)
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

uint32_t ComputeUniformBuffer::calculateBufferPartSize(size_t descriptorSetNdx) const
{
    uint32_t size =
        static_cast<uint32_t>(m_bufferSize) - static_cast<uint32_t>(descriptorSetNdx) * MAX_UNIFORM_BUFFER_SIZE;
    if (size < MAX_UNIFORM_BUFFER_SIZE)
        return size;
    return MAX_UNIFORM_BUFFER_SIZE;
}

class ComputeStorageBuffer : public ComputeCommand
{
public:
    ComputeStorageBuffer(void)
    {
    }
    ~ComputeStorageBuffer(void)
    {
    }

    const char *getName(void) const
    {
        return "ComputeStorageBuffer";
    }
    void logPrepare(TestLog &, size_t) const;
    void logSubmit(TestLog &, size_t) const;
    void prepare(PrepareComputePassContext &);
    void submit(SubmitContext &context);
    void verify(VerifyComputePassContext &, size_t);

private:
    ComputePipelineResources m_resources;
    vk::Move<vk::VkDescriptorPool> m_descriptorPool;
    vk::Move<vk::VkDescriptorSet> m_descriptorSet;

    vk::VkDeviceSize m_bufferSize;
    size_t m_targetWidth;
    size_t m_targetHeight;
};

void ComputeStorageBuffer::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName()
        << " Create pipeline to dispatch buffer as storage buffer." << TestLog::EndMessage;
}

void ComputeStorageBuffer::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Dispatch using buffer as storage buffer."
        << TestLog::EndMessage;
}

void ComputeStorageBuffer::prepare(PrepareComputePassContext &context)
{
    const vk::DeviceInterface &vkd = context.getContext().getDeviceInterface();
    const vk::VkDevice device      = context.getContext().getDevice();
    const vk::Unique<vk::VkShaderModule> shaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("storage-buffer.comp"), 0));
    vector<vk::VkDescriptorSetLayoutBinding> bindings;

    // make sure buffer size is multiple of 16 (in glsl we use uvec4 to store 16 values)
    m_bufferSize   = static_cast<vk::VkDeviceSize>(context.getBufferSize() / 16u) * 16u;
    m_targetWidth  = context.getTargetWidth();
    m_targetHeight = context.getTargetHeight();

    {
        const vk::VkDescriptorSetLayoutBinding outputBinding = {
            0u,                                   // uint32_t binding;
            vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // VkDescriptorType descriptorType;
            1,                                    // uint32_t descriptorCount;
            vk::VK_SHADER_STAGE_COMPUTE_BIT,      // VkShaderStageFlags stageFlags;
            nullptr                               // const VkSampler* pImmutableSamplers;
        };
        const vk::VkDescriptorSetLayoutBinding bufferBinding = {
            1u,                                    // uint32_t binding;
            vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // VkDescriptorType descriptorType;
            1,                                     // uint32_t descriptorCount;
            vk::VK_SHADER_STAGE_COMPUTE_BIT,       // VkShaderStageFlags stageFlags;
            nullptr                                // const VkSampler* pImmutableSamplers;
        };

        bindings.push_back(outputBinding);
        bindings.push_back(bufferBinding);
    }
    const vk::VkPushConstantRange pushConstantRange = {
        vk::VK_SHADER_STAGE_COMPUTE_BIT, // VkShaderStageFlags stageFlags;
        0u,                              // uint32_t offset;
        8u                               // uint32_t size;
    };

    createComputePipelineWithResources(vkd, device, *shaderModule, bindings, 1u, &pushConstantRange, m_resources);

    {
        const vk::VkDescriptorPoolSize poolSizes[] = {
            {vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u},  // VkDescriptorType type; uint32_t descriptorCount;
            {vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u}, // VkDescriptorType type; uint32_t descriptorCount;
        };
        const vk::VkDescriptorPoolCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,     // VkStructureType sType;
            nullptr,                                               // const void* pNext;
            vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // VkDescriptorPoolCreateFlags flags;
            1u,                                                    // uint32_t maxSets;
            DE_LENGTH_OF_ARRAY(poolSizes),                         // uint32_t poolSizeCount;
            poolSizes,                                             // const VkDescriptorPoolSize* pPoolSizes;
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
            const vk::VkDescriptorImageInfo outputInfo = {
                VK_NULL_HANDLE,               // VkSampler sampler;
                context.getOutputImageView(), // VkImageView imageView;
                vk::VK_IMAGE_LAYOUT_GENERAL   // VkImageLayout imageLayout;
            };
            const vk::VkDescriptorBufferInfo bufferInfo = {
                context.getBuffer(), // VkBuffer buffer;
                0u,                  // VkDeviceSize offset;
                m_bufferSize         // VkDeviceSize range;
            };
            const vk::VkWriteDescriptorSet writes[] = {
                {
                    vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType sType;
                    nullptr,                                    // const void* pNext;
                    *m_descriptorSet,                           // VkDescriptorSet dstSet;
                    0u,                                         // uint32_t dstBinding;
                    0u,                                         // uint32_t dstArrayElement;
                    1u,                                         // uint32_t descriptorCount;
                    vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,       // VkDescriptorType descriptorType;
                    &outputInfo,                                // const VkDescriptorImageInfo* pImageInfo;
                    nullptr,                                    // const VkDescriptorBufferInfo* pBufferInfo;
                    nullptr                                     // const VkBufferView* pTexelBufferView;
                },
                {
                    vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType sType;
                    nullptr,                                    // const void* pNext;
                    *m_descriptorSet,                           // VkDescriptorSet dstSet;
                    1u,                                         // uint32_t dstBinding;
                    0u,                                         // uint32_t dstArrayElement;
                    1u,                                         // uint32_t descriptorCount;
                    vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,      // VkDescriptorType descriptorType;
                    nullptr,                                    // const VkDescriptorImageInfo* pImageInfo;
                    &bufferInfo,                                // const VkDescriptorBufferInfo* pBufferInfo;
                    nullptr                                     // const VkBufferView* pTexelBufferView;
                },
            };

            vkd.updateDescriptorSets(device, DE_LENGTH_OF_ARRAY(writes), writes, 0u, nullptr);
        }
    }
}

void ComputeStorageBuffer::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();
    const struct
    {
        const uint32_t valuesPerPixel;
        const uint32_t bufferSize;
    } callParams = {(uint32_t)divRoundUp<vk::VkDeviceSize>(m_bufferSize / 4, m_targetWidth * m_targetHeight),
                    (uint32_t)m_bufferSize};

    vkd.cmdBindPipeline(commandBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *m_resources.pipeline);
    vkd.cmdBindDescriptorSets(commandBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *m_resources.pipelineLayout, 0u, 1u,
                              &m_descriptorSet.get(), 0u, nullptr);
    vkd.cmdPushConstants(commandBuffer, *m_resources.pipelineLayout, vk::VK_SHADER_STAGE_COMPUTE_BIT, 0u,
                         (uint32_t)sizeof(callParams), &callParams);
    vkd.cmdDispatch(commandBuffer, (uint32_t)m_targetWidth / 16u, (uint32_t)m_targetHeight / 16u, 1u);
}

void ComputeStorageBuffer::verify(VerifyComputePassContext &context, size_t)
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

class ComputeUniformTexelBuffer : public ComputeCommand
{
public:
    ComputeUniformTexelBuffer(void)
    {
    }
    ~ComputeUniformTexelBuffer(void);

    const char *getName(void) const
    {
        return "ComputeUniformTexelBuffer";
    }
    void logPrepare(TestLog &, size_t) const;
    void logSubmit(TestLog &, size_t) const;
    void prepare(PrepareComputePassContext &);
    void submit(SubmitContext &context);
    void verify(VerifyComputePassContext &, size_t);

private:
    ComputePipelineResources m_resources;
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

ComputeUniformTexelBuffer::~ComputeUniformTexelBuffer(void)
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

void ComputeUniformTexelBuffer::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName()
        << " Create pipeline to dispatch buffer as uniform texel buffer." << TestLog::EndMessage;
}

void ComputeUniformTexelBuffer::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Dispatch using buffer as uniform texel buffer."
        << TestLog::EndMessage;
}

void ComputeUniformTexelBuffer::prepare(PrepareComputePassContext &context)
{
    const vk::InstanceInterface &vki          = context.getContext().getInstanceInterface();
    const vk::VkPhysicalDevice physicalDevice = context.getContext().getPhysicalDevice();
    const vk::DeviceInterface &vkd            = context.getContext().getDeviceInterface();
    const vk::VkDevice device                 = context.getContext().getDevice();
    const vk::Unique<vk::VkShaderModule> shaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("uniform-texel-buffer.comp"), 0));
    vector<vk::VkDescriptorSetLayoutBinding> bindings;

    m_device               = device;
    m_vkd                  = &vkd;
    m_bufferSize           = context.getBufferSize();
    m_maxUniformTexelCount = vk::getPhysicalDeviceProperties(vki, physicalDevice).limits.maxTexelBufferElements;
    m_targetWidth          = context.getTargetWidth();
    m_targetHeight         = context.getTargetHeight();

    {
        const vk::VkDescriptorSetLayoutBinding outputBinding = {
            0u,                                   // uint32_t binding;
            vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // VkDescriptorType descriptorType;
            1,                                    // uint32_t descriptorCount;
            vk::VK_SHADER_STAGE_COMPUTE_BIT,      // VkShaderStageFlags stageFlags;
            nullptr                               // const VkSampler* pImmutableSamplers;
        };
        const vk::VkDescriptorSetLayoutBinding bufferBinding = {
            1u,                                          // uint32_t binding;
            vk::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, // VkDescriptorType descriptorType;
            1,                                           // uint32_t descriptorCount;
            vk::VK_SHADER_STAGE_COMPUTE_BIT,             // VkShaderStageFlags stageFlags;
            nullptr                                      // const VkSampler* pImmutableSamplers;
        };

        bindings.push_back(outputBinding);
        bindings.push_back(bufferBinding);
    }
    const vk::VkPushConstantRange pushConstantRange = {
        vk::VK_SHADER_STAGE_COMPUTE_BIT, // VkShaderStageFlags stageFlags;
        0u,                              // uint32_t offset;
        12u                              // uint32_t size;
    };

    createComputePipelineWithResources(vkd, device, *shaderModule, bindings, 1u, &pushConstantRange, m_resources);

    {
        const uint32_t descriptorCount =
            (uint32_t)(divRoundUp(m_bufferSize, (vk::VkDeviceSize)m_maxUniformTexelCount * (uint64_t)(4)));
        const vk::VkDescriptorPoolSize poolSizes[] = {
            {vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, descriptorCount}, // VkDescriptorType type; uint32_t descriptorCount;
            {vk::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
             descriptorCount}, // VkDescriptorType type; uint32_t descriptorCount;
        };
        const vk::VkDescriptorPoolCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,     // VkStructureType sType;
            nullptr,                                               // const void* pNext;
            vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // VkDescriptorPoolCreateFlags flags;
            descriptorCount,                                       // uint32_t maxSets;
            DE_LENGTH_OF_ARRAY(poolSizes),                         // uint32_t poolSizeCount;
            poolSizes,                                             // const VkDescriptorPoolSize* pPoolSizes;
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
            const vk::VkDescriptorImageInfo outputInfo = {
                VK_NULL_HANDLE,               // VkSampler sampler;
                context.getOutputImageView(), // VkImageView imageView;
                vk::VK_IMAGE_LAYOUT_GENERAL   // VkImageLayout imageLayout;
            };
            const vk::VkWriteDescriptorSet writes[] = {
                {
                    vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType sType;
                    nullptr,                                    // const void* pNext;
                    m_descriptorSets[descriptorSetNdx],         // VkDescriptorSet dstSet;
                    0u,                                         // uint32_t dstBinding;
                    0u,                                         // uint32_t dstArrayElement;
                    1u,                                         // uint32_t descriptorCount;
                    vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,       // VkDescriptorType descriptorType;
                    &outputInfo,                                // const VkDescriptorImageInfo* pImageInfo;
                    nullptr,                                    // const VkDescriptorBufferInfo* pBufferInfo;
                    nullptr                                     // const VkBufferView* pTexelBufferView;
                },
                {
                    vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,  // VkStructureType sType;
                    nullptr,                                     // const void* pNext;
                    m_descriptorSets[descriptorSetNdx],          // VkDescriptorSet dstSet;
                    1u,                                          // uint32_t dstBinding;
                    0u,                                          // uint32_t dstArrayElement;
                    1u,                                          // uint32_t descriptorCount;
                    vk::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, // VkDescriptorType descriptorType;
                    nullptr,                                     // const VkDescriptorImageInfo* pImageInfo;
                    nullptr,                                     // const VkDescriptorBufferInfo* pBufferInfo;
                    &m_bufferViews[descriptorSetNdx]             // const VkBufferView* pTexelBufferView;
                },
            };

            vkd.updateDescriptorSets(device, DE_LENGTH_OF_ARRAY(writes), writes, 0u, nullptr);
        }
    }
}

void ComputeUniformTexelBuffer::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();

    vkd.cmdBindPipeline(commandBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *m_resources.pipeline);

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

        vkd.cmdBindDescriptorSets(commandBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *m_resources.pipelineLayout, 0u,
                                  1u, &m_descriptorSets[descriptorSetNdx], 0u, nullptr);
        vkd.cmdPushConstants(commandBuffer, *m_resources.pipelineLayout, vk::VK_SHADER_STAGE_COMPUTE_BIT, 0u,
                             (uint32_t)sizeof(callParams), &callParams);
        vkd.cmdDispatch(commandBuffer, (uint32_t)m_targetWidth / 16u, (uint32_t)m_targetHeight / 16u, 1u);

        // The next chunk's dispatch must overwrite this one's output-image writes in order.
        insertComputeWriteBarrier(vkd, commandBuffer);
    }
}

void ComputeUniformTexelBuffer::verify(VerifyComputePassContext &context, size_t)
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

class ComputeStorageTexelBuffer : public ComputeCommand
{
public:
    ComputeStorageTexelBuffer(void)
    {
    }
    ~ComputeStorageTexelBuffer(void);

    const char *getName(void) const
    {
        return "ComputeStorageTexelBuffer";
    }
    void logPrepare(TestLog &, size_t) const;
    void logSubmit(TestLog &, size_t) const;
    void prepare(PrepareComputePassContext &);
    void submit(SubmitContext &context);
    void verify(VerifyComputePassContext &, size_t);

private:
    ComputePipelineResources m_resources;
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

ComputeStorageTexelBuffer::~ComputeStorageTexelBuffer(void)
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

void ComputeStorageTexelBuffer::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName()
        << " Create pipeline to dispatch buffer as storage texel buffer." << TestLog::EndMessage;
}

void ComputeStorageTexelBuffer::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Dispatch using buffer as storage texel buffer."
        << TestLog::EndMessage;
}

void ComputeStorageTexelBuffer::prepare(PrepareComputePassContext &context)
{
    const vk::InstanceInterface &vki          = context.getContext().getInstanceInterface();
    const vk::VkPhysicalDevice physicalDevice = context.getContext().getPhysicalDevice();
    const vk::DeviceInterface &vkd            = context.getContext().getDeviceInterface();
    const vk::VkDevice device                 = context.getContext().getDevice();
    const vk::Unique<vk::VkShaderModule> shaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("storage-texel-buffer.comp"), 0));
    vector<vk::VkDescriptorSetLayoutBinding> bindings;

    m_device               = device;
    m_vkd                  = &vkd;
    m_bufferSize           = context.getBufferSize();
    m_maxStorageTexelCount = vk::getPhysicalDeviceProperties(vki, physicalDevice).limits.maxTexelBufferElements;
    m_targetWidth          = context.getTargetWidth();
    m_targetHeight         = context.getTargetHeight();

    {
        const vk::VkDescriptorSetLayoutBinding outputBinding = {
            0u,                                   // uint32_t binding;
            vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // VkDescriptorType descriptorType;
            1,                                    // uint32_t descriptorCount;
            vk::VK_SHADER_STAGE_COMPUTE_BIT,      // VkShaderStageFlags stageFlags;
            nullptr                               // const VkSampler* pImmutableSamplers;
        };
        const vk::VkDescriptorSetLayoutBinding bufferBinding = {
            1u,                                          // uint32_t binding;
            vk::VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, // VkDescriptorType descriptorType;
            1,                                           // uint32_t descriptorCount;
            vk::VK_SHADER_STAGE_COMPUTE_BIT,             // VkShaderStageFlags stageFlags;
            nullptr                                      // const VkSampler* pImmutableSamplers;
        };

        bindings.push_back(outputBinding);
        bindings.push_back(bufferBinding);
    }
    const vk::VkPushConstantRange pushConstantRange = {
        vk::VK_SHADER_STAGE_COMPUTE_BIT, // VkShaderStageFlags stageFlags;
        0u,                              // uint32_t offset;
        16u                              // uint32_t size;
    };

    createComputePipelineWithResources(vkd, device, *shaderModule, bindings, 1u, &pushConstantRange, m_resources);

    {
        const uint32_t descriptorCount =
            (uint32_t)(divRoundUp(m_bufferSize, (vk::VkDeviceSize)m_maxStorageTexelCount * (uint64_t)(4)));
        const vk::VkDescriptorPoolSize poolSizes[] = {
            {vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, descriptorCount}, // VkDescriptorType type; uint32_t descriptorCount;
            {vk::VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
             descriptorCount}, // VkDescriptorType type; uint32_t descriptorCount;
        };
        const vk::VkDescriptorPoolCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,     // VkStructureType sType;
            nullptr,                                               // const void* pNext;
            vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // VkDescriptorPoolCreateFlags flags;
            descriptorCount,                                       // uint32_t maxSets;
            DE_LENGTH_OF_ARRAY(poolSizes),                         // uint32_t poolSizeCount;
            poolSizes,                                             // const VkDescriptorPoolSize* pPoolSizes;
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
            const vk::VkDescriptorImageInfo outputInfo = {
                VK_NULL_HANDLE,               // VkSampler sampler;
                context.getOutputImageView(), // VkImageView imageView;
                vk::VK_IMAGE_LAYOUT_GENERAL   // VkImageLayout imageLayout;
            };
            const vk::VkWriteDescriptorSet writes[] = {
                {
                    vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType sType;
                    nullptr,                                    // const void* pNext;
                    m_descriptorSets[descriptorSetNdx],         // VkDescriptorSet dstSet;
                    0u,                                         // uint32_t dstBinding;
                    0u,                                         // uint32_t dstArrayElement;
                    1u,                                         // uint32_t descriptorCount;
                    vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,       // VkDescriptorType descriptorType;
                    &outputInfo,                                // const VkDescriptorImageInfo* pImageInfo;
                    nullptr,                                    // const VkDescriptorBufferInfo* pBufferInfo;
                    nullptr                                     // const VkBufferView* pTexelBufferView;
                },
                {
                    vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,  // VkStructureType sType;
                    nullptr,                                     // const void* pNext;
                    m_descriptorSets[descriptorSetNdx],          // VkDescriptorSet dstSet;
                    1u,                                          // uint32_t dstBinding;
                    0u,                                          // uint32_t dstArrayElement;
                    1u,                                          // uint32_t descriptorCount;
                    vk::VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, // VkDescriptorType descriptorType;
                    nullptr,                                     // const VkDescriptorImageInfo* pImageInfo;
                    nullptr,                                     // const VkDescriptorBufferInfo* pBufferInfo;
                    &m_bufferViews[descriptorSetNdx]             // const VkBufferView* pTexelBufferView;
                },
            };

            vkd.updateDescriptorSets(device, DE_LENGTH_OF_ARRAY(writes), writes, 0u, nullptr);
        }
    }
}

void ComputeStorageTexelBuffer::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();

    vkd.cmdBindPipeline(commandBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *m_resources.pipeline);

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

        vkd.cmdBindDescriptorSets(commandBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *m_resources.pipelineLayout, 0u,
                                  1u, &m_descriptorSets[descriptorSetNdx], 0u, nullptr);
        vkd.cmdPushConstants(commandBuffer, *m_resources.pipelineLayout, vk::VK_SHADER_STAGE_COMPUTE_BIT, 0u,
                             (uint32_t)sizeof(callParams), &callParams);
        vkd.cmdDispatch(commandBuffer, (uint32_t)m_targetWidth / 16u, (uint32_t)m_targetHeight / 16u, 1u);

        // The next chunk's dispatch must overwrite this one's output-image writes in order.
        insertComputeWriteBarrier(vkd, commandBuffer);
    }
}

void ComputeStorageTexelBuffer::verify(VerifyComputePassContext &context, size_t)
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

class ComputeStorageImage : public ComputeCommand
{
public:
    ComputeStorageImage(void)
    {
    }
    ~ComputeStorageImage(void)
    {
    }

    const char *getName(void) const
    {
        return "ComputeStorageImage";
    }
    void logPrepare(TestLog &, size_t) const;
    void logSubmit(TestLog &, size_t) const;
    void prepare(PrepareComputePassContext &);
    void submit(SubmitContext &context);
    void verify(VerifyComputePassContext &, size_t);

private:
    ComputePipelineResources m_resources;
    vk::Move<vk::VkDescriptorPool> m_descriptorPool;
    vk::Move<vk::VkDescriptorSet> m_descriptorSet;
    vk::Move<vk::VkImageView> m_imageView;
};

void ComputeStorageImage::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Create pipeline to dispatch storage image."
        << TestLog::EndMessage;
}

void ComputeStorageImage::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Dispatch using storage image."
        << TestLog::EndMessage;
}

void ComputeStorageImage::prepare(PrepareComputePassContext &context)
{
    const vk::DeviceInterface &vkd = context.getContext().getDeviceInterface();
    const vk::VkDevice device      = context.getContext().getDevice();
    const vk::Unique<vk::VkShaderModule> shaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("storage-image.comp"), 0));
    vector<vk::VkDescriptorSetLayoutBinding> bindings;

    {
        const vk::VkDescriptorSetLayoutBinding inputBinding = {
            0u,                                   // uint32_t binding;
            vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // VkDescriptorType descriptorType;
            1,                                    // uint32_t descriptorCount;
            vk::VK_SHADER_STAGE_COMPUTE_BIT,      // VkShaderStageFlags stageFlags;
            nullptr                               // const VkSampler* pImmutableSamplers;
        };
        const vk::VkDescriptorSetLayoutBinding outputBinding = {
            1u,                                   // uint32_t binding;
            vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // VkDescriptorType descriptorType;
            1,                                    // uint32_t descriptorCount;
            vk::VK_SHADER_STAGE_COMPUTE_BIT,      // VkShaderStageFlags stageFlags;
            nullptr                               // const VkSampler* pImmutableSamplers;
        };

        bindings.push_back(inputBinding);
        bindings.push_back(outputBinding);
    }

    createComputePipelineWithResources(vkd, device, *shaderModule, bindings, 0u, nullptr, m_resources);

    {
        const vk::VkDescriptorPoolSize poolSizes = {
            vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // VkDescriptorType type;
            2                                     // uint32_t descriptorCount;
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
            const vk::VkDescriptorImageInfo inputInfo = {
                VK_NULL_HANDLE,          // VkSampler sampler;
                *m_imageView,            // VkImageView imageView;
                context.getImageLayout() // VkImageLayout imageLayout;
            };
            const vk::VkDescriptorImageInfo outputInfo = {
                VK_NULL_HANDLE,               // VkSampler sampler;
                context.getOutputImageView(), // VkImageView imageView;
                vk::VK_IMAGE_LAYOUT_GENERAL   // VkImageLayout imageLayout;
            };
            const vk::VkWriteDescriptorSet writes[] = {
                {
                    vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType sType;
                    nullptr,                                    // const void* pNext;
                    *m_descriptorSet,                           // VkDescriptorSet dstSet;
                    0u,                                         // uint32_t dstBinding;
                    0u,                                         // uint32_t dstArrayElement;
                    1u,                                         // uint32_t descriptorCount;
                    vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,       // VkDescriptorType descriptorType;
                    &inputInfo,                                 // const VkDescriptorImageInfo* pImageInfo;
                    nullptr,                                    // const VkDescriptorBufferInfo* pBufferInfo;
                    nullptr                                     // const VkBufferView* pTexelBufferView;
                },
                {
                    vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType sType;
                    nullptr,                                    // const void* pNext;
                    *m_descriptorSet,                           // VkDescriptorSet dstSet;
                    1u,                                         // uint32_t dstBinding;
                    0u,                                         // uint32_t dstArrayElement;
                    1u,                                         // uint32_t descriptorCount;
                    vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,       // VkDescriptorType descriptorType;
                    &outputInfo,                                // const VkDescriptorImageInfo* pImageInfo;
                    nullptr,                                    // const VkDescriptorBufferInfo* pBufferInfo;
                    nullptr                                     // const VkBufferView* pTexelBufferView;
                },
            };

            vkd.updateDescriptorSets(device, DE_LENGTH_OF_ARRAY(writes), writes, 0u, nullptr);
        }
    }
}

void ComputeStorageImage::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();

    vkd.cmdBindPipeline(commandBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *m_resources.pipeline);
    vkd.cmdBindDescriptorSets(commandBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *m_resources.pipelineLayout, 0u, 1u,
                              &m_descriptorSet.get(), 0u, nullptr);
    vkd.cmdDispatch(commandBuffer, 256u / 16u, 256u / 16u, 1u);
}

void ComputeStorageImage::verify(VerifyComputePassContext &context, size_t)
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

class ComputeSampledImage : public ComputeCommand
{
public:
    ComputeSampledImage(void)
    {
    }
    ~ComputeSampledImage(void)
    {
    }

    const char *getName(void) const
    {
        return "ComputeSampledImage";
    }
    void logPrepare(TestLog &, size_t) const;
    void logSubmit(TestLog &, size_t) const;
    void prepare(PrepareComputePassContext &);
    void submit(SubmitContext &context);
    void verify(VerifyComputePassContext &, size_t);

private:
    ComputePipelineResources m_resources;
    vk::Move<vk::VkDescriptorPool> m_descriptorPool;
    vk::Move<vk::VkDescriptorSet> m_descriptorSet;
    vk::Move<vk::VkImageView> m_imageView;
    vk::Move<vk::VkSampler> m_sampler;
};

void ComputeSampledImage::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Create pipeline to dispatch sampled image."
        << TestLog::EndMessage;
}

void ComputeSampledImage::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Dispatch using sampled image."
        << TestLog::EndMessage;
}

void ComputeSampledImage::prepare(PrepareComputePassContext &context)
{
    const vk::DeviceInterface &vkd = context.getContext().getDeviceInterface();
    const vk::VkDevice device      = context.getContext().getDevice();
    const vk::Unique<vk::VkShaderModule> shaderModule(
        vk::createShaderModule(vkd, device, context.getBinaryCollection().get("sampled-image.comp"), 0));
    vector<vk::VkDescriptorSetLayoutBinding> bindings;

    {
        const vk::VkDescriptorSetLayoutBinding samplerBinding = {
            0u,                                            // uint32_t binding;
            vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, // VkDescriptorType descriptorType;
            1,                                             // uint32_t descriptorCount;
            vk::VK_SHADER_STAGE_COMPUTE_BIT,               // VkShaderStageFlags stageFlags;
            nullptr                                        // const VkSampler* pImmutableSamplers;
        };
        const vk::VkDescriptorSetLayoutBinding outputBinding = {
            1u,                                   // uint32_t binding;
            vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // VkDescriptorType descriptorType;
            1,                                    // uint32_t descriptorCount;
            vk::VK_SHADER_STAGE_COMPUTE_BIT,      // VkShaderStageFlags stageFlags;
            nullptr                               // const VkSampler* pImmutableSamplers;
        };

        bindings.push_back(samplerBinding);
        bindings.push_back(outputBinding);
    }

    createComputePipelineWithResources(vkd, device, *shaderModule, bindings, 0u, nullptr, m_resources);

    {
        const vk::VkDescriptorPoolSize poolSizes[] = {
            {vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u}, // VkDescriptorType type; uint32_t descriptorCount;
            {vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u},          // VkDescriptorType type; uint32_t descriptorCount;
        };
        const vk::VkDescriptorPoolCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,     // VkStructureType sType;
            nullptr,                                               // const void* pNext;
            vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // VkDescriptorPoolCreateFlags flags;
            1u,                                                    // uint32_t maxSets;
            DE_LENGTH_OF_ARRAY(poolSizes),                         // uint32_t poolSizeCount;
            poolSizes,                                             // const VkDescriptorPoolSize* pPoolSizes;
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
            const vk::VkDescriptorImageInfo samplerInfo = {
                *m_sampler,              // VkSampler sampler;
                *m_imageView,            // VkImageView imageView;
                context.getImageLayout() // VkImageLayout imageLayout;
            };
            const vk::VkDescriptorImageInfo outputInfo = {
                VK_NULL_HANDLE,               // VkSampler sampler;
                context.getOutputImageView(), // VkImageView imageView;
                vk::VK_IMAGE_LAYOUT_GENERAL   // VkImageLayout imageLayout;
            };
            const vk::VkWriteDescriptorSet writes[] = {
                {
                    vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,    // VkStructureType sType;
                    nullptr,                                       // const void* pNext;
                    *m_descriptorSet,                              // VkDescriptorSet dstSet;
                    0u,                                            // uint32_t dstBinding;
                    0u,                                            // uint32_t dstArrayElement;
                    1u,                                            // uint32_t descriptorCount;
                    vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, // VkDescriptorType descriptorType;
                    &samplerInfo,                                  // const VkDescriptorImageInfo* pImageInfo;
                    nullptr,                                       // const VkDescriptorBufferInfo* pBufferInfo;
                    nullptr                                        // const VkBufferView* pTexelBufferView;
                },
                {
                    vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType sType;
                    nullptr,                                    // const void* pNext;
                    *m_descriptorSet,                           // VkDescriptorSet dstSet;
                    1u,                                         // uint32_t dstBinding;
                    0u,                                         // uint32_t dstArrayElement;
                    1u,                                         // uint32_t descriptorCount;
                    vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,       // VkDescriptorType descriptorType;
                    &outputInfo,                                // const VkDescriptorImageInfo* pImageInfo;
                    nullptr,                                    // const VkDescriptorBufferInfo* pBufferInfo;
                    nullptr                                     // const VkBufferView* pTexelBufferView;
                },
            };

            vkd.updateDescriptorSets(device, DE_LENGTH_OF_ARRAY(writes), writes, 0u, nullptr);
        }
    }
}

void ComputeSampledImage::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();

    vkd.cmdBindPipeline(commandBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *m_resources.pipeline);
    vkd.cmdBindDescriptorSets(commandBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *m_resources.pipelineLayout, 0u, 1u,
                              &m_descriptorSet.get(), 0u, nullptr);
    vkd.cmdDispatch(commandBuffer, 256u / 16u, 256u / 16u, 1u);
}

void ComputeSampledImage::verify(VerifyComputePassContext &context, size_t)
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

de::MovePtr<ComputeCommand> createComputePassCommand(Op op)
{
    switch (op)
    {
    case OP_COMPUTE_UNIFORM_BUFFER:
        return de::MovePtr<ComputeCommand>(new ComputeUniformBuffer());
    case OP_COMPUTE_UNIFORM_TEXEL_BUFFER:
        return de::MovePtr<ComputeCommand>(new ComputeUniformTexelBuffer());
    case OP_COMPUTE_STORAGE_BUFFER:
        return de::MovePtr<ComputeCommand>(new ComputeStorageBuffer());
    case OP_COMPUTE_STORAGE_TEXEL_BUFFER:
        return de::MovePtr<ComputeCommand>(new ComputeStorageTexelBuffer());
    case OP_COMPUTE_STORAGE_IMAGE:
        return de::MovePtr<ComputeCommand>(new ComputeStorageImage());
    case OP_COMPUTE_SAMPLED_IMAGE:
        return de::MovePtr<ComputeCommand>(new ComputeSampledImage());

    default:
        DE_FATAL("Unknown op");
        return de::MovePtr<ComputeCommand>(nullptr);
    }
}

de::MovePtr<CmdCommand> createComputeCommands(const Memory &memory, de::Random &nextOpRng, State &state,
                                              const TestConfig &testConfig, size_t &opNdx, size_t opCount)
{
    vector<ComputeCommand *> commands;

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

                if (op == OP_COMPUTEPASS_END)
                {
                    break;
                }
                else
                {
                    commands.push_back(createComputePassCommand(op).release());
                    applyOp(state, memory, op, testConfig.usage);
                }
            }
        }

        applyOp(state, memory, OP_COMPUTEPASS_END, testConfig.usage);
        return de::MovePtr<CmdCommand>(new SubmitComputePass(commands));
    }
    catch (...)
    {
        for (size_t commandNdx = 0; commandNdx < commands.size(); commandNdx++)
            delete commands[commandNdx];

        throw;
    }
}

namespace
{

void checkSupport(vkt::Context &context, TestConfig config)
{
    DE_UNREF(config);

    if (context.getComputeQueueFamilyIndex() == -1)
        TCU_THROW(NotSupportedError, "No dedicated compute queue available");
}

struct AddComputePrograms
{
    void init(vk::SourceCollections &sources, TestConfig config) const
    {
        if (config.usage & USAGE_UNIFORM_BUFFER)
        {
            const size_t arraySize    = MAX_UNIFORM_BUFFER_SIZE / (sizeof(uint32_t) * 4);
            const size_t arrayIntSize = arraySize * 4;
            std::ostringstream computeShader;

            computeShader << "#version 450\n"
                             "layout(local_size_x = 16, local_size_y = 16) in;\n"
                             "layout(set=0, binding=0, rgba8) uniform writeonly highp image2D u_output;\n"
                             "layout(set=0, binding=1) uniform Block\n"
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
                             "\thighp uvec2 pos = gl_GlobalInvocationID.xy;\n"
                             "\thighp uint id = pos.y * 256u + pos.x;\n"
                             "\tif (pos.y * 256u + pos.x < pushC.callId * ("
                          << arrayIntSize
                          << "u / pushC.valuesPerPixel))\n"
                             "\t\treturn;\n"
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
                             "\timageStore(u_output, ivec2(pos), vec4(valueOut) / vec4(255.0));\n"
                             "}\n";

            sources.glslSources.add("uniform-buffer.comp") << glu::ComputeSource(computeShader.str());
        }

        if (config.usage & USAGE_UNIFORM_TEXEL_BUFFER)
        {
            const char *const computeShader =
                "#version 450\n"
                "#extension GL_EXT_samplerless_texture_functions : require\n"
                "layout(local_size_x = 16, local_size_y = 16) in;\n"
                "layout(set=0, binding=0, rgba8) uniform writeonly highp image2D u_output;\n"
                "layout(set=0, binding=1) uniform highp utextureBuffer u_sampler;\n"
                "layout(push_constant) uniform PushC\n"
                "{\n"
                "\tuint callId;\n"
                "\tuint valuesPerPixel;\n"
                "\tuint maxTexelCount;\n"
                "} pushC;\n"
                "void main (void) {\n"
                "\thighp uvec2 pos = gl_GlobalInvocationID.xy;\n"
                "\thighp uint id = pos.y * 256u + pos.x;\n"
                "\thighp uint value = id;\n"
                "\tif (pos.y * 256u + pos.x < pushC.callId * (pushC.maxTexelCount / pushC.valuesPerPixel))\n"
                "\t\treturn;\n"
                "\tfor (uint i = 0u; i < pushC.valuesPerPixel; i++)\n"
                "\t{\n"
                "\t\tvalue = texelFetch(u_sampler, int(value % uint(textureSize(u_sampler)))).x;\n"
                "\t}\n"
                "\tuvec4 valueOut = uvec4(value & 0xFFu, (value >> 8u) & 0xFFu, (value >> 16u) & 0xFFu, (value >> "
                "24u) & 0xFFu);\n"
                "\timageStore(u_output, ivec2(pos), vec4(valueOut) / vec4(255.0));\n"
                "}\n";

            sources.glslSources.add("uniform-texel-buffer.comp") << glu::ComputeSource(computeShader);
        }

        if (config.usage & USAGE_STORAGE_TEXEL_BUFFER)
        {
            const char *const computeShader =
                "#version 450\n"
                "layout(local_size_x = 16, local_size_y = 16) in;\n"
                "layout(set=0, binding=0, rgba8) uniform writeonly highp image2D u_output;\n"
                "layout(set=0, binding=1, r32ui) uniform readonly highp uimageBuffer u_sampler;\n"
                "layout(push_constant) uniform PushC\n"
                "{\n"
                "\tuint callId;\n"
                "\tuint valuesPerPixel;\n"
                "\tuint maxTexelCount;\n"
                "\tuint width;\n"
                "} pushC;\n"
                "void main (void) {\n"
                "\thighp uvec2 pos = gl_GlobalInvocationID.xy;\n"
                "\thighp uint id = pos.y * 256u + pos.x;\n"
                "\thighp uint value = id;\n"
                "\tif (pos.y * 256u + pos.x < pushC.callId * (pushC.maxTexelCount / pushC.valuesPerPixel))\n"
                "\t\treturn;\n"
                "\tfor (uint i = 0u; i < pushC.valuesPerPixel; i++)\n"
                "\t{\n"
                "\t\tvalue = imageLoad(u_sampler, int(value % pushC.width)).x;\n"
                "\t}\n"
                "\tuvec4 valueOut = uvec4(value & 0xFFu, (value >> 8u) & 0xFFu, (value >> 16u) & 0xFFu, (value >> "
                "24u) & 0xFFu);\n"
                "\timageStore(u_output, ivec2(pos), vec4(valueOut) / vec4(255.0));\n"
                "}\n";

            sources.glslSources.add("storage-texel-buffer.comp") << glu::ComputeSource(computeShader);
        }

        if (config.usage & USAGE_STORAGE_BUFFER)
        {
            const char *const computeShader =
                "#version 450\n"
                "layout(local_size_x = 16, local_size_y = 16) in;\n"
                "layout(set=0, binding=0, rgba8) uniform writeonly highp image2D u_output;\n"
                "layout(set=0, binding=1) buffer Block\n"
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
                "\thighp uvec2 pos = gl_GlobalInvocationID.xy;\n"
                "\thighp uint id = pos.y * 256u + pos.x;\n"
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
                "\tuvec4 valueOut = uvec4(value & 0xFFu, (value >> 8u) & 0xFFu, (value >> 16u) & 0xFFu, (value >> "
                "24u) & 0xFFu);\n"
                "\timageStore(u_output, ivec2(pos), vec4(valueOut) / vec4(255.0));\n"
                "}\n";

            sources.glslSources.add("storage-buffer.comp") << glu::ComputeSource(computeShader);
        }

        if (config.usage & USAGE_STORAGE_IMAGE)
        {
            const char *const computeShader =
                "#version 450\n"
                "layout(local_size_x = 16, local_size_y = 16) in;\n"
                "layout(set=0, binding=0, rgba8) uniform readonly highp image2D u_image;\n"
                "layout(set=0, binding=1, rgba8) uniform writeonly highp image2D u_output;\n"
                "void main (void) {\n"
                "\thighp uvec2 size = uvec2(imageSize(u_image).x, imageSize(u_image).y);\n"
                "\thighp uint valuesPerPixel = max(1u, (size.x * size.y) / (256u * 256u));\n"
                "\thighp uvec2 pos = gl_GlobalInvocationID.xy;\n"
                "\thighp uvec4 value = uvec4(pos.x, pos.y, 0u, 0u);\n"
                "\tfor (uint i = 0u; i < valuesPerPixel; i++)\n"
                "\t{\n"
                "\t\thighp vec4 floatValue = imageLoad(u_image, ivec2(int((value.z *  256u + (value.x ^ value.z)) "
                "% size.x), int((value.w * 256u + (value.y ^ value.w)) % size.y)));\n"
                "\t\tvalue = uvec4(uint(round(floatValue.x * 255.0)), uint(round(floatValue.y * 255.0)), "
                "uint(round(floatValue.z * 255.0)), uint(round(floatValue.w * 255.0)));\n"
                "\t}\n"
                "\timageStore(u_output, ivec2(pos), vec4(value) / vec4(255.0));\n"
                "}\n";

            sources.glslSources.add("storage-image.comp") << glu::ComputeSource(computeShader);
        }

        if (config.usage & USAGE_SAMPLED_IMAGE)
        {
            const char *const computeShader =
                "#version 450\n"
                "layout(local_size_x = 16, local_size_y = 16) in;\n"
                "layout(set=0, binding=0) uniform highp sampler2D u_sampler;\n"
                "layout(set=0, binding=1, rgba8) uniform writeonly highp image2D u_output;\n"
                "void main (void) {\n"
                "\thighp uvec2 size = uvec2(textureSize(u_sampler, 0).x, textureSize(u_sampler, 0).y);\n"
                "\thighp uint valuesPerPixel = max(1u, (size.x * size.y) / (256u * 256u));\n"
                "\thighp uvec2 pos = gl_GlobalInvocationID.xy;\n"
                "\thighp uvec4 value = uvec4(pos.x, pos.y, 0u, 0u);\n"
                "\tfor (uint i = 0u; i < valuesPerPixel; i++)\n"
                "\t{\n"
                "\t\thighp vec4 floatValue = texelFetch(u_sampler, ivec2(int((value.z *  256u + (value.x ^ "
                "value.z)) % size.x), int((value.w * 256u + (value.y ^ value.w)) % size.y)), 0);\n"
                "\t\tvalue = uvec4(uint(round(floatValue.x * 255.0)), uint(round(floatValue.y * 255.0)), "
                "uint(round(floatValue.z * 255.0)), uint(round(floatValue.w * 255.0)));\n"
                "\t}\n"
                "\timageStore(u_output, ivec2(pos), vec4(value) / vec4(255.0));\n"
                "}\n";

            sources.glslSources.add("sampled-image.comp") << glu::ComputeSource(computeShader);
        }
    }
};

} // namespace

tcu::TestCaseGroup *createComputeTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "compute"));
    const vk::VkDeviceSize sizes[] = {
        1024,         // 1K
        8 * 1024,     // 8K
        64 * 1024,    // 64K
        ONE_MEGABYTE, // 1M
    };
    const Usage readUsages[] = {
        USAGE_HOST_READ,      USAGE_TRANSFER_SRC,         USAGE_UNIFORM_BUFFER, USAGE_UNIFORM_TEXEL_BUFFER,
        USAGE_STORAGE_BUFFER, USAGE_STORAGE_TEXEL_BUFFER, USAGE_STORAGE_IMAGE,  USAGE_SAMPLED_IMAGE};
    const Usage writeUsages[] = {USAGE_HOST_WRITE, USAGE_TRANSFER_DST};

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
                const string testName(de::toString((uint64_t)(size)));
                const TestConfig config = {usage, DEFAULT_VERTEX_BUFFER_STRIDE, size, vk::VK_SHARING_MODE_EXCLUSIVE,
                                           BACKEND_COMPUTE};

                usageGroup->addChild(new InstanceFactory1WithSupport<MemoryTestInstance, TestConfig,
                                                                     FunctionSupport1<TestConfig>, AddComputePrograms>(
                    testCtx, testName, config, typename FunctionSupport1<TestConfig>::Args(checkSupport, config)));
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
