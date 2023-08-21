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
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Video Decoding Base Classe Functionality
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

#include "tcuPlatform.hpp"
#include "vkDefs.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkStrUtil.hpp"
#include "deRandom.hpp"

#include <iostream>
#include <algorithm>
#include <numeric>
#include <random>

namespace vkt
{
namespace video
{
using namespace vk;
using namespace std;
using de::MovePtr;
using de::SharedPtr;

static const deUint32 topFieldShift		   = 0;
static const deUint32 topFieldMask		   = (1 << topFieldShift);
static const deUint32 bottomFieldShift	   = 1;
static const deUint32 bottomFieldMask	   = (1 << bottomFieldShift);
static const deUint32 fieldIsReferenceMask = (topFieldMask | bottomFieldMask);

#define HEVC_MAX_DPB_SLOTS 16
#define AVC_MAX_DPB_SLOTS 17

inline vkPicBuffBase *GetPic(VkPicIf *pPicBuf)
{
	return (vkPicBuffBase *) pPicBuf;
}

inline VkVideoChromaSubsamplingFlagBitsKHR ConvertStdH264ChromaFormatToVulkan(StdVideoH264ChromaFormatIdc stdFormat)
{
	switch (stdFormat)
	{
		case STD_VIDEO_H264_CHROMA_FORMAT_IDC_420:
			return VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
		case STD_VIDEO_H264_CHROMA_FORMAT_IDC_422:
			return VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR;
		case STD_VIDEO_H264_CHROMA_FORMAT_IDC_444:
			return VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR;
		default:
			TCU_THROW(InternalError, "Invalid chroma sub-sampling format");
	}
}

VkFormat codecGetVkFormat(VkVideoChromaSubsamplingFlagBitsKHR chromaFormatIdc,
						  int bitDepthLuma,
						  bool isSemiPlanar)
{
	switch (chromaFormatIdc)
	{
		case VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR:
		{
			switch (bitDepthLuma)
			{
				case VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR:
					return VK_FORMAT_R8_UNORM;
				case VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR:
					return VK_FORMAT_R10X6_UNORM_PACK16;
				case VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR:
					return VK_FORMAT_R12X4_UNORM_PACK16;
				default:
					TCU_THROW(InternalError, "Cannot map monochrome format to VkFormat");
			}
		}
		case VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR:
		{
			switch (bitDepthLuma)
			{
				case VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR:
					return (isSemiPlanar ? VK_FORMAT_G8_B8R8_2PLANE_420_UNORM : VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM);
				case VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR:
					return (isSemiPlanar ? VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 : VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16);
				case VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR:
					return (isSemiPlanar ? VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16 : VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16);
				default:
					TCU_THROW(InternalError, "Cannot map 420 format to VkFormat");
			}
		}
		case VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR:
		{
			switch (bitDepthLuma)
			{
				case VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR:
					return (isSemiPlanar ? VK_FORMAT_G8_B8R8_2PLANE_422_UNORM : VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM);
				case VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR:
					return (isSemiPlanar ? VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16 : VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16);
				case VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR:
					return (isSemiPlanar ? VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16 : VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16);
				default:
					TCU_THROW(InternalError, "Cannot map 422 format to VkFormat");
			}
		}
		case VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR:
		{
			switch (bitDepthLuma)
			{
				case VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR:
					return (isSemiPlanar ? VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT : VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM);
				case VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR:
					return (isSemiPlanar ? VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT : VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16);
				case VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR:
					return (isSemiPlanar ? VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT : VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16);
				default:
					TCU_THROW(InternalError, "Cannot map 444 format to VkFormat");
			}
		}
		default:
			TCU_THROW(InternalError, "Unknown input idc format");
	}
}

VkVideoComponentBitDepthFlagsKHR getLumaBitDepth(deUint8 lumaBitDepthMinus8)
{
	switch (lumaBitDepthMinus8)
	{
		case 0:
			return VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
		case 2:
			return VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR;
		case 4:
			return VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR;
		default:
			TCU_THROW(InternalError, "Unhandler lumaBitDepthMinus8");
	}
}

VkVideoComponentBitDepthFlagsKHR getChromaBitDepth(deUint8 chromaBitDepthMinus8)
{
	switch (chromaBitDepthMinus8)
	{
		case 0:
			return VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
		case 2:
			return VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR;
		case 4:
			return VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR;
		default:
			TCU_THROW(InternalError, "Unhandler chromaBitDepthMinus8");
	}
}

void setImageLayout(const DeviceInterface &vkd,
					VkCommandBuffer cmdBuffer,
					VkImage image,
					VkImageLayout oldImageLayout,
					VkImageLayout newImageLayout,
					VkPipelineStageFlags2KHR srcStages,
					VkPipelineStageFlags2KHR dstStages,
					VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT)
{
	VkAccessFlags2KHR srcAccessMask = 0;
	VkAccessFlags2KHR dstAccessMask = 0;

	switch (static_cast<VkImageLayout>(oldImageLayout))
	{
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_PREINITIALIZED:
			srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR:
			srcAccessMask = VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR;
			break;
		default:
			srcAccessMask = 0;
			break;
	}

	switch (static_cast<VkImageLayout>(newImageLayout))
	{
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR:
			dstAccessMask = VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR;
			break;
		case VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR:
			dstAccessMask = VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR;
			break;
		case VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR:
			dstAccessMask = VK_ACCESS_2_VIDEO_ENCODE_READ_BIT_KHR;
			break;
		case VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR:
			dstAccessMask = VK_ACCESS_2_VIDEO_ENCODE_WRITE_BIT_KHR | VK_ACCESS_2_VIDEO_ENCODE_READ_BIT_KHR;
			break;
		case VK_IMAGE_LAYOUT_GENERAL:
			dstAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			break;
		default:
			dstAccessMask = 0;
			break;
	}

	const VkImageMemoryBarrier2KHR imageMemoryBarrier =
			{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,    //  VkStructureType				sType;
					DE_NULL,                                        //  const void*					pNext;
					srcStages,                                        //  VkPipelineStageFlags2KHR	srcStageMask;
					srcAccessMask,                                    //  VkAccessFlags2KHR			srcAccessMask;
					dstStages,                                        //  VkPipelineStageFlags2KHR	dstStageMask;
					dstAccessMask,                                    //  VkAccessFlags2KHR			dstAccessMask;
					oldImageLayout,                                    //  VkImageLayout				oldLayout;
					newImageLayout,                                    //  VkImageLayout				newLayout;
					VK_QUEUE_FAMILY_IGNORED,                        //  deUint32					srcQueueFamilyIndex;
					VK_QUEUE_FAMILY_IGNORED,                        //  deUint32					dstQueueFamilyIndex;
					image,                                            //  VkImage						image;
					{aspectMask, 0, 1, 0, 1},                        //  VkImageSubresourceRange		subresourceRange;
			};

	const VkDependencyInfoKHR dependencyInfo =
			{
					VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,    //  VkStructureType						sType;
					DE_NULL,                                //  const void*							pNext;
					VK_DEPENDENCY_BY_REGION_BIT,            //  VkDependencyFlags					dependencyFlags;
					0,                                        //  deUint32							memoryBarrierCount;
					DE_NULL,                                //  const VkMemoryBarrier2KHR*			pMemoryBarriers;
					0,                                        //  deUint32							bufferMemoryBarrierCount;
					DE_NULL,                                //  const VkBufferMemoryBarrier2KHR*	pBufferMemoryBarriers;
					1,                                        //  deUint32							imageMemoryBarrierCount;
					&imageMemoryBarrier,                    //  const VkImageMemoryBarrier2KHR*		pImageMemoryBarriers;
			};

	vkd.cmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
}

typedef struct dpbH264Entry {
	int8_t dpbSlot;
	// bit0(used_for_reference)=1: top field used for reference,
	// bit1(used_for_reference)=1: bottom field used for reference
	deUint32 used_for_reference : 2;
	deUint32 is_long_term : 1; // 0 = short-term, 1 = long-term
	deUint32 is_non_existing : 1; // 1 = marked as non-existing
	deUint32 is_field_ref : 1; // set if unpaired field or complementary field pair
	union {
		int16_t FieldOrderCnt[2]; // h.264 : 2*32 [top/bottom].
		int32_t PicOrderCnt; // HEVC PicOrderCnt
	};
	union {
		int16_t FrameIdx; // : 16   short-term: FrameNum (16 bits), long-term:
		// LongTermFrameIdx (4 bits)
		int8_t originalDpbIndex; // Original Dpb source Index.
	};
	vkPicBuffBase* m_picBuff; // internal picture reference

	void setReferenceAndTopBottomField(
			bool isReference, bool nonExisting, bool isLongTerm, bool isFieldRef,
			bool topFieldIsReference, bool bottomFieldIsReference, int16_t frameIdx,
			const int16_t fieldOrderCntList[2], vkPicBuffBase* picBuff)
	{
		is_non_existing = nonExisting;
		is_long_term = isLongTerm;
		is_field_ref = isFieldRef;
		if (isReference && isFieldRef) {
			used_for_reference = (bottomFieldIsReference << bottomFieldShift) | (topFieldIsReference << topFieldShift);
		} else {
			used_for_reference = isReference ? 3 : 0;
		}

		FrameIdx = frameIdx;

		FieldOrderCnt[0] = fieldOrderCntList[used_for_reference == 2]; // 0: for progressive and top reference; 1: for
		// bottom reference only.
		FieldOrderCnt[1] = fieldOrderCntList[used_for_reference != 1]; // 0: for top reference only;  1: for bottom
		// reference and progressive.

		dpbSlot = -1;
		m_picBuff = picBuff;
	}

	void setReference(bool isLongTerm, int32_t picOrderCnt,
					  vkPicBuffBase* picBuff)
	{
		is_non_existing = (picBuff == NULL);
		is_long_term = isLongTerm;
		is_field_ref = false;
		used_for_reference = (picBuff != NULL) ? 3 : 0;

		PicOrderCnt = picOrderCnt;

		dpbSlot = -1;
		m_picBuff = picBuff;
		originalDpbIndex = -1;
	}

	bool isRef() { return (used_for_reference != 0); }

	StdVideoDecodeH264ReferenceInfoFlags getPictureFlag(bool currentPictureIsProgressive)
	{
		StdVideoDecodeH264ReferenceInfoFlags picFlags = StdVideoDecodeH264ReferenceInfoFlags();
		if (videoLoggingEnabled())
			std::cout << "\t\t Flags: ";

		if (used_for_reference) {
			if (videoLoggingEnabled())
				std::cout << "FRAME_IS_REFERENCE ";
			// picFlags.is_reference = true;
		}

		if (is_long_term) {
			if (videoLoggingEnabled())
				std::cout << "IS_LONG_TERM ";
			picFlags.used_for_long_term_reference = true;
		}
		if (is_non_existing) {
			if (videoLoggingEnabled())
				std::cout << "IS_NON_EXISTING ";
			picFlags.is_non_existing = true;
		}

		if (is_field_ref) {
			if (videoLoggingEnabled())
				std::cout << "IS_FIELD ";
			// picFlags.field_pic_flag = true;
		}

		if (!currentPictureIsProgressive && (used_for_reference & topFieldMask)) {
			if (videoLoggingEnabled())
				std::cout << "TOP_FIELD_IS_REF ";
			picFlags.top_field_flag = true;
		}
		if (!currentPictureIsProgressive && (used_for_reference & bottomFieldMask)) {
			if (videoLoggingEnabled())
				std::cout << "BOTTOM_FIELD_IS_REF ";
			picFlags.bottom_field_flag = true;
		}

		return picFlags;
	}

	void setH264PictureData(nvVideoDecodeH264DpbSlotInfo* pDpbRefList,
							VkVideoReferenceSlotInfoKHR* pReferenceSlots,
							deUint32 dpbEntryIdx, deUint32 dpbSlotIndex,
							bool currentPictureIsProgressive)
	{
		DE_ASSERT(dpbEntryIdx < AVC_MAX_DPB_SLOTS);
		DE_ASSERT(dpbSlotIndex < AVC_MAX_DPB_SLOTS);

		DE_ASSERT((dpbSlotIndex == (deUint32)dpbSlot) || is_non_existing);
		pReferenceSlots[dpbEntryIdx].sType = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
		pReferenceSlots[dpbEntryIdx].slotIndex = dpbSlotIndex;
		pReferenceSlots[dpbEntryIdx].pNext = pDpbRefList[dpbEntryIdx].Init(dpbSlotIndex);

		StdVideoDecodeH264ReferenceInfo* pRefPicInfo = &pDpbRefList[dpbEntryIdx].stdReferenceInfo;
		pRefPicInfo->FrameNum = FrameIdx;
		if (videoLoggingEnabled()) {
			std::cout << "\tdpbEntryIdx: " << dpbEntryIdx
					  << "dpbSlotIndex: " << dpbSlotIndex
					  << " FrameIdx: " << (int32_t)FrameIdx;
		}
		pRefPicInfo->flags = getPictureFlag(currentPictureIsProgressive);
		pRefPicInfo->PicOrderCnt[0] = FieldOrderCnt[0];
		pRefPicInfo->PicOrderCnt[1] = FieldOrderCnt[1];
		if (videoLoggingEnabled())
			std::cout << " fieldOrderCnt[0]: " << pRefPicInfo->PicOrderCnt[0]
					  << " fieldOrderCnt[1]: " << pRefPicInfo->PicOrderCnt[1]
					  << std::endl;
	}

	void setH265PictureData(nvVideoDecodeH265DpbSlotInfo* pDpbSlotInfo,
							VkVideoReferenceSlotInfoKHR* pReferenceSlots,
							deUint32 dpbEntryIdx, deUint32 dpbSlotIndex)
	{
		DE_ASSERT(dpbEntryIdx < HEVC_MAX_DPB_SLOTS);
		DE_ASSERT(dpbSlotIndex < HEVC_MAX_DPB_SLOTS);
		DE_ASSERT(isRef());

		DE_ASSERT((dpbSlotIndex == (deUint32)dpbSlot) || is_non_existing);
		pReferenceSlots[dpbEntryIdx].sType = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
		pReferenceSlots[dpbEntryIdx].slotIndex = dpbSlotIndex;
		pReferenceSlots[dpbEntryIdx].pNext = pDpbSlotInfo[dpbEntryIdx].Init(dpbSlotIndex);

		StdVideoDecodeH265ReferenceInfo* pRefPicInfo = &pDpbSlotInfo[dpbEntryIdx].stdReferenceInfo;
		pRefPicInfo->PicOrderCntVal = PicOrderCnt;
		pRefPicInfo->flags.used_for_long_term_reference = is_long_term;

		if (videoLoggingEnabled()) {
			std::cout << "\tdpbIndex: " << dpbSlotIndex
					  << " picOrderCntValList: " << PicOrderCnt;

			std::cout << "\t\t Flags: ";
			std::cout << "FRAME IS REFERENCE ";
			if (pRefPicInfo->flags.used_for_long_term_reference) {
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

		if ((picIndex >= 0) && ((deUint32) picIndex < m_maxNumDecodeSurfaces))
		{
			return (int8_t) picIndex;
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
	DE_ASSERT((picIndex >= 0) && ((deUint32) picIndex < m_maxNumDecodeSurfaces));

	return !!(m_fieldPicFlagMask & (1 << (deUint32) picIndex));
}

bool VideoBaseDecoder::SetFieldPicFlag(int8_t picIndex, bool fieldPicFlag)
{
	DE_ASSERT((picIndex >= 0) && ((deUint32) picIndex < m_maxNumDecodeSurfaces));

	bool oldFieldPicFlag = GetFieldPicFlag(picIndex);

	if (fieldPicFlag)
	{
		m_fieldPicFlagMask |= (1 << (deUint32) picIndex);
	}
	else
	{
		m_fieldPicFlagMask &= ~(1 << (deUint32) picIndex);
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

deUint32 VideoBaseDecoder::ResetPicDpbSlots(deUint32 picIndexSlotValidMask)
{
	deUint32 resetSlotsMask = ~(picIndexSlotValidMask | ~m_dpbSlotsMask);

	for (deUint32 picIdx = 0; (picIdx < m_maxNumDecodeSurfaces) && resetSlotsMask; picIdx++)
	{
		if (resetSlotsMask & (1 << picIdx))
		{
			resetSlotsMask &= ~(1 << picIdx);

			SetPicDpbSlot((int8_t) picIdx, -1);
		}
	}

	return m_dpbSlotsMask;
}

VideoBaseDecoder::VideoBaseDecoder(Parameters&& params)
	: m_deviceContext(params.context)
	, m_profile(*params.profile)
	, m_framesToCheck(params.framesToCheck)
	, m_dpb(3)
	, m_videoFrameBuffer(params.framebuffer)
	// TODO: interface cleanup
	, m_decodeFramesData(params.context->getDeviceDriver(), params.context->device, params.context->decodeQueueFamilyIdx())
	, m_resetPictureParametersFrameTriggerHack(params.pictureParameterUpdateTriggerHack)
	, m_queryResultWithStatus(params.queryDecodeStatus)
	, m_outOfOrderDecoding(params.outOfOrderDecoding)
	, m_alwaysRecreateDPB(params.alwaysRecreateDPB)
{
	std::fill(m_pictureToDpbSlotMap.begin(), m_pictureToDpbSlotMap.end(), -1);

	VK_CHECK(util::getVideoDecodeCapabilities(*m_deviceContext, *params.profile, m_videoCaps, m_decodeCaps));

	VK_CHECK(util::getSupportedVideoFormats(*m_deviceContext, m_profile, m_decodeCaps.flags,
											 m_outImageFormat,
											 m_dpbImageFormat));

	m_supportedVideoCodecs = util::getSupportedCodecs(*m_deviceContext,
													  m_deviceContext->decodeQueueFamilyIdx(),
													 VK_QUEUE_VIDEO_DECODE_BIT_KHR,
													  VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR | VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR);
	DE_ASSERT(m_supportedVideoCodecs != VK_VIDEO_CODEC_OPERATION_NONE_KHR);
}

void VideoBaseDecoder::Deinitialize()
{
	const DeviceInterface& vkd			 = m_deviceContext->getDeviceDriver();
	VkDevice			   device		 = m_deviceContext->device;
	VkQueue				   queueDecode	 = m_deviceContext->decodeQueue;
	VkQueue				   queueTransfer = m_deviceContext->transferQueue;

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

int32_t VideoBaseDecoder::StartVideoSequence (const VkParserDetectedVideoFormat* pVideoFormat)
{
	VkExtent2D codedExtent = { pVideoFormat->coded_width, pVideoFormat->coded_height };

	// Width and height of the image surface
	VkExtent2D imageExtent = VkExtent2D { std::max((deUint32)(pVideoFormat->display_area.right  - pVideoFormat->display_area.left), pVideoFormat->coded_width),
										  std::max((deUint32)(pVideoFormat->display_area.bottom - pVideoFormat->display_area.top),  pVideoFormat->coded_height) };

	// REVIEW: There is some inflexibility in the parser regarding this parameter. For the Jellyfish content, it continues wanting to allocate buffers
	// well past what is advertises here. The tangential problem with that content is that the second GOP doesn't start with an IDR frame like all the
	// other test content. Should look more ino this problem, but for now cheese it by always allocating the total number of frames we might need
	// to allocate, even if many of them could be recycled if the parser output pictures earlier (which would be legal but isn't happening for some
	// reason)
	m_numDecodeSurfaces = std::max(4u, m_framesToCheck); // N.B. pVideoFormat->minNumDecodeSurfaces is NOT advertised correctly!
	VkResult result = VK_SUCCESS;

	if (videoLoggingEnabled()) {
		std::cout << "\t" << std::hex << m_supportedVideoCodecs << " HW codec types are available: " << std::dec << std::endl;
	}

	VkVideoCodecOperationFlagBitsKHR detectedVideoCodec = pVideoFormat->codec;

	VkVideoCoreProfile videoProfile(detectedVideoCodec, pVideoFormat->chromaSubsampling, pVideoFormat->lumaBitDepth, pVideoFormat->chromaBitDepth,
									pVideoFormat->codecProfile);
	DE_ASSERT(videoProfile == m_profile);

	// Check the detected profile is the same as the specified test profile.
	DE_ASSERT(m_profile == videoProfile);

	DE_ASSERT(((detectedVideoCodec & m_supportedVideoCodecs) != 0) && (detectedVideoCodec == m_profile.GetCodecType()));

	if (m_videoFormat.coded_width && m_videoFormat.coded_height) {
		// CreateDecoder() has been called before, and now there's possible config change
		m_deviceContext->waitDecodeQueue();
		m_deviceContext->deviceWaitIdle();
	}

	deUint32 maxDpbSlotCount = pVideoFormat->maxNumDpbSlots;

	if (videoLoggingEnabled())
	{
		// TODO: Tidy up all the logging stuff copied from NVIDIA...
		std::cout << std::dec << "Video Input Information" << std::endl
				  << "\tCodec        : " << util::getVideoCodecString(pVideoFormat->codec) << std::endl
				  << "\tFrame rate   : " << pVideoFormat->frame_rate.numerator << "/" << pVideoFormat->frame_rate.denominator << " = " << ((pVideoFormat->frame_rate.denominator != 0) ? (1.0 * pVideoFormat->frame_rate.numerator / pVideoFormat->frame_rate.denominator) : 0.0) << " fps" << std::endl
				  << "\tSequence     : " << (pVideoFormat->progressive_sequence ? "Progressive" : "Interlaced") << std::endl
				  << "\tCoded size   : [" << codedExtent.width << ", " << codedExtent.height << "]" << std::endl
				  << "\tDisplay area : [" << pVideoFormat->display_area.left << ", " << pVideoFormat->display_area.top << ", " << pVideoFormat->display_area.right << ", " << pVideoFormat->display_area.bottom << "]" << std::endl
				  << "\tChroma       : " << util::getVideoChromaFormatString(pVideoFormat->chromaSubsampling) << std::endl
				  << "\tBit depth    : " << pVideoFormat->bit_depth_luma_minus8 + 8 << std::endl
				  << "\tCodec        : " << VkVideoCoreProfile::CodecToName(detectedVideoCodec) << std::endl
				  << "\t#Decode surf : " << m_numDecodeSurfaces << std::endl
				  << "\tCoded extent : " << codedExtent.width << " x " << codedExtent.height << std::endl
				  << "\tMax DPB slots : " << maxDpbSlotCount << std::endl;
	}

	DE_ASSERT(VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR == pVideoFormat->chromaSubsampling ||
		   VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR == pVideoFormat->chromaSubsampling ||
		   VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR == pVideoFormat->chromaSubsampling ||
		   VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR == pVideoFormat->chromaSubsampling);
	DE_ASSERT(pVideoFormat->chromaSubsampling == m_profile.GetColorSubsampling());

	imageExtent.width  = std::max(imageExtent.width, m_videoCaps.minCodedExtent.width);
	imageExtent.height = std::max(imageExtent.height, m_videoCaps.minCodedExtent.height);

	imageExtent.width = deAlign32(imageExtent.width,  m_videoCaps.pictureAccessGranularity.width);
	imageExtent.height = deAlign32(imageExtent.height,  m_videoCaps.pictureAccessGranularity.height);

	if (!m_videoSession ||
		!m_videoSession->IsCompatible(m_deviceContext->device,
									  m_deviceContext->decodeQueueFamilyIdx(),
									  &videoProfile,
									  m_outImageFormat,
									  imageExtent,
									  m_dpbImageFormat,
									  maxDpbSlotCount,
									  maxDpbSlotCount) ||
		m_alwaysRecreateDPB)
	{

		VK_CHECK(VulkanVideoSession::Create(*m_deviceContext,
											m_deviceContext->decodeQueueFamilyIdx(),
											&videoProfile,
											m_outImageFormat,
											imageExtent,
											m_dpbImageFormat,
											maxDpbSlotCount,
											std::min<deUint32>(maxDpbSlotCount, m_videoCaps.maxActiveReferencePictures),
											m_videoSession));

		// after creating a new video session, we need codec reset.
		m_resetDecoder = true;
		DE_ASSERT(result == VK_SUCCESS);
	}

	if (m_currentPictureParameters)
	{
		m_currentPictureParameters->FlushPictureParametersQueue(m_videoSession);
	}

	VkImageUsageFlags outImageUsage = (VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR |
									   VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
									   VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	VkImageUsageFlags dpbImageUsage = VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR;

	if (dpbAndOutputCoincide()) {
		dpbImageUsage = VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR | VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	} else {
		// The implementation does not support dpbAndOutputCoincide
		m_useSeparateOutputImages = true;
	}

	if(!(m_videoCaps.flags & VK_VIDEO_CAPABILITY_SEPARATE_REFERENCE_IMAGES_BIT_KHR)) {
		// The implementation does not support individual images for DPB and so must use arrays
		m_useImageArray = true;
		m_useImageViewArray = true;
	}

	bool useLinearOutput = false;
	int32_t ret = m_videoFrameBuffer->InitImagePool(videoProfile.GetProfile(),
													m_numDecodeSurfaces,
													m_dpbImageFormat,
													m_outImageFormat,
													codedExtent,
													imageExtent,
													dpbImageUsage,
													outImageUsage,
													m_deviceContext->decodeQueueFamilyIdx(),
													m_useImageArray, m_useImageViewArray,
													m_useSeparateOutputImages, useLinearOutput);

	DE_ASSERT((deUint32)ret >= m_numDecodeSurfaces);
	if ((deUint32)ret != m_numDecodeSurfaces) {
		fprintf(stderr, "\nERROR: InitImagePool() ret(%d) != m_numDecodeSurfaces(%d)\n", ret, m_numDecodeSurfaces);
	}

	if (videoLoggingEnabled()) {
		std::cout << "Allocating Video Device Memory" << std::endl
				  << "Allocating " << m_numDecodeSurfaces << " Num Decode Surfaces and "
				  << maxDpbSlotCount << " Video Device Memory Images for DPB " << std::endl
				  << imageExtent.width << " x " << imageExtent.height << std::endl;
	}

	// There will be no more than 32 frames in the queue.
	m_decodeFramesData.resize(m_numDecodeSurfaces);

	int32_t availableBuffers = (int32_t)m_decodeFramesData.GetBitstreamBuffersQueue().GetAvailableNodesNumber();
	if (availableBuffers < m_numBitstreamBuffersToPreallocate) {
		deUint32 allocateNumBuffers = std::min<deUint32>(
				m_decodeFramesData.GetBitstreamBuffersQueue().GetMaxNodes(),
				(m_numBitstreamBuffersToPreallocate - availableBuffers));

		allocateNumBuffers = std::min<deUint32>(allocateNumBuffers,
												m_decodeFramesData.GetBitstreamBuffersQueue().GetFreeNodesNumber());

		for (deUint32 i = 0; i < 1; i++) {

			VkSharedBaseObj<BitstreamBufferImpl> bitstreamBuffer;
			VkDeviceSize allocSize = 2 * 1024 * 1024;

			result = BitstreamBufferImpl::Create(m_deviceContext,
													   m_deviceContext->decodeQueueFamilyIdx(),
													   allocSize,
													   m_videoCaps.minBitstreamBufferOffsetAlignment,
													   m_videoCaps.minBitstreamBufferSizeAlignment,
													   bitstreamBuffer,
													   m_profile.GetProfileListInfo());
			DE_ASSERT(result == VK_SUCCESS);
			if (result != VK_SUCCESS) {
				fprintf(stderr, "\nERROR: CreateVideoBitstreamBuffer() result: 0x%x\n", result);
				break;
			}

			int32_t nodeAddedWithIndex = m_decodeFramesData.GetBitstreamBuffersQueue().
					AddNodeToPool(bitstreamBuffer, false);
			if (nodeAddedWithIndex < 0) {
				DE_ASSERT("Could not add the new node to the pool");
				break;
			}
		}
	}

	// Save the original config
	m_videoFormat = *pVideoFormat;
	return m_numDecodeSurfaces;
}

int32_t VideoBaseDecoder::BeginSequence (const VkParserSequenceInfo* pnvsi)
{
	bool sequenceUpdate = m_nvsi.nMaxWidth != 0 && m_nvsi.nMaxHeight != 0;

	const deUint32 maxDpbSlots =  (pnvsi->eCodec == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR) ? VkParserPerFrameDecodeParameters::MAX_DPB_REF_AND_SETUP_SLOTS : VkParserPerFrameDecodeParameters::MAX_DPB_REF_SLOTS;
	deUint32 configDpbSlots = (pnvsi->nMinNumDpbSlots > 0) ? pnvsi->nMinNumDpbSlots : maxDpbSlots;
	configDpbSlots = std::min<deUint32>(configDpbSlots, maxDpbSlots);

	bool sequenceReconfigureFormat = false;
	bool sequenceReconfigureCodedExtent = false;
	if (sequenceUpdate) {
		if ((pnvsi->eCodec != m_nvsi.eCodec) ||
			(pnvsi->nChromaFormat != m_nvsi.nChromaFormat) || (pnvsi->uBitDepthLumaMinus8 != m_nvsi.uBitDepthLumaMinus8) ||
			(pnvsi->uBitDepthChromaMinus8 != m_nvsi.uBitDepthChromaMinus8) ||
			(pnvsi->bProgSeq != m_nvsi.bProgSeq)) {
			sequenceReconfigureFormat = true;
		}

		if ((pnvsi->nCodedWidth != m_nvsi.nCodedWidth) || (pnvsi->nCodedHeight != m_nvsi.nCodedHeight)) {
			sequenceReconfigureCodedExtent = true;
		}

	}

	m_nvsi = *pnvsi;
	m_nvsi.nMaxWidth = pnvsi->nCodedWidth;
	m_nvsi.nMaxHeight = pnvsi->nCodedHeight;

	m_maxNumDecodeSurfaces = pnvsi->nMinNumDecodeSurfaces;

		VkParserDetectedVideoFormat detectedFormat;
		deUint8 raw_seqhdr_data[1024]; /* Output the sequence header data, currently
                                      not used */

		memset(&detectedFormat, 0, sizeof(detectedFormat));

		detectedFormat.sequenceUpdate = sequenceUpdate;
		detectedFormat.sequenceReconfigureFormat = sequenceReconfigureFormat;
		detectedFormat.sequenceReconfigureCodedExtent = sequenceReconfigureCodedExtent;

		detectedFormat.codec = pnvsi->eCodec;
		detectedFormat.frame_rate.numerator = NV_FRAME_RATE_NUM(pnvsi->frameRate);
		detectedFormat.frame_rate.denominator = NV_FRAME_RATE_DEN(pnvsi->frameRate);
		detectedFormat.progressive_sequence = pnvsi->bProgSeq;
		detectedFormat.coded_width = pnvsi->nCodedWidth;
		detectedFormat.coded_height = pnvsi->nCodedHeight;
		detectedFormat.display_area.right = pnvsi->nDisplayWidth;
		detectedFormat.display_area.bottom = pnvsi->nDisplayHeight;

		if ((StdChromaFormatIdc)pnvsi->nChromaFormat == chroma_format_idc_420) {
			detectedFormat.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
		} else if ((StdChromaFormatIdc)pnvsi->nChromaFormat == chroma_format_idc_422) {
			detectedFormat.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR;
		} else if ((StdChromaFormatIdc)pnvsi->nChromaFormat == chroma_format_idc_444) {
			detectedFormat.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR;
		} else {
			DE_ASSERT(!"Invalid chroma sub-sampling format");
		}

		switch (pnvsi->uBitDepthLumaMinus8) {
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

		switch (pnvsi->uBitDepthChromaMinus8) {
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

		detectedFormat.bit_depth_luma_minus8 = pnvsi->uBitDepthLumaMinus8;
		detectedFormat.bit_depth_chroma_minus8 = pnvsi->uBitDepthChromaMinus8;
		detectedFormat.bitrate = pnvsi->lBitrate;
		detectedFormat.display_aspect_ratio.x = pnvsi->lDARWidth;
		detectedFormat.display_aspect_ratio.y = pnvsi->lDARHeight;
		detectedFormat.video_signal_description.video_format = pnvsi->lVideoFormat;
		detectedFormat.video_signal_description.video_full_range_flag = pnvsi->uVideoFullRange;
		detectedFormat.video_signal_description.color_primaries = pnvsi->lColorPrimaries;
		detectedFormat.video_signal_description.transfer_characteristics = pnvsi->lTransferCharacteristics;
		detectedFormat.video_signal_description.matrix_coefficients = pnvsi->lMatrixCoefficients;
		detectedFormat.seqhdr_data_length = (deUint32)std::min((size_t)pnvsi->cbSequenceHeader, sizeof(raw_seqhdr_data));
		detectedFormat.minNumDecodeSurfaces = pnvsi->nMinNumDecodeSurfaces;
		detectedFormat.maxNumDpbSlots = configDpbSlots;
		detectedFormat.codecProfile = pnvsi->codecProfile;

		if (detectedFormat.seqhdr_data_length > 0) {
			memcpy(raw_seqhdr_data, pnvsi->SequenceHeaderData,
				   detectedFormat.seqhdr_data_length);
		}
		int32_t maxDecodeRTs = StartVideoSequence(&detectedFormat);
		// nDecodeRTs <= 0 means SequenceCallback failed
		// nDecodeRTs  = 1 means SequenceCallback succeeded
		// nDecodeRTs  > 1 means we need to overwrite the MaxNumDecodeSurfaces
		if (maxDecodeRTs <= 0) {
			return 0;
		}
		// MaxNumDecodeSurface may not be correctly calculated by the client while
		// parser creation so overwrite it with NumDecodeSurface. (only if nDecodeRT
		// > 1)
		if (maxDecodeRTs > 1) {
			m_maxNumDecodeSurfaces = maxDecodeRTs;
		}

	// Always deinit the DPB between sequences. The optimization path does not work for resolution change cases.
	m_maxNumDpbSlots = m_dpb.Init(configDpbSlots, false /* reconfigure the DPB size if true */);
	// Ensure the picture map is empited, so that DPB slot management doesn't get confused in-between sequences.
	m_pictureToDpbSlotMap.fill(-1);

	return m_maxNumDecodeSurfaces;
}

bool VideoBaseDecoder::AllocPictureBuffer (VkPicIf** ppNvidiaVulkanPicture)
{
	DEBUGLOG(std::cout << "VideoBaseDecoder::AllocPictureBuffer" << std::endl);
	bool result = false;

	*ppNvidiaVulkanPicture = m_videoFrameBuffer->ReservePictureBuffer();

	if (*ppNvidiaVulkanPicture)
	{
		result = true;

		DEBUGLOG(std::cout << "\tVideoBaseDecoder::AllocPictureBuffer " << (void*)*ppNvidiaVulkanPicture << std::endl);
	}

	if (!result)
	{
		*ppNvidiaVulkanPicture = (VkPicIf*)nullptr;
	}

	return result;
}

bool VideoBaseDecoder::DecodePicture (VkParserPictureData* pd)
{
	DEBUGLOG(std::cout << "VideoBaseDecoder::DecodePicture" << std::endl);
	bool							result				= false;

	if (!pd->pCurrPic)
	{
		return result;
	}

	vkPicBuffBase*	pVkPicBuff	= GetPic(pd->pCurrPic);
	const int32_t				picIdx		= pVkPicBuff ? pVkPicBuff->m_picIdx : -1;
	if (videoLoggingEnabled())
		std::cout
            << "\t ==> VulkanVideoParser::DecodePicture " << picIdx << std::endl
            << "\t\t progressive: " << (bool)pd->progressive_frame
            << // Frame is progressive
            "\t\t field: " << (bool)pd->field_pic_flag << std::endl
            << // 0 = frame picture, 1 = field picture
            "\t\t\t bottom_field: " << (bool)pd->bottom_field_flag
            << // 0 = top field, 1 = bottom field (ignored if field_pic_flag=0)
            "\t\t\t second_field: " << (bool)pd->second_field
            << // Second field of a complementary field pair
            "\t\t\t top_field: " << (bool)pd->top_field_first << std::endl
            << // Frame pictures only
            "\t\t repeat_first: " << pd->repeat_first_field
            << // For 3:2 pulldown (number of additional fields, 2 = frame
            // doubling, 4 = frame tripling)
            "\t\t ref_pic: " << (bool)pd->ref_pic_flag
            << std::endl; // Frame is a reference frame

	DE_ASSERT(picIdx < MAX_FRM_CNT);

	VkParserDecodePictureInfo	decodePictureInfo	= VkParserDecodePictureInfo();
	decodePictureInfo.pictureIndex				= picIdx;
	decodePictureInfo.flags.progressiveFrame	= pd->progressive_frame;
	decodePictureInfo.flags.fieldPic			= pd->field_pic_flag;			// 0 = frame picture, 1 = field picture
	decodePictureInfo.flags.repeatFirstField	= pd->repeat_first_field;	// For 3:2 pulldown (number of additional fields, 2 = frame doubling, 4 = frame tripling)
	decodePictureInfo.flags.refPic				= pd->ref_pic_flag;				// Frame is a reference frame

	// Mark the first field as unpaired Detect unpaired fields
	if (pd->field_pic_flag)
	{
		decodePictureInfo.flags.bottomField		= pd->bottom_field_flag;	// 0 = top field, 1 = bottom field (ignored if field_pic_flag=0)
		decodePictureInfo.flags.secondField		= pd->second_field;			// Second field of a complementary field pair
		decodePictureInfo.flags.topFieldFirst	= pd->top_field_first;		// Frame pictures only

		if (!pd->second_field)
		{
			decodePictureInfo.flags.unpairedField = true; // Incomplete (half) frame.
		}
		else
		{
			if (decodePictureInfo.flags.unpairedField)
			{
				decodePictureInfo.flags.syncToFirstField = true;
				decodePictureInfo.flags.unpairedField = false;
			}
		}
	}

	decodePictureInfo.frameSyncinfo.unpairedField		= decodePictureInfo.flags.unpairedField;
	decodePictureInfo.frameSyncinfo.syncToFirstField	= decodePictureInfo.flags.syncToFirstField;

	return DecodePicture(pd, pVkPicBuff, &decodePictureInfo);
}


bool VideoBaseDecoder::DecodePicture (VkParserPictureData* pd,
									  vkPicBuffBase* /*pVkPicBuff*/,
									  VkParserDecodePictureInfo* pDecodePictureInfo)
{
	DEBUGLOG(std::cout << "\tDecodePicture sps_sid:" << (deUint32)pNvidiaVulkanParserPictureData->CodecSpecific.h264.pStdSps->seq_parameter_set_id << " pps_sid:" << (deUint32)pNvidiaVulkanParserPictureData->CodecSpecific.h264.pStdPps->seq_parameter_set_id << " pps_pid:" << (deUint32)pNvidiaVulkanParserPictureData->CodecSpecific.h264.pStdPps->pic_parameter_set_id << std::endl);

	if (!pd->pCurrPic)
	{
		return false;
	}
	const deUint32 PicIdx = GetPicIdx(pd->pCurrPic);
	TCU_CHECK (PicIdx < MAX_FRM_CNT);

	m_cachedDecodeParams.emplace_back(new CachedDecodeParameters);
	auto& cachedParameters = m_cachedDecodeParams.back();
	bool bRet = false;

	if (m_resetDecoder)
	{
		cachedParameters->performCodecReset = true;
		m_resetDecoder = false;
	}
	else
	{
		cachedParameters->performCodecReset = false;
	}

	// Copy the picture data over, taking care to memcpy the heap resources that might get freed on the parser side (we have no guarantees about those pointers)
	cachedParameters->pd = *pd;
	if (pd->sideDataLen > 0)
	{
		cachedParameters->pd.pSideData = new deUint8[pd->sideDataLen];
		deMemcpy(cachedParameters->pd.pSideData, pd->pSideData, pd->sideDataLen);
	}
	// And again for the decoded picture information, these are all POD types for now.
	cachedParameters->decodedPictureInfo = *pDecodePictureInfo;
	pDecodePictureInfo = &cachedParameters->decodedPictureInfo;

	// Now build up the frame's decode parameters and store it in the cache
	cachedParameters->pictureParams = VkParserPerFrameDecodeParameters();
	VkParserPerFrameDecodeParameters* pCurrFrameDecParams = &cachedParameters->pictureParams;
	pCurrFrameDecParams->currPicIdx = PicIdx;
	pCurrFrameDecParams->numSlices = pd->numSlices;
	pCurrFrameDecParams->firstSliceIndex = pd->firstSliceIndex;
	pCurrFrameDecParams->bitstreamDataOffset = pd->bitstreamDataOffset;
	pCurrFrameDecParams->bitstreamDataLen = pd->bitstreamDataLen;
	pCurrFrameDecParams->bitstreamData = pd->bitstreamData;

	auto& referenceSlots = cachedParameters->referenceSlots;
	auto& setupReferenceSlot = cachedParameters->setupReferenceSlot;
	setupReferenceSlot.sType = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
	setupReferenceSlot.pPictureResource = nullptr;
	setupReferenceSlot.slotIndex = -1;

	pCurrFrameDecParams->decodeFrameInfo.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_INFO_KHR;
	pCurrFrameDecParams->decodeFrameInfo.dstPictureResource.sType = VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR;
	pCurrFrameDecParams->dpbSetupPictureResource.sType = VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR;

	if (m_profile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR)
	{
		const VkParserH264PictureData* const pin = &pd->CodecSpecific.h264;
		cachedParameters->h264PicParams = nvVideoH264PicParameters();
		VkVideoDecodeH264PictureInfoKHR* h264PictureInfo = &cachedParameters->h264PicParams.pictureInfo;
		nvVideoDecodeH264DpbSlotInfo* h264DpbReferenceList = cachedParameters->h264PicParams.dpbRefList;
		StdVideoDecodeH264PictureInfo* h264StandardPictureInfo = &cachedParameters->h264PicParams.stdPictureInfo;

		pCurrFrameDecParams->pStdPps = pin->pStdPps;
		pCurrFrameDecParams->pStdSps = pin->pStdSps;
		pCurrFrameDecParams->pStdVps = nullptr;

		cachedParameters->decodedPictureInfo.videoFrameType = 0; // pd->CodecSpecific.h264.slice_type;
		// FIXME: If mvcext is enabled.
		cachedParameters->decodedPictureInfo.viewId = pd->CodecSpecific.h264.mvcext.view_id;

		h264PictureInfo->pStdPictureInfo = &cachedParameters->h264PicParams.stdPictureInfo;
		h264PictureInfo->sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PICTURE_INFO_KHR;
		h264PictureInfo->pNext = nullptr;
		pCurrFrameDecParams->decodeFrameInfo.pNext = h264PictureInfo;

		h264StandardPictureInfo->pic_parameter_set_id = pin->pic_parameter_set_id; // PPS ID
		h264StandardPictureInfo->seq_parameter_set_id = pin->seq_parameter_set_id; // SPS ID;
		h264StandardPictureInfo->frame_num = (deUint16)pin->frame_num;
		h264PictureInfo->sliceCount = pd->numSlices;

		deUint32 maxSliceCount = 0;
		DE_ASSERT(pd->firstSliceIndex == 0); // No slice and MV modes are supported yet
		h264PictureInfo->pSliceOffsets = pd->bitstreamData->GetStreamMarkersPtr(pd->firstSliceIndex, maxSliceCount);
		DE_ASSERT(maxSliceCount == pd->numSlices);

		StdVideoDecodeH264PictureInfoFlags currPicFlags = StdVideoDecodeH264PictureInfoFlags();
		currPicFlags.is_intra = (pd->intra_pic_flag != 0);
		// 0 = frame picture, 1 = field picture
		if (pd->field_pic_flag) {
			// 0 = top field, 1 = bottom field (ignored if field_pic_flag = 0)
			currPicFlags.field_pic_flag = true;
			if (pd->bottom_field_flag) {
				currPicFlags.bottom_field_flag = true;
			}
		}
		// Second field of a complementary field pair
		if (pd->second_field) {
			currPicFlags.complementary_field_pair = true;
		}
		// Frame is a reference frame
		if (pd->ref_pic_flag) {
			currPicFlags.is_reference = true;
		}
		h264StandardPictureInfo->flags = currPicFlags;
		if (!pd->field_pic_flag) {
			h264StandardPictureInfo->PicOrderCnt[0] = pin->CurrFieldOrderCnt[0];
			h264StandardPictureInfo->PicOrderCnt[1] = pin->CurrFieldOrderCnt[1];
		} else {
			h264StandardPictureInfo->PicOrderCnt[pd->bottom_field_flag] = pin->CurrFieldOrderCnt[pd->bottom_field_flag];
		}

		const deUint32 maxDpbInputSlots = sizeof(pin->dpb) / sizeof(pin->dpb[0]);
		pCurrFrameDecParams->numGopReferenceSlots = FillDpbH264State(
				pd, pin->dpb, maxDpbInputSlots, h264DpbReferenceList,
				VkParserPerFrameDecodeParameters::MAX_DPB_REF_SLOTS, // 16 reference pictures
				referenceSlots, pCurrFrameDecParams->pGopReferenceImagesIndexes,
				h264StandardPictureInfo->flags, &setupReferenceSlot.slotIndex);

		DE_ASSERT(!pd->ref_pic_flag || (setupReferenceSlot.slotIndex >= 0));

		// TODO: Dummy struct to silence validation. The root problem is that the dpb map doesn't take account of the setup slot,
		// for some reason... So we can't use the existing logic to setup the picture flags and frame number from the dpbEntry
		// class.
		cachedParameters->h264SlotInfo.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_DPB_SLOT_INFO_KHR;
		cachedParameters->h264SlotInfo.pNext = nullptr;
		cachedParameters->h264SlotInfo.pStdReferenceInfo = &cachedParameters->h264RefInfo;

		if (setupReferenceSlot.slotIndex >= 0) {
			setupReferenceSlot.pPictureResource = &pCurrFrameDecParams->dpbSetupPictureResource;
			setupReferenceSlot.pNext = &cachedParameters->h264SlotInfo;
			pCurrFrameDecParams->decodeFrameInfo.pSetupReferenceSlot = &setupReferenceSlot;
		}
		if (pCurrFrameDecParams->numGopReferenceSlots) {
			DE_ASSERT(pCurrFrameDecParams->numGopReferenceSlots <= (int32_t)VkParserPerFrameDecodeParameters::MAX_DPB_REF_SLOTS);
			for (deUint32 dpbEntryIdx = 0; dpbEntryIdx < (deUint32)pCurrFrameDecParams->numGopReferenceSlots;
				 dpbEntryIdx++) {
				pCurrFrameDecParams->pictureResources[dpbEntryIdx].sType = VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR;
				referenceSlots[dpbEntryIdx].pPictureResource = &pCurrFrameDecParams->pictureResources[dpbEntryIdx];
				DE_ASSERT(h264DpbReferenceList[dpbEntryIdx].IsReference());
			}

			pCurrFrameDecParams->decodeFrameInfo.pReferenceSlots = referenceSlots;
			pCurrFrameDecParams->decodeFrameInfo.referenceSlotCount = pCurrFrameDecParams->numGopReferenceSlots;
		} else {
			pCurrFrameDecParams->decodeFrameInfo.pReferenceSlots = NULL;
			pCurrFrameDecParams->decodeFrameInfo.referenceSlotCount = 0;
		}
	}
	else if (m_profile.GetCodecType() == VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR)
	{
		const VkParserHevcPictureData* const pin = &pd->CodecSpecific.hevc;
		cachedParameters->h265PicParams = nvVideoH265PicParameters();
		VkVideoDecodeH265PictureInfoKHR* pPictureInfo = &cachedParameters->h265PicParams.pictureInfo;
		StdVideoDecodeH265PictureInfo* pStdPictureInfo = &cachedParameters->h265PicParams.stdPictureInfo;
		nvVideoDecodeH265DpbSlotInfo* pDpbRefList = cachedParameters->h265PicParams.dpbRefList;

		pCurrFrameDecParams->pStdPps = pin->pStdPps;
		pCurrFrameDecParams->pStdSps = pin->pStdSps;
		pCurrFrameDecParams->pStdVps = pin->pStdVps;
		if (videoLoggingEnabled()) {
			std::cout << "\n\tCurrent h.265 Picture VPS update : "
					  << pin->pStdVps->GetUpdateSequenceCount() << std::endl;
			std::cout << "\n\tCurrent h.265 Picture SPS update : "
					  << pin->pStdSps->GetUpdateSequenceCount() << std::endl;
			std::cout << "\tCurrent h.265 Picture PPS update : "
					  << pin->pStdPps->GetUpdateSequenceCount() << std::endl;
		}

		pPictureInfo->sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PICTURE_INFO_KHR;
		pPictureInfo->pNext = nullptr;

		pPictureInfo->pStdPictureInfo = &cachedParameters->h265PicParams.stdPictureInfo;
		pCurrFrameDecParams->decodeFrameInfo.pNext = &cachedParameters->h265PicParams.pictureInfo;

		pDecodePictureInfo->videoFrameType = 0; // pd->CodecSpecific.hevc.SliceType;
		if (pd->CodecSpecific.hevc.mv_hevc_enable) {
			pDecodePictureInfo->viewId = pd->CodecSpecific.hevc.nuh_layer_id;
		} else {
			pDecodePictureInfo->viewId = 0;
		}

		pPictureInfo->sliceSegmentCount = pd->numSlices;
		deUint32 maxSliceCount = 0;
		DE_ASSERT(pd->firstSliceIndex == 0); // No slice and MV modes are supported yet
		pPictureInfo->pSliceSegmentOffsets = pd->bitstreamData->GetStreamMarkersPtr(pd->firstSliceIndex, maxSliceCount);
		DE_ASSERT(maxSliceCount == pd->numSlices);

		pStdPictureInfo->pps_pic_parameter_set_id   = pin->pic_parameter_set_id;       // PPS ID
		pStdPictureInfo->pps_seq_parameter_set_id   = pin->seq_parameter_set_id;       // SPS ID
		pStdPictureInfo->sps_video_parameter_set_id = pin->vps_video_parameter_set_id; // VPS ID

		// hevc->irapPicFlag = m_slh.nal_unit_type >= NUT_BLA_W_LP &&
		// m_slh.nal_unit_type <= NUT_CRA_NUT;
		pStdPictureInfo->flags.IrapPicFlag = pin->IrapPicFlag; // Intra Random Access Point for current picture.
		// hevc->idrPicFlag = m_slh.nal_unit_type == NUT_IDR_W_RADL ||
		// m_slh.nal_unit_type == NUT_IDR_N_LP;
		pStdPictureInfo->flags.IdrPicFlag = pin->IdrPicFlag; // Instantaneous Decoding Refresh for current picture.

		// NumBitsForShortTermRPSInSlice = s->sh.short_term_rps ?
		// s->sh.short_term_ref_pic_set_size : 0
		pStdPictureInfo->NumBitsForSTRefPicSetInSlice = pin->NumBitsForShortTermRPSInSlice;

		// NumDeltaPocsOfRefRpsIdx = s->sh.short_term_rps ?
		// s->sh.short_term_rps->rps_idx_num_delta_pocs : 0
		pStdPictureInfo->NumDeltaPocsOfRefRpsIdx = pin->NumDeltaPocsOfRefRpsIdx;
		pStdPictureInfo->PicOrderCntVal = pin->CurrPicOrderCntVal;

		if (videoLoggingEnabled())
			std::cout << "\tnumPocStCurrBefore: " << (int32_t)pin->NumPocStCurrBefore
					  << " numPocStCurrAfter: " << (int32_t)pin->NumPocStCurrAfter
					  << " numPocLtCurr: " << (int32_t)pin->NumPocLtCurr << std::endl;

		pCurrFrameDecParams->numGopReferenceSlots = FillDpbH265State(pd, pin, pDpbRefList, pStdPictureInfo,
																	 VkParserPerFrameDecodeParameters::MAX_DPB_REF_SLOTS, // max 16 reference pictures
																	 referenceSlots, pCurrFrameDecParams->pGopReferenceImagesIndexes,
																	 &setupReferenceSlot.slotIndex);

		DE_ASSERT(!pd->ref_pic_flag || (setupReferenceSlot.slotIndex >= 0));
		// TODO: Dummy struct to silence validation. The root problem is that the dpb map doesn't take account of the setup slot,
		// for some reason... So we can't use the existing logic to setup the picture flags and frame number from the dpbEntry
		// class.
		cachedParameters->h265SlotInfo.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_DPB_SLOT_INFO_KHR;
		cachedParameters->h265SlotInfo.pNext = nullptr;
		cachedParameters->h265SlotInfo.pStdReferenceInfo = &cachedParameters->h265RefInfo;

		if (setupReferenceSlot.slotIndex >= 0) {
			setupReferenceSlot.pPictureResource = &pCurrFrameDecParams->dpbSetupPictureResource;
			setupReferenceSlot.pNext = &cachedParameters->h265SlotInfo;
			pCurrFrameDecParams->decodeFrameInfo.pSetupReferenceSlot = &setupReferenceSlot;
		}
		if (pCurrFrameDecParams->numGopReferenceSlots) {
			DE_ASSERT(pCurrFrameDecParams->numGopReferenceSlots <= (int32_t)VkParserPerFrameDecodeParameters::MAX_DPB_REF_SLOTS);
			for (deUint32 dpbEntryIdx = 0; dpbEntryIdx < (deUint32)pCurrFrameDecParams->numGopReferenceSlots;
				 dpbEntryIdx++) {
				pCurrFrameDecParams->pictureResources[dpbEntryIdx].sType = VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR;
				referenceSlots[dpbEntryIdx].pPictureResource = &pCurrFrameDecParams->pictureResources[dpbEntryIdx];
				DE_ASSERT(pDpbRefList[dpbEntryIdx].IsReference());
			}

			pCurrFrameDecParams->decodeFrameInfo.pReferenceSlots = referenceSlots;
			pCurrFrameDecParams->decodeFrameInfo.referenceSlotCount = pCurrFrameDecParams->numGopReferenceSlots;
		} else {
			pCurrFrameDecParams->decodeFrameInfo.pReferenceSlots = nullptr;
			pCurrFrameDecParams->decodeFrameInfo.referenceSlotCount = 0;
		}

		if (videoLoggingEnabled()) {
			for (int32_t i = 0; i < HEVC_MAX_DPB_SLOTS; i++) {
				std::cout << "\tdpbIndex: " << i;
				if (pDpbRefList[i]) {
					std::cout << " REFERENCE FRAME";

					std::cout << " picOrderCntValList: "
							  << (int32_t)pDpbRefList[i]
									  .dpbSlotInfo.pStdReferenceInfo->PicOrderCntVal;

					std::cout << "\t\t Flags: ";
					if (pDpbRefList[i]
							.dpbSlotInfo.pStdReferenceInfo->flags.used_for_long_term_reference) {
						std::cout << "IS LONG TERM ";
					}

				} else {
					std::cout << " NOT A REFERENCE ";
				}
				std::cout << std::endl;
			}
		}
	}

	pDecodePictureInfo->displayWidth	= m_nvsi.nDisplayWidth;
	pDecodePictureInfo->displayHeight	= m_nvsi.nDisplayHeight;

	bRet = DecodePictureWithParameters(cachedParameters) >= 0;

	DE_ASSERT(bRet);

	m_nCurrentPictureID++;

	return bRet;
}

int32_t VideoBaseDecoder::DecodePictureWithParameters(MovePtr<CachedDecodeParameters>& cachedParameters)
{
	TCU_CHECK_MSG(m_videoSession, "Video session has not been initialized!");

	auto *pPicParams = &cachedParameters->pictureParams;

	int32_t currPicIdx = pPicParams->currPicIdx;
	DE_ASSERT((deUint32) currPicIdx < m_numDecodeSurfaces);

	cachedParameters->picNumInDecodeOrder = m_decodePicCount++;
	m_videoFrameBuffer->SetPicNumInDecodeOrder(currPicIdx, cachedParameters->picNumInDecodeOrder);

	DE_ASSERT(pPicParams->bitstreamData->GetMaxSize() >= pPicParams->bitstreamDataLen);
	pPicParams->decodeFrameInfo.srcBuffer = pPicParams->bitstreamData->GetBuffer();
	DE_ASSERT(pPicParams->bitstreamDataOffset == 0);
	DE_ASSERT(pPicParams->firstSliceIndex == 0);
	pPicParams->decodeFrameInfo.srcBufferOffset = pPicParams->bitstreamDataOffset;
	pPicParams->decodeFrameInfo.srcBufferRange = deAlign64(pPicParams->bitstreamDataLen, m_videoCaps.minBitstreamBufferSizeAlignment);

	int32_t retPicIdx = GetCurrentFrameData((deUint32) currPicIdx, cachedParameters->frameDataSlot);
	DE_ASSERT(retPicIdx == currPicIdx);

	if (retPicIdx != currPicIdx)
	{
		fprintf(stderr, "\nERROR: DecodePictureWithParameters() retPicIdx(%d) != currPicIdx(%d)\n", retPicIdx, currPicIdx);
	}

	auto &decodeBeginInfo = cachedParameters->decodeBeginInfo;
	decodeBeginInfo.sType = VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR;
	// CmdResetQueryPool are NOT Supported yet.
	decodeBeginInfo.pNext = pPicParams->beginCodingInfoPictureParametersExt;
	decodeBeginInfo.videoSession = m_videoSession->GetVideoSession();

	cachedParameters->currentPictureParameterObject = m_currentPictureParameters;

	DE_ASSERT(!!pPicParams->decodeFrameInfo.srcBuffer);
	cachedParameters->bitstreamBufferMemoryBarrier = {
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR,
			nullptr,
			VK_PIPELINE_STAGE_2_NONE_KHR,
			0, // VK_ACCESS_2_HOST_WRITE_BIT_KHR,
			VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR,
			VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR,
			(deUint32) m_deviceContext->decodeQueueFamilyIdx(),
			(deUint32) m_deviceContext->decodeQueueFamilyIdx(),
			pPicParams->decodeFrameInfo.srcBuffer,
			pPicParams->decodeFrameInfo.srcBufferOffset,
			pPicParams->decodeFrameInfo.srcBufferRange
	};

	deUint32 baseArrayLayer = (m_useImageArray || m_useImageViewArray) ? pPicParams->currPicIdx : 0;
	const VkImageMemoryBarrier2KHR dpbBarrierTemplate = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR, // VkStructureType sType
			nullptr, // const void*     pNext
			VK_PIPELINE_STAGE_2_NONE_KHR, // VkPipelineStageFlags2KHR srcStageMask
			0, // VkAccessFlags2KHR        srcAccessMask
			VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR, // VkPipelineStageFlags2KHR dstStageMask;
			VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR, // VkAccessFlags   dstAccessMask
			VK_IMAGE_LAYOUT_UNDEFINED, // VkImageLayout   oldLayout
			VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR, // VkImageLayout   newLayout
			(deUint32) m_deviceContext->decodeQueueFamilyIdx(), // deUint32        srcQueueFamilyIndex
			(deUint32) m_deviceContext->decodeQueueFamilyIdx(), // deUint32   dstQueueFamilyIndex
			VK_NULL_HANDLE, // VkImage         image;
			{
					// VkImageSubresourceRange   subresourceRange
					VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask
					0, // deUint32           baseMipLevel
					1, // deUint32           levelCount
					baseArrayLayer, // deUint32           baseArrayLayer
					1, // deUint32           layerCount;
			},
	};

	cachedParameters->currentDpbPictureResourceInfo = VulkanVideoFrameBuffer::PictureResourceInfo();
	cachedParameters->currentOutputPictureResourceInfo = VulkanVideoFrameBuffer::PictureResourceInfo();
	deMemset(&cachedParameters->currentOutputPictureResource, 0, sizeof(VkVideoPictureResourceInfoKHR));
	cachedParameters->currentOutputPictureResource.sType = VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR;

	auto *pOutputPictureResource = cachedParameters->pOutputPictureResource;
	auto *pOutputPictureResourceInfo = cachedParameters->pOutputPictureResourceInfo;
	if (!dpbAndOutputCoincide())
	{
		// Output Distinct will use the decodeFrameInfo.dstPictureResource directly.
		pOutputPictureResource = &pPicParams->decodeFrameInfo.dstPictureResource;
	}
	else if (true) // TODO: Tidying
	{
		// Output Coincide needs the output only if we are processing linear images that we need to copy to below.
		pOutputPictureResource = &cachedParameters->currentOutputPictureResource;
	}

	if (pOutputPictureResource)
	{
		// if the pOutputPictureResource is set then we also need the pOutputPictureResourceInfo.
		pOutputPictureResourceInfo = &cachedParameters->currentOutputPictureResourceInfo;
	}

	if (pPicParams->currPicIdx !=
		m_videoFrameBuffer->GetCurrentImageResourceByIndex(pPicParams->currPicIdx,
														   &pPicParams->dpbSetupPictureResource,
														   &cachedParameters->currentDpbPictureResourceInfo,
														   VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR,
														   pOutputPictureResource,
														   pOutputPictureResourceInfo,
														   VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR))
	{
		DE_ASSERT(!"GetImageResourcesByIndex has failed");
	}

	if (dpbAndOutputCoincide())
	{
		// For the Output Coincide, the DPB and destination output resources are the same.
		pPicParams->decodeFrameInfo.dstPictureResource = pPicParams->dpbSetupPictureResource;
	}
	else if (pOutputPictureResourceInfo)
	{
		// For Output Distinct transition the image to DECODE_DST
		if (pOutputPictureResourceInfo->currentImageLayout == VK_IMAGE_LAYOUT_UNDEFINED)
		{
			VkImageMemoryBarrier2KHR barrier = dpbBarrierTemplate;
			barrier.oldLayout = pOutputPictureResourceInfo->currentImageLayout;
			barrier.newLayout = VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR;
			barrier.image = pOutputPictureResourceInfo->image;
			barrier.dstAccessMask = VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR;
			cachedParameters->imageBarriers.push_back(barrier);
			DE_ASSERT(!!cachedParameters->imageBarriers.back().image);
		}
	}

	if (cachedParameters->currentDpbPictureResourceInfo.currentImageLayout == VK_IMAGE_LAYOUT_UNDEFINED)
	{
		VkImageMemoryBarrier2KHR barrier = dpbBarrierTemplate;
		barrier.oldLayout = cachedParameters->currentDpbPictureResourceInfo.currentImageLayout;
		barrier.newLayout = VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR;
		barrier.image = cachedParameters->currentDpbPictureResourceInfo.image;
		barrier.dstAccessMask = VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR;
		cachedParameters->imageBarriers.push_back(barrier);
		DE_ASSERT(!!cachedParameters->imageBarriers.back().image);
	}

	// Transition all the DPB images to DECODE_DPB layout, if necessary.
	deMemset(cachedParameters->pictureResourcesInfo, 0, DE_LENGTH_OF_ARRAY(cachedParameters->pictureResourcesInfo) * sizeof(cachedParameters->pictureResourcesInfo[0]));
	const int8_t *pGopReferenceImagesIndexes = pPicParams->pGopReferenceImagesIndexes;
	if (pPicParams->numGopReferenceSlots)
	{
		if (pPicParams->numGopReferenceSlots != m_videoFrameBuffer->GetDpbImageResourcesByIndex(
				pPicParams->numGopReferenceSlots,
				pGopReferenceImagesIndexes,
				pPicParams->pictureResources,
				cachedParameters->pictureResourcesInfo,
				VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR))
		{
			DE_ASSERT(!"GetImageResourcesByIndex has failed");
		}
		for (int32_t resId = 0; resId < pPicParams->numGopReferenceSlots; resId++)
		{
			// slotLayer requires NVIDIA specific extension VK_KHR_video_layers, not enabled, just yet.
			// pGopReferenceSlots[resId].slotLayerIndex = 0;
			// pictureResourcesInfo[resId].image can be a nullptr handle if the picture is not-existent.
			if (!!cachedParameters->pictureResourcesInfo[resId].image &&
				(cachedParameters->pictureResourcesInfo[resId].currentImageLayout != VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR) &&
				(cachedParameters->pictureResourcesInfo[resId].currentImageLayout != VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR))
			{
				VkImageMemoryBarrier2KHR barrier = dpbBarrierTemplate;
				barrier.oldLayout = cachedParameters->currentDpbPictureResourceInfo.currentImageLayout;
				barrier.newLayout = VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR;
				barrier.image = cachedParameters->pictureResourcesInfo[resId].image;
				barrier.dstAccessMask = VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR;
				cachedParameters->imageBarriers.push_back(barrier);
				DE_ASSERT(!!cachedParameters->imageBarriers.back().image);
			}
		}
	}

	decodeBeginInfo.referenceSlotCount = pPicParams->decodeFrameInfo.referenceSlotCount;
	decodeBeginInfo.pReferenceSlots = pPicParams->decodeFrameInfo.pReferenceSlots;

	// Ensure the resource for the resources associated with the
	// reference slot (if it exists) are in the bound picture
	// resources set.  See VUID-vkCmdDecodeVideoKHR-pDecodeInfo-07149.
	if (pPicParams->decodeFrameInfo.pSetupReferenceSlot != nullptr)
	{
		cachedParameters->fullReferenceSlots.clear();
		for (deUint32 i = 0; i < decodeBeginInfo.referenceSlotCount; i++)
			cachedParameters->fullReferenceSlots.push_back(decodeBeginInfo.pReferenceSlots[i]);
		VkVideoReferenceSlotInfoKHR setupActivationSlot = {};
		setupActivationSlot.sType = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
		setupActivationSlot.slotIndex = -1;
		setupActivationSlot.pPictureResource = &pPicParams->dpbSetupPictureResource; //dpbAndOutputCoincide() ? &pPicParams->decodeFrameInfo.dstPictureResource : &pPicParams->pictureResources[pPicParams->numGopReferenceSlots];
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
	cachedParameters->frameSynchronizationInfo.hasFrameCompleteSignalSemaphore = true;


	VulkanVideoFrameBuffer::ReferencedObjectsInfo referencedObjectsInfo(pPicParams->bitstreamData,
																		pPicParams->pStdPps,
																		pPicParams->pStdSps,
																		pPicParams->pStdVps);
	int picIdx = m_videoFrameBuffer->QueuePictureForDecode(currPicIdx, &cachedParameters->decodedPictureInfo, &referencedObjectsInfo,
														   &cachedParameters->frameSynchronizationInfo);
	DE_ASSERT(picIdx == currPicIdx);
	DE_UNREF(picIdx);

	if (m_outOfOrderDecoding)
		return currPicIdx;

	WaitForFrameFences(cachedParameters);
	ApplyPictureParameters(cachedParameters);
	RecordCommandBuffer(cachedParameters);
	SubmitQueue(cachedParameters);
	if (m_queryResultWithStatus)
	{
		QueryDecodeResults(cachedParameters);
	}

	return currPicIdx;
}

void VideoBaseDecoder::ApplyPictureParameters(de::MovePtr<CachedDecodeParameters> &cachedParameters)
{
	auto* pPicParams = &cachedParameters->pictureParams;
	VkSharedBaseObj<VkVideoRefCountBase> currentVkPictureParameters;
	bool valid = pPicParams->pStdPps->GetClientObject(currentVkPictureParameters);
	DE_ASSERT(currentVkPictureParameters && valid);
	VkParserVideoPictureParameters *pOwnerPictureParameters =
			VkParserVideoPictureParameters::VideoPictureParametersFromBase(currentVkPictureParameters);
	DE_ASSERT(pOwnerPictureParameters);
	int32_t ret = pOwnerPictureParameters->FlushPictureParametersQueue(m_videoSession);
	DE_ASSERT(ret >= 0);
	DE_UNREF(ret);
	bool isSps = false;
	int32_t spsId = pPicParams->pStdPps->GetSpsId(isSps);
	DE_ASSERT(!isSps);
	DE_ASSERT(spsId >= 0);
	DE_ASSERT(pOwnerPictureParameters->HasSpsId(spsId));
	bool isPps = false;
	int32_t ppsId = pPicParams->pStdPps->GetPpsId(isPps);
	DE_ASSERT(isPps);
	DE_ASSERT(ppsId >= 0);
	DE_ASSERT(pOwnerPictureParameters->HasPpsId(ppsId));
	DE_UNREF(valid);

	cachedParameters->decodeBeginInfo.videoSessionParameters = *pOwnerPictureParameters;

	if (videoLoggingEnabled())
	{
		std::cout << "ApplyPictureParameters object " << cachedParameters->decodeBeginInfo.videoSessionParameters << " with ID: (" << pOwnerPictureParameters->GetId() << ")"
				  << " for SPS: " << spsId << ", PPS: " << ppsId << std::endl;
	}
}

void VideoBaseDecoder::WaitForFrameFences(de::MovePtr<CachedDecodeParameters> &cachedParameters)
{
	// Check here that the frame for this entry (for this command buffer) has already completed decoding.
	// Otherwise we may step over a hot command buffer by starting a new recording.
	// This fence wait should be NOP in 99.9% of the cases, because the decode queue is deep enough to
	// ensure the frame has already been completed.
	VK_CHECK(m_deviceContext->getDeviceDriver().waitForFences(m_deviceContext->device, 1, &cachedParameters->frameSynchronizationInfo.frameCompleteFence, true, TIMEOUT_100ms));
	VkResult result = m_deviceContext->getDeviceDriver().getFenceStatus(m_deviceContext->device, cachedParameters->frameSynchronizationInfo.frameCompleteFence);
	TCU_CHECK_MSG(result == VK_SUCCESS || result == VK_NOT_READY, "Bad fence status");
}

void VideoBaseDecoder::RecordCommandBuffer(de::MovePtr<CachedDecodeParameters> &cachedParameters)
{
	auto &vk = m_deviceContext->getDeviceDriver();

	VkCommandBuffer commandBuffer = cachedParameters->frameDataSlot.commandBuffer;

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	beginInfo.pInheritanceInfo = nullptr;

	vk.beginCommandBuffer(commandBuffer, &beginInfo);

	if (m_queryResultWithStatus)
	{
		vk.cmdResetQueryPool(commandBuffer, cachedParameters->frameSynchronizationInfo.queryPool, cachedParameters->frameSynchronizationInfo.startQueryId,
							 cachedParameters->frameSynchronizationInfo.numQueries);
	}

	vk.cmdBeginVideoCodingKHR(commandBuffer, &cachedParameters->decodeBeginInfo);

	if (cachedParameters->performCodecReset)
	{
		VkVideoCodingControlInfoKHR codingControlInfo = {VK_STRUCTURE_TYPE_VIDEO_CODING_CONTROL_INFO_KHR,
														 nullptr,
														 VK_VIDEO_CODING_CONTROL_RESET_BIT_KHR};
		vk.cmdControlVideoCodingKHR(commandBuffer, &codingControlInfo);
	}

	const VkDependencyInfoKHR dependencyInfo = {
			VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
			nullptr,
			VK_DEPENDENCY_BY_REGION_BIT,
			0,
			nullptr,
			1,
			&cachedParameters->bitstreamBufferMemoryBarrier,
			static_cast<deUint32>(cachedParameters->imageBarriers.size()),
			cachedParameters->imageBarriers.data(),
	};
	vk.cmdPipelineBarrier2(commandBuffer, &dependencyInfo);

	if (m_queryResultWithStatus)
	{
		vk.cmdBeginQuery(commandBuffer, cachedParameters->frameSynchronizationInfo.queryPool, cachedParameters->frameSynchronizationInfo.startQueryId, VkQueryControlFlags());
	}

	vk.cmdDecodeVideoKHR(commandBuffer, &cachedParameters->pictureParams.decodeFrameInfo);

	if (m_queryResultWithStatus)
	{
		vk.cmdEndQuery(commandBuffer, cachedParameters->frameSynchronizationInfo.queryPool, cachedParameters->frameSynchronizationInfo.startQueryId);
	}

	VkVideoEndCodingInfoKHR decodeEndInfo{};
	decodeEndInfo.sType = VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR;
	vk.cmdEndVideoCodingKHR(commandBuffer, &decodeEndInfo);

	m_deviceContext->getDeviceDriver().endCommandBuffer(commandBuffer);
}

void VideoBaseDecoder::SubmitQueue(de::MovePtr<CachedDecodeParameters> &cachedParameters)
{
	auto&			vk								 = m_deviceContext->getDeviceDriver();
	auto			device							 = m_deviceContext->device;
	VkCommandBuffer commandBuffer					 = cachedParameters->frameDataSlot.commandBuffer;
	VkSubmitInfo	submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount					 = (cachedParameters->frameSynchronizationInfo.frameConsumerDoneSemaphore == VK_NULL_HANDLE) ? 0 : 1;
	submitInfo.pWaitSemaphores						 = &cachedParameters->frameSynchronizationInfo.frameConsumerDoneSemaphore;
	VkPipelineStageFlags videoDecodeSubmitWaitStages = VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR;
	submitInfo.pWaitDstStageMask					 = &videoDecodeSubmitWaitStages;
	submitInfo.commandBufferCount					 = 1;
	submitInfo.pCommandBuffers						 = &commandBuffer;
	submitInfo.signalSemaphoreCount					 = 1;
	submitInfo.pSignalSemaphores					 = &cachedParameters->frameSynchronizationInfo.frameCompleteSemaphore;

	if ((cachedParameters->frameSynchronizationInfo.frameConsumerDoneSemaphore == VK_NULL_HANDLE) &&
		(cachedParameters->frameSynchronizationInfo.frameConsumerDoneFence != VK_NULL_HANDLE))
	{
		VK_CHECK(vk.waitForFences(device, 1, &cachedParameters->frameSynchronizationInfo.frameConsumerDoneFence, true, TIMEOUT_100ms));
		VkResult result = vk.getFenceStatus(device, cachedParameters->frameSynchronizationInfo.frameCompleteFence);
		TCU_CHECK_MSG(result == VK_SUCCESS || result == VK_NOT_READY, "Bad fence status");
	}

	VK_CHECK(vk.resetFences(device, 1, &cachedParameters->frameSynchronizationInfo.frameCompleteFence));
	VkResult result = vk.getFenceStatus(device, cachedParameters->frameSynchronizationInfo.frameCompleteFence);
	TCU_CHECK_MSG(result == VK_SUCCESS || result == VK_NOT_READY, "Bad fence status");

	VK_CHECK(vk.queueSubmit(m_deviceContext->decodeQueue, 1, &submitInfo, cachedParameters->frameSynchronizationInfo.frameCompleteFence));

	if (videoLoggingEnabled())
	{
		std::cout << "\t +++++++++++++++++++++++++++< " << cachedParameters->pictureParams.currPicIdx << " >++++++++++++++++++++++++++++++" << std::endl;
		std::cout << std::dec << "\t => Decode Submitted for CurrPicIdx: " << cachedParameters->pictureParams.currPicIdx << std::endl
				  << "\t\tm_nPicNumInDecodeOrder: " << cachedParameters->picNumInDecodeOrder << "\t\tframeCompleteFence " << cachedParameters->frameSynchronizationInfo.frameCompleteFence
				  << "\t\tframeCompleteSemaphore " << cachedParameters->frameSynchronizationInfo.frameCompleteSemaphore << "\t\tdstImageView "
				  << cachedParameters->pictureParams.decodeFrameInfo.dstPictureResource.imageViewBinding << std::endl;
	}

	const bool checkDecodeIdleSync = false; // For fence/sync/idle debugging
	if (checkDecodeIdleSync)
	{ // For fence/sync debugging
		if (cachedParameters->frameSynchronizationInfo.frameCompleteFence == VK_NULL_HANDLE)
		{
			VK_CHECK(vk.queueWaitIdle(m_deviceContext->decodeQueue));
		}
		else
		{
			if (cachedParameters->frameSynchronizationInfo.frameCompleteSemaphore == VK_NULL_HANDLE)
			{
				VK_CHECK(vk.waitForFences(device, 1, &cachedParameters->frameSynchronizationInfo.frameCompleteFence, true, TIMEOUT_100ms));
				result = vk.getFenceStatus(device, cachedParameters->frameSynchronizationInfo.frameCompleteFence);
				TCU_CHECK_MSG(result == VK_SUCCESS || result == VK_NOT_READY, "Bad fence status");
			}
		}
	}
}

void VideoBaseDecoder::QueryDecodeResults(de::MovePtr<CachedDecodeParameters> &cachedParameters)
{
	auto& vk						   = m_deviceContext->getDeviceDriver();
	auto  device					   = m_deviceContext->device;

	VkQueryResultStatusKHR decodeStatus;
	VkResult result = vk.getQueryPoolResults(device,
											 cachedParameters->frameSynchronizationInfo.queryPool,
											 cachedParameters->frameSynchronizationInfo.startQueryId,
											 1,
											 sizeof(decodeStatus),
											 &decodeStatus,
											 sizeof(decodeStatus),
											 VK_QUERY_RESULT_WITH_STATUS_BIT_KHR | VK_QUERY_RESULT_WAIT_BIT);
	if (videoLoggingEnabled())
	{
		std::cout << "\t +++++++++++++++++++++++++++< " << cachedParameters->pictureParams.currPicIdx << " >++++++++++++++++++++++++++++++" << std::endl;
		std::cout << "\t => Decode Status for CurrPicIdx: " << cachedParameters->pictureParams.currPicIdx << std::endl
				  << "\t\tdecodeStatus: " << decodeStatus << std::endl;
	}

	TCU_CHECK_AND_THROW(TestError, result == VK_SUCCESS || result == VK_ERROR_DEVICE_LOST, "Driver has returned an invalid query result");
	TCU_CHECK_AND_THROW(TestError, decodeStatus != VK_QUERY_RESULT_STATUS_ERROR_KHR, "Decode query returned an unexpected error");
}

void VideoBaseDecoder::decodeFramesOutOfOrder()
{
	std::vector<int> ordering(m_cachedDecodeParams.size());
	std::iota(ordering.begin(), ordering.end(), 0);
	if (ordering.size() == 2)
		std::swap(ordering[0], ordering[1]);
	else // TODO: test seeding
		std::shuffle(ordering.begin(), ordering.end(), std::mt19937{std::random_device{}()});

	DE_ASSERT(m_cachedDecodeParams.size() > 1);

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
		auto& cachedParams = m_cachedDecodeParams[i];
		SubmitQueue(cachedParams);
		if (m_queryResultWithStatus)
		{
			QueryDecodeResults(cachedParams);
		}
	}
}

bool VideoBaseDecoder::UpdatePictureParameters(VkSharedBaseObj<StdVideoPictureParametersSet>& pictureParametersObject, /* in */
											   VkSharedBaseObj<VkVideoRefCountBase>&		  client)
{
	triggerPictureParameterSequenceCount();

	VkResult result = VkParserVideoPictureParameters::AddPictureParameters(*m_deviceContext,
																		   m_videoSession,
																		   pictureParametersObject,
																		   m_currentPictureParameters);
	client			= m_currentPictureParameters;
	return (result == VK_SUCCESS);
}

bool VideoBaseDecoder::DisplayPicture (VkPicIf*	pNvidiaVulkanPicture,
									   int64_t					/*llPTS*/)
{
	vkPicBuffBase* pVkPicBuff = GetPic(pNvidiaVulkanPicture);

	DE_ASSERT(pVkPicBuff != DE_NULL);
	int32_t picIdx = pVkPicBuff ? pVkPicBuff->m_picIdx : -1;
	DE_ASSERT(picIdx != -1);
	DE_ASSERT(m_videoFrameBuffer != nullptr);

	if (videoLoggingEnabled())
	{
		std::cout << "\t ======================< " << picIdx << " >============================" << std::endl;
		std::cout << "\t ==> VulkanVideoParser::DisplayPicture " << picIdx << std::endl;
	}

	VulkanVideoDisplayPictureInfo dispInfo = VulkanVideoDisplayPictureInfo();

	dispInfo.timestamp					   = 0; // NOTE: we ignore PTS in the CTS

	const int32_t retVal				   = m_videoFrameBuffer->QueueDecodedPictureForDisplay((int8_t)picIdx, &dispInfo);
	DE_ASSERT(picIdx == retVal);
	DE_UNREF(retVal);

	return true;
}

int32_t VideoBaseDecoder::ReleaseDisplayedFrame(DecodedFrame* pDisplayedFrame)
{
	if (pDisplayedFrame->pictureIndex == -1)
		return -1;

	DecodedFrameRelease	 decodedFramesRelease		 = {pDisplayedFrame->pictureIndex, 0, 0, 0, 0, 0};
	DecodedFrameRelease* decodedFramesReleasePtr	 = &decodedFramesRelease;
	pDisplayedFrame->pictureIndex					 = -1;
	decodedFramesRelease.decodeOrder				 = pDisplayedFrame->decodeOrder;
	decodedFramesRelease.displayOrder				 = pDisplayedFrame->displayOrder;
	decodedFramesRelease.hasConsummerSignalFence	 = pDisplayedFrame->hasConsummerSignalFence;
	decodedFramesRelease.hasConsummerSignalSemaphore = pDisplayedFrame->hasConsummerSignalSemaphore;
	decodedFramesRelease.timestamp					 = 0;

	return m_videoFrameBuffer->ReleaseDisplayedPicture(&decodedFramesReleasePtr, 1);
}

VkDeviceSize VideoBaseDecoder::GetBitstreamBuffer(VkDeviceSize size, VkDeviceSize minBitstreamBufferOffsetAlignment, VkDeviceSize minBitstreamBufferSizeAlignment, const deUint8* pInitializeBufferMemory, VkDeviceSize initializeBufferMemorySize, VkSharedBaseObj<VulkanBitstreamBuffer>& bitstreamBuffer)
{
	DE_ASSERT(initializeBufferMemorySize <= size);
	VkDeviceSize							newSize = size;
	VkSharedBaseObj<BitstreamBufferImpl>	newBitstreamBuffer;

	VK_CHECK(BitstreamBufferImpl::Create(m_deviceContext,
										 m_deviceContext->decodeQueueFamilyIdx(),
										 newSize,
										 minBitstreamBufferOffsetAlignment,
										 minBitstreamBufferSizeAlignment,
										 newBitstreamBuffer,
										 m_profile.GetProfileListInfo()));
	if (videoLoggingEnabled())
	{
		std::cout << "\tAllocated bitstream buffer with size " << newSize << " B, " << newSize / 1024 << " KB, " << newSize / 1024 / 1024 << " MB" << std::endl;
	}

	DE_ASSERT(newBitstreamBuffer);
	newSize = newBitstreamBuffer->GetMaxSize();
	DE_ASSERT(initializeBufferMemorySize <= newSize);

	size_t bytesToCopy = std::min(initializeBufferMemorySize, newSize);
	size_t bytesCopied = newBitstreamBuffer->CopyDataFromBuffer((const deUint8*)pInitializeBufferMemory, 0, 0, bytesToCopy);
	DE_ASSERT(bytesToCopy == bytesCopied);
	DE_UNREF(bytesCopied);

	newBitstreamBuffer->MemsetData(0x0, bytesToCopy, newSize - bytesToCopy);

	if (videoLoggingEnabled())
	{
		std::cout << "\t\tFrom bitstream buffer pool with size " << newSize << " B, " << newSize / 1024 << " KB, " << newSize / 1024 / 1024 << " MB" << std::endl;

		std::cout << "\t\t\t FreeNodes " << m_decodeFramesData.GetBitstreamBuffersQueue().GetFreeNodesNumber();
		std::cout << " of MaxNodes " << m_decodeFramesData.GetBitstreamBuffersQueue().GetMaxNodes();
		std::cout << ", AvailableNodes " << m_decodeFramesData.GetBitstreamBuffersQueue().GetAvailableNodesNumber();
		std::cout << std::endl;
	}

	bitstreamBuffer = newBitstreamBuffer;
	if (videoLoggingEnabled() && newSize > m_maxStreamBufferSize)
	{
		std::cout << "\tAllocated bitstream buffer with size " << newSize << " B, " << newSize / 1024 << " KB, " << newSize / 1024 / 1024 << " MB" << std::endl;
		m_maxStreamBufferSize = newSize;
	}
	return bitstreamBuffer->GetMaxSize();
}

void VideoBaseDecoder::UnhandledNALU (const deUint8*	pbData,
									  size_t			cbData)
{
	const vector<deUint8> data (pbData, pbData + cbData);
	ostringstream css;

	css << "UnhandledNALU=";

	for (const auto& i: data)
		css << std::hex << std::setw(2) << std::setfill('0') << (deUint32)i << ' ';

	TCU_THROW(InternalError, css.str());
}

deUint32 VideoBaseDecoder::FillDpbH264State (const VkParserPictureData *	pd,
											 const VkParserH264DpbEntry*	dpbIn,
											 deUint32								maxDpbInSlotsInUse,
											 nvVideoDecodeH264DpbSlotInfo*		pDpbRefList,
											 deUint32			/*maxRefPictures*/,
											 VkVideoReferenceSlotInfoKHR*			pReferenceSlots,
											 int8_t*								pGopReferenceImagesIndexes,
											 StdVideoDecodeH264PictureInfoFlags		currPicFlags,
											 int32_t*								pCurrAllocatedSlotIndex)
{
	// #### Update m_dpb based on dpb parameters ####
	// Create unordered DPB and generate a bitmask of all render targets present
	// in DPB
	deUint32 num_ref_frames = pd->CodecSpecific.h264.pStdSps->GetStdH264Sps()->max_num_ref_frames;
	DE_ASSERT(num_ref_frames <= HEVC_MAX_DPB_SLOTS);
	DE_ASSERT(num_ref_frames <= m_maxNumDpbSlots);
	dpbH264Entry refOnlyDpbIn[AVC_MAX_DPB_SLOTS]; // max number of Dpb
	// surfaces
	memset(&refOnlyDpbIn, 0, m_maxNumDpbSlots * sizeof(refOnlyDpbIn[0]));
	deUint32 refDpbUsedAndValidMask = 0;
	deUint32 numUsedRef = 0;
	for (int32_t inIdx = 0; (deUint32)inIdx < maxDpbInSlotsInUse; inIdx++) {
		// used_for_reference: 0 = unused, 1 = top_field, 2 = bottom_field, 3 =
		// both_fields
		const deUint32 used_for_reference = dpbIn[inIdx].used_for_reference & fieldIsReferenceMask;
		if (used_for_reference) {
			int8_t picIdx = (!dpbIn[inIdx].not_existing && dpbIn[inIdx].pPicBuf)
							? GetPicIdx(dpbIn[inIdx].pPicBuf)
							: -1;
			const bool isFieldRef = (picIdx >= 0) ? GetFieldPicFlag(picIdx)
												  : (used_for_reference && (used_for_reference != fieldIsReferenceMask));
			const int16_t fieldOrderCntList[2] = {
					(int16_t)dpbIn[inIdx].FieldOrderCnt[0],
					(int16_t)dpbIn[inIdx].FieldOrderCnt[1]
			};
			refOnlyDpbIn[numUsedRef].setReferenceAndTopBottomField(
					!!used_for_reference,
					(picIdx < 0), /* not_existing is frame inferred by the decoding
                           process for gaps in frame_num */
					!!dpbIn[inIdx].is_long_term, isFieldRef,
					!!(used_for_reference & topFieldMask),
					!!(used_for_reference & bottomFieldMask), dpbIn[inIdx].FrameIdx,
					fieldOrderCntList, GetPic(dpbIn[inIdx].pPicBuf));
			if (picIdx >= 0) {
				refDpbUsedAndValidMask |= (1 << picIdx);
			}
			numUsedRef++;
		}
		// Invalidate all slots.
		pReferenceSlots[inIdx].slotIndex = -1;
		pGopReferenceImagesIndexes[inIdx] = -1;
	}

	DE_ASSERT(numUsedRef <= HEVC_MAX_DPB_SLOTS);
	DE_ASSERT(numUsedRef <= m_maxNumDpbSlots);
	DE_ASSERT(numUsedRef <= num_ref_frames);

	if (videoLoggingEnabled()) {
		std::cout << " =>>> ********************* picIdx: "
				  << (int32_t)GetPicIdx(pd->pCurrPic)
				  << " *************************" << std::endl;
		std::cout << "\tRef frames data in for picIdx: "
				  << (int32_t)GetPicIdx(pd->pCurrPic) << std::endl
				  << "\tSlot Index:\t\t";
		if (numUsedRef == 0)
			std::cout << "(none)" << std::endl;
		else
		{
			for (deUint32 slot = 0; slot < numUsedRef; slot++)
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
			for (deUint32 slot = 0; slot < numUsedRef; slot++)
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
		std::cout << "\n\tTotal Ref frames for picIdx: "
				  << (int32_t)GetPicIdx(pd->pCurrPic) << " : " << numUsedRef
				  << " out of " << num_ref_frames << " MAX(" << m_maxNumDpbSlots
				  << ")" << std::endl
				  << std::endl;

		std::cout << std::flush;
	}

	// Map all frames not present in DPB as non-reference, and generate a mask of
	// all used DPB entries
	/* deUint32 destUsedDpbMask = */ ResetPicDpbSlots(refDpbUsedAndValidMask);

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
	if (refDpbUsedAndValidMask) {
		int32_t minFrameNumDiff = 0x10000;
		for (int32_t dpbIdx = 0; (deUint32)dpbIdx < numUsedRef; dpbIdx++) {
			if (!refOnlyDpbIn[dpbIdx].is_non_existing) {
				vkPicBuffBase* picBuff = refOnlyDpbIn[dpbIdx].m_picBuff;
				int8_t picIdx = GetPicIdx(picBuff); // should always be valid at this point
				DE_ASSERT(picIdx >= 0);
				// We have up to 17 internal frame buffers, but only MAX_DPB_SIZE dpb
				// entries, so we need to re-map the index from the [0..MAX_DPB_SIZE]
				// range to [0..15]
				int8_t dpbSlot = GetPicDpbSlot(picIdx);
				if (dpbSlot < 0) {
					dpbSlot = m_dpb.AllocateSlot();
					DE_ASSERT((dpbSlot >= 0) && ((deUint32)dpbSlot < m_maxNumDpbSlots));
					SetPicDpbSlot(picIdx, dpbSlot);
					m_dpb[dpbSlot].setPictureResource(picBuff, m_nCurrentPictureID);
				}
				m_dpb[dpbSlot].MarkInUse(m_nCurrentPictureID);
				DE_ASSERT(dpbSlot >= 0);

				if (dpbSlot >= 0) {
					refOnlyDpbIn[dpbIdx].dpbSlot = dpbSlot;
				} else {
					// This should never happen
					printf("DPB mapping logic broken!\n");
					DE_ASSERT(0);
				}

				int32_t frameNumDiff = ((int32_t)pd->CodecSpecific.h264.frame_num - refOnlyDpbIn[dpbIdx].FrameIdx);
				if (frameNumDiff <= 0) {
					frameNumDiff = 0xffff;
				}
				if (frameNumDiff < minFrameNumDiff) {
					bestNonExistingPicIdx = picIdx;
					minFrameNumDiff = frameNumDiff;
				} else if (bestNonExistingPicIdx == currPicIdx) {
					bestNonExistingPicIdx = picIdx;
				}
			}
		}
	}
	// In Vulkan, we always allocate a Dbp slot for the current picture,
	// regardless if it is going to become a reference or not. Non-reference slots
	// get freed right after usage. if (pd->ref_pic_flag) {
	int8_t currPicDpbSlot = AllocateDpbSlotForCurrentH264(GetPic(pd->pCurrPic),
														  currPicFlags, pd->current_dpb_id);
	DE_ASSERT(currPicDpbSlot >= 0);
	*pCurrAllocatedSlotIndex = currPicDpbSlot;

	if (refDpbUsedAndValidMask) {
		// Find or allocate slots for non existing dpb items and populate the slots.
		deUint32 dpbInUseMask = m_dpb.getSlotInUseMask();
		int8_t firstNonExistingDpbSlot = 0;
		for (deUint32 dpbIdx = 0; dpbIdx < numUsedRef; dpbIdx++) {
			int8_t dpbSlot = -1;
			int8_t picIdx = -1;
			if (refOnlyDpbIn[dpbIdx].is_non_existing) {
				DE_ASSERT(refOnlyDpbIn[dpbIdx].m_picBuff == NULL);
				while (((deUint32)firstNonExistingDpbSlot < m_maxNumDpbSlots) && (dpbSlot == -1)) {
					if (!(dpbInUseMask & (1 << firstNonExistingDpbSlot))) {
						dpbSlot = firstNonExistingDpbSlot;
					}
					firstNonExistingDpbSlot++;
				}
				DE_ASSERT((dpbSlot >= 0) && ((deUint32)dpbSlot < m_maxNumDpbSlots));
				picIdx = bestNonExistingPicIdx;
				// Find the closest valid refpic already in the DPB
				deUint32 minDiffPOC = 0x7fff;
				for (deUint32 j = 0; j < numUsedRef; j++) {
					if (!refOnlyDpbIn[j].is_non_existing && (refOnlyDpbIn[j].used_for_reference & refOnlyDpbIn[dpbIdx].used_for_reference) == refOnlyDpbIn[dpbIdx].used_for_reference) {
						deUint32 diffPOC = abs((int32_t)(refOnlyDpbIn[j].FieldOrderCnt[0] - refOnlyDpbIn[dpbIdx].FieldOrderCnt[0]));
						if (diffPOC <= minDiffPOC) {
							minDiffPOC = diffPOC;
							picIdx = GetPicIdx(refOnlyDpbIn[j].m_picBuff);
						}
					}
				}
			} else {
				DE_ASSERT(refOnlyDpbIn[dpbIdx].m_picBuff != NULL);
				dpbSlot = refOnlyDpbIn[dpbIdx].dpbSlot;
				picIdx = GetPicIdx(refOnlyDpbIn[dpbIdx].m_picBuff);
			}
			DE_ASSERT((dpbSlot >= 0) && ((deUint32)dpbSlot < m_maxNumDpbSlots));
			refOnlyDpbIn[dpbIdx].setH264PictureData(pDpbRefList, pReferenceSlots,
													dpbIdx, dpbSlot, pd->progressive_frame);
			pGopReferenceImagesIndexes[dpbIdx] = picIdx;
		}
	}

	if (videoLoggingEnabled()) {
		deUint32 slotInUseMask = m_dpb.getSlotInUseMask();
		deUint32 slotsInUseCount = 0;
		std::cout << "\tAllocated DPB slot " << (int32_t)currPicDpbSlot << " for "
				  << (pd->ref_pic_flag ? "REFERENCE" : "NON-REFERENCE")
				  << " picIdx: " << (int32_t)currPicIdx << std::endl;
		std::cout << "\tDPB frames map for picIdx: " << (int32_t)currPicIdx
				  << std::endl
				  << "\tSlot Index:\t\t";
		for (deUint32 slot = 0; slot < m_dpb.getMaxSize(); slot++) {
			if (slotInUseMask & (1 << slot)) {
				std::cout << slot << ",\t";
				slotsInUseCount++;
			} else {
				std::cout << 'X' << ",\t";
			}
		}
		std::cout << std::endl
				  << "\tPict Index:\t\t";
		for (deUint32 slot = 0; slot < m_dpb.getMaxSize(); slot++) {
			if (slotInUseMask & (1 << slot)) {
				if (m_dpb[slot].getPictureResource()) {
					std::cout << m_dpb[slot].getPictureResource()->m_picIdx << ",\t";
				} else {
					std::cout << "non existent"
							  << ",\t";
				}
			} else {
				std::cout << 'X' << ",\t";
			}
		}
		std::cout << "\n\tTotal slots in use for picIdx: " << (int32_t)currPicIdx
				  << " : " << slotsInUseCount << " out of " << m_dpb.getMaxSize()
				  << std::endl;
		std::cout << " <<<= ********************* picIdx: "
				  << (int32_t)GetPicIdx(pd->pCurrPic)
				  << " *************************" << std::endl
				  << std::endl;
		std::cout << std::flush;
	}
	return refDpbUsedAndValidMask ? numUsedRef : 0;}

deUint32 VideoBaseDecoder::FillDpbH265State (const VkParserPictureData* pd,
											 const VkParserHevcPictureData* pin,
											 nvVideoDecodeH265DpbSlotInfo* pDpbSlotInfo,
											 StdVideoDecodeH265PictureInfo* pStdPictureInfo,
											 deUint32 /*maxRefPictures*/,
											 VkVideoReferenceSlotInfoKHR* pReferenceSlots,
											 int8_t* pGopReferenceImagesIndexes,
											 int32_t* pCurrAllocatedSlotIndex)
{
	// #### Update m_dpb based on dpb parameters ####
	// Create unordered DPB and generate a bitmask of all render targets present
	// in DPB
	dpbH264Entry refOnlyDpbIn[HEVC_MAX_DPB_SLOTS];
	DE_ASSERT(m_maxNumDpbSlots <= HEVC_MAX_DPB_SLOTS);
	memset(&refOnlyDpbIn, 0, m_maxNumDpbSlots * sizeof(refOnlyDpbIn[0]));
	deUint32 refDpbUsedAndValidMask = 0;
	deUint32 numUsedRef = 0;
	if (videoLoggingEnabled())
		std::cout << "Ref frames data: " << std::endl;
	for (int32_t inIdx = 0; inIdx < HEVC_MAX_DPB_SLOTS; inIdx++) {
		// used_for_reference: 0 = unused, 1 = top_field, 2 = bottom_field, 3 =
		// both_fields
		int8_t picIdx = GetPicIdx(pin->RefPics[inIdx]);
		if (picIdx >= 0) {
			DE_ASSERT(numUsedRef < HEVC_MAX_DPB_SLOTS);
			refOnlyDpbIn[numUsedRef].setReference((pin->IsLongTerm[inIdx] == 1),
												  pin->PicOrderCntVal[inIdx],
												  GetPic(pin->RefPics[inIdx]));
			if (picIdx >= 0) {
				refDpbUsedAndValidMask |= (1 << picIdx);
			}
			refOnlyDpbIn[numUsedRef].originalDpbIndex = inIdx;
			numUsedRef++;
		}
		// Invalidate all slots.
		pReferenceSlots[inIdx].slotIndex = -1;
		pGopReferenceImagesIndexes[inIdx] = -1;
	}

	if (videoLoggingEnabled())
		std::cout << "Total Ref frames: " << numUsedRef << std::endl;

	DE_ASSERT(numUsedRef <= m_maxNumDpbSlots);
	DE_ASSERT(numUsedRef <= HEVC_MAX_DPB_SLOTS);

	// Take into account the reference picture now.
	int8_t currPicIdx = GetPicIdx(pd->pCurrPic);
	DE_ASSERT(currPicIdx >= 0);
	if (currPicIdx >= 0) {
		refDpbUsedAndValidMask |= (1 << currPicIdx);
	}

	// Map all frames not present in DPB as non-reference, and generate a mask of
	// all used DPB entries
	/* deUint32 destUsedDpbMask = */ ResetPicDpbSlots(refDpbUsedAndValidMask);

	// Now, map DPB render target indices to internal frame buffer index,
	// assign each reference a unique DPB entry, and create the ordered DPB
	// This is an undocumented MV restriction: the position in the DPB is stored
	// along with the co-located data, so once a reference frame is assigned a DPB
	// entry, it can no longer change.

	int8_t frmListToDpb[HEVC_MAX_DPB_SLOTS];
	// TODO change to -1 for invalid indexes.
	memset(&frmListToDpb, 0, sizeof(frmListToDpb));
	// Find or allocate slots for existing dpb items.
	for (int32_t dpbIdx = 0; (deUint32)dpbIdx < numUsedRef; dpbIdx++) {
		if (!refOnlyDpbIn[dpbIdx].is_non_existing) {
			vkPicBuffBase* picBuff = refOnlyDpbIn[dpbIdx].m_picBuff;
			int32_t picIdx = GetPicIdx(picBuff); // should always be valid at this point
			DE_ASSERT(picIdx >= 0);
			// We have up to 17 internal frame buffers, but only HEVC_MAX_DPB_SLOTS
			// dpb entries, so we need to re-map the index from the
			// [0..HEVC_MAX_DPB_SLOTS] range to [0..15]
			int8_t dpbSlot = GetPicDpbSlot(picIdx);
			if (dpbSlot < 0) {
				dpbSlot = m_dpb.AllocateSlot();
				DE_ASSERT(dpbSlot >= 0);
				SetPicDpbSlot(picIdx, dpbSlot);
				m_dpb[dpbSlot].setPictureResource(picBuff, m_nCurrentPictureID);
			}
			m_dpb[dpbSlot].MarkInUse(m_nCurrentPictureID);
			DE_ASSERT(dpbSlot >= 0);

			if (dpbSlot >= 0) {
				refOnlyDpbIn[dpbIdx].dpbSlot = dpbSlot;
				deUint32 originalDpbIndex = refOnlyDpbIn[dpbIdx].originalDpbIndex;
				DE_ASSERT(originalDpbIndex < HEVC_MAX_DPB_SLOTS);
				frmListToDpb[originalDpbIndex] = dpbSlot;
			} else {
				// This should never happen
				printf("DPB mapping logic broken!\n");
				DE_ASSERT(0);
			}
		}
	}

	// Find or allocate slots for non existing dpb items and populate the slots.
	deUint32 dpbInUseMask = m_dpb.getSlotInUseMask();
	int8_t firstNonExistingDpbSlot = 0;
	for (deUint32 dpbIdx = 0; dpbIdx < numUsedRef; dpbIdx++) {
		int8_t dpbSlot = -1;
		if (refOnlyDpbIn[dpbIdx].is_non_existing) {
			// There shouldn't be  not_existing in h.265
			DE_ASSERT(0);
			DE_ASSERT(refOnlyDpbIn[dpbIdx].m_picBuff == NULL);
			while (((deUint32)firstNonExistingDpbSlot < m_maxNumDpbSlots) && (dpbSlot == -1)) {
				if (!(dpbInUseMask & (1 << firstNonExistingDpbSlot))) {
					dpbSlot = firstNonExistingDpbSlot;
				}
				firstNonExistingDpbSlot++;
			}
			DE_ASSERT((dpbSlot >= 0) && ((deUint32)dpbSlot < m_maxNumDpbSlots));
		} else {
			DE_ASSERT(refOnlyDpbIn[dpbIdx].m_picBuff != NULL);
			dpbSlot = refOnlyDpbIn[dpbIdx].dpbSlot;
		}
		DE_ASSERT((dpbSlot >= 0) && (dpbSlot < HEVC_MAX_DPB_SLOTS));
		refOnlyDpbIn[dpbIdx].setH265PictureData(pDpbSlotInfo, pReferenceSlots,
												dpbIdx, dpbSlot);
		pGopReferenceImagesIndexes[dpbIdx] = GetPicIdx(refOnlyDpbIn[dpbIdx].m_picBuff);
	}

	if (videoLoggingEnabled()) {
		std::cout << "frmListToDpb:" << std::endl;
		for (int8_t dpbResIdx = 0; dpbResIdx < HEVC_MAX_DPB_SLOTS; dpbResIdx++) {
			std::cout << "\tfrmListToDpb[" << (int32_t)dpbResIdx << "] is "
					  << (int32_t)frmListToDpb[dpbResIdx] << std::endl;
		}
	}

	int32_t numPocStCurrBefore = 0;
	const size_t maxNumPocStCurrBefore = sizeof(pStdPictureInfo->RefPicSetStCurrBefore) / sizeof(pStdPictureInfo->RefPicSetStCurrBefore[0]);
	DE_ASSERT((size_t)pin->NumPocStCurrBefore <= maxNumPocStCurrBefore);
	if ((size_t)pin->NumPocStCurrBefore > maxNumPocStCurrBefore) {
		tcu::print("\nERROR: FillDpbH265State() pin->NumPocStCurrBefore(%d) must be smaller than maxNumPocStCurrBefore(%zd)\n", pin->NumPocStCurrBefore, maxNumPocStCurrBefore);
	}
	for (int32_t i = 0; i < pin->NumPocStCurrBefore; i++) {
		deUint8 idx = (deUint8)pin->RefPicSetStCurrBefore[i];
		if (idx < HEVC_MAX_DPB_SLOTS) {
			if (videoLoggingEnabled())
				std::cout << "\trefPicSetStCurrBefore[" << i << "] is " << (int32_t)idx
						  << " -> " << (int32_t)frmListToDpb[idx] << std::endl;
			pStdPictureInfo->RefPicSetStCurrBefore[numPocStCurrBefore++] = frmListToDpb[idx] & 0xf;
		}
	}
	while (numPocStCurrBefore < 8) {
		pStdPictureInfo->RefPicSetStCurrBefore[numPocStCurrBefore++] = 0xff;
	}

	int32_t numPocStCurrAfter = 0;
	const size_t maxNumPocStCurrAfter = sizeof(pStdPictureInfo->RefPicSetStCurrAfter) / sizeof(pStdPictureInfo->RefPicSetStCurrAfter[0]);
	DE_ASSERT((size_t)pin->NumPocStCurrAfter <= maxNumPocStCurrAfter);
	if ((size_t)pin->NumPocStCurrAfter > maxNumPocStCurrAfter) {
		fprintf(stderr, "\nERROR: FillDpbH265State() pin->NumPocStCurrAfter(%d) must be smaller than maxNumPocStCurrAfter(%zd)\n", pin->NumPocStCurrAfter, maxNumPocStCurrAfter);
	}
	for (int32_t i = 0; i < pin->NumPocStCurrAfter; i++) {
		deUint8 idx = (deUint8)pin->RefPicSetStCurrAfter[i];
		if (idx < HEVC_MAX_DPB_SLOTS) {
			if (videoLoggingEnabled())
				std::cout << "\trefPicSetStCurrAfter[" << i << "] is " << (int32_t)idx
						  << " -> " << (int32_t)frmListToDpb[idx] << std::endl;
			pStdPictureInfo->RefPicSetStCurrAfter[numPocStCurrAfter++] = frmListToDpb[idx] & 0xf;
		}
	}
	while (numPocStCurrAfter < 8) {
		pStdPictureInfo->RefPicSetStCurrAfter[numPocStCurrAfter++] = 0xff;
	}

	int32_t numPocLtCurr = 0;
	const size_t maxNumPocLtCurr = sizeof(pStdPictureInfo->RefPicSetLtCurr) / sizeof(pStdPictureInfo->RefPicSetLtCurr[0]);
	DE_ASSERT((size_t)pin->NumPocLtCurr <= maxNumPocLtCurr);
	if ((size_t)pin->NumPocLtCurr > maxNumPocLtCurr) {
		fprintf(stderr, "\nERROR: FillDpbH265State() pin->NumPocLtCurr(%d) must be smaller than maxNumPocLtCurr(%zd)\n", pin->NumPocLtCurr, maxNumPocLtCurr);
	}
	for (int32_t i = 0; i < pin->NumPocLtCurr; i++) {
		deUint8 idx = (deUint8)pin->RefPicSetLtCurr[i];
		if (idx < HEVC_MAX_DPB_SLOTS) {
			if (videoLoggingEnabled())
				std::cout << "\trefPicSetLtCurr[" << i << "] is " << (int32_t)idx
						  << " -> " << (int32_t)frmListToDpb[idx] << std::endl;
			pStdPictureInfo->RefPicSetLtCurr[numPocLtCurr++] = frmListToDpb[idx] & 0xf;
		}
	}
	while (numPocLtCurr < 8) {
		pStdPictureInfo->RefPicSetLtCurr[numPocLtCurr++] = 0xff;
	}

	for (int32_t i = 0; i < 8; i++) {
		if (videoLoggingEnabled())
			std::cout << "\tlist indx " << i << ": "
					  << " refPicSetStCurrBefore: "
					  << (int32_t)pStdPictureInfo->RefPicSetStCurrBefore[i]
					  << " refPicSetStCurrAfter: "
					  << (int32_t)pStdPictureInfo->RefPicSetStCurrAfter[i]
					  << " refPicSetLtCurr: "
					  << (int32_t)pStdPictureInfo->RefPicSetLtCurr[i] << std::endl;
	}

	int8_t dpbSlot = AllocateDpbSlotForCurrentH265(GetPic(pd->pCurrPic),
												   true /* isReference */, pd->current_dpb_id);
	*pCurrAllocatedSlotIndex = dpbSlot;
	DE_ASSERT(!(dpbSlot < 0));
	if (dpbSlot >= 0) {
		DE_ASSERT(pd->ref_pic_flag);
	}

	return numUsedRef;
}

int8_t VideoBaseDecoder::AllocateDpbSlotForCurrentH264 (vkPicBuffBase* pPic, StdVideoDecodeH264PictureInfoFlags currPicFlags,
														int8_t /*presetDpbSlot*/)
{
	// Now, map the current render target
	int8_t dpbSlot = -1;
	int8_t currPicIdx = GetPicIdx(pPic);
	DE_ASSERT(currPicIdx >= 0);
	SetFieldPicFlag(currPicIdx, currPicFlags.field_pic_flag);
	// In Vulkan we always allocate reference slot for the current picture.
	if (true /* currPicFlags.is_reference */) {
		dpbSlot = GetPicDpbSlot(currPicIdx);
		if (dpbSlot < 0) {
			dpbSlot = m_dpb.AllocateSlot();
			DE_ASSERT(dpbSlot >= 0);
			SetPicDpbSlot(currPicIdx, dpbSlot);
			m_dpb[dpbSlot].setPictureResource(pPic, m_nCurrentPictureID);
		}
		DE_ASSERT(dpbSlot >= 0);
	}
	return dpbSlot;
}

int8_t VideoBaseDecoder::AllocateDpbSlotForCurrentH265 (vkPicBuffBase* pPic,
														bool isReference, int8_t /*presetDpbSlot*/)
{
	// Now, map the current render target
	int8_t dpbSlot = -1;
	int8_t currPicIdx = GetPicIdx(pPic);
	DE_ASSERT(currPicIdx >= 0);
	DE_ASSERT(isReference);
	if (isReference) {
		dpbSlot = GetPicDpbSlot(currPicIdx);
		if (dpbSlot < 0) {
			dpbSlot = m_dpb.AllocateSlot();
			DE_ASSERT(dpbSlot >= 0);
			SetPicDpbSlot(currPicIdx, dpbSlot);
			m_dpb[dpbSlot].setPictureResource(pPic, m_nCurrentPictureID);
		}
		DE_ASSERT(dpbSlot >= 0);
	}
	return dpbSlot;
}

VkFormat getRecommendedFormat (const vector<VkFormat>& formats, VkFormat recommendedFormat)
{
	if (formats.empty())
		return VK_FORMAT_UNDEFINED;
	else if (recommendedFormat != VK_FORMAT_UNDEFINED && std::find(formats.begin(), formats.end(), recommendedFormat) != formats.end())
		return recommendedFormat;
	else
		return formats[0];
}

VkResult VulkanVideoSession::Create(DeviceContext& vkDevCtx,
								   deUint32            videoQueueFamily,
								   VkVideoCoreProfile* pVideoProfile,
								   VkFormat            pictureFormat,
								   const VkExtent2D&   maxCodedExtent,
								   VkFormat            referencePicturesFormat,
								   deUint32            maxDpbSlots,
								   deUint32            maxActiveReferencePictures,
								   VkSharedBaseObj<VulkanVideoSession>& videoSession)
{
	auto& vk = vkDevCtx.getDeviceDriver();
	auto device = vkDevCtx.device;

	VulkanVideoSession* pNewVideoSession = new VulkanVideoSession(vkDevCtx, pVideoProfile);

	static const VkExtensionProperties h264DecodeStdExtensionVersion = { VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_SPEC_VERSION };
	static const VkExtensionProperties h265DecodeStdExtensionVersion = { VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_SPEC_VERSION };
	static const VkExtensionProperties h264EncodeStdExtensionVersion = { VK_STD_VULKAN_VIDEO_CODEC_H264_ENCODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H264_ENCODE_SPEC_VERSION };
	static const VkExtensionProperties h265EncodeStdExtensionVersion = { VK_STD_VULKAN_VIDEO_CODEC_H265_ENCODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H265_ENCODE_SPEC_VERSION };

	VkVideoSessionCreateInfoKHR& createInfo = pNewVideoSession->m_createInfo;
	createInfo.flags = 0;
	createInfo.pVideoProfile = pVideoProfile->GetProfile();
	createInfo.queueFamilyIndex = videoQueueFamily;
	createInfo.pictureFormat = pictureFormat;
	createInfo.maxCodedExtent = maxCodedExtent;
	createInfo.maxDpbSlots = maxDpbSlots;
	createInfo.maxActiveReferencePictures = maxActiveReferencePictures;
	createInfo.referencePictureFormat = referencePicturesFormat;

	switch ((int32_t)pVideoProfile->GetCodecType()) {
		case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
			createInfo.pStdHeaderVersion = &h264DecodeStdExtensionVersion;
			break;
		case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
			createInfo.pStdHeaderVersion = &h265DecodeStdExtensionVersion;
			break;
		case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_EXT:
			createInfo.pStdHeaderVersion = &h264EncodeStdExtensionVersion;
			break;
		case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_EXT:
			createInfo.pStdHeaderVersion = &h265EncodeStdExtensionVersion;
			break;
		default:
			DE_ASSERT(0);
	}
	VkResult result = vk.createVideoSessionKHR(device, &createInfo, NULL, &pNewVideoSession->m_videoSession);
	if (result != VK_SUCCESS) {
		return result;
	}

	deUint32 videoSessionMemoryRequirementsCount = 0;
	VkVideoSessionMemoryRequirementsKHR decodeSessionMemoryRequirements[MAX_BOUND_MEMORY];
	// Get the count first
	result = vk.getVideoSessionMemoryRequirementsKHR(device, pNewVideoSession->m_videoSession,
															&videoSessionMemoryRequirementsCount, NULL);
	DE_ASSERT(result == VK_SUCCESS);
	DE_ASSERT(videoSessionMemoryRequirementsCount <= MAX_BOUND_MEMORY);

	memset(decodeSessionMemoryRequirements, 0x00, sizeof(decodeSessionMemoryRequirements));
	for (deUint32 i = 0; i < videoSessionMemoryRequirementsCount; i++) {
		decodeSessionMemoryRequirements[i].sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR;
	}

	result = vk.getVideoSessionMemoryRequirementsKHR(device, pNewVideoSession->m_videoSession,
															&videoSessionMemoryRequirementsCount,
															decodeSessionMemoryRequirements);
	if (result != VK_SUCCESS) {
		return result;
	}

	deUint32 decodeSessionBindMemoryCount = videoSessionMemoryRequirementsCount;
	VkBindVideoSessionMemoryInfoKHR decodeSessionBindMemory[MAX_BOUND_MEMORY];

	for (deUint32 memIdx = 0; memIdx < decodeSessionBindMemoryCount; memIdx++) {

		deUint32 memoryTypeIndex = 0;
		deUint32 memoryTypeBits = decodeSessionMemoryRequirements[memIdx].memoryRequirements.memoryTypeBits;
		if (memoryTypeBits == 0) {
			return VK_ERROR_INITIALIZATION_FAILED;
		}

		// Find an available memory type that satisfies the requested properties.
		for (; !(memoryTypeBits & 1); memoryTypeIndex++  ) {
			memoryTypeBits >>= 1;
		}

		VkMemoryAllocateInfo memInfo = {
				VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,                          // sType
				NULL,                                                            // pNext
				decodeSessionMemoryRequirements[memIdx].memoryRequirements.size, // allocationSize
				memoryTypeIndex,                                                 // memoryTypeIndex
		};

		result = vk.allocateMemory(device, &memInfo, 0,
										  &pNewVideoSession->m_memoryBound[memIdx]);
		if (result != VK_SUCCESS) {
			return result;
		}

		DE_ASSERT(result == VK_SUCCESS);
		decodeSessionBindMemory[memIdx].pNext = NULL;
		decodeSessionBindMemory[memIdx].sType = VK_STRUCTURE_TYPE_BIND_VIDEO_SESSION_MEMORY_INFO_KHR;
		decodeSessionBindMemory[memIdx].memory = pNewVideoSession->m_memoryBound[memIdx];

		decodeSessionBindMemory[memIdx].memoryBindIndex = decodeSessionMemoryRequirements[memIdx].memoryBindIndex;
		decodeSessionBindMemory[memIdx].memoryOffset = 0;
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






VkResult VkImageResource::Create(DeviceContext&					   vkDevCtx,
								 const VkImageCreateInfo*		   pImageCreateInfo,
								 VkSharedBaseObj<VkImageResource>& imageResource)
{
	imageResource = new VkImageResource(vkDevCtx,
										pImageCreateInfo);

	return VK_SUCCESS;
}

VkResult VkImageResourceView::Create(DeviceContext& vkDevCtx,
									 VkSharedBaseObj<VkImageResource>& imageResource,
									 VkImageSubresourceRange &imageSubresourceRange,
									 VkSharedBaseObj<VkImageResourceView>& imageResourceView)
{
	auto& vk = vkDevCtx.getDeviceDriver();
	VkDevice device = vkDevCtx.device;
	VkImageView  imageView;
	VkImageViewCreateInfo viewInfo = VkImageViewCreateInfo();
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.pNext = nullptr;
	viewInfo.image = imageResource->GetImage();
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = imageResource->GetImageCreateInfo().format;
	viewInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
							VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
	viewInfo.subresourceRange = imageSubresourceRange;
	viewInfo.flags = 0;
	VkResult result = vk.createImageView(device, &viewInfo, nullptr, &imageView);
	if (result != VK_SUCCESS) {
		return result;
	}

	imageResourceView = new VkImageResourceView(vkDevCtx, imageResource,
												imageView, imageSubresourceRange);

	return result;
}

VkImageResourceView::~VkImageResourceView()
{
	auto& vk = m_vkDevCtx.getDeviceDriver();
	auto device = m_vkDevCtx.device;

	if (m_imageView != VK_NULL_HANDLE) {
		vk.destroyImageView(device, m_imageView, nullptr);
		m_imageView = VK_NULL_HANDLE;
	}

	m_imageResource = nullptr;
}


const char* VkParserVideoPictureParameters::m_refClassId = "VkParserVideoPictureParameters";
int32_t VkParserVideoPictureParameters::m_currentId = 0;

int32_t VkParserVideoPictureParameters::PopulateH264UpdateFields(const StdVideoPictureParametersSet* pStdPictureParametersSet,
																 VkVideoDecodeH264SessionParametersAddInfoKHR& h264SessionParametersAddInfo)
{
	int32_t currentId = -1;
	if (pStdPictureParametersSet == nullptr) {
		return currentId;
	}

	DE_ASSERT( (pStdPictureParametersSet->GetStdType() == StdVideoPictureParametersSet::TYPE_H264_SPS) ||
			(pStdPictureParametersSet->GetStdType() == StdVideoPictureParametersSet::TYPE_H264_PPS));

	DE_ASSERT(h264SessionParametersAddInfo.sType == VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_ADD_INFO_KHR);

	if (pStdPictureParametersSet->GetStdType() == StdVideoPictureParametersSet::TYPE_H264_SPS) {
		h264SessionParametersAddInfo.stdSPSCount = 1;
		h264SessionParametersAddInfo.pStdSPSs = pStdPictureParametersSet->GetStdH264Sps();
		bool isSps = false;
		currentId = pStdPictureParametersSet->GetSpsId(isSps);
		DE_ASSERT(isSps);
	} else if (pStdPictureParametersSet->GetStdType() ==  StdVideoPictureParametersSet::TYPE_H264_PPS ) {
		h264SessionParametersAddInfo.stdPPSCount = 1;
		h264SessionParametersAddInfo.pStdPPSs = pStdPictureParametersSet->GetStdH264Pps();
		bool isPps = false;
		currentId = pStdPictureParametersSet->GetPpsId(isPps);
		DE_ASSERT(isPps);
	} else {
		DE_ASSERT(!"Incorrect h.264 type");
	}

	return currentId;
}

int32_t VkParserVideoPictureParameters::PopulateH265UpdateFields(const StdVideoPictureParametersSet* pStdPictureParametersSet,
																 VkVideoDecodeH265SessionParametersAddInfoKHR& h265SessionParametersAddInfo)
{
	int32_t currentId = -1;
	if (pStdPictureParametersSet == nullptr) {
		return currentId;
	}

	DE_ASSERT( (pStdPictureParametersSet->GetStdType() == StdVideoPictureParametersSet::TYPE_H265_VPS) ||
			(pStdPictureParametersSet->GetStdType() == StdVideoPictureParametersSet::TYPE_H265_SPS) ||
			(pStdPictureParametersSet->GetStdType() == StdVideoPictureParametersSet::TYPE_H265_PPS));

	DE_ASSERT(h265SessionParametersAddInfo.sType == VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_SESSION_PARAMETERS_ADD_INFO_KHR);

	if (pStdPictureParametersSet->GetStdType() == StdVideoPictureParametersSet::TYPE_H265_VPS) {
		h265SessionParametersAddInfo.stdVPSCount = 1;
		h265SessionParametersAddInfo.pStdVPSs = pStdPictureParametersSet->GetStdH265Vps();
		bool isVps = false;
		currentId = pStdPictureParametersSet->GetVpsId(isVps);
		DE_ASSERT(isVps);
	} else if (pStdPictureParametersSet->GetStdType() == StdVideoPictureParametersSet::TYPE_H265_SPS) {
		h265SessionParametersAddInfo.stdSPSCount = 1;
		h265SessionParametersAddInfo.pStdSPSs = pStdPictureParametersSet->GetStdH265Sps();
		bool isSps = false;
		currentId = pStdPictureParametersSet->GetSpsId(isSps);
		DE_ASSERT(isSps);
	} else if (pStdPictureParametersSet->GetStdType() == StdVideoPictureParametersSet::TYPE_H265_PPS) {
		h265SessionParametersAddInfo.stdPPSCount = 1;
		h265SessionParametersAddInfo.pStdPPSs = pStdPictureParametersSet->GetStdH265Pps();
		bool isPps = false;
		currentId = pStdPictureParametersSet->GetPpsId(isPps);
		DE_ASSERT(isPps);
	} else {
		DE_ASSERT(!"Incorrect h.265 type");
	}

	return currentId;
}

VkResult
VkParserVideoPictureParameters::Create(DeviceContext& deviceContext,
									   VkSharedBaseObj<VkParserVideoPictureParameters>& templatePictureParameters,
									   VkSharedBaseObj<VkParserVideoPictureParameters>& videoPictureParameters)
{
	VkSharedBaseObj<VkParserVideoPictureParameters> newVideoPictureParameters(
			new VkParserVideoPictureParameters(deviceContext, templatePictureParameters));
	if (!newVideoPictureParameters) {
		return VK_ERROR_OUT_OF_HOST_MEMORY;
	}

	videoPictureParameters = newVideoPictureParameters;
	return VK_SUCCESS;
}

VkResult VkParserVideoPictureParameters::CreateParametersObject(VkSharedBaseObj<VulkanVideoSession>& videoSession,
																const StdVideoPictureParametersSet* pStdVideoPictureParametersSet,
																VkParserVideoPictureParameters* pTemplatePictureParameters)
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

	StdVideoPictureParametersSet::StdType updateType = pStdVideoPictureParametersSet->GetStdType();
	switch (updateType)
	{
		case StdVideoPictureParametersSet::TYPE_H264_SPS:
		case StdVideoPictureParametersSet::TYPE_H264_PPS:
		{
			createInfo.pNext =  &h264SessionParametersCreateInfo;
			h264SessionParametersCreateInfo.maxStdSPSCount = MAX_SPS_IDS;
			h264SessionParametersCreateInfo.maxStdPPSCount = MAX_PPS_IDS;
			h264SessionParametersCreateInfo.pParametersAddInfo = &h264SessionParametersAddInfo;

			currentId = PopulateH264UpdateFields(pStdVideoPictureParametersSet, h264SessionParametersAddInfo);

		}
			break;
		case StdVideoPictureParametersSet::TYPE_H265_VPS:
		case StdVideoPictureParametersSet::TYPE_H265_SPS:
		case StdVideoPictureParametersSet::TYPE_H265_PPS:
		{
			createInfo.pNext =  &h265SessionParametersCreateInfo;
			h265SessionParametersCreateInfo.maxStdVPSCount = MAX_VPS_IDS;
			h265SessionParametersCreateInfo.maxStdSPSCount = MAX_SPS_IDS;
			h265SessionParametersCreateInfo.maxStdPPSCount = MAX_PPS_IDS;
			h265SessionParametersCreateInfo.pParametersAddInfo = &h265SessionParametersAddInfo;

			currentId = PopulateH265UpdateFields(pStdVideoPictureParametersSet, h265SessionParametersAddInfo);
		}
			break;
		default:
			DE_ASSERT(!"Invalid Parser format");
			return VK_ERROR_INITIALIZATION_FAILED;
	}

	createInfo.videoSessionParametersTemplate = pTemplatePictureParameters ? VkVideoSessionParametersKHR(*pTemplatePictureParameters) : VK_NULL_HANDLE;
	createInfo.videoSession = videoSession->GetVideoSession();
	VkResult result = m_deviceContext.getDeviceDriver().createVideoSessionParametersKHR(m_deviceContext.device,
																&createInfo,
																nullptr,
																&m_sessionParameters);

	if (result != VK_SUCCESS) {

		DE_ASSERT(!"Could not create Session Parameters Object");
		return result;

	} else {

		m_videoSession = videoSession;

		if (pTemplatePictureParameters) {
			m_vpsIdsUsed = pTemplatePictureParameters->m_vpsIdsUsed;
			m_spsIdsUsed = pTemplatePictureParameters->m_spsIdsUsed;
			m_ppsIdsUsed = pTemplatePictureParameters->m_ppsIdsUsed;
		}

		assert (currentId >= 0);
		switch (pStdVideoPictureParametersSet->GetParameterType()) {
			case StdVideoPictureParametersSet::PPS_TYPE:
				m_ppsIdsUsed.set(currentId, true);
				break;

			case StdVideoPictureParametersSet::SPS_TYPE:
				m_spsIdsUsed.set(currentId, true);
				break;

			case StdVideoPictureParametersSet::VPS_TYPE:
				m_vpsIdsUsed.set(currentId, true);
				break;
			default:
				DE_ASSERT(!"Invalid StdVideoPictureParametersSet Parameter Type!");
		}
		m_Id = ++m_currentId;
	}

	return result;
}

VkResult VkParserVideoPictureParameters::UpdateParametersObject(StdVideoPictureParametersSet* pStdVideoPictureParametersSet)
{
	if (pStdVideoPictureParametersSet == nullptr) {
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
			currentId = PopulateH264UpdateFields(pStdVideoPictureParametersSet, h264SessionParametersAddInfo);
		}
			break;
		case StdVideoPictureParametersSet::TYPE_H265_VPS:
		case StdVideoPictureParametersSet::TYPE_H265_SPS:
		case StdVideoPictureParametersSet::TYPE_H265_PPS:
		{
			updateInfo.pNext = &h265SessionParametersAddInfo;
			currentId = PopulateH265UpdateFields(pStdVideoPictureParametersSet, h265SessionParametersAddInfo);
		}
			break;
		default:
			DE_ASSERT(!"Invalid Parser format");
			return VK_ERROR_INITIALIZATION_FAILED;
	}

	updateInfo.updateSequenceCount = ++m_updateCount;
	VK_CHECK(m_deviceContext.getDeviceDriver().updateVideoSessionParametersKHR(m_deviceContext.device,
																			   m_sessionParameters,
																			   &updateInfo));


	DE_ASSERT(currentId >= 0);
	switch (pStdVideoPictureParametersSet->GetParameterType()) {
		case StdVideoPictureParametersSet::PPS_TYPE:
			m_ppsIdsUsed.set(currentId, true);
			break;

		case StdVideoPictureParametersSet::SPS_TYPE:
			m_spsIdsUsed.set(currentId, true);
			break;

		case StdVideoPictureParametersSet::VPS_TYPE:
			m_vpsIdsUsed.set(currentId, true);
			break;
		default:
			DE_ASSERT(!"Invalid StdVideoPictureParametersSet Parameter Type!");
	}

	return VK_SUCCESS;
}

VkParserVideoPictureParameters::~VkParserVideoPictureParameters()
{
	if (!!m_sessionParameters) {
		m_deviceContext.getDeviceDriver().destroyVideoSessionParametersKHR(m_deviceContext.device, m_sessionParameters, nullptr);
		m_sessionParameters = VK_NULL_HANDLE;
	}
	m_videoSession = nullptr;
}

bool VkParserVideoPictureParameters::UpdatePictureParametersHierarchy(
		VkSharedBaseObj<StdVideoPictureParametersSet>& pictureParametersObject)
{
	int32_t nodeId = -1;
	bool isNodeId = false;
	StdVideoPictureParametersSet::ParameterType nodeParent = StdVideoPictureParametersSet::INVALID_TYPE;
	StdVideoPictureParametersSet::ParameterType nodeChild = StdVideoPictureParametersSet::INVALID_TYPE;
	switch (pictureParametersObject->GetParameterType()) {
		case StdVideoPictureParametersSet::PPS_TYPE:
			nodeParent = StdVideoPictureParametersSet::SPS_TYPE;
			nodeId = pictureParametersObject->GetPpsId(isNodeId);
			if (!((deUint32)nodeId < VkParserVideoPictureParameters::MAX_PPS_IDS)) {
				DE_ASSERT(!"PPS ID is out of bounds");
				return false;
			}
			DE_ASSERT(isNodeId);
			if (m_lastPictParamsQueue[nodeParent]) {
				bool isParentId = false;
				const int32_t spsParentId = pictureParametersObject->GetSpsId(isParentId);
				DE_ASSERT(!isParentId);
				if (spsParentId == m_lastPictParamsQueue[nodeParent]->GetSpsId(isParentId)) {
					DE_ASSERT(isParentId);
					pictureParametersObject->m_parent = m_lastPictParamsQueue[nodeParent];
				}
			}
			break;
		case StdVideoPictureParametersSet::SPS_TYPE:
			nodeParent = StdVideoPictureParametersSet::VPS_TYPE;
			nodeChild = StdVideoPictureParametersSet::PPS_TYPE;
			nodeId = pictureParametersObject->GetSpsId(isNodeId);
			if (!((deUint32)nodeId < VkParserVideoPictureParameters::MAX_SPS_IDS)) {
				DE_ASSERT(!"SPS ID is out of bounds");
				return false;
			}
			DE_ASSERT(isNodeId);
			if (m_lastPictParamsQueue[nodeChild]) {
				const int32_t spsChildId = m_lastPictParamsQueue[nodeChild]->GetSpsId(isNodeId);
				DE_ASSERT(!isNodeId);
				if (spsChildId == nodeId) {
					m_lastPictParamsQueue[nodeChild]->m_parent = pictureParametersObject;
				}
			}
			if (m_lastPictParamsQueue[nodeParent]) {
				const int32_t vpsParentId = pictureParametersObject->GetVpsId(isNodeId);
				DE_ASSERT(!isNodeId);
				if (vpsParentId == m_lastPictParamsQueue[nodeParent]->GetVpsId(isNodeId)) {
					pictureParametersObject->m_parent = m_lastPictParamsQueue[nodeParent];
					DE_ASSERT(isNodeId);
				}
			}
			break;
		case StdVideoPictureParametersSet::VPS_TYPE:
			nodeChild = StdVideoPictureParametersSet::SPS_TYPE;
			nodeId = pictureParametersObject->GetVpsId(isNodeId);
			if (!((deUint32)nodeId < VkParserVideoPictureParameters::MAX_VPS_IDS)) {
				DE_ASSERT(!"VPS ID is out of bounds");
				return false;
			}
			DE_ASSERT(isNodeId);
			if (m_lastPictParamsQueue[nodeChild]) {
				const int32_t vpsParentId = m_lastPictParamsQueue[nodeChild]->GetVpsId(isNodeId);
				DE_ASSERT(!isNodeId);
				if (vpsParentId == nodeId) {
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

VkResult VkParserVideoPictureParameters::AddPictureParametersToQueue(VkSharedBaseObj<StdVideoPictureParametersSet>& pictureParametersSet)
{
	m_pictureParametersQueue.push(pictureParametersSet);
	return VK_SUCCESS;
}

VkResult VkParserVideoPictureParameters::HandleNewPictureParametersSet(VkSharedBaseObj<VulkanVideoSession>& videoSession,
																	   StdVideoPictureParametersSet* pStdVideoPictureParametersSet)
{
	VkResult result;
	if (m_sessionParameters == VK_NULL_HANDLE) {
		DE_ASSERT(videoSession != VK_NULL_HANDLE);
		DE_ASSERT(m_videoSession == VK_NULL_HANDLE);
		if (m_templatePictureParameters) {
			m_templatePictureParameters->FlushPictureParametersQueue(videoSession);
		}
		result = CreateParametersObject(videoSession, pStdVideoPictureParametersSet,
										m_templatePictureParameters);
		DE_ASSERT(result == VK_SUCCESS);
		m_templatePictureParameters = nullptr; // the template object is not needed anymore
		m_videoSession = videoSession;

	} else {
		DE_ASSERT(m_videoSession != VK_NULL_HANDLE);
		DE_ASSERT(m_sessionParameters != VK_NULL_HANDLE);
		result = UpdateParametersObject(pStdVideoPictureParametersSet);
		DE_ASSERT(result == VK_SUCCESS);
	}

	return result;
}


int32_t VkParserVideoPictureParameters::FlushPictureParametersQueue(VkSharedBaseObj<VulkanVideoSession>& videoSession)
{
	if (!videoSession) {
		return -1;
	}
	deUint32 numQueueItems = 0;
	while (!m_pictureParametersQueue.empty()) {
		VkSharedBaseObj<StdVideoPictureParametersSet>& stdVideoPictureParametersSet = m_pictureParametersQueue.front();

		VkResult result =  HandleNewPictureParametersSet(videoSession, stdVideoPictureParametersSet);
		if (result != VK_SUCCESS) {
			return -1;
		}

		m_pictureParametersQueue.pop();
		numQueueItems++;
	}

	return numQueueItems;
}

bool VkParserVideoPictureParameters::CheckStdObjectBeforeUpdate(VkSharedBaseObj<StdVideoPictureParametersSet>& stdPictureParametersSet,
																VkSharedBaseObj<VkParserVideoPictureParameters>& currentVideoPictureParameters)
{
	if (!stdPictureParametersSet) {
		return false;
	}

	bool stdObjectUpdate = (stdPictureParametersSet->GetUpdateSequenceCount() > 0);

	if (!currentVideoPictureParameters || stdObjectUpdate) {

		// Create new Vulkan Picture Parameters object
		return true;

	} else { // existing VkParserVideoPictureParameters object
		DE_ASSERT(currentVideoPictureParameters);
		// Update with the existing Vulkan Picture Parameters object
	}

	VkSharedBaseObj<VkVideoRefCountBase> clientObject;
	stdPictureParametersSet->GetClientObject(clientObject);
	DE_ASSERT(!clientObject);

	return false;
}

VkResult
VkParserVideoPictureParameters::AddPictureParameters(DeviceContext& deviceContext,
													 VkSharedBaseObj<VulkanVideoSession>& /*videoSession*/,
													 VkSharedBaseObj<StdVideoPictureParametersSet>& stdPictureParametersSet,
													 VkSharedBaseObj<VkParserVideoPictureParameters>& currentVideoPictureParameters)
{
	if (!stdPictureParametersSet) {
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	VkResult result;
	if (CheckStdObjectBeforeUpdate(stdPictureParametersSet, currentVideoPictureParameters)) {
		result = VkParserVideoPictureParameters::Create(deviceContext,
														currentVideoPictureParameters,
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
	deUint32 ret;
	ret = --m_refCount;
	// Destroy the device if refcount reaches zero
	if (ret == 0) {
		delete this;
	}
	return ret;
}

}	// video
}	// vkt
