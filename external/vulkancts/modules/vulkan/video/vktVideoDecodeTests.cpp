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
 * \brief Video Decoding Session tests
 *//*--------------------------------------------------------------------*/

#include "vktVideoDecodeTests.hpp"
#include "vktVideoTestUtils.hpp"
#include "vktTestCase.hpp"
#include "vktVideoPictureUtils.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuPlatform.hpp"
#include "tcuFunctionLibrary.hpp"
#include "tcuImageCompare.hpp"

#include <deDefs.h>
#include "vkDefs.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"

#include "../ycbcr/vktYCbCrUtil.hpp"

#ifdef DE_BUILD_VIDEO
#include "vktVideoSessionNvUtils.hpp"
#include "extESExtractor.hpp"
#include "vktVideoBaseDecodeUtils.hpp"
#endif

#include <atomic>

namespace vkt
{
namespace video
{
namespace
{
using namespace vk;
using namespace std;

using de::MovePtr;
using vkt::ycbcr::MultiPlaneImageData;

enum TestType
{
    TEST_TYPE_H264_DECODE_I,                           // Case 6
    TEST_TYPE_H264_DECODE_I_P,                         // Case 7
    TEST_TYPE_H264_DECODE_I_P_B_13,                    // Case 7a
    TEST_TYPE_H264_DECODE_I_P_NOT_MATCHING_ORDER,      // Case 8
    TEST_TYPE_H264_DECODE_I_P_B_13_NOT_MATCHING_ORDER, // Case 8a
    TEST_TYPE_H264_DECODE_QUERY_RESULT_WITH_STATUS,    // Case 9
    TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE,           // Case 17
    TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE_DPB,       // Case 18
    TEST_TYPE_H264_DECODE_INTERLEAVED,                 // Case 21
    TEST_TYPE_H264_BOTH_DECODE_ENCODE_INTERLEAVED,     // Case 23
    TEST_TYPE_H264_H265_DECODE_INTERLEAVED,            // Case 24

    TEST_TYPE_H265_DECODE_I,                           // Case 15
    TEST_TYPE_H265_DECODE_I_P,                         // Case 16
    TEST_TYPE_H265_DECODE_I_P_NOT_MATCHING_ORDER,      // Case 16-2
    TEST_TYPE_H265_DECODE_I_P_B_13,                    // Case 16-3
    TEST_TYPE_H265_DECODE_I_P_B_13_NOT_MATCHING_ORDER, // Case 16-4

    TEST_TYPE_LAST
};

struct CaseDef
{
    TestType testType;
};

// Vulkan video is not supported on android platform
// all external libraries, helper functions and test instances has been excluded
#ifdef DE_BUILD_VIDEO
DecodedFrame initDecodeFrame(void)
{
    DecodedFrame frameTemplate = {
        -1,                        //  int32_t pictureIndex;
        DE_NULL,                   //  const ImageObject* pDecodedImage;
        VK_IMAGE_LAYOUT_UNDEFINED, //  VkImageLayout decodedImageLayout;
        DE_NULL,                   //  VkFence frameCompleteFence;
        DE_NULL,                   //  VkFence frameConsumerDoneFence;
        DE_NULL,                   //  VkSemaphore frameCompleteSemaphore;
        DE_NULL,                   //  VkSemaphore frameConsumerDoneSemaphore;
        DE_NULL,                   //  VkQueryPool queryPool;
        0,                         //  int32_t startQueryId;
        0,                         //  uint32_t numQueries;
        0,                         //  uint64_t timestamp;
        0,                         //  uint32_t hasConsummerSignalFence : 1;
        0,                         //  uint32_t hasConsummerSignalSemaphore : 1;
        0,                         //  int32_t decodeOrder;
        0,                         //  int32_t displayOrder;
    };

    return frameTemplate;
}

// Avoid useless sampler in writeImage 2.5x faster
MovePtr<tcu::TextureLevel> convertToRGBASized(const tcu::ConstPixelBufferAccess &src, const tcu::UVec2 &size)
{
    const tcu::TextureFormat format(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8);
    MovePtr<tcu::TextureLevel> result(new tcu::TextureLevel(format, size.x(), size.y()));
    tcu::PixelBufferAccess access(result->getAccess());

    for (int y = 0; y < result->getHeight(); ++y)
        for (int x = 0; x < result->getWidth(); ++x)
            access.setPixel(src.getPixelUint(x, y), x, y);

    return result;
}

MovePtr<tcu::TextureLevel> convertToRGBA(const tcu::ConstPixelBufferAccess &src)
{
    return convertToRGBASized(src, tcu::UVec2((uint32_t)src.getWidth(), (uint32_t)src.getHeight()));
}

MovePtr<MultiPlaneImageData> getDecodedImage(const DeviceInterface &vkd, VkDevice device, Allocator &allocator,
                                             VkImage image, VkImageLayout layout, VkFormat format,
                                             VkExtent2D codedExtent, uint32_t queueFamilyIndexTransfer,
                                             uint32_t queueFamilyIndexDecode)
{
    MovePtr<MultiPlaneImageData> multiPlaneImageData(
        new MultiPlaneImageData(format, tcu::UVec2(codedExtent.width, codedExtent.height)));
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
        VK_ACCESS_NONE_KHR, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, image, imageSubresourceRange,
        queueFamilyIndexDecode, queueFamilyIndexTransfer);
    const VkImageMemoryBarrier2KHR imageBarrierOwnershipTransfer = makeImageMemoryBarrier2(
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR, VK_ACCESS_NONE_KHR, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,
        VK_ACCESS_NONE_KHR, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, image, imageSubresourceRange,
        queueFamilyIndexDecode, queueFamilyIndexTransfer);
    const VkImageMemoryBarrier2KHR imageBarrierTransfer =
        makeImageMemoryBarrier2(VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR, VK_ACCESS_2_TRANSFER_READ_BIT_KHR,
                                VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR, VK_ACCESS_NONE_KHR, VK_IMAGE_LAYOUT_GENERAL,
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, imageSubresourceRange);
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
        DE_NULL,                       // const void* pNext;
        0u,                            // uint32_t waitSemaphoreCount;
        DE_NULL,                       // const VkSemaphore* pWaitSemaphores;
        DE_NULL,                       // const VkPipelineStageFlags* pWaitDstStageMask;
        1u,                            // uint32_t commandBufferCount;
        &*cmdDecodeBuffer,             // const VkCommandBuffer* pCommandBuffers;
        1u,                            // uint32_t signalSemaphoreCount;
        &*semaphore,                   // const VkSemaphore* pSignalSemaphores;
    };
    const VkSubmitInfo transferSubmitInfo{
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType sType;
        DE_NULL,                       // const void* pNext;
        1u,                            // uint32_t waitSemaphoreCount;
        &*semaphore,                   // const VkSemaphore* pWaitSemaphores;
        &waitDstStageMask,             // const VkPipelineStageFlags* pWaitDstStageMask;
        1u,                            // uint32_t commandBufferCount;
        &*cmdTransferBuffer,           // const VkCommandBuffer* pCommandBuffers;
        0u,                            // uint32_t signalSemaphoreCount;
        DE_NULL,                       // const VkSemaphore* pSignalSemaphores;
    };

    DEBUGLOG(std::cout << "getDecodedImage: " << image << " " << layout << std::endl);

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

class VideoDecodeTestInstance : public VideoBaseTestInstance
{
public:
    typedef std::pair<tcu::IVec3, tcu::IVec3> ReferencePixel;

    VideoDecodeTestInstance(Context &context, const CaseDef &data);
    ~VideoDecodeTestInstance(void);

    std::string getTestVideoData(void);

    tcu::TestStatus iterate(void);
    tcu::TestStatus iterateSingleFrame(void);
    tcu::TestStatus iterateDoubleFrame(void);
    tcu::TestStatus iterateMultipleFrame(void);
    bool verifyImage(uint32_t frameNumber, const MultiPlaneImageData &multiPlaneImageData);
    bool verifyImageMultipleFrame(uint32_t frameNumber, const MultiPlaneImageData &multiPlaneImageData);
    bool verifyImageMultipleFrameNoReference(uint32_t frameNumber, const MultiPlaneImageData &multiPlaneImageData,
                                             const vector<ReferencePixel> &referencePixels);
    bool verifyImageMultipleFrameWithReference(uint32_t frameNumber, const MultiPlaneImageData &multiPlaneImageData);

protected:
    CaseDef m_caseDef;
    MovePtr<VideoBaseDecoder> m_decoder;
    VkVideoCodecOperationFlagBitsKHR m_videoCodecOperation;
    int32_t m_frameCountTrigger;
    bool m_queryWithStatusRequired;
};

VideoDecodeTestInstance::VideoDecodeTestInstance(Context &context, const CaseDef &data)
    : VideoBaseTestInstance(context)
    , m_caseDef(data)
    , m_decoder(new VideoBaseDecoder(context))
    , m_videoCodecOperation(VK_VIDEO_CODEC_OPERATION_NONE_KHR)
    , m_frameCountTrigger(0)
    , m_queryWithStatusRequired(false)
{
    const bool queryResultWithStatus    = m_caseDef.testType == TEST_TYPE_H264_DECODE_QUERY_RESULT_WITH_STATUS;
    const bool twoCachedPicturesSwapped = queryResultWithStatus ||
                                          m_caseDef.testType == TEST_TYPE_H264_DECODE_I_P_NOT_MATCHING_ORDER ||
                                          m_caseDef.testType == TEST_TYPE_H265_DECODE_I_P_NOT_MATCHING_ORDER;
    const bool randomOrSwapped = twoCachedPicturesSwapped ||
                                 m_caseDef.testType == TEST_TYPE_H264_DECODE_I_P_B_13_NOT_MATCHING_ORDER ||
                                 m_caseDef.testType == TEST_TYPE_H265_DECODE_I_P_B_13_NOT_MATCHING_ORDER;
    const uint32_t gopSize  = m_caseDef.testType == TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE     ? 15 :
                              m_caseDef.testType == TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE_DPB ? 15 :
                                                                                                  0;
    const uint32_t gopCount = m_caseDef.testType == TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE     ? 2 :
                              m_caseDef.testType == TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE_DPB ? 1 :
                                                                                                  0;
    const bool submitDuringRecord =
        m_caseDef.testType == TEST_TYPE_H264_DECODE_I || m_caseDef.testType == TEST_TYPE_H264_DECODE_I_P ||
        m_caseDef.testType == TEST_TYPE_H265_DECODE_I || m_caseDef.testType == TEST_TYPE_H265_DECODE_I_P;
    const bool submitAfter = !submitDuringRecord;

    m_frameCountTrigger = m_caseDef.testType == TEST_TYPE_H264_DECODE_I                           ? 1 :
                          m_caseDef.testType == TEST_TYPE_H264_DECODE_I_P                         ? 2 :
                          m_caseDef.testType == TEST_TYPE_H264_DECODE_I_P_B_13                    ? 13 * 2 :
                          m_caseDef.testType == TEST_TYPE_H264_DECODE_I_P_NOT_MATCHING_ORDER      ? 2 :
                          m_caseDef.testType == TEST_TYPE_H264_DECODE_I_P_B_13_NOT_MATCHING_ORDER ? 13 * 2 :
                          m_caseDef.testType == TEST_TYPE_H264_DECODE_QUERY_RESULT_WITH_STATUS    ? 2 :
                          m_caseDef.testType == TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE           ? 15 * 2 :
                          m_caseDef.testType == TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE_DPB       ? 15 * 2 :
                          m_caseDef.testType == TEST_TYPE_H265_DECODE_I                           ? 1 :
                          m_caseDef.testType == TEST_TYPE_H265_DECODE_I_P                         ? 2 :
                          m_caseDef.testType == TEST_TYPE_H265_DECODE_I_P_NOT_MATCHING_ORDER      ? 2 :
                          m_caseDef.testType == TEST_TYPE_H265_DECODE_I_P_B_13                    ? 13 * 2 :
                          m_caseDef.testType == TEST_TYPE_H265_DECODE_I_P_B_13_NOT_MATCHING_ORDER ? 13 * 2 :
                                                                                                    0;

    m_decoder->setDecodeParameters(randomOrSwapped, queryResultWithStatus, m_frameCountTrigger, submitAfter, gopSize,
                                   gopCount);

    m_videoCodecOperation = de::inBounds(m_caseDef.testType, TEST_TYPE_H264_DECODE_I, TEST_TYPE_H265_DECODE_I) ?
                                VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR :
                            de::inBounds(m_caseDef.testType, TEST_TYPE_H265_DECODE_I, TEST_TYPE_LAST) ?
                                VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR :
                                VK_VIDEO_CODEC_OPERATION_NONE_KHR;

    DE_ASSERT(m_videoCodecOperation != VK_VIDEO_CODEC_OPERATION_NONE_KHR);

    m_queryWithStatusRequired = (m_caseDef.testType == TEST_TYPE_H264_DECODE_QUERY_RESULT_WITH_STATUS);
}

VideoDecodeTestInstance::~VideoDecodeTestInstance(void)
{
}

std::string VideoDecodeTestInstance::getTestVideoData(void)
{
    switch (m_caseDef.testType)
    {
    case TEST_TYPE_H264_DECODE_I:
    case TEST_TYPE_H264_DECODE_I_P:
    case TEST_TYPE_H264_DECODE_I_P_NOT_MATCHING_ORDER:
    case TEST_TYPE_H264_DECODE_QUERY_RESULT_WITH_STATUS:
        return getVideoDataClipA();
    case TEST_TYPE_H264_DECODE_I_P_B_13:
    case TEST_TYPE_H264_DECODE_I_P_B_13_NOT_MATCHING_ORDER:
        return getVideoDataClipH264G13();
    case TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE:
    case TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE_DPB:
        return getVideoDataClipC();
    case TEST_TYPE_H265_DECODE_I:
    case TEST_TYPE_H265_DECODE_I_P:
    case TEST_TYPE_H265_DECODE_I_P_NOT_MATCHING_ORDER:
        return getVideoDataClipD();
    case TEST_TYPE_H265_DECODE_I_P_B_13:
    case TEST_TYPE_H265_DECODE_I_P_B_13_NOT_MATCHING_ORDER:
        return getVideoDataClipH265G13();

    default:
        TCU_THROW(InternalError, "Unknown testType");
    }
}

tcu::TestStatus VideoDecodeTestInstance::iterate(void)
{
    if (m_frameCountTrigger == 1)
        return iterateSingleFrame();
    else if (m_frameCountTrigger == 2)
        return iterateDoubleFrame();
    else
        return iterateMultipleFrame();
}

vk::VkExtensionProperties getExtensionVersion(VkVideoCodecOperationFlagBitsKHR videoCodecOperation)
{
    static const vk::VkExtensionProperties h264StdExtensionVersion = {
        VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_SPEC_VERSION};
    static const vk::VkExtensionProperties h265StdExtensionVersion = {
        VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_SPEC_VERSION};

    if (videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR)
    {
        return h264StdExtensionVersion;
    }
    else if (videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR)
    {
        return h265StdExtensionVersion;
    }

    TCU_THROW(InternalError, "Unsupported Codec Type");
}

tcu::TestStatus VideoDecodeTestInstance::iterateSingleFrame(void)
{
    tcu::TestLog &log = m_context.getTestContext().getLog();
    const VideoDevice::VideoDeviceFlags videoDeviceFlags =
        VideoDevice::VIDEO_DEVICE_FLAG_REQUIRE_SYNC2_OR_NOT_SUPPORTED |
        (m_queryWithStatusRequired ? VideoDevice::VIDEO_DEVICE_FLAG_QUERY_WITH_STATUS_FOR_DECODE_SUPPORT : 0);
    const VkDevice device      = getDeviceSupportingQueue(VK_QUEUE_VIDEO_DECODE_BIT_KHR | VK_QUEUE_TRANSFER_BIT,
                                                          m_videoCodecOperation, videoDeviceFlags);
    const DeviceInterface &vkd = getDeviceDriver();
    const uint32_t queueFamilyIndexDecode     = getQueueFamilyIndexDecode();
    const uint32_t queueFamilyIndexTransfer   = getQueueFamilyIndexTransfer();
    Allocator &allocator                      = getAllocator();
    std::string videoData                     = getTestVideoData();
    VkExtensionProperties stdExtensionVersion = getExtensionVersion(m_videoCodecOperation);

    MovePtr<IfcVulkanVideoDecodeParser> vulkanVideoDecodeParser(
        m_decoder->GetNvFuncs()->createIfcVulkanVideoDecodeParser(m_videoCodecOperation, &stdExtensionVersion));
    bool videoStreamHasEnded = false;
    int32_t framesInQueue    = 0;
    int32_t frameNumber      = 0;
    int32_t framesCorrect    = 0;
    DecodedFrame frame       = initDecodeFrame();
    ESEDemuxer demuxer(videoData, log);

    m_decoder->initialize(m_videoCodecOperation, vkd, device, queueFamilyIndexTransfer, queueFamilyIndexDecode,
                          allocator);

    if (!vulkanVideoDecodeParser->initialize(dynamic_cast<NvidiaVulkanParserVideoDecodeClient *>(m_decoder.get())))
    {
        TCU_THROW(InternalError, "vulkanVideoDecodeParser->initialize()");
    }

    while (framesInQueue > 0 || !videoStreamHasEnded)
    {
        framesInQueue = m_decoder->GetVideoFrameBuffer()->DequeueDecodedPicture(&frame);

        while (framesInQueue == 0 && !videoStreamHasEnded)
        {
            if (!videoStreamHasEnded)
            {
                uint8_t *pData            = 0;
                int64_t size              = 0;
                const bool demuxerSuccess = demuxer.Demux(&pData, &size);
                const bool parserSuccess  = vulkanVideoDecodeParser->parseByteStream(pData, size);

                if (!demuxerSuccess || !parserSuccess)
                    videoStreamHasEnded = true;
            }

            framesInQueue = m_decoder->GetVideoFrameBuffer()->DequeueDecodedPicture(&frame);
        }

        if (frame.pictureIndex >= 0)
        {
            const VkExtent2D imageExtent = frame.pDecodedImage->getExtent();
            const VkImage image          = frame.pDecodedImage->getImage();
            const VkFormat format        = frame.pDecodedImage->getFormat();
            const VkImageLayout layout   = frame.decodedImageLayout;
            MovePtr<MultiPlaneImageData> resultImage =
                getDecodedImage(vkd, device, allocator, image, layout, format, imageExtent, queueFamilyIndexTransfer,
                                queueFamilyIndexDecode);

            if (verifyImage(frameNumber, *resultImage))
                framesCorrect++;

            m_decoder->ReleaseDisplayedFrame(&frame);
            frameNumber++;

            if (frameNumber >= 1)
                break;
        }
    }

    if (!vulkanVideoDecodeParser->deinitialize())
    {
        TCU_THROW(InternalError, "vulkanVideoDecodeParser->deinitialize()");
    }

    if (framesCorrect > 0 && framesCorrect == frameNumber)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("Some frames has not been decoded correctly (" + de::toString(framesCorrect) +
                                     "/" + de::toString(frameNumber) + ")");
}

tcu::TestStatus VideoDecodeTestInstance::iterateDoubleFrame(void)
{
    tcu::TestLog &log = m_context.getTestContext().getLog();
    const VideoDevice::VideoDeviceFlags videoDeviceFlags =
        VideoDevice::VIDEO_DEVICE_FLAG_REQUIRE_SYNC2_OR_NOT_SUPPORTED |
        (m_queryWithStatusRequired ? VideoDevice::VIDEO_DEVICE_FLAG_QUERY_WITH_STATUS_FOR_DECODE_SUPPORT : 0);
    const VkDevice device      = getDeviceSupportingQueue(VK_QUEUE_VIDEO_DECODE_BIT_KHR | VK_QUEUE_TRANSFER_BIT,
                                                          m_videoCodecOperation, videoDeviceFlags);
    const DeviceInterface &vkd = getDeviceDriver();
    const uint32_t queueFamilyIndexDecode     = getQueueFamilyIndexDecode();
    const uint32_t queueFamilyIndexTransfer   = getQueueFamilyIndexTransfer();
    Allocator &allocator                      = getAllocator();
    std::string videoData                     = getTestVideoData();
    VkExtensionProperties stdExtensionVersion = getExtensionVersion(m_videoCodecOperation);

    MovePtr<IfcVulkanVideoDecodeParser> vulkanVideoDecodeParser(
        m_decoder->GetNvFuncs()->createIfcVulkanVideoDecodeParser(m_videoCodecOperation, &stdExtensionVersion));
    bool videoStreamHasEnded = false;
    int32_t framesInQueue    = 0;
    int32_t frameNumber      = 0;
    int32_t framesCorrect    = 0;
    DecodedFrame frames[2]   = {initDecodeFrame(), initDecodeFrame()};
    ESEDemuxer demuxer(videoData, log);

    m_decoder->initialize(m_videoCodecOperation, vkd, device, queueFamilyIndexTransfer, queueFamilyIndexDecode,
                          allocator);

    if (!vulkanVideoDecodeParser->initialize(dynamic_cast<NvidiaVulkanParserVideoDecodeClient *>(m_decoder.get())))
    {
        TCU_THROW(InternalError, "vulkanVideoDecodeParser->initialize()");
    }

    while (framesInQueue > 0 || !videoStreamHasEnded)
    {
        framesInQueue = m_decoder->GetVideoFrameBuffer()->GetDisplayFramesCount();

        while (framesInQueue < 2 && !videoStreamHasEnded)
        {
            if (!videoStreamHasEnded)
            {
                uint8_t *pData            = 0;
                int64_t size              = 0;
                const bool demuxerSuccess = demuxer.Demux(&pData, &size);
                const bool parserSuccess  = vulkanVideoDecodeParser->parseByteStream(pData, size);

                if (!demuxerSuccess || !parserSuccess)
                    videoStreamHasEnded = true;
            }

            framesInQueue = m_decoder->GetVideoFrameBuffer()->GetDisplayFramesCount();
        }

        for (size_t frameNdx = 0; frameNdx < 2; ++frameNdx)
        {
            DecodedFrame &frame = frames[frameNdx];

            m_decoder->GetVideoFrameBuffer()->DequeueDecodedPicture(&frame);
        }

        for (size_t frameNdx = 0; frameNdx < 2; ++frameNdx)
        {
            DecodedFrame &frame          = frames[frameNdx];
            const VkExtent2D imageExtent = frame.pDecodedImage->getExtent();
            const VkImage image          = frame.pDecodedImage->getImage();
            const VkFormat format        = frame.pDecodedImage->getFormat();
            const VkImageLayout layout   = frame.decodedImageLayout;

            if (frame.pictureIndex >= 0)
            {
                const bool assumeCorrect = m_caseDef.testType == TEST_TYPE_H264_DECODE_QUERY_RESULT_WITH_STATUS;
                MovePtr<MultiPlaneImageData> resultImage =
                    getDecodedImage(vkd, device, allocator, image, layout, format, imageExtent,
                                    queueFamilyIndexTransfer, queueFamilyIndexDecode);

                if (assumeCorrect || verifyImage(frameNumber, *resultImage))
                    framesCorrect++;

                m_decoder->ReleaseDisplayedFrame(&frame);
                frameNumber++;

                if (frameNumber >= DE_LENGTH_OF_ARRAY(frames))
                    break;
            }
        }

        if (frameNumber >= DE_LENGTH_OF_ARRAY(frames))
            break;
    }

    if (!vulkanVideoDecodeParser->deinitialize())
        TCU_THROW(InternalError, "vulkanVideoDecodeParser->deinitialize()");

    if (framesCorrect > 0 && framesCorrect == frameNumber)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("Some frames has not been decoded correctly (" + de::toString(framesCorrect) +
                                     "/" + de::toString(frameNumber) + ")");
}

tcu::TestStatus VideoDecodeTestInstance::iterateMultipleFrame(void)
{
    tcu::TestLog &log = m_context.getTestContext().getLog();
    const VideoDevice::VideoDeviceFlags videoDeviceFlags =
        VideoDevice::VIDEO_DEVICE_FLAG_REQUIRE_SYNC2_OR_NOT_SUPPORTED |
        (m_queryWithStatusRequired ? VideoDevice::VIDEO_DEVICE_FLAG_QUERY_WITH_STATUS_FOR_DECODE_SUPPORT : 0);
    const VkDevice device      = getDeviceSupportingQueue(VK_QUEUE_VIDEO_DECODE_BIT_KHR | VK_QUEUE_TRANSFER_BIT,
                                                          m_videoCodecOperation, videoDeviceFlags);
    const DeviceInterface &vkd = getDeviceDriver();
    const uint32_t queueFamilyIndexDecode     = getQueueFamilyIndexDecode();
    const uint32_t queueFamilyIndexTransfer   = getQueueFamilyIndexTransfer();
    Allocator &allocator                      = getAllocator();
    std::string videoData                     = getTestVideoData();
    VkExtensionProperties stdExtensionVersion = getExtensionVersion(m_videoCodecOperation);

    MovePtr<IfcVulkanVideoDecodeParser> vulkanVideoDecodeParser(
        m_decoder->GetNvFuncs()->createIfcVulkanVideoDecodeParser(m_videoCodecOperation, &stdExtensionVersion));
    bool videoStreamHasEnded = false;
    int32_t framesInQueue    = 0;
    int32_t frameNumber      = 0;
    int32_t framesCorrect    = 0;
    vector<DecodedFrame> frames(m_frameCountTrigger, initDecodeFrame());
    ESEDemuxer demuxer(videoData, log);

    m_decoder->initialize(m_videoCodecOperation, vkd, device, queueFamilyIndexTransfer, queueFamilyIndexDecode,
                          allocator);

    if (!vulkanVideoDecodeParser->initialize(dynamic_cast<NvidiaVulkanParserVideoDecodeClient *>(m_decoder.get())))
        TCU_THROW(InternalError, "vulkanVideoDecodeParser->initialize()");

    while (framesInQueue > 0 || !videoStreamHasEnded)
    {
        framesInQueue = m_decoder->GetVideoFrameBuffer()->GetDisplayFramesCount();

        while (framesInQueue < m_frameCountTrigger && !videoStreamHasEnded)
        {
            if (!videoStreamHasEnded)
            {
                uint8_t *pData            = 0;
                int64_t size              = 0;
                const bool demuxerSuccess = demuxer.Demux(&pData, &size);
                const bool parserSuccess  = vulkanVideoDecodeParser->parseByteStream(pData, size);

                if (!demuxerSuccess || !parserSuccess)
                    videoStreamHasEnded = true;
            }

            framesInQueue = m_decoder->GetVideoFrameBuffer()->GetDisplayFramesCount();
        }

        for (int32_t frameNdx = 0; frameNdx < m_frameCountTrigger; ++frameNdx)
        {
            DecodedFrame &frame = frames[frameNdx];

            m_decoder->GetVideoFrameBuffer()->DequeueDecodedPicture(&frame);
        }

        bool success = true;

        for (int32_t frameNdx = 0; frameNdx < m_frameCountTrigger; ++frameNdx)
        {
            DecodedFrame &frame          = frames[frameNdx];
            const VkExtent2D imageExtent = frame.pDecodedImage->getExtent();
            const VkImage image          = frame.pDecodedImage->getImage();
            const VkFormat format        = frame.pDecodedImage->getFormat();
            const VkImageLayout layout   = frame.decodedImageLayout;

            if (frame.pictureIndex >= 0)
            {
                MovePtr<MultiPlaneImageData> resultImage =
                    getDecodedImage(vkd, device, allocator, image, layout, format, imageExtent,
                                    queueFamilyIndexTransfer, queueFamilyIndexDecode);

                if (success && verifyImageMultipleFrame(frameNumber, *resultImage))
                    framesCorrect++;
                else
                    success = false;

                m_decoder->ReleaseDisplayedFrame(&frame);
                frameNumber++;
            }
        }
    }

    if (!vulkanVideoDecodeParser->deinitialize())
        TCU_THROW(InternalError, "vulkanVideoDecodeParser->deinitialize()");

    if (framesCorrect > 0 && framesCorrect == frameNumber)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("Some frames has not been decoded correctly (" + de::toString(framesCorrect) +
                                     "/" + de::toString(frameNumber) + ")");
}

bool VideoDecodeTestInstance::verifyImage(uint32_t frameNumber, const MultiPlaneImageData &multiPlaneImageData)
{
    const tcu::UVec2 imageSize                        = multiPlaneImageData.getSize();
    const uint32_t barCount                           = 10;
    const uint32_t barWidth                           = 16;
    const uint32_t barNum                             = uint32_t(frameNumber) % barCount;
    const uint32_t edgeX                              = imageSize.x() - barWidth * barNum;
    const uint32_t colorNdx                           = uint32_t(frameNumber) / barCount;
    const int32_t refColorsV[]                        = {240, 34, 110};
    const int32_t refColorsY[]                        = {81, 145, 41};
    const int32_t refColorsU[]                        = {90, 0, 0};
    const tcu::UVec4 refColorV                        = tcu::UVec4(refColorsV[colorNdx], 0, 0, 0);
    const tcu::UVec4 refColorY                        = tcu::UVec4(refColorsY[colorNdx], 0, 0, 0);
    const tcu::UVec4 refColorU                        = tcu::UVec4(refColorsU[colorNdx], 0, 0, 0);
    const tcu::UVec4 refBlankV                        = tcu::UVec4(128, 0, 0, 0);
    const tcu::UVec4 refBlankY                        = tcu::UVec4(16, 0, 0, 0);
    const tcu::UVec4 refBlankU                        = tcu::UVec4(128, 0, 0, 0);
    tcu::ConstPixelBufferAccess outPixelBufferAccessV = multiPlaneImageData.getChannelAccess(0);
    tcu::ConstPixelBufferAccess outPixelBufferAccessY = multiPlaneImageData.getChannelAccess(1);
    tcu::ConstPixelBufferAccess outPixelBufferAccessU = multiPlaneImageData.getChannelAccess(2);
    tcu::TextureLevel refPixelBufferV(mapVkFormat(VK_FORMAT_R8_UNORM), imageSize.x(), imageSize.y());
    tcu::TextureLevel refPixelBufferY(mapVkFormat(VK_FORMAT_R8_UNORM), imageSize.x(), imageSize.y());
    tcu::TextureLevel refPixelBufferU(mapVkFormat(VK_FORMAT_R8_UNORM), imageSize.x(), imageSize.y());
    tcu::PixelBufferAccess refPixelBufferAccessV = refPixelBufferV.getAccess();
    tcu::PixelBufferAccess refPixelBufferAccessY = refPixelBufferY.getAccess();
    tcu::PixelBufferAccess refPixelBufferAccessU = refPixelBufferU.getAccess();
    tcu::TestLog &log                            = m_context.getTestContext().getLog();
    const string titleV                          = "Rendered frame " + de::toString(frameNumber) + ". V Component";
    const string titleY                          = "Rendered frame " + de::toString(frameNumber) + ". Y Component";
    const string titleU                          = "Rendered frame " + de::toString(frameNumber) + ". U Component";
    const tcu::UVec4 threshold                   = tcu::UVec4(0, 0, 0, 0);

    for (uint32_t x = 0; x < imageSize.x(); ++x)
    {
        const tcu::UVec4 &colorV = x < edgeX ? refColorV : refBlankV;
        const tcu::UVec4 &colorY = x < edgeX ? refColorY : refBlankY;
        const tcu::UVec4 &colorU = x < edgeX ? refColorU : refBlankU;

        for (uint32_t y = 0; y < imageSize.y(); ++y)
        {
            refPixelBufferAccessV.setPixel(colorV, x, y);
            refPixelBufferAccessY.setPixel(colorY, x, y);
            refPixelBufferAccessU.setPixel(colorU, x, y);
        }
    }

    const bool resultV = tcu::intThresholdCompare(log, titleV.c_str(), "", refPixelBufferAccessV, outPixelBufferAccessV,
                                                  threshold, tcu::COMPARE_LOG_ON_ERROR);
    const bool resultY = tcu::intThresholdCompare(log, titleY.c_str(), "", refPixelBufferAccessY, outPixelBufferAccessY,
                                                  threshold, tcu::COMPARE_LOG_ON_ERROR);
    const bool resultU = tcu::intThresholdCompare(log, titleU.c_str(), "", refPixelBufferAccessU, outPixelBufferAccessU,
                                                  threshold, tcu::COMPARE_LOG_ON_ERROR);

    return resultV && resultY && resultU;
}

bool VideoDecodeTestInstance::verifyImageMultipleFrame(uint32_t frameNumber,
                                                       const MultiPlaneImageData &multiPlaneImageData)
{
    const bool noReferenceTests = m_caseDef.testType == TEST_TYPE_H264_DECODE_I_P_B_13 ||
                                  m_caseDef.testType == TEST_TYPE_H264_DECODE_I_P_B_13_NOT_MATCHING_ORDER ||
                                  m_caseDef.testType == TEST_TYPE_H265_DECODE_I_P_B_13 ||
                                  m_caseDef.testType == TEST_TYPE_H265_DECODE_I_P_B_13_NOT_MATCHING_ORDER;

    if (noReferenceTests)
    {
        const bool h264 = m_caseDef.testType == TEST_TYPE_H264_DECODE_I_P_B_13 ||
                          m_caseDef.testType == TEST_TYPE_H264_DECODE_I_P_B_13_NOT_MATCHING_ORDER;
        const vector<ReferencePixel> referencePixels264{
            ReferencePixel(tcu::IVec3(0, 0, 0), tcu::IVec3(124, 53, 140)),
            ReferencePixel(tcu::IVec3(1920 - 1, 1080 - 1, 0), tcu::IVec3(131, 190, 115)),
            ReferencePixel(tcu::IVec3(0, 0, 12), tcu::IVec3(140, 223, 92)),
            ReferencePixel(tcu::IVec3(1920 - 1, 1080 - 1, 12), tcu::IVec3(138, 166, 98)),
        };
        const vector<ReferencePixel> referencePixels265{
            ReferencePixel(tcu::IVec3(0, 0, 0), tcu::IVec3(124, 55, 144)),
            ReferencePixel(tcu::IVec3(1920 - 1, 1080 - 1, 0), tcu::IVec3(130, 190, 114)),
            ReferencePixel(tcu::IVec3(0, 0, 12), tcu::IVec3(142, 210, 94)),
            ReferencePixel(tcu::IVec3(1920 - 1, 1080 - 1, 12), tcu::IVec3(137, 166, 96)),
        };
        const vector<ReferencePixel> &referencePixels = h264 ? referencePixels264 : referencePixels265;

        return verifyImageMultipleFrameNoReference(frameNumber, multiPlaneImageData, referencePixels);
    }
    else
        return verifyImageMultipleFrameWithReference(frameNumber, multiPlaneImageData);
}

bool VideoDecodeTestInstance::verifyImageMultipleFrameWithReference(uint32_t frameNumber,
                                                                    const MultiPlaneImageData &multiPlaneImageData)
{
    tcu::TestLog &log           = m_context.getTestContext().getLog();
    const bool firstHalf        = frameNumber < 15;
    const bool resolutionChange = m_caseDef.testType == TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE ||
                                  m_caseDef.testType == TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE_DPB;
    const uint32_t k            = resolutionChange ? (firstHalf ? 2 : 1) : 1;
    const uint32_t cellSize     = 16 * k;
    const uint32_t cellCountX   = 11;
    const uint32_t cellCountV   = 9;
    const tcu::UVec2 imageSize  = {cellSize * cellCountX, cellSize * cellCountV};
    const string titleV         = "Rendered frame " + de::toString(frameNumber) + ". V Component";
    const tcu::UVec4 refColor0V = tcu::UVec4(128, 0, 0, 255);
    const tcu::UVec4 refColor1V = tcu::UVec4(128, 0, 0, 255);
    const tcu::UVec4 &refColorV = firstHalf ? refColor0V : refColor1V;
    const tcu::UVec4 &refBlankV = firstHalf ? refColor1V : refColor0V;
    tcu::TextureLevel refPixelBufferV(mapVkFormat(VK_FORMAT_R8_UNORM), imageSize.x(), imageSize.y());
    tcu::PixelBufferAccess refPixelBufferAccessV = refPixelBufferV.getAccess();
    MovePtr<tcu::TextureLevel> outPixelBufferV = convertToRGBASized(multiPlaneImageData.getChannelAccess(0), imageSize);
    tcu::PixelBufferAccess outPixelBufferAccessV = outPixelBufferV->getAccess();
    const string titleY                          = "Rendered frame " + de::toString(frameNumber) + ". Y Component";
    const tcu::UVec4 refColor0Y                  = tcu::UVec4(235, 0, 0, 255);
    const tcu::UVec4 refColor1Y                  = tcu::UVec4(16, 0, 0, 255);
    const tcu::UVec4 &refColorY                  = firstHalf ? refColor0Y : refColor1Y;
    const tcu::UVec4 &refBlankY                  = firstHalf ? refColor1Y : refColor0Y;
    tcu::TextureLevel refPixelBufferY(mapVkFormat(VK_FORMAT_R8_UNORM), imageSize.x(), imageSize.y());
    tcu::PixelBufferAccess refPixelBufferAccessY = refPixelBufferY.getAccess();
    MovePtr<tcu::TextureLevel> outPixelBufferY = convertToRGBASized(multiPlaneImageData.getChannelAccess(1), imageSize);
    tcu::PixelBufferAccess outPixelBufferAccessY = outPixelBufferY->getAccess();
    const string titleU                          = "Rendered frame " + de::toString(frameNumber) + ". U Component";
    const tcu::UVec4 refColor0U                  = tcu::UVec4(128, 0, 0, 255);
    const tcu::UVec4 refColor1U                  = tcu::UVec4(128, 0, 0, 255);
    const tcu::UVec4 &refColorU                  = firstHalf ? refColor0U : refColor1U;
    const tcu::UVec4 &refBlankU                  = firstHalf ? refColor1U : refColor0U;
    tcu::TextureLevel refPixelBufferU(mapVkFormat(VK_FORMAT_R8_UNORM), imageSize.x(), imageSize.y());
    tcu::PixelBufferAccess refPixelBufferAccessU = refPixelBufferU.getAccess();
    MovePtr<tcu::TextureLevel> outPixelBufferU = convertToRGBASized(multiPlaneImageData.getChannelAccess(2), imageSize);
    tcu::PixelBufferAccess outPixelBufferAccessU = outPixelBufferU->getAccess();
    const tcu::UVec4 threshold                   = tcu::UVec4(0, 0, 0, 0);

    for (uint32_t x = 0; x < imageSize.x(); ++x)
        for (uint32_t y = 0; y < imageSize.y(); ++y)
        {
            refPixelBufferAccessV.setPixel(refBlankV, x, y);
            refPixelBufferAccessY.setPixel(refBlankY, x, y);
            refPixelBufferAccessU.setPixel(refBlankU, x, y);
        }

    for (uint32_t cellNdx = 0; cellNdx <= frameNumber % 15; cellNdx++)
    {
        const uint32_t cellOfs = firstHalf ? 0 : 6 * cellSize;
        const uint32_t cellX0  = cellSize * (cellNdx % 5);
        const uint32_t cellV0  = cellSize * (cellNdx / 5) + cellOfs;
        const uint32_t cellX1  = cellX0 + cellSize;
        const uint32_t cellV1  = cellV0 + cellSize;

        for (uint32_t x = cellX0; x < cellX1; ++x)
            for (uint32_t y = cellV0; y < cellV1; ++y)
            {
                refPixelBufferAccessV.setPixel(refColorV, x, y);
                refPixelBufferAccessY.setPixel(refColorY, x, y);
                refPixelBufferAccessU.setPixel(refColorU, x, y);
            }
    }

    const bool resultV = tcu::intThresholdCompare(log, titleV.c_str(), "", refPixelBufferAccessV, outPixelBufferAccessV,
                                                  threshold, tcu::COMPARE_LOG_ON_ERROR);
    const bool resultY = tcu::intThresholdCompare(log, titleY.c_str(), "", refPixelBufferAccessY, outPixelBufferAccessY,
                                                  threshold, tcu::COMPARE_LOG_ON_ERROR);
    const bool resultU = tcu::intThresholdCompare(log, titleU.c_str(), "", refPixelBufferAccessU, outPixelBufferAccessU,
                                                  threshold, tcu::COMPARE_LOG_ON_ERROR);

    return resultV && resultY && resultU;
}

bool VideoDecodeTestInstance::verifyImageMultipleFrameNoReference(uint32_t frameNumber,
                                                                  const MultiPlaneImageData &multiPlaneImageData,
                                                                  const vector<ReferencePixel> &referencePixels)
{
    bool decodeFrame = false;
    for (size_t i = 0; i < referencePixels.size(); i++)
        if (referencePixels[i].first.z() == static_cast<int>(frameNumber))
            decodeFrame = true;

    if (decodeFrame)
    {
        MovePtr<tcu::TextureLevel> outPixelBufferV   = convertToRGBA(multiPlaneImageData.getChannelAccess(0));
        tcu::PixelBufferAccess outPixelBufferAccessV = outPixelBufferV->getAccess();
        MovePtr<tcu::TextureLevel> outPixelBufferY   = convertToRGBA(multiPlaneImageData.getChannelAccess(1));
        tcu::PixelBufferAccess outPixelBufferAccessY = outPixelBufferY->getAccess();
        MovePtr<tcu::TextureLevel> outPixelBufferU   = convertToRGBA(multiPlaneImageData.getChannelAccess(2));
        tcu::PixelBufferAccess outPixelBufferAccessU = outPixelBufferU->getAccess();
        tcu::TestLog &log                            = m_context.getTestContext().getLog();

        log << tcu::TestLog::Message << "TODO: WARNING: ONLY FEW PIXELS ARE CHECKED\n" << tcu::TestLog::EndMessage;

        log << tcu::TestLog::ImageSet("Frame", "") << tcu::TestLog::Image("Result V", "Result V", outPixelBufferAccessV)
            << tcu::TestLog::Image("Result Y", "Result Y", outPixelBufferAccessY)
            << tcu::TestLog::Image("Result U", "Result U", outPixelBufferAccessU) << tcu::TestLog::EndImageSet;

        for (size_t i = 0; i < referencePixels.size(); i++)
            if (referencePixels[i].first.z() == static_cast<int>(frameNumber))
            {
                const tcu::IVec3 &pos  = referencePixels[i].first;
                const tcu::IVec3 &ref  = referencePixels[i].second;
                const tcu::IVec3 value = tcu::IVec3(outPixelBufferAccessV.getPixelInt(pos.x(), pos.y()).x(),
                                                    outPixelBufferAccessY.getPixelInt(pos.x(), pos.y()).x(),
                                                    outPixelBufferAccessU.getPixelInt(pos.x(), pos.y()).x());

                if (value != ref)
                    return false;
            }
    }

    return true;
}

class DualVideoDecodeTestInstance : public VideoBaseTestInstance
{
public:
    DualVideoDecodeTestInstance(Context &context, const CaseDef &data);
    ~DualVideoDecodeTestInstance(void);

    std::string getTestVideoData(bool primary);
    tcu::TestStatus iterate(void);
    bool verifyImage(bool firstClip, int32_t frameNumber, const MultiPlaneImageData &multiPlaneImageData);

protected:
    CaseDef m_caseDef;
    MovePtr<VideoBaseDecoder> m_decoder1;
    MovePtr<VideoBaseDecoder> m_decoder2;
    VkVideoCodecOperationFlagBitsKHR m_videoCodecOperation;
    VkVideoCodecOperationFlagBitsKHR m_videoCodecOperation1;
    VkVideoCodecOperationFlagBitsKHR m_videoCodecOperation2;
    int32_t m_frameCountTrigger;
};

DualVideoDecodeTestInstance::DualVideoDecodeTestInstance(Context &context, const CaseDef &data)
    : VideoBaseTestInstance(context)
    , m_caseDef(data)
    , m_decoder1(new VideoBaseDecoder(context))
    , m_decoder2(new VideoBaseDecoder(context))
    , m_videoCodecOperation(VK_VIDEO_CODEC_OPERATION_NONE_KHR)
    , m_videoCodecOperation1(VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR)
    , m_videoCodecOperation2(VK_VIDEO_CODEC_OPERATION_NONE_KHR)
    , m_frameCountTrigger(10)
{
    const bool randomOrSwapped       = false;
    const bool queryResultWithStatus = false;

    m_decoder1->setDecodeParameters(randomOrSwapped, queryResultWithStatus, m_frameCountTrigger + 1);
    m_decoder2->setDecodeParameters(randomOrSwapped, queryResultWithStatus, m_frameCountTrigger + 1);

    m_videoCodecOperation2 =
        m_caseDef.testType == TEST_TYPE_H264_DECODE_INTERLEAVED ? VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR :
        m_caseDef.testType == TEST_TYPE_H264_BOTH_DECODE_ENCODE_INTERLEAVED ?
                                                                  VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_EXT :
        m_caseDef.testType == TEST_TYPE_H264_H265_DECODE_INTERLEAVED ? VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR :
                                                                       VK_VIDEO_CODEC_OPERATION_NONE_KHR;

    DE_ASSERT(m_videoCodecOperation2 != VK_VIDEO_CODEC_OPERATION_NONE_KHR);

    m_videoCodecOperation =
        static_cast<VkVideoCodecOperationFlagBitsKHR>(m_videoCodecOperation1 | m_videoCodecOperation2);

    if (m_videoCodecOperation2 == VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_EXT)
        TCU_THROW(NotSupportedError, "NOT IMPLEMENTED: REQUIRES ENCODE QUEUE");
}

DualVideoDecodeTestInstance::~DualVideoDecodeTestInstance(void)
{
}

std::string DualVideoDecodeTestInstance::getTestVideoData(bool primary)
{
    switch (m_caseDef.testType)
    {
    case TEST_TYPE_H264_DECODE_INTERLEAVED:
        return primary ? getVideoDataClipA() : getVideoDataClipB();
    case TEST_TYPE_H264_BOTH_DECODE_ENCODE_INTERLEAVED:
        return getVideoDataClipA();
    case TEST_TYPE_H264_H265_DECODE_INTERLEAVED:
        return primary ? getVideoDataClipA() : getVideoDataClipD();
    default:
        TCU_THROW(InternalError, "Unknown testType");
    }
}

tcu::TestStatus DualVideoDecodeTestInstance::iterate(void)
{
    tcu::TestLog &log = m_context.getTestContext().getLog();
    const VideoDevice::VideoDeviceFlags videoDeviceFlags =
        VideoDevice::VIDEO_DEVICE_FLAG_REQUIRE_SYNC2_OR_NOT_SUPPORTED;
    const VkDevice device      = getDeviceSupportingQueue(VK_QUEUE_VIDEO_DECODE_BIT_KHR | VK_QUEUE_TRANSFER_BIT,
                                                          m_videoCodecOperation, videoDeviceFlags);
    const DeviceInterface &vkd = getDeviceDriver();
    const uint32_t queueFamilyIndexDecode   = getQueueFamilyIndexDecode();
    const uint32_t queueFamilyIndexTransfer = getQueueFamilyIndexTransfer();
    Allocator &allocator                    = getAllocator();
    std::string videoData1                  = getTestVideoData(true);
    std::string videoData2                  = getTestVideoData(false);

    VkExtensionProperties stdExtensionVersion1 = getExtensionVersion(m_videoCodecOperation1);
    VkExtensionProperties stdExtensionVersion2 = getExtensionVersion(m_videoCodecOperation2);

    MovePtr<IfcVulkanVideoDecodeParser> vulkanVideoDecodeParser1(
        m_decoder1->GetNvFuncs()->createIfcVulkanVideoDecodeParser(m_videoCodecOperation1, &stdExtensionVersion1));
    MovePtr<IfcVulkanVideoDecodeParser> vulkanVideoDecodeParser2(
        m_decoder2->GetNvFuncs()->createIfcVulkanVideoDecodeParser(m_videoCodecOperation2, &stdExtensionVersion2));
    int32_t frameNumber   = 0;
    int32_t framesCorrect = 0;
    vector<DecodedFrame> frames(m_frameCountTrigger, initDecodeFrame());
    ESEDemuxer demuxer1(videoData1, log);
    ESEDemuxer demuxer2(videoData2, log);

    m_decoder1->initialize(m_videoCodecOperation1, vkd, device, queueFamilyIndexTransfer, queueFamilyIndexDecode,
                           allocator);

    if (!vulkanVideoDecodeParser1->initialize(dynamic_cast<NvidiaVulkanParserVideoDecodeClient *>(m_decoder1.get())))
    {
        TCU_THROW(InternalError, "vulkanVideoDecodeParser->initialize()");
    }

    m_decoder2->initialize(m_videoCodecOperation2, vkd, device, queueFamilyIndexTransfer, queueFamilyIndexDecode,
                           allocator);

    if (!vulkanVideoDecodeParser2->initialize(dynamic_cast<NvidiaVulkanParserVideoDecodeClient *>(m_decoder2.get())))
    {
        TCU_THROW(InternalError, "vulkanVideoDecodeParser->initialize()");
    }

    {
        bool videoStreamHasEnded = false;
        int32_t framesInQueue    = 0;

        while (framesInQueue < m_frameCountTrigger && !videoStreamHasEnded)
        {
            uint8_t *pData            = 0;
            int64_t size              = 0;
            const bool demuxerSuccess = demuxer1.Demux(&pData, &size);
            const bool parserSuccess  = vulkanVideoDecodeParser1->parseByteStream(pData, size);

            if (!demuxerSuccess || !parserSuccess)
                videoStreamHasEnded = true;

            framesInQueue = m_decoder1->GetVideoFrameBuffer()->GetDisplayFramesCount();
        }
    }

    {
        bool videoStreamHasEnded = false;
        int32_t framesInQueue    = 0;

        while (framesInQueue < m_frameCountTrigger && !videoStreamHasEnded)
        {
            uint8_t *pData            = 0;
            int64_t size              = 0;
            const bool demuxerSuccess = demuxer2.Demux(&pData, &size);
            const bool parserSuccess  = vulkanVideoDecodeParser2->parseByteStream(pData, size);

            if (!demuxerSuccess || !parserSuccess)
                videoStreamHasEnded = true;

            framesInQueue = m_decoder2->GetVideoFrameBuffer()->GetDisplayFramesCount();
        }
    }

    m_decoder1->DecodeCachedPictures(m_decoder2.get());

    for (size_t decoderNdx = 0; decoderNdx < 2; ++decoderNdx)
    {
        const bool firstDecoder   = (decoderNdx == 0);
        VideoBaseDecoder *decoder = firstDecoder ? m_decoder1.get() : m_decoder2.get();
        const bool firstClip      = firstDecoder ? true : m_caseDef.testType == TEST_TYPE_H264_H265_DECODE_INTERLEAVED;

        for (int32_t frameNdx = 0; frameNdx < m_frameCountTrigger; ++frameNdx)
        {
            decoder->GetVideoFrameBuffer()->DequeueDecodedPicture(&frames[frameNdx]);

            DecodedFrame &frame          = frames[frameNdx];
            const VkExtent2D imageExtent = frame.pDecodedImage->getExtent();
            const VkImage image          = frame.pDecodedImage->getImage();
            const VkFormat format        = frame.pDecodedImage->getFormat();
            const VkImageLayout layout   = frame.decodedImageLayout;

            if (frame.pictureIndex >= 0)
            {
                MovePtr<MultiPlaneImageData> resultImage =
                    getDecodedImage(vkd, device, allocator, image, layout, format, imageExtent,
                                    queueFamilyIndexTransfer, queueFamilyIndexDecode);

                if (verifyImage(firstClip, frameNdx, *resultImage))
                    framesCorrect++;

                decoder->ReleaseDisplayedFrame(&frame);
                frameNumber++;
            }
        }
    }

    if (!vulkanVideoDecodeParser2->deinitialize())
        TCU_THROW(InternalError, "vulkanVideoDecodeParser->deinitialize()");

    if (!vulkanVideoDecodeParser1->deinitialize())
        TCU_THROW(InternalError, "vulkanVideoDecodeParser->deinitialize()");

    if (framesCorrect > 0 && framesCorrect == frameNumber)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("Some frames has not been decoded correctly (" + de::toString(framesCorrect) +
                                     "/" + de::toString(frameNumber) + ")");
}

bool DualVideoDecodeTestInstance::verifyImage(bool firstClip, int32_t frameNumber,
                                              const MultiPlaneImageData &multiPlaneImageData)
{
    const tcu::UVec2 imageSize  = multiPlaneImageData.getSize();
    const uint32_t k            = firstClip ? 1 : 2;
    const uint32_t barCount     = 10;
    const uint32_t barWidth     = 16 * k;
    const uint32_t barNum       = uint32_t(frameNumber) % barCount;
    const uint32_t edgeX        = imageSize.x() - barWidth * barNum;
    const uint32_t colorNdx     = uint32_t(frameNumber) / barCount;
    const int32_t refColorsV1[] = {240, 34, 110};
    const int32_t refColorsY1[] = {81, 145, 41};
    const int32_t refColorsU1[] = {90, 0, 0};
    const int32_t refColorsV2[] = {16, 0, 0};
    const int32_t refColorsY2[] = {170, 0, 0};
    const int32_t refColorsU2[] = {166, 0, 0};
    const tcu::UVec4 refColorV  = tcu::UVec4(firstClip ? refColorsV1[colorNdx] : refColorsV2[colorNdx], 0, 0, 0);
    const tcu::UVec4 refColorY  = tcu::UVec4(firstClip ? refColorsY1[colorNdx] : refColorsY2[colorNdx], 0, 0, 0);
    const tcu::UVec4 refColorU  = tcu::UVec4(firstClip ? refColorsU1[colorNdx] : refColorsU2[colorNdx], 0, 0, 0);
    const tcu::UVec4 refBlankV  = tcu::UVec4(128, 0, 0, 0);
    const tcu::UVec4 refBlankY  = tcu::UVec4(16, 0, 0, 0);
    const tcu::UVec4 refBlankU  = tcu::UVec4(128, 0, 0, 0);
    tcu::ConstPixelBufferAccess outPixelBufferAccessV = multiPlaneImageData.getChannelAccess(0);
    tcu::ConstPixelBufferAccess outPixelBufferAccessY = multiPlaneImageData.getChannelAccess(1);
    tcu::ConstPixelBufferAccess outPixelBufferAccessU = multiPlaneImageData.getChannelAccess(2);
    tcu::TextureLevel refPixelBufferV(mapVkFormat(VK_FORMAT_R8_UNORM), imageSize.x(), imageSize.y());
    tcu::TextureLevel refPixelBufferY(mapVkFormat(VK_FORMAT_R8_UNORM), imageSize.x(), imageSize.y());
    tcu::TextureLevel refPixelBufferU(mapVkFormat(VK_FORMAT_R8_UNORM), imageSize.x(), imageSize.y());
    tcu::PixelBufferAccess refPixelBufferAccessV = refPixelBufferV.getAccess();
    tcu::PixelBufferAccess refPixelBufferAccessY = refPixelBufferY.getAccess();
    tcu::PixelBufferAccess refPixelBufferAccessU = refPixelBufferU.getAccess();
    tcu::TestLog &log                            = m_context.getTestContext().getLog();
    const string titleV                          = "Rendered frame " + de::toString(frameNumber) + ". V Component";
    const string titleY                          = "Rendered frame " + de::toString(frameNumber) + ". Y Component";
    const string titleU                          = "Rendered frame " + de::toString(frameNumber) + ". U Component";
    const tcu::UVec4 threshold                   = tcu::UVec4(0, 0, 0, 0);

    for (uint32_t x = 0; x < imageSize.x(); ++x)
    {
        const tcu::UVec4 &colorV = (x < edgeX) ? refColorV : refBlankV;
        const tcu::UVec4 &colorY = (x < edgeX) ? refColorY : refBlankY;
        const tcu::UVec4 &colorU = (x < edgeX) ? refColorU : refBlankU;

        for (uint32_t y = 0; y < imageSize.y(); ++y)
        {
            refPixelBufferAccessV.setPixel(colorV, x, y);
            refPixelBufferAccessY.setPixel(colorY, x, y);
            refPixelBufferAccessU.setPixel(colorU, x, y);
        }
    }

    const bool resultV = tcu::intThresholdCompare(log, titleV.c_str(), "", refPixelBufferAccessV, outPixelBufferAccessV,
                                                  threshold, tcu::COMPARE_LOG_ON_ERROR);
    const bool resultY = tcu::intThresholdCompare(log, titleY.c_str(), "", refPixelBufferAccessY, outPixelBufferAccessY,
                                                  threshold, tcu::COMPARE_LOG_ON_ERROR);
    const bool resultU = tcu::intThresholdCompare(log, titleU.c_str(), "", refPixelBufferAccessU, outPixelBufferAccessU,
                                                  threshold, tcu::COMPARE_LOG_ON_ERROR);

    return resultV && resultY && resultU;
}
#endif // #ifdef DE_BUILD_VIDEO
class VideoDecodeTestCase : public TestCase
{
public:
    VideoDecodeTestCase(tcu::TestContext &context, const char *name, const char *desc, const CaseDef caseDef);
    ~VideoDecodeTestCase(void);

    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    CaseDef m_caseDef;
};

VideoDecodeTestCase::VideoDecodeTestCase(tcu::TestContext &context, const char *name, const char *desc,
                                         const CaseDef caseDef)
    : vkt::TestCase(context, name, desc)
    , m_caseDef(caseDef)
{
}

VideoDecodeTestCase::~VideoDecodeTestCase(void)
{
}

void VideoDecodeTestCase::checkSupport(Context &context) const
{
#if (DE_PTR_SIZE != 8)
    // Issue #4253: https://gitlab.khronos.org/Tracker/vk-gl-cts/-/issues/4253
    // These tests rely on external libraries to do the video parsing,
    // and those libraries are only available as 64-bit at this time.
    TCU_THROW(NotSupportedError, "CTS is not built 64-bit so cannot use the 64-bit video parser library");
#endif

    context.requireDeviceFunctionality("VK_KHR_video_queue");
    context.requireDeviceFunctionality("VK_KHR_synchronization2");

    switch (m_caseDef.testType)
    {
    case TEST_TYPE_H264_DECODE_I:
    case TEST_TYPE_H264_DECODE_I_P:
    case TEST_TYPE_H264_DECODE_I_P_NOT_MATCHING_ORDER:
    case TEST_TYPE_H264_DECODE_I_P_B_13:
    case TEST_TYPE_H264_DECODE_I_P_B_13_NOT_MATCHING_ORDER:
    case TEST_TYPE_H264_DECODE_QUERY_RESULT_WITH_STATUS:
    case TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE:
    case TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE_DPB:
    case TEST_TYPE_H264_DECODE_INTERLEAVED:
    case TEST_TYPE_H264_BOTH_DECODE_ENCODE_INTERLEAVED:
    {
        context.requireDeviceFunctionality("VK_KHR_video_decode_h264");
        break;
    }
    case TEST_TYPE_H265_DECODE_I:
    case TEST_TYPE_H265_DECODE_I_P:
    case TEST_TYPE_H265_DECODE_I_P_NOT_MATCHING_ORDER:
    case TEST_TYPE_H265_DECODE_I_P_B_13:
    case TEST_TYPE_H265_DECODE_I_P_B_13_NOT_MATCHING_ORDER:
    {
        context.requireDeviceFunctionality("VK_KHR_video_decode_h265");
        break;
    }
    case TEST_TYPE_H264_H265_DECODE_INTERLEAVED:
    {
        context.requireDeviceFunctionality("VK_KHR_video_decode_h264");
        context.requireDeviceFunctionality("VK_KHR_video_decode_h265");
        break;
    }
    default:
        TCU_THROW(InternalError, "Unknown TestType");
    }
}

TestInstance *VideoDecodeTestCase::createInstance(Context &context) const
{
    // Vulkan video is unsupported for android platform
    switch (m_caseDef.testType)
    {
    case TEST_TYPE_H264_DECODE_I:
    case TEST_TYPE_H264_DECODE_I_P:
    case TEST_TYPE_H264_DECODE_I_P_NOT_MATCHING_ORDER:
    case TEST_TYPE_H264_DECODE_I_P_B_13:
    case TEST_TYPE_H264_DECODE_I_P_B_13_NOT_MATCHING_ORDER:
    case TEST_TYPE_H264_DECODE_QUERY_RESULT_WITH_STATUS:
    case TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE:
    case TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE_DPB:
    case TEST_TYPE_H265_DECODE_I:
    case TEST_TYPE_H265_DECODE_I_P:
    case TEST_TYPE_H265_DECODE_I_P_NOT_MATCHING_ORDER:
    case TEST_TYPE_H265_DECODE_I_P_B_13:
    case TEST_TYPE_H265_DECODE_I_P_B_13_NOT_MATCHING_ORDER:
    {
#ifdef DE_BUILD_VIDEO
        return new VideoDecodeTestInstance(context, m_caseDef);
#endif
    }
    case TEST_TYPE_H264_DECODE_INTERLEAVED:
    case TEST_TYPE_H264_BOTH_DECODE_ENCODE_INTERLEAVED:
    case TEST_TYPE_H264_H265_DECODE_INTERLEAVED:
    {
#ifdef DE_BUILD_VIDEO
        return new DualVideoDecodeTestInstance(context, m_caseDef);
#endif
    }
    default:
        TCU_THROW(InternalError, "Unknown TestType");
    }
#ifndef DE_BUILD_VIDEO
    DE_UNREF(context);
#endif
}

const char *getTestName(const TestType testType)
{
    switch (testType)
    {
    case TEST_TYPE_H264_DECODE_I:
        return "h264_i";
    case TEST_TYPE_H264_DECODE_I_P:
        return "h264_i_p";
    case TEST_TYPE_H264_DECODE_I_P_NOT_MATCHING_ORDER:
        return "h264_i_p_not_matching_order";
    case TEST_TYPE_H264_DECODE_I_P_B_13:
        return "h264_i_p_b_13";
    case TEST_TYPE_H264_DECODE_I_P_B_13_NOT_MATCHING_ORDER:
        return "h264_i_p_b_13_not_matching_order";
    case TEST_TYPE_H264_DECODE_QUERY_RESULT_WITH_STATUS:
        return "h264_query_with_status";
    case TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE:
        return "h264_resolution_change";
    case TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE_DPB:
        return "h264_resolution_change_dpb";
    case TEST_TYPE_H264_DECODE_INTERLEAVED:
        return "h264_interleaved";
    case TEST_TYPE_H264_BOTH_DECODE_ENCODE_INTERLEAVED:
        return "h264_decode_encode_interleaved";
    case TEST_TYPE_H264_H265_DECODE_INTERLEAVED:
        return "h264_h265_interleaved";
    case TEST_TYPE_H265_DECODE_I:
        return "h265_i";
    case TEST_TYPE_H265_DECODE_I_P:
        return "h265_i_p";
    case TEST_TYPE_H265_DECODE_I_P_NOT_MATCHING_ORDER:
        return "h265_i_p_not_matching_order";
    case TEST_TYPE_H265_DECODE_I_P_B_13:
        return "h265_i_p_b_13";
    case TEST_TYPE_H265_DECODE_I_P_B_13_NOT_MATCHING_ORDER:
        return "h265_i_p_b_13_not_matching_order";
    default:
        TCU_THROW(InternalError, "Unknown TestType");
    }
}
} // namespace

tcu::TestCaseGroup *createVideoDecodeTests(tcu::TestContext &testCtx)
{
    MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "decode", "Video decoding session tests"));

    for (int testTypeNdx = 0; testTypeNdx < TEST_TYPE_LAST; ++testTypeNdx)
    {
        const TestType testType = static_cast<TestType>(testTypeNdx);
        const CaseDef caseDef   = {
            testType, //  TestType testType;
        };

        group->addChild(new VideoDecodeTestCase(testCtx, getTestName(testType), "", caseDef));
    }

    return group.release();
}
} // namespace video
} // namespace vkt
