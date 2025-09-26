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
 * \brief Video decoding module
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

#include "vktVideoBaseDecodeUtils.hpp"

#include "vkDefs.hpp"
#include "vkStrUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkObjUtil.hpp"

#include <unordered_set>
#include <algorithm>
#include <numeric>
#include <random>

// FIXME: The samples repo is missing this internal include from their H265 decoder
#include "nvVulkanh265ScalingList.h"
#include <VulkanH264Decoder.h>
#include <VulkanH265Decoder.h>
#include <VulkanAV1Decoder.h>
#include <VulkanVP9Decoder.h>

namespace vkt
{
namespace video
{
using namespace vk;
using namespace std;
using de::MovePtr;

static const uint32_t topFieldShift        = 0;
static const uint32_t topFieldMask         = (1 << topFieldShift);
static const uint32_t bottomFieldShift     = 1;
static const uint32_t bottomFieldMask      = (1 << bottomFieldShift);
static const uint32_t fieldIsReferenceMask = (topFieldMask | bottomFieldMask);

// The number of frame surfaces and associated frame data to
// pool. This is an exuberant maximum for testing convenience. No real
// sequence should require this many concurrent surfaces.
static constexpr uint32_t MAX_NUM_DECODE_SURFACES = 32u;

static constexpr uint32_t H26X_MAX_DPB_SLOTS = 16u;
// static constexpr uint32_t AV1_MAX_DPB_SLOTS = 8u;

using VkVideoParser = VkSharedBaseObj<VulkanVideoDecodeParser>;

void createParser(VkVideoCodecOperationFlagBitsKHR codecOperation, std::shared_ptr<VideoBaseDecoder> decoder,
                  VkVideoParser &parser, ElementaryStreamFraming framing)
{
    const VkVideoCapabilitiesKHR *videoCaps     = decoder->getVideoCaps();
    const VkParserInitDecodeParameters pdParams = {
        NV_VULKAN_VIDEO_PARSER_API_VERSION,
        dynamic_cast<VkParserVideoDecodeClient *>(decoder.get()),
        static_cast<uint32_t>(2 * 1024 * 1024), // 2MiB is an arbitrary choice (and pointless for the CTS)
        static_cast<uint32_t>(videoCaps->minBitstreamBufferOffsetAlignment),
        static_cast<uint32_t>(videoCaps->minBitstreamBufferSizeAlignment),
        0,
        0,
        nullptr,
        true,
        framing == ElementaryStreamFraming::AV1_ANNEXB,
    };

    VkExtensionProperties stdExtensionVersion;

    switch (codecOperation)
    {
    case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
    {
        stdExtensionVersion = vk::VkExtensionProperties{VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_EXTENSION_NAME,
                                                        VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_SPEC_VERSION};
        break;
    }
    case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
    {
        stdExtensionVersion = vk::VkExtensionProperties{VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_EXTENSION_NAME,
                                                        VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_SPEC_VERSION};
        break;
    }
    case VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR:
    {
        stdExtensionVersion = vk::VkExtensionProperties{VK_STD_VULKAN_VIDEO_CODEC_AV1_DECODE_EXTENSION_NAME,
                                                        VK_STD_VULKAN_VIDEO_CODEC_AV1_DECODE_SPEC_VERSION};
        break;
    }
    case VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR:
    {
        stdExtensionVersion = vk::VkExtensionProperties{VK_STD_VULKAN_VIDEO_CODEC_VP9_DECODE_EXTENSION_NAME,
                                                        VK_STD_VULKAN_VIDEO_CODEC_VP9_DECODE_SPEC_VERSION};
        break;
    }
    default:
        TCU_FAIL("Unsupported codec type");
    }
    VkResult res = CreateVulkanVideoDecodeParser(codecOperation, (const VkExtensionProperties *)&stdExtensionVersion,
                                                 nullptr, 0, &pdParams, parser);

    if (res != vk::VK_SUCCESS)
    {
        TCU_FAIL("Failed to create a parser");
    }
}

inline vkPicBuffBase *GetPic(VkPicIf *pPicBuf)
{
    return (vkPicBuffBase *)pPicBuf;
}

typedef struct dpbH264Entry
{
    int8_t dpbSlot;
    // bit0(used_for_reference)=1: top field used for reference,
    // bit1(used_for_reference)=1: bottom field used for reference
    uint32_t used_for_reference : 2;
    uint32_t is_long_term       : 1; // 0 = short-term, 1 = long-term
    uint32_t is_non_existing    : 1; // 1 = marked as non-existing
    uint32_t is_field_ref       : 1; // set if unpaired field or complementary field pair
    union
    {
        int16_t FieldOrderCnt[2]; // h.264 : 2*32 [top/bottom].
        int32_t PicOrderCnt;      // HEVC PicOrderCnt
    };
    union
    {
        int16_t FrameIdx; // : 16   short-term: FrameNum (16 bits), long-term:
        // LongTermFrameIdx (4 bits)
        int8_t originalDpbIndex; // Original Dpb source Index.
    };
    vkPicBuffBase *m_picBuff; // internal picture reference

    void setReferenceAndTopBottomField(bool isReference, bool nonExisting, bool isLongTerm, bool isFieldRef,
                                       bool topFieldIsReference, bool bottomFieldIsReference, int16_t frameIdx,
                                       const int16_t fieldOrderCntList[2], vkPicBuffBase *picBuff)
    {
        is_non_existing = nonExisting;
        is_long_term    = isLongTerm;
        is_field_ref    = isFieldRef;
        if (isReference && isFieldRef)
        {
            used_for_reference = (bottomFieldIsReference << bottomFieldShift) | (topFieldIsReference << topFieldShift);
        }
        else
        {
            used_for_reference = isReference ? 3 : 0;
        }

        FrameIdx = frameIdx;

        FieldOrderCnt[0] = fieldOrderCntList[used_for_reference == 2]; // 0: for progressive and top reference; 1: for
        // bottom reference only.
        FieldOrderCnt[1] = fieldOrderCntList[used_for_reference != 1]; // 0: for top reference only;  1: for bottom
        // reference and progressive.

        dpbSlot   = -1;
        m_picBuff = picBuff;
    }

    void setReference(bool isLongTerm, int32_t picOrderCnt, vkPicBuffBase *picBuff)
    {
        is_non_existing    = (picBuff == NULL);
        is_long_term       = isLongTerm;
        is_field_ref       = false;
        used_for_reference = (picBuff != NULL) ? 3 : 0;

        PicOrderCnt = picOrderCnt;

        dpbSlot          = -1;
        m_picBuff        = picBuff;
        originalDpbIndex = -1;
    }

    bool isRef()
    {
        return (used_for_reference != 0);
    }

    StdVideoDecodeH264ReferenceInfoFlags getPictureFlag(bool currentPictureIsProgressive, bool videoLogEnabled)
    {
        StdVideoDecodeH264ReferenceInfoFlags picFlags = StdVideoDecodeH264ReferenceInfoFlags();
        if (videoLogEnabled)
            std::cout << "\t\t Flags: ";

        if (used_for_reference)
        {
            if (videoLogEnabled)
                std::cout << "FRAME_IS_REFERENCE ";
            // picFlags.is_reference = true;
        }

        if (is_long_term)
        {
            if (videoLogEnabled)
                std::cout << "IS_LONG_TERM ";
            picFlags.used_for_long_term_reference = true;
        }
        if (is_non_existing)
        {
            if (videoLogEnabled)
                std::cout << "IS_NON_EXISTING ";
            picFlags.is_non_existing = true;
        }

        if (is_field_ref)
        {
            if (videoLogEnabled)
                std::cout << "IS_FIELD ";
            // picFlags.field_pic_flag = true;
        }

        if (!currentPictureIsProgressive && (used_for_reference & topFieldMask))
        {
            if (videoLogEnabled)
                std::cout << "TOP_FIELD_IS_REF ";
            picFlags.top_field_flag = true;
        }
        if (!currentPictureIsProgressive && (used_for_reference & bottomFieldMask))
        {
            if (videoLogEnabled)
                std::cout << "BOTTOM_FIELD_IS_REF ";
            picFlags.bottom_field_flag = true;
        }

        return picFlags;
    }

    void setH264PictureData(nvVideoDecodeH264DpbSlotInfo *pDpbRefList, VkVideoReferenceSlotInfoKHR *pReferenceSlots,
                            uint32_t dpbEntryIdx, uint32_t dpbSlotIndex, bool currentPictureIsProgressive,
                            bool videoLogEnabled)
    {
        DE_ASSERT(dpbEntryIdx <= H26X_MAX_DPB_SLOTS && dpbSlotIndex <= H26X_MAX_DPB_SLOTS);

        DE_ASSERT((dpbSlotIndex == (uint32_t)dpbSlot) || is_non_existing);
        pReferenceSlots[dpbEntryIdx].sType     = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
        pReferenceSlots[dpbEntryIdx].slotIndex = dpbSlotIndex;
        pReferenceSlots[dpbEntryIdx].pNext     = pDpbRefList[dpbEntryIdx].Init(dpbSlotIndex);

        StdVideoDecodeH264ReferenceInfo *pRefPicInfo = &pDpbRefList[dpbEntryIdx].stdReferenceInfo;
        pRefPicInfo->FrameNum                        = FrameIdx;
        if (videoLogEnabled)
        {
            std::cout << "\tdpbEntryIdx: " << dpbEntryIdx << "dpbSlotIndex: " << dpbSlotIndex
                      << " FrameIdx: " << (int32_t)FrameIdx;
        }
        pRefPicInfo->flags          = getPictureFlag(currentPictureIsProgressive, videoLogEnabled);
        pRefPicInfo->PicOrderCnt[0] = FieldOrderCnt[0];
        pRefPicInfo->PicOrderCnt[1] = FieldOrderCnt[1];
        if (videoLogEnabled)
            std::cout << " fieldOrderCnt[0]: " << pRefPicInfo->PicOrderCnt[0]
                      << " fieldOrderCnt[1]: " << pRefPicInfo->PicOrderCnt[1] << std::endl;
    }

    void setH265PictureData(nvVideoDecodeH265DpbSlotInfo *pDpbSlotInfo, VkVideoReferenceSlotInfoKHR *pReferenceSlots,
                            uint32_t dpbEntryIdx, uint32_t dpbSlotIndex, bool videoLogEnabled)
    {
        DE_ASSERT(dpbEntryIdx <= H26X_MAX_DPB_SLOTS && dpbSlotIndex <= H26X_MAX_DPB_SLOTS);

        DE_ASSERT(isRef());

        DE_ASSERT((dpbSlotIndex == (uint32_t)dpbSlot) || is_non_existing);
        pReferenceSlots[dpbEntryIdx].sType     = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
        pReferenceSlots[dpbEntryIdx].slotIndex = dpbSlotIndex;
        pReferenceSlots[dpbEntryIdx].pNext     = pDpbSlotInfo[dpbEntryIdx].Init(dpbSlotIndex);

        StdVideoDecodeH265ReferenceInfo *pRefPicInfo    = &pDpbSlotInfo[dpbEntryIdx].stdReferenceInfo;
        pRefPicInfo->PicOrderCntVal                     = PicOrderCnt;
        pRefPicInfo->flags.used_for_long_term_reference = is_long_term;

        if (videoLogEnabled)
        {
            std::cout << "\tdpbIndex: " << dpbSlotIndex << " picOrderCntValList: " << PicOrderCnt;

            std::cout << "\t\t Flags: ";
            std::cout << "FRAME IS REFERENCE ";
            if (pRefPicInfo->flags.used_for_long_term_reference)
            {
                std::cout << "IS LONG TERM ";
            }
            std::cout << std::endl;
        }
    }

} dpbH264Entry;

int8_t VideoBaseDecoder::GetPicIdx(vkPicBuffBase *pPicBuf)
{
    if (pPicBuf)
    {
        int32_t picIndex = pPicBuf->m_picIdx;

        if ((picIndex >= 0) && ((uint32_t)picIndex < MAX_NUM_DECODE_SURFACES))
        {
            return (int8_t)picIndex;
        }
    }

    return -1;
}

int8_t VideoBaseDecoder::GetPicIdx(VkPicIf *pPicBuf)
{
    return GetPicIdx(GetPic(pPicBuf));
}

int8_t VideoBaseDecoder::GetPicDpbSlot(int8_t picIndex)
{
    return m_pictureToDpbSlotMap[picIndex];
}

bool VideoBaseDecoder::GetFieldPicFlag(int8_t picIndex)
{
    DE_ASSERT((picIndex >= 0) && ((uint32_t)picIndex < MAX_NUM_DECODE_SURFACES));

    return !!(m_fieldPicFlagMask & (1 << (uint32_t)picIndex));
}

bool VideoBaseDecoder::SetFieldPicFlag(int8_t picIndex, bool fieldPicFlag)
{
    DE_ASSERT((picIndex >= 0) && ((uint32_t)picIndex < MAX_NUM_DECODE_SURFACES));

    bool oldFieldPicFlag = GetFieldPicFlag(picIndex);

    if (fieldPicFlag)
    {
        m_fieldPicFlagMask |= (1 << (uint32_t)picIndex);
    }
    else
    {
        m_fieldPicFlagMask &= ~(1 << (uint32_t)picIndex);
    }

    return oldFieldPicFlag;
}

int8_t VideoBaseDecoder::SetPicDpbSlot(int8_t picIndex, int8_t dpbSlot)
{
    int8_t oldDpbSlot = m_pictureToDpbSlotMap[picIndex];

    m_pictureToDpbSlotMap[picIndex] = dpbSlot;

    if (dpbSlot >= 0)
    {
        m_dpbSlotsMask |= (1 << picIndex);
    }
    else
    {
        m_dpbSlotsMask &= ~(1 << picIndex);

        if (oldDpbSlot >= 0)
        {
            m_dpb.FreeSlot(oldDpbSlot);
        }
    }

    return oldDpbSlot;
}

uint32_t VideoBaseDecoder::ResetPicDpbSlots(uint32_t picIndexSlotValidMask)
{
    uint32_t resetSlotsMask = ~(picIndexSlotValidMask | ~m_dpbSlotsMask);

    for (uint32_t picIdx = 0; (picIdx < MAX_NUM_DECODE_SURFACES) && resetSlotsMask; picIdx++)
    {
        if (resetSlotsMask & (1 << picIdx))
        {
            resetSlotsMask &= ~(1 << picIdx);

            SetPicDpbSlot((int8_t)picIdx, -1);
        }
    }

    return m_dpbSlotsMask;
}

VideoBaseDecoder::VideoBaseDecoder(Parameters &&params)
    : m_deviceContext(params.context)
    , m_profile(*params.profile)
    , m_framesToCheck(params.framesToCheck)
    , m_dpb(3)
    , m_layeredDpb(params.layeredDpb)
    , m_videoFrameBuffer(params.framebuffer)
    , m_decodeFramesData(params.context->getDeviceDriver(), params.context->device,
                         params.context->decodeQueueFamilyIdx())
    , m_resetPictureParametersFrameTriggerHack(params.pictureParameterUpdateTriggerHack)
    , m_forceDisableFilmGrain(params.forceDisableFilmGrain)
    , m_queryResultWithStatus(params.queryDecodeStatus)
    , m_useInlineQueries(params.useInlineQueries)
    , m_useInlineSessionParams(params.useInlineSessionParams)
    , m_resetCodecNoSessionParams(params.resetCodecNoSessionParams)
    , m_resourcesWithoutProfiles(params.resourcesWithoutProfiles)
    , m_outOfOrderDecoding(params.outOfOrderDecoding)
    , m_alwaysRecreateDPB(params.alwaysRecreateDPB)
    , m_intraOnlyDecodingNoSetupRef(params.intraOnlyDecodingNoSetupRef)
    , m_videoLogPrintEnable(params.context->context->getTestContext().getCommandLine().getVideoLogPrint())
{
    std::fill(m_pictureToDpbSlotMap.begin(), m_pictureToDpbSlotMap.end(), -1);
    reinitializeFormatsForProfile(params.profile);
}

void VideoBaseDecoder::reinitializeFormatsForProfile(const VkVideoCoreProfile *profile)
{
    VkResult res;
    res = util::getVideoDecodeCapabilities(*m_deviceContext, *profile, m_videoCaps, m_decodeCaps);
    if (res != VK_SUCCESS)
        TCU_THROW(NotSupportedError, "Implementation does not support this video profile");

    res = util::getSupportedVideoFormats(*m_deviceContext, m_profile, m_decodeCaps.flags, m_outImageFormat,
                                         m_dpbImageFormat);
    if (res != VK_SUCCESS)
        TCU_THROW(NotSupportedError, "Implementation does not have any supported video formats for this profile");

    m_supportedVideoCodecs = util::getSupportedCodecs(
        *m_deviceContext, m_deviceContext->decodeQueueFamilyIdx(), VK_QUEUE_VIDEO_DECODE_BIT_KHR,
        VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR | VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR |
            VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR | VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR);
    DE_ASSERT(m_supportedVideoCodecs != VK_VIDEO_CODEC_OPERATION_NONE_KHR);

    VK_CHECK(BitstreamBufferImpl::Create(m_deviceContext, m_deviceContext->decodeQueueFamilyIdx(), MAX_BUFFER_SIZE,
                                         0,    // Not used
                                         4096, // Be generous, this is a default
                                         m_bitstreamBuffer, m_profile.GetProfileListInfo()));
}

void VideoBaseDecoder::Deinitialize()
{
    const DeviceInterface &vkd = m_deviceContext->getDeviceDriver();
    VkDevice device            = m_deviceContext->device;
    VkQueue queueDecode        = m_deviceContext->decodeQueue;
    VkQueue queueTransfer      = m_deviceContext->transferQueue;

    // Sometimes(eg. scaling lists decoding tesets) decoding finishes before the current
    // picture params are flushed, which leads to a memory leak and validation errors.
    if (m_currentPictureParameters)
        m_currentPictureParameters->FlushPictureParametersQueue(m_videoSession);

    if (queueDecode)
        vkd.queueWaitIdle(queueDecode);

    if (queueTransfer)
        vkd.queueWaitIdle(queueTransfer);

    vkd.deviceWaitIdle(device);

    m_dpb.Deinit();
    m_videoFrameBuffer = nullptr;
    m_decodeFramesData.deinit();
    m_videoSession = nullptr;
}

void VideoBaseDecoder::StartVideoSequence(const VkParserDetectedVideoFormat *pVideoFormat)
{
    VkExtent2D codedExtent = {pVideoFormat->coded_width, pVideoFormat->coded_height};
    DE_ASSERT(pVideoFormat->display_area.right >= 0 && pVideoFormat->display_area.left >= 0 &&
              pVideoFormat->display_area.top >= 0 && pVideoFormat->display_area.bottom >= 0);
    uint32_t displayWidth = static_cast<uint32_t>(pVideoFormat->display_area.right) -
                            static_cast<uint32_t>(pVideoFormat->display_area.left);
    uint32_t displayHeight = static_cast<uint32_t>(pVideoFormat->display_area.bottom) -
                             static_cast<uint32_t>(pVideoFormat->display_area.top);
    VkExtent2D imageExtent = {std::max(codedExtent.width, displayWidth), std::max(codedExtent.height, displayHeight)};
    imageExtent.width      = deAlign32(imageExtent.width, m_videoCaps.pictureAccessGranularity.width);
    imageExtent.height     = deAlign32(imageExtent.height, m_videoCaps.pictureAccessGranularity.height);

    VkVideoCodecOperationFlagBitsKHR detectedVideoCodec = pVideoFormat->codec;

    if (!de::inRange(codedExtent.width, m_videoCaps.minCodedExtent.width, m_videoCaps.maxCodedExtent.width) ||
        !de::inRange(codedExtent.height, m_videoCaps.minCodedExtent.height, m_videoCaps.maxCodedExtent.height))
    {
        stringstream msg;
        msg << "Session coded extent (" << codedExtent.width << ", " << codedExtent.height
            << ") is not within the supported range of: (" << m_videoCaps.minCodedExtent.width << ", "
            << m_videoCaps.minCodedExtent.height << ") -- (" << m_videoCaps.maxCodedExtent.width << ", "
            << m_videoCaps.maxCodedExtent.height << ")";
        TCU_THROW(NotSupportedError, msg.str());
    }

    if (!de::inRange(imageExtent.width, m_videoCaps.minCodedExtent.width, m_videoCaps.maxCodedExtent.width) ||
        !de::inRange(imageExtent.height, m_videoCaps.minCodedExtent.height, m_videoCaps.maxCodedExtent.height))
    {
        stringstream msg;
        msg << "Session image extent (" << imageExtent.width << ", " << imageExtent.height
            << ") is not within the supported range of: (" << m_videoCaps.minCodedExtent.width << ", "
            << m_videoCaps.minCodedExtent.height << ") -- (" << m_videoCaps.maxCodedExtent.width << ", "
            << m_videoCaps.maxCodedExtent.height << ")";
        TCU_THROW(NotSupportedError, msg.str());
    }

    VkVideoCoreProfile videoProfile(detectedVideoCodec, pVideoFormat->chromaSubsampling, pVideoFormat->lumaBitDepth,
                                    pVideoFormat->chromaBitDepth, pVideoFormat->codecProfile,
                                    pVideoFormat->filmGrainUsed);
    m_profile = videoProfile;
    reinitializeFormatsForProfile(&videoProfile);

    DE_ASSERT(((detectedVideoCodec & m_supportedVideoCodecs) != 0));

    if (m_videoFormat.coded_width && m_videoFormat.coded_height)
    {
        // CreateDecoder() has been called before, and now there's possible config change
        m_deviceContext->waitDecodeQueue();
        m_deviceContext->deviceWaitIdle();
    }

    uint32_t maxDpbSlotCount = pVideoFormat->maxNumDpbSlots;

    if (getVideoLogPrintEnable())
    {
        std::cout << std::dec << "Sequence/GOP Information" << std::endl
                  << "\tCodec        : " << util::getVideoCodecString(pVideoFormat->codec) << std::endl
                  << "\tFrame rate   : " << pVideoFormat->frame_rate.numerator << "/"
                  << pVideoFormat->frame_rate.denominator << " = "
                  << ((pVideoFormat->frame_rate.denominator != 0) ?
                          (1.0 * pVideoFormat->frame_rate.numerator / pVideoFormat->frame_rate.denominator) :
                          0.0)
                  << " fps" << std::endl
                  << "\tSequence     : " << (pVideoFormat->progressive_sequence ? "Progressive" : "Interlaced")
                  << std::endl
                  << "\tCoded size   : [" << codedExtent.width << ", " << codedExtent.height << "]" << std::endl
                  << "\tImage size   : [" << imageExtent.width << ", " << imageExtent.height << "]" << std::endl
                  << "\tDisplay area : [" << pVideoFormat->display_area.left << ", " << pVideoFormat->display_area.top
                  << ", " << pVideoFormat->display_area.right << ", " << pVideoFormat->display_area.bottom << "]"
                  << std::endl
                  << "\tChroma       : " << util::getVideoChromaFormatString(pVideoFormat->chromaSubsampling)
                  << std::endl
                  << "\tBit depth    : " << pVideoFormat->bit_depth_luma_minus8 + 8 << std::endl
                  << "\tCodec        : " << VkVideoCoreProfile::CodecToName(detectedVideoCodec) << std::endl
                  << "\tCoded extent : " << codedExtent.width << " x " << codedExtent.height << std::endl
                  << "\tMax DPB slots : " << maxDpbSlotCount << std::endl;
    }

    if (!m_videoSession ||
        !m_videoSession->IsCompatible(m_deviceContext->device, m_deviceContext->decodeQueueFamilyIdx(), &videoProfile,
                                      m_outImageFormat, imageExtent, m_dpbImageFormat, maxDpbSlotCount,
                                      std::min<uint32_t>(maxDpbSlotCount, m_videoCaps.maxActiveReferencePictures)) ||
        m_alwaysRecreateDPB)
    {

        VK_CHECK(VulkanVideoSession::Create(*m_deviceContext, m_deviceContext->decodeQueueFamilyIdx(), &videoProfile,
                                            m_outImageFormat, imageExtent, m_dpbImageFormat, maxDpbSlotCount,
                                            std::min<uint32_t>(maxDpbSlotCount, m_videoCaps.maxActiveReferencePictures),
                                            m_useInlineQueries, m_useInlineSessionParams, m_videoSession));
        // after creating a new video session, we need codec reset.
        m_resetDecoder = true;

        if (m_currentPictureParameters)
        {
            m_currentPictureParameters->FlushPictureParametersQueue(m_videoSession);
        }

        VkImageUsageFlags outImageUsage = (VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                           VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        VkImageUsageFlags dpbImageUsage = VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR;

        if (dpbAndOutputCoincide() && (!pVideoFormat->filmGrainUsed || m_forceDisableFilmGrain))
        {
            dpbImageUsage = VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR | VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR |
                            VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }
        else
        {
            m_useSeparateOutputImages = true;
        }

        if (!(m_videoCaps.flags & VK_VIDEO_CAPABILITY_SEPARATE_REFERENCE_IMAGES_BIT_KHR) && !m_layeredDpb)
        {
            TCU_THROW(NotSupportedError, "separate reference images are not supported");
        }

        if (m_layeredDpb)
        {
            m_useImageArray     = true;
            m_useImageViewArray = true;
        }
        else
        {
            m_useImageArray     = false;
            m_useImageViewArray = false;
        }

        bool useLinearOutput = false;
        int32_t ret          = m_videoFrameBuffer->InitImagePool(
            videoProfile.GetProfile(), MAX_NUM_DECODE_SURFACES, m_dpbImageFormat, m_outImageFormat, imageExtent,
            dpbImageUsage, outImageUsage, m_deviceContext->decodeQueueFamilyIdx(), m_useImageArray, m_useImageViewArray,
            m_useSeparateOutputImages, useLinearOutput);

        DE_ASSERT((uint32_t)ret == MAX_NUM_DECODE_SURFACES);
        DE_UNREF(ret);
        m_decodeFramesData.resize(MAX_NUM_DECODE_SURFACES);
    }
    // Save the original config
    m_videoFormat = *pVideoFormat;
}

int32_t VideoBaseDecoder::BeginSequence(const VkParserSequenceInfo *pnvsi)
{
    // TODO: The base class needs refactoring between the codecs
    bool isAv1  = (pnvsi->eCodec == VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR);
    bool isVP9  = (pnvsi->eCodec == VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR);
    bool isH264 = (pnvsi->eCodec == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR);
    bool isH265 = (pnvsi->eCodec == VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR);
    bool isH26x = isH264 || isH265;
    TCU_CHECK_AND_THROW(InternalError, isAv1 || isH26x || isVP9, "Unsupported codec");

    // TODO: This is not used by anything...
    bool sequenceUpdate = m_nvsi.nMaxWidth != 0 && m_nvsi.nMaxHeight != 0;

    uint32_t maxDpbSlots = 0;
    if (isAv1)
        maxDpbSlots = STD_VIDEO_AV1_NUM_REF_FRAMES + 1; // +1 for the nearly aways present setup slot.
    else if (isH264)
        maxDpbSlots = VkParserPerFrameDecodeParameters::MAX_DPB_REF_AND_SETUP_SLOTS;
    else if (isH265)
        maxDpbSlots = VkParserPerFrameDecodeParameters::MAX_DPB_REF_SLOTS;
    else if (isVP9)
        maxDpbSlots = STD_VIDEO_VP9_NUM_REF_FRAMES + 1; // +1 for the nearly aways present setup slot.

    uint32_t configDpbSlots = (pnvsi->nMinNumDpbSlots > 0) ? pnvsi->nMinNumDpbSlots : maxDpbSlots;
    configDpbSlots          = std::min<uint32_t>(configDpbSlots, maxDpbSlots);

    if (m_intraOnlyDecodingNoSetupRef)
    {
        maxDpbSlots    = 0;
        configDpbSlots = 0;
    }

    bool sequenceReconfigureFormat      = false;
    bool sequenceReconfigureCodedExtent = false;
    if (sequenceUpdate)
    {
        if ((pnvsi->eCodec != m_nvsi.eCodec) || (pnvsi->nChromaFormat != m_nvsi.nChromaFormat) ||
            (pnvsi->uBitDepthLumaMinus8 != m_nvsi.uBitDepthLumaMinus8) ||
            (pnvsi->uBitDepthChromaMinus8 != m_nvsi.uBitDepthChromaMinus8) || (pnvsi->bProgSeq != m_nvsi.bProgSeq))
        {
            sequenceReconfigureFormat = true;
        }

        if ((pnvsi->nCodedWidth != m_nvsi.nCodedWidth) || (pnvsi->nCodedHeight != m_nvsi.nCodedHeight))
        {
            sequenceReconfigureCodedExtent = true;
        }
    }

    m_nvsi = *pnvsi;

    if (isAv1 || isVP9)
    {
        m_nvsi.nMaxWidth  = std::max(m_nvsi.nMaxWidth, m_nvsi.nDisplayWidth);
        m_nvsi.nMaxHeight = std::max(m_nvsi.nMaxHeight, m_nvsi.nDisplayHeight);
    }
    else if (isH26x)
    {
        m_nvsi.nMaxWidth  = m_nvsi.nDisplayWidth;
        m_nvsi.nMaxHeight = m_nvsi.nDisplayHeight;
    }

    VkParserDetectedVideoFormat detectedFormat;
    memset(&detectedFormat, 0, sizeof(detectedFormat));

    detectedFormat.sequenceUpdate                 = sequenceUpdate;
    detectedFormat.sequenceReconfigureFormat      = sequenceReconfigureFormat;
    detectedFormat.sequenceReconfigureCodedExtent = sequenceReconfigureCodedExtent;
    detectedFormat.codec                          = m_nvsi.eCodec;
    detectedFormat.frame_rate.numerator           = NV_FRAME_RATE_NUM(m_nvsi.frameRate);
    detectedFormat.frame_rate.denominator         = NV_FRAME_RATE_DEN(m_nvsi.frameRate);
    detectedFormat.progressive_sequence           = m_nvsi.bProgSeq;
    // Note: it is strange to have the upscaled width here. Tidy all the indirection from the sample app here.
    detectedFormat.coded_width =
        (m_nvsi.eCodec == VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR) ? m_nvsi.nDisplayWidth : m_nvsi.nCodedWidth;
    detectedFormat.coded_height        = m_nvsi.nCodedHeight;
    detectedFormat.display_area.right  = m_nvsi.nDisplayWidth;
    detectedFormat.display_area.bottom = m_nvsi.nDisplayHeight;
    detectedFormat.max_session_width   = m_nvsi.nMaxWidth;
    detectedFormat.max_session_height  = m_nvsi.nMaxHeight;

    if ((StdChromaFormatIdc)pnvsi->nChromaFormat == chroma_format_idc_monochrome)
    {
        detectedFormat.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR;
    }
    else if ((StdChromaFormatIdc)pnvsi->nChromaFormat == chroma_format_idc_420)
    {
        detectedFormat.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
    }
    else if ((StdChromaFormatIdc)pnvsi->nChromaFormat == chroma_format_idc_422)
    {
        detectedFormat.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR;
    }
    else if ((StdChromaFormatIdc)pnvsi->nChromaFormat == chroma_format_idc_444)
    {
        detectedFormat.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR;
    }
    else
    {
        DE_ASSERT(!"Invalid chroma sub-sampling format");
    }

    switch (pnvsi->uBitDepthLumaMinus8)
    {
    case 0:
        detectedFormat.lumaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
        break;
    case 2:
        detectedFormat.lumaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR;
        break;
    case 4:
        detectedFormat.lumaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR;
        break;
    default:
        DE_ASSERT(false);
    }

    switch (pnvsi->uBitDepthChromaMinus8)
    {
    case 0:
        detectedFormat.chromaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
        break;
    case 2:
        detectedFormat.chromaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR;
        break;
    case 4:
        detectedFormat.chromaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR;
        break;
    default:
        DE_ASSERT(false);
    }

    detectedFormat.bit_depth_luma_minus8                             = pnvsi->uBitDepthLumaMinus8;
    detectedFormat.bit_depth_chroma_minus8                           = pnvsi->uBitDepthChromaMinus8;
    detectedFormat.bitrate                                           = pnvsi->lBitrate;
    detectedFormat.display_aspect_ratio.x                            = pnvsi->lDARWidth;
    detectedFormat.display_aspect_ratio.y                            = pnvsi->lDARHeight;
    detectedFormat.video_signal_description.video_format             = pnvsi->lVideoFormat;
    detectedFormat.video_signal_description.video_full_range_flag    = pnvsi->uVideoFullRange;
    detectedFormat.video_signal_description.color_primaries          = pnvsi->lColorPrimaries;
    detectedFormat.video_signal_description.transfer_characteristics = pnvsi->lTransferCharacteristics;
    detectedFormat.video_signal_description.matrix_coefficients      = pnvsi->lMatrixCoefficients;
    detectedFormat.seqhdr_data_length                                = 0; // Not used.
    detectedFormat.minNumDecodeSurfaces                              = pnvsi->nMinNumDecodeSurfaces;
    detectedFormat.maxNumDpbSlots                                    = configDpbSlots;
    detectedFormat.codecProfile                                      = pnvsi->codecProfile;
    detectedFormat.filmGrainUsed                                     = pnvsi->hasFilmGrain;

    // NVIDIA sample app legacy
    StartVideoSequence(&detectedFormat);

    // AV1 and VP9 support cross-sequence referencing.
    if (pnvsi->eCodec == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR ||
        pnvsi->eCodec == VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR)
    {
        m_maxNumDpbSlots = m_dpb.Init(configDpbSlots, false /* reconfigure the DPB size if true */);
        // Ensure the picture map is empited, so that DPB slot management doesn't get confused in-between sequences.
        m_pictureToDpbSlotMap.fill(-1);
    }
    else if (pnvsi->eCodec == VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR ||
             pnvsi->eCodec == VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR)
    {
        if (m_dpb.getMaxSize() < configDpbSlots)
        {
            m_maxNumDpbSlots = m_dpb.Init(configDpbSlots, false);
        }
    }
    else
    {
        TCU_THROW(InternalError, "Codec DPB management not fully implemented");
    }

    return MAX_NUM_DECODE_SURFACES;
}

bool VideoBaseDecoder::AllocPictureBuffer(VkPicIf **ppPicBuf)
{
    *ppPicBuf = m_videoFrameBuffer->ReservePictureBuffer();

    return *ppPicBuf != nullptr;
}

bool VideoBaseDecoder::DecodePicture(VkParserPictureData *pd)
{
    bool result = false;

    if (!pd->pCurrPic)
    {
        return result;
    }

    vkPicBuffBase *pVkPicBuff = GetPic(pd->pCurrPic);
    const int32_t picIdx      = pVkPicBuff ? pVkPicBuff->m_picIdx : -1;
    if (getVideoLogPrintEnable())
        tcu::print("VulkanVideoParser::DecodePicture picIdx=%d progressive=%d\n", picIdx, pd->progressive_frame);

    DE_ASSERT(picIdx < MAX_FRM_CNT);

    VkParserDecodePictureInfo decodePictureInfo = VkParserDecodePictureInfo();
    decodePictureInfo.pictureIndex              = picIdx;
    decodePictureInfo.flags.progressiveFrame    = pd->progressive_frame;
    decodePictureInfo.flags.fieldPic            = pd->field_pic_flag; // 0 = frame picture, 1 = field picture
    decodePictureInfo.flags.repeatFirstField =
        pd->repeat_first_field; // For 3:2 pulldown (number of additional fields, 2 = frame doubling, 4 = frame tripling)
    decodePictureInfo.flags.refPic = pd->ref_pic_flag; // Frame is a reference frame

    // Mark the first field as unpaired Detect unpaired fields
    if (pd->field_pic_flag)
    {
        decodePictureInfo.flags.bottomField =
            pd->bottom_field_flag; // 0 = top field, 1 = bottom field (ignored if field_pic_flag=0)
        decodePictureInfo.flags.secondField   = pd->second_field;    // Second field of a complementary field pair
        decodePictureInfo.flags.topFieldFirst = pd->top_field_first; // Frame pictures only

        if (!pd->second_field)
        {
            decodePictureInfo.flags.unpairedField = true; // Incomplete (half) frame.
        }
        else
        {
            if (decodePictureInfo.flags.unpairedField)
            {
                decodePictureInfo.flags.syncToFirstField = true;
                decodePictureInfo.flags.unpairedField    = false;
            }
        }
    }

    decodePictureInfo.frameSyncinfo.unpairedField    = decodePictureInfo.flags.unpairedField;
    decodePictureInfo.frameSyncinfo.syncToFirstField = decodePictureInfo.flags.syncToFirstField;

    return DecodePicture(pd, pVkPicBuff, &decodePictureInfo);
}

bool VideoBaseDecoder::DecodePicture(VkParserPictureData *pd, vkPicBuffBase *pVkPicBuff,
                                     VkParserDecodePictureInfo *pDecodePictureInfo)
{
    if (!pd->pCurrPic)
    {
        return false;
    }

    const uint32_t PicIdx = GetPicIdx(pd->pCurrPic);
    TCU_CHECK(PicIdx < MAX_FRM_CNT);

    m_cachedDecodeParams.emplace_back(new CachedDecodeParameters);
    auto &cachedParameters = m_cachedDecodeParams.back();
    bool bRet              = false;

    cachedParameters->performCodecReset = m_resetDecoder;
    m_resetDecoder                      = false;

    // Copy the picture data over, taking care to memcpy the heap resources that might get freed on the parser side (we have no guarantees about those pointers)
    cachedParameters->pd = *pd;

    // And again for the decoded picture information, these are all POD types for now.
    cachedParameters->decodedPictureInfo = *pDecodePictureInfo;
    pDecodePictureInfo                   = &cachedParameters->decodedPictureInfo;

    // Now build up the frame's decode parameters and store it in the cache
    cachedParameters->pictureParams                       = VkParserPerFrameDecodeParameters();
    VkParserPerFrameDecodeParameters *pCurrFrameDecParams = &cachedParameters->pictureParams;
    pCurrFrameDecParams->currPicIdx                       = PicIdx;
    pCurrFrameDecParams->numSlices                        = pd->numSlices;
    pCurrFrameDecParams->firstSliceIndex                  = pd->firstSliceIndex;

    // We must copy to properly support out-of-order use cases, since the parser will overwrite this frames data with subsequent frames.
    m_bitstreamBuffer->CopyDataFromBuffer(pd->bitstreamData, pd->bitstreamDataOffset, m_bitstreamBytesProcessed,
                                          pd->bitstreamDataLen);
    pCurrFrameDecParams->bitstreamDataLen    = pd->bitstreamDataLen;
    pCurrFrameDecParams->bitstreamDataOffset = m_bitstreamBytesProcessed;
    pCurrFrameDecParams->bitstreamData       = m_bitstreamBuffer;
    m_bitstreamBytesProcessed += deAlignSize(pd->bitstreamDataLen, m_videoCaps.minBitstreamBufferOffsetAlignment);

    // Setup the frame references
    auto &referenceSlots                = cachedParameters->referenceSlots;
    auto &setupReferenceSlot            = cachedParameters->setupReferenceSlot;
    setupReferenceSlot.sType            = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
    setupReferenceSlot.pPictureResource = nullptr;
    setupReferenceSlot.slotIndex        = -1;

    pCurrFrameDecParams->decodeFrameInfo.sType                    = VK_STRUCTURE_TYPE_VIDEO_DECODE_INFO_KHR;
    pCurrFrameDecParams->decodeFrameInfo.dstPictureResource.sType = VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR;
    pCurrFrameDecParams->dpbSetupPictureResource.sType            = VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR;

    if (m_profile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR)
    {
        const VkParserH264PictureData *const pin               = &pd->CodecSpecific.h264;
        cachedParameters->h264PicParams                        = nvVideoH264PicParameters();
        VkVideoDecodeH264PictureInfoKHR *h264PictureInfo       = &cachedParameters->h264PicParams.pictureInfo;
        nvVideoDecodeH264DpbSlotInfo *h264DpbReferenceList     = cachedParameters->h264PicParams.dpbRefList;
        StdVideoDecodeH264PictureInfo *h264StandardPictureInfo = &cachedParameters->h264PicParams.stdPictureInfo;

        pCurrFrameDecParams->pStdPps = pin->pStdPps;
        pCurrFrameDecParams->pStdSps = pin->pStdSps;
        pCurrFrameDecParams->pStdVps = nullptr;

        h264PictureInfo->pStdPictureInfo           = &cachedParameters->h264PicParams.stdPictureInfo;
        h264PictureInfo->sType                     = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PICTURE_INFO_KHR;
        h264PictureInfo->pNext                     = nullptr;
        pCurrFrameDecParams->decodeFrameInfo.pNext = h264PictureInfo;

        h264StandardPictureInfo->pic_parameter_set_id = pin->pic_parameter_set_id; // PPS ID
        h264StandardPictureInfo->seq_parameter_set_id = pin->seq_parameter_set_id; // SPS ID;
        h264StandardPictureInfo->frame_num            = (uint16_t)pin->frame_num;
        h264PictureInfo->sliceCount                   = pd->numSlices;

        uint32_t maxSliceCount = 0;
        DE_ASSERT(pd->firstSliceIndex == 0); // No slice and MV modes are supported yet
        h264PictureInfo->pSliceOffsets = pd->bitstreamData->GetStreamMarkersPtr(pd->firstSliceIndex, maxSliceCount);
        DE_ASSERT(maxSliceCount == pd->numSlices);

        StdVideoDecodeH264PictureInfoFlags currPicFlags = StdVideoDecodeH264PictureInfoFlags();
        currPicFlags.is_intra                           = (pd->intra_pic_flag != 0);
        // 0 = frame picture, 1 = field picture
        if (pd->field_pic_flag)
        {
            // 0 = top field, 1 = bottom field (ignored if field_pic_flag = 0)
            currPicFlags.field_pic_flag = true;
            if (pd->bottom_field_flag)
            {
                currPicFlags.bottom_field_flag = true;
            }
        }
        // Second field of a complementary field pair
        if (pd->second_field)
        {
            currPicFlags.complementary_field_pair = true;
        }
        // Frame is a reference frame
        if (pd->ref_pic_flag)
        {
            currPicFlags.is_reference = true;
        }
        h264StandardPictureInfo->flags = currPicFlags;
        if (!pd->field_pic_flag)
        {
            h264StandardPictureInfo->PicOrderCnt[0] = pin->CurrFieldOrderCnt[0];
            h264StandardPictureInfo->PicOrderCnt[1] = pin->CurrFieldOrderCnt[1];
        }
        else
        {
            h264StandardPictureInfo->PicOrderCnt[pd->bottom_field_flag] = pin->CurrFieldOrderCnt[pd->bottom_field_flag];
        }

        const uint32_t maxDpbInputSlots = sizeof(pin->dpb) / sizeof(pin->dpb[0]);
        pCurrFrameDecParams->numGopReferenceSlots =
            FillDpbH264State(pd, pin->dpb, maxDpbInputSlots, h264DpbReferenceList,
                             VkParserPerFrameDecodeParameters::MAX_DPB_REF_SLOTS, // 16 reference pictures
                             referenceSlots, pCurrFrameDecParams->pGopReferenceImagesIndexes,
                             h264StandardPictureInfo->flags, &setupReferenceSlot.slotIndex);

        DE_ASSERT(!pd->ref_pic_flag || (setupReferenceSlot.slotIndex >= 0));

        // TODO: Dummy struct to silence validation. The root problem is that the dpb map doesn't take account of the setup slot,
        // for some reason... So we can't use the existing logic to setup the picture flags and frame number from the dpbEntry
        // class.
        cachedParameters->h264SlotInfo.sType             = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_DPB_SLOT_INFO_KHR;
        cachedParameters->h264SlotInfo.pNext             = nullptr;
        cachedParameters->h264SlotInfo.pStdReferenceInfo = &cachedParameters->h264RefInfo;

        if (setupReferenceSlot.slotIndex >= 0)
        {
            setupReferenceSlot.pPictureResource                      = &pCurrFrameDecParams->dpbSetupPictureResource;
            setupReferenceSlot.pNext                                 = &cachedParameters->h264SlotInfo;
            pCurrFrameDecParams->decodeFrameInfo.pSetupReferenceSlot = &setupReferenceSlot;
        }
        if (pCurrFrameDecParams->numGopReferenceSlots)
        {
            DE_ASSERT(pCurrFrameDecParams->numGopReferenceSlots <=
                      (int32_t)VkParserPerFrameDecodeParameters::MAX_DPB_REF_SLOTS);
            for (uint32_t dpbEntryIdx = 0; dpbEntryIdx < (uint32_t)pCurrFrameDecParams->numGopReferenceSlots;
                 dpbEntryIdx++)
            {
                pCurrFrameDecParams->pictureResources[dpbEntryIdx].sType =
                    VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR;
                referenceSlots[dpbEntryIdx].pPictureResource = &pCurrFrameDecParams->pictureResources[dpbEntryIdx];
                DE_ASSERT(h264DpbReferenceList[dpbEntryIdx].IsReference());
            }

            pCurrFrameDecParams->decodeFrameInfo.pReferenceSlots    = referenceSlots;
            pCurrFrameDecParams->decodeFrameInfo.referenceSlotCount = pCurrFrameDecParams->numGopReferenceSlots;
        }
        else
        {
            pCurrFrameDecParams->decodeFrameInfo.pReferenceSlots    = NULL;
            pCurrFrameDecParams->decodeFrameInfo.referenceSlotCount = 0;
        }

        pDecodePictureInfo->displayWidth  = m_nvsi.nDisplayWidth;
        pDecodePictureInfo->displayHeight = m_nvsi.nDisplayHeight;

        pVkPicBuff->decodeSuperResWidth = pVkPicBuff->decodeWidth = m_nvsi.nDisplayWidth;
        pVkPicBuff->decodeHeight                                  = m_nvsi.nDisplayHeight;
    }
    else if (m_profile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR)
    {
        const VkParserHevcPictureData *const pin       = &pd->CodecSpecific.hevc;
        cachedParameters->h265PicParams                = nvVideoH265PicParameters();
        VkVideoDecodeH265PictureInfoKHR *pPictureInfo  = &cachedParameters->h265PicParams.pictureInfo;
        StdVideoDecodeH265PictureInfo *pStdPictureInfo = &cachedParameters->h265PicParams.stdPictureInfo;
        nvVideoDecodeH265DpbSlotInfo *pDpbRefList      = cachedParameters->h265PicParams.dpbRefList;

        pCurrFrameDecParams->pStdPps = pin->pStdPps;
        pCurrFrameDecParams->pStdSps = pin->pStdSps;
        pCurrFrameDecParams->pStdVps = pin->pStdVps;
        if (getVideoLogPrintEnable())
        {
            std::cout << "\n\tCurrent h.265 Picture VPS update : " << pin->pStdVps->GetUpdateSequenceCount()
                      << std::endl;
            std::cout << "\n\tCurrent h.265 Picture SPS update : " << pin->pStdSps->GetUpdateSequenceCount()
                      << std::endl;
            std::cout << "\tCurrent h.265 Picture PPS update : " << pin->pStdPps->GetUpdateSequenceCount() << std::endl;
        }

        pPictureInfo->sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PICTURE_INFO_KHR;
        pPictureInfo->pNext = nullptr;

        pPictureInfo->pStdPictureInfo              = &cachedParameters->h265PicParams.stdPictureInfo;
        pCurrFrameDecParams->decodeFrameInfo.pNext = &cachedParameters->h265PicParams.pictureInfo;

        if (pd->CodecSpecific.hevc.mv_hevc_enable)
        {
            pDecodePictureInfo->viewId = pd->CodecSpecific.hevc.nuh_layer_id;
        }
        else
        {
            pDecodePictureInfo->viewId = 0;
        }

        pPictureInfo->sliceSegmentCount = pd->numSlices;
        uint32_t maxSliceCount          = 0;
        DE_ASSERT(pd->firstSliceIndex == 0); // No slice and MV modes are supported yet
        pPictureInfo->pSliceSegmentOffsets = pd->bitstreamData->GetStreamMarkersPtr(pd->firstSliceIndex, maxSliceCount);
        DE_ASSERT(maxSliceCount == pd->numSlices);

        pStdPictureInfo->pps_pic_parameter_set_id   = pin->pic_parameter_set_id;       // PPS ID
        pStdPictureInfo->pps_seq_parameter_set_id   = pin->seq_parameter_set_id;       // SPS ID
        pStdPictureInfo->sps_video_parameter_set_id = pin->vps_video_parameter_set_id; // VPS ID

        pStdPictureInfo->flags.IrapPicFlag = pin->IrapPicFlag; // Intra Random Access Point for current picture.
        pStdPictureInfo->flags.IdrPicFlag  = pin->IdrPicFlag;  // Instantaneous Decoding Refresh for current picture.
        pStdPictureInfo->flags.IsReference = pd->ref_pic_flag;
        pStdPictureInfo->flags.short_term_ref_pic_set_sps_flag = pin->short_term_ref_pic_set_sps_flag;

        pStdPictureInfo->NumBitsForSTRefPicSetInSlice = pin->NumBitsForShortTermRPSInSlice;

        // NumDeltaPocsOfRefRpsIdx = s->sh.short_term_rps ?
        // s->sh.short_term_rps->rps_idx_num_delta_pocs : 0
        pStdPictureInfo->NumDeltaPocsOfRefRpsIdx = pin->NumDeltaPocsOfRefRpsIdx;
        pStdPictureInfo->PicOrderCntVal          = pin->CurrPicOrderCntVal;

        if (getVideoLogPrintEnable())
            std::cout << "\tnumPocStCurrBefore: " << (int32_t)pin->NumPocStCurrBefore
                      << " numPocStCurrAfter: " << (int32_t)pin->NumPocStCurrAfter
                      << " numPocLtCurr: " << (int32_t)pin->NumPocLtCurr << std::endl;

        pCurrFrameDecParams->numGopReferenceSlots = FillDpbH265State(
            pd, pin, pDpbRefList, pStdPictureInfo,
            VkParserPerFrameDecodeParameters::MAX_DPB_REF_SLOTS, // max 16 reference pictures
            referenceSlots, pCurrFrameDecParams->pGopReferenceImagesIndexes, &setupReferenceSlot.slotIndex);

        DE_ASSERT(!pd->ref_pic_flag || (setupReferenceSlot.slotIndex >= 0));
        // TODO: Dummy struct to silence validation. The root problem is that the dpb map doesn't take account of the setup slot,
        // for some reason... So we can't use the existing logic to setup the picture flags and frame number from the dpbEntry
        // class.
        cachedParameters->h265SlotInfo.sType             = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_DPB_SLOT_INFO_KHR;
        cachedParameters->h265SlotInfo.pNext             = nullptr;
        cachedParameters->h265SlotInfo.pStdReferenceInfo = &cachedParameters->h265RefInfo;

        if (setupReferenceSlot.slotIndex >= 0)
        {
            setupReferenceSlot.pPictureResource                      = &pCurrFrameDecParams->dpbSetupPictureResource;
            setupReferenceSlot.pNext                                 = &cachedParameters->h265SlotInfo;
            pCurrFrameDecParams->decodeFrameInfo.pSetupReferenceSlot = &setupReferenceSlot;
        }
        if (pCurrFrameDecParams->numGopReferenceSlots)
        {
            DE_ASSERT(pCurrFrameDecParams->numGopReferenceSlots <=
                      (int32_t)VkParserPerFrameDecodeParameters::MAX_DPB_REF_SLOTS);
            for (uint32_t dpbEntryIdx = 0; dpbEntryIdx < (uint32_t)pCurrFrameDecParams->numGopReferenceSlots;
                 dpbEntryIdx++)
            {
                pCurrFrameDecParams->pictureResources[dpbEntryIdx].sType =
                    VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR;
                referenceSlots[dpbEntryIdx].pPictureResource = &pCurrFrameDecParams->pictureResources[dpbEntryIdx];
                DE_ASSERT(pDpbRefList[dpbEntryIdx].IsReference());
            }

            pCurrFrameDecParams->decodeFrameInfo.pReferenceSlots    = referenceSlots;
            pCurrFrameDecParams->decodeFrameInfo.referenceSlotCount = pCurrFrameDecParams->numGopReferenceSlots;
        }
        else
        {
            pCurrFrameDecParams->decodeFrameInfo.pReferenceSlots    = nullptr;
            pCurrFrameDecParams->decodeFrameInfo.referenceSlotCount = 0;
        }

        if (getVideoLogPrintEnable())
        {
            for (int32_t i = 0; i < H26X_MAX_DPB_SLOTS; i++)
            {
                std::cout << "\tdpbIndex: " << i;
                if (pDpbRefList[i])
                {
                    std::cout << " REFERENCE FRAME";

                    std::cout << " picOrderCntValList: "
                              << (int32_t)pDpbRefList[i].dpbSlotInfo.pStdReferenceInfo->PicOrderCntVal;

                    std::cout << "\t\t Flags: ";
                    if (pDpbRefList[i].dpbSlotInfo.pStdReferenceInfo->flags.used_for_long_term_reference)
                    {
                        std::cout << "IS LONG TERM ";
                    }
                }
                else
                {
                    std::cout << " NOT A REFERENCE ";
                }
                std::cout << std::endl;
            }
        }

        pDecodePictureInfo->displayWidth  = m_nvsi.nDisplayWidth;
        pDecodePictureInfo->displayHeight = m_nvsi.nDisplayHeight;

        pVkPicBuff->decodeSuperResWidth = pVkPicBuff->decodeWidth = m_nvsi.nDisplayWidth;
        pVkPicBuff->decodeHeight                                  = m_nvsi.nDisplayHeight;
    }
    else if (m_profile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR)
    {
        // Keep a reference for out-of-order decoding
        memcpy(&cachedParameters->av1PicParams, &pd->CodecSpecific.av1, sizeof(VkParserAv1PictureData));
        VkParserAv1PictureData *const p      = &cachedParameters->av1PicParams;
        VkVideoDecodeAV1PictureInfoKHR *pKhr = &p->khr_info;
        StdVideoDecodeAV1PictureInfo *pStd   = &p->std_info;

        // Chain up KHR structures
        pKhr->sType             = VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_PICTURE_INFO_KHR;
        pKhr->pNext             = nullptr;
        pKhr->pStdPictureInfo   = pStd;
        pKhr->frameHeaderOffset = 0;
        pKhr->pTileOffsets      = &p->tileOffsets[0];
        pKhr->pTileSizes        = &p->tileSizes[0];
        DE_ASSERT(pKhr->tileCount > 0);

        p->tileInfo.pWidthInSbsMinus1  = &p->width_in_sbs_minus_1[0];
        p->tileInfo.pHeightInSbsMinus1 = &p->height_in_sbs_minus_1[0];
        p->tileInfo.pMiColStarts       = &p->MiColStarts[0];
        p->tileInfo.pMiRowStarts       = &p->MiRowStarts[0];
        pStd->pTileInfo                = &p->tileInfo;

        pStd->pQuantization = &p->quantization;
        pStd->pSegmentation = &p->segmentation;
        pStd->pLoopFilter   = &p->loopFilter;
        pStd->pCDEF         = &p->CDEF;

        if (pStd->flags.UsesLr)
        {
            // Shift values for calculating loop restoration unit sizes (1 << (5 + shift)).
            // So: 0 -> 32, 1 -> 64, 2 -> 128, 3 -> 256.
            std::unordered_set<int> allowableRestorationShiftValues = {0, 1, 2, 3};
            DE_UNREF(allowableRestorationShiftValues);

#if defined(DE_DEBUG)
            auto &lrs = p->loopRestoration.LoopRestorationSize;
            for (int i = 0; i < STD_VIDEO_AV1_MAX_NUM_PLANES; i++)
            {
                DE_ASSERT(allowableRestorationShiftValues.find(lrs[i]) != allowableRestorationShiftValues.end());
            }
#endif
            pStd->pLoopRestoration = &p->loopRestoration;
        }
        else
        {
            pStd->pLoopRestoration = nullptr;
        }

        pStd->pGlobalMotion = &p->globalMotion;
        pStd->pFilmGrain    = &p->filmGrain;

        if (getVideoLogPrintEnable())
        {
            const char *frameTypeStr = getdVideoAV1FrameTypeName(p->std_info.frame_type);
            printf(";;;; ======= AV1 begin frame %d (%dx%d) (frame type: %s) (show frame? %s) =======\n",
                   m_nCurrentPictureID, p->upscaled_width, p->frame_height, frameTypeStr, p->showFrame ? "yes" : "no");

            printf("ref_frame_idx: ");
            for (int i = 0; i < 7; i++)
                printf("%02d ", i);
            printf("\nref_frame_idx: ");
            for (int i = 0; i < 7; i++)
                printf("%02d ", p->ref_frame_idx[i]);
            printf("\n");
            printf("m_pictureToDpbSlotMap: ");
            for (int i = 0; i < MAX_FRM_CNT; i++)
            {
                printf("%02d ", i);
            }
            printf("\nm_pictureToDpbSlotMap: ");
            for (int i = 0; i < MAX_FRM_CNT; i++)
            {
                printf("%02d ", m_pictureToDpbSlotMap[i]);
            }
            printf("\n");

            printf("ref_frame_picture: ");
            for (int32_t inIdx = 0; inIdx < STD_VIDEO_AV1_NUM_REF_FRAMES; inIdx++)
            {
                printf("%02d ", inIdx);
            }
            printf("\nref_frame_picture: ");
            for (int32_t inIdx = 0; inIdx < STD_VIDEO_AV1_NUM_REF_FRAMES; inIdx++)
            {
                int8_t picIdx = p->pic_idx[inIdx];
                printf("%02d ", picIdx);
            }
            printf("\n");
        }

        pCurrFrameDecParams->pStdSps = p->pStdSps;
        pCurrFrameDecParams->pStdPps = nullptr;
        pCurrFrameDecParams->pStdVps = nullptr;

        pCurrFrameDecParams->decodeFrameInfo.pNext = pKhr;
        p->setupSlot.pStdReferenceInfo             = &p->setupSlotInfo;
        setupReferenceSlot.pNext                   = &p->setupSlot;

        if (!m_intraOnlyDecodingNoSetupRef)
        {
            DE_ASSERT(m_maxNumDpbSlots <= STD_VIDEO_AV1_NUM_REF_FRAMES + 1); // + 1 for scratch slot
            uint32_t refDpbUsedAndValidMask = 0;
            uint32_t referenceIndex         = 0;
            std::unordered_set<int8_t> activeReferences;
            bool isKeyFrame       = p->std_info.frame_type == STD_VIDEO_AV1_FRAME_TYPE_KEY;
            bool isIntraOnlyFrame = p->std_info.frame_type == STD_VIDEO_AV1_FRAME_TYPE_INTRA_ONLY;
            for (size_t refName = 0; refName < STD_VIDEO_AV1_REFS_PER_FRAME; refName++)
            {
                int8_t picIdx = isKeyFrame ? -1 : p->pic_idx[p->ref_frame_idx[refName]];
                if (picIdx < 0)
                {
                    pKhr->referenceNameSlotIndices[refName] = -1;
                    continue;
                }
                int8_t dpbSlot = GetPicDpbSlot(picIdx);
                assert(dpbSlot >= 0);
                pKhr->referenceNameSlotIndices[refName] = dpbSlot;
                activeReferences.insert(dpbSlot);
                //hdr.delta_frame_id_minus_1[dpbSlot] = pin->delta_frame_id_minus_1[pin->ref_frame_idx[i]];
            }

            if (getVideoLogPrintEnable())
            {
                printf("%d referenceNameSlotIndex: ", m_nCurrentPictureID);
                for (int i = 0; i < STD_VIDEO_AV1_REFS_PER_FRAME; i++)
                {
                    printf("%02d ", i);
                }
                printf("\n%d referenceNameSlotIndex: ", m_nCurrentPictureID);
                for (int i = 0; i < STD_VIDEO_AV1_REFS_PER_FRAME; i++)
                {
                    printf("%02d ", pKhr->referenceNameSlotIndices[i]);
                }
                printf("\n");
            }

            for (int32_t inIdx = 0; inIdx < STD_VIDEO_AV1_NUM_REF_FRAMES; inIdx++)
            {
                int8_t picIdx  = isKeyFrame ? -1 : p->pic_idx[inIdx];
                int8_t dpbSlot = -1;
                if ((picIdx >= 0) && !(refDpbUsedAndValidMask & (1 << picIdx)))
                { // Causes an assert in the driver that the DPB is invalid, with a slotindex of -1.
                    dpbSlot = GetPicDpbSlot(picIdx);

                    DE_ASSERT(dpbSlot >= 0);
                    if (dpbSlot < 0)
                        continue;

                    refDpbUsedAndValidMask |= (1 << picIdx);
                    m_dpb[dpbSlot].MarkInUse(m_nCurrentPictureID);

                    if (activeReferences.count(dpbSlot) == 0)
                    {
                        continue;
                    }

                    // Setup the reference info for the current dpb slot.
                    p->dpbSlots[inIdx].sType             = VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_DPB_SLOT_INFO_KHR;
                    p->dpbSlots[inIdx].pStdReferenceInfo = &p->dpbSlotInfos[inIdx];

                    referenceSlots[referenceIndex].sType     = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
                    referenceSlots[referenceIndex].pNext     = &p->dpbSlots[inIdx];
                    referenceSlots[referenceIndex].slotIndex = dpbSlot;

                    pCurrFrameDecParams->pGopReferenceImagesIndexes[referenceIndex] = picIdx;
                    referenceIndex++;
                }
            }

            if (getVideoLogPrintEnable())
            {
                printf(";;; pReferenceSlots (%d): ", referenceIndex);
                for (size_t i = 0; i < referenceIndex; i++)
                {
                    printf("%02d ", referenceSlots[i].slotIndex);
                }
                printf("\n");
            }

            ResetPicDpbSlots(refDpbUsedAndValidMask);

            // Take into account the reference picture now.
            int8_t currPicIdx = GetPicIdx(pd->pCurrPic);
            int8_t dpbSlot    = -1;
            DE_ASSERT(currPicIdx >= 0);
            if (currPicIdx >= 0)
            {
                refDpbUsedAndValidMask |= (1 << currPicIdx); // How does this do anything?
            }

            if (true /*pd->ref_pic_flag*/)
            {
                dpbSlot = GetPicDpbSlot(currPicIdx); // use the associated slot, if not allocate a new slot.
                if (dpbSlot < 0)
                {
                    dpbSlot = m_dpb.AllocateSlot();
                    DE_ASSERT(dpbSlot >= 0);
                    SetPicDpbSlot(currPicIdx, dpbSlot); // Assign the dpbSlot to the current picture index.
                    m_dpb[dpbSlot].setPictureResource(GetPic(pd->pCurrPic),
                                                      m_nCurrentPictureID); // m_nCurrentPictureID is our main index.
                }
                DE_ASSERT(dpbSlot >= 0);
            }

            setupReferenceSlot.slotIndex = dpbSlot;
            DE_ASSERT(!pd->ref_pic_flag || (setupReferenceSlot.slotIndex >= 0));

            if (getVideoLogPrintEnable())
            {
                printf("SlotsInUse: ");
                uint32_t slotsInUse = m_dpb.getSlotInUseMask();
                for (int i = 0; i < 9; i++)
                {
                    printf("%02d ", i);
                }
                uint8_t greenSquare[]  = {0xf0, 0x9f, 0x9f, 0xa9, 0x00};
                uint8_t redSquare[]    = {0xf0, 0x9f, 0x9f, 0xa5, 0x00};
                uint8_t yellowSquare[] = {0xf0, 0x9f, 0x9f, 0xa8, 0x00};
                printf("\nSlotsInUse: ");
                for (int i = 0; i < 9; i++)
                {
                    printf("%-2s ", (slotsInUse & (1 << i)) ?
                                        (i == dpbSlot ? (char *)yellowSquare : (char *)greenSquare) :
                                        (char *)redSquare);
                }
                printf("\n");
            }
            setupReferenceSlot.pPictureResource                      = &pCurrFrameDecParams->dpbSetupPictureResource;
            pCurrFrameDecParams->decodeFrameInfo.pSetupReferenceSlot = &setupReferenceSlot;
            pCurrFrameDecParams->numGopReferenceSlots                = referenceIndex;

            if (isIntraOnlyFrame)
            {
                // Do not actually reference anything, but ensure the DPB slots for future frames are undisturbed.
                pCurrFrameDecParams->numGopReferenceSlots = 0;
                for (size_t i = 0; i < STD_VIDEO_AV1_REFS_PER_FRAME; i++)
                {
                    pKhr->referenceNameSlotIndices[i] = -1;
                }
            }
        }
        else
        {
            // Intra only decoding
            pCurrFrameDecParams->numGopReferenceSlots = 0;
            for (size_t i = 0; i < STD_VIDEO_AV1_REFS_PER_FRAME; i++)
            {
                pKhr->referenceNameSlotIndices[i] = -1;
            }
        }

        if (pCurrFrameDecParams->numGopReferenceSlots)
        {
            assert(pCurrFrameDecParams->numGopReferenceSlots < 9);
            for (uint32_t dpbEntryIdx = 0; dpbEntryIdx < (uint32_t)pCurrFrameDecParams->numGopReferenceSlots;
                 dpbEntryIdx++)
            {
                pCurrFrameDecParams->pictureResources[dpbEntryIdx].sType =
                    VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR;
                referenceSlots[dpbEntryIdx].pPictureResource = &pCurrFrameDecParams->pictureResources[dpbEntryIdx];
            }

            pCurrFrameDecParams->decodeFrameInfo.pReferenceSlots    = referenceSlots;
            pCurrFrameDecParams->decodeFrameInfo.referenceSlotCount = pCurrFrameDecParams->numGopReferenceSlots;
        }
        else
        {
            pCurrFrameDecParams->decodeFrameInfo.pReferenceSlots    = NULL;
            pCurrFrameDecParams->decodeFrameInfo.referenceSlotCount = 0;
        }

        if (getVideoLogPrintEnable())
        {
            printf(";;; tiling: %d tiles %d cols %d rows\n", p->khr_info.tileCount, p->tileInfo.TileCols,
                   p->tileInfo.TileRows);
            for (uint32_t i = 0; i < p->khr_info.tileCount; i++)
            {
                printf(";;; \ttile %d: offset %d size %d (sbs: %dx%d) (mi: %dx%d): ", i, p->tileOffsets[i],
                       p->tileSizes[i], p->tileInfo.pWidthInSbsMinus1[i] + 1, p->tileInfo.pHeightInSbsMinus1[i] + 1,
                       p->tileInfo.pMiColStarts[i], p->tileInfo.pMiRowStarts[i]);

                VkDeviceSize maxSize = 0;
                uint32_t adjustedTileOffset =
                    p->tileOffsets[i] + static_cast<uint32_t>(pCurrFrameDecParams->bitstreamDataOffset);
                const uint8_t *bitstreamBytes =
                    pCurrFrameDecParams->bitstreamData->GetReadOnlyDataPtr(adjustedTileOffset, maxSize);

                for (uint32_t j = 0; j < std::min(p->tileSizes[i], 16u); j++)
                {
                    printf("%02x ", bitstreamBytes[j]);
                }
                printf("\n");
            }
        }

        if (m_forceDisableFilmGrain)
        {
            pStd->flags.apply_grain = 0;
        }

        pDecodePictureInfo->displayWidth  = p->upscaled_width;
        pDecodePictureInfo->displayHeight = p->frame_height;

        pVkPicBuff->decodeWidth         = p->frame_width;
        pVkPicBuff->decodeHeight        = p->frame_height;
        pVkPicBuff->decodeSuperResWidth = p->upscaled_width;
    }
    else if (m_profile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR)
    {
        // Keep a reference for out-of-order decoding
        memcpy(&cachedParameters->vp9PicParams, &pd->CodecSpecific.vp9, sizeof(VkParserVp9PictureData));
        VkParserVp9PictureData *const p      = &cachedParameters->vp9PicParams;
        VkVideoDecodeVP9PictureInfoKHR *pKhr = &cachedParameters->vp9VkPicInfo;
        StdVideoDecodeVP9PictureInfo *pStd   = &p->stdPictureInfo;

        // Chain up KHR structures
        pKhr->sType                    = VK_STRUCTURE_TYPE_VIDEO_DECODE_VP9_PICTURE_INFO_KHR;
        pKhr->pNext                    = nullptr;
        pKhr->pStdPictureInfo          = pStd;
        pKhr->compressedHeaderOffset   = p->compressedHeaderOffset;
        pKhr->uncompressedHeaderOffset = p->uncompressedHeaderOffset;
        pKhr->tilesOffset              = p->tilesOffset;

        pStd->pSegmentation = pStd->flags.segmentation_enabled ? &p->stdSegmentation : nullptr;
        pStd->pLoopFilter   = &p->stdLoopFilter;
        pStd->pColorConfig  = &p->stdColorConfig;

        if (getVideoLogPrintEnable())
        {
            const char *frameTypeStr = getdVideoVP9FrameTypeName(p->stdPictureInfo.frame_type);
            printf(";;;; ======= VP9 begin frame %d (%dx%d) (frame type: %s show_frame=%d) =======\n",
                   m_nCurrentPictureID, p->FrameWidth, p->FrameHeight, frameTypeStr, pStd->flags.show_frame);

            printf("ref_frame_idx: ");
            for (int i = 0; i < VK_MAX_VIDEO_VP9_REFERENCES_PER_FRAME_KHR; i++)
                printf("%02d ", i);
            printf("\nref_frame_idx: ");
            for (int i = 0; i < VK_MAX_VIDEO_VP9_REFERENCES_PER_FRAME_KHR; i++)
                printf("%02d ", p->ref_frame_idx[i]);
            printf("\n");
            printf("m_pictureToDpbSlotMap: ");
            for (int i = 0; i < MAX_FRM_CNT; i++)
            {
                printf("%02d ", i);
            }
            printf("\nm_pictureToDpbSlotMap: ");
            for (int i = 0; i < MAX_FRM_CNT; i++)
            {
                printf("%02d ", m_pictureToDpbSlotMap[i]);
            }
            printf("\n");

            printf("ref_frame_picture: ");
            for (int32_t inIdx = 0; inIdx < STD_VIDEO_AV1_NUM_REF_FRAMES; inIdx++)
            {
                printf("%02d ", inIdx);
            }
            printf("\nref_frame_picture: ");
            for (int32_t inIdx = 0; inIdx < STD_VIDEO_AV1_NUM_REF_FRAMES; inIdx++)
            {
                int8_t picIdx = p->pic_idx[inIdx];
                printf("%02d ", picIdx);
            }
            printf("\n");
        }

        pCurrFrameDecParams->decodeFrameInfo.pNext = pKhr;

        if (!m_intraOnlyDecodingNoSetupRef)
        {
            DE_ASSERT(m_maxNumDpbSlots <= STD_VIDEO_VP9_NUM_REF_FRAMES + 1); // + 1 for scratch slot
            uint32_t refDpbUsedAndValidMask = 0;
            uint32_t referenceIndex         = 0;
            int8_t dpbSlot                  = -1;
            int8_t picIdx                   = -1;
            std::unordered_set<int8_t> activeReferences;
            bool isKeyFrame       = p->stdPictureInfo.frame_type == STD_VIDEO_VP9_FRAME_TYPE_KEY;
            bool isIntraOnlyFrame = p->FrameIsIntra;

            for (size_t refName = 0; refName < VK_MAX_VIDEO_VP9_REFERENCES_PER_FRAME_KHR; refName++)
            {
                picIdx = isKeyFrame ? -1 : p->pic_idx[p->ref_frame_idx[refName]];
                if (picIdx < 0)
                {
                    pKhr->referenceNameSlotIndices[refName] = -1;
                    continue;
                }
                dpbSlot = GetPicDpbSlot(picIdx);
                assert(dpbSlot >= 0);
                pKhr->referenceNameSlotIndices[refName] = dpbSlot;
                activeReferences.insert(dpbSlot);
            }

            if (getVideoLogPrintEnable())
            {
                printf("%d referenceNameSlotIndex: ", m_nCurrentPictureID);
                for (int i = 0; i < VK_MAX_VIDEO_VP9_REFERENCES_PER_FRAME_KHR; i++)
                {
                    printf("%02d ", i);
                }
                printf("\n%d referenceNameSlotIndex: ", m_nCurrentPictureID);
                for (int i = 0; i < VK_MAX_VIDEO_VP9_REFERENCES_PER_FRAME_KHR; i++)
                {
                    printf("%02d ", pKhr->referenceNameSlotIndices[i]);
                }
                printf("\n");
            }

            for (int32_t inIdx = 0; inIdx < STD_VIDEO_VP9_REFS_PER_FRAME; inIdx++)
            {
                picIdx  = isKeyFrame ? -1 : p->pic_idx[inIdx];
                dpbSlot = -1;
                if ((picIdx >= 0) && !(refDpbUsedAndValidMask & (1 << picIdx)))
                {
                    dpbSlot = GetPicDpbSlot(picIdx);

                    if (dpbSlot < 0)
                        continue;

                    refDpbUsedAndValidMask |= (1 << picIdx);
                    m_dpb[dpbSlot].MarkInUse(m_nCurrentPictureID);

                    if (activeReferences.count(dpbSlot) == 0)
                    {
                        continue;
                    }

                    referenceSlots[referenceIndex].sType     = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
                    referenceSlots[referenceIndex].pNext     = nullptr;
                    referenceSlots[referenceIndex].slotIndex = dpbSlot;

                    pCurrFrameDecParams->pGopReferenceImagesIndexes[referenceIndex] = picIdx;
                    referenceIndex++;
                }
            }

            ResetPicDpbSlots(refDpbUsedAndValidMask);

            // Take into account the reference picture now.
            int8_t currPicIdx = GetPicIdx(pd->pCurrPic);
            dpbSlot           = -1;
            DE_ASSERT(currPicIdx >= 0);

            dpbSlot = GetPicDpbSlot(currPicIdx); // use the associated slot, if not allocate a new slot.
            if (dpbSlot < 0)
            {
                dpbSlot = m_dpb.AllocateSlot();
                DE_ASSERT(dpbSlot >= 0);
                SetPicDpbSlot(currPicIdx, dpbSlot); // Assign the dpbSlot to the current picture index.
                m_dpb[dpbSlot].setPictureResource(GetPic(pd->pCurrPic),
                                                  m_nCurrentPictureID); // m_nCurrentPictureID is our main index.
            }

            DE_ASSERT(!pd->ref_pic_flag || (dpbSlot >= 0));

            if (getVideoLogPrintEnable())
            {
                printf("SlotsInUse: ");
                uint32_t slotsInUse = m_dpb.getSlotInUseMask();
                for (int i = 0; i < 9; i++)
                {
                    printf("%02d ", i);
                }
                uint8_t greenSquare[]  = {0xf0, 0x9f, 0x9f, 0xa9, 0x00};
                uint8_t redSquare[]    = {0xf0, 0x9f, 0x9f, 0xa5, 0x00};
                uint8_t yellowSquare[] = {0xf0, 0x9f, 0x9f, 0xa8, 0x00};
                printf("\nSlotsInUse: ");
                for (int i = 0; i < 9; i++)
                {
                    printf("%-2s ", (slotsInUse & (1 << i)) ?
                                        (i == dpbSlot ? (char *)yellowSquare : (char *)greenSquare) :
                                        (char *)redSquare);
                }
                printf("\n");
            }
            setupReferenceSlot.slotIndex                             = dpbSlot;
            setupReferenceSlot.pNext                                 = nullptr;
            setupReferenceSlot.pPictureResource                      = &pCurrFrameDecParams->dpbSetupPictureResource;
            pCurrFrameDecParams->decodeFrameInfo.pSetupReferenceSlot = &setupReferenceSlot;
            pCurrFrameDecParams->numGopReferenceSlots                = referenceIndex;

            if (isIntraOnlyFrame)
            {
                // Do not actually reference anything, but ensure the DPB slots for future frames are undisturbed.
                pCurrFrameDecParams->numGopReferenceSlots = 0;
                for (size_t i = 0; i < VK_MAX_VIDEO_VP9_REFERENCES_PER_FRAME_KHR; i++)
                {
                    pKhr->referenceNameSlotIndices[i] = -1;
                }
            }
        }
        else
        {
            // Intra only decoding
            pCurrFrameDecParams->numGopReferenceSlots = 0;
            for (size_t i = 0; i < VK_MAX_VIDEO_VP9_REFERENCES_PER_FRAME_KHR; i++)
            {
                pKhr->referenceNameSlotIndices[i] = -1;
            }
        }

        if (pCurrFrameDecParams->numGopReferenceSlots)
        {
            assert(pCurrFrameDecParams->numGopReferenceSlots < m_maxNumDpbSlots);
            for (uint32_t dpbEntryIdx = 0; dpbEntryIdx < (uint32_t)pCurrFrameDecParams->numGopReferenceSlots;
                 dpbEntryIdx++)
            {
                pCurrFrameDecParams->pictureResources[dpbEntryIdx].sType =
                    VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR;
                referenceSlots[dpbEntryIdx].pPictureResource = &pCurrFrameDecParams->pictureResources[dpbEntryIdx];
            }

            pCurrFrameDecParams->decodeFrameInfo.pReferenceSlots    = referenceSlots;
            pCurrFrameDecParams->decodeFrameInfo.referenceSlotCount = pCurrFrameDecParams->numGopReferenceSlots;
        }
        else
        {
            pCurrFrameDecParams->decodeFrameInfo.pReferenceSlots    = NULL;
            pCurrFrameDecParams->decodeFrameInfo.referenceSlotCount = 0;
        }

        pDecodePictureInfo->displayWidth  = p->FrameWidth;
        pDecodePictureInfo->displayHeight = p->FrameHeight;
        pVkPicBuff->decodeSuperResWidth = pVkPicBuff->decodeWidth = p->FrameWidth;
        pVkPicBuff->decodeHeight                                  = p->FrameHeight;
    }

    bRet = DecodePictureWithParameters(cachedParameters) >= 0;

    DE_ASSERT(bRet);

    m_nCurrentPictureID++;

    return bRet;
}

int32_t VideoBaseDecoder::DecodePictureWithParameters(MovePtr<CachedDecodeParameters> &cachedParameters)
{
    TCU_CHECK_MSG(m_videoSession, "Video session has not been initialized!");

    auto *pPicParams    = &cachedParameters->pictureParams;
    bool applyFilmGrain = false;

    if (m_profile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR)
    {
        VkParserAv1PictureData *const p    = &cachedParameters->av1PicParams;
        StdVideoDecodeAV1PictureInfo *pStd = &p->std_info;

        applyFilmGrain = pStd->flags.apply_grain;
    }

    DE_ASSERT((uint32_t)pPicParams->currPicIdx < MAX_NUM_DECODE_SURFACES);

    cachedParameters->picNumInDecodeOrder = m_decodePicCount++;
    m_videoFrameBuffer->SetPicNumInDecodeOrder(pPicParams->currPicIdx, cachedParameters->picNumInDecodeOrder);

    DE_ASSERT(pPicParams->bitstreamData->GetMaxSize() >= pPicParams->bitstreamDataLen);
    pPicParams->decodeFrameInfo.srcBuffer       = pPicParams->bitstreamData->GetBuffer();
    pPicParams->decodeFrameInfo.srcBufferOffset = pPicParams->bitstreamDataOffset;
    pPicParams->decodeFrameInfo.srcBufferRange =
        deAlign64(pPicParams->bitstreamDataLen, m_videoCaps.minBitstreamBufferSizeAlignment);
    DE_ASSERT(pPicParams->firstSliceIndex == 0);

    int32_t retPicIdx = GetCurrentFrameData((uint32_t)pPicParams->currPicIdx, cachedParameters->frameDataSlot);
    DE_ASSERT(retPicIdx == pPicParams->currPicIdx);

    if (retPicIdx != pPicParams->currPicIdx)
    {
        fprintf(stderr, "\nERROR: DecodePictureWithParameters() retPicIdx(%d) != currPicIdx(%d)\n", retPicIdx,
                pPicParams->currPicIdx);
    }

    auto &decodeBeginInfo = cachedParameters->decodeBeginInfo;
    decodeBeginInfo.sType = VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR;
    // CmdResetQueryPool are NOT Supported yet.
    decodeBeginInfo.pNext        = pPicParams->beginCodingInfoPictureParametersExt;
    decodeBeginInfo.videoSession = m_videoSession->GetVideoSession();

    cachedParameters->currentPictureParameterObject = m_currentPictureParameters;

    DE_ASSERT(!!pPicParams->decodeFrameInfo.srcBuffer);
    cachedParameters->bitstreamBufferMemoryBarrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR,
                                                      nullptr,
                                                      VK_PIPELINE_STAGE_2_NONE_KHR,
                                                      0, // VK_ACCESS_2_HOST_WRITE_BIT_KHR,
                                                      VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR,
                                                      VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR,
                                                      (uint32_t)m_deviceContext->decodeQueueFamilyIdx(),
                                                      (uint32_t)m_deviceContext->decodeQueueFamilyIdx(),
                                                      pPicParams->decodeFrameInfo.srcBuffer,
                                                      pPicParams->decodeFrameInfo.srcBufferOffset,
                                                      pPicParams->decodeFrameInfo.srcBufferRange};

    bool isLayeredDpb          = m_useImageArray || m_useImageViewArray;
    uint32_t currPicArrayLayer = isLayeredDpb ? pPicParams->currPicIdx : 0;
    const VkImageSubresourceRange currPicSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, currPicArrayLayer, 1);
    // The destination image is never layered.
    const VkImageSubresourceRange dstSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);

    cachedParameters->currentDpbPictureResourceInfo    = VulkanVideoFrameBuffer::PictureResourceInfo();
    cachedParameters->currentOutputPictureResourceInfo = VulkanVideoFrameBuffer::PictureResourceInfo();
    deMemset(&cachedParameters->currentOutputPictureResource, 0, sizeof(VkVideoPictureResourceInfoKHR));
    cachedParameters->currentOutputPictureResource.sType = VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR;

    auto *pOutputPictureResource     = cachedParameters->pOutputPictureResource;
    auto *pOutputPictureResourceInfo = cachedParameters->pOutputPictureResourceInfo;
    if (!dpbAndOutputCoincide())
    {
        // Output Distinct will use the decodeFrameInfo.dstPictureResource directly.
        pOutputPictureResource = &pPicParams->decodeFrameInfo.dstPictureResource;
    }
    else
    {
        if (!applyFilmGrain && !m_intraOnlyDecodingNoSetupRef)
        {
            pOutputPictureResource = &cachedParameters->currentOutputPictureResource;
        }
        else
        {
            pOutputPictureResource = &pPicParams->decodeFrameInfo.dstPictureResource;
        }
    }

    pOutputPictureResourceInfo = &cachedParameters->currentOutputPictureResourceInfo;

    if (pPicParams->currPicIdx != m_videoFrameBuffer->GetCurrentImageResourceByIndex(
                                      pPicParams->currPicIdx, &pPicParams->dpbSetupPictureResource,
                                      &cachedParameters->currentDpbPictureResourceInfo,
                                      VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR, pOutputPictureResource,
                                      pOutputPictureResourceInfo, VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR))
    {
        DE_ASSERT(!"GetImageResourcesByIndex has failed");
    }

    pPicParams->dpbSetupPictureResource.codedOffset = {
        0, 0}; // FIXME: This parameter must to be adjusted based on the interlaced mode.
    pPicParams->dpbSetupPictureResource.codedExtent = {(uint32_t)cachedParameters->decodedPictureInfo.displayWidth,
                                                       (uint32_t)cachedParameters->decodedPictureInfo.displayHeight};

    if (pOutputPictureResource)
    {
        DE_ASSERT(pOutputPictureResource->sType == VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR);
        pOutputPictureResource->codedOffset = {
            0, 0}; // FIXME: This parameter must to be adjusted based on the interlaced mode.
        pOutputPictureResource->codedExtent = {(uint32_t)cachedParameters->decodedPictureInfo.displayWidth,
                                               (uint32_t)cachedParameters->decodedPictureInfo.displayHeight};
    }

    if (dpbAndOutputCoincide() && !applyFilmGrain)
    {
        // For the Output Coincide, the DPB and destination output resources are the same.
        pPicParams->decodeFrameInfo.dstPictureResource = pPicParams->dpbSetupPictureResource;

        // Also, when we are copying the output we need to know which layer is used for the current frame.
        // This is if a multi-layered image is used for the DPB and the output (since they coincide).
        cachedParameters->decodedPictureInfo.imageLayerIndex = pPicParams->dpbSetupPictureResource.baseArrayLayer;
    }
    else if (pOutputPictureResourceInfo || m_intraOnlyDecodingNoSetupRef)
    {
        // For Output Distinct transition the image to DECODE_DST
        if (pOutputPictureResourceInfo->currentImageLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        {
            VkImageMemoryBarrier2KHR dstBarrier = makeImageMemoryBarrier2(
                VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR, VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR,
                VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR, VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR,
                pOutputPictureResourceInfo->currentImageLayout, VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR,
                pOutputPictureResourceInfo->image, dstSubresourceRange);
            cachedParameters->imageBarriers.push_back(dstBarrier);
        }
    }

    if (!m_intraOnlyDecodingNoSetupRef &&
        cachedParameters->currentDpbPictureResourceInfo.currentImageLayout == VK_IMAGE_LAYOUT_UNDEFINED)
    {
        VkImageMemoryBarrier2KHR dpbBarrier = makeImageMemoryBarrier2(
            VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR, VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR,
            VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR, VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR,
            pOutputPictureResourceInfo->currentImageLayout, VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR,
            cachedParameters->currentDpbPictureResourceInfo.image, currPicSubresourceRange);
        cachedParameters->imageBarriers.push_back(dpbBarrier);
    }

    deMemset(cachedParameters->pictureResourcesInfo, 0,
             DE_LENGTH_OF_ARRAY(cachedParameters->pictureResourcesInfo) *
                 sizeof(cachedParameters->pictureResourcesInfo[0]));
    const int8_t *pGopReferenceImagesIndexes = pPicParams->pGopReferenceImagesIndexes;
    if (pPicParams->numGopReferenceSlots)
    {
        if (pPicParams->numGopReferenceSlots !=
            m_videoFrameBuffer->GetDpbImageResourcesByIndex(
                pPicParams->numGopReferenceSlots, pGopReferenceImagesIndexes, pPicParams->pictureResources,
                cachedParameters->pictureResourcesInfo, VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR))
        {
            DE_ASSERT(!"GetImageResourcesByIndex has failed");
        }
        for (uint32_t resId = 0; resId < pPicParams->numGopReferenceSlots; resId++)
        {
            const VkImageSubresourceRange dpbSubresourceRange = makeImageSubresourceRange(
                VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, isLayeredDpb ? pGopReferenceImagesIndexes[resId] : 0, 1);

            VkImageMemoryBarrier2KHR dpbBarrier = makeImageMemoryBarrier2(
                VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR, VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR,
                VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR, VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR,
                cachedParameters->pictureResourcesInfo[resId].currentImageLayout, VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR,
                cachedParameters->pictureResourcesInfo[resId].image, dpbSubresourceRange);
            cachedParameters->imageBarriers.push_back(dpbBarrier);
        }

        if (getVideoLogPrintEnable())
        {
            for (uint32_t resId = 0; resId < pPicParams->numGopReferenceSlots; resId++)
            {
                tcu::print(";;; DPB %d: %d x %d\n", resId, pPicParams->pictureResources[resId].codedExtent.width,
                           pPicParams->pictureResources[resId].codedExtent.height);
            }
        }
    }

    decodeBeginInfo.referenceSlotCount = pPicParams->decodeFrameInfo.referenceSlotCount;
    decodeBeginInfo.pReferenceSlots    = pPicParams->decodeFrameInfo.pReferenceSlots;

    // Ensure the resource for the resources associated with the
    // reference slot (if it exists) are in the bound picture
    // resources set.  See VUID-vkCmdDecodeVideoKHR-pDecodeInfo-07149.
    if (pPicParams->decodeFrameInfo.pSetupReferenceSlot != nullptr)
    {
        cachedParameters->fullReferenceSlots.clear();
        for (uint32_t i = 0; i < decodeBeginInfo.referenceSlotCount; i++)
            cachedParameters->fullReferenceSlots.push_back(decodeBeginInfo.pReferenceSlots[i]);
        VkVideoReferenceSlotInfoKHR setupActivationSlot = {};
        setupActivationSlot.sType                       = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
        setupActivationSlot.slotIndex                   = -1;
        setupActivationSlot.pPictureResource            = &pPicParams->dpbSetupPictureResource;
        cachedParameters->fullReferenceSlots.push_back(setupActivationSlot);
        decodeBeginInfo.referenceSlotCount++;
        decodeBeginInfo.pReferenceSlots = cachedParameters->fullReferenceSlots.data();
    }

    if (cachedParameters->decodedPictureInfo.flags.unpairedField)
    {
        // DE_ASSERT(pFrameSyncinfo->frameCompleteSemaphore == VK_NULL_HANDLE);
        cachedParameters->decodedPictureInfo.flags.syncFirstReady = true;
    }
    // FIXME: the below sequence for interlaced synchronization.
    cachedParameters->decodedPictureInfo.flags.syncToFirstField = false;

    cachedParameters->frameSynchronizationInfo = VulkanVideoFrameBuffer::FrameSynchronizationInfo();
    cachedParameters->frameSynchronizationInfo.hasFrameCompleteSignalFence = true;
    if (m_profile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR)
        cachedParameters->frameSynchronizationInfo.hasFrameCompleteSignalSemaphore =
            cachedParameters->av1PicParams.showFrame;
    else
        cachedParameters->frameSynchronizationInfo.hasFrameCompleteSignalSemaphore = false;

    VulkanVideoFrameBuffer::ReferencedObjectsInfo referencedObjectsInfo(pPicParams->bitstreamData, pPicParams->pStdPps,
                                                                        pPicParams->pStdSps, pPicParams->pStdVps);
    int currPicIdx =
        m_videoFrameBuffer->QueuePictureForDecode(pPicParams->currPicIdx, &cachedParameters->decodedPictureInfo,
                                                  &referencedObjectsInfo, &cachedParameters->frameSynchronizationInfo);
    DE_ASSERT(currPicIdx == currPicIdx);
    DE_UNREF(currPicIdx);

    if (m_outOfOrderDecoding)
    {
        if (m_profile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR)
            // We do not want the displayed frames to be evicted until we are ready to submit them
            // So keep a reference in the cached object
            cachedParameters->pd.pCurrPic->AddRef();
        return pPicParams->currPicIdx;
    }

    WaitForFrameFences(cachedParameters);
    ApplyPictureParameters(cachedParameters);
    RecordCommandBuffer(cachedParameters);
    SubmitQueue(cachedParameters);
    if (m_queryResultWithStatus)
    {
        QueryDecodeResults(cachedParameters);
    }

    return pPicParams->currPicIdx;
}

void VideoBaseDecoder::ApplyPictureParameters(de::MovePtr<CachedDecodeParameters> &cachedParameters)
{
    auto *pPicParams = &cachedParameters->pictureParams;
    VkSharedBaseObj<VkVideoRefCountBase> currentVkPictureParameters;
    VkParserVideoPictureParameters *pOwnerPictureParameters = nullptr;

    if ((m_profile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR) ||
        (m_profile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR))
    {
        bool valid = pPicParams->pStdPps->GetClientObject(currentVkPictureParameters);
        TCU_CHECK(currentVkPictureParameters && valid);
        pOwnerPictureParameters =
            VkParserVideoPictureParameters::VideoPictureParametersFromBase(currentVkPictureParameters);
        TCU_CHECK(pOwnerPictureParameters);
        int32_t ret = pOwnerPictureParameters->FlushPictureParametersQueue(m_videoSession);
        TCU_CHECK(ret >= 0);
        DE_UNREF(ret);
        bool isSps    = false;
        int32_t spsId = pPicParams->pStdPps->GetSpsId(isSps);
        TCU_CHECK(!isSps);
        TCU_CHECK(spsId >= 0);
        TCU_CHECK(pOwnerPictureParameters->HasSpsId(spsId));
        bool isPps    = false;
        int32_t ppsId = pPicParams->pStdPps->GetPpsId(isPps);
        TCU_CHECK(isPps);
        TCU_CHECK(ppsId >= 0);
        TCU_CHECK(pOwnerPictureParameters->HasPpsId(ppsId));
        DE_UNREF(valid);
        cachedParameters->decodeBeginInfo.videoSessionParameters = *pOwnerPictureParameters;
    }
    else if (m_profile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR)
    {
        bool valid = pPicParams->pStdSps->GetClientObject(currentVkPictureParameters);
        TCU_CHECK(currentVkPictureParameters && valid);
        pOwnerPictureParameters =
            VkParserVideoPictureParameters::VideoPictureParametersFromBase(currentVkPictureParameters);
        TCU_CHECK(pOwnerPictureParameters);
        int32_t ret = pOwnerPictureParameters->FlushPictureParametersQueue(m_videoSession);
        TCU_CHECK(ret >= 0);
        DE_UNREF(ret);
        cachedParameters->decodeBeginInfo.videoSessionParameters = *pOwnerPictureParameters;
    }
    else if (m_profile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR)
    {
        cachedParameters->decodeBeginInfo.videoSessionParameters = VK_NULL_HANDLE;
    }
}

void VideoBaseDecoder::WaitForFrameFences(de::MovePtr<CachedDecodeParameters> &cachedParameters)
{
    // Check here that the frame for this entry (for this command buffer) has already completed decoding.
    // Otherwise we may step over a hot command buffer by starting a new recording.
    // This fence wait should be NOP in 99.9% of the cases, because the decode queue is deep enough to
    // ensure the frame has already been completed.
    VK_CHECK(m_deviceContext->getDeviceDriver().waitForFences(
        m_deviceContext->device, 1, &cachedParameters->frameSynchronizationInfo.frameCompleteFence, true,
        TIMEOUT_100ms));
    VkResult result = m_deviceContext->getDeviceDriver().getFenceStatus(
        m_deviceContext->device, cachedParameters->frameSynchronizationInfo.frameCompleteFence);
    TCU_CHECK_MSG(result == VK_SUCCESS || result == VK_NOT_READY, "Bad fence status");
}

void VideoBaseDecoder::AddInlineSessionParameters(de::MovePtr<CachedDecodeParameters> &cachedParameters,
                                                  union InlineSessionParameters &inlineSessionParams,
                                                  const void *currentNext)
{
    if ((m_profile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR))
    {
        inlineSessionParams.h264.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_INLINE_SESSION_PARAMETERS_INFO_KHR;
        inlineSessionParams.h264.pNext = currentNext;
        inlineSessionParams.h264.pStdSPS =
            cachedParameters->currentPictureParameterObject->currentStdPictureParameters.h264Sps;
        inlineSessionParams.h264.pStdPPS =
            cachedParameters->currentPictureParameterObject->currentStdPictureParameters.h264Pps;
    }
    else if ((m_profile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR))
    {
        inlineSessionParams.h265.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_INLINE_SESSION_PARAMETERS_INFO_KHR;
        inlineSessionParams.h265.pNext = currentNext;
        inlineSessionParams.h265.pStdVPS =
            cachedParameters->currentPictureParameterObject->currentStdPictureParameters.h265Vps;
        inlineSessionParams.h265.pStdSPS =
            cachedParameters->currentPictureParameterObject->currentStdPictureParameters.h265Sps;
        inlineSessionParams.h265.pStdPPS =
            cachedParameters->currentPictureParameterObject->currentStdPictureParameters.h265Pps;
    }
    else if ((m_profile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR))
    {
        inlineSessionParams.av1.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_INLINE_SESSION_PARAMETERS_INFO_KHR;
        inlineSessionParams.av1.pNext = cachedParameters->pictureParams.decodeFrameInfo.pNext;
        inlineSessionParams.av1.pStdSequenceHeader =
            cachedParameters->currentPictureParameterObject->currentStdPictureParameters.av1SequenceHeader;
    }
}

void VideoBaseDecoder::RecordCommandBuffer(de::MovePtr<CachedDecodeParameters> &cachedParameters)
{
    auto &vk = m_deviceContext->getDeviceDriver();

    VkCommandBuffer commandBuffer = cachedParameters->frameDataSlot.commandBuffer;

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = nullptr;

    VkVideoInlineQueryInfoKHR inlineQueryInfo{};
    inlineQueryInfo.sType = VK_STRUCTURE_TYPE_VIDEO_INLINE_QUERY_INFO_KHR;

    vk.beginCommandBuffer(commandBuffer, &beginInfo);

    if (m_queryResultWithStatus || m_useInlineQueries)
    {
        vk.cmdResetQueryPool(commandBuffer, cachedParameters->frameSynchronizationInfo.queryPool,
                             cachedParameters->frameSynchronizationInfo.startQueryId,
                             cachedParameters->frameSynchronizationInfo.numQueries);
    }

    auto videoSessionParameters = cachedParameters->decodeBeginInfo.videoSessionParameters;
    DE_UNREF(videoSessionParameters);
    if (m_useInlineSessionParams || m_resetCodecNoSessionParams)
        cachedParameters->decodeBeginInfo.videoSessionParameters = VK_NULL_HANDLE;

    vk.cmdBeginVideoCodingKHR(commandBuffer, &cachedParameters->decodeBeginInfo);

    if (cachedParameters->performCodecReset || m_resetCodecNoSessionParams)
    {
        VkVideoCodingControlInfoKHR codingControlInfo = {VK_STRUCTURE_TYPE_VIDEO_CODING_CONTROL_INFO_KHR, nullptr,
                                                         VK_VIDEO_CODING_CONTROL_RESET_BIT_KHR};
        vk.cmdControlVideoCodingKHR(commandBuffer, &codingControlInfo);
    }

    // If we just want to test codec reset without session parameters
    // specified (relaxed session params requirement of VK_KHR_video_maintenance2),
    // we need to restart video coding with valid session params for
    // the decode to continue, unless we are also testing inline
    // session params.
    if (m_resetCodecNoSessionParams && !m_useInlineSessionParams)
    {
        // End video coding first
        VkVideoEndCodingInfoKHR decodeEndInfo{};
        decodeEndInfo.sType = VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR;
        vk.cmdEndVideoCodingKHR(commandBuffer, &decodeEndInfo);

        // Restart video coding with a valid session params
        cachedParameters->decodeBeginInfo.videoSessionParameters = videoSessionParameters;
        vk.cmdBeginVideoCodingKHR(commandBuffer, &cachedParameters->decodeBeginInfo);
    }

    const VkDependencyInfoKHR dependencyInfo = {
        VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
        nullptr,
        VK_DEPENDENCY_BY_REGION_BIT,
        0,
        nullptr,
        1,
        &cachedParameters->bitstreamBufferMemoryBarrier,
        static_cast<uint32_t>(cachedParameters->imageBarriers.size()),
        cachedParameters->imageBarriers.data(),
    };
    vk.cmdPipelineBarrier2(commandBuffer, &dependencyInfo);

    if (m_useInlineQueries)
    {
        const void *currentPNext                              = cachedParameters->pictureParams.decodeFrameInfo.pNext;
        inlineQueryInfo.pNext                                 = currentPNext;
        inlineQueryInfo.queryPool                             = cachedParameters->frameSynchronizationInfo.queryPool;
        inlineQueryInfo.firstQuery                            = cachedParameters->frameSynchronizationInfo.startQueryId;
        inlineQueryInfo.queryCount                            = cachedParameters->frameSynchronizationInfo.numQueries;
        cachedParameters->pictureParams.decodeFrameInfo.pNext = &inlineQueryInfo;
    }

    if (m_queryResultWithStatus && !m_useInlineQueries)
    {
        vk.cmdBeginQuery(commandBuffer, cachedParameters->frameSynchronizationInfo.queryPool,
                         cachedParameters->frameSynchronizationInfo.startQueryId, VkQueryControlFlags());
    }

    union InlineSessionParameters inlineSessionParams
    {
    };

    if (m_useInlineSessionParams)
    {
        AddInlineSessionParameters(cachedParameters, inlineSessionParams,
                                   cachedParameters->pictureParams.decodeFrameInfo.pNext);
        cachedParameters->pictureParams.decodeFrameInfo.pNext = &inlineSessionParams;
    }

    vk.cmdDecodeVideoKHR(commandBuffer, &cachedParameters->pictureParams.decodeFrameInfo);

    if (m_queryResultWithStatus && !m_useInlineQueries)
    {
        vk.cmdEndQuery(commandBuffer, cachedParameters->frameSynchronizationInfo.queryPool,
                       cachedParameters->frameSynchronizationInfo.startQueryId);
    }

    VkVideoEndCodingInfoKHR decodeEndInfo{};
    decodeEndInfo.sType = VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR;
    vk.cmdEndVideoCodingKHR(commandBuffer, &decodeEndInfo);

    m_deviceContext->getDeviceDriver().endCommandBuffer(commandBuffer);
}

void VideoBaseDecoder::SubmitQueue(de::MovePtr<CachedDecodeParameters> &cachedParameters)
{
    auto &vk                      = m_deviceContext->getDeviceDriver();
    auto device                   = m_deviceContext->device;
    VkCommandBuffer commandBuffer = cachedParameters->frameDataSlot.commandBuffer;
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount =
        (cachedParameters->frameSynchronizationInfo.frameConsumerDoneSemaphore == VK_NULL_HANDLE) ? 0 : 1;
    submitInfo.pWaitSemaphores = &cachedParameters->frameSynchronizationInfo.frameConsumerDoneSemaphore;
    VkPipelineStageFlags videoDecodeSubmitWaitStages = VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR;
    submitInfo.pWaitDstStageMask                     = &videoDecodeSubmitWaitStages;
    submitInfo.commandBufferCount                    = 1;
    submitInfo.pCommandBuffers                       = &commandBuffer;
    bool haveSignalSemaphore        = cachedParameters->frameSynchronizationInfo.hasFrameCompleteSignalSemaphore;
    submitInfo.signalSemaphoreCount = haveSignalSemaphore ? 1 : 0;
    submitInfo.pSignalSemaphores =
        haveSignalSemaphore ? &cachedParameters->frameSynchronizationInfo.frameCompleteSemaphore : nullptr;

    VK_CHECK(vk.resetFences(device, 1, &cachedParameters->frameSynchronizationInfo.frameCompleteFence));
    VK_CHECK(vk.queueSubmit(m_deviceContext->decodeQueue, 1, &submitInfo,
                            cachedParameters->frameSynchronizationInfo.frameCompleteFence));

    if (getVideoLogPrintEnable())
    {
        std::cout << ";;; submit frame:"
                  << " PicIdx=" << cachedParameters->pictureParams.currPicIdx
                  << " decodeOrder=" << cachedParameters->picNumInDecodeOrder
                  << " frameCompleteFence=" << cachedParameters->frameSynchronizationInfo.frameCompleteFence
                  << " frameCompleteSem=" << cachedParameters->frameSynchronizationInfo.frameCompleteSemaphore
                  << " dstImageView="
                  << cachedParameters->pictureParams.decodeFrameInfo.dstPictureResource.imageViewBinding << std::endl;
    }
}

void VideoBaseDecoder::QueryDecodeResults(de::MovePtr<CachedDecodeParameters> &cachedParameters)
{
    auto &vk    = m_deviceContext->getDeviceDriver();
    auto device = m_deviceContext->device;

    WaitForFrameFences(cachedParameters);

    VkQueryResultStatusKHR decodeStatus;
    VkResult result = vk.getQueryPoolResults(device, cachedParameters->frameSynchronizationInfo.queryPool,
                                             cachedParameters->frameSynchronizationInfo.startQueryId, 1,
                                             sizeof(decodeStatus), &decodeStatus, sizeof(decodeStatus),
                                             VK_QUERY_RESULT_WITH_STATUS_BIT_KHR | VK_QUERY_RESULT_WAIT_BIT);
    if (getVideoLogPrintEnable())
    {
        std::cout << ";;; QueryDecodeResults:"
                  << " PicIdx=" << cachedParameters->pictureParams.currPicIdx << " status=" << decodeStatus
                  << std::endl;
    }

    TCU_CHECK_AND_THROW(TestError, result == VK_SUCCESS || result == VK_ERROR_DEVICE_LOST,
                        "Driver has returned an invalid query result");
    TCU_CHECK_AND_THROW(TestError, decodeStatus != VK_QUERY_RESULT_STATUS_ERROR_KHR,
                        "Decode query returned an unexpected error");
}

void VideoBaseDecoder::decodeFramesOutOfOrder()
{
    if (getVideoLogPrintEnable())
    {
        tcu::print(";;; Begin out of order decoding\n");
    }

    std::vector<int> ordering(m_cachedDecodeParams.size());
    std::iota(ordering.begin(), ordering.end(), 0);
    if (ordering.size() == 2)
        std::swap(ordering[0], ordering[1]);
    else
    {
        auto rng = std::mt19937(42);
        std::shuffle(ordering.begin(), ordering.end(), rng);
    }
    DE_ASSERT(m_cachedDecodeParams.size() > 1);

    if (getVideoLogPrintEnable())
    {
        tcu::print(";;; record order: ");
        for (int recordOrderIdx : ordering)
            tcu::print("%d ", recordOrderIdx);
        tcu::print("\n");
    }

    // Record out of order
    for (int recordOrderIdx : ordering)
    {
        auto &cachedParams = m_cachedDecodeParams[recordOrderIdx];
        WaitForFrameFences(cachedParams);
        ApplyPictureParameters(cachedParams);
        RecordCommandBuffer(cachedParams);
    }

    // Submit in order
    for (int i = 0; i < m_cachedDecodeParams.size(); i++)
    {
        auto &cachedParams = m_cachedDecodeParams[i];
        SubmitQueue(cachedParams);
        if (m_queryResultWithStatus || m_useInlineQueries)
        {
            QueryDecodeResults(cachedParams);
        }
    }
}

bool VideoBaseDecoder::UpdatePictureParameters(
    VkSharedBaseObj<StdVideoPictureParametersSet> &pictureParametersObject, /* in */
    VkSharedBaseObj<VkVideoRefCountBase> &client)
{
    triggerPictureParameterSequenceCount();

    VkResult result = VkParserVideoPictureParameters::AddPictureParameters(
        *m_deviceContext, m_videoSession, pictureParametersObject, m_currentPictureParameters);
    client = m_currentPictureParameters;
    return (result == VK_SUCCESS);
}

bool VideoBaseDecoder::DisplayPicture(VkPicIf *pNvidiaVulkanPicture, int64_t /*llPTS*/)
{
    vkPicBuffBase *pVkPicBuff = GetPic(pNvidiaVulkanPicture);

    DE_ASSERT(pVkPicBuff != nullptr);
    int32_t picIdx = pVkPicBuff ? pVkPicBuff->m_picIdx : -1;
    DE_ASSERT(picIdx != -1);
    DE_ASSERT(m_videoFrameBuffer != nullptr);

    if (getVideoLogPrintEnable())
    {
        std::cout << ";;; DisplayPicture: " << picIdx << std::endl;
    }

    VulkanVideoDisplayPictureInfo dispInfo = VulkanVideoDisplayPictureInfo();

    dispInfo.timestamp = 0; // NOTE: we ignore PTS in the CTS

    const int32_t retVal = m_videoFrameBuffer->QueueDecodedPictureForDisplay((int8_t)picIdx, &dispInfo);
    DE_ASSERT(picIdx == retVal);
    DE_UNREF(retVal);

    return true;
}

int32_t VideoBaseDecoder::ReleaseDisplayedFrame(DecodedFrame *pDisplayedFrame)
{
    if (pDisplayedFrame->pictureIndex == -1)
        return -1;

    DecodedFrameRelease decodedFramesRelease         = {pDisplayedFrame->pictureIndex, 0, 0, 0, 0, 0};
    DecodedFrameRelease *decodedFramesReleasePtr     = &decodedFramesRelease;
    pDisplayedFrame->pictureIndex                    = -1;
    decodedFramesRelease.decodeOrder                 = pDisplayedFrame->decodeOrder;
    decodedFramesRelease.displayOrder                = pDisplayedFrame->displayOrder;
    decodedFramesRelease.hasConsummerSignalFence     = pDisplayedFrame->hasConsummerSignalFence;
    decodedFramesRelease.hasConsummerSignalSemaphore = pDisplayedFrame->hasConsummerSignalSemaphore;
    decodedFramesRelease.timestamp                   = 0;

    return m_videoFrameBuffer->ReleaseDisplayedPicture(&decodedFramesReleasePtr, 1);
}

VkDeviceSize VideoBaseDecoder::GetBitstreamBuffer(VkDeviceSize newSize, VkDeviceSize minBitstreamBufferOffsetAlignment,
                                                  VkDeviceSize minBitstreamBufferSizeAlignment,
                                                  const uint8_t *pInitializeBufferMemory,
                                                  VkDeviceSize initializeBufferMemorySize,
                                                  VkSharedBaseObj<VulkanBitstreamBuffer> &bitstreamBuffer)
{
    DE_ASSERT(initializeBufferMemorySize <= newSize);

    VkSharedBaseObj<BitstreamBufferImpl> newBitstreamBuffer;
    VK_CHECK(BitstreamBufferImpl::Create(m_deviceContext, m_deviceContext->decodeQueueFamilyIdx(), newSize,
                                         minBitstreamBufferOffsetAlignment, minBitstreamBufferSizeAlignment,
                                         newBitstreamBuffer, m_profile.GetProfileListInfo()));
    DE_ASSERT(newBitstreamBuffer);
    newSize = newBitstreamBuffer->GetMaxSize();
    DE_ASSERT(initializeBufferMemorySize <= newSize);

    size_t bytesToCopy = std::min(initializeBufferMemorySize, newSize);
    size_t bytesCopied =
        newBitstreamBuffer->CopyDataFromBuffer((const uint8_t *)pInitializeBufferMemory, 0, 0, bytesToCopy);
    DE_ASSERT(bytesToCopy == bytesCopied);
    DE_UNREF(bytesCopied);

    newBitstreamBuffer->MemsetData(0x0, bytesToCopy, newSize - bytesToCopy);

    bitstreamBuffer = newBitstreamBuffer;
    if (getVideoLogPrintEnable())
    {
        std::cout << "Allocated bitstream buffer with size " << newSize << " B, " << newSize / 1024 << " KB, "
                  << newSize / 1024 / 1024 << " MB" << std::endl;
    }

    return newBitstreamBuffer->GetMaxSize();
}

void VideoBaseDecoder::UnhandledNALU(const uint8_t *pbData, size_t cbData)
{
    const vector<uint8_t> data(pbData, pbData + cbData);
    ostringstream css;

    css << "UnhandledNALU=";

    for (const auto &i : data)
        css << std::hex << std::setw(2) << std::setfill('0') << (uint32_t)i << ' ';

    TCU_THROW(InternalError, css.str());
}

uint32_t VideoBaseDecoder::FillDpbH264State(const VkParserPictureData *pd, const VkParserH264DpbEntry *dpbIn,
                                            uint32_t maxDpbInSlotsInUse, nvVideoDecodeH264DpbSlotInfo *pDpbRefList,
                                            uint32_t /*maxRefPictures*/, VkVideoReferenceSlotInfoKHR *pReferenceSlots,
                                            int8_t *pGopReferenceImagesIndexes,
                                            StdVideoDecodeH264PictureInfoFlags currPicFlags,
                                            int32_t *pCurrAllocatedSlotIndex)
{
    // #### Update m_dpb based on dpb parameters ####
    // Create unordered DPB and generate a bitmask of all render targets present
    // in DPB
    uint32_t num_ref_frames = pd->CodecSpecific.h264.pStdSps->GetStdH264Sps()->max_num_ref_frames;
    DE_ASSERT(num_ref_frames <= H26X_MAX_DPB_SLOTS);
    DE_ASSERT(num_ref_frames <= m_maxNumDpbSlots);
    // TODO(legacy): Why does AVC require a setup slot to be accounted for here, but not HEVC?
    dpbH264Entry refOnlyDpbIn[H26X_MAX_DPB_SLOTS + 1]; // max number of Dpb
    // surfaces
    memset(&refOnlyDpbIn, 0, m_maxNumDpbSlots * sizeof(refOnlyDpbIn[0]));
    uint32_t refDpbUsedAndValidMask = 0;
    uint32_t numUsedRef             = 0;
    for (int32_t inIdx = 0; (uint32_t)inIdx < maxDpbInSlotsInUse; inIdx++)
    {
        // used_for_reference: 0 = unused, 1 = top_field, 2 = bottom_field, 3 =
        // both_fields
        const uint32_t used_for_reference = dpbIn[inIdx].used_for_reference & fieldIsReferenceMask;
        if (used_for_reference)
        {
            int8_t picIdx = (!dpbIn[inIdx].not_existing && dpbIn[inIdx].pPicBuf) ? GetPicIdx(dpbIn[inIdx].pPicBuf) : -1;
            const bool isFieldRef              = (picIdx >= 0) ?
                                                     GetFieldPicFlag(picIdx) :
                                                     (used_for_reference && (used_for_reference != fieldIsReferenceMask));
            const int16_t fieldOrderCntList[2] = {(int16_t)dpbIn[inIdx].FieldOrderCnt[0],
                                                  (int16_t)dpbIn[inIdx].FieldOrderCnt[1]};
            refOnlyDpbIn[numUsedRef].setReferenceAndTopBottomField(
                !!used_for_reference, (picIdx < 0), /* not_existing is frame inferred by the decoding
                                             process for gaps in frame_num */
                !!dpbIn[inIdx].is_long_term, isFieldRef, !!(used_for_reference & topFieldMask),
                !!(used_for_reference & bottomFieldMask), dpbIn[inIdx].FrameIdx, fieldOrderCntList,
                GetPic(dpbIn[inIdx].pPicBuf));
            if (picIdx >= 0)
            {
                refDpbUsedAndValidMask |= (1 << picIdx);
            }
            numUsedRef++;
        }
        // Invalidate all slots.
        pReferenceSlots[inIdx].slotIndex  = -1;
        pGopReferenceImagesIndexes[inIdx] = -1;
    }

    DE_ASSERT(numUsedRef <= H26X_MAX_DPB_SLOTS);
    DE_ASSERT(numUsedRef <= m_maxNumDpbSlots);
    DE_ASSERT(numUsedRef <= num_ref_frames);

    if (getVideoLogPrintEnable())
    {
        std::cout << " =>>> ********************* picIdx: " << (int32_t)GetPicIdx(pd->pCurrPic)
                  << " *************************" << std::endl;
        std::cout << "\tRef frames data in for picIdx: " << (int32_t)GetPicIdx(pd->pCurrPic) << std::endl
                  << "\tSlot Index:\t\t";
        if (numUsedRef == 0)
            std::cout << "(none)" << std::endl;
        else
        {
            for (uint32_t slot = 0; slot < numUsedRef; slot++)
            {
                if (!refOnlyDpbIn[slot].is_non_existing)
                {
                    std::cout << slot << ",\t";
                }
                else
                {
                    std::cout << 'X' << ",\t";
                }
            }
            std::cout << std::endl;
        }
        std::cout << "\tPict Index:\t\t";
        if (numUsedRef == 0)
            std::cout << "(none)" << std::endl;
        else
        {
            for (uint32_t slot = 0; slot < numUsedRef; slot++)
            {
                if (!refOnlyDpbIn[slot].is_non_existing)
                {
                    std::cout << refOnlyDpbIn[slot].m_picBuff->m_picIdx << ",\t";
                }
                else
                {
                    std::cout << 'X' << ",\t";
                }
            }
        }
        std::cout << "\n\tTotal Ref frames for picIdx: " << (int32_t)GetPicIdx(pd->pCurrPic) << " : " << numUsedRef
                  << " out of " << num_ref_frames << " MAX(" << m_maxNumDpbSlots << ")" << std::endl
                  << std::endl;

        std::cout << std::flush;
    }

    // Map all frames not present in DPB as non-reference, and generate a mask of
    // all used DPB entries
    /* uint32_t destUsedDpbMask = */ ResetPicDpbSlots(refDpbUsedAndValidMask);

    // Now, map DPB render target indices to internal frame buffer index,
    // assign each reference a unique DPB entry, and create the ordered DPB
    // This is an undocumented MV restriction: the position in the DPB is stored
    // along with the co-located data, so once a reference frame is assigned a DPB
    // entry, it can no longer change.

    // Find or allocate slots for existing dpb items.
    // Take into account the reference picture now.
    int8_t currPicIdx = GetPicIdx(pd->pCurrPic);
    DE_ASSERT(currPicIdx >= 0);
    int8_t bestNonExistingPicIdx = currPicIdx;
    if (refDpbUsedAndValidMask)
    {
        int32_t minFrameNumDiff = 0x10000;
        for (int32_t dpbIdx = 0; (uint32_t)dpbIdx < numUsedRef; dpbIdx++)
        {
            if (!refOnlyDpbIn[dpbIdx].is_non_existing)
            {
                vkPicBuffBase *picBuff = refOnlyDpbIn[dpbIdx].m_picBuff;
                int8_t picIdx          = GetPicIdx(picBuff); // should always be valid at this point
                DE_ASSERT(picIdx >= 0);
                // We have up to 17 internal frame buffers, but only MAX_DPB_SIZE dpb
                // entries, so we need to re-map the index from the [0..MAX_DPB_SIZE]
                // range to [0..15]
                int8_t dpbSlot = GetPicDpbSlot(picIdx);
                if (dpbSlot < 0)
                {
                    dpbSlot = m_dpb.AllocateSlot();
                    DE_ASSERT((dpbSlot >= 0) && ((uint32_t)dpbSlot < m_maxNumDpbSlots));
                    SetPicDpbSlot(picIdx, dpbSlot);
                    m_dpb[dpbSlot].setPictureResource(picBuff, m_nCurrentPictureID);
                }
                m_dpb[dpbSlot].MarkInUse(m_nCurrentPictureID);
                DE_ASSERT(dpbSlot >= 0);

                if (dpbSlot >= 0)
                {
                    refOnlyDpbIn[dpbIdx].dpbSlot = dpbSlot;
                }
                else
                {
                    // This should never happen
                    printf("DPB mapping logic broken!\n");
                    DE_ASSERT(0);
                }

                int32_t frameNumDiff = ((int32_t)pd->CodecSpecific.h264.frame_num - refOnlyDpbIn[dpbIdx].FrameIdx);
                if (frameNumDiff <= 0)
                {
                    frameNumDiff = 0xffff;
                }
                if (frameNumDiff < minFrameNumDiff)
                {
                    bestNonExistingPicIdx = picIdx;
                    minFrameNumDiff       = frameNumDiff;
                }
                else if (bestNonExistingPicIdx == currPicIdx)
                {
                    bestNonExistingPicIdx = picIdx;
                }
            }
        }
    }
    // In Vulkan, we always allocate a Dbp slot for the current picture,
    // regardless if it is going to become a reference or not. Non-reference slots
    // get freed right after usage. if (pd->ref_pic_flag) {
    int8_t currPicDpbSlot = AllocateDpbSlotForCurrentH264(GetPic(pd->pCurrPic), currPicFlags, pd->current_dpb_id);
    DE_ASSERT(currPicDpbSlot >= 0);
    *pCurrAllocatedSlotIndex = currPicDpbSlot;

    if (refDpbUsedAndValidMask)
    {
        // Find or allocate slots for non existing dpb items and populate the slots.
        uint32_t dpbInUseMask          = m_dpb.getSlotInUseMask();
        int8_t firstNonExistingDpbSlot = 0;
        for (uint32_t dpbIdx = 0; dpbIdx < numUsedRef; dpbIdx++)
        {
            int8_t dpbSlot = -1;
            int8_t picIdx  = -1;
            if (refOnlyDpbIn[dpbIdx].is_non_existing)
            {
                DE_ASSERT(refOnlyDpbIn[dpbIdx].m_picBuff == NULL);
                while (((uint32_t)firstNonExistingDpbSlot < m_maxNumDpbSlots) && (dpbSlot == -1))
                {
                    if (!(dpbInUseMask & (1 << firstNonExistingDpbSlot)))
                    {
                        dpbSlot = firstNonExistingDpbSlot;
                    }
                    firstNonExistingDpbSlot++;
                }
                DE_ASSERT((dpbSlot >= 0) && ((uint32_t)dpbSlot < m_maxNumDpbSlots));
                picIdx = bestNonExistingPicIdx;
                // Find the closest valid refpic already in the DPB
                uint32_t minDiffPOC = 0x7fff;
                for (uint32_t j = 0; j < numUsedRef; j++)
                {
                    if (!refOnlyDpbIn[j].is_non_existing &&
                        (refOnlyDpbIn[j].used_for_reference & refOnlyDpbIn[dpbIdx].used_for_reference) ==
                            refOnlyDpbIn[dpbIdx].used_for_reference)
                    {
                        uint32_t diffPOC =
                            abs((int32_t)(refOnlyDpbIn[j].FieldOrderCnt[0] - refOnlyDpbIn[dpbIdx].FieldOrderCnt[0]));
                        if (diffPOC <= minDiffPOC)
                        {
                            minDiffPOC = diffPOC;
                            picIdx     = GetPicIdx(refOnlyDpbIn[j].m_picBuff);
                        }
                    }
                }
            }
            else
            {
                DE_ASSERT(refOnlyDpbIn[dpbIdx].m_picBuff != NULL);
                dpbSlot = refOnlyDpbIn[dpbIdx].dpbSlot;
                picIdx  = GetPicIdx(refOnlyDpbIn[dpbIdx].m_picBuff);
            }
            DE_ASSERT((dpbSlot >= 0) && ((uint32_t)dpbSlot < m_maxNumDpbSlots));
            refOnlyDpbIn[dpbIdx].setH264PictureData(pDpbRefList, pReferenceSlots, dpbIdx, dpbSlot,
                                                    pd->progressive_frame, getVideoLogPrintEnable());
            pGopReferenceImagesIndexes[dpbIdx] = picIdx;
        }
    }

    if (getVideoLogPrintEnable())
    {
        uint32_t slotInUseMask   = m_dpb.getSlotInUseMask();
        uint32_t slotsInUseCount = 0;
        std::cout << "\tAllocated DPB slot " << (int32_t)currPicDpbSlot << " for "
                  << (pd->ref_pic_flag ? "REFERENCE" : "NON-REFERENCE") << " picIdx: " << (int32_t)currPicIdx
                  << std::endl;
        std::cout << "\tDPB frames map for picIdx: " << (int32_t)currPicIdx << std::endl << "\tSlot Index:\t\t";
        for (uint32_t slot = 0; slot < m_dpb.getMaxSize(); slot++)
        {
            if (slotInUseMask & (1 << slot))
            {
                std::cout << slot << ",\t";
                slotsInUseCount++;
            }
            else
            {
                std::cout << 'X' << ",\t";
            }
        }
        std::cout << std::endl << "\tPict Index:\t\t";
        for (uint32_t slot = 0; slot < m_dpb.getMaxSize(); slot++)
        {
            if (slotInUseMask & (1 << slot))
            {
                if (m_dpb[slot].getPictureResource())
                {
                    std::cout << m_dpb[slot].getPictureResource()->m_picIdx << ",\t";
                }
                else
                {
                    std::cout << "non existent"
                              << ",\t";
                }
            }
            else
            {
                std::cout << 'X' << ",\t";
            }
        }
        std::cout << "\n\tTotal slots in use for picIdx: " << (int32_t)currPicIdx << " : " << slotsInUseCount
                  << " out of " << m_dpb.getMaxSize() << std::endl;
        std::cout << " <<<= ********************* picIdx: " << (int32_t)GetPicIdx(pd->pCurrPic)
                  << " *************************" << std::endl
                  << std::endl;
        std::cout << std::flush;
    }
    return refDpbUsedAndValidMask ? numUsedRef : 0;
}

uint32_t VideoBaseDecoder::FillDpbH265State(const VkParserPictureData *pd, const VkParserHevcPictureData *pin,
                                            nvVideoDecodeH265DpbSlotInfo *pDpbSlotInfo,
                                            StdVideoDecodeH265PictureInfo *pStdPictureInfo, uint32_t /*maxRefPictures*/,
                                            VkVideoReferenceSlotInfoKHR *pReferenceSlots,
                                            int8_t *pGopReferenceImagesIndexes, int32_t *pCurrAllocatedSlotIndex)
{
    // #### Update m_dpb based on dpb parameters ####
    // Create unordered DPB and generate a bitmask of all render targets present
    // in DPB
    dpbH264Entry refOnlyDpbIn[H26X_MAX_DPB_SLOTS];
    DE_ASSERT(m_maxNumDpbSlots <= H26X_MAX_DPB_SLOTS);
    memset(&refOnlyDpbIn, 0, m_maxNumDpbSlots * sizeof(refOnlyDpbIn[0]));
    uint32_t refDpbUsedAndValidMask = 0;
    uint32_t numUsedRef             = 0;
    if (getVideoLogPrintEnable())
        std::cout << "Ref frames data: " << std::endl;
    for (int32_t inIdx = 0; inIdx < H26X_MAX_DPB_SLOTS; inIdx++)
    {
        // used_for_reference: 0 = unused, 1 = top_field, 2 = bottom_field, 3 =
        // both_fields
        int8_t picIdx = GetPicIdx(pin->RefPics[inIdx]);
        if (picIdx >= 0)
        {
            DE_ASSERT(numUsedRef < H26X_MAX_DPB_SLOTS);
            refOnlyDpbIn[numUsedRef].setReference((pin->IsLongTerm[inIdx] == 1), pin->PicOrderCntVal[inIdx],
                                                  GetPic(pin->RefPics[inIdx]));
            if (picIdx >= 0)
            {
                refDpbUsedAndValidMask |= (1 << picIdx);
            }
            refOnlyDpbIn[numUsedRef].originalDpbIndex = inIdx;
            numUsedRef++;
        }
        // Invalidate all slots.
        pReferenceSlots[inIdx].slotIndex  = -1;
        pGopReferenceImagesIndexes[inIdx] = -1;
    }

    if (getVideoLogPrintEnable())
        std::cout << "Total Ref frames: " << numUsedRef << std::endl;

    DE_ASSERT(numUsedRef <= m_maxNumDpbSlots);
    DE_ASSERT(numUsedRef <= H26X_MAX_DPB_SLOTS);

    // Take into account the reference picture now.
    int8_t currPicIdx = GetPicIdx(pd->pCurrPic);
    DE_ASSERT(currPicIdx >= 0);
    if (currPicIdx >= 0)
    {
        refDpbUsedAndValidMask |= (1 << currPicIdx);
    }

    // Map all frames not present in DPB as non-reference, and generate a mask of
    // all used DPB entries
    /* uint32_t destUsedDpbMask = */ ResetPicDpbSlots(refDpbUsedAndValidMask);

    // Now, map DPB render target indices to internal frame buffer index,
    // assign each reference a unique DPB entry, and create the ordered DPB
    // This is an undocumented MV restriction: the position in the DPB is stored
    // along with the co-located data, so once a reference frame is assigned a DPB
    // entry, it can no longer change.

    int8_t frmListToDpb[H26X_MAX_DPB_SLOTS];
    // TODO change to -1 for invalid indexes.
    memset(&frmListToDpb, 0, sizeof(frmListToDpb));
    // Find or allocate slots for existing dpb items.
    for (int32_t dpbIdx = 0; (uint32_t)dpbIdx < numUsedRef; dpbIdx++)
    {
        if (!refOnlyDpbIn[dpbIdx].is_non_existing)
        {
            vkPicBuffBase *picBuff = refOnlyDpbIn[dpbIdx].m_picBuff;
            int32_t picIdx         = GetPicIdx(picBuff); // should always be valid at this point
            DE_ASSERT(picIdx >= 0);
            // We have up to 17 internal frame buffers, but only H26X_MAX_DPB_SLOTS
            // dpb entries, so we need to re-map the index from the
            // [0..H26X_MAX_DPB_SLOTS] range to [0..15]
            int8_t dpbSlot = GetPicDpbSlot(picIdx);
            if (dpbSlot < 0)
            {
                dpbSlot = m_dpb.AllocateSlot();
                DE_ASSERT(dpbSlot >= 0);
                SetPicDpbSlot(picIdx, dpbSlot);
                m_dpb[dpbSlot].setPictureResource(picBuff, m_nCurrentPictureID);
            }
            m_dpb[dpbSlot].MarkInUse(m_nCurrentPictureID);
            DE_ASSERT(dpbSlot >= 0);

            if (dpbSlot >= 0)
            {
                refOnlyDpbIn[dpbIdx].dpbSlot = dpbSlot;
                uint32_t originalDpbIndex    = refOnlyDpbIn[dpbIdx].originalDpbIndex;
                DE_ASSERT(originalDpbIndex < H26X_MAX_DPB_SLOTS);
                frmListToDpb[originalDpbIndex] = dpbSlot;
            }
            else
            {
                // This should never happen
                printf("DPB mapping logic broken!\n");
                DE_ASSERT(0);
            }
        }
    }

    // Find or allocate slots for non existing dpb items and populate the slots.
    uint32_t dpbInUseMask          = m_dpb.getSlotInUseMask();
    int8_t firstNonExistingDpbSlot = 0;
    for (uint32_t dpbIdx = 0; dpbIdx < numUsedRef; dpbIdx++)
    {
        int8_t dpbSlot = -1;
        if (refOnlyDpbIn[dpbIdx].is_non_existing)
        {
            // There shouldn't be  not_existing in h.265
            DE_ASSERT(0);
            DE_ASSERT(refOnlyDpbIn[dpbIdx].m_picBuff == NULL);
            while (((uint32_t)firstNonExistingDpbSlot < m_maxNumDpbSlots) && (dpbSlot == -1))
            {
                if (!(dpbInUseMask & (1 << firstNonExistingDpbSlot)))
                {
                    dpbSlot = firstNonExistingDpbSlot;
                }
                firstNonExistingDpbSlot++;
            }
            DE_ASSERT((dpbSlot >= 0) && ((uint32_t)dpbSlot < m_maxNumDpbSlots));
        }
        else
        {
            DE_ASSERT(refOnlyDpbIn[dpbIdx].m_picBuff != NULL);
            dpbSlot = refOnlyDpbIn[dpbIdx].dpbSlot;
        }
        DE_ASSERT((dpbSlot >= 0) && (dpbSlot < H26X_MAX_DPB_SLOTS));
        refOnlyDpbIn[dpbIdx].setH265PictureData(pDpbSlotInfo, pReferenceSlots, dpbIdx, dpbSlot,
                                                getVideoLogPrintEnable());
        pGopReferenceImagesIndexes[dpbIdx] = GetPicIdx(refOnlyDpbIn[dpbIdx].m_picBuff);
    }

    if (getVideoLogPrintEnable())
    {
        std::cout << "frmListToDpb:" << std::endl;
        for (int8_t dpbResIdx = 0; dpbResIdx < H26X_MAX_DPB_SLOTS; dpbResIdx++)
        {
            std::cout << "\tfrmListToDpb[" << (int32_t)dpbResIdx << "] is " << (int32_t)frmListToDpb[dpbResIdx]
                      << std::endl;
        }
    }

    int32_t numPocStCurrBefore = 0;
    const size_t maxNumPocStCurrBefore =
        sizeof(pStdPictureInfo->RefPicSetStCurrBefore) / sizeof(pStdPictureInfo->RefPicSetStCurrBefore[0]);
    DE_ASSERT((size_t)pin->NumPocStCurrBefore <= maxNumPocStCurrBefore);
    if ((size_t)pin->NumPocStCurrBefore > maxNumPocStCurrBefore)
    {
        tcu::print(
            "\nERROR: FillDpbH265State() pin->NumPocStCurrBefore(%d) must be smaller than maxNumPocStCurrBefore(%zd)\n",
            pin->NumPocStCurrBefore, maxNumPocStCurrBefore);
    }
    for (int32_t i = 0; i < pin->NumPocStCurrBefore; i++)
    {
        uint8_t idx = (uint8_t)pin->RefPicSetStCurrBefore[i];
        if (idx < H26X_MAX_DPB_SLOTS)
        {
            if (getVideoLogPrintEnable())
                std::cout << "\trefPicSetStCurrBefore[" << i << "] is " << (int32_t)idx << " -> "
                          << (int32_t)frmListToDpb[idx] << std::endl;
            pStdPictureInfo->RefPicSetStCurrBefore[numPocStCurrBefore++] = frmListToDpb[idx] & 0xf;
        }
    }
    while (numPocStCurrBefore < 8)
    {
        pStdPictureInfo->RefPicSetStCurrBefore[numPocStCurrBefore++] = 0xff;
    }

    int32_t numPocStCurrAfter = 0;
    const size_t maxNumPocStCurrAfter =
        sizeof(pStdPictureInfo->RefPicSetStCurrAfter) / sizeof(pStdPictureInfo->RefPicSetStCurrAfter[0]);
    DE_ASSERT((size_t)pin->NumPocStCurrAfter <= maxNumPocStCurrAfter);
    if ((size_t)pin->NumPocStCurrAfter > maxNumPocStCurrAfter)
    {
        fprintf(
            stderr,
            "\nERROR: FillDpbH265State() pin->NumPocStCurrAfter(%d) must be smaller than maxNumPocStCurrAfter(%zd)\n",
            pin->NumPocStCurrAfter, maxNumPocStCurrAfter);
    }
    for (int32_t i = 0; i < pin->NumPocStCurrAfter; i++)
    {
        uint8_t idx = (uint8_t)pin->RefPicSetStCurrAfter[i];
        if (idx < H26X_MAX_DPB_SLOTS)
        {
            if (getVideoLogPrintEnable())
                std::cout << "\trefPicSetStCurrAfter[" << i << "] is " << (int32_t)idx << " -> "
                          << (int32_t)frmListToDpb[idx] << std::endl;
            pStdPictureInfo->RefPicSetStCurrAfter[numPocStCurrAfter++] = frmListToDpb[idx] & 0xf;
        }
    }
    while (numPocStCurrAfter < 8)
    {
        pStdPictureInfo->RefPicSetStCurrAfter[numPocStCurrAfter++] = 0xff;
    }

    int32_t numPocLtCurr = 0;
    const size_t maxNumPocLtCurr =
        sizeof(pStdPictureInfo->RefPicSetLtCurr) / sizeof(pStdPictureInfo->RefPicSetLtCurr[0]);
    DE_ASSERT((size_t)pin->NumPocLtCurr <= maxNumPocLtCurr);
    if ((size_t)pin->NumPocLtCurr > maxNumPocLtCurr)
    {
        fprintf(stderr, "\nERROR: FillDpbH265State() pin->NumPocLtCurr(%d) must be smaller than maxNumPocLtCurr(%zd)\n",
                pin->NumPocLtCurr, maxNumPocLtCurr);
    }
    for (int32_t i = 0; i < pin->NumPocLtCurr; i++)
    {
        uint8_t idx = (uint8_t)pin->RefPicSetLtCurr[i];
        if (idx < H26X_MAX_DPB_SLOTS)
        {
            if (getVideoLogPrintEnable())
                std::cout << "\trefPicSetLtCurr[" << i << "] is " << (int32_t)idx << " -> "
                          << (int32_t)frmListToDpb[idx] << std::endl;
            pStdPictureInfo->RefPicSetLtCurr[numPocLtCurr++] = frmListToDpb[idx] & 0xf;
        }
    }
    while (numPocLtCurr < 8)
    {
        pStdPictureInfo->RefPicSetLtCurr[numPocLtCurr++] = 0xff;
    }

    for (int32_t i = 0; i < 8; i++)
    {
        if (getVideoLogPrintEnable())
            std::cout << "\tlist indx " << i << ": "
                      << " refPicSetStCurrBefore: " << (int32_t)pStdPictureInfo->RefPicSetStCurrBefore[i]
                      << " refPicSetStCurrAfter: " << (int32_t)pStdPictureInfo->RefPicSetStCurrAfter[i]
                      << " refPicSetLtCurr: " << (int32_t)pStdPictureInfo->RefPicSetLtCurr[i] << std::endl;
    }

    int8_t dpbSlot = AllocateDpbSlotForCurrentH265(GetPic(pd->pCurrPic), true /* isReference */, pd->current_dpb_id);
    *pCurrAllocatedSlotIndex = dpbSlot;
    DE_ASSERT(!(dpbSlot < 0));
    if (dpbSlot >= 0)
    {
        // TODO: The NVIDIA DPB management is quite broken, and always wants to allocate DPBs even for non-reference frames.
        //DE_ASSERT(pd->ref_pic_flag);
    }

    return numUsedRef;
}

int8_t VideoBaseDecoder::AllocateDpbSlotForCurrentH264(vkPicBuffBase *pPic,
                                                       StdVideoDecodeH264PictureInfoFlags currPicFlags,
                                                       int8_t /*presetDpbSlot*/)
{
    // Now, map the current render target
    int8_t dpbSlot    = -1;
    int8_t currPicIdx = GetPicIdx(pPic);
    DE_ASSERT(currPicIdx >= 0);
    SetFieldPicFlag(currPicIdx, currPicFlags.field_pic_flag);
    // In Vulkan we always allocate reference slot for the current picture.
    if (true /* currPicFlags.is_reference */)
    {
        dpbSlot = GetPicDpbSlot(currPicIdx);
        if (dpbSlot < 0)
        {
            dpbSlot = m_dpb.AllocateSlot();
            DE_ASSERT(dpbSlot >= 0);
            SetPicDpbSlot(currPicIdx, dpbSlot);
            m_dpb[dpbSlot].setPictureResource(pPic, m_nCurrentPictureID);
        }
        DE_ASSERT(dpbSlot >= 0);
    }
    return dpbSlot;
}

int8_t VideoBaseDecoder::AllocateDpbSlotForCurrentH265(vkPicBuffBase *pPic, bool isReference, int8_t /*presetDpbSlot*/)
{
    // Now, map the current render target
    int8_t dpbSlot    = -1;
    int8_t currPicIdx = GetPicIdx(pPic);
    DE_ASSERT(currPicIdx >= 0);
    DE_ASSERT(isReference);
    if (isReference)
    {
        dpbSlot = GetPicDpbSlot(currPicIdx);
        if (dpbSlot < 0)
        {
            dpbSlot = m_dpb.AllocateSlot();
            DE_ASSERT(dpbSlot >= 0);
            SetPicDpbSlot(currPicIdx, dpbSlot);
            m_dpb[dpbSlot].setPictureResource(pPic, m_nCurrentPictureID);
        }
        DE_ASSERT(dpbSlot >= 0);
    }
    return dpbSlot;
}

VkFormat getRecommendedFormat(const vector<VkFormat> &formats, VkFormat recommendedFormat)
{
    if (formats.empty())
        return VK_FORMAT_UNDEFINED;
    else if (recommendedFormat != VK_FORMAT_UNDEFINED &&
             std::find(formats.begin(), formats.end(), recommendedFormat) != formats.end())
        return recommendedFormat;
    else
        return formats[0];
}

VkResult VulkanVideoSession::Create(DeviceContext &vkDevCtx, uint32_t videoQueueFamily,
                                    VkVideoCoreProfile *pVideoProfile, VkFormat pictureFormat,
                                    const VkExtent2D &maxCodedExtent, VkFormat referencePicturesFormat,
                                    uint32_t maxDpbSlots, uint32_t maxActiveReferencePictures,
                                    bool useInlineVideoQueries, bool useInlineParameters,
                                    VkSharedBaseObj<VulkanVideoSession> &videoSession)
{
    auto &vk    = vkDevCtx.getDeviceDriver();
    auto device = vkDevCtx.device;

    VulkanVideoSession *pNewVideoSession = new VulkanVideoSession(vkDevCtx, pVideoProfile);

    static const VkExtensionProperties h264DecodeStdExtensionVersion = {
        VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_SPEC_VERSION};
    static const VkExtensionProperties h265DecodeStdExtensionVersion = {
        VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_SPEC_VERSION};
    static const VkExtensionProperties av1DecodeStdExtensionVersion = {
        VK_STD_VULKAN_VIDEO_CODEC_AV1_DECODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_AV1_DECODE_SPEC_VERSION};
    static const VkExtensionProperties vp9DecodeStdExtensionVersion = {
        VK_STD_VULKAN_VIDEO_CODEC_VP9_DECODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_VP9_DECODE_SPEC_VERSION};
    static const VkExtensionProperties h264EncodeStdExtensionVersion = {
        VK_STD_VULKAN_VIDEO_CODEC_H264_ENCODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H264_ENCODE_SPEC_VERSION};
    static const VkExtensionProperties h265EncodeStdExtensionVersion = {
        VK_STD_VULKAN_VIDEO_CODEC_H265_ENCODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H265_ENCODE_SPEC_VERSION};

    const auto flags = (useInlineVideoQueries ? VK_VIDEO_SESSION_CREATE_INLINE_QUERIES_BIT_KHR : 0) |
                       (useInlineParameters ? VK_VIDEO_SESSION_CREATE_INLINE_SESSION_PARAMETERS_BIT_KHR : 0);

    VkVideoSessionCreateInfoKHR &createInfo = pNewVideoSession->m_createInfo;
    createInfo.flags                        = flags;
    createInfo.pVideoProfile                = pVideoProfile->GetProfile();
    createInfo.queueFamilyIndex             = videoQueueFamily;
    createInfo.pictureFormat                = pictureFormat;
    createInfo.maxCodedExtent               = maxCodedExtent;
    createInfo.maxDpbSlots                  = maxDpbSlots;
    createInfo.maxActiveReferencePictures   = maxActiveReferencePictures;
    createInfo.referencePictureFormat       = referencePicturesFormat;

    switch ((int32_t)pVideoProfile->GetCodecType())
    {
    case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
        createInfo.pStdHeaderVersion = &h264DecodeStdExtensionVersion;
        break;
    case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
        createInfo.pStdHeaderVersion = &h265DecodeStdExtensionVersion;
        break;
    case VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR:
        createInfo.pStdHeaderVersion = &av1DecodeStdExtensionVersion;
        break;
    case VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR:
        createInfo.pStdHeaderVersion = &vp9DecodeStdExtensionVersion;
        break;
    case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR:
        createInfo.pStdHeaderVersion = &h264EncodeStdExtensionVersion;
        break;
    case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR:
        createInfo.pStdHeaderVersion = &h265EncodeStdExtensionVersion;
        break;
    default:
        DE_ASSERT(0);
    }
    VkResult result = vk.createVideoSessionKHR(device, &createInfo, NULL, &pNewVideoSession->m_videoSession);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    uint32_t videoSessionMemoryRequirementsCount = 0;
    VkVideoSessionMemoryRequirementsKHR decodeSessionMemoryRequirements[MAX_BOUND_MEMORY];
    // Get the count first
    result = vk.getVideoSessionMemoryRequirementsKHR(device, pNewVideoSession->m_videoSession,
                                                     &videoSessionMemoryRequirementsCount, NULL);
    DE_ASSERT(result == VK_SUCCESS);
    DE_ASSERT(videoSessionMemoryRequirementsCount <= MAX_BOUND_MEMORY);

    memset(decodeSessionMemoryRequirements, 0x00, sizeof(decodeSessionMemoryRequirements));
    for (uint32_t i = 0; i < videoSessionMemoryRequirementsCount; i++)
    {
        decodeSessionMemoryRequirements[i].sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR;
    }

    result =
        vk.getVideoSessionMemoryRequirementsKHR(device, pNewVideoSession->m_videoSession,
                                                &videoSessionMemoryRequirementsCount, decodeSessionMemoryRequirements);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    uint32_t decodeSessionBindMemoryCount = videoSessionMemoryRequirementsCount;
    VkBindVideoSessionMemoryInfoKHR decodeSessionBindMemory[MAX_BOUND_MEMORY];

    for (uint32_t memIdx = 0; memIdx < decodeSessionBindMemoryCount; memIdx++)
    {

        uint32_t memoryTypeIndex = 0;
        uint32_t memoryTypeBits  = decodeSessionMemoryRequirements[memIdx].memoryRequirements.memoryTypeBits;
        if (memoryTypeBits == 0)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        // Find an available memory type that satisfies the requested properties.
        for (; !(memoryTypeBits & 1); memoryTypeIndex++)
        {
            memoryTypeBits >>= 1;
        }

        VkMemoryAllocateInfo memInfo = {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,                          // sType
            NULL,                                                            // pNext
            decodeSessionMemoryRequirements[memIdx].memoryRequirements.size, // allocationSize
            memoryTypeIndex,                                                 // memoryTypeIndex
        };

        result = vk.allocateMemory(device, &memInfo, 0, &pNewVideoSession->m_memoryBound[memIdx]);
        if (result != VK_SUCCESS)
        {
            return result;
        }

        DE_ASSERT(result == VK_SUCCESS);
        decodeSessionBindMemory[memIdx].pNext  = NULL;
        decodeSessionBindMemory[memIdx].sType  = VK_STRUCTURE_TYPE_BIND_VIDEO_SESSION_MEMORY_INFO_KHR;
        decodeSessionBindMemory[memIdx].memory = pNewVideoSession->m_memoryBound[memIdx];

        decodeSessionBindMemory[memIdx].memoryBindIndex = decodeSessionMemoryRequirements[memIdx].memoryBindIndex;
        decodeSessionBindMemory[memIdx].memoryOffset    = 0;
        decodeSessionBindMemory[memIdx].memorySize = decodeSessionMemoryRequirements[memIdx].memoryRequirements.size;
    }

    result = vk.bindVideoSessionMemoryKHR(device, pNewVideoSession->m_videoSession, decodeSessionBindMemoryCount,
                                          decodeSessionBindMemory);
    DE_ASSERT(result == VK_SUCCESS);

    videoSession = pNewVideoSession;

    // Make sure we do not use dangling (on the stack) pointers
    createInfo.pNext = nullptr;

    return result;
}

VkResult VkImageResource::Create(DeviceContext &vkDevCtx, const VkImageCreateInfo *pImageCreateInfo,
                                 VkSharedBaseObj<VkImageResource> &imageResource)
{
    imageResource = new VkImageResource(vkDevCtx, pImageCreateInfo);

    return VK_SUCCESS;
}

VkResult VkImageResourceView::Create(DeviceContext &vkDevCtx, VkSharedBaseObj<VkImageResource> &imageResource,
                                     const VkImageCreateInfo *pImageCreateInfo,
                                     VkImageSubresourceRange &imageSubresourceRange,
                                     VkSharedBaseObj<VkImageResourceView> &imageResourceView)
{
    auto &vk        = vkDevCtx.getDeviceDriver();
    VkDevice device = vkDevCtx.device;
    VkImageView imageView;
    VkImageViewCreateInfo viewInfo = VkImageViewCreateInfo();
    viewInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.pNext                 = nullptr;
    viewInfo.image                 = imageResource->GetImage();
    viewInfo.viewType   = pImageCreateInfo->arrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format     = imageResource->GetImageCreateInfo().format;
    viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY};
    viewInfo.subresourceRange = imageSubresourceRange;
    viewInfo.flags            = 0;
    VkResult result           = vk.createImageView(device, &viewInfo, nullptr, &imageView);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    imageResourceView = new VkImageResourceView(vkDevCtx, imageResource, imageView, imageSubresourceRange);

    return result;
}

VkImageResourceView::~VkImageResourceView()
{
    auto &vk    = m_vkDevCtx.getDeviceDriver();
    auto device = m_vkDevCtx.device;

    if (m_imageView != VK_NULL_HANDLE)
    {
        vk.destroyImageView(device, m_imageView, nullptr);
        m_imageView = VK_NULL_HANDLE;
    }

    m_imageResource = nullptr;
}

const char *VkParserVideoPictureParameters::m_refClassId = "VkParserVideoPictureParameters";
int32_t VkParserVideoPictureParameters::m_currentId      = 0;

int32_t VkParserVideoPictureParameters::PopulateH264UpdateFields(
    const StdVideoPictureParametersSet *pStdPictureParametersSet,
    VkVideoDecodeH264SessionParametersAddInfoKHR &h264SessionParametersAddInfo)
{
    int32_t currentId = -1;
    if (pStdPictureParametersSet == nullptr)
    {
        return currentId;
    }

    DE_ASSERT((pStdPictureParametersSet->GetStdType() == StdVideoPictureParametersSet::TYPE_H264_SPS) ||
              (pStdPictureParametersSet->GetStdType() == StdVideoPictureParametersSet::TYPE_H264_PPS));

    DE_ASSERT(h264SessionParametersAddInfo.sType ==
              VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_ADD_INFO_KHR);

    if (pStdPictureParametersSet->GetStdType() == StdVideoPictureParametersSet::TYPE_H264_SPS)
    {
        h264SessionParametersAddInfo.stdSPSCount = 1;
        h264SessionParametersAddInfo.pStdSPSs    = pStdPictureParametersSet->GetStdH264Sps();
        bool isSps                               = false;
        currentId                                = pStdPictureParametersSet->GetSpsId(isSps);
        DE_ASSERT(isSps);
    }
    else if (pStdPictureParametersSet->GetStdType() == StdVideoPictureParametersSet::TYPE_H264_PPS)
    {
        h264SessionParametersAddInfo.stdPPSCount = 1;
        h264SessionParametersAddInfo.pStdPPSs    = pStdPictureParametersSet->GetStdH264Pps();
        bool isPps                               = false;
        currentId                                = pStdPictureParametersSet->GetPpsId(isPps);
        DE_ASSERT(isPps);
    }
    else
    {
        DE_ASSERT(!"Incorrect h.264 type");
    }

    return currentId;
}

int32_t VkParserVideoPictureParameters::PopulateH265UpdateFields(
    const StdVideoPictureParametersSet *pStdPictureParametersSet,
    VkVideoDecodeH265SessionParametersAddInfoKHR &h265SessionParametersAddInfo)
{
    int32_t currentId = -1;
    if (pStdPictureParametersSet == nullptr)
    {
        return currentId;
    }

    DE_ASSERT((pStdPictureParametersSet->GetStdType() == StdVideoPictureParametersSet::TYPE_H265_VPS) ||
              (pStdPictureParametersSet->GetStdType() == StdVideoPictureParametersSet::TYPE_H265_SPS) ||
              (pStdPictureParametersSet->GetStdType() == StdVideoPictureParametersSet::TYPE_H265_PPS));

    DE_ASSERT(h265SessionParametersAddInfo.sType ==
              VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_SESSION_PARAMETERS_ADD_INFO_KHR);

    if (pStdPictureParametersSet->GetStdType() == StdVideoPictureParametersSet::TYPE_H265_VPS)
    {
        h265SessionParametersAddInfo.stdVPSCount = 1;
        h265SessionParametersAddInfo.pStdVPSs    = pStdPictureParametersSet->GetStdH265Vps();
        bool isVps                               = false;
        currentId                                = pStdPictureParametersSet->GetVpsId(isVps);
        DE_ASSERT(isVps);
    }
    else if (pStdPictureParametersSet->GetStdType() == StdVideoPictureParametersSet::TYPE_H265_SPS)
    {
        h265SessionParametersAddInfo.stdSPSCount = 1;
        h265SessionParametersAddInfo.pStdSPSs    = pStdPictureParametersSet->GetStdH265Sps();
        bool isSps                               = false;
        currentId                                = pStdPictureParametersSet->GetSpsId(isSps);
        DE_ASSERT(isSps);
    }
    else if (pStdPictureParametersSet->GetStdType() == StdVideoPictureParametersSet::TYPE_H265_PPS)
    {
        h265SessionParametersAddInfo.stdPPSCount = 1;
        h265SessionParametersAddInfo.pStdPPSs    = pStdPictureParametersSet->GetStdH265Pps();
        bool isPps                               = false;
        currentId                                = pStdPictureParametersSet->GetPpsId(isPps);
        DE_ASSERT(isPps);
    }
    else
    {
        DE_ASSERT(!"Incorrect h.265 type");
    }

    return currentId;
}

VkResult VkParserVideoPictureParameters::Create(
    DeviceContext &deviceContext, VkSharedBaseObj<VkParserVideoPictureParameters> &templatePictureParameters,
    VkSharedBaseObj<VkParserVideoPictureParameters> &videoPictureParameters)
{
    VkSharedBaseObj<VkParserVideoPictureParameters> newVideoPictureParameters(
        new VkParserVideoPictureParameters(deviceContext, templatePictureParameters));
    if (!newVideoPictureParameters)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    videoPictureParameters = newVideoPictureParameters;
    return VK_SUCCESS;
}

VkResult VkParserVideoPictureParameters::CreateParametersObject(
    VkSharedBaseObj<VulkanVideoSession> &videoSession,
    const StdVideoPictureParametersSet *pStdVideoPictureParametersSet,
    VkParserVideoPictureParameters *pTemplatePictureParameters)
{
    int32_t currentId = -1;

    VkVideoSessionParametersCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR;

    VkVideoDecodeH264SessionParametersCreateInfoKHR h264SessionParametersCreateInfo{};
    h264SessionParametersCreateInfo.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_CREATE_INFO_KHR;
    VkVideoDecodeH264SessionParametersAddInfoKHR h264SessionParametersAddInfo{};
    h264SessionParametersAddInfo.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_ADD_INFO_KHR;

    VkVideoDecodeH265SessionParametersCreateInfoKHR h265SessionParametersCreateInfo{};
    h265SessionParametersCreateInfo.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_SESSION_PARAMETERS_CREATE_INFO_KHR;
    VkVideoDecodeH265SessionParametersAddInfoKHR h265SessionParametersAddInfo{};
    h265SessionParametersAddInfo.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_SESSION_PARAMETERS_ADD_INFO_KHR;

    VkVideoDecodeAV1SessionParametersCreateInfoKHR av1SessionParametersCreateInfo{};
    av1SessionParametersCreateInfo.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_SESSION_PARAMETERS_CREATE_INFO_KHR;

    createInfo.videoSessionParametersTemplate =
        pTemplatePictureParameters ? VkVideoSessionParametersKHR(*pTemplatePictureParameters) : VK_NULL_HANDLE;

    StdVideoPictureParametersSet::StdType updateType = pStdVideoPictureParametersSet->GetStdType();
    switch (updateType)
    {
    case StdVideoPictureParametersSet::TYPE_H264_SPS:
    case StdVideoPictureParametersSet::TYPE_H264_PPS:
    {
        createInfo.pNext                                   = &h264SessionParametersCreateInfo;
        h264SessionParametersCreateInfo.maxStdSPSCount     = MAX_SPS_IDS;
        h264SessionParametersCreateInfo.maxStdPPSCount     = MAX_PPS_IDS;
        h264SessionParametersCreateInfo.pParametersAddInfo = &h264SessionParametersAddInfo;

        currentId = PopulateH264UpdateFields(pStdVideoPictureParametersSet, h264SessionParametersAddInfo);

        currentStdPictureParameters.h264Sps = h264SessionParametersAddInfo.pStdSPSs;
        currentStdPictureParameters.h264Pps = h264SessionParametersAddInfo.pStdPPSs;
    }
    break;
    case StdVideoPictureParametersSet::TYPE_H265_VPS:
    case StdVideoPictureParametersSet::TYPE_H265_SPS:
    case StdVideoPictureParametersSet::TYPE_H265_PPS:
    {
        createInfo.pNext                                   = &h265SessionParametersCreateInfo;
        h265SessionParametersCreateInfo.maxStdVPSCount     = MAX_VPS_IDS;
        h265SessionParametersCreateInfo.maxStdSPSCount     = MAX_SPS_IDS;
        h265SessionParametersCreateInfo.maxStdPPSCount     = MAX_PPS_IDS;
        h265SessionParametersCreateInfo.pParametersAddInfo = &h265SessionParametersAddInfo;

        currentId = PopulateH265UpdateFields(pStdVideoPictureParametersSet, h265SessionParametersAddInfo);

        currentStdPictureParameters.h265Vps = h265SessionParametersAddInfo.pStdVPSs;
        currentStdPictureParameters.h265Sps = h265SessionParametersAddInfo.pStdSPSs;
        currentStdPictureParameters.h265Pps = h265SessionParametersAddInfo.pStdPPSs;
    }
    break;
    case StdVideoPictureParametersSet::TYPE_AV1_SPS:
    {
        createInfo.pNext = &av1SessionParametersCreateInfo;
        if (pStdVideoPictureParametersSet == nullptr)
        {
            currentId = -1;
        }
        else
        {
            assert(pStdVideoPictureParametersSet->GetStdType() == StdVideoPictureParametersSet::TYPE_AV1_SPS);
            assert(av1SessionParametersCreateInfo.sType ==
                   VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_SESSION_PARAMETERS_CREATE_INFO_KHR);
            av1SessionParametersCreateInfo.pStdSequenceHeader =
                const_cast<StdVideoAV1SequenceHeader *>(pStdVideoPictureParametersSet->GetStdAV1Sps());
            currentId = 0;

            currentStdPictureParameters.av1SequenceHeader = av1SessionParametersCreateInfo.pStdSequenceHeader;
        }
        createInfo.videoSessionParametersTemplate =
            VK_NULL_HANDLE; // TODO: The parameter set code is legacy from the sample app, it could be radically simplified.
    }
    break;
    default:
        DE_ASSERT(!"Invalid parameter set type");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    createInfo.videoSession = videoSession->GetVideoSession();
    VkResult result         = m_deviceContext.getDeviceDriver().createVideoSessionParametersKHR(
        m_deviceContext.device, &createInfo, nullptr, &m_sessionParameters);

    TCU_CHECK_AND_THROW(InternalError, result == VK_SUCCESS, "Could not create video session");
    m_videoSession = videoSession;

    if (pTemplatePictureParameters)
    {
        m_vpsIdsUsed    = pTemplatePictureParameters->m_vpsIdsUsed;
        m_spsIdsUsed    = pTemplatePictureParameters->m_spsIdsUsed;
        m_ppsIdsUsed    = pTemplatePictureParameters->m_ppsIdsUsed;
        m_av1SpsIdsUsed = pTemplatePictureParameters->m_av1SpsIdsUsed; // TODO Review
    }

    assert(currentId >= 0);
    switch (pStdVideoPictureParametersSet->GetParameterType())
    {
    case StdVideoPictureParametersSet::PPS_TYPE:
        m_ppsIdsUsed.set(currentId, true);
        break;

    case StdVideoPictureParametersSet::SPS_TYPE:
        m_spsIdsUsed.set(currentId, true);
        break;

    case StdVideoPictureParametersSet::VPS_TYPE:
        m_vpsIdsUsed.set(currentId, true);
        break;
    case StdVideoPictureParametersSet::AV1_SPS_TYPE:
        m_av1SpsIdsUsed.set(currentId, true);
        break;
    default:
        DE_ASSERT(!"Invalid StdVideoPictureParametersSet Parameter Type!");
    }
    m_Id = ++m_currentId;

    return result;
}

VkResult VkParserVideoPictureParameters::UpdateParametersObject(
    StdVideoPictureParametersSet *pStdVideoPictureParametersSet)
{
    if (pStdVideoPictureParametersSet == nullptr)
    {
        return VK_SUCCESS;
    }

    int32_t currentId = -1;
    VkVideoSessionParametersUpdateInfoKHR updateInfo{};
    updateInfo.sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_UPDATE_INFO_KHR;
    VkVideoDecodeH264SessionParametersAddInfoKHR h264SessionParametersAddInfo{};
    h264SessionParametersAddInfo.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_ADD_INFO_KHR;
    VkVideoDecodeH265SessionParametersAddInfoKHR h265SessionParametersAddInfo{};
    h265SessionParametersAddInfo.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_SESSION_PARAMETERS_ADD_INFO_KHR;

    StdVideoPictureParametersSet::StdType updateType = pStdVideoPictureParametersSet->GetStdType();
    switch (updateType)
    {
    case StdVideoPictureParametersSet::TYPE_H264_SPS:
    case StdVideoPictureParametersSet::TYPE_H264_PPS:
    {
        updateInfo.pNext = &h264SessionParametersAddInfo;
        currentId        = PopulateH264UpdateFields(pStdVideoPictureParametersSet, h264SessionParametersAddInfo);

        if (h264SessionParametersAddInfo.stdSPSCount == 1)
        {
            DE_ASSERT(h264SessionParametersAddInfo.stdSPSCount == 1);
            currentStdPictureParameters.h264Sps = h264SessionParametersAddInfo.pStdSPSs;
        }
        if (h264SessionParametersAddInfo.stdPPSCount == 1)
        {
            DE_ASSERT(h264SessionParametersAddInfo.stdPPSCount == 1);
            currentStdPictureParameters.h264Pps = h264SessionParametersAddInfo.pStdPPSs;
        }
    }
    break;
    case StdVideoPictureParametersSet::TYPE_H265_VPS:
    case StdVideoPictureParametersSet::TYPE_H265_SPS:
    case StdVideoPictureParametersSet::TYPE_H265_PPS:
    {
        updateInfo.pNext = &h265SessionParametersAddInfo;
        currentId        = PopulateH265UpdateFields(pStdVideoPictureParametersSet, h265SessionParametersAddInfo);

        if (h265SessionParametersAddInfo.stdVPSCount > 0)
        {
            DE_ASSERT(h265SessionParametersAddInfo.stdVPSCount == 1);
            currentStdPictureParameters.h265Vps = h265SessionParametersAddInfo.pStdVPSs;
        }
        if (h265SessionParametersAddInfo.stdSPSCount > 0)
        {
            DE_ASSERT(h265SessionParametersAddInfo.stdSPSCount == 1);
            currentStdPictureParameters.h265Sps = h265SessionParametersAddInfo.pStdSPSs;
        }
        if (h265SessionParametersAddInfo.stdPPSCount > 0)
        {
            DE_ASSERT(h265SessionParametersAddInfo.stdPPSCount == 1);
            currentStdPictureParameters.h265Pps = h265SessionParametersAddInfo.pStdPPSs;
        }
    }
    break;
    case StdVideoPictureParametersSet::TYPE_AV1_SPS:
    {
        // Control should not get here. New parameter objects in AV1 imply the creation of a new session..
        // TODO: Properly fix the call chains in the case of AV1, for now just ignore the updates...
        return VK_SUCCESS;
        DE_ASSERT(false && "There should be no calls to UpdateParametersObject for AV1");
        break;
    }
    default:
        DE_ASSERT(!"Invalid Parser format");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    updateInfo.updateSequenceCount = ++m_updateCount;
    VK_CHECK(m_deviceContext.getDeviceDriver().updateVideoSessionParametersKHR(m_deviceContext.device,
                                                                               m_sessionParameters, &updateInfo));

    DE_ASSERT(currentId >= 0);
    switch (pStdVideoPictureParametersSet->GetParameterType())
    {
    case StdVideoPictureParametersSet::PPS_TYPE:
        m_ppsIdsUsed.set(currentId, true);
        break;

    case StdVideoPictureParametersSet::SPS_TYPE:
        m_spsIdsUsed.set(currentId, true);
        break;

    case StdVideoPictureParametersSet::VPS_TYPE:
        m_vpsIdsUsed.set(currentId, true);
        break;
    case StdVideoPictureParametersSet::AV1_SPS_TYPE:
        TCU_FAIL("Parameter set updates are not supported in AV1!");
        break;
    default:
        DE_ASSERT(!"Invalid StdVideoPictureParametersSet Parameter Type!");
    }

    return VK_SUCCESS;
}

VkParserVideoPictureParameters::~VkParserVideoPictureParameters()
{
    if (!!m_sessionParameters)
    {
        m_deviceContext.getDeviceDriver().destroyVideoSessionParametersKHR(m_deviceContext.device, m_sessionParameters,
                                                                           nullptr);
        m_sessionParameters = VK_NULL_HANDLE;
    }
    m_videoSession = nullptr;
}

bool VkParserVideoPictureParameters::UpdatePictureParametersHierarchy(
    VkSharedBaseObj<StdVideoPictureParametersSet> &pictureParametersObject)
{
    int32_t nodeId                                         = -1;
    bool isNodeId                                          = false;
    StdVideoPictureParametersSet::ParameterType nodeParent = StdVideoPictureParametersSet::INVALID_TYPE;
    StdVideoPictureParametersSet::ParameterType nodeChild  = StdVideoPictureParametersSet::INVALID_TYPE;
    switch (pictureParametersObject->GetParameterType())
    {
    case StdVideoPictureParametersSet::PPS_TYPE:
        nodeParent = StdVideoPictureParametersSet::SPS_TYPE;
        nodeId     = pictureParametersObject->GetPpsId(isNodeId);
        if (!((uint32_t)nodeId < VkParserVideoPictureParameters::MAX_PPS_IDS))
        {
            DE_ASSERT(!"PPS ID is out of bounds");
            return false;
        }
        DE_ASSERT(isNodeId);
        if (m_lastPictParamsQueue[nodeParent])
        {
            bool isParentId           = false;
            const int32_t spsParentId = pictureParametersObject->GetSpsId(isParentId);
            DE_ASSERT(!isParentId);
            if (spsParentId == m_lastPictParamsQueue[nodeParent]->GetSpsId(isParentId))
            {
                DE_ASSERT(isParentId);
                pictureParametersObject->m_parent = m_lastPictParamsQueue[nodeParent];
            }
        }
        break;
    case StdVideoPictureParametersSet::SPS_TYPE:
        nodeParent = StdVideoPictureParametersSet::VPS_TYPE;
        nodeChild  = StdVideoPictureParametersSet::PPS_TYPE;
        nodeId     = pictureParametersObject->GetSpsId(isNodeId);
        if (!((uint32_t)nodeId < VkParserVideoPictureParameters::MAX_SPS_IDS))
        {
            DE_ASSERT(!"SPS ID is out of bounds");
            return false;
        }
        DE_ASSERT(isNodeId);
        if (m_lastPictParamsQueue[nodeChild])
        {
            const int32_t spsChildId = m_lastPictParamsQueue[nodeChild]->GetSpsId(isNodeId);
            DE_ASSERT(!isNodeId);
            if (spsChildId == nodeId)
            {
                m_lastPictParamsQueue[nodeChild]->m_parent = pictureParametersObject;
            }
        }
        if (m_lastPictParamsQueue[nodeParent])
        {
            const int32_t vpsParentId = pictureParametersObject->GetVpsId(isNodeId);
            DE_ASSERT(!isNodeId);
            if (vpsParentId == m_lastPictParamsQueue[nodeParent]->GetVpsId(isNodeId))
            {
                pictureParametersObject->m_parent = m_lastPictParamsQueue[nodeParent];
                DE_ASSERT(isNodeId);
            }
        }
        break;
    case StdVideoPictureParametersSet::VPS_TYPE:
        nodeChild = StdVideoPictureParametersSet::SPS_TYPE;
        nodeId    = pictureParametersObject->GetVpsId(isNodeId);
        if (!((uint32_t)nodeId < VkParserVideoPictureParameters::MAX_VPS_IDS))
        {
            DE_ASSERT(!"VPS ID is out of bounds");
            return false;
        }
        DE_ASSERT(isNodeId);
        if (m_lastPictParamsQueue[nodeChild])
        {
            const int32_t vpsParentId = m_lastPictParamsQueue[nodeChild]->GetVpsId(isNodeId);
            DE_ASSERT(!isNodeId);
            if (vpsParentId == nodeId)
            {
                m_lastPictParamsQueue[nodeChild]->m_parent = pictureParametersObject;
            }
        }
        break;
    default:
        DE_ASSERT("!Invalid STD type");
        return false;
    }
    m_lastPictParamsQueue[pictureParametersObject->GetParameterType()] = pictureParametersObject;

    return true;
}

VkResult VkParserVideoPictureParameters::AddPictureParametersToQueue(
    VkSharedBaseObj<StdVideoPictureParametersSet> &pictureParametersSet)
{
    m_pictureParametersQueue.push(pictureParametersSet);
    return VK_SUCCESS;
}

VkResult VkParserVideoPictureParameters::HandleNewPictureParametersSet(
    VkSharedBaseObj<VulkanVideoSession> &videoSession, StdVideoPictureParametersSet *pStdVideoPictureParametersSet)
{
    VkResult result;
    if (m_sessionParameters == VK_NULL_HANDLE)
    {
        DE_ASSERT(videoSession != VK_NULL_HANDLE);
        DE_ASSERT(m_videoSession == VK_NULL_HANDLE);
        if (m_templatePictureParameters)
        {
            m_templatePictureParameters->FlushPictureParametersQueue(videoSession);
        }
        result = CreateParametersObject(videoSession, pStdVideoPictureParametersSet, m_templatePictureParameters);
        DE_ASSERT(result == VK_SUCCESS);
        m_templatePictureParameters = nullptr; // the template object is not needed anymore
        m_videoSession              = videoSession;
    }
    else
    {
        DE_ASSERT(m_videoSession != VK_NULL_HANDLE);
        DE_ASSERT(m_sessionParameters != VK_NULL_HANDLE);
        result = UpdateParametersObject(pStdVideoPictureParametersSet);
        DE_ASSERT(result == VK_SUCCESS);
    }

    return result;
}

int32_t VkParserVideoPictureParameters::FlushPictureParametersQueue(VkSharedBaseObj<VulkanVideoSession> &videoSession)
{
    if (!videoSession)
    {
        return -1;
    }
    uint32_t numQueueItems = 0;
    while (!m_pictureParametersQueue.empty())
    {
        VkSharedBaseObj<StdVideoPictureParametersSet> &stdVideoPictureParametersSet = m_pictureParametersQueue.front();

        VkResult result = HandleNewPictureParametersSet(videoSession, stdVideoPictureParametersSet);
        if (result != VK_SUCCESS)
        {
            return -1;
        }

        m_pictureParametersQueue.pop();
        numQueueItems++;
    }

    return numQueueItems;
}

bool VkParserVideoPictureParameters::CheckStdObjectBeforeUpdate(
    VkSharedBaseObj<StdVideoPictureParametersSet> &stdPictureParametersSet,
    VkSharedBaseObj<VkParserVideoPictureParameters> &currentVideoPictureParameters)
{
    if (!stdPictureParametersSet)
    {
        return false;
    }

    bool stdObjectUpdate = (stdPictureParametersSet->GetUpdateSequenceCount() > 0);

    if (!currentVideoPictureParameters || stdObjectUpdate)
    {

        // Create new Vulkan Picture Parameters object
        return true;
    }
    else
    { // existing VkParserVideoPictureParameters object
        DE_ASSERT(currentVideoPictureParameters);
        // Update with the existing Vulkan Picture Parameters object
    }

    VkSharedBaseObj<VkVideoRefCountBase> clientObject;
    stdPictureParametersSet->GetClientObject(clientObject);
    DE_ASSERT(!clientObject);

    return false;
}

VkResult VkParserVideoPictureParameters::AddPictureParameters(
    DeviceContext &deviceContext, VkSharedBaseObj<VulkanVideoSession> & /*videoSession*/,
    VkSharedBaseObj<StdVideoPictureParametersSet> &stdPictureParametersSet, /* from the parser */
    VkSharedBaseObj<VkParserVideoPictureParameters>
        &currentVideoPictureParameters /* reference to member field of decoder */)
{
    DE_ASSERT(stdPictureParametersSet);

    VkResult result;
    if (CheckStdObjectBeforeUpdate(stdPictureParametersSet, currentVideoPictureParameters))
    {
        result = VkParserVideoPictureParameters::Create(deviceContext, currentVideoPictureParameters,
                                                        currentVideoPictureParameters);
    }

    result = currentVideoPictureParameters->AddPictureParametersToQueue(stdPictureParametersSet);

    return result;
}

int32_t VkParserVideoPictureParameters::AddRef()
{
    return ++m_refCount;
}

int32_t VkParserVideoPictureParameters::Release()
{
    uint32_t ret;
    ret = --m_refCount;
    // Destroy the device if refcount reaches zero
    if (ret == 0)
    {
        delete this;
    }
    return ret;
}

shared_ptr<VideoBaseDecoder> createBasicDecoder(DeviceContext *deviceContext, const VkVideoCoreProfile *profile,
                                                size_t framesToCheck, bool resolutionChange)
{
    VkVideoCapabilitiesKHR videoCapabilities;
    VkVideoDecodeCapabilitiesKHR videoDecodeCapabilities;
    VkResult res =
        util::getVideoDecodeCapabilities(*deviceContext, *profile, videoCapabilities, videoDecodeCapabilities);
    if (res != VK_SUCCESS)
        TCU_THROW(NotSupportedError, "Implementation does not support this video profile");

    bool separateReferenceImages = videoCapabilities.flags & VK_VIDEO_CAPABILITY_SEPARATE_REFERENCE_IMAGES_BIT_KHR;

    VkSharedBaseObj<VulkanVideoFrameBuffer> vkVideoFrameBuffer;

    VK_CHECK(VulkanVideoFrameBuffer::Create(deviceContext,
                                            false, // UseResultStatusQueries
                                            false, // ResourcesWithoutProfiles
                                            vkVideoFrameBuffer));

    VideoBaseDecoder::Parameters params;

    params.profile            = profile;
    params.context            = deviceContext;
    params.framebuffer        = vkVideoFrameBuffer;
    params.framesToCheck      = framesToCheck;
    params.queryDecodeStatus  = false;
    params.outOfOrderDecoding = false;
    params.alwaysRecreateDPB  = resolutionChange;
    params.layeredDpb         = !separateReferenceImages;

    return std::make_shared<VideoBaseDecoder>(std::move(params));
}

de::MovePtr<vkt::ycbcr::MultiPlaneImageData> getDecodedImageFromContext(DeviceContext &deviceContext,
                                                                        VkImageLayout layout, const DecodedFrame *frame)
{
    auto &videoDeviceDriver       = deviceContext.getDeviceDriver();
    auto device                   = deviceContext.device;
    auto queueFamilyIndexDecode   = deviceContext.decodeQueueFamilyIdx();
    auto queueFamilyIndexTransfer = deviceContext.transferQueueFamilyIdx();
    const VkExtent2D imageExtent{(uint32_t)frame->displayWidth, (uint32_t)frame->displayHeight};
    const VkImage image      = frame->outputImageView->GetImageResource()->GetImage();
    const VkFormat format    = frame->outputImageView->GetImageResource()->GetImageCreateInfo().format;
    uint32_t imageLayerIndex = frame->imageLayerIndex;

    MovePtr<vkt::ycbcr::MultiPlaneImageData> multiPlaneImageData(
        new vkt::ycbcr::MultiPlaneImageData(format, tcu::UVec2(imageExtent.width, imageExtent.height)));
    const VkQueue queueDecode   = getDeviceQueue(videoDeviceDriver, device, queueFamilyIndexDecode, 0u);
    const VkQueue queueTransfer = getDeviceQueue(videoDeviceDriver, device, queueFamilyIndexTransfer, 0u);
    const VkImageSubresourceRange imageSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, imageLayerIndex, 1);

    const VkImageMemoryBarrier2KHR imageBarrierDecode =
        makeImageMemoryBarrier2(VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR, VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR,
                                VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR, VK_ACCESS_NONE_KHR, layout,
                                VK_IMAGE_LAYOUT_GENERAL, image, imageSubresourceRange);

    const VkImageMemoryBarrier2KHR imageBarrierOwnershipDecode = makeImageMemoryBarrier2(
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR, VK_ACCESS_NONE_KHR, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,
        VK_ACCESS_NONE_KHR, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, image, imageSubresourceRange,
        queueFamilyIndexDecode, queueFamilyIndexTransfer);

    const VkImageMemoryBarrier2KHR imageBarrierOwnershipTransfer = makeImageMemoryBarrier2(
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR, VK_ACCESS_NONE_KHR, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_NONE_KHR, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, image, imageSubresourceRange,
        queueFamilyIndexDecode, queueFamilyIndexTransfer);

    const VkImageMemoryBarrier2KHR imageBarrierTransfer = makeImageMemoryBarrier2(
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
        VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image,
        imageSubresourceRange);

    const Move<VkCommandPool> cmdDecodePool(makeCommandPool(videoDeviceDriver, device, queueFamilyIndexDecode));
    const Move<VkCommandBuffer> cmdDecodeBuffer(
        allocateCommandBuffer(videoDeviceDriver, device, *cmdDecodePool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const Move<VkCommandPool> cmdTransferPool(makeCommandPool(videoDeviceDriver, device, queueFamilyIndexTransfer));
    const Move<VkCommandBuffer> cmdTransferBuffer(
        allocateCommandBuffer(videoDeviceDriver, device, *cmdTransferPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    Move<VkSemaphore> semaphore                 = createSemaphore(videoDeviceDriver, device);
    Move<VkFence> decodeFence                   = createFence(videoDeviceDriver, device);
    Move<VkFence> transferFence                 = createFence(videoDeviceDriver, device);
    VkFence fences[]                            = {*decodeFence, *transferFence};
    const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    VkSubmitInfo decodeSubmitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, //  VkStructureType sType;
        nullptr,                       //  const void* pNext;
        0u,                            //  uint32_t waitSemaphoreCount;
        nullptr,                       //  const VkSemaphore* pWaitSemaphores;
        nullptr,                       //  const VkPipelineStageFlags* pWaitDstStageMask;
        1u,                            //  uint32_t commandBufferCount;
        &*cmdDecodeBuffer,             //  const VkCommandBuffer* pCommandBuffers;
        1u,                            //  uint32_t signalSemaphoreCount;
        &*semaphore,                   //  const VkSemaphore* pSignalSemaphores;
    };
    if (frame->frameCompleteSemaphore != VK_NULL_HANDLE)
    {
        decodeSubmitInfo.waitSemaphoreCount = 1;
        decodeSubmitInfo.pWaitSemaphores    = &frame->frameCompleteSemaphore;
        decodeSubmitInfo.pWaitDstStageMask  = &waitDstStageMask;
    }
    const VkSubmitInfo transferSubmitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, //  VkStructureType sType;
        nullptr,                       //  const void* pNext;
        1u,                            //  uint32_t waitSemaphoreCount;
        &*semaphore,                   //  const VkSemaphore* pWaitSemaphores;
        &waitDstStageMask,             //  const VkPipelineStageFlags* pWaitDstStageMask;
        1u,                            //  uint32_t commandBufferCount;
        &*cmdTransferBuffer,           //  const VkCommandBuffer* pCommandBuffers;
        0u,                            //  uint32_t signalSemaphoreCount;
        nullptr,                       //  const VkSemaphore* pSignalSemaphores;
    };

    beginCommandBuffer(videoDeviceDriver, *cmdDecodeBuffer, 0u);
    cmdPipelineImageMemoryBarrier2(videoDeviceDriver, *cmdDecodeBuffer, &imageBarrierDecode);
    cmdPipelineImageMemoryBarrier2(videoDeviceDriver, *cmdDecodeBuffer, &imageBarrierOwnershipDecode);
    endCommandBuffer(videoDeviceDriver, *cmdDecodeBuffer);

    beginCommandBuffer(videoDeviceDriver, *cmdTransferBuffer, 0u);
    cmdPipelineImageMemoryBarrier2(videoDeviceDriver, *cmdTransferBuffer, &imageBarrierOwnershipTransfer);
    cmdPipelineImageMemoryBarrier2(videoDeviceDriver, *cmdTransferBuffer, &imageBarrierTransfer);
    endCommandBuffer(videoDeviceDriver, *cmdTransferBuffer);

    VK_CHECK(videoDeviceDriver.queueSubmit(queueDecode, 1u, &decodeSubmitInfo, *decodeFence));
    VK_CHECK(videoDeviceDriver.queueSubmit(queueTransfer, 1u, &transferSubmitInfo, *transferFence));

    VK_CHECK(videoDeviceDriver.waitForFences(device, DE_LENGTH_OF_ARRAY(fences), fences, true, ~0ull));

    vkt::ycbcr::downloadImage(videoDeviceDriver, device, queueFamilyIndexTransfer, deviceContext.allocator(), image,
                              multiPlaneImageData.get(), 0, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, imageLayerIndex);

    const VkImageMemoryBarrier2KHR imageBarrierTransfer2 =
        makeImageMemoryBarrier2(VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR, VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,
                                VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR, VK_ACCESS_NONE_KHR,
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, layout, image, imageSubresourceRange);

    videoDeviceDriver.resetCommandBuffer(*cmdTransferBuffer, 0u);
    videoDeviceDriver.resetFences(device, 1, &*transferFence);
    beginCommandBuffer(videoDeviceDriver, *cmdTransferBuffer, 0u);
    cmdPipelineImageMemoryBarrier2(videoDeviceDriver, *cmdTransferBuffer, &imageBarrierTransfer2);
    endCommandBuffer(videoDeviceDriver, *cmdTransferBuffer);

    const VkSubmitInfo transferSubmitInfo2 = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType sType;
        nullptr,                       //  const void* pNext;
        0u,                            //  uint32_t waitSemaphoreCount;
        nullptr,                       //  const VkSemaphore* pWaitSemaphores;
        nullptr,                       //  const VkPipelineStageFlags* pWaitDstStageMask;
        1u,                            //  uint32_t commandBufferCount;
        &*cmdTransferBuffer,           //  const VkCommandBuffer* pCommandBuffers;
        0u,                            //  uint32_t signalSemaphoreCount;
        nullptr,                       // const VkSemaphore* pSignalSemaphores;
    };

    VK_CHECK(videoDeviceDriver.queueSubmit(queueTransfer, 1u, &transferSubmitInfo2, *transferFence));
    VK_CHECK(videoDeviceDriver.waitForFences(device, 1, &*transferFence, true, ~0ull));

    return multiPlaneImageData;
}

} // namespace video
} // namespace vkt
