#ifndef _VKTVIDEOTESTUTILS_HPP
#define _VKTVIDEOTESTUTILS_HPP
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

#include "vkDefs.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkPlatform.hpp"
#include "vktTestCase.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "ycbcr/vktYCbCrUtil.hpp"
#include "vkMd5Sum.hpp"

namespace vkt
{
namespace video
{

using namespace vk;
using namespace std;

typedef de::MovePtr<Allocation> AllocationPtr;

struct DeviceContext
{

    Context *context{};
    VideoDevice *vd{};
    VkPhysicalDevice phys{VK_NULL_HANDLE};
    VkDevice device{VK_NULL_HANDLE};
    VkQueue decodeQueue{VK_NULL_HANDLE};
    VkQueue encodeQueue{VK_NULL_HANDLE};
    VkQueue transferQueue{VK_NULL_HANDLE};

    DeviceContext(Context *c, VideoDevice *v, VkPhysicalDevice p = VK_NULL_HANDLE, VkDevice d = VK_NULL_HANDLE,
                  VkQueue decodeQ = VK_NULL_HANDLE, VkQueue encodeQ = VK_NULL_HANDLE,
                  VkQueue transferQ = VK_NULL_HANDLE)
        : context(c)
        , vd(v)
        , phys(p)
        , device(d)
        , decodeQueue(decodeQ)
        , encodeQueue(encodeQ)
        , transferQueue(transferQ)
    {
    }

    void updateDevice(VkPhysicalDevice p, VkDevice d, VkQueue decodeQ, VkQueue encodeQ, VkQueue transferQ)
    {
        phys          = p;
        device        = d;
        decodeQueue   = decodeQ;
        encodeQueue   = encodeQ;
        transferQueue = transferQ;
    }

    const InstanceInterface &getInstanceInterface() const
    {
        return context->getInstanceInterface();
    }
    const DeviceDriver &getDeviceDriver() const
    {
        return vd->getDeviceDriver();
    }
    uint32_t decodeQueueFamilyIdx() const
    {
        return vd->getQueueFamilyIndexDecode();
    }
    uint32_t encodeQueueFamilyIdx() const
    {
        return vd->getQueueFamilyIndexEncode();
    }
    uint32_t transferQueueFamilyIdx() const
    {
        return vd->getQueueFamilyIndexTransfer();
    }
    Allocator &allocator() const
    {
        return vd->getAllocator();
    }
    void waitDecodeQueue() const
    {
        VK_CHECK(getDeviceDriver().queueWaitIdle(decodeQueue));
    }
    void waitEncodeQueue() const
    {
        VK_CHECK(getDeviceDriver().queueWaitIdle(encodeQueue));
    }
    void deviceWaitIdle() const
    {
        VK_CHECK(getDeviceDriver().deviceWaitIdle(device));
    }
};

typedef de::MovePtr<Allocation> AllocationPtr;

bool videoLoggingEnabled();

bool videoLoggingEnabled();

VkVideoDecodeH264ProfileInfoKHR getProfileOperationH264Decode(
    StdVideoH264ProfileIdc stdProfileIdc = STD_VIDEO_H264_PROFILE_IDC_MAIN,
    VkVideoDecodeH264PictureLayoutFlagBitsKHR pictureLayout =
        VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_INTERLACED_INTERLEAVED_LINES_BIT_KHR);
VkVideoEncodeH264ProfileInfoKHR getProfileOperationH264Encode(
    StdVideoH264ProfileIdc stdProfileIdc = STD_VIDEO_H264_PROFILE_IDC_MAIN);
VkVideoDecodeH265ProfileInfoKHR getProfileOperationH265Decode(
    StdVideoH265ProfileIdc stdProfileIdc = STD_VIDEO_H265_PROFILE_IDC_MAIN);
VkVideoEncodeH265ProfileInfoKHR getProfileOperationH265Encode(
    StdVideoH265ProfileIdc stdProfileIdc = STD_VIDEO_H265_PROFILE_IDC_MAIN);
VkVideoDecodeAV1ProfileInfoKHR getProfileOperationAV1Decode(StdVideoAV1Profile stdProfile = STD_VIDEO_AV1_PROFILE_MAIN,
                                                            bool filmgrainSupport         = true);
const VkExtensionProperties *getVideoExtensionProperties(const VkVideoCodecOperationFlagBitsKHR codecOperation);

de::MovePtr<vector<VkFormat>> getSupportedFormats(const InstanceInterface &vk, const VkPhysicalDevice physicalDevice,
                                                  const VkImageUsageFlags imageUsageFlags,
                                                  const VkVideoProfileListInfoKHR *videoProfileList);

void cmdPipelineImageMemoryBarrier2(const DeviceInterface &vk, const VkCommandBuffer commandBuffer,
                                    const VkImageMemoryBarrier2KHR *pImageMemoryBarriers,
                                    const size_t imageMemoryBarrierCount    = 1u,
                                    const VkDependencyFlags dependencyFlags = 0);

void validateVideoProfileList(const InstanceInterface &vk, VkPhysicalDevice physicalDevice,
                              const VkVideoProfileListInfoKHR *videoProfileList, const VkFormat format,
                              const VkImageUsageFlags usage);

de::MovePtr<VkVideoDecodeCapabilitiesKHR> getVideoDecodeCapabilities(void *pNext);

de::MovePtr<VkVideoDecodeH264CapabilitiesKHR> getVideoCapabilitiesExtensionH264D(void);
de::MovePtr<VkVideoEncodeH264CapabilitiesKHR> getVideoCapabilitiesExtensionH264E(void);
de::MovePtr<VkVideoDecodeH265CapabilitiesKHR> getVideoCapabilitiesExtensionH265D(void);
de::MovePtr<VkVideoEncodeH265CapabilitiesKHR> getVideoCapabilitiesExtensionH265E(void);
de::MovePtr<VkVideoEncodeCapabilitiesKHR> getVideoEncodeCapabilities(void *pNext);
de::MovePtr<VkVideoCapabilitiesKHR> getVideoCapabilities(const InstanceInterface &vk, VkPhysicalDevice physicalDevice,
                                                         const VkVideoProfileInfoKHR *videoProfile, void *pNext);

de::MovePtr<VkVideoDecodeH264ProfileInfoKHR> getVideoProfileExtensionH264D(
    StdVideoH264ProfileIdc stdProfileIdc = STD_VIDEO_H264_PROFILE_IDC_MAIN,
    VkVideoDecodeH264PictureLayoutFlagBitsKHR pictureLayout =
        VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_INTERLACED_INTERLEAVED_LINES_BIT_KHR);
de::MovePtr<VkVideoEncodeH264ProfileInfoKHR> getVideoProfileExtensionH264E(
    StdVideoH264ProfileIdc stdProfileIdc = STD_VIDEO_H264_PROFILE_IDC_MAIN);
de::MovePtr<VkVideoDecodeH265ProfileInfoKHR> getVideoProfileExtensionH265D(
    StdVideoH265ProfileIdc stdProfileIdc = STD_VIDEO_H265_PROFILE_IDC_MAIN);
de::MovePtr<VkVideoEncodeH265ProfileInfoKHR> getVideoProfileExtensionH265E(
    StdVideoH265ProfileIdc stdProfileIdc = STD_VIDEO_H265_PROFILE_IDC_MAIN);

de::MovePtr<VkVideoEncodeUsageInfoKHR> getEncodeUsageInfo(void *pNext, VkVideoEncodeUsageFlagsKHR videoUsageHints,
                                                          VkVideoEncodeContentFlagsKHR videoContentHints,
                                                          VkVideoEncodeTuningModeKHR tuningMode);
de::MovePtr<VkVideoProfileInfoKHR> getVideoProfile(
    VkVideoCodecOperationFlagBitsKHR videoCodecOperation, void *pNext,
    VkVideoChromaSubsamplingFlagsKHR chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,
    VkVideoComponentBitDepthFlagsKHR lumaBitDepth      = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,
    VkVideoComponentBitDepthFlagsKHR chromaBitDepth    = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR);
de::MovePtr<VkVideoProfileListInfoKHR> getVideoProfileList(const VkVideoProfileInfoKHR *videoProfile,
                                                           const uint32_t profileCount);

de::MovePtr<VkVideoSessionCreateInfoKHR> getVideoSessionCreateInfo(
    uint32_t queueFamilyIndex, VkVideoSessionCreateFlagsKHR flags, const VkVideoProfileInfoKHR *videoProfile,
    const VkExtent2D &codedExtent = {1920, 1080}, VkFormat pictureFormat = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
    VkFormat referencePicturesFormat = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, uint32_t maxReferencePicturesSlotsCount = 2u,
    uint32_t maxReferencePicturesActiveCount = 2u);

vector<AllocationPtr> getAndBindVideoSessionMemory(const DeviceInterface &vkd, const VkDevice device,
                                                   VkVideoSessionKHR videoSession, Allocator &allocator);

bool validateFormatSupport(const InstanceInterface &vk, VkPhysicalDevice physicalDevice,
                           const VkImageUsageFlags imageUsageFlags, const VkVideoProfileListInfoKHR *videoProfileList,
                           const VkFormat format, const bool throwException = true);

VkImageCreateInfo makeImageCreateInfo(VkFormat format, const VkExtent2D &extent, const VkImageCreateFlags flags,
                                      const uint32_t *queueFamilyIndex, const VkImageUsageFlags usage, void *pNext,
                                      const uint32_t arrayLayers        = 1,
                                      const VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED);

de::MovePtr<StdVideoH264SequenceParameterSet> getStdVideoH264DecodeSequenceParameterSet(
    uint32_t width, uint32_t height, StdVideoH264SequenceParameterSetVui *stdVideoH264SequenceParameterSetVui);
de::MovePtr<StdVideoH264SequenceParameterSet> getStdVideoH264EncodeSequenceParameterSet(
    uint32_t width, uint32_t height, uint8_t maxNumRefs,
    StdVideoH264SequenceParameterSetVui *stdVideoH264SequenceParameterSetVui);

de::MovePtr<StdVideoH264PictureParameterSet> getStdVideoH264DecodePictureParameterSet(void);
de::MovePtr<StdVideoH264PictureParameterSet> getStdVideoH264EncodePictureParameterSet(uint8_t numL0, uint8_t numL1);

de::MovePtr<VkVideoEncodeH264SessionParametersAddInfoKHR> createVideoEncodeH264SessionParametersAddInfoKHR(
    uint32_t stdSPSCount, const StdVideoH264SequenceParameterSet *pStdSPSs, uint32_t stdPPSCount,
    const StdVideoH264PictureParameterSet *pStdPPSs);

de::MovePtr<VkVideoEncodeH264SessionParametersCreateInfoKHR> createVideoEncodeH264SessionParametersCreateInfoKHR(
    const void *pNext, uint32_t maxStdSPSCount, uint32_t maxStdPPSCount,
    const VkVideoEncodeH264SessionParametersAddInfoKHR *pParametersAddInfo);

de::MovePtr<StdVideoH265ProfileTierLevel> getStdVideoH265ProfileTierLevel(StdVideoH265ProfileIdc general_profile_idc,
                                                                          StdVideoH265LevelIdc general_level_idc);
de::MovePtr<StdVideoH265DecPicBufMgr> getStdVideoH265DecPicBufMgr(void);
de::MovePtr<StdVideoH265VideoParameterSet> getStdVideoH265VideoParameterSet(
    const StdVideoH265DecPicBufMgr *pDecPicBufMgr, const StdVideoH265ProfileTierLevel *pProfileTierLevel);

de::MovePtr<StdVideoH265SequenceParameterSetVui> getStdVideoH265SequenceParameterSetVui(uint32_t vui_time_scale);
de::MovePtr<StdVideoH265ShortTermRefPicSet> getStdVideoH265ShortTermRefPicSet(StdVideoH265PictureType pictureType,
                                                                              uint32_t frameIdx,
                                                                              uint32_t consecutiveBFrameCount);
de::MovePtr<StdVideoH265SequenceParameterSet> getStdVideoH265SequenceParameterSet(
    uint32_t width, uint32_t height, VkVideoEncodeH265CtbSizeFlagsKHR ctbSizesFlag,
    VkVideoEncodeH265TransformBlockSizeFlagsKHR transformBlockSizesFlag, const StdVideoH265DecPicBufMgr *pDecPicBufMgr,
    const StdVideoH265ProfileTierLevel *pProfileTierLevel,
    const StdVideoH265SequenceParameterSetVui *pSequenceParameterSetVui);

de::MovePtr<StdVideoH265PictureParameterSet> getStdVideoH265PictureParameterSet(
    const VkVideoEncodeH265CapabilitiesKHR *videoH265CapabilitiesExtension);

de::MovePtr<VkVideoEncodeH265SessionParametersAddInfoKHR> getVideoEncodeH265SessionParametersAddInfoKHR(
    uint32_t stdVPSCount, const StdVideoH265VideoParameterSet *pStdVPSs, uint32_t stdSPSCount,
    const StdVideoH265SequenceParameterSet *pStdSPSs, uint32_t stdPPSCount,
    const StdVideoH265PictureParameterSet *pStdPPSs);

de::MovePtr<VkVideoEncodeH265SessionParametersCreateInfoKHR> getVideoEncodeH265SessionParametersCreateInfoKHR(
    const void *pNext, uint32_t maxStdVPSCount, uint32_t maxStdSPSCount, uint32_t maxStdPPSCount,
    const VkVideoEncodeH265SessionParametersAddInfoKHR *pParametersAddInfo);

de::MovePtr<VkVideoSessionParametersCreateInfoKHR> getVideoSessionParametersCreateInfoKHR(
    const void *pNext, VkVideoSessionKHR videoSession);

de::MovePtr<StdVideoEncodeH264ReferenceInfo> getStdVideoEncodeH264ReferenceInfo(
    StdVideoH264PictureType primary_pic_type, uint32_t FrameNum, int32_t PicOrderCnt);
de::MovePtr<VkVideoEncodeH264DpbSlotInfoKHR> getVideoEncodeH264DpbSlotInfo(
    const StdVideoEncodeH264ReferenceInfo *pStdReferenceInfo);
de::MovePtr<StdVideoEncodeH265ReferenceInfo> getStdVideoEncodeH265ReferenceInfo(StdVideoH265PictureType pic_type,
                                                                                int32_t PicOrderCntVal);
de::MovePtr<VkVideoEncodeH265DpbSlotInfoKHR> getVideoEncodeH265DpbSlotInfo(
    const StdVideoEncodeH265ReferenceInfo *pStdReferenceInfo);

de::MovePtr<StdVideoEncodeH264SliceHeader> getStdVideoEncodeH264SliceHeader(StdVideoH264SliceType sliceType,
                                                                            bool activeOverrideFlag);
de::MovePtr<StdVideoEncodeH265SliceSegmentHeader> getStdVideoEncodeH265SliceSegmentHeader(
    StdVideoH265SliceType sliceType);

de::MovePtr<VkVideoEncodeH264NaluSliceInfoKHR> getVideoEncodeH264NaluSlice(
    StdVideoEncodeH264SliceHeader *stdVideoEncodeH264SliceHeader, const int32_t qpValue = 0);
de::MovePtr<VkVideoEncodeH265NaluSliceSegmentInfoKHR> getVideoEncodeH265NaluSliceSegment(
    StdVideoEncodeH265SliceSegmentHeader *stdVideoEncodeH265SliceSegmentHeader, const int32_t qpValue = 0);

de::MovePtr<StdVideoEncodeH264ReferenceListsInfo> getVideoEncodeH264ReferenceListsInfo(
    uint8_t RefPicList0[STD_VIDEO_H264_MAX_NUM_LIST_REF], uint8_t RefPicList1[STD_VIDEO_H264_MAX_NUM_LIST_REF],
    uint8_t numL0, uint8_t numL1);
de::MovePtr<StdVideoEncodeH265ReferenceListsInfo> getVideoEncodeH265ReferenceListsInfo(
    uint8_t RefPicList0[STD_VIDEO_H265_MAX_NUM_LIST_REF], uint8_t RefPicList1[STD_VIDEO_H265_MAX_NUM_LIST_REF]);

de::MovePtr<StdVideoEncodeH264PictureInfo> getStdVideoEncodeH264PictureInfo(
    StdVideoH264PictureType pictureType, uint32_t frameNum, int32_t PicOrderCnt, uint16_t idr_pic_id,
    const StdVideoEncodeH264ReferenceListsInfo *pRefLists);

de::MovePtr<VkVideoEncodeH264PictureInfoKHR> getVideoEncodeH264PictureInfo(
    const StdVideoEncodeH264PictureInfo *pictureInfo,
    const VkVideoEncodeH264NaluSliceInfoKHR *pNaluSliceEntries = nullptr);

de::MovePtr<StdVideoEncodeH265PictureInfo> getStdVideoEncodeH265PictureInfo(
    StdVideoH265PictureType pictureType, int32_t PicOrderCntVal, const StdVideoEncodeH265ReferenceListsInfo *pRefLists,
    StdVideoH265ShortTermRefPicSet *pShortTermRefPicSet);

de::MovePtr<VkVideoEncodeH265PictureInfoKHR> getVideoEncodeH265PictureInfo(
    const StdVideoEncodeH265PictureInfo *pictureInfo,
    const VkVideoEncodeH265NaluSliceSegmentInfoKHR *pNaluSliceSegmentInfo);

de::MovePtr<VkVideoBeginCodingInfoKHR> getVideoBeginCodingInfo(
    VkVideoSessionKHR videoEncodeSession, VkVideoSessionParametersKHR videoEncodeSessionParameters,
    uint32_t referenceSlotCount = 0, const VkVideoReferenceSlotInfoKHR *pReferenceSlots = nullptr,
    const void *pNext = nullptr);

de::MovePtr<VkVideoInlineQueryInfoKHR> getVideoInlineQueryInfo(VkQueryPool queryPool, uint32_t firstQuery,
                                                               uint32_t queryCount, const void *pNext = nullptr);

de::MovePtr<StdVideoDecodeH264PictureInfo> getStdVideoDecodeH264PictureInfo(void);

de::SharedPtr<VkVideoDecodeH264PictureInfoKHR> getVideoDecodeH264PictureInfo(
    StdVideoDecodeH264PictureInfo *stdPictureInfo, uint32_t *sliceOffset);

de::MovePtr<VkVideoEncodeH264RateControlLayerInfoKHR> getVideoEncodeH264RateControlLayerInfo(
    VkBool32 useMinQp, int32_t minQpI = 0, int32_t minQpP = 0, int32_t minQpB = 0, VkBool32 useMaxQp = 0,
    int32_t maxQpI = 0, int32_t maxQpP = 0, int32_t maxQpB = 0);
de::MovePtr<VkVideoEncodeH265RateControlLayerInfoKHR> getVideoEncodeH265RateControlLayerInfo(VkBool32 useQp,
                                                                                             int32_t qpI = 0,
                                                                                             int32_t qpP = 0,
                                                                                             int32_t qpB = 0);

de::MovePtr<VkVideoEncodeRateControlLayerInfoKHR> getVideoEncodeRateControlLayerInfo(
    const void *pNext, VkVideoEncodeRateControlModeFlagBitsKHR rateControlMode, const uint32_t frameRateNumerator);
de::MovePtr<VkVideoEncodeRateControlInfoKHR> getVideoEncodeRateControlInfo(
    const void *pNext, VkVideoEncodeRateControlModeFlagBitsKHR rateControlMode,
    VkVideoEncodeRateControlLayerInfoKHR *videoEncodeRateControlLayerInfo);

de::MovePtr<VkVideoEncodeH264QualityLevelPropertiesKHR> getvideoEncodeH264QualityLevelProperties(int32_t qpI,
                                                                                                 int32_t qpP,
                                                                                                 int32_t qpB);
de::MovePtr<VkVideoEncodeH265QualityLevelPropertiesKHR> getvideoEncodeH265QualityLevelProperties(int32_t qpI,
                                                                                                 int32_t qpP,
                                                                                                 int32_t qpB);

de::MovePtr<VkVideoEncodeQualityLevelPropertiesKHR> getVideoEncodeQualityLevelProperties(
    void *pNext, VkVideoEncodeRateControlModeFlagBitsKHR preferredRateControlMode);
de::MovePtr<VkPhysicalDeviceVideoEncodeQualityLevelInfoKHR> getPhysicalDeviceVideoEncodeQualityLevelInfo(
    const VkVideoProfileInfoKHR *pVideoProfile, uint32_t qualityLevel);

de::MovePtr<VkVideoEncodeQualityLevelInfoKHR> getVideoEncodeQualityLevelInfo(
    uint32_t qualityLevel, VkVideoEncodeQualityLevelPropertiesKHR *videoEncodeQualityLevelProperties = nullptr);

de::MovePtr<VkVideoCodingControlInfoKHR> getVideoCodingControlInfo(VkVideoCodingControlFlagsKHR flags,
                                                                   const void *pNext = nullptr);

de::MovePtr<VkVideoEncodeInfoKHR> getVideoEncodeInfo(const void *pNext, const VkBuffer &dstBuffer,
                                                     const VkDeviceSize &dstBufferOffset,
                                                     const VkVideoPictureResourceInfoKHR &srcPictureResource,
                                                     const VkVideoReferenceSlotInfoKHR *pSetupReferenceSlot,
                                                     const uint32_t &referenceSlotCount,
                                                     const VkVideoReferenceSlotInfoKHR *pReferenceSlots);

void transferImageOwnership(const DeviceInterface &vkd, VkDevice device, VkImage image,
                            uint32_t transferQueueFamilyIndex, uint32_t encodeQueueFamilyIndex,
                            VkImageLayout newLayout);

VkDeviceSize getBufferSize(VkFormat format, uint32_t width, uint32_t height);

de::MovePtr<vkt::ycbcr::MultiPlaneImageData> getDecodedImage(const DeviceInterface &vkd, VkDevice device,
                                                             Allocator &allocator, VkImage image, VkImageLayout layout,
                                                             VkFormat format, VkExtent2D codedExtent,
                                                             uint32_t queueFamilyIndexTransfer,
                                                             uint32_t queueFamilyIndexDecode);

const VkImageFormatProperties getImageFormatProperties(const InstanceInterface &vk, VkPhysicalDevice physicalDevice,
                                                       const VkVideoProfileListInfoKHR *videoProfileList,
                                                       const VkFormat format, const VkImageUsageFlags usage);

class VideoBaseTestInstance : public TestInstance
{
public:
    explicit VideoBaseTestInstance(Context &context) : TestInstance(context), m_videoDevice(context)
    {
    }

    explicit VideoBaseTestInstance(Context &context, const VkVideoCodecOperationFlagsKHR videoCodecOperation,
                                   const uint32_t videoDeviceFlags)
        : TestInstance(context)
        , m_videoDevice(context, videoCodecOperation, videoDeviceFlags)
    {
    }

    ~VideoBaseTestInstance() override = default;

    VkDevice getDeviceSupportingQueue(
        VkQueueFlags queueFlagsRequired = 0, VkVideoCodecOperationFlagsKHR videoCodecOperationFlags = 0,
        VideoDevice::VideoDeviceFlags videoDeviceFlags = VideoDevice::VIDEO_DEVICE_FLAG_NONE);
    bool createDeviceSupportingQueue(
        VkQueueFlags queueFlagsRequired, VkVideoCodecOperationFlagsKHR videoCodecOperationFlags,
        VideoDevice::VideoDeviceFlags videoDeviceFlags = VideoDevice::VIDEO_DEVICE_FLAG_NONE);
    const DeviceDriver &getDeviceDriver();
    uint32_t getQueueFamilyIndexTransfer();
    uint32_t getQueueFamilyIndexDecode();
    uint32_t getQueueFamilyIndexEncode();
    Allocator &getAllocator();

protected:
    de::MovePtr<vector<uint8_t>> loadVideoData(const string &filename);
    VideoDevice m_videoDevice;
};

typedef enum StdChromaFormatIdc
{
    chroma_format_idc_monochrome = STD_VIDEO_H264_CHROMA_FORMAT_IDC_MONOCHROME,
    chroma_format_idc_420        = STD_VIDEO_H264_CHROMA_FORMAT_IDC_420,
    chroma_format_idc_422        = STD_VIDEO_H264_CHROMA_FORMAT_IDC_422,
    chroma_format_idc_444        = STD_VIDEO_H264_CHROMA_FORMAT_IDC_444,
} StdChromaFormatIdc;

class VkVideoCoreProfile
{
public:
    static bool isValidCodec(VkVideoCodecOperationFlagsKHR videoCodecOperations)
    {
        return (videoCodecOperations &
                (VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR | VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR |
                 VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR | VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR |
                 VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR));
    }

    bool PopulateProfileExt(VkBaseInStructure const *pVideoProfileExt)
    {
        if (m_profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR)
        {
            VkVideoDecodeH264ProfileInfoKHR const *pProfileExt =
                (VkVideoDecodeH264ProfileInfoKHR const *)pVideoProfileExt;
            if (pProfileExt && (pProfileExt->sType != VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PROFILE_INFO_KHR))
            {
                m_profile.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
                return false;
            }
            if (pProfileExt)
            {
                m_h264DecodeProfile = *pProfileExt;
            }
            else
            {
                //  Use default ext profile parameters
                m_h264DecodeProfile.sType         = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PROFILE_INFO_KHR;
                m_h264DecodeProfile.stdProfileIdc = STD_VIDEO_H264_PROFILE_IDC_MAIN;
                m_h264DecodeProfile.pictureLayout =
                    VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_INTERLACED_INTERLEAVED_LINES_BIT_KHR;
            }
            m_profile.pNext           = &m_h264DecodeProfile;
            m_h264DecodeProfile.pNext = NULL;
        }
        else if (m_profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR)
        {
            VkVideoDecodeH265ProfileInfoKHR const *pProfileExt =
                (VkVideoDecodeH265ProfileInfoKHR const *)pVideoProfileExt;
            if (pProfileExt && (pProfileExt->sType != VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PROFILE_INFO_KHR))
            {
                m_profile.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
                return false;
            }
            if (pProfileExt)
            {
                m_h265DecodeProfile = *pProfileExt;
            }
            else
            {
                //  Use default ext profile parameters
                m_h265DecodeProfile.sType         = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PROFILE_INFO_KHR;
                m_h265DecodeProfile.stdProfileIdc = STD_VIDEO_H265_PROFILE_IDC_MAIN;
            }
            m_profile.pNext           = &m_h265DecodeProfile;
            m_h265DecodeProfile.pNext = NULL;
        }
        else if (m_profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR)
        {
            VkVideoDecodeAV1ProfileInfoKHR const *pProfileExt =
                (VkVideoDecodeAV1ProfileInfoKHR const *)pVideoProfileExt;
            if (pProfileExt && (pProfileExt->sType != VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_PROFILE_INFO_KHR))
            {
                m_profile.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
                return false;
            }
            if (pProfileExt)
            {
                m_av1DecodeProfile = *pProfileExt;
            }
            else
            {
                //  Use default ext profile parameters
                m_av1DecodeProfile.sType      = VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_PROFILE_INFO_KHR;
                m_av1DecodeProfile.stdProfile = STD_VIDEO_AV1_PROFILE_MAIN;
            }
            m_profile.pNext          = &m_av1DecodeProfile;
            m_av1DecodeProfile.pNext = NULL;
        }
        else if (m_profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR)
        {
            VkVideoEncodeH264ProfileInfoKHR const *pProfileExt =
                (VkVideoEncodeH264ProfileInfoKHR const *)pVideoProfileExt;
            if (pProfileExt && (pProfileExt->sType != VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_PROFILE_INFO_KHR))
            {
                m_profile.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
                return false;
            }
            if (pProfileExt)
            {
                m_h264EncodeProfile = *pProfileExt;
            }
            else
            {
                //  Use default ext profile parameters
                m_h264DecodeProfile.sType         = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_PROFILE_INFO_KHR;
                m_h264DecodeProfile.stdProfileIdc = STD_VIDEO_H264_PROFILE_IDC_MAIN;
            }
            m_profile.pNext           = &m_h264EncodeProfile;
            m_h264EncodeProfile.pNext = NULL;
        }
        else if (m_profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR)
        {
            VkVideoEncodeH265ProfileInfoKHR const *pProfileExt =
                (VkVideoEncodeH265ProfileInfoKHR const *)pVideoProfileExt;
            if (pProfileExt && (pProfileExt->sType != VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_PROFILE_INFO_KHR))
            {
                m_profile.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
                return false;
            }
            if (pProfileExt)
            {
                m_h265EncodeProfile = *pProfileExt;
            }
            else
            {
                //  Use default ext profile parameters
                m_h265EncodeProfile.sType         = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_PROFILE_INFO_KHR;
                m_h265EncodeProfile.stdProfileIdc = STD_VIDEO_H265_PROFILE_IDC_MAIN;
            }
            m_profile.pNext           = &m_h265EncodeProfile;
            m_h265EncodeProfile.pNext = NULL;
        }
        else
        {
            DE_ASSERT(false && "Unknown codec!");
            return false;
        }

        return true;
    }

    bool InitFromProfile(const VkVideoProfileInfoKHR *pVideoProfile)
    {
        m_profile       = *pVideoProfile;
        m_profile.pNext = NULL;
        return PopulateProfileExt((VkBaseInStructure const *)pVideoProfile->pNext);
    }

    VkVideoCoreProfile(const VkVideoProfileInfoKHR *pVideoProfile) : m_profile(*pVideoProfile)
    {

        PopulateProfileExt((VkBaseInStructure const *)pVideoProfile->pNext);
    }

    VkVideoCoreProfile(VkVideoCodecOperationFlagBitsKHR videoCodecOperation = VK_VIDEO_CODEC_OPERATION_NONE_KHR,
                       VkVideoChromaSubsamplingFlagsKHR chromaSubsampling   = VK_VIDEO_CHROMA_SUBSAMPLING_INVALID_KHR,
                       VkVideoComponentBitDepthFlagsKHR lumaBitDepth        = VK_VIDEO_COMPONENT_BIT_DEPTH_INVALID_KHR,
                       VkVideoComponentBitDepthFlagsKHR chromaBitDepth      = VK_VIDEO_COMPONENT_BIT_DEPTH_INVALID_KHR,
                       uint32_t videoProfileIdc = 0, bool filmGrainPresent = false)
        : m_profile({VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR, NULL, videoCodecOperation, chromaSubsampling,
                     lumaBitDepth, chromaBitDepth})
        , m_profileList({VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR, NULL, 1, &m_profile})
    {
        if (!isValidCodec(videoCodecOperation))
        {
            return;
        }

        VkVideoDecodeH264ProfileInfoKHR decodeH264ProfilesRequest;
        VkVideoDecodeH265ProfileInfoKHR decodeH265ProfilesRequest;
        VkVideoDecodeAV1ProfileInfoKHR decodeAV1ProfilesRequest;
        VkVideoEncodeH264ProfileInfoKHR encodeH264ProfilesRequest;
        VkVideoEncodeH265ProfileInfoKHR encodeH265ProfilesRequest;
        VkBaseInStructure *pVideoProfileExt = NULL;

        if (videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR)
        {
            decodeH264ProfilesRequest.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PROFILE_INFO_KHR;
            decodeH264ProfilesRequest.pNext = NULL;
            decodeH264ProfilesRequest.stdProfileIdc =
                (videoProfileIdc == 0) ? STD_VIDEO_H264_PROFILE_IDC_INVALID : (StdVideoH264ProfileIdc)videoProfileIdc;
            decodeH264ProfilesRequest.pictureLayout =
                VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_INTERLACED_INTERLEAVED_LINES_BIT_KHR;
            pVideoProfileExt = (VkBaseInStructure *)&decodeH264ProfilesRequest;
        }
        else if (videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR)
        {
            decodeH265ProfilesRequest.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PROFILE_INFO_KHR;
            decodeH265ProfilesRequest.pNext = NULL;
            decodeH265ProfilesRequest.stdProfileIdc =
                (videoProfileIdc == 0) ? STD_VIDEO_H265_PROFILE_IDC_INVALID : (StdVideoH265ProfileIdc)videoProfileIdc;
            pVideoProfileExt = (VkBaseInStructure *)&decodeH265ProfilesRequest;
        }
        else if (videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR)
        {
            decodeAV1ProfilesRequest.sType            = VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_PROFILE_INFO_KHR;
            decodeAV1ProfilesRequest.pNext            = NULL;
            decodeAV1ProfilesRequest.stdProfile       = (StdVideoAV1Profile)videoProfileIdc;
            decodeAV1ProfilesRequest.filmGrainSupport = filmGrainPresent;
            pVideoProfileExt                          = (VkBaseInStructure *)&decodeAV1ProfilesRequest;
        }
        else if (videoCodecOperation == VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR)
        {
            encodeH264ProfilesRequest.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_PROFILE_INFO_KHR;
            encodeH264ProfilesRequest.pNext = NULL;
            encodeH264ProfilesRequest.stdProfileIdc =
                (videoProfileIdc == 0) ? STD_VIDEO_H264_PROFILE_IDC_INVALID : (StdVideoH264ProfileIdc)videoProfileIdc;
            pVideoProfileExt = (VkBaseInStructure *)&encodeH264ProfilesRequest;
        }
        else if (videoCodecOperation == VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR)
        {
            encodeH265ProfilesRequest.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_PROFILE_INFO_KHR;
            encodeH265ProfilesRequest.pNext = NULL;
            encodeH265ProfilesRequest.stdProfileIdc =
                (videoProfileIdc == 0) ? STD_VIDEO_H265_PROFILE_IDC_INVALID : (StdVideoH265ProfileIdc)videoProfileIdc;
            pVideoProfileExt = (VkBaseInStructure *)&encodeH265ProfilesRequest;
        }
        else
        {
            DE_ASSERT(false && "Unknown codec!");
            return;
        }

        PopulateProfileExt(pVideoProfileExt);
    }

    VkVideoCodecOperationFlagBitsKHR GetCodecType() const
    {
        return m_profile.videoCodecOperation;
    }

    bool IsEncodeCodecType() const
    {
        return ((m_profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR) ||
                (m_profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR));
    }

    bool IsDecodeCodecType() const
    {
        return ((m_profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR) ||
                (m_profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR));
    }

    bool IsH264() const
    {
        return ((m_profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR) ||
                (m_profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR));
    }

    bool IsH265() const
    {
        return ((m_profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR) ||
                (m_profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR));
    }

    operator bool() const
    {
        return (m_profile.sType == VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR);
    }

    const VkVideoProfileInfoKHR *GetProfile() const
    {
        if (m_profile.sType == VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR)
        {
            return &m_profile;
        }
        else
        {
            return NULL;
        }
    }

    const VkVideoProfileListInfoKHR *GetProfileListInfo() const
    {
        if (m_profileList.sType == VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR)
        {
            return &m_profileList;
        }
        else
        {
            return NULL;
        }
    }

    const VkVideoDecodeH264ProfileInfoKHR *GetDecodeH264Profile() const
    {
        if (m_h264DecodeProfile.sType == VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PROFILE_INFO_KHR)
        {
            return &m_h264DecodeProfile;
        }
        else
        {
            return NULL;
        }
    }

    const VkVideoDecodeH265ProfileInfoKHR *GetDecodeH265Profile() const
    {
        if (m_h265DecodeProfile.sType == VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PROFILE_INFO_KHR)
        {
            return &m_h265DecodeProfile;
        }
        else
        {
            return NULL;
        }
    }

    const VkVideoEncodeH264ProfileInfoKHR *GetEncodeH264Profile() const
    {
        if (m_h264EncodeProfile.sType == VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_PROFILE_INFO_KHR)
        {
            return &m_h264EncodeProfile;
        }
        else
        {
            return NULL;
        }
    }

    const VkVideoEncodeH265ProfileInfoKHR *GetEncodeH265Profile() const
    {
        if (m_h265EncodeProfile.sType == VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_PROFILE_INFO_KHR)
        {
            return &m_h265EncodeProfile;
        }
        else
        {
            return NULL;
        }
    }

    bool copyProfile(const VkVideoCoreProfile &src)
    {
        if (!src)
        {
            return false;
        }

        m_profile       = src.m_profile;
        m_profile.pNext = nullptr;

        m_profileList       = src.m_profileList;
        m_profileList.pNext = nullptr;

        m_profileList.pProfiles = &m_profile;

        PopulateProfileExt((VkBaseInStructure const *)src.m_profile.pNext);

        return true;
    }

    VkVideoCoreProfile(const VkVideoCoreProfile &other)
    {
        copyProfile(other);
    }

    VkVideoCoreProfile &operator=(const VkVideoCoreProfile &other)
    {
        copyProfile(other);
        return *this;
    }

    bool operator==(const VkVideoCoreProfile &other) const
    {
        if (m_profile.videoCodecOperation != other.m_profile.videoCodecOperation)
        {
            return false;
        }

        if (m_profile.chromaSubsampling != other.m_profile.chromaSubsampling)
        {
            return false;
        }

        if (m_profile.lumaBitDepth != other.m_profile.lumaBitDepth)
        {
            return false;
        }

        if (m_profile.chromaBitDepth != other.m_profile.chromaBitDepth)
        {
            return false;
        }

        if (m_profile.pNext != nullptr)
        {
            switch (m_profile.videoCodecOperation)
            {
            case vk::VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
            {
                auto *ours   = (VkVideoDecodeH264ProfileInfoKHR *)m_profile.pNext;
                auto *theirs = (VkVideoDecodeH264ProfileInfoKHR *)other.m_profile.pNext;
                if (ours->sType != theirs->sType)
                    return false;
                if (ours->stdProfileIdc != theirs->stdProfileIdc)
                    return false;
                if (ours->pictureLayout != theirs->pictureLayout)
                    return false;
                break;
            }
            case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
            {
                auto *ours   = (VkVideoDecodeH265ProfileInfoKHR *)m_profile.pNext;
                auto *theirs = (VkVideoDecodeH265ProfileInfoKHR *)other.m_profile.pNext;
                if (ours->sType != theirs->sType)
                    return false;
                if (ours->stdProfileIdc != theirs->stdProfileIdc)
                    return false;
                break;
            }
            case VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR:
            {
                auto *ours   = (VkVideoDecodeAV1ProfileInfoKHR *)m_profile.pNext;
                auto *theirs = (VkVideoDecodeAV1ProfileInfoKHR *)other.m_profile.pNext;
                if (ours->sType != theirs->sType)
                    return false;
                if (ours->stdProfile != theirs->stdProfile)
                    return false;
                break;
            }
            default:
                tcu::die("Unknown codec");
            }
        }

        return true;
    }

    bool operator!=(const VkVideoCoreProfile &other) const
    {
        return !(*this == other);
    }

    VkVideoChromaSubsamplingFlagsKHR GetColorSubsampling() const
    {
        return m_profile.chromaSubsampling;
    }

    StdChromaFormatIdc GetNvColorSubsampling() const
    {
        if (m_profile.chromaSubsampling & VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR)
        {
            return chroma_format_idc_monochrome;
        }
        else if (m_profile.chromaSubsampling & VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR)
        {
            return chroma_format_idc_420;
        }
        else if (m_profile.chromaSubsampling & VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR)
        {
            return chroma_format_idc_422;
        }
        else if (m_profile.chromaSubsampling & VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR)
        {
            return chroma_format_idc_444;
        }

        return chroma_format_idc_monochrome;
    }

    uint32_t GetLumaBitDepthMinus8() const
    {
        if (m_profile.lumaBitDepth & VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR)
        {
            return 8 - 8;
        }
        else if (m_profile.lumaBitDepth & VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR)
        {
            return 10 - 8;
        }
        else if (m_profile.lumaBitDepth & VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR)
        {
            return 12 - 8;
        }
        return 0;
    }

    uint32_t GetChromaBitDepthMinus8() const
    {
        if (m_profile.chromaBitDepth & VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR)
        {
            return 8 - 8;
        }
        else if (m_profile.chromaBitDepth & VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR)
        {
            return 10 - 8;
        }
        else if (m_profile.chromaBitDepth & VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR)
        {
            return 12 - 8;
        }
        return 0;
    }

    bool is16BitFormat() const
    {
        return !!GetLumaBitDepthMinus8() || !!GetChromaBitDepthMinus8();
    }

    static VkFormat CodecGetVkFormat(VkVideoChromaSubsamplingFlagBitsKHR chromaFormatIdc,
                                     VkVideoComponentBitDepthFlagBitsKHR lumaBitDepth, bool isSemiPlanar)
    {
        VkFormat vkFormat = VK_FORMAT_UNDEFINED;
        switch (chromaFormatIdc)
        {
        case VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR:
            switch (lumaBitDepth)
            {
            case VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR:
                vkFormat = VK_FORMAT_R8_UNORM;
                break;
            case VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR:
                vkFormat = VK_FORMAT_R10X6_UNORM_PACK16;
                break;
            case VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR:
                vkFormat = VK_FORMAT_R12X4_UNORM_PACK16;
                break;
            default:
                DE_ASSERT(false);
            }
            break;
        case VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR:
            switch (lumaBitDepth)
            {
            case VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR:
                vkFormat = isSemiPlanar ? VK_FORMAT_G8_B8R8_2PLANE_420_UNORM : VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM;
                break;
            case VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR:
                vkFormat = isSemiPlanar ? VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 :
                                          VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16;
                break;
            case VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR:
                vkFormat = isSemiPlanar ? VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16 :
                                          VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16;
                break;
            default:
                DE_ASSERT(false);
            }
            break;
        case VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR:
            switch (lumaBitDepth)
            {
            case VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR:
                vkFormat = isSemiPlanar ? VK_FORMAT_G8_B8R8_2PLANE_422_UNORM : VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM;
                break;
            case VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR:
                vkFormat = isSemiPlanar ? VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16 :
                                          VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16;
                break;
            case VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR:
                vkFormat = isSemiPlanar ? VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16 :
                                          VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16;
                break;
            default:
                DE_ASSERT(false);
            }
            break;
        case VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR:
            switch (lumaBitDepth)
            {
            case VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR:
                vkFormat = isSemiPlanar ? VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT : VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM;
                break;
            case VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR:
                vkFormat = isSemiPlanar ? VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT :
                                          VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16;
                break;
            case VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR:
                vkFormat = isSemiPlanar ? VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT :
                                          VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16;
                break;
            default:
                DE_ASSERT(false);
            }
            break;
        default:
            DE_ASSERT(false);
        }

        return vkFormat;
    }

    static StdChromaFormatIdc GetVideoChromaFormatFromVkFormat(VkFormat format)
    {
        StdChromaFormatIdc videoChromaFormat = chroma_format_idc_420;
        switch ((uint32_t)format)
        {
        case VK_FORMAT_R8_UNORM:
        case VK_FORMAT_R10X6_UNORM_PACK16:
        case VK_FORMAT_R12X4_UNORM_PACK16:
            videoChromaFormat = chroma_format_idc_monochrome;
            break;

        case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
        case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
            videoChromaFormat = chroma_format_idc_420;
            break;

        case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
        case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
            videoChromaFormat = chroma_format_idc_422;
            break;

        case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
        case VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT:
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT:
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT:
        case VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT:
            videoChromaFormat = chroma_format_idc_444;
            break;
        default:
            DE_ASSERT(false);
        }

        return videoChromaFormat;
    }

    static const char *CodecToName(VkVideoCodecOperationFlagBitsKHR codec)
    {
        switch ((int32_t)codec)
        {
        case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
            return "decode h.264";
        case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
            return "decode h.265";
        case VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR:
            return "decode av1";
        case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR:
            return "encode h.264";
        case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR:
            return "encode h.265";
        default:;
        }
        DE_ASSERT(false && "Unknown codec");
        return "UNKNON";
    }

    static void DumpFormatProfiles(VkVideoProfileInfoKHR *pVideoProfile)
    {
        // formatProfile info based on supported chroma_format_idc
        if (pVideoProfile->chromaSubsampling & VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR)
        {
            std::cout << "MONO, ";
        }
        if (pVideoProfile->chromaSubsampling & VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR)
        {
            std::cout << " 420, ";
        }
        if (pVideoProfile->chromaSubsampling & VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR)
        {
            std::cout << " 422, ";
        }
        if (pVideoProfile->chromaSubsampling & VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR)
        {
            std::cout << " 444, ";
        }

        // Profile info based on max bit_depth_luma_minus8
        if (pVideoProfile->lumaBitDepth & VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR)
        {
            std::cout << "LUMA:   8-bit, ";
        }
        if (pVideoProfile->lumaBitDepth & VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR)
        {
            std::cout << "LUMA:  10-bit, ";
        }
        if (pVideoProfile->lumaBitDepth & VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR)
        {
            std::cout << "LUMA:  12-bit, ";
        }

        // Profile info based on max bit_depth_chroma_minus8
        if (pVideoProfile->chromaBitDepth & VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR)
        {
            std::cout << "CHROMA: 8-bit, ";
        }
        if (pVideoProfile->chromaBitDepth & VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR)
        {
            std::cout << "CHROMA:10-bit, ";
        }
        if (pVideoProfile->chromaBitDepth & VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR)
        {
            std::cout << "CHROMA:12-bit,";
        }
    }

    static void DumpH264Profiles(VkVideoDecodeH264ProfileInfoKHR *pH264Profiles)
    {
        switch (pH264Profiles->stdProfileIdc)
        {
        case STD_VIDEO_H264_PROFILE_IDC_BASELINE:
            std::cout << "BASELINE, ";
            break;
        case STD_VIDEO_H264_PROFILE_IDC_MAIN:
            std::cout << "MAIN, ";
            break;
        case STD_VIDEO_H264_PROFILE_IDC_HIGH:
            std::cout << "HIGH, ";
            break;
        case STD_VIDEO_H264_PROFILE_IDC_HIGH_444_PREDICTIVE:
            std::cout << "HIGH_444_PREDICTIVE, ";
            break;
        default:
            std::cout << "UNKNOWN PROFILE, ";
            break;
        }
    }

    static void DumpH265Profiles(VkVideoDecodeH265ProfileInfoKHR *pH265Profiles)
    {
        switch (pH265Profiles->stdProfileIdc)
        {
        case STD_VIDEO_H265_PROFILE_IDC_MAIN:
            std::cout << "MAIN, ";
            break;
        case STD_VIDEO_H265_PROFILE_IDC_MAIN_10:
            std::cout << "MAIN_10, ";
            break;
        case STD_VIDEO_H265_PROFILE_IDC_MAIN_STILL_PICTURE:
            std::cout << "MAIN_STILL_PICTURE, ";
            break;
        case STD_VIDEO_H265_PROFILE_IDC_FORMAT_RANGE_EXTENSIONS:
            std::cout << "FORMAT_RANGE_EXTENSIONS, ";
            break;
        case STD_VIDEO_H265_PROFILE_IDC_SCC_EXTENSIONS:
            std::cout << "SCC_EXTENSIONS, ";
            break;
        default:
            std::cout << "UNKNOWN PROFILE, ";
            break;
        }
    }

private:
    VkVideoProfileInfoKHR m_profile;
    VkVideoProfileListInfoKHR m_profileList;
    union
    {
        VkVideoDecodeH264ProfileInfoKHR m_h264DecodeProfile;
        VkVideoDecodeH265ProfileInfoKHR m_h265DecodeProfile;
        VkVideoDecodeAV1ProfileInfoKHR m_av1DecodeProfile;
        VkVideoEncodeH264ProfileInfoKHR m_h264EncodeProfile;
        VkVideoEncodeH265ProfileInfoKHR m_h265EncodeProfile;
    };
};

namespace util
{
const char *getVideoCodecString(VkVideoCodecOperationFlagBitsKHR codec);

const char *getVideoChromaFormatString(VkVideoChromaSubsamplingFlagBitsKHR chromaFormat);

VkVideoCodecOperationFlagsKHR getSupportedCodecs(
    DeviceContext &devCtx, uint32_t selectedVideoQueueFamily,
    VkQueueFlags queueFlagsRequired                   = (VK_QUEUE_VIDEO_DECODE_BIT_KHR | VK_QUEUE_VIDEO_ENCODE_BIT_KHR),
    VkVideoCodecOperationFlagsKHR videoCodeOperations = (VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR |
                                                         VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR |
                                                         VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR |
                                                         VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR));

VkResult getVideoFormats(DeviceContext &devCtx, const VkVideoCoreProfile &videoProfile, VkImageUsageFlags imageUsage,
                         uint32_t &formatCount, VkFormat *formats, bool dumpData = false);

VkResult getSupportedVideoFormats(DeviceContext &devCtx, const VkVideoCoreProfile &videoProfile,
                                  VkVideoDecodeCapabilityFlagsKHR capabilityFlags, VkFormat &pictureFormat,
                                  VkFormat &referencePicturesFormat);

const char *codecToName(VkVideoCodecOperationFlagBitsKHR codec);

VkResult getVideoCapabilities(DeviceContext &devCtx, const VkVideoCoreProfile &videoProfile,
                              VkVideoCapabilitiesKHR *pVideoCapabilities);

VkResult getVideoDecodeCapabilities(DeviceContext &devCtx, const VkVideoCoreProfile &videoProfile,
                                    VkVideoCapabilitiesKHR &videoCapabilities,
                                    VkVideoDecodeCapabilitiesKHR &videoDecodeCapabilities);
double PSNR(const std::vector<uint8_t> &img1, const std::vector<uint8_t> &img2);

} // namespace util

} // namespace video
} // namespace vkt

#endif // _VKTVIDEOTESTUTILS_HPP
