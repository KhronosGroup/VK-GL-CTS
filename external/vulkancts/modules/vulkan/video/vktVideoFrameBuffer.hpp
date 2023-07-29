#ifndef _VKTVIDEOFRAMEBUFFER_HPP
#define _VKTVIDEOFRAMEBUFFER_HPP
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
* \brief Video framebuffer
*//*--------------------------------------------------------------------*/
/*
 * Copyright 2020 NVIDIA Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "deDefs.hpp"
#include "vktVideoTestUtils.hpp"
#include "vktBitstreamBufferImpl.hpp"

#include "vkvideo_parser/VulkanVideoParser.h"   // IVulkanVideoFrameBufferParserCb
#include "vkvideo_parser/VulkanVideoParserIf.h" // vkPicBuffBase VkParser*

namespace vkt
{
namespace video
{

using ImagePtr = de::MovePtr<ImageWithMemory>;

class VkImageResource : public VkVideoRefCountBase
{
public:
    static VkResult Create(DeviceContext &vkDevCtx, const VkImageCreateInfo *pImageCreateInfo,
                           VkSharedBaseObj<VkImageResource> &imageResource);

    int32_t AddRef() override
    {
        return ++m_refCount;
    }

    int32_t Release() override
    {
        uint32_t ret = --m_refCount;
        // Destroy the device if ref-count reaches zero
        if (ret == 0)
        {
            delete this;
        }
        return ret;
    }

    VkImage GetImage() const
    {
        return m_imageWithMemory->get();
    }

    const VkImageCreateInfo &GetImageCreateInfo() const
    {
        return m_imageCreateInfo;
    }

private:
    std::atomic<int32_t> m_refCount;
    const VkImageCreateInfo m_imageCreateInfo;
    ImagePtr m_imageWithMemory;

    VkImageResource(DeviceContext &vkDevCtx, const VkImageCreateInfo *pImageCreateInfo)
        : m_refCount(0)
        , m_imageCreateInfo(*pImageCreateInfo)
    {
        m_imageWithMemory =
            ImagePtr(new ImageWithMemory(vkDevCtx.getDeviceDriver(), vkDevCtx.device, vkDevCtx.allocator(),
                                         *pImageCreateInfo, MemoryRequirement::Local));
    }
};

class VkImageResourceView : public VkVideoRefCountBase
{
public:
    static VkResult Create(DeviceContext &vkDevCtx, VkSharedBaseObj<VkImageResource> &imageResource,
                           VkImageSubresourceRange &imageSubresourceRange,
                           VkSharedBaseObj<VkImageResourceView> &imageResourceView);

    virtual int32_t AddRef()
    {
        return ++m_refCount;
    }

    virtual int32_t Release()
    {
        uint32_t ret = --m_refCount;
        // Destroy the device if ref-count reaches zero
        if (ret == 0)
        {
            delete this;
        }
        return ret;
    }

    operator VkImageView() const
    {
        return m_imageView;
    }
    VkImageView GetImageView() const
    {
        return m_imageView;
    }

    const VkSharedBaseObj<VkImageResource> &GetImageResource()
    {
        return m_imageResource;
    }

private:
    std::atomic<int32_t> m_refCount;
    DeviceContext &m_vkDevCtx;
    VkSharedBaseObj<VkImageResource> m_imageResource;
    VkImageView m_imageView;

    VkImageResourceView(DeviceContext &vkDevCtx, VkSharedBaseObj<VkImageResource> &imageResource, VkImageView imageView,
                        VkImageSubresourceRange & /*imageSubresourceRange*/)
        : m_refCount(0)
        , m_vkDevCtx(vkDevCtx)
        , m_imageResource(imageResource)
        , m_imageView(imageView)
    {
    }

    virtual ~VkImageResourceView();
};

struct DecodedFrame
{
    int32_t pictureIndex;
    uint32_t imageLayerIndex; // The layer of a multi-layered images. Always "0" for single layered images
    int32_t displayWidth;
    int32_t displayHeight;
    VkSharedBaseObj<VkImageResourceView> decodedImageView;
    VkSharedBaseObj<VkImageResourceView> outputImageView;
    VkFence frameCompleteFence; // If valid, the fence is signaled when the decoder is done decoding the frame.
    VkFence
        frameConsumerDoneFence; // If valid, the fence is signaled when the consumer (graphics, compute or display) is done using the frame.
    VkSemaphore
        frameCompleteSemaphore; // If valid, the semaphore is signaled when the decoder is done decoding the frame.
    VkSemaphore
        frameConsumerDoneSemaphore; // If valid, the semaphore is signaled when the consumer (graphics, compute or display) is done using the frame.
    VkQueryPool queryPool; // queryPool handle used for the video queries.
    int32_t startQueryId;  // query Id used for the this frame.
    uint32_t numQueries;   // usually one query per frame
    // If multiple queues are available, submittedVideoQueueIndex is the queue index that the video frame was submitted to.
    // if only one queue is available, submittedVideoQueueIndex will always have a value of "0".
    int32_t submittedVideoQueueIndex;
    uint64_t timestamp;
    uint32_t hasConsummerSignalFence     : 1;
    uint32_t hasConsummerSignalSemaphore : 1;
    // For debugging
    int32_t decodeOrder;
    int32_t displayOrder;

    void Reset()
    {
        pictureIndex                = -1;
        imageLayerIndex             = 0;
        displayWidth                = 0;
        displayHeight               = 0;
        decodedImageView            = nullptr;
        outputImageView             = nullptr;
        frameCompleteFence          = VK_NULL_HANDLE;
        frameConsumerDoneFence      = VK_NULL_HANDLE;
        frameCompleteSemaphore      = VK_NULL_HANDLE;
        frameConsumerDoneSemaphore  = VK_NULL_HANDLE;
        queryPool                   = VK_NULL_HANDLE;
        startQueryId                = 0;
        numQueries                  = 0;
        submittedVideoQueueIndex    = 0;
        timestamp                   = 0;
        hasConsummerSignalFence     = false;
        hasConsummerSignalSemaphore = false;
        // For debugging
        decodeOrder  = 0;
        displayOrder = 0;
    }
};

struct DecodedFrameRelease
{
    int32_t pictureIndex;
    VkVideotimestamp timestamp;
    uint32_t hasConsummerSignalFence     : 1;
    uint32_t hasConsummerSignalSemaphore : 1;
    // For debugging
    int32_t decodeOrder;
    int32_t displayOrder;
};

class VulkanVideoFrameBuffer : public IVulkanVideoFrameBufferParserCb
{
public:
    // Synchronization
    struct FrameSynchronizationInfo
    {
        VkFence frameCompleteFence{VK_NULL_HANDLE};
        VkSemaphore frameCompleteSemaphore{VK_NULL_HANDLE};
        VkFence frameConsumerDoneFence{VK_NULL_HANDLE};
        VkSemaphore frameConsumerDoneSemaphore{VK_NULL_HANDLE};
        VkQueryPool queryPool{VK_NULL_HANDLE};
        int32_t startQueryId;
        uint32_t numQueries;
        uint32_t hasFrameCompleteSignalFence     : 1;
        uint32_t hasFrameCompleteSignalSemaphore : 1;
    };

    struct ReferencedObjectsInfo
    {

        // The bitstream Buffer
        const VkVideoRefCountBase *pBitstreamData;
        // PPS
        const VkVideoRefCountBase *pStdPps;
        // SPS
        const VkVideoRefCountBase *pStdSps;
        // VPS
        const VkVideoRefCountBase *pStdVps;

        // AV1
        const VkVideoRefCountBase *pStdAV1Sps;

        ReferencedObjectsInfo(const VkVideoRefCountBase *pBitstreamDataRef, const VkVideoRefCountBase *pStdPpsRef,
                              const VkVideoRefCountBase *pStdSpsRef, const VkVideoRefCountBase *pStdVpsRef,
                              const VkVideoRefCountBase *pStdAV1SpsRef)
            : pBitstreamData(pBitstreamDataRef)
            , pStdPps(pStdPpsRef)
            , pStdSps(pStdSpsRef)
            , pStdVps(pStdVpsRef)
            , pStdAV1Sps(pStdAV1SpsRef)
        {
        }
    };

    struct PictureResourceInfo
    {
        VkImage image;
        VkFormat imageFormat;
        VkImageLayout currentImageLayout;
    };

    virtual int32_t InitImagePool(const VkVideoProfileInfoKHR *pDecodeProfile, uint32_t numImages,
                                  VkFormat dpbImageFormat, VkFormat outImageFormat, const VkExtent2D &maxImageExtent,
                                  VkImageUsageFlags dpbImageUsage, VkImageUsageFlags outImageUsage,
                                  uint32_t queueFamilyIndex, bool useImageArray = false, bool useImageViewArray = false,
                                  bool useSeparateOutputImage = false, bool useLinearOutput = false) = 0;

    virtual int32_t QueuePictureForDecode(int8_t picId, VkParserDecodePictureInfo *pDecodePictureInfo,
                                          ReferencedObjectsInfo *pReferencedObjectsInfo,
                                          FrameSynchronizationInfo *pFrameSynchronizationInfo) = 0;
    virtual int32_t DequeueDecodedPicture(DecodedFrame *pDecodedFrame)                         = 0;
    virtual int32_t ReleaseDisplayedPicture(DecodedFrameRelease **pDecodedFramesRelease,
                                            uint32_t numFramesToRelease)                       = 0;
    virtual int32_t GetDpbImageResourcesByIndex(
        uint32_t numResources, const int8_t *referenceSlotIndexes, VkVideoPictureResourceInfoKHR *pictureResources,
        PictureResourceInfo *pictureResourcesInfo,
        VkImageLayout newDpbImageLayerLayout = VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR) = 0;
    virtual int32_t GetCurrentImageResourceByIndex(
        int8_t referenceSlotIndex, VkVideoPictureResourceInfoKHR *dpbPictureResource,
        PictureResourceInfo *dpbPictureResourceInfo,
        VkImageLayout newDpbImageLayerLayout                 = VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR,
        VkVideoPictureResourceInfoKHR *outputPictureResource = nullptr,
        PictureResourceInfo *outputPictureResourceInfo       = nullptr,
        VkImageLayout newOutputImageLayerLayout              = VK_IMAGE_LAYOUT_MAX_ENUM)               = 0;
    virtual int32_t ReleaseImageResources(uint32_t numResources, const uint32_t *indexes) = 0;
    virtual int32_t SetPicNumInDecodeOrder(int32_t picId, int32_t picNumInDecodeOrder)    = 0;

    virtual int32_t SetPicNumInDisplayOrder(int32_t picId, int32_t picNumInDisplayOrder) = 0;
    virtual size_t GetSize()                                                             = 0;
    virtual size_t GetDisplayedFrameCount() const                                        = 0;

    virtual ~VulkanVideoFrameBuffer()
    {
    }

    static VkResult Create(DeviceContext *devCtx, bool supportsQueries, bool resourcesWithoutProfiles,
                           VkSharedBaseObj<VulkanVideoFrameBuffer> &vkVideoFrameBuffer);
};

} // namespace video
} // namespace vkt

#endif // _VKTVIDEOFRAMEBUFFER_HPP
