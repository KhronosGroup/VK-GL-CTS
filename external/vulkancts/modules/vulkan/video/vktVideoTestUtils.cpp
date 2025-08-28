/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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
 * \brief Video Encoding and Decoding Utility Functions
 *//*--------------------------------------------------------------------*/

#include "vktVideoTestUtils.hpp"

#ifdef DE_BUILD_VIDEO
#include "vktVideoBaseDecodeUtils.hpp"
#endif

#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkImageUtil.hpp"
#include "tcuCommandLine.hpp"
#include "tcuResource.hpp"

#include "vktCustomInstancesDevices.hpp"
#include "vktTestCase.hpp"

#include "vktVideoDecodeTests.hpp"

#include "vkMd5Sum.hpp"
#include "deFilePath.hpp"

#ifdef DE_BUILD_VIDEO
#include "video_generator.h"
#endif

#ifndef STREAM_DUMP_DEBUG
#define STREAM_DUMP_DEBUG 0
#endif

using namespace vk;
using namespace std;

namespace vkt
{
namespace video
{

using namespace vk;
using namespace std;

bool videoLoggingEnabled()
{
    static int debuggingEnabled = -1; // -1 means it hasn't been checked yet
    if (debuggingEnabled == -1)
    {
        const char *s    = getenv("CTS_DEBUG_VIDEO");
        debuggingEnabled = s != nullptr;
    }

    return debuggingEnabled > 0;
}

void cmdPipelineImageMemoryBarrier2(const DeviceInterface &vk, const VkCommandBuffer commandBuffer,
                                    const VkImageMemoryBarrier2KHR *pImageMemoryBarriers,
                                    const size_t imageMemoryBarrierCount, const VkDependencyFlags dependencyFlags)
{
    const uint32_t imageMemoryBarrierCount32 = static_cast<uint32_t>(imageMemoryBarrierCount);
    const VkDependencyInfo dependencyInfoKHR = {
        vk::VK_STRUCTURE_TYPE_DEPENDENCY_INFO, //  VkStructureType sType;
        nullptr,                               //  const void* pNext;
        dependencyFlags,                       //  VkDependencyFlags dependencyFlags;
        0u,                                    //  uint32_t memoryBarrierCount;
        nullptr,                               //  const VkMemoryBarrier2KHR* pMemoryBarriers;
        0u,                                    //  uint32_t bufferMemoryBarrierCount;
        nullptr,                               //  const VkBufferMemoryBarrier2KHR* pBufferMemoryBarriers;
        imageMemoryBarrierCount32,             //  uint32_t imageMemoryBarrierCount;
        pImageMemoryBarriers,                  //  const VkImageMemoryBarrier2KHR* pImageMemoryBarriers;
    };

    DE_ASSERT(imageMemoryBarrierCount == imageMemoryBarrierCount32);

    vk.cmdPipelineBarrier2(commandBuffer, &dependencyInfoKHR);
}

static VkExtensionProperties makeExtensionProperties(const char *extensionName, uint32_t specVersion)
{
    const uint32_t extensionNameLen = static_cast<uint32_t>(strlen(extensionName));
    VkExtensionProperties result;

    deMemset(&result, 0, sizeof(result));

    deMemcpy(&result.extensionName, extensionName, extensionNameLen);

    result.specVersion = specVersion;

    return result;
}

static const VkExtensionProperties EXTENSION_PROPERTIES_H264_DECODE = makeExtensionProperties(
    VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_SPEC_VERSION);
static const VkExtensionProperties EXTENSION_PROPERTIES_H264_ENCODE = makeExtensionProperties(
    VK_STD_VULKAN_VIDEO_CODEC_H264_ENCODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H264_ENCODE_SPEC_VERSION);
static const VkExtensionProperties EXTENSION_PROPERTIES_AV1_ENCODE = makeExtensionProperties(
    VK_STD_VULKAN_VIDEO_CODEC_AV1_ENCODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_AV1_ENCODE_SPEC_VERSION);
static const VkExtensionProperties EXTENSION_PROPERTIES_H265_DECODE = makeExtensionProperties(
    VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_SPEC_VERSION);
static const VkExtensionProperties EXTENSION_PROPERTIES_H265_ENCODE = makeExtensionProperties(
    VK_STD_VULKAN_VIDEO_CODEC_H265_ENCODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H265_ENCODE_SPEC_VERSION);
static const VkExtensionProperties EXTENSION_PROPERTIES_AV1_DECODE = makeExtensionProperties(
    VK_STD_VULKAN_VIDEO_CODEC_AV1_DECODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_AV1_DECODE_SPEC_VERSION);
static const VkExtensionProperties EXTENSION_PROPERTIES_VP9_DECODE = makeExtensionProperties(
    VK_STD_VULKAN_VIDEO_CODEC_VP9_DECODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_VP9_DECODE_SPEC_VERSION);

VkDeviceSize getBufferSize(VkFormat format, uint32_t width, uint32_t height)
{
    VkDeviceSize result = 0;

    if (vk::isYCbCrFormat(format))
    {
        const PlanarFormatDescription formatDescription = getPlanarFormatDescription(format);
        const tcu::UVec2 baseExtend(width, height);

        for (uint32_t plane = 0; plane < formatDescription.numPlanes; ++plane)
            result += getPlaneSizeInBytes(formatDescription, baseExtend, plane, 0u, 1u);
    }
    else
    {
        result = static_cast<VkDeviceSize>(mapVkFormat(format).getPixelSize()) * width * height;
    }

    return result;
}

void transferImageOwnership(const DeviceInterface &vkd, VkDevice device, VkImage image,
                            uint32_t transferQueueFamilyIndex, uint32_t encodeQueueFamilyIndex, VkImageLayout newLayout)
{
    const VkImageSubresourceRange imageSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);
    const VkImageMemoryBarrier2KHR imageBarrierOwnershipTransfer = makeImageMemoryBarrier2(
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR, VK_ACCESS_NONE_KHR, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,
        VK_ACCESS_NONE_KHR, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, image, imageSubresourceRange,
        transferQueueFamilyIndex, encodeQueueFamilyIndex);
    const VkImageMemoryBarrier2KHR imageBarrierOwnershipEncode = makeImageMemoryBarrier2(
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR, VK_ACCESS_NONE_KHR, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,
        VK_ACCESS_NONE_KHR, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, image, imageSubresourceRange,
        transferQueueFamilyIndex, encodeQueueFamilyIndex);
    const VkImageMemoryBarrier2KHR imageBarrierChangeDstLayout = makeImageMemoryBarrier2(
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR, VK_ACCESS_NONE_KHR, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,
        VK_ACCESS_NONE_KHR, VK_IMAGE_LAYOUT_GENERAL, newLayout, image, imageSubresourceRange, encodeQueueFamilyIndex,
        encodeQueueFamilyIndex);
    const Move<VkCommandPool> cmdEncodePool(makeCommandPool(vkd, device, encodeQueueFamilyIndex));
    const Move<VkCommandBuffer> cmdEncodeBuffer(
        allocateCommandBuffer(vkd, device, *cmdEncodePool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const Move<VkCommandPool> cmdTransferPool(makeCommandPool(vkd, device, transferQueueFamilyIndex));
    const Move<VkCommandBuffer> cmdTransferBuffer(
        allocateCommandBuffer(vkd, device, *cmdTransferPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    Move<VkSemaphore> semaphore                 = createSemaphore(vkd, device);
    Move<VkFence> encodeFence                   = createFence(vkd, device);
    Move<VkFence> transferFence                 = createFence(vkd, device);
    VkFence fences[]                            = {*encodeFence, *transferFence};
    const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    const VkSubmitInfo transferSubmitInfo{
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType sType;
        nullptr,                       // const void* pNext;
        0u,                            // uint32_t waitSemaphoreCount;
        nullptr,                       // const VkSemaphore* pWaitSemaphores;
        nullptr,                       // const VkPipelineStageFlags* pWaitDstStageMask;
        1u,                            // uint32_t commandBufferCount;
        &*cmdTransferBuffer,           // const VkCommandBuffer* pCommandBuffers;
        1u,                            // uint32_t signalSemaphoreCount;
        &*semaphore,                   // const VkSemaphore* pSignalSemaphores;
    };
    const VkSubmitInfo encodeSubmitInfo{
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType sType;
        nullptr,                       // const void* pNext;
        1u,                            // uint32_t waitSemaphoreCount;
        &*semaphore,                   // const VkSemaphore* pWaitSemaphores;
        &waitDstStageMask,             // const VkPipelineStageFlags* pWaitDstStageMask;
        1u,                            // uint32_t commandBufferCount;
        &*cmdEncodeBuffer,             // const VkCommandBuffer* pCommandBuffers;
        0u,                            // uint32_t signalSemaphoreCount;
        nullptr,                       // const VkSemaphore* pSignalSemaphores;
    };
    const VkQueue encodeQueue   = getDeviceQueue(vkd, device, encodeQueueFamilyIndex, 0u);
    const VkQueue transferQueue = getDeviceQueue(vkd, device, transferQueueFamilyIndex, 0u);

    beginCommandBuffer(vkd, *cmdTransferBuffer, 0u);
    cmdPipelineImageMemoryBarrier2(vkd, *cmdTransferBuffer, &imageBarrierOwnershipTransfer);
    endCommandBuffer(vkd, *cmdTransferBuffer);

    beginCommandBuffer(vkd, *cmdEncodeBuffer, 0u);
    cmdPipelineImageMemoryBarrier2(vkd, *cmdEncodeBuffer, &imageBarrierOwnershipEncode);
    cmdPipelineImageMemoryBarrier2(vkd, *cmdEncodeBuffer, &imageBarrierChangeDstLayout);
    endCommandBuffer(vkd, *cmdEncodeBuffer);

    VK_CHECK(vkd.queueSubmit(transferQueue, 1u, &transferSubmitInfo, *transferFence));
    VK_CHECK(vkd.queueSubmit(encodeQueue, 1u, &encodeSubmitInfo, *encodeFence));

    VK_CHECK(vkd.waitForFences(device, DE_LENGTH_OF_ARRAY(fences), fences, true, ~0ull));
}

de::MovePtr<vkt::ycbcr::MultiPlaneImageData> getDecodedImage(const DeviceInterface &vkd, VkDevice device,
                                                             Allocator &allocator, VkImage image, VkImageLayout layout,
                                                             VkFormat format, VkExtent2D codedExtent,
                                                             uint32_t queueFamilyIndexTransfer,
                                                             uint32_t queueFamilyIndexDecode)
{
    de::MovePtr<vkt::ycbcr::MultiPlaneImageData> multiPlaneImageData(
        new vkt::ycbcr::MultiPlaneImageData(format, tcu::UVec2(codedExtent.width, codedExtent.height)));
    const VkQueue queueDecode   = getDeviceQueue(vkd, device, queueFamilyIndexDecode, 0u);
    const VkQueue queueTransfer = getDeviceQueue(vkd, device, queueFamilyIndexTransfer, 0u);
    const VkImageSubresourceRange imageSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);
    const VkImageMemoryBarrier2KHR imageBarrierDecode =
        makeImageMemoryBarrier2(VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR, VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR,
                                VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR, VK_ACCESS_NONE_KHR, layout,
                                VK_IMAGE_LAYOUT_GENERAL, image, imageSubresourceRange);
    const VkImageMemoryBarrier2KHR imageBarrierOwnershipDecode = makeImageMemoryBarrier2(
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR, VK_ACCESS_NONE_KHR, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,
        VK_ACCESS_NONE_KHR, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, imageSubresourceRange,
        queueFamilyIndexDecode, queueFamilyIndexTransfer);
    const VkImageMemoryBarrier2KHR imageBarrierOwnershipTransfer = makeImageMemoryBarrier2(
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR, VK_ACCESS_NONE_KHR, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,
        VK_ACCESS_NONE_KHR, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,

        image, imageSubresourceRange, queueFamilyIndexDecode, queueFamilyIndexTransfer);
    const VkImageMemoryBarrier2KHR imageBarrierTransfer = makeImageMemoryBarrier2(
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT_KHR, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image,
        imageSubresourceRange);
    const Move<VkCommandPool> cmdDecodePool(makeCommandPool(vkd, device, queueFamilyIndexDecode));
    const Move<VkCommandBuffer> cmdDecodeBuffer(
        allocateCommandBuffer(vkd, device, *cmdDecodePool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const Move<VkCommandPool> cmdTransferPool(makeCommandPool(vkd, device, queueFamilyIndexTransfer));
    const Move<VkCommandBuffer> cmdTransferBuffer(
        allocateCommandBuffer(vkd, device, *cmdTransferPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    Move<VkSemaphore> semaphore                 = createSemaphore(vkd, device);
    Move<VkFence> decodeFence                   = createFence(vkd, device);
    Move<VkFence> transferFence                 = createFence(vkd, device);
    VkFence fences[]                            = {*decodeFence, *transferFence};
    const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    const VkSubmitInfo decodeSubmitInfo{
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType sType;
        nullptr,                       // const void* pNext;
        0u,                            // uint32_t waitSemaphoreCount;
        nullptr,                       // const VkSemaphore* pWaitSemaphores;
        nullptr,                       // const VkPipelineStageFlags* pWaitDstStageMask;
        1u,                            // uint32_t commandBufferCount;
        &*cmdDecodeBuffer,             // const VkCommandBuffer* pCommandBuffers;
        1u,                            // uint32_t signalSemaphoreCount;
        &*semaphore,                   // const VkSemaphore* pSignalSemaphores;
    };
    const VkSubmitInfo transferSubmitInfo{
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType sType;
        nullptr,                       // const void* pNext;
        1u,                            // uint32_t waitSemaphoreCount;
        &*semaphore,                   // const VkSemaphore* pWaitSemaphores;
        &waitDstStageMask,             // const VkPipelineStageFlags* pWaitDstStageMask;
        1u,                            // uint32_t commandBufferCount;
        &*cmdTransferBuffer,           // const VkCommandBuffer* pCommandBuffers;
        0u,                            // uint32_t signalSemaphoreCount;
        nullptr,                       // const VkSemaphore* pSignalSemaphores;
    };

    beginCommandBuffer(vkd, *cmdDecodeBuffer, 0u);
    cmdPipelineImageMemoryBarrier2(vkd, *cmdDecodeBuffer, &imageBarrierDecode);
    cmdPipelineImageMemoryBarrier2(vkd, *cmdDecodeBuffer, &imageBarrierOwnershipDecode);
    endCommandBuffer(vkd, *cmdDecodeBuffer);

    beginCommandBuffer(vkd, *cmdTransferBuffer, 0u);
    cmdPipelineImageMemoryBarrier2(vkd, *cmdTransferBuffer, &imageBarrierOwnershipTransfer);
    cmdPipelineImageMemoryBarrier2(vkd, *cmdTransferBuffer, &imageBarrierTransfer);
    endCommandBuffer(vkd, *cmdTransferBuffer);

    VK_CHECK(vkd.queueSubmit(queueDecode, 1u, &decodeSubmitInfo, *decodeFence));
    VK_CHECK(vkd.queueSubmit(queueTransfer, 1u, &transferSubmitInfo, *transferFence));

    VK_CHECK(vkd.waitForFences(device, DE_LENGTH_OF_ARRAY(fences), fences, true, ~0ull));

    vkt::ycbcr::downloadImage(vkd, device, queueFamilyIndexTransfer, allocator, image, multiPlaneImageData.get(), 0,
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    return multiPlaneImageData;
}

bool VideoBaseTestInstance::createDeviceSupportingQueue(const VkQueueFlags queueFlagsRequired,
                                                        const VkVideoCodecOperationFlagsKHR videoCodecOperationFlags,
                                                        const VideoDevice::VideoDeviceFlags videoDeviceFlags)
{
    return m_videoDevice.createDeviceSupportingQueue(queueFlagsRequired, videoCodecOperationFlags, videoDeviceFlags);
}

VkDevice VideoBaseTestInstance::getDeviceSupportingQueue(const VkQueueFlags queueFlagsRequired,
                                                         const VkVideoCodecOperationFlagsKHR videoCodecOperationFlags,
                                                         const VideoDevice::VideoDeviceFlags videoDeviceFlags)
{
    return m_videoDevice.getDeviceSupportingQueue(queueFlagsRequired, videoCodecOperationFlags, videoDeviceFlags);
}

const DeviceDriver &VideoBaseTestInstance::getDeviceDriver(void)
{
    return m_videoDevice.getDeviceDriver();
}

uint32_t VideoBaseTestInstance::getQueueFamilyIndexTransfer(void)
{
    return m_videoDevice.getQueueFamilyIndexTransfer();
}

uint32_t VideoBaseTestInstance::getQueueFamilyIndexDecode(void)
{
    return m_videoDevice.getQueueFamilyIndexDecode();
}

uint32_t VideoBaseTestInstance::getQueueFamilyIndexEncode(void)
{
    return m_videoDevice.getQueueFamilyIndexEncode();
}

Allocator &VideoBaseTestInstance::getAllocator(void)
{
    return m_videoDevice.getAllocator();
}

de::MovePtr<vector<uint8_t>> VideoBaseTestInstance::loadVideoData(const string &filename)
{
    tcu::Archive &archive = m_context.getTestContext().getArchive();
    de::UniquePtr<tcu::Resource> resource(archive.getResource(filename.c_str()));
    const int resourceSize = resource->getSize();
    de::MovePtr<vector<uint8_t>> result(new vector<uint8_t>(resourceSize));

    resource->read(result->data(), resource->getSize());

    return result;
}
#ifdef DE_BUILD_VIDEO
tcu::TestStatus VideoBaseTestInstance::validateEncodedContent(
    VkVideoCodecOperationFlagBitsKHR videoCodecEncodeOperation, StdVideoAV1Profile profile, const char *encodedFileName,
    const char *yuvFileName, int32_t numberOfFrames, int32_t inputWidth, int32_t inputHeight,
    const VkExtent2D expectedOutputExtent, VkVideoChromaSubsamplingFlagsKHR chromaSubsampling,
    VkVideoComponentBitDepthFlagsKHR lumaBitDepth, VkVideoComponentBitDepthFlagsKHR chromaBitDepth,
    double psnrThresholdLowerLimit)
{
    double criticalPsnrThreshold                               = 10.0;
    VkVideoCodecOperationFlagBitsKHR videoCodecDecodeOperation = VK_VIDEO_CODEC_OPERATION_NONE_KHR;
    ElementaryStreamFraming framing                            = ElementaryStreamFraming::UNKNOWN;

    switch (videoCodecEncodeOperation)
    {
    case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR:
        videoCodecDecodeOperation = VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR;
        framing                   = ElementaryStreamFraming::H26X_BYTE_STREAM;
        break;
    case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR:
        videoCodecDecodeOperation = VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR;
        framing                   = ElementaryStreamFraming::H26X_BYTE_STREAM;
        break;
    case VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR:
        videoCodecDecodeOperation = VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR;
        framing                   = ElementaryStreamFraming::IVF;
        break;
    default:
        tcu::TestStatus::fail("Unable to validate the encoded content, the decode operation is not supported.");
    };

    VideoDevice::VideoDeviceFlags videoDeviceFlags = VideoDevice::VIDEO_DEVICE_FLAG_REQUIRE_SYNC2_OR_NOT_SUPPORTED;
    const VkPhysicalDevice physicalDevice          = m_context.getPhysicalDevice();
    const VkDevice videoDevice =
        getDeviceSupportingQueue(VK_QUEUE_VIDEO_ENCODE_BIT_KHR | VK_QUEUE_VIDEO_DECODE_BIT_KHR | VK_QUEUE_TRANSFER_BIT,
                                 videoCodecDecodeOperation | videoCodecEncodeOperation, videoDeviceFlags);
    const DeviceInterface &videoDeviceDriver = getDeviceDriver();

    const uint32_t encodeQueueFamilyIndex   = getQueueFamilyIndexEncode();
    const uint32_t decodeQueueFamilyIndex   = getQueueFamilyIndexDecode();
    const uint32_t transferQueueFamilyIndex = getQueueFamilyIndexTransfer();

    const VkQueue encodeQueue   = getDeviceQueue(videoDeviceDriver, videoDevice, encodeQueueFamilyIndex, 0u);
    const VkQueue decodeQueue   = getDeviceQueue(videoDeviceDriver, videoDevice, decodeQueueFamilyIndex, 0u);
    const VkQueue transferQueue = getDeviceQueue(videoDeviceDriver, videoDevice, transferQueueFamilyIndex, 0u);

    DeviceContext deviceContext(&m_context, &m_videoDevice, physicalDevice, videoDevice, decodeQueue, encodeQueue,
                                transferQueue);
    auto decodeProfile =
        VkVideoCoreProfile(videoCodecDecodeOperation, chromaSubsampling, lumaBitDepth, chromaBitDepth, profile);
    auto basicDecoder = createBasicDecoder(&deviceContext, &decodeProfile, numberOfFrames, false);

    Demuxer::Params demuxParams = {};
    demuxParams.data            = std::make_unique<BufferedReader>(static_cast<const char *>(encodedFileName));
    demuxParams.codecOperation  = videoCodecDecodeOperation;

    demuxParams.framing = framing;
    auto demuxer        = Demuxer::create(std::move(demuxParams));

    VkVideoParser parser;
    // TODO: Check for decoder extension support before attempting validation!
    createParser(demuxer->codecOperation(), basicDecoder, parser, demuxer->framing());

    FrameProcessor processor(std::move(demuxer), basicDecoder);
    std::vector<int> incorrectFrames;
    std::vector<int> correctFrames;
    for (int frameIdx = 0; frameIdx < numberOfFrames; frameIdx++)
    {
        DecodedFrame frame;
        TCU_CHECK_AND_THROW(
            InternalError, processor.getNextFrame(&frame) > 0,
            "Expected more frames from the bitstream. Most likely an internal CTS bug, or maybe an invalid bitstream");

        auto resultImage =
            getDecodedImageFromContext(deviceContext,
                                       basicDecoder->dpbAndOutputCoincide() ? VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR :
                                                                              VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR,
                                       &frame);

        if (frame.displayWidth != expectedOutputExtent.width || frame.displayHeight != expectedOutputExtent.height)
        {
            return tcu::TestStatus::fail(
                "Decoded frame resolution (" + std::to_string(frame.displayWidth) + "," +
                std::to_string(frame.displayHeight) + ")" + " doesn't match expected resolution (" +
                std::to_string(expectedOutputExtent.width) + "," + std::to_string(expectedOutputExtent.height) + ")");
        }

        double psnr;
        if (lumaBitDepth == VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR)
        {
            de::MovePtr<std::vector<uint8_t>> out =
                vkt::ycbcr::YCbCrConvUtil<uint8_t>::MultiPlanarNV12toI420(resultImage.get());
            de::MovePtr<std::vector<uint8_t>> inputFrame =
                vkt::ycbcr::YCbCrContent<uint8_t>::getFrame(yuvFileName, inputWidth, inputHeight, frameIdx);
            psnr = util::PSNRImplicitCrop(*inputFrame, inputWidth, inputHeight, *out, expectedOutputExtent.width,
                                          expectedOutputExtent.height);
#if STREAM_DUMP_DEBUG
            const string outputFileName = "out_" + std::to_string(frameIdx) + ".yuv";
            vkt::ycbcr::YCbCrContent<uint8_t>::save(*out, outputFileName);
            const string refFileName = "ref_" + std::to_string(frameIdx) + ".yuv";
            vkt::ycbcr::YCbCrContent<uint8_t>::save(*inputFrame, refFileName);
#endif
        }
        else
        {
            de::MovePtr<std::vector<uint16_t>> out =
                vkt::ycbcr::YCbCrConvUtil<uint16_t>::MultiPlanarNV12toI420(resultImage.get());
            de::MovePtr<std::vector<uint16_t>> inputFrame =
                vkt::ycbcr::YCbCrContent<uint16_t>::getFrame(yuvFileName, inputWidth, inputHeight, frameIdx);
            psnr = util::PSNRImplicitCrop(*inputFrame, inputWidth, inputHeight, *out, expectedOutputExtent.width,
                                          expectedOutputExtent.height);
#if STREAM_DUMP_DEBUG
            const string outputFileName = "out_" + std::to_string(frameIdx) + ".yuv";
            vkt::ycbcr::YCbCrContent<uint16_t>::save(*out, outputFileName);
            const string refFileName = "ref_" + std::to_string(frameIdx) + ".yuv";
            vkt::ycbcr::YCbCrContent<uint16_t>::save(*inputFrame, refFileName);
#endif
        }
#if STREAM_DUMP_DEBUG
        cout << "Current PSNR: " << psnr << endl;
#endif

        string failMessage;

        if (psnr < psnrThresholdLowerLimit)
        {
            double difference = psnrThresholdLowerLimit - psnr;

            if (psnr > criticalPsnrThreshold)
            {
                failMessage = "Frame " + std::to_string(frameIdx) + " with PSNR " + std::to_string(psnr) + " is " +
                              std::to_string(difference) + " points below the lower threshold";
                return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, failMessage);
            }
            else
            {
                failMessage = "Frame " + std::to_string(frameIdx) + " with PSNR " + std::to_string(psnr) + " is " +
                              std::to_string(difference) + " points below the critical threshold";
                return tcu::TestStatus::fail(failMessage);
            }
        }
    }

    return tcu::TestStatus::pass("Video encoding completed successfully");
}
#endif

de::MovePtr<VkVideoDecodeCapabilitiesKHR> getVideoDecodeCapabilities(void *pNext)
{
    const VkVideoDecodeCapabilitiesKHR videoDecodeCapabilities = {
        vk::VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR, //  VkStructureType sType;
        pNext,                                               //  void* pNext;
        0,                                                   //  VkVideoDecodeCapabilityFlagsKHR Flags;
    };

    return de::MovePtr<VkVideoDecodeCapabilitiesKHR>(new VkVideoDecodeCapabilitiesKHR(videoDecodeCapabilities));
}

de::MovePtr<VkVideoDecodeH264CapabilitiesKHR> getVideoCapabilitiesExtensionH264D(void)
{
    const VkVideoDecodeH264CapabilitiesKHR videoCapabilitiesExtension = {
        vk::VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_CAPABILITIES_KHR, //  VkStructureType sType;
        nullptr,                                                  //  void* pNext;
        STD_VIDEO_H264_LEVEL_IDC_1_0,                             //  StdVideoH264Level maxLevel;
        {0, 0},                                                   //  VkOffset2D fieldOffsetGranularity;
    };

    return de::MovePtr<VkVideoDecodeH264CapabilitiesKHR>(
        new VkVideoDecodeH264CapabilitiesKHR(videoCapabilitiesExtension));
}

de::MovePtr<VkVideoEncodeH264CapabilitiesKHR> getVideoCapabilitiesExtensionH264E(void *pNext)
{
    const VkVideoEncodeH264CapabilitiesKHR videoCapabilitiesExtension = {
        vk::VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_CAPABILITIES_KHR, //  VkStructureType sType;
        pNext,                                                    //  const void* pNext;
        0u,                                                       //  VkVideoEncodeH264CapabilityFlagsKHR flags;
        static_cast<StdVideoH264LevelIdc>(0u),                    //  StdVideoH264LevelIdc maxLevelIdc;
        0u,                                                       //  uint32_t maxSliceCount;
        0u,                                                       //  uint8_t maxPPictureL0ReferenceCount;
        0u,                                                       //  uint8_t maxBPictureL0ReferenceCount;
        0u,                                                       //  uint8_t maxL1ReferenceCount;
        0u,                                                       //  uint32_t maxTemporalLayerCount;
        false,                                                    //  VkBool32 expectDyadicTemporalLayerPattern;
        0u,                                                       //  uint32_t minQp;
        0u,                                                       //  uint32_t maxQp;
        false,                                                    //  VkBool32 prefersGopRemainingFrames;
        false,                                                    //  VkBool32 requiresGopRemainingFrames;
        static_cast<VkVideoEncodeH264StdFlagsKHR>(0)              //  VkVideoEncodeH264StdFlagsKHR stdSyntaxFlags;
    };

    return de::MovePtr<VkVideoEncodeH264CapabilitiesKHR>(
        new VkVideoEncodeH264CapabilitiesKHR(videoCapabilitiesExtension));
}

de::MovePtr<VkVideoEncodeH264QuantizationMapCapabilitiesKHR> getVideoEncodeH264QuantizationMapCapabilities(void)
{
    const VkVideoEncodeH264QuantizationMapCapabilitiesKHR quantizationMapCapabilities = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_QUANTIZATION_MAP_CAPABILITIES_KHR, // VkStructureType sType
        nullptr,                                                               // void* pNext
        0,                                                                     // int32_t minQpDelta
        0                                                                      // int32_t maxQpDelta
    };
    return de::MovePtr<VkVideoEncodeH264QuantizationMapCapabilitiesKHR>(
        new VkVideoEncodeH264QuantizationMapCapabilitiesKHR(quantizationMapCapabilities));
}

de::MovePtr<VkVideoEncodeCapabilitiesKHR> getVideoEncodeCapabilities(void *pNext)
{
    const VkVideoEncodeCapabilitiesKHR videoEncodeCapabilities = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_CAPABILITIES_KHR, //  VkStructureType sType;
        pNext,                                           //  void* pNext;
        0,                                               //  VkVideoEncodeCapabilityFlagsKHR flags;
        0,                                               //  VkVideoEncodeRateControlModeFlagsKHR rateControlModes;
        0,                                               //  uint32_t maxRateControlLayers;
        0,                                               //  uint64_t maxBitrate;
        0,                                               //  uint32_t maxQualityLevels;
        {0, 0},                                          //  VkExtent2D encodeInputPictureGranularity;
        static_cast<VkVideoEncodeFeedbackFlagsKHR>(0),   //  VkVideoEncodeFeedbackFlagsKHR supportedEncodeFeedbackFlags;
    };

    return de::MovePtr<VkVideoEncodeCapabilitiesKHR>(new VkVideoEncodeCapabilitiesKHR(videoEncodeCapabilities));
}

de::MovePtr<VkVideoDecodeH265CapabilitiesKHR> getVideoCapabilitiesExtensionH265D(void)
{
    const VkVideoDecodeH265CapabilitiesKHR videoCapabilitiesExtension = {
        VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_CAPABILITIES_KHR, //  VkStructureType sType;
        nullptr,                                              //  void* pNext;
        STD_VIDEO_H265_LEVEL_IDC_1_0,                         //  StdVideoH265Level maxLevel;
    };

    return de::MovePtr<VkVideoDecodeH265CapabilitiesKHR>(
        new VkVideoDecodeH265CapabilitiesKHR(videoCapabilitiesExtension));
}

de::MovePtr<VkVideoEncodeH265CapabilitiesKHR> getVideoCapabilitiesExtensionH265E(void *pNext)
{
    const VkVideoEncodeH265CapabilitiesKHR videoCapabilitiesExtension = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_CAPABILITIES_KHR, //  VkStructureType sType;
        pNext,                                                //  const void* pNext;
        0u,                                                   //  VkVideoEncodeH265CapabilityFlagsKHR flags;
        static_cast<StdVideoH265LevelIdc>(0),                 //  StdVideoH265LevelIdc maxLevelIdc;
        0u,                                                   //  uint32_t maxSliceSegmentCount;
        {0, 0},                                               //  VkExtent2D maxTiles;
        VK_VIDEO_ENCODE_H265_CTB_SIZE_32_BIT_KHR,             //  VkVideoEncodeH265CtbSizeFlagsKHR ctbSizes;
        0u,    //  VkVideoEncodeH265TransformBlockSizeFlagsKHR transformBlockSizes;
        0u,    //  uint8_t maxPPictureL0ReferenceCount;
        0u,    //  uint8_t maxBPictureL0ReferenceCount;
        0u,    //  uint32_t maxL1ReferenceCount;
        0u,    //  uint32_t maxSubLayerCount;
        false, //  VkBool32 expectDyadicTemporalSubLayerPattern;
        0u,    //  int32_t minQp;
        0u,    //  int32_t maxQp;
        false, //  VkBool32 prefersGopRemainingFrames;
        false, //  VkBool32 requiresGopRemainingFrames;
        static_cast<VkVideoEncodeH265StdFlagsKHR>(0), //  VkVideoEncodeH265StdFlagsKHR stdSyntaxFlags;
    };

    return de::MovePtr<VkVideoEncodeH265CapabilitiesKHR>(
        new VkVideoEncodeH265CapabilitiesKHR(videoCapabilitiesExtension));
}

de::MovePtr<VkVideoEncodeH265QuantizationMapCapabilitiesKHR> getVideoEncodeH265QuantizationMapCapabilities(void)
{
    const VkVideoEncodeH265QuantizationMapCapabilitiesKHR quantizationMapCapabilities = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_QUANTIZATION_MAP_CAPABILITIES_KHR, // VkStructureType sType
        nullptr,                                                               // void* pNext
        0,                                                                     // int32_t minQpDelta
        0                                                                      // int32_t maxQpDelta
    };
    return de::MovePtr<VkVideoEncodeH265QuantizationMapCapabilitiesKHR>(
        new VkVideoEncodeH265QuantizationMapCapabilitiesKHR(quantizationMapCapabilities));
}

de::MovePtr<VkVideoEncodeAV1CapabilitiesKHR> getVideoCapabilitiesExtensionAV1E(void)
{
    const VkVideoEncodeAV1CapabilitiesKHR videoCapabilitiesExtension = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_AV1_CAPABILITIES_KHR, // VkStructureType sType
        nullptr,                                             // void* pNext
        0u,                                                  // VkVideoEncodeAV1CapabilityFlagsKHR flags
        static_cast<StdVideoAV1Level>(0),                    // StdVideoAV1Level maxLevel
        {0, 0},                                              // codedPictureAlignment
        {0, 0},                                              // VkExtent2D maxTiles
        {0, 0},                                              // VkExtent2D minTileSize
        {0, 0},                                              // VkExtent2D maxTileSize
        static_cast<VkVideoEncodeAV1SuperblockSizeFlagsKHR>(
            0),                                     // VkVideoEncodeAV1SuperblockSizeFlagsKHR superblockSizes
        0u,                                         // uint32_t maxSingleReferenceCount
        0u,                                         // uint32_t singleReferenceNameMask
        0u,                                         // uint32_t maxUnidirectionalCompoundReferenceCount
        0u,                                         // uint32_t maxUnidirectionalCompoundGroup1ReferenceCount
        0u,                                         // uint32_t unidirectionalCompoundReferenceNameMask
        0u,                                         // uint32_t maxBidirectionalCompoundReferenceCount
        0u,                                         // uint32_t maxBidirectionalCompoundGroup1ReferenceCount
        0u,                                         // uint32_t maxBidirectionalCompoundGroup2ReferenceCount
        0u,                                         // uint32_t bidirectionalCompoundReferenceNameMask
        0u,                                         // uint32_t maxTemporalLayerCount
        0u,                                         // uint32_t maxSpatialLayerCount
        0u,                                         // uint32_t maxOperatingPoints
        0u,                                         // uint32_t minQIndex
        0u,                                         // uint32_t maxQIndex
        VK_FALSE,                                   // VkBool32 prefersGopRemainingFrames
        VK_FALSE,                                   // VkBool32 requiresGopRemainingFrames
        static_cast<VkVideoEncodeAV1StdFlagsKHR>(0) // VkVideoEncodeAV1StdFlagsKHR stdSyntaxFlags
    };

    return de::MovePtr<VkVideoEncodeAV1CapabilitiesKHR>(
        new VkVideoEncodeAV1CapabilitiesKHR(videoCapabilitiesExtension));
}

de::MovePtr<VkVideoCapabilitiesKHR> getVideoCapabilities(const InstanceInterface &vk, VkPhysicalDevice physicalDevice,
                                                         const VkVideoProfileInfoKHR *videoProfile, void *pNext)
{
    VkVideoCapabilitiesKHR *videoCapabilities = new VkVideoCapabilitiesKHR{
        VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR, //  VkStructureType sType;
        pNext,                                    //  void* pNext;
        0,                                        //  VkVideoCapabilityFlagsKHR capabilityFlags;
        0,                                        //  VkDeviceSize minBitstreamBufferOffsetAlignment;
        0,                                        //  VkDeviceSize minBitstreamBufferSizeAlignment;
        {0, 0},                                   //  VkExtent2D videoPictureExtentGranularity;
        {0, 0},                                   //  VkExtent2D minExtent;
        {0, 0},                                   //  VkExtent2D maxExtent;
        0,                                        //  uint32_t maxReferencePicturesSlotsCount;
        0,                                        //  uint32_t maxReferencePicturesActiveCount;
        {{0}, 0},                                 //  VkExtensionProperties stdHeaderVersion;
    };
    de::MovePtr<VkVideoCapabilitiesKHR> result = de::MovePtr<VkVideoCapabilitiesKHR>(videoCapabilities);

    VK_CHECK(vk.getPhysicalDeviceVideoCapabilitiesKHR(physicalDevice, videoProfile, videoCapabilities));

    return result;
}

de::MovePtr<VkVideoEncodeIntraRefreshCapabilitiesKHR> getIntraRefreshCapabilities(void)
{
    VkVideoEncodeIntraRefreshCapabilitiesKHR intraRefreshCapabilities = {};
    intraRefreshCapabilities.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_INTRA_REFRESH_CAPABILITIES_KHR;
    intraRefreshCapabilities.pNext = VK_NULL_HANDLE;

    return de::MovePtr<VkVideoEncodeIntraRefreshCapabilitiesKHR>(
        new VkVideoEncodeIntraRefreshCapabilitiesKHR(intraRefreshCapabilities));
}

de::MovePtr<VkVideoDecodeH264ProfileInfoKHR> getVideoProfileExtensionH264D(
    StdVideoH264ProfileIdc stdProfileIdc, VkVideoDecodeH264PictureLayoutFlagBitsKHR pictureLayout)
{
    VkVideoDecodeH264ProfileInfoKHR *videoCodecOperation =
        new VkVideoDecodeH264ProfileInfoKHR(getProfileOperationH264Decode(stdProfileIdc, pictureLayout));
    de::MovePtr<VkVideoDecodeH264ProfileInfoKHR> result =
        de::MovePtr<VkVideoDecodeH264ProfileInfoKHR>(videoCodecOperation);

    return result;
}

de::MovePtr<VkVideoEncodeH264ProfileInfoKHR> getVideoProfileExtensionH264E(StdVideoH264ProfileIdc stdProfileIdc)
{
    VkVideoEncodeH264ProfileInfoKHR *videoCodecOperation =
        new VkVideoEncodeH264ProfileInfoKHR(getProfileOperationH264Encode(stdProfileIdc));
    de::MovePtr<VkVideoEncodeH264ProfileInfoKHR> result =
        de::MovePtr<VkVideoEncodeH264ProfileInfoKHR>(videoCodecOperation);

    return result;
}

de::MovePtr<VkVideoDecodeH265ProfileInfoKHR> getVideoProfileExtensionH265D(StdVideoH265ProfileIdc stdProfileIdc)
{
    VkVideoDecodeH265ProfileInfoKHR *videoCodecOperation =
        new VkVideoDecodeH265ProfileInfoKHR(getProfileOperationH265Decode(stdProfileIdc));
    de::MovePtr<VkVideoDecodeH265ProfileInfoKHR> result =
        de::MovePtr<VkVideoDecodeH265ProfileInfoKHR>(videoCodecOperation);

    return result;
}

de::MovePtr<VkVideoEncodeH265ProfileInfoKHR> getVideoProfileExtensionH265E(StdVideoH265ProfileIdc stdProfileIdc)
{
    VkVideoEncodeH265ProfileInfoKHR *videoCodecOperation =
        new VkVideoEncodeH265ProfileInfoKHR(getProfileOperationH265Encode(stdProfileIdc));
    de::MovePtr<VkVideoEncodeH265ProfileInfoKHR> result =
        de::MovePtr<VkVideoEncodeH265ProfileInfoKHR>(videoCodecOperation);

    return result;
}

de::MovePtr<VkVideoEncodeUsageInfoKHR> getEncodeUsageInfo(void *pNext, VkVideoEncodeUsageFlagsKHR videoUsageHints,
                                                          VkVideoEncodeContentFlagsKHR videoContentHints,
                                                          VkVideoEncodeTuningModeKHR tuningMode)
{
    VkVideoEncodeUsageInfoKHR *encodeUsageInfo = new VkVideoEncodeUsageInfoKHR{
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_USAGE_INFO_KHR, //  VkStructureType sType
        pNext,                                         //  const void* pNext
        videoUsageHints,                               //  VkVideoEncodeUsageFlagsKHR videoUsageHints
        videoContentHints,                             //  VkVideoEncodeContentFlagsKHR videoContentHints
        tuningMode                                     //  VkVideoEncodeTuningModeKHR tuningMode
    };

    de::MovePtr<VkVideoEncodeUsageInfoKHR> result = de::MovePtr<VkVideoEncodeUsageInfoKHR>(encodeUsageInfo);

    return result;
}

de::MovePtr<VkVideoProfileInfoKHR> getVideoProfile(VkVideoCodecOperationFlagBitsKHR videoCodecOperation, void *pNext,
                                                   VkVideoChromaSubsamplingFlagsKHR chromaSubsampling,
                                                   VkVideoComponentBitDepthFlagsKHR lumaBitDepth,
                                                   VkVideoComponentBitDepthFlagsKHR chromaBitDepth)
{
    VkVideoProfileInfoKHR *videoProfile = new VkVideoProfileInfoKHR{
        VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR, //  VkStructureType sType;
        pNext,                                    //  void* pNext;
        videoCodecOperation,                      //  VkVideoCodecOperationFlagBitsKHR videoCodecOperation;
        chromaSubsampling,                        //  VkVideoChromaSubsamplingFlagsKHR chromaSubsampling;
        lumaBitDepth,                             //  VkVideoComponentBitDepthFlagsKHR lumaBitDepth;
        chromaBitDepth,                           //  VkVideoComponentBitDepthFlagsKHR chromaBitDepth;
    };
    de::MovePtr<VkVideoProfileInfoKHR> result = de::MovePtr<VkVideoProfileInfoKHR>(videoProfile);

    return result;
}

de::MovePtr<VkVideoProfileListInfoKHR> getVideoProfileList(const VkVideoProfileInfoKHR *videoProfile,
                                                           uint32_t profileCount)
{
    VkVideoProfileListInfoKHR *videoProfileList = new VkVideoProfileListInfoKHR{
        VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR, //  VkStructureType sType;
        nullptr,                                       //  const void* pNext;
        profileCount,                                  //  uint32_t profileCount;
        videoProfile,                                  //  const VkVideoProfileInfoKHR* pProfiles;
    };

    de::MovePtr<VkVideoProfileListInfoKHR> result = de::MovePtr<VkVideoProfileListInfoKHR>(videoProfileList);

    return result;
}

const VkExtensionProperties *getVideoExtensionProperties(const VkVideoCodecOperationFlagBitsKHR codecOperation)
{
    switch (codecOperation)
    {
    case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR:
        return &EXTENSION_PROPERTIES_H264_ENCODE;
    case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR:
        return &EXTENSION_PROPERTIES_H265_ENCODE;
    case VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR:
        return &EXTENSION_PROPERTIES_AV1_ENCODE;
    case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
        return &EXTENSION_PROPERTIES_H264_DECODE;
    case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
        return &EXTENSION_PROPERTIES_H265_DECODE;
    case VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR:
        return &EXTENSION_PROPERTIES_AV1_DECODE;
    case VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR:
        return &EXTENSION_PROPERTIES_VP9_DECODE;
    default:
        TCU_THROW(InternalError, "Unkown codec operation");
    }
}

de::MovePtr<VkVideoSessionCreateInfoKHR> getVideoSessionCreateInfo(
    uint32_t queueFamilyIndex, VkVideoSessionCreateFlagsKHR flags, const VkVideoProfileInfoKHR *videoProfile,
    const VkExtent2D &codedExtent, VkFormat pictureFormat, VkFormat referencePicturesFormat,
    uint32_t maxReferencePicturesSlotsCount, uint32_t maxReferencePicturesActiveCount)
{
    const VkExtensionProperties *extensionProperties = getVideoExtensionProperties(videoProfile->videoCodecOperation);

    VkVideoSessionCreateInfoKHR *videoSessionCreateInfo = new VkVideoSessionCreateInfoKHR{
        VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR, //  VkStructureType sType;
        nullptr,                                         //  const void* pNext;
        queueFamilyIndex,                                //  uint32_t queueFamilyIndex;
        flags,                                           //  VkVideoSessionCreateFlagsKHR flags;
        videoProfile,                                    //  const VkVideoProfileInfoKHR* pVideoProfile;
        pictureFormat,                                   //  VkFormat pictureFormat;
        codedExtent,                                     //  VkExtent2D maxCodedExtent;
        referencePicturesFormat,                         //  VkFormat referencePicturesFormat;
        maxReferencePicturesSlotsCount,                  //  uint32_t maxReferencePicturesSlotsCount;
        maxReferencePicturesActiveCount,                 //  uint32_t maxReferencePicturesActiveCount;
        extensionProperties,                             //  const VkExtensionProperties* pStdHeaderVersion;
    };

    de::MovePtr<VkVideoSessionCreateInfoKHR> result = de::MovePtr<VkVideoSessionCreateInfoKHR>(videoSessionCreateInfo);

    return result;
}

vector<AllocationPtr> getAndBindVideoSessionMemory(const DeviceInterface &vkd, const VkDevice device,
                                                   VkVideoSessionKHR videoSession, Allocator &allocator)
{
    uint32_t videoSessionMemoryRequirementsCount = 0;

    DE_ASSERT(videoSession != VK_NULL_HANDLE);

    VK_CHECK(
        vkd.getVideoSessionMemoryRequirementsKHR(device, videoSession, &videoSessionMemoryRequirementsCount, nullptr));

    const VkVideoSessionMemoryRequirementsKHR videoGetMemoryPropertiesKHR = {
        VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR, //  VkStructureType sType;
        nullptr,                                                 //  const void* pNext;
        0u,                                                      //  uint32_t memoryBindIndex;
        {0ull, 0ull, 0u},                                        //  VkMemoryRequirements    memoryRequirements;
    };

    vector<VkVideoSessionMemoryRequirementsKHR> videoSessionMemoryRequirements(videoSessionMemoryRequirementsCount,
                                                                               videoGetMemoryPropertiesKHR);

    for (size_t ndx = 0; ndx < videoSessionMemoryRequirements.size(); ++ndx)
        videoSessionMemoryRequirements[ndx].sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR;

    VK_CHECK(vkd.getVideoSessionMemoryRequirementsKHR(device, videoSession, &videoSessionMemoryRequirementsCount,
                                                      videoSessionMemoryRequirements.data()));

    vector<AllocationPtr> allocations(videoSessionMemoryRequirements.size());
    vector<VkBindVideoSessionMemoryInfoKHR> videoBindsMemoryKHR(videoSessionMemoryRequirements.size());

    for (size_t ndx = 0; ndx < allocations.size(); ++ndx)
    {
        const VkMemoryRequirements &requirements = videoSessionMemoryRequirements[ndx].memoryRequirements;
        const uint32_t memoryBindIndex           = videoSessionMemoryRequirements[ndx].memoryBindIndex;
        de::MovePtr<Allocation> alloc            = allocator.allocate(requirements, MemoryRequirement::Any);

        const VkBindVideoSessionMemoryInfoKHR videoBindMemoryKHR = {
            VK_STRUCTURE_TYPE_BIND_VIDEO_SESSION_MEMORY_INFO_KHR, //  VkStructureType sType;
            nullptr,                                              //  const void* pNext;
            memoryBindIndex,                                      //  uint32_t memoryBindIndex;
            alloc->getMemory(),                                   //  VkDeviceMemory memory;
            alloc->getOffset(),                                   //  VkDeviceSize memoryOffset;
            requirements.size,                                    //  VkDeviceSize memorySize;
        };

        allocations[ndx] = alloc;

        videoBindsMemoryKHR[ndx] = videoBindMemoryKHR;
    }

    VK_CHECK(vkd.bindVideoSessionMemoryKHR(device, videoSession, static_cast<uint32_t>(videoBindsMemoryKHR.size()),
                                           videoBindsMemoryKHR.data()));

    return allocations;
}

de::MovePtr<vector<VkFormat>> getSupportedFormats(const InstanceInterface &vk, const VkPhysicalDevice physicalDevice,
                                                  const VkImageUsageFlags imageUsageFlags,
                                                  const VkVideoProfileListInfoKHR *videoProfileList)

{
    uint32_t videoFormatPropertiesCount = 0u;

    const VkPhysicalDeviceVideoFormatInfoKHR videoFormatInfo = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_FORMAT_INFO_KHR, //  VkStructureType sType;
        videoProfileList,                                        //  const void* pNext;
        imageUsageFlags,                                         //  VkImageUsageFlags imageUsage;
    };

    VkVideoFormatPropertiesKHR videoFormatPropertiesKHR = {};
    videoFormatPropertiesKHR.sType                      = VK_STRUCTURE_TYPE_VIDEO_FORMAT_PROPERTIES_KHR;
    videoFormatPropertiesKHR.pNext                      = nullptr;

    vector<VkVideoFormatPropertiesKHR> videoFormatProperties;
    de::MovePtr<vector<VkFormat>> result;

    const VkResult res = vk.getPhysicalDeviceVideoFormatPropertiesKHR(physicalDevice, &videoFormatInfo,
                                                                      &videoFormatPropertiesCount, nullptr);

    if (res != VK_SUCCESS)
        return de::MovePtr<vector<VkFormat>>(nullptr);

    videoFormatProperties.resize(videoFormatPropertiesCount, videoFormatPropertiesKHR);

    VK_CHECK(vk.getPhysicalDeviceVideoFormatPropertiesKHR(physicalDevice, &videoFormatInfo, &videoFormatPropertiesCount,
                                                          videoFormatProperties.data()));

    DE_ASSERT(videoFormatPropertiesCount == videoFormatProperties.size());

    result = de::MovePtr<vector<VkFormat>>(new vector<VkFormat>);

    result->reserve(videoFormatProperties.size());

    for (const auto &videoFormatProperty : videoFormatProperties)
        result->push_back(videoFormatProperty.format);

    return result;
}

VkVideoFormatPropertiesKHR getSupportedFormatProperties(const InstanceInterface &vk,
                                                        const VkPhysicalDevice physicalDevice,
                                                        const VkImageUsageFlags imageUsageFlags, void *pNext,
                                                        const VkFormat format)

{
    if (format == VK_FORMAT_UNDEFINED)
        return VkVideoFormatPropertiesKHR();

    uint32_t videoFormatPropertiesCount = 0u;

    const VkPhysicalDeviceVideoFormatInfoKHR videoFormatInfo = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_FORMAT_INFO_KHR, //  VkStructureType sType;
        pNext,                                                   //  const void* pNext;
        imageUsageFlags,                                         //  VkImageUsageFlags imageUsage;
    };

    VkVideoFormatPropertiesKHR videoFormatPropertiesKHR = {};
    videoFormatPropertiesKHR.sType                      = VK_STRUCTURE_TYPE_VIDEO_FORMAT_PROPERTIES_KHR;
    videoFormatPropertiesKHR.pNext                      = nullptr;

    vector<VkVideoFormatPropertiesKHR> videoFormatProperties;

    const VkResult res = vk.getPhysicalDeviceVideoFormatPropertiesKHR(physicalDevice, &videoFormatInfo,
                                                                      &videoFormatPropertiesCount, nullptr);

    if (res != VK_SUCCESS)
        return VkVideoFormatPropertiesKHR();

    videoFormatProperties.resize(videoFormatPropertiesCount, videoFormatPropertiesKHR);

    VK_CHECK(vk.getPhysicalDeviceVideoFormatPropertiesKHR(physicalDevice, &videoFormatInfo, &videoFormatPropertiesCount,
                                                          videoFormatProperties.data()));

    DE_ASSERT(videoFormatPropertiesCount == videoFormatProperties.size());

    for (const auto &videoFormatProperty : videoFormatProperties)
    {
        if (videoFormatProperty.format == format)
            return videoFormatProperty;
    };

    TCU_THROW(NotSupportedError, "Video format not found in properties list");
}

bool validateVideoExtent(const VkExtent2D &codedExtent, const VkVideoCapabilitiesKHR &videoCapabilities)
{
    if (!de::inRange(codedExtent.width, videoCapabilities.minCodedExtent.width, videoCapabilities.maxCodedExtent.width))
        TCU_THROW(NotSupportedError, "Video width does not fit capabilities");

    if (!de::inRange(codedExtent.height, videoCapabilities.minCodedExtent.height,
                     videoCapabilities.maxCodedExtent.height))
        TCU_THROW(NotSupportedError, "Video height does not fit capabilities");

    return true;
}

bool validateFormatSupport(const InstanceInterface &vk, VkPhysicalDevice physicalDevice,
                           const VkImageUsageFlags imageUsageFlags, const VkVideoProfileListInfoKHR *videoProfileList,
                           const VkFormat format, bool throwException)
{
    de::MovePtr<vector<VkFormat>> supportedVideoFormats =
        getSupportedFormats(vk, physicalDevice, imageUsageFlags, videoProfileList);

    if (supportedVideoFormats)
    {
        if (supportedVideoFormats->size() == 0)
            if (throwException)
                TCU_THROW(NotSupportedError, "Supported video formats count is 0");

        for (const auto &supportedVideoFormat : *supportedVideoFormats)
        {
            if (supportedVideoFormat == format)
                return true;
        }

        if (throwException)
            TCU_THROW(NotSupportedError, "Required format is not supported for video");
    }
    else
    {
        if (throwException)
            TCU_THROW(NotSupportedError, "Separate DPB and DST buffers expected");
    }

    return false;
}

const VkImageFormatProperties getImageFormatProperties(const InstanceInterface &vk, VkPhysicalDevice physicalDevice,
                                                       const VkVideoProfileListInfoKHR *videoProfileList,
                                                       const VkFormat format, const VkImageUsageFlags usage)
{
    /*
    VkImageFormatProperties                                            imageFormatProperties =
    {
        {0,0}, //  VkExtent3D maxExtent;
        0, //  uint32_t maxMipLevels;
        0, //  uint32_t maxArrayLayers;
        0, //  VkSampleCountFlags sampleCounts;
        0, //  VkDeviceSize maxResourceSize;
    };

    VK_CHECK(vk.getPhysicalDeviceImageFormatProperties(physicalDevice, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, static_cast<VkImageCreateFlags>(0), &imageFormatProperties));

    return imageFormatProperties;
    */

    VkPhysicalDeviceImageFormatInfo2 imageFormatInfo = {};
    imageFormatInfo.sType                            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
    imageFormatInfo.pNext                            = videoProfileList;
    imageFormatInfo.format                           = format;
    imageFormatInfo.usage                            = usage;

    VkSamplerYcbcrConversionImageFormatProperties samplerYcbcrConversionImage = {};
    samplerYcbcrConversionImage.sType = vk::VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES;
    samplerYcbcrConversionImage.pNext = nullptr;

    VkImageFormatProperties2 imageFormatProperties2 = {};
    imageFormatProperties2.sType                    = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
    imageFormatProperties2.pNext                    = &samplerYcbcrConversionImage;

    VK_CHECK(vk.getPhysicalDeviceImageFormatProperties2(physicalDevice, &imageFormatInfo, &imageFormatProperties2));

    return imageFormatProperties2.imageFormatProperties;
}

VkVideoDecodeH264ProfileInfoKHR getProfileOperationH264Decode(StdVideoH264ProfileIdc stdProfileIdc,
                                                              VkVideoDecodeH264PictureLayoutFlagBitsKHR pictureLayout)
{
    const VkVideoDecodeH264ProfileInfoKHR videoProfileOperation = {
        VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PROFILE_INFO_KHR, //  VkStructureType sType;
        nullptr,                                              //  const void* pNext;
        stdProfileIdc,                                        //  StdVideoH264ProfileIdc stdProfileIdc;
        pictureLayout, //  VkVideoDecodeH264PictureLayoutFlagBitsKHR pictureLayout;
    };

    return videoProfileOperation;
}

VkVideoEncodeH264ProfileInfoKHR getProfileOperationH264Encode(StdVideoH264ProfileIdc stdProfileIdc)
{
    const VkVideoEncodeH264ProfileInfoKHR videoProfileOperation = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_PROFILE_INFO_KHR, //  VkStructureType sType;
        nullptr,                                              //  const void* pNext;
        stdProfileIdc,                                        //  StdVideoH264ProfileIdc stdProfileIdc;
    };

    return videoProfileOperation;
}

VkVideoDecodeH265ProfileInfoKHR getProfileOperationH265Decode(StdVideoH265ProfileIdc stdProfileIdc)
{
    const VkVideoDecodeH265ProfileInfoKHR videoProfileOperation = {
        VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PROFILE_INFO_KHR, //  VkStructureType sType;
        nullptr,                                              //  const void* pNext;
        stdProfileIdc,                                        //  StdVideoH265ProfileIdc stdProfileIdc;
    };

    return videoProfileOperation;
}

VkVideoEncodeH265ProfileInfoKHR getProfileOperationH265Encode(StdVideoH265ProfileIdc stdProfileIdc)
{
    const VkVideoEncodeH265ProfileInfoKHR videoProfileOperation = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_PROFILE_INFO_KHR, //  VkStructureType sType;
        nullptr,                                              //  const void* pNext;
        stdProfileIdc,                                        //  StdVideoH265ProfileIdc stdProfileIdc;
    };

    return videoProfileOperation;
}

VkVideoDecodeAV1ProfileInfoKHR getProfileOperationAV1Decode(StdVideoAV1Profile stdProfile, bool filmgrainSupport)
{
    const VkVideoDecodeAV1ProfileInfoKHR videoProfileOperation = {
        VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_PROFILE_INFO_KHR,
        nullptr,
        stdProfile,
        filmgrainSupport,
    };

    return videoProfileOperation;
}

VkVideoDecodeVP9ProfileInfoKHR getProfileOperationVP9Decode(StdVideoVP9Profile stdProfile)
{
    const VkVideoDecodeVP9ProfileInfoKHR videoProfileOperation = {
        VK_STRUCTURE_TYPE_VIDEO_DECODE_VP9_PROFILE_INFO_KHR,
        nullptr,
        stdProfile,
    };

    return videoProfileOperation;
}

VkVideoEncodeAV1ProfileInfoKHR getProfileOperationAV1Encode(StdVideoAV1Profile stdProfile)
{
    const VkVideoEncodeAV1ProfileInfoKHR videoProfileOperation = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_AV1_PROFILE_INFO_KHR,
        nullptr,
        stdProfile,
    };

    return videoProfileOperation;
}

VkImageCreateInfo makeImageCreateInfo(VkFormat format, const VkExtent2D &extent, const VkImageCreateFlags flags,
                                      const uint32_t *queueFamilyIndex, const VkImageUsageFlags usage, void *pNext,
                                      const uint32_t arrayLayers, const VkImageLayout initialLayout,
                                      const VkImageTiling tiling)
{

    const VkExtent3D extent3D = makeExtent3D(extent.width, extent.height, 1u);

    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, //  VkStructureType sType;
        pNext,                               //  const void* pNext;
        flags,                               //  VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    //  VkImageType imageType;
        format,                              //  VkFormat format;
        extent3D,                            //  VkExtent3D extent;
        1,                                   //  uint32_t mipLevels;
        arrayLayers,                         //  uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               //  VkSampleCountFlagBits samples;
        tiling,                              //  VkImageTiling tiling;
        usage,                               //  VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           //  VkSharingMode sharingMode;
        1u,                                  //  uint32_t queueFamilyIndexCount;
        queueFamilyIndex,                    //  const uint32_t* pQueueFamilyIndices;
        initialLayout,                       //  VkImageLayout initialLayout;
    };

    return imageCreateInfo;
}

de::MovePtr<StdVideoH264SequenceParameterSet> getStdVideoH264DecodeSequenceParameterSet(
    uint32_t width, uint32_t height, StdVideoH264SequenceParameterSetVui *stdVideoH264SequenceParameterSetVui)
{
    const StdVideoH264SpsFlags stdVideoH264SpsFlags = {
        0u, //  uint32_t constraint_set0_flag:1;
        0u, //  uint32_t constraint_set1_flag:1;
        0u, //  uint32_t constraint_set2_flag:1;
        0u, //  uint32_t constraint_set3_flag:1;
        0u, //  uint32_t constraint_set4_flag:1;
        0u, //  uint32_t constraint_set5_flag:1;
        1u, //  uint32_t direct_8x8_inference_flag:1;
        0u, //  uint32_t mb_adaptive_frame_field_flag:1;
        1u, //  uint32_t frame_mbs_only_flag:1;
        0u, //  uint32_t delta_pic_order_always_zero_flag:1;
        0u, //  uint32_t separate_colour_plane_flag:1;
        0u, //  uint32_t gaps_in_frame_num_value_allowed_flag:1;
        0u, //  uint32_t qpprime_y_zero_transform_bypass_flag:1;
        0u, //  uint32_t frame_cropping_flag:1;
        0u, //  uint32_t seq_scaling_matrix_present_flag:1;
        0u, //  uint32_t vui_parameters_present_flag:1;
    };

    const StdVideoH264SequenceParameterSet stdVideoH264SequenceParameterSet = {
        stdVideoH264SpsFlags,                 //  StdVideoH264SpsFlags flags;
        STD_VIDEO_H264_PROFILE_IDC_BASELINE,  //  StdVideoH264ProfileIdc profile_idc;
        STD_VIDEO_H264_LEVEL_IDC_4_1,         //  StdVideoH264Level level_idc;
        STD_VIDEO_H264_CHROMA_FORMAT_IDC_420, //  StdVideoH264ChromaFormatIdc chroma_format_idc;
        0u,                                   //  uint8_t seq_parameter_set_id;
        0u,                                   //  uint8_t bit_depth_luma_minus8;
        0u,                                   //  uint8_t bit_depth_chroma_minus8;
        0u,                                   //  uint8_t log2_max_frame_num_minus4;
        STD_VIDEO_H264_POC_TYPE_2,            //  StdVideoH264PocType pic_order_cnt_type;
        0,                                    //  int32_t offset_for_non_ref_pic;
        0,                                    //  int32_t offset_for_top_to_bottom_field;
        0u,                                   //  uint8_t log2_max_pic_order_cnt_lsb_minus4;
        0u,                                   //  uint8_t num_ref_frames_in_pic_order_cnt_cycle;
        3u,                                   //  uint8_t max_num_ref_frames;
        0u,                                   //  uint8_t reserved1;
        (width + 15) / 16 - 1,                //  uint32_t pic_width_in_mbs_minus1;
        (height + 15) / 16 - 1,               //  uint32_t pic_height_in_map_units_minus1;
        0u,                                   //  uint32_t frame_crop_left_offset;
        0u,                                   //  uint32_t frame_crop_right_offset;
        0u,                                   //  uint32_t frame_crop_top_offset;
        0u,                                   //  uint32_t frame_crop_bottom_offset;
        0u,                                   //  uint32_t reserved2;
        nullptr,                              //  const int32_t* pOffsetForRefFrame;
        nullptr,                              //  const StdVideoH264ScalingLists* pScalingLists;
        stdVideoH264SequenceParameterSetVui,  //  const StdVideoH264SequenceParameterSetVui* pSequenceParameterSetVui;
    };

    return de::MovePtr<StdVideoH264SequenceParameterSet>(
        new StdVideoH264SequenceParameterSet(stdVideoH264SequenceParameterSet));
}

de::MovePtr<StdVideoH264SequenceParameterSet> getStdVideoH264EncodeSequenceParameterSet(
    uint32_t width, uint32_t height, uint8_t maxNumRefs,
    StdVideoH264SequenceParameterSetVui *stdVideoH264SequenceParameterSetVui)
{
    const StdVideoH264SpsFlags stdVideoH264SpsFlags = {
        0u, //  uint32_t constraint_set0_flag:1;
        0u, //  uint32_t constraint_set1_flag:1;
        0u, //  uint32_t constraint_set2_flag:1;
        0u, //  uint32_t constraint_set3_flag:1;
        0u, //  uint32_t constraint_set4_flag:1;
        0u, //  uint32_t constraint_set5_flag:1;
        1u, //  uint32_t direct_8x8_inference_flag:1;
        0u, //  uint32_t mb_adaptive_frame_field_flag:1;
        1u, //  uint32_t frame_mbs_only_flag:1;
        0u, //  uint32_t delta_pic_order_always_zero_flag:1;
        0u, //  uint32_t separate_colour_plane_flag:1;
        0u, //  uint32_t gaps_in_frame_num_value_allowed_flag:1;
        0u, //  uint32_t qpprime_y_zero_transform_bypass_flag:1;
        0u, //  uint32_t frame_cropping_flag:1;
        0u, //  uint32_t seq_scaling_matrix_present_flag:1;
        0u, //  uint32_t vui_parameters_present_flag:1;
    };

    const StdVideoH264SequenceParameterSet stdVideoH264SequenceParameterSet = {
        stdVideoH264SpsFlags,            //  StdVideoH264SpsFlags flags;
        STD_VIDEO_H264_PROFILE_IDC_MAIN, //  StdVideoH264ProfileIdc profile_idc;
        // ResourceError (videoDeviceDriver.getEncodedVideoSessionParametersKHR(videoDevice, &videoEncodeSessionParametersGetInfo, &videoEncodeSessionParametersFeedbackInfo, &bitstreamBufferOffset, nullptr): VK_ERROR_OUT_OF_HOST_MEMORY at vktVideoEncodeTests.cpp:1386)
        //STD_VIDEO_H264_PROFILE_IDC_MAIN, //  StdVideoH264ProfileIdc profile_idc;
        STD_VIDEO_H264_LEVEL_IDC_4_1, //  StdVideoH264Level level_idc;
        //STD_VIDEO_H264_LEVEL_IDC_1_1, //  StdVideoH264Level level_idc;
        STD_VIDEO_H264_CHROMA_FORMAT_IDC_420,                //  StdVideoH264ChromaFormatIdc chroma_format_idc;
        0u,                                                  //  uint8_t seq_parameter_set_id;
        0u,                                                  //  uint8_t bit_depth_luma_minus8;
        0u,                                                  //  uint8_t bit_depth_chroma_minus8;
        0u,                                                  //  uint8_t log2_max_frame_num_minus4;
        STD_VIDEO_H264_POC_TYPE_0,                           //  StdVideoH264PocType pic_order_cnt_type;
        0,                                                   //  int32_t offset_for_non_ref_pic;
        0,                                                   //  int32_t offset_for_top_to_bottom_field;
        4u,                                                  //  uint8_t log2_max_pic_order_cnt_lsb_minus4;
        0u,                                                  //  uint8_t num_ref_frames_in_pic_order_cnt_cycle;
        maxNumRefs,                                          //  uint8_t max_num_ref_frames;
        0u,                                                  //  uint8_t reserved1;
        static_cast<uint32_t>(std::ceil(width / 16.0) - 1),  //  uint32_t pic_width_in_mbs_minus1;
        static_cast<uint32_t>(std::ceil(height / 16.0) - 1), //  uint32_t pic_height_in_map_units_minus1;
        0u,                                                  //  uint32_t frame_crop_left_offset;
        0u,                                                  //  uint32_t frame_crop_right_offset;
        0u,                                                  //  uint32_t frame_crop_top_offset;
        0u,                                                  //  uint32_t frame_crop_bottom_offset;
        0u,                                                  //  uint32_t reserved2;
        nullptr,                                             //  const int32_t* pOffsetForRefFrame;
        nullptr,                                             //  const StdVideoH264ScalingLists* pScalingLists;
        stdVideoH264SequenceParameterSetVui, //  const StdVideoH264SequenceParameterSetVui* pSequenceParameterSetVui;
    };

    return de::MovePtr<StdVideoH264SequenceParameterSet>(
        new StdVideoH264SequenceParameterSet(stdVideoH264SequenceParameterSet));
}

de::MovePtr<StdVideoH264PictureParameterSet> getStdVideoH264EncodePictureParameterSet(uint8_t numL0, uint8_t numL1)
{
    const StdVideoH264PpsFlags stdVideoH264PpsFlags = {
        0u, //  uint32_t transform_8x8_mode_flag:1;
        0u, //  uint32_t redundant_pic_cnt_present_flag:1;
        0u, //  uint32_t constrained_intra_pred_flag:1;
        1u, //  uint32_t deblocking_filter_control_present_flag:1;
        0u, //  uint32_t weighted_pred_flag:1;
        0u, //  uint32_4 bottom_field_pic_order_in_frame_present_flag:1;
        1u, //  uint32_t entropy_coding_mode_flag:1;
        0u, //  uint32_t pic_scaling_matrix_present_flag;
    };

    const StdVideoH264PictureParameterSet stdVideoH264PictureParameterSet = {
        stdVideoH264PpsFlags,                        //  StdVideoH264PpsFlags flags;
        0u,                                          //  uint8_t seq_parameter_set_id;
        0u,                                          //  uint8_t pic_parameter_set_id;
        static_cast<uint8_t>(numL0 ? numL0 - 1 : 0), //  uint8_t num_ref_idx_l0_default_active_minus1;
        static_cast<uint8_t>(numL1 ? numL1 - 1 : 0), //  uint8_t num_ref_idx_l1_default_active_minus1;
        STD_VIDEO_H264_WEIGHTED_BIPRED_IDC_DEFAULT,  //  StdVideoH264WeightedBipredIdc weighted_bipred_idc;
        0,                                           //  int8_t pic_init_qp_minus26;
        0,                                           //  int8_t pic_init_qs_minus26;
        0,                                           //  int8_t chroma_qp_index_offset;
        0,                                           //  int8_t second_chroma_qp_index_offset;
        nullptr,                                     //  const StdVideoH264ScalingLists* pScalingLists;
    };

    return de::MovePtr<StdVideoH264PictureParameterSet>(
        new StdVideoH264PictureParameterSet(stdVideoH264PictureParameterSet));
}

de::MovePtr<VkVideoEncodeH264SessionParametersAddInfoKHR> createVideoEncodeH264SessionParametersAddInfoKHR(
    uint32_t stdSPSCount, const StdVideoH264SequenceParameterSet *pStdSPSs, uint32_t stdPPSCount,
    const StdVideoH264PictureParameterSet *pStdPPSs)
{
    VkVideoEncodeH264SessionParametersAddInfoKHR videoEncodeH264SessionParametersAddInfoKHR = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_ADD_INFO_KHR, //  VkStructureType                            sType
        nullptr,     //  const void*                                pNext
        stdSPSCount, //  uint32_t                                stdSPSCount
        pStdSPSs,    //  const StdVideoH264SequenceParameterSet*    pStdSPSs
        stdPPSCount, //  uint32_t                                stdPPSCount
        pStdPPSs     //  const StdVideoH264PictureParameterSet*    pStdPPSs
    };

    return de::MovePtr<VkVideoEncodeH264SessionParametersAddInfoKHR>(
        new VkVideoEncodeH264SessionParametersAddInfoKHR(videoEncodeH264SessionParametersAddInfoKHR));
}

de::MovePtr<VkVideoEncodeH264SessionParametersCreateInfoKHR> createVideoEncodeH264SessionParametersCreateInfoKHR(
    const void *pNext, uint32_t maxStdSPSCount, uint32_t maxStdPPSCount,
    const VkVideoEncodeH264SessionParametersAddInfoKHR *pParametersAddInfo)
{
    VkVideoEncodeH264SessionParametersCreateInfoKHR videoEncodeH264SessionParametersCreateInfoKHR = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_CREATE_INFO_KHR, //  VkStructureType                                        sType
        pNext,             //  const void*                                            pNext
        maxStdSPSCount,    //  uint32_t                                            maxStdSPSCount
        maxStdPPSCount,    //  uint32_t                                            maxStdPPSCount
        pParametersAddInfo //  const VkVideoEncodeH264SessionParametersAddInfoKHR*    pParametersAddInfo
    };

    return de::MovePtr<VkVideoEncodeH264SessionParametersCreateInfoKHR>(
        new VkVideoEncodeH264SessionParametersCreateInfoKHR(videoEncodeH264SessionParametersCreateInfoKHR));
}

de::MovePtr<StdVideoH265ProfileTierLevel> getStdVideoH265ProfileTierLevel(StdVideoH265ProfileIdc general_profile_idc,
                                                                          StdVideoH265LevelIdc general_level_idc)
{
    const StdVideoH265ProfileTierLevelFlags stdVideoH265ProfileTierLevelFlags = {
        0, // general_tier_flag : 1;
        1, // general_progressive_source_flag : 1;
        0, // general_interlaced_source_flag : 1;
        0, // general_non_packed_constraint_flag : 1;
        1, // general_frame_only_constraint_flag : 1;
    };

    const StdVideoH265ProfileTierLevel stdVideoH265ProfileTierLevelInstance = {
        stdVideoH265ProfileTierLevelFlags, // StdVideoH265ProfileTierLevelFlags flags;
        general_profile_idc,               // StdVideoH265ProfileIdc general_profile_idc;
        general_level_idc,                 // StdVideoH265LevelIdc general_level_idc;
    };

    return de::MovePtr<StdVideoH265ProfileTierLevel>(
        new StdVideoH265ProfileTierLevel(stdVideoH265ProfileTierLevelInstance));
}

de::MovePtr<StdVideoH265DecPicBufMgr> getStdVideoH265DecPicBufMgr()
{
    const StdVideoH265DecPicBufMgr stdVideoH265DecPicBufMgrInstance = {
        {5}, // max_latency_increase_plus1[STD_VIDEO_H265_SUBLAYERS_LIST_SIZE];
        {4}, // max_dec_pic_buffering_minus1[STD_VIDEO_H265_SUBLAYERS_LIST_SIZE];
        {2}, // max_num_reorder_pics[STD_VIDEO_H265_SUBLAYERS_LIST_SIZE];
    };

    return de::MovePtr<StdVideoH265DecPicBufMgr>(new StdVideoH265DecPicBufMgr(stdVideoH265DecPicBufMgrInstance));
}

de::MovePtr<StdVideoH265VideoParameterSet> getStdVideoH265VideoParameterSet(
    const StdVideoH265DecPicBufMgr *pDecPicBufMgr, const StdVideoH265ProfileTierLevel *pProfileTierLevel)
{
    const StdVideoH265VpsFlags stdVideoH265VpsFlags = {
        1, //  vps_temporal_id_nesting_flag : 1;
        1, //  vps_sub_layer_ordering_info_present_flag : 1;
        0, //  vps_timing_info_present_flag : 1;
        0  //  vps_poc_proportional_to_timing_flag : 1;
    };

    const StdVideoH265VideoParameterSet stdVideoH265VideoParameterSet = {
        stdVideoH265VpsFlags, //  StdVideoH265VpsFlags flags;
        0u,                   //  uint8_t vps_video_parameter_set_id;
        0u,                   //  uint8_t vps_max_sub_layers_minus1;
        0u,                   //  uint8_t reserved1;
        0u,                   //  uint8_t reserved2;
        0u,                   //  uint32_t vps_num_units_in_tick;
        0u,                   //  uint32_t vps_time_scale;
        0u,                   //  uint32_t vps_num_ticks_poc_diff_one_minus1;
        0u,                   //  uint32_t reserved3;
        pDecPicBufMgr,        //  const StdVideoH265DecPicBufMgr* pDecPicBufMgr;
        nullptr,              //  const StdVideoH265HrdParameters* pHrdParameters;
        pProfileTierLevel,    //  const StdVideoH265ProfileTierLevel* pProfileTierLevel;
    };

    return de::MovePtr<StdVideoH265VideoParameterSet>(new StdVideoH265VideoParameterSet(stdVideoH265VideoParameterSet));
}

de::MovePtr<StdVideoH265ShortTermRefPicSet> getStdVideoH265ShortTermRefPicSet(StdVideoH265PictureType pictureType,
                                                                              uint32_t frameIdx,
                                                                              uint32_t consecutiveBFrameCount)
{
    struct StdVideoH265ShortTermRefPicSet strps = {
        StdVideoH265ShortTermRefPicSetFlags(), //  StdVideoH265ShortTermRefPicSetFlags flags;
        0,                                     //  uint32_t delta_idx_minus1;
        0,                                     //  uint16_t use_delta_flag;
        0,                                     //  uint16_t abs_delta_rps_minus1;
        0,                                     //  uint16_t used_by_curr_pic_flag;
        1,                                     //  uint16_t used_by_curr_pic_s0_flag;
        0,                                     //  uint16_t used_by_curr_pic_s1_flag;
        0,                                     //  uint16_t reserved1;
        0,                                     //  uint8_t reserved2;
        0,                                     //  uint8_t reserved3;
        0,                                     //  uint8_t num_negative_pics;
        0,                                     //  uint8_t num_positive_pics;
        {0},                                   //  uint16_t delta_poc_s0_minus1[STD_VIDEO_H265_MAX_DPB_SIZE];
        {0},                                   //  uint16_t delta_poc_s1_minus1[STD_VIDEO_H265_MAX_DPB_SIZE];
    };

    uint32_t frameIdxMod = frameIdx % (consecutiveBFrameCount + 1);

    switch (pictureType)
    {
    case STD_VIDEO_H265_PICTURE_TYPE_P:
        strps.num_negative_pics = 1;
        // For where frameIdx == 3, 6, 9, 12 in the h265.i_p_b_13 test, need to set 2.
        if (consecutiveBFrameCount)
            strps.delta_poc_s0_minus1[0] = (frameIdxMod == 0) ? 2 : 0;
        break;

    case STD_VIDEO_H265_PICTURE_TYPE_B:
        strps.used_by_curr_pic_s1_flag = 1;
        strps.num_negative_pics        = 1;
        strps.num_positive_pics        = 1;
        strps.delta_poc_s1_minus1[0]   = (frameIdxMod == 1) ? 1 : 0;
        strps.delta_poc_s0_minus1[0]   = (frameIdxMod == 2) ? 1 : 0;
        break;

    default:
        // explicitly ignore other variants
        break;
    }

    DE_UNREF(pictureType);

    return de::MovePtr<StdVideoH265ShortTermRefPicSet>(new StdVideoH265ShortTermRefPicSet(strps));
}

de::MovePtr<StdVideoH265SequenceParameterSetVui> getStdVideoH265SequenceParameterSetVui(uint32_t vui_time_scale)
{
    const StdVideoH265SpsVuiFlags stdVideoH265SpsVuiFlags = {
        0, //  aspect_ratio_info_present_flag : 1;
        0, //  overscan_info_present_flag : 1;
        0, //  overscan_appropriate_flag : 1;
        1, //  video_signal_type_present_flag : 1;
        0, //  video_full_range_flag : 1;
        0, //  colour_description_present_flag : 1;
        0, //  chroma_loc_info_present_flag : 1;
        0, //  neutral_chroma_indication_flag : 1;
        0, //  field_seq_flag : 1;
        0, //  frame_field_info_present_flag : 1;
        0, //  default_display_window_flag : 1;
        1, //  vui_timing_info_present_flag : 1;
        0, //  vui_poc_proportional_to_timing_flag : 1;
        0, //  vui_hrd_parameters_present_flag : 1;
        0, //  bitstream_restriction_flag : 1;
        0, //  tiles_fixed_structure_flag : 1;
        0, //  motion_vectors_over_pic_boundaries_flag : 1;
        0  //  restricted_ref_pic_lists_flag : 1;
    };

    const StdVideoH265SequenceParameterSetVui stdVideoH265SequenceParameterSetVui = {
        stdVideoH265SpsVuiFlags,                     // flags;
        STD_VIDEO_H265_ASPECT_RATIO_IDC_UNSPECIFIED, // aspect_ratio_idc;
        0,                                           //  sar_width;
        0,                                           //  sar_height;
        1,                                           //  video_format;
        0,                                           //  colour_primaries;
        0,                                           //  transfer_characteristics;
        0,                                           //  matrix_coeffs;
        0,                                           //  chroma_sample_loc_type_top_field;
        0,                                           //  chroma_sample_loc_type_bottom_field;
        0,                                           //  reserved1;
        0,                                           //  reserved2;
        0,                                           //  def_disp_win_left_offset;
        0,                                           //  def_disp_win_right_offset;
        0,                                           //  def_disp_win_top_offset;
        0,                                           //  def_disp_win_bottom_offset;
        1,                                           //  vui_num_units_in_tick;
        vui_time_scale,                              //  vui_time_scale;
        0,                                           //  vui_num_ticks_poc_diff_one_minus1;
        0,                                           //  min_spatial_segmentation_idc;
        0,                                           //  reserved3;
        0,                                           //  max_bytes_per_pic_denom;
        0,                                           //  max_bits_per_min_cu_denom;
        0,                                           //  log2_max_mv_length_horizontal;
        0,                                           //  log2_max_mv_length_vertical;
        0,                                           //  pHrdParameters;
    };

    return de::MovePtr<StdVideoH265SequenceParameterSetVui>(
        new StdVideoH265SequenceParameterSetVui(stdVideoH265SequenceParameterSetVui));
}

de::MovePtr<StdVideoH265SequenceParameterSet> getStdVideoH265SequenceParameterSet(
    uint32_t width, uint32_t height, VkVideoEncodeH265CtbSizeFlagsKHR ctbSizesFlag,
    VkVideoEncodeH265TransformBlockSizeFlagsKHR transformBlockSizesFlag, const StdVideoH265DecPicBufMgr *pDecPicBufMgr,
    const StdVideoH265ProfileTierLevel *pProfileTierLevel,
    const StdVideoH265SequenceParameterSetVui *pSequenceParameterSetVui)
{
    const StdVideoH265SpsFlags stdVideoH265SpsFlags = {
        1, //  sps_temporal_id_nesting_flag : 1;
        0, //  separate_colour_plane_flag : 1;
        1, //  conformance_window_flag : 1;
        1, //  sps_sub_layer_ordering_info_present_flag : 1;
        0, //  scaling_list_enabled_flag : 1;
        0, //  sps_scaling_list_data_present_flag : 1;
        0, //  amp_enabled_flag : 1;
        1, //  sample_adaptive_offset_enabled_flag : 1;
        0, //  pcm_enabled_flag : 1;
        0, //  pcm_loop_filter_disabled_flag : 1;
        0, //  long_term_ref_pics_present_flag : 1;
        1, //  sps_temporal_mvp_enabled_flag : 1;
        1, //  strong_intra_smoothing_enabled_flag : 1;
        1, //  vui_parameters_present_flag : 1;
        0, //  sps_extension_present_flag : 1;
        0, //  sps_range_extension_flag : 1;
        0, //  transform_skip_rotation_enabled_flag : 1;
        0, //  transform_skip_context_enabled_flag : 1;
        0, //  implicit_rdpcm_enabled_flag : 1;
        0, //  explicit_rdpcm_enabled_flag : 1;
        0, //  extended_precision_processing_flag : 1;
        0, //  intra_smoothing_disabled_flag : 1;
        0, //  high_precision_offsets_enabled_flag : 1;
        0, //  persistent_rice_adaptation_enabled_flag : 1;
        0, //  cabac_bypass_alignment_enabled_flag : 1;
        0, //  sps_scc_extension_flag : 1;
        0, //  sps_curr_pic_ref_enabled_flag : 1;
        0, //  palette_mode_enabled_flag : 1;
        0, //  sps_palette_predictor_initializers_present_flag : 1;
        0  //  intra_boundary_filtering_disabled_flag : 1;
    };

    int max_ctb_size = 16;
    int min_ctb_size = 64;

    if (ctbSizesFlag & VK_VIDEO_ENCODE_H265_CTB_SIZE_64_BIT_KHR)
    {
        max_ctb_size = 64;
    }
    else if (ctbSizesFlag & VK_VIDEO_ENCODE_H265_CTB_SIZE_32_BIT_KHR)
    {
        max_ctb_size = 32;
    }

    if (ctbSizesFlag & VK_VIDEO_ENCODE_H265_CTB_SIZE_16_BIT_KHR)
    {
        min_ctb_size = 16;
    }
    else if (ctbSizesFlag & VK_VIDEO_ENCODE_H265_CTB_SIZE_32_BIT_KHR)
    {
        min_ctb_size = 32;
    }

    //DE_UNREF(min_ctb_size);

    int min_tb_size = 0;
    int max_tb_size = 0;

    if (transformBlockSizesFlag & VK_VIDEO_ENCODE_H265_TRANSFORM_BLOCK_SIZE_4_BIT_KHR)
        min_tb_size = 4;
    else if (transformBlockSizesFlag & VK_VIDEO_ENCODE_H265_TRANSFORM_BLOCK_SIZE_8_BIT_KHR)
        min_tb_size = 8;
    else if (transformBlockSizesFlag & VK_VIDEO_ENCODE_H265_TRANSFORM_BLOCK_SIZE_16_BIT_KHR)
        min_tb_size = 16;
    else if (transformBlockSizesFlag & VK_VIDEO_ENCODE_H265_TRANSFORM_BLOCK_SIZE_32_BIT_KHR)
        min_tb_size = 32;

    if (transformBlockSizesFlag & VK_VIDEO_ENCODE_H265_TRANSFORM_BLOCK_SIZE_32_BIT_KHR)
        max_tb_size = 32;
    else if (transformBlockSizesFlag & VK_VIDEO_ENCODE_H265_TRANSFORM_BLOCK_SIZE_16_BIT_KHR)
        max_tb_size = 16;
    else if (transformBlockSizesFlag & VK_VIDEO_ENCODE_H265_TRANSFORM_BLOCK_SIZE_8_BIT_KHR)
        max_tb_size = 8;
    else if (transformBlockSizesFlag & VK_VIDEO_ENCODE_H265_TRANSFORM_BLOCK_SIZE_4_BIT_KHR)
        max_tb_size = 4;

    uint8_t log2_min_luma_coding_block_size_minus3   = 0; // 8x8 cb is smallest
    uint8_t log2_diff_max_min_luma_coding_block_size = static_cast<uint8_t>(std::log2(max_ctb_size) - 3);

    uint8_t log2_min_luma_transform_block_size_minus2 = static_cast<uint8_t>(std::log2(min_tb_size) - 2);
    uint8_t log2_diff_max_min_luma_transform_block_size =
        static_cast<uint8_t>(std::log2(max_tb_size) - std::log2(min_tb_size));

    uint8_t max_transform_hierarchy = static_cast<uint8_t>(std::log2(max_ctb_size) - std::log2(min_tb_size));

    uint32_t pic_width_in_luma_samples =
        static_cast<uint32_t>((std::ceil(static_cast<double>(width) / min_ctb_size)) * min_ctb_size);
    uint32_t pic_height_in_luma_samples =
        static_cast<uint32_t>((std::ceil(static_cast<double>(height) / min_ctb_size)) * min_ctb_size);

    uint32_t conf_win_left_offset   = 0;
    uint32_t conf_win_right_offset  = (pic_width_in_luma_samples - width) / 2;
    uint32_t conf_win_top_offset    = 0;
    uint32_t conf_win_bottom_offset = (pic_height_in_luma_samples - height) / 2;

    const StdVideoH265SequenceParameterSet stdVideoH265SequenceParameterSet = {
        stdVideoH265SpsFlags,                 //  StdVideoH265SpsFlags flags;
        STD_VIDEO_H265_CHROMA_FORMAT_IDC_420, //  StdVideoH265ChromaFormatIdc chroma_format_idc;
        pic_width_in_luma_samples,            //  uint32_t pic_width_in_luma_samples;
        pic_height_in_luma_samples,           //  uint32_t pic_height_in_luma_samples;
        0u,                                   //  uint8_t sps_video_parameter_set_id;
        0u,                                   //  uint8_t sps_max_sub_layers_minus1;
        0u,                                   //  uint8_t sps_seq_parameter_set_id;
        0u,                                   //  uint8_t bit_depth_luma_minus8;
        0u,                                   //  uint8_t bit_depth_chroma_minus8;
        4u, //  uint8_t                                log2_max_pic_order_cnt_lsb_minus;4
        log2_min_luma_coding_block_size_minus3,      //  uint8_t log2_min_luma_coding_block_size_minus3;
        log2_diff_max_min_luma_coding_block_size,    //  uint8_t log2_diff_max_min_luma_coding_block_size;
        log2_min_luma_transform_block_size_minus2,   //  uint8_t log2_min_luma_transform_block_size_minus2;
        log2_diff_max_min_luma_transform_block_size, //  uint8_t log2_diff_max_min_luma_transform_block_size;
        max_transform_hierarchy,                     //  uint8_t max_transform_hierarchy_depth_inter;
        max_transform_hierarchy,                     //  uint8_t max_transform_hierarchy_depth_intra;
        0u,                                          //  uint8_t num_short_term_ref_pic_sets;
        0u,                                          //  uint8_t num_long_term_ref_pics_sps;
        0u,                                          //  uint8_t pcm_sample_bit_depth_luma_minus1;
        0u,                                          //  uint8_t pcm_sample_bit_depth_chroma_minus1;
        0u,                                          //  uint8_t log2_min_pcm_luma_coding_block_size_minus3;
        0u,                                          //  uint8_t log2_diff_max_min_pcm_luma_coding_block_size;
        0u,                                          //  uint8_t reserved1;
        0u,                                          //  uint8_t reserved2;
        0u,                                          //  uint8_t palette_max_size;
        0u,                                          //  uint8_t delta_palette_max_predictor_size;
        0u,                                          //  uint8_t motion_vector_resolution_control_idc;
        0u,                                          //  uint8_t sps_num_palette_predictor_initializers_minus1;
        conf_win_left_offset,                        //  uint32_t conf_win_left_offset;
        conf_win_right_offset,                       //  uint32_t conf_win_right_offset;
        conf_win_top_offset,                         //  uint32_t conf_win_top_offset;
        conf_win_bottom_offset,                      //  uint32_t conf_win_bottom_offset;
        pProfileTierLevel,                           //  const StdVideoH265ProfileTierLevel* pProfileTierLevel;
        pDecPicBufMgr,                               //  const StdVideoH265DecPicBufMgr* pDecPicBufMgr;
        nullptr,                                     //  const StdVideoH265ScalingLists* pScalingLists;
        nullptr,                                     //  const StdVideoH265ShortTermRefPicSet* pShortTermRefPicSet;
        nullptr,                                     //  const StdVideoH265LongTermRefPicsSps* pLongTermRefPicsSps;
        pSequenceParameterSetVui, //  const StdVideoH265SequenceParameterSetVui* pSequenceParameterSetVui;
        nullptr,                  //  const StdVideoH265PredictorPaletteEntries* pPredictorPaletteEntries;
    };

    return de::MovePtr<StdVideoH265SequenceParameterSet>(
        new StdVideoH265SequenceParameterSet(stdVideoH265SequenceParameterSet));
}

de::MovePtr<StdVideoH265PictureParameterSet> getStdVideoH265PictureParameterSet(
    const VkVideoEncodeH265CapabilitiesKHR *videoH265CapabilitiesExtension)
{
    uint32_t weighted_pred_flag =
        (videoH265CapabilitiesExtension->stdSyntaxFlags & VK_VIDEO_ENCODE_H265_STD_WEIGHTED_PRED_FLAG_SET_BIT_KHR) ? 1 :
                                                                                                                     0;
    uint32_t transform_skip_enabled_flag = (videoH265CapabilitiesExtension->stdSyntaxFlags &
                                            VK_VIDEO_ENCODE_H265_STD_TRANSFORM_SKIP_ENABLED_FLAG_SET_BIT_KHR) ?
                                               1 :
                                               0;
    uint32_t entropy_coding_sync_enabled_flag =
        (videoH265CapabilitiesExtension->maxTiles.width > 1 || videoH265CapabilitiesExtension->maxTiles.height > 1) ?
            1 :
            0;

    const StdVideoH265PpsFlags stdVideoH265PpsFlags = {
        0,                                //  dependent_slice_segments_enabled_flag : 1;
        0,                                //  output_flag_present_flag : 1;
        0,                                //  sign_data_hiding_enabled_flag : 1;
        0,                                //  cabac_init_present_flag : 1;
        0,                                //  constrained_intra_pred_flag : 1;
        transform_skip_enabled_flag,      //  transform_skip_enabled_flag : 1;
        1,                                //  cu_qp_delta_enabled_flag : 1;
        0,                                //  pps_slice_chroma_qp_offsets_present_flag : 1;
        weighted_pred_flag,               //  weighted_pred_flag : 1;
        0,                                //  weighted_bipred_flag : 1;
        0,                                //  transquant_bypass_enabled_flag : 1;
        0,                                //  tiles_enabled_flag : 1;
        entropy_coding_sync_enabled_flag, //  entropy_coding_sync_enabled_flag : 1;
        0,                                //  uniform_spacing_flag : 1;
        0,                                //  loop_filter_across_tiles_enabled_flag : 1;
        1,                                //  pps_loop_filter_across_slices_enabled_flag : 1;
        0,                                //  deblocking_filter_control_present_flag : 1;
        0,                                //  deblocking_filter_override_enabled_flag : 1;
        0,                                //  pps_deblocking_filter_disabled_flag : 1;
        0,                                //  pps_scaling_list_data_present_flag : 1;
        0,                                //  lists_modification_present_flag : 1;
        0,                                //  slice_segment_header_extension_present_flag : 1;
        0,                                //  pps_extension_present_flag : 1;
        0,                                //  cross_component_prediction_enabled_flag : 1;
        0,                                //  chroma_qp_offset_list_enabled_flag : 1;
        0,                                //  pps_curr_pic_ref_enabled_flag : 1;
        0,                                //  residual_adaptive_colour_transform_enabled_flag : 1;
        0,                                //  pps_slice_act_qp_offsets_present_flag : 1;
        0,                                //  pps_palette_predictor_initializers_present_flag : 1;
        0,                                //  monochrome_palette_flag : 1;
        0,                                //  pps_range_extension_flag : 1;
    };

    const StdVideoH265PictureParameterSet stdVideoH265PictureParameterSet = {
        stdVideoH265PpsFlags, //  StdVideoH265PpsFlags flags;
        0u,                   //  uint8_t pps_pic_parameter_set_id;
        0u,                   //  uint8_t pps_seq_parameter_set_id;
        0u,                   //  uint8_t sps_video_parameter_set_id;
        0u,                   //  uint8_t num_extra_slice_header_bits;
        0u,                   //  uint8_t num_ref_idx_l0_default_active_minus1;
        0u,                   //  uint8_t num_ref_idx_l1_default_active_minus1;
        0,                    //  int8_t init_qp_minus26;
        1u,                   //  uint8_t diff_cu_qp_delta_depth;
        0,                    //  int8_t pps_cb_qp_offset;
        0,                    //  int8_t pps_cr_qp_offset;
        0,                    //  int8_t pps_beta_offset_div2;
        0,                    //  int8_t pps_tc_offset_div2;
        0u,                   //  uint8_t log2_parallel_merge_level_minus2;
        0u,                   //  uint8_t log2_max_transform_skip_block_size_minus2;
        0u,                   //  uint8_t diff_cu_chroma_qp_offset_depth;
        0u,                   //  uint8_t chroma_qp_offset_list_len_minus1;
        {},                   //  int8_t cb_qp_offset_list[STD_VIDEO_H265_CHROMA_QP_OFFSET_LIST_SIZE];
        {},                   //  int8_t cr_qp_offset_list[STD_VIDEO_H265_CHROMA_QP_OFFSET_LIST_SIZE];
        0u,                   //  uint8_t log2_sao_offset_scale_luma;
        0u,                   //  uint8_t log2_sao_offset_scale_chroma;
        0,                    //  int8_t pps_act_y_qp_offset_plus5;
        0,                    //  int8_t pps_act_cb_qp_offset_plus5;
        0,                    //  int8_t pps_act_cr_qp_offset_plus3;
        0u,                   //  uint8_t pps_num_palette_predictor_initializers;
        0u,                   //  uint8_t luma_bit_depth_entry_minus8;
        0u,                   //  uint8_t chroma_bit_depth_entry_minus8;
        0u,                   //  uint8_t num_tile_columns_minus1;
        0u,                   //  uint8_t num_tile_rows_minus1;
        0u,                   //  uint8_t reserved1;
        0u,                   //  uint8_t reserved2;
        {},                   //  uint16_t column_width_minus1[STD_VIDEO_H265_CHROMA_QP_OFFSET_TILE_COLS_LIST_SIZE];
        {},                   //  uint16_t row_height_minus1[STD_VIDEO_H265_CHROMA_QP_OFFSET_TILE_ROWS_LIST_SIZE];
        0u,                   //  uint32_t reserved3;
        nullptr,              //  const StdVideoH265ScalingLists* pScalingLists;
        nullptr,              //  const StdVideoH265PredictorPaletteEntries* pPredictorPaletteEntries;
    };

    return de::MovePtr<StdVideoH265PictureParameterSet>(
        new StdVideoH265PictureParameterSet(stdVideoH265PictureParameterSet));
}

de::MovePtr<VkVideoEncodeH265SessionParametersAddInfoKHR> getVideoEncodeH265SessionParametersAddInfoKHR(
    uint32_t stdVPSCount, const StdVideoH265VideoParameterSet *pStdVPSs, uint32_t stdSPSCount,
    const StdVideoH265SequenceParameterSet *pStdSPSs, uint32_t stdPPSCount,
    const StdVideoH265PictureParameterSet *pStdPPSs)
{
    VkVideoEncodeH265SessionParametersAddInfoKHR encodeH265SessionParametersAddInfoKHR = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_SESSION_PARAMETERS_ADD_INFO_KHR, //  VkStructureType sType;
        nullptr,                                                             //  const void* pNext;
        stdVPSCount,                                                         //  uint32_t stdVPSCount;
        pStdVPSs,    //  const StdVideoH265VideoParameterSet* pStdVPSs;
        stdSPSCount, //  uint32_t stdSPSCount;
        pStdSPSs,    //  const StdVideoH265SequenceParameterSet* pStdSPSs;
        stdPPSCount, //  uint32_t stdPPSCount;
        pStdPPSs     //  const StdVideoH265PictureParameterSet* pStdPPSs;
    };

    return de::MovePtr<VkVideoEncodeH265SessionParametersAddInfoKHR>(
        new VkVideoEncodeH265SessionParametersAddInfoKHR(encodeH265SessionParametersAddInfoKHR));
}

de::MovePtr<VkVideoEncodeH265SessionParametersCreateInfoKHR> getVideoEncodeH265SessionParametersCreateInfoKHR(
    const void *pNext, uint32_t maxStdVPSCount, uint32_t maxStdSPSCount, uint32_t maxStdPPSCount,
    const VkVideoEncodeH265SessionParametersAddInfoKHR *pParametersAddInfo)
{
    VkVideoEncodeH265SessionParametersCreateInfoKHR sessionParametersCreateInfoKHR = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_SESSION_PARAMETERS_CREATE_INFO_KHR, //  VkStructureType sType;
        pNext,                                                                  //  const void* pNext;
        maxStdVPSCount,                                                         //  uint32_t maxStdVPSCount;
        maxStdSPSCount,                                                         //  uint32_t maxStdSPSCount;
        maxStdPPSCount,                                                         //  uint32_t maxStdPPSCount;
        pParametersAddInfo //  const VkVideoEncodeH265SessionParametersAddInfoKHR* pParametersAddInfo;
    };

    return de::MovePtr<VkVideoEncodeH265SessionParametersCreateInfoKHR>(
        new VkVideoEncodeH265SessionParametersCreateInfoKHR(sessionParametersCreateInfoKHR));
}

de::MovePtr<VkVideoSessionParametersCreateInfoKHR> getVideoSessionParametersCreateInfoKHR(
    const void *pNext, const VkVideoSessionParametersCreateFlagsKHR flags, VkVideoSessionKHR videoSession)
{
    VkVideoSessionParametersCreateInfoKHR sessionParametersCreateInfo = {
        VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR, //  VkStructureType sType;
        pNext,                                                      //  const void* pNext;
        flags,                                                      //  VkVideoSessionParametersCreateFlagsKHR flags;
        VK_NULL_HANDLE, //  VkVideoSessionParametersKHR videoEncodeSessionParametersTemplate;
        videoSession,   //  VkVideoSessionKHR videoEncodeSession;
    };

    return de::MovePtr<VkVideoSessionParametersCreateInfoKHR>(
        new VkVideoSessionParametersCreateInfoKHR(sessionParametersCreateInfo));
}

de::MovePtr<StdVideoEncodeH264ReferenceInfo> getStdVideoEncodeH264ReferenceInfo(
    StdVideoH264PictureType primary_pic_type, uint32_t FrameNum, int32_t PicOrderCnt)
{
    const StdVideoEncodeH264ReferenceInfoFlags H264referenceInfoFlags = {
        0, //  uint32_t used_for_long_term_reference : 1;
        0, //  uint32_t reserved : 31;
    };

    const StdVideoEncodeH264ReferenceInfo H264referenceInfo = {
        H264referenceInfoFlags, //  StdVideoEncodeH264ReferenceInfoFlags flags;
        primary_pic_type,       //  StdVideoH264PictureType primary_pic_type;
        FrameNum,               //  uint32_t FrameNum;
        PicOrderCnt,            //  int32_t PicOrderCnt;
        0,                      //  uint16_t long_term_pic_num;
        0,                      //  uint16_t long_term_frame_idx;
        0,                      //  uint8_t temporal_id;
    };

    return de::MovePtr<StdVideoEncodeH264ReferenceInfo>(new StdVideoEncodeH264ReferenceInfo(H264referenceInfo));
}

de::MovePtr<VkVideoEncodeH264DpbSlotInfoKHR> getVideoEncodeH264DpbSlotInfo(
    const StdVideoEncodeH264ReferenceInfo *pStdReferenceInfo)
{
    const VkVideoEncodeH264DpbSlotInfoKHR h264DpbSlotInfo = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_DPB_SLOT_INFO_KHR, //  VkStructureType sType;
        nullptr,                                               //  const void* pNext;
        pStdReferenceInfo, //  const StdVideoEncodeH264ReferenceInfo* pStdReferenceInfo;
    };

    return de::MovePtr<VkVideoEncodeH264DpbSlotInfoKHR>(new VkVideoEncodeH264DpbSlotInfoKHR(h264DpbSlotInfo));
}

de::MovePtr<StdVideoEncodeH265ReferenceInfo> getStdVideoEncodeH265ReferenceInfo(StdVideoH265PictureType pic_type,
                                                                                int32_t PicOrderCntVal)
{
    const StdVideoEncodeH265ReferenceInfoFlags H265referenceInfoFlags = {
        0, //  uint32_t used_for_long_term_reference:1;
        0, //  uint32_t unused_for_reference:1;
        0, //  uint32_t reserved:30;
    };

    const StdVideoEncodeH265ReferenceInfo H265referenceInfo = {
        H265referenceInfoFlags, //  StdVideoEncodeH265ReferenceInfoFlags flags;
        pic_type,               //  StdVideoH265PictureType pic_type;
        PicOrderCntVal,         //  int32_t PicOrderCntVal;
        0,                      //  uint8_t TemporalId;
    };

    return de::MovePtr<StdVideoEncodeH265ReferenceInfo>(new StdVideoEncodeH265ReferenceInfo(H265referenceInfo));
}

de::MovePtr<VkVideoEncodeH265DpbSlotInfoKHR> getVideoEncodeH265DpbSlotInfo(
    const StdVideoEncodeH265ReferenceInfo *pStdReferenceInfo)
{
    const VkVideoEncodeH265DpbSlotInfoKHR h265DpbSlotInfo = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_DPB_SLOT_INFO_KHR, //  VkStructureType sType;
        nullptr,                                               //  const void* pNext;
        pStdReferenceInfo, //  const StdVideoEncodeH265ReferenceInfo* pStdReferenceInfo;
    };

    return de::MovePtr<VkVideoEncodeH265DpbSlotInfoKHR>(new VkVideoEncodeH265DpbSlotInfoKHR(h265DpbSlotInfo));
}

de::MovePtr<StdVideoEncodeH264SliceHeader> getStdVideoEncodeH264SliceHeader(StdVideoH264SliceType sliceType,
                                                                            bool activeOverrideFlag)
{
    StdVideoEncodeH264SliceHeaderFlags stdVideoEncodeH264SliceHeaderFlag = {
        0,                  //  uint32_t direct_spatial_mv_pred_flag : 1;
        activeOverrideFlag, //  uint32_t num_ref_idx_active_override_flag : 1;
        0,                  //  uint32_t reserved : 30;
    };

    const StdVideoEncodeH264SliceHeader stdVideoEncodeH264SliceHeader = {
        stdVideoEncodeH264SliceHeaderFlag,                     //  StdVideoEncodeH264SliceHeaderFlags flags;
        0u,                                                    //  uint32_t first_mb_in_slice;
        sliceType,                                             //  StdVideoH264SliceType slice_type;
        0,                                                     //  int8_t slice_alpha_c0_offset_div2;
        0,                                                     //  int8_t slice_beta_offset_div2;
        0,                                                     //  int8_t slice_qp_delta;
        0u,                                                    //  uint16_t reserved1;
        STD_VIDEO_H264_CABAC_INIT_IDC_0,                       //  StdVideoH264CabacInitIdc cabac_init_idc;
        STD_VIDEO_H264_DISABLE_DEBLOCKING_FILTER_IDC_DISABLED, //  StdVideoH264DisableDeblockingFilterIdc disable_deblocking_filter_idc;
        nullptr,                                               //  const StdVideoEncodeH264WeightTable* pWeightTable;
    };

    return de::MovePtr<StdVideoEncodeH264SliceHeader>(new StdVideoEncodeH264SliceHeader(stdVideoEncodeH264SliceHeader));
}

de::MovePtr<StdVideoEncodeH265SliceSegmentHeader> getStdVideoEncodeH265SliceSegmentHeader(
    StdVideoH265SliceType sliceType)
{
    StdVideoEncodeH265SliceSegmentHeaderFlags stdVideoEncodeH265SliceSegmentHeaderFlags = {
        1, //  first_slice_segment_in_pic_flag;
        0, //  dependent_slice_segment_flag;
        1, //  slice_sao_luma_flag;
        1, //  slice_sao_chroma_flag;
        0, //  num_ref_idx_active_override_flag;
        0, //  mvd_l1_zero_flag;
        0, //  cabac_init_flag;
        1, //  cu_chroma_qp_offset_enabled_flag;
        1, //  deblocking_filter_override_flag;
        0, //  slice_deblocking_filter_disabled_flag;
        0, //  collocated_from_l0_flag;
        0, //  slice_loop_filter_across_slices_enabled_flag;
        0, //  reserved
    };

    const StdVideoEncodeH265SliceSegmentHeader stdVideoEncodeH265SliceSegmentHeader = {
        stdVideoEncodeH265SliceSegmentHeaderFlags, //  StdVideoEncodeH265SliceSegmentHeaderFlags flags;
        sliceType,                                 //  StdVideoH265SliceType slice_type;
        0u,                                        //  uint32_t slice_segment_address;
        0u,                                        //  uint8_t collocated_ref_idx;
        5u,                                        //  uint8_t MaxNumMergeCand;
        0,                                         //  int8_t slice_cb_qp_offset;
        0,                                         //  int8_t slice_cr_qp_offset;
        0,                                         //  int8_t slice_beta_offset_div2;
        0,                                         //  int8_t slice_tc_offset_div2;
        0,                                         //  int8_t slice_act_y_qp_offset;
        0,                                         //  int8_t slice_act_cb_qp_offset;
        0,                                         //  int8_t slice_act_cr_qp_offset;
        0,                                         //  int8_t slice_qp_delta;
        0,                                         //  uint16_t reserved1;
        nullptr                                    //  const StdVideoEncodeH265WeightTable* pWeightTable;
    };

    return de::MovePtr<StdVideoEncodeH265SliceSegmentHeader>(
        new StdVideoEncodeH265SliceSegmentHeader(stdVideoEncodeH265SliceSegmentHeader));
}

de::MovePtr<VkVideoEncodeH264NaluSliceInfoKHR> getVideoEncodeH264NaluSlice(
    StdVideoEncodeH264SliceHeader *stdVideoEncodeH264SliceHeader, const int32_t qpValue)
{
    const VkVideoEncodeH264NaluSliceInfoKHR videoEncodeH264NaluSlice = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_NALU_SLICE_INFO_KHR, //  VkStructureType sType;
        nullptr,                                                 //  const void* pNext;
        qpValue,                                                 //  uint32_t constantQp;
        stdVideoEncodeH264SliceHeader, //  const StdVideoEncodeH264SliceHeader*        pStdSliceHeader
    };

    return de::MovePtr<VkVideoEncodeH264NaluSliceInfoKHR>(
        new VkVideoEncodeH264NaluSliceInfoKHR(videoEncodeH264NaluSlice));
}

de::MovePtr<VkVideoEncodeH265NaluSliceSegmentInfoKHR> getVideoEncodeH265NaluSliceSegment(
    StdVideoEncodeH265SliceSegmentHeader *stdVideoEncodeH265SliceSegmentHeader, const int32_t qpValue)
{
    const VkVideoEncodeH265NaluSliceSegmentInfoKHR videoEncodeH265NaluSliceSegmentInfoKHR = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_NALU_SLICE_SEGMENT_INFO_KHR, //  VkStructureType sType;
        nullptr,                                                         //  const void* pNext;
        qpValue,                                                         //  int32_t constantQp;
        stdVideoEncodeH265SliceSegmentHeader //  const StdVideoEncodeH265SliceSegmentHeader* pStdSliceSegmentHeader;
    };

    return de::MovePtr<VkVideoEncodeH265NaluSliceSegmentInfoKHR>(
        new VkVideoEncodeH265NaluSliceSegmentInfoKHR(videoEncodeH265NaluSliceSegmentInfoKHR));
}

de::MovePtr<StdVideoEncodeH264ReferenceListsInfo> getVideoEncodeH264ReferenceListsInfo(
    uint8_t RefPicList0[STD_VIDEO_H264_MAX_NUM_LIST_REF], uint8_t RefPicList1[STD_VIDEO_H264_MAX_NUM_LIST_REF],
    uint8_t numL0, uint8_t numL1)
{
    const StdVideoEncodeH264ReferenceListsInfoFlags videoEncodeH264ReferenceListsInfoFlags{
        0, //  uint32_t ref_pic_list_modification_flag_l0:1;
        0, //  uint32_t ref_pic_list_modification_flag_l1:1;
        0, //  uint32_t reserved:30;
    };

    StdVideoEncodeH264ReferenceListsInfo videoEncodeH264ReferenceListsInfo = {
        videoEncodeH264ReferenceListsInfoFlags,      //  StdVideoEncodeH264ReferenceListsInfoFlags flags;
        static_cast<uint8_t>(numL0 ? numL0 - 1 : 0), //  uint8_t num_ref_idx_l0_active_minus1;
        static_cast<uint8_t>(numL1 ? numL1 - 1 : 0), //  uint8_t num_ref_idx_l1_active_minus1;
        {},                                          //  uint8_t RefPicList0[STD_VIDEO_H264_MAX_NUM_LIST_REF];
        {},                                          //  uint8_t RefPicList1[STD_VIDEO_H264_MAX_NUM_LIST_REF];
        0,                                           //  uint8_t refList0ModOpCount;
        0,                                           //  uint8_t refList1ModOpCount;
        0,                                           //  uint8_t refPicMarkingOpCount;
        {0, 0, 0, 0, 0, 0, 0},                       //  uint8_t reserved1[7];
        nullptr, //  const StdVideoEncodeH264RefListModEntry* pRefList0ModOperations;
        nullptr, //  const StdVideoEncodeH264RefListModEntry* pRefList1ModOperations;
        nullptr, //  const StdVideoEncodeH264RefPicMarkingEntry* pRefPicMarkingOperations;
    };

    for (int i = 0; i < STD_VIDEO_H264_MAX_NUM_LIST_REF; ++i)
    {
        videoEncodeH264ReferenceListsInfo.RefPicList0[i] = RefPicList0[i];
        videoEncodeH264ReferenceListsInfo.RefPicList1[i] = RefPicList1[i];
    }

    return de::MovePtr<StdVideoEncodeH264ReferenceListsInfo>(
        new StdVideoEncodeH264ReferenceListsInfo(videoEncodeH264ReferenceListsInfo));
}

de::MovePtr<StdVideoEncodeH265ReferenceListsInfo> getVideoEncodeH265ReferenceListsInfo(
    uint8_t RefPicList0[STD_VIDEO_H265_MAX_NUM_LIST_REF], uint8_t RefPicList1[STD_VIDEO_H265_MAX_NUM_LIST_REF])
{
    const StdVideoEncodeH265ReferenceListsInfoFlags videoEncodeH265ReferenceListsInfoFlags{
        0, //  uint32_t ref_pic_list_modification_flag_l0:1;
        0, //  uint32_t ref_pic_list_modification_flag_l1:1;
        0, //  uint32_t reserved:30;
    };

    StdVideoEncodeH265ReferenceListsInfo videoEncodeH265ReferenceListsInfo = {
        videoEncodeH265ReferenceListsInfoFlags, //  StdVideoEncodeH264ReferenceListsInfoFlags flags;
        0,                                      //  uint8_t num_ref_idx_l0_active_minus1;
        0,                                      //  uint8_t num_ref_idx_l1_active_minus1;
        {},                                     //  uint8_t RefPicList0[STD_VIDEO_H265_MAX_NUM_LIST_REF];
        {},                                     //  uint8_t RefPicList1[STD_VIDEO_H265_MAX_NUM_LIST_REF];
        {},                                     //  uint8_t list_entry_l0[STD_VIDEO_H265_MAX_NUM_LIST_REF];
        {},                                     //  uint8_t list_entry_l1[STD_VIDEO_H265_MAX_NUM_LIST_REF];
    };

    for (int i = 0; i < STD_VIDEO_H265_MAX_NUM_LIST_REF; ++i)
    {
        videoEncodeH265ReferenceListsInfo.RefPicList0[i] = RefPicList0[i];
        videoEncodeH265ReferenceListsInfo.RefPicList1[i] = RefPicList1[i];
    }

    return de::MovePtr<StdVideoEncodeH265ReferenceListsInfo>(
        new StdVideoEncodeH265ReferenceListsInfo(videoEncodeH265ReferenceListsInfo));
}

de::MovePtr<StdVideoEncodeH264PictureInfo> getStdVideoEncodeH264PictureInfo(
    StdVideoH264PictureType pictureType, uint32_t frameNum, int32_t PicOrderCnt, uint16_t idr_pic_id,
    const StdVideoEncodeH264ReferenceListsInfo *pRefLists)
{
    const StdVideoEncodeH264PictureInfoFlags pictureInfoFlags = {
        (pictureType == STD_VIDEO_H264_PICTURE_TYPE_IDR), //  uint32_t idr_flag : 1;
        (pictureType != STD_VIDEO_H264_PICTURE_TYPE_B),   //  uint32_t is_reference_flag : 1;
        0,                                                //  uint32_t no_output_of_prior_pics_flag : 1;
        0,                                                //  uint32_t long_term_reference_flag : 1;
        0,                                                //  uint32_t adaptive_ref_pic_marking_mode_flag : 1;
        0,                                                //  uint32_t reserved : 27;
    };

    const StdVideoEncodeH264PictureInfo pictureInfo = {
        pictureInfoFlags, //  StdVideoEncodeH264PictureInfoFlags flags;
        0u,               //  uint8_t seq_parameter_set_id;
        0u,               //  uint8_t pic_parameter_set_id;
        idr_pic_id,       //  uint16_t idr_pic_id;
        pictureType,      //  StdVideoH264PictureType pictureType;
        frameNum,         //  uint32_t frame_num;
        PicOrderCnt,      //  int32_t PicOrderCnt;
        0,                //  uint8_t temporal_id;
        {0, 0, 0},        //  uint8_t reserved1[3];
        pRefLists         //  const StdVideoEncodeH264ReferenceListsInfo* pRefLists;
    };

    return de::MovePtr<StdVideoEncodeH264PictureInfo>(new StdVideoEncodeH264PictureInfo(pictureInfo));
}

de::MovePtr<StdVideoEncodeH265PictureInfo> getStdVideoEncodeH265PictureInfo(
    StdVideoH265PictureType pictureType, int32_t PicOrderCntVal, const StdVideoEncodeH265ReferenceListsInfo *pRefLists,
    StdVideoH265ShortTermRefPicSet *pShortTermRefPicSet)
{
    const StdVideoEncodeH265PictureInfoFlags IRDpictureInfoFlags = {
        1, //  is_reference : 1;
        1, //  IrapPicFlag : 1;
        0, //  used_for_long_term_reference : 1;
        0, //  discardable_flag : 1;
        0, //  cross_layer_bla_flag : 1;
        1, //  pic_output_flag : 1;
        0, //  no_output_of_prior_pics_flag : 1;
        0, //  short_term_ref_pic_set_sps_flag : 1;
        0, //  slice_temporal_mvp_enabled_flag : 1;
        0  //  reserved : 23;
    };

    const StdVideoEncodeH265PictureInfoFlags PpictureInfoFlags = {
        1, //  is_reference : 1;
        0, //  IrapPicFlag : 1;
        0, //  used_for_long_term_reference : 1;
        0, //  discardable_flag : 1;
        0, //  cross_layer_bla_flag : 1;
        0, //  pic_output_flag : 1;
        0, //  no_output_of_prior_pics_flag : 1;
        0, //  short_term_ref_pic_set_sps_flag : 1;
        0, //  slice_temporal_mvp_enabled_flag : 1;
        0  //  reserved : 23;
    };

    const StdVideoEncodeH265PictureInfoFlags BpictureInfoFlags = {
        0, //  is_reference : 1;
        0, //  IrapPicFlag : 1;
        0, //  used_for_long_term_reference : 1;
        0, //  discardable_flag : 1;
        0, //  cross_layer_bla_flag : 1;
        0, //  pic_output_flag : 1;
        0, //  no_output_of_prior_pics_flag : 1;
        0, //  short_term_ref_pic_set_sps_flag : 1;
        0, //  slice_temporal_mvp_enabled_flag : 1;
        0  //  reserved : 23;
    };

    StdVideoEncodeH265PictureInfoFlags flags = IRDpictureInfoFlags;

    switch (pictureType)
    {
    case STD_VIDEO_H265_PICTURE_TYPE_IDR:
    case STD_VIDEO_H265_PICTURE_TYPE_I:
        flags = IRDpictureInfoFlags;
        break;
    case STD_VIDEO_H265_PICTURE_TYPE_P:
        flags = PpictureInfoFlags;
        break;
    case STD_VIDEO_H265_PICTURE_TYPE_B:
        flags = BpictureInfoFlags;
        break;
    default:
        TCU_THROW(InternalError, "Unknown frame type");
    }

    const StdVideoEncodeH265PictureInfo pictureInfo = {
        flags,                 //  StdVideoEncodeH265PictureInfoFlags flags;
        pictureType,           //  StdVideoH265PictureType pictureType;
        0u,                    //  uint8_t sps_video_parameter_set_id;
        0u,                    //  uint8_t pps_seq_parameter_set_id;
        0u,                    //  uint8_t pps_pic_parameter_set_id;
        0u,                    //  uint8_t short_term_ref_pic_set_idx;
        PicOrderCntVal,        //  int32_t PicOrderCntVal;
        0u,                    //  uint8_t TemporalId;
        {0, 0, 0, 0, 0, 0, 0}, //  uint8_t reserved1[7];
        pRefLists,             //  const StdVideoEncodeH265ReferenceListsInfo* pRefLists;
        pShortTermRefPicSet,   //  const StdVideoH265ShortTermRefPicSet* pShortTermRefPicSet;
        nullptr,               //  const StdVideoEncodeH265SliceSegmentLongTermRefPics* pLongTermRefPics;
    };

    return de::MovePtr<StdVideoEncodeH265PictureInfo>(new StdVideoEncodeH265PictureInfo(pictureInfo));
}

de::MovePtr<VkVideoEncodeH264PictureInfoKHR> getVideoEncodeH264PictureInfo(
    const StdVideoEncodeH264PictureInfo *pictureInfo, uint32_t naluSliceEntryCount,
    const VkVideoEncodeH264NaluSliceInfoKHR *pNaluSliceEntries)
{
    const VkVideoEncodeH264PictureInfoKHR videoEncodeH264PictureInfo = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_PICTURE_INFO_KHR, //  VkStructureType sType;
        nullptr,                                              //  const void* pNext;
        naluSliceEntryCount,                                  //  uint32_t naluSliceEntryCount;
        pNaluSliceEntries, //  const VkVideoEncodeH264NaluSliceInfoKHR* pNaluSliceEntries;
        pictureInfo,       //  const StdVideoEncodeH264PictureInfo* pStdPictureInfo;
        false,             //  VkBool32 generatePrefixNalu;

    };

    return de::MovePtr<VkVideoEncodeH264PictureInfoKHR>(
        new VkVideoEncodeH264PictureInfoKHR(videoEncodeH264PictureInfo));
}

de::MovePtr<VkVideoEncodeH265PictureInfoKHR> getVideoEncodeH265PictureInfo(
    const StdVideoEncodeH265PictureInfo *pictureInfo, uint32_t naluSliceSegmentEntryCount,
    const VkVideoEncodeH265NaluSliceSegmentInfoKHR *pNaluSliceSegmentEntries)
{
    const VkVideoEncodeH265PictureInfoKHR videoEncodeH265PictureInfo = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_PICTURE_INFO_KHR, //  VkStructureType sType;
        nullptr,                                              //  const void* pNext;
        naluSliceSegmentEntryCount,                           //  uint32_t naluSliceSegmentEntryCount;
        pNaluSliceSegmentEntries, //  const VkVideoEncodeH265NaluSliceSegmentInfoKHR* pNaluSliceSegmentEntries;
        pictureInfo,              //  const StdVideoEncodeH265PictureInfo* pStdPictureInfo;
    };

    return de::MovePtr<VkVideoEncodeH265PictureInfoKHR>(
        new VkVideoEncodeH265PictureInfoKHR(videoEncodeH265PictureInfo));
}

de::MovePtr<VkVideoBeginCodingInfoKHR> getVideoBeginCodingInfo(VkVideoSessionKHR videoEncodeSession,
                                                               VkVideoSessionParametersKHR videoEncodeSessionParameters,
                                                               uint32_t referenceSlotCount,
                                                               const VkVideoReferenceSlotInfoKHR *pReferenceSlots,
                                                               const void *pNext)
{
    const VkVideoBeginCodingInfoKHR videoBeginCodingInfo = {
        VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR, //  VkStructureType sType;
        pNext,                                         //  const void* pNext;
        0u,                                            //  VkVideoBeginCodingFlagsKHR flags;
        videoEncodeSession,                            //  VkVideoSessionKHR videoSession;
        videoEncodeSessionParameters,                  //  VkVideoSessionParametersKHR videoSessionParameters;
        referenceSlotCount,                            //  uint32_t referenceSlotCount;
        pReferenceSlots,                               //  const VkVideoReferenceSlotInfoKHR* pReferenceSlots;
    };

    return de::MovePtr<VkVideoBeginCodingInfoKHR>(new VkVideoBeginCodingInfoKHR(videoBeginCodingInfo));
}

de::MovePtr<VkVideoInlineQueryInfoKHR> getVideoInlineQueryInfo(VkQueryPool queryPool, uint32_t firstQuery,
                                                               uint32_t queryCount, const void *pNext)
{
    const VkVideoInlineQueryInfoKHR videoInlineQueryInfo = {
        VK_STRUCTURE_TYPE_VIDEO_INLINE_QUERY_INFO_KHR, //  VkStructureType sType;
        pNext,                                         //  const void* pNext;
        queryPool,                                     //  VkQueryPool queryPool;
        firstQuery,                                    //  uint32_t firstQuery;
        queryCount,                                    //  uint32_t queryCount;
    };

    return de::MovePtr<VkVideoInlineQueryInfoKHR>(new VkVideoInlineQueryInfoKHR(videoInlineQueryInfo));
}

de::MovePtr<VkVideoEncodeQuantizationMapSessionParametersCreateInfoKHR> getVideoEncodeH264QuantizationMapParameters(
    VkExtent2D quantizationMapTexelSize)
{

    const VkVideoEncodeQuantizationMapSessionParametersCreateInfoKHR quantizationMapParameters = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_QUANTIZATION_MAP_SESSION_PARAMETERS_CREATE_INFO_KHR, // VkStructureType sType;
        nullptr,                                                                            // void* pNext;
        quantizationMapTexelSize, // VkExtent2D quantizationMapTexelSize;
    };

    return de::MovePtr<VkVideoEncodeQuantizationMapSessionParametersCreateInfoKHR>(
        new VkVideoEncodeQuantizationMapSessionParametersCreateInfoKHR(quantizationMapParameters));
}

de::MovePtr<VkVideoEncodeQuantizationMapInfoKHR> getQuantizationMapInfo(VkImageView quantizationMap,
                                                                        VkExtent2D quantizationMapExtent,
                                                                        const void *pNext)
{
    const VkVideoEncodeQuantizationMapInfoKHR quantizationMapInfo = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_QUANTIZATION_MAP_INFO_KHR, //  VkStructureType sType;
        pNext,                                                    //  const void* pNext;
        quantizationMap,                                          //  VkImageView quantizationMap;
        quantizationMapExtent,                                    //  VkExtent2D quantizationMapTexelSize;
    };

    return de::MovePtr<VkVideoEncodeQuantizationMapInfoKHR>(
        new VkVideoEncodeQuantizationMapInfoKHR(quantizationMapInfo));
}

de::MovePtr<StdVideoH264PictureParameterSet> getStdVideoH264DecodePictureParameterSet(void)
{
    const StdVideoH264PpsFlags stdVideoH264PpsFlags = {
        1u, //  uint32_t transform_8x8_mode_flag:1;
        0u, //  uint32_t redundant_pic_cnt_present_flag:1;
        0u, //  uint32_t constrained_intra_pred_flag:1;
        1u, //  uint32_t deblocking_filter_control_present_flag:1;
        0u, //  uint32_t weighted_pred_flag:1;
        0u, //  uint32_4 bottom_field_pic_order_in_frame_present_flag:1;
        1u, //  uint32_t entropy_coding_mode_flag:1;
        0u, //  uint32_t pic_scaling_matrix_present_flag;
    };

    const StdVideoH264PictureParameterSet stdVideoH264PictureParameterSet = {
        stdVideoH264PpsFlags,                       //  StdVideoH264PpsFlags flags;
        0u,                                         //  uint8_t seq_parameter_set_id;
        0u,                                         //  uint8_t pic_parameter_set_id;
        1u,                                         //  uint8_t num_ref_idx_l0_default_active_minus1;
        0u,                                         //  uint8_t num_ref_idx_l1_default_active_minus1;
        STD_VIDEO_H264_WEIGHTED_BIPRED_IDC_DEFAULT, //  StdVideoH264WeightedBipredIdc weighted_bipred_idc;
        -16,                                        //  int8_t pic_init_qp_minus26;
        0,                                          //  int8_t pic_init_qs_minus26;
        -2,                                         //  int8_t chroma_qp_index_offset;
        -2,                                         //  int8_t second_chroma_qp_index_offset;
        nullptr,                                    //  const StdVideoH264ScalingLists* pScalingLists;
    };

    return de::MovePtr<StdVideoH264PictureParameterSet>(
        new StdVideoH264PictureParameterSet(stdVideoH264PictureParameterSet));
}

de::MovePtr<VkVideoEncodeInfoKHR> getVideoEncodeInfo(const void *pNext, const VkVideoEncodeFlagsKHR encodeFlags,
                                                     const VkBuffer &dstBuffer, const VkDeviceSize &dstBufferOffset,
                                                     const VkVideoPictureResourceInfoKHR &srcPictureResource,
                                                     const VkVideoReferenceSlotInfoKHR *pSetupReferenceSlot,
                                                     const uint32_t &referenceSlotCount,
                                                     const VkVideoReferenceSlotInfoKHR *pReferenceSlots)
{
    const VkVideoEncodeInfoKHR videoEncodeFrameInfo = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_INFO_KHR, //  VkStructureType sType;
        pNext,                                   //  const void* pNext;
        encodeFlags,                             //  VkVideoEncodeFlagsKHR flags;
        dstBuffer,                               //  VkBuffer dstBuffer;
        dstBufferOffset,                         //  VkDeviceSize dstBufferOffset;
        0u,                                      //  VkDeviceSize dstBufferRange;
        srcPictureResource,                      //  VkVideoPictureResourceInfoKHR srcPictureResource;
        pSetupReferenceSlot,                     //  const VkVideoReferenceSlotInfoKHR* pSetupReferenceSlot;
        referenceSlotCount,                      //  uint32_t referenceSlotCount;
        pReferenceSlots,                         //  const VkVideoReferenceSlotInfoKHR* pReferenceSlots;
        0                                        //  uint32_t precedingExternallyEncodedBytes;
    };

    return de::MovePtr<VkVideoEncodeInfoKHR>(new VkVideoEncodeInfoKHR(videoEncodeFrameInfo));
}

std::vector<uint8_t> semiplanarToYV12(const ycbcr::MultiPlaneImageData &multiPlaneImageData)
{
    DE_ASSERT(multiPlaneImageData.getFormat() == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM);

    std::vector<uint8_t> YV12Buffer;
    size_t plane0Size = multiPlaneImageData.getPlaneSize(0);
    size_t plane1Size = multiPlaneImageData.getPlaneSize(1);

    YV12Buffer.resize(plane0Size + plane1Size);

    // Copy the luma plane.
    deMemcpy(YV12Buffer.data(), multiPlaneImageData.getPlanePtr(0), plane0Size);

    // Deinterleave the Cr and Cb plane.
    uint16_t *plane2                    = (uint16_t *)multiPlaneImageData.getPlanePtr(1);
    std::vector<uint8_t>::size_type idx = plane0Size;
    for (unsigned i = 0; i < plane1Size / 2; i++)
        YV12Buffer[idx++] = static_cast<uint8_t>(plane2[i] & 0xFF);
    for (unsigned i = 0; i < plane1Size / 2; i++)
        YV12Buffer[idx++] = static_cast<uint8_t>((plane2[i] >> 8) & 0xFF);

    return YV12Buffer;
}

bool imageMatchesReferenceChecksum(const ycbcr::MultiPlaneImageData &multiPlaneImageData,
                                   const std::string &referenceChecksum)
{
    std::vector<uint8_t> yv12 = semiplanarToYV12(multiPlaneImageData);
    std::string checksum      = MD5SumBase16(yv12.data(), yv12.size());
    return checksum == referenceChecksum;
}

namespace util
{
#ifdef DE_BUILD_VIDEO
void generateYCbCrFile(std::string fileName, uint32_t n_frames, uint32_t width, uint32_t height, uint32_t format,
                       uint8_t bitdepth)
{
    video_generator gen;
    video_generator_settings cfg;
    uint32_t max_frames;

    // Create directory if it doesn't exist
    de::FilePath filePath(fileName);
    std::string dirName = filePath.getDirName();
    if (!dirName.empty() && !de::FilePath(dirName).exists())
    {
        de::createDirectoryAndParents(dirName.c_str());
    }

    std::ofstream outFile(fileName, std::ios::binary | std::ios::out);

    if (!outFile.is_open())
    {
        TCU_THROW(NotSupportedError, "Unable to create the file to generate the YUV content");
    }

    max_frames = n_frames;
    memset(&cfg, 0, sizeof(cfg));
    cfg.width    = width;
    cfg.height   = height;
    cfg.format   = format;
    cfg.bitdepth = bitdepth;

    if (video_generator_init(&cfg, &gen))
    {
        TCU_THROW(NotSupportedError, "Unable to create the video generator");
    }

    while (gen.frame < max_frames)
    {
        video_generator_update(&gen);
        // write video planes to a file
        outFile.write((char *)gen.y, gen.ybytes);
        outFile.write((char *)gen.u, gen.ubytes);
        outFile.write((char *)gen.v, gen.vbytes);
    }

    outFile.close();
    video_generator_clear(&gen);
}
#endif

const char *getVideoCodecString(VkVideoCodecOperationFlagBitsKHR codec)
{
    static struct
    {
        VkVideoCodecOperationFlagBitsKHR eCodec;
        const char *name;
    } aCodecName[] = {
        {VK_VIDEO_CODEC_OPERATION_NONE_KHR, "None"},
        {VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR, "AVC/H.264"},
        {VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR, "H.265/HEVC"},
    };

    for (auto &i : aCodecName)
    {
        if (codec == i.eCodec)
            return aCodecName[codec].name;
    }

    return "Unknown";
}

const char *getVideoChromaFormatString(VkVideoChromaSubsamplingFlagBitsKHR chromaFormat)
{
    switch (chromaFormat)
    {
    case VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR:
        return "YCbCr 400 (Monochrome)";
    case VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR:
        return "YCbCr 420";
    case VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR:
        return "YCbCr 422";
    case VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR:
        return "YCbCr 444";
    default:
        DE_ASSERT(false && "Unknown Chroma sub-sampled format");
    };

    return "Unknown";
}

VkVideoCodecOperationFlagsKHR getSupportedCodecs(DeviceContext &devCtx, uint32_t selectedVideoQueueFamily,
                                                 VkQueueFlags queueFlagsRequired,
                                                 VkVideoCodecOperationFlagsKHR videoCodeOperations)
{
    uint32_t count = 0;
    auto &vkif     = devCtx.context->getInstanceInterface();
    vkif.getPhysicalDeviceQueueFamilyProperties2(devCtx.phys, &count, nullptr);
    std::vector<VkQueueFamilyProperties2> queues(count);
    std::vector<VkQueueFamilyVideoPropertiesKHR> videoQueues(count);
    std::vector<VkQueueFamilyQueryResultStatusPropertiesKHR> queryResultStatus(count);
    for (std::vector<VkQueueFamilyProperties2>::size_type i = 0; i < queues.size(); i++)
    {
        queues[i].sType            = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
        videoQueues[i].sType       = VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR;
        queues[i].pNext            = &videoQueues[i];
        queryResultStatus[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_QUERY_RESULT_STATUS_PROPERTIES_KHR;
        videoQueues[i].pNext       = &queryResultStatus[i];
    }
    vkif.getPhysicalDeviceQueueFamilyProperties2(devCtx.phys, &count, queues.data());

    TCU_CHECK(selectedVideoQueueFamily < queues.size());

    const VkQueueFamilyProperties2 &q                 = queues[selectedVideoQueueFamily];
    const VkQueueFamilyVideoPropertiesKHR &videoQueue = videoQueues[selectedVideoQueueFamily];

    if (q.queueFamilyProperties.queueFlags & queueFlagsRequired &&
        videoQueue.videoCodecOperations & videoCodeOperations)
    {
        // The video queues may or may not support queryResultStatus
        // DE_ASSERT(queryResultStatus[queueIndx].queryResultStatusSupport);
        return videoQueue.videoCodecOperations;
    }

    return VK_VIDEO_CODEC_OPERATION_NONE_KHR;
}

VkResult getVideoFormats(DeviceContext &devCtx, const VkVideoCoreProfile &videoProfile, VkImageUsageFlags imageUsage,
                         uint32_t &formatCount, VkFormat *formats, bool dumpData)
{
    auto &vkif = devCtx.context->getInstanceInterface();

    for (uint32_t i = 0; i < formatCount; i++)
    {
        formats[i] = VK_FORMAT_UNDEFINED;
    }

    const VkVideoProfileListInfoKHR videoProfiles = {VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR, nullptr, 1,
                                                     videoProfile.GetProfile()};
    const VkPhysicalDeviceVideoFormatInfoKHR videoFormatInfo = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_FORMAT_INFO_KHR,
                                                                const_cast<VkVideoProfileListInfoKHR *>(&videoProfiles),
                                                                imageUsage};

    uint32_t supportedFormatCount = 0;
    VkResult result =
        vkif.getPhysicalDeviceVideoFormatPropertiesKHR(devCtx.phys, &videoFormatInfo, &supportedFormatCount, nullptr);
    DE_ASSERT(result == VK_SUCCESS);
    DE_ASSERT(supportedFormatCount);

    VkVideoFormatPropertiesKHR *pSupportedFormats = new VkVideoFormatPropertiesKHR[supportedFormatCount];
    memset(pSupportedFormats, 0x00, supportedFormatCount * sizeof(VkVideoFormatPropertiesKHR));
    for (uint32_t i = 0; i < supportedFormatCount; i++)
    {
        pSupportedFormats[i].sType = VK_STRUCTURE_TYPE_VIDEO_FORMAT_PROPERTIES_KHR;
    }

    result = vkif.getPhysicalDeviceVideoFormatPropertiesKHR(devCtx.phys, &videoFormatInfo, &supportedFormatCount,
                                                            pSupportedFormats);
    DE_ASSERT(result == VK_SUCCESS);
    if (dumpData)
    {
        std::cout << "\t"
                  << ((videoProfile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR) ? "h264" : "h265")
                  << "decode formats: " << std::endl;
        for (uint32_t fmt = 0; fmt < supportedFormatCount; fmt++)
        {
            std::cout << "\t " << fmt << ": " << std::hex << pSupportedFormats[fmt].format << std::dec << std::endl;
        }
    }

    formatCount = std::min(supportedFormatCount, formatCount);

    for (uint32_t i = 0; i < formatCount; i++)
    {
        formats[i] = pSupportedFormats[i].format;
    }

    delete[] pSupportedFormats;

    return result;
}

VkResult getSupportedVideoFormats(DeviceContext &devCtx, const VkVideoCoreProfile &videoProfile,
                                  VkVideoDecodeCapabilityFlagsKHR capabilityFlags, VkFormat &pictureFormat,
                                  VkFormat &referencePicturesFormat)
{
    VkResult result = VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR;
    if ((capabilityFlags & VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_COINCIDE_BIT_KHR) != 0)
    {
        // NV, Intel
        VkFormat supportedDpbFormats[8];
        uint32_t formatCount = sizeof(supportedDpbFormats) / sizeof(supportedDpbFormats[0]);
        result               = util::getVideoFormats(devCtx, videoProfile,
                                                     (VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR |
                                        VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR | VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
                                                     formatCount, supportedDpbFormats);

        referencePicturesFormat = supportedDpbFormats[0];
        pictureFormat           = supportedDpbFormats[0];
    }
    else if ((capabilityFlags & VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_DISTINCT_BIT_KHR) != 0)
    {
        // AMD
        VkFormat supportedDpbFormats[8];
        VkFormat supportedOutFormats[8];
        uint32_t formatCount = sizeof(supportedDpbFormats) / sizeof(supportedDpbFormats[0]);
        result = util::getVideoFormats(devCtx, videoProfile, VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR, formatCount,
                                       supportedDpbFormats);

        DE_ASSERT(result == VK_SUCCESS);

        result = util::getVideoFormats(devCtx, videoProfile,
                                       VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                       formatCount, supportedOutFormats);

        referencePicturesFormat = supportedDpbFormats[0];
        pictureFormat           = supportedOutFormats[0];
    }
    else
    {
        fprintf(stderr, "\nERROR: Unsupported decode capability flags.");
        return VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR;
    }

    DE_ASSERT(result == VK_SUCCESS);
    if (result != VK_SUCCESS)
    {
        fprintf(stderr, "\nERROR: GetVideoFormats() result: 0x%x\n", result);
    }

    DE_ASSERT((referencePicturesFormat != VK_FORMAT_UNDEFINED) && (pictureFormat != VK_FORMAT_UNDEFINED));
    DE_ASSERT(referencePicturesFormat == pictureFormat);

    return result;
}

const char *codecToName(VkVideoCodecOperationFlagBitsKHR codec)
{
    switch ((int32_t)codec)
    {
    case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
        return "decode h.264";
    case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
        return "decode h.265";
    case VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR:
        return "decode av1";
    case VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR:
        return "decode vp9";
    case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR:
        return "encode h.264";
    case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR:
        return "encode h.265";
    case VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR:
        return "encode av1";
    default:
        tcu::die("Unknown video codec");
    }

    return "";
}

VkResult getVideoCapabilities(DeviceContext &devCtx, const VkVideoCoreProfile &videoProfile,
                              VkVideoCapabilitiesKHR *pVideoCapabilities)
{
    auto &vkif = devCtx.context->getInstanceInterface();
    DE_ASSERT(pVideoCapabilities->sType == VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR);

    VkVideoDecodeCapabilitiesKHR *pVideoDecodeCapabilities{};
    VkVideoDecodeH264CapabilitiesKHR *pH264DecodeCapabilities{};
    VkVideoDecodeH265CapabilitiesKHR *pH265DecodeCapabilities{};
    VkVideoDecodeAV1CapabilitiesKHR *pAV1DecodeCapabilities{};
    VkVideoDecodeVP9CapabilitiesKHR *pVP9DecodeCapabilities{};

    VkVideoEncodeCapabilitiesKHR *pVideoEncodeCapabilities{};
    VkVideoEncodeH264CapabilitiesKHR *pH264EncodeCapabilities{};
    VkVideoEncodeH265CapabilitiesKHR *pH265EncodeCapabilities{};

    pVideoDecodeCapabilities = (VkVideoDecodeCapabilitiesKHR *)pVideoCapabilities->pNext;
    pVideoEncodeCapabilities = (VkVideoEncodeCapabilitiesKHR *)pVideoCapabilities->pNext;

    if (videoProfile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR)
    {
        DE_ASSERT(pVideoDecodeCapabilities->sType == VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR);
        pH264DecodeCapabilities = (VkVideoDecodeH264CapabilitiesKHR *)pVideoDecodeCapabilities->pNext;
        DE_ASSERT(pH264DecodeCapabilities->sType == VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_CAPABILITIES_KHR);
    }
    else if (videoProfile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR)
    {
        DE_ASSERT(pVideoDecodeCapabilities->sType == VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR);
        pH265DecodeCapabilities = (VkVideoDecodeH265CapabilitiesKHR *)pVideoDecodeCapabilities->pNext;
        DE_ASSERT(pH265DecodeCapabilities->sType == VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_CAPABILITIES_KHR);
    }
    else if (videoProfile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR)
    {
        DE_ASSERT(pVideoDecodeCapabilities->sType == VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR);
        pAV1DecodeCapabilities = (VkVideoDecodeAV1CapabilitiesKHR *)pVideoDecodeCapabilities->pNext;
        DE_ASSERT(pAV1DecodeCapabilities->sType == VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_CAPABILITIES_KHR);
    }
    else if (videoProfile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR)
    {
        DE_ASSERT(pVideoDecodeCapabilities->sType == VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR);
        pVP9DecodeCapabilities = (VkVideoDecodeVP9CapabilitiesKHR *)pVideoDecodeCapabilities->pNext;
        DE_ASSERT(pVP9DecodeCapabilities->sType == VK_STRUCTURE_TYPE_VIDEO_DECODE_VP9_CAPABILITIES_KHR);
    }
    else if (videoProfile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR)
    {
        DE_ASSERT(pVideoEncodeCapabilities->sType == VK_STRUCTURE_TYPE_VIDEO_ENCODE_CAPABILITIES_KHR);
        pH264EncodeCapabilities = (VkVideoEncodeH264CapabilitiesKHR *)pVideoEncodeCapabilities->pNext;
        DE_ASSERT(pH264EncodeCapabilities->sType == VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_CAPABILITIES_KHR);
    }
    else if (videoProfile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR)
    {
        DE_ASSERT(pVideoEncodeCapabilities->sType == VK_STRUCTURE_TYPE_VIDEO_ENCODE_CAPABILITIES_KHR);
        pH265EncodeCapabilities = (VkVideoEncodeH265CapabilitiesKHR *)pVideoEncodeCapabilities->pNext;
        DE_ASSERT(pH265EncodeCapabilities->sType == VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_CAPABILITIES_KHR);
    }
    else
    {
        DE_ASSERT(false && "Unsupported codec");
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    // For silencing unused variables static analysis error
    DE_UNREF(pVideoDecodeCapabilities);
    DE_UNREF(pH264DecodeCapabilities);
    DE_UNREF(pH265DecodeCapabilities);
    DE_UNREF(pAV1DecodeCapabilities);
    DE_UNREF(pVP9DecodeCapabilities);
    DE_UNREF(pVideoEncodeCapabilities);
    DE_UNREF(pH264EncodeCapabilities);
    DE_UNREF(pH265EncodeCapabilities);

    VkResult result =
        vkif.getPhysicalDeviceVideoCapabilitiesKHR(devCtx.phys, videoProfile.GetProfile(), pVideoCapabilities);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    if (videoLoggingEnabled())
    {
        if (pVideoCapabilities->flags & VK_VIDEO_CAPABILITY_SEPARATE_REFERENCE_IMAGES_BIT_KHR)
            tcu::print("\tseparate reference images\n");

        std::cout << "\t"
                  << "minBitstreamBufferOffsetAlignment: " << pVideoCapabilities->minBitstreamBufferOffsetAlignment
                  << std::endl;
        std::cout << "\t"
                  << "minBitstreamBufferSizeAlignment: " << pVideoCapabilities->minBitstreamBufferSizeAlignment
                  << std::endl;
        std::cout << "\t"
                  << "pictureAccessGranularity: " << pVideoCapabilities->pictureAccessGranularity.width << " x "
                  << pVideoCapabilities->pictureAccessGranularity.height << std::endl;
        std::cout << "\t"
                  << "minCodedExtent: " << pVideoCapabilities->minCodedExtent.width << " x "
                  << pVideoCapabilities->minCodedExtent.height << std::endl;
        std::cout << "\t"
                  << "maxCodedExtent: " << pVideoCapabilities->maxCodedExtent.width << " x "
                  << pVideoCapabilities->maxCodedExtent.height << std::endl;
        std::cout << "\t"
                  << "maxDpbSlots: " << pVideoCapabilities->maxDpbSlots << std::endl;
        std::cout << "\t"
                  << "maxActiveReferencePictures: " << pVideoCapabilities->maxActiveReferencePictures << std::endl;
    }

    if (videoProfile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR)
    {
        if (videoLoggingEnabled())
        {
            std::cout << "\t"
                      << "maxLevelIdc: " << pH264DecodeCapabilities->maxLevelIdc << std::endl;
            std::cout << "\t"
                      << "fieldOffsetGranularity: " << pH264DecodeCapabilities->fieldOffsetGranularity.x << " x "
                      << pH264DecodeCapabilities->fieldOffsetGranularity.y << std::endl;
            ;
        }

        if (strncmp(pVideoCapabilities->stdHeaderVersion.extensionName,
                    VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_EXTENSION_NAME,
                    sizeof(pVideoCapabilities->stdHeaderVersion.extensionName) - 1U) ||
            (pVideoCapabilities->stdHeaderVersion.specVersion != VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_SPEC_VERSION))
        {
            DE_ASSERT(false && "Unsupported AVC extension specification");
            return VK_ERROR_INCOMPATIBLE_DRIVER;
        }
    }
    else if (videoProfile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR)
    {
        if (videoLoggingEnabled())
        {
            std::cout << "\t"
                      << "maxLevelIdc: " << pH265DecodeCapabilities->maxLevelIdc << std::endl;
        }
        if (strncmp(pVideoCapabilities->stdHeaderVersion.extensionName,
                    VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_EXTENSION_NAME,
                    sizeof(pVideoCapabilities->stdHeaderVersion.extensionName) - 1U) ||
            (pVideoCapabilities->stdHeaderVersion.specVersion != VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_SPEC_VERSION))
        {
            DE_ASSERT(false && "Unsupported HEVC extension specification");
            return VK_ERROR_INCOMPATIBLE_DRIVER;
        }
    }
    else if (videoProfile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR)
    {
        if (videoLoggingEnabled())
        {
            std::cout << "\t"
                      << "maxLevel: " << pAV1DecodeCapabilities->maxLevel << std::endl;
        }
        if ((strncmp(pVideoCapabilities->stdHeaderVersion.extensionName,
                     VK_STD_VULKAN_VIDEO_CODEC_AV1_DECODE_EXTENSION_NAME,
                     sizeof(pVideoCapabilities->stdHeaderVersion.extensionName) - 1U) ||
             (pVideoCapabilities->stdHeaderVersion.specVersion != VK_STD_VULKAN_VIDEO_CODEC_AV1_DECODE_SPEC_VERSION)))
        {
            DE_ASSERT(false && "Unsupported AV1 extension specification");
            return VK_ERROR_INCOMPATIBLE_DRIVER;
        }
    }
    else if (videoProfile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR)
    {
        if (videoLoggingEnabled())
        {
            std::cout << "\t"
                      << "maxLevel: " << pVP9DecodeCapabilities->maxLevel << std::endl;
        }
        if ((strncmp(pVideoCapabilities->stdHeaderVersion.extensionName,
                     VK_STD_VULKAN_VIDEO_CODEC_VP9_DECODE_EXTENSION_NAME,
                     sizeof(pVideoCapabilities->stdHeaderVersion.extensionName) - 1U) ||
             (pVideoCapabilities->stdHeaderVersion.specVersion != VK_STD_VULKAN_VIDEO_CODEC_VP9_DECODE_SPEC_VERSION)))
        {
            DE_ASSERT(false && "Unsupported VP9 extension specification");
            return VK_ERROR_INCOMPATIBLE_DRIVER;
        }
    }
    else
    {
        DE_ASSERT(false && "Unsupported codec extension");
    }

    return result;
}

VkResult getVideoDecodeCapabilities(DeviceContext &devCtx, const VkVideoCoreProfile &videoProfile,
                                    VkVideoCapabilitiesKHR &videoCapabilities,
                                    VkVideoDecodeCapabilitiesKHR &videoDecodeCapabilities)
{

    VkVideoCodecOperationFlagsKHR videoCodec = videoProfile.GetProfile()->videoCodecOperation;

    videoDecodeCapabilities = VkVideoDecodeCapabilitiesKHR{VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR, nullptr, 0};

    deMemset(&videoCapabilities, 0, sizeof(VkVideoCapabilitiesKHR));
    videoCapabilities.sType = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
    videoCapabilities.pNext = &videoDecodeCapabilities;

    VkVideoDecodeH264CapabilitiesKHR h264Capabilities{};
    h264Capabilities.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_CAPABILITIES_KHR;

    VkVideoDecodeH265CapabilitiesKHR h265Capabilities{};
    h265Capabilities.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_CAPABILITIES_KHR;

    VkVideoDecodeAV1CapabilitiesKHR av1Capabilities{};
    av1Capabilities.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_CAPABILITIES_KHR;

    VkVideoDecodeVP9CapabilitiesKHR vp9Capabilities{};
    vp9Capabilities.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_VP9_CAPABILITIES_KHR;

    if (videoCodec == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR)
    {
        videoDecodeCapabilities.pNext = &h264Capabilities;
    }
    else if (videoCodec == VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR)
    {
        videoDecodeCapabilities.pNext = &h265Capabilities;
    }
    else if (videoCodec == VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR)
    {
        videoDecodeCapabilities.pNext = &av1Capabilities;
    }
    else if (videoCodec == VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR)
    {
        videoDecodeCapabilities.pNext = &vp9Capabilities;
    }
    else
    {
        DE_ASSERT(false && "Unsupported codec");
        return VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR;
    }
    VkResult result = util::getVideoCapabilities(devCtx, videoProfile, &videoCapabilities);

    return result;
}

VkResult getVideoEncodeCapabilities(DeviceContext &devCtx, const VkVideoCoreProfile &videoProfile,
                                    VkVideoCapabilitiesKHR &videoCapabilities,
                                    VkVideoEncodeCapabilitiesKHR &videoEncodeCapabilities)
{
    VkVideoCodecOperationFlagsKHR videoCodec = videoProfile.GetProfile()->videoCodecOperation;

    // Encode Capabilities

    videoEncodeCapabilities = VkVideoEncodeCapabilitiesKHR();

    VkVideoEncodeH264CapabilitiesKHR h264EncodeCapabilities{};
    h264EncodeCapabilities.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_CAPABILITIES_KHR;

    VkVideoEncodeH265CapabilitiesKHR h265EncodeCapabilities{};
    h265EncodeCapabilities.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_CAPABILITIES_KHR;

    deMemset(&videoCapabilities, 0, sizeof(VkVideoCapabilitiesKHR));
    videoCapabilities.sType = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
    videoCapabilities.pNext = &videoEncodeCapabilities;

    if (videoCodec == VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR)
    {
        videoEncodeCapabilities.pNext = &h264EncodeCapabilities;
    }
    else if (videoCodec == VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR)
    {
        videoEncodeCapabilities.pNext = &h265EncodeCapabilities;
    }
    else
    {
        DE_ASSERT(false && "Unsupported codec");
        return VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR;
    }

    VkResult result = util::getVideoCapabilities(devCtx, videoProfile, &videoCapabilities);
    DE_ASSERT(result == VK_SUCCESS);
    if (result != VK_SUCCESS)
    {
        fprintf(stderr, "\nERROR: Input is not supported. GetVideoCapabilities() result: 0x%x\n", result);
    }
    return result;
}

double PSNR(const std::vector<uint8_t> &img1, const std::vector<uint8_t> &img2)
{
    TCU_CHECK_AND_THROW(InternalError, (img1.size() > 0) && (img1.size() == img2.size()),
                        "Input and output YUVs have different sizes " + de::toString(img1.size()) + " vs " +
                            de::toString(img2.size()));

    using sizet         = std::vector<uint8_t>::size_type;
    sizet sz            = img1.size();
    double squaredError = 0.0;

    for (sizet i = 0; i < sz; i++)
    {
        int diff = static_cast<int>(img1[i]) - static_cast<int>(img2[i]);
        squaredError += std::abs(diff);
    }

    double mse = squaredError / static_cast<double>(sz);
    if (mse == 0)
    {
        return std::numeric_limits<double>::infinity();
    }

    return 10 * std::log10((255.0 * 255.0) / mse);
}

double calculatePSNRdifference(const std::vector<uint8_t> &inVector, const std::vector<uint8_t> &out,
                               const VkExtent2D &codedExtent, const VkExtent2D &quantizationMapExtent,
                               const VkExtent2D &quantizationMapTexelSize)
{
    uint32_t halfWidthInPixels = (quantizationMapExtent.width / 2) * quantizationMapTexelSize.width;
    halfWidthInPixels          = std::min(halfWidthInPixels, codedExtent.width);

    std::vector<uint8_t> inLeftHalfRef =
        util::cropImage(inVector, codedExtent.width, codedExtent.height, 0, 0, halfWidthInPixels, codedExtent.height);
    std::vector<uint8_t> inRightHalfRef =
        util::cropImage(inVector, codedExtent.width, codedExtent.height, halfWidthInPixels, 0,
                        codedExtent.width - halfWidthInPixels, codedExtent.height);
    std::vector<uint8_t> outLeftHalf =
        util::cropImage(out, codedExtent.width, codedExtent.height, 0, 0, halfWidthInPixels, codedExtent.height);
    std::vector<uint8_t> outRightHalf = util::cropImage(out, codedExtent.width, codedExtent.height, halfWidthInPixels,
                                                        0, codedExtent.width - halfWidthInPixels, codedExtent.height);

    double leftPSNR  = PSNR(inLeftHalfRef, outLeftHalf);
    double rightPSNR = PSNR(inRightHalfRef, outRightHalf);

    return rightPSNR - leftPSNR;
}

std::vector<uint8_t> cropImage(const std::vector<uint8_t> &imageData, int imageWidth, int imageHeight, int roiX,
                               int roiY, int roiWidth, int roiHeight)
{
    DE_ASSERT(roiX >= 0 && roiY >= 0 && roiWidth > 0 && roiHeight > 0);
    DE_ASSERT(roiX + roiWidth <= imageWidth && roiY + roiHeight <= imageHeight);
    DE_UNREF(imageHeight);

    std::vector<uint8_t> croppedImage;
    croppedImage.reserve(roiWidth * roiHeight);

    for (int y = roiY; y < roiY + roiHeight; ++y)
    {
        for (int x = roiX; x < roiX + roiWidth; ++x)
        {
            croppedImage.push_back((imageData)[y * imageWidth + x]);
        }
    }

    return croppedImage;
}

} // namespace util

de::MovePtr<StdVideoDecodeH264PictureInfo> getStdVideoDecodeH264PictureInfo(void)
{
    const StdVideoDecodeH264PictureInfoFlags stdPictureInfoFlags = {
        0u, //  uint32_t field_pic_flag;
        0u, //  uint32_t is_intra;
        0u, //  uint32_t IdrPicFlag;
        0u, //  uint32_t bottom_field_flag;
        0u, //  uint32_t is_reference;
        0u, //  uint32_t complementary_field_pair;
    };

    const StdVideoDecodeH264PictureInfo stdPictureInfo = {
        stdPictureInfoFlags, //  StdVideoDecodeH264PictureInfoFlags flags;
        0u,                  //  uint8_t seq_parameter_set_id;
        0u,                  //  uint8_t pic_parameter_set_id;
        0u,                  //  uint8_t reserved1;
        0u,                  //  uint8_t reserved2;
        0u,                  //  uint16_t frame_num;
        0u,                  //  uint16_t idr_pic_id;
        {0},                 //  int32_t PicOrderCnt[STD_VIDEO_DECODE_H264_FIELD_ORDER_COUNT_LIST_SIZE];
    };

    return de::MovePtr<StdVideoDecodeH264PictureInfo>(new StdVideoDecodeH264PictureInfo(stdPictureInfo));
}

de::SharedPtr<VkVideoDecodeH264PictureInfoKHR> getVideoDecodeH264PictureInfo(
    StdVideoDecodeH264PictureInfo *stdPictureInfo, uint32_t *sliceOffset)
{
    const VkVideoDecodeH264PictureInfoKHR pictureInfoHeap = {
        VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PICTURE_INFO_KHR, //  VkStructureType sType;
        nullptr,                                              //  const void* pNext;
        stdPictureInfo,                                       //  const StdVideoDecodeH264PictureInfo* pStdPictureInfo;
        1u,                                                   //  uint32_t sliceCount;
        sliceOffset,                                          //  const uint32_t* pSliceOffsets;
    };

    return de::SharedPtr<VkVideoDecodeH264PictureInfoKHR>(new VkVideoDecodeH264PictureInfoKHR(pictureInfoHeap));
}

de::MovePtr<VkVideoEncodeH264RateControlLayerInfoKHR> getVideoEncodeH264RateControlLayerInfo(
    VkBool32 useMinQp, int32_t minQpI, int32_t minQpP, int32_t minQpB, VkBool32 useMaxQp, int32_t maxQpI,
    int32_t maxQpP, int32_t maxQpB)
{
    const VkVideoEncodeH264FrameSizeKHR videoEncodeH264FrameSize = {
        0, //  uint32_t frameISize;
        0, //  uint32_t framePSize;
        0, //  uint32_t frameBSize;
    };

    const VkVideoEncodeH264QpKHR videoEncodeH264MinQp = {
        minQpI, //  int32_t qpI;
        minQpP, //  int32_t qpP;
        minQpB, //  int32_t qpB;
    };

    const VkVideoEncodeH264QpKHR videoEncodeH264MaxQp = {
        maxQpI, //  int32_t qpI;
        maxQpP, //  int32_t qpI;
        maxQpB, //  int32_t qpI;
    };

    const VkVideoEncodeH264RateControlLayerInfoKHR videoEncodeH264RateControlLayerInfo = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_RATE_CONTROL_LAYER_INFO_KHR, //  VkStructureType sType;
        nullptr,                                                         //  const void* pNext;
        useMinQp,                                                        //  VkBool32 useMinQp;
        videoEncodeH264MinQp,                                            //  VkVideoEncodeH264QpKHR minQp;
        useMaxQp,                                                        //  VkBool32 useMaxQp;
        videoEncodeH264MaxQp,                                            //  VkVideoEncodeH264QpKHR maxQp;
        true,                                                            //  VkBool32 useMaxFrameSize;
        videoEncodeH264FrameSize,                                        //  VkVideoEncodeH264FrameSizeKHR maxFrameSize;
    };

    return de::MovePtr<VkVideoEncodeH264RateControlLayerInfoKHR>(
        new VkVideoEncodeH264RateControlLayerInfoKHR(videoEncodeH264RateControlLayerInfo));
}

de::MovePtr<VkVideoEncodeH265RateControlLayerInfoKHR> getVideoEncodeH265RateControlLayerInfo(
    VkBool32 useMinQp, int32_t minQpI, int32_t minQpP, int32_t minQpB, VkBool32 useMaxQp, int32_t maxQpI,
    int32_t maxQpP, int32_t maxQpB)
{
    const VkVideoEncodeH265FrameSizeKHR videoEncodeH265FrameSize = {
        0, //  uint32_t frameISize;
        0, //  uint32_t framePSize;
        0, //  uint32_t frameBSize;
    };

    const VkVideoEncodeH265QpKHR videoEncodeH265MinQp = {
        minQpI, //  int32_t qpI;
        minQpP, //  int32_t qpP;
        minQpB, //  int32_t qpB;
    };

    const VkVideoEncodeH265QpKHR videoEncodeH265MaxQp = {
        maxQpI, //  int32_t qpI;
        maxQpP, //  int32_t qpP;
        maxQpB, //  int32_t qpB;
    };

    const VkVideoEncodeH265RateControlLayerInfoKHR videoEncodeH265RateControlLayerInfo = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_RATE_CONTROL_LAYER_INFO_KHR, //  VkStructureType sType;
        nullptr,                                                         //  const void* pNext;
        useMinQp,                                                        //  VkBool32 useMinQp;
        videoEncodeH265MinQp,                                            //  VkVideoEncodeH265QpKHR minQp;
        useMaxQp,                                                        //  VkBool32 useMaxQp;
        videoEncodeH265MaxQp,                                            //  VkVideoEncodeH265QpKHR maxQp;
        true,                                                            //  VkBool32 useMaxFrameSize;
        videoEncodeH265FrameSize,                                        //  VkVideoEncodeH265FrameSizeKHR maxFrameSize;
    };

    return de::MovePtr<VkVideoEncodeH265RateControlLayerInfoKHR>(
        new VkVideoEncodeH265RateControlLayerInfoKHR(videoEncodeH265RateControlLayerInfo));
}

de::MovePtr<VkVideoEncodeRateControlLayerInfoKHR> getVideoEncodeRateControlLayerInfo(
    const void *pNext, VkVideoEncodeRateControlModeFlagBitsKHR rateControlMode, const uint32_t frameRateNumerator)
{
    const VkVideoEncodeRateControlLayerInfoKHR videoEncodeRateControlLayerInfo = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_RATE_CONTROL_LAYER_INFO_KHR, //  VkStructureType sType;
        pNext,                                                      //  const void* pNext;
        50000,                                                      //  uint64_t averageBitrate;
        static_cast<uint64_t>(
            rateControlMode == VK_VIDEO_ENCODE_RATE_CONTROL_MODE_CBR_BIT_KHR ? 50000 : 75000), //  uint64_t maxBitrate;
        frameRateNumerator, //  uint32_t frameRateNumerator;
        1,                  //  uint32_t frameRateDenominator;
    };

    return de::MovePtr<VkVideoEncodeRateControlLayerInfoKHR>(
        new VkVideoEncodeRateControlLayerInfoKHR(videoEncodeRateControlLayerInfo));
}

de::MovePtr<VkVideoEncodeRateControlInfoKHR> getVideoEncodeRateControlInfo(
    const void *pNext, VkVideoEncodeRateControlModeFlagBitsKHR rateControlMode,
    VkVideoEncodeRateControlLayerInfoKHR *videoEncodeRateControlLayerInfo)
{
    const VkVideoEncodeRateControlInfoKHR videoEncodeRateControlInfo = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_RATE_CONTROL_INFO_KHR, //  VkStructureType sType;
        pNext,                                                //  const void* pNext;
        static_cast<VkVideoEncodeRateControlFlagsKHR>(0u),    //  VkVideoEncodeRateControlFlagsKHR flags;
        rateControlMode, //  VkVideoEncodeRateControlModeFlagBitsKHR rateControlMode;
        videoEncodeRateControlLayerInfo == nullptr ? 0 : 1U,    //  uint8_t layerCount;
        videoEncodeRateControlLayerInfo,                        //  const VkVideoEncodeRateControlLayerInfoKHR* pLayers;
        videoEncodeRateControlLayerInfo == nullptr ? 0 : 1000U, //  uint32_t virtualBufferSizeInMs;
        videoEncodeRateControlLayerInfo == nullptr ? 0 : 500U,  //  uint32_t initialVirtualBufferSizeInMs;
    };

    return de::MovePtr<VkVideoEncodeRateControlInfoKHR>(
        new VkVideoEncodeRateControlInfoKHR(videoEncodeRateControlInfo));
}

de::MovePtr<VkVideoEncodeH264QualityLevelPropertiesKHR> getvideoEncodeH264QualityLevelProperties(int32_t qpI,
                                                                                                 int32_t qpP,
                                                                                                 int32_t qpB)
{
    const VkVideoEncodeH264QpKHR preferredConstantQp = {
        qpI, //  int32_t qpI;
        qpP, //  int32_t qpP;
        qpB, //  int32_t qpB;
    };

    const VkVideoEncodeH264QualityLevelPropertiesKHR videoEncodeH264QualityLevelProperties = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_QUALITY_LEVEL_PROPERTIES_KHR, //  VkStructureType sType;
        nullptr,                                                          //  void* pNext;
        VK_VIDEO_ENCODE_H264_RATE_CONTROL_REGULAR_GOP_BIT_KHR, //  VkVideoEncodeH264RateControlFlagsKHR preferredRateControlFlags;
        0,                                                     //  uint32_t preferredGopFrameCount;
        0,                                                     //  uint32_t preferredIdrPeriod;
        0,                                                     //  uint32_t preferredConsecutiveBFrameCount;
        0,                                                     //  uint32_t preferredTemporalLayerCount;
        preferredConstantQp,                                   //  VkVideoEncodeH264QpKHR preferredConstantQp;
        0,                                                     //  uint32_t preferredMaxL0ReferenceCount;
        0,                                                     //  uint32_t preferredMaxL1ReferenceCount;
        0,                                                     //  VkBool32 preferredStdEntropyCodingModeFlag;
    };

    return de::MovePtr<VkVideoEncodeH264QualityLevelPropertiesKHR>(
        new VkVideoEncodeH264QualityLevelPropertiesKHR(videoEncodeH264QualityLevelProperties));
}

de::MovePtr<VkVideoEncodeH265QualityLevelPropertiesKHR> getvideoEncodeH265QualityLevelProperties(int32_t qpI,
                                                                                                 int32_t qpP,
                                                                                                 int32_t qpB)
{
    const VkVideoEncodeH265QpKHR preferredConstantQp = {
        qpI, //  int32_t qpI;
        qpP, //  int32_t qpP;
        qpB, //  int32_t qpB;
    };

    const VkVideoEncodeH265QualityLevelPropertiesKHR videoEncodeH265QualityLevelProperties = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_QUALITY_LEVEL_PROPERTIES_KHR, //  VkStructureType sType;
        nullptr,                                                          //  void* pNext;
        VK_VIDEO_ENCODE_H264_RATE_CONTROL_REGULAR_GOP_BIT_KHR, //  VkVideoEncodeH265RateControlFlagsKHR preferredRateControlFlags;
        0,                                                     //  uint32_t preferredGopFrameCount;
        0,                                                     //  uint32_t preferredIdrPeriod;
        0,                                                     //  uint32_t preferredConsecutiveBFrameCount;
        0,                                                     //  uint32_t preferredSubLayerCount;
        preferredConstantQp,                                   //  VkVideoEncodeH265QpKHR preferredConstantQp;
        0,                                                     //  uint32_t preferredMaxL0ReferenceCount;
        0,                                                     //  uint32_t preferredMaxL1ReferenceCount;
    };

    return de::MovePtr<VkVideoEncodeH265QualityLevelPropertiesKHR>(
        new VkVideoEncodeH265QualityLevelPropertiesKHR(videoEncodeH265QualityLevelProperties));
}

de::MovePtr<VkVideoEncodeQualityLevelPropertiesKHR> getVideoEncodeQualityLevelProperties(
    void *pNext, VkVideoEncodeRateControlModeFlagBitsKHR preferredRateControlMode)
{
    const VkVideoEncodeQualityLevelPropertiesKHR videoEncodeQualityLevelProperties = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_QUALITY_LEVEL_PROPERTIES_KHR, //  VkStructureType sType;
        pNext,                                                       //  void* pNext;
        preferredRateControlMode, //  VkVideoEncodeRateControlModeFlagBitsKHR preferredRateControlMode;
        1U,                       //  uint32_t preferredRateControlLayerCount;
    };

    return de::MovePtr<VkVideoEncodeQualityLevelPropertiesKHR>(
        new VkVideoEncodeQualityLevelPropertiesKHR(videoEncodeQualityLevelProperties));
}

de::MovePtr<VkPhysicalDeviceVideoEncodeQualityLevelInfoKHR> getPhysicalDeviceVideoEncodeQualityLevelInfo(
    const VkVideoProfileInfoKHR *pVideoProfile, uint32_t qualityLevel)
{
    VkPhysicalDeviceVideoEncodeQualityLevelInfoKHR physicalDeviceVideoEncodeQualityLevelInfo = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_ENCODE_QUALITY_LEVEL_INFO_KHR, //  VkStructureType sType;
        nullptr,                                                               //  const void* pNext;
        pVideoProfile, //  const VkVideoProfileInfoKHR* pVideoProfile;
        qualityLevel,  //  uint32_t qualityLevel;
    };

    return de::MovePtr<VkPhysicalDeviceVideoEncodeQualityLevelInfoKHR>(
        new VkPhysicalDeviceVideoEncodeQualityLevelInfoKHR(physicalDeviceVideoEncodeQualityLevelInfo));
}

de::MovePtr<VkVideoEncodeQualityLevelInfoKHR> getVideoEncodeQualityLevelInfo(
    uint32_t qualityLevel, VkVideoEncodeQualityLevelPropertiesKHR *videoEncodeQualityLevelProperties)
{
    const VkVideoEncodeQualityLevelInfoKHR videoEncodeQualityLevelInfo = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_QUALITY_LEVEL_INFO_KHR, //  VkStructureType sType;
        videoEncodeQualityLevelProperties,                     //  const void* pNext;
        qualityLevel,                                          //  uint32_t qualityLevel;
    };

    return de::MovePtr<VkVideoEncodeQualityLevelInfoKHR>(
        new VkVideoEncodeQualityLevelInfoKHR(videoEncodeQualityLevelInfo));
}

de::MovePtr<VkVideoCodingControlInfoKHR> getVideoCodingControlInfo(VkVideoCodingControlFlagsKHR flags,
                                                                   const void *pNext)
{
    const VkVideoCodingControlInfoKHR videoEncodingControlInfo = {
        VK_STRUCTURE_TYPE_VIDEO_CODING_CONTROL_INFO_KHR, //  VkStructureType sType;
        pNext,                                           //  const void* pNext;
        flags,                                           //  VkVideoCodingControlFlagsKHR flags;
    };

    return de::MovePtr<VkVideoCodingControlInfoKHR>(new VkVideoCodingControlInfoKHR(videoEncodingControlInfo));
}

} // namespace video
} // namespace vkt
