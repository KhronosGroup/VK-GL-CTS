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
#include "vkRef.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkStrUtil.hpp"
#include "deSTLUtil.hpp"
#include "deRandom.hpp"

#include <iostream>

namespace vkt
{
namespace video
{
using namespace vk;
using namespace std;
using de::MovePtr;
using de::SharedPtr;

static const uint32_t	topFieldShift			= 0;
static const uint32_t	topFieldMask			= (1 << topFieldShift);
static const uint32_t	bottomFieldShift		= 1;
static const uint32_t	bottomFieldMask			= (1 << bottomFieldShift);
static const uint32_t	fieldIsReferenceMask	= (topFieldMask | bottomFieldMask);
static const uint32_t	EXTRA_DPB_SLOTS			= 1;
static const uint32_t	MAX_DPB_SLOTS_PLUS_1	= 16 + EXTRA_DPB_SLOTS;

#define HEVC_MAX_DPB_SLOTS			16
#define AVC_MAX_DPB_SLOTS			17

#define NVIDIA_FRAME_RATE_NUM(rate)	((rate) >> 14)
#define NVIDIA_FRAME_RATE_DEN(rate)	((rate)&0x3fff)

template<typename T>
inline const T* dataOrNullPtr (const std::vector<T>& v)
{
	return (v.empty() ? DE_NULL : &v[0]);
}

template<typename T>
inline T* dataOrNullPtr (std::vector<T>& v)
{
	return (v.empty() ? DE_NULL : &v[0]);
}

template<typename T>
inline T& incSizeSafe (std::vector<T>& v)
{
	DE_ASSERT(v.size() < v.capacity()); // Disable grow

	v.resize(v.size() + 1);

	return v.back();
}



/******************************************************/
//! \struct nvVideoH264PicParameters
//! H.264 picture parameters
/******************************************************/
struct nvVideoH264PicParameters
{
	enum { MAX_REF_PICTURES_LIST_ENTRIES = 16 };

	StdVideoDecodeH264PictureInfo					stdPictureInfo;
	VkVideoDecodeH264PictureInfoKHR					pictureInfo;
	VkVideoDecodeH264SessionParametersAddInfoKHR	pictureParameters;
	VkVideoDecodeH264DpbSlotInfoKHR					mvcInfo;
	NvidiaVideoDecodeH264DpbSlotInfo				currentDpbSlotInfo;
	NvidiaVideoDecodeH264DpbSlotInfo				dpbRefList[MAX_REF_PICTURES_LIST_ENTRIES];
};

/*******************************************************/
//! \struct nvVideoH265PicParameters
//! HEVC picture parameters
/*******************************************************/
struct nvVideoH265PicParameters
{
	enum { MAX_REF_PICTURES_LIST_ENTRIES = 16 };

	StdVideoDecodeH265PictureInfo					stdPictureInfo;
	VkVideoDecodeH265PictureInfoKHR					pictureInfo;
	VkVideoDecodeH265SessionParametersAddInfoKHR	pictureParameters;
	VkVideoDecodeH265DpbSlotInfoKHR					setupSlotInfo;
	NvidiaVideoDecodeH265DpbSlotInfo				dpbRefList[MAX_REF_PICTURES_LIST_ENTRIES];
};


inline NvidiaVulkanPictureBase* GetPic (INvidiaVulkanPicture* pPicBuf)
{
	return (NvidiaVulkanPictureBase*)pPicBuf;
}

inline VkVideoChromaSubsamplingFlagBitsKHR ConvertStdH264ChromaFormatToVulkan (StdVideoH264ChromaFormatIdc stdFormat)
{
	switch (stdFormat)
	{
		case STD_VIDEO_H264_CHROMA_FORMAT_IDC_420:	return VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
		case STD_VIDEO_H264_CHROMA_FORMAT_IDC_422:	return VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR;
		case STD_VIDEO_H264_CHROMA_FORMAT_IDC_444:	return VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR;
		default:									TCU_THROW(InternalError, "Invalid chroma sub-sampling format");
	}
}

VkFormat codecGetVkFormat (VkVideoChromaSubsamplingFlagBitsKHR	chromaFormatIdc,
						   int									bitDepthLuma,
						   bool									isSemiPlanar)
{
	switch (chromaFormatIdc)
	{
		case VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR:
		{
			switch (bitDepthLuma)
			{
				case VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR:	return VK_FORMAT_R8_UNORM;
				case VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR:	return VK_FORMAT_R10X6_UNORM_PACK16;
				case VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR:	return VK_FORMAT_R12X4_UNORM_PACK16;
				default: TCU_THROW(InternalError, "Cannot map monochrome format to VkFormat");
			}
		}
		case VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR:
		{
			switch (bitDepthLuma)
			{
				case VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR:	return (isSemiPlanar ? VK_FORMAT_G8_B8R8_2PLANE_420_UNORM : VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM);
				case VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR:	return (isSemiPlanar ? VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 : VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16);
				case VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR:	return (isSemiPlanar ? VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16 : VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16);
				default: TCU_THROW(InternalError, "Cannot map 420 format to VkFormat");
			}
		}
		case VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR:
		{
			switch (bitDepthLuma)
			{
				case VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR:	return (isSemiPlanar ? VK_FORMAT_G8_B8R8_2PLANE_422_UNORM : VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM);
				case VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR:	return (isSemiPlanar ? VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16 : VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16);
				case VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR:	return (isSemiPlanar ? VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16 : VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16);
				default: TCU_THROW(InternalError, "Cannot map 422 format to VkFormat");
			}
		}
		case VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR:
		{
			switch (bitDepthLuma)
			{
				case VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR:	return (isSemiPlanar ? VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT : VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM);
				case VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR:	return (isSemiPlanar ? VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT : VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16);
				case VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR:	return (isSemiPlanar ? VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT : VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16);
				default: TCU_THROW(InternalError, "Cannot map 444 format to VkFormat");
			}
		}
		default: TCU_THROW(InternalError, "Unknown input idc format");
	}
}

VkVideoComponentBitDepthFlagsKHR getLumaBitDepth (deUint8 lumaBitDepthMinus8)
{
	switch (lumaBitDepthMinus8)
	{
		case 0: return VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
		case 2: return VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR;
		case 4: return VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR;
		default:TCU_THROW(InternalError, "Unhandler lumaBitDepthMinus8");
	}
}

VkVideoComponentBitDepthFlagsKHR getChromaBitDepth (deUint8 chromaBitDepthMinus8)
{
	switch (chromaBitDepthMinus8)
	{
		case 0: return VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
		case 2: return VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR;
		case 4: return VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR;
		default:TCU_THROW(InternalError, "Unhandler chromaBitDepthMinus8");
	}
}

void setImageLayout (const DeviceInterface&		vkd,
					 VkCommandBuffer			cmdBuffer,
					 VkImage					image,
					 VkImageLayout				oldImageLayout,
					 VkImageLayout				newImageLayout,
					 VkPipelineStageFlags2KHR	srcStages,
					 VkPipelineStageFlags2KHR	dstStages,
					 VkImageAspectFlags			aspectMask = VK_IMAGE_ASPECT_COLOR_BIT)
{
	VkAccessFlags2KHR	srcAccessMask	= 0;
	VkAccessFlags2KHR	dstAccessMask	= 0;

	switch (static_cast<VkImageLayout>(oldImageLayout))
	{
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:	srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;	break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:		srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;			break;
		case VK_IMAGE_LAYOUT_PREINITIALIZED:			srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;				break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:	srcAccessMask = VK_ACCESS_SHADER_READ_BIT;				break;
		case VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR:		srcAccessMask = VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR;	break;
		default:										srcAccessMask = 0;										break;
	}

	switch (static_cast<VkImageLayout>(newImageLayout))
	{
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:				dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;													break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:				dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;													break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:			dstAccessMask = VK_ACCESS_SHADER_READ_BIT;														break;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:			dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;											break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:	dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;									break;
		case VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR:				dstAccessMask = VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR;											break;
		case VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR:				dstAccessMask = VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR;											break;
		case VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR:				dstAccessMask = VK_ACCESS_2_VIDEO_ENCODE_READ_BIT_KHR;											break;
		case VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR:				dstAccessMask = VK_ACCESS_2_VIDEO_ENCODE_WRITE_BIT_KHR | VK_ACCESS_2_VIDEO_ENCODE_READ_BIT_KHR;	break;
		case VK_IMAGE_LAYOUT_GENERAL:							dstAccessMask = VK_ACCESS_HOST_WRITE_BIT;														break;
		default:												dstAccessMask = 0;																				break;
	}

	const VkImageMemoryBarrier2KHR	imageMemoryBarrier	=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,	//  VkStructureType				sType;
		DE_NULL,										//  const void*					pNext;
		srcStages,										//  VkPipelineStageFlags2KHR	srcStageMask;
		srcAccessMask,									//  VkAccessFlags2KHR			srcAccessMask;
		dstStages,										//  VkPipelineStageFlags2KHR	dstStageMask;
		dstAccessMask,									//  VkAccessFlags2KHR			dstAccessMask;
		oldImageLayout,									//  VkImageLayout				oldLayout;
		newImageLayout,									//  VkImageLayout				newLayout;
		VK_QUEUE_FAMILY_IGNORED,						//  deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						//  deUint32					dstQueueFamilyIndex;
		image,											//  VkImage						image;
		{ aspectMask, 0, 1, 0, 1 },						//  VkImageSubresourceRange		subresourceRange;
	};

	const VkDependencyInfoKHR dependencyInfo =
	{
		VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,	//  VkStructureType						sType;
		DE_NULL,								//  const void*							pNext;
		VK_DEPENDENCY_BY_REGION_BIT,			//  VkDependencyFlags					dependencyFlags;
		0,										//  deUint32							memoryBarrierCount;
		DE_NULL,								//  const VkMemoryBarrier2KHR*			pMemoryBarriers;
		0,										//  deUint32							bufferMemoryBarrierCount;
		DE_NULL,								//  const VkBufferMemoryBarrier2KHR*	pBufferMemoryBarriers;
		1,										//  deUint32							imageMemoryBarrierCount;
		&imageMemoryBarrier,					//  const VkImageMemoryBarrier2KHR*		pImageMemoryBarriers;
	};

	vkd.cmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
}

NvidiaVideoDecodeH264DpbSlotInfo::NvidiaVideoDecodeH264DpbSlotInfo ()
	: dpbSlotInfo()
	, stdReferenceInfo()
{
}

const VkVideoDecodeH264DpbSlotInfoKHR* NvidiaVideoDecodeH264DpbSlotInfo::Init (int32_t slotIndex)
{
	DE_UNREF(slotIndex);

	dpbSlotInfo.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_DPB_SLOT_INFO_KHR;
	dpbSlotInfo.pNext = DE_NULL;
	dpbSlotInfo.pStdReferenceInfo = &stdReferenceInfo;

	return &dpbSlotInfo;
}

bool NvidiaVideoDecodeH264DpbSlotInfo::IsReference () const
{
	return (dpbSlotInfo.pStdReferenceInfo == &stdReferenceInfo);
}

NvidiaVideoDecodeH264DpbSlotInfo::operator bool() const
{
	return IsReference();
}

void NvidiaVideoDecodeH264DpbSlotInfo::Invalidate ()
{
	deMemset(this, 0x00, sizeof(*this));
}

NvidiaVideoDecodeH265DpbSlotInfo::NvidiaVideoDecodeH265DpbSlotInfo ()
	: dpbSlotInfo()
	, stdReferenceInfo()
{
}

const VkVideoDecodeH265DpbSlotInfoKHR* NvidiaVideoDecodeH265DpbSlotInfo::Init (int32_t slotIndex)
{
	DE_UNREF(slotIndex);

	dpbSlotInfo.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_DPB_SLOT_INFO_KHR;
	dpbSlotInfo.pNext = DE_NULL;
	dpbSlotInfo.pStdReferenceInfo = &stdReferenceInfo;

	return &dpbSlotInfo;
}

bool NvidiaVideoDecodeH265DpbSlotInfo::IsReference() const
{
	return (dpbSlotInfo.pStdReferenceInfo == &stdReferenceInfo);
}

NvidiaVideoDecodeH265DpbSlotInfo::operator bool() const
{
	return IsReference();
}

void NvidiaVideoDecodeH265DpbSlotInfo::Invalidate()
{
	deMemset(this, 0x00, sizeof(*this));
}

// Keeps track of data associated with active internal reference frames
bool DpbSlot::isInUse (void)
{
	return (m_reserved || m_inUse);
}

bool DpbSlot::isAvailable (void)
{
	return !isInUse();
}

bool DpbSlot::Invalidate (void)
{
	bool wasInUse = isInUse();

	if (m_picBuf)
	{
		m_picBuf->Release();
		m_picBuf = DE_NULL;
	}

	m_reserved = m_inUse = false;

	return wasInUse;
}

NvidiaVulkanPictureBase* DpbSlot::getPictureResource (void)
{
	return m_picBuf;
}

NvidiaVulkanPictureBase* DpbSlot::setPictureResource (NvidiaVulkanPictureBase* picBuf)
{
	NvidiaVulkanPictureBase* oldPic = m_picBuf;

	if (picBuf)
	{
		picBuf->AddRef();
	}

	m_picBuf = picBuf;

	if (oldPic)
	{
		oldPic->Release();
	}

	return oldPic;
}

void DpbSlot::Reserve (void)
{
	m_reserved = true;
}

void DpbSlot::MarkInUse ()
{
	m_inUse = true;
}

DpbSlots::DpbSlots (uint32_t dpbMaxSize)
	: m_dpbMaxSize			(0)
	, m_slotInUseMask		(0)
	, m_dpb					(m_dpbMaxSize)
	, m_dpbSlotsAvailable	()
{
	Init(dpbMaxSize, false);
}

int32_t DpbSlots::Init (uint32_t newDpbMaxSize, bool reconfigure)
{
	DE_ASSERT(newDpbMaxSize <= MAX_DPB_SLOTS_PLUS_1);

	if (!reconfigure)
	{
		Deinit();
	}

	if (reconfigure && newDpbMaxSize < m_dpbMaxSize)
	{
		return m_dpbMaxSize;
	}

	uint32_t oldDpbMaxSize = reconfigure ? m_dpbMaxSize : 0;
	m_dpbMaxSize = newDpbMaxSize;

	m_dpb.resize(m_dpbMaxSize);

	for (uint32_t ndx = oldDpbMaxSize; ndx < m_dpbMaxSize; ndx++)
	{
		m_dpb[ndx].Invalidate();
	}

	for (uint32_t dpbIndx = oldDpbMaxSize; dpbIndx < m_dpbMaxSize; dpbIndx++)
	{
		m_dpbSlotsAvailable.push((uint8_t)dpbIndx);
	}

	return m_dpbMaxSize;
}

void DpbSlots::Deinit (void)
{
	for (uint32_t ndx = 0; ndx < m_dpbMaxSize; ndx++)
		m_dpb[ndx].Invalidate();

	while (!m_dpbSlotsAvailable.empty())
		m_dpbSlotsAvailable.pop();

	m_dpbMaxSize = 0;
	m_slotInUseMask = 0;
}

DpbSlots::~DpbSlots ()
{
	Deinit();
}

int8_t DpbSlots::AllocateSlot (void)
{
	DE_ASSERT(!m_dpbSlotsAvailable.empty());

	int8_t slot = (int8_t)m_dpbSlotsAvailable.front();

	DE_ASSERT((slot >= 0) && ((uint8_t)slot < m_dpbMaxSize));

	m_slotInUseMask |= (1 << slot);
	m_dpbSlotsAvailable.pop();
	m_dpb[slot].Reserve();

	return slot;
}

void DpbSlots::FreeSlot (int8_t slot)
{
	DE_ASSERT((uint8_t)slot < m_dpbMaxSize);
	DE_ASSERT(m_dpb[slot].isInUse());
	DE_ASSERT(m_slotInUseMask & (1 << slot));

	m_dpb[slot].Invalidate();
	m_dpbSlotsAvailable.push(slot);
	m_slotInUseMask &= ~(1 << slot);
}

DpbSlot& DpbSlots::operator[] (uint32_t slot)
{
	DE_ASSERT(slot < m_dpbMaxSize);

	return m_dpb[slot];
}

void DpbSlots::MapPictureResource (NvidiaVulkanPictureBase* pPic, int32_t dpbSlot)
{
	for (uint32_t slot = 0; slot < m_dpbMaxSize; slot++)
	{
		if ((uint8_t)slot == dpbSlot)
		{
			m_dpb[slot].setPictureResource(pPic);
		}
		else if (pPic)
		{
			if (m_dpb[slot].getPictureResource() == pPic)
			{
				FreeSlot((uint8_t)slot);
			}
		}
	}
}

uint32_t DpbSlots::getSlotInUseMask ()
{
	return m_slotInUseMask;
}

uint32_t DpbSlots::getMaxSize ()
{
	return m_dpbMaxSize;
}

typedef struct dpbEntry
{
	int8_t		dpbSlot;
	// bit0(used_for_reference)=1: top field used for reference,
	// bit1(used_for_reference)=1: bottom field used for reference
	uint32_t	used_for_reference : 2;
	uint32_t	is_long_term : 1; // 0 = short-term, 1 = long-term
	uint32_t	is_non_existing : 1; // 1 = marked as non-existing
	uint32_t	is_field_ref : 1; // set if unpaired field or complementary field pair

	union
	{
		int16_t FieldOrderCnt[2]; // h.264 : 2*32 [top/bottom].
		int32_t PicOrderCnt; // HEVC PicOrderCnt
	};

	union
	{
		int16_t FrameIdx; // : 16   short-term: FrameNum (16 bits), long-term: LongTermFrameIdx (4 bits)
		int8_t originalDpbIndex; // Original Dpb source Index.
	};

	NvidiaVulkanPictureBase* m_picBuff; // internal picture reference

	void setReferenceAndTopBoottomField (bool						isReference,
										 bool						nonExisting,
										 bool						isLongTerm,
										 bool						isFieldRef,
										 bool						topFieldIsReference,
										 bool						bottomFieldIsReference,
										 int16_t					frameIdx,
										 const int16_t				fieldOrderCntList[2],
										 NvidiaVulkanPictureBase*	picBuff)
	{
		is_non_existing = nonExisting;
		is_long_term = isLongTerm;
		is_field_ref = isFieldRef;

		if (isReference && isFieldRef)
		{
			used_for_reference = (unsigned char)(3 & ((bottomFieldIsReference << bottomFieldShift) | (topFieldIsReference << topFieldShift)));
		}
		else
		{
			used_for_reference = isReference ? 3 : 0;
		}

		FrameIdx			= frameIdx;
		FieldOrderCnt[0]	= fieldOrderCntList[used_for_reference == 2]; // 0: for progressive and top reference; 1: for bottom reference only.
		FieldOrderCnt[1]	= fieldOrderCntList[used_for_reference != 1]; // 0: for top reference only;  1: for bottom reference and progressive.
		dpbSlot				= -1;
		m_picBuff			= picBuff;
	}

	void setReference (bool						isLongTerm,
					   int32_t					picOrderCnt,
					   NvidiaVulkanPictureBase*	picBuff)
	{
		is_non_existing = (picBuff == DE_NULL);
		is_long_term = isLongTerm;
		is_field_ref = false;
		used_for_reference = (picBuff != DE_NULL) ? 3 : 0;

		PicOrderCnt = picOrderCnt;

		dpbSlot = -1;
		m_picBuff = picBuff;
		originalDpbIndex = -1;
	}

	bool isRef ()
	{
		return (used_for_reference != 0);
	}

	StdVideoDecodeH264ReferenceInfoFlags getPictureFlag (bool currentPictureIsProgressive)
	{
		StdVideoDecodeH264ReferenceInfoFlags picFlags = StdVideoDecodeH264ReferenceInfoFlags();

		if (used_for_reference)
		{
			// picFlags.is_reference = true;
		}

		if (is_long_term)
		{
			picFlags.used_for_long_term_reference = true;
		}

		if (is_non_existing)
		{
			picFlags.is_non_existing = true;
		}

		if (is_field_ref)
		{
			// picFlags.field_pic_flag = true;
		}

		if (!currentPictureIsProgressive && (used_for_reference & topFieldMask))
		{
			picFlags.top_field_flag = true;
		}

		if (!currentPictureIsProgressive && (used_for_reference & bottomFieldMask))
		{
			picFlags.bottom_field_flag = true;
		}

		return picFlags;
	}

	void setH264PictureData (NvidiaVideoDecodeH264DpbSlotInfo*	pDpbRefList,
							 VkVideoReferenceSlotInfoKHR*			pReferenceSlots,
							 uint32_t							dpbEntryIdx,
							 uint32_t							dpbSlotIndex,
							 bool								currentPictureIsProgressive)
	{
		DE_ASSERT(dpbEntryIdx < AVC_MAX_DPB_SLOTS);
		DE_ASSERT(dpbSlotIndex < AVC_MAX_DPB_SLOTS);

		DE_ASSERT((dpbSlotIndex == (uint32_t)dpbSlot) || is_non_existing);
		pReferenceSlots[dpbEntryIdx].sType = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
		pReferenceSlots[dpbEntryIdx].slotIndex = dpbSlotIndex;
		pReferenceSlots[dpbEntryIdx].pNext = pDpbRefList[dpbEntryIdx].Init(dpbSlotIndex);

		StdVideoDecodeH264ReferenceInfo* pRefPicInfo = &pDpbRefList[dpbEntryIdx].stdReferenceInfo;

		pRefPicInfo->FrameNum = FrameIdx;
		pRefPicInfo->flags = getPictureFlag(currentPictureIsProgressive);
		pRefPicInfo->PicOrderCnt[0] = FieldOrderCnt[0];
		pRefPicInfo->PicOrderCnt[1] = FieldOrderCnt[1];
	}

	void setH265PictureData (NvidiaVideoDecodeH265DpbSlotInfo*	pDpbSlotInfo,
							 VkVideoReferenceSlotInfoKHR*			pReferenceSlots,
							 uint32_t							dpbEntryIdx,
							 uint32_t							dpbSlotIndex)
	{
		DE_ASSERT(dpbEntryIdx < HEVC_MAX_DPB_SLOTS);
		DE_ASSERT(dpbSlotIndex < HEVC_MAX_DPB_SLOTS);
		DE_ASSERT(isRef());

		DE_ASSERT((dpbSlotIndex == (uint32_t)dpbSlot) || is_non_existing);
		pReferenceSlots[dpbEntryIdx].sType = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
		pReferenceSlots[dpbEntryIdx].slotIndex = dpbSlotIndex;
		pReferenceSlots[dpbEntryIdx].pNext = pDpbSlotInfo[dpbEntryIdx].Init(dpbSlotIndex);

		StdVideoDecodeH265ReferenceInfo* pRefPicInfo = &pDpbSlotInfo[dpbEntryIdx].stdReferenceInfo;
		pRefPicInfo->PicOrderCntVal = PicOrderCnt;
		pRefPicInfo->flags.used_for_long_term_reference = is_long_term;
		pRefPicInfo->flags.unused_for_reference = is_non_existing;

	}

} dpbEntry;

int8_t VideoBaseDecoder::GetPicIdx (NvidiaVulkanPictureBase* pPicBuf)
{
	if (pPicBuf)
	{
		int32_t picIndex = pPicBuf->m_picIdx;

		if ((picIndex >= 0) && ((uint32_t)picIndex < m_maxNumDecodeSurfaces))
		{
			return (int8_t)picIndex;
		}
	}

	return -1;
}

int8_t VideoBaseDecoder::GetPicIdx (INvidiaVulkanPicture* pPicBuf)
{
	return GetPicIdx(GetPic(pPicBuf));
}

int8_t VideoBaseDecoder::GetPicDpbSlot (int8_t picIndex)
{
	return m_pictureToDpbSlotMap[picIndex];
}

int8_t VideoBaseDecoder::GetPicDpbSlot (NvidiaVulkanPictureBase* pPicBuf)
{
	int8_t picIndex = GetPicIdx(pPicBuf);
	DE_ASSERT((picIndex >= 0) && ((uint32_t)picIndex < m_maxNumDecodeSurfaces));
	return GetPicDpbSlot(picIndex);
}

bool VideoBaseDecoder::GetFieldPicFlag (int8_t picIndex)
{
	DE_ASSERT((picIndex >= 0) && ((uint32_t)picIndex < m_maxNumDecodeSurfaces));

	return !!(m_fieldPicFlagMask & (1 << (uint32_t)picIndex));
}

bool VideoBaseDecoder::SetFieldPicFlag (int8_t picIndex, bool fieldPicFlag)
{
	DE_ASSERT((picIndex >= 0) && ((uint32_t)picIndex < m_maxNumDecodeSurfaces));

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

int8_t VideoBaseDecoder::SetPicDpbSlot (int8_t picIndex, int8_t dpbSlot)
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

int8_t VideoBaseDecoder::SetPicDpbSlot (NvidiaVulkanPictureBase* pPicBuf, int8_t dpbSlot)
{
	int8_t picIndex = GetPicIdx(pPicBuf);

	DE_ASSERT((picIndex >= 0) && ((uint32_t)picIndex < m_maxNumDecodeSurfaces));

	return SetPicDpbSlot(picIndex, dpbSlot);
}

uint32_t VideoBaseDecoder::ResetPicDpbSlots (uint32_t picIndexSlotValidMask)
{
	uint32_t resetSlotsMask = ~(picIndexSlotValidMask | ~m_dpbSlotsMask);

	for (uint32_t picIdx = 0; (picIdx < m_maxNumDecodeSurfaces) && resetSlotsMask; picIdx++)
	{
		if (resetSlotsMask & (1 << picIdx))
		{
			resetSlotsMask &= ~(1 << picIdx);

			SetPicDpbSlot((int8_t)picIdx, -1);
		}
	}

	return m_dpbSlotsMask;
}

VideoBaseDecoder::VideoBaseDecoder (Context& context)
	: m_context									(context)
	, m_nvFuncs									(createIfcNvFunctions(context.getTestContext().getPlatform().getVulkanPlatform()))
	, m_videoCodecOperation						(VK_VIDEO_CODEC_OPERATION_NONE_KHR)
	, m_vkd										(DE_NULL)
	, m_device									(DE_NULL)
	, m_queueFamilyIndexTransfer				(VK_QUEUE_FAMILY_IGNORED)
	, m_queueFamilyIndexDecode					(VK_QUEUE_FAMILY_IGNORED)
	, m_queueTransfer							(DE_NULL)
	, m_queueDecode								(DE_NULL)
	, m_allocator								(DE_NULL)
	, m_nCurrentPictureID						(0)
	, m_dpbSlotsMask							(0)
	, m_fieldPicFlagMask						(0)
	, m_dpb										(3)
	, m_pictureToDpbSlotMap						()
	, m_maxNumDecodeSurfaces					(1)
	, m_maxNumDpbSurfaces						(1)
	, m_clockRate								(0)
	, m_minBitstreamBufferSizeAlignment			(0)
	, m_minBitstreamBufferOffsetAlignment		(0)
	, m_videoDecodeSession						()
	, m_videoDecodeSessionAllocs				()
	, m_numDecodeSurfaces						()
	, m_videoCommandPool						()
	, m_videoFrameBuffer						(new VideoFrameBuffer())
	, m_decodeFramesData						(DE_NULL)
	, m_maxDecodeFramesCount					(0)
	, m_maxDecodeFramesAllocated				(0)
	, m_width									(0)
	, m_height									(0)
	, m_codedWidth								(0)
	, m_codedHeight								(0)
	, m_chromaFormat							()
	, m_bitLumaDepthMinus8						(0)
	, m_bitChromaDepthMinus8					(0)
	, m_decodePicCount							(0)
	, m_videoFormat								()
	, m_lastSpsIdInQueue						(-1)
	, m_pictureParametersQueue					()
	, m_lastVpsPictureParametersQueue			()
	, m_lastSpsPictureParametersQueue			()
	, m_lastPpsPictureParametersQueue			()
	, m_currentPictureParameters				()
	, m_randomOrSwapped							(false)
	, m_queryResultWithStatus					(false)
	, m_frameCountTrigger						(0)
	, m_submitAfter								(false)
	, m_gopSize									(0)
	, m_dpbCount								(0)
	, m_heaps									()
	, m_pPerFrameDecodeParameters				()
	, m_pVulkanParserDecodePictureInfo			()
	, m_pFrameDatas								()
	, m_bitstreamBufferMemoryBarriers			()
	, m_imageBarriersVec						()
	, m_frameSynchronizationInfos				()
	, m_commandBufferSubmitInfos				()
	, m_decodeBeginInfos						()
	, m_pictureResourcesInfos					()
	, m_dependencyInfos							()
	, m_decodeEndInfos							()
	, m_submitInfos								()
	, m_frameCompleteFences						()
	, m_frameConsumerDoneFences					()
	, m_frameCompleteSemaphoreSubmitInfos		()
	, m_frameConsumerDoneSemaphoreSubmitInfos	()
	, m_distinctDstDpbImages					(false)
{
	deMemset(&m_nvidiaVulkanParserSequenceInfo, 0, sizeof(m_nvidiaVulkanParserSequenceInfo));

	for (uint32_t picNdx = 0; picNdx < DE_LENGTH_OF_ARRAY(m_pictureToDpbSlotMap); picNdx++)
		m_pictureToDpbSlotMap[picNdx] = -1;

	ReinitCaches();
}

VideoBaseDecoder::~VideoBaseDecoder (void)
{
	Deinitialize();
}

void VideoBaseDecoder::initialize (const VkVideoCodecOperationFlagBitsKHR	videoCodecOperation,
								   const DeviceInterface&					vkd,
								   const VkDevice							device,
								   const deUint32							queueFamilyIndexTransfer,
								   const deUint32							queueFamilyIndexDecode,
								   Allocator&								allocator)
{
	DE_ASSERT(m_videoCodecOperation == VK_VIDEO_CODEC_OPERATION_NONE_KHR);
	DE_ASSERT(m_vkd == DE_NULL);
	DE_ASSERT(m_device == DE_NULL);
	DE_ASSERT(queueFamilyIndexTransfer != VK_QUEUE_FAMILY_IGNORED);
	DE_ASSERT(queueFamilyIndexDecode != VK_QUEUE_FAMILY_IGNORED);
	DE_ASSERT(m_allocator == DE_NULL);

	m_videoCodecOperation		= videoCodecOperation;
	m_vkd						= &vkd;
	m_device					= device;
	m_queueFamilyIndexTransfer	= queueFamilyIndexTransfer;
	m_queueFamilyIndexDecode	= queueFamilyIndexDecode;
	m_allocator					= &allocator;
	m_queueTransfer				= getDeviceQueue(vkd, device, m_queueFamilyIndexTransfer, 0u);
	m_queueDecode				= getDeviceQueue(vkd, device, m_queueFamilyIndexDecode, 0u);
}

VkDevice VideoBaseDecoder::getDevice (void)
{
	DE_ASSERT(m_device != DE_NULL);

	return m_device;
}

const DeviceInterface& VideoBaseDecoder::getDeviceDriver (void)
{
	DE_ASSERT(m_vkd != DE_NULL);

	return *m_vkd;
}

deUint32 VideoBaseDecoder::getQueueFamilyIndexTransfer (void)
{
	DE_ASSERT(m_queueFamilyIndexTransfer != VK_QUEUE_FAMILY_IGNORED);

	return m_queueFamilyIndexTransfer;
}

VkQueue VideoBaseDecoder::getQueueTransfer (void)
{
	DE_ASSERT(m_queueTransfer != DE_NULL);

	return m_queueTransfer;
}

deUint32 VideoBaseDecoder::getQueueFamilyIndexDecode (void)
{
	DE_ASSERT(m_queueFamilyIndexDecode != VK_QUEUE_FAMILY_IGNORED);

	return m_queueFamilyIndexDecode;
}

VkQueue VideoBaseDecoder::getQueueDecode (void)
{
	DE_ASSERT(m_queueDecode != DE_NULL);

	return m_queueDecode;
}

Allocator& VideoBaseDecoder::getAllocator (void)
{
	DE_ASSERT(m_allocator != DE_NULL);

	return *m_allocator;
}


void VideoBaseDecoder::setDecodeParameters (bool		randomOrSwapped,
											bool		queryResultWithStatus,
											uint32_t	frameCountTrigger,
											bool		submitAfter,
											uint32_t	gopSize,
											uint32_t	dpbCount)

{
	m_randomOrSwapped			= randomOrSwapped;
	m_queryResultWithStatus		= queryResultWithStatus;
	m_frameCountTrigger			= frameCountTrigger;
	m_submitAfter				= submitAfter;
	m_gopSize					= gopSize  ? gopSize : frameCountTrigger;
	m_dpbCount					= dpbCount ? dpbCount : 1;

	DEBUGLOG(std::cout << m_randomOrSwapped << " " << m_queryResultWithStatus << " " << m_frameCountTrigger << " " << m_submitAfter << " " << m_gopSize << " " << m_dpbCount << std::endl);

	ReinitCaches();
}

int32_t VideoBaseDecoder::BeginSequence (const NvidiaVulkanParserSequenceInfo* pnvsi)
{
	DEBUGLOG(std::cout << "VideoBaseDecoder::BeginSequence " << std::dec << pnvsi->nCodedWidth << "x" << pnvsi->nCodedHeight << std::endl);
	DEBUGLOG(std::cout << "VideoBaseDecoder::BeginSequence nMinNumDecodeSurfaces=" << pnvsi->nMinNumDecodeSurfaces << " pnvsi->isSVC=" << pnvsi->isSVC << std::endl);

	const int32_t								maxDbpSlots						= MAX_DPB_SLOTS_PLUS_1 - ((pnvsi->eCodec == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR) ? 0 : EXTRA_DPB_SLOTS);
	const int32_t								configDpbSlotsPre				= (pnvsi->nMinNumDecodeSurfaces > 0)
																				? (pnvsi->nMinNumDecodeSurfaces - (pnvsi->isSVC ? 3 : 1))
																				: 0;
	const int32_t								configDpbSlots					= std::min(maxDbpSlots, configDpbSlotsPre);
	const int32_t								configDpbSlotsPlus1				= std::min(configDpbSlots + 1, (int32_t)MAX_DPB_SLOTS_PLUS_1);

	DEBUGLOG(std::cout << "VideoBaseDecoder::BeginSequence configDpbSlots=" << configDpbSlots << " configDpbSlotsPlus1=" << configDpbSlotsPlus1 << std::endl);

	const bool									sequenceUpdate					= (m_nvidiaVulkanParserSequenceInfo.nMaxWidth != 0) && (m_nvidiaVulkanParserSequenceInfo.nMaxHeight != 0);
	const bool									formatChange					=  (pnvsi->eCodec					!= m_nvidiaVulkanParserSequenceInfo.eCodec)
																				|| (pnvsi->codecProfile				!= m_nvidiaVulkanParserSequenceInfo.codecProfile)
																				|| (pnvsi->nChromaFormat			!= m_nvidiaVulkanParserSequenceInfo.nChromaFormat)
																				|| (pnvsi->uBitDepthLumaMinus8		!= m_nvidiaVulkanParserSequenceInfo.uBitDepthLumaMinus8)
																				|| (pnvsi->uBitDepthChromaMinus8	!= m_nvidiaVulkanParserSequenceInfo.uBitDepthChromaMinus8)
																				|| (pnvsi->bProgSeq					!= m_nvidiaVulkanParserSequenceInfo.bProgSeq);
	const bool									extentChange					=  (pnvsi->nCodedWidth				!= m_nvidiaVulkanParserSequenceInfo.nCodedWidth)
																				|| (pnvsi->nCodedHeight				!= m_nvidiaVulkanParserSequenceInfo.nCodedHeight);
	const bool									sequenceReconfigireFormat		= sequenceUpdate && formatChange;
	const bool									sequenceReconfigireCodedExtent	= sequenceUpdate && extentChange;
	const VkVideoChromaSubsamplingFlagBitsKHR	chromaSubsampling				= ConvertStdH264ChromaFormatToVulkan((StdVideoH264ChromaFormatIdc)pnvsi->nChromaFormat);
	const VulkanParserDetectedVideoFormat		detectedFormat					=
	{
		pnvsi->eCodec,											//  vk::VkVideoCodecOperationFlagBitsKHR	codec;
		pnvsi->codecProfile,									//  uint32_t								codecProfile;
		getLumaBitDepth(pnvsi->uBitDepthLumaMinus8),			//  VkVideoComponentBitDepthFlagsKHR		lumaBitDepth;
		getChromaBitDepth(pnvsi->uBitDepthChromaMinus8),		//  VkVideoComponentBitDepthFlagsKHR		chromaBitDepth;
		chromaSubsampling,										//  VkVideoChromaSubsamplingFlagBitsKHR		chromaSubsampling;
		NVIDIA_FRAME_RATE_NUM(pnvsi->frameRate),				//  uint32_t								frame_rate_numerator;
		NVIDIA_FRAME_RATE_DEN(pnvsi->frameRate),				//  uint32_t								frame_rate_denominator;
		(uint8_t)(sequenceUpdate != 0 ? 1 : 0),					//  uint8_t									sequenceUpdate : 1;
		(uint8_t)(sequenceReconfigireFormat != 0 ? 1 : 0),		//  uint8_t									sequenceReconfigireFormat : 1;
		(uint8_t)(sequenceReconfigireCodedExtent != 0 ? 1 : 0),	//  uint8_t									sequenceReconfigireCodedExtent : 1;
		(uint8_t)(pnvsi->bProgSeq != 0 ? 1 : 0),				//  uint8_t									progressive_sequence : 1;
		pnvsi->uBitDepthLumaMinus8,								//  uint8_t									bit_depth_luma_minus8;
		pnvsi->uBitDepthChromaMinus8,							//  uint8_t									bit_depth_chroma_minus8;
		0u,														//  uint8_t									reserved1;
		(uint32_t)pnvsi->nCodedWidth,							//  uint32_t								coded_width;
		(uint32_t)pnvsi->nCodedHeight,							//  uint32_t								coded_height;

		{
			0u,													//  int32_t									left;
			0u,													//  int32_t									top;
			pnvsi->nDisplayWidth,								//  int32_t									right;
			pnvsi->nDisplayHeight,								//  int32_t									bottom;
		},

		(uint32_t)pnvsi->lBitrate,								//  uint32_t								bitrate;
		(int32_t)pnvsi->lDARWidth,								//  int32_t									display_aspect_ratio_x;
		(int32_t)pnvsi->lDARHeight,								//  int32_t									display_aspect_ratio_y;
		(uint32_t)pnvsi->nMinNumDecodeSurfaces,					//  uint32_t								minNumDecodeSurfaces;
		(uint32_t)configDpbSlotsPlus1,							//  uint32_t								maxNumDpbSlots;

		{
			(uint8_t)(7 & pnvsi->lVideoFormat),					//  uint8_t									video_format : 3;
			(uint8_t)(pnvsi->uVideoFullRange != 0 ? 1 : 0),		//  uint8_t									video_full_range_flag : 1;
			0u,													//  uint8_t									reserved_zero_bits : 4;
			(uint8_t)pnvsi->lColorPrimaries,					//  uint8_t									color_primaries;
			(uint8_t)pnvsi->lTransferCharacteristics,			//  uint8_t									transfer_characteristics;
			(uint8_t)pnvsi->lMatrixCoefficients,				//  uint8_t									matrix_coefficients;
		},

		0u,														//  uint32_t								seqhdr_data_length;
	};

	m_nvidiaVulkanParserSequenceInfo				= *pnvsi;
	m_nvidiaVulkanParserSequenceInfo.nMaxWidth		= pnvsi->nCodedWidth;
	m_nvidiaVulkanParserSequenceInfo.nMaxHeight		= pnvsi->nCodedHeight;

	int maxDecodeRTs = StartVideoSequence(&detectedFormat);

	// nDecodeRTs = 0 means SequenceCallback failed
	// nDecodeRTs = 1 means SequenceCallback succeeded
	// nDecodeRTs > 1 means we need to overwrite the MaxNumDecodeSurfaces
	if (!maxDecodeRTs)
	{
		return 0;
	}
	// MaxNumDecodeSurface may not be correctly calculated by the client while
	// parser creation so overwrite it with NumDecodeSurface. (only if nDecodeRT
	// > 1)
	if (maxDecodeRTs > 1)
	{
		m_maxNumDecodeSurfaces = maxDecodeRTs;
	}

	// The number of minNumDecodeSurfaces can be overwritten.
	// Add one for the current Dpb setup slot.
	m_maxNumDpbSurfaces = configDpbSlotsPlus1;

	m_dpb.Init(m_maxNumDpbSurfaces, sequenceUpdate);

	// NOTE: Important Tegra parser requires the maxDpbSlotsPlus1 and not dpbSlots.
	return configDpbSlotsPlus1;
}

bool VideoBaseDecoder::AllocPictureBuffer (INvidiaVulkanPicture** ppNvidiaVulkanPicture)
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
		*ppNvidiaVulkanPicture = (INvidiaVulkanPicture*)DE_NULL;
	}

	return result;
}

bool VideoBaseDecoder::DecodePicture (NvidiaVulkanParserPictureData* pNvidiaVulkanParserPictureData)
{
	DEBUGLOG(std::cout << "VideoBaseDecoder::DecodePicture" << std::endl);

	VulkanParserDecodePictureInfo	decodePictureInfo	= VulkanParserDecodePictureInfo();
	bool							result				= false;

	if (!pNvidiaVulkanParserPictureData->pCurrPic)
	{
		return result;
	}

	NvidiaVulkanPictureBase*	pVkPicBuff	= GetPic(pNvidiaVulkanParserPictureData->pCurrPic);
	const int32_t				picIdx		= pVkPicBuff ? pVkPicBuff->m_picIdx : -1;

	DEBUGLOG(std::cout << "\tVideoBaseDecoder::DecodePicture " << (void*)pVkPicBuff << std::endl);

	DE_ASSERT(picIdx < MAX_FRM_CNT);

	decodePictureInfo.pictureIndex				= picIdx;
	decodePictureInfo.flags.progressiveFrame	= pNvidiaVulkanParserPictureData->progressive_frame ? 1 : 0;
	decodePictureInfo.flags.fieldPic			= pNvidiaVulkanParserPictureData->field_pic_flag ? 1 : 0;			// 0 = frame picture, 1 = field picture
	decodePictureInfo.flags.repeatFirstField	= 3 & (uint32_t)pNvidiaVulkanParserPictureData->repeat_first_field;	// For 3:2 pulldown (number of additional fields, 2 = frame doubling, 4 = frame tripling)
	decodePictureInfo.flags.refPic				= pNvidiaVulkanParserPictureData->ref_pic_flag ? 1 : 0;				// Frame is a reference frame

	// Mark the first field as unpaired Detect unpaired fields
	if (pNvidiaVulkanParserPictureData->field_pic_flag)
	{
		decodePictureInfo.flags.bottomField		= pNvidiaVulkanParserPictureData->bottom_field_flag ? 1 : 0;	// 0 = top field, 1 = bottom field (ignored if field_pic_flag=0)
		decodePictureInfo.flags.secondField		= pNvidiaVulkanParserPictureData->second_field ? 1 : 0;			// Second field of a complementary field pair
		decodePictureInfo.flags.topFieldFirst	= pNvidiaVulkanParserPictureData->top_field_first ? 1 : 0;		// Frame pictures only

		if (!pNvidiaVulkanParserPictureData->second_field)
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

	return DecodePicture(pNvidiaVulkanParserPictureData, &decodePictureInfo);
}

bool VideoBaseDecoder::UpdatePictureParameters (NvidiaVulkanPictureParameters*						pNvidiaVulkanPictureParameters,
												NvidiaSharedBaseObj<NvidiaParserVideoRefCountBase>&	pictureParametersObject,
												uint64_t											updateSequenceCount)
{
	DEBUGLOG(std::cout << "VideoBaseDecoder::UpdatePictureParameters " << (void*)pNvidiaVulkanPictureParameters << " " << updateSequenceCount << std::endl);

	if (pNvidiaVulkanPictureParameters == DE_NULL)
		return DE_NULL;

	return UpdatePictureParametersHandler(pNvidiaVulkanPictureParameters, pictureParametersObject, updateSequenceCount);
}

bool VideoBaseDecoder::DisplayPicture (INvidiaVulkanPicture*	pNvidiaVulkanPicture,
									   int64_t					llPTS)
{
	DEBUGLOG(std::cout << "VideoBaseDecoder::DisplayPicture" << std::endl);

	bool result = false;

	NvidiaVulkanPictureBase* pVkPicBuff = GetPic(pNvidiaVulkanPicture);

	DE_ASSERT(pVkPicBuff != DE_NULL);

	int32_t picIdx = pVkPicBuff ? pVkPicBuff->m_picIdx : -1;

	DE_ASSERT(picIdx != -1);

	if (m_videoFrameBuffer != DE_NULL && picIdx != -1)
	{
		DisplayPictureInfo dispInfo = DisplayPictureInfo();

		dispInfo.timestamp = (int64_t)llPTS;

		const int32_t retVal = m_videoFrameBuffer->QueueDecodedPictureForDisplay((int8_t)picIdx, &dispInfo);

		DE_ASSERT(picIdx == retVal);
		DE_UNREF(retVal);

		result = true;
	}

	return result;
}

void VideoBaseDecoder::UnhandledNALU (const uint8_t*	pbData,
									  int32_t			cbData)
{
	const vector<uint8_t> data (pbData, pbData + cbData);
	ostringstream css;

	css << "UnhandledNALU=";

	for (const auto& i: data)
		css << std::hex << std::setw(2) << std::setfill('0') << (deUint32)i << ' ';

	TCU_THROW(InternalError, css.str());
}

bool VideoBaseDecoder::DecodePicture (NvidiaVulkanParserPictureData*	pNvidiaVulkanParserPictureData,
									  VulkanParserDecodePictureInfo*	pDecodePictureInfo)
{
	DEBUGLOG(std::cout << "\tDecodePicture sps_sid:" << (uint32_t)pNvidiaVulkanParserPictureData->CodecSpecific.h264.pStdSps->seq_parameter_set_id << " pps_sid:" << (uint32_t)pNvidiaVulkanParserPictureData->CodecSpecific.h264.pStdPps->seq_parameter_set_id << " pps_pid:" << (uint32_t)pNvidiaVulkanParserPictureData->CodecSpecific.h264.pStdPps->pic_parameter_set_id << std::endl);
	bool bRet = false;

	if (!pNvidiaVulkanParserPictureData->pCurrPic)
	{
		return false;
	}

	const uint32_t PicIdx = GetPicIdx(pNvidiaVulkanParserPictureData->pCurrPic);

	if (PicIdx >= MAX_FRM_CNT)
	{
		DE_ASSERT(0);
		return false;
	}

	HeapType heap;

	PerFrameDecodeParameters*	pPictureParams		= ALLOC_HEAP_OBJECT(heap, PerFrameDecodeParameters);
	VkVideoReferenceSlotInfoKHR*	pReferenceSlots		= ALLOC_HEAP_OBJECT_ARRAY(heap, VkVideoReferenceSlotInfoKHR, PerFrameDecodeParameters::MAX_DPB_REF_SLOTS);
	VkVideoReferenceSlotInfoKHR*	pSetupReferenceSlot	= ALLOC_HEAP_OBJECT(heap, VkVideoReferenceSlotInfoKHR);

	*pSetupReferenceSlot =
	{
		VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR,	//  VkStructureType						sType;
		DE_NULL,											//  const void*							pNext;
		-1,													//  deInt8								slotIndex;
		DE_NULL												//  const VkVideoPictureResourceInfoKHR*	pPictureResource;
	};

	PerFrameDecodeParameters*	pPerFrameDecodeParameters = pPictureParams;

	pPerFrameDecodeParameters->currPicIdx		= PicIdx;
	pPerFrameDecodeParameters->bitstreamDataLen	= pNvidiaVulkanParserPictureData->nBitstreamDataLen;
	pPerFrameDecodeParameters->pBitstreamData	= pNvidiaVulkanParserPictureData->pBitstreamData;

	pPerFrameDecodeParameters->decodeFrameInfo.sType					= VK_STRUCTURE_TYPE_VIDEO_DECODE_INFO_KHR;
	pPerFrameDecodeParameters->decodeFrameInfo.dstPictureResource.sType	= VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR;

	if (m_videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR)
	{
		const NvidiaVulkanParserH264PictureData*const	pin				= &pNvidiaVulkanParserPictureData->CodecSpecific.h264;
		nvVideoH264PicParameters*						pH264			= ALLOC_HEAP_OBJECT(heap, nvVideoH264PicParameters);
		VkVideoDecodeH264PictureInfoKHR*				pPictureInfo	= &pH264->pictureInfo;
		NvidiaVideoDecodeH264DpbSlotInfo*				pDpbRefList		= pH264->dpbRefList;
		StdVideoDecodeH264PictureInfo*					pStdPictureInfo	= &pH264->stdPictureInfo;

		*pH264 = nvVideoH264PicParameters();

		pPerFrameDecodeParameters->pCurrentPictureParameters = StdVideoPictureParametersSet::StdVideoPictureParametersSetFromBase(pin->pPpsClientObject);
		DEBUGLOG(std::cout << "\tDecodePicture SPS:" << (void*)pin->pSpsClientObject << " PPS:" << (void*)pin->pPpsClientObject << std::endl);

		pDecodePictureInfo->videoFrameType = 0; // pd->CodecSpecific.h264.slice_type;
		// FIXME: If mvcext is enabled.
		pDecodePictureInfo->viewId = (uint16_t)pNvidiaVulkanParserPictureData->CodecSpecific.h264.mvcext.view_id;

		pPictureInfo->sType					= VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PICTURE_INFO_KHR;
		pPictureInfo->pNext					= DE_NULL;
		pPictureInfo->pStdPictureInfo		= &pH264->stdPictureInfo;
		pPictureInfo->sliceCount			= pNvidiaVulkanParserPictureData->nNumSlices;
		pPictureInfo->pSliceOffsets			= static_cast<uint32_t*>(copyToHeap(heap, pNvidiaVulkanParserPictureData->pSliceDataOffsets, sizeof(uint32_t) * pNvidiaVulkanParserPictureData->nNumSlices));

		pPerFrameDecodeParameters->decodeFrameInfo.pNext = &pH264->pictureInfo;

		pStdPictureInfo->pic_parameter_set_id	= pin->pic_parameter_set_id; // PPS ID
		pStdPictureInfo->seq_parameter_set_id	= pin->seq_parameter_set_id; // SPS ID;
		pStdPictureInfo->frame_num				= (uint16_t)pin->frame_num;

		pH264->mvcInfo.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_DPB_SLOT_INFO_KHR;
		pH264->mvcInfo.pNext = DE_NULL; // No more extension structures.

		StdVideoDecodeH264ReferenceInfo referenceInfo = StdVideoDecodeH264ReferenceInfo();
		pH264->mvcInfo.pStdReferenceInfo = &referenceInfo;
		pSetupReferenceSlot->pNext = &pH264->mvcInfo;

		StdVideoDecodeH264PictureInfoFlags currPicFlags = StdVideoDecodeH264PictureInfoFlags();

		currPicFlags.is_intra = (pNvidiaVulkanParserPictureData->intra_pic_flag != 0);

		// 0 = frame picture, 1 = field picture
		if (pNvidiaVulkanParserPictureData->field_pic_flag)
		{
			// 0 = top field, 1 = bottom field (ignored if field_pic_flag = 0)
			currPicFlags.field_pic_flag = true;
			if (pNvidiaVulkanParserPictureData->bottom_field_flag)
			{
				currPicFlags.bottom_field_flag = true;
			}
		}
		// Second field of a complementary field pair
		if (pNvidiaVulkanParserPictureData->second_field)
		{
			currPicFlags.complementary_field_pair = true;
		}

		// Frame is a reference frame
		if (pNvidiaVulkanParserPictureData->ref_pic_flag)
		{
			currPicFlags.is_reference = true;
		}

		pStdPictureInfo->flags = currPicFlags;

		if (!pNvidiaVulkanParserPictureData->field_pic_flag)
		{
			pStdPictureInfo->PicOrderCnt[0] = pin->CurrFieldOrderCnt[0];
			pStdPictureInfo->PicOrderCnt[1] = pin->CurrFieldOrderCnt[1];
		}
		else
		{
			pStdPictureInfo->PicOrderCnt[pNvidiaVulkanParserPictureData->bottom_field_flag] = pin->CurrFieldOrderCnt[pNvidiaVulkanParserPictureData->bottom_field_flag];
		}

		pPerFrameDecodeParameters->numGopReferenceSlots = FillDpbH264State(pNvidiaVulkanParserPictureData,
																		   pin->dpb,
																		   DE_LENGTH_OF_ARRAY(pin->dpb),
																		   pDpbRefList,
																		   pReferenceSlots,
																		   pPerFrameDecodeParameters->pGopReferenceImagesIndexes,
																		   pH264->stdPictureInfo.flags,
																		   &pSetupReferenceSlot->slotIndex);

		DEBUGLOG(cout<<"pSetupReferenceSlot->slotIndex=" << dec << pSetupReferenceSlot->slotIndex <<endl);

		if (pSetupReferenceSlot->slotIndex >= 0)
		{
			if (m_distinctDstDpbImages)
			{
				const int32_t setupSlotNdx = pPerFrameDecodeParameters->numGopReferenceSlots;

				DE_ASSERT(setupSlotNdx < PerFrameDecodeParameters::MAX_DPB_REF_SLOTS);

				pReferenceSlots[setupSlotNdx] = *pSetupReferenceSlot;

				pSetupReferenceSlot = &pReferenceSlots[setupSlotNdx];

				pPerFrameDecodeParameters->pictureResources[setupSlotNdx] = pPerFrameDecodeParameters->decodeFrameInfo.dstPictureResource;

				pSetupReferenceSlot->pPictureResource = &pPerFrameDecodeParameters->pictureResources[setupSlotNdx];
			}
			else
			{
				pSetupReferenceSlot->pPictureResource = &pPerFrameDecodeParameters->decodeFrameInfo.dstPictureResource;
			}

			pPerFrameDecodeParameters->decodeFrameInfo.pSetupReferenceSlot	= pSetupReferenceSlot;
		}

		ostringstream s;
		s << "numGopReferenceSlots:" << std::dec << pPerFrameDecodeParameters->numGopReferenceSlots << "(";
		if (pPerFrameDecodeParameters->numGopReferenceSlots)
		{
			for (int32_t dpbEntryIdx = 0; dpbEntryIdx < pPerFrameDecodeParameters->numGopReferenceSlots; dpbEntryIdx++)
			{
				pPerFrameDecodeParameters->pictureResources[dpbEntryIdx].sType	= VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR;
				pReferenceSlots[dpbEntryIdx].pPictureResource					= &pPerFrameDecodeParameters->pictureResources[dpbEntryIdx];

				DE_ASSERT(pDpbRefList[dpbEntryIdx].IsReference());

				s << std::dec << pReferenceSlots[dpbEntryIdx].slotIndex << " ";
			}

			pPerFrameDecodeParameters->decodeFrameInfo.pReferenceSlots		= pReferenceSlots;
			pPerFrameDecodeParameters->decodeFrameInfo.referenceSlotCount	= pPerFrameDecodeParameters->numGopReferenceSlots;
		}
		else
		{
			pPerFrameDecodeParameters->decodeFrameInfo.pReferenceSlots		= DE_NULL;
			pPerFrameDecodeParameters->decodeFrameInfo.referenceSlotCount	= 0;
		}
		s << ")";

		DEBUGLOG(cout << s.str() <<endl);
	}
	else if (m_videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR)
	{
		const NvidiaVulkanParserH265PictureData* const	pin				= &pNvidiaVulkanParserPictureData->CodecSpecific.h265;
		nvVideoH265PicParameters*						pHevc			= ALLOC_HEAP_OBJECT(heap, nvVideoH265PicParameters);
		VkVideoDecodeH265PictureInfoKHR*				pPictureInfo	= &pHevc->pictureInfo;
		StdVideoDecodeH265PictureInfo*					pStdPictureInfo	= &pHevc->stdPictureInfo;
		NvidiaVideoDecodeH265DpbSlotInfo*				pDpbRefList		= pHevc->dpbRefList;

		*pHevc = nvVideoH265PicParameters();

		pPerFrameDecodeParameters->pCurrentPictureParameters	= StdVideoPictureParametersSet::StdVideoPictureParametersSetFromBase(pin->pPpsClientObject);
		pPerFrameDecodeParameters->decodeFrameInfo.pNext		= &pHevc->pictureInfo;

		pPictureInfo->sType				= VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PICTURE_INFO_KHR;
		pPictureInfo->pNext				= DE_NULL;
		pPictureInfo->pStdPictureInfo	= &pHevc->stdPictureInfo;

		pDecodePictureInfo->videoFrameType = 0;
		if (pNvidiaVulkanParserPictureData->CodecSpecific.h265.mv_hevc_enable)
		{
			pDecodePictureInfo->viewId = pNvidiaVulkanParserPictureData->CodecSpecific.h265.nuh_layer_id;
		}
		else
		{
			pDecodePictureInfo->viewId = 0;
		}

		pPictureInfo->sliceSegmentCount				= pNvidiaVulkanParserPictureData->nNumSlices;
		pPictureInfo->pSliceSegmentOffsets			= static_cast<uint32_t*>(copyToHeap(heap, pNvidiaVulkanParserPictureData->pSliceDataOffsets, sizeof(uint32_t) * pNvidiaVulkanParserPictureData->nNumSlices));

		pStdPictureInfo->sps_video_parameter_set_id		= pin->vps_video_parameter_set_id;	// VPS ID
		pStdPictureInfo->pps_pic_parameter_set_id		= pin->pic_parameter_set_id;		// PPS ID
		pStdPictureInfo->flags.IrapPicFlag				= (pin->IrapPicFlag ? 1 : 0);		// Intra Random Access Point for current picture.
		pStdPictureInfo->flags.IdrPicFlag				= (pin->IdrPicFlag ? 1 : 0);		// Instantaneous Decoding Refresh for current picture.
		pStdPictureInfo->NumBitsForSTRefPicSetInSlice	= (uint16_t)pin->NumBitsForShortTermRPSInSlice;
		pStdPictureInfo->NumDeltaPocsOfRefRpsIdx		= (uint8_t)pin->NumDeltaPocsOfRefRpsIdx;
		pStdPictureInfo->PicOrderCntVal					= pin->CurrPicOrderCntVal;

		int8_t dpbSlot = AllocateDpbSlotForCurrentH265(GetPic(pNvidiaVulkanParserPictureData->pCurrPic), true);

		pHevc->setupSlotInfo.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_DPB_SLOT_INFO_KHR;
		pHevc->setupSlotInfo.pNext = nullptr; // No more extension structures.
		StdVideoDecodeH265ReferenceInfo referenceInfo = StdVideoDecodeH265ReferenceInfo();
		pHevc->setupSlotInfo.pStdReferenceInfo = &referenceInfo; // No more extension structures.
		pSetupReferenceSlot->slotIndex = dpbSlot;
		pSetupReferenceSlot->pNext = &pHevc->setupSlotInfo;

		// slotLayer requires NVIDIA specific extension VK_KHR_video_layers, not
		// enabled, just yet. setupReferenceSlot.slotLayerIndex = 0;
		DE_ASSERT(!(dpbSlot < 0));

		if (dpbSlot >= 0)
		{
			DE_ASSERT(pNvidiaVulkanParserPictureData->ref_pic_flag);
		}

		pPerFrameDecodeParameters->numGopReferenceSlots = FillDpbH265State(pNvidiaVulkanParserPictureData,
																		   pin,
																		   pDpbRefList,
																		   pStdPictureInfo,
																		   pReferenceSlots,
																		   pPerFrameDecodeParameters->pGopReferenceImagesIndexes);

		DE_ASSERT(!pNvidiaVulkanParserPictureData->ref_pic_flag || (pSetupReferenceSlot->slotIndex >= 0));


		if (pSetupReferenceSlot->slotIndex >= 0)
		{
			if (m_distinctDstDpbImages)
			{
				const int32_t setupSlotNdx = pPerFrameDecodeParameters->numGopReferenceSlots;

				DE_ASSERT(setupSlotNdx < PerFrameDecodeParameters::MAX_DPB_REF_SLOTS);

				pReferenceSlots[setupSlotNdx] = *pSetupReferenceSlot;

				pSetupReferenceSlot = &pReferenceSlots[setupSlotNdx];

				pPerFrameDecodeParameters->pictureResources[setupSlotNdx] = pPerFrameDecodeParameters->decodeFrameInfo.dstPictureResource;

				pSetupReferenceSlot->pPictureResource = &pPerFrameDecodeParameters->pictureResources[setupSlotNdx];
			}
			else
			{
				pSetupReferenceSlot->pPictureResource = &pPerFrameDecodeParameters->decodeFrameInfo.dstPictureResource;
			}

			pPerFrameDecodeParameters->decodeFrameInfo.pSetupReferenceSlot	= pSetupReferenceSlot;
		}

		if (pPerFrameDecodeParameters->numGopReferenceSlots)
		{
			for (int32_t dpbEntryIdx = 0; dpbEntryIdx < pPerFrameDecodeParameters->numGopReferenceSlots; dpbEntryIdx++)
			{
				pPerFrameDecodeParameters->pictureResources[dpbEntryIdx].sType	= VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR;
				pReferenceSlots[dpbEntryIdx].pPictureResource					= &pPerFrameDecodeParameters->pictureResources[dpbEntryIdx];

				DE_ASSERT(pDpbRefList[dpbEntryIdx].IsReference());
			}

			pPerFrameDecodeParameters->decodeFrameInfo.pReferenceSlots		= pReferenceSlots;
			pPerFrameDecodeParameters->decodeFrameInfo.referenceSlotCount	= pPerFrameDecodeParameters->numGopReferenceSlots;
		}
		else
		{
			pPerFrameDecodeParameters->decodeFrameInfo.pReferenceSlots		= DE_NULL;
			pPerFrameDecodeParameters->decodeFrameInfo.referenceSlotCount	= 0;
		}
	}

	pDecodePictureInfo->displayWidth	= m_nvidiaVulkanParserSequenceInfo.nDisplayWidth;
	pDecodePictureInfo->displayHeight	= m_nvidiaVulkanParserSequenceInfo.nDisplayHeight;

	bRet = DecodePictureWithParameters(pPictureParams, pDecodePictureInfo, heap) >= 0;

	m_nCurrentPictureID++;

	return bRet;
}

// FillDpbState
uint32_t VideoBaseDecoder::FillDpbH264State (const NvidiaVulkanParserPictureData*	pNvidiaVulkanParserPictureData,
											 const NvidiaVulkanParserH264DpbEntry*	dpbIn,
											 uint32_t								maxDpbInSlotsInUse,
											 NvidiaVideoDecodeH264DpbSlotInfo*		pDpbRefList,
											 VkVideoReferenceSlotInfoKHR*			pReferenceSlots,
											 int8_t*								pGopReferenceImagesIndexes,
											 StdVideoDecodeH264PictureInfoFlags		currPicFlags,
											 int32_t*								pCurrAllocatedSlotIndex)
{
	// #### Update m_dpb based on dpb parameters ####
	// Create unordered DPB and generate a bitmask of all render targets present in DPB
	uint32_t num_ref_frames = pNvidiaVulkanParserPictureData->CodecSpecific.h264.pStdSps->max_num_ref_frames;

	DE_ASSERT(num_ref_frames <= m_maxNumDpbSurfaces);
	DE_UNREF(num_ref_frames);

	dpbEntry refOnlyDpbIn[AVC_MAX_DPB_SLOTS]; // max number of Dpb // surfaces
	deMemset(&refOnlyDpbIn, 0, m_maxNumDpbSurfaces * sizeof(refOnlyDpbIn[0]));

	uint32_t refDpbUsedAndValidMask	= 0;
	uint32_t numUsedRef				= 0;

	for (uint32_t inIdx = 0; inIdx < maxDpbInSlotsInUse; inIdx++)
	{
		// used_for_reference: 0 = unused, 1 = top_field, 2 = bottom_field, 3 = both_fields
		const uint32_t used_for_reference = dpbIn[inIdx].used_for_reference & fieldIsReferenceMask;

		if (used_for_reference)
		{
			const int8_t	picIdx					= (!dpbIn[inIdx].not_existing && dpbIn[inIdx].pNvidiaVulkanPicture)
													? GetPicIdx(dpbIn[inIdx].pNvidiaVulkanPicture)
													: -1;
			const bool		isFieldRef				= (picIdx >= 0)
													? GetFieldPicFlag(picIdx)
													: (used_for_reference && (used_for_reference != fieldIsReferenceMask));
			const int16_t	fieldOrderCntList[2]	=
			{
				(int16_t)dpbIn[inIdx].FieldOrderCnt[0],
				(int16_t)dpbIn[inIdx].FieldOrderCnt[1]
			};

			refOnlyDpbIn[numUsedRef].setReferenceAndTopBoottomField(
				!!used_for_reference,
				(picIdx < 0), /* not_existing is frame inferred by the decoding process for gaps in frame_num */
				!!dpbIn[inIdx].is_long_term,
				isFieldRef,
				!!(used_for_reference & topFieldMask),
				!!(used_for_reference & bottomFieldMask),
				(int16_t)dpbIn[inIdx].FrameIdx,
				fieldOrderCntList,
				GetPic(dpbIn[inIdx].pNvidiaVulkanPicture));

			if (picIdx >= 0)
			{
				refDpbUsedAndValidMask |= (1 << picIdx);
			}

			numUsedRef++;
		}
		// Invalidate all slots.
		pReferenceSlots[inIdx].slotIndex = -1;
		pGopReferenceImagesIndexes[inIdx] = -1;
	}

	DE_ASSERT(numUsedRef <= 16);
	DE_ASSERT(numUsedRef <= m_maxNumDpbSurfaces);
	DE_ASSERT(numUsedRef <= num_ref_frames);

	// Map all frames not present in DPB as non-reference, and generate a mask of all used DPB entries
	/* uint32_t destUsedDpbMask = */ ResetPicDpbSlots(refDpbUsedAndValidMask);

	// Now, map DPB render target indices to internal frame buffer index,
	// assign each reference a unique DPB entry, and create the ordered DPB
	// This is an undocumented MV restriction: the position in the DPB is stored
	// along with the co-located data, so once a reference frame is assigned a DPB
	// entry, it can no longer change.

	// Find or allocate slots for existing dpb items.
	// Take into account the reference picture now.
	int8_t currPicIdx				= GetPicIdx(pNvidiaVulkanParserPictureData->pCurrPic);
	int8_t bestNonExistingPicIdx	= currPicIdx;

	DE_ASSERT(currPicIdx >= 0);

	if (refDpbUsedAndValidMask)
	{
		int32_t minFrameNumDiff = 0x10000;

		for (int32_t dpbIdx = 0; (uint32_t)dpbIdx < numUsedRef; dpbIdx++)
		{
			if (!refOnlyDpbIn[dpbIdx].is_non_existing)
			{
				NvidiaVulkanPictureBase*	picBuff	= refOnlyDpbIn[dpbIdx].m_picBuff;
				int8_t						picIdx	= GetPicIdx(picBuff); // should always be valid at this point

				DE_ASSERT(picIdx >= 0);

				// We have up to 17 internal frame buffers, but only MAX_DPB_SIZE dpb
				// entries, so we need to re-map the index from the [0..MAX_DPB_SIZE]
				// range to [0..15]
				int8_t dpbSlot = GetPicDpbSlot(picIdx);

				if (dpbSlot < 0)
				{
					dpbSlot = m_dpb.AllocateSlot();

					DE_ASSERT((dpbSlot >= 0) && ((uint32_t)dpbSlot < m_maxNumDpbSurfaces));

					SetPicDpbSlot(picIdx, dpbSlot);

					m_dpb[dpbSlot].setPictureResource(picBuff);
				}

				m_dpb[dpbSlot].MarkInUse();

				DE_ASSERT(dpbSlot >= 0); // DPB mapping logic broken!

				refOnlyDpbIn[dpbIdx].dpbSlot = dpbSlot;

				int32_t frameNumDiff = ((int32_t)pNvidiaVulkanParserPictureData->CodecSpecific.h264.frame_num - refOnlyDpbIn[dpbIdx].FrameIdx);

				if (frameNumDiff <= 0)
				{
					frameNumDiff = 0xffff;
				}

				if (frameNumDiff < minFrameNumDiff)
				{
					bestNonExistingPicIdx = picIdx;
					minFrameNumDiff = frameNumDiff;
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
	// get freed right after usage. if (pNvidiaVulkanParserPictureData->ref_pic_flag)
	int8_t currPicDpbSlot = AllocateDpbSlotForCurrentH264(GetPic(pNvidiaVulkanParserPictureData->pCurrPic), currPicFlags);

	DE_ASSERT(currPicDpbSlot >= 0);

	*pCurrAllocatedSlotIndex = currPicDpbSlot;

	if (refDpbUsedAndValidMask)
	{
		// Find or allocate slots for non existing dpb items and populate the slots.
		uint32_t	dpbInUseMask			= m_dpb.getSlotInUseMask();
		int8_t		firstNonExistingDpbSlot	= 0;

		for (uint32_t dpbIdx = 0; dpbIdx < numUsedRef; dpbIdx++)
		{
			int8_t dpbSlot	= -1;
			int8_t picIdx	= -1;

			if (refOnlyDpbIn[dpbIdx].is_non_existing)
			{
				DE_ASSERT(refOnlyDpbIn[dpbIdx].m_picBuff == DE_NULL);

				while (((uint32_t)firstNonExistingDpbSlot < m_maxNumDpbSurfaces) && (dpbSlot == -1))
				{
					if (!(dpbInUseMask & (1 << firstNonExistingDpbSlot)))
					{
						dpbSlot = firstNonExistingDpbSlot;
					}

					firstNonExistingDpbSlot++;
				}

				picIdx = bestNonExistingPicIdx;

				// Find the closest valid refpic already in the DPB
				uint32_t minDiffPOC = 0x7fff;

				for (uint32_t j = 0; j < numUsedRef; j++)
				{
					if (!refOnlyDpbIn[j].is_non_existing && (refOnlyDpbIn[j].used_for_reference & refOnlyDpbIn[dpbIdx].used_for_reference) == refOnlyDpbIn[dpbIdx].used_for_reference)
					{
						uint32_t diffPOC = abs((int32_t)(refOnlyDpbIn[j].FieldOrderCnt[0] - refOnlyDpbIn[dpbIdx].FieldOrderCnt[0]));

						if (diffPOC <= minDiffPOC)
						{
							minDiffPOC = diffPOC;
							picIdx = GetPicIdx(refOnlyDpbIn[j].m_picBuff);
						}
					}
				}
			}
			else
			{
				DE_ASSERT(refOnlyDpbIn[dpbIdx].m_picBuff != DE_NULL);

				dpbSlot	= refOnlyDpbIn[dpbIdx].dpbSlot;
				picIdx	= GetPicIdx(refOnlyDpbIn[dpbIdx].m_picBuff);
			}

			DE_ASSERT((dpbSlot >= 0) && ((uint32_t)dpbSlot < m_maxNumDpbSurfaces));

			refOnlyDpbIn[dpbIdx].setH264PictureData(pDpbRefList, pReferenceSlots, dpbIdx, dpbSlot, pNvidiaVulkanParserPictureData->progressive_frame);

			pGopReferenceImagesIndexes[dpbIdx] = picIdx;
		}
	}

	return refDpbUsedAndValidMask ? numUsedRef : 0;
}

uint32_t VideoBaseDecoder::FillDpbH265State (const NvidiaVulkanParserPictureData*		pNvidiaVulkanParserPictureData,
											 const NvidiaVulkanParserH265PictureData*	pin,
											 NvidiaVideoDecodeH265DpbSlotInfo*			pDpbSlotInfo,
											 StdVideoDecodeH265PictureInfo*				pStdPictureInfo,
											 VkVideoReferenceSlotInfoKHR*					pReferenceSlots,
											 int8_t*									pGopReferenceImagesIndexes)
{
	// #### Update m_dpb based on dpb parameters ####
	// Create unordered DPB and generate a bitmask of all render targets present in DPB

	dpbEntry refOnlyDpbIn[AVC_MAX_DPB_SLOTS];
	deMemset(&refOnlyDpbIn, 0, m_maxNumDpbSurfaces * sizeof(refOnlyDpbIn[0]));
	uint32_t refDpbUsedAndValidMask = 0;
	uint32_t numUsedRef = 0;

	for (int32_t inIdx = 0; inIdx < HEVC_MAX_DPB_SLOTS; inIdx++)
	{
		// used_for_reference: 0 = unused, 1 = top_field, 2 = bottom_field, 3 = both_fields
		int8_t picIdx = GetPicIdx(pin->RefPics[inIdx]);
		if (picIdx >= 0)
		{
			refOnlyDpbIn[numUsedRef].setReference((pin->IsLongTerm[inIdx] == 1), pin->PicOrderCntVal[inIdx], GetPic(pin->RefPics[inIdx]));

			refDpbUsedAndValidMask |= (1 << picIdx);

			refOnlyDpbIn[numUsedRef].originalDpbIndex = (int8_t)inIdx;
			numUsedRef++;
		}
		// Invalidate all slots.
		pReferenceSlots[inIdx].slotIndex = -1;
		pGopReferenceImagesIndexes[inIdx] = -1;
	}

	DE_ASSERT(numUsedRef <= m_maxNumDpbSurfaces);

	// Take into account the reference picture now.
	int8_t currPicIdx = GetPicIdx(pNvidiaVulkanParserPictureData->pCurrPic);
	int8_t currPicDpbSlot = -1;

	DE_ASSERT(currPicIdx >= 0);

	if (currPicIdx >= 0)
	{
		currPicDpbSlot = GetPicDpbSlot(currPicIdx);
		refDpbUsedAndValidMask |= (1 << currPicIdx);
	}

	DE_UNREF(currPicDpbSlot);
	DE_ASSERT(currPicDpbSlot >= 0);

	// Map all frames not present in DPB as non-reference, and generate a mask of
	// all used DPB entries
	/* uint32_t destUsedDpbMask = */ ResetPicDpbSlots(refDpbUsedAndValidMask);

	// Now, map DPB render target indices to internal frame buffer index,
	// assign each reference a unique DPB entry, and create the ordered DPB
	// This is an undocumented MV restriction: the position in the DPB is stored
	// along with the co-located data, so once a reference frame is assigned a DPB
	// entry, it can no longer change.

	int8_t frmListToDpb[HEVC_MAX_DPB_SLOTS]; // TODO change to -1 for invalid indexes.
	deMemset(&frmListToDpb, 0, sizeof(frmListToDpb));

	// Find or allocate slots for existing dpb items.
	for (int32_t dpbIdx = 0; (uint32_t)dpbIdx < numUsedRef; dpbIdx++)
	{
		if (!refOnlyDpbIn[dpbIdx].is_non_existing)
		{
			NvidiaVulkanPictureBase* picBuff = refOnlyDpbIn[dpbIdx].m_picBuff;

			int8_t picIdx = GetPicIdx(picBuff); // should always be valid at this point

			DE_ASSERT(picIdx >= 0);
			// We have up to 17 internal frame buffers, but only HEVC_MAX_DPB_SLOTS
			// dpb entries, so we need to re-map the index from the
			// [0..HEVC_MAX_DPB_SLOTS] range to [0..15]

			int8_t dpbSlot = GetPicDpbSlot(picIdx);

			if (dpbSlot < 0)
			{
				dpbSlot = m_dpb.AllocateSlot();

				DE_ASSERT(dpbSlot >= 0);

				SetPicDpbSlot(picIdx, dpbSlot);

				m_dpb[dpbSlot].setPictureResource(picBuff);
			}

			m_dpb[dpbSlot].MarkInUse();

			DE_ASSERT(dpbSlot >= 0); // DPB mapping logic broken!

			refOnlyDpbIn[dpbIdx].dpbSlot = dpbSlot;

			uint32_t originalDpbIndex = refOnlyDpbIn[dpbIdx].originalDpbIndex;

			DE_ASSERT(originalDpbIndex < HEVC_MAX_DPB_SLOTS);

			frmListToDpb[originalDpbIndex] = dpbSlot;
		}
	}

	// Find or allocate slots for non existing dpb items and populate the slots.
	uint32_t	dpbInUseMask			= m_dpb.getSlotInUseMask();
	int8_t		firstNonExistingDpbSlot	= 0;

	for (uint32_t dpbIdx = 0; dpbIdx < numUsedRef; dpbIdx++)
	{
		int8_t dpbSlot = -1;

		if (refOnlyDpbIn[dpbIdx].is_non_existing)
		{
			// There shouldn't be  not_existing in h.265
			DE_ASSERT(0);
			DE_ASSERT(refOnlyDpbIn[dpbIdx].m_picBuff == DE_NULL);

			while (((uint32_t)firstNonExistingDpbSlot < m_maxNumDpbSurfaces) && (dpbSlot == -1))
			{
				if (!(dpbInUseMask & (1 << firstNonExistingDpbSlot)))
				{
					dpbSlot = firstNonExistingDpbSlot;
				}
				firstNonExistingDpbSlot++;
			}

			DE_ASSERT((dpbSlot >= 0) && ((uint32_t)dpbSlot < m_maxNumDpbSurfaces));
		}
		else
		{
			DE_ASSERT(refOnlyDpbIn[dpbIdx].m_picBuff != DE_NULL);
			dpbSlot = refOnlyDpbIn[dpbIdx].dpbSlot;
		}

		DE_ASSERT((dpbSlot >= 0) && (dpbSlot < HEVC_MAX_DPB_SLOTS));

		refOnlyDpbIn[dpbIdx].setH265PictureData(pDpbSlotInfo, pReferenceSlots, dpbIdx, dpbSlot);
		pGopReferenceImagesIndexes[dpbIdx] = GetPicIdx(refOnlyDpbIn[dpbIdx].m_picBuff);
	}

	int32_t			numPocStCurrBefore		= 0;
	const size_t	maxNumPocStCurrBefore	= sizeof(pStdPictureInfo->RefPicSetStCurrBefore) / sizeof(pStdPictureInfo->RefPicSetStCurrBefore[0]);

	DE_UNREF(maxNumPocStCurrBefore);
	DE_ASSERT((size_t)pin->NumPocStCurrBefore < maxNumPocStCurrBefore);

	for (int32_t i = 0; i < pin->NumPocStCurrBefore; i++)
	{
		uint8_t idx = (uint8_t)pin->RefPicSetStCurrBefore[i];

		if (idx < HEVC_MAX_DPB_SLOTS)
		{
			pStdPictureInfo->RefPicSetStCurrBefore[numPocStCurrBefore++] = frmListToDpb[idx] & 0xf;
		}
	}
	while (numPocStCurrBefore < 8)
	{
		pStdPictureInfo->RefPicSetStCurrBefore[numPocStCurrBefore++] = 0xff;
	}

	int32_t			numPocStCurrAfter		= 0;
	const size_t	maxNumPocStCurrAfter	= sizeof(pStdPictureInfo->RefPicSetStCurrAfter) / sizeof(pStdPictureInfo->RefPicSetStCurrAfter[0]);

	DE_UNREF(maxNumPocStCurrAfter);
	DE_ASSERT((size_t)pin->NumPocStCurrAfter < maxNumPocStCurrAfter);

	for (int32_t i = 0; i < pin->NumPocStCurrAfter; i++)
	{
		uint8_t idx = (uint8_t)pin->RefPicSetStCurrAfter[i];

		if (idx < HEVC_MAX_DPB_SLOTS)
		{
			pStdPictureInfo->RefPicSetStCurrAfter[numPocStCurrAfter++] = frmListToDpb[idx] & 0xf;
		}
	}

	while (numPocStCurrAfter < 8)
	{
		pStdPictureInfo->RefPicSetStCurrAfter[numPocStCurrAfter++] = 0xff;
	}

	int32_t			numPocLtCurr	= 0;
	const size_t	maxNumPocLtCurr	= sizeof(pStdPictureInfo->RefPicSetLtCurr) / sizeof(pStdPictureInfo->RefPicSetLtCurr[0]);

	DE_UNREF(maxNumPocLtCurr);
	DE_ASSERT((size_t)pin->NumPocLtCurr < maxNumPocLtCurr);

	for (int32_t i = 0; i < pin->NumPocLtCurr; i++)
	{
		uint8_t idx = (uint8_t)pin->RefPicSetLtCurr[i];

		if (idx < HEVC_MAX_DPB_SLOTS)
		{
			pStdPictureInfo->RefPicSetLtCurr[numPocLtCurr++] = frmListToDpb[idx] & 0xf;
		}
	}

	while (numPocLtCurr < 8)
	{
		pStdPictureInfo->RefPicSetLtCurr[numPocLtCurr++] = 0xff;
	}

	return numUsedRef;
}

int8_t VideoBaseDecoder::AllocateDpbSlotForCurrentH264 (NvidiaVulkanPictureBase*			pNvidiaVulkanPictureBase,
														StdVideoDecodeH264PictureInfoFlags	currPicFlags)
{
	// Now, map the current render target
	int8_t dpbSlot		= -1;
	int8_t currPicIdx	= GetPicIdx(pNvidiaVulkanPictureBase);

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

			m_dpb[dpbSlot].setPictureResource(pNvidiaVulkanPictureBase);
		}

		DE_ASSERT(dpbSlot >= 0);
	}

	return dpbSlot;
}

int8_t VideoBaseDecoder::AllocateDpbSlotForCurrentH265 (NvidiaVulkanPictureBase*	pNvidiaVulkanPictureBase,
														bool						isReference)
{
	// Now, map the current render target
	int8_t dpbSlot		= -1;
	int8_t currPicIdx	= GetPicIdx(pNvidiaVulkanPictureBase);

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

			m_dpb[dpbSlot].setPictureResource(pNvidiaVulkanPictureBase);
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

vector<pair<VkFormat, VkImageUsageFlags>> getImageFormatAndUsageForOutputAndDPB (const InstanceInterface&	vk,
																				 const VkPhysicalDevice		physicalDevice,
																				 const VkVideoProfileListInfoKHR*	videoProfileList,
																				 const VkFormat				recommendedFormat,
																				 const bool					distinctDstDpbImages)
{
	const VkImageUsageFlags						dstFormatUsages		= VK_IMAGE_USAGE_TRANSFER_SRC_BIT
																	| VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR;
	const VkImageUsageFlags						dpbFormatUsages		= VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR;
	const VkImageUsageFlags						bothImageUsages		= dstFormatUsages | dpbFormatUsages;
	VkFormat									dstFormat			= VK_FORMAT_UNDEFINED;
	VkFormat									dpbFormat			= VK_FORMAT_UNDEFINED;
	vector<pair<VkFormat, VkImageUsageFlags>>	result;

	// Check if both image usages are not supported on this platform
	if (!distinctDstDpbImages)
	{
		const MovePtr<vector<VkFormat>>				bothUsageFormats = getSupportedFormats(vk, physicalDevice, bothImageUsages, videoProfileList);
		VkFormat pickedFormat = getRecommendedFormat(*bothUsageFormats, recommendedFormat);

		result.push_back(pair<VkFormat, VkImageUsageFlags>(pickedFormat, bothImageUsages));
		result.push_back(pair<VkFormat, VkImageUsageFlags>(pickedFormat, VkImageUsageFlags()));
	}
	else
	{
		{
			const MovePtr<vector<VkFormat>>	dstUsageFormats = getSupportedFormats(vk, physicalDevice, dstFormatUsages, videoProfileList);

			if (dstUsageFormats == DE_NULL)
				TCU_FAIL("Implementation must report format for VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR");

			dstFormat = getRecommendedFormat(*dstUsageFormats, recommendedFormat);

			if (dstFormat == VK_FORMAT_UNDEFINED)
				TCU_FAIL("Implementation must report format for VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR");

			result.push_back(pair<VkFormat, VkImageUsageFlags>(dstFormat, dstFormatUsages));
		}

		{
			const MovePtr<vector<VkFormat>>	dpbUsageFormats = getSupportedFormats(vk, physicalDevice, dpbFormatUsages, videoProfileList);

			if (dpbUsageFormats == DE_NULL)
				TCU_FAIL("Implementation must report format for VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR");

			dpbFormat = getRecommendedFormat(*dpbUsageFormats, recommendedFormat);

			result.push_back(pair<VkFormat, VkImageUsageFlags>(dpbFormat, dpbFormatUsages));
		}
	}

	DE_ASSERT(result.size() == 2);

	return result;
}

/* Callback function to be registered for getting a callback when decoding of
 * sequence starts. Return value from HandleVideoSequence() are interpreted as :
 *  0: fail, 1: suceeded, > 1: override dpb size of parser (set by
 * nvVideoParseParameters::ulMaxNumDecodeSurfaces while creating parser)
 */
int32_t VideoBaseDecoder::StartVideoSequence (const VulkanParserDetectedVideoFormat* pVideoFormat)
{
	const InstanceInterface&	vki					= m_context.getInstanceInterface();
	const VkPhysicalDevice		physDevice			= m_context.getPhysicalDevice();
	const VkDevice				device				= getDevice();
	const DeviceInterface&		vkd					= getDeviceDriver();
	const deUint32				queueFamilyIndex	= getQueueFamilyIndexDecode();
	Allocator&					allocator			= getAllocator();
	const VkDeviceSize			bufferSize			= 4 * 1024 * 1024;

	DE_ASSERT(m_videoFrameBuffer != DE_NULL);
	DE_ASSERT(m_videoCodecOperation == pVideoFormat->codec); // Make sure video have same format the queue was created for
	DE_ASSERT(VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR == pVideoFormat->chromaSubsampling || VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR == pVideoFormat->chromaSubsampling || VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR == pVideoFormat->chromaSubsampling || VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR == pVideoFormat->chromaSubsampling);

	const VkVideoCodecOperationFlagBitsKHR					videoCodec							= pVideoFormat->codec;
	const uint32_t											maxDpbSlotCount						= pVideoFormat->maxNumDpbSlots; // This is currently configured by the parser to maxNumDpbSlots from the stream plus 1 for the current slot on the fly
	const bool												semiPlanarFormat					= pVideoFormat->chromaSubsampling != VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR;
	const VkVideoChromaSubsamplingFlagBitsKHR				chromaSubsampling					= pVideoFormat->chromaSubsampling;
	const VkVideoComponentBitDepthFlagsKHR					lumaBitDepth						= getLumaBitDepth(pVideoFormat->bit_depth_luma_minus8);
	const VkVideoComponentBitDepthFlagsKHR					chromaBitDepth						= getChromaBitDepth(pVideoFormat->bit_depth_chroma_minus8);
	const VkFormat											videoVkFormat						= codecGetVkFormat(chromaSubsampling, lumaBitDepth, semiPlanarFormat);
	const VkExtent2D										codedExtent							= { pVideoFormat->coded_width, pVideoFormat->coded_height };
	const bool												h264								= (videoCodec == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR);
	const bool												h265								= (videoCodec == VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR);

	DE_ASSERT(pVideoFormat->coded_width <= 3840);
	DE_ASSERT(videoVkFormat == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM);
	DE_ASSERT(h264 || h265);

	m_numDecodeSurfaces = std::max(m_gopSize * m_dpbCount, 4u);

	const MovePtr<VkVideoDecodeH264ProfileInfoKHR>				videoProfileExtentionH264			= getVideoProfileExtensionH264D();
	const MovePtr<VkVideoDecodeH265ProfileInfoKHR>				videoProfileExtentionH265			= getVideoProfileExtensionH265D();
	void*													videoProfileExtention				= h264 ? (void*)videoProfileExtentionH264.get()
																								: h265 ? (void*)videoProfileExtentionH265.get()
																								: DE_NULL;
	const MovePtr<VkVideoProfileInfoKHR>						videoProfile						= getVideoProfile(videoCodec, videoProfileExtention, chromaSubsampling, lumaBitDepth, chromaBitDepth);
	const MovePtr<VkVideoProfileListInfoKHR>					videoProfileList					= getVideoProfileList(videoProfile.get());


	const MovePtr<VkVideoDecodeH264CapabilitiesKHR>			videoCapabilitiesExtension264D		= getVideoCapabilitiesExtensionH264D();
	const MovePtr<VkVideoDecodeH265CapabilitiesKHR>			videoCapabilitiesExtension265D		= getVideoCapabilitiesExtensionH265D();
	void*													videoCapabilitiesExtension			= h264 ? (void*)videoCapabilitiesExtension264D.get()
																								: h265 ? (void*)videoCapabilitiesExtension265D.get()
																								: DE_NULL;
	MovePtr<VkVideoDecodeCapabilitiesKHR>					videoDecodeCapabilities				= getVideoDecodeCapabilities(videoCapabilitiesExtension);
	const MovePtr<VkVideoCapabilitiesKHR>					videoCapabilites					= getVideoCapabilities(vki, physDevice, videoProfile.get(), videoDecodeCapabilities.get());

	DEBUGLOG(std::cout << "StartVideoSequence: maxDpbSlots=" << videoCapabilites->maxDpbSlots << " maxActiveReferencePictures=" << videoCapabilites->maxActiveReferencePictures << std::endl);

	const bool												videoExtentSupported				= validateVideoExtent(codedExtent, *videoCapabilites);

	m_distinctDstDpbImages = (videoDecodeCapabilities->flags & VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_DISTINCT_BIT_KHR) ? true : false;

	const vector<pair<VkFormat, VkImageUsageFlags>>			imageFormatAndUsageForOutputAndDPB	= getImageFormatAndUsageForOutputAndDPB(vki, physDevice, videoProfileList.get(), videoVkFormat, m_distinctDstDpbImages);

	const bool												outFormatValidate					= validateFormatSupport(vki, physDevice, imageFormatAndUsageForOutputAndDPB[0].second, videoProfileList.get(), imageFormatAndUsageForOutputAndDPB[0].first);
	const bool												isVideoProfileSutable				= validateVideoProfileList(vki, physDevice, videoProfileList.get(), imageFormatAndUsageForOutputAndDPB[0].first, imageFormatAndUsageForOutputAndDPB[0].second);

	DE_UNREF(outFormatValidate);
	DE_UNREF(isVideoProfileSutable);

	const VkFormat											outPictureFormat					= imageFormatAndUsageForOutputAndDPB[0].first;
	const VkImageUsageFlags									outPictureUsage						= imageFormatAndUsageForOutputAndDPB[0].second;
	const VkFormat											dpbPictureFormat					= imageFormatAndUsageForOutputAndDPB[1].first;
	const VkImageUsageFlags									dpbPictureUsage						= imageFormatAndUsageForOutputAndDPB[1].second;
	const deUint32											dpbPictureImageLayers				= (videoCapabilites->flags & VK_VIDEO_CAPABILITY_SEPARATE_REFERENCE_IMAGES_BIT_KHR) ? 1 : m_numDecodeSurfaces;
	const VkImageCreateInfo									outImageCreateInfo					= makeImageCreateInfo(outPictureFormat, codedExtent, &queueFamilyIndex, outPictureUsage, videoProfileList.get());
	const VkImageCreateInfo									dpbImageCreateInfo					= makeImageCreateInfo(dpbPictureFormat, codedExtent, &queueFamilyIndex, dpbPictureUsage, videoProfileList.get(), dpbPictureImageLayers);
	const VkImageCreateInfo*								pDpbImageCreateInfo					= dpbPictureUsage == static_cast<VkImageUsageFlags>(0) ? DE_NULL : &dpbImageCreateInfo;

	if (m_width == 0 || m_height == 0)
	{
		const MovePtr<VkVideoSessionCreateInfoKHR>				videoSessionCreateInfo				= getVideoSessionCreateInfo(queueFamilyIndex,
																																videoProfile.get(),
																																codedExtent,
																																outPictureFormat,
																																dpbPictureFormat,
																																std::min(maxDpbSlotCount, videoCapabilites->maxDpbSlots),
																																std::min(maxDpbSlotCount, videoCapabilites->maxActiveReferencePictures));
		Move<VkVideoSessionKHR>									videoSession						= createVideoSessionKHR(vkd, device, videoSessionCreateInfo.get());
		vector<AllocationPtr>									allocations							= getAndBindVideoSessionMemory(vkd, device, *videoSession, allocator);

		DE_UNREF(videoExtentSupported);

		m_minBitstreamBufferSizeAlignment	= videoCapabilites->minBitstreamBufferSizeAlignment;
		m_minBitstreamBufferOffsetAlignment	= videoCapabilites->minBitstreamBufferOffsetAlignment;
		m_videoDecodeSessionAllocs.swap(allocations);

		m_videoDecodeSession		= videoSession;
		m_videoCodecOperation		= pVideoFormat->codec;
		m_chromaFormat				= pVideoFormat->chromaSubsampling;
		m_bitLumaDepthMinus8		= pVideoFormat->bit_depth_luma_minus8;
		m_bitChromaDepthMinus8		= pVideoFormat->bit_depth_chroma_minus8;
		m_videoFormat				= *pVideoFormat;
		m_codedWidth				= pVideoFormat->coded_width;
		m_codedHeight				= pVideoFormat->coded_height;
		m_width						= pVideoFormat->display_area.right - pVideoFormat->display_area.left;
		m_height					= pVideoFormat->display_area.bottom - pVideoFormat->display_area.top;
		m_maxDecodeFramesAllocated	= std::max((uint32_t)m_frameCountTrigger, m_numDecodeSurfaces);
		m_maxDecodeFramesCount		= m_numDecodeSurfaces;

		DE_ASSERT(m_maxDecodeFramesCount <= m_maxDecodeFramesAllocated);

		m_videoFrameBuffer->InitImagePool(vkd, device, queueFamilyIndex, allocator, m_maxDecodeFramesCount, m_maxDecodeFramesAllocated, &outImageCreateInfo, pDpbImageCreateInfo, videoProfile.get());

		m_decodeFramesData			= new NvVkDecodeFrameData[m_maxDecodeFramesAllocated];
		m_videoCommandPool			= makeCommandPool(vkd, device, queueFamilyIndex);

		for (uint32_t decodeFrameId = 0; decodeFrameId < m_maxDecodeFramesCount; decodeFrameId++)
		{
			m_decodeFramesData[decodeFrameId].bitstreamBuffer.CreateVideoBitstreamBuffer(vkd, device, allocator, bufferSize, m_minBitstreamBufferOffsetAlignment, m_minBitstreamBufferSizeAlignment, videoProfileList.get());
			m_decodeFramesData[decodeFrameId].commandBuffer = allocateCommandBuffer(vkd, device, *m_videoCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		}
	}
	else if (m_maxDecodeFramesCount < m_maxDecodeFramesAllocated)
	{
		const uint32_t	firstIndex	= m_maxDecodeFramesCount;

		DE_ASSERT(m_maxDecodeFramesCount > 0);

		m_maxDecodeFramesCount += m_gopSize;

		DE_ASSERT(m_maxDecodeFramesCount <= m_maxDecodeFramesAllocated);

		m_numDecodeSurfaces	= m_maxDecodeFramesCount;
		m_codedWidth		= pVideoFormat->coded_width;
		m_codedHeight		= pVideoFormat->coded_height;
		m_width				= pVideoFormat->display_area.right - pVideoFormat->display_area.left;
		m_height			= pVideoFormat->display_area.bottom - pVideoFormat->display_area.top;

		m_videoFrameBuffer->InitImagePool(vkd, device, queueFamilyIndex, allocator, m_maxDecodeFramesCount, m_maxDecodeFramesAllocated, &outImageCreateInfo, pDpbImageCreateInfo, videoProfile.get());

		for (uint32_t decodeFrameId = firstIndex; decodeFrameId < m_maxDecodeFramesCount; decodeFrameId++)
		{
			m_decodeFramesData[decodeFrameId].bitstreamBuffer.CreateVideoBitstreamBuffer(vkd, device, allocator, bufferSize, m_minBitstreamBufferOffsetAlignment, m_minBitstreamBufferSizeAlignment, videoProfileList.get());
			m_decodeFramesData[decodeFrameId].commandBuffer = allocateCommandBuffer(vkd, device, *m_videoCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		}
	}

	return m_numDecodeSurfaces;
}

bool VideoBaseDecoder::UpdatePictureParametersHandler (NvidiaVulkanPictureParameters*						pNvidiaVulkanPictureParameters,
													   NvidiaSharedBaseObj<NvidiaParserVideoRefCountBase>&	pictureParametersObject,
													   uint64_t												updateSequenceCount)
{
	NvidiaSharedBaseObj<StdVideoPictureParametersSet> pictureParametersSet(StdVideoPictureParametersSet::Create(pNvidiaVulkanPictureParameters, updateSequenceCount));

	DEBUGLOG(std::cout << "\tUpdatePictureParametersHandler::PPS:" << (void*)pictureParametersSet.Get() << std::endl);

	if (!pictureParametersSet)
	{
		DE_ASSERT(0 && "Invalid pictureParametersSet");
		return false;
	}

	const bool hasSpsPpsPair = AddPictureParametersToQueue(pictureParametersSet);

	if (m_videoDecodeSession.get() != DE_NULL && hasSpsPpsPair)
	{
		FlushPictureParametersQueue();
	}

	pictureParametersObject = pictureParametersSet;

	return true;
}

bool VideoBaseDecoder::AddPictureParametersToQueue (NvidiaSharedBaseObj<StdVideoPictureParametersSet>& pictureParametersSet)
{
	bool isVps = false;
	pictureParametersSet->GetVpsId(isVps);

	if (isVps)
	{
		m_pictureParametersQueue.push(pictureParametersSet);

		return false;
	}

	if (!m_pictureParametersQueue.empty())
	{
		m_pictureParametersQueue.push(pictureParametersSet);

		return false;
	}

	bool isSps = false;
	int32_t spsId = pictureParametersSet->GetSpsId(isSps);
	bool isPps = false;
	pictureParametersSet->GetPpsId(isPps);

	// Attempt to combine the pair of SPS/PPS to avid creatingPicture Parameter Objects
	if ((!!m_lastSpsPictureParametersQueue && !!m_lastPpsPictureParametersQueue) ||	// the last slots are already occupied
		(isSps && !!m_lastSpsPictureParametersQueue) ||								// the current one is SPS but SPS slot is already occupied
		(isPps && !!m_lastPpsPictureParametersQueue) ||							// the current one is PPS but PPS slot is already occupied
		((m_lastSpsIdInQueue != -1) && (m_lastSpsIdInQueue != spsId)))				// This has a different spsId
	{
		if (m_lastSpsPictureParametersQueue)
		{
			m_pictureParametersQueue.push(m_lastSpsPictureParametersQueue);
			m_lastSpsPictureParametersQueue = DE_NULL;
		}

		if (m_lastPpsPictureParametersQueue)
		{
			m_pictureParametersQueue.push(m_lastPpsPictureParametersQueue);
			m_lastPpsPictureParametersQueue = DE_NULL;
		}

		m_pictureParametersQueue.push(pictureParametersSet);

		m_lastSpsIdInQueue = -1;

		return false;
	}

	if (m_lastSpsIdInQueue == -1)
	{
		m_lastSpsIdInQueue = spsId;
	}

	DE_ASSERT(m_lastSpsIdInQueue != -1);

	if (isSps)
	{
		m_lastSpsPictureParametersQueue = pictureParametersSet;
	}
	else
	{
		m_lastPpsPictureParametersQueue = pictureParametersSet;
	}

	return m_lastSpsPictureParametersQueue && m_lastPpsPictureParametersQueue;
}

void VideoBaseDecoder::FlushPictureParametersQueue ()
{
	NvidiaSharedBaseObj<StdVideoPictureParametersSet> emptyStdPictureParametersSet;

	while (!m_pictureParametersQueue.empty())
	{
		NvidiaSharedBaseObj<StdVideoPictureParametersSet>& ppItem = m_pictureParametersQueue.front();

		bool isSps = false;
		ppItem->GetSpsId(isSps);
		bool isPps = false;
		ppItem->GetPpsId(isPps);
		bool isVps = false;
		ppItem->GetVpsId(isVps);

		if (isSps)
			AddPictureParameters(emptyStdPictureParametersSet, ppItem, emptyStdPictureParametersSet);
		else if (isPps)
			AddPictureParameters(emptyStdPictureParametersSet, emptyStdPictureParametersSet, ppItem);
		else if (isVps)
			AddPictureParameters(ppItem, emptyStdPictureParametersSet, emptyStdPictureParametersSet);
		else
			TCU_THROW(InternalError, "Invalid parameter type in the queue");

		m_pictureParametersQueue.pop();
	}

	AddPictureParameters(m_lastVpsPictureParametersQueue, m_lastSpsPictureParametersQueue, m_lastPpsPictureParametersQueue);

	if (m_lastVpsPictureParametersQueue)
		m_lastVpsPictureParametersQueue = nullptr;

	if (m_lastSpsPictureParametersQueue)
		m_lastSpsPictureParametersQueue = nullptr;

	if (m_lastPpsPictureParametersQueue)
		m_lastPpsPictureParametersQueue = nullptr;

	m_lastSpsIdInQueue = -1;
}

bool VideoBaseDecoder::CheckStdObjectBeforeUpdate (NvidiaSharedBaseObj<StdVideoPictureParametersSet>& stdPictureParametersSet)
{
	if (!stdPictureParametersSet)
	{
		return false;
	}

	bool stdObjectUpdate = (stdPictureParametersSet->m_updateSequenceCount > 0);

	if (!m_currentPictureParameters || stdObjectUpdate)
	{
		DE_ASSERT(m_videoDecodeSession.get() != DE_NULL);
		DE_ASSERT(stdObjectUpdate || (stdPictureParametersSet->m_vkVideoDecodeSession == DE_NULL));
		// DE_ASSERT(!stdObjectUpdate || stdPictureParametersSet->m_vkObjectOwner);
		// Create new Vulkan Picture Parameters object
		return true;

	}
	else
	{
		// new std object
		DE_ASSERT(!stdPictureParametersSet->m_vkObjectOwner);
		DE_ASSERT(stdPictureParametersSet->m_vkVideoDecodeSession == DE_NULL);
		DE_ASSERT(m_currentPictureParameters);
		// Update the existing Vulkan Picture Parameters object

		return false;
	}
}

NvidiaParserVideoPictureParameters* NvidiaParserVideoPictureParameters::VideoPictureParametersFromBase (NvidiaParserVideoRefCountBase* pBase)
{
	if (!pBase)
		return DE_NULL;

	NvidiaParserVideoPictureParameters* result = dynamic_cast<NvidiaParserVideoPictureParameters*>(pBase);

	if (result)
		return result;

	TCU_THROW(InternalError, "Invalid NvidiaParserVideoPictureParameters from base");
}

VkVideoSessionParametersKHR NvidiaParserVideoPictureParameters::GetVideoSessionParametersKHR () const
{
	return m_sessionParameters.get();
}

int32_t NvidiaParserVideoPictureParameters::GetId () const
{
	return m_Id;
}

bool NvidiaParserVideoPictureParameters::HasVpsId (uint32_t vpsId) const
{
	return m_vpsIdsUsed[vpsId];
}

bool NvidiaParserVideoPictureParameters::HasSpsId (uint32_t spsId) const
{
	return m_spsIdsUsed[spsId];
}

bool NvidiaParserVideoPictureParameters::HasPpsId (uint32_t ppsId) const
{
	return m_ppsIdsUsed[ppsId];
}

NvidiaParserVideoPictureParameters::NvidiaParserVideoPictureParameters (VkDevice device)
	: m_Id					(-1)
	, m_refCount			(0)
	, m_device				(device)
	, m_sessionParameters	()
{
}

NvidiaParserVideoPictureParameters::~NvidiaParserVideoPictureParameters ()
{
}

VulkanVideoBitstreamBuffer::VulkanVideoBitstreamBuffer ()
	: m_bufferSize				(0)
	, m_bufferOffsetAlignment	(0)
	, m_bufferSizeAlignment		(0)
	, m_buffer					(DE_NULL)
{
}

const VkBuffer& VulkanVideoBitstreamBuffer::get ()
{
	return m_buffer->get();
}

VkResult VulkanVideoBitstreamBuffer::CreateVideoBitstreamBuffer (const DeviceInterface&	vkd,
																 VkDevice				device,
																 Allocator&				allocator,
																 VkDeviceSize			bufferSize,
																 VkDeviceSize			bufferOffsetAlignment,
																 VkDeviceSize			bufferSizeAlignment,
																 void*					pNext,
																 const unsigned char*	pBitstreamData,
																 VkDeviceSize			bitstreamDataSize)
{
	DestroyVideoBitstreamBuffer();

	m_bufferSizeAlignment	= bufferSizeAlignment;
	m_bufferSize			= deAlign64(bufferSize, bufferSizeAlignment);
	m_bufferOffsetAlignment	= bufferOffsetAlignment;

	const VkBufferCreateInfo	bufferCreateInfo	=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		//  VkStructureType		sType;
		pNext,										//  const void*			pNext;
		0,											//  VkBufferCreateFlags	flags;
		m_bufferSize,								//  VkDeviceSize		size;
		VK_BUFFER_USAGE_VIDEO_DECODE_SRC_BIT_KHR,	//  VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,					//  VkSharingMode		sharingMode;
		0,											//  deUint32			queueFamilyIndexCount;
		DE_NULL,									//  const deUint32*		pQueueFamilyIndices;
	};

	m_buffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));

	VK_CHECK(CopyVideoBitstreamToBuffer(vkd, device, pBitstreamData, bitstreamDataSize));

	return VK_SUCCESS;
}

VkResult VulkanVideoBitstreamBuffer::CopyVideoBitstreamToBuffer (const DeviceInterface&	vkd,
																 VkDevice				device,
																 const unsigned char*	pBitstreamData,
																 VkDeviceSize			bitstreamDataSize)
{
	if (pBitstreamData && bitstreamDataSize)
	{
		void* ptr = m_buffer->getAllocation().getHostPtr();

		DE_ASSERT(bitstreamDataSize <= m_bufferSize);

		//Copy Bitstream nvdec hw  requires min bitstream size to be 16 (see bug 1599347). memset padding to 0 if bitstream size less than 16
		if (bitstreamDataSize < 16)
			deMemset(ptr, 0, 16);

		deMemcpy(ptr, pBitstreamData, (size_t)bitstreamDataSize);

		flushAlloc(vkd, device, m_buffer->getAllocation());
	}

	return VK_SUCCESS;
}

void VulkanVideoBitstreamBuffer::DestroyVideoBitstreamBuffer ()
{
	m_buffer				= de::MovePtr<BufferWithMemory>();
	m_bufferSize			= 0;
	m_bufferOffsetAlignment	= 0;
	m_bufferSizeAlignment	= 0;
}

VulkanVideoBitstreamBuffer::~VulkanVideoBitstreamBuffer ()
{
	DestroyVideoBitstreamBuffer();
}

VkDeviceSize VulkanVideoBitstreamBuffer::GetBufferSize ()
{
	return m_bufferSize;
}

VkDeviceSize VulkanVideoBitstreamBuffer::GetBufferOffsetAlignment ()
{
	return m_bufferOffsetAlignment;
}

NvidiaParserVideoPictureParameters* VideoBaseDecoder::CheckStdObjectAfterUpdate (NvidiaSharedBaseObj<StdVideoPictureParametersSet>&	stdPictureParametersSet,
																				 NvidiaParserVideoPictureParameters*				pNewPictureParametersObject)
{
	if (!stdPictureParametersSet)
	{
		return DE_NULL;
	}

	if (pNewPictureParametersObject)
	{
		if (stdPictureParametersSet->m_updateSequenceCount == 0)
		{
			stdPictureParametersSet->m_vkVideoDecodeSession = m_videoDecodeSession.get();
		}
		else
		{
			// DE_ASSERT(stdPictureParametersSet->m_vkObjectOwner);
			// DE_ASSERT(stdPictureParametersSet->m_vkVideoDecodeSession == m_vkVideoDecodeSession);
			const NvidiaParserVideoPictureParameters*	pOwnerPictureParameters	=	NvidiaParserVideoPictureParameters::VideoPictureParametersFromBase(stdPictureParametersSet->m_vkObjectOwner);

			if (pOwnerPictureParameters)
			{
				DE_ASSERT(pOwnerPictureParameters->GetId() < pNewPictureParametersObject->GetId());
			}
		}

		// new object owner
		stdPictureParametersSet->m_vkObjectOwner = pNewPictureParametersObject;
		return pNewPictureParametersObject;

	}
	else
	{ // new std object
		stdPictureParametersSet->m_vkVideoDecodeSession = m_videoDecodeSession.get();
		stdPictureParametersSet->m_vkObjectOwner = m_currentPictureParameters;
	}

	return m_currentPictureParameters;
}

NvidiaParserVideoPictureParameters* VideoBaseDecoder::AddPictureParameters (NvidiaSharedBaseObj<StdVideoPictureParametersSet>&	vpsStdPictureParametersSet,
																			NvidiaSharedBaseObj<StdVideoPictureParametersSet>&	spsStdPictureParametersSet,
																			NvidiaSharedBaseObj<StdVideoPictureParametersSet>&	ppsStdPictureParametersSet)
{
	const DeviceInterface&				vkd							= getDeviceDriver();
	const VkDevice						device						= getDevice();
	NvidiaParserVideoPictureParameters*	pPictureParametersObject	= DE_NULL;
	const bool							createNewObject				= CheckStdObjectBeforeUpdate(vpsStdPictureParametersSet)
																	|| CheckStdObjectBeforeUpdate(spsStdPictureParametersSet)
																	|| CheckStdObjectBeforeUpdate(ppsStdPictureParametersSet);
#ifdef TODO
	if (createNewObject)
#else
	DE_UNREF(createNewObject);

	if (true)
#endif
	{
		pPictureParametersObject = NvidiaParserVideoPictureParameters::Create(vkd, device, m_videoDecodeSession.get(), vpsStdPictureParametersSet, spsStdPictureParametersSet, ppsStdPictureParametersSet, m_currentPictureParameters);
		if (pPictureParametersObject)
		    m_currentPictureParameters = pPictureParametersObject;
	}
	else
	{
		m_currentPictureParameters->Update(vkd, vpsStdPictureParametersSet, spsStdPictureParametersSet, ppsStdPictureParametersSet);
	}

	CheckStdObjectAfterUpdate(vpsStdPictureParametersSet, pPictureParametersObject);
	CheckStdObjectAfterUpdate(spsStdPictureParametersSet, pPictureParametersObject);
	CheckStdObjectAfterUpdate(ppsStdPictureParametersSet, pPictureParametersObject);

	return pPictureParametersObject;
}

NvVkDecodeFrameData* VideoBaseDecoder::GetCurrentFrameData (uint32_t	currentSlotId)
{
	DE_ASSERT(currentSlotId < m_maxDecodeFramesCount);

	return &m_decodeFramesData[currentSlotId];
}

int32_t VideoBaseDecoder::ReleaseDisplayedFrame (DecodedFrame* pDisplayedFrame)
{
	if (pDisplayedFrame->pictureIndex != -1)
	{
		DecodedFrameRelease		decodedFramesRelease	= { pDisplayedFrame->pictureIndex, 0, 0, 0, 0, 0 };
		DecodedFrameRelease*	decodedFramesReleasePtr	= &decodedFramesRelease;

		pDisplayedFrame->pictureIndex = -1;

		decodedFramesRelease.decodeOrder = pDisplayedFrame->decodeOrder;
		decodedFramesRelease.displayOrder = pDisplayedFrame->displayOrder;

		decodedFramesRelease.hasConsummerSignalFence = pDisplayedFrame->hasConsummerSignalFence;
		decodedFramesRelease.hasConsummerSignalSemaphore = pDisplayedFrame->hasConsummerSignalSemaphore;
		decodedFramesRelease.timestamp = 0;

		return m_videoFrameBuffer->ReleaseDisplayedPicture(&decodedFramesReleasePtr, 1);
	}

	return -1;
}

VideoFrameBuffer* VideoBaseDecoder::GetVideoFrameBuffer (void)
{
	DE_ASSERT(m_videoFrameBuffer.get() != DE_NULL);

	return m_videoFrameBuffer.get();
}

IfcNvFunctions* VideoBaseDecoder::GetNvFuncs (void)
{
	DE_ASSERT(m_nvFuncs.get() != DE_NULL);

	return m_nvFuncs.get();
}

void* copyToHeap (HeapType& heap, const void* p, size_t size)
{
	if (p == DE_NULL || size == 0)
		return DE_NULL;

	heap.push_back(de::MovePtr<vector<deUint8>>(new vector<deUint8>(size)));

	deMemcpy(heap.back()->data(), p, size);

	return heap.back()->data();
}

void appendHeap (HeapType& heapTo, HeapType& heapFrom)
{
	heapTo.reserve(heapTo.size() + heapFrom.size());

	for (auto& item : heapFrom)
		heapTo.push_back(de::MovePtr<vector<deUint8>>(item.release()));

	heapFrom.clear();
}

void appendPerFrameDecodeParameters (PerFrameDecodeParameters*				pPerFrameDecodeParameters,
									 vector<PerFrameDecodeParameters*>&		perFrameDecodeParameters,
									 HeapType&								heap)
{
	perFrameDecodeParameters.push_back(pPerFrameDecodeParameters);

	pPerFrameDecodeParameters->pCurrentPictureParameters->AddRef();

	if (pPerFrameDecodeParameters->bitstreamDataLen > 0)
		pPerFrameDecodeParameters->pBitstreamData = static_cast<unsigned char*>(copyToHeap(heap, pPerFrameDecodeParameters->pBitstreamData, static_cast<size_t>(deAlign64(pPerFrameDecodeParameters->bitstreamDataLen, 16))));
}

/* Callback function to be registered for getting a callback when a decoded
 * frame is ready to be decoded. Return value from HandlePictureDecode() are
 * interpreted as: 0: fail, >=1: suceeded
 */
int32_t VideoBaseDecoder::DecodePictureWithParameters (PerFrameDecodeParameters*			pPerFrameDecodeParameters,
													   VulkanParserDecodePictureInfo*		pVulkanParserDecodePictureInfo,
													   HeapType&							heap)
{
	DEBUGLOG(std::cout << "\tDecodePictureWithParameters:" << std::dec << m_pPerFrameDecodeParameters.size() << std::endl);

	int32_t	result	= -1;

	const size_t ndx = m_pPerFrameDecodeParameters.size();

	FlushPictureParametersQueue();

	appendHeap(incSizeSafe(m_heaps), heap);

	m_pVulkanParserDecodePictureInfo.push_back((VulkanParserDecodePictureInfo*)copyToHeap(m_heaps[ndx], pVulkanParserDecodePictureInfo, sizeof(*pVulkanParserDecodePictureInfo)));
	appendPerFrameDecodeParameters(pPerFrameDecodeParameters, m_pPerFrameDecodeParameters, m_heaps[ndx]);

	result = pPerFrameDecodeParameters->currPicIdx;

	if (m_pPerFrameDecodeParameters.size() >= (size_t)m_frameCountTrigger)
		result = DecodeCachedPictures();

	return result;
}

void VideoBaseDecoder::ReinitCaches (void)
{
	const size_t size	= m_frameCountTrigger;

	for (auto& it : m_pPerFrameDecodeParameters)
		it->pCurrentPictureParameters->Release();

	m_pPerFrameDecodeParameters.clear();
	m_pVulkanParserDecodePictureInfo.clear();
	m_pFrameDatas.clear();
	m_bitstreamBufferMemoryBarriers.clear();
	m_imageBarriersVec.clear();
	m_frameSynchronizationInfos.clear();
	m_commandBufferSubmitInfos.clear();
	m_decodeBeginInfos.clear();
	m_pictureResourcesInfos.clear();
	m_dependencyInfos.clear();
	m_decodeEndInfos.clear();
	m_submitInfos.clear();
	m_frameCompleteFences.clear();
	m_frameConsumerDoneFences.clear();
	m_frameCompleteSemaphoreSubmitInfos.clear();
	m_frameConsumerDoneSemaphoreSubmitInfos.clear();
	m_heaps.clear();

	// Make sure pointers will stay consistent
	m_pPerFrameDecodeParameters.reserve(size);
	m_pVulkanParserDecodePictureInfo.reserve(size);
	m_pFrameDatas.reserve(size);
	m_bitstreamBufferMemoryBarriers.reserve(size);
	m_imageBarriersVec.reserve(size);
	m_frameSynchronizationInfos.reserve(size);
	m_commandBufferSubmitInfos.reserve(size);
	m_decodeBeginInfos.reserve(size);
	m_pictureResourcesInfos.reserve(size);
	m_dependencyInfos.reserve(size);
	m_decodeEndInfos.reserve(size);
	m_submitInfos.reserve(size);
	m_frameCompleteFences.reserve(size);
	m_frameConsumerDoneFences.reserve(size);
	m_frameCompleteSemaphoreSubmitInfos.reserve(size);
	m_frameConsumerDoneSemaphoreSubmitInfos.reserve(size);
	m_heaps.reserve(size);
}

int32_t VideoBaseDecoder::DecodeCachedPictures (VideoBaseDecoder*	friendDecoder,
												bool				waitSubmitted)
{
	DEBUGLOG(std::cout << "DecodeCachedPictures" << std::endl);

	const DeviceInterface&	vkd					= getDeviceDriver();
	const VkDevice			device				= getDevice();
	const deUint32			queueFamilyIndex	= getQueueFamilyIndexDecode();
	const size_t			ndxMax				= m_pPerFrameDecodeParameters.size();
	const bool				interleaved			= friendDecoder != DE_NULL || !waitSubmitted;
	vector<size_t>			recordOrderIndices;

	DE_ASSERT(m_minBitstreamBufferSizeAlignment != 0);
	DE_ASSERT(m_videoDecodeSession.get() != DE_NULL);

	m_pFrameDatas.resize(ndxMax);
	m_bitstreamBufferMemoryBarriers.resize(ndxMax);
	m_imageBarriersVec.resize(ndxMax);
	m_frameSynchronizationInfos.resize(ndxMax);
	m_commandBufferSubmitInfos.resize(ndxMax);
	m_decodeBeginInfos.resize(ndxMax);
	m_pictureResourcesInfos.resize(ndxMax);
	m_dependencyInfos.resize(ndxMax);
	m_decodeEndInfos.resize(ndxMax);
	m_submitInfos.resize(ndxMax);
	m_frameCompleteFences.resize(ndxMax);
	m_frameConsumerDoneSemaphoreSubmitInfos.resize(ndxMax);
	m_frameCompleteSemaphoreSubmitInfos.resize(ndxMax);

	for (size_t ndx = 0; ndx < ndxMax; ++ndx)
	{
		const size_t									picNdx							= ndx;
		PerFrameDecodeParameters*						pPicParams						= m_pPerFrameDecodeParameters[picNdx];
		vector<VkImageMemoryBarrier2KHR>&				imageBarriers					= m_imageBarriersVec[picNdx];
		VideoFrameBuffer::FrameSynchronizationInfo&		frameSynchronizationInfo		= m_frameSynchronizationInfos[picNdx];
		NvVkDecodeFrameData*&							pFrameData						= m_pFrameDatas[picNdx];
		vector<VideoFrameBuffer::PictureResourceInfo>&	pictureResourcesInfo			= m_pictureResourcesInfos[picNdx];
		VkBufferMemoryBarrier2KHR&						bitstreamBufferMemoryBarrier	= m_bitstreamBufferMemoryBarriers[picNdx];
		VkFence&										frameCompleteFence				= m_frameCompleteFences[picNdx];
		VulkanParserDecodePictureInfo*					pDecodePictureInfo				= m_pVulkanParserDecodePictureInfo[picNdx];
		const int32_t									currPicIdx						= pPicParams->currPicIdx;

		DE_ASSERT((uint32_t)currPicIdx < m_numDecodeSurfaces);

		m_videoFrameBuffer->SetPicNumInDecodeOrder(currPicIdx, ++m_decodePicCount);

		pFrameData = GetCurrentFrameData((uint32_t)currPicIdx);

		DE_ASSERT(pFrameData->bitstreamBuffer.GetBufferSize() >= pPicParams->bitstreamDataLen);

		pFrameData->bitstreamBuffer.CopyVideoBitstreamToBuffer(vkd, device, pPicParams->pBitstreamData, pPicParams->bitstreamDataLen);

		pPicParams->decodeFrameInfo.srcBuffer		= pFrameData->bitstreamBuffer.get();
		pPicParams->decodeFrameInfo.srcBufferOffset	= 0;
		pPicParams->decodeFrameInfo.srcBufferRange	= deAlign64((VkDeviceSize)pPicParams->bitstreamDataLen, m_minBitstreamBufferSizeAlignment);

		DE_ASSERT(pPicParams->decodeFrameInfo.srcBuffer != DE_NULL);

		bitstreamBufferMemoryBarrier =
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR,	//  VkStructureType				sType;
			DE_NULL,										//  const void*					pNext;
			VK_PIPELINE_STAGE_2_NONE_KHR,					//  VkPipelineStageFlags2KHR	srcStageMask;
			0,												//  VkAccessFlags2KHR			srcAccessMask;
			VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR,		//  VkPipelineStageFlags2KHR	dstStageMask;
			VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR,			//  VkAccessFlags2KHR			dstAccessMask;
			queueFamilyIndex,								//  deUint32					srcQueueFamilyIndex;
			queueFamilyIndex,								//  deUint32					dstQueueFamilyIndex;
			pPicParams->decodeFrameInfo.srcBuffer,			//  VkBuffer					buffer;
			pPicParams->decodeFrameInfo.srcBufferOffset,	//  VkDeviceSize				offset;
			pPicParams->decodeFrameInfo.srcBufferRange		//  VkDeviceSize				size;
		};

		imageBarriers.reserve(2 * PerFrameDecodeParameters::MAX_DPB_REF_SLOTS);

		const VkImageMemoryBarrier2KHR	dpbBarrierTemplate =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,	//  VkStructureType				sType;
			DE_NULL,										//  const void*					pNext;
			VK_PIPELINE_STAGE_2_NONE_KHR,					//  VkPipelineStageFlags2KHR	srcStageMask;
			0,												//  VkAccessFlags2KHR			srcAccessMask;
			VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR,		//  VkPipelineStageFlags2KHR	dstStageMask;
			VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR,			//  VkAccessFlags2KHR			dstAccessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,						//  VkImageLayout				oldLayout;
			VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR,			//  VkImageLayout				newLayout;
			queueFamilyIndex,								//  deUint32					srcQueueFamilyIndex;
			queueFamilyIndex,								//  deUint32					dstQueueFamilyIndex;
			DE_NULL,										//  VkImage						image;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1, }		//  VkImageSubresourceRange		subresourceRange;
		};

		{
			const VkImageLayout							newLayout				= m_distinctDstDpbImages ? VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR : VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR;
			VkVideoPictureResourceInfoKHR&				pictureResource			= pPicParams->decodeFrameInfo.dstPictureResource;
			VideoFrameBuffer::PictureResourceInfo	currentPictureResource	= { DE_NULL, VK_IMAGE_LAYOUT_UNDEFINED };

			m_videoFrameBuffer->GetImageResourcesByIndex((int8_t)pPicParams->currPicIdx, &pictureResource, &currentPictureResource, newLayout);

			DEBUGLOG(std::cout << "PicNdx: " << std::dec << (int32_t)picNdx << " " << currentPictureResource.image << std::endl);

			if (currentPictureResource.currentImageLayout == VK_IMAGE_LAYOUT_UNDEFINED)
			{
				VkImageMemoryBarrier2KHR& imageBarrier = incSizeSafe(imageBarriers);

				imageBarrier				= dpbBarrierTemplate;
				imageBarrier.oldLayout		= currentPictureResource.currentImageLayout;
				imageBarrier.newLayout		= newLayout;
				imageBarrier.image			= currentPictureResource.image;
				imageBarrier.dstAccessMask	= VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR;

				DEBUGLOG(std::cout << "\tTransit DST: " << imageBarrier.image << " from " << imageBarrier.oldLayout << std::endl);

				DE_ASSERT(imageBarrier.image != DE_NULL);
			}
		}

		if (m_distinctDstDpbImages)
		{
			const VkImageLayout						newLayout				= VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR;
			VkVideoPictureResourceInfoKHR&				pictureResource			= pPicParams->pictureResources[pPicParams->numGopReferenceSlots];
			VideoFrameBuffer::PictureResourceInfo	currentPictureResource	= { DE_NULL, VK_IMAGE_LAYOUT_UNDEFINED };

			m_videoFrameBuffer->GetImageResourcesByIndex((int8_t)pPicParams->currPicIdx, &pictureResource, &currentPictureResource, newLayout);

			DEBUGLOG(std::cout << "PicNdx: " << std::dec << (int32_t)picNdx << " " << currentPictureResource.image << std::endl);

			if (currentPictureResource.currentImageLayout == VK_IMAGE_LAYOUT_UNDEFINED)
			{
				VkImageMemoryBarrier2KHR& imageBarrier = incSizeSafe(imageBarriers);

				imageBarrier									= dpbBarrierTemplate;
				imageBarrier.oldLayout							= currentPictureResource.currentImageLayout;
				imageBarrier.newLayout							= newLayout;
				imageBarrier.image								= currentPictureResource.image;
				imageBarrier.dstAccessMask						= VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR;
				imageBarrier.subresourceRange.baseArrayLayer	= pPicParams->decodeFrameInfo.dstPictureResource.baseArrayLayer;

				DEBUGLOG(std::cout << "\tTransit DPB: " << imageBarrier.image << ":" << imageBarrier.subresourceRange.baseArrayLayer << " from " << imageBarrier.oldLayout << std::endl);

				DE_ASSERT(imageBarrier.image != DE_NULL);
			}
		}


		stringstream s;

		s << "\tGOP:" << std::dec << (int32_t)pPicParams->numGopReferenceSlots << " ( ";

		if (pPicParams->numGopReferenceSlots)
		{
			const VkImageLayout newLayout = VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR;

			pictureResourcesInfo.resize(PerFrameDecodeParameters::MAX_DPB_REF_SLOTS);

			deMemset(pictureResourcesInfo.data(), 0, sizeof(pictureResourcesInfo[0]) * pictureResourcesInfo.size());

			m_videoFrameBuffer->GetImageResourcesByIndex(pPicParams->numGopReferenceSlots, &pPicParams->pGopReferenceImagesIndexes[0], pPicParams->pictureResources, pictureResourcesInfo.data(), newLayout);

			for (int32_t resId = 0; resId < pPicParams->numGopReferenceSlots; resId++)
			{
				s << std::dec << (int32_t)pPicParams->pGopReferenceImagesIndexes[resId] << ":" << pictureResourcesInfo[resId].image << " ";

				// slotLayer requires NVIDIA specific extension VK_KHR_video_layers, not enabled, just yet.
				// pGopReferenceSlots[resId].slotLayerIndex = 0;
				// pictureResourcesInfo[resId].image can be a DE_NULL handle if the picture is not-existent.
				if (pictureResourcesInfo[resId].image != DE_NULL && pictureResourcesInfo[resId].currentImageLayout != newLayout)
				{
					VkImageMemoryBarrier2KHR& imageBarrier = incSizeSafe(imageBarriers);

					imageBarrier			= dpbBarrierTemplate;
					imageBarrier.oldLayout	= pictureResourcesInfo[resId].currentImageLayout;
					imageBarrier.newLayout	= newLayout;
					imageBarrier.image		= pictureResourcesInfo[resId].image;

					DEBUGLOG(std::cout << "\tTransit DPB: " << imageBarrier.image << " from " << imageBarrier.oldLayout << std::endl);

					pictureResourcesInfo[resId].currentImageLayout = imageBarrier.newLayout;

					DE_ASSERT(imageBarrier.image != DE_NULL);
				}
			}
		}

		DEBUGLOG(std::cout << s.str() << ")" << std::endl);

		if (pDecodePictureInfo->flags.unpairedField)
			pDecodePictureInfo->flags.syncFirstReady = true;

		pDecodePictureInfo->flags.syncToFirstField = false;

		frameSynchronizationInfo.hasFrameCompleteSignalFence		= true;
		frameSynchronizationInfo.hasFrameCompleteSignalSemaphore	= true;

		int32_t retVal = m_videoFrameBuffer->QueuePictureForDecode((int8_t)currPicIdx, pDecodePictureInfo, pPicParams->pCurrentPictureParameters->m_vkObjectOwner, &frameSynchronizationInfo);

		if (currPicIdx != retVal)
			DE_ASSERT(0 && "QueuePictureForDecode has failed");

		frameCompleteFence	= frameSynchronizationInfo.frameCompleteFence;
	}

	recordOrderIndices.reserve(ndxMax);
	for (size_t ndx = 0; ndx < ndxMax; ++ndx)
		recordOrderIndices.push_back(ndx);

	if (m_randomOrSwapped)
	{
		if (ndxMax == 2)
		{
			std::swap(recordOrderIndices[0], recordOrderIndices[1]);
		}
		else
		{
			de::Random rnd(0);

			DE_ASSERT(recordOrderIndices.size() % m_gopSize == 0);

			for (vector<size_t>::iterator	it =  recordOrderIndices.begin();
											it != recordOrderIndices.end();
											it += m_gopSize)
			{
				rnd.shuffle(it, it + m_gopSize);
			}
		}
	}

	for (size_t ndx = 0; ndx < ndxMax; ++ndx)
	{
		const size_t									picNdx							= recordOrderIndices[ndx];
		PerFrameDecodeParameters*						pPicParams						= m_pPerFrameDecodeParameters[picNdx];
		vector<VkImageMemoryBarrier2KHR>&				imageBarriers					= m_imageBarriersVec[picNdx];
		VideoFrameBuffer::FrameSynchronizationInfo&		frameSynchronizationInfo		= m_frameSynchronizationInfos[picNdx];
		NvVkDecodeFrameData*&							pFrameData						= m_pFrameDatas[picNdx];
		VkBufferMemoryBarrier2KHR&						bitstreamBufferMemoryBarrier	= m_bitstreamBufferMemoryBarriers[picNdx];
		VkVideoBeginCodingInfoKHR&						decodeBeginInfo					= m_decodeBeginInfos[picNdx];
		VkDependencyInfoKHR&							dependencyInfo					= m_dependencyInfos[picNdx];
		VkVideoEndCodingInfoKHR&						decodeEndInfo					= m_decodeEndInfos[picNdx];
		VkCommandBufferSubmitInfoKHR&					commandBufferSubmitInfo			= m_commandBufferSubmitInfos[picNdx];
		VkCommandBuffer&								commandBuffer					= commandBufferSubmitInfo.commandBuffer;
		const VkVideoCodingControlInfoKHR				videoCodingControlInfoKHR		=
		{
			VK_STRUCTURE_TYPE_VIDEO_CODING_CONTROL_INFO_KHR,	//  VkStructureType					sType;
			DE_NULL,											//  const void*						pNext;
			VK_VIDEO_CODING_CONTROL_RESET_BIT_KHR,				//  VkVideoCodingControlFlagsKHR	flags;
		};

		commandBufferSubmitInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR,	//  VkStructureType	sType;
			DE_NULL,											//  const void*		pNext;
			pFrameData->commandBuffer.get(),					//  VkCommandBuffer	commandBuffer;
			0u,													//  uint32_t		deviceMask;
		};

		// Effectively its done above
		commandBuffer = pFrameData->commandBuffer.get();

		DEBUGLOG(std::cout << "PicNdx: " << std::dec << picNdx << " commandBuffer:" << commandBuffer << std::endl);

		DE_ASSERT(pPicParams->pCurrentPictureParameters->m_vkObjectOwner);
		const NvidiaParserVideoPictureParameters* pOwnerPictureParameters = NvidiaParserVideoPictureParameters::VideoPictureParametersFromBase(pPicParams->pCurrentPictureParameters->m_vkObjectOwner);
		DE_ASSERT(pOwnerPictureParameters);
		//DE_ASSERT(pOwnerPictureParameters->GetId() <= m_currentPictureParameters->GetId());

		bool isSps = false;
		bool isPps = false;
		bool isVps = false;


		int32_t spsId = pPicParams->pCurrentPictureParameters->GetSpsId(isSps);
		int32_t ppsId = pPicParams->pCurrentPictureParameters->GetPpsId(isPps);
		int32_t vpsId = pPicParams->pCurrentPictureParameters->GetVpsId(isVps);

		DE_ASSERT(isPps);

		DE_ASSERT(spsId >= 0);
		DE_ASSERT(pOwnerPictureParameters->HasSpsId(spsId));
		DE_UNREF(spsId);

		DE_ASSERT(ppsId >= 0);
		DE_ASSERT(pOwnerPictureParameters->HasPpsId(ppsId));
		DE_UNREF(ppsId);

		if (m_videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR)
		{
			DE_ASSERT(vpsId >= 0);
			DE_ASSERT(pOwnerPictureParameters->HasVpsId(vpsId));
			DE_UNREF(vpsId);
		}

		beginCommandBuffer(vkd, commandBuffer);

		DEBUGLOG(std::cout << "beginCommandBuffer " << commandBuffer << " VkVideoSessionParametersKHR:" << pOwnerPictureParameters->GetVideoSessionParametersKHR() << std::endl);

		const uint32_t referenceSlotCount = pPicParams->decodeFrameInfo.referenceSlotCount;

		decodeBeginInfo =
		{
			VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR,				//  VkStructureType						sType;
			DE_NULL,													//  const void*							pNext;
			0u,															//  VkVideoBeginCodingFlagsKHR			flags;
			m_videoDecodeSession.get(),									//  VkVideoSessionKHR					videoSession;
			pOwnerPictureParameters->GetVideoSessionParametersKHR(),	//  VkVideoSessionParametersKHR			videoSessionParameters;
			referenceSlotCount,											//  uint32_t							referenceSlotCount;
			pPicParams->decodeFrameInfo.pReferenceSlots,				//  const VkVideoReferenceSlotInfoKHR*	pReferenceSlots;
		};
		dependencyInfo =
		{
			VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,	//  VkStructureType						sType;
			DE_NULL,								//  const void*							pNext;
			VK_DEPENDENCY_BY_REGION_BIT,			//  VkDependencyFlags					dependencyFlags;
			0,										//  deUint32							memoryBarrierCount;
			DE_NULL,								//  const VkMemoryBarrier2KHR*			pMemoryBarriers;
			1,										//  deUint32							bufferMemoryBarrierCount;
			&bitstreamBufferMemoryBarrier,			//  const VkBufferMemoryBarrier2KHR*	pBufferMemoryBarriers;
			(uint32_t)imageBarriers.size(),			//  deUint32							imageMemoryBarrierCount;
			dataOrNullPtr(imageBarriers),			//  const VkImageMemoryBarrier2KHR*		pImageMemoryBarriers;
		};
		decodeEndInfo =
		{
			VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR,	//  VkStructureType				sType;
			DE_NULL,										//  const void*					pNext;
			0,												//  VkVideoEndCodingFlagsKHR	flags;
		};

		if (m_queryResultWithStatus)
			vkd.cmdResetQueryPool(commandBuffer, frameSynchronizationInfo.queryPool, frameSynchronizationInfo.startQueryId, frameSynchronizationInfo.numQueries);


		// Ensure the resource for the resources associated with the
		// reference slot (if it exists) are in the bound picture
		// resources set.  See VUID-vkCmdDecodeVideoKHR-pDecodeInfo-07149.
		if (pPicParams->decodeFrameInfo.pSetupReferenceSlot != nullptr)
		{
			unsigned idx = 0;
			for (; idx < decodeBeginInfo.referenceSlotCount; idx++) {
				pPicParams->decodeBeginSlots[idx] = decodeBeginInfo.pReferenceSlots[idx];
			}
			DE_ASSERT(idx < PerFrameDecodeParameters::MAX_DPB_REF_SLOTS - 1);
			VkVideoReferenceSlotInfoKHR& dstPictureSlot = pPicParams->decodeBeginSlots[idx];
			dstPictureSlot.sType = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
			dstPictureSlot.slotIndex = -1;  // Activate the setup reference slot
			dstPictureSlot.pPictureResource =  m_distinctDstDpbImages ? &pPicParams->pictureResources[pPicParams->numGopReferenceSlots] : &pPicParams->decodeFrameInfo.dstPictureResource;
			decodeBeginInfo.pReferenceSlots = pPicParams->decodeBeginSlots;
			decodeBeginInfo.referenceSlotCount++;
		}

		vkd.cmdBeginVideoCodingKHR(commandBuffer, &decodeBeginInfo);

		if (picNdx == 0)
			vkd.cmdControlVideoCodingKHR(commandBuffer, &videoCodingControlInfoKHR);

		vkd.cmdPipelineBarrier2(commandBuffer, &dependencyInfo);

		if (m_queryResultWithStatus)
			vkd.cmdBeginQuery(commandBuffer, frameSynchronizationInfo.queryPool, frameSynchronizationInfo.startQueryId, 0u);

		vkd.cmdDecodeVideoKHR(commandBuffer, &pPicParams->decodeFrameInfo);

		stringstream s;

		s << "Slots: " << (int32_t)pPicParams->decodeFrameInfo.referenceSlotCount << ": ( ";
		for (uint32_t i = 0; i < pPicParams->decodeFrameInfo.referenceSlotCount; i++)
			s << std::dec << (int32_t)pPicParams->decodeFrameInfo.pReferenceSlots[i].slotIndex << " ";

		DEBUGLOG(std::cout << s.str() << ")" << std::endl);

		if (m_queryResultWithStatus)
			vkd.cmdEndQuery(commandBuffer, frameSynchronizationInfo.queryPool, frameSynchronizationInfo.startQueryId);

		vkd.cmdEndVideoCodingKHR(commandBuffer, &decodeEndInfo);

		endCommandBuffer(vkd, commandBuffer);

		DEBUGLOG(std::cout << "endCommandBuffer " << commandBuffer << std::endl);

		if (!m_submitAfter)
			SubmitQueue(&commandBufferSubmitInfo, &m_submitInfos[picNdx], &m_frameSynchronizationInfos[picNdx], &m_frameConsumerDoneSemaphoreSubmitInfos[picNdx], &m_frameCompleteSemaphoreSubmitInfos[picNdx]);
	}

	if (m_submitAfter && !interleaved)
	{
		for (size_t ndx = 0; ndx < recordOrderIndices.size(); ++ndx)
			SubmitQueue(&m_commandBufferSubmitInfos[ndx], &m_submitInfos[ndx], &m_frameSynchronizationInfos[ndx], &m_frameConsumerDoneSemaphoreSubmitInfos[ndx], &m_frameCompleteSemaphoreSubmitInfos[ndx]);
	}

	m_frameConsumerDoneSemaphoreSubmitInfos.clear();
	m_frameCompleteSemaphoreSubmitInfos.clear();

	if (interleaved)
	{
		for (uint32_t ndx = 0; ndx < ndxMax; ++ndx)
		{
			if (m_frameSynchronizationInfos[ndx].frameConsumerDoneFence != DE_NULL)
				m_frameConsumerDoneFences.push_back(m_frameSynchronizationInfos[ndx].frameConsumerDoneFence);

			if (m_frameSynchronizationInfos[ndx].frameCompleteSemaphore != DE_NULL)
				m_frameCompleteSemaphoreSubmitInfos.push_back(makeSemaphoreSubmitInfo(m_frameSynchronizationInfos[ndx].frameCompleteSemaphore, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR));

			if (m_frameSynchronizationInfos[ndx].frameConsumerDoneSemaphore != DE_NULL)
				m_frameConsumerDoneSemaphoreSubmitInfos.push_back(makeSemaphoreSubmitInfo(m_frameSynchronizationInfos[ndx].frameConsumerDoneSemaphore, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR));
		}

		if (friendDecoder != DE_NULL)
		{
			friendDecoder->DecodeCachedPictures(DE_NULL, false);

			for (uint32_t ndx = 0; ndx < ndxMax; ++ndx)
			{
				if (friendDecoder->m_frameSynchronizationInfos[ndx].frameConsumerDoneFence != DE_NULL)
					m_frameConsumerDoneFences.push_back(friendDecoder->m_frameSynchronizationInfos[ndx].frameConsumerDoneFence);

				if (friendDecoder->m_frameSynchronizationInfos[ndx].frameCompleteSemaphore != DE_NULL)
					m_frameCompleteSemaphoreSubmitInfos.push_back(makeSemaphoreSubmitInfo(friendDecoder->m_frameSynchronizationInfos[ndx].frameCompleteSemaphore, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR));

				if (friendDecoder->m_frameSynchronizationInfos[ndx].frameConsumerDoneSemaphore != DE_NULL)
					m_frameConsumerDoneSemaphoreSubmitInfos.push_back(makeSemaphoreSubmitInfo(friendDecoder->m_frameSynchronizationInfos[ndx].frameConsumerDoneSemaphore, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR));
			}
		}

		if (waitSubmitted)
		{
			vector<VkCommandBufferSubmitInfoKHR>	commandBufferSubmitInfos;

			DE_ASSERT(m_commandBufferSubmitInfos.size() == friendDecoder->m_commandBufferSubmitInfos.size());

			commandBufferSubmitInfos.reserve(m_commandBufferSubmitInfos.size() + friendDecoder->m_commandBufferSubmitInfos.size());

			for (uint32_t ndx = 0; ndx < ndxMax; ++ndx)
			{
				incSizeSafe(commandBufferSubmitInfos) = m_commandBufferSubmitInfos[ndx];
				incSizeSafe(commandBufferSubmitInfos) = friendDecoder->m_commandBufferSubmitInfos[ndx];
			}

			SubmitQueue(
				commandBufferSubmitInfos,
				&m_submitInfos[ndxMax - 1],
				m_frameCompleteFences[ndxMax - 1],
				m_frameConsumerDoneFences,
				m_frameCompleteSemaphoreSubmitInfos,
				m_frameConsumerDoneSemaphoreSubmitInfos);
		}
	}

	if (waitSubmitted)
	{
		VK_CHECK(vkd.waitForFences(device, (uint32_t)m_frameCompleteFences.size(), m_frameCompleteFences.data(), true, ~0ull));

		m_frameCompleteFences.clear();
		m_frameConsumerDoneFences.clear();
		m_frameCompleteSemaphoreSubmitInfos.clear();
		m_frameConsumerDoneSemaphoreSubmitInfos.clear();

		if (friendDecoder != DE_NULL)
			friendDecoder->ReinitCaches();
	}

	if (m_queryResultWithStatus)
	{
		for (size_t ndx = 0; ndx < ndxMax; ++ndx)
		{
			struct nvVideoGetDecodeStatus
			{
				VkQueryResultStatusKHR	decodeStatus;
				uint32_t				hwCyclesCount;			//  OUT: HW cycle count per frame
				uint32_t				hwStatus;				//  OUT: HW decode status
				uint32_t				mbsCorrectlyDecoded;	//  total numers of correctly decoded macroblocks
				uint32_t				mbsInError;				//  number of error macroblocks.
				uint16_t				instanceId;				//  OUT: nvdec instance id
				uint16_t				reserved1;				//  Reserved for future use
			} queryResult;

			VkResult result = vkd.getQueryPoolResults(device,
													  m_frameSynchronizationInfos[ndx].queryPool,
													  m_frameSynchronizationInfos[ndx].startQueryId,
													  1,
													  sizeof(queryResult),
													  &queryResult,
													  sizeof(queryResult),
													  VK_QUERY_RESULT_WITH_STATUS_BIT_KHR | VK_QUERY_RESULT_WAIT_BIT);

			if (queryResult.decodeStatus != VK_QUERY_RESULT_STATUS_COMPLETE_KHR)
				TCU_FAIL("VK_QUERY_RESULT_STATUS_COMPLETE_KHR expected");

			//TCU_FAIL("TODO: nvVideoGetDecodeStatus is not specified in spec");

			DE_UNREF(result);
		}
	}

	const int result = m_pPerFrameDecodeParameters[m_pPerFrameDecodeParameters.size() - 1]->currPicIdx;

	if (waitSubmitted)
		ReinitCaches();

	return result;
}

void VideoBaseDecoder::SubmitQueue (VkCommandBufferSubmitInfoKHR*				commandBufferSubmitInfo,
									VkSubmitInfo2KHR*							submitInfo,
									VideoFrameBuffer::FrameSynchronizationInfo*	frameSynchronizationInfo,
									VkSemaphoreSubmitInfoKHR*					frameConsumerDoneSemaphore,
									VkSemaphoreSubmitInfoKHR*					frameCompleteSemaphore)
{
	const DeviceInterface&	vkd							= getDeviceDriver();
	const VkDevice			device						= getDevice();
	const VkQueue			queue						= getQueueDecode();
	const deUint32			waitSemaphoreCount			= (frameSynchronizationInfo->frameConsumerDoneSemaphore == DE_NULL) ? 0u : 1u;
	const deUint32			signalSemaphoreInfoCount	= (frameSynchronizationInfo->frameCompleteSemaphore == DE_NULL) ? 0u : 1u;

	*frameConsumerDoneSemaphore	= makeSemaphoreSubmitInfo(frameSynchronizationInfo->frameConsumerDoneSemaphore, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR);
	*frameCompleteSemaphore		= makeSemaphoreSubmitInfo(frameSynchronizationInfo->frameCompleteSemaphore, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR);

	*submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,	//  VkStructureType						sType;
		DE_NULL,								//  const void*							pNext;
		0u,										//  VkSubmitFlagsKHR					flags;
		waitSemaphoreCount,						//  uint32_t							waitSemaphoreInfoCount;
		frameConsumerDoneSemaphore,				//  const VkSemaphoreSubmitInfoKHR*		pWaitSemaphoreInfos;
		1u,										//  uint32_t							commandBufferInfoCount;
		commandBufferSubmitInfo,				//  const VkCommandBufferSubmitInfoKHR*	pCommandBufferInfos;
		signalSemaphoreInfoCount,				//  uint32_t							signalSemaphoreInfoCount;
		frameCompleteSemaphore,					//  const VkSemaphoreSubmitInfoKHR*		pSignalSemaphoreInfos;
	};

	VkResult result = VK_SUCCESS;

	if ((frameSynchronizationInfo->frameConsumerDoneSemaphore == DE_NULL) && (frameSynchronizationInfo->frameConsumerDoneFence != DE_NULL))
		VK_CHECK(vkd.waitForFences(device, 1, &frameSynchronizationInfo->frameConsumerDoneFence, true, ~0ull));

	VK_CHECK(vkd.getFenceStatus(device, frameSynchronizationInfo->frameCompleteFence));

	VK_CHECK(vkd.resetFences(device, 1, &frameSynchronizationInfo->frameCompleteFence));

	result = vkd.getFenceStatus(device, frameSynchronizationInfo->frameCompleteFence);
	DE_ASSERT(result == VK_NOT_READY);
	DE_UNREF(result);

	vkd.queueSubmit2(queue, 1, submitInfo, frameSynchronizationInfo->frameCompleteFence);
}

void VideoBaseDecoder::SubmitQueue (vector<VkCommandBufferSubmitInfoKHR>&	commandBufferSubmitInfos,
									VkSubmitInfo2KHR*						submitInfo,
									const VkFence							frameCompleteFence,
									const vector<VkFence>&					frameConsumerDoneFence,
									const vector<VkSemaphoreSubmitInfoKHR>&	frameConsumerDoneSemaphores,
									const vector<VkSemaphoreSubmitInfoKHR>&	frameCompleteSemaphores)
{
	const DeviceInterface&	vkd		= getDeviceDriver();
	const VkDevice			device	= getDevice();
	const VkQueue			queue	= getQueueDecode();

	for (uint32_t ndx = 0; ndx < frameConsumerDoneSemaphores.size(); ++ndx)
		if ((frameConsumerDoneSemaphores[ndx].semaphore == DE_NULL) && (frameConsumerDoneFence[ndx] != DE_NULL))
			VK_CHECK(vkd.waitForFences(device, 1, &frameConsumerDoneFence[ndx], true, ~0ull));

	VK_CHECK(vkd.getFenceStatus(device, frameCompleteFence));
	VK_CHECK(vkd.resetFences(device, 1, &frameCompleteFence));
	DE_ASSERT(vkd.getFenceStatus(device, frameCompleteFence) == VK_NOT_READY);

	*submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,			//  VkStructureType						sType;
		DE_NULL,										//  const void*							pNext;
		0,												//  VkSubmitFlagsKHR					flags;
		(deUint32)frameCompleteSemaphores.size(),		//  uint32_t							waitSemaphoreInfoCount;
		de::dataOrNull(frameCompleteSemaphores),		//  const VkSemaphoreSubmitInfoKHR*		pWaitSemaphoreInfos;
		(uint32_t)commandBufferSubmitInfos.size(),		//  uint32_t							commandBufferInfoCount;
		dataOrNullPtr(commandBufferSubmitInfos),		//  const VkCommandBufferSubmitInfoKHR*	pCommandBufferInfos;
		(deUint32)frameConsumerDoneSemaphores.size(),	//  uint32_t							signalSemaphoreInfoCount;
		de::dataOrNull(frameConsumerDoneSemaphores),	//  const VkSemaphoreSubmitInfoKHR*		pSignalSemaphoreInfos;
	};

	vkd.queueSubmit2(queue, 1, submitInfo, frameCompleteFence);
}

void VideoBaseDecoder::Deinitialize ()
{
	if (m_device != DE_NULL)
	{
		const DeviceInterface&	vkd					= getDeviceDriver();
		const VkDevice			device				= getDevice();
		const VkQueue			queueDecode			= getQueueDecode();
		const VkQueue			queueTransfer		= getQueueTransfer();

		if (queueDecode)
		{
			vkd.queueWaitIdle(queueDecode);
		}

		if (queueTransfer)
		{
			vkd.queueWaitIdle(queueTransfer);
		}

		if (device)
		{
			vkd.deviceWaitIdle(device);
		}
	}

	m_dpb.Deinit();

	if (m_videoFrameBuffer)
	{
		m_videoFrameBuffer = MovePtr<VideoFrameBuffer>();
	}

	if (m_decodeFramesData && m_videoCommandPool)
	{
		for (size_t decodeFrameId = 0; decodeFrameId < m_maxDecodeFramesCount; decodeFrameId++)
			m_decodeFramesData[decodeFrameId].commandBuffer = Move<VkCommandBuffer>();

		m_videoCommandPool = Move<VkCommandPool>();
	}

	if (m_decodeFramesData)
	{
		for (size_t decodeFrameId = 0; decodeFrameId < m_maxDecodeFramesCount; decodeFrameId++)
		{
			m_decodeFramesData[decodeFrameId].bitstreamBuffer.DestroyVideoBitstreamBuffer();
		}

		delete[] m_decodeFramesData;

		m_decodeFramesData = DE_NULL;
	}

}

int32_t NvidiaParserVideoPictureParameters::m_currentId = 0;

int32_t NvidiaParserVideoPictureParameters::PopulateH264UpdateFields (const StdVideoPictureParametersSet*				pStdPictureParametersSet,
																	  vk::VkVideoDecodeH264SessionParametersAddInfoKHR&	h264SessionParametersAddInfo)
{
	if (pStdPictureParametersSet == DE_NULL)
		return -1;

	DE_ASSERT((pStdPictureParametersSet->m_updateType == VK_PICTURE_PARAMETERS_UPDATE_H264_SPS) || (pStdPictureParametersSet->m_updateType == VK_PICTURE_PARAMETERS_UPDATE_H264_PPS));
	DE_ASSERT(h264SessionParametersAddInfo.sType == VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_ADD_INFO_KHR);

	if (pStdPictureParametersSet->m_updateType == VK_PICTURE_PARAMETERS_UPDATE_H264_SPS)
	{
		h264SessionParametersAddInfo.stdSPSCount = 1;
		h264SessionParametersAddInfo.pStdSPSs = &pStdPictureParametersSet->m_data.h264Sps.stdSps;
		return pStdPictureParametersSet->m_data.h264Sps.stdSps.seq_parameter_set_id;
	}
	else if (pStdPictureParametersSet->m_updateType == VK_PICTURE_PARAMETERS_UPDATE_H264_PPS)
	{
		h264SessionParametersAddInfo.stdPPSCount = 1;
		h264SessionParametersAddInfo.pStdPPSs = &pStdPictureParametersSet->m_data.h264Pps.stdPps;
		return pStdPictureParametersSet->m_data.h264Pps.stdPps.pic_parameter_set_id;
	}
	else
	{
		TCU_THROW(InternalError, "Incorrect h.264 type");
	}
}

int32_t NvidiaParserVideoPictureParameters::PopulateH265UpdateFields (const StdVideoPictureParametersSet*				pStdPictureParametersSet,
																	  vk::VkVideoDecodeH265SessionParametersAddInfoKHR&	h265SessionParametersAddInfo)
{
	if (pStdPictureParametersSet == DE_NULL)
		return -1;

	DE_ASSERT((pStdPictureParametersSet->m_updateType == VK_PICTURE_PARAMETERS_UPDATE_H265_SPS) || (pStdPictureParametersSet->m_updateType == VK_PICTURE_PARAMETERS_UPDATE_H265_PPS)
		|| (pStdPictureParametersSet->m_updateType == VK_PICTURE_PARAMETERS_UPDATE_H265_VPS));
	DE_ASSERT(h265SessionParametersAddInfo.sType == VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_SESSION_PARAMETERS_ADD_INFO_KHR);

	if (pStdPictureParametersSet->m_updateType == VK_PICTURE_PARAMETERS_UPDATE_H265_VPS)
	{
		h265SessionParametersAddInfo.stdVPSCount = 1;
		h265SessionParametersAddInfo.pStdVPSs = &pStdPictureParametersSet->m_data.h265Vps.stdVps;
		return pStdPictureParametersSet->m_data.h265Vps.stdVps.vps_video_parameter_set_id;
	}
	else if (pStdPictureParametersSet->m_updateType == VK_PICTURE_PARAMETERS_UPDATE_H265_SPS)
	{
		h265SessionParametersAddInfo.stdSPSCount = 1;
		h265SessionParametersAddInfo.pStdSPSs = &pStdPictureParametersSet->m_data.h265Sps.stdSps;
		return pStdPictureParametersSet->m_data.h265Sps.stdSps.sps_seq_parameter_set_id;
	}
	else if (pStdPictureParametersSet->m_updateType == VK_PICTURE_PARAMETERS_UPDATE_H265_PPS)
	{
		h265SessionParametersAddInfo.stdPPSCount = 1;
		h265SessionParametersAddInfo.pStdPPSs = &pStdPictureParametersSet->m_data.h265Pps.stdPps;
		return pStdPictureParametersSet->m_data.h265Pps.stdPps.pps_seq_parameter_set_id;
	}
	else
	{
		TCU_THROW(InternalError, "Incorrect h.265 type");
	}
}

NvidiaParserVideoPictureParameters* NvidiaParserVideoPictureParameters::Create (const DeviceInterface&					vkd,
																				VkDevice								device,
																				VkVideoSessionKHR						videoSession,
																				const StdVideoPictureParametersSet*		pVpsStdPictureParametersSet,
																				const StdVideoPictureParametersSet*		pSpsStdPictureParametersSet,
																				const StdVideoPictureParametersSet*		pPpsStdPictureParametersSet,
																				NvidiaParserVideoPictureParameters*		pTemplate)
{
	int32_t												currentVpsId					= -1;
	int32_t												currentSpsId					= -1;
	int32_t												currentPpsId					= -1;
	const NvidiaParserVideoPictureParameters*			pTemplatePictureParameters		= pTemplate;
	vk::VkVideoSessionParametersCreateInfoKHR			createInfo						= vk::initVulkanStructure();
	vk::VkVideoDecodeH264SessionParametersCreateInfoKHR	h264SessionParametersCreateInfo	= vk::initVulkanStructure();
	vk::VkVideoDecodeH264SessionParametersAddInfoKHR	h264SessionParametersAddInfo	= vk::initVulkanStructure();
	vk::VkVideoDecodeH265SessionParametersCreateInfoKHR	h265SessionParametersCreateInfo	= vk::initVulkanStructure();
	vk::VkVideoDecodeH265SessionParametersAddInfoKHR	h265SessionParametersAddInfo	= vk::initVulkanStructure();

	NvidiaParserPictureParametersUpdateType				updateType;
	if (pSpsStdPictureParametersSet)
		updateType = pSpsStdPictureParametersSet->m_updateType;
	else if (pPpsStdPictureParametersSet)
		updateType = pPpsStdPictureParametersSet->m_updateType;
	else if (pVpsStdPictureParametersSet)
		updateType = pVpsStdPictureParametersSet->m_updateType;
	else
		return nullptr; // Nothing to update

	NvidiaParserVideoPictureParameters*					pPictureParameters				= new NvidiaParserVideoPictureParameters(device);

	if (pPictureParameters == DE_NULL)
		return DE_NULL;

	switch (updateType)
	{
		case VK_PICTURE_PARAMETERS_UPDATE_H264_SPS:
		case VK_PICTURE_PARAMETERS_UPDATE_H264_PPS:
		{
			createInfo.pNext = &h264SessionParametersCreateInfo;
			h264SessionParametersCreateInfo.maxStdSPSCount		= MAX_SPS_IDS;
			h264SessionParametersCreateInfo.maxStdPPSCount		= MAX_PPS_IDS;
			h264SessionParametersCreateInfo.pParametersAddInfo	= &h264SessionParametersAddInfo;

			currentSpsId = PopulateH264UpdateFields(pSpsStdPictureParametersSet, h264SessionParametersAddInfo);
			currentPpsId = PopulateH264UpdateFields(pPpsStdPictureParametersSet, h264SessionParametersAddInfo);

			break;
		}
		case VK_PICTURE_PARAMETERS_UPDATE_H265_VPS:
		case VK_PICTURE_PARAMETERS_UPDATE_H265_SPS:
		case VK_PICTURE_PARAMETERS_UPDATE_H265_PPS:
		{
			createInfo.pNext = &h265SessionParametersCreateInfo;

			h265SessionParametersCreateInfo.maxStdVPSCount		= MAX_VPS_IDS;
			h265SessionParametersCreateInfo.maxStdSPSCount		= MAX_SPS_IDS;
			h265SessionParametersCreateInfo.maxStdPPSCount		= MAX_PPS_IDS;
			h265SessionParametersCreateInfo.pParametersAddInfo	= &h265SessionParametersAddInfo;

			currentVpsId = PopulateH265UpdateFields(pVpsStdPictureParametersSet, h265SessionParametersAddInfo);
			currentSpsId = PopulateH265UpdateFields(pSpsStdPictureParametersSet, h265SessionParametersAddInfo);
			currentPpsId = PopulateH265UpdateFields(pPpsStdPictureParametersSet, h265SessionParametersAddInfo);

			break;
		}
		default:
			TCU_THROW(InternalError, "Invalid Parser format");
	}

	createInfo.videoSessionParametersTemplate	= pTemplatePictureParameters != DE_NULL
												? pTemplatePictureParameters->GetVideoSessionParametersKHR()
												: VkVideoSessionParametersKHR(DE_NULL);
	createInfo.videoSession						= videoSession;

	pPictureParameters->m_sessionParameters = createVideoSessionParametersKHR(vkd, device, &createInfo);

	DEBUGLOG(cout << "VkVideoSessionParametersKHR:" << pPictureParameters->m_sessionParameters.get() << endl);

	if (pTemplatePictureParameters)
	{
		pPictureParameters->m_vpsIdsUsed = pTemplatePictureParameters->m_vpsIdsUsed;
		pPictureParameters->m_spsIdsUsed = pTemplatePictureParameters->m_spsIdsUsed;
		pPictureParameters->m_ppsIdsUsed = pTemplatePictureParameters->m_ppsIdsUsed;
	}

	DE_ASSERT((currentVpsId != -1) || (currentSpsId != -1) || (currentPpsId != -1));

	if (currentVpsId != -1)
	{
		pPictureParameters->m_vpsIdsUsed.set(currentVpsId, true);
	}

	if (currentSpsId != -1)
	{
		pPictureParameters->m_spsIdsUsed.set(currentSpsId, true);
	}

	if (currentPpsId != -1)
	{
		pPictureParameters->m_ppsIdsUsed.set(currentPpsId, true);
	}

	pPictureParameters->m_Id = ++m_currentId;

	return pPictureParameters;
}

VkResult NvidiaParserVideoPictureParameters::Update (const DeviceInterface&					vkd,
													 const StdVideoPictureParametersSet*	pVpsStdPictureParametersSet,
													 const StdVideoPictureParametersSet*	pSpsStdPictureParametersSet,
													 const StdVideoPictureParametersSet*	pPpsStdPictureParametersSet)
{
	int32_t currentVpsId = -1;
	int32_t currentSpsId = -1;
	int32_t currentPpsId = -1;

	VkVideoSessionParametersUpdateInfoKHR			updateInfo						= vk::initVulkanStructure();
	VkVideoDecodeH264SessionParametersAddInfoKHR	h264SessionParametersAddInfo	= vk::initVulkanStructure();
	VkVideoDecodeH265SessionParametersAddInfoKHR	h265SessionParametersAddInfo	= vk::initVulkanStructure();
	NvidiaParserPictureParametersUpdateType				updateType;
	if (pSpsStdPictureParametersSet)
		updateType = pSpsStdPictureParametersSet->m_updateType;
	else if (pPpsStdPictureParametersSet)
		updateType = pPpsStdPictureParametersSet->m_updateType;
	else if (pVpsStdPictureParametersSet)
		updateType = pVpsStdPictureParametersSet->m_updateType;
	else
		TCU_THROW(InternalError, "Invalid parameter type");

	switch (updateType)
	{
		case VK_PICTURE_PARAMETERS_UPDATE_H264_SPS:
		case VK_PICTURE_PARAMETERS_UPDATE_H264_PPS:
		{

			updateInfo.pNext = &h264SessionParametersAddInfo;

			currentSpsId = PopulateH264UpdateFields(pSpsStdPictureParametersSet, h264SessionParametersAddInfo);
			currentPpsId = PopulateH264UpdateFields(pPpsStdPictureParametersSet, h264SessionParametersAddInfo);

			break;
		}
		case VK_PICTURE_PARAMETERS_UPDATE_H265_VPS:
		case VK_PICTURE_PARAMETERS_UPDATE_H265_SPS:
		case VK_PICTURE_PARAMETERS_UPDATE_H265_PPS:
		{

			updateInfo.pNext = &h265SessionParametersAddInfo;

			currentVpsId = PopulateH265UpdateFields(pVpsStdPictureParametersSet, h265SessionParametersAddInfo);
			currentSpsId = PopulateH265UpdateFields(pSpsStdPictureParametersSet, h265SessionParametersAddInfo);
			currentPpsId = PopulateH265UpdateFields(pPpsStdPictureParametersSet, h265SessionParametersAddInfo);

			break;
		}
		default:
			TCU_THROW(InternalError, "Invalid Parser format");
	}

	if (pVpsStdPictureParametersSet)
	{
		updateInfo.updateSequenceCount = std::max(pVpsStdPictureParametersSet->m_updateSequenceCount, updateInfo.updateSequenceCount);
	}

	if (pSpsStdPictureParametersSet)
	{
		updateInfo.updateSequenceCount = std::max(pSpsStdPictureParametersSet->m_updateSequenceCount, updateInfo.updateSequenceCount);
	}

	if (pPpsStdPictureParametersSet)
	{
		updateInfo.updateSequenceCount = std::max(pPpsStdPictureParametersSet->m_updateSequenceCount, updateInfo.updateSequenceCount);
	}

	vk::VkResult result = vkd.updateVideoSessionParametersKHR(m_device, *m_sessionParameters, &updateInfo);

	if (result == VK_SUCCESS)
	{
		DE_ASSERT((currentVpsId != -1) || (currentSpsId != -1) || (currentPpsId != -1));

		if (currentVpsId != -1)
		{
			m_vpsIdsUsed.set(currentVpsId, true);
		}

		if (currentSpsId != -1)
		{
			m_spsIdsUsed.set(currentSpsId, true);
		}

		if (currentPpsId != -1)
		{
			m_ppsIdsUsed.set(currentPpsId, true);
		}
	}
	else
	{
		TCU_THROW(InternalError, "Could not update Session Parameters Object");
	}

	return result;
}

int32_t NvidiaParserVideoPictureParameters::AddRef ()
{
	return ++m_refCount;
}

int32_t NvidiaParserVideoPictureParameters::Release ()
{
	uint32_t ret = --m_refCount;

	if (ret == 0)
	{
		delete this;
	}

	return ret;
}

ImageObject::ImageObject ()
	: m_imageFormat			(VK_FORMAT_UNDEFINED)
	, m_imageExtent			()
	, m_image				(DE_NULL)
	, m_imageView			()
	, m_imageArrayLayers	(0)
{
}

ImageObject::~ImageObject ()
{
	DestroyImage();
}

void ImageObject::DestroyImage ()
{
	m_image				= de::MovePtr<ImageWithMemory>(DE_NULL);
	m_imageView			= Move<VkImageView>();
	m_imageFormat		= VK_FORMAT_UNDEFINED;
	m_imageExtent		= makeExtent2D(0,0);
	m_imageArrayLayers	= 0;
}

VkResult ImageObject::CreateImage (const DeviceInterface&	vkd,
								   VkDevice					device,
								   int32_t					queueFamilyIndex,
								   Allocator&				allocator,
								   const VkImageCreateInfo*	pImageCreateInfo,
								   const MemoryRequirement	memoryRequirement)
{
	DestroyImage();

	m_imageFormat		= pImageCreateInfo->format;
	m_imageExtent		= makeExtent2D(pImageCreateInfo->extent.width, pImageCreateInfo->extent.height);
	m_image				= de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, allocator, *pImageCreateInfo, memoryRequirement));
	m_imageArrayLayers	= pImageCreateInfo->arrayLayers;

	DE_ASSERT(m_imageArrayLayers != 0);

	VkResult status = StageImage(vkd, device, pImageCreateInfo->usage, memoryRequirement, queueFamilyIndex);

	if (VK_SUCCESS != status)
	{
		return status;
	}

	const VkImageViewCreateInfo imageViewCreateInfo	=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,					//  VkStructureType			sType;
		DE_NULL,													//  const void*				pNext;
		0,															//  VkImageViewCreateFlags	flags;
		m_image->get(),												//  VkImage					image;
		VK_IMAGE_VIEW_TYPE_2D,										//  VkImageViewType			viewType;
		m_imageFormat,												//  VkFormat				format;
		makeComponentMappingIdentity(),								//  VkComponentMapping		components;
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, m_imageArrayLayers },	//  VkImageSubresourceRange	subresourceRange;
	};

	m_imageView = createImageView(vkd, device, &imageViewCreateInfo);

	return VK_SUCCESS;
}

VkResult ImageObject::StageImage (const DeviceInterface&	vkd,
								  VkDevice					device,
								  VkImageUsageFlags			usage,
								  const MemoryRequirement	memoryRequirement,
								  uint32_t					queueFamilyIndex)
{
	if (usage == 0 && memoryRequirement == MemoryRequirement::Any)
	{
		return VK_ERROR_FORMAT_NOT_SUPPORTED;
	}

	Move<VkCommandPool>				cmdPool		= createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
	Move<VkCommandBuffer>			cmdBuffer	= allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const VkQueue					queue		= getDeviceQueue(vkd, device, queueFamilyIndex, 0u);
	const VkImageLayout				layout		= (usage & VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR) ? VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR
												: (usage & VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR) ? VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR
												: VK_IMAGE_LAYOUT_UNDEFINED;

	DE_ASSERT(layout != VK_IMAGE_LAYOUT_UNDEFINED);

	beginCommandBuffer(vkd, *cmdBuffer, 0u);

	setImageLayout(vkd, *cmdBuffer, m_image->get(), VK_IMAGE_LAYOUT_UNDEFINED, layout, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR);

	endCommandBuffer(vkd, *cmdBuffer);

	submitCommandsAndWait(vkd, device, queue, *cmdBuffer);

	return VK_SUCCESS;
}

VkFormat ImageObject::getFormat (void) const
{
	return m_imageFormat;
}

VkExtent2D ImageObject::getExtent (void) const
{
	return m_imageExtent;
}

VkImage ImageObject::getImage (void) const
{
	return m_image->get();
}

VkImageView ImageObject::getView (void) const
{
	return m_imageView.get();
}

bool ImageObject::isArray (void) const
{
	return m_imageArrayLayers > 1;
}

bool ImageObject::isImageExist (void) const
{
	return (m_image != DE_NULL) && (m_image->get() != DE_NULL);
}


NvidiaPerFrameDecodeImage::NvidiaPerFrameDecodeImage ()
	: NvidiaVulkanPictureBase			()
	, m_picDispInfo						()
	, m_frameImage						()
	, m_frameImageCurrentLayout			(VK_IMAGE_LAYOUT_UNDEFINED)
	, m_frameCompleteFence				()
	, m_frameCompleteSemaphore			()
	, m_frameConsumerDoneFence			()
	, m_frameConsumerDoneSemaphore		()
	, m_hasFrameCompleteSignalFence		(false)
	, m_hasFrameCompleteSignalSemaphore	(false)
	, m_hasConsummerSignalFence			(false)
	, m_hasConsummerSignalSemaphore		(false)
	, m_inDecodeQueue					(false)
	, m_inDisplayQueue					(false)
	, m_ownedByDisplay					(false)
	, m_dpbImage						()
	, m_dpbImageCurrentLayout			(VK_IMAGE_LAYOUT_UNDEFINED)
{
}

void NvidiaPerFrameDecodeImage::init (const DeviceInterface&	vkd,
									  VkDevice					device)
{
	const VkFenceCreateInfo		fenceFrameCompleteInfo	=	// The fence waited on for the first frame should be signaled.
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	//  VkStructureType		sType;
		DE_NULL,								//  const void*			pNext;
		VK_FENCE_CREATE_SIGNALED_BIT,			//  VkFenceCreateFlags	flags;
	};
	const VkFenceCreateInfo		fenceInfo				=
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	//  VkStructureType		sType;
		DE_NULL,								//  const void*			pNext;
		0,										//  VkFenceCreateFlags	flags;
	};
	const VkSemaphoreCreateInfo	semInfo					=
	{
		VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,	//  VkStructureType			sType;
		DE_NULL,									//  const void*				pNext;
		0,											//  VkSemaphoreCreateFlags	flags;
	};

	m_frameCompleteFence			= createFence(vkd, device, &fenceFrameCompleteInfo);
	m_frameConsumerDoneFence		= createFence(vkd, device, &fenceInfo);
	m_frameCompleteSemaphore		= createSemaphore(vkd, device, &semInfo);
	m_frameConsumerDoneSemaphore	= createSemaphore(vkd, device, &semInfo);

	Reset();
}

VkResult NvidiaPerFrameDecodeImage::CreateImage (const DeviceInterface&		vkd,
												 VkDevice					device,
												 int32_t					queueFamilyIndex,
												 Allocator&					allocator,
												 const VkImageCreateInfo*	pImageCreateInfo,
												 const MemoryRequirement	memoryRequirement)
{
	VK_CHECK(m_frameImage.CreateImage(vkd, device, queueFamilyIndex, allocator, pImageCreateInfo, memoryRequirement));

	m_frameImageCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	return VK_SUCCESS;
}

const ImageObject* NvidiaPerFrameDecodeImage::GetImageObject (void)
{
	return isImageExist() ? &m_frameImage : DE_NULL;
}

bool NvidiaPerFrameDecodeImage::isImageExist (void)
{
	return m_frameImage.isImageExist();
}

const ImageObject* NvidiaPerFrameDecodeImage::GetDPBImageObject (void)
{
	return m_dpbImage.get();
}

void NvidiaPerFrameDecodeImage::deinit ()
{
	currentVkPictureParameters		= DE_NULL;

	m_frameCompleteFence			= Move<VkFence>();
	m_frameConsumerDoneFence		= Move<VkFence>();
	m_frameCompleteSemaphore		= Move<VkSemaphore>();
	m_frameConsumerDoneSemaphore	= Move<VkSemaphore>();

	m_frameImage.DestroyImage();
	m_dpbImage.clear();

	Reset();
}

NvidiaPerFrameDecodeImage::~NvidiaPerFrameDecodeImage ()
{
	deinit();
}

NvidiaPerFrameDecodeImageSet::NvidiaPerFrameDecodeImageSet ()
	: m_size				(0)
	, m_frameDecodeImages	()
{
}

int32_t NvidiaPerFrameDecodeImageSet::init (const DeviceInterface&		vkd,
											VkDevice					device,
											int32_t						queueFamilyIndex,
											Allocator&					allocator,
											uint32_t					numImages,
											const VkImageCreateInfo*	pOutImageCreateInfo,
											const VkImageCreateInfo*	pDpbImageCreateInfo,
											MemoryRequirement			memoryRequirement)
{
	const uint32_t firstIndex = (uint32_t)m_size;

	// CTS is not designed to reinitialize images
	DE_ASSERT(numImages > m_size);

	DE_ASSERT(numImages < DE_LENGTH_OF_ARRAY(m_frameDecodeImages));

	for (uint32_t imageIndex = firstIndex; imageIndex < numImages; imageIndex++)
	{
		DE_ASSERT(!m_frameDecodeImages[imageIndex].isImageExist());

		m_frameDecodeImages[imageIndex].init(vkd, device);

		VK_CHECK(m_frameDecodeImages[imageIndex].CreateImage(vkd, device, queueFamilyIndex, allocator, pOutImageCreateInfo, memoryRequirement));

		DEBUGLOG(std::cout << "CreateImg: " << m_frameDecodeImages[imageIndex].m_frameImage.getImage() << " " << std::dec << pOutImageCreateInfo->extent.width << "x" << pOutImageCreateInfo->extent.height << " " << m_frameDecodeImages[imageIndex].m_frameImageCurrentLayout << std::endl);

		if (pDpbImageCreateInfo != DE_NULL)
		{
			DE_ASSERT(pDpbImageCreateInfo->arrayLayers == 1 || pDpbImageCreateInfo->arrayLayers >= numImages - m_size);

			if (pDpbImageCreateInfo->arrayLayers == 1 || imageIndex == firstIndex)
			{
				m_frameDecodeImages[imageIndex].m_dpbImage = de::SharedPtr<ImageObject>(new ImageObject());

				VK_CHECK(m_frameDecodeImages[imageIndex].m_dpbImage->CreateImage(vkd, device, queueFamilyIndex, allocator, pDpbImageCreateInfo, memoryRequirement));

				DEBUGLOG(std::cout << "CreateDPB: " << m_frameDecodeImages[imageIndex].m_dpbImage->getImage() << " " << std::dec << pOutImageCreateInfo->extent.width << "x" << pOutImageCreateInfo->extent.height << " " << m_frameDecodeImages[imageIndex].m_dpbImageCurrentLayout << std::endl);
			}
			else
			{
				m_frameDecodeImages[imageIndex].m_dpbImage = m_frameDecodeImages[firstIndex].m_dpbImage;
			}
		}
	}

	m_size = numImages;

	return (int32_t)m_size;
}

void NvidiaPerFrameDecodeImageSet::deinit ()
{
	for (uint32_t ndx = 0; ndx < m_size; ndx++)
		m_frameDecodeImages[ndx].deinit();

	m_size = 0;
}

NvidiaPerFrameDecodeImageSet::~NvidiaPerFrameDecodeImageSet ()
{
	deinit();
}

NvidiaPerFrameDecodeImage& NvidiaPerFrameDecodeImageSet::operator[] (size_t index)
{
	DE_ASSERT(index < m_size);

	return m_frameDecodeImages[index];
}

size_t NvidiaPerFrameDecodeImageSet::size ()
{
	return m_size;
}

VideoFrameBuffer::VideoFrameBuffer ()
	: m_perFrameDecodeImageSet	()
	, m_displayFrames			()
	, m_queryPool				()
	, m_ownedByDisplayMask		(0)
	, m_frameNumInDecodeOrder	(0)
	, m_frameNumInDisplayOrder	(0)
	, m_extent					{ 0, 0 }
{
}

Move<VkQueryPool> VideoFrameBuffer::CreateVideoQueries (const DeviceInterface&		vkd,
														VkDevice					device,
														uint32_t					numSlots,
														const VkVideoProfileInfoKHR*	pDecodeProfile)
{
	const VkQueryPoolCreateInfo	queryPoolCreateInfo	=
	{
		VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,	//  VkStructureType					sType;
		pDecodeProfile,								//  const void*						pNext;
		0,											//  VkQueryPoolCreateFlags			flags;
		VK_QUERY_TYPE_RESULT_STATUS_ONLY_KHR,		//  VkQueryType						queryType;
		numSlots,									//  deUint32						queryCount;
		0,											//  VkQueryPipelineStatisticFlags	pipelineStatistics;
	};

	return createQueryPool(vkd, device, &queryPoolCreateInfo);
}

int32_t VideoFrameBuffer::InitImagePool (const DeviceInterface&		vkd,
										 VkDevice					device,
										 int32_t					queueFamilyIndex,
										 Allocator&					allocator,
										 uint32_t					numImages,
										 uint32_t					maxNumImages,
										 const VkImageCreateInfo*	pOutImageCreateInfo,
										 const VkImageCreateInfo*	pDpbImageCreateInfo,
										 const VkVideoProfileInfoKHR*	pDecodeProfile)
{
	if (numImages && pDecodeProfile && m_queryPool.get() == DE_NULL)
		m_queryPool = CreateVideoQueries(vkd, device, maxNumImages, pDecodeProfile);

	if (numImages && pOutImageCreateInfo)
	{
		m_extent = makeExtent2D(pOutImageCreateInfo->extent.width, pOutImageCreateInfo->extent.height);

		return m_perFrameDecodeImageSet.init(vkd, device, queueFamilyIndex, allocator, numImages, pOutImageCreateInfo, pDpbImageCreateInfo, MemoryRequirement::Local);
	}

	return 0;
}

int32_t VideoFrameBuffer::QueueDecodedPictureForDisplay (int8_t picId, DisplayPictureInfo* pDispInfo)
{
	DE_ASSERT((uint32_t)picId < m_perFrameDecodeImageSet.size());

	m_perFrameDecodeImageSet[picId].m_displayOrder		= m_frameNumInDisplayOrder++;
	m_perFrameDecodeImageSet[picId].m_timestamp			= pDispInfo->timestamp;
	m_perFrameDecodeImageSet[picId].m_inDisplayQueue	= true;
	m_perFrameDecodeImageSet[picId].AddRef();

	m_displayFrames.push((uint8_t)picId);

	return picId;
}

int32_t VideoFrameBuffer::QueuePictureForDecode (int8_t								picId,
												 VulkanParserDecodePictureInfo*		pDecodePictureInfo,
												 NvidiaParserVideoRefCountBase*		pCurrentVkPictureParameters,
												 FrameSynchronizationInfo*			pFrameSynchronizationInfo)
{
	DE_ASSERT((uint32_t)picId < m_perFrameDecodeImageSet.size());

	m_perFrameDecodeImageSet[picId].m_picDispInfo				= *pDecodePictureInfo;
	m_perFrameDecodeImageSet[picId].m_decodeOrder				= m_frameNumInDecodeOrder++;
	m_perFrameDecodeImageSet[picId].m_inDecodeQueue				= true;
	m_perFrameDecodeImageSet[picId].currentVkPictureParameters	= pCurrentVkPictureParameters;

	if (pFrameSynchronizationInfo->hasFrameCompleteSignalFence)
	{
		pFrameSynchronizationInfo->frameCompleteFence = m_perFrameDecodeImageSet[picId].m_frameCompleteFence.get();

		if (pFrameSynchronizationInfo->frameCompleteFence != DE_NULL)
		{
			m_perFrameDecodeImageSet[picId].m_hasFrameCompleteSignalFence = true;
		}
	}

	if (m_perFrameDecodeImageSet[picId].m_hasConsummerSignalFence)
	{
		pFrameSynchronizationInfo->frameConsumerDoneFence = m_perFrameDecodeImageSet[picId].m_frameConsumerDoneFence.get();

		m_perFrameDecodeImageSet[picId].m_hasConsummerSignalFence = false;
	}

	if (pFrameSynchronizationInfo->hasFrameCompleteSignalSemaphore)
	{
		pFrameSynchronizationInfo->frameCompleteSemaphore = m_perFrameDecodeImageSet[picId].m_frameCompleteSemaphore.get();

		if (pFrameSynchronizationInfo->frameCompleteSemaphore != DE_NULL)
		{
			m_perFrameDecodeImageSet[picId].m_hasFrameCompleteSignalSemaphore = true;
		}
	}

	if (m_perFrameDecodeImageSet[picId].m_hasConsummerSignalSemaphore)
	{
		pFrameSynchronizationInfo->frameConsumerDoneSemaphore = m_perFrameDecodeImageSet[picId].m_frameConsumerDoneSemaphore.get();

		m_perFrameDecodeImageSet[picId].m_hasConsummerSignalSemaphore = false;
	}

	pFrameSynchronizationInfo->queryPool	= m_queryPool.get();
	pFrameSynchronizationInfo->startQueryId	= picId;
	pFrameSynchronizationInfo->numQueries	= 1;

	return picId;
}

int32_t VideoFrameBuffer::DequeueDecodedPicture (DecodedFrame* pDecodedFrame)
{
	int	numberOfPendingFrames	= 0;
	int	pictureIndex			= -1;

	if (!m_displayFrames.empty())
	{
		numberOfPendingFrames = (int)m_displayFrames.size();
		pictureIndex = m_displayFrames.front();

		DE_ASSERT((pictureIndex >= 0) && ((uint32_t)pictureIndex < m_perFrameDecodeImageSet.size()));
		DE_ASSERT(!(m_ownedByDisplayMask & (1 << pictureIndex)));

		m_ownedByDisplayMask |= (1 << pictureIndex);
		m_displayFrames.pop();
		m_perFrameDecodeImageSet[pictureIndex].m_inDisplayQueue = false;
		m_perFrameDecodeImageSet[pictureIndex].m_ownedByDisplay = true;
	}

	if ((uint32_t)pictureIndex < m_perFrameDecodeImageSet.size())
	{
		pDecodedFrame->pictureIndex = pictureIndex;

		pDecodedFrame->pDecodedImage = &m_perFrameDecodeImageSet[pictureIndex].m_frameImage;

		pDecodedFrame->decodedImageLayout = m_perFrameDecodeImageSet[pictureIndex].m_frameImageCurrentLayout;

		if (m_perFrameDecodeImageSet[pictureIndex].m_hasFrameCompleteSignalFence)
		{
			pDecodedFrame->frameCompleteFence = m_perFrameDecodeImageSet[pictureIndex].m_frameCompleteFence.get();

			m_perFrameDecodeImageSet[pictureIndex].m_hasFrameCompleteSignalFence = false;
		}
		else
		{
			pDecodedFrame->frameCompleteFence = DE_NULL;
		}

		if (m_perFrameDecodeImageSet[pictureIndex].m_hasFrameCompleteSignalSemaphore)
		{
			pDecodedFrame->frameCompleteSemaphore = m_perFrameDecodeImageSet[pictureIndex].m_frameCompleteSemaphore.get();

			m_perFrameDecodeImageSet[pictureIndex].m_hasFrameCompleteSignalSemaphore = false;
		}
		else
		{
			pDecodedFrame->frameCompleteSemaphore = DE_NULL;
		}

		pDecodedFrame->frameConsumerDoneFence		= m_perFrameDecodeImageSet[pictureIndex].m_frameConsumerDoneFence.get();
		pDecodedFrame->frameConsumerDoneSemaphore	= m_perFrameDecodeImageSet[pictureIndex].m_frameConsumerDoneSemaphore.get();
		pDecodedFrame->timestamp					= m_perFrameDecodeImageSet[pictureIndex].m_timestamp;
		pDecodedFrame->decodeOrder					= m_perFrameDecodeImageSet[pictureIndex].m_decodeOrder;
		pDecodedFrame->displayOrder					= m_perFrameDecodeImageSet[pictureIndex].m_displayOrder;
		pDecodedFrame->queryPool					= m_queryPool.get();
		pDecodedFrame->startQueryId					= pictureIndex;
		pDecodedFrame->numQueries					= 1;
	}

	return numberOfPendingFrames;
}

int32_t VideoFrameBuffer::GetDisplayFramesCount (void)
{
	return static_cast<int32_t>(m_displayFrames.size());
}

int32_t VideoFrameBuffer::ReleaseDisplayedPicture (DecodedFrameRelease**	pDecodedFramesRelease,
												   uint32_t					numFramesToRelease)
{
	for (uint32_t i = 0; i < numFramesToRelease; i++)
	{
		const DecodedFrameRelease*	pDecodedFrameRelease	= pDecodedFramesRelease[i];
		int							picId					= pDecodedFrameRelease->pictureIndex;

		DE_ASSERT((picId >= 0) && ((uint32_t)picId < m_perFrameDecodeImageSet.size()));
		DE_ASSERT(m_perFrameDecodeImageSet[picId].m_decodeOrder == pDecodedFrameRelease->decodeOrder);
		DE_ASSERT(m_perFrameDecodeImageSet[picId].m_displayOrder == pDecodedFrameRelease->displayOrder);
		DE_ASSERT(m_ownedByDisplayMask & (1 << picId));

		m_ownedByDisplayMask &= ~(1 << picId);
		m_perFrameDecodeImageSet[picId].m_inDecodeQueue				= false;
		m_perFrameDecodeImageSet[picId].currentVkPictureParameters	= DE_NULL;
		m_perFrameDecodeImageSet[picId].m_ownedByDisplay			= false;
		m_perFrameDecodeImageSet[picId].Release();

		m_perFrameDecodeImageSet[picId].m_hasConsummerSignalFence = pDecodedFrameRelease->hasConsummerSignalFence;
		m_perFrameDecodeImageSet[picId].m_hasConsummerSignalSemaphore = pDecodedFrameRelease->hasConsummerSignalSemaphore;
	}

	return 0;
}

void VideoFrameBuffer::GetImageResourcesByIndex (int32_t					numResources,
												 const int8_t*				referenceSlotIndexes,
												 VkVideoPictureResourceInfoKHR*	videoPictureResources,
												 PictureResourceInfo*		pictureResourcesInfos,
												 VkImageLayout				newImageLayout)
{
	DE_ASSERT(newImageLayout == VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR || newImageLayout == VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR);
	DE_ASSERT(pictureResourcesInfos != DE_NULL);

	for (int32_t resId = 0; resId < numResources; resId++)
	{
		const int32_t	referenceSlotIndex			= referenceSlotIndexes[resId];
		const int32_t	perFrameDecodeImageSetSize	= (int32_t)m_perFrameDecodeImageSet.size();

		if (de::inBounds(referenceSlotIndex, 0, perFrameDecodeImageSetSize))
		{
			NvidiaPerFrameDecodeImage&	perFrameDecodeImage		= m_perFrameDecodeImageSet[referenceSlotIndex];
			VkVideoPictureResourceInfoKHR&	videoPictureResource	= videoPictureResources[resId];
			PictureResourceInfo&		pictureResourcesInfo	= pictureResourcesInfos[resId];

			DE_ASSERT(videoPictureResource.sType == VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR);

			if (newImageLayout == VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR || perFrameDecodeImage.m_dpbImage == DE_NULL)
			{
				videoPictureResource.codedOffset				= { 0, 0 }; // FIXME: This parameter must to be adjusted based on the interlaced mode.
				videoPictureResource.codedExtent				= m_extent;
				videoPictureResource.baseArrayLayer				= 0;
				videoPictureResource.imageViewBinding			= perFrameDecodeImage.m_frameImage.getView();

				pictureResourcesInfo.image						= perFrameDecodeImage.m_frameImage.getImage();
				pictureResourcesInfo.currentImageLayout			= perFrameDecodeImage.m_frameImageCurrentLayout;

				perFrameDecodeImage.m_frameImageCurrentLayout	= newImageLayout;
			}
			else if (newImageLayout == VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR)
			{
				videoPictureResource.codedOffset				= { 0, 0 }; // FIXME: This parameter must to be adjusted based on the interlaced mode.
				videoPictureResource.codedExtent				= m_extent;
				videoPictureResource.baseArrayLayer				= perFrameDecodeImage.m_dpbImage->isArray() ? referenceSlotIndex : 0;
				videoPictureResource.imageViewBinding			= perFrameDecodeImage.m_dpbImage->getView();

				pictureResourcesInfo.image						= perFrameDecodeImage.m_dpbImage->getImage();
				pictureResourcesInfo.currentImageLayout			= perFrameDecodeImage.m_dpbImageCurrentLayout;

				perFrameDecodeImage.m_dpbImageCurrentLayout		= newImageLayout;
			}
			else
				DE_ASSERT(0 && "Unknown image resource requested");
		}
	}
}

void VideoFrameBuffer::GetImageResourcesByIndex (const int8_t				referenceSlotIndex,
												 VkVideoPictureResourceInfoKHR*	videoPictureResources,
												 PictureResourceInfo*		pictureResourcesInfos,
												 VkImageLayout				newImageLayout)
{
	GetImageResourcesByIndex(1, &referenceSlotIndex, videoPictureResources, pictureResourcesInfos, newImageLayout);
}

int32_t VideoFrameBuffer::SetPicNumInDecodeOrder (int32_t picId, int32_t picNumInDecodeOrder)
{
	if ((uint32_t)picId < m_perFrameDecodeImageSet.size())
	{
		int32_t oldPicNumInDecodeOrder = m_perFrameDecodeImageSet[picId].m_decodeOrder;

		m_perFrameDecodeImageSet[picId].m_decodeOrder = picNumInDecodeOrder;

		return oldPicNumInDecodeOrder;
	}

	TCU_THROW(InternalError, "Impossible in SetPicNumInDecodeOrder");
}

int32_t VideoFrameBuffer::SetPicNumInDisplayOrder (int32_t picId, int32_t picNumInDisplayOrder)
{
	if ((uint32_t)picId < m_perFrameDecodeImageSet.size())
	{
		int32_t oldPicNumInDisplayOrder = m_perFrameDecodeImageSet[picId].m_displayOrder;

		m_perFrameDecodeImageSet[picId].m_displayOrder = picNumInDisplayOrder;

		return oldPicNumInDisplayOrder;
	}

	TCU_THROW(InternalError, "Impossible in SetPicNumInDisplayOrder");
}

NvidiaVulkanPictureBase* VideoFrameBuffer::ReservePictureBuffer (void)
{
	for (uint32_t picId = 0; picId < m_perFrameDecodeImageSet.size(); picId++)
	{
		NvidiaVulkanPictureBase& perFrameDecodeImage = m_perFrameDecodeImageSet[picId];

		if (perFrameDecodeImage.IsAvailable())
		{
			perFrameDecodeImage.Reset();
			perFrameDecodeImage.AddRef();
			perFrameDecodeImage.m_picIdx = picId;

			DEBUGLOG(std::cout << "\tReservePictureBuffer " << picId << std::endl);

			return &perFrameDecodeImage;
		}
	}

	TCU_THROW(InternalError, "ReservePictureBuffer failed");
}

size_t VideoFrameBuffer::GetSize (void)
{
	return m_perFrameDecodeImageSet.size();
}

VideoFrameBuffer::~VideoFrameBuffer ()
{
}

}	// video
}	// vkt
