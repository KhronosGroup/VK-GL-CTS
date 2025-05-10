/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
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
 * \file  vktImageTranscodingSupportTests.cpp
 * \brief Transcoding support tests
 *//*--------------------------------------------------------------------*/

#include "vktImageTranscodingSupportTests.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deSharedPtr.hpp"
#include "deRandom.hpp"

#include "vktTestCaseUtil.hpp"
#include "vkPrograms.hpp"
#include "vkImageUtil.hpp"
#include "vktImageTestsUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuTexture.hpp"
#include "tcuCompressedTexture.hpp"
#include "tcuVectorType.hpp"
#include "tcuResource.hpp"
#include "tcuImageIO.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"
#include "tcuRGBA.hpp"
#include "tcuSurface.hpp"
#include "tcuFloat.hpp"

#include <vector>
#include <iomanip>

using namespace vk;
namespace vkt
{
namespace image
{
namespace
{
using de::MovePtr;
using de::Random;
using de::SharedPtr;
using std::string;
using std::vector;
using tcu::Archive;
using tcu::CompressedTexFormat;
using tcu::CompressedTexture;
using tcu::ConstPixelBufferAccess;
using tcu::IVec3;
using tcu::Resource;
using tcu::TestContext;
using tcu::TestStatus;
using tcu::UVec3;

enum Operation
{
    OPERATION_ATTACHMENT_READ,
    OPERATION_ATTACHMENT_WRITE,
    OPERATION_TEXTURE_READ,
    OPERATION_TEXTURE_WRITE,
    OPERATION_LAST
};

struct TestParameters
{
    Operation operation;
    UVec3 size;
    ImageType imageType;
    VkImageUsageFlagBits testedImageUsageFeature;
    VkFormat featuredFormat;
    VkFormat featurelessFormat;
    VkImageUsageFlags testedImageUsage;
    VkImageUsageFlags pairedImageUsage;
    const VkFormat *compatibleFormats;
};

const uint32_t SINGLE_LEVEL = 1u;
const uint32_t SINGLE_LAYER = 1u;

class BasicTranscodingTestInstance : public TestInstance
{
public:
    BasicTranscodingTestInstance(Context &context, const TestParameters &parameters);
    virtual TestStatus iterate(void) = 0;

protected:
    void generateData(uint8_t *toFill, size_t size, const VkFormat format = VK_FORMAT_UNDEFINED);
    const TestParameters m_parameters;
};

BasicTranscodingTestInstance::BasicTranscodingTestInstance(Context &context, const TestParameters &parameters)
    : TestInstance(context)
    , m_parameters(parameters)
{
}

// Replace Infs and NaNs with the largest normal value.
// Replace denormal numbers with the smallest normal value.
// Leave the rest untouched.
// T is a tcu::Float specialization.
template <class T>
void fixFloatIfNeeded(uint8_t *ptr_)
{
    T *ptr = reinterpret_cast<T *>(ptr_);
    if (ptr->isInf() || ptr->isNaN())
        *ptr = T::largestNormal(ptr->sign());
    else if (ptr->isDenorm())
        *ptr = T::smallestNormal(ptr->sign());
}

void BasicTranscodingTestInstance::generateData(uint8_t *toFill, size_t size, const VkFormat format)
{
    const uint8_t pattern[] = {
        // 64-bit values
        0x11,
        0x11,
        0x11,
        0x11,
        0x22,
        0x22,
        0x22,
        0x22,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x01,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x01,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x01,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x01,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xFF,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xFF,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xFF,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xFF,
        0x00,
        0x00,
        0x00,
        0x7F,
        0xF0,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00, // Positive infinity
        0xFF,
        0xF0,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00, // Negative infinity
        0x7F,
        0xF0,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x01, // Start of a signalling NaN (NANS)
        0x7F,
        0xF7,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF, // End of a signalling NaN (NANS)
        0xFF,
        0xF0,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x01, // Start of a signalling NaN (NANS)
        0xFF,
        0xF7,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF, // End of a signalling NaN (NANS)
        0x7F,
        0xF8,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00, // Start of a quiet NaN (NANQ)
        0x7F,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF, // End of of a quiet NaN (NANQ)
        0xFF,
        0xF8,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00, // Start of a quiet NaN (NANQ)
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF, // End of a quiet NaN (NANQ)
        // 32-bit values
        0x7F,
        0x80,
        0x00,
        0x00, // Positive infinity
        0xFF,
        0x80,
        0x00,
        0x00, // Negative infinity
        0x7F,
        0x80,
        0x00,
        0x01, // Start of a signalling NaN (NANS)
        0x7F,
        0xBF,
        0xFF,
        0xFF, // End of a signalling NaN (NANS)
        0xFF,
        0x80,
        0x00,
        0x01, // Start of a signalling NaN (NANS)
        0xFF,
        0xBF,
        0xFF,
        0xFF, // End of a signalling NaN (NANS)
        0x7F,
        0xC0,
        0x00,
        0x00, // Start of a quiet NaN (NANQ)
        0x7F,
        0xFF,
        0xFF,
        0xFF, // End of of a quiet NaN (NANQ)
        0xFF,
        0xC0,
        0x00,
        0x00, // Start of a quiet NaN (NANQ)
        0xFF,
        0xFF,
        0xFF,
        0xFF, // End of a quiet NaN (NANQ)
        0xAA,
        0xAA,
        0xAA,
        0xAA,
        0x55,
        0x55,
        0x55,
        0x55,
    };

    uint8_t *start   = toFill;
    size_t sizeToRnd = size;

    // Pattern part
    if (size >= 2 * sizeof(pattern))
    {
        // Rotated pattern
        for (size_t i = 0; i < sizeof(pattern); i++)
            start[sizeof(pattern) - i - 1] = pattern[i];

        start += sizeof(pattern);
        sizeToRnd -= sizeof(pattern);

        // Direct pattern
        deMemcpy(start, pattern, sizeof(pattern));

        start += sizeof(pattern);
        sizeToRnd -= sizeof(pattern);
    }

    // Random part
    {
        DE_ASSERT(sizeToRnd % sizeof(uint32_t) == 0);

        uint32_t *start32  = reinterpret_cast<uint32_t *>(start);
        size_t sizeToRnd32 = sizeToRnd / sizeof(uint32_t);
        Random rnd(static_cast<uint32_t>(format));

        for (size_t i = 0; i < sizeToRnd32; i++)
            start32[i] = rnd.getUint32();
    }

    {
        // Remove certain values that may not be preserved based on the uncompressed view format
        if (isSnormFormat(m_parameters.featuredFormat))
        {
            tcu::TextureFormat textureFormat = mapVkFormat(m_parameters.featuredFormat);

            if (textureFormat.type == tcu::TextureFormat::SNORM_INT8)
            {
                for (size_t i = 0; i < size; i++)
                {
                    // SNORM fix: due to write operation in SNORM format
                    // replaces 0x80 to 0x81, remove these values from test
                    if (toFill[i] == 0x80)
                        toFill[i] = 0x81;
                }
            }
            else
            {
                for (size_t i = 0; i < size; i += 2)
                {
                    // SNORM fix: due to write operation in SNORM format
                    // replaces 0x00 0x80 to 0x01 0x80
                    if (toFill[i] == 0x00 && toFill[i + 1] == 0x80)
                        toFill[i + 1] = 0x81;
                }
            }
        }
        else if (isFloatFormat(m_parameters.featuredFormat))
        {
            tcu::TextureFormat textureFormat = mapVkFormat(m_parameters.featuredFormat);

            if (textureFormat.type == tcu::TextureFormat::HALF_FLOAT)
            {
                for (size_t i = 0; i < size; i += 2)
                    fixFloatIfNeeded<tcu::Float16>(toFill + i);
            }
            else if (textureFormat.type == tcu::TextureFormat::FLOAT)
            {
                for (size_t i = 0; i < size; i += 4)
                    fixFloatIfNeeded<tcu::Float16>(toFill + i);

                for (size_t i = 0; i < size; i += 4)
                    fixFloatIfNeeded<tcu::Float32>(toFill + i);
            }
        }
    }
}

class GraphicsAttachmentsTestInstance : public BasicTranscodingTestInstance
{
public:
    GraphicsAttachmentsTestInstance(Context &context, const TestParameters &parameters);
    virtual TestStatus iterate(void);

protected:
    VkImageCreateInfo makeCreateImageInfo(const VkFormat format, const ImageType type, const UVec3 &size,
                                          const VkImageUsageFlags usageFlags, const bool extendedImageCreateFlag);
    VkImageViewUsageCreateInfo makeImageViewUsageCreateInfo(VkImageUsageFlags imageUsageFlags);
    VkDeviceSize getUncompressedImageData(const VkFormat format, const UVec3 &size, std::vector<uint8_t> &data);
    virtual void transcode(std::vector<uint8_t> &srcData, std::vector<uint8_t> &dstData,
                           de::MovePtr<Image> &outputImage);
    bool compareAndLog(const void *reference, const void *result, size_t size);
};

GraphicsAttachmentsTestInstance::GraphicsAttachmentsTestInstance(Context &context, const TestParameters &parameters)
    : BasicTranscodingTestInstance(context, parameters)
{
}

TestStatus GraphicsAttachmentsTestInstance::iterate(void)
{
    std::vector<uint8_t> srcData;
    std::vector<uint8_t> dstData;
    de::MovePtr<Image> outputImage;

    transcode(srcData, dstData, outputImage);

    DE_ASSERT(srcData.size() > 0 && srcData.size() == dstData.size());

    if (!compareAndLog(&srcData[0], &dstData[0], srcData.size()))
        return TestStatus::fail("Output differs from input");

    return TestStatus::pass("Pass");
}

void GraphicsAttachmentsTestInstance::transcode(std::vector<uint8_t> &srcData, std::vector<uint8_t> &dstData,
                                                de::MovePtr<Image> &outputImage)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    const VkQueue queue             = m_context.getUniversalQueue();
    Allocator &allocator            = m_context.getDefaultAllocator();

    const VkImageSubresourceRange subresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, SINGLE_LEVEL, 0u, SINGLE_LAYER);
    const VkImageViewUsageCreateInfo *imageViewUsageNull = nullptr;
    const VkImageViewUsageCreateInfo imageViewUsage      = makeImageViewUsageCreateInfo(m_parameters.testedImageUsage);

    const VkFormat srcFormat = (m_parameters.operation == OPERATION_ATTACHMENT_READ)  ? m_parameters.featurelessFormat :
                               (m_parameters.operation == OPERATION_ATTACHMENT_WRITE) ? m_parameters.featuredFormat :
                                                                                        VK_FORMAT_UNDEFINED;
    const bool srcExtendedImageCreate = (m_parameters.operation == OPERATION_ATTACHMENT_READ)  ? true :
                                        (m_parameters.operation == OPERATION_ATTACHMENT_WRITE) ? false :
                                                                                                 false;
    const VkImageUsageFlags srcImageUsageFlags =
        (m_parameters.operation == OPERATION_ATTACHMENT_READ)  ? m_parameters.testedImageUsage :
        (m_parameters.operation == OPERATION_ATTACHMENT_WRITE) ? m_parameters.pairedImageUsage :
                                                                 0;
    const VkImageViewUsageCreateInfo *srcImageViewUsageFlags =
        (m_parameters.operation == OPERATION_ATTACHMENT_READ)  ? &imageViewUsage :
        (m_parameters.operation == OPERATION_ATTACHMENT_WRITE) ? imageViewUsageNull :
                                                                 imageViewUsageNull;
    const VkDeviceSize srcImageSizeInBytes = getUncompressedImageData(srcFormat, m_parameters.size, srcData);

    const VkFormat dstFormat = (m_parameters.operation == OPERATION_ATTACHMENT_READ)  ? m_parameters.featuredFormat :
                               (m_parameters.operation == OPERATION_ATTACHMENT_WRITE) ? m_parameters.featurelessFormat :
                                                                                        VK_FORMAT_UNDEFINED;
    const bool dstExtendedImageCreate = (m_parameters.operation == OPERATION_ATTACHMENT_READ)  ? false :
                                        (m_parameters.operation == OPERATION_ATTACHMENT_WRITE) ? true :
                                                                                                 false;
    const VkImageUsageFlags dstImageUsageFlags =
        (m_parameters.operation == OPERATION_ATTACHMENT_READ)  ? m_parameters.pairedImageUsage :
        (m_parameters.operation == OPERATION_ATTACHMENT_WRITE) ? m_parameters.testedImageUsage :
                                                                 0;
    const VkImageViewUsageCreateInfo *dstImageViewUsageFlags =
        (m_parameters.operation == OPERATION_ATTACHMENT_READ)  ? imageViewUsageNull :
        (m_parameters.operation == OPERATION_ATTACHMENT_WRITE) ? &imageViewUsage :
                                                                 imageViewUsageNull;
    const VkDeviceSize dstImageSizeInBytes = getUncompressedImageSizeInBytes(dstFormat, m_parameters.size);

    const std::vector<tcu::Vec4> vertexArray     = createFullscreenQuad();
    const uint32_t vertexCount                   = static_cast<uint32_t>(vertexArray.size());
    const size_t vertexBufferSizeInBytes         = vertexCount * sizeof(vertexArray[0]);
    const MovePtr<BufferWithMemory> vertexBuffer = MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, device, allocator, makeBufferCreateInfo(vertexBufferSizeInBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
        MemoryRequirement::HostVisible));
    const Allocation &vertexBufferAlloc          = vertexBuffer->getAllocation();
    const VkDeviceSize vertexBufferOffset[]      = {0};

    const VkBufferCreateInfo srcImageBufferInfo(
        makeBufferCreateInfo(srcImageSizeInBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
    const MovePtr<BufferWithMemory> srcImageBuffer = MovePtr<BufferWithMemory>(
        new BufferWithMemory(vk, device, allocator, srcImageBufferInfo, MemoryRequirement::HostVisible));

    const VkImageCreateInfo srcImageCreateInfo = makeCreateImageInfo(
        srcFormat, m_parameters.imageType, m_parameters.size, srcImageUsageFlags, srcExtendedImageCreate);
    const MovePtr<Image> srcImage(new Image(vk, device, allocator, srcImageCreateInfo, MemoryRequirement::Any));
    Move<VkImageView> srcImageView(makeImageView(vk, device, srcImage->get(), mapImageViewType(m_parameters.imageType),
                                                 m_parameters.featuredFormat, subresourceRange,
                                                 srcImageViewUsageFlags));

    const VkImageCreateInfo dstImageCreateInfo = makeCreateImageInfo(
        dstFormat, m_parameters.imageType, m_parameters.size, dstImageUsageFlags, dstExtendedImageCreate);
    de::MovePtr<Image> dstImage(new Image(vk, device, allocator, dstImageCreateInfo, MemoryRequirement::Any));
    Move<VkImageView> dstImageView(makeImageView(vk, device, dstImage->get(), mapImageViewType(m_parameters.imageType),
                                                 m_parameters.featuredFormat, subresourceRange,
                                                 dstImageViewUsageFlags));

    const VkBufferCreateInfo dstImageBufferInfo(
        makeBufferCreateInfo(dstImageSizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
    MovePtr<BufferWithMemory> dstImageBuffer = MovePtr<BufferWithMemory>(
        new BufferWithMemory(vk, device, allocator, dstImageBufferInfo, MemoryRequirement::HostVisible));

    const Unique<VkShaderModule> vertShaderModule(
        createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0));
    const Unique<VkShaderModule> fragShaderModule(
        createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0));

    const Unique<VkRenderPass> renderPass(
        vkt::image::makeRenderPass(vk, device, m_parameters.featuredFormat, m_parameters.featuredFormat));

    const Move<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(vk, device));
    const Move<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, SINGLE_LAYER)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, SINGLE_LAYER));
    const Move<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
    const VkDescriptorImageInfo descriptorSrcImageInfo(
        makeDescriptorImageInfo(VK_NULL_HANDLE, *srcImageView, VK_IMAGE_LAYOUT_GENERAL));

    const VkExtent2D renderSize(makeExtent2D(m_parameters.size[0], m_parameters.size[1]));
    const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
    const Unique<VkPipeline> pipeline(makeGraphicsPipeline(vk, device, *pipelineLayout, *renderPass, *vertShaderModule,
                                                           *fragShaderModule, renderSize, 1u));
#ifndef CTS_USES_VULKANSC
    const Unique<VkCommandPool> cmdPool(
        createCommandPool(vk, device, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT, queueFamilyIndex));
#else
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, device, VkCommandPoolCreateFlags(0u), queueFamilyIndex));
#endif // CTS_USES_VULKANSC
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    const VkBufferImageCopy srcCopyRegion = makeBufferImageCopy(m_parameters.size[0], m_parameters.size[1]);
    const VkBufferMemoryBarrier srcCopyBufferBarrierPre = makeBufferMemoryBarrier(
        VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, srcImageBuffer->get(), 0ull, srcImageSizeInBytes);
    const VkImageMemoryBarrier srcCopyImageBarrierPre =
        makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, srcImage->get(), subresourceRange);
    const VkImageMemoryBarrier srcCopyImageBarrierPost = makeImageMemoryBarrier(
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL, srcImage->get(), subresourceRange);
    const VkBufferImageCopy dstCopyRegion = makeBufferImageCopy(m_parameters.size[0], m_parameters.size[1]);

    const VkImageView attachmentBindInfos[] = {*srcImageView, *dstImageView};
    const Move<VkFramebuffer> framebuffer(makeFramebuffer(vk, device, *renderPass,
                                                          DE_LENGTH_OF_ARRAY(attachmentBindInfos), attachmentBindInfos,
                                                          renderSize.width, renderSize.height, SINGLE_LAYER));

    DE_ASSERT(srcImageSizeInBytes == dstImageSizeInBytes);

    // Upload vertex data
    deMemcpy(vertexBufferAlloc.getHostPtr(), &vertexArray[0], vertexBufferSizeInBytes);
    flushAlloc(vk, device, vertexBufferAlloc);

    // Upload source image data
    const Allocation &alloc = srcImageBuffer->getAllocation();
    deMemcpy(alloc.getHostPtr(), &srcData[0], (size_t)srcImageSizeInBytes);
    flushAlloc(vk, device, alloc);

    beginCommandBuffer(vk, *cmdBuffer);
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

    //Copy buffer to image
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0,
                          0, nullptr, 1u, &srcCopyBufferBarrierPre, 1u, &srcCopyImageBarrierPre);
    vk.cmdCopyBufferToImage(*cmdBuffer, srcImageBuffer->get(), srcImage->get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            1u, &srcCopyRegion);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1u, &srcCopyImageBarrierPost);

    beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderSize);

    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &descriptorSrcImageInfo)
        .update(vk, device);

    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &descriptorSet.get(),
                             0u, nullptr);
    vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer->get(), vertexBufferOffset);
    vk.cmdDraw(*cmdBuffer, vertexCount, 1, 0, 0);

    endRenderPass(vk, *cmdBuffer);

    const VkImageMemoryBarrier prepareForTransferBarrier =
        makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                               VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, dstImage->get(), subresourceRange);

    const VkBufferMemoryBarrier copyBarrier = makeBufferMemoryBarrier(
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, dstImageBuffer->get(), 0ull, dstImageSizeInBytes);

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &prepareForTransferBarrier);
    vk.cmdCopyImageToBuffer(*cmdBuffer, dstImage->get(), VK_IMAGE_LAYOUT_GENERAL, dstImageBuffer->get(), 1u,
                            &dstCopyRegion);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0,
                          0, nullptr, 1, &copyBarrier, 0, nullptr);

    endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    const Allocation &dstImageBufferAlloc = dstImageBuffer->getAllocation();
    invalidateAlloc(vk, device, dstImageBufferAlloc);
    dstData.resize((size_t)dstImageSizeInBytes);
    deMemcpy(&dstData[0], dstImageBufferAlloc.getHostPtr(), (size_t)dstImageSizeInBytes);

    outputImage = dstImage;
}

VkImageCreateInfo GraphicsAttachmentsTestInstance::makeCreateImageInfo(const VkFormat format, const ImageType type,
                                                                       const UVec3 &size,
                                                                       const VkImageUsageFlags usageFlags,
                                                                       const bool extendedImageCreateFlag)
{
    const VkImageType imageType                    = mapImageType(type);
    const VkImageCreateFlags imageCreateFlagsBase  = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    const VkImageCreateFlags imageCreateFlagsAddOn = extendedImageCreateFlag ? VK_IMAGE_CREATE_EXTENDED_USAGE_BIT : 0;
    const VkImageCreateFlags imageCreateFlags      = imageCreateFlagsBase | imageCreateFlagsAddOn;

    const VkImageCreateInfo createImageInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,    // VkStructureType sType;
        nullptr,                                // const void* pNext;
        imageCreateFlags,                       // VkImageCreateFlags flags;
        imageType,                              // VkImageType imageType;
        format,                                 // VkFormat format;
        makeExtent3D(getLayerSize(type, size)), // VkExtent3D extent;
        1u,                                     // uint32_t mipLevels;
        1u,                                     // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,                  // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,                // VkImageTiling tiling;
        usageFlags,                             // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,              // VkSharingMode sharingMode;
        0u,                                     // uint32_t queueFamilyIndexCount;
        nullptr,                                // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout initialLayout;
    };

    return createImageInfo;
}

VkImageViewUsageCreateInfo GraphicsAttachmentsTestInstance::makeImageViewUsageCreateInfo(
    VkImageUsageFlags imageUsageFlags)
{
    VkImageViewUsageCreateInfo imageViewUsageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO, //VkStructureType sType;
        nullptr,                                        //const void* pNext;
        imageUsageFlags,                                //VkImageUsageFlags usage;
    };

    return imageViewUsageCreateInfo;
}

VkDeviceSize GraphicsAttachmentsTestInstance::getUncompressedImageData(const VkFormat format, const UVec3 &size,
                                                                       std::vector<uint8_t> &data)
{
    tcu::IVec3 sizeAsIVec3 =
        tcu::IVec3(static_cast<int>(size[0]), static_cast<int>(size[1]), static_cast<int>(size[2]));
    VkDeviceSize sizeBytes = getImageSizeBytes(sizeAsIVec3, format);

    data.resize((size_t)sizeBytes);
    generateData(&data[0], data.size(), format);

    return sizeBytes;
}

bool GraphicsAttachmentsTestInstance::compareAndLog(const void *reference, const void *result, size_t size)
{
    tcu::TestLog &log = m_context.getTestContext().getLog();

    const uint64_t *ref64 = reinterpret_cast<const uint64_t *>(reference);
    const uint64_t *res64 = reinterpret_cast<const uint64_t *>(result);
    const size_t sizew    = size / sizeof(uint64_t);
    bool equal            = true;

    DE_ASSERT(size % sizeof(uint64_t) == 0);

    for (uint32_t ndx = 0u; ndx < static_cast<uint32_t>(sizew); ndx++)
    {
        if (ref64[ndx] != res64[ndx])
        {
            std::stringstream str;

            str << "Difference begins near byte " << ndx * sizeof(uint64_t) << "."
                << " reference value: 0x" << std::hex << std::setw(2ull * sizeof(uint64_t)) << std::setfill('0')
                << ref64[ndx] << " result value: 0x" << std::hex << std::setw(2ull * sizeof(uint64_t))
                << std::setfill('0') << res64[ndx];

            log.writeMessage(str.str().c_str());

            equal = false;

            break;
        }
    }

    return equal;
}

class GraphicsTextureTestInstance : public GraphicsAttachmentsTestInstance
{
public:
    GraphicsTextureTestInstance(Context &context, const TestParameters &parameters);

protected:
    void transcode(std::vector<uint8_t> &srcData, std::vector<uint8_t> &dstData, de::MovePtr<Image> &outputImage);
};

GraphicsTextureTestInstance::GraphicsTextureTestInstance(Context &context, const TestParameters &parameters)
    : GraphicsAttachmentsTestInstance(context, parameters)
{
}

void GraphicsTextureTestInstance::transcode(std::vector<uint8_t> &srcData, std::vector<uint8_t> &dstData,
                                            de::MovePtr<Image> &outputImage)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    const VkQueue queue             = m_context.getUniversalQueue();
    Allocator &allocator            = m_context.getDefaultAllocator();

    const VkImageSubresourceRange subresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, SINGLE_LEVEL, 0u, SINGLE_LAYER);
    const VkImageViewUsageCreateInfo *imageViewUsageNull = nullptr;
    const VkImageViewUsageCreateInfo imageViewUsage      = makeImageViewUsageCreateInfo(m_parameters.testedImageUsage);

    const VkFormat srcFormat = (m_parameters.operation == OPERATION_TEXTURE_READ)  ? m_parameters.featurelessFormat :
                               (m_parameters.operation == OPERATION_TEXTURE_WRITE) ? m_parameters.featuredFormat :
                                                                                     VK_FORMAT_UNDEFINED;
    const bool srcExtendedImageCreate = (m_parameters.operation == OPERATION_TEXTURE_READ)  ? true :
                                        (m_parameters.operation == OPERATION_TEXTURE_WRITE) ? false :
                                                                                              false;
    const VkImageUsageFlags srcImageUsageFlags =
        (m_parameters.operation == OPERATION_TEXTURE_READ)  ? m_parameters.testedImageUsage :
        (m_parameters.operation == OPERATION_TEXTURE_WRITE) ? m_parameters.pairedImageUsage :
                                                              0;
    const VkImageViewUsageCreateInfo *srcImageViewUsage =
        (m_parameters.operation == OPERATION_TEXTURE_READ)  ? &imageViewUsage :
        (m_parameters.operation == OPERATION_TEXTURE_WRITE) ? imageViewUsageNull :
                                                              imageViewUsageNull;
    const VkDeviceSize srcImageSizeInBytes = getUncompressedImageData(srcFormat, m_parameters.size, srcData);

    const VkFormat dstFormat = (m_parameters.operation == OPERATION_TEXTURE_READ)  ? m_parameters.featuredFormat :
                               (m_parameters.operation == OPERATION_TEXTURE_WRITE) ? m_parameters.featurelessFormat :
                                                                                     VK_FORMAT_UNDEFINED;
    const bool dstExtendedImageCreate = (m_parameters.operation == OPERATION_TEXTURE_READ)  ? false :
                                        (m_parameters.operation == OPERATION_TEXTURE_WRITE) ? true :
                                                                                              false;
    const VkImageUsageFlags dstImageUsageFlags =
        (m_parameters.operation == OPERATION_TEXTURE_READ)  ? m_parameters.pairedImageUsage :
        (m_parameters.operation == OPERATION_TEXTURE_WRITE) ? m_parameters.testedImageUsage :
                                                              0;
    const VkImageViewUsageCreateInfo *dstImageViewUsage =
        (m_parameters.operation == OPERATION_TEXTURE_READ)  ? imageViewUsageNull :
        (m_parameters.operation == OPERATION_TEXTURE_WRITE) ? &imageViewUsage :
                                                              imageViewUsageNull;
    const VkDeviceSize dstImageSizeInBytes = getUncompressedImageSizeInBytes(dstFormat, m_parameters.size);

    const std::vector<tcu::Vec4> vertexArray     = createFullscreenQuad();
    const uint32_t vertexCount                   = static_cast<uint32_t>(vertexArray.size());
    const size_t vertexBufferSizeInBytes         = vertexCount * sizeof(vertexArray[0]);
    const MovePtr<BufferWithMemory> vertexBuffer = MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, device, allocator, makeBufferCreateInfo(vertexBufferSizeInBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
        MemoryRequirement::HostVisible));
    const Allocation &vertexBufferAlloc          = vertexBuffer->getAllocation();
    const VkDeviceSize vertexBufferOffset[]      = {0};

    const VkBufferCreateInfo srcImageBufferInfo(
        makeBufferCreateInfo(srcImageSizeInBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
    const MovePtr<BufferWithMemory> srcImageBuffer = MovePtr<BufferWithMemory>(
        new BufferWithMemory(vk, device, allocator, srcImageBufferInfo, MemoryRequirement::HostVisible));

    const VkImageCreateInfo srcImageCreateInfo = makeCreateImageInfo(
        srcFormat, m_parameters.imageType, m_parameters.size, srcImageUsageFlags, srcExtendedImageCreate);
    const MovePtr<Image> srcImage(new Image(vk, device, allocator, srcImageCreateInfo, MemoryRequirement::Any));
    Move<VkImageView> srcImageView(makeImageView(vk, device, srcImage->get(), mapImageViewType(m_parameters.imageType),
                                                 m_parameters.featuredFormat, subresourceRange, srcImageViewUsage));

    const VkImageCreateInfo dstImageCreateInfo = makeCreateImageInfo(
        dstFormat, m_parameters.imageType, m_parameters.size, dstImageUsageFlags, dstExtendedImageCreate);
    de::MovePtr<Image> dstImage(new Image(vk, device, allocator, dstImageCreateInfo, MemoryRequirement::Any));
    Move<VkImageView> dstImageView(makeImageView(vk, device, dstImage->get(), mapImageViewType(m_parameters.imageType),
                                                 m_parameters.featuredFormat, subresourceRange, dstImageViewUsage));
    const VkImageMemoryBarrier dstCopyImageBarrier =
        makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                               dstImage->get(), subresourceRange);

    const VkBufferCreateInfo dstImageBufferInfo(
        makeBufferCreateInfo(dstImageSizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
    MovePtr<BufferWithMemory> dstImageBuffer = MovePtr<BufferWithMemory>(
        new BufferWithMemory(vk, device, allocator, dstImageBufferInfo, MemoryRequirement::HostVisible));

    const Unique<VkShaderModule> vertShaderModule(
        createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0));
    const Unique<VkShaderModule> fragShaderModule(
        createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0));

    const Unique<VkRenderPass> renderPass(makeRenderPass(vk, device));

    const Move<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(vk, device));
    const Move<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
    const Move<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
    const VkSamplerCreateInfo srcSamplerInfo(makeSamplerCreateInfo());
    const Move<VkSampler> srcSampler = vk::createSampler(vk, device, &srcSamplerInfo);
    const VkDescriptorImageInfo descriptorSrcImage(
        makeDescriptorImageInfo(*srcSampler, *srcImageView, VK_IMAGE_LAYOUT_GENERAL));
    const VkDescriptorImageInfo descriptorDstImage(
        makeDescriptorImageInfo(VK_NULL_HANDLE, *dstImageView, VK_IMAGE_LAYOUT_GENERAL));

    const VkExtent2D renderSize(makeExtent2D(m_parameters.size[0], m_parameters.size[1]));
    const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
    const Unique<VkPipeline> pipeline(makeGraphicsPipeline(vk, device, *pipelineLayout, *renderPass, *vertShaderModule,
                                                           *fragShaderModule, renderSize, 0u));
#ifndef CTS_USES_VULKANSC
    const Unique<VkCommandPool> cmdPool(
        createCommandPool(vk, device, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT, queueFamilyIndex));
#else
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, device, VkCommandPoolCreateFlags(0u), queueFamilyIndex));
#endif // CTS_USES_VULKANSC

    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    const VkBufferImageCopy srcCopyRegion            = makeBufferImageCopy(m_parameters.size[0], m_parameters.size[1]);
    const VkBufferMemoryBarrier srcCopyBufferBarrier = makeBufferMemoryBarrier(
        VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, srcImageBuffer->get(), 0ull, srcImageSizeInBytes);
    const VkImageMemoryBarrier srcCopyImageBarrier =
        makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                               srcImage->get(), subresourceRange);
    const VkImageMemoryBarrier srcCopyImageBarrierPost =
        makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                               VK_IMAGE_LAYOUT_GENERAL, srcImage->get(), subresourceRange);

    const VkBufferImageCopy dstCopyRegion = makeBufferImageCopy(m_parameters.size[0], m_parameters.size[1]);

    const VkExtent2D framebufferSize(makeExtent2D(m_parameters.size[0], m_parameters.size[1]));
    const Move<VkFramebuffer> framebuffer(makeFramebuffer(vk, device, *renderPass, 0, nullptr, framebufferSize.width,
                                                          framebufferSize.height, SINGLE_LAYER));

    DE_ASSERT(srcImageSizeInBytes == dstImageSizeInBytes);

    // Upload vertex data
    deMemcpy(vertexBufferAlloc.getHostPtr(), &vertexArray[0], vertexBufferSizeInBytes);
    flushAlloc(vk, device, vertexBufferAlloc);

    // Upload source image data
    const Allocation &alloc = srcImageBuffer->getAllocation();
    deMemcpy(alloc.getHostPtr(), &srcData[0], (size_t)srcImageSizeInBytes);
    flushAlloc(vk, device, alloc);

    beginCommandBuffer(vk, *cmdBuffer);
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

    //Copy buffer to image
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0,
                          0, nullptr, 1u, &srcCopyBufferBarrier, 1u, &srcCopyImageBarrier);
    vk.cmdCopyBufferToImage(*cmdBuffer, srcImageBuffer->get(), srcImage->get(), VK_IMAGE_LAYOUT_GENERAL, 1u,
                            &srcCopyRegion);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1u, &srcCopyImageBarrierPost);

    // Make source image readable
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 0u, nullptr, 1u, &dstCopyImageBarrier);

    beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderSize);
    {
        DescriptorSetUpdateBuilder()
            .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descriptorSrcImage)
            .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                         VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorDstImage)
            .update(vk, device);

        vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
                                 &descriptorSet.get(), 0u, nullptr);
        vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer->get(), vertexBufferOffset);
        vk.cmdDraw(*cmdBuffer, vertexCount, 1, 0, 0);
    }
    endRenderPass(vk, *cmdBuffer);

    const VkImageMemoryBarrier prepareForTransferBarrier =
        makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                               VK_IMAGE_LAYOUT_GENERAL, dstImage->get(), subresourceRange);

    const VkBufferMemoryBarrier copyBarrier = makeBufferMemoryBarrier(
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, dstImageBuffer->get(), 0ull, dstImageSizeInBytes);

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &prepareForTransferBarrier);
    vk.cmdCopyImageToBuffer(*cmdBuffer, dstImage->get(), VK_IMAGE_LAYOUT_GENERAL, dstImageBuffer->get(), 1u,
                            &dstCopyRegion);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0,
                          0, nullptr, 1, &copyBarrier, 0, nullptr);

    endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    const Allocation &dstImageBufferAlloc = dstImageBuffer->getAllocation();
    invalidateAlloc(vk, device, dstImageBufferAlloc);
    dstData.resize((size_t)dstImageSizeInBytes);
    deMemcpy(&dstData[0], dstImageBufferAlloc.getHostPtr(), (size_t)dstImageSizeInBytes);

    outputImage = dstImage;
}

class ImageTranscodingCase : public TestCase
{
public:
    ImageTranscodingCase(TestContext &testCtx, const std::string &name, const TestParameters &parameters);
    void initPrograms(SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;
    bool isFormatUsageFlagSupported(Context &context, const VkFormat format, VkImageUsageFlags formatUsageFlags) const;

protected:
    const TestParameters m_parameters;
};

ImageTranscodingCase::ImageTranscodingCase(TestContext &testCtx, const std::string &name,
                                           const TestParameters &parameters)
    : TestCase(testCtx, name)
    , m_parameters(parameters)
{
}

void ImageTranscodingCase::initPrograms(vk::SourceCollections &programCollection) const
{
    DE_ASSERT(m_parameters.size.x() > 0);
    DE_ASSERT(m_parameters.size.y() > 0);

    ImageType imageTypeForFS = (m_parameters.imageType == IMAGE_TYPE_2D_ARRAY) ? IMAGE_TYPE_2D : m_parameters.imageType;

    // Vertex shader
    {
        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n\n"
            << "layout(location = 0) in vec4 v_in_position;\n"
            << "\n"
            << "void main (void)\n"
            << "{\n"
            << "    gl_Position = v_in_position;\n"
            << "}\n";

        programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
    }

    // Fragment shader
    {
        switch (m_parameters.operation)
        {
        case OPERATION_ATTACHMENT_READ:
        case OPERATION_ATTACHMENT_WRITE:
        {
            std::ostringstream src;

            const std::string dstTypeStr = getGlslAttachmentType(m_parameters.featuredFormat);
            const std::string srcTypeStr = getGlslInputAttachmentType(m_parameters.featuredFormat);

            src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n\n"
                << "precision highp int;\n"
                << "precision highp float;\n"
                << "\n"
                << "layout (location = 0) out highp " << dstTypeStr << " o_color;\n"
                << "layout (input_attachment_index = 0, set = 0, binding = 0) uniform highp " << srcTypeStr
                << " inputImage1;\n"
                << "\n"
                << "void main (void)\n"
                << "{\n"
                << "    o_color = " << dstTypeStr << "(subpassLoad(inputImage1));\n"
                << "}\n";

            programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());

            break;
        }

        case OPERATION_TEXTURE_READ:
        case OPERATION_TEXTURE_WRITE:
        {
            std::ostringstream src;

            const std::string srcSamplerTypeStr =
                getGlslSamplerType(mapVkFormat(m_parameters.featuredFormat), mapImageViewType(imageTypeForFS));
            const std::string dstImageTypeStr =
                getShaderImageType(mapVkFormat(m_parameters.featuredFormat), imageTypeForFS);
            const std::string dstFormatQualifierStr =
                getShaderImageFormatQualifier(mapVkFormat(m_parameters.featuredFormat));

            src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n\n"
                << "layout (binding = 0) uniform " << srcSamplerTypeStr << " u_imageIn;\n"
                << "layout (binding = 1, " << dstFormatQualifierStr << ") writeonly uniform " << dstImageTypeStr
                << " u_imageOut;\n"
                << "\n"
                << "void main (void)\n"
                << "{\n"
                << "    const ivec2 out_pos = ivec2(gl_FragCoord.xy);\n"
                << "    const vec2 pixels_resolution = vec2(textureSize(u_imageIn, 0));\n"
                << "    const vec2 in_pos = vec2(gl_FragCoord.xy) / vec2(pixels_resolution);\n"
                << "    imageStore(u_imageOut, out_pos, texture(u_imageIn, in_pos));\n"
                << "}\n";

            programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());

            break;
        }

        default:
            DE_ASSERT(false);
        }
    }
}

bool ImageTranscodingCase::isFormatUsageFlagSupported(Context &context, const VkFormat format,
                                                      VkImageUsageFlags formatUsageFlags) const
{
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const InstanceInterface &vk           = context.getInstanceInterface();
    VkImageFormatProperties imageFormatProperties;
    const VkResult queryResult = vk.getPhysicalDeviceImageFormatProperties(
        physicalDevice, format, mapImageType(m_parameters.imageType), VK_IMAGE_TILING_OPTIMAL, formatUsageFlags,
        VK_IMAGE_CREATE_EXTENDED_USAGE_BIT, &imageFormatProperties);

    return (queryResult == VK_SUCCESS);
}

void ImageTranscodingCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_maintenance2");

    if ((m_parameters.operation == OPERATION_TEXTURE_READ || m_parameters.operation == OPERATION_TEXTURE_WRITE) &&
        !context.getDeviceFeatures().fragmentStoresAndAtomics)
        TCU_THROW(NotSupportedError, "fragmentStoresAndAtomics not supported");

    if (!isFormatUsageFlagSupported(context, m_parameters.featuredFormat, m_parameters.testedImageUsageFeature))
        TCU_THROW(NotSupportedError, "Test skipped due to feature is not supported by the format");

    if (!isFormatUsageFlagSupported(context, m_parameters.featuredFormat,
                                    m_parameters.testedImageUsage | m_parameters.pairedImageUsage))
        TCU_THROW(NotSupportedError, "Required image usage flags are not supported by the format");
}

TestInstance *ImageTranscodingCase::createInstance(Context &context) const
{
    VkFormat featurelessFormat = VK_FORMAT_UNDEFINED;
    bool differenceFound       = false;

    DE_ASSERT(m_parameters.testedImageUsageFeature != 0);

    for (uint32_t i = 0; m_parameters.compatibleFormats[i] != VK_FORMAT_UNDEFINED; i++)
    {
        featurelessFormat = m_parameters.compatibleFormats[i];

        if (isSupportedByFramework(featurelessFormat) &&
            !isFormatUsageFlagSupported(context, featurelessFormat, m_parameters.testedImageUsageFeature) &&
            isFormatUsageFlagSupported(context, featurelessFormat,
                                       m_parameters.testedImageUsage & (~m_parameters.testedImageUsageFeature)))
        {
            differenceFound = true;

            break;
        }
    }

    if (differenceFound)
    {
#ifndef CTS_USES_VULKANSC
        if ((context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
             !context.getPortabilitySubsetFeatures().imageViewFormatReinterpretation))
        {
            tcu::TextureFormat textureImageFormat = vk::mapVkFormat(m_parameters.featuredFormat);
            tcu::TextureFormat textureViewFormat  = vk::mapVkFormat(featurelessFormat);

            if (tcu::getTextureFormatBitDepth(textureImageFormat) != tcu::getTextureFormatBitDepth(textureViewFormat))
                TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Format must not contain a different number of "
                                             "bits in each component, than the format of the VkImage");
        }
#endif // CTS_USES_VULKANSC

        TestParameters calculatedParameters = {
            m_parameters.operation,               // Operation                operation
            m_parameters.size,                    // UVec3                    size
            m_parameters.imageType,               // ImageType                imageType
            m_parameters.testedImageUsageFeature, // VkImageUsageFlagBits        testedImageUsageFeature
            m_parameters.featuredFormat,          // VkFormat                    featuredFormat
            featurelessFormat,                    // VkFormat                    featurelessFormat
            m_parameters.testedImageUsage,        // VkImageUsageFlags        testedImageUsage
            m_parameters.pairedImageUsage,        // VkImageUsageFlags        pairedImageUsage
            nullptr,                              // const VkFormat*            compatibleFormats
        };

        switch (m_parameters.operation)
        {
        case OPERATION_ATTACHMENT_READ:
        case OPERATION_ATTACHMENT_WRITE:
            return new GraphicsAttachmentsTestInstance(context, calculatedParameters);

        case OPERATION_TEXTURE_READ:
        case OPERATION_TEXTURE_WRITE:
            return new GraphicsTextureTestInstance(context, calculatedParameters);

        default:
            TCU_THROW(InternalError, "Impossible");
        }
    }
    else
        TCU_THROW(NotSupportedError, "All formats in group contain tested feature. Test is impossible.");
}

} // namespace

static const VkFormat compatibleFormatList8Bit[] = {
    VK_FORMAT_R4G4_UNORM_PACK8, VK_FORMAT_R8_UNORM, VK_FORMAT_R8_SNORM, VK_FORMAT_R8_USCALED,
    VK_FORMAT_R8_SSCALED,       VK_FORMAT_R8_UINT,  VK_FORMAT_R8_SINT,  VK_FORMAT_R8_SRGB,

    VK_FORMAT_UNDEFINED};

static const VkFormat compatibleFormatList16Bit[] = {VK_FORMAT_R4G4B4A4_UNORM_PACK16,
                                                     VK_FORMAT_B4G4R4A4_UNORM_PACK16,
                                                     VK_FORMAT_R5G6B5_UNORM_PACK16,
                                                     VK_FORMAT_B5G6R5_UNORM_PACK16,
                                                     VK_FORMAT_R5G5B5A1_UNORM_PACK16,
                                                     VK_FORMAT_B5G5R5A1_UNORM_PACK16,
                                                     VK_FORMAT_A1R5G5B5_UNORM_PACK16,
                                                     VK_FORMAT_R8G8_UNORM,
                                                     VK_FORMAT_R8G8_SNORM,
                                                     VK_FORMAT_R8G8_USCALED,
                                                     VK_FORMAT_R8G8_SSCALED,
                                                     VK_FORMAT_R8G8_UINT,
                                                     VK_FORMAT_R8G8_SINT,
                                                     VK_FORMAT_R8G8_SRGB,
                                                     VK_FORMAT_R16_UNORM,
                                                     VK_FORMAT_R16_SNORM,
                                                     VK_FORMAT_R16_USCALED,
                                                     VK_FORMAT_R16_SSCALED,
                                                     VK_FORMAT_R16_UINT,
                                                     VK_FORMAT_R16_SINT,
                                                     VK_FORMAT_R16_SFLOAT,
                                                     VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT,
                                                     VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT,

                                                     VK_FORMAT_UNDEFINED};

static const VkFormat compatibleFormatList24Bit[] = {
    VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8_SNORM,   VK_FORMAT_R8G8B8_USCALED, VK_FORMAT_R8G8B8_SSCALED,
    VK_FORMAT_R8G8B8_UINT,  VK_FORMAT_R8G8B8_SINT,    VK_FORMAT_R8G8B8_SRGB,    VK_FORMAT_B8G8R8_UNORM,
    VK_FORMAT_B8G8R8_SNORM, VK_FORMAT_B8G8R8_USCALED, VK_FORMAT_B8G8R8_SSCALED, VK_FORMAT_B8G8R8_UINT,
    VK_FORMAT_B8G8R8_SINT,  VK_FORMAT_B8G8R8_SRGB,

    VK_FORMAT_UNDEFINED};

static const VkFormat compatibleFormatList32Bit[] = {VK_FORMAT_R8G8B8A8_UNORM,
                                                     VK_FORMAT_R8G8B8A8_SNORM,
                                                     VK_FORMAT_R8G8B8A8_USCALED,
                                                     VK_FORMAT_R8G8B8A8_SSCALED,
                                                     VK_FORMAT_R8G8B8A8_UINT,
                                                     VK_FORMAT_R8G8B8A8_SINT,
                                                     VK_FORMAT_R8G8B8A8_SRGB,
                                                     VK_FORMAT_B8G8R8A8_UNORM,
                                                     VK_FORMAT_B8G8R8A8_SNORM,
                                                     VK_FORMAT_B8G8R8A8_USCALED,
                                                     VK_FORMAT_B8G8R8A8_SSCALED,
                                                     VK_FORMAT_B8G8R8A8_UINT,
                                                     VK_FORMAT_B8G8R8A8_SINT,
                                                     VK_FORMAT_B8G8R8A8_SRGB,
                                                     VK_FORMAT_A8B8G8R8_UNORM_PACK32,
                                                     VK_FORMAT_A8B8G8R8_SNORM_PACK32,
                                                     VK_FORMAT_A8B8G8R8_USCALED_PACK32,
                                                     VK_FORMAT_A8B8G8R8_SSCALED_PACK32,
                                                     VK_FORMAT_A8B8G8R8_UINT_PACK32,
                                                     VK_FORMAT_A8B8G8R8_SINT_PACK32,
                                                     VK_FORMAT_A8B8G8R8_SRGB_PACK32,
                                                     VK_FORMAT_A2R10G10B10_UNORM_PACK32,
                                                     VK_FORMAT_A2R10G10B10_SNORM_PACK32,
                                                     VK_FORMAT_A2R10G10B10_USCALED_PACK32,
                                                     VK_FORMAT_A2R10G10B10_SSCALED_PACK32,
                                                     VK_FORMAT_A2R10G10B10_UINT_PACK32,
                                                     VK_FORMAT_A2R10G10B10_SINT_PACK32,
                                                     VK_FORMAT_A2B10G10R10_UNORM_PACK32,
                                                     VK_FORMAT_A2B10G10R10_SNORM_PACK32,
                                                     VK_FORMAT_A2B10G10R10_USCALED_PACK32,
                                                     VK_FORMAT_A2B10G10R10_SSCALED_PACK32,
                                                     VK_FORMAT_A2B10G10R10_UINT_PACK32,
                                                     VK_FORMAT_A2B10G10R10_SINT_PACK32,
                                                     VK_FORMAT_R16G16_UNORM,
                                                     VK_FORMAT_R16G16_SNORM,
                                                     VK_FORMAT_R16G16_USCALED,
                                                     VK_FORMAT_R16G16_SSCALED,
                                                     VK_FORMAT_R16G16_UINT,
                                                     VK_FORMAT_R16G16_SINT,
                                                     VK_FORMAT_R16G16_SFLOAT,
                                                     VK_FORMAT_R32_UINT,
                                                     VK_FORMAT_R32_SINT,
                                                     VK_FORMAT_R32_SFLOAT,

                                                     VK_FORMAT_UNDEFINED};

static const VkFormat compatibleFormatList48Bit[] = {
    VK_FORMAT_R16G16B16_UNORM, VK_FORMAT_R16G16B16_SNORM, VK_FORMAT_R16G16B16_USCALED, VK_FORMAT_R16G16B16_SSCALED,
    VK_FORMAT_R16G16B16_UINT,  VK_FORMAT_R16G16B16_SINT,  VK_FORMAT_R16G16B16_SFLOAT,

    VK_FORMAT_UNDEFINED};

static const VkFormat compatibleFormatList64Bit[] = {VK_FORMAT_R16G16B16A16_UNORM,
                                                     VK_FORMAT_R16G16B16A16_SNORM,
                                                     VK_FORMAT_R16G16B16A16_USCALED,
                                                     VK_FORMAT_R16G16B16A16_SSCALED,
                                                     VK_FORMAT_R16G16B16A16_UINT,
                                                     VK_FORMAT_R16G16B16A16_SINT,
                                                     VK_FORMAT_R16G16B16A16_SFLOAT,
                                                     VK_FORMAT_R32G32_UINT,
                                                     VK_FORMAT_R32G32_SINT,
                                                     VK_FORMAT_R32G32_SFLOAT,
                                                     VK_FORMAT_R64_UINT,
                                                     VK_FORMAT_R64_SINT,
                                                     VK_FORMAT_R64_SFLOAT,

                                                     VK_FORMAT_UNDEFINED};

static const VkFormat compatibleFormatList96Bit[] = {VK_FORMAT_R32G32B32_UINT, VK_FORMAT_R32G32B32_SINT,
                                                     VK_FORMAT_R32G32B32_SFLOAT,

                                                     VK_FORMAT_UNDEFINED};

static const VkFormat compatibleFormatList128Bit[] = {
    VK_FORMAT_R32G32B32A32_UINT, VK_FORMAT_R32G32B32A32_SINT, VK_FORMAT_R32G32B32A32_SFLOAT,
    VK_FORMAT_R64G64_UINT,       VK_FORMAT_R64G64_SINT,       VK_FORMAT_R64G64_SFLOAT,

    VK_FORMAT_UNDEFINED};

const VkFormat compatibleFormatList192Bit[] = {VK_FORMAT_R64G64B64_UINT, VK_FORMAT_R64G64B64_SINT,
                                               VK_FORMAT_R64G64B64_SFLOAT,

                                               VK_FORMAT_UNDEFINED};

static const VkFormat compatibleFormatList256Bit[] = {VK_FORMAT_R64G64B64A64_UINT, VK_FORMAT_R64G64B64A64_SINT,
                                                      VK_FORMAT_R64G64B64A64_SFLOAT,

                                                      VK_FORMAT_UNDEFINED};

static const VkFormat *compatibleFormatsList[] = {
    compatibleFormatList8Bit,   compatibleFormatList16Bit,  compatibleFormatList24Bit, compatibleFormatList32Bit,
    compatibleFormatList48Bit,  compatibleFormatList64Bit,  compatibleFormatList96Bit, compatibleFormatList128Bit,
    compatibleFormatList192Bit, compatibleFormatList256Bit,
};

tcu::TestCaseGroup *createImageTranscodingSupportTests(tcu::TestContext &testCtx)
{
    const std::string operationName[OPERATION_LAST] = {
        "attachment_read",
        "attachment_write",
        "texture_read",
        "texture_write",
    };
    const VkImageUsageFlagBits testedImageUsageFlags[OPERATION_LAST] = {
        VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_USAGE_STORAGE_BIT,
    };
    const VkImageUsageFlagBits pairedImageUsageFlags[OPERATION_LAST] = {
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        VK_IMAGE_USAGE_STORAGE_BIT,
        VK_IMAGE_USAGE_SAMPLED_BIT,
    };
    VkImageUsageFlags baseFlagsAddOn = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    MovePtr<tcu::TestCaseGroup> imageTranscodingTests(new tcu::TestCaseGroup(testCtx, "extended_usage_bit"));

    for (int operationNdx = OPERATION_ATTACHMENT_READ; operationNdx < OPERATION_LAST; ++operationNdx)
    {
        MovePtr<tcu::TestCaseGroup> imageOperationGroup(
            new tcu::TestCaseGroup(testCtx, operationName[operationNdx].c_str()));

        for (uint32_t groupNdx = 0; groupNdx < DE_LENGTH_OF_ARRAY(compatibleFormatsList); groupNdx++)
        {
            for (uint32_t featuredFormatNdx = 0;
                 compatibleFormatsList[groupNdx][featuredFormatNdx] != VK_FORMAT_UNDEFINED; featuredFormatNdx++)
            {
                const VkFormat featuredFormat    = compatibleFormatsList[groupNdx][featuredFormatNdx];
                const VkFormat featurelessFormat = VK_FORMAT_UNDEFINED; // Lookup process is in createInstance()

                if (!isSupportedByFramework(featuredFormat))
                    continue;

                // Cannot handle SRGB in shader layout classifier
                if (isSrgbFormat(featuredFormat))
                    continue;

                // Cannot handle packed in shader layout classifier
                if (isPackedType(featuredFormat))
                    continue;

                // Cannot handle swizzled component format (i.e. bgr) in shader layout classifier
                if (isComponentSwizzled(featuredFormat))
                    continue;

                // Cannot handle three-component images in shader layout classifier
                if (getNumUsedChannels(featuredFormat) == 3)
                    continue;

                const std::string testName      = getFormatShortString(featuredFormat);
                const TestParameters parameters = {
                    static_cast<Operation>(operationNdx), // Operation                operation
                    UVec3(16u, 16u, 1u),                  // UVec3                    size
                    IMAGE_TYPE_2D,                        // ImageType                imageType
                    testedImageUsageFlags[operationNdx],  // VkImageUsageFlagBits        testedImageUsageFeature
                    featuredFormat,                       // VkFormat                    featuredFormat
                    featurelessFormat,                    // VkFormat                    featurelessFormat
                    baseFlagsAddOn | testedImageUsageFlags[operationNdx], // VkImageUsageFlags        testedImageUsage
                    baseFlagsAddOn | pairedImageUsageFlags[operationNdx], // VkImageUsageFlags        pairedImageUsage
                    compatibleFormatsList[groupNdx] // const VkFormat*            compatibleFormats
                };

                imageOperationGroup->addChild(new ImageTranscodingCase(testCtx, testName, parameters));
            }
        }

        imageTranscodingTests->addChild(imageOperationGroup.release());
    }

    return imageTranscodingTests.release();
}

} // namespace image
} // namespace vkt
