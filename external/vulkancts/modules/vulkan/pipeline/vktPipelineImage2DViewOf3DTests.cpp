/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
 * Copyright (c) 2022 Google LLC.
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief
 *//*--------------------------------------------------------------------*/

#include "vktPipelineImage2DViewOf3DTests.hpp"
#include "vkPipelineConstructionUtil.hpp"
#include "vktTestCase.hpp"
#include "vkImageUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBarrierUtil.hpp"
#include "tcuTexture.hpp"
#include "tcuPlatform.hpp"
#include "tcuImageCompare.hpp"
#include "deMemory.h"

#include <cstdint>
#include <sstream>
#include <vector>

namespace vkt
{
namespace pipeline
{

using namespace vk;
using de::MovePtr;

namespace
{

using DeviceMemorySp = de::SharedPtr<vk::Unique<vk::VkDeviceMemory>>;

enum ImageAccessType
{
    StorageImage = 0,
    Sampler,
    CombinedImageSampler
};

enum ImageBindingType
{
    Normal = 0,
    Sparse,
};

enum TestType
{
    Compute,
    Fragment
};

struct TestParameters
{
    tcu::IVec3 imageSize;
    uint32_t mipLevel;
    int32_t layerNdx;
    ImageAccessType imageType;
    TestType testType;
    VkFormat imageFormat;
    PipelineConstructionType pipelineConstructionType;
    ImageBindingType imageBindingType;
};

inline int32_t computeMipLevelDimension(int32_t baseLevelDimension, uint32_t mipLevel)
{
    return de::max(baseLevelDimension >> mipLevel, 1);
}

tcu::IVec3 computeMipLevelSize(tcu::IVec3 baseLevelSize, uint32_t mipLevel)
{
    int32_t width  = computeMipLevelDimension(baseLevelSize.x(), mipLevel);
    int32_t height = computeMipLevelDimension(baseLevelSize.y(), mipLevel);
    int32_t depth  = computeMipLevelDimension(baseLevelSize.z(), mipLevel);
    return tcu::IVec3(width, height, depth);
}

void copyImageLayerToBuffer(const DeviceInterface &vk, VkCommandBuffer cmdBuffer, VkImage image, VkBuffer buffer,
                            tcu::IVec2 size, VkAccessFlags srcAccessMask, VkImageLayout oldLayout, uint32_t layerToCopy,
                            uint32_t mipLevel)
{
    const VkImageSubresourceRange subresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, 1u, 0, 1u);
    const VkImageMemoryBarrier imageBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType            sType
        nullptr,                                // const void*                pNext
        srcAccessMask,                          // VkAccessFlags            srcAccessMask
        VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags            dstAccessMask
        oldLayout,                              // VkImageLayout            oldLayout
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,   // VkImageLayout            newLayout
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t                    srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t                    destQueueFamilyIndex
        image,                                  // VkImage                    image
        subresourceRange                        // VkImageSubresourceRange    subresourceRange
    };

    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                          nullptr, 0u, nullptr, 1u, &imageBarrier);

    const VkImageSubresourceLayers subresource = {
        subresourceRange.aspectMask, // VkImageAspectFlags    aspectMask
        mipLevel,                    // uint32_t                mipLevel
        0u,                          // uint32_t                baseArrayLayer
        1u,                          // uint32_t                layerCount
    };

    const VkBufferImageCopy region = {
        0ull,                                 // VkDeviceSize                    bufferOffset
        0u,                                   // uint32_t                        bufferRowLength
        0u,                                   // uint32_t                        bufferImageHeight
        subresource,                          // VkImageSubresourceLayers        imageSubresource
        makeOffset3D(0, 0, (int)layerToCopy), // VkOffset3D                    imageOffset
        makeExtent3D(size.x(), size.y(), 1u)  // VkExtent3D                    imageExtent
    };

    vk.cmdCopyImageToBuffer(cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1u, &region);

    const VkBufferMemoryBarrier bufferBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType    sType
        nullptr,                                 // const void*        pNext
        VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags    srcAccessMask
        VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags    dstAccessMask
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t            srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t            dstQueueFamilyIndex
        buffer,                                  // VkBuffer            buffer
        0ull,                                    // VkDeviceSize        offset
        VK_WHOLE_SIZE                            // VkDeviceSize        size
    };

    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 1u,
                          &bufferBarrier, 0u, nullptr);
}

// Draws a chess pattern to the given 'layer' (z-dimension) of the 'image'. Other layers will be cleared to white.
void fillImage(const tcu::PixelBufferAccess &image, const int layer)
{
    const tcu::Vec4 clearColor = tcu::Vec4(1); // White clear color.
    for (int z = 0; z < image.getSize().z(); ++z)
        for (int y = 0; y < image.getSize().y(); ++y)
            for (int x = 0; x < image.getSize().x(); ++x)
            {
                if (z == layer)
                {
                    const float c         = (float)((x + y) & 1);
                    const tcu::Vec4 color = tcu::Vec4(c, c, c, 1.0f);
                    image.setPixel(color, x, y, z);
                }
                else
                {
                    image.setPixel(clearColor, x, y, z);
                }
            }
}

template <typename T>
inline de::SharedPtr<vk::Unique<T>> makeVkSharedPtr(vk::Move<T> vkMove)
{
    return de::SharedPtr<vk::Unique<T>>(new vk::Unique<T>(vkMove));
}

bool getMemoryType(const InstanceInterface &instance, const VkPhysicalDevice physicalDevice,
                   const VkMemoryRequirements &objectMemoryRequirements, const MemoryRequirement &memoryRequirement,
                   uint32_t &memTypeIdx)
{
    bool memTypeFound = false;
    const VkPhysicalDeviceMemoryProperties deviceMemoryProperties =
        getPhysicalDeviceMemoryProperties(instance, physicalDevice);

    for (uint32_t memoryTypeIdx = 0; !memTypeFound && memoryTypeIdx < deviceMemoryProperties.memoryTypeCount;
         ++memoryTypeIdx)
    {
        if ((objectMemoryRequirements.memoryTypeBits & (1u << memoryTypeIdx)) != 0 &&
            memoryRequirement.matchesHeap(deviceMemoryProperties.memoryTypes[memoryTypeIdx].propertyFlags))
        {
            memTypeIdx   = memoryTypeIdx;
            memTypeFound = true;
        }
    }
    return memTypeFound;
}

VkSparseMemoryBind makeSparseMemoryBinding(const DeviceInterface &vk, const VkDevice device,
                                           const VkDeviceSize allocationSize, const uint32_t memoryType,
                                           const VkDeviceSize resourceOffset, const VkSparseMemoryBindFlags flags)
{
    const VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                // const void* pNext;
        allocationSize,                         // VkDeviceSize allocationSize;
        memoryType,                             // uint32_t memoryTypeIndex;
    };

    VkDeviceMemory deviceMemory = VK_NULL_HANDLE;
    VK_CHECK(vk.allocateMemory(device, &allocInfo, VK_NULL_HANDLE, &deviceMemory));

    VkSparseMemoryBind memoryBind;

    memoryBind.resourceOffset = resourceOffset;
    memoryBind.size           = allocationSize;
    memoryBind.memory         = deviceMemory;
    memoryBind.memoryOffset   = 0u;
    memoryBind.flags          = flags;

    return memoryBind;
}

class Image2DView3DImageInstance : public vkt::TestInstance
{
public:
    Image2DView3DImageInstance(Context &context, const TestParameters testParameters)
        : vkt::TestInstance(context)
        , m_testParameters(testParameters)
    {
    }

    tcu::TestStatus iterate(void);

private:
    void runComputePipeline(const VkDescriptorSet &descriptorSet, const VkDescriptorSetLayout descriptorSetLayout,
                            tcu::IVec3 &testMipLevelSize, VkCommandBuffer cmdBuffer, VkImage image,
                            VkBuffer outputBuffer, const VkSemaphore *sparseImageSemaphore);

    void runGraphicsPipeline(const VkDescriptorSet &descriptorSet, const VkDescriptorSetLayout descriptorSetLayout,
                             tcu::IVec3 &testMipLevelSize, VkCommandBuffer cmdBuffer, VkImage image,
                             VkBuffer outputBuffer, const VkSemaphore *sparseImageSemaphore);
    const TestParameters m_testParameters;
};

void commonSubmission(const DeviceInterface &vk, const VkDevice &device, const VkQueue &queue,
                      VkCommandBuffer &cmdBuffer, const VkSemaphore *sparseImageSemaphore)
{
    const VkPipelineStageFlags stageFlags[] = {VK_PIPELINE_STAGE_TRANSFER_BIT};
    const uint32_t waitSemaphoreCount       = (sparseImageSemaphore != nullptr) ? 1u : 0u;
    const VkPipelineStageFlags *waitStages  = (sparseImageSemaphore != nullptr) ? stageFlags : nullptr;
    submitCommandsAndWait(vk, device, queue, cmdBuffer, /*useDeviceGroups*/ false, /*deviceMask*/ 1u,
                          waitSemaphoreCount, sparseImageSemaphore, waitStages);
}

void Image2DView3DImageInstance::runComputePipeline(const VkDescriptorSet &descriptorSet,
                                                    const VkDescriptorSetLayout descriptorSetLayout,
                                                    tcu::IVec3 &testMipLevelSize, VkCommandBuffer cmdBuffer,
                                                    VkImage image, VkBuffer outputBuffer,
                                                    const VkSemaphore *sparseImageSemaphore)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    const VkQueue queue       = m_context.getUniversalQueue();
    const bool useSampler     = m_testParameters.imageType != StorageImage;

    const Unique<VkShaderModule> shaderModule(
        createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u));
    const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, descriptorSetLayout));
    const Unique<VkPipeline> pipeline(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));

    vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet, 0u,
                             nullptr);
    vk.cmdDispatch(cmdBuffer, testMipLevelSize.x(), testMipLevelSize.y(), 1u);

    // Copy the result image to a buffer.
    copyImageLayerToBuffer(vk, cmdBuffer, image, outputBuffer, testMipLevelSize.xy(), VK_ACCESS_SHADER_WRITE_BIT,
                           VK_IMAGE_LAYOUT_GENERAL, useSampler ? 0u : m_testParameters.layerNdx,
                           useSampler ? 0u : m_testParameters.mipLevel);

    endCommandBuffer(vk, cmdBuffer);

    // Wait for completion.
    commonSubmission(vk, device, queue, cmdBuffer, sparseImageSemaphore);
}

void Image2DView3DImageInstance::runGraphicsPipeline(const VkDescriptorSet &descriptorSet,
                                                     const VkDescriptorSetLayout descriptorSetLayout,
                                                     tcu::IVec3 &testMipLevelSize, VkCommandBuffer cmdBuffer,
                                                     VkImage image, VkBuffer outputBuffer,
                                                     const VkSemaphore *sparseImageSemaphore)
{
    const InstanceInterface &vki          = m_context.getInstanceInterface();
    const DeviceInterface &vk             = m_context.getDeviceInterface();
    const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    const VkDevice device                 = m_context.getDevice();
    const VkQueue queue                   = m_context.getUniversalQueue();
    const bool useSampler                 = m_testParameters.imageType != StorageImage;

    const ShaderWrapper vertShader(ShaderWrapper(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
    const ShaderWrapper fragShader(ShaderWrapper(vk, device, m_context.getBinaryCollection().get("frag"), 0u));
    const PipelineLayoutWrapper pipelineLayout(m_testParameters.pipelineConstructionType, vk, device,
                                               descriptorSetLayout);
    RenderPassWrapper renderPass(m_testParameters.pipelineConstructionType, vk, device);
    const std::vector<VkViewport> viewport = {
        makeViewport(m_testParameters.imageSize.x(), m_testParameters.imageSize.y())};
    const std::vector<VkRect2D> scissor = {makeRect2D(m_testParameters.imageSize.x(), m_testParameters.imageSize.y())};

    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType                            sType
        nullptr,                                                     // const void*                                pNext
        0u,                                                          // VkPipelineInputAssemblyStateCreateFlags    flags
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN, // VkPrimitiveTopology                        topology
        VK_FALSE                            // VkBool32                                    primitiveRestartEnable
    };

    const VkVertexInputBindingDescription vertexInputBindingDescription = {
        0u,                          // uint32_t                binding
        sizeof(tcu::Vec4),           // uint32_t                stride
        VK_VERTEX_INPUT_RATE_VERTEX, // VkVertexInputRate    inputRate
    };

    const VkVertexInputAttributeDescription vertexInputAttributeDescription = {
        0u,                            // uint32_t        location
        0u,                            // uint32_t        binding
        VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat        format
        0u                             // uint32_t        offset
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfoDefault = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                                sType
        nullptr,                                  // const void*                                    pNext
        (VkPipelineVertexInputStateCreateFlags)0, // VkPipelineVertexInputStateCreateFlags        flags
        1u,                             // uint32_t                                        vertexBindingDescriptionCount
        &vertexInputBindingDescription, // const VkVertexInputBindingDescription*        pVertexBindingDescriptions
        1u, // uint32_t                                        vertexAttributeDescriptionCount
        &vertexInputAttributeDescription // const VkVertexInputAttributeDescription*        pVertexAttributeDescriptions
    };

    vk::GraphicsPipelineWrapper graphicsPipeline(vki, vk, physicalDevice, device, m_context.getDeviceExtensions(),
                                                 m_testParameters.pipelineConstructionType, 0u);
    graphicsPipeline.setMonolithicPipelineLayout(pipelineLayout)
        .setDefaultDepthStencilState()
        .setDefaultRasterizationState()
        .setDefaultMultisampleState()
        .setupVertexInputState(&vertexInputStateCreateInfoDefault, &inputAssemblyStateCreateInfo)
        .setupPreRasterizationShaderState(viewport, scissor, pipelineLayout, *renderPass, 0u, vertShader)
        .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragShader)
        .setupFragmentOutputState(*renderPass, 0u)
        .buildPipeline();

    renderPass.createFramebuffer(vk, device, 0u, nullptr, nullptr, testMipLevelSize.x(), testMipLevelSize.y());

    // Create vertex buffer and fill it with full screen quad.
    const std::vector<tcu::Vec4> vertexData = {
        {-1, -1, 1, 1},
        {1, -1, 1, 1},
        {1, 1, 1, 1},
        {-1, 1, 1, 1},
    };
    size_t vertexBufferSize = sizeof(tcu::Vec4) * vertexData.size();
    BufferWithMemory vertexBuffer(vk, device, m_context.getDefaultAllocator(),
                                  makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
                                  MemoryRequirement::HostVisible);
    deMemcpy(vertexBuffer.getAllocation().getHostPtr(), vertexData.data(), vertexBufferSize);
    flushAlloc(vk, device, vertexBuffer.getAllocation());

    VkDeviceSize vertexBufferOffset = 0;
    vk.cmdBindVertexBuffers(cmdBuffer, 0, 1, &*vertexBuffer, &vertexBufferOffset);

    graphicsPipeline.bind(cmdBuffer);
    vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &descriptorSet, 0u,
                             nullptr);

    renderPass.begin(vk, cmdBuffer, makeRect2D(testMipLevelSize.xy()));
    vk.cmdDraw(cmdBuffer, 4, 1, 0, 0);
    renderPass.end(vk, cmdBuffer);

    // Copy the result image to a buffer.
    copyImageLayerToBuffer(vk, cmdBuffer, image, outputBuffer, testMipLevelSize.xy(), VK_ACCESS_SHADER_WRITE_BIT,
                           VK_IMAGE_LAYOUT_GENERAL, useSampler ? 0u : m_testParameters.layerNdx,
                           useSampler ? 0u : m_testParameters.mipLevel);

    endCommandBuffer(vk, cmdBuffer);

    // Wait for completion.
    commonSubmission(vk, device, queue, cmdBuffer, sparseImageSemaphore);
}

tcu::TestStatus Image2DView3DImageInstance::iterate(void)
{
    const DeviceInterface &vk              = m_context.getDeviceInterface();
    const VkDevice device                  = m_context.getDevice();
    const uint32_t queueFamilyIndex        = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator                   = m_context.getDefaultAllocator();
    tcu::IVec3 imageSize                   = m_testParameters.imageSize;
    const bool useSampler                  = m_testParameters.imageType != StorageImage;
    const bool useSparseBinding            = m_testParameters.imageBindingType == Sparse;
    const tcu::TextureFormat textureFormat = mapVkFormat(m_testParameters.imageFormat);
    const uint32_t mipLevelCount           = 3;

    tcu::IVec3 testMipLevelSize = computeMipLevelSize(m_testParameters.imageSize, m_testParameters.mipLevel);
    uint32_t bufferSize =
        testMipLevelSize.x() * testMipLevelSize.y() * testMipLevelSize.z() * textureFormat.getPixelSize();
    const BufferWithMemory outputBuffer(vk, device, allocator,
                                        makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                                        MemoryRequirement::HostVisible);

    // Input image is used with sampler cases only.
    de::MovePtr<BufferWithMemory> inputImageBuffer;

    // Upload the test image data for sampler cases.
    if (useSampler)
    {
        // Initialize the input image's mip level and fill the target layer with a chess pattern, others will be white.
        tcu::TextureLevel inputImageMipLevel(textureFormat, testMipLevelSize.x(), testMipLevelSize.y(),
                                             testMipLevelSize.z());
        fillImage(inputImageMipLevel.getAccess(), m_testParameters.layerNdx);

        // Create a buffer to upload the image.
        const VkBufferCreateInfo bufferCreateInfo =
            makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        inputImageBuffer = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));

        // Upload target mip level to the input buffer.
        deMemcpy(inputImageBuffer->getAllocation().getHostPtr(), inputImageMipLevel.getAccess().getDataPtr(),
                 bufferSize);
        flushAlloc(vk, device, inputImageBuffer->getAllocation());
    }

    VkImageCreateFlags flags = VK_IMAGE_CREATE_2D_VIEW_COMPATIBLE_BIT_EXT;

    if (useSparseBinding)
        flags |= VK_IMAGE_CREATE_SPARSE_BINDING_BIT;

    // Create the test image: sampled image or storage image, depending on the test type.
    const VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                    (useSampler ? VK_IMAGE_USAGE_SAMPLED_BIT : VK_IMAGE_USAGE_STORAGE_BIT);
    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                       // VkStructureType            sType
        nullptr,                                                   // const void*                pNext
        flags,                                                     // VkImageCreateFlags        flags
        VK_IMAGE_TYPE_3D,                                          // VkImageType                imageType
        m_testParameters.imageFormat,                              // VkFormat                    format
        makeExtent3D(imageSize.x(), imageSize.y(), imageSize.z()), // VkExtent3D                extent
        (uint32_t)mipLevelCount,                                   // uint32_t                    mipLevels
        1u,                                                        // uint32_t                    arrayLayers
        VK_SAMPLE_COUNT_1_BIT,                                     // VkSampleCountFlagBits    samples
        VK_IMAGE_TILING_OPTIMAL,                                   // VkImageTiling            tiling
        usage,                                                     // VkImageUsageFlags        usage
        VK_SHARING_MODE_EXCLUSIVE,                                 // VkSharingMode            sharingMode
        0u,                                                        // uint32_t                    queueFamilyIndexCount
        nullptr,                                                   // const uint32_t*            pQueueFamilyIndices
        VK_IMAGE_LAYOUT_UNDEFINED,                                 // VkImageLayout            initialLayout
    };

    de::MovePtr<ImageWithMemory> normalImage;
    Move<VkImage> sparseImage;
    Move<VkSemaphore> sparseImageSemaphore;
    std::vector<DeviceMemorySp> deviceMemUniquePtrVec;

    if (!useSparseBinding)
        // Create a normal image and bind device memory too
        normalImage = de::MovePtr<ImageWithMemory>(
            new ImageWithMemory(vk, device, allocator, imageCreateInfo, MemoryRequirement::Any));
    else
        // Create an image now and bind device memory later
        sparseImage = createImage(vk, device, &imageCreateInfo);

    const VkImage &testImage = useSparseBinding ? sparseImage.get() : (*normalImage).get();

    if (useSparseBinding)
    {
        const InstanceInterface &instance     = m_context.getInstanceInterface();
        const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
        const VkQueue sparseQueue             = m_context.getSparseQueue();
        std::vector<VkSparseMemoryBind> sparseMemoryBindings;

        const VkMemoryRequirements imageMemoryReqs = getImageMemoryRequirements(vk, device, testImage);

        if (imageMemoryReqs.size > getPhysicalDeviceProperties(instance, physicalDevice).limits.sparseAddressSpaceSize)
            TCU_THROW(NotSupportedError, "Required memory size for sparse resource exceeds device limits");

        DE_ASSERT((imageMemoryReqs.size % imageMemoryReqs.alignment) == 0);

        uint32_t memoryType;
        bool memoryTypeFound =
            getMemoryType(instance, physicalDevice, imageMemoryReqs, MemoryRequirement::Any, memoryType);
        if (!memoryTypeFound)
            return tcu::TestStatus::fail("Required memory type for sparse resouce not found");

        const uint32_t numSparseBindings = static_cast<uint32_t>(imageMemoryReqs.size / imageMemoryReqs.alignment);
        for (uint32_t bindingIdx = 0; bindingIdx < numSparseBindings; ++bindingIdx)
        {
            const VkSparseMemoryBind sparseMemoryBinding =
                makeSparseMemoryBinding(vk, device, imageMemoryReqs.alignment, memoryType,
                                        imageMemoryReqs.alignment * bindingIdx, (VkSparseMemoryBindFlags)0u);

            deviceMemUniquePtrVec.push_back(makeVkSharedPtr(Move<VkDeviceMemory>(
                check<VkDeviceMemory>(sparseMemoryBinding.memory), Deleter<VkDeviceMemory>(vk, device, nullptr))));

            sparseMemoryBindings.push_back(sparseMemoryBinding);
        }

        const VkSparseImageOpaqueMemoryBindInfo opaqueBindingInfo = makeSparseImageOpaqueMemoryBindInfo(
            testImage, de::sizeU32(sparseMemoryBindings), de::dataOrNull(sparseMemoryBindings));

        sparseImageSemaphore = createSemaphore(vk, device);

        const VkBindSparseInfo bindSparseInfo = {
            VK_STRUCTURE_TYPE_BIND_SPARSE_INFO, //VkStructureType sType;
            nullptr,                            //const void* pNext;
            0u,                                 //uint32_t waitSemaphoreCount;
            nullptr,                            //const VkSemaphore* pWaitSemaphores;
            0u,                                 //uint32_t bufferBindCount;
            nullptr,                            //const VkSparseBufferMemoryBindInfo* pBufferBinds;
            1u,                                 //uint32_t imageOpaqueBindCount;
            &opaqueBindingInfo,                 //const VkSparseImageOpaqueMemoryBindInfo* pImageOpaqueBinds;
            0u,                                 //uint32_t imageBindCount;
            nullptr,                            //const VkSparseImageMemoryBindInfo* pImageBinds;
            1u,                                 //uint32_t signalSemaphoreCount;
            &sparseImageSemaphore.get()         //const VkSemaphore* pSignalSemaphores;
        };

        VK_CHECK(vk.queueBindSparse(sparseQueue, 1u, &bindSparseInfo, VK_NULL_HANDLE));
    }

    // Make an image view covering one of the mip levels.
    const VkImageSubresourceRange viewSubresourceRange = makeImageSubresourceRange(
        VK_IMAGE_ASPECT_COLOR_BIT, m_testParameters.mipLevel, 1u, m_testParameters.layerNdx, 1u);
    const Unique<VkImageView> imageView(makeImageView(vk, device, testImage, VK_IMAGE_VIEW_TYPE_2D,
                                                      m_testParameters.imageFormat, viewSubresourceRange));

    // resultImage is used in sampler / combined image sampler tests to verify the sampled image.
    MovePtr<ImageWithMemory> resultImage;
    VkImageSubresourceRange resultImgSubresourceRange = {};
    Move<VkImageView> resultImageView;
    Move<VkSampler> sampler;
    if (useSampler)
    {
        const VkImageCreateInfo resultImageCreateInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                         // VkStructureType            sType
            nullptr,                                                     // const void*                pNext
            0U,                                                          // VkImageCreateFlags        flags
            VK_IMAGE_TYPE_2D,                                            // VkImageType                imageType
            m_testParameters.imageFormat,                                // VkFormat                    format
            makeExtent3D(testMipLevelSize.x(), testMipLevelSize.y(), 1), // VkExtent3D                extent
            1u,                                                          // uint32_t                    mipLevels
            1u,                                                          // uint32_t                    arrayLayers
            VK_SAMPLE_COUNT_1_BIT,                                       // VkSampleCountFlagBits    samples
            VK_IMAGE_TILING_OPTIMAL,                                     // VkImageTiling            tiling
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags        usage
            VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode            sharingMode
            0u,                                  // uint32_t                    queueFamilyIndexCount
            nullptr,                             // const uint32_t*            pQueueFamilyIndices
            VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout            initialLayout
        };

        resultImage = MovePtr<ImageWithMemory>(
            new ImageWithMemory(vk, device, allocator, resultImageCreateInfo, MemoryRequirement::Any));
        resultImgSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
        resultImageView = makeImageView(vk, device, **resultImage, VK_IMAGE_VIEW_TYPE_2D, m_testParameters.imageFormat,
                                        resultImgSubresourceRange);

        const VkSamplerCreateInfo samplerCreateInfo = {
            VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,   // VkStructureType            sType
            nullptr,                                 // const void*                pNext
            (VkSamplerCreateFlags)0,                 // VkSamplerCreateFlags        flags
            VK_FILTER_NEAREST,                       // VkFilter                    magFilter
            VK_FILTER_NEAREST,                       // VkFilter                    minFilter
            VK_SAMPLER_MIPMAP_MODE_NEAREST,          // VkSamplerMipmapMode        mipmapMode
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode        addressModeU
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode        addressModeV
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode        addressModeW
            0.0f,                                    // float                    mipLodBias
            VK_FALSE,                                // VkBool32                    anisotropyEnable
            1.0f,                                    // float                    maxAnisotropy
            VK_FALSE,                                // VkBool32                    compareEnable
            VK_COMPARE_OP_ALWAYS,                    // VkCompareOp                compareOp
            0.0f,                                    // float                    minLod
            1.0f,                                    // float                    maxLod
            VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, // VkBorderColor            borderColor
            VK_FALSE,                                // VkBool32                    unnormalizedCoordinates
        };
        sampler = createSampler(vk, device, &samplerCreateInfo);
    }

    // Create the descriptor set.
    DescriptorSetLayoutBuilder descriptorSetLayoutBuilder;
    DescriptorPoolBuilder descriptorPoolBuilder;

    VkShaderStageFlags shaderStage =
        m_testParameters.testType == Compute ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
    VkPipelineStageFlags pipelineStage = m_testParameters.testType == Compute ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT :
                                                                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    switch (m_testParameters.imageType)
    {
    case StorageImage:
        descriptorSetLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, shaderStage);
        descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        break;
    case Sampler:
        descriptorSetLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, shaderStage);
        descriptorSetLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLER, shaderStage);
        descriptorSetLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, shaderStage);
        descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
        descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_SAMPLER);
        descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        break;
    case CombinedImageSampler:
        descriptorSetLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, shaderStage);
        descriptorSetLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, shaderStage);
        descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        break;
    default:
        TCU_THROW(InternalError, "Unimplemented testImage type.");
    }

    // Prepare the command buffer.
    const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Start recording commands.
    beginCommandBuffer(vk, *cmdBuffer);

    if (useSampler)
    {
        // Clear the result image.
        const VkImageMemoryBarrier preImageBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                // const void* pNext;
            0u,                                     // VkAccessFlags srcAccessMask;
            VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
            VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout oldLayout;
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout newLayout;
            queueFamilyIndex,                       // uint32_t srcQueueFamilyIndex;
            queueFamilyIndex,                       // uint32_t dstQueueFamilyIndex;
            **resultImage,                          // VkImage image;
            resultImgSubresourceRange               // VkImageSubresourceRange subresourceRange;
        };
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &preImageBarrier);

        const VkClearColorValue clearColor = makeClearValueColor(tcu::Vec4(0, 0, 0, 1)).color;
        vk.cmdClearColorImage(*cmdBuffer, **resultImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1,
                              &resultImgSubresourceRange);

        const VkImageMemoryBarrier postImageBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                // const void* pNext;
            VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
            VK_ACCESS_SHADER_WRITE_BIT,             // VkAccessFlags dstAccessMask;
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
            VK_IMAGE_LAYOUT_GENERAL,                // VkImageLayout newLayout;
            queueFamilyIndex,                       // uint32_t srcQueueFamilyIndex;
            queueFamilyIndex,                       // uint32_t dstQueueFamilyIndex;
            **resultImage,                          // VkImage image;
            resultImgSubresourceRange               // VkImageSubresourceRange subresourceRange;
        };
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, pipelineStage, (VkDependencyFlags)0, 0,
                              nullptr, 0, nullptr, 1, &postImageBarrier);
    }
    else
    {
        const auto singleMipSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
        const auto allMipsSRR   = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, mipLevelCount, 0u, 1u);

        // Clear the test image.
        const VkImageMemoryBarrier preImageBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                // const void* pNext;
            0u,                                     // VkAccessFlags srcAccessMask;
            VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
            VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout oldLayout;
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout newLayout;
            queueFamilyIndex,                       // uint32_t srcQueueFamilyIndex;
            queueFamilyIndex,                       // uint32_t dstQueueFamilyIndex;
            testImage,                              // VkImage image;
            allMipsSRR,                             // VkImageSubresourceRange subresourceRange;
        };
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &preImageBarrier);

        const VkClearColorValue clearColor = makeClearValueColor(tcu::Vec4(0, 0, 0, 1)).color;
        vk.cmdClearColorImage(*cmdBuffer, testImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1,
                              &singleMipSRR);

        const VkImageMemoryBarrier postImageBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                // const void* pNext;
            VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
            VK_ACCESS_SHADER_WRITE_BIT,             // VkAccessFlags dstAccessMask;
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
            VK_IMAGE_LAYOUT_GENERAL,                // VkImageLayout newLayout;
            queueFamilyIndex,                       // uint32_t srcQueueFamilyIndex;
            queueFamilyIndex,                       // uint32_t dstQueueFamilyIndex;
            testImage,                              // VkImage image;
            allMipsSRR,                             // VkImageSubresourceRange subresourceRange;
        };
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, pipelineStage, (VkDependencyFlags)0, 0,
                              nullptr, 0, nullptr, 1, &postImageBarrier);
    }

    if (useSampler)
    {
        // Copy the input image to the target mip level.
        std::vector<VkBufferImageCopy> copies;
        copies.push_back(makeBufferImageCopy(
            makeExtent3D(testMipLevelSize),
            makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, m_testParameters.mipLevel, 0, 1)));
        copyBufferToImage(vk, *cmdBuffer, **inputImageBuffer, bufferSize, copies, VK_IMAGE_ASPECT_COLOR_BIT,
                          mipLevelCount, 1u, testImage, VK_IMAGE_LAYOUT_GENERAL, pipelineStage);
    }

    const Move<VkDescriptorSetLayout> descriptorSetLayout(descriptorSetLayoutBuilder.build(vk, device));
    const Move<VkDescriptorPool> descriptorPool(
        descriptorPoolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
    const VkDescriptorImageInfo testImageDescriptorInfo =
        makeDescriptorImageInfo(*sampler, *imageView, VK_IMAGE_LAYOUT_GENERAL);

    // Write descriptor update.
    {
        DescriptorSetUpdateBuilder descriptorSetUpdateBuilder;
        uint32_t bindingIdx = 0;

        switch (m_testParameters.imageType)
        {
        case StorageImage:
            descriptorSetUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                                                   VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &testImageDescriptorInfo);
            break;
        case Sampler:
            descriptorSetUpdateBuilder.writeSingle(*descriptorSet,
                                                   DescriptorSetUpdateBuilder::Location::binding(bindingIdx++),
                                                   VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &testImageDescriptorInfo);
            descriptorSetUpdateBuilder.writeSingle(*descriptorSet,
                                                   DescriptorSetUpdateBuilder::Location::binding(bindingIdx++),
                                                   VK_DESCRIPTOR_TYPE_SAMPLER, &testImageDescriptorInfo);
            break;
        case CombinedImageSampler:
            descriptorSetUpdateBuilder.writeSingle(*descriptorSet,
                                                   DescriptorSetUpdateBuilder::Location::binding(bindingIdx++),
                                                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &testImageDescriptorInfo);
            break;
        }

        if (useSampler)
        {
            const VkDescriptorImageInfo resultImageDescriptorInfo =
                makeDescriptorImageInfo(VK_NULL_HANDLE, *resultImageView, VK_IMAGE_LAYOUT_GENERAL);
            descriptorSetUpdateBuilder.writeSingle(*descriptorSet,
                                                   DescriptorSetUpdateBuilder::Location::binding(bindingIdx),
                                                   VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &resultImageDescriptorInfo);
        }

        descriptorSetUpdateBuilder.update(vk, device);
    }

    const VkSemaphore *sparseImageSemaphorePtr = (useSparseBinding ? &sparseImageSemaphore.get() : nullptr);

    if (m_testParameters.testType == Compute)
        runComputePipeline(*descriptorSet, *descriptorSetLayout, testMipLevelSize, *cmdBuffer,
                           useSampler ? **resultImage : testImage, *outputBuffer, sparseImageSemaphorePtr);
    else
        runGraphicsPipeline(*descriptorSet, *descriptorSetLayout, testMipLevelSize, *cmdBuffer,
                            useSampler ? **resultImage : testImage, *outputBuffer, sparseImageSemaphorePtr);

    // Validate the results.
    {
        // Create a reference image.
        // The reference image has always a depth of 1, because it will be compared to the 2D result image (sampler cases) or to a single layer of a 3D image.
        tcu::TextureLevel referenceImage(textureFormat, testMipLevelSize.x(), testMipLevelSize.y(), 1u);
        fillImage(referenceImage.getAccess(), 0u);

        const Allocation &outputBufferAllocation = outputBuffer.getAllocation();
        invalidateAlloc(vk, device, outputBufferAllocation);

        const uint32_t *bufferPtr = static_cast<uint32_t *>(outputBufferAllocation.getHostPtr());
        tcu::ConstPixelBufferAccess pixelBufferAccess(mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM), testMipLevelSize.x(),
                                                      testMipLevelSize.y(), 1u, bufferPtr);

        if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Result", "Result comparison",
                                        referenceImage, pixelBufferAccess, tcu::Vec4(0.01f), tcu::COMPARE_LOG_ON_ERROR))
            return tcu::TestStatus::fail("Pixel comparison failed.");
    }

    return tcu::TestStatus::pass("pass");
}

class ComputeImage2DView3DImageTest : public vkt::TestCase
{
public:
    ComputeImage2DView3DImageTest(tcu::TestContext &testContext, const char *name, const TestParameters &testParameters)
        : vkt::TestCase(testContext, name)
        , m_testParameters(testParameters)
    {
    }
    virtual ~ComputeImage2DView3DImageTest(void)
    {
    }

    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual void checkSupport(Context &context) const;
    virtual TestInstance *createInstance(Context &context) const;

private:
    const TestParameters m_testParameters;
};

void ComputeImage2DView3DImageTest::checkSupport(Context &context) const
{
    DE_ASSERT(m_testParameters.pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC);

    if (!context.isDeviceFunctionalitySupported("VK_EXT_image_2d_view_of_3d"))
        TCU_THROW(NotSupportedError, "VK_EXT_image_2d_view_of_3d functionality not supported.");

    if (!context.getImage2DViewOf3DFeaturesEXT().image2DViewOf3D)
        TCU_THROW(NotSupportedError, "image2DViewOf3D not supported.");

    if (m_testParameters.imageType != StorageImage && !context.getImage2DViewOf3DFeaturesEXT().sampler2DViewOf3D)
        TCU_THROW(NotSupportedError, "sampler2DViewOf3D not supported.");

    if (m_testParameters.imageBindingType == Sparse)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_BINDING);
}

void ComputeImage2DView3DImageTest::initPrograms(SourceCollections &sourceCollections) const
{
    std::ostringstream src;
    tcu::IVec3 mipLevelSize = computeMipLevelSize(m_testParameters.imageSize, m_testParameters.mipLevel);
    if (m_testParameters.imageType == StorageImage)
    {
        src << "#version 450 core\n"
            << "layout (local_size_x = 1, local_size_y = 1) in;\n"
            << "layout (binding = 0, rgba8) writeonly uniform highp image2D storageImage;\n"
            << "void main (void) {\n"
            << "    ivec2 uv = ivec2(gl_GlobalInvocationID.xy);\n"
            << "    float c = float((uv.x + uv.y) & 1);\n"
            << "    vec4 color = vec4(c, c, c, 1.0);\n"
            << "    imageStore(storageImage, uv, color);\n"
            << "}\n";
    }
    else if (m_testParameters.imageType == Sampler)
    {
        src << "#version 450 core\n"
            << "layout (local_size_x = 1, local_size_y = 1) in;\n"
            << "layout (set=0, binding = 0) uniform texture2D image;\n"
            << "layout (set=0, binding = 1) uniform sampler samp;\n"
            << "layout (rgba8, set=0, binding = 2) writeonly uniform highp image2D verifyImage;\n"
            << "void main (void) {\n"
            << "    ivec2 uv = ivec2(gl_GlobalInvocationID.xy);\n"
            << "    vec2 texCoord = vec2(gl_GlobalInvocationID.xy) / " << mipLevelSize.x() << ".0;\n"
            << "    vec4 color = texture(sampler2D(image, samp), texCoord);\n"
            << "    imageStore(verifyImage, uv, color);\n"
            << "}\n";
    }
    else if (m_testParameters.imageType == CombinedImageSampler)
    {
        src << "#version 450 core\n"
            << "layout (local_size_x = 1, local_size_y = 1) in;\n"
            << "layout (binding = 0) uniform sampler2D combinedSampler;\n"
            << "layout (rgba8, set=0, binding=1) writeonly uniform highp image2D verifyImage;\n"
            << "void main (void) {\n"
            << "    ivec2 uv = ivec2(gl_GlobalInvocationID.xy);\n"
            << "    vec2 texCoord = vec2(gl_GlobalInvocationID.xy) / " << mipLevelSize.x() << ".0;\n"
            << "    vec4 color = texture(combinedSampler, texCoord);\n"
            << "    imageStore(verifyImage, uv, color);\n"
            << "}\n";
    }

    sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance *ComputeImage2DView3DImageTest::createInstance(Context &context) const
{
    return new Image2DView3DImageInstance(context, m_testParameters);
}

class FragmentImage2DView3DImageTest : public vkt::TestCase
{
public:
    FragmentImage2DView3DImageTest(tcu::TestContext &testContext, const char *name,
                                   const TestParameters &testParameters)
        : vkt::TestCase(testContext, name)
        , m_testParameters(testParameters)
    {
    }
    virtual ~FragmentImage2DView3DImageTest(void)
    {
    }

    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual void checkSupport(Context &context) const;
    virtual TestInstance *createInstance(Context &context) const;

private:
    const TestParameters m_testParameters;
};

void FragmentImage2DView3DImageTest::initPrograms(SourceCollections &sourceCollections) const
{
    std::stringstream vertShader;
    vertShader << "#version 450 core\n"
               << "layout(location = 0) in vec4 in_position;\n"
               << "out gl_PerVertex {\n"
               << "    vec4  gl_Position;\n"
               << "    float gl_PointSize;\n"
               << "};\n"
               << "void main() {\n"
               << "    gl_PointSize = 1.0;\n"
               << "    gl_Position  = in_position;\n"
               << "}\n";
    sourceCollections.glslSources.add("vert") << glu::VertexSource(vertShader.str());

    tcu::IVec3 mipLevelSize = computeMipLevelSize(m_testParameters.imageSize, m_testParameters.mipLevel);
    std::stringstream fragShader;
    if (m_testParameters.imageType == StorageImage)
    {
        fragShader << "#version 450 core\n"
                   << "layout(rgba8, set = 0, binding = 0) uniform image2D storageImage;\n"
                   << "void main()\n"
                   << "{\n"
                   << "    ivec2 uv = ivec2(gl_FragCoord.xy);\n"
                   << "    float c = float((uv.x + uv.y) & 1);\n"
                   << "    vec4 color = vec4(c, c, c, 1.0);\n"
                   << "    imageStore(storageImage, uv, color);\n"
                   << "}\n";
    }

    else if (m_testParameters.imageType == Sampler)
    {
        fragShader << "#version 450 core\n"
                   << "layout (set = 0, binding = 0) uniform texture2D image;\n"
                   << "layout (set = 0, binding = 1) uniform sampler samp;\n"
                   << "layout (rgba8, set = 0, binding = 2) uniform image2D verifyImage;\n"
                   << "void main (void) {\n"
                   << "    ivec2 uv = ivec2(gl_FragCoord.xy);\n"
                   << "    vec2 texCoord = gl_FragCoord.xy / " << mipLevelSize.x() << ".0;\n"
                   << "    vec4 color = texture(sampler2D(image, samp), texCoord);\n"
                   << "    imageStore(verifyImage, uv, color);\n"
                   << "}\n";
    }
    else if (m_testParameters.imageType == CombinedImageSampler)
    {
        fragShader << "#version 450 core\n"
                   << "layout (set = 0, binding = 0) uniform sampler2D combinedSampler;\n"
                   << "layout (rgba8, set = 0, binding = 1) uniform image2D verifyImage;\n"
                   << "void main (void) {\n"
                   << "    ivec2 uv = ivec2(gl_FragCoord.xy);\n"
                   << "    vec2 texCoord = gl_FragCoord.xy / " << mipLevelSize.x() << ".0;\n"
                   << "    vec4 color = texture(combinedSampler, texCoord);\n"
                   << "    imageStore(verifyImage, uv, color);\n"
                   << "}\n";
    }
    sourceCollections.glslSources.add("frag") << glu::FragmentSource(fragShader.str());
}

void FragmentImage2DView3DImageTest::checkSupport(Context &context) const
{
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_testParameters.pipelineConstructionType);

    if (!context.isDeviceFunctionalitySupported("VK_EXT_image_2d_view_of_3d"))
        TCU_THROW(NotSupportedError, "VK_EXT_image_2d_view_of_3d functionality not supported.");

    if (!context.getImage2DViewOf3DFeaturesEXT().image2DViewOf3D)
        TCU_THROW(NotSupportedError, "image2DViewOf3D not supported.");

    if (m_testParameters.imageType != StorageImage && !context.getImage2DViewOf3DFeaturesEXT().sampler2DViewOf3D)
        TCU_THROW(NotSupportedError, "texture2DViewOf3D not supported.");

    if (!context.getDeviceFeatures().fragmentStoresAndAtomics)
        TCU_THROW(NotSupportedError, "fragmentStoresAndAtomics not supported");

    if (m_testParameters.imageBindingType == Sparse)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_BINDING);
}

TestInstance *FragmentImage2DView3DImageTest::createInstance(Context &context) const
{
    return new Image2DView3DImageInstance(context, m_testParameters);
}

} // namespace

tcu::TestCaseGroup *createImage2DViewOf3DTests(tcu::TestContext &testCtx,
                                               PipelineConstructionType pipelineConstructionType)
{
    de::MovePtr<tcu::TestCaseGroup> imageTests(new tcu::TestCaseGroup(testCtx, "image_2d_view_3d_image"));
    de::MovePtr<tcu::TestCaseGroup> computeGroup(new tcu::TestCaseGroup(testCtx, "compute"));
    de::MovePtr<tcu::TestCaseGroup> fragmentGroup(new tcu::TestCaseGroup(testCtx, "fragment"));

    const struct
    {
        const ImageAccessType imageType;
        const std::string name;
    } imageAccessTypes[]{
        {StorageImage, "storage"}, {Sampler, "sampler"}, {CombinedImageSampler, "combined_image_sampler"}};

    const int32_t imageDimension = 64;
    for (const auto &imageAccessType : imageAccessTypes)
    {
        de::MovePtr<tcu::TestCaseGroup> computeSubGroup(new tcu::TestCaseGroup(testCtx, imageAccessType.name.c_str()));
        de::MovePtr<tcu::TestCaseGroup> fragmentSubGroup(new tcu::TestCaseGroup(testCtx, imageAccessType.name.c_str()));
        for (uint32_t mipLevel = 0; mipLevel < 3; mipLevel += 2)
        {
            // Test the first and the last layer of the mip level.
            std::vector<int32_t> layers = {0, computeMipLevelDimension(imageDimension, mipLevel) - 1};
            for (const auto &layer : layers)
            {
                for (const auto &imageBindingType : {ImageBindingType::Normal, ImageBindingType::Sparse})
                {
                    TestParameters testParameters{
                        tcu::IVec3(imageDimension), // IVec3                        imageSize
                        mipLevel,                   // uint32_t                        mipLevel
                        layer,                      // int32_t                        layerNdx
                        imageAccessType.imageType,  // ImageAccessType                imageType
                        Fragment,                   // TestType                        testType
                        VK_FORMAT_R8G8B8A8_UNORM,   // VkFormat                        imageFormat
                        pipelineConstructionType,   // PipelineConstructionType        pipelineConstructionType
                        imageBindingType            // ImageBindingType             imageBindingType
                    };
                    std::string testName = "mip" + std::to_string(mipLevel) + "_layer" + std::to_string(layer) +
                                           (imageBindingType == Sparse ? "_sparse" : "");
                    fragmentSubGroup->addChild(
                        new FragmentImage2DView3DImageTest(testCtx, testName.c_str(), testParameters));

                    if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
                    {
                        testParameters.testType = Compute;
                        computeSubGroup->addChild(
                            new ComputeImage2DView3DImageTest(testCtx, testName.c_str(), testParameters));
                    }
                }
            }
        }
        computeGroup->addChild(computeSubGroup.release());
        fragmentGroup->addChild(fragmentSubGroup.release());
    }

    imageTests->addChild(computeGroup.release());
    imageTests->addChild(fragmentGroup.release());
    return imageTests.release();
}

} // namespace pipeline
} // namespace vkt
